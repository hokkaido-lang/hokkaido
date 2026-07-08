#include "codegen.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// -------------------------------------------------------------------------
// Expression type resolution
// -------------------------------------------------------------------------

TypeAnnotation CodeGen::resolve_expr_type(Expr *expr) {
  if (auto *id = dynamic_cast<IdentExpr *>(expr)) {
    auto it = named_type_anns.find(id->name);
    if (it != named_type_anns.end())
      return it->second;
    auto kt = named_types.find(id->name);
    if (kt != named_types.end())
      return {kt->second};
    auto gi = global_values.find(id->name);
    if (gi != global_values.end()) {
      return {TypeKind::Int64};
    }
    return {TypeKind::Void};
  }
  if (auto *field = dynamic_cast<FieldAccessExpr *>(expr)) {
    TypeAnnotation base = resolve_expr_type(field->object.get());
    if (base.kind == TypeKind::Void || base.kind == TypeKind::Struct) {
      std::string struct_key = base.struct_name;
      if (!base.type_args.empty())
        struct_key = struct_mangled_name(base.struct_name, base.type_args);
      return get_struct_field_type(struct_key, field->field);
    }
    return {TypeKind::Void};
  }
  if (auto *addr = dynamic_cast<AddressOfExpr *>(expr)) {
    TypeAnnotation base = resolve_expr_type(addr->operand.get());
    base.pointer_depth++;
    return base;
  }
  if (auto *deref = dynamic_cast<DerefExpr *>(expr)) {
    TypeAnnotation base = resolve_expr_type(deref->operand.get());
    if (base.pointer_depth > 0) {
      base.pointer_depth--;
      return base;
    }
    return {TypeKind::Void};
  }
  if (auto *sub = dynamic_cast<SubscriptExpr *>(expr)) {
    TypeAnnotation base = resolve_expr_type(sub->array.get());
    if (base.array_size > 0) {
      base.array_size = 0;
      return base;
    }
    if (base.kind == TypeKind::Slice) {
      return base.tuple_types[0];
    }
    if (base.pointer_depth > 0) {
      base.pointer_depth--;
      return base;
    }
    return {base.kind};
  }
  if (auto *bin = dynamic_cast<BinaryExpr *>(expr)) {
    TypeAnnotation l = resolve_expr_type(bin->left.get());
    TypeAnnotation r = resolve_expr_type(bin->right.get());
    if (l.pointer_depth > 0) return l;
    if (r.pointer_depth > 0) return r;
    switch (bin->op) {
      case BinOp::Eq: case BinOp::Ne:
      case BinOp::Less: case BinOp::Greater: case BinOp::Le: case BinOp::Ge:
      case BinOp::And: case BinOp::Or:
        return {TypeKind::Bool};
      default:
        if (l.kind != TypeKind::Void) return l;
        return {TypeKind::Int64};
    }
  }
  if (dynamic_cast<NumberExpr *>(expr))
    return {TypeKind::Int64};
  if (auto *ctor = dynamic_cast<ConstructorExpr *>(expr)) {
    for (auto &[ename, _] : enum_types) {
      int vi = get_enum_variant_index(ename, ctor->variant_name);
      if (vi >= 0)
        return {TypeKind::Enum, 0, 0, ename};
    }
    std::string struct_key = ctor->variant_name;
    if (struct_types.count(struct_key) == 0 && struct_templates.count(struct_key) > 0)
      struct_key = struct_mangled_name(ctor->variant_name, ctor->type_args);
    if (struct_types.count(struct_key) > 0)
      return {TypeKind::Struct, 0, 0, struct_key};
    return {TypeKind::Void};
  }
  if (auto *atm = dynamic_cast<AtomicExpr *>(expr)) {
    if (atm->op == AtomicOp::Fence)
      return {TypeKind::Void};
    if (!atm->args.empty()) {
      TypeAnnotation ptr_ann = resolve_expr_type(atm->args[0].get());
      if (ptr_ann.pointer_depth > 0) {
        ptr_ann.pointer_depth--;
        return ptr_ann;
      }
    }
    return {TypeKind::Int64};
  }
  if (auto *ifexpr = dynamic_cast<IfExpr *>(expr)) {
    return resolve_expr_type(ifexpr->then_expr.get());
  }
  if (auto *tup = dynamic_cast<TupleExpr *>(expr)) {
    TypeAnnotation ann = {TypeKind::Tuple};
    for (auto &el : tup->elements)
      ann.tuple_types.push_back(resolve_expr_type(el.get()));
    return ann;
  }
  if (auto *closure = dynamic_cast<ClosureExpr *>(expr)) {
    TypeAnnotation ann = {TypeKind::Fn};
    for (auto &p : closure->params)
      ann.tuple_types.push_back(p.type_ann);
    ann.tuple_types.push_back(closure->return_type);
    return ann;
  }
  if (auto *call = dynamic_cast<CallExpr *>(expr)) {
    if (call->callee_expr) {
      TypeAnnotation callee_ann = resolve_expr_type(call->callee_expr.get());
      if (callee_ann.kind == TypeKind::Fn && callee_ann.tuple_types.size() >= 1)
        return callee_ann.tuple_types.back();
      return {TypeKind::Void};
    }
    if (!call->type_args.empty()) {
      auto it = generic_templates.find(call->callee);
      if (it != generic_templates.end()) {
        TypeAnnotation ret = it->second->return_type;
        for (size_t i = 0; i < it->second->type_params.size() && i < call->type_args.size(); i++) {
          if (ret.kind == TypeKind::TypeParam && ret.struct_name == it->second->type_params[i]) {
            ret = call->type_args[i];
            break;
          }
        }
        return ret;
      }
      return {TypeKind::Void};
    }
    Function *f = M.getFunction(call->callee);
    if (f) {
      Type *ret_type = f->getReturnType();
      if (ret_type->isIntegerTy(1)) return {TypeKind::Bool};
      if (ret_type->isIntegerTy(8)) return {TypeKind::Int8};
      if (ret_type->isIntegerTy(16)) return {TypeKind::Int16};
      if (ret_type->isIntegerTy(32)) return {TypeKind::Int32};
      if (ret_type->isIntegerTy(64)) return {TypeKind::Int64};
      if (ret_type->isHalfTy()) return {TypeKind::Float16};
      if (ret_type->isFloatTy()) return {TypeKind::Float32};
      if (ret_type->isDoubleTy()) return {TypeKind::Float64};
      if (ret_type->isPointerTy()) return {TypeKind::String};
      if (ret_type->isStructTy()) {
        if (auto *st = dyn_cast<StructType>(ret_type)) {
          if (st->hasName())
            return {TypeKind::Struct, 0, 0, std::string(st->getName())};
        }
        return {TypeKind::Void};
      }
    }
    return {TypeKind::Void};
  }
  if (auto *mcall = dynamic_cast<MethodCallExpr *>(expr)) {
    TypeAnnotation obj_type = resolve_expr_type(mcall->object.get());
    std::string type_name;
    if (obj_type.kind == TypeKind::Struct)
      type_name = obj_type.struct_name;
    else if (obj_type.kind == TypeKind::Enum)
      type_name = obj_type.struct_name;
    else
      return {TypeKind::Void};
    auto it = impl_methods.find({type_name, mcall->method_name});
    if (it != impl_methods.end()) {
      auto rit = impl_method_ret_types.find(it->second);
      if (rit != impl_method_ret_types.end())
        return rit->second;
    }
    return {TypeKind::Void};
  }
  return {TypeKind::Void};
}

// -------------------------------------------------------------------------
// Lvalue pointer resolution
// -------------------------------------------------------------------------

Value *CodeGen::get_lvalue_ptr(Expr *expr, Type **out_type) {
  if (auto *id = dynamic_cast<IdentExpr *>(expr)) {
    auto it = named_values.find(id->name);
    if (it == named_values.end()) {
      auto gi = global_values.find(id->name);
      if (gi != global_values.end()) {
        if (out_type) *out_type = gi->second->getValueType();
        return gi->second;
      }
      errs() << "Error: undefined variable '" << id->name << "'\n";
      return nullptr;
    }
    if (out_type) *out_type = it->second->getAllocatedType();
    return it->second;
  }
  if (auto *deref = dynamic_cast<DerefExpr *>(expr)) {
    Value *ptr = eval_expr(deref->operand.get(), PointerType::getUnqual(Context));
    if (!ptr) return nullptr;
    if (out_type) {
      TypeAnnotation ann = resolve_expr_type(deref);
      *out_type = get_llvm_type(ann);
    }
    return ptr;
  }
  if (auto *sub = dynamic_cast<SubscriptExpr *>(expr)) {
    TypeAnnotation base_ann = resolve_expr_type(sub->array.get());
    Value *index = eval_expr(sub->index.get(), Type::getInt64Ty(Context));
    if (!index) return nullptr;

    if (base_ann.kind == TypeKind::Slice) {
      // Slice subscript: extract ptr from slice struct, then GEP
      Type *slice_type = get_llvm_type(base_ann);
      Value *slice_val = eval_expr(sub->array.get(), slice_type);
      if (!slice_val) return nullptr;
      Value *ptr = Builder.CreateExtractValue(slice_val, {0}, "slice_ptr");
      TypeAnnotation elem_ann = base_ann.tuple_types[0];
      Type *elem_type = get_llvm_type(elem_ann);
      Value *elem_ptr = Builder.CreateGEP(elem_type, ptr, index, "slice_elem_ptr");
      if (out_type) *out_type = elem_type;
      return elem_ptr;
    }

    if (base_ann.array_size > 0) {
      Type *arr_llvm_type = nullptr;
      Value *arr_ptr = get_lvalue_ptr(sub->array.get(), &arr_llvm_type);
      if (!arr_ptr) return nullptr;
      Value *indices[] = {
        ConstantInt::get(Type::getInt64Ty(Context), 0),
        index
      };
      Value *gep = Builder.CreateGEP(arr_llvm_type, arr_ptr, indices, "elem_ptr");
      if (out_type) {
        if (auto *arr = dyn_cast_or_null<ArrayType>(arr_llvm_type))
          *out_type = arr->getElementType();
      }
      return gep;
    }

    if (base_ann.pointer_depth > 0) {
      Type *base_llvm = get_llvm_type(base_ann);
      if (!base_llvm) return nullptr;
      Value *arr_ptr = eval_expr(sub->array.get(), base_llvm);
      if (!arr_ptr) return nullptr;
      base_ann.pointer_depth--;
      Type *pointee = get_llvm_type(base_ann);
      if (!pointee) pointee = Type::getInt8Ty(Context);
      Value *gep = Builder.CreateGEP(pointee, arr_ptr, index, "elem_ptr");
      if (out_type) *out_type = pointee;
      return gep;
    }

    errs() << "Error: subscript requires an array or pointer\n";
    return nullptr;
  }
  if (auto *field = dynamic_cast<FieldAccessExpr *>(expr)) {
    TypeAnnotation base_ann = resolve_expr_type(field->object.get());

    if (base_ann.kind == TypeKind::Tuple) {
      int idx = std::stoi(field->field);
      Type *tup_type = nullptr;
      Value *tup_ptr = get_lvalue_ptr(field->object.get(), &tup_type);
      if (!tup_ptr) return nullptr;
      Value *field_ptr = Builder.CreateStructGEP(tup_type, tup_ptr, idx, field->field);
      if (out_type) {
        if (idx < (int)base_ann.tuple_types.size())
          *out_type = get_llvm_type(base_ann.tuple_types[idx]);
      }
      return field_ptr;
    }

    Type *struct_type = nullptr;
    Value *struct_ptr = get_lvalue_ptr(field->object.get(), &struct_type);
    if (!struct_ptr) return nullptr;

    StructType *st = dyn_cast<StructType>(struct_type);
    if (!st) {
      errs() << "Error: field access on non-struct type\n";
      return nullptr;
    }

    std::string struct_name = st->getName().str();
    int field_idx = get_struct_field_index(struct_name, field->field);
    if (field_idx < 0) {
      errs() << "Error: struct '" << struct_name << "' has no field named '"
             << field->field << "'\n";
      return nullptr;
    }

    Value *field_ptr = Builder.CreateStructGEP(
        struct_type, struct_ptr, field_idx, field->field);
    if (out_type) {
      TypeAnnotation field_ann = get_struct_field_type(struct_name, field->field);
      *out_type = get_llvm_type(field_ann);
    }
    return field_ptr;
  }
  return nullptr;
}

// -------------------------------------------------------------------------
// Expression evaluation
// -------------------------------------------------------------------------

Value *CodeGen::eval_expr(Expr *expr, Type *expected_type) {
  // Handle __region_alloc builtin: allocate from the current region
  if (auto *call = dynamic_cast<CallExpr *>(expr)) {
    if (call->callee == "__region_alloc" && !call->callee_expr) {
      if (region_stack.empty()) {
        errs() << "Error: __region_alloc called outside of a region\n";
        return nullptr;
      }
      RegionInfo &ri = region_stack.back();
      Value *size_val = eval_expr(call->args[0].get(), Type::getInt64Ty(Context));
      if (!size_val) return nullptr;

      // Load current bump pointer
      Value *cur = Builder.CreateLoad(PointerType::getUnqual(Context), ri.current_ptr, "region.cur_load");
      // Calculate new pointer: cur + size
      Value *new_ptr = Builder.CreateGEP(Type::getInt8Ty(Context), cur, size_val, "region.new");
      // Check for overflow: if new_ptr > end_ptr, trap
      Value *past_end = Builder.CreateICmpUGT(new_ptr, ri.end_ptr, "region.oob");
      // Store new pointer back
      Builder.CreateStore(new_ptr, ri.current_ptr);

      // If past end, insert a trap
      Function *fn = Builder.GetInsertBlock()->getParent();
      BasicBlock *ok_bb = BasicBlock::Create(Context, "region.ok", fn);
      BasicBlock *trap_bb = BasicBlock::Create(Context, "region.trap", fn);
      Builder.CreateCondBr(past_end, trap_bb, ok_bb);

      Builder.SetInsertPoint(trap_bb);
      // Call __trap or just unreachable
      Builder.CreateUnreachable();

      Builder.SetInsertPoint(ok_bb);
      // Return the old current pointer (start of allocation)
      return cur;
    }
  }

  if (auto *num = dynamic_cast<NumberExpr *>(expr)) {
    if (expected_type && expected_type->isFPOrFPVectorTy())
      return ConstantFP::get(expected_type, num->value);
    Type *t = expected_type ? expected_type : Type::getInt64Ty(Context);
    return ConstantInt::get(t, (int64_t)num->value);
  }

  if (auto *ch = dynamic_cast<CharExpr *>(expr)) {
    Type *t = expected_type ? expected_type : Type::getInt8Ty(Context);
    return ConstantInt::get(t, ch->value);
  }

  if (auto *str = dynamic_cast<StringExpr *>(expr)) {
    return Builder.CreateGlobalString(str->value, "str");
  }

  if (auto *id = dynamic_cast<IdentExpr *>(expr)) {
    auto it = named_values.find(id->name);
    if (it == named_values.end()) {
      auto gi = global_values.find(id->name);
      if (gi != global_values.end()) {
        Type *gv_type = gi->second->getValueType();
        return Builder.CreateLoad(gv_type, gi->second, id->name);
      }
      errs() << "Error: undefined variable '" << id->name << "'\n";
      return nullptr;
    }
    Type *alloc_type = it->second->getAllocatedType();
    if (auto *arr_type = dyn_cast<ArrayType>(alloc_type)) {
      if (expected_type && expected_type->isPointerTy()) {
        Value *indices[] = {
          ConstantInt::get(Type::getInt64Ty(Context), 0),
          ConstantInt::get(Type::getInt64Ty(Context), 0)
        };
        return Builder.CreateGEP(arr_type, it->second, indices, id->name);
      }
    }
    // Check if this variable is an array and we need array-to-slice conversion
    auto ann_it = named_type_anns.find(id->name);
    if (ann_it != named_type_anns.end() && ann_it->second.array_size > 0) {
      TypeAnnotation &var_ann = ann_it->second;
      Type *var_llvm = get_llvm_type(var_ann);
      if (auto *var_arr_type = dyn_cast_or_null<ArrayType>(var_llvm)) {
        // If expected type is the slice type for this element, do conversion
        TypeAnnotation slice_elem = var_ann;
        slice_elem.array_size = 0;
        Type *slice_type = get_slice_type(slice_elem);
        if (expected_type == slice_type) {
          Value *indices[] = {
            ConstantInt::get(Type::getInt64Ty(Context), 0),
            ConstantInt::get(Type::getInt64Ty(Context), 0)
          };
          Value *ptr = Builder.CreateGEP(var_arr_type, it->second, indices, "slice_ptr");
          Value *len = ConstantInt::get(Type::getInt64Ty(Context), var_arr_type->getNumElements());
          Value *undef = UndefValue::get(expected_type);
          Value *with_ptr = Builder.CreateInsertValue(undef, ptr, {0});
          return Builder.CreateInsertValue(with_ptr, len, {1});
        }
      }
    }
    if (expected_type)
      return Builder.CreateLoad(expected_type, it->second, id->name);
    return Builder.CreateLoad(alloc_type, it->second, id->name);
  }

  if (auto *null_expr = dynamic_cast<NullExpr *>(expr)) {
    return ConstantPointerNull::get(cast<PointerType>(expected_type));
  }

  if (auto *ctor = dynamic_cast<ConstructorExpr *>(expr)) {
    // Normalize variant_name: strip enum prefix if present
    std::string ctor_var_short = ctor->variant_name;
    {
      size_t pos = ctor_var_short.rfind("::");
      if (pos != std::string::npos)
        ctor_var_short = ctor_var_short.substr(pos + 2);
    }
    StructType *enum_st = nullptr;
    std::string enum_name;
    int variant_idx = -1;
    for (auto &[ename, st] : enum_types) {
      int idx = get_enum_variant_index(ename, ctor_var_short);
      if (idx >= 0) {
        enum_st = st;
        enum_name = ename;
        variant_idx = idx;
        break;
      }
    }
    if (enum_st) {
      Value *result = UndefValue::get(enum_st);
      Type *tag_type = enum_st->getElementType(0);
      result = Builder.CreateInsertValue(result,
          ConstantInt::get(tag_type, variant_idx), {0});
      StructType *var_type = cast<StructType>(enum_st->getElementType(1 + variant_idx));
      Value *var_data = UndefValue::get(var_type);
      for (size_t fi = 0; fi < ctor->fields.size(); fi++) {
        auto &[field_name, field_expr] = ctor->fields[fi];
        int actual_idx;
        TypeAnnotation field_ann;
        if (field_name.empty()) {
          // Positional: use position index
          actual_idx = (int)fi;
          std::string struct_key = enum_name + "::" + ctor_var_short;
          auto var_it = struct_fields.find(struct_key);
          if (var_it != struct_fields.end() && (size_t)actual_idx < var_it->second.size())
            field_ann = var_it->second[actual_idx].second;
          else
            field_ann = {TypeKind::Int64};
        } else {
          std::string struct_key = enum_name + "::" + ctor_var_short;
          field_ann = get_struct_field_type(struct_key, field_name);
          actual_idx = get_struct_field_index(struct_key, field_name);
        }
        if (actual_idx < 0) {
          errs() << "Error: variant '" << ctor->variant_name
                 << "' has no field '" << field_name << "'\n";
          return nullptr;
        }
        Type *field_type = get_llvm_type(field_ann);
        if (!field_type) field_type = Type::getInt64Ty(Context);
        Value *fv = eval_expr(field_expr.get(), field_type);
        if (!fv) return nullptr;
        var_data = Builder.CreateInsertValue(var_data, fv,
            {(unsigned)actual_idx});
      }
      result = Builder.CreateInsertValue(result, var_data, {1 + (unsigned)variant_idx});
      return result;
    }

    std::string struct_key = ctor->variant_name;
    if (!ctor->type_args.empty())
      struct_key = struct_mangled_name(ctor->variant_name, ctor->type_args);
    auto st_it = struct_types.find(struct_key);
    if (st_it != struct_types.end()) {
      StructType *st = st_it->second;
      Value *result = UndefValue::get(st);
      for (size_t fi = 0; fi < ctor->fields.size(); fi++) {
        auto &[field_name, field_expr] = ctor->fields[fi];
        int actual_idx;
        TypeAnnotation field_ann;
        if (field_name.empty()) {
          actual_idx = (int)fi;
          auto it = struct_fields.find(struct_key);
          if (it != struct_fields.end() && (size_t)actual_idx < it->second.size())
            field_ann = it->second[actual_idx].second;
          else
            field_ann = {TypeKind::Int64};
        } else {
          field_ann = get_struct_field_type(struct_key, field_name);
          actual_idx = get_struct_field_index(struct_key, field_name);
        }
        if (actual_idx < 0) {
          errs() << "Error: struct '" << struct_key
                 << "' has no field '" << field_name << "'\n";
          return nullptr;
        }
        Type *field_type = get_llvm_type(field_ann);
        if (!field_type) field_type = Type::getInt64Ty(Context);
        Value *fv = eval_expr(field_expr.get(), field_type);
        if (!fv) return nullptr;
        result = Builder.CreateInsertValue(result, fv,
            {(unsigned)actual_idx});
      }
      return result;
    }

    errs() << "Error: unknown variant '" << ctor->variant_name
           << "' in constructor expression\n";
    return nullptr;
  }

  if (auto *tup = dynamic_cast<TupleExpr *>(expr)) {
    StructType *st = expected_type ? dyn_cast<StructType>(expected_type) : nullptr;
    if (!st) {
      TypeAnnotation ann = resolve_expr_type(tup);
      st = dyn_cast<StructType>(get_llvm_type(ann));
    }
    if (!st) {
      errs() << "Error: cannot determine tuple type\n";
      return nullptr;
    }
    Value *result = UndefValue::get(st);
    for (size_t i = 0; i < tup->elements.size(); i++) {
      Type *elem_type = st->getElementType(i);
      Value *ev = eval_expr(tup->elements[i].get(), elem_type);
      if (!ev) return nullptr;
      result = Builder.CreateInsertValue(result, ev, {(unsigned)i}, "tup.el");
    }
    return result;
  }

  if (auto *addr = dynamic_cast<AddressOfExpr *>(expr)) {
    // Check for &fn_name — create a function pointer value
    if (auto *id = dynamic_cast<IdentExpr *>(addr->operand.get())) {
      std::string fname = id->name;
      Function *f = M.getFunction(fname);
      if (!f && fname == "main")
        f = M.getFunction("__user_main");
      if (f && !f->isIntrinsic()) {
        llvm::GlobalVariable *gv = get_fnval_wrapper(fname, f);
        if (!gv) {
          errs() << "Error: failed to create function value for '" << fname << "'\n";
          return nullptr;
        }
        // Use constant expression for global scope compatibility
        if (Builder.GetInsertBlock())
          return Builder.CreateBitCast(gv, PointerType::getUnqual(Context),
                                       fname + ".fnval");
        else
          return ConstantExpr::getBitCast(gv, PointerType::getUnqual(Context));
      }
    }
    Type *ptr_type = nullptr;
    Value *lvalue_ptr = get_lvalue_ptr(addr->operand.get(), &ptr_type);
    if (!lvalue_ptr) {
      errs() << "Error: address-of requires an lvalue expression\n";
      return nullptr;
    }
    if (!ptr_type)
      ptr_type = lvalue_ptr->getType();
    return lvalue_ptr;
  }

  if (auto *deref = dynamic_cast<DerefExpr *>(expr)) {
    Value *ptr = nullptr;
    if (auto *id = dynamic_cast<IdentExpr *>(deref->operand.get())) {
      auto it = named_values.find(id->name);
      if (it == named_values.end()) {
        auto gi = global_values.find(id->name);
        if (gi != global_values.end()) {
          ptr = Builder.CreateLoad(gi->second->getValueType(), gi->second, id->name);
        } else {
          errs() << "Error: undefined variable '" << id->name << "'\n";
          return nullptr;
        }
      } else {
        ptr = Builder.CreateLoad(it->second->getAllocatedType(), it->second, id->name);
      }
    } else {
      ptr = eval_expr(deref->operand.get(), PointerType::getUnqual(Context));
    }
    if (!ptr) return nullptr;
    return Builder.CreateLoad(expected_type, ptr);
  }

  if (auto *sub = dynamic_cast<SubscriptExpr *>(expr)) {
    TypeAnnotation base_ann = resolve_expr_type(sub->array.get());
    bool is_array_sub = base_ann.array_size > 0;
    bool is_slice_sub = base_ann.kind == TypeKind::Slice;
    bool is_ptr_sub = !is_array_sub && !is_slice_sub && base_ann.pointer_depth > 0;
    Value *elem_ptr = nullptr;

    if (is_slice_sub) {
      Type *slice_type = get_llvm_type(base_ann);
      Value *slice_val = eval_expr(sub->array.get(), slice_type);
      if (!slice_val) return nullptr;
      Value *ptr = Builder.CreateExtractValue(slice_val, {0}, "slice_ptr");
      Value *index = eval_expr(sub->index.get(), Type::getInt64Ty(Context));
      if (!index) return nullptr;
      TypeAnnotation elem_ann = base_ann.tuple_types[0];
      Type *elem_type = get_llvm_type(elem_ann);
      elem_ptr = Builder.CreateGEP(elem_type, ptr, index, "slice_elem_ptr");
    } else if (is_array_sub) {
      Type *arr_llvm_type = nullptr;
      Value *arr_ptr = get_lvalue_ptr(sub->array.get(), &arr_llvm_type);
      if (!arr_ptr) return nullptr;
      Value *index = eval_expr(sub->index.get(), Type::getInt64Ty(Context));
      if (!index) return nullptr;
      Value *indices[] = {
        ConstantInt::get(Type::getInt64Ty(Context), 0),
        index
      };
      elem_ptr = Builder.CreateGEP(arr_llvm_type, arr_ptr, indices, "elem_ptr");
    } else if (is_ptr_sub) {
      Type *base_llvm = get_llvm_type(base_ann);
      if (!base_llvm) return nullptr;
      Value *arr_ptr = eval_expr(sub->array.get(), base_llvm);
      if (!arr_ptr) return nullptr;
      Value *index = eval_expr(sub->index.get(), Type::getInt64Ty(Context));
      if (!index) return nullptr;
      TypeAnnotation pointee_ann = base_ann;
      pointee_ann.pointer_depth--;
      Type *pointee = get_llvm_type(pointee_ann);
      if (!pointee) pointee = Type::getInt8Ty(Context);
      elem_ptr = Builder.CreateGEP(pointee, arr_ptr, index, "elem_ptr");
    } else {
      errs() << "Error: subscript requires an array or pointer\n";
      return nullptr;
    }
    Type *load_type = expected_type;
    if (is_array_sub)
      load_type = get_llvm_type(TypeAnnotation{base_ann.kind, 0, 0, base_ann.struct_name});
    else if (is_ptr_sub) {
      TypeAnnotation elem_ann = base_ann;
      elem_ann.pointer_depth--;
      load_type = get_llvm_type(elem_ann);
    }
    if (!load_type) load_type = expected_type;
    return Builder.CreateLoad(load_type, elem_ptr);
  }

  if (auto *field = dynamic_cast<FieldAccessExpr *>(expr)) {
    TypeAnnotation base_ann = resolve_expr_type(field->object.get());

    if (base_ann.kind == TypeKind::Tuple) {
      int idx = std::stoi(field->field);
      Type *base_llvm_type = get_llvm_type(base_ann);
      if (!base_llvm_type) return nullptr;
      Value *obj_val = eval_expr(field->object.get(), base_llvm_type);
      if (!obj_val) return nullptr;
      return Builder.CreateExtractValue(obj_val, {(unsigned)idx}, field->field);
    }

    if (base_ann.kind != TypeKind::Struct) {
      errs() << "Error: field access on non-struct expression\n";
      return nullptr;
    }

    Type *base_llvm_type = get_llvm_type(base_ann);
    if (!base_llvm_type) return nullptr;
    Value *obj_val = eval_expr(field->object.get(), base_llvm_type);
    if (!obj_val) return nullptr;

    std::string struct_key = base_ann.struct_name;
    if (!base_ann.type_args.empty())
      struct_key = struct_mangled_name(base_ann.struct_name, base_ann.type_args);
    int field_idx = get_struct_field_index(struct_key, field->field);
    if (field_idx < 0) {
      errs() << "Error: struct '" << struct_key << "' has no field named '"
             << field->field << "'\n";
      return nullptr;
    }

    Type *obj_type = obj_val->getType();
    StructType *st = dyn_cast<StructType>(obj_type);
    if (!st) {
      errs() << "Error: field access target is not a struct type\n";
      return nullptr;
    }

    return Builder.CreateExtractValue(obj_val, {(unsigned)field_idx}, field->field);
  }

  if (auto *match = dynamic_cast<MatchExpr *>(expr)) {
    TypeAnnotation val_ann = resolve_expr_type(match->value.get());
    Type *val_type = get_llvm_type(val_ann);
    if (!val_type) val_type = expected_type;

    Value *val = eval_expr(match->value.get(), val_type);
    if (!val) return nullptr;

    auto saved_named_values = named_values;
    Function *fn = Builder.GetInsertBlock()->getParent();

    AllocaInst *result_alloca = Builder.CreateAlloca(expected_type, nullptr, "match_result");

    BasicBlock *merge_bb = BasicBlock::Create(Context, "match_merge", fn);
    BasicBlock *else_bb = BasicBlock::Create(Context, "match_else", fn);
    BasicBlock *current_bb = Builder.GetInsertBlock();

    for (size_t i = 0; i < match->arms.size(); i++) {
      auto &arm = match->arms[i];
      bool is_last = (i == match->arms.size() - 1);

      BasicBlock *body_bb = BasicBlock::Create(Context, "arm_body", fn);
      BasicBlock *next_bb = is_last ? else_bb
                                     : BasicBlock::Create(Context, "arm_check", fn);

      Builder.SetInsertPoint(current_bb);
      Value *cond = gen_pattern_check(arm.pattern.get(), val, val_ann);
      if (!cond) return nullptr;

      Builder.CreateCondBr(cond, body_bb, next_bb);

      Builder.SetInsertPoint(body_bb);
      named_values = saved_named_values;
      if (!gen_pattern_bind(arm.pattern.get(), val, val_ann)) return nullptr;

      Value *arm_val = eval_expr(arm.expr.get(), expected_type);
      if (!arm_val) return nullptr;
      Builder.CreateStore(arm_val, result_alloca);
      Builder.CreateBr(merge_bb);

      current_bb = next_bb;
    }

    Builder.SetInsertPoint(else_bb);
    errs() << "Warning: non-exhaustive match pattern\n";
    Builder.CreateStore(Constant::getNullValue(expected_type), result_alloca);
    Builder.CreateBr(merge_bb);

    Builder.SetInsertPoint(merge_bb);
    named_values = std::move(saved_named_values);
    return Builder.CreateLoad(expected_type, result_alloca);
  }

  if (auto *arr_lit = dynamic_cast<ArrayLitExpr *>(expr)) {
    if (auto *arr_type = dyn_cast<ArrayType>(expected_type)) {
      return eval_array_literal(arr_lit, arr_type);
    }
    return ConstantPointerNull::get(cast<PointerType>(expected_type));
  }

  if (auto *un = dynamic_cast<UnaryExpr *>(expr)) {
    Value *op = eval_expr(un->operand.get(), expected_type);
    if (!op) return nullptr;
    switch (un->op) {
      case UnaryOp::BitNot:
        return Builder.CreateNot(op);
      case UnaryOp::Neg:
      default:
        if (expected_type && expected_type->isFPOrFPVectorTy())
          return Builder.CreateFNeg(op);
        return Builder.CreateNeg(op);
    }
  }

  if (auto *bin = dynamic_cast<BinaryExpr *>(expr)) {
    Value *l = eval_expr(bin->left.get(), expected_type);
    Value *r = eval_expr(bin->right.get(), expected_type);
    if (!l || !r) return nullptr;

    bool is_float = expected_type && expected_type->isFPOrFPVectorTy();

    if ((bin->op == BinOp::Add || bin->op == BinOp::Sub) &&
        (l->getType()->isPointerTy() || r->getType()->isPointerTy())) {
      Type *pointee = nullptr;
      Value *ptr_val = nullptr;
      Value *idx_val = nullptr;
      if (l->getType()->isPointerTy() && !r->getType()->isPointerTy()) {
        ptr_val = l; idx_val = r;
        if (bin->op == BinOp::Sub) idx_val = Builder.CreateNeg(r);
        TypeAnnotation ann = resolve_expr_type(bin->left.get());
        if (ann.pointer_depth > 0) {
          ann.pointer_depth--;
          pointee = get_llvm_type(ann);
        }
      } else if (r->getType()->isPointerTy() && !l->getType()->isPointerTy()) {
        ptr_val = r; idx_val = l;
        TypeAnnotation ann = resolve_expr_type(bin->right.get());
        if (ann.pointer_depth > 0) {
          ann.pointer_depth--;
          pointee = get_llvm_type(ann);
        }
      }
      if (pointee && ptr_val && idx_val)
        return Builder.CreateGEP(pointee, ptr_val, idx_val, "ptr.offset");
      if (!pointee && ptr_val && idx_val)
        return Builder.CreateGEP(Type::getInt8Ty(Context), ptr_val, idx_val, "ptr.offset");
    }

    TypeAnnotation l_ann = resolve_expr_type(bin->left.get());
    bool is_unsigned = is_unsigned_type(l_ann.kind);

    switch (bin->op) {
      case BinOp::Add: return is_float ? Builder.CreateFAdd(l, r) : Builder.CreateAdd(l, r);
      case BinOp::Sub: return is_float ? Builder.CreateFSub(l, r) : Builder.CreateSub(l, r);
      case BinOp::Mul: return is_float ? Builder.CreateFMul(l, r) : Builder.CreateMul(l, r);
      case BinOp::Div: return is_float ? Builder.CreateFDiv(l, r) : (is_unsigned ? Builder.CreateUDiv(l, r) : Builder.CreateSDiv(l, r));
      case BinOp::Mod: return is_unsigned ? Builder.CreateURem(l, r) : Builder.CreateSRem(l, r);
      case BinOp::Eq: {
        Value *cmp = Builder.CreateICmpEQ(l, r);
        if (expected_type && !expected_type->isIntegerTy(1))
          return Builder.CreateZExt(cmp, expected_type);
        return cmp;
      }
      case BinOp::Ne: {
        Value *cmp = Builder.CreateICmpNE(l, r);
        if (expected_type && !expected_type->isIntegerTy(1))
          return Builder.CreateZExt(cmp, expected_type);
        return cmp;
      }
      case BinOp::Less: {
        Value *cmp = is_unsigned ? Builder.CreateICmpULT(l, r) : Builder.CreateICmpSLT(l, r);
        if (expected_type && !expected_type->isIntegerTy(1))
          return Builder.CreateZExt(cmp, expected_type);
        return cmp;
      }
      case BinOp::Greater: {
        Value *cmp = is_unsigned ? Builder.CreateICmpUGT(l, r) : Builder.CreateICmpSGT(l, r);
        if (expected_type && !expected_type->isIntegerTy(1))
          return Builder.CreateZExt(cmp, expected_type);
        return cmp;
      }
      case BinOp::Le: {
        Value *cmp = is_unsigned ? Builder.CreateICmpULE(l, r) : Builder.CreateICmpSLE(l, r);
        if (expected_type && !expected_type->isIntegerTy(1))
          return Builder.CreateZExt(cmp, expected_type);
        return cmp;
      }
      case BinOp::Ge: {
        Value *cmp = is_unsigned ? Builder.CreateICmpUGE(l, r) : Builder.CreateICmpSGE(l, r);
        if (expected_type && !expected_type->isIntegerTy(1))
          return Builder.CreateZExt(cmp, expected_type);
        return cmp;
      }
      case BinOp::And: {
        Function *fn = Builder.GetInsertBlock()->getParent();
        BasicBlock *rhs_bb = BasicBlock::Create(Context, "and.rhs", fn);
        BasicBlock *merge_bb = BasicBlock::Create(Context, "and.merge", fn);

        AllocaInst *result_alloca = Builder.CreateAlloca(Type::getInt1Ty(Context), nullptr, "and.result");
        Builder.CreateStore(ConstantInt::getFalse(Context), result_alloca);

        Value *l_bool = Builder.CreateICmpNE(l, ConstantInt::get(l->getType(), 0));
        Builder.CreateCondBr(l_bool, rhs_bb, merge_bb);

        Builder.SetInsertPoint(rhs_bb);
        Value *r_val = eval_expr(bin->right.get(), expected_type);
        if (!r_val) return nullptr;
        Value *r_bool = Builder.CreateICmpNE(r_val, ConstantInt::get(r_val->getType(), 0));
        Builder.CreateStore(r_bool, result_alloca);
        Builder.CreateBr(merge_bb);

        Builder.SetInsertPoint(merge_bb);
        Value *result = Builder.CreateLoad(Type::getInt1Ty(Context), result_alloca);
        if (expected_type && !expected_type->isIntegerTy(1))
          return Builder.CreateZExt(result, expected_type);
        return result;
      }
      case BinOp::Or: {
        Value *lb = Builder.CreateICmpNE(l, ConstantInt::get(l->getType(), 0));
        Value *rb = Builder.CreateICmpNE(r, ConstantInt::get(r->getType(), 0));
        Value *cmp = Builder.CreateOr(lb, rb);
        if (expected_type && !expected_type->isIntegerTy(1))
          return Builder.CreateZExt(cmp, expected_type);
        return cmp;
      }
      case BinOp::Shr:
        return is_unsigned ? Builder.CreateLShr(l, r) : Builder.CreateAShr(l, r);
      case BinOp::Shl:
        return Builder.CreateShl(l, r);
      case BinOp::BitAnd:
        return Builder.CreateAnd(l, r);
      case BinOp::BitOr:
        return Builder.CreateOr(l, r);
      case BinOp::Xor:
        return Builder.CreateXor(l, r);
    }
  }

  if (auto *closure = dynamic_cast<ClosureExpr *>(expr)) {
    // --- Discover captured variables ---
    std::unordered_set<std::string> param_names;
    for (auto &p : closure->params)
      param_names.insert(p.name);

    std::vector<std::string> captures;
    discover_captures_in_body(closure->body, param_names, captures);

    int id = closure_counter++;
    std::string fn_name = "__closure_" + std::to_string(id);
    std::string struct_name = "__closure_s_" + std::to_string(id);

    // --- Build capture types ---
    std::vector<Type *> cap_types;
    std::vector<TypeAnnotation> cap_anns;
    for (auto &cap_name : captures) {
      auto ann_it = named_type_anns.find(cap_name);
      if (ann_it != named_type_anns.end()) {
        cap_types.push_back(get_llvm_type(ann_it->second));
        cap_anns.push_back(ann_it->second);
      } else {
        auto kt = named_types.find(cap_name);
        if (kt != named_types.end()) {
          cap_types.push_back(get_llvm_type(kt->second));
          cap_anns.push_back({kt->second});
        } else {
          cap_types.push_back(Type::getInt64Ty(Context));
          cap_anns.push_back({TypeKind::Int64});
        }
      }
    }

    // --- Function type: R (i8*, T1, T2, ...) ---
    std::vector<Type *> fn_param_types;
    fn_param_types.push_back(PointerType::getUnqual(Context)); // captures context
    for (auto &p : closure->params)
      fn_param_types.push_back(get_llvm_type(p.type_ann));

    Type *ret_type = get_llvm_type(closure->return_type);
    if (!ret_type) ret_type = Type::getInt64Ty(Context);

    FunctionType *FT = FunctionType::get(ret_type, fn_param_types, false);
    Function *helper = Function::Create(FT, Function::InternalLinkage, fn_name, &M);

    // --- Closure struct type: { i8*, caps... } ---
    std::vector<Type *> closure_elem_types;
    closure_elem_types.push_back(PointerType::getUnqual(Context));
    for (auto *ct : cap_types)
      closure_elem_types.push_back(ct);
    StructType *closure_st = StructType::create(Context, closure_elem_types, struct_name);

    // --- Emit helper function body ---
    BasicBlock *saved_insert_bb = Builder.GetInsertBlock();
    {
      BasicBlock *BB = BasicBlock::Create(Context, "entry", helper);
      Builder.SetInsertPoint(BB);

      auto saved_values = std::move(named_values);
      auto saved_types = std::move(named_types);
      auto saved_type_anns = std::move(named_type_anns);
      named_values.clear();
      named_types.clear();
      named_type_anns.clear();

      Value *context_ptr = helper->getArg(0);
      Value *closure_ctx = Builder.CreateBitCast(context_ptr,
          PointerType::getUnqual(Context), "closure_ctx");

      // Load captures from context into local allocas
      for (size_t ci = 0; ci < captures.size(); ci++) {
        Value *cap_ptr = Builder.CreateGEP(closure_st, closure_ctx,
            {ConstantInt::get(Type::getInt32Ty(Context), 0),
             ConstantInt::get(Type::getInt32Ty(Context), 1 + (int)ci)},
            captures[ci] + ".ptr");
        AllocaInst *cap_alloca = Builder.CreateAlloca(cap_types[ci], nullptr, captures[ci]);
        Value *cap_val = Builder.CreateLoad(cap_types[ci], cap_ptr, captures[ci]);
        Builder.CreateStore(cap_val, cap_alloca);
        named_values[captures[ci]] = cap_alloca;
        named_types[captures[ci]] = cap_anns[ci].kind;
        if (cap_anns[ci].pointer_depth > 0 || cap_anns[ci].array_size > 0 ||
            cap_anns[ci].kind == TypeKind::Struct || cap_anns[ci].kind == TypeKind::Enum ||
            cap_anns[ci].kind == TypeKind::Tuple || cap_anns[ci].kind == TypeKind::Slice)
          named_type_anns[captures[ci]] = cap_anns[ci];
      }

      // Store params
      size_t ai = 1;
      for (size_t pi = 0; pi < closure->params.size(); pi++, ai++) {
        Value *arg_val = helper->getArg(ai);
        arg_val->setName(closure->params[pi].name);
        Type *param_llvm_type = get_llvm_type(closure->params[pi].type_ann);
        AllocaInst *param_alloca = Builder.CreateAlloca(param_llvm_type, nullptr,
            closure->params[pi].name);
        Builder.CreateStore(arg_val, param_alloca);
        named_values[closure->params[pi].name] = param_alloca;
        named_types[closure->params[pi].name] = closure->params[pi].type_ann.kind;
        auto &ta = closure->params[pi].type_ann;
        if (ta.kind == TypeKind::Fn || ta.kind == TypeKind::Struct || ta.kind == TypeKind::Enum ||
            ta.kind == TypeKind::Tuple || ta.kind == TypeKind::Slice ||
            ta.pointer_depth > 0 || ta.array_size > 0)
          named_type_anns[closure->params[pi].name] = ta;
      }

      for (auto &stmt : closure->body) {
        if (!gen_stmt(stmt.get())) {
          named_values = std::move(saved_values);
          named_types = std::move(saved_types);
          named_type_anns = std::move(saved_type_anns);
          return nullptr;
        }
      }

      if (!Builder.GetInsertBlock()->getTerminator()) {
        if (ret_type->isVoidTy())
          Builder.CreateRetVoid();
        else if (ret_type->isIntegerTy())
          Builder.CreateRet(ConstantInt::get(ret_type, 0));
        else if (ret_type->isFPOrFPVectorTy())
          Builder.CreateRet(ConstantFP::get(ret_type, 0.0));
        else
          Builder.CreateRet(ConstantPointerNull::get(cast<PointerType>(ret_type)));
      }

      named_values = std::move(saved_values);
      named_types = std::move(saved_types);
      named_type_anns = std::move(saved_type_anns);
    }

    if (saved_insert_bb)
      Builder.SetInsertPoint(saved_insert_bb);

    // --- Build closure struct at lambda site ---
    AllocaInst *closure_alloca = Builder.CreateAlloca(closure_st, nullptr, "lambda");

    Value *fn_ptr = helper;
    if (fn_ptr->getType() != PointerType::getUnqual(Context))
      fn_ptr = Builder.CreateBitCast(fn_ptr, PointerType::getUnqual(Context), "fn_ptr");
    Value *fn_gep = Builder.CreateGEP(closure_st, closure_alloca,
        {ConstantInt::get(Type::getInt32Ty(Context), 0),
         ConstantInt::get(Type::getInt32Ty(Context), 0)},
        "fn.ptr");
    Builder.CreateStore(fn_ptr, fn_gep);

    for (size_t ci = 0; ci < captures.size(); ci++) {
      auto cap_val_it = named_values.find(captures[ci]);
      if (cap_val_it != named_values.end()) {
        Value *cap_val = Builder.CreateLoad(cap_types[ci], cap_val_it->second,
            captures[ci]);
        Value *cap_gep = Builder.CreateGEP(closure_st, closure_alloca,
            {ConstantInt::get(Type::getInt32Ty(Context), 0),
             ConstantInt::get(Type::getInt32Ty(Context), 1 + (int)ci)},
            captures[ci] + ".cap");
        Builder.CreateStore(cap_val, cap_gep);
      }
    }

    return Builder.CreateBitCast(closure_alloca, PointerType::getUnqual(Context), "lambda.ptr");
  }

  if (auto *call = dynamic_cast<CallExpr *>(expr)) {
    // Expression-based call (e.g. closure variable / lambda call)
    if (call->callee_expr) {
      TypeAnnotation callee_ann = resolve_expr_type(call->callee_expr.get());
      Type *callee_llvm = get_llvm_type(callee_ann);
      if (!callee_llvm) callee_llvm = PointerType::getUnqual(Context);

      Value *closure_ptr = eval_expr(call->callee_expr.get(), callee_llvm);
      if (!closure_ptr) return nullptr;

      // Load fn_ptr from first field of closure struct: *(i8**)closure_ptr
      Value *fn_ptr_ptr = Builder.CreateBitCast(closure_ptr,
          PointerType::getUnqual(Context), "fn_ptr.ptr");
      Value *fn_i8 = Builder.CreateLoad(PointerType::getUnqual(Context),
          fn_ptr_ptr, "fn_ptr");

      Type *ret_type = get_llvm_type(callee_ann.tuple_types.back());
      if (!ret_type) ret_type = Type::getInt64Ty(Context);

      std::vector<Type *> call_param_types;
      call_param_types.push_back(PointerType::getUnqual(Context));
      for (size_t pi = 0; pi + 1 < callee_ann.tuple_types.size(); pi++)
        call_param_types.push_back(get_llvm_type(callee_ann.tuple_types[pi]));

      FunctionType *call_ft = FunctionType::get(ret_type, call_param_types, false);
      Value *fn = Builder.CreateBitCast(fn_i8, PointerType::getUnqual(Context), "fn");

      std::vector<Value *> args;
      args.push_back(closure_ptr);
      for (size_t i = 0; i < call->args.size(); i++) {
        Type *arg_type = call_param_types.size() > i + 1
            ? call_param_types[i + 1] : Type::getInt64Ty(Context);
        Value *arg = eval_expr(call->args[i].get(), arg_type);
        if (!arg) return nullptr;
        args.push_back(arg);
      }

      return Builder.CreateCall(call_ft, fn, args, "call");
    }

    std::string actual_callee = call->callee;
    if (!call->type_args.empty()) {
      actual_callee = mangle_name(call->callee, call->type_args);
      if (!M.getFunction(actual_callee)) {
        auto it = generic_templates.find(call->callee);
        if (it == generic_templates.end()) {
          errs() << "Error: '" << call->callee << "' is not a generic function\n";
          return nullptr;
        }
        if (call->type_args.size() != it->second->type_params.size()) {
          errs() << "Error: wrong number of type arguments for '" << call->callee << "'\n";
          return nullptr;
        }
        if (!monomorphize_and_codegen(it->second, call->type_args, actual_callee))
          return nullptr;
      }
    }

    Function *callee = M.getFunction(actual_callee);
    if (!callee) {
      // Try package-prefixed name (for recursive/cross-calls within packages)
      BasicBlock *current_bb = Builder.GetInsertBlock();
      if (current_bb) {
        std::string cur_fn_name = current_bb->getParent()->getName().str();
        size_t colon = cur_fn_name.rfind("::");
        if (colon != std::string::npos) {
          std::string prefix = cur_fn_name.substr(0, colon);
          std::string prefixed = prefix + "::" + actual_callee;
          callee = M.getFunction(prefixed);
        }
      }
    }
    if (!callee) {
      // Not a known function — check if it's a variable holding a function value
      auto fnv_it = named_values.find(actual_callee);
      auto fnt_it = named_type_anns.find(actual_callee);
      if (fnv_it != named_values.end() && fnt_it != named_type_anns.end() &&
          fnt_it->second.kind == TypeKind::Fn) {
        // Treat as expression-based call on a function-typed variable
        TypeAnnotation &callee_ann = fnt_it->second;
        Type *callee_llvm = get_llvm_type(callee_ann);
        if (!callee_llvm) callee_llvm = PointerType::getUnqual(Context);

        Value *closure_ptr = Builder.CreateLoad(callee_llvm, fnv_it->second,
                                                actual_callee);
        if (!closure_ptr) return nullptr;

        // Load fn_ptr from first field
        Value *fn_ptr_ptr = Builder.CreateBitCast(closure_ptr,
            PointerType::getUnqual(Context), "fn_ptr.ptr");
        Value *fn_i8 = Builder.CreateLoad(PointerType::getUnqual(Context),
            fn_ptr_ptr, "fn_ptr");

        Type *ret_type = get_llvm_type(callee_ann.tuple_types.back());
        if (!ret_type) ret_type = Type::getInt64Ty(Context);

        std::vector<Type *> call_param_types;
        call_param_types.push_back(PointerType::getUnqual(Context));
        for (size_t pi = 0; pi + 1 < callee_ann.tuple_types.size(); pi++)
          call_param_types.push_back(get_llvm_type(callee_ann.tuple_types[pi]));

        FunctionType *call_ft = FunctionType::get(ret_type, call_param_types, false);
        Value *fn = Builder.CreateBitCast(fn_i8, PointerType::getUnqual(Context), "fn");

        std::vector<Value *> args_v;
        args_v.push_back(closure_ptr);
        for (size_t i = 0; i < call->args.size(); i++) {
          Type *arg_type = call_param_types.size() > i + 1
              ? call_param_types[i + 1] : Type::getInt64Ty(Context);
          Value *arg = eval_expr(call->args[i].get(), arg_type);
          if (!arg) return nullptr;
          args_v.push_back(arg);
        }

        return Builder.CreateCall(call_ft, fn, args_v, "call");
      }
      errs() << "Error: undefined function '" << call->callee << "'\n";
      return nullptr;
    }
    size_t fixed_params = callee->arg_size();
    if (callee->isVarArg()) {
      if (call->args.size() < fixed_params) {
        errs() << "Error: too few arguments to '" << call->callee << "'\n";
        return nullptr;
      }
    } else if (fixed_params != call->args.size()) {
      errs() << "Error: wrong number of arguments to '" << call->callee << "'\n";
      return nullptr;
    }
    std::vector<Value *> args;
    for (size_t i = 0; i < call->args.size(); i++) {
      Type *param_type;
      if (i < fixed_params) {
        param_type = callee->getArg(i)->getType();
      } else {
        if (dynamic_cast<StringExpr *>(call->args[i].get())) {
          param_type = PointerType::getUnqual(Context);
        } else {
          param_type = Type::getInt64Ty(Context);
        }
      }
      Value *arg = eval_expr(call->args[i].get(), param_type);
      if (!arg) return nullptr;
      args.push_back(arg);
    }
    return Builder.CreateCall(callee, args);
  }

  if (auto *mcall = dynamic_cast<MethodCallExpr *>(expr)) {
    TypeAnnotation obj_type = resolve_expr_type(mcall->object.get());
    std::string type_name;
    if (obj_type.kind == TypeKind::Struct)
      type_name = obj_type.struct_name;
    else if (obj_type.kind == TypeKind::Enum)
      type_name = obj_type.struct_name;
    else {
      errs() << "Error: method call on non-struct/enum type\n";
      return nullptr;
    }
    auto it = impl_methods.find({type_name, mcall->method_name});
    if (it == impl_methods.end()) {
      errs() << "Error: type '" << type_name << "' has no method named '"
             << mcall->method_name << "'\n";
      return nullptr;
    }
    Function *callee = M.getFunction(it->second);
    if (!callee) {
      errs() << "Error: method '" << mcall->method_name << "' for type '"
             << type_name << "' has no compiled function\n";
      return nullptr;
    }

    Type *obj_llvm_type = get_llvm_type(obj_type);
    Value *obj_val = eval_expr(mcall->object.get(), obj_llvm_type);
    if (!obj_val) return nullptr;

    if (callee->arg_size() != 1 + mcall->args.size()) {
      errs() << "Error: wrong number of arguments for method '"
             << mcall->method_name << "' (expected "
             << (callee->arg_size() - 1) << ", got " << mcall->args.size() << ")\n";
      return nullptr;
    }

    std::vector<Value *> margs;
    margs.push_back(obj_val); // self
    for (size_t i = 0; i < mcall->args.size(); i++) {
      Type *param_type = callee->getArg(i + 1)->getType();
      Value *arg = eval_expr(mcall->args[i].get(), param_type);
      if (!arg) return nullptr;
      margs.push_back(arg);
    }
    return Builder.CreateCall(callee, margs);
  }

  if (auto *atm = dynamic_cast<AtomicExpr *>(expr)) {
    if (atm->op == AtomicOp::Fence) {
      Builder.CreateFence(AtomicOrdering::SequentiallyConsistent);
      return ConstantInt::get(Type::getInt64Ty(Context), 0);
    }

    if (atm->args.empty()) {
      errs() << "Error: atomic operation needs at least a pointer argument\n";
      return nullptr;
    }
    TypeAnnotation ptr_ann = resolve_expr_type(atm->args[0].get());
    if (ptr_ann.pointer_depth < 1) {
      errs() << "Error: first argument of atomic must be a pointer (&var or ptr)\n";
      return nullptr;
    }
    ptr_ann.pointer_depth--;
    Type *val_type = get_llvm_type(ptr_ann);
    if (!val_type || (!val_type->isIntegerTy() && atm->op != AtomicOp::Xchg)) {
      errs() << "Error: atomic operations require integer pointer types\n";
      return nullptr;
    }

    Value *ptr = eval_expr(atm->args[0].get(), PointerType::getUnqual(Context));
    if (!ptr) return nullptr;

    if (atm->op == AtomicOp::CmpXchg) {
      if (atm->args.size() < 3) {
        errs() << "Error: atomic cas needs 3 arguments (ptr, expected, desired)\n";
        return nullptr;
      }
      Value *expected = eval_expr(atm->args[1].get(), val_type);
      Value *desired = eval_expr(atm->args[2].get(), val_type);
      if (!expected || !desired) return nullptr;
      Value *pair = Builder.CreateAtomicCmpXchg(
          ptr, expected, desired, MaybeAlign(),
          AtomicOrdering::SequentiallyConsistent,
          AtomicOrdering::SequentiallyConsistent);
      return Builder.CreateExtractValue(pair, {0}, "cmpxchg.old");
    }

    if (atm->args.size() < 2) {
      errs() << "Error: atomic " << (atm->op == AtomicOp::Xchg ? "xchg" : "rmw")
             << " needs 2 arguments (ptr, value)\n";
      return nullptr;
    }
    Value *val = eval_expr(atm->args[1].get(), val_type);
    if (!val) return nullptr;

    AtomicRMWInst::BinOp rmw_op;
    switch (atm->op) {
      case AtomicOp::Xchg: rmw_op = AtomicRMWInst::Xchg; break;
      case AtomicOp::Add:  rmw_op = AtomicRMWInst::Add; break;
      case AtomicOp::Sub:  rmw_op = AtomicRMWInst::Sub; break;
      case AtomicOp::And:  rmw_op = AtomicRMWInst::And; break;
      case AtomicOp::Or:   rmw_op = AtomicRMWInst::Or; break;
      case AtomicOp::Xor:  rmw_op = AtomicRMWInst::Xor; break;
      default:
        errs() << "Error: unsupported atomic operation\n";
        return nullptr;
    }
    return Builder.CreateAtomicRMW(rmw_op, ptr, val, MaybeAlign(),
                                    AtomicOrdering::SequentiallyConsistent);
  }

  if (auto *asm_ = dynamic_cast<AsmExpr *>(expr)) {
    FunctionType *FT = FunctionType::get(Type::getVoidTy(Context), false);
    InlineAsm *IA = InlineAsm::get(FT, asm_->asm_code, "", true, false);
    Builder.CreateCall(IA);
    return ConstantInt::get(Type::getInt64Ty(Context), 0);
  }

  if (auto *assign = dynamic_cast<AssignExpr *>(expr)) {
    Value *val = eval_expr(assign->value.get(), expected_type);
    if (!val) return nullptr;

    Type *target_type = nullptr;
    Value *target_ptr = get_lvalue_ptr(assign->target.get(), &target_type);
    if (!target_ptr) {
      errs() << "Error: invalid assignment target\n";
      return nullptr;
    }
    Builder.CreateStore(val, target_ptr);
    return val;
  }

  if (auto *ifexpr = dynamic_cast<IfExpr *>(expr)) {
    Function *fn = Builder.GetInsertBlock()->getParent();

    Value *cond = eval_expr(ifexpr->condition.get(), nullptr);
    if (!cond) return nullptr;
    if (!cond->getType()->isIntegerTy(1))
      cond = Builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0));

    BasicBlock *then_bb = BasicBlock::Create(Context, "if.then", fn);
    BasicBlock *else_bb = BasicBlock::Create(Context, "if.else", fn);
    BasicBlock *merge_bb = BasicBlock::Create(Context, "if.merge", fn);

    AllocaInst *result = Builder.CreateAlloca(expected_type, nullptr, "if_result");

    Builder.CreateCondBr(cond, then_bb, else_bb);

    Builder.SetInsertPoint(then_bb);
    Value *then_val = eval_expr(ifexpr->then_expr.get(), expected_type);
    if (!then_val) return nullptr;
    Builder.CreateStore(then_val, result);
    Builder.CreateBr(merge_bb);

    Builder.SetInsertPoint(else_bb);
    Value *else_val = eval_expr(ifexpr->else_expr.get(), expected_type);
    if (!else_val) return nullptr;
    Builder.CreateStore(else_val, result);
    Builder.CreateBr(merge_bb);

    Builder.SetInsertPoint(merge_bb);
    return Builder.CreateLoad(expected_type, result, "if_val");
  }

  if (auto *compound = dynamic_cast<CompoundAssignExpr *>(expr)) {
    Type *target_type = nullptr;
    Value *target_ptr = get_lvalue_ptr(compound->target.get(), &target_type);
    if (!target_ptr) {
      errs() << "Error: invalid compound assignment target\n";
      return nullptr;
    }
    Value *current = Builder.CreateLoad(target_type, target_ptr);
    Value *rhs = eval_expr(compound->value.get(), target_type);
    if (!rhs) return nullptr;
    TypeAnnotation target_ann = resolve_expr_type(compound->target.get());
    bool is_unsigned = is_unsigned_type(target_ann.kind);
    Value *result;
    switch (compound->op) {
      case BinOp::Add: result = Builder.CreateAdd(current, rhs); break;
      case BinOp::Sub: result = Builder.CreateSub(current, rhs); break;
      case BinOp::Mul: result = Builder.CreateMul(current, rhs); break;
      case BinOp::Div: result = is_unsigned ? Builder.CreateUDiv(current, rhs) : Builder.CreateSDiv(current, rhs); break;
      case BinOp::Mod: result = is_unsigned ? Builder.CreateURem(current, rhs) : Builder.CreateSRem(current, rhs); break;
      case BinOp::BitAnd: result = Builder.CreateAnd(current, rhs); break;
      case BinOp::BitOr: result = Builder.CreateOr(current, rhs); break;
      case BinOp::Xor: result = Builder.CreateXor(current, rhs); break;
      case BinOp::Shl: result = Builder.CreateShl(current, rhs); break;
      case BinOp::Shr: result = is_unsigned ? Builder.CreateLShr(current, rhs) : Builder.CreateAShr(current, rhs); break;
      default:
        errs() << "Error: unsupported operator for compound assignment\n";
        return nullptr;
    }
    Builder.CreateStore(result, target_ptr);
    return result;
  }

  errs() << "Error: unknown expression type\n";
  return nullptr;
}

// -------------------------------------------------------------------------
// Capture discovery helpers for closures
// -------------------------------------------------------------------------

void CodeGen::discover_captures(Expr *expr,
                                 const std::unordered_set<std::string> &param_names,
                                 std::vector<std::string> &captures) {
  if (!expr) return;
  if (auto *id = dynamic_cast<IdentExpr *>(expr)) {
    if (param_names.find(id->name) == param_names.end() &&
        named_values.find(id->name) != named_values.end()) {
      if (std::find(captures.begin(), captures.end(), id->name) == captures.end())
        captures.push_back(id->name);
    }
    return;
  }
  if (auto *bin = dynamic_cast<BinaryExpr *>(expr)) {
    discover_captures(bin->left.get(), param_names, captures);
    discover_captures(bin->right.get(), param_names, captures);
    return;
  }
  if (auto *unary = dynamic_cast<UnaryExpr *>(expr)) {
    discover_captures(unary->operand.get(), param_names, captures);
    return;
  }
  if (auto *call = dynamic_cast<CallExpr *>(expr)) {
    if (call->callee_expr)
      discover_captures(call->callee_expr.get(), param_names, captures);
    for (auto &arg : call->args)
      discover_captures(arg.get(), param_names, captures);
    return;
  }
  if (auto *assign = dynamic_cast<AssignExpr *>(expr)) {
    discover_captures(assign->target.get(), param_names, captures);
    discover_captures(assign->value.get(), param_names, captures);
    return;
  }
  if (auto *compound = dynamic_cast<CompoundAssignExpr *>(expr)) {
    discover_captures(compound->target.get(), param_names, captures);
    discover_captures(compound->value.get(), param_names, captures);
    return;
  }
  if (auto *field = dynamic_cast<FieldAccessExpr *>(expr)) {
    discover_captures(field->object.get(), param_names, captures);
    return;
  }
  if (auto *mcall = dynamic_cast<MethodCallExpr *>(expr)) {
    discover_captures(mcall->object.get(), param_names, captures);
    for (auto &arg : mcall->args)
      discover_captures(arg.get(), param_names, captures);
    return;
  }
  if (auto *deref = dynamic_cast<DerefExpr *>(expr)) {
    discover_captures(deref->operand.get(), param_names, captures);
    return;
  }
  if (auto *addr = dynamic_cast<AddressOfExpr *>(expr)) {
    discover_captures(addr->operand.get(), param_names, captures);
    return;
  }
  if (auto *sub = dynamic_cast<SubscriptExpr *>(expr)) {
    discover_captures(sub->array.get(), param_names, captures);
    discover_captures(sub->index.get(), param_names, captures);
    return;
  }
  if (auto *arr = dynamic_cast<ArrayLitExpr *>(expr)) {
    for (auto &el : arr->elements)
      discover_captures(el.get(), param_names, captures);
    return;
  }
  if (auto *tup = dynamic_cast<TupleExpr *>(expr)) {
    for (auto &el : tup->elements)
      discover_captures(el.get(), param_names, captures);
    return;
  }
  if (auto *ctor = dynamic_cast<ConstructorExpr *>(expr)) {
    for (auto &[_, fexpr] : ctor->fields)
      discover_captures(fexpr.get(), param_names, captures);
    return;
  }
  if (auto *match = dynamic_cast<MatchExpr *>(expr)) {
    discover_captures(match->value.get(), param_names, captures);
    for (auto &arm : match->arms)
      discover_captures(arm.expr.get(), param_names, captures);
    return;
  }
  if (auto *ifexpr = dynamic_cast<IfExpr *>(expr)) {
    discover_captures(ifexpr->condition.get(), param_names, captures);
    discover_captures(ifexpr->then_expr.get(), param_names, captures);
    discover_captures(ifexpr->else_expr.get(), param_names, captures);
    return;
  }
}

void CodeGen::discover_captures_in_body(
    const std::vector<std::unique_ptr<Stmt>> &body,
    const std::unordered_set<std::string> &param_names,
    std::vector<std::string> &captures) {
  for (auto &stmt : body) {
    if (auto *exprs = dynamic_cast<ExprStmt *>(stmt.get())) {
      discover_captures(exprs->expr.get(), param_names, captures);
    } else if (auto *let = dynamic_cast<LetStmt *>(stmt.get())) {
      discover_captures(let->init_expr.get(), param_names, captures);
    } else if (auto *ret = dynamic_cast<ReturnStmt *>(stmt.get())) {
      discover_captures(ret->value.get(), param_names, captures);
    } else if (auto *ifs = dynamic_cast<IfStmt *>(stmt.get())) {
      discover_captures(ifs->condition.get(), param_names, captures);
      discover_captures_in_body(ifs->then_branch, param_names, captures);
      discover_captures_in_body(ifs->else_branch, param_names, captures);
    } else if (auto *for_s = dynamic_cast<ForStmt *>(stmt.get())) {
      if (for_s->init) {
        if (auto *let_init = dynamic_cast<LetStmt *>(for_s->init.get()))
          discover_captures(let_init->init_expr.get(), param_names, captures);
      }
      discover_captures(for_s->condition.get(), param_names, captures);
      discover_captures(for_s->update.get(), param_names, captures);
      discover_captures_in_body(for_s->body, param_names, captures);
    }
  }
}
