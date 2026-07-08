use std::fs;
use std::path::Path;

pub fn run(name: &str) {
    let project_dir = Path::new(name);
    if project_dir.exists() {
        eprintln!("Error: directory '{}' already exists", name);
        std::process::exit(1);
    }

    fs::create_dir_all(project_dir.join("src"))
        .unwrap_or_else(|e| { eprintln!("Error creating project: {}", e); std::process::exit(1); });

    // Create otaru.toml
    let manifest = crate::manifest::Manifest {
        package: crate::manifest::Package {
            name: name.to_string(),
            version: "0.1.0".to_string(),
            authors: vec![],
            edition: "2024".to_string(),
        },
        dependencies: Default::default(),
    };
    manifest.save(&project_dir.join("otaru.toml"))
        .unwrap_or_else(|e| { eprintln!("{}", e); std::process::exit(1); });

    // Create src/main.hk
    let main_hk = "fn main() -> int {\n    return 0\n}\n";
    fs::write(project_dir.join("src/main.hk"), main_hk)
        .unwrap_or_else(|e| { eprintln!("Error writing src/main.hk: {}", e); std::process::exit(1); });

    // Create hk.mod (project root marker)
    fs::write(project_dir.join("hk.mod"), "")
        .unwrap_or_else(|e| { eprintln!("Error writing hk.mod: {}", e); std::process::exit(1); });

    // Write embedded std library
    let std_dir = project_dir.join("std");
    fs::create_dir_all(&std_dir)
        .unwrap_or_else(|e| { eprintln!("Error creating std/ directory: {}", e); std::process::exit(1); });

    for f in crate::std_embed::files() {
        fs::write(std_dir.join(f.path), f.content)
            .unwrap_or_else(|e| { eprintln!("Error writing std/{}: {}", f.path, e); std::process::exit(1); });
    }

    println!("  std/ prepared");
    println!("Created project '{}'", name);
    println!("  cd {}", name);
    println!("  otaru build");
}
