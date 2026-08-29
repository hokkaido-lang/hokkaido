#include "borrow_checker.h"
#include "error.h"

#include <algorithm>
#include <iostream>
#include <sstream>

// =========================================================================
// Helpers
// =========================================================================

void NLLBorrowChecker::set_error(const std::string &msg, Expr *expr) {
  if (has_error_) return;
  has_error_ = true;
  if (expr && expr->line > 0)
    error_msg_ = error_at(expr->file, expr->line, expr->col, msg);
  else
    error_msg_ = "error: " + msg;
}

// =========================================================================
// Step 1: Walk AST to find borrow expressions and map them to CFG nodes
// =========================================================================

// For each CFG node, we scan its associated AST to find BorrowExpr nodes.
// The Stmt* or Expr* stored in each CFGNode tells us which AST subtree to scan.

static std::string root_var(Expr *expr) {
  if (!expr) return "";
  if (auto *id = dynamic_cast<IdentExpr *>(expr))
    return id->name;
  if (auto *field = dynamic_cast<FieldAccessExpr *>(expr))
    return root_var(field->object.get());
  if (auto *deref = dynamic_cast<DerefExpr *>(expr))
    return root_var(deref->operand.get());
  if (auto *sub = dynamic_cast<SubscriptExpr *>(expr))
    return root_var(sub->array.get());
  if (auto *unary = dynamic_cast<UnaryExpr *>(expr))
    return root_var(unary->operand.get());
  return "";
}

static void find_borrows_in_expr(
    Expr *expr, int node_id, const std::string &ref_var,
    std::vector<NLLBorrowChecker::BorrowRegion> &borrows) {
  if (!expr) return;

  if (auto *borrow = dynamic_cast<BorrowExpr *>(expr)) {
    std::string var = root_var(borrow->operand.get());
    if (!var.empty()) {
      NLLBorrowChecker::BorrowRegion br;
      br.var_name = var;
      br.ref_var = ref_var; // empty for inline borrows
      br.is_mut = borrow->is_mut;
      br.create_node = node_id;
      br.end_node = node_id; // will be refined
      br.expr = expr;
      borrows.push_back(br);
    }
    // Don't recurse into BorrowExpr operand — we only want the top-level borrow
    return;
  }

  // Recurse into sub-expressions (but NOT into nested borrows)
  if (auto *bin = dynamic_cast<BinaryExpr *>(expr)) {
    find_borrows_in_expr(bin->left.get(), node_id, ref_var, borrows);
    find_borrows_in_expr(bin->right.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *unary = dynamic_cast<UnaryExpr *>(expr)) {
    find_borrows_in_expr(unary->operand.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *call = dynamic_cast<CallExpr *>(expr)) {
    for (auto &arg : call->args)
      find_borrows_in_expr(arg.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *assign = dynamic_cast<AssignExpr *>(expr)) {
    find_borrows_in_expr(assign->value.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *compound = dynamic_cast<CompoundAssignExpr *>(expr)) {
    find_borrows_in_expr(compound->value.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *deref = dynamic_cast<DerefExpr *>(expr)) {
    find_borrows_in_expr(deref->operand.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *sub = dynamic_cast<SubscriptExpr *>(expr)) {
    find_borrows_in_expr(sub->array.get(), node_id, ref_var, borrows);
    find_borrows_in_expr(sub->index.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *field = dynamic_cast<FieldAccessExpr *>(expr)) {
    find_borrows_in_expr(field->object.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *arr = dynamic_cast<ArrayLitExpr *>(expr)) {
    for (auto &el : arr->elements)
      find_borrows_in_expr(el.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *tup = dynamic_cast<TupleExpr *>(expr)) {
    for (auto &el : tup->elements)
      find_borrows_in_expr(el.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *ctor = dynamic_cast<ConstructorExpr *>(expr)) {
    for (auto &[_, fexpr] : ctor->fields)
      find_borrows_in_expr(fexpr.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *ifexpr = dynamic_cast<IfExpr *>(expr)) {
    find_borrows_in_expr(ifexpr->condition.get(), node_id, ref_var, borrows);
    find_borrows_in_expr(ifexpr->then_expr.get(), node_id, ref_var, borrows);
    find_borrows_in_expr(ifexpr->else_expr.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *mcall = dynamic_cast<MethodCallExpr *>(expr)) {
    for (auto &arg : mcall->args)
      find_borrows_in_expr(arg.get(), node_id, ref_var, borrows);
    return;
  }
}

static void find_borrows_in_stmt(
    Stmt *stmt, int node_id, const std::string &ref_var,
    std::vector<NLLBorrowChecker::BorrowRegion> &borrows) {
  if (!stmt) return;

  if (auto *expr_s = dynamic_cast<ExprStmt *>(stmt)) {
    find_borrows_in_expr(expr_s->expr.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *let = dynamic_cast<LetStmt *>(stmt)) {
    if (let->init_expr)
      find_borrows_in_expr(let->init_expr.get(), node_id, let->name, borrows);
    return;
  }
  if (auto *ret = dynamic_cast<ReturnStmt *>(stmt)) {
    if (ret->value)
      find_borrows_in_expr(ret->value.get(), node_id, ref_var, borrows);
    return;
  }
  // IfStmt, ForStmt, WhileStmt, RegionStmt — recurse into bodies
  if (auto *ifs = dynamic_cast<IfStmt *>(stmt)) {
    find_borrows_in_expr(ifs->condition.get(), node_id, ref_var, borrows);
    for (auto &s : ifs->then_branch)
      find_borrows_in_stmt(s.get(), node_id, ref_var, borrows);
    for (auto &s : ifs->else_branch)
      find_borrows_in_stmt(s.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *for_s = dynamic_cast<ForStmt *>(stmt)) {
    if (for_s->init)
      find_borrows_in_stmt(for_s->init.get(), node_id, ref_var, borrows);
    if (for_s->condition)
      find_borrows_in_expr(for_s->condition.get(), node_id, ref_var, borrows);
    if (for_s->update)
      find_borrows_in_expr(for_s->update.get(), node_id, ref_var, borrows);
    for (auto &s : for_s->body)
      find_borrows_in_stmt(s.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *while_s = dynamic_cast<WhileStmt *>(stmt)) {
    if (while_s->condition)
      find_borrows_in_expr(while_s->condition.get(), node_id, ref_var, borrows);
    for (auto &s : while_s->body)
      find_borrows_in_stmt(s.get(), node_id, ref_var, borrows);
    return;
  }
  if (auto *region = dynamic_cast<RegionStmt *>(stmt)) {
    for (auto &s : region->body)
      find_borrows_in_stmt(s.get(), node_id, ref_var, borrows);
    return;
  }
}

void NLLBorrowChecker::collect_borrows(Stmt *stmt, CFG &cfg, int node_id) {
  find_borrows_in_stmt(stmt, node_id, "", borrows);
}

// =========================================================================
// Step 2: Compute borrow lifetimes using liveness
// =========================================================================
//
// For each borrow of variable `v`:
//   - The borrow is created at `create_node`
//   - The borrow's reference is "live" as long as the variable holding the
//     reference is live. For simplicity, we use the liveness of the
//     borrowed variable `v` itself, but bounded to start AFTER create_node.
//   - The borrow ends at the last CFG node where `v` is live after create_node.
//
// This is conservative but correct: if `v` is live somewhere after the
// borrow, we keep the borrow alive until that point. The key NLL benefit
// is that borrows can end BEFORE scope exit.

void NLLBorrowChecker::compute_borrow_lifetimes(CFG &cfg) {
  for (auto &br : borrows) {
    // Determine which variable to track for lifetime:
    // - If stored in a ref_var (let r = &a), track the ref_var's liveness
    // - If inline (foo(&a)), borrow ends at creation node
    const std::string &track_var = br.ref_var.empty() ? br.var_name : br.ref_var;

    if (br.ref_var.empty()) {
      // Inline borrow: the reference is consumed immediately.
      // Borrow lifetime is just the creation node.
      br.end_node = br.create_node;
      continue;
    }

    // Let-bound borrow: find last use of the reference variable
    // after the creation node.
    int last = -1;
    for (auto &node : cfg.nodes) {
      if (node.id < br.create_node) continue;
      if (node.gen.count(track_var) || node.kill.count(track_var) ||
          cfg.live_out[node.id].count(track_var)) {
        last = node.id;
      }
    }
    br.end_node = (last >= 0) ? last : br.create_node;
  }
}

// =========================================================================
// Step 3: Check borrow rules at each program point
// =========================================================================
//
// For each CFG node, we check:
//   - If node N reads variable V, and there's an active borrow of V
//     where create_node < N <= end_node, it's an error if the borrow is mut.
//   - If node N writes variable V, and there's an active borrow of V
//     where create_node < N <= end_node, it's an error.
//   - If node N creates a borrow of V, the read of V at node N is NOT
//     a conflict (that's the borrow creation itself).
//   - Multiple mutable borrows of the same variable that overlap → error.
//   - Shared + mutable borrow overlap → error.

static void collect_var_uses(Expr *expr, std::set<std::string> &reads,
                             std::set<std::string> &writes,
                             bool is_lvalue = false) {
  if (!expr) return;
  if (auto *id = dynamic_cast<IdentExpr *>(expr)) {
    if (is_lvalue) writes.insert(id->name);
    else reads.insert(id->name);
    return;
  }
  if (auto *bin = dynamic_cast<BinaryExpr *>(expr)) {
    collect_var_uses(bin->left.get(), reads, writes, false);
    collect_var_uses(bin->right.get(), reads, writes, false);
    return;
  }
  if (auto *unary = dynamic_cast<UnaryExpr *>(expr)) {
    collect_var_uses(unary->operand.get(), reads, writes, false);
    return;
  }
  if (auto *call = dynamic_cast<CallExpr *>(expr)) {
    collect_var_uses(call->callee_expr.get(), reads, writes, false);
    for (auto &arg : call->args)
      collect_var_uses(arg.get(), reads, writes, false);
    return;
  }
  if (auto *assign = dynamic_cast<AssignExpr *>(expr)) {
    collect_var_uses(assign->value.get(), reads, writes, false);
    collect_var_uses(assign->target.get(), reads, writes, true);
    return;
  }
  if (auto *compound = dynamic_cast<CompoundAssignExpr *>(expr)) {
    collect_var_uses(compound->value.get(), reads, writes, false);
    collect_var_uses(compound->target.get(), reads, writes, false);
    collect_var_uses(compound->target.get(), reads, writes, true);
    return;
  }
  if (auto *deref = dynamic_cast<DerefExpr *>(expr)) {
    collect_var_uses(deref->operand.get(), reads, writes, false);
    return;
  }
  if (auto *sub = dynamic_cast<SubscriptExpr *>(expr)) {
    collect_var_uses(sub->array.get(), reads, writes, false);
    collect_var_uses(sub->index.get(), reads, writes, false);
    return;
  }
  if (auto *field = dynamic_cast<FieldAccessExpr *>(expr)) {
    collect_var_uses(field->object.get(), reads, writes, is_lvalue);
    return;
  }
  if (auto *arr = dynamic_cast<ArrayLitExpr *>(expr)) {
    for (auto &el : arr->elements)
      collect_var_uses(el.get(), reads, writes, false);
    return;
  }
  if (auto *tup = dynamic_cast<TupleExpr *>(expr)) {
    for (auto &el : tup->elements)
      collect_var_uses(el.get(), reads, writes, false);
    return;
  }
  if (auto *ctor = dynamic_cast<ConstructorExpr *>(expr)) {
    for (auto &[_, fexpr] : ctor->fields)
      collect_var_uses(fexpr.get(), reads, writes, false);
    return;
  }
  if (auto *ifexpr = dynamic_cast<IfExpr *>(expr)) {
    collect_var_uses(ifexpr->condition.get(), reads, writes, false);
    collect_var_uses(ifexpr->then_expr.get(), reads, writes, false);
    collect_var_uses(ifexpr->else_expr.get(), reads, writes, false);
    return;
  }
  if (auto *mcall = dynamic_cast<MethodCallExpr *>(expr)) {
    collect_var_uses(mcall->object.get(), reads, writes, false);
    for (auto &arg : mcall->args)
      collect_var_uses(arg.get(), reads, writes, false);
    return;
  }
  if (auto *borrow = dynamic_cast<BorrowExpr *>(expr)) {
    collect_var_uses(borrow->operand.get(), reads, writes, false);
    return;
  }
}

bool NLLBorrowChecker::check_borrow_rules(CFG &cfg) {
  // For each CFG node, gather reads and writes from the AST
  for (auto &node : cfg.nodes) {
    if (node.id == cfg.entry || node.id == cfg.exit) continue;
    if (!node.stmt && !node.expr) continue;

    std::set<std::string> reads, writes;
    if (node.stmt) {
      if (auto *es = dynamic_cast<ExprStmt *>(node.stmt))
        collect_var_uses(es->expr.get(), reads, writes, false);
      else if (auto *let = dynamic_cast<LetStmt *>(node.stmt)) {
        if (let->init_expr)
          collect_var_uses(let->init_expr.get(), reads, writes, false);
        writes.insert(let->name);
      } else if (auto *ret = dynamic_cast<ReturnStmt *>(node.stmt)) {
        if (ret->value)
          collect_var_uses(ret->value.get(), reads, writes, false);
      }
    }
    if (node.expr) {
      // For expr-only nodes (e.g. for-update)
      collect_var_uses(node.expr, reads, writes, false);
    }

    // Check reads against active borrows
    for (auto &var : reads) {
      for (auto &br : borrows) {
        if (br.var_name != var) continue;
        // The borrow is active at this node if:
        //   create_node < node.id <= end_node
        // (create_node is excluded — that's the borrow creation itself)
        if (node.id > br.create_node && node.id <= br.end_node) {
          if (br.is_mut) {
            set_error("cannot use '" + var +
                          "' because it is mutably borrowed here",
                      br.expr);
            return false;
          }
          // Shared borrow: reads are OK
        }
      }
    }

    // Check writes against active borrows
    for (auto &var : writes) {
      for (auto &br : borrows) {
        if (br.var_name != var) continue;
        if (node.id > br.create_node && node.id <= br.end_node) {
          set_error("cannot assign to '" + var +
                        "' because it is borrowed here",
                    br.expr);
          return false;
        }
      }
    }
  }

  // Check for overlapping mutable borrows
  for (size_t i = 0; i < borrows.size(); i++) {
    for (size_t j = i + 1; j < borrows.size(); j++) {
      auto &a = borrows[i];
      auto &b = borrows[j];
      if (a.var_name != b.var_name) continue;
      // Check if their lifetimes overlap
      int a_start = a.create_node + 1;
      int a_end = a.end_node;
      int b_start = b.create_node + 1;
      int b_end = b.end_node;
      if (a_start <= b_end && b_start <= a_end) {
        // Lifetimes overlap
        if (a.is_mut || b.is_mut) {
          set_error("cannot borrow '" + a.var_name +
                        "' because it is also borrowed here",
                    a.is_mut ? a.expr : b.expr);
          return false;
        }
      }
    }
  }

  return true;
}

// =========================================================================
// Closure body checking
// =========================================================================
//
// When we encounter a ClosureExpr during AST walk, check its body as a
// separate function. Closures capture by value, so they get their own
// independent borrow checking scope.

bool NLLBorrowChecker::check_closures_in_expr(Expr *expr) {
  if (!expr) return false;

  if (auto *closure = dynamic_cast<ClosureExpr *>(expr)) {
    NLLBorrowChecker inner;
    if (!inner.check_body("<closure>", closure->body)) {
      has_error_ = true;
      error_msg_ = inner.error();
      return true;
    }
    // Don't recurse into closure bodies — already checked
    return false;
  }

  // Recurse into sub-expressions
  if (auto *bin = dynamic_cast<BinaryExpr *>(expr)) {
    if (check_closures_in_expr(bin->left.get())) return true;
    if (check_closures_in_expr(bin->right.get())) return true;
    return false;
  }
  if (auto *call = dynamic_cast<CallExpr *>(expr)) {
    if (check_closures_in_expr(call->callee_expr.get())) return true;
    for (auto &arg : call->args)
      if (check_closures_in_expr(arg.get())) return true;
    return false;
  }
  if (auto *assign = dynamic_cast<AssignExpr *>(expr)) {
    if (check_closures_in_expr(assign->value.get())) return true;
    return false;
  }
  if (auto *ifexpr = dynamic_cast<IfExpr *>(expr)) {
    if (check_closures_in_expr(ifexpr->condition.get())) return true;
    if (check_closures_in_expr(ifexpr->then_expr.get())) return true;
    if (check_closures_in_expr(ifexpr->else_expr.get())) return true;
    return false;
  }
  if (auto *mcall = dynamic_cast<MethodCallExpr *>(expr)) {
    if (check_closures_in_expr(mcall->object.get())) return true;
    for (auto &arg : mcall->args)
      if (check_closures_in_expr(arg.get())) return true;
    return false;
  }
  return false;
}

bool NLLBorrowChecker::check_closures_in_stmt(Stmt *stmt) {
  if (!stmt) return false;

  if (auto *expr_s = dynamic_cast<ExprStmt *>(stmt))
    return check_closures_in_expr(expr_s->expr.get());
  if (auto *let = dynamic_cast<LetStmt *>(stmt))
    if (let->init_expr)
      return check_closures_in_expr(let->init_expr.get());
  if (auto *ret = dynamic_cast<ReturnStmt *>(stmt))
    if (ret->value)
      return check_closures_in_expr(ret->value.get());
  if (auto *ifs = dynamic_cast<IfStmt *>(stmt)) {
    if (check_closures_in_expr(ifs->condition.get())) return true;
    for (auto &s : ifs->then_branch)
      if (check_closures_in_stmt(s.get())) return true;
    for (auto &s : ifs->else_branch)
      if (check_closures_in_stmt(s.get())) return true;
    return false;
  }
  if (auto *for_s = dynamic_cast<ForStmt *>(stmt)) {
    if (for_s->init && check_closures_in_stmt(for_s->init.get())) return true;
    if (for_s->condition && check_closures_in_expr(for_s->condition.get()))
      return true;
    if (for_s->update && check_closures_in_expr(for_s->update.get()))
      return true;
    for (auto &s : for_s->body)
      if (check_closures_in_stmt(s.get())) return true;
    return false;
  }
  if (auto *while_s = dynamic_cast<WhileStmt *>(stmt)) {
    if (while_s->condition &&
        check_closures_in_expr(while_s->condition.get()))
      return true;
    for (auto &s : while_s->body)
      if (check_closures_in_stmt(s.get())) return true;
    return false;
  }
  return false;
}

// =========================================================================
// Public API
// =========================================================================

bool NLLBorrowChecker::check_body(
    const std::string &name,
    const std::vector<std::unique_ptr<Stmt>> &body) {
  has_error_ = false;
  error_msg_.clear();
  borrows.clear();

  // Step 1: Build CFG
  CFG cfg;
  CFGBuilder builder(cfg);
  builder.build(body);

  // Step 2: Compute liveness
  cfg.compute_liveness();

  // Step 3: Walk CFG nodes to find borrows and map them to nodes.
  // When a ClosureExpr is found, check its body recursively.
  for (auto &node : cfg.nodes) {
    if (node.id == cfg.entry || node.id == cfg.exit) continue;
    if (node.stmt)
      collect_borrows(node.stmt, cfg, node.id);
    if (node.expr)
      find_borrows_in_expr(node.expr, node.id, "", borrows);
  }

  // Step 3b: Check closure bodies found during AST walk
  for (auto &stmt : body) {
    if (check_closures_in_stmt(stmt.get())) {
      if (has_error_) return false;
    }
  }

  // Step 4: Compute borrow lifetimes
  compute_borrow_lifetimes(cfg);

  // Step 5: Check rules
  if (!check_borrow_rules(cfg)) {
    std::cerr << error_msg_ << "\n";
    return false;
  }

  return true;
}

bool NLLBorrowChecker::check_fn(const std::string &fn_name, FnDecl *decl) {
  return check_body(fn_name, decl->body);
}
