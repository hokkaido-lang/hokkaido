#pragma once

#include <map>
#include <string>
#include <vector>

#include "ast.h"

// =========================================================================
// Hokkaido Language — Borrow Checker
// =========================================================================
//
// A lexical borrow checker that enforces:
//   - At any point, a value may have either one mutable borrow (&mut T)
//     or any number of shared borrows (&T), but not both.
//   - While a value is borrowed, the original cannot be mutated (if
//     immutably borrowed) or used at all (if mutably borrowed).
//   - References cannot outlive the scope of the borrow expression.

class BorrowChecker {
  struct BorrowEntry {
    int scope_depth;
    bool is_mut;
  };

  // Per-variable list of active borrows
  std::map<std::string, std::vector<BorrowEntry>> active_borrows;
  int current_depth = 0;
  bool has_error_ = false;
  std::string error_msg_;

  void set_error(const std::string &msg, Expr *expr = nullptr);
  std::string expr_location(Expr *expr);

  // Scope management
  void enter_scope();
  void exit_scope();

  // Resolve the root variable name from a chain of field accesses, etc.
  // Returns empty string if we can't statically determine it.
  std::string root_variable(Expr *expr);

  // Core borrow checks
  void check_shared_borrow(const std::string &var, Expr *expr);
  void check_mut_borrow(const std::string &var, Expr *expr);
  void register_shared_borrow(const std::string &var);
  void register_mut_borrow(const std::string &var);
  void release_borrows_at_depth(int depth);

  // Check access to a variable (read or write)
  void check_var_read(const std::string &var, Expr *expr);
  void check_var_write(const std::string &var, Expr *expr);

  // AST walkers
  void walk_expr(Expr *expr, bool is_lvalue = false);
  void walk_stmt(Stmt *stmt);
  void walk_body(const std::vector<std::unique_ptr<Stmt>> &body);
  void walk_pattern(Pattern *pat);

public:
  BorrowChecker() = default;

  bool check_fn(const std::string &fn_name, FnDecl *decl);
  bool check_expr(Expr *expr, bool is_lvalue = false);

  bool ok() const { return !has_error_; }
  const std::string &error() const { return error_msg_; }
};
