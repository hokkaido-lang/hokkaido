#include "parser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

// =========================================================================
// Declaration parsing: struct, enum, trait, impl, namespace, include,
// let, fn, extern fn
// =========================================================================

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

  auto decl = make_decl<StructDecl>();
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

  auto decl = make_decl<AdtDecl>();
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

  auto decl = make_decl<TraitDecl>();
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

  auto decl = make_decl<ImplDecl>();
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
  Parser sub_parser(sub_lexer, full_path.string(), full_path.parent_path().string(), included_files, imported_packages);
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

  // Optional type annotation: `let name = expr` or `let name: Type = expr`
  if (cur_tok.type == TokenType::Colon) {
    next_token();
    ann = parse_type_annotation();
    if (has_error) return false;

    if (ann.kind == TypeKind::Void) {
      set_error("variable cannot have void type");
      return false;
    }
  } else {
    // No type annotation — infer from init expression
    ann.kind = TypeKind::Infer;
  }

  if (cur_tok.type != TokenType::Equals) {
    set_error("expected '=' after " + std::string(ann.kind == TypeKind::Infer ? "name" : "type"));
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

  auto decl = make_decl<LetDecl>();
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

  auto decl = make_decl<FnDecl>();
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

  auto decl = make_decl<FnDecl>();
  decl->name = name;
  decl->params = std::move(params);
  decl->return_type = return_type;
  decl->is_extern = true;
  decl->is_variadic = is_variadic;
  return decl;
}
