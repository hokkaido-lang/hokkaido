# API Reference

All functions are imported via `import "sapporo"` and called as `sapporo::function_name(...)`.

Element IDs can be passed as strings (looked up by ID) or as integer handles (from `by_id`, `query`, `create`).

---

## Console

### `sapporo::log(message: string) -> int32`

Log a message to the browser console (`console.log`).

### `sapporo::error(message: string) -> int32`

Log an error to the browser console (`console.error`).

### `sapporo::warn(message: string) -> int32`

Log a warning to the browser console (`console.warn`).

### `sapporo::info(message: string) -> int32`

Log an info message to the browser console (`console.info`).

### `sapporo::debug(message: string) -> int32`

Log a debug message to the browser console (`console.debug`).

---

## DOM content

### `sapporo::set_text(id: string, text: string) -> int32`

Set the text content of an element. Clears any child elements.

```ocaml
sapporo::set_text("greeting", "Hello, World!")
```

### `sapporo::get_text(id: string) -> string`

Get the text content of an element.

```ocaml
let name: string = sapporo::get_text("name-input")
```

### `sapporo::set_html(id: string, html: string) -> int32`

Set the inner HTML of an element. **Warning:** only use with trusted content.

```ocaml
sapporo::set_html("content", "<b>Bold text</b>")
```

### `sapporo::get_html(id: string) -> string`

Get the inner HTML of an element.

### `sapporo::set_value(id: string, value: string) -> int32`

Set the value of an input element (`<input>`, `<textarea>`, `<select>`).

```ocaml
sapporo::set_value("username", "alice")
```

### `sapporo::get_value(id: string) -> string`

Get the current value of an input element.

```ocaml
let email: string = sapporo::get_value("email-input")
```

---

## Attributes

### `sapporo::set_attr(id: string, name: string, value: string) -> int32`

Set an HTML attribute on an element.

```ocaml
sapporo::set_attr("link", "href", "https://example.com")
sapporo::set_attr("img", "src", "photo.png")
```

### `sapporo::get_attr(id: string, name: string) -> string`

Get an HTML attribute value.

```ocaml
let href: string = sapporo::get_attr("link", "href")
```

---

## CSS classes

### `sapporo::add_class(id: string, class: string) -> int32`

Add a CSS class to an element.

```ocaml
sapporo::add_class("button", "active")
```

### `sapporo::remove_class(id: string, class: string) -> int32`

Remove a CSS class from an element.

```ocaml
sapporo::remove_class("button", "active")
```

### `sapporo::has_class(id: string, class: string) -> int32`

Check if an element has a CSS class. Returns `1` if yes, `0` if no.

```ocaml
let is_active: int32 = sapporo::has_class("button", "active")
```

### `sapporo::toggle_class(id: string, class: string) -> int32`

Toggle a CSS class on an element. Adds it if absent, removes it if present.

```ocaml
sapporo::toggle_class("sidebar", "collapsed")
```

---

## Styles

### `sapporo::set_style(id: string, prop: string, value: string) -> int32`

Set an inline CSS style property.

```ocaml
sapporo::set_style("box", "background-color", "#ff0000")
sapporo::set_style("box", "font-size", "18px")
```

### `sapporo::get_style(id: string, prop: string) -> string`

Get an inline CSS style property value.

```ocaml
let color: string = sapporo::get_style("box", "color")
```

---

## Element queries

### `sapporo::by_id(id: string) -> int32`

Look up an element by its `id` attribute. Returns an integer handle.

```ocaml
let btn: int32 = sapporo::by_id("submit-button")
```

### `sapporo::query(selector: string) -> int32`

Look up an element by CSS selector. Returns the first match.

```ocaml
let first: int32 = sapporo::query(".todo-item")
```

---

## DOM tree

### `sapporo::create(tag: string) -> int32`

Create a new HTML element. Returns an integer handle.

```ocaml
let div: int32 = sapporo::create("div")
sapporo::set_text("item-1", "Hello")  // sets text on the handle
sapporo::add_class(div, "card")       // wait, need to use the handle directly
```

Note: `create` returns a handle (int32). To use string-based functions on it, you'd need to use the handle directly with the extern functions, or append it to the DOM first and give it an ID.

### `sapporo::append(parentId: int32, childId: int32) -> int32`

Append a child element to a parent.

```ocaml
let parent: int32 = sapporo::by_id("list")
let item: int32 = sapporo::create("li")
sapporo::append(parent, item)
```

### `sapporo::remove(elementId: int32) -> int32`

Remove an element from the DOM.

```ocaml
let el: int32 = sapporo::by_id("old-item")
sapporo::remove(el)
```

### `sapporo::insert_before(parentId: int32, childId: int32, beforeId: int32) -> int32`

Insert a child before a reference element.

```ocaml
let parent: int32 = sapporo::by_id("list")
let new_item: int32 = sapporo::create("li")
let ref: int32 = sapporo::by_id("item-3")
sapporo::insert_before(parent, new_item, ref)
```

### `sapporo::clone(elementId: int32) -> int32`

Deep-clone an element. Returns a handle to the clone.

```ocaml
let template: int32 = sapporo::by_id("template-row")
let clone: int32 = sapporo::clone(template)
```

### `sapporo::set_children(parentId: int32) -> int32`

Remove all children from an element. Useful for clearing a list or container.

```ocaml
let list: int32 = sapporo::by_id("todo-list")
sapporo::set_children(list)  // clears all items
```

Note: `set_children` takes an integer handle. To use it with a string ID, call `sapporo::by_id` first:

```ocaml
sapporo::set_children(sapporo::by_id("todo-list"))
```

---

## Events

See [callbacks.md](callbacks.md) for how callback IDs work.

### `sapporo::on_click(id: string, callbackId: int32) -> int32`

Register a click handler.

```ocaml
sapporo::on_click("submit-btn", 1)
```

### `sapporo::on_dblclick(id: string, callbackId: int32) -> int32`

Register a double-click handler.

### `sapporo::on_input(id: string, callbackId: int32) -> int32`

Register an input handler. Fires on every keystroke/change to the input value.

```ocaml
sapporo::on_input("search-box", 2)
```

### `sapporo::on_change(id: string, callbackId: int32) -> int32`

Register a change handler. Fires when the element loses focus after its value changed (for `<input>`, `<select>`, `<textarea>`).

```ocaml
sapporo::on_change("color-picker", 3)
```

### `sapporo::on_keydown(id: string, callbackId: int32) -> int32`

Register a keydown handler. The callback receives the key code as a second argument.

### `sapporo::on_keyup(id: string, callbackId: int32) -> int32`

Register a keyup handler. The callback receives the key code as a second argument.

### `sapporo::on_submit(id: string, callbackId: int32) -> int32`

Register a form submit handler. Automatically calls `preventDefault()`.

```ocaml
sapporo::on_submit("login-form", 4)
```

### `sapporo::on_focus(id: string, callbackId: int32) -> int32`

Register a focus handler.

### `sapporo::on_blur(id: string, callbackId: int32) -> int32`

Register a blur handler.

### `sapporo::on_mouseenter(id: string, callbackId: int32) -> int32`

Register a mouseenter handler.

### `sapporo::on_mouseleave(id: string, callbackId: int32) -> int32`

Register a mouseleave handler.

---

## Timers

### `sapporo::set_timeout(callbackId: int32, ms: int32) -> int32`

Call a function after `ms` milliseconds. Returns a timer ID.

### `sapporo::set_interval(callbackId: int32, ms: int32) -> int32`

Call a function every `ms` milliseconds. Returns a timer ID.

### `sapporo::clear_timeout(id: int32) -> int32`

Cancel a timeout by its timer ID.

### `sapporo::clear_interval(id: int32) -> int32`

Cancel an interval by its timer ID.

---

## Fetch (HTTP)

All fetch functions call `handle_fetch(callbackId, textPtr, success)` on your WASM module. See [callbacks.md](callbacks.md).

### `sapporo::fetch_get(url: string, callbackId: int32) -> int32`

Perform a GET request.

### `sapporo::fetch_post(url: string, body: string, callbackId: int32) -> int32`

Perform a POST request with a JSON body.

### `sapporo::fetch_put(url: string, body: string, callbackId: int32) -> int32`

Perform a PUT request with a JSON body.

### `sapporo::fetch_delete(url: string, callbackId: int32) -> int32`

Perform a DELETE request.

---

## Local storage

### `sapporo::local_storage_set(key: string, value: string) -> int32`

Store a value in `localStorage`. Returns 1 on success, 0 on failure (e.g., quota exceeded).

```ocaml
sapporo::local_storage_set("theme", "dark")
```

### `sapporo::local_storage_get(key: string) -> string`

Read a value from `localStorage`. Returns empty string if not found.

```ocaml
let theme: string = sapporo::local_storage_get("theme")
```

### `sapporo::local_storage_remove(key: string) -> int32`

Remove a value from `localStorage`.

```ocaml
sapporo::local_storage_remove("theme")
```

---

## Navigation

### `sapporo::navigate(url: string) -> int32`

Navigate to a URL.

```ocaml
sapporo::navigate("https://example.com")
sapporo::navigate("/about")  // relative URL
```

### `sapporo::reload() -> int32`

Reload the current page.

### `sapporo::get_url() -> string`

Get the current page URL.

```ocaml
let url: string = sapporo::get_url()
```

### `sapporo::set_url_hash(hash: string) -> int32`

Set the URL hash fragment (without navigating).

```ocaml
sapporo::set_url_hash("section-2")
```

---

## Viewport

### `sapporo::window_width() -> int32`

Get the browser window width in pixels.

### `sapporo::window_height() -> int32`

Get the browser window height in pixels.

---

## Memory management

### `sapporo::reset_memory() -> int32`

Reset the bump allocator used for string passing. Call this periodically in long-running apps to prevent the WASM linear memory from growing unbounded. After calling this, any previously-allocated strings in the JS side become invalid (but the WASM side handles this transparently).

```ocaml
// In a loop or periodic task:
sapporo::reset_memory()
```

### `sapporo::memory_stats() -> string`

Get a summary of memory usage, element registry size, and ID cache size. Useful for debugging memory issues.

```ocaml
let stats: string = sapporo::memory_stats()
sapporo::log(stats)
// Output: "memory: 1024/65536 bytes, elements: 12, cached IDs: 5"
```

### `sapporo::copy_text(src_id: string, dst_id: string) -> int32`

Copy text content from one element to another. Convenience wrapper that combines `get_text` and `set_text`.

```ocaml
sapporo::copy_text("source", "destination")
```
