use std::path::{Path, PathBuf};
use std::process::Command;

use crate::manifest::Manifest;

// ---------------------------------------------------------------------------
// Manifest helpers
// ---------------------------------------------------------------------------

pub fn load_manifest() -> Manifest {
    load_manifest_from(Path::new("otaru.toml"))
}

pub fn load_manifest_from(path: &Path) -> Manifest {
    Manifest::load(path).unwrap_or_else(|e| {
        eprintln!("{}", e);
        std::process::exit(1);
    })
}

// ---------------------------------------------------------------------------
// Hokkaido compiler discovery
// ---------------------------------------------------------------------------

pub fn find_hokkaido() -> String {
    // 1. Same directory as otaru binary
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            let candidate = dir.join("hokkaido");
            if candidate.exists() {
                return candidate.to_string_lossy().to_string();
            }
        }
    }

    // 2. PATH
    if let Ok(path) = std::env::var("PATH") {
        for dir in path.split(':') {
            let candidate = Path::new(dir).join("hokkaido");
            if candidate.exists() {
                return candidate.to_string_lossy().to_string();
            }
        }
    }

    // 3. Well-known locations
    for candidate in &[
        "./build/hokkaido",
        "../build/hokkaido",
        "/usr/local/bin/hokkaido",
        "/usr/bin/hokkaido",
    ] {
        if Path::new(candidate).exists() {
            return candidate.to_string();
        }
    }

    // 4. Walk up from binary location looking for build/hokkaido
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            let mut current = dir.to_path_buf();
            for _ in 0..10 {
                let candidate = current.join("build").join("hokkaido");
                if candidate.exists() {
                    return candidate.to_string_lossy().to_string();
                }
                let candidate = current.join("hokkaido").join("build").join("hokkaido");
                if candidate.exists() {
                    return candidate.to_string_lossy().to_string();
                }
                if !current.pop() {
                    break;
                }
            }
        }
    }

    // 5. HOKKAIDO_HOME
    if let Ok(home) = std::env::var("HOKKAIDO_HOME") {
        let path = Path::new(&home).join("hokkaido");
        if path.exists() {
            return path.to_string_lossy().to_string();
        }
    }

    eprintln!("Error: hokkaido compiler not found. Install it or add it to PATH.");
    eprintln!("Hint: set HOKKAIDO_HOME to the directory containing the hokkaido binary.");
    std::process::exit(1);
}

// ---------------------------------------------------------------------------
// Standard library discovery
// ---------------------------------------------------------------------------

pub fn find_std_dir() -> Option<PathBuf> {
    // HOKKAIDO_STD env var
    if let Ok(std_path) = std::env::var("HOKKAIDO_STD") {
        let candidate = Path::new(&std_path);
        if candidate.join("hk.mod").exists() {
            return Some(candidate.to_path_buf());
        }
    }

    // Walk up from binary location
    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            let mut dir = parent.to_path_buf();
            loop {
                let candidate = dir.join("std");
                if candidate.join("hk.mod").exists() {
                    return Some(candidate);
                }
                let nix_candidate = dir.join("share/otaru/std");
                if nix_candidate.join("hk.mod").exists() {
                    return Some(nix_candidate);
                }
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    // HOKKAIDO_HOME fallback
    if let Ok(home) = std::env::var("HOKKAIDO_HOME") {
        let candidate = Path::new(&home).join("../std");
        if candidate.exists() {
            return Some(candidate);
        }
    }

    None
}

// ---------------------------------------------------------------------------
// Sapporo library discovery (DOM bindings)
// ---------------------------------------------------------------------------

/// Finds sapporo.hk (the Hokkaido DOM library).
/// Returns the path to sapporo.hk if found.
pub fn find_sapporo_hk() -> Option<PathBuf> {
    // 1. HOKKAIDO_SAPPORO env var
    if let Ok(sapporo_path) = std::env::var("HOKKAIDO_SAPPORO") {
        let candidate = Path::new(&sapporo_path).join("sapporo.hk");
        if candidate.exists() {
            return Some(candidate);
        }
        // Also check sapporo/sapporo.hk subdirectory layout
        let candidate = Path::new(&sapporo_path).join("sapporo").join("sapporo.hk");
        if candidate.exists() {
            return Some(candidate);
        }
    }

    // 2. Walk up from binary location
    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            let mut dir = parent.to_path_buf();
            loop {
                // Dev tree: <root>/sapporo/sapporo/sapporo.hk
                let candidate = dir.join("sapporo").join("sapporo").join("sapporo.hk");
                if candidate.exists() {
                    return Some(candidate);
                }
                // Nix layout: <root>/share/otaru/sapporo/sapporo.hk
                let candidate = dir.join("share/otaru/sapporo/sapporo.hk");
                if candidate.exists() {
                    return Some(candidate);
                }
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    // 3. HOKKAIDO_HOME fallback
    if let Ok(home) = std::env::var("HOKKAIDO_HOME") {
        let candidate = Path::new(&home).join("../sapporo/sapporo/sapporo.hk");
        if candidate.exists() {
            return Some(candidate);
        }
    }

    None
}

/// Finds sapporo.js (the JavaScript bridge).
/// Returns the path to sapporo.js if found.
pub fn find_sapporo_js() -> Option<PathBuf> {
    // 1. HOKKAIDO_SAPPORO env var
    if let Ok(sapporo_path) = std::env::var("HOKKAIDO_SAPPORO") {
        let candidate = Path::new(&sapporo_path).join("sapporo.js");
        if candidate.exists() {
            return Some(candidate);
        }
    }

    // 2. Walk up from binary location
    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            let mut dir = parent.to_path_buf();
            loop {
                // Dev tree: <root>/sapporo/sapporo.js
                let candidate = dir.join("sapporo").join("sapporo.js");
                if candidate.exists() {
                    return Some(candidate);
                }
                // Nix layout: <root>/share/otaru/sapporo/sapporo.js
                let candidate = dir.join("share/otaru/sapporo/sapporo.js");
                if candidate.exists() {
                    return Some(candidate);
                }
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    // 3. HOKKAIDO_HOME fallback
    if let Ok(home) = std::env::var("HOKKAIDO_HOME") {
        let candidate = Path::new(&home).join("../sapporo/sapporo.js");
        if candidate.exists() {
            return Some(candidate);
        }
    }

    None
}

// ---------------------------------------------------------------------------
// File utilities
// ---------------------------------------------------------------------------

pub fn collect_hk_files(dir: &Path) -> Vec<PathBuf> {
    let mut files = Vec::new();
    if let Ok(entries) = std::fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_file() && path.extension().map_or(false, |e| e == "hk") {
                files.push(path);
            }
        }
    }
    files.sort();
    files
}

pub fn has_hk_files() -> bool {
    !collect_hk_files(Path::new("src")).is_empty()
}

// ---------------------------------------------------------------------------
// Executable discovery (generic)
// ---------------------------------------------------------------------------

pub fn find_executable(candidates: &[&str]) -> Option<String> {
    // Try `which` first
    for name in candidates {
        if let Ok(output) = Command::new("which").arg(name).output() {
            if output.status.success() {
                let path = String::from_utf8_lossy(&output.stdout).trim().to_string();
                if !path.is_empty() {
                    return Some(path);
                }
            }
        }
    }

    // Search PATH manually
    if let Ok(path) = std::env::var("PATH") {
        for dir in path.split(':') {
            for name in candidates {
                let full = format!("{}/{}", dir, name);
                if Path::new(&full).is_file() {
                    return Some(full);
                }
            }
        }
    }

    None
}

pub fn find_wasm_ld() -> Option<String> {
    find_executable(&["wasm-ld", "wasm-ld-19", "wasm-ld-18", "wasm-ld-17"])
        .or_else(|| {
            // Fallback: search /nix/store
            if let Ok(output) = Command::new("find")
                .arg("/nix/store")
                .arg("-name")
                .arg("wasm-ld")
                .arg("-type")
                .arg("f")
                .arg("-print")
                .arg("-quit")
                .output()
            {
                if output.status.success() {
                    let path = String::from_utf8_lossy(&output.stdout).trim().to_string();
                    if !path.is_empty() && Path::new(&path).is_file() {
                        return Some(path);
                    }
                }
            }
            None
        })
}

// ---------------------------------------------------------------------------
// Command execution
// ---------------------------------------------------------------------------

pub fn run_cmd(cmd: &mut Command, description: &str) {
    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error {}: {}", description, e);
        std::process::exit(1);
    });
    if !status.success() {
        eprintln!("{} failed (exit code: {})", description, status.code().unwrap_or(-1));
        std::process::exit(1);
    }
}

pub fn ensure_build_dir() {
    std::fs::create_dir_all("build").unwrap_or_else(|e| {
        eprintln!("Error creating build/ directory: {}", e);
        std::process::exit(1);
    });
}

// ---------------------------------------------------------------------------
// Project type detection
// ---------------------------------------------------------------------------

pub fn is_c_project() -> bool {
    load_manifest().build.is_some()
}

pub fn has_build_targets() -> bool {
    load_manifest()
        .build
        .as_ref()
        .map_or(false, |b| b.targets.is_some())
}
