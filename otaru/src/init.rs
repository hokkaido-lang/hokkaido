use std::fs;
use std::path::Path;

pub fn run(wasm: bool) {
    let cwd = std::env::current_dir().unwrap_or_else(|e| {
        eprintln!("Error getting current directory: {}", e);
        std::process::exit(1);
    });
    let name = cwd
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or("myapp");

    if Path::new("otaru.toml").exists() {
        eprintln!("Error: otaru.toml already exists in the current directory");
        std::process::exit(1);
    }

    fs::create_dir_all("src").unwrap_or_else(|e| {
        eprintln!("Error creating src/: {}", e);
        std::process::exit(1);
    });

    let manifest = crate::manifest::Manifest {
        package: crate::manifest::Package {
            name: name.to_string(),
            version: "0.1.0".to_string(),
            authors: vec![],
            edition: "2024".to_string(),
        },
        dependencies: Default::default(),
        build: if wasm {
            Some(crate::manifest::Build {
                kind: "wasm".to_string(),
                ..Default::default()
            })
        } else {
            None
        },
        scripts: Default::default(),
    };
    manifest
        .save(Path::new("otaru.toml"))
        .unwrap_or_else(|e| {
            eprintln!("{}", e);
            std::process::exit(1);
        });
    println!("  Created otaru.toml");

    if !Path::new("src/main.hk").exists() {
        fs::write("src/main.hk", "fn main() -> int {\n    return 42\n}\n").unwrap_or_else(
            |e| {
                eprintln!("Error writing src/main.hk: {}", e);
                std::process::exit(1);
            },
        );
        println!("  Created src/main.hk");
    } else {
        println!("  src/main.hk already exists, skipping");
    }

    if !Path::new("hk.mod").exists() {
        fs::write("hk.mod", "").unwrap_or_else(|e| {
            eprintln!("Error writing hk.mod: {}", e);
            std::process::exit(1);
        });
        println!("  Created hk.mod");
    }

    crate::template::prepare_std(Path::new("."));

    if wasm {
        let wasm_dir = Path::new("wasm32");
        fs::create_dir_all(wasm_dir).unwrap_or_else(|e| {
            eprintln!("Error creating wasm32/: {}", e);
            std::process::exit(1);
        });

        if !wasm_dir.join("index.html").exists() {
            crate::template::write_index_html(wasm_dir);
        }
        if !wasm_dir.join("build.sh").exists() {
            crate::template::write_build_sh(wasm_dir);
        }
        if !wasm_dir.join("serve.sh").exists() {
            crate::template::write_serve_sh(wasm_dir);
        }
        println!("  Created wasm32/");
    }

    println!("Initialized project '{}'", name);
    if wasm {
        println!("  otaru build            # compile to wasm32/<name>.wasm");
        println!("  otaru run              # run in browser (wasmtime or http server)");
    } else {
        println!("  otaru build");
    }
}
