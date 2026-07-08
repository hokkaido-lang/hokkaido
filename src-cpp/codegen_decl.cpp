#include "codegen.h"

#include <iostream>

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// -------------------------------------------------------------------------
// Alloca + store helper
// -------------------------------------------------------------------------

bool CodeGen::alloc_and_store(const std::string &name, TypeKind kind,
                               Value *init, Type *llvm_type,
                               TypeAnnotation ann) {
  AllocaInst *alloca = Builder.CreateAlloca(llvm_type, nullptr, name);
  Builder.CreateStore(init, alloca);
  named_values[name] = alloca;
  named_types[name] = kind;
  if (kind == TypeKind::Fn || kind == TypeKind::Struct || kind == TypeKind::Enum || kind == TypeKind::Tuple || kind == TypeKind::Slice || ann.pointer_depth > 0)
    named_type_anns[name] = ann;
  return true;
}

bool CodeGen::alloc_and_store_array(const std::string &name, TypeKind kind,
                                     int array_size, ArrayType *array_type,
                                     Value *init,
                                     TypeAnnotation ann) {
  AllocaInst *alloca = Builder.CreateAlloca(array_type, nullptr, name);
  Builder.CreateStore(init, alloca);
  named_values[name] = alloca;
  named_types[name] = kind;
  if (kind == TypeKind::Fn || kind == TypeKind::Struct || kind == TypeKind::Enum || kind == TypeKind::Tuple || ann.array_size > 0)
    named_type_anns[name] = ann;
  return true;
}

// -------------------------------------------------------------------------
// Let declarations (top-level)
// -------------------------------------------------------------------------

bool CodeGen::gen_global_let_decl(LetDecl *decl) {
  Type *llvm_type = get_llvm_type(decl->type_ann);
  if (!llvm_type) return false;

  if (decl->type_ann.kind == TypeKind::Cubical) {
    std::string debug;
    Value *init = eval_cubical_init(decl->init_expr.get(), &debug);
    if (!init) return false;
    std::cout << "  " << decl->name << " = " << debug << "\n";

    auto *gv = new GlobalVariable(M, init->getType(), true,
                                   GlobalVariable::InternalLinkage,
                                   cast<Constant>(init), decl->name);
    gv->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    global_values[decl->name] = gv;
    named_types[decl->name] = TypeKind::Int64;
    return true;
  }

  Value *init = eval_expr(decl->init_expr.get(), llvm_type);
  if (!init) return false;

  auto *gv = new GlobalVariable(M, llvm_type, true,
                                 GlobalVariable::InternalLinkage,
                                 cast<Constant>(init), decl->name);
  gv->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  global_values[decl->name] = gv;
  named_types[decl->name] = decl->type_ann.kind;
    if (decl->type_ann.kind == TypeKind::Fn ||
      decl->type_ann.kind == TypeKind::Struct ||
      decl->type_ann.kind == TypeKind::Enum ||
      decl->type_ann.kind == TypeKind::Tuple ||
      decl->type_ann.pointer_depth > 0)
    named_type_anns[decl->name] = decl->type_ann;
  return true;
}

// -------------------------------------------------------------------------
// Let declarations (local)
// -------------------------------------------------------------------------

bool CodeGen::gen_let_decl(LetDecl *decl) {
  Type *llvm_type = get_llvm_type(decl->type_ann);
  if (!llvm_type) return false;

  if (decl->type_ann.array_size > 0) {
    ArrayType *arr_type = cast<ArrayType>(llvm_type);
    Value *init = eval_array_init(decl->init_expr.get(), arr_type);
    if (!init) return false;
    return alloc_and_store_array(decl->name, decl->type_ann.kind,
                                  decl->type_ann.array_size, arr_type, init,
                                  decl->type_ann);
  }

  if (decl->type_ann.kind == TypeKind::Struct) {
    Value *init = nullptr;
    if (decl->init_expr) {
      bool skip = false;
      if (auto *id = dynamic_cast<IdentExpr *>(decl->init_expr.get()))
        skip = named_values.find(id->name) == named_values.end();
      if (!skip)
        init = eval_expr(decl->init_expr.get(), llvm_type);
    }
    if (!init)
      init = ConstantAggregateZero::get(llvm_type);
    return alloc_and_store(decl->name, decl->type_ann.kind, init, llvm_type,
                            decl->type_ann);
  }

  if (decl->type_ann.kind == TypeKind::Tuple) {
    Value *init = nullptr;
    if (decl->init_expr)
      init = eval_expr(decl->init_expr.get(), llvm_type);
    if (!init)
      init = ConstantAggregateZero::get(llvm_type);
    return alloc_and_store(decl->name, decl->type_ann.kind, init, llvm_type,
                            decl->type_ann);
  }

  Value *init = nullptr;
  std::string debug;

  if (decl->type_ann.pointer_depth > 0) {
    init = eval_expr(decl->init_expr.get(), llvm_type);
    if (!init) return false;
    if (!alloc_and_store(decl->name, decl->type_ann.kind, init, llvm_type,
                          decl->type_ann))
      return false;
    track_region_alloc_init(decl->name, decl->init_expr.get());
    return true;
  }

  switch (decl->type_ann.kind) {
    case TypeKind::Void:
      errs() << "Error: variable cannot have void type\n";
      return false;
    case TypeKind::Int8:
      init = eval_expr(decl->init_expr.get(), Type::getInt8Ty(Context));
      break;
    case TypeKind::Int16:
      init = eval_expr(decl->init_expr.get(), Type::getInt16Ty(Context));
      break;
    case TypeKind::Int32:
      init = eval_expr(decl->init_expr.get(), Type::getInt32Ty(Context));
      break;
    case TypeKind::Int64:
      init = eval_int_init(decl->init_expr.get());
      break;
    case TypeKind::Uint8:
      init = eval_expr(decl->init_expr.get(), Type::getInt8Ty(Context));
      break;
    case TypeKind::Uint16:
      init = eval_expr(decl->init_expr.get(), Type::getInt16Ty(Context));
      break;
    case TypeKind::Uint32:
      init = eval_expr(decl->init_expr.get(), Type::getInt32Ty(Context));
      break;
    case TypeKind::Uint64:
      init = eval_int_init(decl->init_expr.get());
      break;
    case TypeKind::Float16:
      init = eval_expr(decl->init_expr.get(), Type::getHalfTy(Context));
      break;
    case TypeKind::Float32:
      init = eval_expr(decl->init_expr.get(), Type::getFloatTy(Context));
      break;
    case TypeKind::Float64:
      init = eval_float_init(decl->init_expr.get());
      break;
    case TypeKind::Bool:
      init = eval_expr(decl->init_expr.get(), Type::getInt1Ty(Context));
      break;
    case TypeKind::Char:
      init = eval_expr(decl->init_expr.get(), Type::getInt8Ty(Context));
      break;
    case TypeKind::String:
      init = eval_string_init(decl->init_expr.get());
      break;
    case TypeKind::Cubical:
      init = eval_cubical_init(decl->init_expr.get(), &debug);
      if (init) std::cout << "  " << decl->name << " = " << debug << "\n";
      break;
    case TypeKind::Tuple:
      init = eval_expr(decl->init_expr.get(), llvm_type);
      break;
    case TypeKind::Slice:
      init = eval_expr(decl->init_expr.get(), llvm_type);
      if (!init) return false;
      break;
    case TypeKind::Fn:
      init = eval_expr(decl->init_expr.get(), llvm_type);
      if (!init) return false;
      break;
    default:
      break;
  }
  if (!init) return false;
  return alloc_and_store(decl->name, decl->type_ann.kind, init, llvm_type,
                          decl->type_ann);
}

// -------------------------------------------------------------------------
// Let statements
// -------------------------------------------------------------------------

bool CodeGen::gen_let_stmt(LetStmt *stmt) {
  Type *llvm_type = get_llvm_type(stmt->type_ann);
  if (!llvm_type) return false;

  if (stmt->type_ann.array_size > 0) {
    ArrayType *arr_type = cast<ArrayType>(llvm_type);
    Value *init = eval_array_init(stmt->init_expr.get(), arr_type);
    if (!init) return false;
    return alloc_and_store_array(stmt->name, stmt->type_ann.kind,
                                  stmt->type_ann.array_size, arr_type, init,
                                  stmt->type_ann);
  }

  if (stmt->type_ann.kind == TypeKind::Struct) {
    Value *init = nullptr;
    if (stmt->init_expr) {
      bool skip = false;
      if (auto *id = dynamic_cast<IdentExpr *>(stmt->init_expr.get()))
        skip = named_values.find(id->name) == named_values.end();
      if (!skip)
        init = eval_expr(stmt->init_expr.get(), llvm_type);
    }
    if (!init)
      init = ConstantAggregateZero::get(llvm_type);
    return alloc_and_store(stmt->name, stmt->type_ann.kind, init, llvm_type,
                            stmt->type_ann);
  }

  if (stmt->type_ann.kind == TypeKind::Tuple) {
    Value *init = nullptr;
    if (stmt->init_expr)
      init = eval_expr(stmt->init_expr.get(), llvm_type);
    if (!init)
      init = ConstantAggregateZero::get(llvm_type);
    return alloc_and_store(stmt->name, stmt->type_ann.kind, init, llvm_type,
                            stmt->type_ann);
  }

  Value *init = nullptr;

  if (stmt->type_ann.pointer_depth > 0) {
    init = eval_expr(stmt->init_expr.get(), llvm_type);
    if (!init) return false;
    if (!alloc_and_store(stmt->name, stmt->type_ann.kind, init, llvm_type,
                          stmt->type_ann))
      return false;
    track_region_alloc_init(stmt->name, stmt->init_expr.get());
    return true;
  }

  switch (stmt->type_ann.kind) {
    case TypeKind::Void:
      errs() << "Error: variable cannot have void type\n";
      return false;
    case TypeKind::Int8:
      init = eval_expr(stmt->init_expr.get(), Type::getInt8Ty(Context));
      break;
    case TypeKind::Int16:
      init = eval_expr(stmt->init_expr.get(), Type::getInt16Ty(Context));
      break;
    case TypeKind::Int32:
      init = eval_expr(stmt->init_expr.get(), Type::getInt32Ty(Context));
      break;
    case TypeKind::Int64:
      init = eval_int_init(stmt->init_expr.get());
      break;
    case TypeKind::Uint8:
      init = eval_expr(stmt->init_expr.get(), Type::getInt8Ty(Context));
      break;
    case TypeKind::Uint16:
      init = eval_expr(stmt->init_expr.get(), Type::getInt16Ty(Context));
      break;
    case TypeKind::Uint32:
      init = eval_expr(stmt->init_expr.get(), Type::getInt32Ty(Context));
      break;
    case TypeKind::Uint64:
      init = eval_int_init(stmt->init_expr.get());
      break;
    case TypeKind::Float16:
      init = eval_expr(stmt->init_expr.get(), Type::getHalfTy(Context));
      break;
    case TypeKind::Float32:
      init = eval_expr(stmt->init_expr.get(), Type::getFloatTy(Context));
      break;
    case TypeKind::Float64:
      init = eval_float_init(stmt->init_expr.get());
      break;
    case TypeKind::Bool:
      init = eval_expr(stmt->init_expr.get(), Type::getInt1Ty(Context));
      break;
    case TypeKind::Char:
      init = eval_expr(stmt->init_expr.get(), Type::getInt8Ty(Context));
      break;
    case TypeKind::String:
      init = eval_string_init(stmt->init_expr.get());
      break;
    case TypeKind::Cubical:
      init = eval_cubical_init(stmt->init_expr.get(), nullptr);
      break;
    case TypeKind::Tuple:
      init = eval_expr(stmt->init_expr.get(), llvm_type);
      break;
    case TypeKind::Slice:
      init = eval_expr(stmt->init_expr.get(), llvm_type);
      if (!init) return false;
      break;
    case TypeKind::Fn:
      init = eval_expr(stmt->init_expr.get(), llvm_type);
      if (!init) return false;
      break;
    default:
      break;
  }
  if (!init) return false;
  return alloc_and_store(stmt->name, stmt->type_ann.kind, init, llvm_type,
                          stmt->type_ann);
}

// -------------------------------------------------------------------------
// Functions
// -------------------------------------------------------------------------

bool CodeGen::gen_fn_body(FnDecl *decl, Function *fn) {
  BasicBlock *BB = BasicBlock::Create(Context, "entry", fn);
  Builder.SetInsertPoint(BB);

  auto saved_values = std::move(named_values);
  auto saved_types = std::move(named_types);
  auto saved_type_anns = std::move(named_type_anns);
  named_values.clear();
  named_types.clear();
  named_type_anns.clear();

  size_t i = 0;
  for (auto &arg : fn->args()) {
    arg.setName(decl->params[i].name);
    AllocaInst *alloca = Builder.CreateAlloca(arg.getType(), nullptr, arg.getName());
    Builder.CreateStore(&arg, alloca);
    std::string pname = std::string(arg.getName());
    named_values[pname] = alloca;
    named_types[pname] = decl->params[i].type_ann.kind;
    {
      auto &ta = decl->params[i].type_ann;
      if (ta.kind == TypeKind::Fn || ta.kind == TypeKind::Struct || ta.kind == TypeKind::Enum || ta.kind == TypeKind::Tuple || ta.kind == TypeKind::Slice || ta.pointer_depth > 0 || ta.array_size > 0)
        named_type_anns[pname] = ta;
    }
    i++;
  }

  for (auto &stmt : decl->body) {
      if (!gen_stmt(stmt.get())) {
        named_values = std::move(saved_values);
        named_types = std::move(saved_types);
        named_type_anns = std::move(saved_type_anns);
        return false;
      }
  }

  Type *ret_type = fn->getReturnType();
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
  return true;
}

// -------------------------------------------------------------------------
// Impl registration
// -------------------------------------------------------------------------

bool CodeGen::register_impl_decl(ImplDecl *decl) {
  for (auto &method : decl->methods) {
    // Determine mangled function name
    std::string mangled_name;
    if (!decl->trait_name.empty()) {
      mangled_name = "impl_" + decl->trait_name + "_for_" + decl->type_name + "_" + method->name;
    } else {
      mangled_name = "impl_" + decl->type_name + "_" + method->name;
    }

    // Substitute Self -> type_name for trait impls
    if (!decl->trait_name.empty()) {
      TypeAnnotation self_type = {TypeKind::Struct, 0, 0, decl->type_name};
      for (auto &p : method->params) {
        substitute_type_params_recursive(p.type_ann, {"Self"}, {self_type});
      }
      substitute_type_params_recursive(method->return_type, {"Self"}, {self_type});
    }

    // Create LLVM function
    std::vector<Type *> param_types;
    for (auto &p : method->params)
      param_types.push_back(get_llvm_type(p.type_ann));

    FunctionType *FT = FunctionType::get(
        get_llvm_type(method->return_type), param_types, method->is_variadic);
    Function::Create(FT, Function::ExternalLinkage, mangled_name, &M);

    // Register in impl lookup maps
    impl_methods[{decl->type_name, method->name}] = mangled_name;
    impl_method_ret_types[mangled_name] = method->return_type;

    // Codegen the method body
    auto saved_named_values = named_values;
    auto saved_named_types = named_types;
    auto saved_named_type_anns = named_type_anns;
    auto saved_insert_block = Builder.GetInsertBlock();

    bool ok = gen_fn_body(method.get(), M.getFunction(mangled_name));

    if (saved_insert_block)
      Builder.SetInsertPoint(saved_insert_block);
    named_values = std::move(saved_named_values);
    named_types = std::move(saved_named_types);
    named_type_anns = std::move(saved_named_type_anns);

    if (!ok) return false;
  }

  // Register trait impl relationship
  if (!decl->trait_name.empty()) {
    type_impls[{decl->type_name, decl->trait_name}] = true;
  }

  return true;
}
