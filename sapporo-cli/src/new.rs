use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

use crate::build;
use crate::manifest::{BuildConfig, Manifest, MANIFEST_FILE};

pub fn run(name: &str) {
    let project_dir = Path::new(name);
    if project_dir.exists() {
        eprintln!("Error: directory '{}' already exists", name);
        std::process::exit(1);
    }

    fs::create_dir_all(project_dir.join("src")).unwrap_or_else(|e| {
        eprintln!("Error creating project: {}", e);
        std::process::exit(1);
    });

    // sapporo.toml
    let manifest = Manifest {
        package: crate::manifest::Package {
            name: name.to_string(),
            version: "0.1.0".to_string(),
            authors: vec![],
        },
        build: Some(BuildConfig::default()),
        dependencies: BTreeMap::new(),
        scripts: BTreeMap::new(),
    };
    manifest
        .save(&project_dir.join(MANIFEST_FILE))
        .unwrap_or_else(|e| {
            eprintln!("{}", e);
            std::process::exit(1);
        });

    // hk.mod
    fs::write(project_dir.join("hk.mod"), "").unwrap_or_else(|e| {
        eprintln!("Error writing hk.mod: {}", e);
        std::process::exit(1);
    });

    // Embed sapporo library files into the project
    build::write_sapporo_hk(project_dir);
    build::write_sapporo_js(project_dir);

    // src/main.hk
    fs::write(
        project_dir.join("src/main.hk"),
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
            name, name
        ),
    )
    .unwrap_or_else(|e| {
        eprintln!("Error writing src/main.hk: {}", e);
        std::process::exit(1);
    });

    // index.html
    fs::write(
        project_dir.join("index.html"),
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
            name, name, name
        ),
    )
    .unwrap_or_else(|e| {
        eprintln!("Error writing index.html: {}", e);
        std::process::exit(1);
    });

    // .gitignore
    fs::write(
        project_dir.join(".gitignore"),
        "dist/\nsapporo/\nnode_modules/\n",
    )
    .unwrap_or_else(|e| {
        eprintln!("Error writing .gitignore: {}", e);
        std::process::exit(1);
    });

    println!("Created project '{}'", name);
    println!();
    println!("  cd {}", name);
    println!("  sapporo build       # compile to dist/{}.wasm", name);
    println!("  sapporo run         # start dev server + open browser");
}
