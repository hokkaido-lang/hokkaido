#include "lexer.h"

#include <cctype>
#include <unordered_map>

char Lexer::peek() {
  if (pos >= input.size()) return '\0';
  return input[pos];
}

char Lexer::advance() {
  char c = input[pos++];
  if (c == '\n') { line++; col = 1; }
  else { col++; }
  return c;
}

void Lexer::skip_whitespace() {
  while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\t' || input[pos] == '\r'))
    advance();
}

void Lexer::skip_line_comment() {
  while (pos < input.size() && input[pos] != '\n') advance();
}

void Lexer::skip_block_comment() {
  while (pos < input.size()) {
    if (peek() == '*' && pos + 1 < input.size() && input[pos + 1] == '/') {
      advance(); advance(); // skip */
      return;
    }
    advance();
  }
}

Token Lexer::next_token() {
  skip_whitespace();

  if (pos >= input.size()) return {TokenType::Eof, "", 0, line, col};

  int l = line, c = col;
  char ch = peek();

  // Line comments and block comments
  if (ch == '/') {
    if (pos + 1 < input.size() && input[pos + 1] == '/') {
      skip_line_comment();
      return next_token();
    }
    if (pos + 1 < input.size() && input[pos + 1] == '*') {
      advance(); advance(); // skip /*
      skip_block_comment();
      return next_token();
    }
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::SlashEq, "/=", 0, l, c};
    }
    advance();
    return {TokenType::Slash, "/", 0, l, c};
  }

  // Newlines (statement separators)
  if (ch == '\n') {
    advance();
    return {TokenType::Newline, "\\n", 0, l, c};
  }

  // Single-character tokens
  if (ch == ';') { advance(); return {TokenType::Semicolon, ";", 0, l, c}; }
  if (ch == '(') { advance(); return {TokenType::LParen, "(", 0, l, c}; }
  if (ch == ')') { advance(); return {TokenType::RParen, ")", 0, l, c}; }
  if (ch == '{') { advance(); return {TokenType::LBrace, "{", 0, l, c}; }
  if (ch == '}') { advance(); return {TokenType::RBrace, "}", 0, l, c}; }
  if (ch == ',') { advance(); return {TokenType::Comma, ",", 0, l, c}; }
  if (ch == '+') {
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::PlusEq, "+=", 0, l, c};
    }
    advance(); return {TokenType::Plus, "+", 0, l, c};
  }
  if (ch == ':') {
    if (pos + 1 < input.size() && input[pos + 1] == ':') {
      advance(); advance();
      return {TokenType::ColonColon, "::", 0, l, c};
    }
    advance();
    return {TokenType::Colon, ":", 0, l, c};
  }
  if (ch == '.') {
    if (pos + 2 < input.size() && input[pos + 1] == '.' && input[pos + 2] == '.') {
      advance(); advance(); advance();
      return {TokenType::Ellipsis, "...", 0, l, c};
    }
    advance();
    return {TokenType::Dot, ".", 0, l, c};
  }
  if (ch == '&') {
    if (pos + 1 < input.size() && input[pos + 1] == '&') {
      advance(); advance();
      return {TokenType::AndAnd, "&&", 0, l, c};
    }
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::AndEq, "&=", 0, l, c};
    }
    advance();
    return {TokenType::Ampersand, "&", 0, l, c};
  }
  if (ch == '|') {
    if (pos + 1 < input.size() && input[pos + 1] == '|') {
      advance(); advance();
      return {TokenType::OrOr, "||", 0, l, c};
    }
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::OrEq, "|=", 0, l, c};
    }
    advance();
    return {TokenType::BitOr, "|", 0, l, c};
  }
  if (ch == '^') {
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::XorEq, "^=", 0, l, c};
    }
    advance(); return {TokenType::Xor, "^", 0, l, c};
  }
  if (ch == '%') {
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::PercentEq, "%=", 0, l, c};
    }
    advance(); return {TokenType::Percent, "%", 0, l, c};
  }
  if (ch == '~') { advance(); return {TokenType::BitNot, "~", 0, l, c}; }
  if (ch == '*') {
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::StarEq, "*=", 0, l, c};
    }
    advance(); return {TokenType::Star, "*", 0, l, c};
  }
  if (ch == '[') { advance(); return {TokenType::LSquare, "[", 0, l, c}; }
  if (ch == ']') { advance(); return {TokenType::RSquare, "]", 0, l, c}; }

  // Multi-character tokens
  if (ch == '=') {
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::Eq, "==", 0, l, c};
    }
    if (pos + 1 < input.size() && input[pos + 1] == '>') {
      advance(); advance();
      return {TokenType::FatArrow, "=>", 0, l, c};
    }
    advance();
    return {TokenType::Equals, "=", 0, l, c};
  }
  if (ch == '!') {
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::Ne, "!=", 0, l, c};
    }
    advance();
    return {TokenType::Eof, "unexpected '!'", 0, l, c};
  }
  if (ch == '<') {
    if (pos + 2 < input.size() && input[pos + 1] == '<' && input[pos + 2] == '=') {
      advance(); advance(); advance();
      return {TokenType::ShlEq, "<<=", 0, l, c};
    }
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::LessEqual, "<=", 0, l, c};
    }
    if (pos + 1 < input.size() && input[pos + 1] == '<') {
      advance(); advance();
      return {TokenType::Shl, "<<", 0, l, c};
    }
    advance();
    return {TokenType::Less, "<", 0, l, c};
  }
  if (ch == '>') {
    if (pos + 2 < input.size() && input[pos + 1] == '>' && input[pos + 2] == '=') {
      advance(); advance(); advance();
      return {TokenType::ShrEq, ">>=", 0, l, c};
    }
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::GreaterEqual, ">=", 0, l, c};
    }
    if (pos + 1 < input.size() && input[pos + 1] == '>') {
      advance(); advance();
      return {TokenType::Shr, ">>", 0, l, c};
    }
    advance();
    return {TokenType::Greater, ">", 0, l, c};
  }

  // Arrow and minus
  if (ch == '-') {
    if (pos + 1 < input.size() && input[pos + 1] == '>') {
      advance(); advance();
      return {TokenType::Arrow, "->", 0, l, c};
    }
    if (pos + 1 < input.size() && input[pos + 1] == '=') {
      advance(); advance();
      return {TokenType::MinusEq, "-=", 0, l, c};
    }
    advance();
    return {TokenType::Minus, "-", 0, l, c};
  }

  // String literals
  if (ch == '"') {
    return lex_string(l, c);
  }

  // Character literals: 'a', '\n', etc.
  // Label prefix: 'identifier
  if (ch == '\'') {
    // Check if this is a label prefix 'ident (no closing quote soon)
    if (pos + 2 < input.size() && input[pos + 2] != '\'' && input[pos + 1] != '\\' &&
        std::isalpha(input[pos + 1])) {
      advance(); // skip '
      return {TokenType::Tick, "'", 0, l, c};
    }
    return lex_char_literal(l, c);
  }

  // Numbers (including negative literals)
  if (std::isdigit(ch) || (ch == '-' && pos + 1 < input.size() && std::isdigit(input[pos + 1]))) {
    return lex_number(l, c);
  }

  // Identifiers and keywords
  if (std::isalpha(ch) || ch == '_') {
    return lex_identifier(l, c);
  }

  // Unknown character
  std::string err = "unexpected character '";
  err += ch;
  err += "'";
  advance();
  return {TokenType::Eof, err, 0, l, c};
}

Token Lexer::lex_string(int l, int c) {
  advance(); // skip opening "
  std::string val;
  while (pos < input.size() && peek() != '"') {
    if (peek() == '\\') {
      advance();
      if (peek() == 'n') val += '\n';
      else if (peek() == 't') val += '\t';
      else if (peek() == '"') val += '"';
      else if (peek() == '\\') val += '\\';
      else val += peek();
      advance();
    } else {
      val += advance();
    }
  }
  if (pos >= input.size()) {
    return {TokenType::Eof, "unterminated string", 0, l, c};
  }
  advance(); // skip closing "
  return {TokenType::StringLiteral, val, 0, l, c};
}

Token Lexer::lex_number(int l, int c) {
  std::string num;
  bool is_float = false;
  if (peek() == '-') { num += advance(); }
  while (pos < input.size() && std::isdigit(peek())) {
    num += advance();
  }
  if (pos < input.size() && peek() == '.') {
    is_float = true;
    num += advance();
    while (pos < input.size() && std::isdigit(peek())) {
      num += advance();
    }
  }
  double val = std::stod(num);
  return {TokenType::Number, num, val, l, c};
}

Token Lexer::lex_identifier(int l, int c) {
  std::string id;
  while (pos < input.size() && (std::isalnum(peek()) || peek() == '_')) {
    id += advance();
  }

  const auto &kws = keywords();
  auto it = kws.find(id);
  if (it != kws.end()) {
    double val = (it->second == TokenType::True)  ? 1.0
               : (it->second == TokenType::False) ? 0.0 : 0.0;
    return {it->second, id, val, l, c};
  }

  return {TokenType::Identifier, id, 0, l, c};
}

const std::unordered_map<std::string, TokenType> &Lexer::keywords() {
  static const std::unordered_map<std::string, TokenType> kw = {
    {"let", TokenType::Let},       {"fn", TokenType::Fn},
    {"lambda", TokenType::Lambda}, {"return", TokenType::Return},
    {"asm", TokenType::Asm},       {"if", TokenType::If},
    {"else", TokenType::Else},     {"for", TokenType::For},
    {"break", TokenType::Break},   {"continue", TokenType::Continue},
    {"atomic", TokenType::Atomic}, {"struct", TokenType::Struct},
    {"include", TokenType::Include}, {"namespace", TokenType::Namespace},
    {"extern", TokenType::Extern}, {"trait", TokenType::Trait},
    {"impl", TokenType::Impl},     {"pub", TokenType::Pub},
    {"import", TokenType::Import}, {"package", TokenType::Package},
    {"match", TokenType::Match},   {"enum", TokenType::Enum},
    {"null", TokenType::Null},     {"mut", TokenType::Mut},
    {"cubical", TokenType::Cubical}, {"region", TokenType::Region},
    {"int", TokenType::Int64},     {"int8", TokenType::Int8},
    {"int16", TokenType::Int16},   {"int32", TokenType::Int32},
    {"int64", TokenType::Int64},   {"uint8", TokenType::Uint8},
    {"uint16", TokenType::Uint16}, {"uint32", TokenType::Uint32},
    {"uint64", TokenType::Uint64}, {"float", TokenType::Float64},
    {"float16", TokenType::Float16}, {"float32", TokenType::Float32},
    {"float64", TokenType::Float64}, {"bool", TokenType::Bool},
    {"string", TokenType::String}, {"void", TokenType::Void},
    {"char", TokenType::Char},     {"true", TokenType::True},
    {"false", TokenType::False},
  };
  return kw;
}

Token Lexer::lex_char_literal(int l, int c) {
  advance(); // skip opening '
  char val;
  if (peek() == '\\') {
    advance();
    switch (peek()) {
      case 'n': val = '\n'; break;
      case 't': val = '\t'; break;
      case '\\': val = '\\'; break;
      case '\'': val = '\''; break;
      default: val = peek(); break;
    }
    advance();
  } else {
    if (peek() == '\'') {
      return {TokenType::Eof, "empty char literal", 0, l, c};
    }
    val = advance();
  }
  if (peek() != '\'') {
    return {TokenType::Eof, "unterminated char literal", 0, l, c};
  }
  advance(); // skip closing '
  return {TokenType::CharLiteral, std::string(1, val), (double)(unsigned char)val, l, c};
}