# TODO

## Compiler

### ~~Generics~~ ✅ DONE
- Generic functions: `fn max<T>(a: T, b: T) -> T` ✅
- Generic structs: `struct Pair<A, B> { first: A, second: B }` ✅
- Monomorphization in codegen (like Rust/C++) ✅
- Trait bounds: `fn sort<T: Ord>(arr: T[], len: int)` — parsed but not enforced yet
- Direct syntax `fn<T>(args)` and turbofish `fn::<T>(args)` both work ✅
- Constructor type arg inference (no need to write `Pair<int, int> { ... }`) ✅

### ~~Better Error Messages~~ ✅ DONE
- Source spans with underline + source line display ✅
- Suggestion hints on common errors ✅
- Colored output (ANSI escape codes, auto-detects terminal) ✅
- `NO_COLOR` env var support ✅

### ~~NLL Borrow Checker~~ ✅ DONE
- Replace lexical borrows with non-lexical lifetimes ✅
- Allow borrows to end at last use, not end of scope ✅
- Support split borrows: `let (head, tail) = split_slice(&mut arr)`
- Remove need for `let x; x = ...` workarounds

## Standard Library

### String Operations
- `str_len(s: string) -> int`
- `str_eq(a: string, b: string) -> bool`
- `str_concat(a: string, b: string) -> string`
- `str_slice(s: string, start: int, end: int) -> string`
- `str_contains(haystack: string, needle: string) -> bool`
- `str_split(s: string, delim: string) -> ???` (needs generics/iterators first)
- `str_to_int(s: string) -> int` / `int_to_str(n: int) -> string`

### Collections
- `struct Vec<T>` — dynamic array (push, pop, get, len, capacity)
- `struct HashMap<K, V>` — hash map (get, insert, remove, contains_key)
- `struct Opt<T>` — option type (some, none, unwrap, is_some, is_none)
- `struct Result<T, E>` — result type (ok, err, unwrap, is_ok)

### Sorting & Searching
- `sort_int(arr: int[], len: int)` — O(n log n) quicksort/mergesort
- `sort_by(arr: T[], len: int, cmp: fn(T, T) -> int)` — comparator-based sort
- `binary_search_int(arr: int[], len: int, target: int) -> int` — O(log n)
- `binary_search_by(arr: T[], len: int, target: T, cmp: fn(T, T) -> int) -> int`

### Math
- `abs(x: int) -> int`
- `min(a: int, b: int) -> int` / `max(a: int, b: int) -> int`
- `pow(base: int, exp: int) -> int` — O(log n) exponentiation
- `sqrt(x: float) -> float`

### Iterators (needs generics ✅ + closures)
- `struct Iter<T>` with `next() -> Opt<T>`
- `fn iter_range(start: int, end: int) -> Iter<int>`
- Chaining: `.map(f).filter(g).collect()`

## Sapporo / WASM
- Hot reload for WASM dev server
- Richer DOM abstractions (reactive state, not just get/set)
- Component model for composable widgets

## Tooling
- `hok check` — type-check without codegen
- `hok fmt` — auto-formatter
- LSP: AST-based references (not text search)
- LSP: Cross-file symbol resolution
