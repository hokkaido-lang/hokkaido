#include "codegen.h"

#include <filesystem>
#include <fstream>
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
      errs() << "Error: undefined variable '" << id->name << "'\n";
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
// Cubical structured value → LLVM constant
// -------------------------------------------------------------------------

Type *CodeGen::build_cubical_type(const cubical_value::CubicalValue *val) {
  switch (val->kind) {
    case cubical_value::CubicalValue::Nat:
      return Type::getInt64Ty(Context);
    case cubical_value::CubicalValue::Bool:
      return Type::getInt8Ty(Context);
    case cubical_value::CubicalValue::Pair: {
      Type *first = build_cubical_type(val->first.get());
      Type *second = build_cubical_type(val->second.get());
      return StructType::get(Context, {first, second});
    }
    case cubical_value::CubicalValue::Array: {
      if (val->elements.empty())
        return ArrayType::get(Type::getInt64Ty(Context), 0);
      Type *elem = build_cubical_type(val->elements[0].get());
      return ArrayType::get(elem, val->elements.size());
    }
    default:
      return Type::getInt64Ty(Context);
  }
}

Constant *CodeGen::build_cubical_constant(const cubical_value::CubicalValue *val) {
  switch (val->kind) {
    case cubical_value::CubicalValue::Nat:
      return ConstantInt::get(Type::getInt64Ty(Context), val->nat_value);
    case cubical_value::CubicalValue::Bool:
      return ConstantInt::get(Type::getInt8Ty(Context), val->bool_value ? 1 : 0);
    case cubical_value::CubicalValue::Pair: {
      Type *ty = build_cubical_type(val);
      Constant *first = build_cubical_constant(val->first.get());
      Constant *second = build_cubical_constant(val->second.get());
      return ConstantStruct::get(cast<StructType>(ty), {first, second});
    }
    case cubical_value::CubicalValue::Array: {
      std::vector<Constant *> elems;
      for (auto &e : val->elements)
        elems.push_back(build_cubical_constant(e.get()));
      Type *elem_type = elems.empty() ? Type::getInt64Ty(Context) : elems[0]->getType();
      auto *arr_ty = ArrayType::get(elem_type, elems.size());
      return ConstantArray::get(arr_ty, elems);
    }
    default:
      return nullptr;
  }
}

Value *CodeGen::eval_cubical_init(Expr *expr, std::string *debug_out) {
  auto *str = dynamic_cast<StringExpr *>(expr);
  if (!str) {
    errs() << "Error: cubical variable requires a string (inline source or file path)\n";
    return nullptr;
  }

  std::string cubical_source = str->value;

  if (cubical_source.size() >= 4 &&
      (cubical_source.substr(cubical_source.size() - 4) == ".cub")) {
    namespace fs = std::filesystem;
    fs::path file_path(cubical_source);
    if (file_path.is_relative() && !base_dir.empty()) {
      file_path = fs::path(base_dir) / cubical_source;
    }
    std::ifstream ifs(file_path);
    if (!ifs) {
      errs() << "Error: cannot open cubical file '" << file_path.string() << "'\n";
      return nullptr;
    }
    cubical_source.assign((std::istreambuf_iterator<char>(ifs)),
                           std::istreambuf_iterator<char>());
  }

  cubical_value cv(cubical_source);
  if (!cv.valid()) {
    errs() << "Error: cubical evaluation failed: " << cv.error() << "\n";
    return nullptr;
  }

  int64_t int_val = cv.as_int();
  if (int_val >= 0) {
    if (debug_out)
      *debug_out = std::to_string(int_val) + "  (from cubical: " + cv.str() + ")";
    return ConstantInt::get(Type::getInt64Ty(Context), int_val);
  }

  std::string result_str = cv.str();
  {
    auto eq_pos = result_str.find(" = ");
    std::string val_str = (eq_pos != std::string::npos)
                              ? result_str.substr(eq_pos + 3)
                              : result_str;
    while (!val_str.empty() && val_str.front() == ' ') val_str.erase(0, 1);
    if (val_str == "True" || val_str == "true") {
      if (debug_out)
        *debug_out = "true  (from cubical: " + result_str + ")";
      return ConstantInt::get(Type::getInt8Ty(Context), 1);
    }
    if (val_str == "False" || val_str == "false") {
      if (debug_out)
        *debug_out = "false  (from cubical: " + result_str + ")";
      return ConstantInt::get(Type::getInt8Ty(Context), 0);
    }
  }

  {
    auto cv_root = cv.parse_json();
    if (cv_root) {
      Constant *c = build_cubical_constant(cv_root.get());
      if (c) {
        if (debug_out)
          *debug_out = "structured cubical value";
        return c;
      }
    }
  }

  if (debug_out)
    *debug_out = "\"" + result_str + "\"";
  Constant *StrConst = ConstantDataArray::getString(Context, result_str);
  auto *StrGV = new GlobalVariable(M, StrConst->getType(), true,
                                    GlobalVariable::PrivateLinkage,
                                    StrConst, "cubical_result");
  Constant *Zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
  Constant *Indices[] = {Zero, Zero};
  return ConstantExpr::getInBoundsGetElementPtr(StrConst->getType(), StrGV, Indices);
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
