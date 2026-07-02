#include "codegen.h"

#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// -------------------------------------------------------------------------
// Pattern matching helpers
// -------------------------------------------------------------------------

Value *CodeGen::gen_pattern_check(Pattern *pat, Value *val,
                                   const TypeAnnotation &val_ann) {
  if (auto *wc = dynamic_cast<WildcardPattern *>(pat)) {
    return ConstantInt::getTrue(Context);
  }

  if (auto *lit = dynamic_cast<LiteralPattern *>(pat)) {
    Value *lit_val = eval_expr(lit->value.get(), val->getType());
    if (!lit_val) return nullptr;
    if (val->getType()->isIntegerTy())
      return Builder.CreateICmpEQ(val, lit_val);
    if (val->getType()->isFPOrFPVectorTy())
      return Builder.CreateFCmpOEQ(val, lit_val);
    if (val->getType()->isPointerTy())
      return Builder.CreateICmpEQ(val, lit_val);
    errs() << "Error: unsupported type for literal pattern\n";
    return nullptr;
  }

  if (auto *var = dynamic_cast<VariablePattern *>(pat)) {
    bool is_enum = (val_ann.kind == TypeKind::Enum) ||
                   (val_ann.kind == TypeKind::Struct &&
                    enum_types.find(val_ann.struct_name) != enum_types.end());
    if (is_enum) {
      int vi = get_enum_variant_index(val_ann.struct_name, var->name);
      if (vi >= 0) {
        Value *tag = Builder.CreateExtractValue(val, {0}, "tag");
        return Builder.CreateICmpEQ(tag,
            ConstantInt::get(Type::getInt64Ty(Context), vi));
      }
    }
    return ConstantInt::getTrue(Context);
  }

  if (auto *sp = dynamic_cast<StructPattern *>(pat)) {
    bool is_variant = (val_ann.kind == TypeKind::Enum) ||
                      (val_ann.kind == TypeKind::Struct &&
                       enum_types.find(val_ann.struct_name) != enum_types.end());
    int variant_data_idx = -1;

    if (is_variant) {
      int vi = get_enum_variant_index(val_ann.struct_name, sp->struct_name);
      if (vi < 0) {
        errs() << "Error: '" << sp->struct_name << "' is not a variant of enum '"
               << val_ann.struct_name << "'\n";
        return nullptr;
      }
      variant_data_idx = 1 + vi;

      Value *tag = Builder.CreateExtractValue(val, {0}, "tag");
      Value *tag_match = Builder.CreateICmpEQ(tag,
          ConstantInt::get(Type::getInt64Ty(Context), vi));
      Value *cond = tag_match;

      for (auto &[field_name, sub_pat] : sp->fields) {
        int idx = get_struct_field_index(
            val_ann.struct_name + "::" + sp->struct_name, field_name);
        if (idx < 0) {
          errs() << "Error: variant '" << sp->struct_name
                 << "' has no field '" << field_name << "'\n";
          return nullptr;
        }
        Value *field_val = Builder.CreateExtractValue(val,
            {(unsigned)variant_data_idx, (unsigned)idx}, field_name);
        TypeAnnotation field_ann = get_struct_field_type(
            val_ann.struct_name + "::" + sp->struct_name, field_name);
        Value *sub_cond = gen_pattern_check(sub_pat.get(), field_val, field_ann);
        if (!sub_cond) return nullptr;
        cond = Builder.CreateAnd(cond, sub_cond);
      }
      return cond;
    }

    StructType *st = dyn_cast<StructType>(val->getType());
    if (!st) {
      errs() << "Error: struct pattern on non-struct value\n";
      return nullptr;
    }
    Value *cond = ConstantInt::getTrue(Context);
    for (auto &[field_name, sub_pat] : sp->fields) {
      int idx = get_struct_field_index(sp->struct_name, field_name);
      if (idx < 0) {
        errs() << "Error: struct '" << sp->struct_name << "' has no field '"
               << field_name << "'\n";
        return nullptr;
      }
      Value *field_val = Builder.CreateExtractValue(val, {(unsigned)idx}, field_name);
      TypeAnnotation field_ann = get_struct_field_type(sp->struct_name, field_name);
      Value *sub_cond = gen_pattern_check(sub_pat.get(), field_val, field_ann);
      if (!sub_cond) return nullptr;
      cond = Builder.CreateAnd(cond, sub_cond);
    }
    return cond;
  }

  errs() << "Error: unknown pattern type\n";
  return nullptr;
}

bool CodeGen::gen_pattern_bind(Pattern *pat, Value *val,
                                const TypeAnnotation &val_ann) {
  if (auto *wc = dynamic_cast<WildcardPattern *>(pat)) {
    return true;
  }

  if (auto *lit = dynamic_cast<LiteralPattern *>(pat)) {
    return true;
  }

  if (auto *var = dynamic_cast<VariablePattern *>(pat)) {
    Type *ty = val->getType();
    AllocaInst *alloca = Builder.CreateAlloca(ty, nullptr, var->name);
    named_values[var->name] = alloca;
    Builder.CreateStore(val, alloca);
    return true;
  }

  if (auto *sp = dynamic_cast<StructPattern *>(pat)) {
    bool is_variant = (val_ann.kind == TypeKind::Enum) ||
                      (val_ann.kind == TypeKind::Struct &&
                       enum_types.find(val_ann.struct_name) != enum_types.end());
    int variant_data_idx = -1;

    if (is_variant) {
      int vi = get_enum_variant_index(val_ann.struct_name, sp->struct_name);
      if (vi < 0) return false;
      variant_data_idx = 1 + vi;
    }

    for (auto &[field_name, sub_pat] : sp->fields) {
      Value *field_val;
      TypeAnnotation field_ann;
      if (is_variant) {
        int idx = get_struct_field_index(
            val_ann.struct_name + "::" + sp->struct_name, field_name);
        if (idx < 0) return false;
        field_val = Builder.CreateExtractValue(val,
            {(unsigned)variant_data_idx, (unsigned)idx}, field_name);
        field_ann = get_struct_field_type(
            val_ann.struct_name + "::" + sp->struct_name, field_name);
      } else {
        int idx = get_struct_field_index(sp->struct_name, field_name);
        if (idx < 0) return false;
        field_val = Builder.CreateExtractValue(val, {(unsigned)idx}, field_name);
        field_ann = get_struct_field_type(sp->struct_name, field_name);
      }
      if (!gen_pattern_bind(sub_pat.get(), field_val, field_ann))
        return false;
    }
    return true;
  }

  errs() << "Error: unknown pattern type in bind\n";
  return false;
}
