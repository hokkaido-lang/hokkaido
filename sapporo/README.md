# Sapporo — Hokkaido Web UI Library

A lightweight library for building web UIs with [Hokkaido](https://github.com/jihoo/hokkaido) compiled to WebAssembly.

Sapporo provides bindings to browser DOM APIs, letting you build interactive web pages in Hokkaido without writing JavaScript.

## How it works

```
┌──────────────┐     WASM imports      ┌──────────────┐
│  Hokkaido    │ ────────────────────>  │  sapporo.js  │ ──> Browser DOM
│  (.hk code)  │ <────────────────────  │  (bridge)    │ <── Events, fetch
└──────────────┘     WASM exports       └──────────────┘
```

1. **`sapporo.js`** loads your WASM module and provides DOM functions as WASM imports
2. **`sapporo/sapporo.hk`** declares those imports and wraps them in a clean API
3. Your `.hk` code calls `sapporo.set_text("id", "hello")` — it just works

## Quick start

### With sapporo CLI (recommended)

```bash
sapporo new my-app
cd my-app
sapporo build
sapporo run
```

### Manual

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
├── docs/
│   └── docs.md         # Full documentation
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

4. Create an HTML page that loads `sapporo.js` and your WASM:

```html
<p id="output">Loading...</p>
<script src="sapporo.js"></script>
<script>
  Sapporo.load("app.wasm").then(m => m.main());
</script>
```

5. Build with:

```bash
hokkaido main.hk -o app --target wasm32-unknown-unknown
wasm-ld --no-entry --export=main --allow-undefined -o app.wasm app.o
```

Or use the sapporo CLI:

```bash
sapporo build   # compiles to dist/
sapporo run     # starts dev server + opens browser
```

## API overview

### Console

| Function | Description |
|----------|-------------|
| `log(message)` | `console.log()` |
| `error(message)` | `console.error()` |
| `warn(message)` | `console.warn()` |
| `info(message)` | `console.info()` |
| `debug(message)` | `console.debug()` |

### DOM manipulation

| Function | Description |
|----------|-------------|
| `set_text(id, text)` | Set text content |
| `get_text(id)` | Get text content |
| `set_html(id, html)` | Set inner HTML |
| `get_html(id)` | Get inner HTML |
| `set_value(id, value)` | Set input value |
| `get_value(id)` | Get input value |
| `set_attr(id, name, value)` | Set an attribute |
| `get_attr(id, name)` | Get an attribute value |

### CSS classes

| Function | Description |
|----------|-------------|
| `add_class(id, className)` | Add a CSS class |
| `remove_class(id, className)` | Remove a CSS class |
| `has_class(id, className)` | Check if element has a class (returns 0 or 1) |
| `toggle_class(id, className)` | Toggle a CSS class |

### Element queries & DOM tree

| Function | Description |
|----------|-------------|
| `by_id(id)` | Get element by ID |
| `query(selector)` | Query selector |
| `create(tag)` | Create a new element |
| `append(parent, child)` | Append a child element |
| `remove(elementId)` | Remove an element from the DOM |
| `insert_before(parent, child, before)` | Insert before a reference element |
| `clone(elementId)` | Clone an element |

### Events

| Function | Description |
|----------|-------------|
| `on_click(id, callbackId)` | Click handler |
| `on_dblclick(id, callbackId)` | Double-click handler |
| `on_input(id, callbackId)` | Input handler (fires on every keystroke) |
| `on_change(id, callbackId)` | Change handler (fires on blur/submit for inputs) |
| `on_keydown(id, callbackId)` | Keydown handler (passes keyCode as 2nd arg) |
| `on_keyup(id, callbackId)` | Keyup handler (passes keyCode as 2nd arg) |
| `on_submit(id, callbackId)` | Submit handler (prevents default) |
| `on_focus(id, callbackId)` | Focus handler |
| `on_blur(id, callbackId)` | Blur handler |
| `on_mouseenter(id, callbackId)` | Mouse enter handler |
| `on_mouseleave(id, callbackId)` | Mouse leave handler |

### Styles

| Function | Description |
|----------|-------------|
| `set_style(id, prop, value)` | Set a CSS style property |
| `get_style(id, prop)` | Get a CSS style property value |

### Timers

| Function | Description |
|----------|-------------|
| `set_timeout(callbackId, ms)` | Call a function after ms (returns timer ID) |
| `set_interval(callbackId, ms)` | Call a function every ms (returns timer ID) |
| `clear_timeout(id)` | Cancel a timeout |
| `clear_interval(id)` | Cancel an interval |

### Fetch (HTTP)

| Function | Description |
|----------|-------------|
| `fetch_get(url, callbackId)` | GET request |
| `fetch_post(url, body, callbackId)` | POST request with JSON body |
| `fetch_put(url, body, callbackId)` | PUT request with JSON body |
| `fetch_delete(url, callbackId)` | DELETE request |

### Local storage

| Function | Description |
|----------|-------------|
| `local_storage_set(key, value)` | Store a value |
| `local_storage_get(key)` | Read a value |
| `local_storage_remove(key)` | Remove a value |

### Navigation

| Function | Description |
|----------|-------------|
| `navigate(url)` | Navigate to a URL |
| `reload()` | Reload the page |
| `get_url()` | Get the current URL |
| `set_url_hash(hash)` | Set the URL hash fragment |

### Viewport

| Function | Description |
|----------|-------------|
| `window_width()` | Get window width in pixels |
| `window_height()` | Get window height in pixels |

### Memory management

| Function | Description |
|----------|-------------|
| `reset_memory()` | Reset the string allocator (call periodically in long-running apps) |

## Callback model

Sapporo uses integer callback IDs for events, timers, and fetch. When you register an event handler, you pass a `callbackId` (an `int32`). When the event fires, `sapporo.js` calls your WASM module's `handle_callback(callbackId)` export.

For fetch, `sapporo.js` calls `handle_fetch(callbackId, textPtr, success)` where `success` is 1 for OK and 0 for error.

You must export these functions from your WASM module:

```ocaml
extern fn handle_callback(id: int32) -> void
extern fn handle_fetch(id: int32, text: string, success: int32) -> void
```

## Browser compatibility

Works in any browser that supports WebAssembly:
- Chrome 57+
- Firefox 52+
- Safari 11+
- Edge 16+

## License

Apache 2.0
