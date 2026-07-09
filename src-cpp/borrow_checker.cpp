#include "borrow_checker.h"

#include <algorithm>
#include <iostream>
#include <sstream>

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

void BorrowChecker::set_error(const std::string &msg, Expr *expr) {
  if (has_error_) return;
  has_error_ = true;
  error_msg_ = msg;
  (void)expr; // location info TBD
}

// -------------------------------------------------------------------------
// Scope management
// -------------------------------------------------------------------------

void BorrowChecker::enter_scope() {
  current_depth++;
}

void BorrowChecker::exit_scope() {
  // Release all borrows that were created at exactly this depth.
  // We iterate the map and erase entries whose borrows are all at >= depth.
  for (auto it = active_borrows.begin(); it != active_borrows.end(); ) {
    auto &vec = it->second;
    vec.erase(
      std::remove_if(vec.begin(), vec.end(),
                     [this](const BorrowEntry &e) {
                       return e.scope_depth == current_depth;
                     }),
      vec.end());
    if (vec.empty())
      it = active_borrows.erase(it);
    else
      ++it;
  }
  current_depth--;
}

// -------------------------------------------------------------------------
// Variable resolution
// -------------------------------------------------------------------------

std::string BorrowChecker::root_variable(Expr *expr) {
  if (auto *id = dynamic_cast<IdentExpr *>(expr))
    return id->name;
  if (auto *field = dynamic_cast<FieldAccessExpr *>(expr))
    return root_variable(field->object.get());
  if (auto *deref = dynamic_cast<DerefExpr *>(expr))
    return root_variable(deref->operand.get());
  if (auto *sub = dynamic_cast<SubscriptExpr *>(expr))
    return root_variable(sub->array.get());
  if (auto *unary = dynamic_cast<UnaryExpr *>(expr))
    return root_variable(unary->operand.get());
  return "";
}

// -------------------------------------------------------------------------
// Borrow registration and checks
// -------------------------------------------------------------------------

void BorrowChecker::check_shared_borrow(const std::string &var, Expr *expr) {
  auto it = active_borrows.find(var);
  if (it != active_borrows.end()) {
    for (auto &e : it->second) {
      if (e.is_mut) {
        set_error("cannot borrow '" + var + "' as shared because it is already borrowed as mutable", expr);
        return;
      }
    }
  }
  register_shared_borrow(var);
}

void BorrowChecker::check_mut_borrow(const std::string &var, Expr *expr) {
  auto it = active_borrows.find(var);
  if (it != active_borrows.end() && !it->second.empty()) {
    for (auto &e : it->second) {
      if (e.is_mut) {
        set_error("cannot borrow '" + var + "' as mutable because it is already borrowed as mutable", expr);
        return;
      }
    }
    set_error("cannot borrow '" + var + "' as mutable because it is also borrowed as shared", expr);
    return;
  }
  register_mut_borrow(var);
}

void BorrowChecker::register_shared_borrow(const std::string &var) {
  active_borrows[var].push_back({current_depth, false});
}

void BorrowChecker::register_mut_borrow(const std::string &var) {
  active_borrows[var].push_back({current_depth, true});
}

void BorrowChecker::check_var_read(const std::string &var, Expr *expr) {
  auto it = active_borrows.find(var);
  if (it != active_borrows.end()) {
    for (auto &e : it->second) {
      if (e.is_mut) {
        set_error("cannot use '" + var + "' because it is mutably borrowed", expr);
        return;
      }
    }
  }
}

void BorrowChecker::check_var_write(const std::string &var, Expr *expr) {
  auto it = active_borrows.find(var);
  if (it != active_borrows.end() && !it->second.empty()) {
    if (it->second.size() == 1 && !it->second[0].is_mut) {
      set_error("cannot assign to '" + var + "' because it is borrowed", expr);
    } else {
      set_error("cannot assign to '" + var + "' because it is borrowed", expr);
    }
  }
}

// -------------------------------------------------------------------------
// Expression walker
// -------------------------------------------------------------------------

void BorrowChecker::walk_expr(Expr *expr, bool is_lvalue) {
  if (!expr) return;

  if (auto *num = dynamic_cast<NumberExpr *>(expr)) {
    (void)num;
    return;
  }
  if (auto *str = dynamic_cast<StringExpr *>(expr)) {
    (void)str;
    return;
  }
  if (auto *ch = dynamic_cast<CharExpr *>(expr)) {
    (void)ch;
    return;
  }
  if (auto *null = dynamic_cast<NullExpr *>(expr)) {
    (void)null;
    return;
  }
  if (auto *ident = dynamic_cast<IdentExpr *>(expr)) {
    if (is_lvalue)
      check_var_write(ident->name, expr);
    else
      check_var_read(ident->name, expr);
    return;
  }
  if (auto *unary = dynamic_cast<UnaryExpr *>(expr)) {
    walk_expr(unary->operand.get(), false);
    return;
  }
  if (auto *bin = dynamic_cast<BinaryExpr *>(expr)) {
    walk_expr(bin->left.get(), false);
    walk_expr(bin->right.get(), false);
    return;
  }
  if (auto *call = dynamic_cast<CallExpr *>(expr)) {
    // Evaluate callee and arguments
    walk_expr(call->callee_expr.get(), false);
    for (auto &arg : call->args)
      walk_expr(arg.get(), false);
    return;
  }
  if (auto *asm_expr = dynamic_cast<AsmExpr *>(expr)) {
    (void)asm_expr;
    return;
  }
  if (auto *atomic = dynamic_cast<AtomicExpr *>(expr)) {
    for (auto &arg : atomic->args)
      walk_expr(arg.get(), false);
    return;
  }
  if (auto *assign = dynamic_cast<AssignExpr *>(expr)) {
    walk_expr(assign->value.get(), false);
    walk_expr(assign->target.get(), true);
    return;
  }
  if (auto *compound = dynamic_cast<CompoundAssignExpr *>(expr)) {
    walk_expr(compound->value.get(), false);
    walk_expr(compound->target.get(), true);
    return;
  }
  if (auto *borrow = dynamic_cast<BorrowExpr *>(expr)) {
    // Evaluate operand
    walk_expr(borrow->operand.get(), false);
    // Then record the borrow on the root variable
    std::string var = root_variable(borrow->operand.get());
    if (!var.empty()) {
      if (borrow->is_mut)
        check_mut_borrow(var, expr);
      else
        check_shared_borrow(var, expr);
    }
    return;
  }
  if (auto *deref = dynamic_cast<DerefExpr *>(expr)) {
    walk_expr(deref->operand.get(), false);
    return;
  }
  if (auto *sub = dynamic_cast<SubscriptExpr *>(expr)) {
    walk_expr(sub->array.get(), false);
    walk_expr(sub->index.get(), false);
    return;
  }
  if (auto *arr = dynamic_cast<ArrayLitExpr *>(expr)) {
    for (auto &el : arr->elements)
      walk_expr(el.get(), false);
    return;
  }
  if (auto *tup = dynamic_cast<TupleExpr *>(expr)) {
    for (auto &el : tup->elements)
      walk_expr(el.get(), false);
    return;
  }
  if (auto *field = dynamic_cast<FieldAccessExpr *>(expr)) {
    walk_expr(field->object.get(), is_lvalue);
    return;
  }
  if (auto *mcall = dynamic_cast<MethodCallExpr *>(expr)) {
    walk_expr(mcall->object.get(), false);
    for (auto &arg : mcall->args)
      walk_expr(arg.get(), false);
    return;
  }
  if (auto *ctor = dynamic_cast<ConstructorExpr *>(expr)) {
    for (auto &[_, fexpr] : ctor->fields)
      walk_expr(fexpr.get(), false);
    return;
  }
  if (auto *ifexpr = dynamic_cast<IfExpr *>(expr)) {
    walk_expr(ifexpr->condition.get(), false);
    // For if-expressions, conservatively check both branches
    enter_scope();
    walk_expr(ifexpr->then_expr.get(), false);
    exit_scope();
    enter_scope();
    walk_expr(ifexpr->else_expr.get(), false);
    exit_scope();
    return;
  }
  if (auto *closure = dynamic_cast<ClosureExpr *>(expr)) {
    // Closures capture by value for now — no borrow tracking through closures
    // Just check the body with fresh borrow state
    std::map<std::string, std::vector<BorrowEntry>> saved_borrows;
    saved_borrows.swap(active_borrows);
    walk_body(closure->body);
    active_borrows = std::move(saved_borrows);
    return;
  }
}

// -------------------------------------------------------------------------
// Statement walker
// -------------------------------------------------------------------------

void BorrowChecker::walk_stmt(Stmt *stmt) {
  if (!stmt) return;

  if (auto *expr_s = dynamic_cast<ExprStmt *>(stmt)) {
    walk_expr(expr_s->expr.get(), false);
    return;
  }
  if (auto *let = dynamic_cast<LetStmt *>(stmt)) {
    // let x = expr; — the init expression may borrow, but x itself is fresh
    if (let->init_expr)
      walk_expr(let->init_expr.get(), false);
    return;
  }
  if (auto *ret = dynamic_cast<ReturnStmt *>(stmt)) {
    if (ret->value)
      walk_expr(ret->value.get(), false);
    return;
  }
  if (auto *brk = dynamic_cast<BreakStmt *>(stmt)) {
    (void)brk;
    return;
  }
  if (auto *cont = dynamic_cast<ContinueStmt *>(stmt)) {
    (void)cont;
    return;
  }
  if (auto *region = dynamic_cast<RegionStmt *>(stmt)) {
    enter_scope();
    walk_body(region->body);
    exit_scope();
    return;
  }
  if (auto *ifs = dynamic_cast<IfStmt *>(stmt)) {
    walk_expr(ifs->condition.get(), false);
    enter_scope();
    walk_body(ifs->then_branch);
    exit_scope();
    enter_scope();
    walk_body(ifs->else_branch);
    exit_scope();
    return;
  }
  if (auto *for_s = dynamic_cast<ForStmt *>(stmt)) {
    if (for_s->init)
      walk_stmt(for_s->init.get());
    if (for_s->condition)
      walk_expr(for_s->condition.get(), false);
    if (for_s->update)
      walk_expr(for_s->update.get(), false);
    // Loop body — this is conservative: borrows inside the loop persist
    enter_scope();
    walk_body(for_s->body);
    exit_scope();
    return;
  }
}

void BorrowChecker::walk_body(const std::vector<std::unique_ptr<Stmt>> &body) {
  for (auto &stmt : body) {
    walk_stmt(stmt.get());
    if (has_error_) return;
  }
}

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

bool BorrowChecker::check_fn(const std::string &fn_name, FnDecl *decl) {
  has_error_ = false;
  error_msg_.clear();
  active_borrows.clear();
  current_depth = 0;

  // Function parameters enter scope at depth 0
  for (auto &p : decl->params) {
    // params are bindings — they start out uncontested
  }

  walk_body(decl->body);

  if (has_error_) {
    std::cerr << "Borrow check error in function '" << fn_name << "': " << error_msg_ << "\n";
    return false;
  }
  return true;
}

bool BorrowChecker::check_expr(Expr *expr, bool is_lvalue) {
  has_error_ = false;
  error_msg_.clear();
  active_borrows.clear();
  current_depth = 0;
  walk_expr(expr, is_lvalue);
  return !has_error_;
}
