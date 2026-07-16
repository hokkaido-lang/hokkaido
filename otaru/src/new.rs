use std::fs;
use std::path::Path;

pub fn run(name: &str, wasm: bool) {
    let project_dir = Path::new(name);
    if project_dir.exists() {
        eprintln!("Error: directory '{}' already exists", name);
        std::process::exit(1);
    }

    fs::create_dir_all(project_dir.join("src"))
        .unwrap_or_else(|e| { eprintln!("Error creating project: {}", e); std::process::exit(1); });

    // Create otaru.toml
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
    manifest.save(&project_dir.join("otaru.toml"))
        .unwrap_or_else(|e| { eprintln!("{}", e); std::process::exit(1); });

    // Create src/main.hk
    let main_hk = if wasm {
        "fn main() -> int {\n    return 0\n}\n"
    } else {
        "fn main() -> int {\n    return 0\n}\n"
    };
    fs::write(project_dir.join("src/main.hk"), main_hk)
        .unwrap_or_else(|e| { eprintln!("Error writing src/main.hk: {}", e); std::process::exit(1); });

    // Create hk.mod (project root marker)
    fs::write(project_dir.join("hk.mod"), "")
        .unwrap_or_else(|e| { eprintln!("Error writing hk.mod: {}", e); std::process::exit(1); });

    // Copy std library from the bundled location
    let std_dir = find_bundled_std();
    if let Some(src) = std_dir {
        let dst = project_dir.join("std");
        if let Err(e) = copy_dir(Path::new(&src), &dst) {
            eprintln!("Warning: could not copy std library: {}", e);
        } else {
            println!("  std/ prepared");
        }
    } else {
        eprintln!("Warning: std/ directory not found (stdlib features unavailable)");
        eprintln!("Hint: install otaru via Nix or build from the hokkaido repository");
    }

    if wasm {
        // Create wasm32 directory with build instructions
        fs::create_dir_all(project_dir.join("wasm32"))
            .unwrap_or_else(|e| { eprintln!("Error creating wasm32/ directory: {}", e); std::process::exit(1); });
        
        // Create a simple HTML file for testing in browser
        let html_content = r#"<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Hokkaido WebAssembly</title>
</head>
<body>
    <h1>Hokkaido WebAssembly</h1>
    <div id="output"></div>
    <script>
        async function run() {
            const response = await fetch('main.wasm');
            const bytes = await response.arrayBuffer();
            const imports = {};
            const { instance } = await WebAssembly.instantiate(bytes, imports);
            const result = instance.exports._start();
            document.getElementById('output').textContent = 'Result: ' + result;
        }
        run().catch(console.error);
    </script>
</body>
</html>"#;
        fs::write(project_dir.join("wasm32/index.html"), html_content)
            .unwrap_or_else(|e| { eprintln!("Error writing wasm32/index.html: {}", e); std::process::exit(1); });
        
        // Create a build script for wasm32
        let build_script = r#"#!/bin/sh
# Build script for WebAssembly
set -e

echo "Building for WebAssembly..."
otaru build src/main.hk --target wasm32-wasi

# Link with wasm-ld if available
if command -v wasm-ld >/dev/null 2>&1; then
    wasm-ld --no-entry --export-all build/main.wasm -o build/main.wasm
    echo "Linked: build/main.wasm"
elif command -v wasm-ld-19 >/dev/null 2>&1; then
    wasm-ld-19 --no-entry --export-all build/main.wasm -o build/main.wasm
    echo "Linked: build/main.wasm"
else
    echo "Warning: wasm-ld not found, object file only"
    echo "Install LLVM or run: cargo install wasm-ld"
fi

echo "Built: build/main.wasm"
"#;
        fs::write(project_dir.join("wasm32/build.sh"), build_script)
            .unwrap_or_else(|e| { eprintln!("Error writing wasm32/build.sh: {}", e); std::process::exit(1); });
        
        // Make build script executable
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            fs::set_permissions(project_dir.join("wasm32/build.sh"), fs::Permissions::from_mode(0o755))
                .unwrap_or_else(|e| { eprintln!("Error setting permissions: {}", e); std::process::exit(1); });
        }
    }

    println!("Created project '{}'", name);
    if wasm {
        println!("  cd {}", name);
        println!("  otaru build src/main.hk --target wasm32-wasi");
        println!("  # Or use the build script:");
        println!("  ./wasm32/build.sh");
    } else {
        println!("  cd {}", name);
        println!("  otaru build");
    }
}

fn find_bundled_std() -> Option<String> {
    // Check next to the running otaru binary
    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            let mut dir = parent.to_path_buf();
            // Walk up the directory tree looking for std/hk.mod
            loop {
                let candidate = dir.join("std");
                if candidate.join("hk.mod").exists() {
                    return Some(candidate.to_string_lossy().to_string());
                }
                // Also check share/otaru/std (Nix layout)
                let nix_candidate = dir.join("share/otaru/std");
                if nix_candidate.join("hk.mod").exists() {
                    return Some(nix_candidate.to_string_lossy().to_string());
                }
                if !dir.pop() { break; }
            }
        }
    }

    // Fallback: HOKKAIDO_HOME points to build dir
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
