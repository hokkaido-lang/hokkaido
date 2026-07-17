use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

use crate::manifest::{BuildConfig, Manifest, MANIFEST_FILE};

pub fn run() {
    if Path::new(MANIFEST_FILE).exists() {
        eprintln!("Error: {} already exists in this directory.", MANIFEST_FILE);
        eprintln!("Use 'sapporo build' to build the project.");
        std::process::exit(1);
    }

    // Get project name from directory name
    let dir_name = std::env::current_dir()
        .ok()
        .and_then(|d| {
            d.file_name()
                .map(|n| n.to_string_lossy().to_string())
        })
        .unwrap_or_else(|| "my-app".to_string());

    // sapporo.toml
    let manifest = Manifest {
        package: crate::manifest::Package {
            name: dir_name.clone(),
            version: "0.1.0".to_string(),
            authors: vec![],
        },
        build: Some(BuildConfig::default()),
        dependencies: BTreeMap::new(),
        scripts: BTreeMap::new(),
    };
    manifest
        .save(Path::new(MANIFEST_FILE))
        .unwrap_or_else(|e| {
            eprintln!("{}", e);
            std::process::exit(1);
        });

    // hk.mod
    if !Path::new("hk.mod").exists() {
        fs::write("hk.mod", "").unwrap_or_else(|e| {
            eprintln!("Error writing hk.mod: {}", e);
            std::process::exit(1);
        });
    }

    // Create src/ if it doesn't exist
    fs::create_dir_all("src").unwrap_or_else(|e| {
        eprintln!("Error creating src/: {}", e);
        std::process::exit(1);
    });

    // src/main.hk if it doesn't exist
    if !Path::new("src/main.hk").exists() {
        fs::write(
            "src/main.hk",
            format!(
                r#"package main

import "sapporo"

fn main() -> int {{
    sapporo::log("Hello from {}!")

    // Set text on an element by ID
    sapporo::set_text("output", "Hello, {}!")

    return 0
}}
"#,
                dir_name, dir_name
            ),
        )
        .unwrap_or_else(|e| {
            eprintln!("Error writing src/main.hk: {}", e);
            std::process::exit(1);
        });
    }

    // index.html if it doesn't exist
    if !Path::new("index.html").exists() {
        fs::write(
            "index.html",
            format!(
                r#"<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{}</title>
<style>
  body {{ font-family: system-ui, sans-serif; max-width: 640px; margin: 2rem auto; padding: 0 1rem; }}
  #output {{ margin: 1rem 0; font-size: 24px; color: #e63946; }}
</style>
</head>
<body>
<h1>{}</h1>
<p id="output">Loading...</p>
<script src="sapporo.js"></script>
<script>
  Sapporo.load("{}.wasm").then(m => {{
    m.main();
  }}).catch(err => {{
    document.getElementById("output").textContent = "Error: " + err.message;
  }});
</script>
</body>
</html>
"#,
                dir_name, dir_name, dir_name
            ),
        )
        .unwrap_or_else(|e| {
            eprintln!("Error writing index.html: {}", e);
            std::process::exit(1);
        });
    }

    // .gitignore
    if !Path::new(".gitignore").exists() {
        fs::write(".gitignore", "dist/\nnode_modules/\n").unwrap_or_else(|e| {
            eprintln!("Error writing .gitignore: {}", e);
            std::process::exit(1);
        });
    }

    println!("Initialized sapporo project '{}'", dir_name);
    println!();
    println!("  sapporo build       # compile to dist/{}.wasm", dir_name);
    println!("  sapporo run         # start dev server + open browser");
}
