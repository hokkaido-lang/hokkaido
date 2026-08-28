#pragma once

#include <string>

// =========================================================================
// Hokkaido Language — Lexer (Rust-backed via FFI)
// =========================================================================

enum class TokenType {
  Eof,
  Let, Fn, Lambda, Return, Asm, If, Else, For, While, In, Break, Continue, Atomic, Struct, Include, Namespace, Extern,
  Trait, Impl,
  Pub, Import, Package,
  Match, Enum,
  Null, Mut,
  Tick,
  Int8, Int16, Int32, Int64, Uint8, Uint16, Uint32, Uint64, Float16, Float32, Float64, Bool, String, Char, Void,
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
  struct LexerData *data_ = nullptr;

public:
  Lexer(const std::string &src);
  ~Lexer();

  Lexer(const Lexer&) = delete;
  Lexer& operator=(const Lexer&) = delete;
  Lexer(Lexer &&other) noexcept;
  Lexer& operator=(Lexer &&other) noexcept;

  Token next_token();
};
