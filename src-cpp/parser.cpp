#include "parser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

void Parser::next_token() {
  cur_tok = lexer.next_token();
}

void Parser::skip_newlines() {
  while (cur_tok.type == TokenType::Newline)
    cur_tok = lexer.next_token();
}

void Parser::set_error(const std::string &msg) {
  has_error = true;
  error_msg = msg + " at line " + std::to_string(cur_tok.line) + ":" +
              std::to_string(cur_tok.col);
}

// -------------------------------------------------------------------------
// Module root discovery
// -------------------------------------------------------------------------

std::string Parser::find_module_root(const std::string &dir) {
  namespace fs = std::filesystem;
  fs::path current = dir.empty() ? fs::current_path() : fs::path(dir);
  // Walk up the directory tree looking for hk.mod
  fs::path parent = current;
  while (true) {
    if (fs::exists(parent / "hk.mod"))
      return parent.string();
    fs::path next = parent.parent_path();
    if (next == parent) break; // reached filesystem root
    parent = next;
  }
  // Fall back to the entry file's directory
  return dir.empty() ? fs::current_path().string() : std::string(dir);
}

// -------------------------------------------------------------------------
// Top-level
// -------------------------------------------------------------------------

std::vector<std::unique_ptr<Decl>> Parser::parse_program(std::string known_package) {
  std::vector<std::unique_ptr<Decl>> decls;
  skip_newlines();

  // Parse optional `package name` declaration
  if (cur_tok.type == TokenType::Package) {
    if (!parse_package_decl()) return decls;
    skip_newlines();
  }

  // If this file is being imported (known_package set), verify the package
  // name matches.
  if (!known_package.empty()) {
    if (known_package != package_name) {
      set_error("expected package '" + known_package + "' but file has '" +
                package_name + "'");
      return decls;
    }
  }

  while (cur_tok.type != TokenType::Eof) {
    bool is_pub = false;
    if (cur_tok.type == TokenType::Pub) {
      is_pub = true;
      next_token();
    }
    if (cur_tok.type == TokenType::Let) {
      auto decl = parse_let_decl(is_pub);
      if (decl) decls.push_back(std::move(decl));
      else break;
    } else if (cur_tok.type == TokenType::Fn) {
      auto decl = parse_fn_decl(is_pub);
      if (decl) decls.push_back(std::move(decl));
      else break;
    } else if (cur_tok.type == TokenType::Struct) {
      auto decl = parse_struct_decl(is_pub);
      if (decl) decls.push_back(std::move(decl));
      else break;
    } else if (cur_tok.type == TokenType::Enum) {
      auto decl = parse_enum_decl(is_pub);
      if (decl) decls.push_back(std::move(decl));
      else break;
    } else if (cur_tok.type == TokenType::Include) {
      if (!parse_include_decl(decls)) break;
    } else if (cur_tok.type == TokenType::Import) {
      if (!parse_import_decl(decls)) break;
    } else if (cur_tok.type == TokenType::Namespace) {
      if (!parse_namespace_decl(decls)) break;
    } else if (cur_tok.type == TokenType::Impl) {
      auto decl = parse_impl_decl();
      if (decl) decls.push_back(std::move(decl));
      else break;
    } else if (cur_tok.type == TokenType::Trait) {
      auto decl = parse_trait_decl(is_pub);
      if (decl) decls.push_back(std::move(decl));
      else break;
    } else if (cur_tok.type == TokenType::Extern) {
      auto decl = parse_extern_fn_decl();
      if (decl) decls.push_back(std::move(decl));
      else break;
    } else {
      set_error("expected declaration (let, fn, struct, enum, impl, trait, include, import, namespace, extern)");
      break;
    }
    skip_newlines();
  }
  return decls;
}

// -------------------------------------------------------------------------
// Package declaration
// -------------------------------------------------------------------------

bool Parser::parse_package_decl() {
  next_token(); // consume 'package'
  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected package name after 'package'");
    return false;
  }
  package_name = cur_tok.text;
  next_token();
  return true;
}

// -------------------------------------------------------------------------
// Import declaration
// -------------------------------------------------------------------------

bool Parser::parse_import_decl(std::vector<std::unique_ptr<Decl>> &decls) {
  next_token(); // consume 'import'

  std::string alias;
  std::string path;

  // import alias "path"   or   import "path"
  if (cur_tok.type == TokenType::StringLiteral) {
    path = cur_tok.text;
    next_token();
    // Derive alias from last path component
    size_t slash = path.rfind('/');
    alias = (slash != std::string::npos) ? path.substr(slash + 1) : path;
  } else if (cur_tok.type == TokenType::Identifier) {
    alias = cur_tok.text;
    next_token();
    if (cur_tok.type != TokenType::StringLiteral) {
      set_error("expected string literal path after import alias");
      return false;
    }
    path = cur_tok.text;
    next_token();
  } else {
    set_error("expected string literal path or alias after 'import'");
    return false;
  }

  // Resolve the import path relative to the module root
  namespace fs = std::filesystem;
  fs::path import_dir = fs::path(module_root) / path;

  // Canonicalize to detect duplicate imports
  std::error_code ec;
  fs::path canonical_dir = fs::weakly_canonical(import_dir, ec);
  if (ec) canonical_dir = import_dir;
  std::string dir_key = canonical_dir.string();

  if (!fs::is_directory(import_dir)) {
    set_error("cannot import '" + path + "': not a directory");
    return false;
  }

  // Deduplicate: skip if this package has already been imported
  if (imported_packages->count(dir_key)) {
    return true;
  }
  imported_packages->insert(dir_key);

  // Collect and parse all .hk files in the directory
  std::vector<std::string> hk_files;
  for (auto &entry : fs::directory_iterator(import_dir)) {
    if (entry.path().extension() == ".hk") {
      hk_files.push_back(entry.path().string());
    }
  }

  // Sort for deterministic order
  std::sort(hk_files.begin(), hk_files.end());

  for (auto &file_path : hk_files) {
    std::ifstream ifs(file_path);
    if (!ifs) {
      set_error("cannot open '" + file_path + "' in imported package '" + path + "'");
      return false;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();

    // Derive expected package name from the directory path (last segment)
    size_t last_slash = path.rfind('/');
    std::string expected_pkg = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;

    Lexer sub_lexer(content);
    Parser sub_parser(sub_lexer, fs::path(file_path).parent_path().string(),
                      included_files, imported_packages);
    auto sub_decls = sub_parser.parse_program(expected_pkg);

    if (!sub_parser.ok()) {
      set_error("in imported package '" + path + "': " + sub_parser.error());
      return false;
    }

    // Prefix all declaration names with alias::
    prefix_decl_names(sub_decls, alias);

    for (auto &d : sub_decls) {
      decls.push_back(std::move(d));
    }
  }

  return true;
}

// -------------------------------------------------------------------------
// Prefix declaration names with a namespace prefix
// -------------------------------------------------------------------------

void Parser::prefix_decl_names(std::vector<std::unique_ptr<Decl>> &decls,
                                const std::string &prefix) {
  for (auto &d : decls) {
    if (auto *fn = dynamic_cast<FnDecl *>(d.get())) {
      fn->name = prefix + "::" + fn->name;
    } else if (auto *let = dynamic_cast<LetDecl *>(d.get())) {
      let->name = prefix + "::" + let->name;
    } else if (auto *st = dynamic_cast<StructDecl *>(d.get())) {
      st->name = prefix + "::" + st->name;
    } else if (auto *adt = dynamic_cast<AdtDecl *>(d.get())) {
      adt->name = prefix + "::" + adt->name;
    } else if (auto *tr = dynamic_cast<TraitDecl *>(d.get())) {
      tr->name = prefix + "::" + tr->name;
    } else if (auto *im = dynamic_cast<ImplDecl *>(d.get())) {
      im->trait_name = prefix + "::" + im->trait_name;
      im->type_name = prefix + "::" + im->type_name;
    }
  }
}

// -------------------------------------------------------------------------
// Type annotations
// -------------------------------------------------------------------------

TypeAnnotation Parser::parse_type_annotation() {
  TypeAnnotation ann;
  bool is_linear = false;
  if (cur_tok.type == TokenType::Linear) {
    is_linear = true;
    next_token();
  }
  if (cur_tok.type == TokenType::Void) {
    ann = {TypeKind::Void};
    next_token();
  } else if (cur_tok.type == TokenType::Int8) {
    ann = {TypeKind::Int8};
    next_token();
  } else if (cur_tok.type == TokenType::Int16) {
    ann = {TypeKind::Int16};
    next_token();
  } else if (cur_tok.type == TokenType::Int32) {
    ann = {TypeKind::Int32};
    next_token();
  } else if (cur_tok.type == TokenType::Int64) {
    ann = {TypeKind::Int64};
    next_token();
  } else if (cur_tok.type == TokenType::Uint8) {
    ann = {TypeKind::Uint8};
    next_token();
  } else if (cur_tok.type == TokenType::Uint16) {
    ann = {TypeKind::Uint16};
    next_token();
  } else if (cur_tok.type == TokenType::Uint32) {
    ann = {TypeKind::Uint32};
    next_token();
  } else if (cur_tok.type == TokenType::Uint64) {
    ann = {TypeKind::Uint64};
    next_token();
  } else if (cur_tok.type == TokenType::Float16) {
    ann = {TypeKind::Float16};
    next_token();
  } else if (cur_tok.type == TokenType::Float32) {
    ann = {TypeKind::Float32};
    next_token();
  } else if (cur_tok.type == TokenType::Float64) {
    ann = {TypeKind::Float64};
    next_token();
  } else if (cur_tok.type == TokenType::Bool) {
    ann = {TypeKind::Bool};
    next_token();
  } else if (cur_tok.type == TokenType::String) {
    ann = {TypeKind::String};
    next_token();
  } else if (cur_tok.type == TokenType::Char) {
    ann = {TypeKind::Char};
    next_token();
  } else if (cur_tok.type == TokenType::Cubical) {
    ann = {TypeKind::Cubical};
    next_token();
  } else if (cur_tok.type == TokenType::LParen) {
    // Tuple type: (T1, T2, ...)
    next_token(); // consume '('
    skip_newlines();
    std::vector<TypeAnnotation> elem_types;
    while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
      if (!elem_types.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or ')' in tuple type");
          return ann;
        }
        next_token();
        skip_newlines();
      }
      elem_types.push_back(parse_type_annotation());
      if (has_error) return ann;
      skip_newlines();
    }
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')' to close tuple type");
      return ann;
    }
    next_token(); // consume ')'
    // A 1-tuple (T) is just T, not a tuple
    if (elem_types.size() == 1) {
      ann = elem_types[0];
      ann.is_linear = is_linear;
    } else {
      ann = {TypeKind::Tuple};
      ann.tuple_types = std::move(elem_types);
    }
  } else if (cur_tok.type == TokenType::Fn) {
    // Function type: fn(T1, T2, ...) -> Ret
    next_token(); // consume 'fn'
    if (cur_tok.type != TokenType::LParen) {
      set_error("expected '(' after 'fn' in function type");
      return ann;
    }
    next_token(); // consume '('
    skip_newlines();
    std::vector<TypeAnnotation> fn_param_types;
    while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
      if (!fn_param_types.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or ')' in function type parameter list");
          return ann;
        }
        next_token();
        skip_newlines();
      }
      fn_param_types.push_back(parse_type_annotation());
      if (has_error) return ann;
      skip_newlines();
    }
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')' to close function type parameter list");
      return ann;
    }
    next_token(); // consume ')'
    if (cur_tok.type != TokenType::Arrow) {
      set_error("expected '->' and return type in function type");
      return ann;
    }
    next_token(); // consume '->'
    TypeAnnotation fn_ret_type = parse_type_annotation();
    if (has_error) return ann;
    ann = {TypeKind::Fn};
    ann.is_linear = is_linear;
    ann.tuple_types = std::move(fn_param_types);
    ann.tuple_types.push_back(std::move(fn_ret_type));
  } else if (cur_tok.type == TokenType::Identifier) {
    // Check if this is a type parameter name or Self
    if (cur_tok.text == "Self") {
      ann = {TypeKind::TypeParam};
      ann.struct_name = "Self";
      next_token();
    } else if (type_param_names.find(cur_tok.text) != type_param_names.end()) {
      ann = {TypeKind::TypeParam};
      ann.struct_name = cur_tok.text;
      next_token();
    } else {
      // Struct type: the identifier is the struct name (possibly namespaced,
      // e.g. foo::Point or a::b::Point).
      ann = {TypeKind::Struct};
      ann.struct_name = cur_tok.text;
      next_token();
      while (cur_tok.type == TokenType::ColonColon) {
        next_token(); // consume '::'
        if (cur_tok.type != TokenType::Identifier) {
          set_error("expected identifier after '::' in type name");
          return ann;
        }
        ann.struct_name += "::" + cur_tok.text;
        next_token();
      }
    }
  } else {
    set_error("expected type (void, int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, bool, string, char, cubical, tuple, or struct name)");
    ann = {TypeKind::Int64};
    ann.is_linear = is_linear;
    has_error = true;
    return ann;
  }

  ann.is_linear = is_linear;

  // Handle generic type arguments: Foo<int, float>
  if (cur_tok.type == TokenType::Less && ann.kind == TypeKind::Struct) {
    next_token();
    while (cur_tok.type != TokenType::Greater && cur_tok.type != TokenType::Shr
           && cur_tok.type != TokenType::Eof) {
      if (!ann.type_args.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or '>' in generic type arguments");
          return ann;
        }
        next_token();
      }
      ann.type_args.push_back(parse_type_annotation());
      if (has_error) return ann;
    }
    if (cur_tok.type == TokenType::Shr) {
      cur_tok.type = TokenType::Greater;
      // Don't consume: the caller's level will also see this as its '>'
      return ann;
    }
    if (cur_tok.type != TokenType::Greater) {
      set_error("expected '>' to close generic type arguments");
      return ann;
    }
    next_token();
  }

  // Parse pointer indirection levels (e.g. int* -> pointer to int)
  while (cur_tok.type == TokenType::Star) {
    ann.pointer_depth++;
    next_token();
  }

  // Parse array size (e.g. int[10]) or slice (e.g. int[])
  if (cur_tok.type == TokenType::LSquare) {
    next_token(); // consume '['
    if (cur_tok.type == TokenType::RSquare) {
      // Slice type: T[]
      next_token(); // consume ']'
      TypeAnnotation slice_ann = {TypeKind::Slice};
      slice_ann.tuple_types.push_back(ann);
      return slice_ann;
    }
    if (cur_tok.type != TokenType::Number) {
      set_error("expected array size as number literal");
      return ann;
    }
    ann.array_size = (int)cur_tok.num_val;
    next_token(); // consume number
    if (cur_tok.type != TokenType::RSquare) {
      set_error("expected ']' after array size");
      return ann;
    }
    next_token(); // consume ']'
  }

  return ann;
}

// -------------------------------------------------------------------------
// Struct declarations
// -------------------------------------------------------------------------

std::unique_ptr<StructDecl> Parser::parse_struct_decl(bool is_pub) {
  next_token(); // consume 'struct'

  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected struct name");
    return nullptr;
  }
  std::string name = cur_tok.text;
  next_token();

  std::vector<std::string> type_params;
  std::map<std::string, std::vector<std::string>> type_param_bounds;
  if (cur_tok.type == TokenType::Less) {
    next_token();
    while (cur_tok.type != TokenType::Greater && cur_tok.type != TokenType::Eof) {
      if (!type_params.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or '>' in generic type parameters");
          return nullptr;
        }
        next_token();
      }
      if (cur_tok.type != TokenType::Identifier) {
        set_error("expected type parameter name");
        return nullptr;
      }
      std::string tp_name = cur_tok.text;
      type_params.push_back(tp_name);
      type_param_names.insert(tp_name);
      next_token();

      // Parse optional trait bound: T: TraitName
      if (cur_tok.type == TokenType::Colon) {
        next_token(); // consume ':'
        std::vector<std::string> bounds;
        while (true) {
          if (cur_tok.type != TokenType::Identifier) {
            set_error("expected trait name in type parameter bound");
            return nullptr;
          }
          bounds.push_back(cur_tok.text);
          next_token();
          if (cur_tok.type == TokenType::Plus) {
            next_token();
          } else {
            break;
          }
        }
        type_param_bounds[tp_name] = bounds;
      }
    }
    if (cur_tok.type != TokenType::Greater) {
      set_error("expected '>' to close generic type parameters");
      return nullptr;
    }
    next_token();
  }

  skip_newlines();
  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{' after struct name");
    return nullptr;
  }
  next_token(); // consume '{'
  skip_newlines();

  std::vector<StructField> fields;
  while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
    StructField field;
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected field name");
      return nullptr;
    }
    field.name = cur_tok.text;
    next_token();

    if (cur_tok.type != TokenType::Colon) {
      set_error("expected ':' after field name");
      return nullptr;
    }
    next_token();

    field.type_ann = parse_type_annotation();
    if (has_error) return nullptr;

    fields.push_back(std::move(field));
    skip_newlines();
    if (cur_tok.type == TokenType::Comma) {
      next_token();
      skip_newlines();
    }
  }

  if (cur_tok.type != TokenType::RBrace) {
    set_error("expected '}' after struct fields");
    return nullptr;
  }
  next_token(); // consume '}'

  auto decl = std::make_unique<StructDecl>();
  decl->name = name;
  known_structs.insert(name);
  decl->fields = std::move(fields);
  decl->is_pub = is_pub;
  decl->type_params = std::move(type_params);
  decl->type_param_bounds = std::move(type_param_bounds);
  for (auto &p : decl->type_params)
    type_param_names.erase(p);
  return decl;
}

// -------------------------------------------------------------------------
// Enum declarations
// -------------------------------------------------------------------------

std::unique_ptr<AdtDecl> Parser::parse_enum_decl(bool is_pub) {
  next_token(); // consume 'enum'

  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected enum name");
    return nullptr;
  }
  std::string name = cur_tok.text;
  next_token();

  skip_newlines();
  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{' after enum name");
    return nullptr;
  }
  next_token(); // consume '{'
  skip_newlines();

  std::vector<AdtVariant> variants;
  while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
    AdtVariant variant;
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected variant name");
      return nullptr;
    }
    variant.name = cur_tok.text;
    known_variants.insert(cur_tok.text);
    known_variants.insert(name + "::" + cur_tok.text);
    next_token();
    skip_newlines();

    // Variant with fields: Some { x: int, y: int }
    if (cur_tok.type == TokenType::LBrace) {
      next_token(); // consume '{'
      skip_newlines();
      while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
        StructField field;
        if (cur_tok.type != TokenType::Identifier) {
          set_error("expected field name in variant");
          return nullptr;
        }
        field.name = cur_tok.text;
        next_token();
        if (cur_tok.type != TokenType::Colon) {
          set_error("expected ':' after field name");
          return nullptr;
        }
        next_token();
        field.type_ann = parse_type_annotation();
        if (has_error) return nullptr;
        variant.fields.push_back(std::move(field));
        skip_newlines();
        if (cur_tok.type == TokenType::Comma) {
          next_token();
          skip_newlines();
        }
      }
      if (cur_tok.type != TokenType::RBrace) {
        set_error("expected '}' to close variant fields");
        return nullptr;
      }
      next_token(); // consume '}'
    }
    // else: unit variant (no fields)

    variants.push_back(std::move(variant));
    skip_newlines();
    if (cur_tok.type == TokenType::Comma) {
      next_token();
      skip_newlines();
    }
  }

  if (cur_tok.type != TokenType::RBrace) {
    set_error("expected '}' after enum variants");
    return nullptr;
  }
  next_token(); // consume '}'

  auto decl = std::make_unique<AdtDecl>();
  decl->name = name;
  decl->variants = std::move(variants);
  decl->is_pub = is_pub;
  return decl;
}

// -------------------------------------------------------------------------
// Trait declarations
// -------------------------------------------------------------------------

std::unique_ptr<TraitDecl> Parser::parse_trait_decl(bool is_pub) {
  next_token(); // consume 'trait'

  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected trait name");
    return nullptr;
  }
  std::string name = cur_tok.text;
  known_structs.insert(name); // treat trait names as known for method call resolution
  next_token();

  skip_newlines();
  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{' after trait name");
    return nullptr;
  }
  next_token(); // consume '{'
  skip_newlines();

  std::vector<TraitMethodSig> methods;
  while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
    if (cur_tok.type != TokenType::Fn) {
      set_error("expected 'fn' in trait declaration");
      return nullptr;
    }
    next_token(); // consume 'fn'

    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected method name in trait");
      return nullptr;
    }
    TraitMethodSig sig;
    sig.name = cur_tok.text;
    next_token();

    if (cur_tok.type != TokenType::LParen) {
      set_error("expected '(' after method name in trait");
      return nullptr;
    }
    next_token();

    while (cur_tok.type != TokenType::RParen) {
      if (!sig.params.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or ')' in trait method params");
          return nullptr;
        }
        next_token();
      }
      Param param;
      if (cur_tok.type != TokenType::Identifier) {
        set_error("expected parameter name in trait method");
        return nullptr;
      }
      param.name = cur_tok.text;
      next_token();

      if (cur_tok.type != TokenType::Colon) {
        set_error("expected ':' after parameter name");
        return nullptr;
      }
      next_token();

      param.type_ann = parse_type_annotation();
      if (has_error) return nullptr;
      sig.params.push_back(std::move(param));
    }
    next_token(); // consume ')'

    if (cur_tok.type != TokenType::Arrow) {
      set_error("expected '->' and return type in trait method");
      return nullptr;
    }
    next_token();

    sig.return_type = parse_type_annotation();
    if (has_error) return nullptr;

    methods.push_back(std::move(sig));
    skip_newlines();
  }

  if (cur_tok.type != TokenType::RBrace) {
    set_error("expected '}' to close trait declaration");
    return nullptr;
  }
  next_token(); // consume '}'

  auto decl = std::make_unique<TraitDecl>();
  decl->name = name;
  decl->methods = std::move(methods);
  decl->is_pub = is_pub;
  return decl;
}

// -------------------------------------------------------------------------
// Impl blocks
// -------------------------------------------------------------------------

std::unique_ptr<ImplDecl> Parser::parse_impl_decl() {
  next_token(); // consume 'impl'

  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected type or trait name after 'impl'");
    return nullptr;
  }

  // Read the first identifier (could be trait name or type name)
  std::string first_name = cur_tok.text;
  next_token();

  // Handle namespaced identifiers: a::b::c
  while (cur_tok.type == TokenType::ColonColon) {
    next_token();
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected identifier after '::' in impl");
      return nullptr;
    }
    first_name += "::" + cur_tok.text;
    next_token();
  }

  skip_newlines();

  std::string trait_name;
  std::string type_name;

  if (cur_tok.type == TokenType::For) {
    // Trait impl: impl Trait for Type
    trait_name = first_name;
    next_token(); // consume 'for'
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected type name after 'for' in impl");
      return nullptr;
    }
    type_name = cur_tok.text;
    next_token();
    while (cur_tok.type == TokenType::ColonColon) {
      next_token();
      if (cur_tok.type != TokenType::Identifier) {
        set_error("expected identifier after '::' in impl type");
        return nullptr;
      }
      type_name += "::" + cur_tok.text;
      next_token();
    }
  } else {
    // Inherent impl: impl Type
    type_name = first_name;
  }

  skip_newlines();
  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{' after impl type name");
    return nullptr;
  }
  next_token(); // consume '{'
  skip_newlines();

  std::vector<std::unique_ptr<FnDecl>> methods;
  while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
    bool method_pub = false;
    if (cur_tok.type == TokenType::Pub) {
      method_pub = true;
      next_token();
    }
    if (cur_tok.type != TokenType::Fn) {
      set_error("expected function in impl block");
      return nullptr;
    }
    auto method = parse_fn_decl(method_pub);
    if (!method) return nullptr;
    methods.push_back(std::move(method));
    skip_newlines();
  }

  if (cur_tok.type != TokenType::RBrace) {
    set_error("expected '}' to close impl block");
    return nullptr;
  }
  next_token(); // consume '}'

  auto decl = std::make_unique<ImplDecl>();
  decl->trait_name = trait_name;
  decl->type_name = type_name;
  decl->methods = std::move(methods);
  return decl;
}

// -------------------------------------------------------------------------
// Namespace declarations
// -------------------------------------------------------------------------
//
// Namespaces are resolved entirely here, at parse time: every declaration
// directly inside `namespace foo { ... }` has its name rewritten to
// "foo::name" before being handed back to the caller. Nested namespaces
// fall out for free, since each level only ever prefixes with its own
// name — `namespace a { namespace b { fn f() {} } }` first becomes
// `b::f` when the inner namespace returns, then `a::b::f` when the outer
// one does. Because this happens before codegen ever sees the AST, no
// codegen changes are needed: "a::b::f" is just a function name like any
// other, and `eval_expr`/`M.getFunction`/`struct_types[...]` etc. all key
// on plain strings already.

bool Parser::parse_namespace_decl(std::vector<std::unique_ptr<Decl>> &decls) {
  next_token(); // consume 'namespace'

  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected namespace name");
    return false;
  }
  std::string ns_name = cur_tok.text;
  next_token();

  skip_newlines();
  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{' after namespace name");
    return false;
  }
  next_token(); // consume '{'
  skip_newlines();

  std::vector<std::unique_ptr<Decl>> inner_decls;
  while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
    bool is_pub = false;
    if (cur_tok.type == TokenType::Pub) {
      is_pub = true;
      next_token();
    }
    if (cur_tok.type == TokenType::Let) {
      auto decl = parse_let_decl(is_pub);
      if (!decl) return false;
      inner_decls.push_back(std::move(decl));
    } else if (cur_tok.type == TokenType::Fn) {
      auto decl = parse_fn_decl(is_pub);
      if (!decl) return false;
      inner_decls.push_back(std::move(decl));
    } else if (cur_tok.type == TokenType::Struct) {
      auto decl = parse_struct_decl(is_pub);
      if (!decl) return false;
      inner_decls.push_back(std::move(decl));
    } else if (cur_tok.type == TokenType::Enum) {
      auto decl = parse_enum_decl(is_pub);
      if (!decl) return false;
      inner_decls.push_back(std::move(decl));
    } else if (cur_tok.type == TokenType::Include) {
      if (!parse_include_decl(inner_decls)) return false;
    } else if (cur_tok.type == TokenType::Import) {
      if (!parse_import_decl(inner_decls)) return false;
    } else if (cur_tok.type == TokenType::Impl) {
      auto decl = parse_impl_decl();
      if (!decl) return false;
      inner_decls.push_back(std::move(decl));
    } else if (cur_tok.type == TokenType::Trait) {
      auto decl = parse_trait_decl(is_pub);
      if (!decl) return false;
      inner_decls.push_back(std::move(decl));
    } else if (cur_tok.type == TokenType::Namespace) {
      if (!parse_namespace_decl(inner_decls)) return false;
    } else {
      set_error("expected declaration inside namespace (let, fn, struct, enum, impl, trait, include, import, namespace)");
      return false;
    }
    skip_newlines();
  }

  if (cur_tok.type != TokenType::RBrace) {
    set_error("expected '}' to close namespace '" + ns_name + "'");
    return false;
  }
  next_token(); // consume '}'

  for (auto &d : inner_decls) {
    if (auto *fn = dynamic_cast<FnDecl *>(d.get())) {
      fn->name = ns_name + "::" + fn->name;
    } else if (auto *let = dynamic_cast<LetDecl *>(d.get())) {
      let->name = ns_name + "::" + let->name;
    } else if (auto *st = dynamic_cast<StructDecl *>(d.get())) {
      st->name = ns_name + "::" + st->name;
    } else if (auto *adt = dynamic_cast<AdtDecl *>(d.get())) {
      adt->name = ns_name + "::" + adt->name;
    } else if (auto *tr = dynamic_cast<TraitDecl *>(d.get())) {
      tr->name = ns_name + "::" + tr->name;
    } else if (auto *im = dynamic_cast<ImplDecl *>(d.get())) {
      im->trait_name = ns_name + "::" + im->trait_name;
      im->type_name = ns_name + "::" + im->type_name;
    }
    decls.push_back(std::move(d));
  }
  return true;
}

// -------------------------------------------------------------------------
// Include declarations
// -------------------------------------------------------------------------

bool Parser::parse_include_decl(std::vector<std::unique_ptr<Decl>> &decls) {
  next_token(); // consume 'include'

  if (cur_tok.type != TokenType::StringLiteral) {
    set_error("expected string literal path after 'include'");
    return false;
  }
  std::string raw_path = cur_tok.text;
  next_token();

  namespace fs = std::filesystem;
  fs::path requested(raw_path);
  fs::path full_path = requested.is_absolute()
                            ? requested
                            : fs::path(base_dir.empty() ? "." : base_dir) / requested;

  std::error_code ec;
  fs::path canonical = fs::weakly_canonical(full_path, ec);
  if (ec) canonical = full_path;

  std::ifstream ifs(full_path);
  if (!ifs) {
    set_error("cannot open included file '" + raw_path + "'");
    return false;
  }

  // Skip files we've already pulled in (directly or transitively), so
  // diamond includes don't duplicate declarations and self/mutual
  // includes don't recurse forever.
  std::string key = canonical.string();
  if (included_files->count(key)) {
    return true;
  }
  included_files->insert(key);

  std::stringstream ss;
  ss << ifs.rdbuf();
  std::string content = ss.str();

  Lexer sub_lexer(content);
  Parser sub_parser(sub_lexer, full_path.parent_path().string(), included_files, imported_packages);
  auto sub_decls = sub_parser.parse_program();

  if (!sub_parser.ok()) {
    set_error("in included file '" + raw_path + "': " + sub_parser.error());
    return false;
  }

  for (auto &d : sub_decls) {
    decls.push_back(std::move(d));
  }
  return true;
}

// -------------------------------------------------------------------------
// Let declarations (top-level)
// -------------------------------------------------------------------------

bool Parser::parse_let_common(TypeAnnotation &ann, std::string &name,
                               std::unique_ptr<Expr> &init) {
  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected variable name");
    return false;
  }
  name = cur_tok.text;
  next_token();

  if (cur_tok.type != TokenType::Colon) {
    set_error("expected ':' after variable name");
    return false;
  }
  next_token();

  ann = parse_type_annotation();
  if (has_error) return false;

  if (ann.kind == TypeKind::Void) {
    set_error("variable cannot have void type");
    return false;
  }

  if (cur_tok.type != TokenType::Equals) {
    set_error("expected '=' after type");
    return false;
  }
  next_token();

  init = parse_expr();
  if (!init && !has_error) {
    set_error("expected expression");
    return false;
  }
  return !has_error;
}

std::unique_ptr<LetDecl> Parser::parse_let_decl(bool is_pub) {
  next_token(); // consume 'let'

  TypeAnnotation type_ann;
  std::string name;
  std::unique_ptr<Expr> init;
  if (!parse_let_common(type_ann, name, init)) return nullptr;

  auto decl = std::make_unique<LetDecl>();
  decl->type_ann = type_ann;
  decl->name = name;
  decl->is_pub = is_pub;
  decl->init_expr = std::move(init);
  return decl;
}

// -------------------------------------------------------------------------
// Function declarations
// -------------------------------------------------------------------------

std::unique_ptr<FnDecl> Parser::parse_fn_decl(bool is_pub) {
  next_token(); // consume 'fn'

  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected function name");
    return nullptr;
  }
  std::string name = cur_tok.text;
  next_token();

  // Optional generic type parameter list: <T, U> or <T: Trait, U: Bound>
  std::vector<std::string> type_params;
  std::map<std::string, std::vector<std::string>> type_param_bounds;
  if (cur_tok.type == TokenType::Less) {
    next_token(); // consume '<'
    while (cur_tok.type != TokenType::Greater) {
      if (!type_params.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or '>' in type parameter list");
          return nullptr;
        }
        next_token();
      }
      if (cur_tok.type != TokenType::Identifier) {
        set_error("expected type parameter name");
        return nullptr;
      }
      std::string tp_name = cur_tok.text;
      // Check for duplicate
      if (type_param_names.find(tp_name) != type_param_names.end() ||
          std::find(type_params.begin(), type_params.end(), tp_name) != type_params.end()) {
        set_error("duplicate type parameter '" + tp_name + "'");
        return nullptr;
      }
      type_params.push_back(tp_name);
      next_token();

      // Parse optional trait bound: T: TraitName
      if (cur_tok.type == TokenType::Colon) {
        next_token(); // consume ':'
        std::vector<std::string> bounds;
        while (true) {
          if (cur_tok.type != TokenType::Identifier) {
            set_error("expected trait name in type parameter bound");
            return nullptr;
          }
          bounds.push_back(cur_tok.text);
          next_token();
          // Support T: Trait1 + Trait2 syntax
          if (cur_tok.type == TokenType::Plus) {
            next_token();
          } else {
            break;
          }
        }
        type_param_bounds[tp_name] = bounds;
      }
    }
    next_token(); // consume '>'
  }

  // Push type params into scope for parsing parameter/return types and body
  for (auto &tp : type_params)
    type_param_names.insert(tp);

  if (cur_tok.type != TokenType::LParen) {
    set_error("expected '(' after function name");
    return nullptr;
  }
  next_token();

  std::vector<Param> params;
  while (cur_tok.type != TokenType::RParen) {
    if (!params.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in parameter list");
        return nullptr;
      }
      next_token();
    }
    Param param;
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected parameter name");
      return nullptr;
    }
    param.name = cur_tok.text;
    next_token();

    if (cur_tok.type != TokenType::Colon) {
      set_error("expected ':' after parameter name");
      return nullptr;
    }
    next_token();

    param.type_ann = parse_type_annotation();
    if (has_error) return nullptr;
    params.push_back(std::move(param));
  }
  next_token(); // consume ')'

  if (cur_tok.type != TokenType::Arrow) {
    set_error("expected '->' and return type");
    return nullptr;
  }
  next_token();

  TypeAnnotation return_type = parse_type_annotation();
  if (has_error) return nullptr;

  skip_newlines();
  auto body = parse_block();
  if (has_error) return nullptr;

  auto decl = std::make_unique<FnDecl>();
  decl->name = name;
  decl->params = std::move(params);
  decl->return_type = return_type;
  decl->body = std::move(body);
  decl->type_params = std::move(type_params);
  decl->type_param_bounds = std::move(type_param_bounds);
  decl->is_pub = is_pub;
  // Pop type params from scope
  for (auto &tp : decl->type_params)
    type_param_names.erase(tp);
  return decl;
}

// `extern fn name(params, ...) -> T` declares a foreign symbol (typically
// from a C library) to link against. It has no body — codegen emits an
// LLVM function declaration only, never a definition — and its parameter
// list may end in a bare `...` to mark it variadic (e.g. printf).
std::unique_ptr<FnDecl> Parser::parse_extern_fn_decl() {
  next_token(); // consume 'extern'

  if (cur_tok.type != TokenType::Fn) {
    set_error("expected 'fn' after 'extern'");
    return nullptr;
  }
  next_token(); // consume 'fn'

  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected function name");
    return nullptr;
  }
  std::string name = cur_tok.text;
  next_token();

  if (cur_tok.type != TokenType::LParen) {
    set_error("expected '(' after function name");
    return nullptr;
  }
  next_token();

  std::vector<Param> params;
  bool is_variadic = false;
  while (cur_tok.type != TokenType::RParen) {
    if (!params.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in parameter list");
        return nullptr;
      }
      next_token();
    }
    if (cur_tok.type == TokenType::Ellipsis) {
      next_token(); // consume '...'
      is_variadic = true;
      break; // '...' must be the last thing in the parameter list
    }
    Param param;
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected parameter name or '...'");
      return nullptr;
    }
    param.name = cur_tok.text;
    next_token();

    if (cur_tok.type != TokenType::Colon) {
      set_error("expected ':' after parameter name");
      return nullptr;
    }
    next_token();

    param.type_ann = parse_type_annotation();
    if (has_error) return nullptr;
    params.push_back(std::move(param));
  }

  if (cur_tok.type != TokenType::RParen) {
    set_error("expected ')' after '...'");
    return nullptr;
  }
  next_token(); // consume ')'

  if (cur_tok.type != TokenType::Arrow) {
    set_error("expected '->' and return type");
    return nullptr;
  }
  next_token();

  TypeAnnotation return_type = parse_type_annotation();
  if (has_error) return nullptr;

  auto decl = std::make_unique<FnDecl>();
  decl->name = name;
  decl->params = std::move(params);
  decl->return_type = return_type;
  decl->is_extern = true;
  decl->is_variadic = is_variadic;
  return decl;
}

std::vector<std::unique_ptr<Stmt>> Parser::parse_block() {
  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{'");
    return {};
  }
  next_token(); // consume '{'
  skip_newlines();

  std::vector<std::unique_ptr<Stmt>> stmts;
  while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
    auto stmt = parse_stmt();
    if (!stmt) break;
    stmts.push_back(std::move(stmt));
    skip_newlines();
  }
  if (has_error) return {};

  if (cur_tok.type != TokenType::RBrace) {
    set_error("expected '}'");
    return {};
  }
  next_token(); // consume '}'
  return stmts;
}

std::unique_ptr<Stmt> Parser::parse_stmt() {
  // Labeled statement: 'label: for ...
  if (cur_tok.type == TokenType::Tick) {
    next_token();
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected identifier after label prefix '");
      return nullptr;
    }
    std::string label = cur_tok.text;
    next_token();
    skip_newlines();
    if (cur_tok.type != TokenType::Colon) {
      set_error("expected ':' after label name");
      return nullptr;
    }
    next_token();
    skip_newlines();
    if (cur_tok.type != TokenType::For) {
      set_error("expected 'for' after label");
      return nullptr;
    }
    auto stmt = parse_for_stmt();
    if (stmt)
      static_cast<ForStmt *>(stmt.get())->label = label;
    return stmt;
  }
  if (cur_tok.type == TokenType::Let) {
    return parse_let_stmt();
  }
  if (cur_tok.type == TokenType::Return) {
    return parse_return_stmt();
  }
  if (cur_tok.type == TokenType::If) {
    return parse_if_stmt();
  }
  if (cur_tok.type == TokenType::For) {
    return parse_for_stmt();
  }
  if (cur_tok.type == TokenType::Break) {
    return parse_break_stmt();
  }
  if (cur_tok.type == TokenType::Continue) {
    return parse_continue_stmt();
  }
  if (cur_tok.type == TokenType::Region) {
    return parse_region_stmt();
  }
  auto expr = parse_expr();
  if (!expr) return nullptr;
  return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<LetStmt> Parser::parse_let_stmt() {
  next_token(); // consume 'let'

  TypeAnnotation type_ann;
  std::string name;
  std::unique_ptr<Expr> init;
  if (!parse_let_common(type_ann, name, init)) return nullptr;

  auto stmt = std::make_unique<LetStmt>();
  stmt->type_ann = type_ann;
  stmt->name = name;
  stmt->init_expr = std::move(init);
  return stmt;
}

std::unique_ptr<ReturnStmt> Parser::parse_return_stmt() {
  next_token(); // consume 'return'

  if (cur_tok.type == TokenType::Number ||
      cur_tok.type == TokenType::True ||
      cur_tok.type == TokenType::False ||
      cur_tok.type == TokenType::CharLiteral ||
      cur_tok.type == TokenType::StringLiteral ||
      cur_tok.type == TokenType::Identifier ||
      cur_tok.type == TokenType::Asm ||
      cur_tok.type == TokenType::Match ||
      cur_tok.type == TokenType::Enum ||
      cur_tok.type == TokenType::LParen ||
      cur_tok.type == TokenType::LSquare ||
      cur_tok.type == TokenType::Minus ||
      cur_tok.type == TokenType::BitNot ||
      cur_tok.type == TokenType::Star ||
      cur_tok.type == TokenType::Ampersand ||
      cur_tok.type == TokenType::Null ||
      cur_tok.type == TokenType::Lambda) {
    auto expr = parse_expr();
    if (!expr) return nullptr;
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->value = std::move(expr);
    return stmt;
  }

  return std::make_unique<ReturnStmt>(); // bare return (void)
}

std::unique_ptr<IfStmt> Parser::parse_if_stmt() {
  next_token(); // consume 'if'

  auto cond = parse_expr();
  if (!cond) return nullptr;

  skip_newlines();
  auto then_branch = parse_block();
  if (has_error) return nullptr;

  skip_newlines();
  std::vector<std::unique_ptr<Stmt>> else_branch;
  if (cur_tok.type == TokenType::Else) {
    next_token(); // consume 'else'
    skip_newlines();
    if (cur_tok.type == TokenType::If) {
      // `else if ...` chains: parse the nested if as a single statement
      // rather than requiring a `{ }` block. gen_if_stmt() runs
      // else_branch through gen_stmt(), which dynamic_casts back to
      // IfStmt and recurses, so no codegen changes are needed.
      auto elseif_stmt = parse_if_stmt();
      if (!elseif_stmt) return nullptr;
      else_branch.push_back(std::move(elseif_stmt));
    } else {
      else_branch = parse_block();
      if (has_error) return nullptr;
    }
  }

  auto stmt = std::make_unique<IfStmt>();
  stmt->condition = std::move(cond);
  stmt->then_branch = std::move(then_branch);
  stmt->else_branch = std::move(else_branch);
  return stmt;
}

std::unique_ptr<ForStmt> Parser::parse_for_stmt() {
  next_token(); // consume 'for'
  skip_newlines();

  std::unique_ptr<Stmt> init;
  if (cur_tok.type == TokenType::Let) {
    init = parse_let_stmt();
    if (!init) return nullptr;
  } else if (cur_tok.type != TokenType::Semicolon) {
    auto expr = parse_expr();
    if (!expr) return nullptr;
    init = std::make_unique<ExprStmt>(std::move(expr));
  }

  if (cur_tok.type != TokenType::Semicolon) {
    set_error("expected ';' after for init");
    return nullptr;
  }
  next_token();
  skip_newlines();

  std::unique_ptr<Expr> cond;
  if (cur_tok.type != TokenType::Semicolon) {
    cond = parse_expr();
    if (!cond) return nullptr;
  }

  if (cur_tok.type != TokenType::Semicolon) {
    set_error("expected ';' after for condition");
    return nullptr;
  }
  next_token();
  skip_newlines();

  std::unique_ptr<Expr> update;
  if (cur_tok.type != TokenType::LBrace) {
    update = parse_expr();
    if (!update) return nullptr;
  }

  skip_newlines();
  auto body = parse_block();
  if (has_error) return nullptr;

  auto stmt = std::make_unique<ForStmt>();
  stmt->init = std::move(init);
  stmt->condition = std::move(cond);
  stmt->update = std::move(update);
  stmt->body = std::move(body);
  return stmt;
}

std::unique_ptr<BreakStmt> Parser::parse_break_stmt() {
  next_token(); // consume 'break'
  skip_newlines();
  std::string label;
  if (cur_tok.type == TokenType::Tick) {
    next_token(); // consume '
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected identifier after ' in break");
      return nullptr;
    }
    label = cur_tok.text;
    next_token();
  }
  auto stmt = std::make_unique<BreakStmt>();
  stmt->label = label;
  return stmt;
}

std::unique_ptr<ContinueStmt> Parser::parse_continue_stmt() {
  next_token(); // consume 'continue'
  skip_newlines();
  std::string label;
  if (cur_tok.type == TokenType::Tick) {
    next_token(); // consume '
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected identifier after ' in continue");
      return nullptr;
    }
    label = cur_tok.text;
    next_token();
  }
  auto stmt = std::make_unique<ContinueStmt>();
  stmt->label = label;
  return stmt;
}

std::unique_ptr<Stmt> Parser::parse_region_stmt() {
  next_token(); // consume 'region'

  std::string name;
  if (cur_tok.type == TokenType::Identifier) {
    name = cur_tok.text;
    next_token();
    skip_newlines();
  }

  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{' after region" + (name.empty() ? "" : " '" + name + "'"));
    return nullptr;
  }

  auto body = parse_block();
  if (has_error) return nullptr;

  auto stmt = std::make_unique<RegionStmt>();
  stmt->name = name;
  stmt->body = std::move(body);
  return stmt;
}

// -------------------------------------------------------------------------
// Expressions (recursive descent, operator precedence)
// -------------------------------------------------------------------------

std::unique_ptr<Expr> Parser::parse_expr() {
  return parse_assignment();
}

std::unique_ptr<Expr> Parser::parse_assignment() {
  auto left = parse_logical_or();
  if (!left) return nullptr;

  auto is_lvalue = [](Expr *e) {
    return dynamic_cast<IdentExpr *>(e) ||
           dynamic_cast<DerefExpr *>(e) ||
           dynamic_cast<SubscriptExpr *>(e) ||
           dynamic_cast<FieldAccessExpr *>(e);
  };

  BinOp compound_op;
  bool is_compound = false;
  if (cur_tok.type == TokenType::PlusEq) { compound_op = BinOp::Add; is_compound = true; }
  else if (cur_tok.type == TokenType::MinusEq) { compound_op = BinOp::Sub; is_compound = true; }
  else if (cur_tok.type == TokenType::StarEq) { compound_op = BinOp::Mul; is_compound = true; }
  else if (cur_tok.type == TokenType::SlashEq) { compound_op = BinOp::Div; is_compound = true; }
  else if (cur_tok.type == TokenType::PercentEq) { compound_op = BinOp::Mod; is_compound = true; }
  else if (cur_tok.type == TokenType::AndEq) { compound_op = BinOp::BitAnd; is_compound = true; }
  else if (cur_tok.type == TokenType::OrEq) { compound_op = BinOp::BitOr; is_compound = true; }
  else if (cur_tok.type == TokenType::XorEq) { compound_op = BinOp::Xor; is_compound = true; }
  else if (cur_tok.type == TokenType::ShlEq) { compound_op = BinOp::Shl; is_compound = true; }
  else if (cur_tok.type == TokenType::ShrEq) { compound_op = BinOp::Shr; is_compound = true; }

  if (is_compound) {
    if (!is_lvalue(left.get())) {
      set_error("left side of compound assignment must be a variable, dereference, subscript, or field access");
      return nullptr;
    }
    next_token(); // consume the compound operator
    auto value = parse_assignment();
    if (!value) return nullptr;
    return std::make_unique<CompoundAssignExpr>(std::move(left), compound_op, std::move(value));
  }

  if (cur_tok.type == TokenType::Equals) {
    if (!is_lvalue(left.get())) {
      set_error("left side of assignment must be a variable, dereference, subscript, or field access");
      return nullptr;
    }
    next_token(); // consume '='
    auto value = parse_assignment(); // right-associative
    if (!value) return nullptr;
    return std::make_unique<AssignExpr>(std::move(left), std::move(value));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_logical_or() {
  auto left = parse_logical_and();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::OrOr) {
    next_token();
    auto right = parse_logical_and();
    if (!right) return nullptr;
    left = std::make_unique<BinaryExpr>(std::move(left), BinOp::Or, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_logical_and() {
  auto left = parse_bitwise_or();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::AndAnd) {
    next_token();
    auto right = parse_bitwise_or();
    if (!right) return nullptr;
    left = std::make_unique<BinaryExpr>(std::move(left), BinOp::And, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_bitwise_or() {
  auto left = parse_bitwise_xor();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::BitOr) {
    next_token();
    auto right = parse_bitwise_xor();
    if (!right) return nullptr;
    left = std::make_unique<BinaryExpr>(std::move(left), BinOp::BitOr, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_bitwise_xor() {
  auto left = parse_bitwise_and();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Xor) {
    next_token();
    auto right = parse_bitwise_and();
    if (!right) return nullptr;
    left = std::make_unique<BinaryExpr>(std::move(left), BinOp::Xor, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_bitwise_and() {
  auto left = parse_comparison();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Ampersand) {
    next_token();
    auto right = parse_comparison();
    if (!right) return nullptr;
    left = std::make_unique<BinaryExpr>(std::move(left), BinOp::BitAnd, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_comparison() {
  auto left = parse_shift();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Eq ||
         cur_tok.type == TokenType::Ne ||
         cur_tok.type == TokenType::Less ||
         cur_tok.type == TokenType::Greater ||
         cur_tok.type == TokenType::LessEqual ||
         cur_tok.type == TokenType::GreaterEqual) {
    BinOp op;
    switch (cur_tok.type) {
      case TokenType::Eq:          op = BinOp::Eq; break;
      case TokenType::Ne:          op = BinOp::Ne; break;
      case TokenType::Less:        op = BinOp::Less; break;
      case TokenType::Greater:     op = BinOp::Greater; break;
      case TokenType::LessEqual:   op = BinOp::Le; break;
      case TokenType::GreaterEqual: op = BinOp::Ge; break;
      default: return left;
    }
    next_token();
    auto right = parse_shift();
    if (!right) return nullptr;
    left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_shift() {
  auto left = parse_additive();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Shr || cur_tok.type == TokenType::Shl) {
    BinOp op = (cur_tok.type == TokenType::Shr) ? BinOp::Shr : BinOp::Shl;
    next_token();
    auto right = parse_additive();
    if (!right) return nullptr;
    left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_additive() {
  auto left = parse_multiplicative();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Plus || cur_tok.type == TokenType::Minus) {
    BinOp op = (cur_tok.type == TokenType::Plus) ? BinOp::Add : BinOp::Sub;
    next_token();
    auto right = parse_multiplicative();
    if (!right) return nullptr;
    left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_multiplicative() {
  auto left = parse_unary();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Star || cur_tok.type == TokenType::Slash || cur_tok.type == TokenType::Percent) {
    BinOp op;
    if (cur_tok.type == TokenType::Star) op = BinOp::Mul;
    else if (cur_tok.type == TokenType::Slash) op = BinOp::Div;
    else op = BinOp::Mod;
    next_token();
    auto right = parse_unary();
    if (!right) return nullptr;
    left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_unary() {
  if (cur_tok.type == TokenType::Minus) {
    next_token();
    auto operand = parse_unary();
    if (!operand) return nullptr;
    return std::make_unique<UnaryExpr>(UnaryOp::Neg, std::move(operand));
  }
  if (cur_tok.type == TokenType::BitNot) {
    next_token();
    auto operand = parse_unary();
    if (!operand) return nullptr;
    return std::make_unique<UnaryExpr>(UnaryOp::BitNot, std::move(operand));
  }
  if (cur_tok.type == TokenType::Star) {
    next_token();
    auto operand = parse_unary();
    if (!operand) return nullptr;
    return std::make_unique<DerefExpr>(std::move(operand));
  }
  auto prim = parse_primary();
  if (!prim) return nullptr;
  return parse_postfix(std::move(prim));
}

std::unique_ptr<Expr> Parser::parse_postfix(std::unique_ptr<Expr> left) {
  // Handle function call: expr(args) — expression-based calls (closures)
  if (cur_tok.type == TokenType::LParen) {
    auto call = std::make_unique<CallExpr>();
    call->callee_expr = std::move(left);
    next_token(); // consume '('
    while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
      if (!call->args.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or ')' in argument list");
          return nullptr;
        }
        next_token();
      }
      auto arg = parse_expr();
      if (!arg) return nullptr;
      call->args.push_back(std::move(arg));
    }
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')' to close argument list");
      return nullptr;
    }
    next_token(); // consume ')'
    left = std::move(call);
  }
  // Handle subscript access: arr[i]
  while (cur_tok.type == TokenType::LSquare) {
    next_token(); // consume '['
    auto index = parse_expr();
    if (!index) return nullptr;
    if (cur_tok.type != TokenType::RSquare) {
      set_error("expected ']' after index expression");
      return nullptr;
    }
    next_token(); // consume ']'
    left = std::make_unique<SubscriptExpr>(std::move(left), std::move(index));
  }
  // Handle field access: obj.field or obj.0 (tuple positional access)
  // and method call: obj.method(args)
  while (cur_tok.type == TokenType::Dot) {
    next_token(); // consume '.'
    if (cur_tok.type == TokenType::Number) {
      // Tuple positional access: .0, .1, etc.
      std::string field = std::to_string((int)cur_tok.num_val);
      next_token();
      left = std::make_unique<FieldAccessExpr>(std::move(left), field);
    } else if (cur_tok.type == TokenType::Identifier) {
      std::string name = cur_tok.text;
      next_token();
      // If followed by '(', it's a method call: obj.method(args)
      if (cur_tok.type == TokenType::LParen) {
        auto mcall = std::make_unique<MethodCallExpr>();
        mcall->object = std::move(left);
        mcall->method_name = name;
        next_token(); // consume '('
        while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
          if (!mcall->args.empty()) {
            if (cur_tok.type != TokenType::Comma) {
              set_error("expected ',' or ')' in method arguments");
              return nullptr;
            }
            next_token();
          }
          auto arg = parse_expr();
          if (!arg) return nullptr;
          mcall->args.push_back(std::move(arg));
        }
        if (cur_tok.type != TokenType::RParen) {
          set_error("expected ')' to close method arguments");
          return nullptr;
        }
        next_token(); // consume ')'
        left = std::move(mcall);
      } else {
        left = std::make_unique<FieldAccessExpr>(std::move(left), name);
      }
    } else {
      set_error("expected field name or index after '.'");
      return nullptr;
    }
  }
  // Handle constructor expression: VariantName { field: expr, ... }
  // or positional: VariantName { expr, expr, ... }
  if (cur_tok.type == TokenType::Less || cur_tok.type == TokenType::LBrace) {
    auto *ident = dynamic_cast<IdentExpr *>(left.get());
    if (ident && (known_variants.count(ident->name) > 0 || known_structs.count(ident->name) > 0)) {
      std::string variant_name = ident->name;
      std::vector<TypeAnnotation> ctor_type_args;
      if (cur_tok.type == TokenType::Less) {
        next_token();
        while (cur_tok.type != TokenType::Greater && cur_tok.type != TokenType::Shr
               && cur_tok.type != TokenType::Eof) {
          if (!ctor_type_args.empty()) {
            if (cur_tok.type != TokenType::Comma) {
              set_error("expected ',' or '>' in generic type arguments");
              return nullptr;
            }
            next_token();
          }
          ctor_type_args.push_back(parse_type_annotation());
          if (has_error) return nullptr;
        }
        if (cur_tok.type == TokenType::Shr) {
          cur_tok.type = TokenType::Greater;
        } else {
          if (cur_tok.type != TokenType::Greater) {
            set_error("expected '>' to close generic type arguments");
            return nullptr;
          }
          next_token();
        }
        if (cur_tok.type != TokenType::LBrace) {
          set_error("expected '{' after generic type arguments");
          return nullptr;
        }
      }
      next_token(); // consume '{'
      skip_newlines();
      auto ctor = std::make_unique<ConstructorExpr>();
      ctor->variant_name = variant_name;
      ctor->type_args = std::move(ctor_type_args);
      bool is_named = false;
      bool has_fields = false;
      while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
        std::string field_name;
        std::unique_ptr<Expr> field_val;
        if (cur_tok.type == TokenType::Identifier) {
          std::string possible_name = cur_tok.text;
          next_token();
          skip_newlines();
          if (cur_tok.type == TokenType::Colon) {
            // Named field
            is_named = true;
            field_name = possible_name;
            next_token(); // consume ':'
            skip_newlines();
            field_val = parse_expr();
            if (!field_val) return nullptr;
          } else {
            // Positional: the identifier is an expression
            if (is_named && has_fields) {
              set_error("cannot mix named and positional fields");
              return nullptr;
            }
            field_val = std::make_unique<IdentExpr>(possible_name);
          }
        } else {
          if (is_named && has_fields) {
            set_error("cannot mix named and positional fields");
            return nullptr;
          }
          field_val = parse_expr();
          if (!field_val) return nullptr;
        }
        ctor->fields.push_back({field_name, std::move(field_val)});
        has_fields = true;
        skip_newlines();
        if (cur_tok.type == TokenType::Comma) {
          next_token();
          skip_newlines();
        }
      }
      if (cur_tok.type != TokenType::RBrace) {
        set_error("expected '}' to close constructor");
        return nullptr;
      }
      next_token(); // consume '}'
      return ctor;
    }
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_primary() {
  if (cur_tok.type == TokenType::True) {
    auto expr = std::make_unique<NumberExpr>(cur_tok.num_val);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::False) {
    auto expr = std::make_unique<NumberExpr>(cur_tok.num_val);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::Number) {
    auto expr = std::make_unique<NumberExpr>(cur_tok.num_val);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::CharLiteral) {
    auto expr = std::make_unique<CharExpr>((uint8_t)cur_tok.num_val);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::StringLiteral) {
    auto expr = std::make_unique<StringExpr>(cur_tok.text);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::Null) {
    next_token();
    return std::make_unique<NullExpr>();
  }
  if (cur_tok.type == TokenType::Ampersand) {
    next_token();
    auto operand = parse_unary();
    if (!operand) return nullptr;
    return std::make_unique<AddressOfExpr>(std::move(operand));
  }
  if (cur_tok.type == TokenType::LSquare) {
    return parse_array_literal();
  }
  if (cur_tok.type == TokenType::Identifier) {
    std::string name = cur_tok.text;
    next_token();
    while (cur_tok.type == TokenType::ColonColon) {
      next_token(); // consume '::'
      // Turbofish: fn::<type1, type2>(args)
      if (cur_tok.type == TokenType::Less) {
        return parse_turbofish_call(name);
      }
      if (cur_tok.type != TokenType::Identifier) {
        set_error("expected identifier after '::'");
        return nullptr;
      }
      name += "::" + cur_tok.text;
      next_token();
    }
    if (cur_tok.type == TokenType::LParen) {
      return parse_call_rest(name);
    }
    return std::make_unique<IdentExpr>(name);
  }
  if (cur_tok.type == TokenType::Asm) {
    next_token(); // consume 'asm'
    if (cur_tok.type != TokenType::LParen) {
      set_error("expected '(' after asm");
      return nullptr;
    }
    next_token(); // consume '('
    if (cur_tok.type != TokenType::StringLiteral) {
      set_error("expected string literal in asm");
      return nullptr;
    }
    auto expr = std::make_unique<AsmExpr>();
    expr->asm_code = cur_tok.text;
    next_token();
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')'");
      return nullptr;
    }
    next_token(); // consume ')'
    return expr;
  }
  if (cur_tok.type == TokenType::LParen) {
    next_token();
    skip_newlines();
    auto first = parse_expr();
    if (!first) return nullptr;
    skip_newlines();
    // If followed by a comma, this is a tuple expression
    if (cur_tok.type == TokenType::Comma) {
      auto tup = std::make_unique<TupleExpr>();
      tup->elements.push_back(std::move(first));
      while (cur_tok.type == TokenType::Comma) {
        next_token(); // consume ','
        skip_newlines();
        auto el = parse_expr();
        if (!el) return nullptr;
        tup->elements.push_back(std::move(el));
        skip_newlines();
      }
      if (cur_tok.type != TokenType::RParen) {
        set_error("expected ')' to close tuple expression");
        return nullptr;
      }
      next_token(); // consume ')'
      return tup;
    }
    // Single expression in parens
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')'");
      return nullptr;
    }
    next_token();
    return first;
  }
  if (cur_tok.type == TokenType::Match) {
    return parse_match_expr();
  }
  if (cur_tok.type == TokenType::If) {
    return parse_if_expr();
  }
  if (cur_tok.type == TokenType::Atomic) {
    return parse_atomic_expr();
  }
  if (cur_tok.type == TokenType::Lambda) {
    return parse_lambda_expr();
  }
  set_error("expected expression");
  return nullptr;
}

std::unique_ptr<Expr> Parser::parse_if_expr() {
  next_token(); // consume 'if'
  auto cond = parse_expr();
  if (!cond) return nullptr;
  skip_newlines();

  // if-expression body is a single expression or a block { ... }
  std::unique_ptr<Expr> then_expr;
  if (cur_tok.type == TokenType::LBrace) {
    auto block_stmts = parse_block();
    if (has_error || block_stmts.empty()) return nullptr;
    // Wrap the last statement as an expression, or use the single expression
    then_expr = std::make_unique<NumberExpr>(0);
    // For blocks, we need to pick the last expression-stmt as the value.
    // Currently blocks produce void — this is a simplification.
    // We evaluate the last expr-stmt's expression.
    if (auto *es = dynamic_cast<ExprStmt *>(block_stmts.back().get())) {
      then_expr = std::move(es->expr);
      block_stmts.pop_back();
    }
    // If there are statements before the final expression, they're side effects.
    // For now, if-expression blocks must be a single expression or end with one.
    if (!block_stmts.empty()) {
      // Still use the last expression as the value; previous stmts are side-effectful
    }
  } else {
    then_expr = parse_expr();
    if (!then_expr) return nullptr;
  }

  skip_newlines();
  if (cur_tok.type != TokenType::Else) {
    set_error("expected 'else' for if-expression");
    return nullptr;
  }
  next_token(); // consume 'else'
  skip_newlines();

  std::unique_ptr<Expr> else_expr;
  if (cur_tok.type == TokenType::If) {
    else_expr = parse_if_expr();
    if (!else_expr) return nullptr;
  } else if (cur_tok.type == TokenType::LBrace) {
    auto block_stmts = parse_block();
    if (has_error || block_stmts.empty()) return nullptr;
    else_expr = std::make_unique<NumberExpr>(0);
    if (auto *es = dynamic_cast<ExprStmt *>(block_stmts.back().get())) {
      else_expr = std::move(es->expr);
      block_stmts.pop_back();
    }
  } else {
    else_expr = parse_expr();
    if (!else_expr) return nullptr;
  }

  auto expr = std::make_unique<IfExpr>();
  expr->condition = std::move(cond);
  expr->then_expr = std::move(then_expr);
  expr->else_expr = std::move(else_expr);
  return expr;
}

std::unique_ptr<Expr> Parser::parse_array_literal() {
  next_token(); // consume '['
  std::vector<std::unique_ptr<Expr>> elements;
  while (cur_tok.type != TokenType::RSquare) {
    if (!elements.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ']' in array literal");
        return nullptr;
      }
      next_token();
    }
    auto el = parse_expr();
    if (!el) return nullptr;
    elements.push_back(std::move(el));
  }
  next_token(); // consume ']'
  auto expr = std::make_unique<ArrayLitExpr>();
  expr->elements = std::move(elements);
  return expr;
}

std::unique_ptr<Expr> Parser::parse_call_rest(const std::string &name) {
  next_token(); // consume '('

  std::vector<std::unique_ptr<Expr>> args;
  while (cur_tok.type != TokenType::RParen) {
    if (!args.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in arguments");
        return nullptr;
      }
      next_token();
    }
    auto arg = parse_expr();
    if (!arg) return nullptr;
    args.push_back(std::move(arg));
  }
  next_token(); // consume ')'

  auto expr = std::make_unique<CallExpr>();
  expr->callee = name;
  expr->args = std::move(args);
  return expr;
}

// Parse a turbofish generic call: name::<type1, type2>(args...)
// The current token is '<' (consumed after '::' by parse_primary).
std::unique_ptr<Expr> Parser::parse_turbofish_call(const std::string &name) {
  next_token(); // consume '<'

  std::vector<TypeAnnotation> type_args;
  while (cur_tok.type != TokenType::Greater) {
    if (!type_args.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or '>' in type arguments");
        return nullptr;
      }
      next_token();
    }
    TypeAnnotation ta = parse_type_annotation();
    if (has_error) return nullptr;
    type_args.push_back(ta);
  }
  next_token(); // consume '>'

  if (cur_tok.type != TokenType::LParen) {
    set_error("expected '(' after type arguments in generic call");
    return nullptr;
  }
  next_token(); // consume '('

  std::vector<std::unique_ptr<Expr>> args;
  while (cur_tok.type != TokenType::RParen) {
    if (!args.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in arguments");
        return nullptr;
      }
      next_token();
    }
    auto arg = parse_expr();
    if (!arg) return nullptr;
    args.push_back(std::move(arg));
  }
  next_token(); // consume ')'

  auto expr = std::make_unique<CallExpr>();
  expr->callee = name;
  expr->args = std::move(args);
  expr->type_args = std::move(type_args);
  return expr;
}

std::unique_ptr<Expr> Parser::parse_match_expr() {
  next_token(); // consume 'match'
  auto value = parse_expr();
  if (!value) return nullptr;
  skip_newlines();
  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{' after match expression");
    return nullptr;
  }
  next_token(); // consume '{'
  skip_newlines();

  auto mexpr = std::make_unique<MatchExpr>();
  mexpr->value = std::move(value);

  while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
    auto pattern = parse_pattern();
    if (!pattern) return nullptr;
    skip_newlines();
    if (cur_tok.type != TokenType::FatArrow) {
      set_error("expected '=>' after pattern in match arm");
      return nullptr;
    }
    next_token(); // consume '=>'
    skip_newlines();
    auto body = parse_expr();
    if (!body) return nullptr;
    MatchArm arm;
    arm.pattern = std::move(pattern);
    arm.expr = std::move(body);
    mexpr->arms.push_back(std::move(arm));
    skip_newlines();
    // Optional comma separator between arms
    if (cur_tok.type == TokenType::Comma) {
      next_token();
      skip_newlines();
    }
  }
  if (cur_tok.type != TokenType::RBrace) {
    set_error("expected '}' to close match");
    return nullptr;
  }
  next_token(); // consume '}'
  return mexpr;
}

std::unique_ptr<Expr> Parser::parse_lambda_expr() {
  next_token(); // consume 'lambda'

  if (cur_tok.type != TokenType::LParen) {
    set_error("expected '(' after 'lambda'");
    return nullptr;
  }
  next_token(); // consume '('

  auto closure = std::make_unique<ClosureExpr>();
  while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
    if (!closure->params.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in lambda parameter list");
        return nullptr;
      }
      next_token();
    }
    Param param;
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected parameter name in lambda");
      return nullptr;
    }
    param.name = cur_tok.text;
    next_token();

    if (cur_tok.type != TokenType::Colon) {
      set_error("expected ':' after lambda parameter name");
      return nullptr;
    }
    next_token();

    param.type_ann = parse_type_annotation();
    if (has_error) return nullptr;
    closure->params.push_back(std::move(param));
  }
  next_token(); // consume ')'

  if (cur_tok.type != TokenType::Arrow) {
    set_error("expected '->' and return type in lambda");
    return nullptr;
  }
  next_token();

  closure->return_type = parse_type_annotation();
  if (has_error) return nullptr;

  skip_newlines();
  closure->body = parse_block();
  if (has_error) return nullptr;

  return closure;
}

std::unique_ptr<Expr> Parser::parse_atomic_expr() {
  next_token(); // consume 'atomic'

  if (cur_tok.type != TokenType::LParen) {
    set_error("expected '(' in atomic expression");
    return nullptr;
  }
  next_token(); // consume '('

  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected atomic operation name (xchg, add, sub, and, or, xor, cas, fence)");
    return nullptr;
  }
  std::string op_name = cur_tok.text;
  next_token();

  AtomicOp op;
  if (op_name == "xchg") op = AtomicOp::Xchg;
  else if (op_name == "add") op = AtomicOp::Add;
  else if (op_name == "sub") op = AtomicOp::Sub;
  else if (op_name == "and") op = AtomicOp::And;
  else if (op_name == "or") op = AtomicOp::Or;
  else if (op_name == "xor") op = AtomicOp::Xor;
  else if (op_name == "cas") op = AtomicOp::CmpXchg;
  else if (op_name == "fence") op = AtomicOp::Fence;
  else {
    set_error("unknown atomic operation '" + op_name + "'");
    return nullptr;
  }

  // Expect comma before first argument (separates op name from args)
  if (cur_tok.type == TokenType::Comma) {
    next_token(); // consume comma
  }

  std::vector<std::unique_ptr<Expr>> args;
  while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
    if (!args.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in atomic args");
        return nullptr;
      }
      next_token();
    }
    auto arg = parse_expr();
    if (!arg) return nullptr;
    args.push_back(std::move(arg));
  }

  if (cur_tok.type != TokenType::RParen) {
    set_error("expected ')' after atomic arguments");
    return nullptr;
  }
  next_token(); // consume ')'

  auto expr = std::make_unique<AtomicExpr>();
  expr->op = op;
  expr->args = std::move(args);
  return expr;
}

std::unique_ptr<Pattern> Parser::parse_pattern() {
  if (cur_tok.type == TokenType::Identifier) {
    std::string name = cur_tok.text;
    next_token();
    // Wildcard
    if (name == "_")
      return std::make_unique<WildcardPattern>();
    // Qualified enum variant: EnumName::VariantName or VariantName
    while (cur_tok.type == TokenType::ColonColon) {
      next_token(); // consume '::'
      if (cur_tok.type != TokenType::Identifier) {
        set_error("expected variant name after '::'");
        return nullptr;
      }
      name += "::" + cur_tok.text;
      next_token();
    }
    // Struct pattern: Identifier { ... }
    skip_newlines();
    if (cur_tok.type == TokenType::LBrace) {
      next_token(); // consume '{'
      skip_newlines();
      auto pat = std::make_unique<StructPattern>();
      pat->struct_name = name;
      while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
        if (cur_tok.type != TokenType::Identifier) {
          set_error("expected field name in struct pattern");
          return nullptr;
        }
        std::string field_name = cur_tok.text;
        next_token();
        skip_newlines();
        std::unique_ptr<Pattern> field_pat;
        if (cur_tok.type == TokenType::Colon) {
          next_token(); // consume ':'
          skip_newlines();
          field_pat = parse_pattern();
          if (!field_pat) return nullptr;
        } else {
          // Shorthand: field_name acts as both the field name and variable binding
          field_pat = std::make_unique<VariablePattern>(field_name);
        }
        pat->fields.push_back({field_name, std::move(field_pat)});
        skip_newlines();
        if (cur_tok.type == TokenType::Comma) {
          next_token();
          skip_newlines();
        }
      }
      if (cur_tok.type != TokenType::RBrace) {
        set_error("expected '}' to close struct pattern");
        return nullptr;
      }
      next_token(); // consume '}'
      return pat;
    }
    // Variable pattern
    return std::make_unique<VariablePattern>(name);
  }
  if (cur_tok.type == TokenType::Number ||
      cur_tok.type == TokenType::True ||
      cur_tok.type == TokenType::False) {
    auto expr = std::make_unique<NumberExpr>(cur_tok.num_val);
    next_token();
    return std::make_unique<LiteralPattern>(std::move(expr));
  }
  if (cur_tok.type == TokenType::StringLiteral) {
    auto expr = std::make_unique<StringExpr>(cur_tok.text);
    next_token();
    return std::make_unique<LiteralPattern>(std::move(expr));
  }
  if (cur_tok.type == TokenType::Null) {
    next_token();
    return std::make_unique<LiteralPattern>(std::make_unique<NullExpr>());
  }
  if (cur_tok.type == TokenType::Minus) {
    next_token();
    if (cur_tok.type != TokenType::Number) {
      set_error("expected number after '-' in pattern");
      return nullptr;
    }
    auto expr = std::make_unique<NumberExpr>(-cur_tok.num_val);
    next_token();
    return std::make_unique<LiteralPattern>(std::move(expr));
  }
  set_error("expected pattern");
  return nullptr;
}