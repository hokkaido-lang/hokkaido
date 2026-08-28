#pragma once

#include <cstdint>

// =========================================================================
// Rust Lexer FFI — link against the hok_lexer_c static library
// =========================================================================

// TokenType enum values must match the Rust TokenType enum exactly.
// Both are #[repr(C)] / C-style enums with identical variant ordering.
enum class RustTokenType : int32_t {
  Eof = 0,
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

extern "C" {
  void *hok_lexer_new(const char *source);
  void hok_lexer_free(void *handle);

  RustTokenType hok_lexer_next(
    void *handle,
    int32_t *out_line,
    int32_t *out_col,
    double *out_num_val,
    char **out_text
  );

  void hok_lexer_free_string(char *s);
}
