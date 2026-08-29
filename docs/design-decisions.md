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

**Decision:** Implement a non-lexical lifetime (NLL) borrow checker with
per-variable borrow tracking, enforcing standard Rust-like aliasing rules:
- One mutable reference XOR any number of shared references.
- The original value is frozen (no read/write) while any borrow is active.
- Borrows end at their last use point (not end of scope).

The checker is a standalone AST-walking pass, run before code generation. It
operates on `BorrowExpr` and `DerefExpr` AST nodes and `&T`/`&mut T` reference
types.

### Rationale

| Option | Pros | Cons |
|--------|------|------|
| Lexical borrow checker | Simple implementation; fast (single pass); predictable error messages; matches Rust's borrow scoping for v1 | Conservative — borrows last until end of scope, not until last use (NLL); no support for reborrowing patterns |
| **Non-lexical lifetimes (NLL)** | More precise; shorter borrow ranges; fewer false positives; borrows end at last use | Requires dataflow analysis (CFG + liveness) — more complex but still tractable |
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

- `NLLBorrowChecker` class with CFG construction (`cfg.h`/`cfg.cpp`), iterative
  liveness analysis, and per-borrow lifetime tracking.
- **CFG Builder**: Flattens function bodies into basic blocks with edges for
  branches, loops, and fall-through. Each node tracks gen/kill sets.
- **Liveness Analysis**: Iterative backward dataflow on the CFG. Computes
  `live_in`/`live_out` for each node — which variables are live (will be used).
- **Borrow Lifetimes**: For `let r: &T = &x`, the borrow's lifetime is the
  liveness range of `r` (the reference variable). For inline borrows
  (`foo(&x)`), the borrow ends at the creation node.
- **Borrow Checking**: For each CFG node, checks reads/writes against active
  borrow regions. The borrow's creation node is excluded (creating a borrow
  reads the variable — that's fine). Overlapping mutable borrows are rejected.
- Defined in `src-cpp/borrow_checker.h`, `src-cpp/borrow_checker.cpp`,
  `src-cpp/cfg.h`, and `src-cpp/cfg.cpp`.
- Integrated into the compilation pipeline in `main.cpp` — run on all
  non-extern, non-generic function declarations before codegen.

### Impact on later phases

- Generics (Phase 5): the borrow checker currently skips generic function
  declarations. Once monomorphization produces concrete instances, those
  instances should be checked.
- Closures (Phase 4): closures capture by value; borrow state save/restore is
  already implemented. Lexical closures that capture references (if added later)
  will need deeper borrow tracking through closure calls.
- NLL: ✅ DONE — the borrow checker uses CFG-based liveness analysis.
  Borrows end at last use, not end of scope.

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
| 10 | Borrow checker | NLL borrow checker with per-variable borrow tracking, enforcing: one mutable XOR many shared; owner frozen while borrowed; borrows end at last use (not end of scope). Implemented via CFG construction, iterative liveness analysis, and per-borrow lifetime tracking | Compile-time data race and use-after-free prevention for reference types (`&T`/`&mut T`). Independent of region lifetime tracking — regions handle scoped allocation safety, borrow checker handles aliasing discipline. |
