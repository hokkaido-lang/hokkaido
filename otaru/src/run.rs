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

    let hokkaido = crate::build::find_hokkaido();
    let entry = "src/main.hk";

    if !Path::new(entry).exists() {
        eprintln!("Error: src/main.hk not found");
        std::process::exit(1);
    }

    let output_base = format!("build/{}", manifest.package.name);

    // Compile to object file (hokkaido appends .o automatically)
    let compile_status = Command::new(&hokkaido)
        .arg(entry)
        .arg("-o")
        .arg(&output_base)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running hokkaido: {}", e);
            std::process::exit(1);
        });

    if !compile_status.success() {
        eprintln!("Compilation failed");
        std::process::exit(1);
    }

    let obj_path = format!("{}.o", output_base);

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

    // Run
    let run_status = Command::new(&output_base)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running '{}': {}", output_base, e);
            std::process::exit(1);
        });

    std::process::exit(run_status.code().unwrap_or(1));
}
