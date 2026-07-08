use std::path::Path;
use std::process::Command;

pub fn run() {
    let manifest_path = Path::new("otaru.toml");
    if !manifest_path.exists() {
        eprintln!("Error: otaru.toml not found (not in an otaru project)");
        std::process::exit(1);
    }

    let manifest = crate::manifest::Manifest::load(manifest_path)
        .unwrap_or_else(|e| { eprintln!("{}", e); std::process::exit(1); });

    let src_dir = Path::new("src");
    if !src_dir.is_dir() {
        eprintln!("Error: src/ directory not found");
        std::process::exit(1);
    }

    // Find the hokkaido compiler
    let hokkaido = find_hokkaido();

    // Create build directory
    fs_extra::create_build_dir();

    // Collect all .hk files from src/ and dependencies
    let mut hk_files: Vec<String> = Vec::new();

    // Source files
    if let Ok(entries) = std::fs::read_dir(src_dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.extension().map_or(false, |e| e == "hk") {
                hk_files.push(path.to_string_lossy().to_string());
            }
        }
    }

    if hk_files.is_empty() {
        eprintln!("Error: no .hk files found in src/");
        std::process::exit(1);
    }

    // Determine entry point (main.hk or the only file)
    // hokkaido appends .o to the output path automatically
    let obj_base = format!("build/{}", manifest.package.name);

    // The first .hk file (typically main.hk) is the entry
    // We compile all src files together via include or separate compilation
    // For now, compile the entry point which includes others
    let entry = hk_files.iter()
        .find(|f| f.contains("main"))
        .unwrap_or(&hk_files[0]);

    let status = Command::new(&hokkaido)
        .arg(entry)
        .arg("-o")
        .arg(&obj_base)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running hokkaido: {}", e);
            std::process::exit(1);
        });

    if !status.success() {
        eprintln!("Build failed");
        std::process::exit(1);
    }

    println!("Compiled {} -> {}.o", entry, obj_base);
}

pub fn find_hokkaido() -> String {
    // Check PATH first — hokkaido doesn't have --help, so check with "which" approach
    if let Ok(path) = std::env::var("PATH") {
        for dir in path.split(':') {
            let candidate = std::path::Path::new(dir).join("hokkaido");
            if candidate.exists() {
                return candidate.to_string_lossy().to_string();
            }
        }
    }

    // Check common locations
    for candidate in &[
        "./build/hokkaido",
        "../build/hokkaido",
        "/usr/local/bin/hokkaido",
        "/usr/bin/hokkaido",
    ] {
        if std::path::Path::new(candidate).exists() {
            return candidate.to_string();
        }
    }

    // Check if HOKKAIDO_HOME is set
    if let Ok(home) = std::env::var("HOKKAIDO_HOME") {
        let path = std::path::Path::new(&home).join("hokkaido");
        if path.exists() {
            return path.to_string_lossy().to_string();
        }
    }

    eprintln!("Error: hokkaido compiler not found. Install it or add it to PATH.");
    eprintln!("Hint: set HOKKAIDO_HOME to the directory containing the hokkaido binary.");
    std::process::exit(1);
}

mod fs_extra {
    pub fn create_build_dir() {
        let _ = std::fs::create_dir_all("build");
    }
}
