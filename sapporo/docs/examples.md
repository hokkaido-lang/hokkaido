# Examples

## Hello World

The simplest Sapporo app:

```ocaml
package main

import "sapporo"

fn main() -> int {
    sapporo::set_text("output", "Hello from Hokkaido!")
    sapporo::set_style("output", "color", "#e63946")
    return 0
}
```

```html
<p id="output">Loading...</p>
<script src="sapporo.js"></script>
<script>Sapporo.load("app.wasm").then(m => m.main())</script>
```

---

## Todo app

A minimal todo list with add, toggle, and delete:

```ocaml
package main

import "sapporo"

let next_id: int = 1

fn main() -> int {
    sapporo::on_click("add-btn", 1)
    sapporo::on_submit("todo-form", 1)
    return 0
}

fn handle_callback(id: int) -> void {
    if id == 1 {
        add_todo()
    }
}

fn add_todo() -> void {
    let text: string = sapporo::get_value("todo-input")
    if text == "" {
        return
    }

    // Create the list item
    let li: int32 = sapporo::create("li")

    // Create checkbox
    let cb: int32 = sapporo::create("input")
    sapporo::set_attr(cb, "type", "checkbox")
    let cb_id: string = "todo-" + next_id
    sapporo::set_attr(cb, "id", cb_id)

    // Create label
    let label: int32 = sapporo::create("span")
    sapporo::set_text(label, text)

    // Create delete button
    let del: int32 = sapporo::create("button")
    sapporo::set_text(del, "x")
    sapporo::add_class(del, "delete")

    // Assemble
    sapporo::append(li, cb)
    sapporo::append(li, label)
    sapporo::append(li, del)

    // Add to list
    let list: int32 = sapporo::by_id("todo-list")
    sapporo::append(list, li)

    // Clear input
    sapporo::set_value("todo-input", "")

    next_id = next_id + 1
}
```

---

## Fetch API

Loading data from a REST API:

```ocaml
package main

import "sapporo"

fn main() -> int {
    sapporo::on_click("load-btn", 1)
    sapporo::on_click("save-btn", 2)
    return 0
}

fn handle_callback(id: int) -> void {
    if id == 1 {
        sapporo::log("Loading data...")
        sapporo::fetch_get("https://jsonplaceholder.typicode.com/posts/1", 100)
    }
    if id == 2 {
        let data: string = sapporo::get_value("editor")
        sapporo::fetch_post("https://jsonplaceholder.typicode.com/posts", data, 101)
    }
}

fn handle_fetch(id: int, response: string, success: int) -> void {
    if id == 100 {
        if success == 1 {
            sapporo::set_text("output", response)
        } else {
            sapporo::error("Load failed: " + response)
        }
    }
    if id == 101 {
        if success == 1 {
            sapporo::log("Saved!")
            sapporo::set_text("status", "Saved successfully")
        } else {
            sapporo::error("Save failed: " + response)
        }
    }
}
```

---

## Local storage persistence

Saving and loading user preferences:

```ocaml
package main

import "sapporo"

fn main() -> int {
    // Load saved theme
    let theme: string = sapporo::local_storage_get("theme")
    if theme != "" {
        sapporo::set_attr("body", "class", theme)
    }

    // Register theme toggle
    sapporo::on_click("theme-btn", 1)

    // Register counter with persistence
    let saved: string = sapporo::local_storage_get("count")
    if saved != "" {
        sapporo::set_text("count", saved)
    }

    sapporo::on_click("inc-btn", 2)
    return 0
}

fn handle_callback(id: int) -> void {
    if id == 1 {
        sapporo::toggle_class("body", "dark")
        let is_dark: int32 = sapporo::has_class("body", "dark")
        if is_dark == 1 {
            sapporo::local_storage_set("theme", "dark")
        } else {
            sapporo::local_storage_set("theme", "light")
        }
    }
    if id == 2 {
        let current: string = sapporo::get_text("count")
        // Simple increment (note: Hokkaido string arithmetic is limited)
        let new_val: string = current + "1"
        sapporo::set_text("count", new_val)
        sapporo::local_storage_set("count", new_val)
    }
}
```

---

## Responsive layout

Using viewport functions:

```ocaml
package main

import "sapporo"

fn main() -> int {
    check_size()
    sapporo::set_interval(1, 1000)
    return 0
}

fn check_size() -> void {
    let w: int32 = sapporo::window_width()
    let h: int32 = sapporo::window_height()

    if w < 600 {
        sapporo::add_class("layout", "mobile")
    } else {
        sapporo::remove_class("layout", "mobile")
    }
}

fn handle_callback(id: int) -> void {
    if id == 1 {
        check_size()
    }
}
```

---

## Navigation / SPA routing

Using hash-based routing:

```ocaml
package main

import "sapporo"

fn main() -> int {
    sapporo::on_click("nav-home", 1)
    sapporo::on_click("nav-about", 2)
    sapporo::on_click("nav-contact", 3)

    navigate_to(sapporo::get_url())
    return 0
}

fn navigate_to(url: string) -> void {
    // Simple hash-based routing
    if url == "" {
        show_page("home")
    } else {
        show_page("home")
    }
}

fn show_page(page: string) -> void {
    sapporo::set_html("content", "<p>Welcome to " + page + "</p>")
}

fn handle_callback(id: int) -> void {
    if id == 1 {
        sapporo::set_url_hash("home")
        show_page("home")
    }
    if id == 2 {
        sapporo::set_url_hash("about")
        show_page("about")
    }
    if id == 3 {
        sapporo::set_url_hash("contact")
        show_page("contact")
    }
}
```
