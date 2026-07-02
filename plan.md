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

## Phase 3 — Memory & Data Structures

Depends on the Phase 0 decision about dynamic memory.

1. **Slices (`T[]` or similar)**
   - A fat pointer: `{ ptr: T*, len: int }`.
   - Support array-to-slice conversion (extending existing array-to-pointer decay).
   - Support slice indexing with the same semantics as array indexing.
   - This directly addresses the "arrays must have compile-time constant size"
     limitation and gives a safe(r) way to pass array data to functions.
2. **Dynamic allocation convention or builtin**
   - If Phase 0 chose builtins: implement `alloc<T>(n)` / `free(ptr)` as compiler
     intrinsics that call the platform allocator.
   - If Phase 0 chose FFI-only: instead ship a documented idiom/stdlib snippet
     wrapping `malloc`/`free`, and add it to the standard library (see Phase 6).
3. **Named field initializers for structs/enums**
   - Grammar: `Point { x: 10, y: 20 }` as an alternative to positional
     `Point { 10, 20 }`, both accepted.
   - Purely a parser/desugaring change (reorder to positional order internally);
     no codegen impact.

**Exit criteria:** slices implemented and documented; struct/enum literals accept
named fields; dynamic allocation path chosen and at least minimally usable.

---

## Phase 4 — Functions & Abstraction

Higher-risk phase; touches calling conventions and possibly closures' capture
semantics.

1. **Closures**
   - Design capture semantics first: by-value copy only (simplest, consistent with
     Hokkaido's "structs are copied by value" philosophy) vs. by-reference capture.
     Recommend by-value-only for v1 to avoid lifetime analysis.
   - Represent a closure as a struct containing captured values + a function pointer,
     similar to existing struct/pointer machinery — no new runtime needed.
2. **Operator overloading (optional/deferred)**
   - Lowest priority in this phase; consider deferring entirely, since it interacts
     with generics and trait bounds (Phase 5). Flag as "not yet scheduled" rather than
     committing a design now.

**Exit criteria:** closures with value-capture ship with tests covering capture,
call, and interaction with existing generic functions. Operator overloading remains
explicitly out of scope until Phase 5 lands.

---

## Phase 5 — Generics Expansion

Builds directly on the Phase 0 decision about trait bounds.

1. **Generic structs and enums**
   - Extend the existing monomorphization machinery (already used for generic
     functions) to type declarations.
   - Update the "Current limitations" section of `docs/syntax-functions.md` once this
     lands, since it explicitly calls out this gap today.
2. **Type parameter bounds/constraints** (only if Phase 0 opted in)
   - Minimal trait-like mechanism: a bound declares a set of required function
     signatures a type must implement.
   - This is the largest-scope item in the roadmap; consider splitting into its own
     sub-roadmap if it proceeds.

**Exit criteria:** generic containers (e.g., a generic `Pair<A, B>` or `Option<T>`)
work end-to-end with turbofish instantiation, matching the existing generic-function
calling convention for consistency.

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
| 3 | Memory & data structures | 0 | Medium |
| 4 | Closures | 3 (loosely) | Medium |
| 5 | Generic types/bounds | 0, 1 | High |
| 6 | Stdlib & docs | 3 | Low |
| 7 | Cubical follow-ups | — | Medium |

Phases 1, 2, and 7 can proceed largely in parallel once Phase 0 concludes. Phases 3
through 6 are best sequenced, since each builds on allocation/slice primitives
introduced in Phase 3.