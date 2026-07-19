use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::collections::HashMap;

// =========================================================================
// Token types — must match the C++ TokenType enum exactly
// =========================================================================

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TokenType {
    Eof,
    Let, Fn, Lambda, Return, Asm, If, Else, For, While, In, Break, Continue, Atomic,
    Struct, Include, Namespace, Extern,
    Trait, Impl,
    Pub, Import, Package,
    Match, Enum,
    Null, Mut,
    Cubical, Tick,
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
}

// =========================================================================
// Token struct — matches C++ Token layout
// =========================================================================

#[repr(C)]
#[derive(Debug, Clone)]
pub struct Token {
    pub ty: TokenType,
    // We return text as a separate pointer to avoid lifetime issues in FFI
    pub num_val: f64,
    pub line: i32,
    pub col: i32,
}

// =========================================================================
// Lexer implementation
// =========================================================================

pub struct Lexer {
    input: Vec<char>,
    pos: usize,
    line: i32,
    col: i32,
    keywords: HashMap<String, TokenType>,
}

impl Lexer {
    pub fn new(src: &str) -> Self {
        let mut keywords = HashMap::new();
        let kws: &[(&str, TokenType)] = &[
            ("let", TokenType::Let),       ("fn", TokenType::Fn),
            ("lambda", TokenType::Lambda), ("return", TokenType::Return),
            ("asm", TokenType::Asm),       ("if", TokenType::If),
            ("else", TokenType::Else),                 ("for", TokenType::For),
            ("while", TokenType::While),
            ("in", TokenType::In),
            ("break", TokenType::Break),   ("continue", TokenType::Continue),
            ("atomic", TokenType::Atomic), ("struct", TokenType::Struct),
            ("include", TokenType::Include), ("namespace", TokenType::Namespace),
            ("extern", TokenType::Extern), ("trait", TokenType::Trait),
            ("impl", TokenType::Impl),     ("pub", TokenType::Pub),
            ("import", TokenType::Import), ("package", TokenType::Package),
            ("match", TokenType::Match),   ("enum", TokenType::Enum),
            ("null", TokenType::Null),     ("mut", TokenType::Mut),
            ("cubical", TokenType::Cubical), ("region", TokenType::Region),
            ("int", TokenType::Int64),     ("int8", TokenType::Int8),
            ("int16", TokenType::Int16),   ("int32", TokenType::Int32),
            ("int64", TokenType::Int64),   ("uint8", TokenType::Uint8),
            ("uint16", TokenType::Uint16), ("uint32", TokenType::Uint32),
            ("uint64", TokenType::Uint64), ("float", TokenType::Float64),
            ("float16", TokenType::Float16), ("float32", TokenType::Float32),
            ("float64", TokenType::Float64), ("bool", TokenType::Bool),
            ("string", TokenType::String), ("void", TokenType::Void),
            ("char", TokenType::Char),     ("true", TokenType::True),
            ("false", TokenType::False),
        ];
        for &(k, v) in kws {
            keywords.insert(k.to_string(), v);
        }

        Lexer {
            input: src.chars().collect(),
            pos: 0,
            line: 1,
            col: 1,
            keywords,
        }
    }

    fn peek(&self) -> char {
        if self.pos >= self.input.len() { '\0' } else { self.input[self.pos] }
    }

    fn peek_offset(&self, offset: usize) -> char {
        let idx = self.pos + offset;
        if idx >= self.input.len() { '\0' } else { self.input[idx] }
    }

    fn advance(&mut self) -> char {
        let c = self.input[self.pos];
        self.pos += 1;
        if c == '\n' { self.line += 1; self.col = 1; }
        else { self.col += 1; }
        c
    }

    fn skip_whitespace(&mut self) {
        while self.pos < self.input.len() {
            let c = self.input[self.pos];
            if c == ' ' || c == '\t' || c == '\r' { self.advance(); }
            else { break; }
        }
    }

    fn skip_line_comment(&mut self) {
        while self.pos < self.input.len() && self.input[self.pos] != '\n' {
            self.advance();
        }
    }

    fn skip_block_comment(&mut self) {
        while self.pos < self.input.len() {
            if self.peek() == '*' && self.peek_offset(1) == '/' {
                self.advance(); self.advance(); // skip */
                return;
            }
            self.advance();
        }
    }

    pub fn next_token(&mut self) -> (Token, String) {
        self.skip_whitespace();

        if self.pos >= self.input.len() {
            return (Token { ty: TokenType::Eof, num_val: 0.0, line: self.line, col: self.col }, String::new());
        }

        let l = self.line;
        let c = self.col;
        let ch = self.peek();

        // Line comments and block comments
        if ch == '/' {
            if self.peek_offset(1) == '/' {
                self.skip_line_comment();
                return self.next_token();
            }
            if self.peek_offset(1) == '*' {
                self.advance(); self.advance(); // skip /*
                self.skip_block_comment();
                return self.next_token();
            }
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::SlashEq, num_val: 0.0, line: l, col: c }, "/=".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Slash, num_val: 0.0, line: l, col: c }, "/".to_string());
        }

        // Newlines
        if ch == '\n' {
            self.advance();
            return (Token { ty: TokenType::Newline, num_val: 0.0, line: l, col: c }, "\\n".to_string());
        }

        // Single-character tokens
        macro_rules! single {
            ($tok:expr, $ch:expr) => {{
                self.advance();
                return (Token { ty: $tok, num_val: 0.0, line: l, col: c }, $ch.to_string());
            }};
        }

        match ch {
            ';' => single!(TokenType::Semicolon, ";"),
            '(' => single!(TokenType::LParen, "("),
            ')' => single!(TokenType::RParen, ")"),
            '{' => single!(TokenType::LBrace, "{"),
            '}' => single!(TokenType::RBrace, "}"),
            ',' => single!(TokenType::Comma, ","),
            '[' => single!(TokenType::LSquare, "["),
            ']' => single!(TokenType::RSquare, "]"),
            '~' => single!(TokenType::BitNot, "~"),
            _ => {}
        }

        // Two/three-character tokens
        if ch == '+' {
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::PlusEq, num_val: 0.0, line: l, col: c }, "+=".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Plus, num_val: 0.0, line: l, col: c }, "+".to_string());
        }
        if ch == ':' {
            if self.peek_offset(1) == ':' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::ColonColon, num_val: 0.0, line: l, col: c }, "::".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Colon, num_val: 0.0, line: l, col: c }, ":".to_string());
        }
        if ch == '.' {
            if self.peek_offset(1) == '.' && self.peek_offset(2) == '=' {
                self.advance(); self.advance(); self.advance();
                return (Token { ty: TokenType::DotDotEq, num_val: 0.0, line: l, col: c }, "..=".to_string());
            }
            if self.peek_offset(1) == '.' && self.peek_offset(2) == '.' {
                self.advance(); self.advance(); self.advance();
                return (Token { ty: TokenType::Ellipsis, num_val: 0.0, line: l, col: c }, "...".to_string());
            }
            if self.peek_offset(1) == '.' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::DotDot, num_val: 0.0, line: l, col: c }, "..".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Dot, num_val: 0.0, line: l, col: c }, ".".to_string());
        }
        if ch == '&' {
            if self.peek_offset(1) == '&' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::AndAnd, num_val: 0.0, line: l, col: c }, "&&".to_string());
            }
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::AndEq, num_val: 0.0, line: l, col: c }, "&=".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Ampersand, num_val: 0.0, line: l, col: c }, "&".to_string());
        }
        if ch == '|' {
            if self.peek_offset(1) == '|' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::OrOr, num_val: 0.0, line: l, col: c }, "||".to_string());
            }
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::OrEq, num_val: 0.0, line: l, col: c }, "|=".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::BitOr, num_val: 0.0, line: l, col: c }, "|".to_string());
        }
        if ch == '^' {
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::XorEq, num_val: 0.0, line: l, col: c }, "^=".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Xor, num_val: 0.0, line: l, col: c }, "^".to_string());
        }
        if ch == '%' {
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::PercentEq, num_val: 0.0, line: l, col: c }, "%=".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Percent, num_val: 0.0, line: l, col: c }, "%".to_string());
        }
        if ch == '*' {
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::StarEq, num_val: 0.0, line: l, col: c }, "*=".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Star, num_val: 0.0, line: l, col: c }, "*".to_string());
        }
        if ch == '=' {
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::Eq, num_val: 0.0, line: l, col: c }, "==".to_string());
            }
            if self.peek_offset(1) == '>' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::FatArrow, num_val: 0.0, line: l, col: c }, "=>".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Equals, num_val: 0.0, line: l, col: c }, "=".to_string());
        }
        if ch == '!' {
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::Ne, num_val: 0.0, line: l, col: c }, "!=".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Eof, num_val: 0.0, line: l, col: c }, "unexpected '!'".to_string());
        }
        if ch == '<' {
            if self.peek_offset(1) == '<' && self.peek_offset(2) == '=' {
                self.advance(); self.advance(); self.advance();
                return (Token { ty: TokenType::ShlEq, num_val: 0.0, line: l, col: c }, "<<=".to_string());
            }
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::LessEqual, num_val: 0.0, line: l, col: c }, "<=".to_string());
            }
            if self.peek_offset(1) == '<' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::Shl, num_val: 0.0, line: l, col: c }, "<<".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Less, num_val: 0.0, line: l, col: c }, "<".to_string());
        }
        if ch == '>' {
            if self.peek_offset(1) == '>' && self.peek_offset(2) == '=' {
                self.advance(); self.advance(); self.advance();
                return (Token { ty: TokenType::ShrEq, num_val: 0.0, line: l, col: c }, ">>=".to_string());
            }
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::GreaterEqual, num_val: 0.0, line: l, col: c }, ">=".to_string());
            }
            if self.peek_offset(1) == '>' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::Shr, num_val: 0.0, line: l, col: c }, ">>".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Greater, num_val: 0.0, line: l, col: c }, ">".to_string());
        }
        if ch == '-' {
            if self.peek_offset(1) == '>' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::Arrow, num_val: 0.0, line: l, col: c }, "->".to_string());
            }
            if self.peek_offset(1) == '=' {
                self.advance(); self.advance();
                return (Token { ty: TokenType::MinusEq, num_val: 0.0, line: l, col: c }, "-=".to_string());
            }
            self.advance();
            return (Token { ty: TokenType::Minus, num_val: 0.0, line: l, col: c }, "-".to_string());
        }

        // String literals
        if ch == '"' {
            return self.lex_string(l, c);
        }

        // Character literals / label prefix
        if ch == '\'' {
            // Check if this is a label prefix 'ident (no closing quote soon)
            if self.peek_offset(1).is_ascii_alphabetic()
                && self.peek_offset(2) != '\''
                && self.peek_offset(1) != '\\'
            {
                self.advance(); // skip '
                return (Token { ty: TokenType::Tick, num_val: 0.0, line: l, col: c }, "'".to_string());
            }
            return self.lex_char_literal(l, c);
        }

        // Numbers
        if ch.is_ascii_digit() || (ch == '-' && self.peek_offset(1).is_ascii_digit()) {
            return self.lex_number(l, c);
        }

        // Identifiers and keywords
        if ch.is_ascii_alphabetic() || ch == '_' {
            return self.lex_identifier(l, c);
        }

        // Unknown character
        let err = format!("unexpected character '{}'", ch);
        self.advance();
        (Token { ty: TokenType::Eof, num_val: 0.0, line: l, col: c }, err)
    }

    fn lex_string(&mut self, l: i32, c: i32) -> (Token, String) {
        self.advance(); // skip opening "
        let mut val = String::new();
        while self.pos < self.input.len() && self.peek() != '"' {
            if self.peek() == '\\' {
                self.advance();
                match self.peek() {
                    'n' => val.push('\n'),
                    't' => val.push('\t'),
                    '"' => val.push('"'),
                    '\\' => val.push('\\'),
                    c => val.push(c),
                }
                self.advance();
            } else {
                val.push(self.advance());
            }
        }
        if self.pos >= self.input.len() {
            return (Token { ty: TokenType::Eof, num_val: 0.0, line: l, col: c }, "unterminated string".to_string());
        }
        self.advance(); // skip closing "
        (Token { ty: TokenType::StringLiteral, num_val: 0.0, line: l, col: c }, val)
    }

    fn lex_number(&mut self, l: i32, c: i32) -> (Token, String) {
        let mut num = String::new();
        if self.peek() == '-' { num.push(self.advance()); }
        while self.pos < self.input.len() && self.peek().is_ascii_digit() {
            num.push(self.advance());
        }
        if self.pos < self.input.len() && self.peek() == '.' {
            // Don't consume `.` if followed by `.` (range) or `=` (range inclusive)
            if self.peek_offset(1) != '.' && self.peek_offset(1) != '=' {
                num.push(self.advance());
                while self.pos < self.input.len() && self.peek().is_ascii_digit() {
                    num.push(self.advance());
                }
            }
        }
        let val: f64 = num.parse().unwrap_or(0.0);
        (Token { ty: TokenType::Number, num_val: val, line: l, col: c }, num)
    }

    fn lex_identifier(&mut self, l: i32, c: i32) -> (Token, String) {
        let mut id = String::new();
        while self.pos < self.input.len() && (self.peek().is_ascii_alphanumeric() || self.peek() == '_') {
            id.push(self.advance());
        }

        if let Some(&tok_type) = self.keywords.get(&id) {
            let val = match tok_type {
                TokenType::True => 1.0,
                TokenType::False => 0.0,
                _ => 0.0,
            };
            return (Token { ty: tok_type, num_val: val, line: l, col: c }, id);
        }

        (Token { ty: TokenType::Identifier, num_val: 0.0, line: l, col: c }, id)
    }

    fn lex_char_literal(&mut self, l: i32, c: i32) -> (Token, String) {
        self.advance(); // skip opening '
        let val: char;
        if self.peek() == '\\' {
            self.advance();
            val = match self.peek() {
                'n' => '\n',
                't' => '\t',
                '\\' => '\\',
                '\'' => '\'',
                other => other,
            };
            self.advance();
        } else {
            if self.peek() == '\'' {
                return (Token { ty: TokenType::Eof, num_val: 0.0, line: l, col: c }, "empty char literal".to_string());
            }
            val = self.advance();
        }
        if self.peek() != '\'' {
            return (Token { ty: TokenType::Eof, num_val: 0.0, line: l, col: c }, "unterminated char literal".to_string());
        }
        self.advance(); // skip closing '
        let num_val = val as u8 as f64;
        (Token { ty: TokenType::CharLiteral, num_val, line: l, col: c }, val.to_string())
    }
}

// =========================================================================
// C FFI interface
// =========================================================================

/// Opaque handle to a Rust Lexer instance
pub struct LexerHandle {
    lexer: Lexer,
}

/// Create a new lexer for the given source string.
/// Returns a handle to the lexer, or null on error.
#[no_mangle]
pub extern "C" fn hok_lexer_new(source: *const c_char) -> *mut LexerHandle {
    if source.is_null() { return std::ptr::null_mut(); }
    let c_str = unsafe { CStr::from_ptr(source) };
    let src = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
    let lexer = Lexer::new(src);
    Box::into_raw(Box::new(LexerHandle { lexer }))
}

/// Destroy a lexer handle and free its memory.
#[no_mangle]
pub extern "C" fn hok_lexer_free(handle: *mut LexerHandle) {
    if !handle.is_null() {
        unsafe { drop(Box::from_raw(handle)); }
    }
}

/// Lex the next token from the lexer.
/// Returns the token type, and writes token metadata to the output pointers.
/// The text is returned via the text pointer (caller must free with hok_lexer_free_string).
/// Returns TokenType::Eof when input is exhausted.
#[no_mangle]
pub extern "C" fn hok_lexer_next(
    handle: *mut LexerHandle,
    out_line: *mut i32,
    out_col: *mut i32,
    out_num_val: *mut f64,
    out_text: *mut *mut c_char,
) -> TokenType {
    if handle.is_null() { return TokenType::Eof; }
    let h = unsafe { &mut *handle };
    let (token, text) = h.lexer.next_token();

    unsafe {
        if !out_line.is_null() { *out_line = token.line; }
        if !out_col.is_null() { *out_col = token.col; }
        if !out_num_val.is_null() { *out_num_val = token.num_val; }
        if !out_text.is_null() {
            *out_text = match CString::new(text) {
                Ok(s) => s.into_raw(),
                Err(_) => std::ptr::null_mut(),
            };
        }
    }

    token.ty
}

/// Free a string returned by hok_lexer_next's out_text parameter.
#[no_mangle]
pub extern "C" fn hok_lexer_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)); }
    }
}

// =========================================================================
// Tests
// =========================================================================

#[cfg(test)]
#[allow(unused_unsafe)]
mod tests {
    use super::*;

    fn lex_all(src: &str) -> Vec<Token> {
        let mut lexer = Lexer::new(src);
        let mut tokens = Vec::new();
        loop {
            let (tok, _) = lexer.next_token();
            if tok.ty == TokenType::Eof {
                break;
            }
            tokens.push(tok);
        }
        tokens
    }

    fn lex_types(src: &str) -> Vec<TokenType> {
        lex_all(src).into_iter().map(|t| t.ty).collect()
    }

    #[test]
    fn empty_input() {
        assert_eq!(lex_types(""), vec![]);
    }

    #[test]
    fn single_tokens() {
        assert_eq!(lex_types(";"), vec![TokenType::Semicolon]);
        assert_eq!(lex_types("("), vec![TokenType::LParen]);
        assert_eq!(lex_types(")"), vec![TokenType::RParen]);
        assert_eq!(lex_types("{"), vec![TokenType::LBrace]);
        assert_eq!(lex_types("}"), vec![TokenType::RBrace]);
        assert_eq!(lex_types("["), vec![TokenType::LSquare]);
        assert_eq!(lex_types("]"), vec![TokenType::RSquare]);
        assert_eq!(lex_types(","), vec![TokenType::Comma]);
        assert_eq!(lex_types("."), vec![TokenType::Dot]);
        assert_eq!(lex_types("~"), vec![TokenType::BitNot]);
    }

    #[test]
    fn arithmetic_operators() {
        assert_eq!(lex_types("+ - * / %"), vec![
            TokenType::Plus, TokenType::Minus, TokenType::Star,
            TokenType::Slash, TokenType::Percent,
        ]);
    }

    #[test]
    fn compound_assignment() {
        assert_eq!(lex_types("+= -= *= /= %= &= |= ^= <<= >>="), vec![
            TokenType::PlusEq, TokenType::MinusEq, TokenType::StarEq,
            TokenType::SlashEq, TokenType::PercentEq, TokenType::AndEq,
            TokenType::OrEq, TokenType::XorEq, TokenType::ShlEq, TokenType::ShrEq,
        ]);
    }

    #[test]
    fn comparison_operators() {
        assert_eq!(lex_types("== != < > <= >="), vec![
            TokenType::Eq, TokenType::Ne, TokenType::Less,
            TokenType::Greater, TokenType::LessEqual, TokenType::GreaterEqual,
        ]);
    }

    #[test]
    fn logical_and_bitwise() {
        assert_eq!(lex_types("&& || & | ^ << >>"), vec![
            TokenType::AndAnd, TokenType::OrOr, TokenType::Ampersand,
            TokenType::BitOr, TokenType::Xor, TokenType::Shl, TokenType::Shr,
        ]);
    }

    #[test]
    fn arrows() {
        assert_eq!(lex_types("-> =>"), vec![TokenType::Arrow, TokenType::FatArrow]);
    }

    #[test]
    fn ellipsis() {
        assert_eq!(lex_types("..."), vec![TokenType::Ellipsis]);
    }

    #[test]
    fn colon_colon() {
        assert_eq!(lex_types(": ::"), vec![TokenType::Colon, TokenType::ColonColon]);
    }

    #[test]
    fn integer_numbers() {
        let tokens = lex_all("42 0 1234567");
        assert_eq!(tokens.len(), 3);
        for t in &tokens {
            assert_eq!(t.ty, TokenType::Number);
        }
        assert_eq!(tokens[0].num_val, 42.0);
        assert_eq!(tokens[1].num_val, 0.0);
        assert_eq!(tokens[2].num_val, 1234567.0);
    }

    #[test]
    fn negative_numbers() {
        // '-' at start of expression is lexed as Minus, then number
        let types = lex_types("-5");
        assert_eq!(types, vec![TokenType::Minus, TokenType::Number]);
        let tokens = lex_all("-5");
        assert_eq!(tokens[1].num_val, 5.0);
    }

    #[test]
    fn float_numbers() {
        let tokens = lex_all("3.14 0.5");
        assert_eq!(tokens.len(), 2);
        assert_eq!(tokens[0].num_val, 3.14);
        assert_eq!(tokens[1].num_val, 0.5);
    }

    #[test]
    fn identifiers() {
        assert_eq!(lex_types("foo bar _baz x1"), vec![
            TokenType::Identifier, TokenType::Identifier,
            TokenType::Identifier, TokenType::Identifier,
        ]);
    }

    #[test]
    fn keywords() {
        assert_eq!(lex_types("let fn return if else for break continue"), vec![
            TokenType::Let, TokenType::Fn, TokenType::Return,
            TokenType::If, TokenType::Else, TokenType::For,
            TokenType::Break, TokenType::Continue,
        ]);
    }

    #[test]
    fn more_keywords() {
        assert_eq!(lex_types("struct enum match impl trait"), vec![
            TokenType::Struct, TokenType::Enum, TokenType::Match,
            TokenType::Impl, TokenType::Trait,
        ]);
    }

    #[test]
    fn type_keywords() {
        assert_eq!(lex_types("int int8 int32 uint64 float32 bool string void"), vec![
            TokenType::Int64, TokenType::Int8, TokenType::Int32,
            TokenType::Uint64, TokenType::Float32, TokenType::Bool,
            TokenType::String, TokenType::Void,
        ]);
    }

    #[test]
    fn boolean_keywords() {
        let tokens = lex_all("true false");
        assert_eq!(tokens[0].ty, TokenType::True);
        assert_eq!(tokens[0].num_val, 1.0);
        assert_eq!(tokens[1].ty, TokenType::False);
        assert_eq!(tokens[1].num_val, 0.0);
    }

    fn lex_all_with_text(src: &str) -> Vec<(Token, String)> {
        let mut lexer = Lexer::new(src);
        let mut tokens = Vec::new();
        loop {
            let (tok, text) = lexer.next_token();
            if tok.ty == TokenType::Eof {
                break;
            }
            tokens.push((tok, text));
        }
        tokens
    }

    #[test]
    fn string_literal() {
        let tokens = lex_all_with_text(r#""hello world""#);
        assert_eq!(tokens.len(), 1);
        assert_eq!(tokens[0].0.ty, TokenType::StringLiteral);
        assert_eq!(tokens[0].1, "hello world");
    }

    #[test]
    fn string_with_escapes() {
        let tokens = lex_all_with_text(r#""line1\nline2\ttab""#);
        assert_eq!(tokens.len(), 1);
        assert_eq!(tokens[0].0.ty, TokenType::StringLiteral);
        assert_eq!(tokens[0].1, "line1\nline2\ttab");
    }

    #[test]
    fn char_literal() {
        let tokens = lex_all("'a'");
        assert_eq!(tokens.len(), 1);
        assert_eq!(tokens[0].ty, TokenType::CharLiteral);
        assert_eq!(tokens[0].num_val, b'a' as f64);
    }

    #[test]
    fn char_literal_escape() {
        let tokens = lex_all("'\\n'");
        assert_eq!(tokens.len(), 1);
        assert_eq!(tokens[0].ty, TokenType::CharLiteral);
        assert_eq!(tokens[0].num_val, b'\n' as f64);
    }

    #[test]
    fn tick_label() {
        // 'label → Tick then identifier; 'x → Tick then identifier
        assert_eq!(lex_types("'label 'x"), vec![TokenType::Tick, TokenType::Identifier, TokenType::Tick, TokenType::Identifier]);
    }

    #[test]
    fn comments_skipped() {
        let types = lex_types("42 // this is a comment\n99");
        assert_eq!(types, vec![TokenType::Number, TokenType::Newline, TokenType::Number]);
    }

    #[test]
    fn block_comment_skipped() {
        assert_eq!(lex_types("1 /* comment */ 2"), vec![TokenType::Number, TokenType::Number]);
    }

    #[test]
    fn line_tracking() {
        let mut lexer = Lexer::new("let\nfn");
        let t1 = lexer.next_token();
        assert_eq!(t1.0.line, 1);
        let t2 = lexer.next_token();
        assert_eq!(t2.0.ty, TokenType::Newline);
        let t3 = lexer.next_token();
        assert_eq!(t3.0.line, 2);
    }

    #[test]
    fn col_tracking() {
        let mut lexer = Lexer::new("  let");
        let (tok, _) = lexer.next_token();
        assert_eq!(tok.col, 3);
    }

    #[test]
    fn mixed_expression() {
        assert_eq!(lex_types("let x = 42 + 3.14;"), vec![
            TokenType::Let, TokenType::Identifier, TokenType::Equals,
            TokenType::Number, TokenType::Plus, TokenType::Number,
            TokenType::Semicolon,
        ]);
    }

    #[test]
    fn function_signature() {
        assert_eq!(lex_types("fn add(a: int, b: int) -> int"), vec![
            TokenType::Fn, TokenType::Identifier,
            TokenType::LParen, TokenType::Identifier, TokenType::Colon,
            TokenType::Int64, TokenType::Comma,
            TokenType::Identifier, TokenType::Colon, TokenType::Int64,
            TokenType::RParen, TokenType::Arrow, TokenType::Int64,
        ]);
    }

    #[test]
    fn namespace_path() {
        assert_eq!(lex_types("std::collections::HashMap"), vec![
            TokenType::Identifier, TokenType::ColonColon,
            TokenType::Identifier, TokenType::ColonColon, TokenType::Identifier,
        ]);
    }

    #[test]
    fn if_else_chain() {
        assert_eq!(lex_types("if x > 0 { } else { }"), vec![
            TokenType::If, TokenType::Identifier, TokenType::Greater,
            TokenType::Number, TokenType::LBrace, TokenType::RBrace,
            TokenType::Else, TokenType::LBrace, TokenType::RBrace,
        ]);
    }

    #[test]
    fn for_loop() {
        assert_eq!(lex_types("for i in 0..10 { }"), vec![
            TokenType::For, TokenType::Identifier, TokenType::In,
            TokenType::Number, TokenType::DotDot, TokenType::Number,
            TokenType::LBrace, TokenType::RBrace,
        ]);
    }

    #[test]
    fn for_loop_inclusive() {
        assert_eq!(lex_types("for i in 0..=10 { }"), vec![
            TokenType::For, TokenType::Identifier, TokenType::In,
            TokenType::Number, TokenType::DotDotEq, TokenType::Number,
            TokenType::LBrace, TokenType::RBrace,
        ]);
    }

    #[test]
    fn while_loop() {
        assert_eq!(lex_types("while x > 0 { x = x - 1 }"), vec![
            TokenType::While, TokenType::Identifier, TokenType::Greater,
            TokenType::Number, TokenType::LBrace, TokenType::Identifier,
            TokenType::Equals, TokenType::Identifier, TokenType::Minus,
            TokenType::Number, TokenType::RBrace,
        ]);
    }

    #[test]
    fn struct_definition() {
        assert_eq!(lex_types("pub struct Point { x: float64, y: float64 }"), vec![
            TokenType::Pub, TokenType::Struct, TokenType::Identifier,
            TokenType::LBrace, TokenType::Identifier, TokenType::Colon,
            TokenType::Float64, TokenType::Comma, TokenType::Identifier,
            TokenType::Colon, TokenType::Float64, TokenType::RBrace,
        ]);
    }

    #[test]
    fn impl_block() {
        assert_eq!(lex_types("impl Foo { fn bar() -> void { } }"), vec![
            TokenType::Impl, TokenType::Identifier, TokenType::LBrace,
            TokenType::Fn, TokenType::Identifier, TokenType::LParen,
            TokenType::RParen, TokenType::Arrow, TokenType::Void,
            TokenType::LBrace, TokenType::RBrace, TokenType::RBrace,
        ]);
    }

    #[test]
    fn import_statement() {
        assert_eq!(lex_types("import \"std\""), vec![
            TokenType::Import, TokenType::StringLiteral,
        ]);
    }

    #[test]
    fn cjk_identifiers() {
        // CJK chars are not ASCII-alphanumeric, so they should cause unexpected char errors
        let types = lex_types("abc 漢字");
        assert!(types.contains(&TokenType::Identifier));
    }

    #[test]
    fn nested_block_comments() {
        // Lexer does NOT support nested block comments; inner /* is consumed
        // `/* a /* b */ c */` → first `/*` opens, `*/` after b closes, leaving `c */`
        assert_eq!(lex_types("1 /* a /* b */ c */ 2"), vec![
            TokenType::Number, TokenType::Identifier, TokenType::Star, TokenType::Slash, TokenType::Number,
        ]);
    }

    #[test]
    fn empty_char_literal_error() {
        let mut lexer = Lexer::new("''");
        let (tok, text) = lexer.next_token();
        assert_eq!(tok.ty, TokenType::Eof);
        assert!(text.contains("empty"));
    }

    #[test]
    fn unterminated_string_error() {
        let mut lexer = Lexer::new(r#""hello"#);
        let (tok, text) = lexer.next_token();
        assert_eq!(tok.ty, TokenType::Eof);
        assert!(text.contains("unterminated"));
    }

    #[test]
    fn unknown_character() {
        let mut lexer = Lexer::new("@");
        let (tok, text) = lexer.next_token();
        assert_eq!(tok.ty, TokenType::Eof);
        assert!(text.contains("unexpected"));
    }

    #[test]
    fn standalone_bang_is_error() {
        let mut lexer = Lexer::new("!");
        let (tok, text) = lexer.next_token();
        assert_eq!(tok.ty, TokenType::Eof);
        assert!(text.contains("unexpected"));
    }

    // === FFI-level tests ===

    #[test]
    fn ffi_basic() {
        let src = CString::new("let x = 42;").unwrap();
        let handle = unsafe { hok_lexer_new(src.as_ptr()) };
        assert!(!handle.is_null());

        let mut line = 0i32;
        let mut col = 0i32;
        let mut num_val = 0.0f64;
        let mut text_ptr: *mut c_char = std::ptr::null_mut();

        let tt = unsafe { hok_lexer_next(handle, &mut line, &mut col, &mut num_val, &mut text_ptr) };
        assert_eq!(tt, TokenType::Let);

        let tt = unsafe { hok_lexer_next(handle, &mut line, &mut col, &mut num_val, &mut text_ptr) };
        assert_eq!(tt, TokenType::Identifier);

        let tt = unsafe { hok_lexer_next(handle, &mut line, &mut col, &mut num_val, &mut text_ptr) };
        assert_eq!(tt, TokenType::Equals);

        let tt = unsafe { hok_lexer_next(handle, &mut line, &mut col, &mut num_val, &mut text_ptr) };
        assert_eq!(tt, TokenType::Number);
        assert_eq!(num_val, 42.0);

        let tt = unsafe { hok_lexer_next(handle, &mut line, &mut col, &mut num_val, &mut text_ptr) };
        assert_eq!(tt, TokenType::Semicolon);

        let tt = unsafe { hok_lexer_next(handle, &mut line, &mut col, &mut num_val, &mut text_ptr) };
        assert_eq!(tt, TokenType::Eof);

        unsafe { hok_lexer_free(handle); }
    }

    #[test]
    fn ffi_null_handle() {
        let mut line = 0i32;
        let mut col = 0i32;
        let mut num_val = 0.0f64;
        let mut text_ptr: *mut c_char = std::ptr::null_mut();
        let tt = unsafe { hok_lexer_next(std::ptr::null_mut(), &mut line, &mut col, &mut num_val, &mut text_ptr) };
        assert_eq!(tt, TokenType::Eof);
    }

    #[test]
    fn ffi_free_string() {
        let s = CString::new("test").unwrap();
        let ptr = s.into_raw();
        // Should not crash
        unsafe { hok_lexer_free_string(ptr); }
    }

    #[test]
    fn ffi_free_null_string() {
        unsafe { hok_lexer_free_string(std::ptr::null_mut()); }
    }

    #[test]
    fn ffi_free_null_handle() {
        unsafe { hok_lexer_free(std::ptr::null_mut()); }
    }
}
