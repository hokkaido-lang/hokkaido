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
            fs::write("wasm32/index.html", html_content)
                .unwrap_or_else(|e| { eprintln!("Error writing wasm32/index.html: {}", e); std::process::exit(1); });
        }

        if !Path::new("wasm32/build.sh").exists() {
            let build_script = r#"#!/bin/sh
set -e
echo "Building for WebAssembly..."
otaru build src/main.hk --target wasm32-wasi
if command -v wasm-ld >/dev/null 2>&1; then
    wasm-ld --no-entry --export-all build/main.wasm -o build/main.wasm
    echo "Linked: build/main.wasm"
elif command -v wasm-ld-19 >/dev/null 2>&1; then
    wasm-ld-19 --no-entry --export-all build/main.wasm -o build/main.wasm
    echo "Linked: build/main.wasm"
else
    echo "Warning: wasm-ld not found, object file only"
fi
echo "Built: build/main.wasm"
"#;
            fs::write("wasm32/build.sh", build_script)
                .unwrap_or_else(|e| { eprintln!("Error writing wasm32/build.sh: {}", e); std::process::exit(1); });
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                fs::set_permissions("wasm32/build.sh", fs::Permissions::from_mode(0o755))
                    .unwrap_or_else(|e| { eprintln!("Error setting permissions: {}", e); std::process::exit(1); });
            }
        }
        println!("  Created wasm32/");
    }

    println!("Initialized project '{}'", name);
    if wasm {
        println!("  otaru build src/main.hk --target wasm32-wasi");
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
