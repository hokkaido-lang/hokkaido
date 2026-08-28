#include "codegen.h"

#include <iostream>

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// -------------------------------------------------------------------------
// Per-type value generators
// -------------------------------------------------------------------------

Value *CodeGen::eval_int_init(Expr *expr) {
  if (auto *id = dynamic_cast<IdentExpr *>(expr)) {
    auto it = named_values.find(id->name);
    if (it == named_values.end()) {
      cg_error(errs(), id, "undefined variable '" + id->name + "'");
      return nullptr;
    }
    return Builder.CreateLoad(Type::getInt64Ty(Context), it->second, id->name);
  }
  return eval_expr(expr, Type::getInt64Ty(Context));
}

Value *CodeGen::eval_float_init(Expr *expr) {
  return eval_expr(expr, Type::getDoubleTy(Context));
}

Value *CodeGen::eval_string_init(Expr *expr) {
  return eval_expr(expr, PointerType::getUnqual(Context));
}

// -------------------------------------------------------------------------
// Array initialization helpers
// -------------------------------------------------------------------------

Value *CodeGen::eval_array_init(Expr *expr, ArrayType *array_type) {
  if (auto *arr_lit = dynamic_cast<ArrayLitExpr *>(expr)) {
    return eval_array_literal(arr_lit, array_type);
  }
  return ConstantAggregateZero::get(array_type);
}

Value *CodeGen::eval_array_literal(ArrayLitExpr *arr, ArrayType *array_type) {
  Type *elem_type = array_type->getElementType();
  unsigned num_elems = array_type->getNumElements();

  bool all_const = true;
  for (size_t i = 0; i < arr->elements.size() && i < num_elems; i++) {
    if (!dynamic_cast<NumberExpr *>(arr->elements[i].get())) {
      all_const = false;
      break;
    }
  }

  if (all_const) {
    std::vector<Constant *> init_vals;
    for (size_t i = 0; i < arr->elements.size() && i < num_elems; i++) {
      auto *num = static_cast<NumberExpr *>(arr->elements[i].get());
      if (elem_type->isFPOrFPVectorTy())
        init_vals.push_back(ConstantFP::get(elem_type, num->value));
      else
        init_vals.push_back(ConstantInt::get(elem_type, (int64_t)num->value));
    }
    while (init_vals.size() < num_elems)
      init_vals.push_back(Constant::getNullValue(elem_type));
    return ConstantArray::get(array_type, init_vals);
  }

  Value *arr_val = ConstantAggregateZero::get(array_type);
  for (size_t i = 0; i < arr->elements.size() && i < num_elems; i++) {
    Value *el = eval_expr(arr->elements[i].get(), elem_type);
    if (!el) return nullptr;
    arr_val = Builder.CreateInsertValue(arr_val, el, {(unsigned)i}, "arr.init");
  }
  return arr_val;
}
