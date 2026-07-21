---
layout: ../../layouts/DocsLayout.astro
title: Cubical Surface Language
---

# Cubical Surface Language — Syntax Reference

## Top-level Declarations

A program is a sequence of declarations. Three forms are allowed.

### Value definition

```
def <name> : <type> = <term>
```

Example:

```
def id : (A : U0) -> A -> A = \A x. x
```

### Datatype declaration

```
data <Name> [: <level>] =
  | <con1> : <type>
  | <con2> : <type>
  ...
```

The `<level>` annotation (`: U_n`) is optional. When present, it sets the universe level of the datatype explicitly (e.g. `data D : U1 = ...`). When absent, the level is inferred as the maximum universe level among constructor argument types.

Example:

```
data Nat = | zero : Nat | suc : Nat -> Nat
data Vec (A : U0) : U1 =
  | vnil : Vec A
  | vcons : A -> Vec A -> Vec A
```

#### Path constructors

A constructor whose return type is followed by `[ <face0> , <face1> ]` is a **path constructor** — it specifies a path whose endpoints are `face0` and `face1`.

```
data S1 =
  | base : S1
  | loop : S1 [ base , base ]
```

#### Strict positivity

Datatype definitions are checked for **strict positivity**. The declared type may only appear in negative positions (to the left of an arrow in the domain, or in a recursive constructor argument). Non-positive occurrences such as `data Bad = mk ((Bad -> Bad) -> Bad)` are rejected at parse time.

---

## Comments

Line comments begin with `--` and extend to the end of the line.

```
-- This is a comment
```

---

## Terms

### Universes

| Syntax | Meaning |
|--------|---------|
| `U0`, `U1`, `U2`, … | Universe at level *n* |
| `Type` | Alias for `U0` |

Universes support **cumulativity**: `U_n` is a subtype of `U_m` whenever `n ≤ m`. This means a term typed at a lower universe level can be used wherever a higher universe is expected. The cumulativity is checked structurally — for Pi types, the domain is contravariant and the codomain is covariant; for Sigma types, both components are covariant.

### Variables

Plain identifiers resolve first as local variables (de Bruijn), then as top-level globals, then as constructors.

Identifier characters: start with a letter or `_`; continue with letters, digits, `_`, `'`, `?`, `!`, `-`.

### Lambda abstraction

```
\x. <body>
\x y z. <body>        -- multi-binder shorthand
```

Alternative keyword form:

```
fun x y z => <body>
```

The `λ` Unicode character is also accepted in place of `\`.

### Let expressions

```
let <x> = <value> in <body>
let <x> : <type> = <value> in <body>
```

Type annotations are parsed but currently discarded (same as parenthesised ascriptions).

Example:

```
let n = suc zero in n
```

This desugars to:

```
(\n. n) (suc zero)
```

Nested `let` binds at the same precedence as `\` and `fun`.

### Function application

```
<f> <arg1> <arg2> ...
```

Application is left-associative and is written by juxtaposition.

### Dependent function type (Π)

Non-dependent arrow:

```
<A> -> <B>
```

Dependent Pi (binder in scope in `B`):

```
(x : A) -> B
```

Explicit Pi former:

```
Pi (x : A). B
Π (x : A). B
```

### Pair / Sigma type

Pair term:

```
(<a> , <b>)
```

Non-dependent product:

```
<A> * <B>
```

Dependent Sigma (binder in scope in `B`):

```
(x : A) * B
```

Explicit Sigma former:

```
Sigma (x : A). B
Σ (x : A). B
```

### Projections

```
fst <pair>
snd <pair>
```

### Type ascription

```
(<term> : <type>)
```

---

## Interval Expressions

The interval type is written `I` or `𝕀`.

| Syntax | Meaning |
|--------|---------|
| `i0` or `0` | Left endpoint |
| `i1` or `1` | Right endpoint |
| `i /\ j` or `i ∧ j` | Meet (min) |
| `i \/ j` or `i ∨ j` | Join (max) |
| `~ i` or `¬ i` | Negation (flip) |

Operator precedence (highest to lowest): `~` > `/\` > `\/`.

---

## Path Types and Path Abstraction

### Path type

```
Path <A> <u> <v>
```

A path in type `A` from `u` to `v`.

### Path abstraction (interval lambda)

```
<i> <body>
```

Binds an interval variable `i` in `body`. The `⟨` and `⟩` Unicode angle brackets are also accepted.

### Path application

```
<p> @ <i>
```

Applies path `p` to interval expression `i`.

---

## Elimination

```
elim <motive> { | <con1> <binders> => <body1> | <con2> <binders> => <body2> ... } <scrutinee>
```

`->` may be used in place of `=>` in case branches.

The motive may optionally be wrapped in brackets:

```
elim[<motive>] { ... } <scrutinee>
```

Example:

```
elim motive { | zero => base_case | suc n => step } value
```

---

## Pattern matching

`match` is sugar for an eliminator with a motive derived from an explicit return type:

```
match <scrutinee> return <return_type> with
  | <con1> <binders> => <body1>
  | <con2> <binders> => <body2>
  ...
```

`->` may be used in place of `=>` in case branches. Cases may be written with or without braces:

```
match n return Nat with | zero => z | suc m => s
match n return Nat with { | zero => z | suc m => s }
```

When the scrutinee is a bare identifier, that name is in scope in the return type (for dependent elimination). Otherwise the scrutinee is bound as `_match` in the return type.

Example (non-dependent):

```
match n return Nat with
  | zero => zero
  | suc m => suc m
```

This desugars to:

```
elim (\n. Nat) { | zero => zero | suc m => suc m } n
```

Path-constructor cases follow the same rules as `elim`: list ordinary argument binders first, then the interval variable last.

---

## Cubical Primitives

### Transport

```
transport <path> <element>
```

Transports `element` along the path `path`.

Transport reduces depending on the type family:

| Family shape | Input | Result |
|---|---|---|
| Constant (`A` doesn't change with `i`) | any `x` | `x` (identity) |
| `Univ n` | any `x` | `x` (universe transport is identity) |
| Pi `(x : A) -> B` | `λx. f x` | `λx. transport B (f x)` |
| `Path A u v` | `p` | `λj. transport A (p @ j)` |
| Sigma `(x : A) * B` | `(a, b)` | `(transport A a, transport B (subst a))` |
| `ua e` | `x` | `equivFwd e x` |
| Glue `Glue A [φ] te` at face `φ` | `glue [φ] t a` | `glue [φ] t (hcomp A [φ] (λi. t) a)` |
| Glue `Glue A [⊥] te` | any `x` | `transport (λi. A) x` |
| Glue `Glue A [⊤] te` | any `x` | `transport (λi. dom(te)) x` |

When the Glue face is non-trivial and the input is not a `glueElem` or the faces don't match, transport stays stuck (no reduction).

### Homogeneous composition

```
hcomp <type> <phi> <tube> <base>
```

Reduces at concrete interval endpoints:

| Application | Result |
|---|---|
| `(hcomp A ⊤ tube base) @ i` | `tube @ i` |
| `(hcomp A ⊥ tube base) @ i` | `base` |
| `(hcomp A φ tube base) @ 0` | `base` |
| `(hcomp A φ tube base) @ 1` | `tube @ 1` |

When the base is a **lambda abstraction** and the type is a **Pi type**, hcomp decomposes pointwise:

| Input | Result |
|---|---|
| `hcomp (Π x:A. B) φ (λi. λx. f i x) (λx. g x)` | `λx. hcomp (B x) φ (λi. f i x) (g x)` |

When the base is a **pair** and the type is a **Sigma type**, hcomp decomposes into projections:

| Input | Result |
|---|---|
| `hcomp (Σ x:A. B) φ (λi. p i , q i) (a , b)` | `(hcomp A φ (λi. fst (p i)) a, ...)` |

For other non-trivial faces, `hcomp` stays stuck as an `VHComp` value until applied to a concrete endpoint.

### Univalence and equivalences

| Syntax | Arguments | Meaning |
|--------|-----------|---------|
| `Equiv A B` | `A B` | Type of equivalences from `A` to `B` |
| `mkEquiv A B f g eta eps` | `A B f g eta eps` | Construct an equivalence |
| `equivFwd e x` | `e x` | Apply the forward map of equivalence `e` to `x` |
| `ua e` | `e` | Univalence: path from equivalence |

### Glue types

| Syntax | Arguments | Meaning |
|--------|-----------|---------|
| `Glue A phi te` | `A phi te` | Glue type: `A` is the underlying type, `phi` is a face formula, `te` is a family of equivalences on `phi` |
| `glueElem phi t a` | `phi t a` | Construct a glue element: `t` is in the equiv domain, `a` is in the underlying type `A` |
| `unglue phi te g` | `phi te g` | Unglue an element `g` to the underlying type |

Glue type reduction:

| Face | Result |
|---|---|
| `Glue A ⊥ te` | `A` (face is empty) |
| `Glue A ⊤ te` | `dom(te)` (face is full) |
| `Glue A φ te` | stuck as `VGlue` for non-trivial `φ` |

Glue element reduction:

| Face | Result |
|---|---|
| `glue ⊤ t a` | `t` (cap is the full result) |
| `glue ⊥ t a` | `a` (base is the full result) |
| `glue φ t a` | stuck as `VGlueElem` for non-trivial `φ` |

---

## Operator Precedence Summary

From lowest to highest binding:

| Level | Construct |
|-------|-----------|
| 1 (lowest) | `\x.`, `fun x =>`, `let x = t in u`, `<i>`, `Pi`, `Sigma`, `,` (pair) |
| 2 | `->`, `*` (non-dependent arrow/product, right-assoc) |
| 3 | `\/` (interval join) |
| 4 | `/\` (interval meet) |
| 5 | `~` (interval negation, prefix) |
| 6 | `@` (path application, left-assoc) |
| 7 | juxtaposition (function application, left-assoc) |
| 8 | `fst`, `snd`, `ua`, `transport`, `equivFwd` (prefix) |
| 9 (highest) | atoms: identifiers, integer literals, parenthesised terms |

---

## Unicode Aliases

The following Unicode symbols are accepted as alternatives to their ASCII counterparts.

| Unicode | ASCII equivalent |
|---------|-----------------|
| `λ` | `\` (lambda) |
| `Π` | `Pi` |
| `Σ` | `Sigma` |
| `𝕀` | `I` (interval type) |
| `⟨` / `⟩` | `<` / `>` (path binder) |
| `×` | `*` (product) |
| `∧` | `/\` (meet) |
| `∨` | `\/` (join) |
| `¬` | `~` (negation) |

---

## Grammar Summary (BNF-style)

```
program  ::= decl*
decl     ::= 'def' ident ':' term '=' term
           | 'data' ident (':' 'U' natural)? '=' ('|' con_decl)+

con_decl ::= ident ':' term ('[' term ',' term ']')?

term     ::= 'let' ident (':' term)? '=' term 'in' term
           | '\' ident+ '.' term
           | 'fun' ident+ '=>' term
           | '<' ident '>' term
           | 'Pi' '(' ident ':' term ')' '.' term
           | 'Sigma' '(' ident ':' term ')' '.' term
           | arrow_star ',' term
           | arrow_star

arrow_star ::= join ('->' arrow_star | '*' arrow_star)?

join     ::= meet ('\/' meet)*
meet     ::= tilde ('/\' tilde)*
tilde    ::= '~' tilde | papp
papp     ::= app ('@' tilde)*
app      ::= prefix_or_atom prefix_or_atom*

prefix_or_atom ::= 'fst' prefix_or_atom
                 | 'snd' prefix_or_atom
                 | 'ua' prefix_or_atom
                 | 'transport' prefix_or_atom prefix_or_atom
                 | 'equivFwd' prefix_or_atom prefix_or_atom
                 | 'Path' prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'hcomp' prefix_or_atom prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'Equiv' prefix_or_atom prefix_or_atom
                 | 'mkEquiv' prefix_or_atom x6
                 | 'Glue' prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'glueElem' prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'unglue' prefix_or_atom prefix_or_atom prefix_or_atom
                 | 'elim' term '{' cases '}' term
                 | atom

atom     ::= ident | '0' | '1' | '(' term ')'
```