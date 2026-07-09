# otaru — Hokkaido Package Manager

otaru is a package manager and project scaffold for the [Hokkaido](https://github.com/hokkaido-lang/hokkaido) compiler.

## Installation

### Nix (flake — recommended)

```bash
nix profile install github:hokkaido-lang/hokkaido          # otaru + hokkaido bundled (default)
nix profile install github:hokkaido-lang/hokkaido#hokkaido # compiler only (optional)
nix develop github:hokkaido-lang/hokkaido                   # dev shell (both + cmake)
```

The Nix package bundles the hokkaido compiler and std library — no extra setup needed.

### Nix (traditional)

```bash
nix-build -E 'with import <nixpkgs> {}; callPackage ./otaru/default.nix {}'
```

### Cargo

```bash
cargo install --path otaru
```

With `cargo install`, you need the hokkaido compiler on `PATH` or `HOKKAIDO_HOME`.

## Prerequisites

- **hokkaido compiler** — on `PATH`, in `HOKKAIDO_HOME`, or bundled (Nix)
- **clang** — needed at runtime to link executables

## Commands

| Command | Description |
|---------|-------------|
| `otaru new <name>` | Scaffold a new Hokkaido project (includes std/) |
| `otaru build` | Build the project in src/ (debug mode) |
| `otaru build --release` | Build with optimizations (`-O2`) |
| `otaru build -f` | Force rebuild, ignoring cached artifacts |
| `otaru build <file.hk>` | Compile a single file (no project needed) |
| `otaru build --freestanding` | Build in freestanding mode (no CRT/libc) |
| `otaru run` | Build project and run |
| `otaru run --release` | Build with optimizations and run |
| `otaru run <file.hk>` | Compile a single file and run |
| `otaru add <name> --git <url>` | Add a git dependency |
| `otaru add <name> --path <path>` | Add a local dependency |
| `otaru install` | Clone/fetch all dependencies |
| `otaru clean` | Remove the `build/` directory |

## Quick Start

```bash
# Create a new project (std/ is automatically prepared)
otaru new myapp
cd myapp

# Write some code with std library
cat > src/main.hk << 'EOF'
import "std"

extern fn putchar(c: int) -> int

fn double(x: int) -> int {
    return x * 2
}

fn main() -> int {
    let x: int = std::twice(&double, 5)
    putchar(48 + x / 10)  # 1
    putchar(48 + x % 10)  # 0
    putchar(10)
    return x
}
EOF

# Build and run
otaru run
```

### Single-file workflow

```bash
# Compile and run any .hk file without a project
cat > hello.hk << 'EOF'
extern fn putchar(c: int) -> int
fn main() -> int {
    putchar(72); putchar(105); putchar(10)
    return 0
}
EOF
otaru run hello.hk
```

### Freestanding mode

```bash
# Build a freestanding object file (no CRT/libc)
otaru build kernel.hk --freestanding
# Produces build/kernel.o — link with ld.lld manually
```

## Project Structure

```
myapp/
├── otaru.toml         # Manifest (package name, version, deps)
├── hk.mod             # Marks the package root
├── std/               # Standard library (prepared by otaru new)
│   ├── hk.mod
│   └── hof.hk
└── src/
    └── main.hk        # Entry point
```

## Incremental Builds

otaru caches compilation results based on file content hashes (modification time + size).
A build is skipped entirely when no inputs have changed:

- Source files in `src/` and `deps/`
- Standard library files
- The `hokkaido` compiler binary itself

Cache files are stored in `build/.hkbuildcache.{debug,release}`. Use `-f` / `--force` to bypass.

### Optimization

| Mode | Flag | Compiler flag | Use case |
|------|------|--------------|----------|
| Debug (default) | *(none)* | `-O0` | Fast compilation, no optimizations |
| Release | `--release` / `-r` | `-O2` | Optimized binary, slower compilation |

Debug and release builds have separate caches — switching modes does not invalidate the
other's cache. Object file sizes are typically ~40% smaller with `--release`.

## Configuration

`otaru.toml`:

```toml
[package]
name = "myapp"
version = "0.1.0"

[dependencies]
mylib = { git = "https://github.com/user/mylib" }
```

## Environment Variables

- `HOKKAIDO_HOME` — Directory containing the `hokkaido` binary (fallback if not on PATH)
- `HOKKAIDO_CRT_DIR` — C runtime directory for hokkaido
- `HOKKAIDO_DYNAMIC_LINKER` — Dynamic linker path for hokkaido
