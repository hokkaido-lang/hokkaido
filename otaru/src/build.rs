use std::path::Path;
use std::process::Command;

pub fn run(file: Option<&str>, freestanding: bool) {
    if let Some(f) = file {
        compile_single(f, freestanding);
    } else {
        compile_project(freestanding);
    }
}

/// Compile a single file or project and return the binary path (without extension)
pub fn compile_single_or_project(file: Option<&str>, freestanding: bool) -> String {
    if let Some(f) = file {
        compile_single(f, freestanding)
    } else {
        compile_project(freestanding)
    }
}

fn compile_single(file: &str, freestanding: bool) -> String {
    let path = Path::new(file);
    if !path.exists() {
        eprintln!("Error: file '{}' not found", file);
        std::process::exit(1);
    }

    let hokkaido = find_hokkaido();

    std::fs::create_dir_all("build").unwrap_or_else(|e| {
        eprintln!("Error creating build/ directory: {}", e);
        std::process::exit(1);
    });

    let stem = path.file_stem().unwrap_or_default().to_string_lossy();
    let output_base = format!("build/{}", stem);

    let mut cmd = Command::new(&hokkaido);
    cmd.arg(file).arg("-o").arg(&output_base);
    if freestanding {
        cmd.arg("--freestanding");
    }

    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error running hokkaido: {}", e);
        std::process::exit(1);
    });

    if !status.success() {
        eprintln!("Compilation failed");
        std::process::exit(1);
    }

    let obj_path = format!("{}.o", output_base);

    if freestanding {
        println!("Compiled {} -> {} (freestanding)", file, obj_path);
        println!("Warning: freestanding object files cannot be linked with clang.");
        println!("Use ld.lld directly or a custom linker script.");
        return output_base;
    }

    // Link with clang
    let link_status = Command::new("clang")
        .arg(&obj_path)
        .arg("-o")
        .arg(&output_base)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error linking with clang: {} (is clang installed?)", e);
            std::process::exit(1);
        });

    if !link_status.success() {
        eprintln!("Linking failed");
        std::process::exit(1);
    }

    println!("Built: {}", output_base);
    output_base
}

fn compile_project(freestanding: bool) -> String {
    let manifest_path = Path::new("otaru.toml");
    if !manifest_path.exists() {
        eprintln!("Error: otaru.toml not found (not in an otaru project)");
        eprintln!("Hint: compile a single file with: otaru build <file.hk>");
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

    let mut cmd = Command::new(&hokkaido);
    cmd.arg(entry).arg("-o").arg(&binary);
    if freestanding {
        cmd.arg("--freestanding");
    }

    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error running hokkaido: {}", e);
        std::process::exit(1);
    });

    if !status.success() {
        eprintln!("Compilation failed");
        std::process::exit(1);
    }

    let obj_path = format!("{}.o", binary);

    if freestanding {
        println!("Compiled {} -> {} (freestanding)", entry, obj_path);
        println!("Warning: freestanding object files cannot be linked with clang.");
        println!("Use ld.lld directly or a custom linker script.");
        return binary;
    }

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
