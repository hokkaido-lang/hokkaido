# sapporo CLI

Command-line tool for building web applications with [Hokkaido](https://github.com/jihoo/hokkaido) compiled to WebAssembly.

Part of the [Hokkaido](https://github.com/jihoo/hokkaido) project.

## Installation

### From release

Download the latest release and add to your PATH:

```bash
tar xzf hokkaido-linux-x86_64.tar.gz
export PATH="$PWD/bin:$PATH"
```

The `sapporo` binary is bundled alongside `hokkaido` and `otaru`.

### From source

```bash
cd sapporo-cli
cargo build --release
cp target/release/sapporo ~/.local/bin/
```

## Quick start

```bash
# Create a new project
sapporo new my-app
cd my-app

# Build to dist/
sapporo build

# Start dev server and open browser
sapporo run
```

## Commands

### `sapporo new <name>`

Create a new project in a directory called `<name>`.

```
my-app/
├── sapporo.toml       # Build configuration
├── hk.mod             # Module root marker
├── index.html         # HTML host page
├── .gitignore
└── src/
    └── main.hk        # Starter code
```

### `sapporo init`

Initialize the current directory as a sapporo project. Creates `sapporo.toml`, `hk.mod`, `src/main.hk`, and `index.html` if they don't exist.

### `sapporo build`

Compile `.hk` sources to WebAssembly and package into `dist/`.

Output:
```
dist/
├── app.wasm           # Compiled WASM module
├── sapporo.js         # JavaScript loader
└── index.html         # HTML host page
```

Flags:
- `--force` — Force rebuild even if output is up to date
- `--verbose` — Show compiler and linker commands

**Incremental builds:** `sapporo build` only recompiles `.hk` files that have changed since the last build. Object files are cached in `dist/`.

### `sapporo run`

Build the project and start a local HTTP server on port 8080. Opens the browser automatically.

### `sapporo add <package>`

Add a dependency to `sapporo.toml`.

```bash
sapporo add sapporo-lib        # Add a Hokkaido package
sapporo add lodash --npm       # Add an npm package
```

## Configuration

`sapporo.toml`:

```toml
[package]
name = "my-app"
version = "0.1.0"

[build]
sources = ["src"]          # Directories to scan for .hk files
dist = "dist"              # Output directory
cflags = []                # Extra hokkaido compiler flags
ldflags = []               # Extra wasm-ld flags
```

## Dependencies

The sapporo CLI requires:

- **hokkaido** — the Hokkaido compiler (found via PATH or relative to the sapporo binary)
- **wasm-ld** — the WASM linker (found via PATH or in the Nix store)

These are bundled in the release tarball.

## Global flags

- `--verbose` — Show detailed output including compiler/linker commands (works with all subcommands)

## How building works

1. `sapporo build` scans the `sources` directories for `.hk` files
2. Each `.hk` file is compiled to a `.o` (WASM object) using `hokkaido --target wasm32-unknown-unknown`
3. The `.o` files are linked into a `.wasm` file using `wasm-ld --no-entry --export=main --allow-undefined`
4. `sapporo.js` and `index.html` are copied to `dist/`
5. The sapporo library (`sapporo.hk`) is copied to the project root for import resolution

Incremental builds compare `.o` timestamps against `.hk` source files and skip recompilation for unchanged files.

## License

Apache 2.0
