use std::fs;
use std::path::Path;

pub fn run(wasm: bool) {
    let cwd = std::env::current_dir()
        .unwrap_or_else(|e| { eprintln!("Error getting current directory: {}", e); std::process::exit(1); });
    let name = cwd
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or("myapp");

    if Path::new("otaru.toml").exists() {
        eprintln!("Error: otaru.toml already exists in the current directory");
        std::process::exit(1);
    }

    fs::create_dir_all("src")
        .unwrap_or_else(|e| { eprintln!("Error creating src/: {}", e); std::process::exit(1); });

    let manifest = if wasm {
        crate::manifest::Manifest {
            package: crate::manifest::Package {
                name: name.to_string(),
                version: "0.1.0".to_string(),
                authors: vec![],
                edition: "2024".to_string(),
            },
            dependencies: Default::default(),
            build: Some(crate::manifest::Build {
                kind: "wasm".to_string(),
                sources: vec![],
                include_dirs: vec![],
                compiler: "cc".to_string(),
                cflags: vec![],
                ldflags: vec!["--target=wasm32-wasi".to_string()],
                link: vec![],
                libraries: vec![],
                lib_dirs: vec![],
                targets: None,
                prebuild: None,
                llvm_config: None,
                llvm_components: None,
            }),
            scripts: Default::default(),
        }
    } else {
        crate::manifest::Manifest {
            package: crate::manifest::Package {
                name: name.to_string(),
                version: "0.1.0".to_string(),
                authors: vec![],
                edition: "2024".to_string(),
            },
            dependencies: Default::default(),
            build: None,
            scripts: Default::default(),
        }
    };
    manifest.save(Path::new("otaru.toml"))
        .unwrap_or_else(|e| { eprintln!("{}", e); std::process::exit(1); });
    println!("  Created otaru.toml");

    if !Path::new("src/main.hk").exists() {
        let main_hk = "fn main() -> int {\n    return 0\n}\n";
        fs::write("src/main.hk", main_hk)
            .unwrap_or_else(|e| { eprintln!("Error writing src/main.hk: {}", e); std::process::exit(1); });
        println!("  Created src/main.hk");
    } else {
        println!("  src/main.hk already exists, skipping");
    }

    if !Path::new("hk.mod").exists() {
        fs::write("hk.mod", "")
            .unwrap_or_else(|e| { eprintln!("Error writing hk.mod: {}", e); std::process::exit(1); });
        println!("  Created hk.mod");
    }

    let std_dir = find_bundled_std();
    if let Some(src) = std_dir {
        let dst = Path::new("std");
        if !dst.exists() {
            if let Err(e) = copy_dir(Path::new(&src), dst) {
                eprintln!("Warning: could not copy std library: {}", e);
            } else {
                println!("  std/ prepared");
            }
        }
    } else {
        eprintln!("Warning: std/ directory not found (stdlib features unavailable)");
        eprintln!("Hint: install otaru via Nix or build from the hokkaido repository");
    }

    if wasm {
        fs::create_dir_all("wasm32")
            .unwrap_or_else(|e| { eprintln!("Error creating wasm32/: {}", e); std::process::exit(1); });

        if !Path::new("wasm32/index.html").exists() {
            let html_content = r#"<!DOCTYPE html>
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
            fs::write("wasm32/index.html", html_content)
                .unwrap_or_else(|e| { eprintln!("Error writing wasm32/index.html: {}", e); std::process::exit(1); });
        }

        if !Path::new("wasm32/build.sh").exists() {
            let build_script = r#"#!/bin/sh
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
  echo "Error: wasm-ld not found. Install LLVM (e.g. nix-shell -p llvmPackages_19.llvm)."
  exit 1
fi

echo "Linking $BUILD_DIR/main.o -> $BUILD_DIR/main.wasm"
"$WASM_LD" --no-entry --export=main --allow-undefined -o "$BUILD_DIR/main.wasm" "$BUILD_DIR/main.o"
cp "$BUILD_DIR/main.wasm" "$SCRIPT_DIR/main.wasm"
echo "Built: $SCRIPT_DIR/main.wasm"
echo ""
echo "To open in browser:"
echo "  cd $SCRIPT_DIR && python3 -m http.server 8080"
echo "  open http://localhost:8080"
"#;
            fs::write("wasm32/build.sh", build_script)
                .unwrap_or_else(|e| { eprintln!("Error writing wasm32/build.sh: {}", e); std::process::exit(1); });
        }

        if !Path::new("wasm32/serve.sh").exists() {
            let serve_script = r#"#!/bin/sh
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-8080}"
echo "Serving $SCRIPT_DIR on http://localhost:$PORT"
echo "Open http://localhost:$PORT in your browser"
echo "Press Ctrl+C to stop"
python3 -m http.server "$PORT" --directory "$SCRIPT_DIR" 2>/dev/null \
  || python -m SimpleHTTPServer "$PORT" 2>/dev/null \
  || python3 -m http.server "$PORT" 2>/dev/null \
  || echo "Error: Python not found. Serve wasm32/ directory manually."
"#;
            fs::write("wasm32/serve.sh", serve_script)
                .unwrap_or_else(|e| { eprintln!("Error writing wasm32/serve.sh: {}", e); std::process::exit(1); });
        }

        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let _ = fs::set_permissions("wasm32/build.sh", fs::Permissions::from_mode(0o755));
            let _ = fs::set_permissions("wasm32/serve.sh", fs::Permissions::from_mode(0o755));
        }
        println!("  Created wasm32/");
    }

    println!("Initialized project '{}'", name);
    if wasm {
        println!("  ./wasm32/build.sh          # compile to wasm");
        println!("  ./wasm32/serve.sh          # open in browser");
    } else {
        println!("  otaru build");
    }
}

fn find_bundled_std() -> Option<String> {
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
                if !dir.pop() { break; }
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

fn copy_dir(src: &Path, dst: &Path) -> std::io::Result<()> {
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
