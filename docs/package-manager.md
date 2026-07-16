---
layout: ../../layouts/DocsLayout.astro
title: Package Manager (otaru)
---

# Package Manager (otaru)

`otaru` is the package manager and project scaffold for Hokkaido. It handles project
creation, building, dependency management, and script execution.

## Installation

### Nix (recommended)

```sh
nix profile install github:hokkaido-lang/hokkaido
```

### Cargo

```sh
cargo install --path otaru
```

When installed via Cargo, the hokkaido compiler must be on `PATH` or `HOKKAIDO_HOME`.

## Quick start

```sh
otaru new myapp
cd myapp
otaru run     # builds and runs src/main.hk
```

## Commands

### Project scaffolding

```
otaru new <name>
otaru init
```

**`otaru new <name>`** creates a new project directory with the standard layout.

**`otaru init`** initializes the current directory as a project (like `cargo init`).
Both accept `--wasm` to scaffold a WebAssembly project instead of a native one.

```
myapp/
  otaru.toml        # project manifest
  src/
    main.hk         # entry point (package main)
  hk.mod            # module root marker
  std/              # standard library (prepared by otaru)
```

### Building

| Command | Description |
|---------|-------------|
| `otaru build` | Build the project |
| `otaru build --release` / `-r` | Build with `-O2` optimizations |
| `otaru build -f` | Force rebuild, ignoring cache |
| `otaru build <file.hk>` | Compile a single file |
| `otaru build --freestanding` | Build without CRT/libc |
| `otaru build --triple <triple>` | Cross-compile for a target triple |

### Running

| Command | Description |
|---------|-------------|
| `otaru run` | Build and run |
| `otaru run --release` | Build with optimizations and run |
| `otaru run <file.hk>` | Compile and run a single file |

### Dependencies

| Command | Description |
|---------|-------------|
| `otaru add <name> --git <url>` | Add a git dependency |
| `otaru add <name> --path <path>` | Add a local dependency |
| `otaru install` | Clone/fetch all dependencies |

### Other

| Command | Description |
|---------|-------------|
| `otaru clean` | Remove the `build/` directory |
| `otaru exec` | List all scripts defined in `[scripts]` |
| `otaru exec <name>` | Run a named script |
| `otaru exec <name> <args>` | Run a script with arguments (`$1`, `$2`, etc.) |

## Project manifest (`otaru.toml`)

The `otaru.toml` file defines project metadata and build configuration:

```toml
[package]
name = "myapp"
version = "0.1.0"

[dependencies]
mylib = { git = "https://github.com/user/mylib" }
other = { path = "../other" }

[build]
type = "executable"          # executable | staticlib | sharedlib | object | wasm
sources = ["src/*.c"]        # glob patterns for source files
include_dirs = ["include"]   # -I directories
compiler = "cc"              # cc | gcc | clang | c++ | g++ | clang++
cflags = ["-Wall", "-Wextra"]
ldflags = ["-L/usr/local/lib"]
link = ["m", "pthread"]      # system libraries → -lm -lpthread
libraries = ["libfoo.a"]     # explicit library files
lib_dirs = ["vendor/lib"]    # custom library search paths

[scripts]
test = "otaru build && ./build/myapp"
bench = "otaru build --release && ./build/myapp --bench"
```

## Project auto-detection

otaru auto-detects the project type based on `otaru.toml` and source files:

| Condition | Build path |
|-----------|-----------|
| `.hk` files in `src/` | Hokkaido compiler |
| `[build]` section, no `.hk` files | C/C++ compiler |
| `[build]` section with `.hk` files | Hokkaido + C compiler (mixed) |

## WebAssembly

otaru natively compiles Hokkaido to WebAssembly via the LLVM backend.
`otaru build` and `otaru run` handle the full compile → link → run pipeline.

### Creating a WebAssembly project

```sh
otaru new mywasm --wasm
# or, in an existing directory:
mkdir mywasm && cd mywasm
otaru init --wasm
```

This creates a project ready to build:

```
mywasm/
  otaru.toml          # [build] type = "wasm"
  src/main.hk         # your code
  wasm32/
    index.html        # loads main.wasm, calls main(), displays result
    build.sh          # compile + link to .wasm (legacy alternative)
    serve.sh          # start local HTTP server for testing
```

### Building for WebAssembly

```sh
otaru build
```

otaru compiles `src/main.hk` with the Hokkaido compiler targeting `wasm32-unknown-unknown`,
then links the result with `wasm-ld --no-entry --export=main --allow-undefined` to produce
`build/<name>.wasm`. The `.wasm` is also copied to `wasm32/main.wasm` so the browser
test page can load it directly.

The `wasm32/build.sh` script is also provided as a standalone alternative if you prefer
not to use `otaru build`.

**Prerequisites:** `hokkaido` on PATH (or built locally) and `wasm-ld` (ships with LLVM/LLD).
otaru will search `/nix/store` for `wasm-ld` if it is not on PATH.

### Running in browser

```sh
otaru run
```

For wasm projects, `otaru run` builds the `.wasm` file and tries to execute it with
`wasmtime` or `wasm3` if available. If no runtime is found, it prints instructions
for running in a browser:

```sh
cd wasm32 && python3 -m http.server 8080
open http://localhost:8080
```

Open `http://localhost:8080` in your browser. The included `index.html` loads the wasm
module, calls the exported `main()` function, and displays the return value.

You can also use the legacy server script directly:

```sh
./wasm32/serve.sh         # starts http://localhost:8080
```

### Cross-compilation with --triple

You can cross-compile any `.hk` file to WebAssembly directly:

```sh
otaru build src/main.hk --triple wasm32-unknown-wasi
```

Or directly with hokkaido:

```sh
hokkaido src/main.hk -o build/main --target wasm32-unknown-unknown
wasm-ld --no-entry --export=main --allow-undefined -o build/main.wasm build/main.o
```

### Template `main.hk` for WebAssembly

The default template is a simple function that returns an integer:

```ocaml
fn main() -> int {
    return 42
}
```

The HTML page calls `instance.exports.main()` and displays the result.
You can modify `main.hk` freely — any integer return value will be shown in the browser.

## Mixed Hokkaido + C projects

For projects that combine Hokkaido and C code, otaru compiles `.hk` files with the
Hokkaido compiler and `.c`/`.cpp` files with the system C compiler, then links them
together.

```toml
[package]
name = "mixed"
version = "0.1.0"

[build]
link = ["m"]
```

Place `.hk` and `.c` files together in `src/` — otaru routes each file to the
correct compiler.

## C/C++ projects

otaru works as a Make replacement for pure C/C++ projects:

```toml
[package]
name = "mylib"
version = "0.1.0"

[build]
type = "executable"
sources = ["src/*.cpp"]
include_dirs = ["include"]
compiler = "clang++"
cflags = ["-std=c++17", "-Wall"]
link = ["pthread"]
```

Multi-target builds are supported — specify `--target <name>` to build a specific target.

## Build types

| Type | Output | Description |
|------|--------|-------------|
| `executable` | `build/<name>` | Default. Linked executable. |
| `staticlib` | `build/lib<name>.a` | Static library (via `ar`). |
| `sharedlib` | `build/lib<name>.so` | Shared/dynamic library. |
| `object` | `build/<stem>.o` | Single object file, no linking. |
| `wasm` | `build/<name>.wasm` | WebAssembly module (hokkaido + wasm-ld). |

## LLVM integration

For projects linking against LLVM:

```toml
[build]
llvm-config = "llvm-config-21"
llvm-components = ["core", "support", "irreader", "codegen", "WebAssembly"]
cflags = ["-std=c++17"]
link = ["pthread", "dl", "m"]
```

otaru runs `llvm-config --cxxflags` and `llvm-config --ldflags --libs <components>`
to auto-resolve include dirs and library flags.

## Scripts

Define named shell commands in `[scripts]` and run them with `otaru exec`:

```toml
[scripts]
clean = "rm -rf build"
test = "./build/myapp --test"
fmt = "clang-format -i src/*.c"
```

```sh
otaru exec              # list all defined scripts
otaru exec test         # run the "test" script
```

Scripts support positional arguments (`$1`, `$2`, ...) via `otaru exec`:

```sh
otaru exec greet World   # if greet = "echo Hello, $1!"
```
