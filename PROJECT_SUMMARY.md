# hokkaido - Project Summary

## Project Structure

```
└── hokkaido/
    ├── .github/
    │   └── workflows/
    │       └── release.yml
    ├── .gitignore
    ├── CMakeLists.txt
    ├── Cargo.lock
    ├── Cargo.toml
    ├── LICENSE
    ├── README.md
    ├── default.nix
    ├── docs/
    │   ├── cubical_surface_language.md
    │   ├── design-decisions.md
    │   ├── example-guess.md
    │   ├── index.astro
    │   ├── lsp.md
    │   ├── package-manager.md
    │   ├── stdlib.md
    │   ├── syntax-control-flow.md
    │   ├── syntax-data-structures.md
    │   ├── syntax-expressions.md
    │   ├── syntax-ffi-cubical.md
    │   ├── syntax-functions.md
    │   ├── syntax-modules.md
    │   ├── syntax-types.md
    │   └── syntax.md
    ├── flake.lock
    ├── flake.nix
    ├── otaru/
    │   ├── Cargo.lock
    │   ├── Cargo.toml
    │   ├── README.md
    │   ├── default.nix
    │   └── src/
    │       ├── add.rs
    │       ├── build.rs
    │       ├── cbuild.rs
    │       ├── install.rs
    │       ├── main.rs
    │       ├── manifest.rs
    │       ├── new.rs
    │       └── run.rs
    ├── otaru.toml
    ├── shell.nix
    ├── src/
    │   ├── cubical/
    │   │   ├── env.rs
    │   │   ├── equality.rs
    │   │   ├── ffi.rs
    │   │   ├── interval.rs
    │   │   ├── json.rs
    │   │   ├── mod.rs
    │   │   ├── nbe.rs
    │   │   ├── parser/
    │   │   │   ├── grammar.rs
    │   │   │   ├── lexer.rs
    │   │   │   ├── mod.rs
    │   │   │   └── tests.rs
    │   │   ├── syntax.rs
    │   │   └── typechecker.rs
    │   └── lib.rs
    ├── src-cpp/
    │   ├── ast.h
    │   ├── borrow_checker.cpp
    │   ├── borrow_checker.h
    │   ├── codegen.cpp
    │   ├── codegen.h
    │   ├── codegen_decl.cpp
    │   ├── codegen_expr.cpp
    │   ├── codegen_init.cpp
    │   ├── codegen_pattern.cpp
    │   ├── codegen_stmt.cpp
    │   ├── codegen_top.cpp
    │   ├── codegen_types.cpp
    │   ├── cubical.cpp
    │   ├── cubical.h
    │   ├── error.h
    │   ├── lexer.cpp
    │   ├── lexer.h
    │   ├── lsp/
    │   │   ├── README.md
    │   │   ├── lsp.cpp
    │   │   └── lsp.h
    │   ├── lsp_main.cpp
    │   ├── main.cpp
    │   ├── parser.cpp
    │   └── parser.h
    ├── std/
    │   ├── functional.hk
    │   ├── hk.mod
    │   └── mem.hk
    ├── test/
    │   ├── cubical_bool.cub
    │   ├── cubical_list.cub
    │   ├── cubical_pair.cub
    │   ├── example.cub
    │   ├── example.hk
    │   ├── example_cffi.hk
    │   ├── guessing_game.hk
    │   ├── pkgtest/
    │   │   ├── hk.mod
    │   │   ├── main.hk
    │   │   ├── main_alias.hk
    │   │   ├── main_multi.hk
    │   │   └── util/
    │   │       ├── extra.hk
    │   │       └── util.hk
    │   ├── test_atomic.hk
    │   ├── test_cubical.hk
    │   ├── test_cubical_bool.hk
    │   ├── test_cubical_comprehensive.hk
    │   ├── test_cubical_file.hk
    │   ├── test_cubical_pair.hk
    │   ├── test_hof.hk
    │   ├── test_mod_break.hk
    │   ├── test_phase1.hk
    │   ├── test_phase1_comprehensive.hk
    │   ├── test_phase2.hk
    │   ├── test_phase3.hk
    │   ├── test_phase4.hk
    │   ├── test_phase5.hk
    │   └── test_phase5b.hk
    └── testnix/
        ├── hk.mod
        ├── otaru.toml
        └── src/
            └── main.hk
```

## Documentation

### README.md

# hokkaido — LLVM-based compiler with cubical compile-time evaluation

## Packages

This repository provides three tools:

- **hokkaido** — the compiler (C++ with LLVM backend, Rust cubical backend)
- **hok-lsp** — the language server ([LSP](https://microsoft.github.io/language-server-protocol/)) for IDE support
- **otaru** — the package manager and project scaffold (Rust)

## Installation

### Nix (flake)

```sh
nix shell github:hokkaido-lang/hokkaido

# Both otaru + hokkaido (default)
nix build github:hokkaido-lang/hokkaido
nix profile install github:hokkaido-lang/hokkaido

# Just the hokkaido compiler
nix build github:hokkaido-lang/hokkaido#hokkaido
nix profile install github:hokkaido-lang/hokkaido#hokkaido

# Otaru explicitly (identical to default)
nix build github:hokkaido-lang/hokkaido#otaru
nix profile install github:hokkaido-lang/hokkaido#otaru
```

The Nix-built `otaru` bundles the `hokkaido` compiler automatically — everything works out of the box.

The installation includes:
- `hokkaido` — the compiler
- `hok-lsp` — the language server
- `otaru` — the package manager

### From source

**Requirements:** clang, cmake, cargo, LLVM (19+)

```sh
git clone https://github.com/hokkaido-lang/hokkaido.git
cd hokkaido

# Build the compiler
mkdir build && cd build
cmake .. && make

# Build the package manager
cd ../otaru
cargo build --release
```

After building, add `build/` to your `PATH` or set `HOKKAIDO_HOME`:

```sh
export HOKKAIDO_HOME=/path/to/hokkaido/build
```

## Quick start

```sh
otaru new myapp
cd myapp
otaru run     # builds and runs src/main.hk
```

## Docs

- [Language syntax](docs/syntax.md)
- [Function types and HOFs](docs/syntax-functions.md)
- [LSP server](src-cpp/lsp/README.md)

## License

[Apache License 2.0](LICENSE)

### docs/cubical_surface_language.md

---
layout: ../../layouts/DocsLayout.astro
title: Cubical Surface Language
---

# Cubical Surface Language — Syntax Reference

## Top-level Declarations

A program is a sequence of declarations. Three forms are allowed.

### Value definition

```
def <name> : <type> = <term>
```

Example:

```
def id : (A : U0) -> A -> A = \A x. x
```

### Datatype declaration

```
data <Name> =
  | <con1> : <type>
  | <con2> : <type>
  ...
```

Constructors must return the declared datatype. A datatype must have at least one constructor.

Example:

```
data Nat = | zero : Nat | suc : Nat -> Nat
```

#### Path constructors

A constructor whose return type is followed by `[ <face0> , <face1> ]` is a **path constructor** — it specifies a path whose endpoints are `face0` and `face1`.

```
data S1 =
  | base : S1
  | loop : S1 [ base , base ]
```

---

## Comments

Line comments begin with `--` and extend to the end of the line.

```
-- This is a comment
```

---

## Terms

### Universes

| Syntax | Meaning |
|--------|---------|
| `U0`, `U1`, `U2`, … | Universe at level *n* |
| `Type` | Alias for `U0` |

### Variables

Plain identifiers resolve first as local variables (de Bruijn), then as top-level globals, then as constructors.

Identifier characters: start with a letter or `_`; continue with letters, digits, `_`, `'`, `?`, `!`, `-`.

### Lambda abstraction

```
\x. <body>
\x y z. <body>        -- multi-binder shorthand
```

Alternative keyword form:

```
fun x y z => <body>
```

The `λ` Unicode character is also accepted in place of `\`.

### Let expressions

```
let <x> = <value> in <body>
let <x> : <type> = <value> in <body>
```

Type annotations are parsed but currently discarded (same as parenthesised ascriptions).

Example:

```
let n = suc zero in n
```

This desugars to:

```
(\n. n) (suc zero)
```

Nested `let` binds at the same precedence as `\` and `fun`.

### Function application

```
<f> <arg1> <arg2> ...
```

Application is left-associative and is written by juxtaposition.

### Dependent function type (Π)

Non-dependent arrow:

```
<A> -> <B>
```

Dependent Pi (binder in scope in `B`):

```
(x : A) -> B
```

Explicit Pi former:

```
Pi (x : A). B
Π (x : A). B
```

### Pair / Sigma type

Pair term:

```
(<a> , <b>)
```

Non-dependent product:

```
<A> * <B>
```

Dependent Sigma (binder in scope in `B`):

```
(x : A) * B
```

Explicit Sigma former:

```
Sigma (x : A). B
Σ (x : A). B
```

### Projections

```
fst <pair>
snd <pair>
```

### Type ascription

```
(<term> : <type>)
```

---

## Interval Expressions

The interval type is written `I` or `𝕀`.

| Syntax | Meaning |
|--------|---------|
| `i0` or `0` | Left endpoint |
| `i1` or `1` | Right endpoint |
| `i /\ j` or `i ∧ j` | Meet (min) |
| `i \/ j` or `i ∨ j` | Join (max) |
| `~ i` or `¬ i` | Negation (flip) |

Operator precedence (highest to lowest): `~` > `/\` > `\/`.

---

## Path Types and Path Abstraction

### Path type

```
Path <A> <u> <v>
```

A path in type `A` from `u` to `v`.

### Path abstraction (interval lambda)

```
<i> <body>
```

Binds an interval variable `i` in `body`. The `⟨` and `⟩` Unicode angle brackets are also accepted.

### Path application

```
<p> @ <i>
```

Applies path `p` to interval expression `i`.

---

## Elimination

```
elim <motive> { | <con1> <binders> => <body1> | <con2> <binders> => <body2> ... } <scrutinee>
```

`->` may be used in place of `=>` in case branches.

The motive may optionally be wrapped in brackets:

```
elim[<motive>] { ... } <scrutinee>
```

Example:

```
elim motive { | zero => base_case | suc n => step } value
```

---

## Pattern matching

`match` is sugar for an eliminator with a motive derived from an explicit return type:

```
match <scrutinee> return <return_type> with
  | <con1> <binders> => <body1>
  | <con2> <binders> => <body2>
  ...
```

`->` may be used in place of `=>` in case branches. Cases may be written with or without braces:

```
match n return Nat with | zero => z | suc m => s
match n return Nat with { | zero => z | suc m => s }
```

When the scrutinee is a bare identifier, that name is in scope in the return type (for dependent elimination). Otherwise the scrutinee is bound as `_match` in the return type.

Example (non-dependent):

```
match n return Nat with
  | zero => zero
  | suc m => suc m
```

This desugars to:

```
elim (\n. Nat) { | zero => zero | suc m => suc m } n
```

Path-constructor cases follow the same rules as `elim`: list ordinary argument binders first, then the interval variable last.

---

## Cubical Primitives

### Transport

```
transport <path> <element>
```

Transports `element` along the path `path`.

### Homogeneous composition

```
hcomp <type> <phi> <system> <base>
```

### Univalence and equivalences

| Syntax | Arguments | Meaning |
|--------|-----------|---------|
| `Equiv A B` | `A B` | Type of equivalences from `A` to `B` |
| `mkEquiv A B f g eta eps` | `A B f g eta eps` | Construct an equivalence |
| `equivFwd e x` | `e x` | Apply the forward map of equivalence `e` to `x` |
| `ua e` | `e` | Univalence: path from equivalence |

### Glue types

| Syntax | Arguments | Meaning |
|--------|-----------|---------|
| `Glue A phi te` | `A phi te` | Glue type |
| `glueElem phi t a` | `phi t a` | Construct a glue element (also `glue`) |
| `unglue phi te g` | `phi te g` | Unglue an element |

---

## Operator Precedence Summary

From lowest to highest binding:

| Level | Construct |
|-------|-----------|
| 1 (lowest) | `\x.`, `fun x =>`, `let x = t in u`, `<i>`, `Pi`, `Sigma`, `,` (pair) |
| 2 | `->`, `*` (non-dependent arrow/product, right-assoc) |
| 3 | `\/` (interval join) |
| 4 | `/\` (interval meet) |
| 5 | `~` (interval negation, prefix) |
| 6 | `@` (path application, left-assoc) |
| 7 | juxtaposition (function application, left-assoc) |
| 8 | `fst`, `snd`, `ua`, `transport`, `equivFwd` (prefix) |
| 9 (highest) | atoms: identifiers, integer literals, parenthesised terms |

---

## Unicode Aliases

The following Unicode symbols are accepted as alternatives to their ASCII counterparts.

| Unicode | ASCII equivalent |
|---------|-----------------|
| `λ` | `\` (lambda) |
| `Π` | `Pi` |
| `Σ` | `Sigma` |
| `𝕀` | `I` (interval type) |
| `⟨` / `⟩` | `<` / `>` (path binder) |
| `×` | `*` (product) |
| `∧` | `/\` (meet) |
| `∨` | `\/` (join) |
| `¬` | `~` (negation) |

---

## Grammar Summary (BNF-style)

```
program  ::= decl*
decl     ::= 'def' ident ':' term '=' term
           | 'data' ident '=' ('|' con_decl)+

con_decl ::= ident ':' term ('[' term ',' term ']')?

term     ::= 'let' ident (':' term)? '=' term 'in' term
           | '\' ident+ '.' term
           | 'fun' ident+ '=>' term
           | '<' ident '>' term
           | 'Pi' '(' ident ':' term ')' '.' term
           | 'Sigma' '(' ident ':' term ')' '.' term
           | arrow_star ',' term
           | arrow_star

arrow_star ::= join ('->' arrow_star | '*' arrow_star)?

join     ::= meet ('\/' meet)*
meet     ::= tilde ('/\' tilde)*
tilde    ::= '~' tilde | papp
papp     ::= app ('@' tilde)*
app      ::= prefix_or_atom prefix_or_atom*

prefix_or_atom ::= 'fst' prefix_or_atom
                 | 'snd' prefix_or_atom
                 | 'ua' prefix_or_atom
                 | 'transport' prefix_or_atom prefix_or_atom
                 | 'equivFwd' prefix_or_atom prefix_or_atom
                 | 'Path' prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'hcomp' prefix_or_atom prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'Equiv' prefix_or_atom prefix_or_atom
                 | 'mkEquiv' prefix_or_atom x6
                 | 'Glue' prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'glueElem' prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'unglue' prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'elim' term '{' cases '}' term
                 | atom

atom     ::= ident | '0' | '1' | '(' term ')'
```

### docs/design-decisions.md

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

**Decision:** Dynamic memory is not a language feature. Users declare `extern fn malloc` and
`extern fn free` from the C standard library and work with raw pointers (`int8*`, `T*`).
Early prototypes used `alloc<T>(n) / free(ptr)` as contextual keyword intrinsics, but
these were removed in favor of the simpler extern-fn approach — a future stdlib can provide
convenience wrappers without adding syntax.

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
extern fn malloc(size: int64) -> int8*
extern fn free(ptr: int8*) -> void
```

- `malloc` returns a raw `int8*` pointer; users cast to `T*` by reassigning to a `T*` variable.
- The number of bytes to allocate is `n * sizeof(T)`, computed manually by the caller.
- `free` accepts an `int8*`; any pointer type can be passed (LLVM opaque pointers).
- There is no built-in `alloc<T>(n)` syntax or `AllocExpr`/`FreeExpr` AST node.
- Out-of-memory returns null; the caller must check. There is no exception mechanism.

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
version. Phase 5a implements generic structs without bounds; Phase 5b
implements traits, impls, method calls, and trait bounds on generics.
Generic enums remain deferred.

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

- Generic structs: `struct Pair<A, B> { first: A, second: B }` — works (Phase 5a).
- Traits: `trait Area { fn area(self: Self) -> int64 }` — works (Phase 5b).
- Impl blocks: `impl Area for Rect { ... }` and `impl Point { ... }` — works (Phase 5b).
- Method calls: `obj.method(args)` — works (Phase 5b).
- Trait bounds: `fn double<T: Area>(x: T) -> int64` — works (Phase 5b).
- Generic enums: `enum Option<T> { Some { value: T }, None {} }` — deferred.
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

- Phase 2: `IfExpr` is implemented. The design was decided
  now so the parser can be written to disambiguate stmt vs. expr context
  cleanly.

---

## 5. Frozen BNF for Phases 1–3

The following BNF supersedes the existing informal syntax descriptions for
all features planned in Phases 1, 2, and 3. It is the canonical reference;
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
type       ::= base_type ("[" int_lit "]")? ("*")* ("[" "]")?   // T[n] array, T[] slice, T* pointer
base_type  ::= "int8" | "int16" | "int32" | "int64" | "int"
             | "uint8" | "uint16" | "uint32" | "uint64"
             | "float16" | "float32" | "float64" | "float"
             | "bool" | "string" | "void" | "char"
             | "cubical"
             | ident                          // struct/enum/type-param name
             | "(" type ("," type)* ")"       // tuple type
             | "fn" "(" (type ("," type)*)? ")" ("->" type)?  // function type
```

Notes:
- `int` is a shorthand for `int64`.
- `float` is a shorthand for `float64`.
- `char` is a shorthand for `uint8` with distinct literal syntax.
- Tuple types `(T1, T2, ...)` are anonymous struct types; a 1-tuple `(T)`
  is just `T` (parenthesized type), not a tuple.
- The array size `int_lit` must be positive. Zero-size arrays are not
  allowed.
- A slice type `T[]` is written with empty brackets and represents a fat
  pointer `{ ptr: T*, len: int64 }`. Slices are a distinct type kind from
  arrays and pointers.
- `int[5][]` is a slice of arrays (each element is `int[5]`). Pointer depth
  and slice are orthogonal: `int[]*` is a pointer to a slice.

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
             | "region" ident block
expr_stmt  ::= expr
let_stmt   ::= "let" ident ":" type "=" expr
return_stmt ::= "return" expr?
if_stmt    ::= "if" expr block ("else" (block | if_stmt))?
for_stmt   ::= ("'" ident)? "for" "(" stmt? ";" expr? ";" expr? ")" block
break_stmt  ::= "break" ("'" ident)?        // optional label
continue_stmt ::= "continue" ("'" ident)?   // optional label
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
             | "if" expr block "else" (expr | "if" ...)  // if-expression
             | "match" expr "{" match_arm ("," match_arm)* "}"
             | "asm" "(" str_lit ")"
             | "atomic" "(" atomic_op "," expr ("," expr)* ")"

struct_or_enum_ctor ::= ident ("::" ident)? "{" field_list "}"
field_list ::= (named_field ("," named_field)* ","?)?    // named: `x: expr`
             | (positional_field ("," positional_field)* ","?)?  // positional: expr
named_field ::= ident ":" expr
positional_field ::= expr
// Mixing named and positional fields in a single constructor is an error.

match_arm  ::= pattern "=>" expr                          // arm body is an expression
pattern    ::= ident ("::" ident)* ("{" (pattern_field ("," pattern_field)*)? "}")?
             | "_"                                        // wildcard
             | int_lit | char_lit                          // literal pattern
pattern_field ::= ident (":" ident)?                     // shorthand `x` or explicit `x: pat`

atomic_op  ::= "xchg" | "add" | "sub" | "and" | "or" | "xor"
             | "cmpxchg" | "fence"
```

---

## 6. Borrow Checker

**Decision:** Implement a lexical borrow checker with per-variable borrow tracking,
enforcing standard Rust-like aliasing rules:
- One mutable reference XOR any number of shared references.
- The original value is frozen (no read/write) while any borrow is active.
- References must not outlive their referent (enforced by scope depth).

The checker is a standalone AST-walking pass, run before code generation. It
operates on `BorrowExpr` and `DerefExpr` AST nodes and `&T`/`&mut T` reference
types.

### Rationale

| Option | Pros | Cons |
|--------|------|------|
| Lexical borrow checker | Simple implementation; fast (single pass); predictable error messages; matches Rust's borrow scoping for v1 | Conservative — borrows last until end of scope, not until last use (NLL); no support for reborrowing patterns |
| Non-lexical lifetimes (NLL) | More precise; shorter borrow ranges; fewer false positives | Significantly more complex (requires dataflow analysis, liveness tracking); overkill for v1 |
| No borrow checker — runtime refcounting | Simple compiler; familiar to GC-language users | Runtime overhead; refcount cycles; not zero-cost; breaks the "pay only for what you use" philosophy |

A lexical checker provides immediate safety guarantees with minimal compiler
complexity. NLL can be added as a future enhancement without changing the
surface language.

### Specification

```
let x: int = 42

// Shared borrow — allowed
let r1: &int = &x         // OK
let r2: &int = &x         // OK (many shared borrows)

// Mutable borrow — exclusive
let rw: &mut int = &mut x  // OK, first mutable borrow

// Violations:
let r3: &int = &x          // ERROR: x is mutably borrowed
let rw2: &mut int = &mut x // ERROR: x is mutably borrowed

// Owner frozen while borrowed:
x = 10                     // ERROR: x is borrowed
let v: int = x             // ERROR: x is borrowed

// Borrow ends at scope exit:
{
    let r: &mut int = &mut x
    *r = 10
}
// Borrow released — x is usable again:
let y: int = x             // OK
```

- Borrows are tracked per variable name (not per memory location).
- The root variable of a borrow chain is resolved by walking through field
  accesses, dereferences, and subscripts.
- Closure bodies capture by value — borrow state is saved and restored around
  closure calls (closures cannot borrow from their enclosing scope).
- `region` blocks interact with the borrow checker only through `enter_scope`/
  `exit_scope` — region pointer tracking is a separate pass.

### Implementation

- `BorrowChecker` class with `active_borrows` map (`var_name -> Vec<BorrowEntry>`)
  and `current_depth` counter.
- `check_shared_borrow()` / `check_mut_borrow()`: validate against active borrows.
- `register_shared_borrow()` / `register_mut_borrow()`: record with current depth.
- `release_borrows_at_depth()`: called on scope exit.
- `check_var_read()` / `check_var_write()`: validate against active borrows.
- Expression walker dispatches on all expression types (not just borrows) to
  check variable accesses against active borrows.
- Defined in `src-cpp/borrow_checker.h` and `src-cpp/borrow_checker.cpp`.
- Integrated into the compilation pipeline in `main.cpp` — run on all
  non-extern, non-generic function declarations before codegen.

### Impact on later phases

- Generics (Phase 5): the borrow checker currently skips generic function
  declarations. Once monomorphization produces concrete instances, those
  instances should be checked.
- Closures (Phase 4): closures capture by value; borrow state save/restore is
  already implemented. Lexical closures that capture references (if added later)
  will need deeper borrow tracking through closure calls.
- NLL (future): a flow-sensitive borrow checker can replace the lexical checker
  with no surface-language changes.

---

## Summary of Decisions

| # | Decision | Choice | Impacts |
|---|----------|--------|---------|
| 1 | Unsigned integer types | New distinct types (`uint8/16/32/64`) | Phase 1 (completed), LLVM instruction selection |
| 2 | Dynamic memory | Extern fn (`malloc`/`free`), not built-in syntax | Phases 3, 6 |
| 3 | Generic trait bounds | Deferred to future major version | Phase 5b scope reduced; operator overloading deferred |
| 4 | `if` expression | New `IfExpr` node (coexists with `IfStmt`) | Phase 2 (completed) |
| 5 | BNF freeze | BNF in this document is canonical for Phases 1–3 | Parser implementation |
| 6 | String type | Opaque pointer (`int8*`) | All phases |
| 7 | Dynamic memory approach | Extern fn (`malloc`/`free`), not built-in syntax | Phase 3 (completed) |
| 8 | Function types and HOFs | `fn(T1, T2) -> Ret` as opaque `i8*` pointer to closure struct; `&fn_name` for named-function values | `std/functional.hk` combined with Phase 4 (completed) |
| 9 | Memory safety — regions + lifetime tracking | Stack-based bump-allocator region blocks (`region R { ... }`) with compile-time rejection of escaping region pointers (tracking through direct `let` assignment). `linear` keyword removed (it tracked variable names, not values — gave false confidence) | Regions: safe scoped memory with zero-cost compile-time escape detection. `std/mem.hk` provides safe memory operations (copy, set, zero, eq, swap). Heap allocation (`malloc`/`free` via FFI) remains inherently unsafe |
| 10 | Borrow checker | Lexical borrow checker with per-variable borrow tracking, enforcing: one mutable XOR many shared; owner frozen while borrowed; references must not outlive their referent. Implemented as a standalone AST-walking pass run before code generation | Compile-time data race and use-after-free prevention for reference types (`&T`/`&mut T`). Independent of region lifetime tracking — regions handle scoped allocation safety, borrow checker handles aliasing discipline. Implemented in Phase 0. |

### docs/example-guess.md

---
layout: ../../layouts/DocsLayout.astro
title: "Example: Guessing Game"
---

# Example: Number Guessing Game

This example demonstrates C FFI, loops, conditionals, pointers, and user I/O by
implementing a classic number guessing game.

## The code

```
extern fn puts(s: string) -> int
extern fn printf(fmt: string, ...) -> int
extern fn scanf(fmt: string, ...) -> int
extern fn rand() -> int
extern fn srand(seed: int) -> void
extern fn time(t: int*) -> int

fn main() -> int {
    let t: int = 0
    srand(time(&t))

    let secret: int = rand() % 100 + 1

    let guess: int = 0
    let attempts: int = 0

    puts("Guess the number (1-100)!")

    for ;; {
        printf("Enter guess: ")
        scanf("%ld", &guess)
        attempts += 1

        if guess < secret {
            puts("Too low!")
        } else if guess > secret {
            puts("Too high!")
        } else {
            printf("Correct! You got it in %ld tries!\n", attempts)
            break
        }
    }

    return 0
}
```

## Building and running

### With otaru (recommended)

```sh
otaru new guess_game
cd guess_game
# paste the code above into src/main.hk
otaru run
```

### Manual compilation

```sh
hokkaido guess.hk          # produces guess.o
clang guess.o -o guess      # link with C runtime
./guess
```

The `hokkaido` compiler produces an object file (`.o`). You must link it with a C
compiler (`clang` or `gcc`) to resolve the C library calls (`printf`, `scanf`, etc.).

### Example session

```
$ ./guess
Guess the number (1-100)!
Enter guess: 50
Too low!
Enter guess: 75
Too high!
Enter guess: 62
Correct! You got it in 3 tries!
```

## What it demonstrates

| Feature | How it's used |
|---------|---------------|
| **C FFI (`extern fn`)** | Calls C standard library: `puts`, `printf`, `scanf`, `rand`, `srand`, `time` |
| **Variadic FFI** | `printf(fmt: string, ...)` and `scanf(fmt: string, ...)` accept a variable number of arguments |
| **Pointers** | `&t` passes the address of `t` to `time()`; `&guess` passes the address of `guess` to `scanf()` |
| **For loop** | Infinite loop `for ;;` with `break` to exit on correct guess |
| **Break** | Exits the loop when the correct number is guessed |
| **If / else if / else** | Three-way branch for too-low / too-high / correct |
| **Comparison** | `<`, `>` on integer values to compare guesses |
| **Compound assignment** | `attempts += 1` increments the attempt counter |
| **Modulo operator** | `rand() % 100 + 1` maps a random value to the range `1..100` |
| **String literals** | Passed directly to C functions as `string` (which maps to `char*`) |
| **Process exit code** | `return 0` signals success to the operating system |

## How it works

1. **Seed the RNG** — `time(&t)` gets the current epoch time. `srand(t)` seeds the
   pseudo-random number generator so each run produces different numbers.

2. **Generate a secret** — `rand() % 100 + 1` produces a number in `1..100`.

3. **Game loop** — An infinite `for ;;` loop reads guesses via `scanf`, compares
   them to the secret, and provides hints until the correct number is guessed.

4. **Exit** — `break` exits the loop, then `return 0` exits the program successfully.

## Notes

- `%ld` in `scanf`/`printf` matches Hokkaido's 64-bit `int` type on x86-64 Linux.
  On other platforms the format specifier may differ.
- The game uses an infinite `for` loop with an explicit `break` when the correct
  number is guessed — no condition in the loop header.
- The `string` type maps to C's `char*` when passed to extern functions, so string
  literals work directly as arguments to `puts` and `printf`.

## Try it yourself

- **Limit attempts**: Add a maximum number of attempts and exit the loop when
  exceeded.
- **Input validation**: Check that the guess is in `1..100` before comparing.
- **Reveal the answer**: Print `secret` after the player gives up or runs out
  of attempts.
- **Play again**: Wrap the game loop in an outer loop that asks "Play again?"
  after each round.

### docs/lsp.md

---
layout: ../../layouts/DocsLayout.astro
title: Language Server (LSP)
---

# Language Server (hok-lsp)

`hok-lsp` is the language server for Hokkaido, providing IDE features for any editor
that supports the [Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
(LSP): VS Code, Neovim, Helix, Emacs, Sublime Text, and more.

## Features

| Feature | Description |
|---------|-------------|
| **Diagnostics** | Parse errors reported inline as you type |
| **Hover** | Shows symbol kind and name on hover |
| **Completion** | Keyword suggestions + symbols from all open documents |
| **Go to Definition** | Navigate to symbol declarations |
| **Find References** | Find all occurrences of a symbol in the current file |
| **Document Symbols** | Outline of functions, variables, structs, enums, traits |
| **Incremental Parsing** | After the first full parse, edits re-parse only the affected portion |

## Building

`hok-lsp` is built as part of the Hokkaido CMake project:

```sh
mkdir build && cd build
cmake .. && make hok-lsp
```

Or with Nix:

```sh
nix build github:hokkaido-lang/hokkaido
```

## Editor setup

### Neovim (vim.lsp)

Add to your init.lua:

```lua
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'hk',
  callback = function()
    vim.lsp.start({
      name = 'hok-lsp',
      cmd = { 'hok-lsp' },
    })
  end,
})
```

### Helix

Add to `~/.config/helix/languages.toml`:

```toml
[[language]]
name = "hokkaido"
scope = "source.hk"
file-types = ["hk"]
language-servers = ["hok-lsp"]

[language-server.hok-lsp]
command = "hok-lsp"
```

### VS Code

Use a generic LSP client extension and configure:

```json
{
  "languages": [{
    "id": "hokkaido",
    "extensions": ["hk"]
  }],
  "languageServer": {
    "command": "hok-lsp"
  }
}
```

## Protocol

`hok-lsp` communicates over **stdio** using the standard LSP JSON-RPC transport
(`Content-Length` headers). It supports:

- `textDocument/didOpen` / `didChange` / `didClose`
- `textDocument/hover`
- `textDocument/completion`
- `textDocument/definition`
- `textDocument/references`
- `textDocument/documentSymbol`

### docs/package-manager.md

---
layout: ../../layouts/DocsLayout.astro
title: Package Manager (otaru)
---

# Package Manager (otaru)

`otaru` is the package manager and project scaffold for Hokkaido. It handles project
creation, building, dependency management, and script execution.

## Installation

### Nix (recommended)

```sh
nix profile install github:hokkaido-lang/hokkaido
```

### Cargo

```sh
cargo install --path otaru
```

When installed via Cargo, the hokkaido compiler must be on `PATH` or `HOKKAIDO_HOME`.

## Quick start

```sh
otaru new myapp
cd myapp
otaru run     # builds and runs src/main.hk
```

## Commands

### Project scaffolding

```
otaru new <name>
```

Creates a new Hokkaido project with the standard directory structure:

```
myapp/
  otaru.toml        # project manifest
  src/
    main.hk         # entry point (package main)
  hk.mod            # module root marker
```

### Building

| Command | Description |
|---------|-------------|
| `otaru build` | Build the project |
| `otaru build --release` / `-r` | Build with `-O2` optimizations |
| `otaru build -f` | Force rebuild, ignoring cache |
| `otaru build <file.hk>` | Compile a single file |
| `otaru build --freestanding` | Build without CRT/libc |

### Running

| Command | Description |
|---------|-------------|
| `otaru run` | Build and run |
| `otaru run --release` | Build with optimizations and run |
| `otaru run <file.hk>` | Compile and run a single file |

### Dependencies

| Command | Description |
|---------|-------------|
| `otaru add <name> --git <url>` | Add a git dependency |
| `otaru add <name> --path <path>` | Add a local dependency |
| `otaru install` | Clone/fetch all dependencies |

### Other

| Command | Description |
|---------|-------------|
| `otaru clean` | Remove the `build/` directory |
| `otaru exec` | List all scripts defined in `[scripts]` |
| `otaru exec <name>` | Run a named script |
| `otaru exec <name> <args>` | Run a script with arguments (`$1`, `$2`, etc.) |

## Project manifest (`otaru.toml`)

The `otaru.toml` file defines project metadata and build configuration:

```toml
[project]
name = "myapp"
version = "0.1.0"
type = "hokkaido"    # "hokkaido" (default), "c", or "cpp"

[src]
dir = "src"
entry = "main.hk"    # entry point (default: "main.hk" or "main.cpp")

[build]
flags = []            # extra compiler flags
link = []             # extra linker flags / libraries (e.g., ["-lm", "-lpthread"])
freestanding = false  # set true for freestanding builds

[scripts]
test = "otaru build && ./build/myapp"
bench = "otaru build --release && ./build/myapp --bench"
```

## Project auto-detection

otaru auto-detects the project type based on `otaru.toml` and source files:

| Condition | Build path |
|-----------|-----------|
| `.hk` files in `src/` | Hokkaido compiler |
| `[build]` section, no `.hk` files | C/C++ compiler |
| `[build]` section with `.hk` files | Hokkaido + C compiler (mixed) |

## Mixed Hokkaido + C projects

For projects that combine Hokkaido and C code, otaru compiles `.hk` files with the
Hokkaido compiler and `.c`/`.cpp` files with the system C compiler, then links them
together.

```toml
[project]
name = "mixed"
type = "hokkaido"

[src]
dir = "src"
entry = "main.hk"

[build]
link = ["-lm"]
```

Place `.hk` and `.c` files together in `src/` — otaru routes each file to the
correct compiler.

## C/C++ projects

otaru works as a Make replacement for pure C/C++ projects:

```toml
[project]
name = "mylib"
type = "cpp"

[src]
dir = "src"
entry = "main.cpp"

[build]
flags = ["-std=c++17", "-Wall"]
link = ["-lpthread"]
```

Multi-target builds are supported — specify `--target <name>` to build a specific target.

### docs/stdlib.md

---
layout: ../../layouts/DocsLayout.astro
title: Standard Library
---

# Standard Library

Hokkaido's standard library (`std`) provides two modules: `std/functional.hk` for
higher-order functions and `std/mem.hk` for safe memory operations.

Import the standard library with `import "std"`:

```
import "std"
```

All standard library functions are accessed via the `std::` prefix.

## Functional (`std/functional.hk`)

Higher-order functions for working with `int` values and function-typed parameters.

```
import "std"

fn add_one(x: int) -> int { return x + 1 }
fn add(x: int, y: int) -> int { return x + y }
fn is_even(x: int) -> bool { return x % 2 == 0 }

fn main() -> int {
    let arr: int[5] = [1, 2, 3, 4, 5]

    let r1: int = std::twice(&add_one, 5)              // 7
    let r2: int = std::thrice(&add_one, 5)             // 8
    let r3: int = std::compose(&add_one, &add_one, 5)  // 7
    let r4: int = std::apply_n(&add_one, 10, 5)        // 15
    let r5: int = std::fold_int(&add, 0, arr, 5)       // 15
    let r6: bool = std::any_int(&is_even, arr, 5)      // true
    let r7: bool = std::all_int(&is_even, arr, 5)      // false
    return 0
}
```

### Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `twice` | `(fn(int)->int, int) -> int` | Apply `f` twice: `f(f(x))` |
| `thrice` | `(fn(int)->int, int) -> int` | Apply `f` three times: `f(f(f(x)))` |
| `compose` | `(fn(int)->int, fn(int)->int, int) -> int` | Compose two functions: `f(g(x))` |
| `apply_n` | `(fn(int)->int, int, int) -> int` | Apply `f` `n` times to `x` |
| `fold_int` | `(fn(int,int)->int, int, int[], int) -> int` | Left fold over a slice of `int` |
| `any_int` | `(fn(int)->bool, int[], int) -> bool` | `true` if any element matches predicate |
| `all_int` | `(fn(int)->bool, int[], int) -> bool` | `true` if all elements match predicate |
| `map_int_into` | `(fn(int)->int, int[], int[], int) -> int` | Map `f` over `src`, write results into `dst` |

All functions operate on `int` (`int64`) values. The `len` parameter for array functions
is the number of elements, not bytes.

## Memory (`std/mem.hk`)

Safe wrappers around libc memory operations. These functions operate on raw byte
pointers and do not manage allocation or deallocation.

```
import "std"

fn main() -> int {
    region R {
        let buf: int8* = __region_alloc(32)

        std::mem_set(buf, 42, 16)     // fill first 16 bytes with 42
        std::mem_zero(buf + 16, 16)   // zero the second half

        let check: int8* = __region_alloc(32)
        std::mem_copy(check, buf, 32) // copy buf into check

        if std::mem_eq(buf, check, 32) {
            std::mem_swap(buf, check, 8)   // swap first 8 bytes
            return 1
        }
    }
    return 0
}
```

### Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `mem_copy` | `(int8*, int8*, int64) -> void` | Copy `n` bytes from `src` to `dst` (non-overlapping) |
| `mem_set` | `(int8*, int8, int64) -> void` | Fill `n` bytes of `ptr` with `val` |
| `mem_zero` | `(int8*, int64) -> void` | Zero `n` bytes at `ptr` |
| `mem_eq` | `(int8*, int8*, int64) -> bool` | Compare `n` bytes for equality |
| `mem_swap` | `(int8*, int8*, int64) -> void` | Exchange `n` bytes between two buffers |

### Safety notes

- `mem_copy` assumes non-overlapping regions. Use `mem_swap` for overlapping copies.
- `mem_eq` returns `true` only if all `n` bytes are identical.
- All functions operate on raw byte pointers (`int8*`). The caller must ensure
  the pointers are valid and point to at least `n` bytes of allocated memory.
- For scoped memory, use [region blocks](/docs/syntax-control-flow#region)
  with `__region_alloc`. For heap memory, use `extern fn malloc`/`extern fn free`.

### docs/syntax-control-flow.md

---
layout: ../../layouts/DocsLayout.astro
title: Control Flow
---

# Control Flow

## If / Else

Conditional execution with `if`, optional `else if`, and optional `else`:

```
if condition {
    // runs when condition is truthy (non-zero)
} else if other_condition {
    // runs when condition is falsy AND other_condition is truthy
} else {
    // runs when all preceding conditions are falsy
}
```

### Condition semantics

The condition is coerced to `bool`: any non-zero value is truthy, zero is falsy.
A condition can be any expression of integer, boolean, or pointer type. The compiler
generates an `icmp ne` comparison when the expression type is not already `i1` (bool).

```
let x: int = 42
if x {
    // runs because 42 ≠ 0
}

let p: int* = &x
if p {
    // runs because pointer is non-null
}
```

### Nested and chained

```
if a {
    if b {
        // both a and b are truthy
    }
} else if c {
    // a is falsy and c is truthy
} else {
    // all false
}
```

The `if` construct is also an **expression** that produces a value when an `else` branch
is present (see [If-expressions](/docs/syntax-expressions#if-expressions)).

## For loop

A `for` loop has three parts: initializer, condition, and update expression, separated
by semicolons inside parentheses:

```
for init; condition; update {
    body
}
```

All three parts are optional. The loop executes:

1. **init** — once before the first iteration. Typically a `let` declaration or assignment.
2. **condition** — evaluated before each iteration (including the first). If falsy, the loop exits.
3. **body** — executed when the condition is truthy.
4. **update** — evaluated after each body execution, then back to step 2.

```
for let i: int = 0; i < 10; i = i + 1 {
    // runs 10 times, i = 0 .. 9
}
```

### While-like loop

Omit the init and update for a while loop:

```
let done: bool = false
for ; done; {
    // runs while done is false
    // ...
    done = true
}
```

### Infinite loop

Omit all three parts:

```
for ;; {
    // runs forever — exit with break or return
}
```

### Break

The `break` statement exits the innermost enclosing loop immediately, transferring
control to the first statement after the loop.

```
for ;; {
    if done {
        break
    }
    // ...
}
// execution continues here after break
```

### Continue

The `continue` statement skips the rest of the current loop iteration and jumps
to the loop's update expression (if any), followed by the condition check.

```
for let i: int = 0; i < 10; i = i + 1 {
    if i % 2 == 0 {
        continue      // skip even numbers
    }
    // process odd i only
}
```

### Labeled break / continue

A loop can be given a label with `'name:` before the `for` keyword. `break 'name` or
`continue 'name` then refer to that specific loop rather than the innermost one. This
is useful for breaking out of nested loops.

```
'outer: for let i: int = 0; i < 10; i = i + 1 {
    for let j: int = 0; j < 10; j = j + 1 {
        if i + j >= 15 {
            break 'outer    // exits outer for loop
        }
    }
}
```

Labels use a tick prefix (`'my_label`) followed by a colon (`:`) before the `for`
keyword. Break and continue labels are written after the keyword (`break 'my_label`,
`continue 'my_label`).

An unlabeled `break`/`continue` always refers to the innermost enclosing loop, even
when labeled loops are present in the nesting.

### Errors

Using `break` or `continue` outside a loop is a compile-time error. Using a label that
does not match any enclosing loop is also a compile-time error.

### Update expression

The update can be any expression whose side effects are useful. Compound assignment
works well:

```
for let i: int = 0; i < 10; i += 1 {
    // ...
}
```

## Match

The `match` expression performs pattern matching on an enum value, dispatching to the
arm that corresponds to the variant's tag.

```
match value {
    Variant1 { field1, field2 } => {
        // body using field1, field2
    }
    Variant2 { field } => {
        // body using field
    }
    // ...
}
```

Each arm consists of a variant name, an optional field-binding list in curly braces,
`=>`, and a body block.

### Destructuring fields

The fields listed inside the curly braces are bound to local variables of the same name
and type as the enum variant's fields. These local variables are visible only inside the
arm's body.

```
enum Shape {
    Circle { radius: float64 },
    Rect { w: float64, h: float64 },
}

fn area(s: Shape) -> float64 {
    match s {
        Circle { radius } => {
            return 3.14159 * radius * radius
        }
        Rect { w, h } => {
            return w * h
        }
    }
}
```

### Completeness

The compiler emits a **warning** when not all enum variants are covered by match arms.
If no arm matches (which can happen with unmatched variants), a default null value is
returned. Future versions may make this a hard error or require an explicit `_ => {}`
catch-all arm.

### Non-enum match

Matching on non-enum types (integers, strings) is not supported. Only enum-tagged dispatch
is available.

### Expression context

`match` is an expression. The body blocks evaluate to whatever they evaluate to, but
currently the result is not unified across arms — each arm executes its body for side
effects and/or `return`. A future version may allow match to yield a value.

## Region

A `region` block allocates a scoped bump-allocator arena on the stack. Memory allocated
inside a region is automatically freed when the block exits (the alloca goes out of scope).

```
region R {
    let p: int* = __region_alloc(8)     // allocate 8 bytes from region R
    p[0] = 42
    p[1] = 99
    return *p
}                                       // R is freed here — all allocations invalidated
```

### `__region_alloc(n)`

The built-in `__region_alloc(n)` function allocates `n` bytes in the innermost enclosing
region. It returns a pointer (`void*` equivalent — assignable to `T*`). If the region overflows
its 4096-byte buffer, the program traps (LLVM `unreachable`). The buffer size is fixed per
region block.

### Nesting

Regions can be nested. Each region has its own bump pointer; allocating from `__region_alloc`
uses the innermost enclosing region:

```
region Outer {
    let a: int* = __region_alloc(8)
    region Inner {
        let b: int* = __region_alloc(8)     // from Inner's buffer
    }
    let c: int* = __region_alloc(8)         // from Outer's buffer (Inner is freed)
}
```

### Region lifetime tracking

The compiler tracks region-allocated pointers and rejects returning them outside the
region scope. This prevents the most common use-after-free bug — escaping a region
pointer to the caller:

```
fn bad() -> int8* {
    region R {
        let p: int8* = __region_alloc(8)
        p[0] = 42
        return p    // Error: region will be freed before the function returns
    }
}
```

Propagation through `let` is also tracked:

```
fn also_bad() -> int8* {
    region R {
        let p: int8* = __region_alloc(8)
        let q: int8* = p       // q inherits the region tag
        return q               // Error: q points into the region
    }
}
```

This check works at compile time with zero runtime cost. It does not track pointers
through pointer arithmetic, function calls, or writes to global variables — those
patterns remain the programmer's responsibility.

Region lifetime analysis is independent of the borrow checker. The borrow checker
enforces safe reference usage (see [Borrow checking](/docs/syntax-expressions#borrow-checking));
region lifetime checking prevents region-allocated pointers from outliving their region.
Both run at compile time with zero runtime cost.

Regions are purely a scoped memory optimization — there is no garbage collection and
no reference counting.

### docs/syntax-data-structures.md

---
layout: ../../layouts/DocsLayout.astro
title: Data Structures
---

# Data Structures

## Arrays

An array is a fixed-size, contiguous sequence of elements of the same type. The size
must be a compile-time constant literal integer.

### Array type

Written as `element_type[size]`:

```
int[5]          // 5 int64 values
int8[256]       // 256 bytes
Point[10]       // 10 Point structs
```

### Array literal

Constructed with square brackets containing comma-separated values:

```
let arr: int[4] = [10, 20, 30, 40]
let bytes: int8[3] = [65, 66, 67]
```

### Array access

Index with square brackets. Indexing is zero-based. Out-of-bounds access is undefined
behavior (no bounds checking at runtime by default).

```
let arr: int[4] = [10, 20, 30, 40]
let first: int = arr[0]       // 10
let third: int = arr[2]       // 30
arr[1] = 99                   // write
```

### Array-to-pointer decay

When an array name is used in a context that expects a pointer, it implicitly converts
("decays") to a pointer to its first element. This is used for pointer arithmetic on
arrays.

```
let arr: int[4] = [10, 20, 30, 40]
let p: int* = arr              // same as &arr[0]
let val: int = *(p + 8)        // 20 — see pointer arithmetic
```

Array subscripts can be used as lvalues for assignment:

```
arr[0] = 99
arr[1] += 5
```

### Memory layout

Elements are laid out consecutively in memory with no padding between elements.
Alignment follows the element type's alignment.

```
// int[4] layout (each int = 8 bytes):
// [0]: bytes 0-7
// [1]: bytes 8-15
// [2]: bytes 16-23
// [3]: bytes 24-31
```

## Structs

A struct is a named product type grouping multiple fields of possibly different types.

### Generic structs

A struct can be parameterized over one or more type parameters using angle brackets:

```
struct Name<T> {
    field1: T,
    field2: type2,
}
struct Pair<A, B> {
    first: A,
    second: B,
}
```

Type parameters can appear anywhere a type annotation is valid: field types, nested
generic types, etc.

```
struct Pair<T> {
    first: T,
    second: T,
}
struct Triple<A, B, C> {
    a: A,
    b: B,
    c: C,
}
```

Instantiation uses angle brackets with concrete types:

```
let p: Pair<int64> = Pair<int64> { first: 10, second: 20 }
let t: Triple<int64, bool, int64> = Triple<int64, bool, int64> { a: 1, b: true, c: 3 }
```

Nested generics (a generic struct whose type argument is itself a generic
instantiation) are supported:

```
let inner: Pair<int64> = Pair<int64> { first: 3, second: 7 }
let outer: Pair<Pair<int64>> = Pair<Pair<int64>> { first: inner, second: inner }
```

Each distinct combination of type arguments produces a separate LLVM struct type at
compile time (monomorphization), keyed by a mangled name (e.g. `Pair$i64`).

**Current limitations:**
- Generic enums are not yet supported.

### Traits

A trait defines a set of method signatures that types can implement. Traits enable
polymorphic generic functions through trait bounds.

```
trait TraitName {
    fn method1(self: Self, param1: type1) -> returntype
    fn method2(self: Self) -> returntype
}
```

The `Self` type refers to the type that will implement the trait.

Example:

```
trait Area {
    fn area(self: Self) -> int64
}
```

A trait declaration contains only method signatures (no bodies). The actual
implementation is provided in an `impl` block.

### Impl blocks and method calls

An `impl` block associates functions with a struct type. These functions are called
as *methods* using dot syntax: `obj.method(args)`.

#### Inherent impls

An inherent impl attaches methods directly to a type without requiring a trait:

```
impl TypeName {
    fn method_name(self: TypeName, param1: type1, ...) -> returntype {
        // body can access self fields
    }
}
```

Example:

```
struct Point { x: int64, y: int64 }

impl Point {
    fn magnitude(self: Point) -> int64 {
        return self.x + self.y
    }
}

let p: Point = Point { x: 10, y: 20 }
let m: int64 = p.magnitude()     // 30
```

The `self` parameter is always the first parameter and must have the impl type.
When calling `obj.method(args)`, `obj` is automatically passed as `self`.

#### Trait impls

A trait impl implements a trait for a type, enabling trait-bounded polymorphism:

```
impl TraitName for TypeName {
    fn method_name(self: TypeName, ...) -> returntype {
        // body
    }
}
```

### Declaration

```
struct Name {
    field1: type1,
    field2: type2,
    // ...
}
```

The trailing comma after the last field is optional.

```
struct Point {
    x: int,
    y: int,
}
struct Person {
    name: string,
    age: int,
}
```

### Construction

Structs are constructed with curly braces using the struct name. Fields can be specified
by name (in any order) or by position (in declaration order):

```
let p: Point = Point { x: 10, y: 20 }       // named fields (any order)
let q: Point = Point { 10, 20 }              // positional fields (declaration order)
let person: Person = Person { name: "Alice", age: 30 }
```

Named and positional fields cannot be mixed within a single constructor — the compiler
emits an error if both forms appear.

### Field access

Fields are accessed with the dot `.` operator:

```
let px: int = p.x
let age: int = person.age
```

Field access can be the target of assignment:

```
p.x = 99
person.age += 1
```

### Structs as values

Structs are passed by value (copied) into functions and returned by value. For large
structs, use a pointer to avoid copying.

```
fn move_x(p: Point, dx: int) -> Point {
    return Point { p.x + dx, p.y }
}
fn move_x_in_place(p: Point*, dx: int) {
    (*p).x = (*p).x + dx
}
```

### Memory layout

Fields are laid out in declaration order with natural alignment padding between fields.
The struct as a whole has the alignment of its most-aligned field.

## Enums

An enum is a tagged union: it holds one of several named variants, each optionally
carrying fields.

### Declaration

```
enum Name {
    Variant1 { field1: type1, field2: type2 },
    Variant2 { field: type },
    Variant3 {},                    // no fields
}
```

The trailing commas are optional. A variant with no fields uses `{}`. At least one
variant is required.

```
enum Option {
    Some { value: int },
    None {},
}
enum Shape {
    Circle { radius: float64 },
    Rect { w: float64, h: float64 },
}
```

### Construction

An enum value is constructed by naming the variant, like calling a function:

```
let v: Option = Some { value: 42 }
let n: Option = None {}
let c: Shape = Circle { radius: 1.0 }
let r: Shape = Rect { w: 3.0, h: 4.0 }
```

Fields can be specified by name (in any order) or by position (in declaration order):

```
let v: Option = Some { value: 42 }        // named field
let v2: Option = Some { 42 }              // positional field
let n: Option = None {}
let c: Shape = Circle { radius: 1.0 }
let r: Shape = Rect { 3.0, 4.0 }          // positional: w=3.0, h=4.0
```

Named and positional fields cannot be mixed within a single constructor.

### Memory layout

The enum stores a tag (a discriminator integer) plus a union of all variant fields.
The tag is at offset zero and occupies one byte. Fields are laid out after the tag,
at the same offset for all variants, sized to the largest variant. Padding ensures
alignment.

For example, `Option` with `Some { value: int }` and `None {}`:
- Offset 0: 1-byte tag (0 = Some, 1 = None)
- Offset 8: value (aligned to int64's 8-byte alignment)

`None {}` contributes 0 bytes of payload. The total struct size is 16 bytes
(1 byte tag + 7 padding + 8 bytes value).

### Pattern matching

Enum values are destructured with [match](/docs/syntax-control-flow#match):

```
let s: Shape = Circle { radius: 2.0 }
let area: float64 = match s {
    Circle { radius } => {
        return 3.14159 * radius * radius
    }
    Rect { w, h } => {
        return w * h
    }
}
```

The match selects the arm whose tag matches the enum's tag. Fields from that variant
are bound to local variables with the same name as declared in the enum definition.

### docs/syntax-expressions.md

---
layout: ../../layouts/DocsLayout.astro
title: Expressions
---

# Expressions

## Operator precedence

Operators are listed below in descending precedence (tighter binds first). Operators on
the same line have the same precedence and associate left-to-right.

| Precedence | Operators                          | Category              |
|------------|------------------------------------|-----------------------|
| 1          | `()` `[]` `::<>` `.`               | Call / index / turbofish / method call / field access |
| 2          | `*` (deref) `&` (borrow) `&mut` (mut borrow) `-` (neg) `~` (bitnot) `!` (not) | Unary prefix     |
| 3          | `*` `/` `%`                        | Multiplicative        |
| 4          | `+` `-`                            | Additive              |
| 5          | `<<` `>>`                          | Shift                 |
| 6          | `&`                                | Bitwise AND           |
| 7          | `^`                                | Bitwise XOR           |
| 8          | `\|`                               | Bitwise OR            |
| 9          | `==` `!=` `<` `>` `<=` `>=`        | Comparison            |
| 10         | `&&`                               | Logical AND           |
| 11         | `\|\|`                             | Logical OR            |
| 12         | `=` `+=` `-=` `*=` `/=` `&=` `\|=` `^=` `<<=` `>>=` | Assignment / compound |

## Comparison operators

Return `bool` (`true` or `false`). Available on all integer and float types.
Pointers can be compared with `==` and `!=`.

```
==    Equal
!=    Not equal
<     Less than
>     Greater than
<=    Less than or equal
>=    Greater than or equal
```

```
let a: int = 10
let b: int = 20
let eq: bool = a == b      // false
let lt: bool = a < b       // true
let ne: bool = a != b      // true
```

Comparison operators are at precedence level 9, lower than bitwise and shift,
so `x & 3 == 0` parses as `x & (3 == 0)`. Use parentheses for the intended meaning:
`(x & 3) == 0`.

## Logical operators

Short-circuiting boolean operators. Operands are coerced to `bool` (non-zero is truthy).
Return `bool`.

```
&&    Logical AND — evaluates RHS only if LHS is truthy
||    Logical OR — evaluates RHS only if LHS is falsy
!     Logical NOT (unary prefix)
```

```
let a: bool = true
let b: bool = false
let c: bool = a && b        // false
let d: bool = a || b        // true
let e: bool = !a            // false
```

`!` is a unary prefix operator at precedence level 2. `&&` is level 10, `||` is level 11.

## Bitwise operators

Operate on integer types. Not available on floats or pointers.

```
&     Bitwise AND
|     Bitwise OR
^     Bitwise XOR
~     Bitwise NOT (unary prefix)
```

```
let a: int = 0xFF00
let b: int = 0x0FF0
let c: int = a & b          // 0x0F00
let d: int = a | b          // 0xFFF0
let e: int = a ^ b          // 0xF0F0
let f: int = ~a             // 0xFFFFFFFFFFFF00FF
```

Bitwise AND `&` is at level 6, XOR `^` at level 7, OR `|` at level 8. Bitwise NOT `~`
is a unary prefix at level 2.

Note: `&` as a binary operator is bitwise AND. The address-of operator is a unary prefix `&`,
distinguished by context.

## Arithmetic operators

Available on all integer and float types. Integer overflow is two's complement wraparound
(as in LLVM, not UB — but this may change to poison in the future).

```
+     Addition
-     Subtraction (binary) / Negation (unary prefix)
*     Multiplication
/     Division
%     Modulo (remainder)
```

```
let x: int = 10 + 20        // 30
let y: int = x - 5          // 25
let z: int = x * y          // 750
let q: int = z / 10         // 75
let r: int = q % 7          // 75 % 7 = 5
let n: int = -q             // -75
```

`*`, `/`, and `%` are level 3 (multiplicative). `+` and `-` (binary) are level 4.
Unary `-` (negation) is level 2.

## Shift operators

Shift left and shift right on integer types. Shift amount must be non-negative. The
behavior for shift amounts equal to or greater than the bit width is poison (LLVM
poison value).

```
<<    Shift left (zero-fill)
>>    Shift right (arithmetic — sign-extending)
```

```
let x: int = 1
let y: int = x << 3         // 8
let z: int = y >> 2         // 2
```

Shift operators are at precedence level 5.

## Assignment

The assignment operator `=` evaluates its right-hand side and assigns the value to the
left-hand side (which must be a place — a variable, a pointer dereference, or an array
subscript). Assignment is an expression: it evaluates to the assigned value.

```
let x: int = 10
let y: int = (x = 20)       // assigns 20 to x, y is also 20
```

### Compound assignment operators

Compound assignment applies an operation and an assignment in one step:

```
+=    Add and assign
-=    Subtract and assign
*=    Multiply and assign
/=    Divide and assign
%=    Modulo and assign
&=    Bitwise AND and assign
|=    Bitwise OR and assign
^=    Bitwise XOR and assign
<<=   Shift left and assign
>>=   Shift right and assign
```

```
let x: int = 10
x += 5                       // x = 15
x *= 2                       // x = 30
x &= 0xFF                    // x = 30
```

Compound assignment operators all have the same precedence as `=` (level 12), the lowest.

### Target evaluation

For assignment and compound assignment, the left-hand side (the "lvalue") is evaluated
first to obtain a pointer, then the right-hand side is evaluated, and finally the store
(or load-modify-store for compound) is performed. The lvalue expression is evaluated
exactly once — there is no double evaluation.

## References and Pointers

Hokkaido has two families of indirection: **references** (safe, borrow-checked) and
**raw pointers** (unsafe, no borrow checking).

Reference types use `&T` and `&mut T` syntax. Raw pointer types use `T*` syntax.
Both compile down to LLVM pointers, but references are subject to compile-time
borrow checking (see [Borrow checking](#borrow-checking) below).

### Borrow expressions

| Expression   | Description                                                       |
|--------------|-------------------------------------------------------------------|
| `&expr`      | Borrows `expr` immutably (shared reference `&T`).                 |
| `&mut expr`  | Borrows `expr` mutably (exclusive reference `&mut T`).            |

```
let x: int = 42
let r: &int = &x                // shared reference to x
let rw: &mut int = &mut x       // mutable reference to x
```

**Function references.** When `&` is applied to a function name, it creates a
function value of the corresponding function type (see
[Higher-Order Functions](/docs/syntax-functions#higher-order-functions)):

```
fn add_one(x: int) -> int { return x + 1 }
let f: fn(int) -> int = &add_one
let r: int = f(5)               // 6
```

Without the `&`, a bare function name is not a value — it can only appear in a
call expression `add_one(...)`. The `&` is required to obtain a function value.

### Dereference

The unary `*` operator reads or writes through a reference or raw pointer.

```
let val: int = *r            // read: val = 42
*r = 99                      // write: x is now 99
```

Dereference can be the target of assignment, including compound assignment:

```
*r = 100
*r += 5                      // x is now 105
```

### Borrow checking

The borrow checker enforces these rules during compilation:

| Rule | Description |
|------|-------------|
| **Exclusivity** | At any given time, you may have either *one* mutable reference or *any number* of shared references to a value, but not both. |
| **Liveness** | References must never outlive the value they refer to (enforced lexically). |
| **Ownership freeze** | The original value cannot be read or written while it is borrowed — the owner is frozen until the reference goes out of scope. |

The checker runs on every function before code generation. It tracks the scope depth
of each borrow in a per-function borrow graph and rejects programs that violate any
of the rules above.

```
let x: int = 42
let r1: &mut int = &mut x
let r2: &mut int = &mut x   // ERROR: cannot borrow `x` as mutable more than once
```

A valid program that respects the borrow rules:

```
let x: int = 42
{
    let r: &mut int = &mut x
    *r = 10
}
// `r` is no longer live here; `x` is usable again
let y: int = x               // OK
```

### Region blocks

A `region { ... }` block creates a memory region that lives for the duration of the
block. Allocations inside the block are freed when the block exits.

```
region {
    let p: int* = __region_alloc(int, 1)
    *p = 42
}
// p is no longer valid here — the region has been freed
```

The compiler rejects returning a region-allocated pointer outside its region block,
preventing use-after-free errors.

### Raw pointers

Raw pointers use `T*` syntax and are not borrow-checked:

```
let p: int* = null
```

**Null pointers.** `null` is a keyword that evaluates to a null pointer of any raw
pointer type. Dereferencing a null pointer is undefined behavior.

```
let p: int* = null
```

**Pointer arithmetic.** Pointer arithmetic is performed by integer
addition/subtraction on pointer-typed expressions. The stride is always 1 element
— no implicit scaling by element size. (Note: this is the reverse of C — `p + n`
advances by `n` bytes, not `n * sizeof(T)`. This design choice simplifies low-level
memory manipulation.)

```
let p: int* = null
let q: int* = p + 8   // advances from null by 8 bytes
```

## Atomic operations

Atomic operations provide lock-free concurrent memory access. The `atomic(operation, ...)` expression
generates LLVM atomic instructions (all with `seq_cst` ordering).

```
atomic(xchg,  ptr, val)      // atomic exchange: *ptr = val; return old *ptr
atomic(add,   ptr, val)      // atomic add: *ptr += val; return old *ptr
atomic(sub,   ptr, val)      // atomic subtract: *ptr -= val; return old *ptr
atomic(and,   ptr, val)      // atomic bitwise AND: *ptr &= val; return old *ptr
atomic(or,    ptr, val)      // atomic bitwise OR:  *ptr |= val; return old *ptr
atomic(xor,   ptr, val)      // atomic bitwise XOR: *ptr ^= val; return old *ptr
atomic(cas,   ptr, cmp, val) // compare-and-swap: if *ptr == cmp, *ptr = val; return old *ptr
atomic(fence)                 // memory fence (no args, returns void)
```

The first argument after the operation name must be a pointer (`&var`, `ptr`, or `*` dereference).
Integer pointer types (int8*, int32*, int64*) are supported. The result type is the pointee type
(or `void` for `fence`).

```
let counter: int = 0
let old: int = atomic(xchg, &counter, 100)    // old=0, counter=100
let prev: int = atomic(add, &counter, 5)       // prev=100, counter=105
let ok: int = atomic(cas, &counter, 105, 200)   // CAS succeeds: counter=200
let no: int = atomic(cas, &counter, 0, 999)     // CAS fails: counter stays 200
atomic(fence)                                   // memory fence
```

`atomic(...)` is a primary expression (precedence level 1).

## Constructor expressions

Struct values and enum variants are constructed using the `{ }` syntax:

```
Point { x: 10, y: 20 }      // named fields
Point { 10, 20 }             // positional fields
Opt::Some { 42 }             // enum variant with positional field
Opt::Some { val: 42 }        // enum variant with named field
```

Named and positional fields cannot be mixed within a single constructor — the parser
emits an error if both forms appear. Named fields can appear in any order; the compiler
maps them by name to the correct struct field position. Positional fields are mapped by
index.

Constructor expressions are a postfix operator on the type/variant name (precedence
level 1, same as function calls and field access).

## Function calls

A function call evaluates the argument expressions left to right, then transfers control
to the function.

```
result = name(arg1, arg2, arg3)
```

### Turbofish type arguments

When calling a [generic function](/docs/syntax-functions#generic-functions), provide explicit
type arguments with the `::< >` syntax:

```
let x: int = identity::<int>(42)
let y: float64 = pair::<int, float64>(42, 3.14)
let z: int = util::wrap::<int>(99)    // namespaced generic call
```

The turbofish sequence `::<` is parsed as a single token by the parser and never
conflicts with the less-than operator.

### Method-call syntax

Methods defined in [impl blocks](/docs/syntax-data-structures#impl-blocks-and-method-calls)
are called with dot syntax:

```
obj.method(arg1, arg2)
```

The `obj` expression is automatically passed as the first argument (`self`) to the method.
Method calls chain naturally with field access:

```
let a: int64 = point.area()
let x: int64 = point.get_x()
```

Method calls have the same precedence as field access and function calls (highest
precedence group).

### Static function calls

Functions inside [namespaces](/docs/syntax-modules#namespaces) are called with `::` syntax:

```
namespace geometry {
    fn distance(p: Point, q: Point) -> float64 { /* ... */ }
}
let d = geometry::distance(p, q)
```

## If-expressions

An `if`-expression evaluates to a value. It requires an `else` branch, and the types of
both branches must match. `else if` chains are supported:

```
let result: int64 = if condition { then_expr } else { else_expr }
```

```
let x: int64 = if a > b { a } else { b }    // x = max(a, b)
let y: int64 = if a > 0 { 1 } else if a < 0 { -1 } else { 0 }
```

`if`-expressions can be nested or used wherever an expression is expected:

```
let z: int64 = if flag { if nested { 1 } else { 2 } } else { 0 }
```

The `else` branch is mandatory — there is no implicit "fall through" to a default value.
Each branch body is either a single expression or a block `{ ... }` whose last statement
is an expression (the block's value). Other statements in the block execute for their
side effects.

If-expressions are primary expressions (precedence level 1).

## Char literals

A char literal represents a single 8-bit character. It is written with single quotes and
has type `char` (equivalent to `uint8`).

```
'a'
'\n'    // newline
'\t'    // tab
'\\'    // backslash
'\''    // single quote
```

```
let c: char = 'A'
let nl: char = '\n'
```

Char literals are distinct from integer literals. They are a primary expression
(precedence level 1) and can be used anywhere an expression is expected.

## Tuple expressions

A tuple expression constructs a tuple value. It is written as a comma-separated list of
expressions inside parentheses:

```
(expr1, expr2, ...)
```

```
let pair: (int, bool) = (42, true)
let triple: (int8, float32, int) = (1, 3.14, 100)
```

A tuple with a single element `(expr)` is treated as a parenthesized expression, not a
1-tuple. Only tuples of two or more elements create a tuple type.

Tuple expressions are primary expressions (precedence level 1).

### Tuple field access

Individual fields of a tuple are accessed using `.0`, `.1`, `.2`, etc. — the `.`
followed by the zero-based field index:

```
let pair: (int, bool) = (42, true)
let first: int = pair.0     // 42
let second: bool = pair.1   // true
```

Tuple field access reuses the field-access codegen path and compiles to LLVM
`extractvalue` / GEP, producing the same efficient code as struct field access.

### docs/syntax-ffi-cubical.md

---
layout: ../../layouts/DocsLayout.astro
title: C FFI & Cubical
---

# C FFI, Freestanding, Inline Assembly, and Cubical

## C FFI

Hokkaido can call C functions and be called from C code. FFI declarations use the
`extern` keyword to indicate that the function is defined externally (linked from a
C library or object file).

### Extern function declarations

```
extern fn name(param1: type1, param2: type2, ...) -> return_type
```

The function body is omitted — the implementation comes from the linker.

```
extern fn putchar(c: int8) -> int32
extern fn printf(fmt: string, ...) -> int32
extern fn sqrt(x: float64) -> float64
```

### Variadic parameters

Extern functions can be declared variadic with `...` after the required parameters.
There is no type checking on variadic arguments — the compiler passes them through
according to the platform calling convention.

```
extern fn printf(fmt: string, ...) -> int32
```

### String / char* handling

Hokkaido's `string` type maps to a C `char*` when passed to an extern function.
When a C function returns `char*`, Hokkaido treats it as a raw pointer (`int8*`).

### Linking

All object files are linked with `cc` (the system C compiler) by default. To link
against a specific library, add `-l<libname>` to the linker flags:

```
// Compile: hokkaido file.hk && clang file.o -lm -o file
```

### Calling convention

Extern functions use the C calling convention (`ccall`). Regular (non-extern)
Hokkaido functions do not use a stable ABI — they may use the C calling convention
internally, but this is not guaranteed.

## Freestanding mode

Freestanding mode produces code with no dependency on the C runtime. The program
must provide its own entry point and is linked directly with the system linker (or
with a custom linker script).

### Entry point

In freestanding mode, `main` is compiled to use the C calling convention and can
serve as the program entry point when linked appropriately. The return value of
`main` is passed to the exit system call via the LLVM-generated `_start` stub or
a user-provided entry point.

### Syscalls

System calls are made via inline assembly (see below) or by calling extern functions
from a static library that implements the raw syscall interface.

### No CRT dependency

Freestanding mode avoids linking against `crt0`, `libc`, and other C runtime objects.
The resulting binary is typically smaller and has no startup overhead.

## Inline assembly

Raw machine instructions can be embedded using assembly blocks. Inline assembly is
parsed as a string by the compiler and passed directly to the LLVM backend.

### Basic syntax

```
asm("instruction_template" : output_operands : input_operands : clobbers)
```

Each operand is written as `"constraint" (expression)`. The template uses `$0`, `$1`,
etc. to refer to operands by position.

```
// x86-64: issue a write syscall
let msg: string = "hello\n"
let len: int = 6
let ret: int = 0
asm(
    "syscall"
    : "={rax}" (ret)
    : "{rax}" (1), "{rdi}" (1), "{rsi}" (msg), "{rdx}" (len)
    : "rcx", "r11"
)
```

### Constraints

- `={reg}` — output operand, assigned from the named register after the asm executes.
- `{reg}` — input operand, placed into the named register before the asm executes.

Common registers: `{rax}`, `{rbx}`, `{rcx}`, `{rdx}`, `{rsi}`, `{rdi}`, `{r8}`...`{r15}`,
`{xmm0}`...`{xmm15}`.

### Clobbers

Registers that are modified by the inline assembly but not listed as outputs must be
listed in the clobber list. The compiler will save and restore them around the asm block.
Current clobber values are passed as a comma-separated list of register names in the
template string after `:`, e.g. `: "rcx", "r11"`.

### Volatile

Inline assembly is treated as volatile by default — the compiler will not remove,
move, or optimize the asm block.

## Cubical

Cubical is a compile-time evaluation subsystem for Hokkaido. It embeds a
Rust cubical language backend that type-checks
and evaluates cubical source files during compilation, embedding the result as a
constant in the generated code.  
for reference check [cubical_surface_language](/docs/cubical_surface_language)

### Cubical type

The `cubical` type is a compile-time placeholder. After evaluation it resolves to
`int64` (Nat), `int8` (Bool), an LLVM anonymous struct (pair), an LLVM array
(cons-chain), or a pointer to a string constant (fallback for non-structurable
values like universes or functions).

### Usage

Declare a cubical value by binding a string literal (a path to a `.cub` file) to the
`cubical` type:

```
let n: cubical = "path/to/example.cub"
```

At compile time, the compiler loads the file, sends it to the Rust cubical backend
for parsing, type-checking, and evaluation (via normalization-by-evaluation), then
embeds the resulting constant directly into the LLVM IR.

```
// --- test/example.cub ---
// (contains a cubical expression that evaluates to a Nat)

// --- main.hk ---
let n: cubical = "../test/example.cub"

fn main() -> int {
    return n       // n is already an int64 constant
}
```

### Result resolution

The cubical backend returns a JSON-serialized result. The compiler resolves it as
follows:

- **Nat** (decimal integer or `suc`/`zero` chain) → the variable becomes an
  `int64` constant.
- **Bool** (`True` or `False`) → the variable becomes an `int8` constant (1 or 0).
- **Pair** (Σ-type, written `(a , b)`) → the variable becomes an LLVM anonymous
  struct constant with the appropriate field types. Nested pairs produce nested
  struct types.
- **Cons-chain** (`nil` / `cons`) → the variable becomes an LLVM array constant.
  All elements must have the same type.
- **Other values** (universes, functions, etc.) → the variable becomes a `string`
  constant containing the cubical backend's text output, stored as an LLVM
  pointer-to-`i8` (compatible with `printf("%s", ...)`).

### Top-level let declarations

Top-level `let` declarations with the `cubical` type produce LLVM `GlobalVariable`
objects (not stack allocas), making them visible from every function body:

```
let n: cubical = "data Nat = | zero : Nat | suc : Nat -> Nat\ndef main : Nat = suc (suc (suc zero))"

fn main() -> int {
    printf("n = %ld\n", n)    // prints "n = 3"
    return 0
}
```

When the cubical result is a string, a pointer-typed global is created that
stores the address of the underlying string constant. Loading it yields a
pointer suitable for passing to C functions like `printf`.

### Inline vs file source

The string literal can contain inline cubical source or a path to a `.cub` file
(resolved relative to the source file's directory):

```
let a: cubical = "path/to/example.cub"   -- file path
let b: cubical = "def main : Nat = ..."  -- inline source
```

There is no runtime cubical evaluation — everything happens during compilation.

### docs/syntax-functions.md

---
layout: ../../layouts/DocsLayout.astro
title: Functions
---

# Functions

## Function declarations

A function is declared with the `fn` keyword, followed by the name, parameter list,
optional return type, and a body block:

```
fn name(param1: type1, param2: type2) -> return_type {
    body
}
fn name(param: type) {
    // implicitly returns void
}
```

### Parameters

Each parameter has an explicit type annotation, just like `let`. Parameters are always
passed by value. There are no default arguments, optional parameters, or variadic
parameters (except in [C FFI](/docs/syntax-ffi-cubical#c-ffi) declarations).

```
fn greet(name: string, count: int) {
    // ...
}
```

### Return type

When a function returns a value, the return type is written after `->`:

```
fn add(a: int, b: int) -> int {
    return a + b
}
```

If the arrow and type are omitted, the function implicitly returns `void`:

```
fn log(msg: string) {
    // no return needed — implicitly returns void
}
```

### The main function

Every program must have a `main` function. It takes no parameters and returns `int`:

```
fn main() -> int {
    return 0
}
```

The return value of `main` is the process exit code. A value of `0` conventionally
indicates success; any non-zero value indicates an error.

### Calling functions

Functions are called with the standard `name(args)` syntax:

```
let result: int = add(3, 4)
log("hello")
```

See [Function calls](/docs/syntax-expressions#function-calls) for details on call expressions,
including the turbofish syntax for providing type arguments to generic functions.

### Forward references

A function can call any other function declared in the same file (or included file),
regardless of declaration order. Top-to-bottom ordering is not required.

```
fn early() -> int {
    return late()     // OK — late is defined later in the file
}

fn late() -> int {
    return 99
}
```

### All paths must return

Hokkaido does not perform control-flow analysis to verify that all paths return a value.
If a function declared with `-> return_type` reaches the end of its body without encountering
a `return` statement, the behavior is undefined (LLVM poison).

```
fn oops() -> int {
    // no return — undefined behavior at runtime!
}
```

## Generic functions

A function can be generic over one or more type parameters. Type parameters are declared
inside angle brackets immediately after the function name:

```
fn identity<T>(x: T) -> T {
    return x
}
fn pair<A, B>(a: A, b: B) -> A {
    return a
}
```

Type parameters can be used anywhere a type annotation appears: parameter types,
return type, local variable types, or as type arguments to other generic functions.

### Explicit type arguments (turbofish)

Generic functions must be called with explicit type arguments using the turbofish `::< >`
syntax. The compiler *never* infers type arguments.

```
let a: int = identity::<int>(42)
let b: float64 = identity::<float64>(3.14)
let f: int = pair::<int, float64>(42, 3.14)
```

The turbofish `::<` sequence disambiguates the opening `<` from the less-than operator.

### Generic calling generic

A generic function may call another generic function, passing its own type parameters
as arguments. The compiler substitutes the concrete types at monomorphization time.

```
fn identity<T>(x: T) -> T {
    return x
}
fn wrap<T>(x: T) -> T {
    return identity::<T>(x)      // T is substituted later
}
fn main() -> int {
    return wrap::<int>(42)       // → monomorphizes both wrap<int> and identity<int>
}
```

### Monomorphization

Each distinct combination of type arguments generates a separate copy of the function
body at compile time. For example, `identity::<int>` and `identity::<float64>` produce
two independent LLVM functions.

### Namespaced generics

Generic functions inside [namespaces](/docs/syntax-modules#namespaces) work the same way:

```
namespace util {
    fn identity<T>(x: T) -> T {
        return x
    }
}
fn main() -> int {
    return util::identity::<int>(42)
}
```

### Trait bounds

A type parameter can be constrained to types that implement a specific trait using
colon syntax:

```
fn double<T: Area>(x: T) -> int64 {
    return x.area() * 2
}
```

Multiple bounds are supported with `+`:

```
fn process<T: Area + Describe>(x: T) -> int64 {
    return x.area() + x.value()
}
```

When a generic function with trait bounds is monomorphized, the compiler verifies
that the concrete type argument implements all required traits. A compile error is
reported if the bound is not satisfied.

Traits and impls are defined with:
- [`trait` declarations](/docs/syntax-data-structures#traits) for defining method signatures
- [`impl` blocks](/docs/syntax-data-structures#impl-blocks-and-method-calls) for implementing
  traits on types or adding inherent methods

### Current limitations

- Generic enums are not yet supported (struct generics work — see
  [Generic structs](/docs/syntax-data-structures#generic-structs)).
- Type parameters cannot be used in array sizes (e.g. `int[N]` is not valid).

## Higher-order functions

Functions can accept other functions as parameters and return them as values,
using function type syntax `fn(T1, T2) -> Ret`.

### Function type syntax

```
fn(int) -> int            // takes int, returns int
fn(int, bool) -> void     // takes int and bool, returns nothing
fn() -> int               // takes nothing, returns int
```

Function type values are opaque pointers to a closure struct. Any function
or lambda with a matching signature can be used.

### Creating function values

**From a named function** with the `&` prefix:

```
fn add_one(x: int) -> int {
    return x + 1
}

let f: fn(int) -> int = &add_one
```

**From a lambda expression** (creates a closure inline):

```
let f: fn(int) -> int = lambda (x: int) -> int { return x + 2 }
```

Lambdas can capture variables from their enclosing scope by value:

```
let offset: int = 3
let f: fn(int) -> int = lambda (x: int) -> int { return x + offset }
```

### Calling function values

A function-typed variable is called with the same `name(args)` syntax as a
regular function:

```
let result: int = f(42)
```

### Passing closures to functions

```
fn apply_twice(f: fn(int) -> int, x: int) -> int {
    return f(f(x))
}

fn main() -> int {
    // With a named function:
    let r1: int = apply_twice(&add_one, 5)        // 7

    // With a lambda:
    let r2: int = apply_twice(
        lambda (x: int) -> int { return x * 2 }, 5
    )                                              // 20

    return r1 + r2
}
```

### Restrictions

- `&fn_name` creates a reference to a specific function; the `&` is required
  (bare `fn_name` without `()` is not a value).
- There is no implicit conversion from a bare function name to a function
  value — the `&` is always needed.
- A function-typed variable cannot be called if its type is incomplete
  (e.g. `fn() ->` without a return type is a parse error).

### Standard library HOFs

The stdlib provides common higher-order functions for `int` in `std/functional.hk`:

```
import "std"

fn add_one(x: int) -> int { return x + 1 }
fn add(x: int, y: int) -> int { return x + y }

fn main() -> int {
    let r1: int = std::twice(&add_one, 5)             // 7
    let r2: int = std::compose(&add_one, &add_one, 5) // 7
    let r3: int = std::apply_n(&add_one, 10, 5)       // 15

    let arr: int[5] = [1, 2, 3, 4, 5]
    let r4: int = std::fold_int(&add, 0, arr, 5)      // 15
    return r1 + r2 + r3 + r4
}
```

Available functions in `std::`:

| Function | Signature | Description |
|----------|-----------|-------------|
| `twice` | `(fn(int)->int, int) -> int` | Applies `f` twice |
| `thrice` | `(fn(int)->int, int) -> int` | Applies `f` three times |
| `compose` | `(fn(int)->int, fn(int)->int, int) -> int` | `f(g(x))` |
| `apply_n` | `(fn(int)->int, int, int) -> int` | Applies `f` `n` times |
| `fold_int` | `(fn(int,int)->int, int, int[], int) -> int` | Left fold over slice |
| `any_int` | `(fn(int)->bool, int[], int) -> bool` | Any element matches? |
| `all_int` | `(fn(int)->bool, int[], int) -> bool` | All elements match? |
| `map_int_into` | `(fn(int)->int, int[], int[], int) -> int` | Map into existing array |

### Standard library memory operations

The stdlib provides safe memory operations in `std/mem.hk` for working with
raw byte buffers. These functions are safe — they operate on caller-provided
valid pointers and do not manage allocation or deallocation.

```
import "std"

fn main() -> int {
    region R {
        let buf: int8* = __region_alloc(32)

        std::mem_set(buf, 42, 16)     // write sixteen 42s
        std::mem_zero(buf + 16, 16)   // zero the second half

        let check: int8* = __region_alloc(32)
        std::mem_copy(check, buf, 32)

        if std::mem_eq(buf, check, 32) {
            std::mem_swap(buf, check, 8)   // swap 8 bytes
            return 1
        }
    }
    return 0
}
```

Available functions in `std::mem`:

| Function | Signature | Description |
|----------|-----------|-------------|
| `mem_copy` | `(int8*, int8*, int64) -> void` | Copy `n` bytes from `src` to `dst` (non-overlapping) |
| `mem_set` | `(int8*, int8, int64) -> void` | Fill `n` bytes of `ptr` with `val` |
| `mem_zero` | `(int8*, int64) -> void` | Zero `n` bytes at `ptr` |
| `mem_eq` | `(int8*, int8*, int64) -> bool` | Compare `n` bytes for equality |
| `mem_swap` | `(int8*, int8*, int64) -> void` | Exchange `n` bytes between two buffers |

Heap allocation (`extern fn malloc`) and deallocation (`extern fn free`) are
available via C FFI but are **inherently unsafe** — the borrow checker does not
track raw pointer lifetimes, so double-free, use-after-free, and memory leaks
are not caught at compile time. Prefer
[region-based allocation](/docs/syntax-control-flow#region) for scoped memory
that is freed automatically when the region exits. Prefer
[borrow-checked references](/docs/syntax-expressions#borrow-checking) for safe
aliasing of stack-allocated data.

## Return

The `return` statement exits the current function and optionally yields a value.

```
return              // exits a void function
return expression   // evaluates expression and returns it
```

```
fn add(a: int, b: int) -> int {
    return a + b
}
fn done() {
    return          // void return — optional at end of function
}
```

If `return` is used outside a function body, it is a compile error.

### docs/syntax-modules.md

---
layout: ../../layouts/DocsLayout.astro
title: Packages, Modules, and Namespaces
---

# Packages, Modules, and Namespaces

Hokkaido uses a Go-style package system for code organization across files and
directories, combined with `namespace` blocks for internal grouping.

## Packages

A package is a directory of `.hk` files that share a common `package` declaration.
Packages form the unit of import and visibility.

### Module root

The module root is the nearest ancestor directory containing an `hk.mod` file
(which can be empty). All import paths are resolved relative to the module root.

```
myproject/
  hk.mod              # marks the module root
  main.hk             # package main
  util/
    util.hk           # package util
    extra.hk          # also package util (same package, multi-file)
```

### Package declaration

Every `.hk` file begins with a `package name;` declaration that identifies
which package it belongs to. All files in the same directory with the same
`package name` are part of the same package — they share scope and can
reference each other's declarations without `include`.

```
// util/util.hk
package util

pub fn add(a: int, b: int) -> int {
    return a + b
}
```

```
// util/extra.hk
package util  // same package as util.hk

pub fn subtract(a: int, b: int) -> int {
    return a - b
}
```

### Entry point

The main executable must use `package main` and define `fn main() -> int`:

```
package main

fn main() -> int {
    return 0
}
```

## Import

Import a package to access its public declarations:

```
import "path/to/package"
import alias "path/to/package"
```

The path is relative to the module root. The last path segment becomes the
default binding name:

```
import "util"          // access items as util::add(...)
import math "util"     // access items as math::add(...)
```

All `.hk` files in the imported directory are parsed. Each must declare
`package <name>` matching the last path segment.

### Import resolution

1. The compiler finds the module root by walking up from the entry file
   looking for `hk.mod`.
2. The import path is resolved relative to the module root.
3. All `.hk` files in that directory are collected (sorted by name for
   deterministic order).
4. Each file's `package` declaration is validated against the expected name.
5. Declarations are merged into the importing file's scope with the
   binding name as a prefix (`util::add`, `util::subtract`).

Duplicate imports (same directory imported twice, directly or transitively)
are silently ignored.

## Visibility

The `pub` keyword controls cross-package visibility:

```
pub fn visible_everywhere() -> int { ... }
fn package_private() -> int { ... }

pub struct Point { x: int, y: int }        // accessible from other packages
struct Internal { z: int }                  // package-private

pub enum Status { Ok, Error }              // accessible from other packages
```

Declarations without `pub` are visible only within their own package.
`pub` applies to `fn`, `let` (top-level), `struct`, and `enum` declarations.

## Namespaces

Within a package, `namespace` blocks provide additional grouping:

```
// util/util.hk
package util

pub namespace geometry {
    pub fn area(width: int, height: int) -> int {
        return width * height
    }
}
```

Access is via the `::` qualifier, combining the package binding, namespace,
and item name:

```
import "util"

fn main() -> int {
    let a: int = util::geometry::area(10, 20)
    return 0
}
```

Namespaces can nest arbitrarily. Declarations inside namespaces can also
have `pub` for cross-package visibility.

## Include (low-level)

The `include "path.hk"` directive is a low-level textual inclusion that
pastes the contents of another file at the point of inclusion. Unlike
`import`, it does not create a package boundary:

- Included declarations are added to the current scope without any name prefix.
- Includes are deduplicated (diamond-shaped includes are safe).
- `include` is primarily useful for code generation, macros, or
  splitting a very large file without creating a package.

For normal code organization across files, prefer `import` and packages.

```
include "helpers.hk"     // declarations from helpers.hk are visible here
```

### docs/syntax-types.md

---
layout: ../../layouts/DocsLayout.astro
title: Types & Variables
---

# Types and Variables

## Types

Every value in Hokkaido has a type, specified with a type annotation using colon syntax:
`name: type`.

### Primitive types

| Type      | Description                          | Size      |
|-----------|--------------------------------------|-----------|
| `int8`    | 8-bit signed integer                 | 1 byte    |
| `int16`   | 16-bit signed integer                | 2 bytes   |
| `int32`   | 32-bit signed integer                | 4 bytes   |
| `int64`   | 64-bit signed integer                | 8 bytes   |
| `int`     | Shorthand for `int64`                | 8 bytes   |
| `uint8`   | 8-bit unsigned integer               | 1 byte    |
| `uint16`  | 16-bit unsigned integer              | 2 bytes   |
| `uint32`  | 32-bit unsigned integer              | 4 bytes   |
| `uint64`  | 64-bit unsigned integer              | 8 bytes   |
| `float16` | 16-bit floating point (half-precision) | 2 bytes |
| `float32` | 32-bit floating point                | 4 bytes   |
| `float64` | 64-bit floating point (double)       | 8 bytes   |
| `float`   | Shorthand for `float64`              | 8 bytes   |
| `bool`    | Boolean — `true` or `false`          | 1 byte    |
| `char`    | 8-bit character (alias for `uint8` with distinct literal syntax `'a'`) | 1 byte |
| `string`  | Opaque string type (internally a pointer) | 8 bytes |
| `void`    | No value (function returns only)     | 0 bytes   |
| `cubical` | Compile-time cubical expression (see [Cubical](/docs/syntax-ffi-cubical#cubical)) | 8 bytes |

```
let a: int8 = -128
let b: int16 = -32000
let c: int32 = 2000000
let d: int64 = 9000000000000000000
let e: int = 42                    // same as int64
let f: uint8 = 255
let g: uint16 = 65535
let h: uint32 = 4000000000
let i: uint64 = 18000000000000000000
let j: float16 = 1.0              // half-precision
let k: float32 = 2.0              // single-precision
let l: float64 = 3.14159265358979
let m: float = 2.71828            // same as float64
let flag: bool = true
let msg: string = "hello"
```

### Struct and enum types

A user-defined [struct](/docs/syntax-data-structures#structs) or [enum](/docs/syntax-data-structures#enums) name is
also a valid type:

```
struct Point { x: int, y: int }
let p: Point = Point { x: 10, y: 20 }

enum Option { Some { value: int }, None }
let v: Option = Some { value: 42 }
```

For [generic structs](/docs/syntax-data-structures#generic-structs), the type
arguments are written inside angle brackets after the struct name:

```
struct Pair<T> { first: T, second: T }
let p: Pair<int64> = Pair<int64> { first: 10, second: 20 }
let nested: Pair<Pair<int64>> = Pair<Pair<int64>> { ... }
```

### Reference types

A reference type is written with the `&` prefix. References are like pointers but with
safety guarantees enforced by the borrow checker at compile time. There are two kinds:

| Type      | Description                          |
|-----------|--------------------------------------|
| `&T`      | Shared (immutable) reference to `T`  |
| `&mut T`  | Mutable (exclusive) reference to `T` |

```
let x: int64 = 42
let r: &int64 = &x         // shared reference to x
let rw: &mut int64 = &mut x  // mutable reference to x
```

See [Borrow Checking](/docs/syntax-expressions#borrow-checking) for the rules governing
reference usage.

### Pointer types

A pointer type is written by appending `*` to the element type — one `*` per level of indirection:

```
int8*      Pointer to int8
int64*     Pointer to int64
int64**    Pointer to pointer to int64
Point*     Pointer to a Point struct
```

Raw pointers are not borrow-checked. They are typically obtained through FFI
(`extern fn malloc`) or low-level memory operations.

```
let p: int8* = null
```

See [Raw pointers](/docs/syntax-expressions#raw-pointers) for more detail.

### Array types

An array type is written by appending `[size]` to the element type:

```
int[5]     Array of 5 int64 values
int8[256]  Array of 256 int8 values
Point[10]  Array of 10 Point structs
```

Array size must be a literal integer (compile-time constant). See [Arrays](/docs/syntax-data-structures#arrays).

### Slice types

A slice type is written by appending `[]` to the element type (no size):

```
int64[]    Slice (dynamically-sized view) of int64 values
int[]      Slice of int values
Point[]    Slice of Point structs
```

A slice is a fat pointer `{ ptr: T*, len: int64 }` in memory — it pairs a pointer to the
first element with a 64-bit length. Slices can be created from an array by passing it to a
function expecting a slice parameter (automatic array-to-slice conversion).

Slice indexing uses the same `s[i]` syntax as arrays.

For heap-allocated slices, use `extern fn malloc` / `extern fn free` from the C standard
library and work with raw pointers:

```
extern fn malloc(size: int64) -> int8*
extern fn free(ptr: int8*) -> void

fn main() -> int64 {
    let ptr: int8* = malloc(24)
    let arr: int64* = ptr
    arr[0] = 10
    arr[1] = 20
    arr[2] = 30
    let s: int64 = arr[0] + arr[1] + arr[2]
    free(ptr)
    return s
}
```

### Function types

A function type is written using the `fn` keyword followed by a parenthesized parameter
type list and an optional return type:

```
fn(T1, T2) -> Ret      Function taking T1, T2 and returning Ret
fn(T) -> Ret            Function taking T and returning Ret
fn() -> Ret             Function taking no arguments and returning Ret
fn(T1, T2)              Function taking T1, T2 and returning void
```

Internally, a function type value is an opaque pointer (`ptr`) to a closure struct
containing a function pointer and any captured values. Function types can be used as
parameter types, return types, and variable types:

```
let f: fn(int) -> int = &add_one
let g: fn(int, int) -> int = lambda (x: int, y: int) -> int { return x + y }
```

Functions with matching signatures are interchangeable:

```
fn apply_twice(f: fn(int) -> int, x: int) -> int {
    return f(f(x))
}

fn main() -> int {
    return apply_twice(&add_one, 5)     // → 7
}
```

See [Higher-Order Functions](/docs/syntax-functions#higher-order-functions) for details.

### Tuple types

A tuple type is written as a comma-separated list of types inside parentheses:

```
(T1, T2, ...)
```

- `(int, bool)` — a 2-element tuple of an int and a bool.
- `(int8, float32, int)` — a 3-element tuple.

Tuples of two or more elements are distinct from parenthesized grouping:
`(int32)` is just `int32`, not a 1-tuple.

Tuple values are constructed using the same syntax:

```
let pair: (int, bool) = (42, true)
```

Positional access uses `.0`, `.1`, etc.:

```
let first: int = pair.0     // 42
let second: bool = pair.1   // true
```

Tuples are represented as anonymous structs in codegen.

### Type equivalence

Two types are considered the same only when they have the same kind, the same pointer depth,
the same array size (or neither is an array), and (for `Struct`/`Enum`) the same name.
There are no implicit conversions except:

- Integer literals coerce to the expected type when the target type is unambiguous.
- `bool` (`i1`) is zero-extended to wider integer types when needed (e.g. in arithmetic).
- `int64` values are silently truncated to `int32` when assigned to an `int32` variable.
- There are no implicit conversions between signed and unsigned types of different signedness.
- An array `T[n]` converts implicitly to a slice `T[]` when passed to a function parameter
  expecting a slice. The slice's `len` is set to `n` (the array's compile-time size).

## Variables

### Declaration

Variables are declared with `let`, which always requires a type annotation and an initializer:

```
let name: type = expression
```

```
let count: int = 0
let label: string = "hello"
let ptr: int* = &count
let flag: bool = true
let arr: int[3] = [10, 20, 30]
let p: Point = Point { x: 1, y: 2 }
```

### Mutability

Variables are mutable by default. Reassign using `=`:

```
let x: int = 10
x = 20               // OK — x is now 20
```

### Scopes

`let` can appear at the top level of a file (outside any function) or inside a function body
or block `{ }`.

- **Top-level** `let`s are initialized once, before `main` runs. They are visible to all
  functions in the file and across `include` boundaries.
- **Local** `let`s inside a function or block are scoped to that block and are destroyed
  when execution leaves the block.

```
let top_level: int = 100          // top-level, visible everywhere

fn main() -> int {
    let local: int = top_level    // local, scoped to main
    {
        let inner: int = 99       // scoped to this block
        local = inner             // OK — both in scope
    }
    // inner is out of scope here
    return local
}
```

### Shadowing

An inner scope may declare a variable with the same name as one in an outer scope, temporarily
hiding the outer one:

```
let x: int = 1
{
    let x: int = 2        // shadows the outer x
    // x is 2 here
}
// x is 1 again here
```

### Restrictions

- A variable cannot have type `void`.
- Every variable must be initialized at the point of declaration. There is no default
  zero-initialization for local variables (unlike top-level `let`s, which are zeroed if the
  initializer evaluates to zero or is omitted — but omission is not valid syntax; the
  initializer is always required syntactically).

### docs/syntax.md

---
layout: ../../layouts/DocsLayout.astro
title: Syntax Overview
---

# Hokkaido Language Syntax

Hokkaido is a small, explicit, systems-programming language that compiles to native code via LLVM.
It draws inspiration from Rust, C, and modern language design while keeping the feature set
deliberately minimal.

## Contents

- [Comments](#comments)
- [Types](/docs/syntax-types#types)
- [Variables](/docs/syntax-types#variables)
- [Functions](/docs/syntax-functions#functions)
- [Generic functions](/docs/syntax-functions#generic-functions)
- [Trait bounds](/docs/syntax-functions#trait-bounds)
- [Return](/docs/syntax-functions#return)
- [If / Else](/docs/syntax-control-flow#if--else)
- [For loop](/docs/syntax-control-flow#for-loop)
- [Break](/docs/syntax-control-flow#break)
- [Continue](/docs/syntax-control-flow#continue)
- [Match](/docs/syntax-control-flow#match)
- [Operator precedence](/docs/syntax-expressions#operator-precedence)
- [Comparison operators](/docs/syntax-expressions#comparison-operators)
- [Logical operators](/docs/syntax-expressions#logical-operators)
- [Bitwise operators](/docs/syntax-expressions#bitwise-operators)
- [Arithmetic operators](/docs/syntax-expressions#arithmetic-operators) (including `%` modulo)
- [Shift operators](/docs/syntax-expressions#shift-operators)
- [Assignment](/docs/syntax-expressions#assignment)
- [References and Pointers](/docs/syntax-expressions#references-and-pointers)
- [Borrow checking](/docs/syntax-expressions#borrow-checking)
- [Region blocks](/docs/syntax-expressions#region-blocks)
- [Raw pointers](/docs/syntax-expressions#raw-pointers)
- [Atomic operations](/docs/syntax-expressions#atomic-operations)
- [Function calls](/docs/syntax-expressions#function-calls)
- [Arrays](/docs/syntax-data-structures#arrays)
- [Structs](/docs/syntax-data-structures#structs)
- [Generic structs](/docs/syntax-data-structures#generic-structs)
- [Traits](/docs/syntax-data-structures#traits)
- [Impl blocks and method calls](/docs/syntax-data-structures#impl-blocks-and-method-calls)
- [Enums](/docs/syntax-data-structures#enums)
- [Tuple types](/docs/syntax-types#tuple-types)
- [Tuple expressions](/docs/syntax-expressions#tuple-expressions)
- [Package declaration](/docs/syntax-modules#packages)
- [Import](/docs/syntax-modules#import)
- [Visibility (pub)](/docs/syntax-modules#visibility)
- [Namespaces](/docs/syntax-modules#namespaces)
- [Region](/docs/syntax-control-flow#region)
- [Include](/docs/syntax-modules#include-low-level)
- [C FFI](/docs/syntax-ffi-cubical#c-ffi)
- [Freestanding mode](/docs/syntax-ffi-cubical#freestanding-mode)
- [Inline assembly](/docs/syntax-ffi-cubical#inline-assembly)
- [Cubical](/docs/syntax-ffi-cubical#cubical)
- [Cubical Surface Language](/docs/cubical_surface_language)
- [Standard Library](/docs/stdlib)
- [Language Server](/docs/lsp)
- [Package Manager](/docs/package-manager)
- [Example: Number guessing game](/docs/example-guess)
- [Design Decisions](/docs/design-decisions)

## Comments

Hokkaido supports two comment forms:

```
// Line comment — everything from // to the end of the line is ignored.

/* Block comment — can span multiple lines.
   Block comments do NOT nest: the first */ ends the comment
   even if it appears inside what looks like a nested /* ... */ pair. */
```

Line comments (`//`) are the idiomatic choice for most documentation. Block comments (`/* */`)
are useful for temporarily disabling large regions during development.

### otaru/README.md

# otaru — Hokkaido Package Manager & C/C++ Build Tool

otaru is a package manager and project scaffold for the [Hokkaido](https://github.com/hokkaido-lang/hokkaido) compiler, and a drop-in replacement for Make for C/C++ projects.

## Installation

### Nix (flake — recommended)

```bash
nix profile install github:hokkaido-lang/hokkaido          # otaru + hokkaido bundled (default)
nix profile install github:hokkaido-lang/hokkaido#hokkaido # compiler only (optional)
nix develop github:hokkaido-lang/hokkaido                   # dev shell (both + cmake)
```

The Nix package bundles the hokkaido compiler and std library — no extra setup needed.

### Nix (traditional)

```bash
nix-build -E 'with import <nixpkgs> {}; callPackage ./otaru/default.nix {}'
```

### Cargo

```bash
cargo install --path otaru
```

With `cargo install`, you need the hokkaido compiler on `PATH` or `HOKKAIDO_HOME`.

## Prerequisites

| Component | Required for | Notes |
|-----------|-------------|-------|
| **hokkaido compiler** | Hokkaido projects | On `PATH`, in `HOKKAIDO_HOME`, or bundled (Nix) |
| **C/C++ compiler** | C/C++ projects | Defaults to `cc`; override with `compiler = "gcc"` etc. |
| **clang** | Hokkaido linking | Default linker for Hokkaido projects |
| **ar** | Static libraries | Only needed for `type = "staticlib"` |

## Commands

### Core

| Command | Description |
|---------|-------------|
| `otaru new <name>` | Scaffold a new project (Hokkaido by default) |
| `otaru build` | Build the project (auto-detects Hokkaido or C/C++) |
| `otaru build --release` / `-r` | Build with `-O2` optimizations |
| `otaru build -f` | Force rebuild, ignoring cache |
| `otaru run` | Build and run |
| `otaru run --release` | Build with optimizations and run |
| `otaru clean` | Remove the `build/` directory |
| `otaru exec` | List all scripts defined in `[scripts]` |
| `otaru exec <name>` | Run a named shell script |
| `otaru exec <name> <args>` | Run a script with arguments ($1, $2, etc.) |

### Hokkaido-specific

| Command | Description |
|---------|-------------|
| `otaru build <file.hk>` | Compile a single `.hk` file |
| `otaru build --freestanding` | Build without CRT/libc (ELF entry point) |
| `otaru run <file.hk>` | Compile and run a single file |
| `otaru add <name> --git <url>` | Add a git dependency |
| `otaru add <name> --path <path>` | Add a local dependency |
| `otaru install` | Clone/fetch all dependencies |

### C/C++-specific

| Command | Description |
|---------|-------------|
| `otaru build <file.c>` | Compile a single C file |
| `otaru build <file.cpp>` | Compile a single C++ file |
| `otaru build --target <name>` | Build a specific target (multi-target projects) |
| `otaru run` | Build and run the C/C++ executable |

### Project auto-detection

otaru auto-detects the project type based on `otaru.toml` and source files:

| Condition | Build path |
|-----------|-----------|
| `.hk` files in `src/` | Hokkaido compiler (also reads `[build]` for linking if present) |
| `[build]` section, no `.hk` files | C/C++ compiler |
| `[build]` section with `.hk` files | Hokkaido compiler + C compiler (mixed build) |
| Neither | Error |

## Quick Start — Hokkaido

```bash
otaru new myapp
cd myapp

cat > src/main.hk << 'EOF'
import "std"

extern fn putchar(c: int) -> int

fn double(x: int) -> int {
    return x * 2
}

fn main() -> int {
    let x: int = std::twice(&double, 5)
    putchar(48 + x / 10)
    putchar(48 + x % 10)
    putchar(10)
    return x
}
EOF

otaru run
```

### Single-file workflow

```bash
cat > hello.hk << 'EOF'
extern fn putchar(c: int) -> int
fn main() -> int {
    putchar(72); putchar(105); putchar(10)
    return 0
}
EOF
otaru run hello.hk
```

### Freestanding mode

```bash
otaru build kernel.hk --freestanding
# Produces build/kernel.o — link with ld.lld manually
```

## Quick Start — C Project

```bash
mkdir mylib && cd mylib

cat > otaru.toml << 'EOF'
[package]
name = "mylib"
version = "0.1.0"

[build]
type = "staticlib"
sources = ["src/*.c"]
include_dirs = ["include"]
cflags = ["-Wall", "-Wextra"]
EOF

mkdir -p src include

cat > include/mylib.h << 'EOF'
#ifndef MYLIB_H
#define MYLIB_H
int add(int a, int b);
#endif
EOF

cat > src/mylib.c << 'EOF'
#include "mylib.h"
int add(int a, int b) { return a + b; }
EOF

otaru build
# Produces build/libmylib.a
```

## Quick Start — Hokkaido + C FFI

```bash
mkdir myapp && cd myapp

cat > otaru.toml << 'EOF'
[package]
name = "myapp"
version = "0.1.0"

[build]
link = ["m"]
EOF

mkdir -p src

cat > src/main.hk << 'EOF'
extern fn sin(x: double) -> double
extern fn putchar(c: int) -> int

fn main() -> int {
    let pi: double = 3.14159265358979
    let s: double = sin(pi / 2.0)
    # print integer part of s (should be 1)
    putchar(48 + int(s))
    putchar(10)
    return 0
}
EOF

otaru run
```

## Project Structure

### Hokkaido project

```
myapp/
├── otaru.toml         # Manifest (package, deps, optional [build])
├── hk.mod             # Marks the package root
├── std/               # Standard library (prepared by otaru new)
│   ├── hk.mod
│   └── hof.hk
└── src/
    └── main.hk        # Entry point
```

### C/C++ project

```
mylib/
├── otaru.toml         # Manifest with [build] section
├── include/
│   └── mylib.h        # Public headers
└── src/
    ├── foo.c
    └── bar.c
```

### Multi-target C/C++ project

```
hokkaido/
├── otaru.toml
└── src-cpp/
    ├── main.cpp
    ├── lexer.cpp
    └── lsp/
        └── lsp.cpp
```

### Mixed Hokkaido + C project

```
myapp/
├── otaru.toml         # [build] section for linking and/or C sources
├── hk.mod
├── std/
└── src/
    ├── main.hk        # Hokkaido source
    ├── ffi.c          # C glue code (compiled by cc)
    └── helpers.c
```

## Configuration

### `otaru.toml` — Hokkaido project

```toml
[package]
name = "myapp"
version = "0.1.0"

[dependencies]
mylib = { git = "https://github.com/user/mylib" }
other = { path = "../other" }
```

### `otaru.toml` — C/C++ project (single target)

```toml
[package]
name = "myapp"
version = "0.1.0"

[build]
type = "executable"          # executable | staticlib | sharedlib | object
sources = ["src/*.c"]        # glob patterns for source files
include_dirs = ["include"]   # -I directories
compiler = "cc"              # cc | gcc | clang | c++ | g++ | clang++
cflags = ["-Wall", "-Wextra"]
ldflags = ["-L/usr/local/lib"]
link = ["m", "pthread"]      # system libraries → -lm -lpthread
libraries = ["libfoo.a"]     # explicit library files (auto-resolved)
lib_dirs = ["vendor/lib"]    # custom library search paths
```

### `otaru.toml` — C/C++ project (multi-target)

```toml
[package]
name = "hokkaido"
version = "0.1.0"

[build.targets.hokkaido]
type = "executable"
sources = ["src-cpp/main.cpp", "src-cpp/cubical.cpp", "src-cpp/lexer.cpp",
           "src-cpp/parser.cpp", "src-cpp/codegen.cpp"]
include_dirs = ["src-cpp"]
link = ["LLVM", "pthread", "dl", "m"]
cflags = ["-std=c++17"]
libraries = ["target/release/libcubical_c.a"]

[build.targets.hok-lsp]
type = "executable"
sources = ["src-cpp/lsp_main.cpp", "src-cpp/lsp/lsp.cpp",
           "src-cpp/lexer.cpp", "src-cpp/parser.cpp"]
include_dirs = ["src-cpp"]
link = ["LLVM", "pthread", "dl", "m"]
cflags = ["-std=c++17"]
```

Build with: `otaru build` (all targets) or `otaru build --target hokkaido` (one target).

### `otaru.toml` — Hokkaido + C FFI

```toml
[package]
name = "myapp"
version = "0.1.0"

[build]
sources = ["src/ffi.c"]                        # optional C glue code
include_dirs = ["include"]
link = ["gtk4", "glib-2.0", "gobject-2.0"]     # -l flags
ldflags = ["-L/usr/local/lib"]                 # extra linker flags
lib_dirs = ["/usr/local/lib"]                  # library search paths
cflags = ["-Wall"]
```

When `.hk` files exist in `src/`, otaru compiles them with the Hokkaido compiler and
links the result together with any C object files and external libraries.

## `[build]` Reference

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `type` | string | `"executable"` | Build output type |
| `sources` | string[] | `[]` | Source file glob patterns |
| `include_dirs` | string[] | `[]` | Header search paths (`-I`) |
| `compiler` | string | `"cc"` | C/C++ compiler binary |
| `cflags` | string[] | `[]` | Extra compiler flags |
| `ldflags` | string[] | `[]` | Extra linker flags |
| `link` | string[] | `[]` | System library names (`-l` flags) |
| `libraries` | string[] | `[]` | Explicit library file paths |
| `lib_dirs` | string[] | `[]` | Library search paths (`-L`) |
| `targets` | table | — | Named sub-targets (multi-target) |
| `prebuild` | string | — | Shell command to run before compiling |
| `llvm-config` | string | — | Path to `llvm-config` binary |
| `llvm-components` | string[] | — | LLVM components (e.g. `["core", "support"]`) |

### `[scripts]` Reference

| Field | Type | Description |
|-------|------|-------------|
| `<name>` | string | Shell command to execute |

- Arguments: `$1`, `$2`, ... are replaced with positional args from `otaru exec`
- Multi-line: use triple-quoted strings (`"""..."""`) for complex commands
- Run with: `otaru exec <name>` or `otaru exec` to list all

### Build types

| Type | Output | Command |
|------|--------|---------|
| `executable` | `build/<name>` | `cc ... -o build/<name>` |
| `staticlib` | `build/lib<name>.a` | `ar rcs build/lib<name>.a ...` |
| `sharedlib` | `build/lib<name>.so` | `cc -shared ... -o build/lib<name>.so` |
| `object` | `build/<stem>.o` | `cc -c <source> -o build/<stem>.o` |

## Scripts

Define named shell commands in `[scripts]` and run them with `otaru exec`:

```toml
[scripts]
clean = "rm -rf build"
test = "./build/myapp --test"
fmt = "clang-format -i src/*.c"
lint = "cargo clippy"
bench = "./build/bench --iterations 1000"
```

```bash
otaru exec              # list all defined scripts
otaru exec test         # run the "test" script
otaru exec fmt          # run the "fmt" script
```

### Argument substitution

Scripts support positional arguments via `$1`, `$2`, etc.:

```toml
[scripts]
greet = "echo Hello, $1!"
```

```bash
otaru exec greet World        # prints: Hello, World!
otaru exec greet Alice        # prints: Hello, Alice!
```

### Combining with build commands

Scripts can call other otaru commands or arbitrary shell:

```toml
[scripts]
all = "otaru build --release && otaru exec test"
release = "otaru build --release"
deploy = """
otaru build --release &&
otaru exec test &&
cp build/myapp /usr/local/bin/
echo 'Deployed!'
"""
```

Multi-line strings (triple-quotes) are supported for complex scripts.

## Library Resolution

Libraries in `libraries = [...]` are resolved in this order:

1. **Absolute path** — used directly if the file exists
2. **Relative path** (contains `/`) — resolved against project root
3. **Bare name** (e.g., `"foo"`) — searched in:
   - `lib_dirs` paths
   - `/usr/lib/`, `/usr/local/lib/`, `/lib/`, `/lib64/`
   - `LIBRARY_PATH` environment variable entries
   - `LD_LIBRARY_PATH` environment variable entries

If not found, the name is passed to the linker as-is (equivalent to `-lfoo`).

Libraries in `link = [...]` are always passed as `-l<name>` flags, letting the linker
handle the search via its default paths and `-L` flags.

## LLVM Discovery

For projects linking against LLVM (compilers, language tools), use `llvm-config` to
auto-resolve include dirs, compiler flags, and library flags:

```toml
[build]
llvm-config = "llvm-config-21"              # or full path
llvm-components = ["core", "support", "irreader", "codegen", "mc", "mcparser"]
link = ["pthread", "dl", "m"]
cflags = ["-std=c++17"]
```

otaru runs `llvm-config --cxxflags` to get include dirs and compiler flags, then
`llvm-config --ldflags --libs <components>` to get linker flags and library names.
The resolved flags are merged into your `[build]` configuration automatically.

### Component naming

LLVM component names match the CMake `llvm_map_components_to_libnames` syntax:
- Core components: `support`, `core`, `irreader`, `codegen`, `target`, `mc`, `mcparser`, `asmparser`, `option`
- Target backends: `X86`, `AArch64`, `ARM`, `WebAssembly`, `RISCV`, `Mips`, etc.

Use `llvm-config --components` to list all available components.

## Prebuild Steps

Run a shell command before compiling (e.g., to build a Rust static library):

```toml
[build]
prebuild = "cargo build --release"

[[build.targets]]
name = "mycompiler"
type = "executable"
sources = ["src/main.cpp"]
libraries = ["target/release/libmylib.a"]
```

The prebuild command runs once before building each target. Use this for:
- `cargo build --release` — build a Rust static library
- `make -C vendor` — build an external dependency
- `python generate.py` — code generation
- `flex parser.l` / `bison grammar.y` — parser generation

## Incremental Builds

### Hokkaido

Cache keys are computed from modification times and sizes of:
- All `.hk` files in `src/`, `deps/`, and `std/`
- The `hokkaido` compiler binary

Cache files: `build/.hkbuildcache.{debug,release}`.

### C/C++

Object files are recompiled only when their source file or any included header is newer.
This uses `.d` dependency files generated via `-MMD -MF`.

Both builds have separate debug/release caches. Use `-f` / `--force` to bypass caching.

### Optimization

| Mode | Flag | Compiler flag | Use case |
|------|------|--------------|----------|
| Debug (default) | *(none)* | `-O0` | Fast compilation, no optimizations |
| Release | `--release` / `-r` | `-O2` | Optimized binary, slower compilation |

## Hokkaido + C FFI (details)

### Linking with external C libraries

Declare `extern fn` in your `.hk` code to call C functions. Then list the libraries
in `[build]`:

```toml
[build]
link = ["gtk4", "glib-2.0", "gobject-2.0"]
ldflags = ["-L/usr/local/lib"]
lib_dirs = ["/usr/local/lib"]
```

```ocaml
extern fn gtk_init(argc: *int, argv: **char) -> int
extern fn gtk_window_new() -> *void
extern fn gtk_widget_show(window: *void)

fn main() -> int {
    let argc: int = 0
    gtk_init(&argc, 0 as **char)
    let win = gtk_window_new()
    gtk_widget_show(win)
    return 0
}
```

The resulting link command:
```
clang build/main.o -o build/myapp -L/usr/local/lib -lgtk4 -lglib-2.0 -lgobject-2.0
```

### Compiling C glue code alongside Hokkaido

For complex C APIs, write wrapper code in C and list it in `sources`:

```toml
[build]
sources = ["src/ffi.c", "src/helpers.c"]
include_dirs = ["include"]
link = ["gtk4", "glib-2.0"]
cflags = ["-Wall", "-Wno-unused-parameter"]
```

otaru compiles `.hk` files with the Hokkaido compiler and `.c` files with `cc`,
then links everything together in a single link step.

## Environment Variables

| Variable | Purpose | Used by |
|----------|---------|---------|
| `HOKKAIDO_HOME` | Directory containing the `hokkaido` binary | Hokkaido compiler lookup |
| `HOKKAIDO_CRT_DIR` | C runtime directory for hokkaido | Hokkaido compiler |
| `HOKKAIDO_DYNAMIC_LINKER` | Dynamic linker path for hokkaido | Hokkaido compiler |
| `LIBRARY_PATH` | Extra library search paths | C/C++ linking (passed to linker) |
| `LD_LIBRARY_PATH` | Extra shared library search paths | C/C++ library resolution |
| `PATH` | System binary search path | Compiler and hokkaido lookup |

### src-cpp/lsp/README.md

# hok-lsp — Hokkaido Language Server

`hok-lsp` is an LSP (Language Server Protocol) implementation for the Hokkaido programming language, providing IDE features for editors that support LSP (VS Code, Neovim, Emacs, Helix, etc.).

## Features

- **Diagnostics** — parse errors reported inline as you type
- **Hover** — shows symbol kind and name on hover
- **Completion** — keyword suggestions + symbols from all open documents
- **Go to Definition** — navigate to symbol declarations
- **Find References** — find all occurrences of a symbol in the current file
- **Document Symbols** — outline of functions, variables, structs, enums, traits
- **Incremental Parsing** — after the first full parse, edits re-parse only the affected portion of the file

## Building

Built as part of the Hokkaido CMake project:

```sh
mkdir build && cd build
cmake .. && make hok-lsp
```

Or with Nix (installed to `$out/bin/hok-lsp`):

```sh
nix build github:hokkaido-lang/hokkaido
```

## Usage

### Neovim (via `vim.lsp`)

```lua
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'hk',
  callback = function()
    vim.lsp.start({
      name = 'hok-lsp',
      cmd = { 'hok-lsp' },
    })
  end,
})
```

### Helix

Add to `~/.config/helix/languages.toml`:

```toml
[[language]]
name = "hokkaido"
scope = "source.hk"
file-types = ["hk"]
language-servers = ["hok-lsp"]

[language-server.hok-lsp]
command = "hok-lsp"
```

### VS Code

Create a simple extension or use a generic LSP client:

```json
{
  "languages": [{
    "id": "hokkaido",
    "extensions": ["hk"]
  }],
  "languageServer": {
    "command": "hok-lsp"
  }
}
```

## Protocol

Communicates over **stdio** using the standard LSP JSON-RPC transport (`Content-Length` headers).

Supports `textDocument/didOpen`, `didChange`, `didClose`, `hover`, `completion`, `definition`, `references`, and `documentSymbol`.

## Architecture

- **`lsp.h`** — type definitions (`LSPPosition`, `LSPRange`, `LSPDiagnostic`, `LSPSymbol`, `LSPCompletionItem`, `DeclRange`, `LSPDocument`) and `LSPServer` class
- **`lsp.cpp`** — JSON-RPC message framing, protocol handlers, symbol index construction, incremental parsing
- **`lsp_main.cpp`** — entry point that instantiates `LSPServer` and calls `run()`

