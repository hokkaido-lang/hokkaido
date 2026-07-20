---
layout: ../../layouts/DocsLayout.astro
title: Standard Library
---

# Standard Library

Hokkaido's standard library (`std`) provides four modules: `functional.hk` for
composition and fold, `predicate.hk` for array predicates, `transform.hk` for
mapping and aggregation, and `mem.hk` for safe memory operations.

Import the standard library with `import "std"`:

```
import "std"
```

All standard library functions are accessed via the `std::` prefix.

## Functional (`std/functional.hk`)

Composition and fold utilities for working with `int` values.

```
import "std"

fn add_one(x: int) -> int { return x + 1 }
fn add(x: int, y: int) -> int { return x + y }

fn main() -> int {
    let r1: int = std::twice(&add_one, 5)              // 7
    let r2: int = std::thrice(&add_one, 5)             // 8
    let r3: int = std::compose_apply(&add_one, &add_one, 5)  // 7
    let r4: int = std::apply_n(&add_one, 10, 5)        // 15

    let arr: int[5] = [1, 2, 3, 4, 5]
    let r5: int = std::fold_int(&add, 0, arr, 5)       // 15
    let r6: int = std::reduce_int(&add, arr, 5)        // 15
    return 0
}
```

### Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `twice` | `(fn(int)->int, int) -> int` | Apply `f` twice: `f(f(x))` |
| `thrice` | `(fn(int)->int, int) -> int` | Apply `f` three times: `f(f(f(x)))` |
| `compose_apply` | `(fn(int)->int, fn(int)->int, int) -> int` | Compose and apply: `f(g(x))` |
| `apply_n` | `(fn(int)->int, int, int) -> int` | Apply `f` `n` times to `x` |
| `fold_int` | `(fn(int,int)->int, int, int[], int) -> int` | Left fold over an array |
| `reduce_int` | `(fn(int,int)->int, int[], int) -> int` | Left fold using first element as init |

All functions operate on `int` (`int64`) values. The `len` parameter for array functions
is the number of elements, not bytes.

## Predicate (`std/predicate.hk`)

Array predicate functions. These take a function pointer `fn(int) -> bool` and
an array, and return a result based on matching elements.

```
import "std"

fn is_even(x: int) -> bool { if x % 2 == 0 { return true } else { return false } }

fn main() -> int {
    let arr: int[5] = [1, 2, 3, 4, 5]

    let a: bool = std::any_int(&is_even, arr, 5)      // true
    let b: bool = std::all_int(&is_even, arr, 5)      // false
    let idx: int = std::find_int(&is_even, arr, 5)    // 1 (first even at index 1)
    let cnt: int = std::count_int(&is_even, arr, 5)   // 2
    return 0
}
```

### Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `any_int` | `(fn(int)->bool, int[], int) -> bool` | `true` if any element matches predicate |
| `all_int` | `(fn(int)->bool, int[], int) -> bool` | `true` if all elements match predicate |
| `find_int` | `(fn(int)->bool, int[], int) -> int` | Index of first match, or `-1` |
| `count_int` | `(fn(int)->bool, int[], int) -> int` | Count of elements matching predicate |

### Note on predicate functions

When writing predicate functions, use the `if/else` pattern for returning bool values:

```
// Correct:
fn is_even(x: int) -> bool { if x % 2 == 0 { return true } else { return false } }

// Incorrect (known compiler bug with bool return):
fn is_even(x: int) -> bool { return x % 2 == 0 }
```

## Transform (`std/transform.hk`)

Array transformation and aggregation functions.

```
import "std"

fn double(x: int) -> int { return x * 2 }
fn is_even(x: int) -> bool { if x % 2 == 0 { return true } else { return false } }

fn main() -> int {
    let arr: int[5] = [1, 2, 3, 4, 5]

    let dst: int[5] = [0, 0, 0, 0, 0]
    std::map_int_into(&double, arr, dst, 5)  // dst = [2, 4, 6, 8, 10]

    let filtered: int[5] = [0, 0, 0, 0, 0]
    let n: int = std::filter_int(&is_even, arr, 5, filtered)  // n=2, filtered=[2,4,0,0,0]

    let s: int = std::sum_int(arr, 5)      // 15
    let mn: int = std::min_int(arr, 5)     // 1
    let mx: int = std::max_int(arr, 5)     // 5
    return 0
}
```

### Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `map_int_into` | `(fn(int)->int, int[], int[], int) -> void` | Map `f` over `src`, write into `dst` |
| `filter_int` | `(fn(int)->bool, int[], int, int[]) -> int` | Copy matching elements to `out`, return count |
| `sum_int` | `(int[], int) -> int` | Sum all elements |
| `min_int` | `(int[], int) -> int` | Minimum element |
| `max_int` | `(int[], int) -> int` | Maximum element |

## Memory (`std/mem.hk`)

Pure-memory operations (no libc dependency, works in WASM freestanding).
These functions operate on raw byte pointers and do not manage allocation.

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
| `mem_find_byte` | `(int8*, int8, int64) -> int64` | Find first occurrence of byte, return index or `-1` |

### Safety notes

- `mem_copy` assumes non-overlapping regions. Use `mem_swap` for overlapping copies.
- `mem_eq` returns `true` only if all `n` bytes are identical.
- All functions operate on raw byte pointers (`int8*`). The caller must ensure
  the pointers are valid and point to at least `n` bytes of allocated memory.
- For scoped memory, use [region blocks](/docs/syntax-control-flow#region)
  with `__region_alloc`. For heap memory, use `extern fn malloc`/`extern fn free`.
