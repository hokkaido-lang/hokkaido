#pragma once

#include <string>

// =========================================================================
// Hokkaido Language — Lexer
// =========================================================================

enum class TokenType {
  Eof,
  Let, Fn, Lambda, Return, Asm, If, Else, For, Break, Continue, Atomic, Struct, Include, Namespace, Extern,
  Trait, Impl,
  Pub, Import, Package,
  Match, Enum,
  Null,
  Cubical, Tick,
  Int8, Int16, Int32, Int64, Uint8, Uint16, Uint32, Uint64, Float16, Float32, Float64, Bool, String, Char, Void,
  Region, Linear,
  Identifier,
  Number, True, False,
  StringLiteral, CharLiteral,
  Equals, Arrow, FatArrow,
  Eq, Ne, Less, Greater, LessEqual, GreaterEqual,
  AndAnd, OrOr, BitOr, Xor, Shr, Shl,
  PlusEq, MinusEq, StarEq, SlashEq, PercentEq,
  AndEq, OrEq, XorEq, ShlEq, ShrEq,
  Semicolon, Comma, Colon, ColonColon, Dot, Ellipsis,
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
  std::string input;
  size_t pos = 0;
  int line = 1;
  int col = 1;

  char peek();
  char advance();
  void skip_whitespace();
  void skip_line_comment();
  void skip_block_comment();

public:
  Lexer(const std::string &src) : input(src) {}

  Token next_token();

private:
  Token lex_string(int l, int c);
  Token lex_number(int l, int c);
  Token lex_char_literal(int l, int c);
  Token lex_identifier(int l, int c);
};