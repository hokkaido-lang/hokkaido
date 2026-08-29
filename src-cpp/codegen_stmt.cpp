#include "codegen.h"

#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// -------------------------------------------------------------------------
// Region lifetime tracking
// -------------------------------------------------------------------------

void CodeGen::track_region_alloc_init(const std::string &name, Expr *init_expr) {
  if (!init_expr) return;
  if (auto *call = dynamic_cast<CallExpr *>(init_expr)) {
    if (call->callee == "__region_alloc" && !call->callee_expr) {
      if (!region_stack.empty())
        region_allocated_vars.insert(name);
    }
  } else if (auto *id = dynamic_cast<IdentExpr *>(init_expr)) {
    if (region_allocated_vars.count(id->name))
      region_allocated_vars.insert(name);
  }
}

bool CodeGen::check_region_lifetime(ReturnStmt *stmt) {
  if (!stmt->value) return true;
  if (auto *id = dynamic_cast<IdentExpr *>(stmt->value.get())) {
    if (region_allocated_vars.count(id->name)) {
      cg_error(errs(), stmt, "cannot return pointer '" + id->name + "' — region will be freed before the function returns", source_text);
      return false;
    }
  }
  return true;
}

// -------------------------------------------------------------------------
// Statements
// -------------------------------------------------------------------------

bool CodeGen::gen_stmt(Stmt *stmt) {
  if (auto *let = dynamic_cast<LetStmt *>(stmt))
    return gen_let_stmt(let);
  if (auto *ret = dynamic_cast<ReturnStmt *>(stmt))
    return gen_return_stmt(ret);
  if (auto *if_ = dynamic_cast<IfStmt *>(stmt))
    return gen_if_stmt(if_);
  if (auto *for_ = dynamic_cast<ForStmt *>(stmt))
    return gen_for_stmt(for_);
  if (auto *while_ = dynamic_cast<WhileStmt *>(stmt))
    return gen_while_stmt(while_);
  if (auto *brk = dynamic_cast<BreakStmt *>(stmt))
    return gen_break_stmt(brk);
  if (auto *cont = dynamic_cast<ContinueStmt *>(stmt))
    return gen_continue_stmt(cont);
  if (auto *region = dynamic_cast<RegionStmt *>(stmt))
    return gen_region_stmt(region);
  if (auto *expr = dynamic_cast<ExprStmt *>(stmt)) {
    Value *v = eval_expr(expr->expr.get(), Type::getInt64Ty(Context));
    return v != nullptr;
  }
  cg_error(errs(), stmt, "unknown statement type", source_text);
  return false;
}

bool CodeGen::gen_return_stmt(ReturnStmt *stmt) {
  Function *fn = Builder.GetInsertBlock()->getParent();
  Type *ret_type = fn->getReturnType();

  // Reject returning region-allocated pointers
  if (!check_region_lifetime(stmt)) return false;

  if (ret_type->isVoidTy()) {
    Builder.CreateRetVoid();
    return true;
  }

  if (!stmt->value) {
    cg_error(errs(), stmt, "non-void function must return a value", source_text);
    return false;
  }

  Value *val = eval_expr(stmt->value.get(), ret_type);
  if (!val) return false;
  Builder.CreateRet(val);
  return true;
}

bool CodeGen::gen_if_stmt(IfStmt *stmt) {
  Function *fn = Builder.GetInsertBlock()->getParent();

  Value *cond = eval_expr(stmt->condition.get(), nullptr);
  if (!cond) return false;
  if (!cond->getType()->isIntegerTy(1))
    cond = Builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0));

  BasicBlock *then_bb = BasicBlock::Create(Context, "if.then", fn);
  BasicBlock *else_bb = BasicBlock::Create(Context, "if.else", fn);
  BasicBlock *merge_bb = BasicBlock::Create(Context, "if.end", fn);

  Builder.CreateCondBr(cond, then_bb, else_bb);

  Builder.SetInsertPoint(then_bb);
  for (auto &s : stmt->then_branch) {
    if (!gen_stmt(s.get())) return false;
  }
  if (!Builder.GetInsertBlock()->getTerminator())
    Builder.CreateBr(merge_bb);

  Builder.SetInsertPoint(else_bb);
  for (auto &s : stmt->else_branch) {
    if (!gen_stmt(s.get())) return false;
  }
  if (!Builder.GetInsertBlock()->getTerminator())
    Builder.CreateBr(merge_bb);

  Builder.SetInsertPoint(merge_bb);
  return true;
}

bool CodeGen::gen_for_stmt(ForStmt *stmt) {
  Function *fn = Builder.GetInsertBlock()->getParent();

  if (stmt->init) {
    if (!gen_stmt(stmt->init.get())) return false;
  }

  BasicBlock *cond_bb = BasicBlock::Create(Context, "for.cond", fn);
  BasicBlock *body_bb = BasicBlock::Create(Context, "for.body", fn);
  BasicBlock *update_bb = BasicBlock::Create(Context, "for.update", fn);
  BasicBlock *end_bb = BasicBlock::Create(Context, "for.end", fn);

  Builder.CreateBr(cond_bb);

  Builder.SetInsertPoint(cond_bb);
  if (stmt->condition) {
    Value *cond = eval_expr(stmt->condition.get(), nullptr);
    if (!cond) return false;
    if (!cond->getType()->isIntegerTy(1))
      cond = Builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0));
    Builder.CreateCondBr(cond, body_bb, end_bb);
  } else {
    Builder.CreateBr(body_bb);
  }

  Builder.SetInsertPoint(body_bb);
  loop_stack.push_back({stmt->label, update_bb, end_bb});
  for (auto &s : stmt->body) {
    if (!gen_stmt(s.get())) return false;
  }
  loop_stack.pop_back();
  if (!Builder.GetInsertBlock()->getTerminator())
    Builder.CreateBr(update_bb);

  Builder.SetInsertPoint(update_bb);
  if (stmt->update) {
    Value *v = eval_expr(stmt->update.get(), Type::getInt64Ty(Context));
    if (!v) return false;
  }
  if (!Builder.GetInsertBlock()->getTerminator())
    Builder.CreateBr(cond_bb);

  Builder.SetInsertPoint(end_bb);
  return true;
}

bool CodeGen::gen_while_stmt(WhileStmt *stmt) {
  Function *fn = Builder.GetInsertBlock()->getParent();

  BasicBlock *cond_bb = BasicBlock::Create(Context, "while.cond", fn);
  BasicBlock *body_bb = BasicBlock::Create(Context, "while.body", fn);
  BasicBlock *end_bb = BasicBlock::Create(Context, "while.end", fn);

  Builder.CreateBr(cond_bb);

  Builder.SetInsertPoint(cond_bb);
  Value *cond = eval_expr(stmt->condition.get(), nullptr);
  if (!cond) return false;
  if (!cond->getType()->isIntegerTy(1))
    cond = Builder.CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0));
  Builder.CreateCondBr(cond, body_bb, end_bb);

  Builder.SetInsertPoint(body_bb);
  loop_stack.push_back({stmt->label, cond_bb, end_bb});
  for (auto &s : stmt->body) {
    if (!gen_stmt(s.get())) return false;
  }
  loop_stack.pop_back();
  if (!Builder.GetInsertBlock()->getTerminator())
    Builder.CreateBr(cond_bb);

  Builder.SetInsertPoint(end_bb);
  return true;
}

bool CodeGen::gen_break_stmt(BreakStmt *stmt) {
  if (stmt->label.empty()) {
    if (loop_stack.empty()) {
      cg_error(errs(), stmt, "'break' outside of loop", source_text);
      return false;
    }
    Builder.CreateBr(loop_stack.back().end_bb);
    return true;
  }
  for (auto it = loop_stack.rbegin(); it != loop_stack.rend(); ++it) {
    if (it->label == stmt->label) {
      Builder.CreateBr(it->end_bb);
      return true;
    }
  }
  cg_error(errs(), stmt, "no loop with label '" + stmt->label + "' for break", source_text);
  return false;
}

bool CodeGen::gen_continue_stmt(ContinueStmt *stmt) {
  if (stmt->label.empty()) {
    if (loop_stack.empty()) {
      cg_error(errs(), stmt, "'continue' outside of loop", source_text);
      return false;
    }
    Builder.CreateBr(loop_stack.back().update_bb);
    return true;
  }
  for (auto it = loop_stack.rbegin(); it != loop_stack.rend(); ++it) {
    if (it->label == stmt->label) {
      Builder.CreateBr(it->update_bb);
      return true;
    }
  }
  cg_error(errs(), stmt, "no loop with label '" + stmt->label + "' for continue", source_text);
  return false;
}

bool CodeGen::gen_region_stmt(RegionStmt *stmt) {
  Function *fn = Builder.GetInsertBlock()->getParent();

  // Allocate a 4096-byte region buffer on the stack
  llvm::Type *i8ty = llvm::Type::getInt8Ty(Context);
  llvm::Type *i64ty = llvm::Type::getInt64Ty(Context);
  llvm::Type *ptr_ty = llvm::PointerType::getUnqual(Context);
  Value *buf_size = llvm::ConstantInt::get(i64ty, 4096);
  AllocaInst *buffer = Builder.CreateAlloca(i8ty, buf_size, "region.buf");

  // Separate alloca for the bump pointer (stores current allocation position)
  AllocaInst *current_ptr_alloca = Builder.CreateAlloca(ptr_ty, nullptr, "region.cur");
  Value *buf_start = Builder.CreatePointerCast(buffer, ptr_ty, "region.start");
  Builder.CreateStore(buf_start, current_ptr_alloca);
  // end_ptr = buffer + 4096
  Value *end_ptr = Builder.CreateGEP(i8ty, buffer, buf_size, "region.end");

  // Create an end block for after the region body
  BasicBlock *end_bb = BasicBlock::Create(Context, "region.end", fn);

  region_stack.push_back({buffer, current_ptr_alloca, end_ptr, end_bb});

  for (auto &s : stmt->body) {
    if (!gen_stmt(s.get())) {
      region_stack.pop_back();
      return false;
    }
  }

  region_stack.pop_back();

  // Branch to end block if not terminated
  if (!Builder.GetInsertBlock()->getTerminator())
    Builder.CreateBr(end_bb);

  Builder.SetInsertPoint(end_bb);
  return true;
}
