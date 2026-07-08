use std::path::Path;
use std::process::Command;

pub fn run() {
    compile_and_link();
}

/// Compile all .hk sources and link into an executable in build/
pub fn compile_and_link() -> String {
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

    let hokkaido = find_hokkaido();

    std::fs::create_dir_all("build").unwrap_or_else(|e| {
        eprintln!("Error creating build/ directory: {}", e);
        std::process::exit(1);
    });

    let mut hk_files: Vec<String> = Vec::new();
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

    let binary = format!("build/{}", manifest.package.name);

    let entry = hk_files.iter()
        .find(|f| f.contains("main"))
        .unwrap_or(&hk_files[0]);

    // hokkaido appends .o to the output path
    let status = Command::new(&hokkaido)
        .arg(entry)
        .arg("-o")
        .arg(&binary)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running hokkaido: {}", e);
            std::process::exit(1);
        });

    if !status.success() {
        eprintln!("Compilation failed");
        std::process::exit(1);
    }

    let obj_path = format!("{}.o", binary);

    // Link with clang
    let link_status = Command::new("clang")
        .arg(&obj_path)
        .arg("-o")
        .arg(&binary)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error linking with clang: {} (is clang installed?)", e);
            std::process::exit(1);
        });

    if !link_status.success() {
        eprintln!("Linking failed");
        std::process::exit(1);
    }

    println!("Built: {}", binary);
    binary
}

pub fn find_hokkaido() -> String {
    // Check next to the running otaru binary (works when bundled via Nix)
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            let candidate = dir.join("hokkaido");
            if candidate.exists() {
                return candidate.to_string_lossy().to_string();
            }
        }
    }

    if let Ok(path) = std::env::var("PATH") {
        for dir in path.split(':') {
            let candidate = std::path::Path::new(dir).join("hokkaido");
            if candidate.exists() {
                return candidate.to_string_lossy().to_string();
            }
        }
    }

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
