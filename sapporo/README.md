# Sapporo — Hokkaido Web UI Library

A lightweight library for building web UIs with [Hokkaido](https://github.com/hokkaido-lang/hokkaido) compiled to WebAssembly.

Sapporo provides bindings to browser DOM APIs, letting you build interactive web pages in Hokkaido without writing JavaScript.

## How it works

```
┌──────────────┐     WASM imports      ┌──────────────┐
│  Hokkaido    │ ────────────────────>  │  sapporo.js  │ ──> Browser DOM
│  (.hk code)  │ <────────────────────  │  (bridge)    │ <── Events, fetch
└──────────────┘     WASM exports       └──────────────┘
```

1. **`sapporo.js`** loads your WASM module and provides DOM functions as WASM imports
2. **`lib/sapporo.hk`** declares those imports and wraps them in a clean API
3. Your `.hk` code calls `sapporo.set_text("id", "hello")` — it just works

## Quick start

```bash
cd examples/hello
./build.sh
python3 -m http.server 8080
open http://localhost:8080
```

## Project structure

```
sapporo/
├── hk.mod              # Module root marker
├── sapporo.js          # JavaScript loader + DOM bindings
├── sapporo/
│   └── sapporo.hk      # Hokkaido API
└── examples/
    └── hello/
        ├── main.hk     # Example app
        ├── index.html  # HTML host page
        └── build.sh    # Build script
```

## Using in your project

1. Copy `sapporo.js` and the inner `sapporo/` directory into your project
2. Place an `hk.mod` file (can be empty) at your project root
3. Import the library in your Hokkaido code:

```ocaml
import "sapporo"

fn main() -> int {
    sapporo.set_text("output", "Hello, World!")
    return 0
}
```

3. Create an HTML page that loads `sapporo.js` and your WASM:

```html
<p id="output">Loading...</p>
<script src="sapporo.js"></script>
<script>
  Sapporo.load("app.wasm").then(m => m.main());
</script>
```

4. Build with:

```bash
hokkaido main.hk -o app --target wasm32-unknown-unknown
wasm-ld --no-entry --export=main --allow-undefined -o app.wasm app.o
```

## API reference

### DOM manipulation

| Function | Description |
|----------|-------------|
| `set_text(id, content)` | Set text content of an element |
| `set_html(id, html)` | Set inner HTML of an element |
| `get_text(id)` | Get text content of an element |
| `set_attr(id, name, value)` | Set an attribute |
| `get_attr(id, name)` | Get an attribute value |
| `add_class(id, className)` | Add a CSS class |
| `remove_class(id, className)` | Remove a CSS class |
| `has_class(id, className)` | Check if element has a class (returns 0 or 1) |

### Element queries

| Function | Description |
|----------|-------------|
| `by_id(id)` | Get element by ID (returns element handle) |
| `query(selector)` | Query selector (returns element handle) |
| `create(tag)` | Create a new element (returns element handle) |
| `append(parent, child)` | Append a child element |
| `remove(elementId)` | Remove an element from the DOM |

### Events

| Function | Description |
|----------|-------------|
| `on_click(id, callbackId)` | Register a click handler |
| `on_input(id, callbackId)` | Register an input handler |
| `on_keydown(id, callbackId)` | Register a keydown handler |
| `on_submit(id, callbackId)` | Register a submit handler |

### Styles

| Function | Description |
|----------|-------------|
| `set_style(id, prop, value)` | Set a CSS style property |

### Console

| Function | Description |
|----------|-------------|
| `log(message)` | `console.log()` |
| `error(message)` | `console.error()` |
| `warn(message)` | `console.warn()` |

### Timers

| Function | Description |
|----------|-------------|
| `set_timeout(callbackId, ms)` | Call a function after `ms` milliseconds |
| `set_interval(callbackId, ms)` | Call a function every `ms` milliseconds |

### Fetch

| Function | Description |
|----------|-------------|
| `fetch_get(url, callbackId)` | GET request, calls callback with response text |
| `fetch_post(url, body, callbackId)` | POST request with JSON body |

## Example

```ocaml
import "sapporo"

fn main() -> int {
    # Set text on an element
    sapporo.set_text("output", "Hello from Hokkaido!")

    # Style it
    sapporo.set_style("output", "color", "#e63946")

    # Log to console
    sapporo.log("App loaded!")

    return 0
}
```

With this HTML:

```html
<p id="output">Loading...</p>
<script src="sapporo.js"></script>
<script>Sapporo.load("app.wasm").then(m => m.main())</script>
```

## How strings work

Hokkaido passes strings as null-terminated C strings (pointers to WASM linear memory). `sapporo.js` handles the conversion between JavaScript strings and WASM memory automatically.

You don't need to worry about this — just pass `str` values to the API functions.

## Browser compatibility

Works in any browser that supports WebAssembly:
- Chrome 57+
- Firefox 52+
- Safari 11+
- Edge 16+

## License

Apache 2.0
