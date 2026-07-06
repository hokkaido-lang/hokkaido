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

The stdlib provides common higher-order functions for `int` in `std/hof.hk`:

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
