---
layout: ../../layouts/DocsLayout.astro
title: Packages, Modules, and Namespaces
---

# Packages, Modules, and Namespaces

Hokkaido uses a Go-style package system for code organization across files and
directories, combined with `namespace` blocks for internal grouping.

## Packages

A package is a directory of `.hk` files that share a common `package` declaration.
Packages form the unit of import and visibility.

### Module root

The module root is the nearest ancestor directory containing an `hk.mod` file
(which can be empty). All import paths are resolved relative to the module root.

```
myproject/
  hk.mod              # marks the module root
  main.hk             # package main
  util/
    util.hk           # package util
    extra.hk          # also package util (same package, multi-file)
```

### Package declaration

Every `.hk` file begins with a `package name;` declaration that identifies
which package it belongs to. All files in the same directory with the same
`package name` are part of the same package — they share scope and can
reference each other's declarations without `include`.

```
// util/util.hk
package util

pub fn add(a: int, b: int) -> int {
    return a + b
}
```

```
// util/extra.hk
package util  // same package as util.hk

pub fn subtract(a: int, b: int) -> int {
    return a - b
}
```

### Entry point

The main executable must use `package main` and define `fn main() -> int`:

```
package main

fn main() -> int {
    return 0
}
```

## Import

Import a package to access its public declarations:

```
import "path/to/package"
import alias "path/to/package"
```

The path is relative to the module root. The last path segment becomes the
default binding name:

```
import "util"          // access items as util::add(...)
import math "util"     // access items as math::add(...)
```

All `.hk` files in the imported directory are parsed. Each must declare
`package <name>` matching the last path segment.

### Import resolution

1. The compiler finds the module root by walking up from the entry file
   looking for `hk.mod`.
2. The import path is resolved relative to the module root.
3. All `.hk` files in that directory are collected (sorted by name for
   deterministic order).
4. Each file's `package` declaration is validated against the expected name.
5. Declarations are merged into the importing file's scope with the
   binding name as a prefix (`util::add`, `util::subtract`).

Duplicate imports (same directory imported twice, directly or transitively)
are silently ignored.

## Visibility

The `pub` keyword controls cross-package visibility:

```
pub fn visible_everywhere() -> int { ... }
fn package_private() -> int { ... }

pub struct Point { x: int, y: int }        // accessible from other packages
struct Internal { z: int }                  // package-private

pub enum Status { Ok, Error }              // accessible from other packages
```

Declarations without `pub` are visible only within their own package.
`pub` applies to `fn`, `let` (top-level), `struct`, and `enum` declarations.

## Namespaces

Within a package, `namespace` blocks provide additional grouping:

```
// util/util.hk
package util

pub namespace geometry {
    pub fn area(width: int, height: int) -> int {
        return width * height
    }
}
```

Access is via the `::` qualifier, combining the package binding, namespace,
and item name:

```
import "util"

fn main() -> int {
    let a: int = util::geometry::area(10, 20)
    return 0
}
```

Namespaces can nest arbitrarily. Declarations inside namespaces can also
have `pub` for cross-package visibility.

## Include (low-level)

The `include "path.hk"` directive is a low-level textual inclusion that
pastes the contents of another file at the point of inclusion. Unlike
`import`, it does not create a package boundary:

- Included declarations are added to the current scope without any name prefix.
- Includes are deduplicated (diamond-shaped includes are safe).
- `include` is primarily useful for code generation, macros, or
  splitting a very large file without creating a package.

For normal code organization across files, prefer `import` and packages.

```
include "helpers.hk"     // declarations from helpers.hk are visible here
```
