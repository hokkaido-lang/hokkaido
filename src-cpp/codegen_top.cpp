#include "codegen.h"

#include <functional>

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// -------------------------------------------------------------------------
// Top-level generate
// -------------------------------------------------------------------------

bool CodeGen::generate(const std::vector<std::unique_ptr<Decl>> &decls) {
  for (auto &decl : decls) {
    if (auto *sd = dynamic_cast<StructDecl *>(decl.get())) {
      register_struct_decl(sd);
    } else if (auto *ed = dynamic_cast<AdtDecl *>(decl.get())) {
      register_enum_decl(ed);
    }
  }

  std::vector<FnDecl *> fn_decls;
  FnDecl *user_main = nullptr;
  for (auto &decl : decls) {
    if (auto *fn = dynamic_cast<FnDecl *>(decl.get())) {
      if (freestanding && fn->is_extern) {
        errs() << "Error: 'extern fn " << fn->name << "' is not allowed in "
                  "freestanding mode (no libc is linked, so there is no "
                  "symbol for it to resolve against)\n";
        return false;
      }

      if (!fn->type_params.empty()) {
        if (fn->is_extern) {
          errs() << "Error: extern function '" << fn->name
                 << "' cannot be generic\n";
          return false;
        }
        generic_templates[fn->name] = fn;
        continue;
      }

      std::vector<Type *> param_types;
      for (auto &p : fn->params)
        param_types.push_back(get_llvm_type(p.type_ann));

      FunctionType *FT = FunctionType::get(
          get_llvm_type(fn->return_type), param_types, fn->is_variadic);
      std::string llvm_name = fn->name;
      if (!fn->is_extern && fn->name == "main") {
        llvm_name = "__user_main";
        user_main = fn;
      }
      Function::Create(FT, Function::ExternalLinkage, llvm_name, &M);
      fn_decls.push_back(fn);
    }
  }

  if (!user_main) {
    errs() << "Error: no main function defined (add 'fn main() -> int { ... }')\n";
    return false;
  }

  if (user_main->return_type.kind != TypeKind::Int64) {
    errs() << "Error: main function must return int64\n";
    return false;
  }

  if (user_main->params.size() != 0 && user_main->params.size() != 2) {
    errs() << "Error: main function must have 0 or 2 parameters"
              " (argc: int, argv: int8**)\n";
    return false;
  }
  if (user_main->params.size() == 2) {
    auto &p0 = user_main->params[0];
    auto &p1 = user_main->params[1];
    if (p0.type_ann.kind != TypeKind::Int64 ||
        p1.type_ann.kind != TypeKind::Int8 ||
        p1.type_ann.pointer_depth != 2) {
      errs() << "Error: main parameters must be (argc: int, argv: int8**)\n";
      return false;
    }
  }

  for (auto &decl : decls) {
    if (auto *let = dynamic_cast<LetDecl *>(decl.get())) {
      if (!gen_global_let_decl(let)) return false;
    }
  }

  for (auto *fn : fn_decls) {
    if (fn->is_extern) continue;
    std::string llvm_name = (fn->name == "main") ? "__user_main" : fn->name;
    if (!gen_fn_body(fn, M.getFunction(llvm_name)))
      return false;
  }

  if (!gen_main_body(decls))
    return false;

  if (verifyModule(M, &errs())) {
    errs() << "Error: module verification failed\n";
    return false;
  }
  return true;
}

bool CodeGen::gen_main_body(const std::vector<std::unique_ptr<Decl>> &decls) {
  Function *user_main = M.getFunction("__user_main");
  bool needs_args = (user_main && user_main->arg_size() == 2 && !freestanding);

  std::vector<Type *> main_param_types;
  if (freestanding) {
    main_param_types = {};
  } else {
    main_param_types = {Type::getInt32Ty(Context),
                        PointerType::getUnqual(Context)};
  }
  FunctionType *FT = FunctionType::get(Type::getInt32Ty(Context), main_param_types, false);
  MainFn = Function::Create(FT, Function::ExternalLinkage, "main", &M);
  EntryBB = BasicBlock::Create(Context, "entry", MainFn);
  Builder.SetInsertPoint(EntryBB);
  named_values.clear();
  named_types.clear();

  Value *result;
  if (user_main) {
    if (needs_args) {
      Function::arg_iterator ai = MainFn->arg_begin();
      Value *argc_i32 = ai;
      Value *argv = ++ai;
      argc_i32->setName("argc");
      argv->setName("argv");
      Value *argc_i64 = Builder.CreateSExt(argc_i32, Type::getInt64Ty(Context), "argc.ext");
      result = Builder.CreateCall(user_main, {argc_i64, argv});
    } else {
      result = Builder.CreateCall(user_main, {});
    }
  } else {
    result = ConstantInt::get(Type::getInt64Ty(Context), 0);
  }

  if (freestanding) {
    FunctionType *AsmFT =
        FunctionType::get(Type::getVoidTy(Context), {Type::getInt64Ty(Context)}, false);
    InlineAsm *ExitSyscall = InlineAsm::get(
        AsmFT, "movq $$60, %rax\n\tsyscall",
        "{rdi},~{rax},~{rcx},~{r11},~{memory}",
        /*hasSideEffects=*/true);
    Builder.CreateCall(ExitSyscall, {result});
    Builder.CreateUnreachable();
  } else {
    Value *truncated = Builder.CreateTrunc(result, Type::getInt32Ty(Context));
    Builder.CreateRet(truncated);
  }
  return true;
}

// -------------------------------------------------------------------------
// Generics / monomorphization helpers
// -------------------------------------------------------------------------

static std::string mangle_ann(const TypeAnnotation &ann) {
  auto fn = [](const TypeAnnotation &a) -> std::string {
    switch (a.kind) {
      case TypeKind::Void:    return "void";
      case TypeKind::Int8:    return "i8";
      case TypeKind::Int16:   return "i16";
      case TypeKind::Int32:   return "i32";
      case TypeKind::Int64:   return "i64";
      case TypeKind::Uint8:   return "u8";
      case TypeKind::Uint16:  return "u16";
      case TypeKind::Uint32:  return "u32";
      case TypeKind::Uint64:  return "u64";
      case TypeKind::Float16: return "f16";
      case TypeKind::Float32: return "f32";
      case TypeKind::Float64: return "f64";
      case TypeKind::Bool:    return "bool";
      case TypeKind::String:  return "str";
      case TypeKind::Char:    return "char";
      case TypeKind::Cubical: return "cub";
      case TypeKind::Struct:
      case TypeKind::TypeParam: {
        std::string s = a.struct_name;
        for (auto &c : s) if (c == ':') c = '_';
        return s;
      }
      case TypeKind::Tuple: {
        std::string s = "tup";
        for (auto &et : a.tuple_types)
          s += "_" + mangle_ann(et);
        return s;
      }
      case TypeKind::Slice:
        return "slice_" + mangle_ann(a.tuple_types[0]);
      case TypeKind::Fn: {
        std::string s = "fn";
        for (size_t pi = 0; pi + 1 < a.tuple_types.size(); pi++)
          s += "_" + mangle_ann(a.tuple_types[pi]);
        s += "_to_" + mangle_ann(a.tuple_types.back());
        return s;
      }
    }
    return "?";
  };
  std::string s = fn(ann);
  for (int i = 0; i < ann.pointer_depth; i++)
    s += "p";
  if (ann.array_size > 0)
    s += "a" + std::to_string(ann.array_size);
  return s;
}

std::string CodeGen::mangle_name(const std::string &fn_name,
                                  const std::vector<TypeAnnotation> &type_args) {
  std::string result = fn_name;
  for (auto &ta : type_args)
    result += "$" + mangle_ann(ta);
  return result;
}

void CodeGen::substitute_type_params(TypeAnnotation &ann,
                                      const std::vector<std::string> &param_names,
                                      const std::vector<TypeAnnotation> &type_args) {
  if (ann.kind == TypeKind::TypeParam) {
    for (size_t i = 0; i < param_names.size(); i++) {
      if (ann.struct_name == param_names[i]) {
        ann = type_args[i];
        return;
      }
    }
    errs() << "Internal error: unresolved type parameter '" << ann.struct_name << "'\n";
  }
}

bool CodeGen::monomorphize_and_codegen(FnDecl *template_decl,
                                        const std::vector<TypeAnnotation> &type_args,
                                        const std::string &mangled_name) {
  std::vector<std::pair<TypeAnnotation *, TypeAnnotation>> saved_anns;

  auto substitute_in = [&](TypeAnnotation &ann) {
    if (ann.kind == TypeKind::TypeParam) {
      saved_anns.push_back({&ann, ann});
      for (size_t i = 0; i < template_decl->type_params.size(); i++) {
        if (ann.struct_name == template_decl->type_params[i]) {
          ann = type_args[i];
          break;
        }
      }
    }
  };

  for (auto &p : template_decl->params)
    substitute_in(p.type_ann);

  substitute_in(template_decl->return_type);

  std::function<void(std::vector<std::unique_ptr<Stmt>>&)> walk_body;
  std::function<void(Expr*)> walk_expr;
  walk_expr = [&](Expr *expr) {
    if (!expr) return;
    if (auto *atm = dynamic_cast<AtomicExpr *>(expr)) {
      for (auto &arg : atm->args)
        walk_expr(arg.get());
      return;
    }
    if (auto *call = dynamic_cast<CallExpr *>(expr)) {
      for (auto &ta : call->type_args)
        substitute_in(ta);
      for (auto &arg : call->args)
        walk_expr(arg.get());
      if (call->callee_expr)
        walk_expr(call->callee_expr.get());
    } else if (auto *bin = dynamic_cast<BinaryExpr *>(expr)) {
      walk_expr(bin->left.get());
      walk_expr(bin->right.get());
    } else if (auto *unary = dynamic_cast<UnaryExpr *>(expr)) {
      walk_expr(unary->operand.get());
    } else if (auto *assign = dynamic_cast<AssignExpr *>(expr)) {
      walk_expr(assign->target.get());
      walk_expr(assign->value.get());
    } else if (auto *compound = dynamic_cast<CompoundAssignExpr *>(expr)) {
      walk_expr(compound->target.get());
      walk_expr(compound->value.get());
    } else if (auto *field = dynamic_cast<FieldAccessExpr *>(expr)) {
      walk_expr(field->object.get());
    } else if (auto *deref = dynamic_cast<DerefExpr *>(expr)) {
      walk_expr(deref->operand.get());
    } else if (auto *addr = dynamic_cast<AddressOfExpr *>(expr)) {
      walk_expr(addr->operand.get());
    } else if (auto *sub = dynamic_cast<SubscriptExpr *>(expr)) {
      walk_expr(sub->array.get());
      walk_expr(sub->index.get());
    } else if (auto *arr = dynamic_cast<ArrayLitExpr *>(expr)) {
      for (auto &el : arr->elements)
        walk_expr(el.get());
    } else if (auto *tup = dynamic_cast<TupleExpr *>(expr)) {
      for (auto &el : tup->elements)
        walk_expr(el.get());
    } else if (auto *ctor = dynamic_cast<ConstructorExpr *>(expr)) {
      for (auto &[_, fexpr] : ctor->fields)
        walk_expr(fexpr.get());
    } else if (auto *match = dynamic_cast<MatchExpr *>(expr)) {
      walk_expr(match->value.get());
      for (auto &arm : match->arms)
        walk_expr(arm.expr.get());
    } else if (auto *ifexpr_e = dynamic_cast<IfExpr *>(expr)) {
      walk_expr(ifexpr_e->condition.get());
      walk_expr(ifexpr_e->then_expr.get());
      walk_expr(ifexpr_e->else_expr.get());
    } else if (auto *closure = dynamic_cast<ClosureExpr *>(expr)) {
      walk_body(closure->body);
    } else if (auto *ret = dynamic_cast<ReturnStmt *>(expr)) {
      walk_expr(ret->value.get());
    }
  };
  walk_body = [&](std::vector<std::unique_ptr<Stmt>> &body) {
    for (auto &stmt : body) {
      if (auto *let = dynamic_cast<LetStmt *>(stmt.get())) {
        substitute_in(let->type_ann);
        walk_expr(let->init_expr.get());
      } else if (auto *exprs = dynamic_cast<ExprStmt *>(stmt.get())) {
        walk_expr(exprs->expr.get());
      } else if (auto *ret = dynamic_cast<ReturnStmt *>(stmt.get())) {
        walk_expr(ret->value.get());
      } else if (auto *ifs = dynamic_cast<IfStmt *>(stmt.get())) {
        walk_expr(ifs->condition.get());
        walk_body(ifs->then_branch);
        walk_body(ifs->else_branch);
      } else if (auto *for_s = dynamic_cast<ForStmt *>(stmt.get())) {
        walk_expr(for_s->condition.get());
        walk_expr(for_s->update.get());
        walk_body(for_s->body);
      } else if (dynamic_cast<BreakStmt *>(stmt.get())) {
      } else if (dynamic_cast<ContinueStmt *>(stmt.get())) {
      }
    }
  };
  walk_body(template_decl->body);

  std::vector<Type *> param_types;
  for (auto &p : template_decl->params)
    param_types.push_back(get_llvm_type(p.type_ann));

  FunctionType *FT = FunctionType::get(
      get_llvm_type(template_decl->return_type), param_types,
      template_decl->is_variadic);
  Function::Create(FT, Function::ExternalLinkage, mangled_name, &M);

  auto saved_named_values = named_values;
  auto saved_named_types = named_types;
  auto saved_named_type_anns = named_type_anns;
  auto saved_insert_block = Builder.GetInsertBlock();

  bool ok = gen_fn_body(template_decl, M.getFunction(mangled_name));

  if (saved_insert_block)
    Builder.SetInsertPoint(saved_insert_block);
  named_values = std::move(saved_named_values);
  named_types = std::move(saved_named_types);
  named_type_anns = std::move(saved_named_type_anns);

  for (auto &[ptr, saved] : saved_anns)
    *ptr = saved;

  return ok;
}
