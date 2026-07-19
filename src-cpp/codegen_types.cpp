#include "codegen.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// -------------------------------------------------------------------------
// LLVM type mapping
// -------------------------------------------------------------------------

Type *CodeGen::get_llvm_type(TypeKind kind) {
  switch (kind) {
    case TypeKind::Void:    return Type::getVoidTy(Context);
    case TypeKind::Int8:    return Type::getInt8Ty(Context);
    case TypeKind::Int16:   return Type::getInt16Ty(Context);
    case TypeKind::Int32:   return Type::getInt32Ty(Context);
    case TypeKind::Int64:   return Type::getInt64Ty(Context);
    case TypeKind::Uint8:   return Type::getInt8Ty(Context);
    case TypeKind::Uint16:  return Type::getInt16Ty(Context);
    case TypeKind::Uint32:  return Type::getInt32Ty(Context);
    case TypeKind::Uint64:  return Type::getInt64Ty(Context);
    case TypeKind::Float16: return Type::getHalfTy(Context);
    case TypeKind::Float32: return Type::getFloatTy(Context);
    case TypeKind::Float64: return Type::getDoubleTy(Context);
    case TypeKind::Bool:    return Type::getInt1Ty(Context);
    case TypeKind::String:  return PointerType::getUnqual(Context);
    case TypeKind::Char:    return Type::getInt8Ty(Context);
    case TypeKind::Cubical: return Type::getInt64Ty(Context);
    case TypeKind::Ref:
    case TypeKind::MutRef:  return PointerType::getUnqual(Context);
    case TypeKind::Tuple:   return nullptr; // must use annotation overload
    case TypeKind::Struct:  return nullptr; // must use annotation overload
    case TypeKind::Slice:
      return nullptr; // must use annotation overload
    case TypeKind::Fn:
      return nullptr; // must use annotation overload
    case TypeKind::TypeParam:
      errs() << "Internal error: unresolved type parameter in codegen\n";
      return nullptr;
  }
  return nullptr;
}

Type *CodeGen::get_llvm_type(const TypeAnnotation &ann) {
  Type *base = nullptr;
  if (ann.kind == TypeKind::Tuple) {
    base = get_tuple_type(ann.tuple_types);
    for (int i = 0; i < ann.pointer_depth; i++)
      base = PointerType::getUnqual(Context);
    return base;
  }
  if (ann.kind == TypeKind::Struct) {
    if (!ann.type_args.empty()) {
      StructType *st = monomorphize_struct(ann.struct_name, ann.type_args);
      if (!st) return nullptr;
      base = st;
      for (int i = 0; i < ann.pointer_depth; i++)
        base = PointerType::getUnqual(Context);
      return base;
    }
    auto it = struct_types.find(ann.struct_name);
    if (it != struct_types.end()) {
      base = it->second;
      for (int i = 0; i < ann.pointer_depth; i++)
        base = PointerType::getUnqual(Context);
      return base;
    }
    auto eit = enum_types.find(ann.struct_name);
    if (eit != enum_types.end()) {
      base = eit->second;
      for (int i = 0; i < ann.pointer_depth; i++)
        base = PointerType::getUnqual(Context);
      return base;
    }
    errs() << "Error: unknown struct/enum type '" << ann.struct_name << "'\n";
    return nullptr;
  }
  if (ann.kind == TypeKind::Slice) {
    return get_slice_type(ann.tuple_types[0]);
  }
  if (ann.kind == TypeKind::Ref || ann.kind == TypeKind::MutRef) {
    // References are just pointers at the LLVM level
    base = PointerType::getUnqual(Context);
    for (int i = 0; i < ann.pointer_depth; i++)
      base = PointerType::getUnqual(Context);
    return base;
  }
  if (ann.kind == TypeKind::Fn) {
    std::vector<Type *> fn_param_types;
    fn_param_types.push_back(PointerType::getUnqual(Context)); // captures context
    for (size_t pi = 0; pi + 1 < ann.tuple_types.size(); pi++)
      fn_param_types.push_back(get_llvm_type(ann.tuple_types[pi]));
    Type *ret_type = get_llvm_type(ann.tuple_types.back());
    FunctionType *fn_type = FunctionType::get(ret_type, fn_param_types, false);
    base = PointerType::getUnqual(Context);
    for (int i = 0; i < ann.pointer_depth; i++)
      base = PointerType::getUnqual(Context);
    return base;
  }
  if (ann.kind == TypeKind::Enum) {
    auto it = enum_types.find(ann.struct_name);
    if (it == enum_types.end()) {
      errs() << "Error: unknown enum type '" << ann.struct_name << "'\n";
      return nullptr;
    }
    base = it->second;
    for (int i = 0; i < ann.pointer_depth; i++)
      base = PointerType::getUnqual(Context);
    return base;
  }
  base = get_llvm_type(ann.kind);
  if (ann.array_size > 0) {
    return ArrayType::get(base, ann.array_size);
  }
  for (int i = 0; i < ann.pointer_depth; i++)
    base = PointerType::getUnqual(Context);
  return base;
}

// -------------------------------------------------------------------------
// Struct helpers
// -------------------------------------------------------------------------

void CodeGen::register_struct_decl(StructDecl *decl) {
  if (!decl->type_params.empty()) {
    struct_templates[decl->name] = decl;
    for (auto &field : decl->fields)
      struct_fields[decl->name].push_back({field.name, field.type_ann});
    return;
  }

  StructType *st = StructType::create(Context, decl->name);

  std::vector<Type *> member_types;
  std::vector<std::pair<std::string, TypeAnnotation>> fields_info;

  for (auto &field : decl->fields) {
    Type *field_type = get_llvm_type(field.type_ann);
    if (!field_type) {
      cg_error(errs(), decl, "invalid field type in struct '" + decl->name + "'");
      return;
    }
    member_types.push_back(field_type);
    fields_info.push_back({field.name, field.type_ann});
  }

  st->setBody(member_types);
  struct_types[decl->name] = st;
  struct_fields[decl->name] = fields_info;
}

int CodeGen::get_struct_field_index(const std::string &struct_name,
                                     const std::string &field_name) {
  auto it = struct_fields.find(struct_name);
  if (it == struct_fields.end()) return -1;
  for (size_t i = 0; i < it->second.size(); i++) {
    if (it->second[i].first == field_name)
      return (int)i;
  }
  return -1;
}

TypeAnnotation CodeGen::get_struct_field_type(const std::string &struct_name,
                                               const std::string &field_name) {
  auto it = struct_fields.find(struct_name);
  if (it == struct_fields.end()) return {TypeKind::Void};
  for (auto &f : it->second) {
    if (f.first == field_name)
      return f.second;
  }
  return {TypeKind::Void};
}

// -------------------------------------------------------------------------
// Enum helpers
// -------------------------------------------------------------------------

void CodeGen::register_enum_decl(AdtDecl *decl) {
  SmallVector<Type *, 8> member_types;
  member_types.push_back(Type::getInt64Ty(Context));
  for (auto &var : decl->variants) {
    SmallVector<Type *, 4> field_types;
    for (auto &f : var.fields) {
      Type *ft = get_llvm_type(f.type_ann);
      if (!ft) ft = Type::getInt64Ty(Context);
      field_types.push_back(ft);
    }
    StructType *var_st = StructType::create(Context, field_types,
                                            decl->name + "::" + var.name);
    member_types.push_back(var_st);
  }
  StructType *st = StructType::create(Context, member_types, decl->name);
  enum_types[decl->name] = st;

  std::vector<std::pair<std::string, std::vector<StructField>>> var_list;
  for (auto &var : decl->variants) {
    std::vector<std::pair<std::string, TypeAnnotation>> fields_info;
    std::vector<StructField> sf_list;
    for (auto &f : var.fields) {
      fields_info.push_back({f.name, f.type_ann});
      sf_list.push_back({f.name, f.type_ann});
    }
    struct_fields[decl->name + "::" + var.name] = fields_info;
    var_list.push_back({var.name, sf_list});
  }
  enum_variants[decl->name] = var_list;
}

int CodeGen::get_enum_variant_index(const std::string &enum_name,
                                     const std::string &variant_name) {
  auto it = enum_variants.find(enum_name);
  if (it == enum_variants.end()) return -1;
  // variant_name may be "Some" or "EnumName::Some" — strip prefix
  std::string short_name = variant_name;
  size_t pos = short_name.rfind("::");
  if (pos != std::string::npos)
    short_name = short_name.substr(pos + 2);
  for (size_t i = 0; i < it->second.size(); i++) {
    if (it->second[i].first == short_name) return (int)i;
  }
  return -1;
}

// -------------------------------------------------------------------------
// Type annotation → string (used for cache keys)
// -------------------------------------------------------------------------

std::string CodeGen::tuple_type_key(const std::vector<TypeAnnotation> &elem_types) {
  std::string key = "tuple";
  for (auto &et : elem_types)
    key += "_" + mangle_ann(et);
  return key;
}

llvm::StructType *CodeGen::get_tuple_type(const std::vector<TypeAnnotation> &elem_types) {
  std::string key = tuple_type_key(elem_types);
  auto it = tuple_type_cache.find(key);
  if (it != tuple_type_cache.end()) return it->second;

  std::vector<Type *> llvm_elem_types;
  for (auto &et : elem_types) {
    Type *t = get_llvm_type(et);
    if (!t) t = Type::getInt64Ty(Context);
    llvm_elem_types.push_back(t);
  }

  StructType *st = StructType::create(Context, llvm_elem_types, key);
  tuple_type_cache[key] = st;
  return st;
}

llvm::StructType *CodeGen::get_slice_type(const TypeAnnotation &elem_ann) {
  Type *elem_type = get_llvm_type(elem_ann);
  std::string key = "slice_" + mangle_ann(elem_ann);
  auto it = slice_type_cache.find(key);
  if (it != slice_type_cache.end()) return it->second;

  Type *ptr = PointerType::getUnqual(Context);
  Type *len = Type::getInt64Ty(Context);
  StructType *st = StructType::create(Context, {ptr, len}, key);
  slice_type_cache[key] = st;
  return st;
}

// -------------------------------------------------------------------------
// Generic struct helpers
// -------------------------------------------------------------------------

std::string CodeGen::struct_mangled_name(const std::string &name,
                                          const std::vector<TypeAnnotation> &type_args) {
  std::string result = name;
  for (auto &ta : type_args)
    result += "$" + mangle_ann(ta);
  return result;
}

llvm::StructType *CodeGen::monomorphize_struct(const std::string &name,
                                                const std::vector<TypeAnnotation> &type_args) {
  std::string mangled = struct_mangled_name(name, type_args);

  auto it = struct_types.find(mangled);
  if (it != struct_types.end()) return it->second;

  auto template_it = struct_templates.find(name);
  if (template_it == struct_templates.end()) {
    errs() << "Error: struct '" << name << "' is not a generic struct\n";
    return nullptr;
  }

  StructDecl *decl = template_it->second;
  StructType *st = StructType::create(Context, mangled);

  std::vector<Type *> member_types;
  std::vector<std::pair<std::string, TypeAnnotation>> fields_info;

  for (auto &field : decl->fields) {
    TypeAnnotation field_ann = field.type_ann;
    substitute_type_params_recursive(field_ann, decl->type_params, type_args);
    Type *field_type = get_llvm_type(field_ann);
    if (!field_type) {
      errs() << "Error: monomorphization failed for field '" << field.name << "' in struct '"
             << name << "'\n";
      return nullptr;
    }
    member_types.push_back(field_type);
    fields_info.push_back({field.name, field_ann});
  }

  st->setBody(member_types);
  struct_types[mangled] = st;
  struct_fields[mangled] = std::move(fields_info);

  return st;
}

void CodeGen::substitute_type_params_recursive(TypeAnnotation &ann,
                                                const std::vector<std::string> &param_names,
                                                const std::vector<TypeAnnotation> &type_args) {
  if (ann.kind == TypeKind::TypeParam) {
    for (size_t i = 0; i < param_names.size(); i++) {
      if (ann.struct_name == param_names[i]) {
        ann = type_args[i];
        return;
      }
    }
  }
  for (auto &ta : ann.tuple_types)
    substitute_type_params_recursive(ta, param_names, type_args);
  for (auto &ta : ann.type_args)
    substitute_type_params_recursive(ta, param_names, type_args);
}
