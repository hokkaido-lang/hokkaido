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
        build: None,
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

    // Copy std library from the bundled location
    let std_dir = find_bundled_std();
    if let Some(src) = std_dir {
        let dst = project_dir.join("std");
        if let Err(e) = copy_dir(Path::new(&src), &dst) {
            eprintln!("Warning: could not copy std library: {}", e);
        } else {
            println!("  std/ prepared");
        }
    } else {
        eprintln!("Warning: std/ directory not found (stdlib features unavailable)");
        eprintln!("Hint: install otaru via Nix or build from the hokkaido repository");
    }

    println!("Created project '{}'", name);
    println!("  cd {}", name);
    println!("  otaru build");
}

fn find_bundled_std() -> Option<String> {
    // Check next to the running otaru binary
    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            let mut dir = parent.to_path_buf();
            // Walk up the directory tree looking for std/hk.mod
            loop {
                let candidate = dir.join("std");
                if candidate.join("hk.mod").exists() {
                    return Some(candidate.to_string_lossy().to_string());
                }
                // Also check share/otaru/std (Nix layout)
                let nix_candidate = dir.join("share/otaru/std");
                if nix_candidate.join("hk.mod").exists() {
                    return Some(nix_candidate.to_string_lossy().to_string());
                }
                if !dir.pop() { break; }
            }
        }
    }

    // Fallback: HOKKAIDO_HOME points to build dir
    if let Ok(home) = std::env::var("HOKKAIDO_HOME") {
        let candidate = Path::new(&home).join("../std");
        if candidate.exists() {
            return Some(candidate.to_string_lossy().to_string());
        }
    }

    None
}

fn copy_dir(src: &Path, dst: &Path) -> std::io::Result<()> {
    fs::create_dir_all(dst)?;
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let ty = entry.file_type()?;
        if ty.is_dir() {
            copy_dir(&entry.path(), &dst.join(entry.file_name()))?;
        } else {
            fs::copy(entry.path(), dst.join(entry.file_name()))?;
        }
    }
    Ok(())
}
