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

### Pointer types

A pointer type is written by appending `*` to the element type — one `*` per level of indirection:

```
int8*      Pointer to int8
int64*     Pointer to int64
int64**    Pointer to pointer to int64
Point*     Pointer to a Point struct
```

Examples:

```
let x: int = 42
let p: int* = &x           // pointer to x
let pp: int** = &p         // pointer to pointer
let val: int = *p          // dereference → 42
```

See [Pointers](/docs/syntax-expressions#pointers) for more detail.

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

### Linear types

A variable can be declared with the `linear` qualifier to enforce single-use semantics:

```
let p: linear T = expression
```

A linear variable must be consumed at most once. After the variable is used (read, passed
to a function, or assigned to another variable), any subsequent use is a compile error:

```
let x: linear int = 42
let y: int = x       // consumes x — OK
let z: int = x       // ERROR: linear variable 'x' has already been consumed
```

Linear types track variable *names*, not values. Copying to another variable
bypasses the protection:

```
let p: linear int8* = __region_alloc(8)
let q: int8* = p          // p consumed, q is unguarded
*q = 42                   // q can be used freely
*(p + 8)                  // ERROR: p already consumed
```

Because of this, linear types are **not suitable** for enforcing heap memory
safety (double-free, use-after-free). The `std::mem` module intentionally
avoids providing heap allocation wrappers — use `extern fn malloc`/`free`
directly, with the understanding that it is inherently unsafe. For safe
scoped allocation, use [region blocks](/docs/syntax-control-flow#region).

Non-linear types are unaffected — they can be used multiple times as before.

### Restrictions

- A variable cannot have type `void`.
- Every variable must be initialized at the point of declaration. There is no default
  zero-initialization for local variables (unlike top-level `let`s, which are zeroed if the
  initializer evaluates to zero or is omitted — but omission is not valid syntax; the
  initializer is always required syntactically).
