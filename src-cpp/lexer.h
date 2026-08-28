#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// =========================================================================
// Hokkaido Language — Lexer
// =========================================================================

enum class TokenType {
  Eof,
  Let, Fn, Lambda, Return, Asm, If, Else, For, While, In, Break, Continue, Atomic,
  Struct, Include, Namespace, Extern,
  Trait, Impl,
  Pub, Import, Package,
  Match, Enum,
  Null, Mut,
  Tick,
  Int8, Int16, Int32, Int64, Uint8, Uint16, Uint32, Uint64,
  Float16, Float32, Float64, Bool, String, Char, Void,
  Region,
  Identifier,
  Number, True, False,
  StringLiteral, CharLiteral,
  Equals, Arrow, FatArrow,
  Eq, Ne, Less, Greater, LessEqual, GreaterEqual,
  AndAnd, OrOr, BitOr, Xor, Shr, Shl,
  PlusEq, MinusEq, StarEq, SlashEq, PercentEq,
  AndEq, OrEq, XorEq, ShlEq, ShrEq,
  Semicolon, Comma, Colon, ColonColon, Dot, DotDot, DotDotEq, Ellipsis,
  Newline,
  LParen, RParen, LBrace, RBrace,
  LSquare, RSquare,
  Plus, Minus, Star, Slash, Percent, Ampersand, BitNot,
};

struct Token {
  TokenType type;
  std::string text;
  double num_val = 0;
  int line = 0;
  int col = 0;
};

class Lexer {
  std::vector<char> input_;
  size_t pos_ = 0;
  int line_ = 1;
  int col_ = 1;
  std::unordered_map<std::string, TokenType> keywords_;

  char peek() const;
  char peek_offset(size_t offset) const;
  char advance();
  void skip_whitespace();
  void skip_line_comment();
  void skip_block_comment();
  Token lex_string(int l, int c);
  Token lex_number(int l, int c);
  Token lex_identifier(int l, int c);
  Token lex_char_literal(int l, int c);

public:
  explicit Lexer(const std::string &src);
  Token next_token();
};
