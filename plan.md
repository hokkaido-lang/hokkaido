# Hokkaido Language — Feature Implementation Roadmap

This document lays out a phased plan for closing the gaps identified in the current
Hokkaido syntax reference. Phases are ordered by (a) how foundational the feature is
for other work, and (b) implementation risk/cost.

---

## Phase 0 — Groundwork & Decisions ✅ Complete

Before writing any code, resolve open design questions that later phases depend on.

- [x] Decide on unsigned integer semantics: new distinct types (`uint8/16/32/64`) vs.
      a signedness flag on existing `int` types. Affects codegen, comparisons, and
      literal coercion rules.
- [x] Decide whether dynamic memory will be a language feature (`alloc`/`free` builtins)
      or remain purely an FFI convention (`extern fn malloc`). This decision gates the
      slice/dynamic-array design in Phase 3.
- [x] Decide whether generics will support trait-like bounds now or defer to a later
      major version. This decision gates Phase 5 scope.
- [x] Decide whether `if` will remain a statement or become an expression, and how to
      handle the transition. See [design-decisions.md](docs/design-decisions.md#4-if-as-statement-vs-expression).
- [x] Freeze grammar changes for Phase 1–2 in an updated BNF before implementation, to
      avoid rework in the parser.

**Exit criteria:** a short design-decisions doc, reviewed and merged, that each later
phase can cite. → See [`docs/design-decisions.md`](docs/design-decisions.md).

---

## Phase 1 — Core Type System Gaps ✅ Complete

Low-risk, high-value, mostly additive to the type checker and codegen; unblocks
correctness issues elsewhere.

1. ✅ **`int16` type**
   - Added to lexer/type table alongside existing `int8/32/64`.
   - Extend truncation/coercion rules in the type-equivalence section.
2. ✅ **Unsigned integer types (`uint8/16/32/64`)**
   - Added signedness to the type representation.
   - Updated arithmetic codegen to select signed vs. unsigned LLVM instructions
     (`sdiv`/`udiv`, `srem`/`urem`, `ashr`/`lshr`).
   - Updated comparison operators to use unsigned predicates for unsigned operands.
3. ✅ **`char` type**
   - Introduced as a distinct 1-byte type with its own literal syntax (`'a'`).
   - Added character literal lexing (`'a'`, `'\n'`, `'\t'`, `'\\'`, `'\''`).
4. ✅ **Tuple types**
   - Grammar: `(T1, T2, ...)` as a type; `(a, b, c)` as a value.
   - Represented as an anonymous struct in codegen (mirrors existing struct layout rules).
   - Added positional access (`.0`, `.1`, ...) reusing the field-access codegen path.

**Exit criteria:** all four land with tests; no changes required to control flow or
functions yet.

---

## Phase 2 — Control Flow Correctness & Ergonomics ✅ Complete

Focused on closing known correctness holes and small ergonomic wins in
`docs/syntax-control-flow.md`.

1. ✅ **`match` exhaustiveness checking**
   - Compile-time warning when not all enum variants are covered.
   - Emitted in `codegen_expr.cpp` at the else branch of match codegen.
2. ✅ **Labeled `break`/`continue`**
   - Grammar: `'label: for ...`, `break 'label`, `continue 'label`.
   - `'label:` prefix consumed in `parse_stmt()`, label stored in `ForStmt::label`.
   - `break 'label` / `continue 'label` parsed in `parse_break_stmt` / `parse_continue_stmt`.
   - Codegen match loop by label in `gen_break_stmt` / `gen_continue_stmt`.
3. ✅ **`if` as an expression**
   - `let x: T = if cond { a } else { b }` supported via `parse_if_expr()` in `parse_primary`.
   - Codegen uses alloca + PHI pattern in `eval_expr`.
   - Parser supports `else if` chaining recursively.

**Exit criteria:** all three items implemented and tested; `test/test_phase2.hk` covers
if-expressions, labeled break/continue, and chained else-if.

---

## Phase 3 — Memory & Data Structures ✅ Complete

Depends on the Phase 0 decision about dynamic memory.

1. ✅ **Slices (`T[]`)**
   - A fat pointer: `{ ptr: T*, len: int64 }` in LLVM, represented as `TypeKind::Slice` in AST.
   - Support array-to-slice conversion at function call boundaries (automatic conversion from
     `T[n]` to `T[]` when passing to a slice parameter).
   - Support slice indexing (`s[i]`) with the same semantics as array indexing via GEP on the
     extracted pointer.
   - Slice element type stored in `tuple_types[0]` of the `TypeAnnotation`.
2. ✅ **Dynamic allocation via extern fn (stdlib convention)**
   - `alloc`/`free` are not special syntax — users declare `extern fn malloc` / `extern fn free`
     from the C standard library and work with raw pointers (`int8*`, `T*`).
   - Slice creation from heap memory is done manually by storing a pointer and tracking length
     (or via future stdlib wrappers).
   - Contextual keyword detection and AllocExpr/FreeExpr AST nodes were implemented and then
     removed in favor of the simpler extern-fn approach.
3. ✅ **Named and positional field initializers for structs/enums**
   - Grammar: `Point { x: 10, y: 20 }` (named) and `Point { 10, 20 }` (positional) both accepted.
   - Mixing named and positional fields within a single constructor is a parse error.
   - Positional fields store empty name strings in `ConstructorExpr::fields`; codegen maps
     them by index at runtime.

**Exit criteria:** slices implemented and documented; struct/enum literals accept
both named and positional fields; dynamic allocation path (`alloc`/`free`) works
end-to-end. Covered by `test/test_phase3.hk` (9 test functions).

---

## Phase 4 — Functions & Abstraction ✅ Complete

Higher-risk phase; touches calling conventions and possibly closures' capture
semantics.

1. ✅ **Closures**
   - By-value capture only (no borrow checker for v1).
   - `lambda` keyword syntax: `lambda (x: T) -> R { body }`.
   - Closure = struct with `{ i8* fn_ptr, captures... }` — the helper function
     takes `(i8* captures_ctx, T1, T2, ...) -> R`.
   - Captures discovered by walking the body, stored inline in the closure struct.
   - Call site: alloca a copy of the closure for stable context pointer, extract
     fn_ptr, bitcast, call.
2. ❌ **Operator overloading** — explicitly deferred until after trait bounds land.

**Exit criteria:** ✅ closures with value-capture ship with tests covering capture,
call, and interaction with existing generic functions. Covered by `test/test_phase4.hk`
(5 test functions, exit 80).

---

## Phase 5 — Generics Expansion

Builds directly on the Phase 0 decision about trait bounds.

### Phase 5a — Generic Structs ✅ Complete

1. ✅ **Generic structs**
   - `struct Foo<T> { field: T }` syntax with `<T, U>` type params.
   - `Foo<int>` instantiates via monomorphization: substitutes type params in field
     types, creates a unique LLVM struct type keyed by mangled name (`Foo$i64`).
   - Supports multiple type params, nested generics (`Pair<Pair<int64>>`), and
     field access through monomorphized types.
   - `>>` (nested `> >`) handled for `Pair<Pair<T>>` syntax.
   - Generic structs as field types in non-generic structs works.
   - Covered by `test/test_phase5.hk` (5 test functions).

### Phase 5b — Type Parameter Bounds & Traits (Not Yet Started)

2. ❌ **Type parameter bounds/constraints**
   - Minimal trait-like mechanism: a bound declares a set of required function
     signatures a type must implement.
   - Includes: `trait` declaration, `impl` blocks, trait bounds `<T: Display>`,
     and dispatch (static via monomorphization, or vtable-based).
   - This is the largest-scope item in the roadmap; split into its own sub-roadmap.

**Exit criteria (Phase 5a ✅):** generic containers (e.g., a generic `Pair<A, B>`
or `Option<T>`) work end-to-end with angle-bracket instantiation, matching the
existing generic-function calling convention for consistency.

---

## Phase 6 — Standard Library & Documentation

Can run in parallel with Phases 3–5 once slices and dynamic allocation land.

1. Minimal string utilities (length, concat, compare) — currently everything routes
   through C FFI (`printf`, etc.) with no native string manipulation.
2. Minimal collection built on slices + Phase 3 allocation (growable array / "vec").
3. Doc-comment syntax (`///`) distinct from the existing `//` and `/* */` forms, plus
   a basic doc-extraction tool, so public API surfaces (`pub fn`, `pub struct`, ...)
   can be documented consistently.

**Exit criteria:** a small but real standard library module exists and is used by an
updated version of the guessing-game example (`docs/example-guess.md`) to demonstrate it
alongside the existing C FFI example.

---

## Phase 7 — Cubical Subsystem Follow-ups

Independent track; can be staffed separately since it touches the Rust cubical
backend rather than the main Hokkaido compiler.

1. Module/import mechanism for `.cub` files, mirroring the host language's
   `import`/`package` model at a small scale (even just `include`-style textual
   inclusion would remove the current "everything must be one file or one inline
   string" constraint).
2. Named projections for Σ-types, to avoid long `fst (snd (fst p))` chains once
   records nest.
3. Document the diagnostic/error-reporting behavior of the cubical backend — currently
   unspecified in `docs/syntax-ffi-cubical.md`.

**Exit criteria:** at least the import mechanism ships; the other two items can be
tracked as follow-on tickets.

---

## Suggested Sequencing Summary

| Phase | Focus | Depends on | Risk |
|-------|-------|------------|------|
| 0 | Design decisions | — | Low |
| 1 | Type system gaps | 0 | Low |
| 2 | Control flow fixes ✅ | — | Low–Medium |
| 3 | Memory & data structures ✅ | 0 | Medium |
| 4 | Closures ✅ | 3 (loosely) | Medium |
| 5a | Generic structs ✅ | 0, 1 | High |
| 5b | Traits/bounds ❌ | 5a | High |
| 6 | Stdlib & docs | 3 | Low |
| 7 | Cubical follow-ups | — | Medium |

Phases 1, 2, and 7 can proceed largely in parallel once Phase 0 concludes. Phases 3
through 6 are best sequenced, since each builds on allocation/slice primitives
introduced in Phase 3.