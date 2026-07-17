// Sapporo — Hokkaido Web UI Library
// JavaScript loader + DOM bindings for WASM
//
// This file provides the bridge between browser APIs and Hokkaido WASM modules.
// Import it before your WASM script tag:
//   <script src="sapporo.js"></script>
//   <script>Sapporo.load("app.wasm").then(m => m.main())</script>

const Sapporo = (() => {
  let memory = null;
  let elements = [null]; // index 0 = null sentinel
  let wasmExports = null;

  // --- Memory helpers ---

  function readString(ptr) {
    if (!memory || ptr === 0) return "";
    const bytes = new Uint8Array(memory.buffer);
    let end = ptr;
    while (end < bytes.length && bytes[end] !== 0) end++;
    return new TextDecoder().decode(bytes.subarray(ptr, end));
  }

  function writeString(str) {
    if (!memory) return 0;
    const bytes = new TextEncoder().encode(str + "\0");
    const ptr = malloc(bytes.length);
    new Uint8Array(memory.buffer).set(bytes, ptr);
    return ptr;
  }

  // Bump allocator for string passing
  let bumpPtr = 0;
  function malloc(size) {
    const ptr = bumpPtr;
    bumpPtr += size;
    return ptr;
  }

  // Timer ID tracking for clear_timeout/clear_interval
  const timerIds = {};

  // --- WASM import functions (env module) ---

  const imports = {
    env: {
      // --- Console ---

      sapporo_log(ptr) {
        console.log(readString(ptr));
        return 1;
      },

      sapporo_error(ptr) {
        console.error(readString(ptr));
        return 1;
      },

      sapporo_warn(ptr) {
        console.warn(readString(ptr));
        return 1;
      },

      sapporo_info(ptr) {
        console.info(readString(ptr));
        return 1;
      },

      sapporo_debug(ptr) {
        console.debug(readString(ptr));
        return 1;
      },

      // --- DOM text/content ---

      sapporo_set_text(elementId, textPtr) {
        const el = elements[elementId];
        if (el) el.textContent = readString(textPtr);
        return 1;
      },

      sapporo_get_text(elementId) {
        const el = elements[elementId];
        return writeString(el ? el.textContent : "");
      },

      sapporo_set_html(elementId, htmlPtr) {
        const el = elements[elementId];
        if (el) el.innerHTML = readString(htmlPtr);
        return 1;
      },

      sapporo_get_html(elementId) {
        const el = elements[elementId];
        return writeString(el ? el.innerHTML : "");
      },

      sapporo_set_value(elementId, valuePtr) {
        const el = elements[elementId];
        if (el) el.value = readString(valuePtr);
        return 1;
      },

      sapporo_get_value(elementId) {
        const el = elements[elementId];
        return writeString(el ? (el.value || "") : "");
      },

      // --- Attributes ---

      sapporo_set_attr(elementId, namePtr, valuePtr) {
        const el = elements[elementId];
        if (!el) return 0;
        el.setAttribute(readString(namePtr), readString(valuePtr));
        return 1;
      },

      sapporo_get_attr(elementId, namePtr) {
        const el = elements[elementId];
        if (!el) return writeString("");
        return writeString(el.getAttribute(readString(namePtr)) || "");
      },

      // --- CSS classes ---

      sapporo_add_class(elementId, classPtr) {
        const el = elements[elementId];
        if (el) el.classList.add(readString(classPtr));
        return 1;
      },

      sapporo_remove_class(elementId, classPtr) {
        const el = elements[elementId];
        if (el) el.classList.remove(readString(classPtr));
        return 1;
      },

      sapporo_has_class(elementId, classPtr) {
        const el = elements[elementId];
        if (!el) return 0;
        return el.classList.contains(readString(classPtr)) ? 1 : 0;
      },

      sapporo_toggle_class(elementId, classPtr) {
        const el = elements[elementId];
        if (!el) return 0;
        el.classList.toggle(readString(classPtr));
        return 1;
      },

      // --- Element queries ---

      sapporo_by_id(idPtr) {
        const id = readString(idPtr);
        const el = document.getElementById(id);
        if (!el) return 0;
        elements.push(el);
        return elements.length - 1;
      },

      sapporo_query(selectorPtr) {
        const el = document.querySelector(readString(selectorPtr));
        if (!el) return 0;
        elements.push(el);
        return elements.length - 1;
      },

      // --- Element creation ---

      sapporo_create(tagPtr) {
        const el = document.createElement(readString(tagPtr));
        elements.push(el);
        return elements.length - 1;
      },

      sapporo_append(parentId, childId) {
        const parent = elements[parentId];
        const child = elements[childId];
        if (parent && child) parent.appendChild(child);
        return 1;
      },

      sapporo_remove(elementId) {
        const el = elements[elementId];
        if (el && el.parentNode) el.parentNode.removeChild(el);
        return 1;
      },

      sapporo_insert_before(parentId, childId, beforeId) {
        const parent = elements[parentId];
        const child = elements[childId];
        const before = elements[beforeId];
        if (parent && child && before) parent.insertBefore(child, before);
        return 1;
      },

      sapporo_clone(elementId) {
        const el = elements[elementId];
        if (!el) return 0;
        const clone = el.cloneNode(true);
        elements.push(clone);
        return elements.length - 1;
      },

      sapporo_set_children(parentId) {
        const parent = elements[parentId];
        if (parent) parent.textContent = "";
        return 1;
      },

      // --- Events ---

      sapporo_on_click(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("click", () => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        });
        return 1;
      },

      sapporo_on_dblclick(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("dblclick", () => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        });
        return 1;
      },

      sapporo_on_input(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("input", () => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        });
        return 1;
      },

      sapporo_on_change(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("change", () => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        });
        return 1;
      },

      sapporo_on_keydown(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("keydown", (e) => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId, e.keyCode);
        });
        return 1;
      },

      sapporo_on_keyup(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("keyup", (e) => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId, e.keyCode);
        });
        return 1;
      },

      sapporo_on_submit(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("submit", (e) => {
          e.preventDefault();
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        });
        return 1;
      },

      sapporo_on_focus(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("focus", () => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        });
        return 1;
      },

      sapporo_on_blur(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("blur", () => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        });
        return 1;
      },

      sapporo_on_mouseenter(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("mouseenter", () => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        });
        return 1;
      },

      sapporo_on_mouseleave(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("mouseleave", () => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        });
        return 1;
      },

      // --- Styles ---

      sapporo_set_style(elementId, propPtr, valuePtr) {
        const el = elements[elementId];
        if (!el) return 0;
        el.style[readString(propPtr)] = readString(valuePtr);
        return 1;
      },

      sapporo_get_style(elementId, propPtr) {
        const el = elements[elementId];
        if (!el) return writeString("");
        return writeString(el.style[readString(propPtr)] || "");
      },

      // --- Timers ---

      sapporo_set_timeout(callbackId, ms) {
        const id = setTimeout(() => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        }, ms);
        return id;
      },

      sapporo_set_interval(callbackId, ms) {
        const id = setInterval(() => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        }, ms);
        return id;
      },

      sapporo_clear_timeout(id) {
        clearTimeout(id);
        return 1;
      },

      sapporo_clear_interval(id) {
        clearInterval(id);
        return 1;
      },

      // --- Fetch ---

      sapporo_fetch_get(urlPtr, callbackId) {
        fetch(readString(urlPtr))
          .then(r => r.text())
          .then(text => {
            if (wasmExports && wasmExports.handle_fetch)
              wasmExports.handle_fetch(callbackId, writeString(text), 1);
          })
          .catch(err => {
            if (wasmExports && wasmExports.handle_fetch)
              wasmExports.handle_fetch(callbackId, writeString(err.message), 0);
          });
        return 1;
      },

      sapporo_fetch_post(urlPtr, bodyPtr, callbackId) {
        fetch(readString(urlPtr), {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: readString(bodyPtr),
        })
          .then(r => r.text())
          .then(text => {
            if (wasmExports && wasmExports.handle_fetch)
              wasmExports.handle_fetch(callbackId, writeString(text), 1);
          })
          .catch(err => {
            if (wasmExports && wasmExports.handle_fetch)
              wasmExports.handle_fetch(callbackId, writeString(err.message), 0);
          });
        return 1;
      },

      sapporo_fetch_put(urlPtr, bodyPtr, callbackId) {
        fetch(readString(urlPtr), {
          method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: readString(bodyPtr),
        })
          .then(r => r.text())
          .then(text => {
            if (wasmExports && wasmExports.handle_fetch)
              wasmExports.handle_fetch(callbackId, writeString(text), 1);
          })
          .catch(err => {
            if (wasmExports && wasmExports.handle_fetch)
              wasmExports.handle_fetch(callbackId, writeString(err.message), 0);
          });
        return 1;
      },

      sapporo_fetch_delete(urlPtr, callbackId) {
        fetch(readString(urlPtr), { method: "DELETE" })
          .then(r => r.text())
          .then(text => {
            if (wasmExports && wasmExports.handle_fetch)
              wasmExports.handle_fetch(callbackId, writeString(text), 1);
          })
          .catch(err => {
            if (wasmExports && wasmExports.handle_fetch)
              wasmExports.handle_fetch(callbackId, writeString(err.message), 0);
          });
        return 1;
      },

      // --- Local Storage ---

      sapporo_local_storage_set(keyPtr, valuePtr) {
        try {
          localStorage.setItem(readString(keyPtr), readString(valuePtr));
          return 1;
        } catch (e) {
          return 0;
        }
      },

      sapporo_local_storage_get(keyPtr) {
        try {
          const val = localStorage.getItem(readString(keyPtr));
          return writeString(val !== null ? val : "");
        } catch (e) {
          return writeString("");
        }
      },

      sapporo_local_storage_remove(keyPtr) {
        try {
          localStorage.removeItem(readString(keyPtr));
          return 1;
        } catch (e) {
          return 0;
        }
      },

      // --- Navigation ---

      sapporo_navigate(urlPtr) {
        window.location.href = readString(urlPtr);
        return 1;
      },

      sapporo_reload() {
        window.location.reload();
        return 1;
      },

      sapporo_get_url() {
        return writeString(window.location.href);
      },

      sapporo_set_url_hash(hashPtr) {
        window.location.hash = readString(hashPtr);
        return 1;
      },

      // --- Viewport ---

      sapporo_window_width() {
        return window.innerWidth;
      },

      sapporo_window_height() {
        return window.innerHeight;
      },

      // --- Memory management ---

      sapporo_reset_memory() {
        bumpPtr = 0;
        return 1;
      },
    },
  };

  // --- Public API ---

  async function load(wasmPath) {
    const response = await fetch(wasmPath);
    const bytes = await response.arrayBuffer();
    const module = await WebAssembly.instantiate(bytes, imports);

    memory = module.instance.exports.memory;
    wasmExports = module.instance.exports;

    // Initialize bump allocator past any static data
    if (memory) {
      bumpPtr = memory.buffer.byteLength > 65536 ? 65536 : 0;
    }

    return module.instance.exports;
  }

  return { load, imports };
})();

if (typeof module !== "undefined") module.exports = Sapporo;
