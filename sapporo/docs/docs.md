# Sapporo Documentation

Sapporo is a lightweight library for building web UIs with the Hokkaido programming language compiled to WebAssembly.

## Table of contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Getting started](#getting-started)
- [Building](#building)
- [Callback model](callbacks.md)
- [API reference](api.md)
- [Examples](examples.md)

## Overview

Sapporo bridges Hokkaido (a language that compiles to WASM) and the browser DOM. It consists of two files:

| File | Language | Purpose |
|------|----------|---------|
| `sapporo.js` | JavaScript | Loads WASM, manages memory, provides DOM functions as WASM imports |
| `sapporo/sapporo.hk` | Hokkaido | Declares WASM imports, wraps them in a clean `sapporo::*` API |

Your Hokkaido code calls `sapporo.set_text("output", "hello")`. Under the hood:
1. `set_text` calls `sapporo_set_text(sapporo_by_id(id), text)` (the extern import)
2. `sapporo.js` receives the call, looks up the element, sets its `textContent`
3. The browser updates the DOM

You never write JavaScript. You never touch the DOM directly. It all goes through the WASM import/export bridge.

## Architecture

```
┌─────────────────────────────────────────────────┐
│                   Browser                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │
│  │ DOM      │  │ Storage  │  │ Fetch / HTTP │  │
│  └────┬─────┘  └────┬─────┘  └──────┬───────┘  │
│       │              │               │           │
│  ┌────┴──────────────┴───────────────┴───────┐  │
│  │            sapporo.js                      │  │
│  │  - WASM loader                             │  │
│  │  - String marshalling (null-terminated)    │  │
│  │  - Element registry (int → DOM element)    │  │
│  │  - Bump allocator for string passing       │  │
│  │  - Event dispatch (callbackId → WASM)      │  │
│  └────────────────────┬───────────────────────┘  │
│                       │ WASM imports/exports     │
│  ┌────────────────────┴───────────────────────┐  │
│  │          Your Hokkaido WASM module          │  │
│  │  import "sapporo"                           │  │
│  │  sapporo::set_text("id", "hello")           │  │
│  │  sapporo::on_click("btn", 1)               │  │
│  └────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

### Memory model

Hokkaido passes strings as null-terminated C strings (pointers into WASM linear memory). `sapporo.js` handles the conversion:

- **JS → WASM**: `writeString()` encodes the string, writes it to linear memory at the bump pointer, returns the pointer
- **WASM → JS**: `readString(ptr)` reads bytes from linear memory until it finds the null terminator

The bump allocator starts after the static data region (offset 65536). Call `sapporo.reset_memory()` periodically in long-running apps to prevent the allocator from growing unbounded.

### Element registry

`sapporo.js` maintains an `elements[]` array that maps integer IDs to DOM elements:

- Index 0 is reserved as a null sentinel
- `sapporo_by_id("foo")` calls `document.getElementById("foo")` and pushes the result into the array
- The returned integer is used as a handle in all subsequent calls
- A cache (`Map<string, number>`) avoids duplicate entries when the same element ID is looked up repeatedly
- Elements are never garbage-collected from the registry (this is by design — it keeps handles stable)

### Callback model

See [callbacks.md](callbacks.md) for full details on how events, timers, and fetch callbacks work.

## Getting started

### Option 1: sapporo CLI (recommended)

```bash
sapporo new my-app
cd my-app
sapporo build
sapporo run
```

This creates a ready-to-go project with:
- `sapporo.toml` — build configuration
- `src/main.hk` — starter code
- `index.html` — HTML host page
- `hk.mod` — module root marker

### Option 2: manual setup

1. Copy `sapporo.js` and the `sapporo/` directory into your project
2. Create `hk.mod` (empty file) at project root
3. Create `src/main.hk`:

```ocaml
package main

import "sapporo"

fn main() -> int {
    sapporo::set_text("output", "Hello from Hokkaido!")
    return 0
}
```

4. Create `index.html`:

```html
<!DOCTYPE html>
<html>
<body>
  <p id="output">Loading...</p>
  <script src="sapporo.js"></script>
  <script>
    Sapporo.load("main.wasm").then(m => m.main());
  </script>
</body>
</html>
```

## Building

### Using sapporo CLI

```bash
sapporo build          # incremental build to dist/
sapporo build --force  # force rebuild
sapporo run            # build + serve + open browser
```

### Manual build

```bash
# Step 1: Compile .hk to .o (WASM object)
hokkaido src/main.hk -o dist/main --target wasm32-unknown-unknown

# Step 2: Link .o to .wasm
wasm-ld --no-entry --export=main --allow-undefined -o dist/main.wasm dist/main.o

# Step 3: Copy runtime files to dist/
cp sapporo.js dist/
cp index.html dist/
```

### Build flags

The sapporo CLI supports custom flags in `sapporo.toml`:

```toml
[build]
sources = ["src"]
dist = "dist"
cflags = ["--some-flag"]    # extra hokkaido flags
ldflags = ["--some-flag"]   # extra wasm-ld flags
```

## Next

- [Callback model](callbacks.md) — how events, timers, and fetch work
- [API reference](api.md) — complete list of all functions
- [Examples](examples.md) — real-world usage patterns
