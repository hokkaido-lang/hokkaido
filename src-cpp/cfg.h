#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ast.h"

// =========================================================================
// Hokkaido Language — Control Flow Graph + Liveness Analysis
// =========================================================================
//
// Builds a CFG from a function body and computes per-node liveness.
// Used by the NLL borrow checker to determine where borrows end.

struct CFGNode {
  int id;
  Stmt *stmt = nullptr; // AST statement (nullptr for synthetic entry/exit)
  Expr *expr = nullptr; // For expression-only nodes (e.g. for-update)

  // Dataflow facts
  std::set<std::string> gen;  // variables read before written at this node
  std::set<std::string> kill; // variables written at this node

  std::vector<int> successors;
  std::vector<int> predecessors;
};

struct CFG {
  std::vector<CFGNode> nodes;
  int entry = -1;
  int exit = -1;

  // Liveness results (indexed by node id)
  std::vector<std::set<std::string>> live_in;
  std::vector<std::set<std::string>> live_out;

  int add_node(Stmt *stmt = nullptr, Expr *expr = nullptr);
  void add_edge(int from, int to);
  void compute_liveness();

  // Find all program points (node ids) where a variable is used (in gen sets).
  // Used by NLL to find the "last use" of a borrow reference.
  std::vector<int> find_uses(const std::string &var) const;

  // Find the last program point where a variable is live (in live_out).
  // Returns -1 if never live.
  int last_live_point(const std::string &var) const;
};

class CFGBuilder {
  CFG &cfg;
  int exit_node = -1;

  // Build statements and return the fall-through node id.
  int build_stmts(const std::vector<std::unique_ptr<Stmt>> &stmts, int entry);

  // Build a single statement. Returns the exit node id for this statement.
  int build_stmt(Stmt *stmt, int entry);

  // Collect gen/kill for a statement's expressions.
  void collect_expr_gen_kill(Expr *expr, std::set<std::string> &gen,
                             std::set<std::string> &kill, bool is_lvalue = false);
  void collect_stmt_gen_kill(Stmt *stmt, std::set<std::string> &gen,
                             std::set<std::string> &kill);

public:
  explicit CFGBuilder(CFG &c) : cfg(c) {}

  // Build CFG for a function body. Returns the CFG with entry/exit set.
  void build(const std::vector<std::unique_ptr<Stmt>> &body);
};
