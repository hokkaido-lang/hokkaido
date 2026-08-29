#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ast.h"
#include "cfg.h"

// =========================================================================
// Hokkaido Language — NLL Borrow Checker
// =========================================================================
//
// Non-lexical lifetime borrow checker that enforces:
//   - At any program point, a value may have either one mutable borrow (&mut T)
//     or any number of shared borrows (&T), but not both.
//   - While a value is borrowed, the original cannot be mutated (if
//     immutably borrowed) or used at all (if mutably borrowed).
//   - Borrows end at their last use point (not end of scope).

class NLLBorrowChecker {
public:
  struct BorrowRegion {
    std::string var_name;     // variable being borrowed (e.g. "a" in &a)
    std::string ref_var;      // variable holding the reference (e.g. "w1" in let w1 = &a)
                               // empty for inline borrows (e.g. foo(&a))
    bool is_mut;
    int create_node;   // CFG node where &x / &mut x is created
    int end_node;      // CFG node where the borrow last matters
    Expr *expr;        // for error reporting
  };

private:
  std::vector<BorrowRegion> borrows;
  bool has_error_ = false;
  std::string error_msg_;

  void set_error(const std::string &msg, Expr *expr = nullptr);

  // Walk AST collecting borrows and mapping each BorrowExpr to its CFG node
  void collect_borrows(Stmt *stmt, CFG &cfg, int node_id);
  void collect_borrows_expr(Expr *expr, CFG &cfg, int node_id,
                            const std::string &borrowed_var, bool is_mut);

  // After collecting borrows, compute lifetimes using liveness
  void compute_borrow_lifetimes(CFG &cfg);

  // Check all borrow rules
  bool check_borrow_rules(CFG &cfg);

public:
  NLLBorrowChecker() = default;
  bool check_fn(const std::string &fn_name, FnDecl *decl);
  bool ok() const { return !has_error_; }
  const std::string &error() const { return error_msg_; }
};
