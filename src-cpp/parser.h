#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "ast.h"
#include "lexer.h"

// =========================================================================
// Hokkaido Language — Parser
// =========================================================================

class Parser {
  Lexer &lexer;
  Token cur_tok;
  bool has_error = false;
  std::string error_msg;

  // Directory the current source file lives in; used to resolve relative
  // `include "..."` paths.
  std::string base_dir;
  // Module root directory — the nearest parent of base_dir that contains
  // an `hk.mod` file, or base_dir itself if not found.
  std::string module_root;
  // Set of canonical file paths already included, shared across the whole
  // include tree, so a file (directly or transitively) cannot include
  // itself and get stuck in infinite recursion.
  std::shared_ptr<std::set<std::string>> included_files;
  // Set of canonical directory paths already imported (to deduplicate).
  std::shared_ptr<std::set<std::string>> imported_packages;

  void next_token();
  void skip_newlines();
  void set_error(const std::string &msg);

public:
  Parser(Lexer &lex, std::string source_file, std::string base_dir,
         std::shared_ptr<std::set<std::string>> included_files = nullptr,
         std::shared_ptr<std::set<std::string>> imported_packages = nullptr)
      : lexer(lex), source_file(std::move(source_file)), base_dir(std::move(base_dir)),
        included_files(included_files ? std::move(included_files)
                                       : std::make_shared<std::set<std::string>>()),
        imported_packages(imported_packages ? std::move(imported_packages)
                                            : std::make_shared<std::set<std::string>>()) {
    module_root = find_module_root(this->base_dir);
    next_token();
  }

  bool ok() const { return !has_error; }
  const std::string &error() const { return error_msg; }

  std::vector<std::unique_ptr<Decl>> parse_program(std::string known_package = "");

private:
  // Package name of the current file (set by parse_package_decl).
  std::string package_name;

  std::string find_module_root(const std::string &dir);
  bool parse_package_decl();
  bool parse_import_decl(std::vector<std::unique_ptr<Decl>> &decls);
  bool parse_include_decl(std::vector<std::unique_ptr<Decl>> &decls);
  bool parse_namespace_decl(std::vector<std::unique_ptr<Decl>> &decls);
  void prefix_decl_names(std::vector<std::unique_ptr<Decl>> &decls, const std::string &prefix);
  std::unique_ptr<LetDecl> parse_let_decl(bool is_pub);
  std::unique_ptr<FnDecl> parse_fn_decl(bool is_pub);
  std::unique_ptr<FnDecl> parse_extern_fn_decl();
  std::unique_ptr<StructDecl> parse_struct_decl(bool is_pub);
  std::unique_ptr<AdtDecl> parse_enum_decl(bool is_pub);
  std::unique_ptr<TraitDecl> parse_trait_decl(bool is_pub);
  std::unique_ptr<ImplDecl> parse_impl_decl();
  TypeAnnotation parse_type_annotation();
  TypeAnnotation parse_ref_type(); // parse &T or &mut T

  // Statements
  std::vector<std::unique_ptr<Stmt>> parse_block();
  std::unique_ptr<Stmt> parse_stmt();
  std::unique_ptr<LetStmt> parse_let_stmt();
  std::unique_ptr<ReturnStmt> parse_return_stmt();
  std::unique_ptr<IfStmt> parse_if_stmt();
  std::unique_ptr<ForStmt> parse_for_stmt();
  std::unique_ptr<WhileStmt> parse_while_stmt();
  std::unique_ptr<BreakStmt> parse_break_stmt();
  std::unique_ptr<ContinueStmt> parse_continue_stmt();
  std::unique_ptr<Stmt> parse_region_stmt();

  // Expressions
  std::unique_ptr<Expr> parse_expr();
  std::unique_ptr<Expr> parse_if_expr();
  std::unique_ptr<Expr> parse_assignment();
  std::unique_ptr<Expr> parse_logical_or();
  std::unique_ptr<Expr> parse_logical_and();
  std::unique_ptr<Expr> parse_bitwise_or();
  std::unique_ptr<Expr> parse_bitwise_xor();
  std::unique_ptr<Expr> parse_bitwise_and();
  std::unique_ptr<Expr> parse_comparison();
  std::unique_ptr<Expr> parse_shift();
  std::unique_ptr<Expr> parse_additive();
  std::unique_ptr<Expr> parse_multiplicative();
  std::unique_ptr<Expr> parse_unary();
  std::unique_ptr<Expr> parse_primary();
  std::unique_ptr<Expr> parse_postfix(std::unique_ptr<Expr> left);
  std::unique_ptr<Expr> parse_call_rest(const std::string &name);
  std::unique_ptr<Expr> parse_turbofish_call(const std::string &name);
  std::unique_ptr<Expr> parse_array_literal();
  std::unique_ptr<Expr> parse_match_expr();
  std::unique_ptr<Expr> parse_atomic_expr();
  std::unique_ptr<Expr> parse_lambda_expr();

  // Patterns
  std::unique_ptr<Pattern> parse_pattern();

  // Set of known enum variant names and struct names — used to distinguish
  // constructors from other identifier + { patterns (e.g. match arm patterns).
  std::unordered_set<std::string> known_variants;
  std::unordered_set<std::string> known_structs;

  // Names of type parameters in scope (for generic function bodies).
  std::unordered_set<std::string> type_param_names;

  // Source file path for error reporting.
  std::string source_file;

  // Helpers to create AST nodes with source location pre-filled.
  template<typename T, typename... Args>
  std::unique_ptr<T> make_expr(Args&&... args) {
    auto node = std::make_unique<T>(std::forward<Args>(args)...);
    node->line = cur_tok.line;
    node->col = cur_tok.col;
    node->file = source_file;
    return node;
  }

  template<typename T, typename... Args>
  std::unique_ptr<T> make_stmt(Args&&... args) {
    auto node = std::make_unique<T>(std::forward<Args>(args)...);
    node->line = cur_tok.line;
    node->col = cur_tok.col;
    node->file = source_file;
    return node;
  }

  template<typename T, typename... Args>
  std::unique_ptr<T> make_decl(Args&&... args) {
    auto node = std::make_unique<T>(std::forward<Args>(args)...);
    node->line = cur_tok.line;
    node->col = cur_tok.col;
    node->file = source_file;
    return node;
  }

  template<typename T, typename... Args>
  std::unique_ptr<T> make_pattern(Args&&... args) {
    auto node = std::make_unique<T>(std::forward<Args>(args)...);
    node->line = cur_tok.line;
    node->col = cur_tok.col;
    node->file = source_file;
    return node;
  }

  // Shared let helper
  bool parse_let_common(TypeAnnotation &ann, std::string &name,
                        std::unique_ptr<Expr> &init);
};