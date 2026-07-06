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

#include "ast.h"
#include "cubical.h"

// =========================================================================
// Hokkaido Language — Code Generator
// =========================================================================

inline bool is_unsigned_type(TypeKind kind) {
  return kind == TypeKind::Uint8 || kind == TypeKind::Uint16 ||
         kind == TypeKind::Uint32 || kind == TypeKind::Uint64;
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
  llvm::StructType *get_struct_type(const std::string &name);
  int get_struct_field_index(const std::string &struct_name, const std::string &field_name);
  TypeAnnotation get_struct_field_type(const std::string &struct_name, const std::string &field_name);
  // Generic struct templates (name -> AST)
  std::map<std::string, StructDecl *> struct_templates;
  std::string struct_mangled_name(const std::string &name,
                                   const std::vector<TypeAnnotation> &type_args);
  llvm::StructType *monomorphize_struct(const std::string &name,
                                         const std::vector<TypeAnnotation> &type_args);

  // Enum declarations
  void register_enum_decl(AdtDecl *decl);
  int get_enum_variant_index(const std::string &enum_name, const std::string &variant_name);
  const std::vector<StructField> *get_enum_variant_fields(
      const std::string &enum_name, const std::string &variant_name);

  // Resolve the type annotation for an expression without evaluating it
  TypeAnnotation resolve_expr_type(Expr *expr);

  // Get a pointer to the memory location of an lvalue expression
  llvm::Value *get_lvalue_ptr(Expr *expr, llvm::Type **out_type = nullptr);

  // Let declarations / statements
  bool gen_let_decl(LetDecl *decl);
  bool gen_global_let_decl(LetDecl *decl);
  bool gen_let_stmt(LetStmt *stmt);
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
  llvm::Value *eval_expr(Expr *expr, llvm::Type *expected_type);

  // Value generators (per-type)
  llvm::Value *eval_int_init(Expr *expr);
  llvm::Value *eval_float_init(Expr *expr);
  llvm::Value *eval_string_init(Expr *expr);
  llvm::Value *eval_cubical_init(Expr *expr, std::string *debug_out);

  // Array helpers
  llvm::Value *eval_array_init(Expr *expr, llvm::ArrayType *array_type);
  llvm::Value *eval_array_literal(ArrayLitExpr *arr, llvm::ArrayType *array_type);

  // LLVM type helpers
  llvm::Type *get_llvm_type(TypeKind kind);
  llvm::Type *get_llvm_type(const TypeAnnotation &ann);
  llvm::StructType *get_tuple_type(const std::vector<TypeAnnotation> &elem_types);
  static std::string tuple_type_key(const std::vector<TypeAnnotation> &elem_types);
  llvm::StructType *get_slice_type(const TypeAnnotation &elem_ann);

  // Pattern matching helpers
  llvm::Value *gen_pattern_check(Pattern *pat, llvm::Value *val,
                                  const TypeAnnotation &val_ann);
  bool gen_pattern_bind(Pattern *pat, llvm::Value *val,
                         const TypeAnnotation &val_ann);

  // Cubical structured value helpers
  llvm::Constant *build_cubical_constant(const cubical_value::CubicalValue *val);
  llvm::Type *build_cubical_type(const cubical_value::CubicalValue *val);
  std::string mangle_ann(const TypeAnnotation &ann);
  std::string mangle_name(const std::string &fn_name,
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
  llvm::GlobalVariable *get_fnval_wrapper(const std::string &fn_name,
                                           llvm::Function *f);

  // Source file base directory, used to resolve .cub file paths.
  std::string base_dir;

  // Loop state for break/continue
  struct LoopInfo {
    std::string label;
    llvm::BasicBlock *update_bb;
    llvm::BasicBlock *end_bb;
  };
  std::vector<LoopInfo> loop_stack;
  bool gen_break_stmt(BreakStmt *stmt);
  bool gen_continue_stmt(ContinueStmt *stmt);
};