use std::path::Path;
use std::process::Command;

pub fn run() {
    let manifest_path = Path::new("otaru.toml");
    if !manifest_path.exists() {
        eprintln!("Error: otaru.toml not found");
        std::process::exit(1);
    }

    let manifest = crate::manifest::Manifest::load(manifest_path)
        .unwrap_or_else(|e| { eprintln!("{}", e); std::process::exit(1); });

    if manifest.dependencies.is_empty() {
        println!("No dependencies to install");
        return;
    }

    // Cache directory
    let cache_dir = get_cache_dir();
    let deps_dir = Path::new("deps");
    std::fs::create_dir_all(&cache_dir).ok();
    std::fs::create_dir_all(deps_dir).ok();

    for (name, dep) in &manifest.dependencies {
        match dep {
            crate::manifest::Dependency::Detailed { git, tag, branch, .. } => {
                if let Some(url) = git {
                    let cache_path = cache_dir.join(sanitize_name(name));
                    let target_path = deps_dir.join(name);

                    // Clone or fetch
                    if cache_path.exists() {
                        println!("Updating '{}'...", name);
                        let _ = Command::new("git")
                            .args(["-C", &cache_path.to_string_lossy(), "fetch", "--tags"])
                            .output();
                    } else {
                        println!("Cloning '{}'...", name);
                        let status = Command::new("git")
                            .args(["clone", url, &cache_path.to_string_lossy()])
                            .status()
                            .unwrap_or_else(|e| {
                                eprintln!("Error cloning {}: {}", name, e);
                                std::process::exit(1);
                            });
                        if !status.success() {
                            eprintln!("Failed to clone {}", name);
                            std::process::exit(1);
                        }
                    }

                    // Checkout specific tag/branch
                    if let Some(t) = tag {
                        let _ = Command::new("git")
                            .args(["-C", &cache_path.to_string_lossy(), "checkout", t])
                            .output();
                    } else if let Some(b) = branch {
                        let _ = Command::new("git")
                            .args(["-C", &cache_path.to_string_lossy(), "checkout", b])
                            .output();
                    }

                    // Symlink/copy to deps/
                    let _ = std::fs::remove_file(&target_path);
                    let _ = std::fs::remove_dir_all(&target_path);
                    if let Err(e) = symlink_dir(&cache_path, &target_path) {
                        // Fallback: copy the src directory
                        copy_dep(&cache_path.join("src"), &target_path);
                        eprintln!("Warning: symlink failed ({}), copied src/ instead", e);
                    }
                    println!("  Installed '{}' from git", name);
                } else if let Some(p) = &dep.path() {
                    // Path dependency: symlink
                    let abs_path = std::fs::canonicalize(Path::new(p))
                        .unwrap_or_else(|_| Path::new(p).to_path_buf());
                    let target_path = deps_dir.join(name);
                    let _ = std::fs::remove_file(&target_path);
                    let _ = std::fs::remove_dir_all(&target_path);
                    symlink_dir(&abs_path, &target_path).ok();
                    println!("  Linked '{}' from {}", name, p);
                }
            }
            crate::manifest::Dependency::Simple(version) => {
                println!("  Skipping '{}' (registry not yet supported, version {})", name, version);
            }
        }
    }

    println!("Done");
}

impl crate::manifest::Dependency {
    fn path(&self) -> Option<&String> {
        match self {
            crate::manifest::Dependency::Detailed { path, .. } => path.as_ref(),
            _ => None,
        }
    }
}

fn get_cache_dir() -> std::path::PathBuf {
    let base = dirs::cache_dir()
        .unwrap_or_else(|| Path::new("~/.cache").to_path_buf());
    base.join("otaru")
}

fn sanitize_name(name: &str) -> String {
    name.replace(|c: char| !c.is_alphanumeric() && c != '-' && c != '_', "_")
}

fn symlink_dir(src: &Path, dst: &Path) -> std::io::Result<()> {
    #[cfg(unix)]
    {
        std::os::unix::fs::symlink(src, dst)
    }
    #[cfg(windows)]
    {
        std::os::windows::fs::symlink_dir(src, dst)
    }
}

fn copy_dep(src: &Path, dst: &Path) {
    if let Ok(entries) = std::fs::read_dir(src) {
        std::fs::create_dir_all(dst).ok();
        for entry in entries.flatten() {
            let path = entry.path();
            let dest = dst.join(entry.file_name());
            if path.is_dir() {
                copy_dir_recursive(&path, &dest);
            } else {
                let _ = std::fs::copy(&path, &dest);
            }
        }
    }
}

fn copy_dir_recursive(src: &Path, dst: &Path) {
    std::fs::create_dir_all(dst).ok();
    if let Ok(entries) = std::fs::read_dir(src) {
        for entry in entries.flatten() {
            let path = entry.path();
            let dest = dst.join(entry.file_name());
            if path.is_dir() {
                copy_dir_recursive(&path, &dest);
            } else {
                let _ = std::fs::copy(&path, &dest);
            }
        }
    }
}
