# Callback Model

Sapporo uses integer callback IDs for asynchronous operations (events, timers, fetch). This is how the browser notifies your WASM code that something happened.

## How it works

```
1. Your code registers a handler with a callback ID:
   sapporo::on_click("button", 42)

2. When the user clicks the button, sapporo.js calls:
   wasmExports.handle_callback(42)

3. Your WASM module receives the callback and reacts:
   extern fn handle_callback(id: int) -> void
   fn handle_callback(id: int) -> void {
       if id == 42 {
           // button was clicked
       }
   }
```

## Required exports

Your WASM module must export two callback functions:

```ocaml
extern fn handle_callback(id: int) -> void
extern fn handle_fetch(id: int, response: string, success: int) -> void
```

`sapporo.js` calls these functions when:
- **`handle_callback`** — an event fires, a timer completes
- **`handle_fetch`** — a fetch request completes (success=1) or fails (success=0)

## Callback ID convention

There's no built-in constraint on callback IDs — they're just integer values. A common pattern is to define them as constants:

```ocaml
fn handle_callback(id: int) -> void {
    if id == 1 {
        // handle "add todo" button
    }
    if id == 2 {
        // handle "delete todo" button
    }
    if id == 10 {
        // handle search input
    }
}
```

Or use a range-based scheme:

```ocaml
// IDs 1-99: button clicks
// IDs 100-199: input handlers
// IDs 200-299: timers
// IDs 300+: fetch responses
```

## Fetch callbacks

Fetch uses a separate callback function with additional parameters:

```ocaml
fn handle_fetch(id: int, response: string, success: int) -> void {
    if id == 100 {
        if success == 1 {
            // response contains the body text
            sapporo::set_text("result", response)
        } else {
            // response contains the error message
            sapporo::set_text("result", "Error: " + response)
        }
    }
}
```

Parameters:
- `id` — the callback ID you passed to `fetch_get` / `fetch_post` / etc.
- `response` — the response body text (on success) or error message (on failure)
- `success` — `1` for success, `0` for failure

## Keyboard events

Keyboard event callbacks receive the key code as a second argument:

```ocaml
extern fn handle_callback(id: int, keyCode: int) -> void

fn handle_callback(id: int, keyCode: int) -> void {
    if id == 10 {
        // search input
        if keyCode == 13 {
            // Enter key — perform search
        }
    }
}
```

Common key codes:
- 13 = Enter
- 27 = Escape
- 8 = Backspace
- 46 = Delete
- 37-40 = Arrow keys

## Complete example

```ocaml
package main

import "sapporo"

extern fn handle_callback(id: int) -> void
extern fn handle_fetch(id: int, response: string, success: int) -> void

fn main() -> int {
    // Register handlers
    sapporo::on_click("load-btn", 1)
    sapporo::on_input("search", 2)
    sapporo::set_interval(3, 5000)

    return 0
}

fn handle_callback(id: int) -> void {
    if id == 1 {
        // "Load" button clicked — fetch data
        sapporo::fetch_get("https://api.example.com/data", 100)
    }
    if id == 2 {
        // Search input changed
        let query: string = sapporo::get_value("search")
        sapporo::log("Search: " + query)
    }
    if id == 3 {
        // Periodic refresh
        sapporo::log("Tick...")
    }
}

fn handle_fetch(id: int, response: string, success: int) -> void {
    if id == 100 {
        if success == 1 {
            sapporo::set_html("data", response)
        } else {
            sapporo::error("Failed to load: " + response)
        }
    }
}
```
