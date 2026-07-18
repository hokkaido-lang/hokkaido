use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use crate::manifest::{Manifest, MANIFEST_FILE};

// Embed sapporo library files at compile time
// In dev: resolved relative to sapporo-cli/src/ -> ../../sapporo/
// In nix: preBuild copies files to ../sapporo/ relative to source root
const SAPPORO_HK: &[u8] = include_bytes!("../../sapporo/sapporo/sapporo.hk");
const SAPPORO_JS: &[u8] = include_bytes!("../../sapporo/sapporo.js");

/// Returns true if `src` is newer than `dst`, or `dst` doesn't exist
fn is_stale(src: &Path, dst: &Path) -> bool {
    match (fs::metadata(src), fs::metadata(dst)) {
        (Ok(s), Ok(d)) => {
            let src_time = s.modified().unwrap_or(std::time::SystemTime::UNIX_EPOCH);
            let dst_time = d.modified().unwrap_or(std::time::SystemTime::UNIX_EPOCH);
            src_time > dst_time
        }
        _ => true,
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
    if let Ok(exe) = std::env::current_exe() {
        if let Some(exe_dir) = exe.parent() {
            let candidates = [
                exe_dir.join("../../build/hokkaido"),
                exe_dir.join("../../../build/hokkaido"),
                exe_dir.join("hokkaido"),
            ];
            for candidate in &candidates {
                if candidate.exists() {
                    return candidate
                        .canonicalize()
                        .unwrap_or_else(|_| candidate.clone())
                        .to_string_lossy()
                        .to_string();
                }
            }
        }
    }

    eprintln!("Error: hokkaido compiler not found.");
    eprintln!("Install hokkaido or add it to PATH.");
    std::process::exit(1);
}

/// Find the std library by checking HOKKAIDO_STD env var, then relative paths.
pub fn find_std() -> Option<String> {
    // 1. Check HOKKAIDO_STD environment variable
    if let Ok(std_path) = std::env::var("HOKKAIDO_STD") {
        let candidate = Path::new(&std_path);
        if candidate.join("hk.mod").exists() {
            return Some(std_path);
        }
    }

    // 2. Walk up from binary location
    if let Ok(exe) = std::env::current_exe() {
        if let Some(exe_dir) = exe.parent() {
            let mut dir = exe_dir.to_path_buf();
            loop {
                let candidate = dir.join("std");
                if candidate.join("hk.mod").exists() {
                    return Some(candidate.to_string_lossy().to_string());
                }
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    None
}

/// Copy std library into the project if not already present
pub fn prepare_std(project_dir: &Path, verbose: bool) {
    if let Some(src) = find_std() {
        let dst = project_dir.join("std");
        if !dst.exists() {
            if let Err(e) = copy_dir_recursive(Path::new(&src), &dst) {
                eprintln!("Warning: could not copy std library: {}", e);
            } else if verbose {
                println!("  std/ prepared from {}", src);
            }
        }
    }
}

fn copy_dir_recursive(src: &Path, dst: &Path) -> std::io::Result<()> {
    fs::create_dir_all(dst)?;
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let ty = entry.file_type()?;
        if ty.is_dir() {
            copy_dir_recursive(&entry.path(), &dst.join(entry.file_name()))?;
        } else {
            fs::copy(entry.path(), dst.join(entry.file_name()))?;
        }
    }
    Ok(())
}

/// Find wasm-ld binary
pub fn find_wasm_ld() -> String {
    for name in &[
        "wasm-ld",
        "wasm-ld-21",
        "wasm-ld-20",
        "wasm-ld-19",
        "wasm-ld-18",
    ] {
        if let Ok(output) = Command::new("which").arg(name).output() {
            if output.status.success() {
                let path = String::from_utf8_lossy(&output.stdout).trim().to_string();
                if !path.is_empty() {
                    return path;
                }
            }
        }
    }

    // Check common locations
    let common_paths = [
        "/usr/bin/wasm-ld",
        "/usr/local/bin/wasm-ld",
        "/opt/homebrew/bin/wasm-ld",
    ];
    for path in &common_paths {
        if Path::new(path).exists() {
            return path.to_string();
        }
    }

    eprintln!("Error: wasm-ld not found.");
    eprintln!("Install LLVM/wasm-ld or add it to PATH.");
    std::process::exit(1);
}

/// Write embedded sapporo.hk to a directory (for compiler import resolution)
/// Always overwrites to ensure the latest version is used
pub fn write_sapporo_hk(dest_dir: &Path, verbose: bool) {
    let hk_dir = dest_dir.join("sapporo");
    let hk_file = hk_dir.join("sapporo.hk");

    fs::create_dir_all(&hk_dir).unwrap_or_else(|e| {
        eprintln!("Error creating {}: {}", hk_dir.display(), e);
        std::process::exit(1);
    });

    if verbose && hk_file.exists() {
        println!("  Updating sapporo/sapporo.hk");
    }

    fs::write(&hk_file, SAPPORO_HK).unwrap_or_else(|e| {
        eprintln!("Error writing {}: {}", hk_file.display(), e);
        std::process::exit(1);
    });

    // hk.mod for the inner sapporo/ package
    let hkmod = hk_dir.join("hk.mod");
    if !hkmod.exists() {
        fs::write(&hkmod, "").unwrap_or(());
    }
}

/// Write embedded sapporo.js to a directory
/// Always overwrites to ensure the latest version is used
pub fn write_sapporo_js(dest_dir: &Path, verbose: bool) {
    let js_file = dest_dir.join("sapporo.js");

    if verbose && js_file.exists() {
        println!("  Updating sapporo.js");
    }

    fs::write(&js_file, SAPPORO_JS).unwrap_or_else(|e| {
        eprintln!("Error writing {}: {}", js_file.display(), e);
        std::process::exit(1);
    });
}

/// Build a sapporo project
pub fn run(force: bool, verbose: bool) {
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

    if verbose {
        println!("  hokkaido: {}", hokkaido);
        println!("  wasm-ld: {}", wasm_ld);
    }

    let dist = Path::new(&build.dist);
    let output_wasm = dist.join(format!("{}.wasm", manifest.package.name));

    println!("Building {}...", manifest.package.name);

    // Create dist directory
    fs::create_dir_all(dist).unwrap_or_else(|e| {
        eprintln!("Error creating {}: {}", dist.display(), e);
        std::process::exit(1);
    });

    // Write embedded sapporo.hk to project root for compiler import resolution
    write_sapporo_hk(Path::new("."), verbose);

    // Write embedded sapporo.js to dist/
    write_sapporo_js(dist, verbose);

    // Prepare std library if available
    prepare_std(Path::new("."), verbose);

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
            if verbose {
                println!("  {} (up to date)", file.display());
            }
            objects.push(obj);
            continue;
        }

        let compile_target = dist.join(&*stem);
        print!("  Compiling {}...", file.display());
        if compile_hk(&hokkaido, file, &compile_target, &build.cflags, verbose) {
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
    if link_wasm(&wasm_ld, &objects, &output_wasm, &build.ldflags, verbose) {
        println!(" ok");
    } else {
        println!(" FAILED");
        std::process::exit(1);
    }

    println!();
    println!("Built: {}", output_wasm.display());
    println!("To run: sapporo run");
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
                    // Skip files inside sapporo/ directory (the library)
                    if path.starts_with("sapporo/") {
                        continue;
                    }
                    files.push(path);
                }
            }
        }
    }
    files.sort();
    files
}

/// Compile a single .hk file to .o (WASM object)
pub fn compile_hk(
    hokkaido: &str,
    file: &Path,
    output: &Path,
    extra_flags: &[String],
    verbose: bool,
) -> bool {
    let mut cmd = Command::new(hokkaido);
    cmd.arg(file)
        .arg("-o")
        .arg(output)
        .arg("--target")
        .arg("wasm32-unknown-unknown");

    for flag in extra_flags {
        cmd.arg(flag);
    }

    if verbose {
        println!("    {:?}", cmd);
    }

    match cmd.output() {
        Ok(output) => {
            if !output.status.success() {
                let stderr = String::from_utf8_lossy(&output.stderr);
                if !stderr.is_empty() {
                    eprintln!("\n{}", stderr);
                }
                false
            } else {
                true
            }
        }
        Err(e) => {
            eprintln!("Error running hokkaido: {}", e);
            false
        }
    }
}

/// Link .o files into .wasm using wasm-ld
pub fn link_wasm(
    wasm_ld: &str,
    objects: &[PathBuf],
    output: &Path,
    ldflags: &[String],
    verbose: bool,
) -> bool {
    let mut cmd = Command::new(wasm_ld);
    cmd.arg("--no-entry")
        .arg("--export-all")
        .arg("--allow-undefined")
        .arg("-o")
        .arg(output);

    for flag in ldflags {
        cmd.arg(flag);
    }

    for obj in objects {
        cmd.arg(obj);
    }

    if verbose {
        println!("    {:?}", cmd);
    }

    match cmd.output() {
        Ok(output) => {
            if !output.status.success() {
                let stderr = String::from_utf8_lossy(&output.stderr);
                if !stderr.is_empty() {
                    eprintln!("\n{}", stderr);
                }
                false
            } else {
                true
            }
        }
        Err(e) => {
            eprintln!("Error running wasm-ld: {}", e);
            false
        }
    }
}
