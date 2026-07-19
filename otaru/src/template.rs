use std::fs;
use std::path::Path;

use crate::manifest::{Build, Manifest, Package};
use crate::utils::{find_sapporo_hk, find_sapporo_js, find_std_dir};

/// Copy a directory recursively.
pub fn copy_dir(src: &Path, dst: &Path) -> std::io::Result<()> {
    fs::create_dir_all(dst)?;
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let ty = entry.file_type()?;
        if ty.is_dir() {
            copy_dir(&entry.path(), &dst.join(entry.file_name()))?;
        } else {
            fs::copy(entry.path(), dst.join(entry.file_name()))?;
        }
    }
    Ok(())
}

/// Copy the std library into the project, printing status.
pub fn prepare_std(project_dir: &Path) {
    if let Some(src) = find_std_dir() {
        let dst = project_dir.join("std");
        if !dst.exists() {
            if let Err(e) = copy_dir(&src, &dst) {
                eprintln!("Warning: could not copy std library: {}", e);
            } else {
                println!("  std/ prepared");
            }
        }
    } else {
        eprintln!("Warning: std/ directory not found (stdlib features unavailable)");
        eprintln!("Hint: set HOKKAIDO_STD=/path/to/hokkaido/std or install via Nix");
    }
}

/// Create all project files for a new or initialized project.
pub fn create_project_files(project_dir: &Path, name: &str, web: bool, wasm: bool) {
    let build_kind = if web {
        "web"
    } else if wasm {
        "wasm"
    } else {
        ""
    };

    let manifest = Manifest {
        package: Package {
            name: name.to_string(),
            version: "0.1.0".to_string(),
            authors: vec![],
            edition: "2024".to_string(),
        },
        dependencies: Default::default(),
        build: if !build_kind.is_empty() {
            Some(Build {
                kind: build_kind.to_string(),
                ..Default::default()
            })
        } else {
            None
        },
        scripts: Default::default(),
    };

    manifest
        .save(&project_dir.join("otaru.toml"))
        .unwrap_or_else(|e| {
            eprintln!("{}", e);
            std::process::exit(1);
        });

    if web {
        write_web_main_hk(project_dir, name);
        write_index_html(project_dir, name);
        write_gitignore(project_dir);
        write_sapporo_hk_to(project_dir);
        write_sapporo_js_to(project_dir);
    } else {
        write_generic_main_hk(project_dir);
    }

    let hk_mod = project_dir.join("hk.mod");
    if !hk_mod.exists() {
        fs::write(&hk_mod, "").unwrap_or_else(|e| {
            eprintln!("Error writing hk.mod: {}", e);
            std::process::exit(1);
        });
    }

    prepare_std(project_dir);

    if wasm && !web {
        let wasm_dir = project_dir.join("wasm32");
        fs::create_dir_all(&wasm_dir).unwrap_or_else(|e| {
            eprintln!("Error creating wasm32/ directory: {}", e);
            std::process::exit(1);
        });
        write_wasm_index_html(&wasm_dir);
        write_build_sh(&wasm_dir);
        write_serve_sh(&wasm_dir);
    }
}

fn write_generic_main_hk(project_dir: &Path) {
    let main_hk = project_dir.join("src/main.hk");
    if !main_hk.exists() {
        fs::write(&main_hk, "fn main() -> int {\n    return 42\n}\n").unwrap_or_else(|e| {
            eprintln!("Error writing src/main.hk: {}", e);
            std::process::exit(1);
        });
    }
}

/// Write sapporo.hk to a directory's sapporo/ subdirectory for compiler import resolution.
/// Used by both scaffolding and cbuild (web builds).
pub fn write_sapporo_hk_to(project_dir: &Path) {
    let src = find_sapporo_hk().unwrap_or_else(|| {
        eprintln!("Error: sapporo.hk not found.");
        eprintln!("Install sapporo library or set HOKKAIDO_SAPPORO to the sapporo directory.");
        std::process::exit(1);
    });

    let hk_dir = project_dir.join("sapporo");
    let hk_file = hk_dir.join("sapporo.hk");

    fs::create_dir_all(&hk_dir).unwrap_or_else(|e| {
        eprintln!("Error creating {}: {}", hk_dir.display(), e);
        std::process::exit(1);
    });

    fs::copy(&src, &hk_file).unwrap_or_else(|e| {
        eprintln!("Error copying {}: {}", src.display(), e);
        std::process::exit(1);
    });

    let hkmod = hk_dir.join("hk.mod");
    if !hkmod.exists() {
        fs::write(&hkmod, "").unwrap_or(());
    }
}

/// Write sapporo.js to a directory.
/// Used by both scaffolding and cbuild (web builds).
pub fn write_sapporo_js_to(dest_dir: &Path) {
    let src = find_sapporo_js().unwrap_or_else(|| {
        eprintln!("Error: sapporo.js not found.");
        eprintln!("Install sapporo library or set HOKKAIDO_SAPPORO to the sapporo directory.");
        std::process::exit(1);
    });

    let dst = dest_dir.join("sapporo.js");
    fs::copy(&src, &dst).unwrap_or_else(|e| {
        eprintln!("Error copying sapporo.js: {}", e);
        std::process::exit(1);
    });
}

/// Write index.html with Sapporo.load() pattern.
fn write_index_html(project_dir: &Path, name: &str) {
    fs::write(project_dir.join("index.html"), generate_index_html(name)).unwrap_or_else(|e| {
        eprintln!("Error writing index.html: {}", e);
        std::process::exit(1);
    });
}

/// Write .gitignore for web projects.
fn write_gitignore(project_dir: &Path) {
    fs::write(project_dir.join(".gitignore"), "dist/\nsapporo/\nnode_modules/\n").unwrap_or_else(
        |e| {
            eprintln!("Error writing .gitignore: {}", e);
            std::process::exit(1);
        },
    );
}

/// Write src/main.hk with sapporo DOM imports.
fn write_web_main_hk(project_dir: &Path, name: &str) {
    let main_hk = project_dir.join("src/main.hk");
    if !main_hk.exists() {
        fs::write(&main_hk, generate_main_hk(name)).unwrap_or_else(|e| {
            eprintln!("Error writing src/main.hk: {}", e);
            std::process::exit(1);
        });
    }
}

fn generate_index_html(name: &str) -> String {
    format!(
        r#"<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{name}</title>
<style>
  body {{ font-family: system-ui, sans-serif; max-width: 640px; margin: 2rem auto; padding: 0 1rem; }}
  #output {{ margin: 1rem 0; font-size: 24px; color: #e63946; }}
</style>
</head>
<body>
<h1>{name}</h1>
<p id="output">Loading...</p>
<script src="sapporo.js"></script>
<script>
  Sapporo.load("{name}.wasm").then(m => {{
    m.main();
  }}).catch(err => {{
    document.getElementById("output").textContent = "Error: " + err.message;
  }});
</script>
</body>
</html>
"#
    )
}

fn generate_main_hk(name: &str) -> String {
    format!(
        r#"package main

import "sapporo"

fn main() -> int {{
    sapporo::log("Hello from {name}!")

    sapporo::set_text("output", "Hello, {name}!")

    return 0
}}
"#
    )
}

// =========================================================================
// Legacy wasm templates (--wasm flag, non-web)
// =========================================================================

fn write_wasm_index_html(project_dir: &Path) {
    fs::write(project_dir.join("index.html"), WASM_INDEX_HTML).unwrap_or_else(|e| {
        eprintln!("Error writing index.html: {}", e);
        std::process::exit(1);
    });
}

fn write_build_sh(project_dir: &Path) {
    fs::write(project_dir.join("build.sh"), BUILD_SH).unwrap_or_else(|e| {
        eprintln!("Error writing build.sh: {}", e);
        std::process::exit(1);
    });
    make_executable(&project_dir.join("build.sh"));
}

fn write_serve_sh(project_dir: &Path) {
    fs::write(project_dir.join("serve.sh"), SERVE_SH).unwrap_or_else(|e| {
        eprintln!("Error writing serve.sh: {}", e);
        std::process::exit(1);
    });
    make_executable(&project_dir.join("serve.sh"));
}

fn make_executable(path: &Path) {
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let _ = fs::set_permissions(path, fs::Permissions::from_mode(0o755));
    }
}

const WASM_INDEX_HTML: &str = r#"<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Hokkaido WebAssembly</title>
<style>
  body { font-family: system-ui, sans-serif; max-width: 640px; margin: 2rem auto; padding: 0 1rem; }
  #status { color: #666; }
  #result { font-size: 1.5rem; font-weight: bold; }
  #error { color: #c00; white-space: pre-wrap; }
</style>
</head>
<body>
<h1>Hokkaido WebAssembly</h1>
<p id="status">Loading module...</p>
<p id="result"></p>
<p id="error"></p>
<script>
async function main() {
  try {
    const resp = await fetch("main.wasm");
    if (!resp.ok) throw new Error("Failed to load main.wasm: " + resp.status);
    const bytes = await resp.arrayBuffer();
    const { instance } = await WebAssembly.instantiate(bytes);
    const result = instance.exports.main();
    document.getElementById("status").textContent = "Program returned:";
    document.getElementById("result").textContent = result;
  } catch (e) {
    document.getElementById("status").textContent = "";
    document.getElementById("error").textContent = e.message;
  }
}
main();
</script>
</body>
</html>"#;

const BUILD_SH: &str = r#"#!/bin/sh
# build.sh - compile hokkaido to WebAssembly
# This is a standalone script. You can also use: otaru build
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

mkdir -p "$BUILD_DIR"

HOKKAIDO="hokkaido"
if ! command -v "$HOKKAIDO" >/dev/null 2>&1; then
  if [ -x "$ROOT_DIR/../build/hokkaido" ]; then
    HOKKAIDO="$ROOT_DIR/../build/hokkaido"
  elif [ -x "$ROOT_DIR/build/hokkaido" ]; then
    HOKKAIDO="$ROOT_DIR/build/hokkaido"
  else
    echo "Error: hokkaido not found. Add it to PATH or build from source."
    exit 1
  fi
fi

echo "Compiling src/main.hk -> $BUILD_DIR/main.o"
"$HOKKAIDO" "$ROOT_DIR/src/main.hk" -o "$BUILD_DIR/main" --target wasm32-unknown-unknown

WASM_LD=""
for candidate in wasm-ld wasm-ld-19 wasm-ld-18 wasm-ld-17; do
  if command -v "$candidate" >/dev/null 2>&1; then
    WASM_LD="$candidate"
    break
  fi
done
if [ -z "$WASM_LD" ]; then
  echo "Error: wasm-ld not found. Install LLVM (e.g. nix-shell -p llvmPackages_19.lld)."
  exit 1
fi

echo "Linking $BUILD_DIR/main.o -> $BUILD_DIR/main.wasm"
"$WASM_LD" --no-entry --export=main --allow-undefined -o "$BUILD_DIR/main.wasm" "$BUILD_DIR/main.o"
cp "$BUILD_DIR/main.wasm" "$SCRIPT_DIR/main.wasm"
echo "Built: $SCRIPT_DIR/main.wasm"
"#;

const SERVE_SH: &str = r#"#!/bin/sh
# serve.sh - start a local dev server for the wasm build
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-8080}"
echo "Serving $SCRIPT_DIR on http://localhost:$PORT"
echo "Open http://localhost:$PORT in your browser"
echo "Press Ctrl+C to stop"
python3 -m http.server "$PORT" --directory "$SCRIPT_DIR" 2>/dev/null \
  || python -m SimpleHTTPServer "$PORT" 2>/dev/null \
  || python3 -m http.server "$PORT" 2>/dev/null \
  || echo "Error: Python not found. Serve the project directory manually."
"#;
