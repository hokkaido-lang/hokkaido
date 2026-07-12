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
```

Creates a new Hokkaido project with the standard directory structure:

```
myapp/
  otaru.toml        # project manifest
  src/
    main.hk         # entry point (package main)
  hk.mod            # module root marker
```

### Building

| Command | Description |
|---------|-------------|
| `otaru build` | Build the project |
| `otaru build --release` / `-r` | Build with `-O2` optimizations |
| `otaru build -f` | Force rebuild, ignoring cache |
| `otaru build <file.hk>` | Compile a single file |
| `otaru build --freestanding` | Build without CRT/libc |

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
[project]
name = "myapp"
version = "0.1.0"
type = "hokkaido"    # "hokkaido" (default), "c", or "cpp"

[src]
dir = "src"
entry = "main.hk"    # entry point (default: "main.hk" or "main.cpp")

[build]
flags = []            # extra compiler flags
link = []             # extra linker flags / libraries (e.g., ["-lm", "-lpthread"])
freestanding = false  # set true for freestanding builds

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

## Mixed Hokkaido + C projects

For projects that combine Hokkaido and C code, otaru compiles `.hk` files with the
Hokkaido compiler and `.c`/`.cpp` files with the system C compiler, then links them
together.

```toml
[project]
name = "mixed"
type = "hokkaido"

[src]
dir = "src"
entry = "main.hk"

[build]
link = ["-lm"]
```

Place `.hk` and `.c` files together in `src/` — otaru routes each file to the
correct compiler.

## C/C++ projects

otaru works as a Make replacement for pure C/C++ projects:

```toml
[project]
name = "mylib"
type = "cpp"

[src]
dir = "src"
entry = "main.cpp"

[build]
flags = ["-std=c++17", "-Wall"]
link = ["-lpthread"]
```

Multi-target builds are supported — specify `--target <name>` to build a specific target.
