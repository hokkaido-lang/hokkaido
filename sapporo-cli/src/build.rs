use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use crate::manifest::{Manifest, MANIFEST_FILE};

/// Returns true if `src` is newer than `dst`, or `dst` doesn't exist
fn is_stale(src: &Path, dst: &Path) -> bool {
    match (fs::metadata(src), fs::metadata(dst)) {
        (Ok(s), Ok(d)) => {
            let src_time = s.modified().unwrap_or(std::time::SystemTime::UNIX_EPOCH);
            let dst_time = d.modified().unwrap_or(std::time::SystemTime::UNIX_EPOCH);
            src_time > dst_time
        }
        _ => true, // missing destination = stale
    }
}

/// Find hokkaido binary by walking up directories and checking PATH
pub fn find_hokkaido() -> String {
    // Check PATH first
    if let Ok(output) = Command::new("which").arg("hokkaido").output() {
        if output.status.success() {
            let path = String::from_utf8_lossy(&output.stdout).trim().to_string();
            if !path.is_empty() {
                return path;
            }
        }
    }

    // Walk up from cwd looking for build/hokkaido
    let mut dir = std::env::current_dir().unwrap_or_default();
    for _ in 0..10 {
        let candidate = dir.join("build").join("hokkaido");
        if candidate.exists() {
            return candidate.to_string_lossy().to_string();
        }
        dir = match dir.parent() {
            Some(p) => p.to_path_buf(),
            None => break,
        };
    }

    // Walk from sapporo binary location
    // Binary: hokkaido/sapporo-cli/target/debug/sapporo
    // Hokkaido: hokkaido/build/hokkaido
    if let Ok(exe) = std::env::current_exe() {
        if let Some(exe_dir) = exe.parent() {
            // Try multiple relative paths from the binary
            let candidates = [
                exe_dir.join("../../build/hokkaido"),        // sapporo-cli/build/hokkaido (wrong)
                exe_dir.join("../../../build/hokkaido"),     // hokkaido/build/hokkaido (right)
                exe_dir.join("../../../../build/hokkaido"),  // one more level up
            ];
            for candidate in &candidates {
                if candidate.exists() {
                    return candidate.canonicalize().unwrap_or_else(|_| candidate.clone())
                        .to_string_lossy().to_string();
                }
            }
        }
    }

    eprintln!("Error: hokkaido compiler not found.");
    eprintln!("Install hokkaido or add it to PATH.");
    std::process::exit(1);
}

/// Find wasm-ld binary
pub fn find_wasm_ld() -> String {
    // Check PATH
    for name in &["wasm-ld", "wasm-ld-21", "wasm-ld-20", "wasm-ld-19", "wasm-ld-18"] {
        if let Ok(output) = Command::new("which").arg(name).output() {
            if output.status.success() {
                let path = String::from_utf8_lossy(&output.stdout).trim().to_string();
                if !path.is_empty() {
                    return path;
                }
            }
        }
    }

    // Search nix store
    if let Ok(output) = Command::new("find")
        .arg("/nix/store")
        .arg("-name")
        .arg("wasm-ld")
        .arg("-type")
        .arg("f")
        .output()
    {
        let stdout = String::from_utf8_lossy(&output.stdout);
        if let Some(path) = stdout.lines().next() {
            if !path.is_empty() {
                return path.to_string();
            }
        }
    }

    eprintln!("Error: wasm-ld not found.");
    eprintln!("Install LLVM/wasm-ld or add it to PATH.");
    std::process::exit(1);
}

/// Find the sapporo library directory (ships with the binary)
pub fn find_sapporo_lib() -> PathBuf {
    // 1. Check relative to binary location
    if let Ok(exe) = std::env::current_exe() {
        if let Some(exe_dir) = exe.parent() {
            let candidates = [
                exe_dir.join("../../sapporo/sapporo"),          // sapporo-cli/sapporo/sapporo (wrong)
                exe_dir.join("../../../sapporo/sapporo"),       // hokkaido/sapporo/sapporo (right)
                exe_dir.join("share/sapporo/sapporo"),         // installed
            ];
            for candidate in &candidates {
                if candidate.join("sapporo.hk").exists() {
                    return candidate.clone();
                }
            }
        }
    }

    // 2. Check relative to workspace root
    let mut dir = std::env::current_dir().unwrap_or_default();
    for _ in 0..10 {
        let candidate = dir.join("sapporo/sapporo");
        if candidate.join("sapporo.hk").exists() {
            return candidate;
        }
        dir = match dir.parent() {
            Some(p) => p.to_path_buf(),
            None => break,
        };
    }

    eprintln!("Error: sapporo library (sapporo.hk) not found.");
    std::process::exit(1);
}

/// Find the sapporo.js file
pub fn find_sapporo_js() -> PathBuf {
    if let Ok(exe) = std::env::current_exe() {
        if let Some(exe_dir) = exe.parent() {
            let candidates = [
                exe_dir.join("../../sapporo/sapporo.js"),       // sapporo-cli/sapporo
                exe_dir.join("../../../sapporo/sapporo.js"),    // hokkaido/sapporo
                exe_dir.join("share/sapporo/sapporo.js"),      // installed
            ];
            for candidate in &candidates {
                if candidate.exists() {
                    return candidate.clone();
                }
            }
        }
    }

    let mut dir = std::env::current_dir().unwrap_or_default();
    for _ in 0..10 {
        let candidate = dir.join("sapporo/sapporo.js");
        if candidate.exists() {
            return candidate;
        }
        dir = match dir.parent() {
            Some(p) => p.to_path_buf(),
            None => break,
        };
    }

    eprintln!("Error: sapporo.js not found.");
    std::process::exit(1);
}

/// Find all .hk source files in the given directories
pub fn find_hk_files(sources: &[String]) -> Vec<PathBuf> {
    let mut files = Vec::new();
    for src in sources {
        let src_path = Path::new(src);
        if src_path.is_file() && src_path.extension().map(|e| e == "hk").unwrap_or(false) {
            files.push(src_path.to_path_buf());
        } else if src_path.is_dir() {
            for entry in glob::glob(&format!("{}/**/*.hk", src)).unwrap() {
                if let Ok(path) = entry {
                    files.push(path);
                }
            }
        }
    }
    files.sort();
    files
}

/// Compile a single .hk file to .o (WASM object)
pub fn compile_hk(hokkaido: &str, file: &Path, output: &Path, extra_flags: &[String]) -> bool {
    let mut cmd = Command::new(hokkaido);
    cmd.arg(file)
        .arg("-o")
        .arg(output)
        .arg("--target")
        .arg("wasm32-unknown-unknown");

    for flag in extra_flags {
        cmd.arg(flag);
    }

    let status = cmd.status();
    match status {
        Ok(s) => s.success(),
        Err(e) => {
            eprintln!("Error running hokkaido: {}", e);
            false
        }
    }
}

/// Link .o files into .wasm using wasm-ld
pub fn link_wasm(wasm_ld: &str, objects: &[PathBuf], output: &Path, ldflags: &[String]) -> bool {
    let mut cmd = Command::new(wasm_ld);
    cmd.arg("--no-entry")
        .arg("--export=main")
        .arg("--allow-undefined")
        .arg("-o")
        .arg(output);

    for flag in ldflags {
        cmd.arg(flag);
    }

    for obj in objects {
        cmd.arg(obj);
    }

    let status = cmd.status();
    match status {
        Ok(s) => s.success(),
        Err(e) => {
            eprintln!("Error running wasm-ld: {}", e);
            false
        }
    }
}

/// Build a sapporo project
pub fn run(force: bool) {
    let manifest_path = Path::new(MANIFEST_FILE);
    if !manifest_path.exists() {
        eprintln!("Error: {} not found. Run 'sapporo init' first.", MANIFEST_FILE);
        std::process::exit(1);
    }

    let manifest = Manifest::load(manifest_path).unwrap_or_else(|e| {
        eprintln!("{}", e);
        std::process::exit(1);
    });

    let build = manifest.build.unwrap_or_default();
    let hokkaido = find_hokkaido();
    let wasm_ld = find_wasm_ld();
    let sapporo_lib = find_sapporo_lib();
    let sapporo_js = find_sapporo_js();

    let dist = Path::new(&build.dist);
    let output_wasm = dist.join(format!("{}.wasm", manifest.package.name));

    println!("Building {}...", manifest.package.name);

    // Create dist directory
    fs::create_dir_all(dist).unwrap_or_else(|e| {
        eprintln!("Error creating {}: {}", dist.display(), e);
        std::process::exit(1);
    });

    // Create sapporo/ import directory in project root for hokkaido compiler
    // hokkaido resolves `import "sapporo"` by looking for sapporo/hk.mod + sapporo/sapporo.hk
    let local_sapporo = Path::new("sapporo");
    let local_sapporo_hk = local_sapporo.join("sapporo.hk");
    if !local_sapporo_hk.exists() {
        if sapporo_lib.join("sapporo.hk").exists() {
            fs::create_dir_all(local_sapporo).unwrap_or_else(|e| {
                eprintln!("Error creating {}: {}", local_sapporo.display(), e);
                std::process::exit(1);
            });
            fs::copy(sapporo_lib.join("sapporo.hk"), &local_sapporo_hk).unwrap_or_else(|e| {
                eprintln!("Error copying sapporo.hk: {}", e);
                std::process::exit(1);
            });
        }
    }

    // Create sapporo/hk.mod if missing
    let local_hkmod = local_sapporo.join("hk.mod");
    if !local_hkmod.exists() {
        fs::write(&local_hkmod, "").unwrap_or_else(|e| {
            eprintln!("Error writing {}: {}", local_hkmod.display(), e);
            std::process::exit(1);
        });
    }

    // Copy sapporo.js to dist
    if sapporo_js.exists() {
        fs::copy(&sapporo_js, dist.join("sapporo.js")).unwrap_or_else(|e| {
            eprintln!("Error copying sapporo.js: {}", e);
            std::process::exit(1);
        });
    }

    // Copy index.html to dist
    if Path::new("index.html").exists() {
        fs::copy("index.html", dist.join("index.html")).unwrap_or_else(|e| {
            eprintln!("Error copying index.html: {}", e);
            std::process::exit(1);
        });
    }

    // Find source files
    let hk_files = find_hk_files(&build.sources);
    if hk_files.is_empty() {
        eprintln!("No .hk files found in {:?}", build.sources);
        std::process::exit(1);
    }

    // Compile each .hk file to .o in dist/ (incremental — skip if .o is newer)
    let mut objects = Vec::new();
    let mut any_compiled = false;
    for file in &hk_files {
        let stem = file.file_stem().unwrap().to_string_lossy();
        let obj = dist.join(format!("{}.o", stem));

        if !force && !is_stale(file, &obj) && obj.exists() {
            println!("  {} (up to date)", file.display());
            objects.push(obj);
            continue;
        }

        let compile_target = dist.join(&*stem);
        print!("  Compiling {}...", file.display());
        if compile_hk(&hokkaido, file, &compile_target, &build.cflags) {
            println!(" ok");
            objects.push(obj);
            any_compiled = true;
        } else {
            println!(" FAILED");
            std::process::exit(1);
        }
    }

    // Relink if any object was recompiled or if .wasm doesn't exist
    if !any_compiled && output_wasm.exists() {
        println!();
        println!("Build up to date: {}", output_wasm.display());
        return;
    }

    // Link all .o files into .wasm
    print!("  Linking {}...", output_wasm.display());
    if link_wasm(&wasm_ld, &objects, &output_wasm, &build.ldflags) {
        println!(" ok");
    } else {
        println!(" FAILED");
        std::process::exit(1);
    }

    println!();
    println!("Built: {}", output_wasm.display());
    println!("To run: sapporo run");
}
