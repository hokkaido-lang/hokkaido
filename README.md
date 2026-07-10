# hokkaido — LLVM-based compiler with cubical compile-time evaluation

## Packages

This repository provides three tools:

- **hokkaido** — the compiler (C++ with LLVM backend, Rust cubical backend)
- **hok-lsp** — the language server ([LSP](https://microsoft.github.io/language-server-protocol/)) for IDE support
- **otaru** — the package manager and project scaffold (Rust)

## Installation

### Nix (flake)

```sh
nix shell github:hokkaido-lang/hokkaido

# Both otaru + hokkaido (default)
nix build github:hokkaido-lang/hokkaido
nix profile install github:hokkaido-lang/hokkaido

# Just the hokkaido compiler
nix build github:hokkaido-lang/hokkaido#hokkaido
nix profile install github:hokkaido-lang/hokkaido#hokkaido

# Otaru explicitly (identical to default)
nix build github:hokkaido-lang/hokkaido#otaru
nix profile install github:hokkaido-lang/hokkaido#otaru
```

The Nix-built `otaru` bundles the `hokkaido` compiler automatically — everything works out of the box.

### From source

**Requirements:** clang, cmake, cargo, LLVM (19+)

```sh
git clone https://github.com/hokkaido-lang/hokkaido.git
cd hokkaido

# Build the compiler
mkdir build && cd build
cmake .. && make

# Build the package manager
cd ../otaru
cargo build --release
```

After building, add `build/` to your `PATH` or set `HOKKAIDO_HOME`:

```sh
export HOKKAIDO_HOME=/path/to/hokkaido/build
```

## Quick start

```sh
otaru new myapp
cd myapp
otaru run     # builds and runs src/main.hk
```

## Docs

- [Language syntax](docs/syntax.md)
- [Function types and HOFs](docs/syntax-functions.md)
- [LSP server](src-cpp/lsp/README.md)

## License

[Apache License 2.0](LICENSE)
