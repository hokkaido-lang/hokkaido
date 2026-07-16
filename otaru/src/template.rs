use std::fs;
use std::path::Path;

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

/// Find the bundled std library by walking up from the binary location.
pub fn find_bundled_std() -> Option<String> {
    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            let mut dir = parent.to_path_buf();
            loop {
                let candidate = dir.join("std");
                if candidate.join("hk.mod").exists() {
                    return Some(candidate.to_string_lossy().to_string());
                }
                let nix_candidate = dir.join("share/otaru/std");
                if nix_candidate.join("hk.mod").exists() {
                    return Some(nix_candidate.to_string_lossy().to_string());
                }
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    if let Ok(home) = std::env::var("HOKKAIDO_HOME") {
        let candidate = Path::new(&home).join("../std");
        if candidate.exists() {
            return Some(candidate.to_string_lossy().to_string());
        }
    }

    None
}

/// Copy the std library into the project, printing status.
pub fn prepare_std(project_dir: &Path) {
    if let Some(src) = find_bundled_std() {
        let dst = project_dir.join("std");
        if !dst.exists() {
            if let Err(e) = copy_dir(Path::new(&src), &dst) {
                eprintln!("Warning: could not copy std library: {}", e);
            } else {
                println!("  std/ prepared");
            }
        }
    } else {
        eprintln!("Warning: std/ directory not found (stdlib features unavailable)");
        eprintln!("Hint: install otaru via Nix or build from the hokkaido repository");
    }
}

/// Write the wasm index.html template.
pub fn write_index_html(project_dir: &Path) {
    fs::write(project_dir.join("index.html"), INDEX_HTML)
        .unwrap_or_else(|e| {
            eprintln!("Error writing index.html: {}", e);
            std::process::exit(1);
        });
}

/// Write the wasm build.sh template.
pub fn write_build_sh(project_dir: &Path) {
    fs::write(project_dir.join("build.sh"), BUILD_SH)
        .unwrap_or_else(|e| {
            eprintln!("Error writing build.sh: {}", e);
            std::process::exit(1);
        });
    make_executable(&project_dir.join("build.sh"));
}

/// Write the wasm serve.sh template.
pub fn write_serve_sh(project_dir: &Path) {
    fs::write(project_dir.join("serve.sh"), SERVE_SH)
        .unwrap_or_else(|e| {
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

const INDEX_HTML: &str = r#"<!DOCTYPE html>
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
