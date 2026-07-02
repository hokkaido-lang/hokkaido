---
layout: ../../layouts/DocsLayout.astro
title: Design Decisions
---

# Phase 0 — Design Decisions

This document records the design decisions made in Phase 0 of the
[Feature Implementation Roadmap](/plan). Each section below states the chosen
design, the rationale, and the implications for later phases. Subsequent
phases should cite this document rather than re-debating the same questions.

---

## 1. Unsigned Integer Semantics

**Decision:** Add distinct unsigned types `uint8`, `uint16`, `uint32`, `uint64`
as first-class type kinds, parallel to the existing signed types.

### Rationale

| Option | Pros | Cons |
|--------|------|------|
| New distinct types | Clear semantics; type-checker can enforce signed/unsigned mismatch errors; matches Rust/LLVM convention | More types to implement; more parser tokens |
| Signedness flag on `int` | Fewer types; simpler lexer | Type equivalence becomes ambiguous (`int` + `signed` = `int` + `unsigned`?); every comparison needs a flag lookup; hard to display in error messages |

LLVM already distinguishes signed/unsigned at the instruction level (`sdiv` vs
`udiv`, `srem` vs `urem`, `ashr` vs `lshr`, signed vs unsigned comparison
predicates). Distinct types map directly to LLVM's model and eliminate an
entire class of subtle bugs (e.g., comparing a "signed flag int" with a
"regular int").

### Implementation

- Add `TypeKind::Uint8`, `Uint16`, `Uint32`, `Uint64` variants.
- Add corresponding tokens: `uint8`, `uint16`, `uint32`, `uint64`.
- Extend `TypeAnnotation` if needed (currently no signedness field — the
  kind itself encodes it).
- Arithmetic codegen:
  - `Add`, `Sub`, `Mul`: LLVM `add`/`sub`/`mul` work identically for
    signed and unsigned (two's complement). No change needed.
  - `Div`: `sdiv` for signed, `udiv` for unsigned.
  - `Mod`: `srem` for signed, `urem` for unsigned.
  - `Shr`: `ashr` (arithmetic) for signed, `lshr` (logical) for unsigned.
  - `Shl`: identical for both.
- Comparison codegen: select signed predicate (`ICMP_SLT`/`ICMP_SLE`/etc.)
  for signed types, unsigned predicate (`ICMP_ULT`/`ICMP_ULE`/etc.) for
  unsigned types.
- Literal coercion: integer literals coerce to any integer type (signed or
  unsigned) when unambiguous from context. An explicit negative literal
  coerces only to signed types (error if assigned to `uint` without a cast).
- No implicit conversions between signed and unsigned types of different
  signedness — the programmer must use an explicit cast or the compiler
  emits a warning.

### Impact on later phases

- Phase 1 (type system gaps): unsigned types were the largest item; they
  touched lexer, parser, type checker, and codegen.
- Phase 3 (slices): slice length should be `uint64` (or `int`? decided
  below).
- Phase 4 (closures): no impact.
- Phase 5 (generics): unsigned types should be usable as type arguments.

---

## 2. Dynamic Memory Strategy

**Decision:** Dynamic memory will be a language feature via built-in
`alloc<T>(count: uint64) -> T*` and `free(ptr: void*)` compiler intrinsics.

### Rationale

| Option | Pros | Cons |
|--------|------|------|
| Builtins | Consistent syntax; type-safe; can add alignment/zeroing guarantees later | Adds compiler complexity; ties to platform allocator |
| FFI-only (`extern fn malloc`) | Zero compiler changes; simple | Unsafe (void*); no type safety; no size tracking; every user re-declares malloc |

Given that Phase 3 introduces slices (fat pointers with a length field), the
language needs a canonical way to allocate backing storage for slices. An
FFI-only approach would require every slice user to manually call `malloc`,
cast the result, and track the length — exactly the kind of boilerplate a
systems language should eliminate.

### Specification

```
// Allocate `count` elements of type T, uninitialized.
// Returns a pointer to the allocated memory, or null on OOM.
fn alloc<T>(count: uint64) -> T*

// Free memory previously returned by alloc.
fn free(ptr: void*)
```

- `alloc` and `free` are compiler intrinsics, not library functions.
- `alloc` maps to the platform's aligned malloc (e.g., `aligned_alloc` on
  POSIX, `_aligned_malloc` on MSVC).
- The size in bytes is `count * sizeof(T)`, computed at compile time
  (monomorphization) or via a size-table lookup at runtime for dynamically
  sized types (deferred to a later phase).
- `free` takes `void*` (represented as `int8*` in the type system).
- Out-of-memory returns null; the caller must check. There is no exception
  mechanism.

### Impact on later phases

- Phase 3 (slices): the growable array / "vec" utility can use `alloc`
  internally.
- Phase 6 (stdlib): the minimal collection described in Phase 6 depends on
  `alloc`/`free` being available.
- Phase 4 (closures): heap-captured closures are not in scope for v1
  (by-value capture only), so no impact.

---

## 3. Generics and Trait Bounds

**Decision:** Defer trait-like bounds on type parameters to a future major
version. Phase 5 will implement generic structs and enums without bounds.

### Rationale

Adding trait bounds introduces a large design surface:
- Trait declaration syntax (`trait Foo { fn bar(); }`)
- Trait implementation syntax (`impl Foo for MyType { ... }`)
- Where-clause syntax on generic declarations
- Resolution of trait method calls (vtable dispatch vs. static dispatch)
- Coherence rules (orphan rules, overlap checking)

None of these exist in the language today. Implementing generic structs/enums
without bounds is strictly additive to the existing monomorphization machinery
and does not require a trait system. Bounds can be added later as a backward-
compatible extension.

### What this means for Phase 5

- Generic structs: `struct Pair<A, B> { first: A, second: B }` — works.
- Generic enums: `enum Option<T> { Some { value: T }, None {} }` — works.
- Type parameters on struct/enum fields can be used in function signatures
  (e.g., `fn first<A, B>(p: Pair<A, B>) -> A`).
- No ability to write `fn sort<T: Ord>(...)` — users must accept concrete
  types or use function pointers for now.

### Impact on later phases

- Phase 4 (operator overloading): flagged as "deferred" in the roadmap.
  This decision reinforces that — operator overloading would benefit from
  trait bounds, so it stays deferred.
- Future major version: trait bounds can be added as a non-breaking
  extension.

---

## 4. `if` as Statement vs. Expression

**Decision:** `if` remains a statement in the AST (`IfStmt`) for now. If
expression support (`let x = if cond { a } else { b }`) will be added as a
separate `IfExpr` AST node in Phase 2, coexisting with `IfStmt`.

### Rationale

Making `if` an expression requires:
- A phi node in codegen to merge the two branch values.
- Type unification across branches (error if branches produce different
  types).
- Handling the case where one branch is missing (no `else`) — should it
  produce `void`? Error? Complicates the design.

By keeping `IfStmt` as-is and adding a new `IfExpr` node, we avoid
refactoring existing codegen for `IfStmt` (which handles control flow but
no value) while cleanly adding expression semantics. The two AST nodes
share parsing (the `if` keyword) but diverge at the statement/expression
boundary.

### Specification

```
// Statement form (unchanged):
if condition { ... }
if condition { ... } else { ... }

// Expression form (new in Phase 2):
let x: T = if condition { expr_a } else { expr_b }
// Both arms must produce the same type T.
// An `else` branch is required — no "implicit void" fallback.
```

### BNF addition

```
stmt      ::= ... | "if" expr block ("else" (block | "if" ...))?
expr      ::= ... | "if" expr block "else" (expr | "if" ...)
```

Note that in expression position, the `else` branch is required and must
produce an expression, not a bare `if` without else. This avoids ambiguity.

### Impact on later phases

- Phase 2: `IfExpr` is explicitly a stretch goal. The design is decided
  now so the parser can be written to disambiguate stmt vs. expr context
  cleanly.

---

## 5. Frozen BNF for Phases 1–2

The following BNF supersedes the existing informal syntax descriptions for
all features planned in Phases 1 and 2. It is the canonical reference;
any discrepancy with the prose documentation should be resolved in favor of
this BNF.

### Lexical structure

```
digit      ::= "0".."9"
alpha      ::= "a".."z" | "A".."Z"
ident      ::= (alpha | "_") (alpha | digit | "_")*
int_lit    ::= digit+ | "-" digit+
float_lit  ::= digit+ "." digit+ | "-" digit+ "." digit+
char_lit   ::= "'" (escaped_char | [^'\\]) "'"
str_lit    ::= '"' (escaped_char | [^"\\])* '"'
escaped_char ::= "\\" ("n" | "t" | "\\" | "'" | '"')
```

### Types

```
type       ::= base_type ("[" int_lit "]")? ("*")*
base_type  ::= "int8" | "int16" | "int32" | "int64" | "int"
             | "uint8" | "uint16" | "uint32" | "uint64"
             | "float16" | "float32" | "float64" | "float"
             | "bool" | "string" | "void" | "char"
             | "cubical"
             | ident                          // struct/enum/type-param name
              | "(" type ("," type)* ")"       // tuple type
```

Notes:
- `int` is a shorthand for `int64`.
- `float` is a shorthand for `float64`.
- `char` is a shorthand for `uint8` with distinct literal syntax.
- Tuple types `(T1, T2, ...)` are anonymous struct types; a 1-tuple `(T)`
  is just `T` (parenthesized type), not a tuple.
- The array size `int_lit` must be positive. Zero-size arrays are not
  allowed.

### Declarations

```
program    ::= decl*
decl       ::= struct_decl | enum_decl | fn_decl | let_decl
             | extern_fn_decl | include_decl | namespace_decl
             | package_decl | import_decl
             | pub decl

struct_decl ::= "struct" ident "{" (struct_field ("," struct_field)* ","?)? "}"
struct_field ::= ident ":" type

enum_decl  ::= "enum" ident "{" variant ("," variant)* ","? "}"
variant    ::= ident "{" (struct_field ("," struct_field)* ","?)? "}"

fn_decl    ::= "fn" ident ("<" ident ("," ident)* ">")? "(" (param ("," param)*)? ")"
               ("->" type)? block
param      ::= ident ":" type

extern_fn_decl ::= "extern" "fn" ident "(" (param ("," param)*)? ("...")? ")"
                   ("->" type)?

let_decl   ::= "let" ident ":" type "=" expr

include_decl ::= "include" str_lit
package_decl ::= "package" ident
import_decl  ::= "import" ident ("." ident)* ("as" ident)?
namespace_decl ::= "namespace" ident "{" program "}"
```

### Statements

```
block      ::= "{" stmt* "}"
stmt       ::= expr_stmt | let_stmt | return_stmt | if_stmt
             | for_stmt | break_stmt | continue_stmt
expr_stmt  ::= expr
let_stmt   ::= "let" ident ":" type "=" expr
return_stmt ::= "return" expr?
if_stmt    ::= "if" expr block ("else" (block | if_stmt))?
for_stmt   ::= "for" "(" stmt? ";" expr? ";" expr? ")" block
break_stmt  ::= "break"
continue_stmt ::= "continue"
```

### Expressions

```
expr       ::= assignment
assignment ::= logical_or ("=" expr | "+=" expr | "-=" expr
                          | "*=" expr | "/=" expr | "%=" expr
                          | "&=" expr | "|=" expr | "^=" expr
                          | "<<=" expr | ">>=" expr)?
logical_or ::= logical_and ("||" logical_and)*
logical_and ::= bitwise_or ("&&" bitwise_or)*
bitwise_or ::= bitwise_xor ("|" bitwise_xor)*
bitwise_xor ::= bitwise_and ("^" bitwise_and)*
bitwise_and ::= comparison ("&" comparison)*
comparison ::= shift (("<" | ">" | "<=" | ">=" | "==" | "!=") shift)?
shift      ::= additive (("<<" | ">>") additive)*
additive   ::= multiplicative (("+" | "-") multiplicative)*
multiplicative ::= unary (("*" | "/" | "%") unary)*
unary      ::= ("-" | "~" | "&" | "*")? postfix
postfix    ::= primary postfix_tail*
postfix_tail ::= "." ident                            // field access (also .0, .1 for tuples)
               | "[" expr "]"                         // subscript
               | "::<" type ("," type)* ">" "(" expr ("," expr)* ")"  // turbofish call
               | "(" expr ("," expr)* ")"             // regular call
               | "." "(" expr? ":" type? ")"?         // placeholder for future use (reserved)
primary    ::= int_lit | float_lit | char_lit | str_lit
             | "true" | "false" | "null"
             | ident
             | "(" expr ")"
             | "[" expr ("," expr)* "]"
             | struct_or_enum_ctor
             | "match" expr "{" match_arm ("," match_arm)* "}"
             | "asm" "(" str_lit ")"
             | "atomic" "(" atomic_op "," expr ("," expr)* ")"

struct_or_enum_ctor ::= ident "{" (named_field ("," named_field)* ","?)? "}"
named_field ::= ident ":" expr

match_arm  ::= pattern "=>" block
pattern    ::= ident ("{" (ident ("," ident)*)? "}")?   // variant pattern
             | "_"                                        // wildcard
             | int_lit | char_lit                          // literal pattern

atomic_op  ::= "xchg" | "add" | "sub" | "and" | "or" | "xor"
             | "cmpxchg" | "fence"
```

---

## Summary of Decisions

| # | Decision | Choice | Impacts |
|---|----------|--------|---------|
| 1 | Unsigned integer types | New distinct types (`uint8/16/32/64`) | Phase 1 (completed), LLVM instruction selection |
| 2 | Dynamic memory | Built-in `alloc<T>` / `free` intrinsics | Phases 3, 6 |
| 3 | Generic trait bounds | Deferred to future major version | Phase 5 scope reduced; operator overloading deferred |
| 4 | `if` expression | New `IfExpr` node (coexists with `IfStmt`) | Phase 2 stretch goal |
| 5 | BNF freeze | BNF in this document is canonical for Phases 1–2 | Parser implementation |