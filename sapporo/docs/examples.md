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

A minimal todo list with add and clear-all:

```ocaml
package main

import "sapporo"

fn main() -> int {
    sapporo::on_click("add-btn", 1)
    sapporo::on_submit("todo-form", 1)
    sapporo::on_click("clear-btn", 2)
    return 0
}

fn handle_callback(id: int) -> void {
    if id == 1 {
        add_todo()
    }
    if id == 2 {
        clear_all()
    }
}

fn add_todo() -> void {
    let text: string = sapporo::get_value("todo-input")
    if text == "" {
        return
    }

    // Create the list item
    let li: int32 = sapporo::create("li")
    sapporo::add_class(li, "todo-item")

    // Create label with todo text
    let label: int32 = sapporo::create("span")
    sapporo::set_text(label, text)

    // Create delete button
    let del: int32 = sapporo::create("button")
    sapporo::set_text(del, "\u00d7")
    sapporo::add_class(del, "delete")

    // Assemble the list item
    sapporo::append(li, label)
    sapporo::append(li, del)

    // Add to the todo list
    let list: int32 = sapporo::by_id("todo-list")
    sapporo::append(list, li)

    // Clear the input
    sapporo::set_value("todo-input", "")
}

fn clear_all() -> void {
    sapporo::set_children(sapporo::by_id("todo-list"))
}
```

**HTML for the todo app:**

```html
<style>
  .todo-item { list-style: none; padding: 4px 0; }
  .delete { margin-left: 8px; color: #e63946; border: none; background: none; cursor: pointer; }
</style>
<form id="todo-form">
  <input id="todo-input" type="text" placeholder="What needs to be done?">
  <button id="add-btn" type="button">Add</button>
</form>
<button id="clear-btn">Clear all</button>
<ul id="todo-list"></ul>
<script src="sapporo.js"></script>
<script>Sapporo.load("app.wasm").then(m => m.main())</script>
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
