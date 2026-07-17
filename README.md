# hokkaido

An LLVM-based systems programming language with cubical compile-time evaluation, WebAssembly support, and a web UI framework.

## What's in the box

| Package | Description | Language |
|---------|-------------|----------|
| [**hokkaido**](#hokkaido-compiler) | Compiler (LLVM + cubical backend) | C++ / Rust |
| [**hok-lsp**](#hok-lsp) | Language server for IDE support | C++ |
| [**otaru**](#otaru-package-manager) | Package manager and project scaffold | Rust |
| [**sapporo**](#sapporo-web-ui) | Web UI library + CLI for WASM apps | JavaScript / Hokkaido / Rust |
| **cubical** | Compile-time evaluation backend (static lib) | Rust |

## Quick start

### CLI app (otaru)

```sh
otaru new myapp
cd myapp
otaru run
```

### Web app (sapporo)

```sh
sapporo new my-webapp
cd my-webapp
sapporo build    # compile to dist/my-webapp.wasm
sapporo run      # dev server + open browser
```

Write your app in `src/main.hk`:

```ocaml
package main

import "sapporo"

fn main() -> int {
    sapporo::set_text("output", "Hello from Hokkaido!")
    return 0
}
```

No JavaScript required. See [sapporo docs](sapporo/docs/docs.md) and [examples](sapporo/docs/examples.md).

## Installation

### Nix (recommended)

```sh
# Everything (hokkaido + otaru + hok-lsp)
nix build github:hokkaido-lang/hokkaido
nix profile install github:hokkaido-lang/hokkaido

# Individual packages
nix build github:hokkaido-lang/hokkaido#hokkaido
nix build github:hokkaido-lang/hokkaido#otaru
nix build github:hokkaido-lang/hokkaido#sapporo
```

The Nix-built `otaru` bundles the `hokkaido` compiler and stdlib automatically.

### From source

**Requirements:** clang, cmake, cargo, LLVM 19+, wasm-ld (for WASM targets)

```sh
git clone https://github.com/hokkaido-lang/hokkaido.git
cd hokkaido

# Build the compiler + language server
mkdir build && cd build
cmake .. && make

# Build the package manager
cd ../otaru
cargo build --release

# Build the web app CLI
cd ../sapporo-cli
cargo build --release
```

Add the binaries to your `PATH`:

```sh
export PATH="$PWD/build:$PWD/otaru/target/release:$PWD/sapporo-cli/target/release:$PATH"
```

## Documentation

### Language

- [Syntax overview](docs/syntax.md)
- [Types](docs/syntax-types.md)
- [Functions and HOFs](docs/syntax-functions.md)
- [Control flow](docs/syntax-control-flow.md)
- [Expressions](docs/syntax-expressions.md)
- [Data structures](docs/syntax-data-structures.md)
- [Modules and imports](docs/syntax-modules.md)
- [FFI and cubical](docs/syntax-ffi-cubical.md)
- [Cubical surface language](docs/cubical_surface_language.md)
- [Standard library](docs/stdlib.md)
- [Design decisions](docs/design-decisions.md)

### Tools

- [Otaru package manager](otaru/README.md)
- [Sapporo web UI library](sapporo/docs/docs.md) — [API reference](sapporo/docs/api.md) — [Examples](sapporo/docs/examples.md)
- [Sapporo CLI](sapporo-cli/README.md)
- [Language server (hok-lsp)](src-cpp/lsp/README.md)

## Standard library

The `std` module is bundled with otaru and sapporo builds:

- **`std::mem`** — Pure-memory operations (WASM-compatible): `mem_copy`, `mem_set`, `mem_zero`, `mem_eq`, `mem_swap`, `mem_find`, `mem_contains`
- **`std::functional`** — Higher-order functions: `twice`, `thrice`, `compose`, `apply_n`, `fold_int`, `any_int`, `all_int`, `map_int_into`, `find_int`, `count_int`, `sum_ints`

Import with `import "std"` in your `.hk` files.

## Version management

All component versions are managed from a single file:

```sh
./scripts/bump-version.sh --show              # View versions
./scripts/bump-version.sh --set sapporo 0.3.0 # Bump + apply everywhere
./scripts/bump-version.sh --apply             # Re-apply from versions.toml
```

See [versions.toml](versions.toml) for current versions.

## License

[Apache License 2.0](LICENSE)
