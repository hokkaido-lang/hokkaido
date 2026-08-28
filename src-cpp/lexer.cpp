#include "lexer.h"
#include <cctype>
#include <cstdlib>

// =========================================================================
// Hokkaido Language — Lexer (C++ rewrite of hok-lexer)
// =========================================================================

Lexer::Lexer(const std::string &src) {
  input_.assign(src.begin(), src.end());

  keywords_["let"] = TokenType::Let;
  keywords_["fn"] = TokenType::Fn;
  keywords_["lambda"] = TokenType::Lambda;
  keywords_["return"] = TokenType::Return;
  keywords_["asm"] = TokenType::Asm;
  keywords_["if"] = TokenType::If;
  keywords_["else"] = TokenType::Else;
  keywords_["for"] = TokenType::For;
  keywords_["while"] = TokenType::While;
  keywords_["in"] = TokenType::In;
  keywords_["break"] = TokenType::Break;
  keywords_["continue"] = TokenType::Continue;
  keywords_["atomic"] = TokenType::Atomic;
  keywords_["struct"] = TokenType::Struct;
  keywords_["include"] = TokenType::Include;
  keywords_["namespace"] = TokenType::Namespace;
  keywords_["extern"] = TokenType::Extern;
  keywords_["trait"] = TokenType::Trait;
  keywords_["impl"] = TokenType::Impl;
  keywords_["pub"] = TokenType::Pub;
  keywords_["import"] = TokenType::Import;
  keywords_["package"] = TokenType::Package;
  keywords_["match"] = TokenType::Match;
  keywords_["enum"] = TokenType::Enum;
  keywords_["null"] = TokenType::Null;
  keywords_["mut"] = TokenType::Mut;
  keywords_["int"] = TokenType::Int64;
  keywords_["float"] = TokenType::Float64;
  keywords_["int8"] = TokenType::Int8;
  keywords_["int16"] = TokenType::Int16;
  keywords_["int32"] = TokenType::Int32;
  keywords_["int64"] = TokenType::Int64;
  keywords_["uint8"] = TokenType::Uint8;
  keywords_["uint16"] = TokenType::Uint16;
  keywords_["uint32"] = TokenType::Uint32;
  keywords_["uint64"] = TokenType::Uint64;
  keywords_["float16"] = TokenType::Float16;
  keywords_["float32"] = TokenType::Float32;
  keywords_["float64"] = TokenType::Float64;
  keywords_["bool"] = TokenType::Bool;
  keywords_["string"] = TokenType::String;
  keywords_["char"] = TokenType::Char;
  keywords_["void"] = TokenType::Void;
  keywords_["region"] = TokenType::Region;
  keywords_["true"] = TokenType::True;
  keywords_["false"] = TokenType::False;
}

char Lexer::peek() const {
  if (pos_ >= input_.size()) return '\0';
  return input_[pos_];
}

char Lexer::peek_offset(size_t offset) const {
  size_t idx = pos_ + offset;
  if (idx >= input_.size()) return '\0';
  return input_[idx];
}

char Lexer::advance() {
  if (pos_ >= input_.size()) return '\0';
  char c = input_[pos_++];
  if (c == '\n') { line_++; col_ = 1; } else { col_++; }
  return c;
}

void Lexer::skip_whitespace() {
  while (pos_ < input_.size()) {
    char c = input_[pos_];
    if (c == ' ' || c == '\t' || c == '\r') { advance(); }
    else if (c == '/' && pos_ + 1 < input_.size()) {
      if (input_[pos_ + 1] == '/') { advance(); advance(); skip_line_comment(); }
      else if (input_[pos_ + 1] == '*') { advance(); advance(); skip_block_comment(); }
      else break;
    }
    else break;
  }
}

void Lexer::skip_line_comment() {
  while (pos_ < input_.size() && input_[pos_] != '\n') advance();
}

void Lexer::skip_block_comment() {
  while (pos_ < input_.size()) {
    if (input_[pos_] == '*' && pos_ + 1 < input_.size() && input_[pos_ + 1] == '/') {
      advance(); advance();
      return;
    }
    advance();
  }
}

Token Lexer::lex_string(int l, int c) {
  std::string s;
  while (pos_ < input_.size()) {
    char c = advance();
    if (c == '"') break;
    if (c == '\\' && pos_ < input_.size()) {
      char e = advance();
      switch (e) {
        case 'n': s += '\n'; break;
        case 't': s += '\t'; break;
        case '\\': s += '\\'; break;
        case '"': s += '"'; break;
        case '0': s += '\0'; break;
        default: s += e; break;
      }
    } else {
      s += c;
    }
  }
  return {TokenType::StringLiteral, std::move(s), 0, l, c};
}

Token Lexer::lex_char_literal(int l, int c) {
  std::string s;
  char ch = advance();
  if (ch == '\\' && pos_ < input_.size()) {
    char e = advance();
    switch (e) {
      case 'n': ch = '\n'; break;
      case 't': ch = '\t'; break;
      case '\\': ch = '\\'; break;
      case '\'': ch = '\''; break;
      default: ch = e; break;
    }
  }
  s += ch;
  advance(); // closing quote
  return {TokenType::CharLiteral, std::move(s), 0, l, c};
}

Token Lexer::lex_number(int l, int c) {
  std::string s;
  bool is_float = false;
  while (pos_ < input_.size() && std::isdigit(input_[pos_])) s += advance();
  if (pos_ < input_.size() && input_[pos_] == '.' && pos_ + 1 < input_.size() && std::isdigit(input_[pos_ + 1])) {
    is_float = true;
    s += advance(); // dot
    while (pos_ < input_.size() && std::isdigit(input_[pos_])) s += advance();
  }
  double val = std::atof(s.c_str());
  std::string text = std::to_string((int64_t)val);
  if (is_float) {
    text = s;
  }
  return {is_float ? TokenType::Number : TokenType::Number, std::move(text), val, l, c};
}

Token Lexer::lex_identifier(int l, int c) {
  std::string s;
  while (pos_ < input_.size() && (std::isalnum(input_[pos_]) || input_[pos_] == '_' || input_[pos_] == '-')) {
    s += advance();
  }
  auto it = keywords_.find(s);
  if (it != keywords_.end()) return {it->second, std::move(s), 0, l, c};
  return {TokenType::Identifier, std::move(s), 0, l, c};
}

Token Lexer::next_token() {
  skip_whitespace();
  if (pos_ >= input_.size()) return {TokenType::Eof, "", 0, line_, col_};

  int l = line_, c = col_;
  char ch = advance();

  if (ch == '\n') return {TokenType::Newline, "\n", 0, l, c};
  if (ch == '"') return lex_string(l, c);

  // When ' is followed by an ident-start char, determine if it's a label
  // prefix ('ident) or a char literal ('c').
  if (ch == '\'' && pos_ < input_.size() &&
      (std::isalpha(input_[pos_]) || input_[pos_] == '_')) {
    // Scan the potential identifier
    size_t scan = pos_;
    while (scan < input_.size() && (std::isalnum(input_[scan]) || input_[scan] == '_'))
      scan++;
    // If NOT followed by a closing quote, it's a label prefix (Tick).
    // Char literals are exactly: 'X' (single char between quotes).
    if (scan >= input_.size() || input_[scan] != '\'') {
      return {TokenType::Tick, "'", 0, l, c};
    }
  }

  if (ch == '\'') return lex_char_literal(l, c);
  if (std::isdigit(ch)) { pos_--; col_--; return lex_number(l, c); }
  if (std::isalpha(ch) || ch == '_') { pos_--; col_--; return lex_identifier(l, c); }

  switch (ch) {
    case '(': return {TokenType::LParen, "(", 0, l, c};
    case ')': return {TokenType::RParen, ")", 0, l, c};
    case '{': return {TokenType::LBrace, "{", 0, l, c};
    case '}': return {TokenType::RBrace, "}", 0, l, c};
    case '[': return {TokenType::LSquare, "[", 0, l, c};
    case ']': return {TokenType::RSquare, "]", 0, l, c};
    case ';': return {TokenType::Semicolon, ";", 0, l, c};
    case ',': return {TokenType::Comma, ",", 0, l, c};
    case ':': {
      if (peek() == ':') { advance(); return {TokenType::ColonColon, "::", 0, l, c}; }
      return {TokenType::Colon, ":", 0, l, c};
    }
    case '.': {
      if (peek() == '.' && peek_offset(1) == '=') { advance(); advance(); return {TokenType::DotDotEq, "..=", 0, l, c}; }
      if (peek() == '.' && peek_offset(1) == '.') { advance(); advance(); return {TokenType::Ellipsis, "...", 0, l, c}; }
      if (peek() == '.') { advance(); return {TokenType::DotDot, "..", 0, l, c}; }
      return {TokenType::Dot, ".", 0, l, c};
    }
    case '+': {
      if (peek() == '=') { advance(); return {TokenType::PlusEq, "+=", 0, l, c}; }
      return {TokenType::Plus, "+", 0, l, c};
    }
    case '-': {
      if (peek() == '>') { advance(); return {TokenType::Arrow, "->", 0, l, c}; }
      if (peek() == '=') { advance(); return {TokenType::MinusEq, "-=", 0, l, c}; }
      return {TokenType::Minus, "-", 0, l, c};
    }
    case '*': {
      if (peek() == '=') { advance(); return {TokenType::StarEq, "*=", 0, l, c}; }
      return {TokenType::Star, "*", 0, l, c};
    }
    case '/': {
      if (peek() == '=') { advance(); return {TokenType::SlashEq, "/=", 0, l, c}; }
      return {TokenType::Slash, "/", 0, l, c};
    }
    case '%': {
      if (peek() == '=') { advance(); return {TokenType::PercentEq, "%=", 0, l, c}; }
      return {TokenType::Percent, "%", 0, l, c};
    }
    case '=': {
      if (peek() == '=') { advance(); return {TokenType::Eq, "==", 0, l, c}; }
      if (peek() == '>') { advance(); return {TokenType::FatArrow, "=>", 0, l, c}; }
      return {TokenType::Equals, "=", 0, l, c};
    }
    case '!': {
      if (peek() == '=') { advance(); return {TokenType::Ne, "!=", 0, l, c}; }
      return {TokenType::BitNot, "!", 0, l, c};
    }
    case '<': {
      if (peek() == '=') { advance(); return {TokenType::LessEqual, "<=", 0, l, c}; }
      if (peek() == '<') { advance(); return {TokenType::Shl, "<<", 0, l, c}; }
      return {TokenType::Less, "<", 0, l, c};
    }
    case '>': {
      if (peek() == '=') { advance(); return {TokenType::GreaterEqual, ">=", 0, l, c}; }
      if (peek() == '>') { advance(); return {TokenType::Shr, ">>", 0, l, c}; }
      return {TokenType::Greater, ">", 0, l, c};
    }
    case '&': {
      if (peek() == '&') { advance(); return {TokenType::AndAnd, "&&", 0, l, c}; }
      if (peek() == '=') { advance(); return {TokenType::AndEq, "&=", 0, l, c}; }
      return {TokenType::Ampersand, "&", 0, l, c};
    }
    case '|': {
      if (peek() == '|') { advance(); return {TokenType::OrOr, "||", 0, l, c}; }
      if (peek() == '=') { advance(); return {TokenType::OrEq, "|=", 0, l, c}; }
      return {TokenType::BitOr, "|", 0, l, c};
    }
    case '^': {
      if (peek() == '=') { advance(); return {TokenType::XorEq, "^=", 0, l, c}; }
      return {TokenType::Xor, "^", 0, l, c};
    }
    case '~': return {TokenType::BitNot, "~", 0, l, c};
    default: return {TokenType::Eof, "", 0, l, c};
  }
}
