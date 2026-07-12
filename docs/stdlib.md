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
