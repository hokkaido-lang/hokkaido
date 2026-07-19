#include "lexer.h"
#include "lexer_ffi.h"

#include <cstdlib>

// =========================================================================
// Lexer implementation — delegates to Rust via FFI
// =========================================================================

struct LexerData {
  void *rust_handle = nullptr;
};

Lexer::Lexer(const std::string &src) {
  data_ = new LexerData();
  data_->rust_handle = hok_lexer_new(src.c_str());
}

Lexer::~Lexer() {
  if (data_) {
    if (data_->rust_handle)
      hok_lexer_free(data_->rust_handle);
    delete data_;
  }
}

Lexer::Lexer(Lexer &&other) noexcept : data_(other.data_) {
  other.data_ = nullptr;
}

Lexer& Lexer::operator=(Lexer &&other) noexcept {
  if (this != &other) {
    if (data_) {
      if (data_->rust_handle)
        hok_lexer_free(data_->rust_handle);
      delete data_;
    }
    data_ = other.data_;
    other.data_ = nullptr;
  }
  return *this;
}

Token Lexer::next_token() {
  if (!data_ || !data_->rust_handle) {
    return {TokenType::Eof, "", 0, 0, 0};
  }

  int32_t line = 0, col = 0;
  double num_val = 0.0;
  char *text_ptr = nullptr;

  RustTokenType rust_tt = hok_lexer_next(
    data_->rust_handle, &line, &col, &num_val, &text_ptr
  );

  // Convert Rust token type to C++ token type (same numeric values)
  TokenType tt = static_cast<TokenType>(rust_tt);

  // Convert text
  std::string text;
  if (text_ptr) {
    text = text_ptr;
    hok_lexer_free_string(text_ptr);
  }

  return {tt, text, num_val, line, col};
}
