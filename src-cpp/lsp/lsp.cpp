#include "lsp/lsp.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// =========================================================================
// JSON conversion helpers
// =========================================================================

json::Object to_json(const LSPPosition &pos) {
  return json::Object{{"line", pos.line}, {"character", pos.character}};
}

json::Object to_json(const LSPRange &range) {
  return json::Object{{"start", to_json(range.start)}, {"end", to_json(range.end)}};
}

json::Object to_json(const LSPLocation &loc) {
  return json::Object{{"uri", loc.uri}, {"range", to_json(loc.range)}};
}

json::Object to_json(const LSPDiagnostic &diag) {
  return json::Object{
    {"range", to_json(diag.range)},
    {"message", diag.message},
    {"severity", diag.severity},
  };
}

json::Object to_json(const LSPSymbol &sym) {
  return json::Object{
    {"name", sym.name},
    {"kind", sym.kind},
    {"range", to_json(sym.range)},
    {"selectionRange", to_json(sym.selection_range)},
  };
}

json::Object to_json(const LSPCompletionItem &item) {
  return json::Object{
    {"label", item.label},
    {"detail", item.detail},
  };
}

// =========================================================================
// Message framing: Content-Length header + JSON body
// =========================================================================

std::string LSPServer::read_message() {
  std::string line;
  int content_length = -1;

  // Read headers
  while (true) {
    line.clear();
    if (!std::getline(std::cin, line)) {
      // EOF or error
      return "";
    }
    if (line.empty() || line == "\r") break; // end of headers

    // Remove trailing \r
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (line.rfind("Content-Length: ", 0) == 0) {
      content_length = std::stoi(line.substr(16));
    }
  }

  if (content_length < 0) return "";

  // Read body
  std::string body(content_length, '\0');
  std::cin.read(&body[0], content_length);
  if (std::cin.gcount() != content_length) return ""; // short read = EOF
  return body;
}

void write_message(const std::string &body) {
  std::string header = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
  llvm::outs() << header << body;
  llvm::outs().flush();
}

// =========================================================================
// Helpers
// =========================================================================

std::string LSPServer::uri_to_path(const std::string &uri) {
  // file:///path/to/file.hk -> /path/to/file.hk
  if (uri.rfind("file://", 0) == 0)
    return uri.substr(7);
  return uri;
}

std::string LSPServer::path_to_uri(const std::string &path) {
  return "file://" + path;
}

LSPPosition LSPServer::get_position_for_offset(const std::string &text, int offset) {
  LSPPosition pos;
  for (int i = 0; i < offset && i < (int)text.size(); i++) {
    if (text[i] == '\n') {
      pos.line++;
      pos.character = 0;
    } else {
      pos.character++;
    }
  }
  return pos;
}

int LSPServer::get_offset_for_position(const std::string &text, LSPPosition pos) {
  int line = 0;
  int col = 0;
  for (int i = 0; i < (int)text.size(); i++) {
    if (line == pos.line && col == pos.character)
      return i;
    if (text[i] == '\n') {
      line++;
      col = 0;
    } else {
      col++;
    }
  }
  return (int)text.size();
}

// =========================================================================
// Document management
// =========================================================================

LSPDocument *LSPServer::get_document(const std::string &uri) {
  auto it = documents.find(uri);
  if (it != documents.end()) return &it->second;
  return nullptr;
}

// =========================================================================
// Declaration boundary scanner for incremental parsing
// =========================================================================

/// Find the end offset of a top-level declaration starting at 'start'.
/// Handles brace-delimited declarations (fn, struct, enum, impl, trait,
/// namespace) by matching the first { at depth 0 to its matching }.
/// For other declarations (let, import, include, extern, package),
/// tracks balanced braces and ends at the first newline at depth 0.
static int find_decl_end(const std::string &text, int start) {
  bool in_string = false;
  bool in_char = false;
  int brace_depth = 0;
  int text_len = (int)text.size();

  // Determine whether this is a brace-delimited declaration keyword
  int kw_start = start;
  // Skip 'pub'
  if (text.compare(kw_start, 3, "pub") == 0 &&
      (kw_start + 3 >= text_len ||
       (!isalnum(text[kw_start + 3]) && text[kw_start + 3] != '_'))) {
    kw_start += 3;
    while (kw_start < text_len &&
           (text[kw_start] == ' ' || text[kw_start] == '\t'))
      kw_start++;
  }

  bool brace_delimited = false;
  {
    int p = kw_start;
    while (p < text_len && (isalpha(text[p]) || text[p] == '_')) p++;
    std::string kw = text.substr(kw_start, p - kw_start);
    brace_delimited =
        (kw == "fn" || kw == "struct" || kw == "enum" || kw == "impl" ||
         kw == "trait" || kw == "namespace");
  }

  for (int pos = start; pos < text_len; pos++) {
    char c = text[pos];

    if (c == '"' && !in_char) {
      in_string = !in_string;
      continue;
    }
    if (c == '\'' && !in_string) {
      in_char = !in_char;
      continue;
    }
    if (in_string || in_char)
      continue;

    // Skip line comments
    if (c == '/' && pos + 1 < text_len) {
      if (text[pos + 1] == '/') {
        while (pos < text_len && text[pos] != '\n')
          pos++;
        continue;
      }
      if (text[pos + 1] == '*') {
        pos += 2;
        while (pos + 1 < text_len &&
               !(text[pos] == '*' && text[pos + 1] == '/'))
          pos++;
        if (pos + 1 < text_len)
          pos++;
        continue;
      }
    }

    if (c == '{') {
      brace_depth++;
    } else if (c == '}') {
      if (brace_delimited && brace_depth == 1) {
        // Closing the top-level brace block
        return pos + 1;
      }
      brace_depth--;
      if (brace_depth < 0)
        brace_depth = 0;
    }

    // Non-brace declaration ends at newline when not inside braces
    if (!brace_delimited && brace_depth == 0 && c == '\n') {
      return pos + 1;
    }
  }

  return text_len;
}

/// Scan the full source text and return byte ranges for each top-level
/// declaration.
static std::vector<std::pair<int, int>> find_decl_boundaries(
    const std::string &text) {
  std::vector<std::pair<int, int>> boundaries;
  int pos = 0;
  int text_len = (int)text.size();

  static const char *decl_kw[] = {"fn",     "let",       "struct", "enum",
                                  "impl",   "trait",     "namespace",
                                  "package","include",   "import", "extern"};

  while (pos < text_len) {
    // Skip whitespace and blank lines
    while (pos < text_len && (text[pos] == ' ' || text[pos] == '\t' ||
                              text[pos] == '\n' || text[pos] == '\r'))
      pos++;

    if (pos >= text_len)
      break;

    int start = pos;

    // Check for 'pub' prefix
    int check = pos;
    if (text.compare(check, 3, "pub") == 0 &&
        (check + 3 >= text_len ||
         (!isalnum(text[check + 3]) && text[check + 3] != '_'))) {
      check += 3;
      while (check < text_len &&
             (text[check] == ' ' || text[check] == '\t'))
        check++;
    }

    // Check for declaration-starting keywords
    bool is_decl = false;
    for (auto *kw : decl_kw) {
      size_t klen = std::strlen(kw);
      if (text.compare(check, klen, kw) == 0 &&
          (check + (int)klen >= text_len ||
           (!isalnum(text[check + klen]) && text[check + klen] != '_'))) {
        is_decl = true;
        break;
      }
    }

    if (is_decl) {
      int end = find_decl_end(text, start);
      boundaries.push_back({start, end});
      pos = end;
    } else {
      // Not a declaration - skip to next line
      while (pos < text_len && text[pos] != '\n')
        pos++;
      if (pos < text_len)
        pos++;
    }
  }

  return boundaries;
}

void LSPServer::parse_document(LSPDocument &doc) {
  doc.diagnostics.clear();
  doc.symbols_by_name.clear();
  doc.all_symbols.clear();
  doc.decl_ranges.clear();

  Lexer lexer(doc.text);
  std::string path = uri_to_path(doc.uri);
  std::string base_dir = ".";
  auto slash = path.rfind('/');
  if (slash != std::string::npos)
    base_dir = path.substr(0, slash);

  Parser parser(lexer, path, base_dir);
  auto ast = parser.parse_program();

  // Build decl_ranges by pairing AST with text boundaries
  auto boundaries = find_decl_boundaries(doc.text);
  int decl_idx = 0;
  for (auto &[start, end] : boundaries) {
    DeclRange dr;
    dr.start_offset = start;
    dr.end_offset = end;
    // Match AST decls to text boundaries (skip side-effect-only decls)
    while (decl_idx < (int)ast.size()) {
      dr.decl = std::move(ast[decl_idx]);
      decl_idx++;
      break; // Take one decl per boundary (some may produce no AST)
    }
    doc.decl_ranges.push_back(std::move(dr));
  }

  // Build diagnostics from parser errors
  if (!parser.ok()) {
    std::string err = parser.error();
    int err_line = 0, err_col = 0;
    auto colon_pos = err.rfind(':');
    if (colon_pos != std::string::npos) {
      auto colon2 = err.rfind(':', colon_pos - 1);
      if (colon2 != std::string::npos) {
        try {
          err_line = std::stoi(err.substr(colon2 + 1, colon_pos - colon2 - 1)) - 1;
          err_col = std::stoi(err.substr(colon_pos + 1)) - 1;
        } catch (...) {}
      }
    }

    LSPDiagnostic diag;
    diag.range.start = {err_line, err_col};
    diag.range.end = {err_line, err_col + 1};
    diag.message = err;
    diag.severity = "Error";
    doc.diagnostics.push_back(diag);
  }

  build_symbol_index(doc);
}

void LSPServer::parse_document_incremental(LSPDocument &doc,
                                           const std::string &old_text) {
  // Find first byte where old and new text differ
  size_t min_len = std::min(old_text.size(), doc.text.size());
  size_t diff_pos = 0;
  while (diff_pos < min_len && old_text[diff_pos] == doc.text[diff_pos])
    diff_pos++;

  // No change or both empty
  if (diff_pos == old_text.size() && diff_pos == doc.text.size())
    return;

  // If no existing decl_ranges, do full parse
  if (doc.decl_ranges.empty()) {
    parse_document(doc);
    return;
  }

  // Find which declaration in the OLD text contains the change
  int affected_idx = -1;
  for (int i = 0; i < (int)doc.decl_ranges.size(); i++) {
    if (doc.decl_ranges[i].end_offset > (int)diff_pos) {
      affected_idx = i;
      break;
    }
  }

  // Change is past all decls (appending at EOF), parse from last decl
  if (affected_idx < 0)
    affected_idx = (int)doc.decl_ranges.size() - 1;

  // Include one decl before the affected one for safety
  if (affected_idx > 0)
    affected_idx--;

  // If re-parsing most of the file, just do full parse
  if (affected_idx <= (int)doc.decl_ranges.size() / 4) {
    parse_document(doc);
    return;
  }

  // Extract new text from affected position to EOF
  int start_offset = doc.decl_ranges[affected_idx].start_offset;
  std::string reparse_text = doc.text.substr(start_offset);

  Lexer re_lexer(reparse_text);
  std::string path = uri_to_path(doc.uri);
  std::string base_dir = ".";
  auto slash = path.rfind('/');
  if (slash != std::string::npos)
    base_dir = path.substr(0, slash);

  Parser re_parser(re_lexer, path, base_dir);
  auto new_decls = re_parser.parse_program();

  // If re-parse succeeded, replace affected decls
  if (re_parser.ok() || !new_decls.empty()) {
    // Remove affected decls from end
    doc.decl_ranges.erase(
        doc.decl_ranges.begin() + affected_idx,
        doc.decl_ranges.end());

    // Get boundaries for the new text
    auto boundaries = find_decl_boundaries(doc.text);

    // Find where to start in boundaries
    int bi = 0;
    while (bi < (int)boundaries.size() &&
           boundaries[bi].first < start_offset)
      bi++;

    // Add new DeclRanges for the re-parsed portion
    int new_idx = 0;
    while (bi < (int)boundaries.size()) {
      DeclRange dr;
      dr.start_offset = boundaries[bi].first;
      dr.end_offset = boundaries[bi].second;
      if (new_idx < (int)new_decls.size()) {
        dr.decl = std::move(new_decls[new_idx]);
        new_idx++;
      }
      doc.decl_ranges.push_back(std::move(dr));
      bi++;
    }

    // Rebuild symbol index from scratch
    build_symbol_index(doc);
  } else {
    // Re-parse failed, fall back to full
    parse_document(doc);
  }
}

void LSPServer::build_symbol_index(LSPDocument &doc) {
  for (auto &dr : doc.decl_ranges) {
    if (dr.decl)
      collect_decl_symbols(dr.decl.get(), doc, "");
  }
}

void LSPServer::collect_decl_symbols(Decl *decl, LSPDocument &doc, const std::string &parent) {
  LSPRange range{{decl->line - 1, decl->col - 1}, {decl->line - 1, decl->col + 1}};

  if (auto *fn = dynamic_cast<FnDecl *>(decl)) {
    LSPSymbol sym;
    sym.name = fn->name;
    sym.kind = fn->is_extern ? "Function" : "Function";
    sym.range = range;
    sym.selection_range = range;
    doc.all_symbols.push_back(sym);
    doc.symbols_by_name[fn->name].push_back(sym);

    // Collect parameters as symbols
    for (auto &param : fn->params) {
      LSPSymbol psym;
      psym.name = param.name;
      psym.kind = "Variable";
      psym.range = range;
      psym.selection_range = range;
      doc.symbols_by_name[param.name].push_back(psym);
    }

    // Walk body for variable declarations
    for (auto &stmt : fn->body) {
      collect_stmt_symbols(stmt.get(), doc, fn->name);
    }
  } else if (auto *sd = dynamic_cast<StructDecl *>(decl)) {
    LSPSymbol sym;
    sym.name = sd->name;
    sym.kind = "Struct";
    sym.range = range;
    sym.selection_range = range;
    doc.all_symbols.push_back(sym);
    doc.symbols_by_name[sd->name].push_back(sym);

    for (auto &field : sd->fields) {
      LSPSymbol fsym;
      fsym.name = field.name;
      fsym.kind = "Field";
      fsym.range = range;
      fsym.selection_range = range;
      doc.symbols_by_name[field.name].push_back(fsym);
    }
  } else if (auto *adt = dynamic_cast<AdtDecl *>(decl)) {
    LSPSymbol sym;
    sym.name = adt->name;
    sym.kind = "Enum";
    sym.range = range;
    sym.selection_range = range;
    doc.all_symbols.push_back(sym);
    doc.symbols_by_name[adt->name].push_back(sym);
  } else if (auto *ld = dynamic_cast<LetDecl *>(decl)) {
    LSPSymbol sym;
    sym.name = ld->name;
    sym.kind = "Variable";
    sym.range = range;
    sym.selection_range = range;
    doc.all_symbols.push_back(sym);
    doc.symbols_by_name[ld->name].push_back(sym);
  } else if (auto *td = dynamic_cast<TraitDecl *>(decl)) {
    LSPSymbol sym;
    sym.name = td->name;
    sym.kind = "Interface";
    sym.range = range;
    sym.selection_range = range;
    doc.all_symbols.push_back(sym);
    doc.symbols_by_name[td->name].push_back(sym);
  } else if (auto *id = dynamic_cast<ImplDecl *>(decl)) {
    (void)id;
    // Impl blocks don't create named symbols themselves
  }
}

void LSPServer::collect_stmt_symbols(Stmt *stmt, LSPDocument &doc, const std::string &parent) {
  LSPRange range{{stmt->line - 1, stmt->col - 1}, {stmt->line - 1, stmt->col + 1}};

  if (auto *ls = dynamic_cast<LetStmt *>(stmt)) {
    LSPSymbol sym;
    sym.name = ls->name;
    sym.kind = "Variable";
    sym.range = range;
    sym.selection_range = range;
    doc.all_symbols.push_back(sym);
    doc.symbols_by_name[ls->name].push_back(sym);
  }
}

void LSPServer::publish_diagnostics(const std::string &uri, LSPDocument &doc) {
  json::Array diags;
  for (auto &d : doc.diagnostics)
    diags.push_back(to_json(d));

  auto notification = make_notification("textDocument/publishDiagnostics",
    json::Object{{"uri", uri}, {"diagnostics", std::move(diags)}});
  write_message(notification);
}

// =========================================================================
// Response / Notification builders
// =========================================================================

std::string LSPServer::make_response(const json::Object &msg,
                                     const json::Value &result) {
  int id = 0;
  if (auto *id_val = msg.get("id")) {
    if (id_val->kind() == json::Value::Kind::Number)
      id = (int)id_val->getAsInteger().value_or(0);
  }
  json::Object resp{
    {"jsonrpc", "2.0"},
    {"id", id},
    {"result", result},
  };
  return llvm::formatv("{0}", json::Value(std::move(resp))).str();
}

std::string LSPServer::make_error_response(const json::Object &msg,
                                           int code, const std::string &msg_text) {
  int id = 0;
  if (auto *id_val = msg.get("id")) {
    if (id_val->kind() == json::Value::Kind::Number)
      id = (int)id_val->getAsInteger().value_or(0);
  }
  json::Object resp{
    {"jsonrpc", "2.0"},
    {"id", id},
    {"error", json::Object{{"code", code}, {"message", msg_text}}},
  };
  return llvm::formatv("{0}", json::Value(std::move(resp))).str();
}

std::string LSPServer::make_notification(const std::string &method,
                                         const json::Value &params) {
  json::Object notif{
    {"jsonrpc", "2.0"},
    {"method", method},
    {"params", params},
  };
  return llvm::formatv("{0}", json::Value(std::move(notif))).str();
}

// =========================================================================
// LSP Method Handlers
// =========================================================================

json::Value LSPServer::handle_initialize(const json::Object &params) {
  (void)params;
  return json::Object{
    {"capabilities", json::Object{
      {"textDocumentSync", 1}, // Full sync
      {"hoverProvider", true},
      {"completionProvider", json::Object{
        {"triggerCharacters", json::Array{":"}},
      }},
      {"definitionProvider", true},
      {"referencesProvider", true},
      {"documentSymbolProvider", true},
    }},
    {"serverInfo", json::Object{
      {"name", "hok-lsp"},
      {"version", "0.22.0"},
    }},
  };
}

json::Value LSPServer::handle_shutdown() {
  shutdown_requested = true;
  return nullptr;
}

void LSPServer::handle_did_open(const json::Object &params) {
  auto *text_doc = params.get("textDocument");
  if (!text_doc || text_doc->kind() != json::Value::Kind::Object) return;
  auto &obj = *text_doc->getAsObject();

  std::string uri = obj.get("uri")->getAsString().value_or("").str();
  std::string text = obj.get("text")->getAsString().value_or("").str();
  int version = (int)obj.get("version")->getAsInteger().value_or(0);

  LSPDocument doc;
  doc.uri = uri;
  doc.text = text;
  doc.version = version;
  parse_document(doc);
  documents[uri] = std::move(doc);

  publish_diagnostics(uri, documents[uri]);
}

void LSPServer::handle_did_change(const json::Object &params) {
  auto *text_doc = params.get("textDocument");
  if (!text_doc || text_doc->kind() != json::Value::Kind::Object) return;

  auto *content_changes = params.get("contentChanges");
  if (!content_changes || content_changes->kind() != json::Value::Kind::Array) return;

  std::string uri = text_doc->getAsObject()->get("uri")->getAsString().value_or("").str();

  auto *doc = get_document(uri);
  if (!doc) return;

  std::string old_text = doc->text;

  // Full sync: take the last change's text
  auto *changes = content_changes->getAsArray();
  if (changes && !changes->empty()) {
    auto &last = changes->back();
    if (last.kind() == json::Value::Kind::Object) {
      doc->text = last.getAsObject()->get("text")->getAsString().value_or("").str();
    }
  }

  // Try incremental parse, falls back to full parse if needed
  parse_document_incremental(*doc, old_text);
  publish_diagnostics(uri, *doc);
}

void LSPServer::handle_did_close(const json::Object &params) {
  auto *text_doc = params.get("textDocument");
  if (!text_doc || text_doc->kind() != json::Value::Kind::Object) return;

  std::string uri = text_doc->getAsObject()->get("uri")->getAsString().value_or("").str();
  documents.erase(uri);
}

json::Value LSPServer::handle_hover(const json::Object &params) {
  auto *text_doc = params.get("textDocument");
  auto *pos_val = params.get("position");
  if (!text_doc || !pos_val) return nullptr;

  std::string uri = text_doc->getAsObject()->get("uri")->getAsString().value_or("").str();
  auto *pos = pos_val->getAsObject();
  if (!pos || uri.empty()) return nullptr;

  auto *doc = get_document(uri);
  if (!doc || doc->decl_ranges.empty()) return nullptr;

  LSPPosition position;
  position.line = (int)pos->get("line")->getAsInteger().value_or(0);
  position.character = (int)pos->get("character")->getAsInteger().value_or(0);

  // Find the word at cursor
  int offset = get_offset_for_position(doc->text, position);
  if (offset < 0 || offset >= (int)doc->text.size()) return nullptr;

  // Scan backward to find start of word
  int start = offset;
  while (start > 0 && (isalnum(doc->text[start - 1]) || doc->text[start - 1] == '_'))
    start--;
  // Scan forward to find end of word
  int end = offset;
  while (end < (int)doc->text.size() && (isalnum(doc->text[end]) || doc->text[end] == '_'))
    end++;

  if (start >= end) return nullptr;

  std::string word = doc->text.substr(start, end - start);

  // Look up in symbol index
  auto it = doc->symbols_by_name.find(word);
  if (it != doc->symbols_by_name.end() && !it->second.empty()) {
    auto &sym = it->second[0];
    std::string hover_text = "**" + sym.name + "**  \nKind: " + sym.kind;
    return json::Object{
      {"contents", json::Object{{"kind", "markdown"}, {"value", hover_text}}},
      {"range", to_json(sym.range)},
    };
  }

  return nullptr;
}

json::Value LSPServer::handle_completion(const json::Object &params) {
  (void)params;
  // Keywords always available
  std::vector<LSPCompletionItem> items = {
    {"fn", "keyword: function declaration"},
    {"let", "keyword: variable declaration"},
    {"return", "keyword: return from function"},
    {"if", "keyword: conditional"},
    {"else", "keyword: else branch"},
    {"for", "keyword: for loop"},
    {"while", "keyword: while loop"},
    {"break", "keyword: break from loop"},
    {"continue", "keyword: continue loop"},
    {"struct", "keyword: struct declaration"},
    {"enum", "keyword: enum declaration"},
    {"impl", "keyword: impl block"},
    {"trait", "keyword: trait declaration"},
    {"match", "keyword: pattern matching"},
    {"mut", "keyword: mutable"},
    {"pub", "keyword: public"},
    {"import", "keyword: import package"},
    {"include", "keyword: include file"},
    {"namespace", "keyword: namespace"},
    {"package", "keyword: package declaration"},
    {"extern", "keyword: extern function"},
    {"true", "keyword: boolean true"},
    {"false", "keyword: boolean false"},
    {"null", "keyword: null value"},
    {"void", "keyword: void type"},
    {"int", "type: signed integer"},
    {"int8", "type: 8-bit integer"},
    {"int16", "type: 16-bit integer"},
    {"int32", "type: 32-bit integer"},
    {"int64", "type: 64-bit integer"},
    {"uint8", "type: 8-bit unsigned integer"},
    {"uint16", "type: 16-bit unsigned integer"},
    {"uint32", "type: 32-bit unsigned integer"},
    {"uint64", "type: 64-bit unsigned integer"},
    {"float", "type: floating point"},
    {"float32", "type: 32-bit float"},
    {"float64", "type: 64-bit float"},
    {"bool", "type: boolean"},
    {"string", "type: string"},
    {"char", "type: character"},
  };

  // Add known symbols from all open documents
  for (auto &[uri, doc] : documents) {
    (void)uri;
    for (auto &[name, syms] : doc.symbols_by_name) {
      (void)syms;
      std::string detail = "symbol: " + doc.symbols_by_name[name][0].kind;
      items.push_back({name, detail});
    }
  }

  json::Array result;
  for (auto &item : items)
    result.push_back(to_json(item));

  // Deduplicate by label
  std::set<std::string> seen;
  json::Array deduped;
  for (auto &item : result) {
    auto label = item.getAsObject()->get("label")->getAsString().value_or("").str();
    if (seen.insert(label).second)
      deduped.push_back(std::move(item));
  }

  return json::Object{{"isIncomplete", false}, {"items", std::move(deduped)}};
}

json::Value LSPServer::handle_definition(const json::Object &params) {
  auto *text_doc = params.get("textDocument");
  auto *pos_val = params.get("position");
  if (!text_doc || !pos_val) return nullptr;

  std::string uri = text_doc->getAsObject()->get("uri")->getAsString().value_or("").str();
  auto *pos = pos_val->getAsObject();
  if (!pos || uri.empty()) return nullptr;

  auto *doc = get_document(uri);
  if (!doc || doc->decl_ranges.empty()) return nullptr;

  LSPPosition position;
  position.line = (int)pos->get("line")->getAsInteger().value_or(0);
  position.character = (int)pos->get("character")->getAsInteger().value_or(0);

  // Find word at cursor
  int offset = get_offset_for_position(doc->text, position);
  int start = offset;
  while (start > 0 && (isalnum(doc->text[start - 1]) || doc->text[start - 1] == '_'))
    start--;
  int end = offset;
  while (end < (int)doc->text.size() && (isalnum(doc->text[end]) || doc->text[end] == '_'))
    end++;

  if (start >= end) return nullptr;

  std::string word = doc->text.substr(start, end - start);

  auto it = doc->symbols_by_name.find(word);
  if (it != doc->symbols_by_name.end() && !it->second.empty()) {
    auto &sym = it->second[0];
    LSPLocation loc;
    loc.uri = uri;
    loc.range = sym.range;
    return to_json(loc);
  }

  return nullptr;
}

json::Value LSPServer::handle_references(const json::Object &params) {
  auto *text_doc = params.get("textDocument");
  auto *pos_val = params.get("position");
  if (!text_doc || !pos_val) return nullptr;

  std::string uri = text_doc->getAsObject()->get("uri")->getAsString().value_or("").str();
  auto *pos = pos_val->getAsObject();
  if (!pos || uri.empty()) return nullptr;

  auto *doc = get_document(uri);
  if (!doc || doc->decl_ranges.empty()) return nullptr;

  LSPPosition position;
  position.line = (int)pos->get("line")->getAsInteger().value_or(0);
  position.character = (int)pos->get("character")->getAsInteger().value_or(0);

  // Find word at cursor
  int offset = get_offset_for_position(doc->text, position);
  int start = offset;
  while (start > 0 && (isalnum(doc->text[start - 1]) || doc->text[start - 1] == '_'))
    start--;
  int end = offset;
  while (end < (int)doc->text.size() && (isalnum(doc->text[end]) || doc->text[end] == '_'))
    end++;

  if (start >= end) return nullptr;

  std::string word = doc->text.substr(start, end - start);

  // Find all occurrences of the word in the document text
  json::Array locations;
  for (int i = 0; i <= (int)doc->text.size() - (int)word.size(); i++) {
    if (doc->text.substr(i, word.size()) == word) {
      // Check it's a word boundary
      bool word_start = (i == 0) || (!isalnum(doc->text[i - 1]) && doc->text[i - 1] != '_');
      bool word_end = (i + (int)word.size() >= (int)doc->text.size()) ||
                      (!isalnum(doc->text[i + word.size()]) && doc->text[i + word.size()] != '_');
      if (word_start && word_end) {
        auto start_pos = get_position_for_offset(doc->text, i);
        auto end_pos = get_position_for_offset(doc->text, i + word.size());
        LSPLocation loc;
        loc.uri = uri;
        loc.range = {start_pos, end_pos};
        locations.push_back(to_json(loc));
      }
    }
  }

  return locations;
}

json::Value LSPServer::handle_document_symbol(const json::Object &params) {
  auto *text_doc = params.get("textDocument");
  if (!text_doc) return nullptr;

  std::string uri = text_doc->getAsObject()->get("uri")->getAsString().value_or("").str();
  auto *doc = get_document(uri);
  if (!doc) return nullptr;

  json::Array symbols;
  for (auto &sym : doc->all_symbols)
    symbols.push_back(to_json(sym));

  return symbols;
}

// =========================================================================
// Main loop
// =========================================================================

void LSPServer::run() {
  while (!shutdown_requested && !std::cin.eof()) {
    std::string body = read_message();
    if (body.empty()) {
      if (std::cin.eof()) break;
      continue;
    }

    auto parsed = json::parse(body);
    if (!parsed) continue;
    auto *msg = parsed->getAsObject();
    if (!msg) continue;

    // Get JSON-RPC fields
    auto *method_val = msg->get("method");
    if (!method_val) continue;
    std::string method = method_val->getAsString().value_or("").str();
    auto *params = msg->get("params");

    // Handle methods
    std::string response;

    if (method == "initialize") {
      auto result = handle_initialize(params ? *params->getAsObject() : json::Object());
      response = make_response(*msg, result);
    } else if (method == "shutdown") {
      handle_shutdown();
      response = make_response(*msg, nullptr);
    } else if (method == "exit") {
      break;
    } else if (method == "textDocument/didOpen") {
      if (params) handle_did_open(*params->getAsObject());
    } else if (method == "textDocument/didChange") {
      if (params) handle_did_change(*params->getAsObject());
    } else if (method == "textDocument/didClose") {
      if (params) handle_did_close(*params->getAsObject());
    } else if (method == "textDocument/hover") {
      if (params) {
        auto result = handle_hover(*params->getAsObject());
        response = make_response(*msg, result);
      }
    } else if (method == "textDocument/completion") {
      if (params) {
        auto result = handle_completion(*params->getAsObject());
        response = make_response(*msg, result);
      }
    } else if (method == "textDocument/definition") {
      if (params) {
        auto result = handle_definition(*params->getAsObject());
        response = make_response(*msg, result);
      }
    } else if (method == "textDocument/references") {
      if (params) {
        auto result = handle_references(*params->getAsObject());
        response = make_response(*msg, result);
      }
    } else if (method == "textDocument/documentSymbol") {
      if (params) {
        auto result = handle_document_symbol(*params->getAsObject());
        response = make_response(*msg, result);
      }
    } else if (method == "initialized") {
      // No-op
    } else {
      // Method not found — return error
      response = make_error_response(*msg, -32601, "Method not found: " + method);
    }

    if (!response.empty()) {
      write_message(response);
    }
  }
}
