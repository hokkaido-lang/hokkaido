# otaru — Hokkaido Package Manager

otaru is a package manager and project scaffold for the [Hokkaido](https://github.com/jihoo/hokkaido) compiler.

## Installation

### Nix (flake)

```bash
nix build github:jihoo/hokkaido#otaru    # standalone binary in result/bin/otaru
nix profile install github:jihoo/hokkaido#otaru  # or install globally
nix develop github:jihoo/hokkaido          # otaru is included in the dev shell
```

### Nix (traditional)

```bash
nix-build -E 'with import <nixpkgs> {}; callPackage ./otaru/default.nix {}'
```

### Cargo

```bash
cargo install --path otaru
```

### Pre-built binary

Download from the [Releases](https://github.com/jihoo/hokkaido/releases) page.

## Prerequisites

- The **hokkaido compiler** must be installed on your `PATH`, or pointed to via `HOKKAIDO_HOME`.
- For `otaru run`: `clang` is needed at runtime to link the compiled object file into an executable.

## Commands

| Command | Description |
|---------|-------------|
| `otaru new <name>` | Scaffold a new Hokkaido project |
| `otaru build` | Compile the current project via hokkaido |
| `otaru run` | Build, link with clang, and run |
| `otaru add <name> --git <url>` | Add a git dependency |
| `otaru add <name> --path <path>` | Add a local dependency |
| `otaru install` | Clone/fetch all dependencies |
| `otaru clean` | Remove the `build/` directory |

## Quick Start

```bash
# Create a new project
otaru new myapp
cd myapp

# Write some code
cat > src/main.hk << 'EOF'
extern fn putchar(c: int) -> int

fn main() -> int {
    putchar(72)  # H
    putchar(105) # i
    putchar(10)  # newline
    return 42
}
EOF

# Build and run
otaru run
```

## Project Structure

```
myapp/
├── otaru.toml         # Manifest (package name, version, deps)
├── hk.mod             # Marks the package root
└── src/
    └── main.hk        # Entry point
```

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
