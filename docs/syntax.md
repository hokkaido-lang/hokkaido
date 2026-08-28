---
layout: ../../layouts/DocsLayout.astro
title: Syntax Overview
---

# Hokkaido Language Syntax

Hokkaido is a small, explicit, systems-programming language that compiles to native code via LLVM.
It draws inspiration from Rust, C, and modern language design while keeping the feature set
deliberately minimal.

## Contents

- [Comments](#comments)
- [Types](/docs/syntax-types#types)
- [Variables](/docs/syntax-types#variables)
- [Functions](/docs/syntax-functions#functions)
- [Generic functions](/docs/syntax-functions#generic-functions)
- [Trait bounds](/docs/syntax-functions#trait-bounds)
- [Return](/docs/syntax-functions#return)
- [If / Else](/docs/syntax-control-flow#if--else)
- [While loop](/docs/syntax-control-flow#while-loop)
- [For loop](/docs/syntax-control-flow#for-loop)
- [For-in loop](/docs/syntax-control-flow#for-in-loop)
- [Break](/docs/syntax-control-flow#break)
- [Continue](/docs/syntax-control-flow#continue)
- [Match](/docs/syntax-control-flow#match)
- [Operator precedence](/docs/syntax-expressions#operator-precedence)
- [Comparison operators](/docs/syntax-expressions#comparison-operators)
- [Logical operators](/docs/syntax-expressions#logical-operators)
- [Bitwise operators](/docs/syntax-expressions#bitwise-operators)
- [Arithmetic operators](/docs/syntax-expressions#arithmetic-operators) (including `%` modulo)
- [Shift operators](/docs/syntax-expressions#shift-operators)
- [Assignment](/docs/syntax-expressions#assignment)
- [References and Pointers](/docs/syntax-expressions#references-and-pointers)
- [Borrow checking](/docs/syntax-expressions#borrow-checking)
- [Region blocks](/docs/syntax-expressions#region-blocks)
- [Raw pointers](/docs/syntax-expressions#raw-pointers)
- [Atomic operations](/docs/syntax-expressions#atomic-operations)
- [Function calls](/docs/syntax-expressions#function-calls)
- [Arrays](/docs/syntax-data-structures#arrays)
- [Structs](/docs/syntax-data-structures#structs)
- [Generic structs](/docs/syntax-data-structures#generic-structs)
- [Traits](/docs/syntax-data-structures#traits)
- [Impl blocks and method calls](/docs/syntax-data-structures#impl-blocks-and-method-calls)
- [Enums](/docs/syntax-data-structures#enums)
- [Tuple types](/docs/syntax-types#tuple-types)
- [Tuple expressions](/docs/syntax-expressions#tuple-expressions)
- [Package declaration](/docs/syntax-modules#packages)
- [Import](/docs/syntax-modules#import)
- [Visibility (pub)](/docs/syntax-modules#visibility)
- [Namespaces](/docs/syntax-modules#namespaces)
- [Region](/docs/syntax-control-flow#region)
- [Include](/docs/syntax-modules#include-low-level)
- [C FFI](/docs/syntax-ffi#c-ffi)
- [Freestanding mode](/docs/syntax-ffi#freestanding-mode)
- [Inline assembly](/docs/syntax-ffi#inline-assembly)
- [Standard Library](/docs/stdlib)
- [Language Server](/docs/lsp)
- [Package Manager](/docs/package-manager)
- [Example: Number guessing game](/docs/example-guess)
- [Design Decisions](/docs/design-decisions)

## Comments

Hokkaido supports two comment forms:

```
// Line comment — everything from // to the end of the line is ignored.

/* Block comment — can span multiple lines.
   Block comments do NOT nest: the first */ ends the comment
   even if it appears inside what looks like a nested /* ... */ pair. */
```

Line comments (`//`) are the idiomatic choice for most documentation. Block comments (`/* */`)
are useful for temporarily disabling large regions during development.
