#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "llvm/Support/JSON.h"

#include "ast.h"
#include "parser.h"

// =========================================================================
// Hok LSP Server
// =========================================================================

struct LSPPosition {
  int line = 0;     // 0-based
  int character = 0; // 0-based
};

struct LSPRange {
  LSPPosition start;
  LSPPosition end;
};

struct LSPLocation {
  std::string uri;
  LSPRange range;
};

struct LSPDiagnostic {
  LSPRange range;
  std::string message;
  std::string severity; // "Error", "Warning", "Information", "Hint"
};

struct LSPSymbol {
  std::string name;
  std::string kind; // "Function", "Variable", "Struct", "Enum", etc.
  LSPRange range;
  LSPRange selection_range;
};

struct LSPCompletionItem {
  std::string label;
  std::string detail;
  std::string insert_text;
};

/// Byte range of a top-level declaration in the source text.
struct DeclRange {
  std::unique_ptr<Decl> decl;
  int start_offset = 0; // inclusive byte offset
  int end_offset = 0;   // exclusive byte offset
};

struct LSPDocument {
  std::string uri;
  std::string text;
  int version = 0;
  std::vector<DeclRange> decl_ranges; // AST + byte ranges for incremental re-parse
  std::vector<LSPDiagnostic> diagnostics;

  // Symbol index for quick lookup
  std::map<std::string, std::vector<LSPSymbol>> symbols_by_name;
  std::vector<LSPSymbol> all_symbols;
};

class LSPServer {
  std::map<std::string, LSPDocument> documents;
  bool shutdown_requested = false;

  // LSP helpers
  std::string read_message();
  std::string make_response(const llvm::json::Object &msg,
                            const llvm::json::Value &result);
  std::string make_error_response(const llvm::json::Object &msg,
                                  int code, const std::string &msg_text);
  std::string make_notification(const std::string &method,
                                const llvm::json::Value &params);

  // Protocol handlers
  llvm::json::Value handle_initialize(const llvm::json::Object &params);
  llvm::json::Value handle_shutdown();
  void handle_did_open(const llvm::json::Object &params);
  void handle_did_change(const llvm::json::Object &params);
  void handle_did_close(const llvm::json::Object &params);
  llvm::json::Value handle_hover(const llvm::json::Object &params);
  llvm::json::Value handle_completion(const llvm::json::Object &params);
  llvm::json::Value handle_definition(const llvm::json::Object &params);
  llvm::json::Value handle_references(const llvm::json::Object &params);
  llvm::json::Value handle_document_symbol(const llvm::json::Object &params);

  // Internal helpers
  LSPDocument *get_document(const std::string &uri);
  void parse_document(LSPDocument &doc);
  void parse_document_incremental(LSPDocument &doc,
                                  const std::string &old_text);
  void build_symbol_index(LSPDocument &doc);
  void publish_diagnostics(const std::string &uri, LSPDocument &doc);
  void collect_decl_symbols(Decl *decl, LSPDocument &doc, const std::string &parent);
  void collect_stmt_symbols(Stmt *stmt, LSPDocument &doc, const std::string &parent);

  LSPPosition get_position_for_offset(const std::string &text, int offset);
  int get_offset_for_position(const std::string &text, LSPPosition pos);
  std::string uri_to_path(const std::string &uri);
  std::string path_to_uri(const std::string &path);

public:
  void run();
};

// Helper: convert LSP types to llvm::json
llvm::json::Object to_json(const LSPPosition &pos);
llvm::json::Object to_json(const LSPRange &range);
llvm::json::Object to_json(const LSPLocation &loc);
llvm::json::Object to_json(const LSPDiagnostic &diag);
llvm::json::Object to_json(const LSPSymbol &sym);
llvm::json::Object to_json(const LSPCompletionItem &item);
