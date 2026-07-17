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
    if (!memory) return "";
    const bytes = new Uint8Array(memory.buffer);
    let end = ptr;
    while (bytes[end] !== 0 && end < bytes.length) end++;
    return new TextDecoder().decode(bytes.subarray(ptr, end));
  }

  function writeString(str) {
    if (!memory) return 0;
    const bytes = new TextEncoder().encode(str + "\0");
    const ptr = malloc(bytes.length);
    new Uint8Array(memory.buffer).set(bytes, ptr);
    return ptr;
  }

  // Tiny bump allocator for string passing
  let bumpPtr = 0;
  function malloc(size) {
    const ptr = bumpPtr;
    bumpPtr += size;
    return ptr;
  }

  // --- WASM import functions (env module) ---

  const imports = {
    env: {
      // --- DOM manipulation ---
      sapporo_set_text(elementId, textPtr) {
        const el = elements[elementId];
        const text = readString(textPtr);
        if (el) el.textContent = text;
        return 1;
      },

      sapporo_set_html(elementId, htmlPtr) {
        const el = elements[elementId];
        const html = readString(htmlPtr);
        if (el) el.innerHTML = html;
        return 1;
      },

      sapporo_get_text(elementId) {
        const el = elements[elementId];
        return writeString(el ? el.textContent : "");
      },

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

      sapporo_on_input(elementId, callbackId) {
        const el = elements[elementId];
        if (!el) return 0;
        el.addEventListener("input", () => {
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

      // --- Styles ---
      sapporo_set_style(elementId, propPtr, valuePtr) {
        const el = elements[elementId];
        if (!el) return 0;
        el.style[readString(propPtr)] = readString(valuePtr);
        return 1;
      },

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

      // --- Timers ---
      sapporo_set_timeout(callbackId, ms) {
        setTimeout(() => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        }, ms);
        return 1;
      },

      sapporo_set_interval(callbackId, ms) {
        setInterval(() => {
          if (wasmExports && wasmExports.handle_callback)
            wasmExports.handle_callback(callbackId);
        }, ms);
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
    },
  };

  // --- Public API ---

  async function load(wasmPath) {
    const response = await fetch(wasmPath);
    const bytes = await response.arrayBuffer();
    const module = await WebAssembly.instantiate(bytes, imports);

    memory = module.instance.exports.memory;
    wasmExports = module.instance.exports;

    return module.instance.exports;
  }

  return { load, imports };
})();

if (typeof module !== "undefined") module.exports = Sapporo;
