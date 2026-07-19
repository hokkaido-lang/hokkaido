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

## While loop

A `while` loop repeats its body as long as a condition is truthy:

```
while condition {
    body
}
```

The condition is evaluated before each iteration. If falsy on the first check, the body
never executes. The condition is coerced to `bool` (non-zero is truthy).

```
let i: int = 0
while i < 10 {
    i = i + 1
}
// i is 10
```

`while` supports `break`, `continue`, and labeled break/continue — the same as `for`:

```
let x: int = 0
'outer: while x < 100 {
    let y: int = 0
    while y < 10 {
        if x + y > 15 {
            break 'outer
        }
        y = y + 1
    }
    x = x + 1
}
```

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

### Infinite loop

Omit all three parts:

```
for ;; {
    // runs forever — exit with break or return
}
```

## For-in loop

A `for-in` loop iterates over a range of integers using `..` (exclusive) or `..=`
(inclusive):

```
for i in start..end {
    body
}
```

The variable `i` is declared by the loop (no `let` keyword needed) and takes values
from `start` up to but not including `end`. The `..=` form includes the upper bound:

```
// exclusive: i = 0, 1, 2, 3, 4
for i in 0..5 {
    // ...
}

// inclusive: i = 0, 1, 2, 3, 4, 5
for i in 0..=5 {
    // ...
}
```

The range expressions are evaluated once before the loop starts. The loop variable is
an `int64` and can be used freely in the body. `break`, `continue`, and labels work
as with any other loop:

```
'outer: for i in 0..10 {
    for j in 0..10 {
        if i + j >= 15 {
            break 'outer
        }
    }
}
```

### Desugaring

For-in loops are syntactic sugar for a C-style `for` loop. `for i in a..b` desugars to:

```
for let i = a; i < b; i = i + 1 { ... }       // exclusive (..)
for let i = a; i <= b; i = i + 1 { ... }      // inclusive (..=)
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

The `match` expression performs pattern matching on a value, dispatching to the
arm whose pattern matches. It works with enums, integers, characters, and booleans.

```
match value {
    pattern1 => expr_or_block
    pattern2 => expr_or_block
    // ...
}
```

### Enum patterns

Matching on an enum value binds the variant's fields:

```
enum Shape {
    Circle { radius: float64 }
    Rect { w: float64, h: float64 }
}

fn area(s: Shape) -> float64 {
    return match s {
        Shape::Circle { radius } => 3.14159 * radius * radius
        Shape::Rect { w, h } => w * h
    }
}
```

### Integer patterns

Integer literals can be used as patterns:

```
let v: int = 2
let label: int = match v {
    1 => 10
    2 => 20
    3 => 30
    _ => 99
}
```

Negative integer literals work too:

```
let x: int = -1
let r: int = match x {
    -1 => 10
    0 => 20
    1 => 30
    _ => 0
}
```

### Char patterns

Character literals can be used as patterns:

```
let ch: char = 'a'
let val: int = match ch {
    'a' => 1
    'b' => 2
    'c' => 3
    _ => 0
}
```

### Bool patterns

Boolean literals `true` and `false` work as patterns:

```
let flag: bool = true
let v: int = match flag {
    true => 1
    false => 0
}
```

### Wildcard pattern

The `_` pattern matches anything and acts as a catch-all:

```
let x: int = 42
let v: int = match x {
    0 => 100
    _ => 999     // matches everything else
}
```

### Match as expression

`match` is an expression — it evaluates to a value. The type is inferred from the arms:

```
let x: int = 42
let label: int = match x {
    0 => 1
    1 => 2
    _ => 3
}
```

`match` can be used anywhere an expression is expected — in arithmetic, function calls,
return statements, or nested inside other expressions:

```
let w: int = 1
let computed: int = match w {
    0 => 100
    1 => 200
    _ => 0
} + 50

let nested: int = match a {
    0 => match b {
        0 => 10
        _ => 20
    }
    _ => 30
}
```

### Completeness

The compiler emits a **warning** when not all enum variants are covered by match arms.
If no arm matches (which can happen with unmatched variants), a default null value is
returned. Future versions may make this a hard error or require an explicit `_ => {}`
catch-all arm.

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
