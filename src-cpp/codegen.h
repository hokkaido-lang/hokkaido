#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include "ast.h"
#include "cubical.h"
#include "error.h"

/// Write a compiler error with source location to the given output stream.
/// If expr is non-null and has line info, includes file:line:col.
inline void cg_error(llvm::raw_ostream &os, Expr *expr, const std::string &msg) {
  if (expr && expr->line > 0)
    os << error_at(expr->file, expr->line, expr->col, msg) << "\n";
  else
    os << "error: " << msg << "\n";
}

inline void cg_error(llvm::raw_ostream &os, Decl *decl, const std::string &msg) {
  if (decl && decl->line > 0)
    os << error_at(decl->file, decl->line, decl->col, msg) << "\n";
  else
    os << "error: " << msg << "\n";
}

inline void cg_error(llvm::raw_ostream &os, Stmt *stmt, const std::string &msg) {
  if (stmt && stmt->line > 0)
    os << error_at(stmt->file, stmt->line, stmt->col, msg) << "\n";
  else
    os << "error: " << msg << "\n";
}

inline void cg_error(llvm::raw_ostream &os, Pattern *pat, const std::string &msg) {
  if (pat && pat->line > 0)
    os << error_at(pat->file, pat->line, pat->col, msg) << "\n";
  else
    os << "error: " << msg << "\n";
}

// =========================================================================
// Hokkaido Language — Code Generator
// =========================================================================

inline bool is_unsigned_type(TypeKind kind) {
  return kind == TypeKind::Uint8 || kind == TypeKind::Uint16 ||
         kind == TypeKind::Uint32 || kind == TypeKind::Uint64;
}

inline TypeKind infer_typekind_from_llvm(llvm::Type *ty) {
  if (!ty) return TypeKind::Void;
  if (ty->isIntegerTy(1))  return TypeKind::Bool;
  if (ty->isIntegerTy(8))  return TypeKind::Int8;
  if (ty->isIntegerTy(16)) return TypeKind::Int16;
  if (ty->isIntegerTy(32)) return TypeKind::Int32;
  if (ty->isIntegerTy(64)) return TypeKind::Int64;
  if (ty->isHalfTy())      return TypeKind::Float16;
  if (ty->isFloatTy())     return TypeKind::Float32;
  if (ty->isDoubleTy())    return TypeKind::Float64;
  if (ty->isPointerTy())   return TypeKind::String;
  return TypeKind::Int64; // fallback
}

inline bool needs_type_annotation(TypeKind kind, int pointer_depth = 0, int array_size = 0) {
  return kind == TypeKind::Fn || kind == TypeKind::Struct || kind == TypeKind::Enum ||
         kind == TypeKind::Tuple || kind == TypeKind::Slice ||
         kind == TypeKind::Ref || kind == TypeKind::MutRef ||
         pointer_depth > 0 || array_size > 0;
}

class CodeGen {
  llvm::LLVMContext &Context;
  llvm::Module &M;
  llvm::IRBuilder<> &Builder;
  llvm::Function *MainFn = nullptr;
  llvm::BasicBlock *EntryBB = nullptr;

  std::map<std::string, llvm::AllocaInst *> named_values;
  std::map<std::string, TypeKind> named_types;
  std::map<std::string, TypeAnnotation> named_type_anns;
  // Module-level global variables (created for top-level lets)
  std::map<std::string, llvm::GlobalVariable *> global_values;

  // Registered struct types (name -> LLVM struct type)
  std::map<std::string, llvm::StructType *> struct_types;
  // Cached tuple types (mangled key -> LLVM struct type)
  std::map<std::string, llvm::StructType *> tuple_type_cache;
  // Cached slice types (mangled key -> LLVM struct type)
  std::map<std::string, llvm::StructType *> slice_type_cache;
  // Struct field types (name -> vector of (field_name, annotation))
  std::map<std::string, std::vector<std::pair<std::string, TypeAnnotation>>> struct_fields;

  // Registered enum types (name -> LLVM struct type)
  std::map<std::string, llvm::StructType *> enum_types;
  // Enum variant info: enum name -> vector of (variant_name, fields_vector)
  std::map<std::string, std::vector<std::pair<std::string, std::vector<StructField>>>> enum_variants;

  // Impl method registry: (type_name, method_name) -> mangled function name
  std::map<std::pair<std::string, std::string>, std::string> impl_methods;
  // Impl method return types: mangled_name -> return type annotation
  std::map<std::string, TypeAnnotation> impl_method_ret_types;
  // Trait declarations: trait_name -> method signatures
  std::map<std::string, std::vector<TraitMethodSig>> trait_decls;
  // Type impl registry: (type_name, trait_name) -> true (for bound checking)
  std::map<std::pair<std::string, std::string>, bool> type_impls;

  // Generic function templates (name -> AST)
  std::map<std::string, FnDecl *> generic_templates;
  // Monomorphized instantiations (mangled_name -> cloned FnDecl)
  std::map<std::string, std::unique_ptr<FnDecl>> monomorphized_fns;

public:
  CodeGen(llvm::LLVMContext &Ctx, llvm::Module &Mod, llvm::IRBuilder<> &Bld,
          bool Freestanding = false, std::string BaseDir = "")
      : Context(Ctx), M(Mod), Builder(Bld), freestanding(Freestanding),
        base_dir(std::move(BaseDir)) {}

  bool generate(const std::vector<std::unique_ptr<Decl>> &decls);

private:
  // When true, `main` is generated as a raw ELF entry point (no CRT/libc):
  // instead of returning normally, it terminates via a direct `exit`
  // syscall, and `extern fn` declarations are rejected at compile time
  // since there is no libc to resolve them against.
  bool freestanding;

  // Top-level codegen
  bool gen_main_body(const std::vector<std::unique_ptr<Decl>> &decls);

  // Impl registration
  bool register_impl_decl(ImplDecl *decl);

  // Struct declarations
  void register_struct_decl(StructDecl *decl);
  [[nodiscard]] int get_struct_field_index(const std::string &struct_name, const std::string &field_name);
  [[nodiscard]] TypeAnnotation get_struct_field_type(const std::string &struct_name, const std::string &field_name);
  // Generic struct templates (name -> AST)
  std::map<std::string, StructDecl *> struct_templates;
  [[nodiscard]] std::string struct_mangled_name(const std::string &name,
                                   const std::vector<TypeAnnotation> &type_args);
  [[nodiscard]] llvm::StructType *monomorphize_struct(const std::string &name,
                                         const std::vector<TypeAnnotation> &type_args);

  // Enum declarations
  void register_enum_decl(AdtDecl *decl);
  [[nodiscard]] int get_enum_variant_index(const std::string &enum_name, const std::string &variant_name);
  [[nodiscard]] const std::vector<StructField> *get_enum_variant_fields(
      const std::string &enum_name, const std::string &variant_name);

  // Resolve the type annotation for an expression without evaluating it
  [[nodiscard]] TypeAnnotation resolve_expr_type(Expr *expr);

  // Get a pointer to the memory location of an lvalue expression
  [[nodiscard]] llvm::Value *get_lvalue_ptr(Expr *expr, llvm::Type **out_type = nullptr);

  // Let declarations / statements
  bool gen_let_decl(LetDecl *decl);
  bool gen_global_let_decl(LetDecl *decl);
  bool gen_let_stmt(LetStmt *stmt);

  // Unified let initialization (shared by gen_let_decl and gen_let_stmt)
  bool gen_let_init(const std::string &name, TypeAnnotation &type_ann,
                    Expr *init_expr);
  bool alloc_and_store(const std::string &name, TypeKind kind,
                       llvm::Value *init, llvm::Type *llvm_type,
                       TypeAnnotation ann = {});
  bool alloc_and_store_array(const std::string &name, TypeKind kind,
                             int array_size, llvm::ArrayType *array_type,
                             llvm::Value *init,
                             TypeAnnotation ann = {});

  // Functions
  bool gen_fn_decl(FnDecl *decl);
  bool gen_fn_body(FnDecl *decl, llvm::Function *fn);

  // Statements
  bool gen_stmt(Stmt *stmt);
  bool gen_return_stmt(ReturnStmt *stmt);
  bool gen_if_stmt(IfStmt *stmt);
  bool gen_for_stmt(ForStmt *stmt);

  // Expression evaluation
  [[nodiscard]] llvm::Value *eval_expr(Expr *expr, llvm::Type *expected_type);

  // Expression evaluation — extracted sub-handlers
  [[nodiscard]] llvm::Value *eval_region_alloc(CallExpr *call);
  [[nodiscard]] llvm::Value *eval_constructor(ConstructorExpr *ctor, llvm::Type *expected_type);
  [[nodiscard]] llvm::Value *eval_closure(ClosureExpr *closure, llvm::Type *expected_type);
  [[nodiscard]] llvm::Value *eval_call(CallExpr *call, llvm::Type *expected_type);
  [[nodiscard]] llvm::Value *eval_method_call(MethodCallExpr *mcall, llvm::Type *expected_type);
  [[nodiscard]] llvm::Value *eval_atomic(AtomicExpr *atm);
  [[nodiscard]] llvm::Value *eval_binary(BinaryExpr *bin, llvm::Type *expected_type);
  [[nodiscard]] llvm::Value *eval_compound_assign(CompoundAssignExpr *compound);
  [[nodiscard]] llvm::Value *eval_if_expr(IfExpr *ifexpr, llvm::Type *expected_type);
  [[nodiscard]] llvm::Value *eval_match(MatchExpr *match, llvm::Type *expected_type);

  // Value generators (per-type)
  [[nodiscard]] llvm::Value *eval_int_init(Expr *expr);
  [[nodiscard]] llvm::Value *eval_float_init(Expr *expr);
  [[nodiscard]] llvm::Value *eval_string_init(Expr *expr);
  [[nodiscard]] llvm::Value *eval_cubical_init(Expr *expr, std::string *debug_out);

  // Array helpers
  [[nodiscard]] llvm::Value *eval_array_init(Expr *expr, llvm::ArrayType *array_type);
  [[nodiscard]] llvm::Value *eval_array_literal(ArrayLitExpr *arr, llvm::ArrayType *array_type);

  // LLVM type helpers
  [[nodiscard]] llvm::Type *get_llvm_type(TypeKind kind);
  [[nodiscard]] llvm::Type *get_llvm_type(const TypeAnnotation &ann);
  [[nodiscard]] llvm::StructType *get_tuple_type(const std::vector<TypeAnnotation> &elem_types);
  [[nodiscard]] std::string tuple_type_key(const std::vector<TypeAnnotation> &elem_types);
  [[nodiscard]] llvm::StructType *get_slice_type(const TypeAnnotation &elem_ann);

  // Pattern matching helpers
  [[nodiscard]] llvm::Value *gen_pattern_check(Pattern *pat, llvm::Value *val,
                                   const TypeAnnotation &val_ann);
  bool gen_pattern_bind(Pattern *pat, llvm::Value *val,
                         const TypeAnnotation &val_ann);

  // Cubical structured value helpers
  [[nodiscard]] llvm::Constant *build_cubical_constant(const cubical_value::CubicalValue *val);
  [[nodiscard]] llvm::Type *build_cubical_type(const cubical_value::CubicalValue *val);
  [[nodiscard]] std::string mangle_ann(const TypeAnnotation &ann);
  [[nodiscard]] std::string mangle_name(const std::string &fn_name,
                           const std::vector<TypeAnnotation> &type_args);
  void substitute_type_params(TypeAnnotation &ann,
                                const std::vector<std::string> &param_names,
                                const std::vector<TypeAnnotation> &type_args);
  void substitute_type_params_recursive(TypeAnnotation &ann,
                                         const std::vector<std::string> &param_names,
                                         const std::vector<TypeAnnotation> &type_args);
  bool monomorphize_and_codegen(FnDecl *template_decl,
                                  const std::vector<TypeAnnotation> &type_args,
                                  const std::string &mangled_name);

  // Closure helpers
  int closure_counter = 0;
  void discover_captures(Expr *expr,
                           const std::unordered_set<std::string> &param_names,
                           std::vector<std::string> &captures);
  void discover_captures_in_body(const std::vector<std::unique_ptr<Stmt>> &body,
                                   const std::unordered_set<std::string> &param_names,
                                   std::vector<std::string> &captures);

  // Function pointer wrapper helpers (for &fn_name expressions)
  std::map<std::string, llvm::GlobalVariable *> fnval_globals;
  [[nodiscard]] llvm::GlobalVariable *get_fnval_wrapper(const std::string &fn_name,
                                           llvm::Function *f);

  // Source file base directory, used to resolve .cub file paths.
  std::string base_dir;

  // Region state
  struct RegionInfo {
    llvm::AllocaInst *buffer;
    llvm::Value *current_ptr;
    llvm::Value *end_ptr;
    llvm::BasicBlock *end_bb;
  };
  std::vector<RegionInfo> region_stack;

  // Region lifetime tracking: variables whose value comes from __region_alloc
  // (i.e., they point into a region buffer). A return that references any of
  // these is an error — the region will be freed before the function returns.
  std::unordered_set<std::string> region_allocated_vars;

  // Loop state for break/continue
  struct LoopInfo {
    std::string label;
    llvm::BasicBlock *update_bb;
    llvm::BasicBlock *end_bb;
  };
  std::vector<LoopInfo> loop_stack;
  bool gen_break_stmt(BreakStmt *stmt);
  bool gen_continue_stmt(ContinueStmt *stmt);
  bool gen_while_stmt(WhileStmt *stmt);
  bool gen_region_stmt(RegionStmt *stmt);

  // Region lifetime tracking helpers
  void track_region_alloc_init(const std::string &name, Expr *init_expr);
  bool check_region_lifetime(ReturnStmt *stmt);
};