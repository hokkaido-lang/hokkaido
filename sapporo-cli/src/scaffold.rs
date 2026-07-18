use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

use crate::build;
use crate::manifest::{BuildConfig, Manifest, MANIFEST_FILE};

/// Generate the default index.html content for a project
pub fn generate_index_html(name: &str) -> String {
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

/// Generate the default main.hk content for a project
pub fn generate_main_hk(name: &str) -> String {
    format!(
        r#"package main

import "sapporo"

fn main() -> int {{
    sapporo::log("Hello from {name}!")

    // Set text on an element by ID
    sapporo::set_text("output", "Hello, {name}!")

    return 0
}}
"#
    )
}

/// Create a new sapporo project in the given directory
pub fn create_project(project_dir: &Path, name: &str, verbose: bool) {
    // Create src/ directory
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
    build::write_sapporo_hk(project_dir, verbose);
    build::write_sapporo_js(project_dir, verbose);

    // Prepare std library if available
    build::prepare_std(project_dir, verbose);

    // src/main.hk
    fs::write(project_dir.join("src/main.hk"), generate_main_hk(name)).unwrap_or_else(|e| {
        eprintln!("Error writing src/main.hk: {}", e);
        std::process::exit(1);
    });

    // index.html
    fs::write(project_dir.join("index.html"), generate_index_html(name)).unwrap_or_else(|e| {
        eprintln!("Error writing index.html: {}", e);
        std::process::exit(1);
    });

    // .gitignore
    fs::write(project_dir.join(".gitignore"), "dist/\nsapporo/\nnode_modules/\n").unwrap_or_else(
        |e| {
            eprintln!("Error writing .gitignore: {}", e);
            std::process::exit(1);
        },
    );
}

/// Initialize the current directory as a sapporo project
pub fn init_project(name: &str, verbose: bool) {
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

    // Embed sapporo library files into the project
    build::write_sapporo_hk(Path::new("."), verbose);
    build::write_sapporo_js(Path::new("."), verbose);

    // Prepare std library if available
    build::prepare_std(Path::new("."), verbose);

    // src/main.hk if it doesn't exist
    if !Path::new("src/main.hk").exists() {
        fs::write("src/main.hk", generate_main_hk(name)).unwrap_or_else(|e| {
            eprintln!("Error writing src/main.hk: {}", e);
            std::process::exit(1);
        });
    }

    // index.html if it doesn't exist
    if !Path::new("index.html").exists() {
        fs::write("index.html", generate_index_html(name)).unwrap_or_else(|e| {
            eprintln!("Error writing index.html: {}", e);
            std::process::exit(1);
        });
    }

    // .gitignore
    if !Path::new(".gitignore").exists() {
        fs::write(".gitignore", "dist/\nsapporo/\nnode_modules/\n").unwrap_or_else(|e| {
            eprintln!("Error writing .gitignore: {}", e);
            std::process::exit(1);
        });
    }
}
