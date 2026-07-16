use std::fs;
use std::path::Path;

pub fn run(name: &str, wasm: bool) {
    let project_dir = Path::new(name);
    if project_dir.exists() {
        eprintln!("Error: directory '{}' already exists", name);
        std::process::exit(1);
    }

    fs::create_dir_all(project_dir.join("src")).unwrap_or_else(|e| {
        eprintln!("Error creating project: {}", e);
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
        .save(&project_dir.join("otaru.toml"))
        .unwrap_or_else(|e| {
            eprintln!("{}", e);
            std::process::exit(1);
        });

    fs::write(project_dir.join("src/main.hk"), "fn main() -> int {\n    return 42\n}\n")
        .unwrap_or_else(|e| {
            eprintln!("Error writing src/main.hk: {}", e);
            std::process::exit(1);
        });

    fs::write(project_dir.join("hk.mod"), "").unwrap_or_else(|e| {
        eprintln!("Error writing hk.mod: {}", e);
        std::process::exit(1);
    });

    crate::template::prepare_std(project_dir);

    if wasm {
        let wasm_dir = project_dir.join("wasm32");
        fs::create_dir_all(&wasm_dir).unwrap_or_else(|e| {
            eprintln!("Error creating wasm32/ directory: {}", e);
            std::process::exit(1);
        });
        crate::template::write_index_html(&wasm_dir);
        crate::template::write_build_sh(&wasm_dir);
        crate::template::write_serve_sh(&wasm_dir);
    }

    println!("Created project '{}'", name);
    println!("  cd {}", name);
    if wasm {
        println!("  otaru build            # compile to wasm32/<name>.wasm");
        println!("  otaru run              # run in browser (wasmtime or http server)");
    } else {
        println!("  otaru build");
    }
}
