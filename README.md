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

### Using hokup (recommended for non-Nix users)

[hokup](hokup/) is a toolchain installer and updater, similar to [rustup](https://rustup.rs/).

```sh
# Download the latest release tarball from GitHub:
# https://github.com/hokkaido-lang/hokkaido/releases

# Extract and run hokup to install (default: ~/.hokkaido)
./hokup install

# Or install to a custom directory
./hokup install --path /opt/hokkaido
```

After installation, add the toolchain to your PATH:

```sh
# hokup automatically configures your shell profile,
# but you can also set it manually:
export HOKKAIDO_HOME=~/.hokkaido
export PATH="$HOKKAIDO_HOME/bin:$PATH"
```

Once installed, you can update to the latest version anytime:

```sh
hokup update
```

The installation includes:
- `hokkaido` — the compiler
- `hok-lsp` — the language server
- `otaru` — the package manager
- `hokup` — the installer itself (for future updates)

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
