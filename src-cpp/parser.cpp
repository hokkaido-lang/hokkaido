#include "parser.h"
#include "error.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

// =========================================================================
// Core parser utilities and module structure
// =========================================================================

void Parser::next_token() {
  cur_tok = lexer.next_token();
}

void Parser::skip_newlines() {
  while (cur_tok.type == TokenType::Newline)
    cur_tok = lexer.next_token();
}

void Parser::set_error(const std::string &msg) {
  has_error = true;
  error_msg = error_at(source_file, cur_tok.line, cur_tok.col, msg);
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
    Parser sub_parser(sub_lexer, file_path, fs::path(file_path).parent_path().string(),
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
      if (!fn->is_extern)
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
