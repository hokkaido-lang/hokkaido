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
| **Liveness** | References must never outlive the value they refer to (enforced via NLL liveness analysis). |
| **Ownership freeze** | The original value cannot be read or written while it is borrowed — the owner is frozen until the borrow ends (at its last use point). |

The checker runs on every function before code generation. It builds a CFG,
computes liveness, and tracks each borrow's lifetime. It rejects programs that
violate any of the rules above.

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
