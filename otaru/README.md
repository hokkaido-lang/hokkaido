# otaru — Hokkaido Package Manager & C/C++ Build Tool

otaru is a package manager and project scaffold for the [Hokkaido](https://github.com/hokkaido-lang/hokkaido) compiler, and a drop-in replacement for Make for C/C++ projects.

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

| Component | Required for | Notes |
|-----------|-------------|-------|
| **hokkaido compiler** | Hokkaido projects | On `PATH`, in `HOKKAIDO_HOME`, or bundled (Nix) |
| **C/C++ compiler** | C/C++ projects | Defaults to `cc`; override with `compiler = "gcc"` etc. |
| **clang** | Hokkaido linking | Default linker for Hokkaido projects |
| **ar** | Static libraries | Only needed for `type = "staticlib"` |

## Commands

### Core

| Command | Description |
|---------|-------------|
| `otaru new <name>` | Scaffold a new project (Hokkaido by default) |
| `otaru build` | Build the project (auto-detects Hokkaido or C/C++) |
| `otaru build --release` / `-r` | Build with `-O2` optimizations |
| `otaru build -f` | Force rebuild, ignoring cache |
| `otaru run` | Build and run |
| `otaru run --release` | Build with optimizations and run |
| `otaru clean` | Remove the `build/` directory |
| `otaru exec` | List all scripts defined in `[scripts]` |
| `otaru exec <name>` | Run a named shell script |
| `otaru exec <name> <args>` | Run a script with arguments ($1, $2, etc.) |

### Hokkaido-specific

| Command | Description |
|---------|-------------|
| `otaru build <file.hk>` | Compile a single `.hk` file |
| `otaru build --freestanding` | Build without CRT/libc (ELF entry point) |
| `otaru run <file.hk>` | Compile and run a single file |
| `otaru add <name> --git <url>` | Add a git dependency |
| `otaru add <name> --path <path>` | Add a local dependency |
| `otaru install` | Clone/fetch all dependencies |

### C/C++-specific

| Command | Description |
|---------|-------------|
| `otaru build <file.c>` | Compile a single C file |
| `otaru build <file.cpp>` | Compile a single C++ file |
| `otaru build --target <name>` | Build a specific target (multi-target projects) |
| `otaru run` | Build and run the C/C++ executable |

### Project auto-detection

otaru auto-detects the project type based on `otaru.toml` and source files:

| Condition | Build path |
|-----------|-----------|
| `.hk` files in `src/` | Hokkaido compiler (also reads `[build]` for linking if present) |
| `[build]` section, no `.hk` files | C/C++ compiler |
| `[build]` section with `.hk` files | Hokkaido compiler + C compiler (mixed build) |
| Neither | Error |

## Quick Start — Hokkaido

```bash
otaru new myapp
cd myapp

cat > src/main.hk << 'EOF'
import "std"

extern fn putchar(c: int) -> int

fn double(x: int) -> int {
    return x * 2
}

fn main() -> int {
    let x: int = std::twice(&double, 5)
    putchar(48 + x / 10)
    putchar(48 + x % 10)
    putchar(10)
    return x
}
EOF

otaru run
```

### Single-file workflow

```bash
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
otaru build kernel.hk --freestanding
# Produces build/kernel.o — link with ld.lld manually
```

## Quick Start — C Project

```bash
mkdir mylib && cd mylib

cat > otaru.toml << 'EOF'
[package]
name = "mylib"
version = "0.1.0"

[build]
type = "staticlib"
sources = ["src/*.c"]
include_dirs = ["include"]
cflags = ["-Wall", "-Wextra"]
EOF

mkdir -p src include

cat > include/mylib.h << 'EOF'
#ifndef MYLIB_H
#define MYLIB_H
int add(int a, int b);
#endif
EOF

cat > src/mylib.c << 'EOF'
#include "mylib.h"
int add(int a, int b) { return a + b; }
EOF

otaru build
# Produces build/libmylib.a
```

## Quick Start — Hokkaido + C FFI

```bash
mkdir myapp && cd myapp

cat > otaru.toml << 'EOF'
[package]
name = "myapp"
version = "0.1.0"

[build]
link = ["m"]
EOF

mkdir -p src

cat > src/main.hk << 'EOF'
extern fn sin(x: double) -> double
extern fn putchar(c: int) -> int

fn main() -> int {
    let pi: double = 3.14159265358979
    let s: double = sin(pi / 2.0)
    # print integer part of s (should be 1)
    putchar(48 + int(s))
    putchar(10)
    return 0
}
EOF

otaru run
```

## Project Structure

### Hokkaido project

```
myapp/
├── otaru.toml         # Manifest (package, deps, optional [build])
├── hk.mod             # Marks the package root
├── std/               # Standard library (prepared by otaru new)
│   ├── hk.mod
│   └── hof.hk
└── src/
    └── main.hk        # Entry point
```

### C/C++ project

```
mylib/
├── otaru.toml         # Manifest with [build] section
├── include/
│   └── mylib.h        # Public headers
└── src/
    ├── foo.c
    └── bar.c
```

### Multi-target C/C++ project

```
hokkaido/
├── otaru.toml
└── src-cpp/
    ├── main.cpp
    ├── lexer.cpp
    └── lsp/
        └── lsp.cpp
```

### Mixed Hokkaido + C project

```
myapp/
├── otaru.toml         # [build] section for linking and/or C sources
├── hk.mod
├── std/
└── src/
    ├── main.hk        # Hokkaido source
    ├── ffi.c          # C glue code (compiled by cc)
    └── helpers.c
```

## Configuration

### `otaru.toml` — Hokkaido project

```toml
[package]
name = "myapp"
version = "0.1.0"

[dependencies]
mylib = { git = "https://github.com/user/mylib" }
other = { path = "../other" }
```

### `otaru.toml` — C/C++ project (single target)

```toml
[package]
name = "myapp"
version = "0.1.0"

[build]
type = "executable"          # executable | staticlib | sharedlib | object
sources = ["src/*.c"]        # glob patterns for source files
include_dirs = ["include"]   # -I directories
compiler = "cc"              # cc | gcc | clang | c++ | g++ | clang++
cflags = ["-Wall", "-Wextra"]
ldflags = ["-L/usr/local/lib"]
link = ["m", "pthread"]      # system libraries → -lm -lpthread
libraries = ["libfoo.a"]     # explicit library files (auto-resolved)
lib_dirs = ["vendor/lib"]    # custom library search paths
```

### `otaru.toml` — C/C++ project (multi-target)

```toml
[package]
name = "hokkaido"
version = "0.1.0"

[build.targets.hokkaido]
type = "executable"
sources = ["src-cpp/main.cpp", "src-cpp/cubical.cpp", "src-cpp/lexer.cpp",
           "src-cpp/parser.cpp", "src-cpp/codegen.cpp"]
include_dirs = ["src-cpp"]
link = ["LLVM", "pthread", "dl", "m"]
cflags = ["-std=c++17"]
libraries = ["target/release/libcubical_c.a"]

[build.targets.hok-lsp]
type = "executable"
sources = ["src-cpp/lsp_main.cpp", "src-cpp/lsp/lsp.cpp",
           "src-cpp/lexer.cpp", "src-cpp/parser.cpp"]
include_dirs = ["src-cpp"]
link = ["LLVM", "pthread", "dl", "m"]
cflags = ["-std=c++17"]
```

Build with: `otaru build` (all targets) or `otaru build --target hokkaido` (one target).

### `otaru.toml` — Hokkaido + C FFI

```toml
[package]
name = "myapp"
version = "0.1.0"

[build]
sources = ["src/ffi.c"]                        # optional C glue code
include_dirs = ["include"]
link = ["gtk4", "glib-2.0", "gobject-2.0"]     # -l flags
ldflags = ["-L/usr/local/lib"]                 # extra linker flags
lib_dirs = ["/usr/local/lib"]                  # library search paths
cflags = ["-Wall"]
```

When `.hk` files exist in `src/`, otaru compiles them with the Hokkaido compiler and
links the result together with any C object files and external libraries.

## `[build]` Reference

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `type` | string | `"executable"` | Build output type |
| `sources` | string[] | `[]` | Source file glob patterns |
| `include_dirs` | string[] | `[]` | Header search paths (`-I`) |
| `compiler` | string | `"cc"` | C/C++ compiler binary |
| `cflags` | string[] | `[]` | Extra compiler flags |
| `ldflags` | string[] | `[]` | Extra linker flags |
| `link` | string[] | `[]` | System library names (`-l` flags) |
| `libraries` | string[] | `[]` | Explicit library file paths |
| `lib_dirs` | string[] | `[]` | Library search paths (`-L`) |
| `targets` | table | — | Named sub-targets (multi-target) |
| `prebuild` | string | — | Shell command to run before compiling |
| `llvm-config` | string | — | Path to `llvm-config` binary |
| `llvm-components` | string[] | — | LLVM components (e.g. `["core", "support"]`) |

### `[scripts]` Reference

| Field | Type | Description |
|-------|------|-------------|
| `<name>` | string | Shell command to execute |

- Arguments: `$1`, `$2`, ... are replaced with positional args from `otaru exec`
- Multi-line: use triple-quoted strings (`"""..."""`) for complex commands
- Run with: `otaru exec <name>` or `otaru exec` to list all

### Build types

| Type | Output | Command |
|------|--------|---------|
| `executable` | `build/<name>` | `cc ... -o build/<name>` |
| `staticlib` | `build/lib<name>.a` | `ar rcs build/lib<name>.a ...` |
| `sharedlib` | `build/lib<name>.so` | `cc -shared ... -o build/lib<name>.so` |
| `object` | `build/<stem>.o` | `cc -c <source> -o build/<stem>.o` |

## Scripts

Define named shell commands in `[scripts]` and run them with `otaru exec`:

```toml
[scripts]
clean = "rm -rf build"
test = "./build/myapp --test"
fmt = "clang-format -i src/*.c"
lint = "cargo clippy"
bench = "./build/bench --iterations 1000"
```

```bash
otaru exec              # list all defined scripts
otaru exec test         # run the "test" script
otaru exec fmt          # run the "fmt" script
```

### Argument substitution

Scripts support positional arguments via `$1`, `$2`, etc.:

```toml
[scripts]
greet = "echo Hello, $1!"
```

```bash
otaru exec greet World        # prints: Hello, World!
otaru exec greet Alice        # prints: Hello, Alice!
```

### Combining with build commands

Scripts can call other otaru commands or arbitrary shell:

```toml
[scripts]
all = "otaru build --release && otaru exec test"
release = "otaru build --release"
deploy = """
otaru build --release &&
otaru exec test &&
cp build/myapp /usr/local/bin/
echo 'Deployed!'
"""
```

Multi-line strings (triple-quotes) are supported for complex scripts.

## Library Resolution

Libraries in `libraries = [...]` are resolved in this order:

1. **Absolute path** — used directly if the file exists
2. **Relative path** (contains `/`) — resolved against project root
3. **Bare name** (e.g., `"foo"`) — searched in:
   - `lib_dirs` paths
   - `/usr/lib/`, `/usr/local/lib/`, `/lib/`, `/lib64/`
   - `LIBRARY_PATH` environment variable entries
   - `LD_LIBRARY_PATH` environment variable entries

If not found, the name is passed to the linker as-is (equivalent to `-lfoo`).

Libraries in `link = [...]` are always passed as `-l<name>` flags, letting the linker
handle the search via its default paths and `-L` flags.

## LLVM Discovery

For projects linking against LLVM (compilers, language tools), use `llvm-config` to
auto-resolve include dirs, compiler flags, and library flags:

```toml
[build]
llvm-config = "llvm-config-21"              # or full path
llvm-components = ["core", "support", "irreader", "codegen", "mc", "mcparser"]
link = ["pthread", "dl", "m"]
cflags = ["-std=c++17"]
```

otaru runs `llvm-config --cxxflags` to get include dirs and compiler flags, then
`llvm-config --ldflags --libs <components>` to get linker flags and library names.
The resolved flags are merged into your `[build]` configuration automatically.

### Component naming

LLVM component names match the CMake `llvm_map_components_to_libnames` syntax:
- Core components: `support`, `core`, `irreader`, `codegen`, `target`, `mc`, `mcparser`, `asmparser`, `option`
- Target backends: `X86`, `AArch64`, `ARM`, `WebAssembly`, `RISCV`, `Mips`, etc.

Use `llvm-config --components` to list all available components.

## Prebuild Steps

Run a shell command before compiling (e.g., to build a Rust static library):

```toml
[build]
prebuild = "cargo build --release"

[[build.targets]]
name = "mycompiler"
type = "executable"
sources = ["src/main.cpp"]
libraries = ["target/release/libmylib.a"]
```

The prebuild command runs once before building each target. Use this for:
- `cargo build --release` — build a Rust static library
- `make -C vendor` — build an external dependency
- `python generate.py` — code generation
- `flex parser.l` / `bison grammar.y` — parser generation

## Incremental Builds

### Hokkaido

Cache keys are computed from modification times and sizes of:
- All `.hk` files in `src/`, `deps/`, and `std/`
- The `hokkaido` compiler binary

Cache files: `build/.hkbuildcache.{debug,release}`.

### C/C++

Object files are recompiled only when their source file or any included header is newer.
This uses `.d` dependency files generated via `-MMD -MF`.

Both builds have separate debug/release caches. Use `-f` / `--force` to bypass caching.

### Optimization

| Mode | Flag | Compiler flag | Use case |
|------|------|--------------|----------|
| Debug (default) | *(none)* | `-O0` | Fast compilation, no optimizations |
| Release | `--release` / `-r` | `-O2` | Optimized binary, slower compilation |

## Hokkaido + C FFI (details)

### Linking with external C libraries

Declare `extern fn` in your `.hk` code to call C functions. Then list the libraries
in `[build]`:

```toml
[build]
link = ["gtk4", "glib-2.0", "gobject-2.0"]
ldflags = ["-L/usr/local/lib"]
lib_dirs = ["/usr/local/lib"]
```

```ocaml
extern fn gtk_init(argc: *int, argv: **char) -> int
extern fn gtk_window_new() -> *void
extern fn gtk_widget_show(window: *void)

fn main() -> int {
    let argc: int = 0
    gtk_init(&argc, 0 as **char)
    let win = gtk_window_new()
    gtk_widget_show(win)
    return 0
}
```

The resulting link command:
```
clang build/main.o -o build/myapp -L/usr/local/lib -lgtk4 -lglib-2.0 -lgobject-2.0
```

### Compiling C glue code alongside Hokkaido

For complex C APIs, write wrapper code in C and list it in `sources`:

```toml
[build]
sources = ["src/ffi.c", "src/helpers.c"]
include_dirs = ["include"]
link = ["gtk4", "glib-2.0"]
cflags = ["-Wall", "-Wno-unused-parameter"]
```

otaru compiles `.hk` files with the Hokkaido compiler and `.c` files with `cc`,
then links everything together in a single link step.

## Environment Variables

| Variable | Purpose | Used by |
|----------|---------|---------|
| `HOKKAIDO_HOME` | Directory containing the `hokkaido` binary | Hokkaido compiler lookup |
| `HOKKAIDO_CRT_DIR` | C runtime directory for hokkaido | Hokkaido compiler |
| `HOKKAIDO_DYNAMIC_LINKER` | Dynamic linker path for hokkaido | Hokkaido compiler |
| `LIBRARY_PATH` | Extra library search paths | C/C++ linking (passed to linker) |
| `LD_LIBRARY_PATH` | Extra shared library search paths | C/C++ library resolution |
| `PATH` | System binary search path | Compiler and hokkaido lookup |
