use std::io::{BufRead, Write};
use std::path::Path;
use std::process::Command;
use std::time::UNIX_EPOCH;

pub fn run(file: Option<&str>, freestanding: bool, force: bool, release: bool) {
    if let Some(f) = file {
        compile_single(f, freestanding, force, release);
    } else {
        compile_project(freestanding, force, release);
    }
}

/// Compile a single file or project and return the binary path (without extension)
pub fn compile_single_or_project(file: Option<&str>, freestanding: bool, force: bool, release: bool) -> String {
    if let Some(f) = file {
        compile_single(f, freestanding, force, release)
    } else {
        compile_project(freestanding, force, release)
    }
}

fn opt_flag(release: bool) -> &'static str {
    if release { "-O2" } else { "-O0" }
}

fn file_metadata_token(path: &Path) -> String {
    let meta = std::fs::metadata(path);
    match meta {
        Ok(m) => {
            let mtime = m
                .modified()
                .ok()
                .and_then(|t| t.duration_since(UNIX_EPOCH).ok())
                .map(|d| d.as_nanos())
                .unwrap_or(0);
            format!("{}:{}:{}", path.display(), mtime, m.len())
        }
        Err(_) => format!("{}:0:0", path.display()),
    }
}

fn collect_hk_files(dir: &Path) -> Vec<std::path::PathBuf> {
    let mut files = Vec::new();
    if let Ok(entries) = std::fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.extension().map_or(false, |e| e == "hk") {
                files.push(path);
            }
        }
    }
    files.sort();
    files
}

fn fnv1a_hash(data: &[u8]) -> u64 {
    let mut hash: u64 = 0xcbf29ce484222325;
    for &byte in data {
        hash ^= byte as u64;
        hash = hash.wrapping_mul(0x100000001b3);
    }
    hash
}

fn compute_cache_key(src_dir: &Path, hokkaido_bin: &str) -> String {
    let mut tokens: Vec<String> = Vec::new();

    // Include the compiler binary metadata
    tokens.push(file_metadata_token(Path::new(hokkaido_bin)));

    // Include all .hk files in src/
    for f in &collect_hk_files(src_dir) {
        tokens.push(file_metadata_token(f));
    }

    // Include all .hk files in deps/ if it exists
    let deps_dir = Path::new("deps");
    if deps_dir.is_dir() {
        if let Ok(entries) = std::fs::read_dir(deps_dir) {
            for entry in entries.flatten() {
                let path = entry.path();
                if path.is_dir() {
                    for f in &collect_hk_files(&path) {
                        tokens.push(file_metadata_token(f));
                    }
                }
            }
        }
    }

    // Include standard library files
    if let Some(std_dir) = find_std_dir() {
        for f in &collect_hk_files(&std_dir) {
            tokens.push(file_metadata_token(f));
        }
    }

    tokens.sort();
    let mut buf = Vec::new();
    for t in &tokens {
        buf.extend_from_slice(t.as_bytes());
        buf.push(0);
    }
    format!("{:x}", fnv1a_hash(&buf))
}

fn find_std_dir() -> Option<std::path::PathBuf> {
    // Check next to the running otaru binary
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
    if let Ok(home) = std::env::var("HOKKAIDO_HOME") {
        let candidate = Path::new(&home).join("../std");
        if candidate.exists() {
            return Some(candidate);
        }
    }
    None
}

fn load_cache(cache_path: &Path) -> Option<String> {
    let file = std::fs::File::open(cache_path).ok()?;
    let mut line = String::new();
    std::io::BufReader::new(file).read_line(&mut line).ok()?;
    let cached = line.trim().to_string();
    if cached.is_empty() { None } else { Some(cached) }
}

fn save_cache(cache_path: &Path, key: &str) {
    if let Some(parent) = cache_path.parent() {
        let _ = std::fs::create_dir_all(parent);
    }
    if let Ok(mut f) = std::fs::File::create(cache_path) {
        let _ = writeln!(f, "{}", key);
    }
}

fn cache_suffix(release: bool) -> &'static str {
    if release { "release" } else { "debug" }
}

fn compile_single(file: &str, freestanding: bool, force: bool, release: bool) -> String {
    let path = Path::new(file);
    if !path.exists() {
        eprintln!("Error: file '{}' not found", file);
        std::process::exit(1);
    }

    let hokkaido = find_hokkaido();
    let build_dir = Path::new("build");
    std::fs::create_dir_all(build_dir).unwrap_or_else(|e| {
        eprintln!("Error creating build/ directory: {}", e);
        std::process::exit(1);
    });

    let stem = path.file_stem().unwrap_or_default().to_string_lossy();
    let output_base = format!("build/{}", stem);
    let cache_key = compute_cache_key(path.parent().unwrap_or(Path::new(".")), &hokkaido);

    // Incremental build check
    if !force {
        let cache_path = build_dir.join(format!(".{}.hkbuildcache.{}", stem, cache_suffix(release)));
        if let Some(cached) = load_cache(&cache_path) {
            if cached == cache_key {
                println!("Cached: {} (unchanged, skipping compilation)", file);
                if freestanding {
                    return output_base;
                }
                let obj_path = format!("{}.o", output_base);
                if Path::new(&obj_path).exists() {
                    link_with_clang(&obj_path, &output_base);
                    println!("Built: {} (cached)", output_base);
                    return output_base;
                }
            }
        }
    }

    let mut cmd = Command::new(&hokkaido);
    cmd.arg(file).arg("-o").arg(&output_base);
    cmd.arg(opt_flag(release));
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

    // Save cache
    if !force {
        let cache_path = build_dir.join(format!(".{}.hkbuildcache.{}", stem, cache_suffix(release)));
        save_cache(&cache_path, &cache_key);
    }

    let obj_path = format!("{}.o", output_base);

    if freestanding {
        println!("Compiled {} -> {} (freestanding)", file, obj_path);
        println!("Warning: freestanding object files cannot be linked with clang.");
        println!("Use ld.lld directly or a custom linker script.");
        return output_base;
    }

    link_with_clang(&obj_path, &output_base);
    println!("Built: {}", output_base);
    output_base
}

fn compile_project(freestanding: bool, force: bool, release: bool) -> String {
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

    let binary = format!("build/{}", manifest.package.name);

    let cache_key = compute_cache_key(src_dir, &hokkaido);

    // Incremental build check
    if !force {
        let cache_path = format!("build/.hkbuildcache.{}", cache_suffix(release));
        if let Some(cached) = load_cache(Path::new(&cache_path)) {
            if cached == cache_key {
                let obj_path = format!("{}.o", binary);
                if Path::new(&obj_path).exists() {
                    if !freestanding {
                        link_with_clang(&obj_path, &binary);
                    }
                    println!("Built: {} (cached)", binary);
                    return binary;
                }
            }
        }
    }

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

    let entry = hk_files.iter()
        .find(|f| f.contains("main"))
        .unwrap_or(&hk_files[0]);

    let mut cmd = Command::new(&hokkaido);
    cmd.arg(entry).arg("-o").arg(&binary);
    cmd.arg(opt_flag(release));
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

    // Save cache
    if !force {
        let cache_path = format!("build/.hkbuildcache.{}", cache_suffix(release));
        save_cache(Path::new(&cache_path), &cache_key);
    }

    let obj_path = format!("{}.o", binary);

    if freestanding {
        println!("Compiled {} -> {} (freestanding)", entry, obj_path);
        println!("Warning: freestanding object files cannot be linked with clang.");
        println!("Use ld.lld directly or a custom linker script.");
        return binary;
    }

    link_with_clang(&obj_path, &binary);
    println!("Built: {}", binary);
    binary
}

fn link_with_clang(obj_path: &str, output_base: &str) {
    let link_status = Command::new("clang")
        .arg(obj_path)
        .arg("-o")
        .arg(output_base)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error linking with clang: {} (is clang installed?)", e);
            std::process::exit(1);
        });

    if !link_status.success() {
        eprintln!("Linking failed");
        std::process::exit(1);
    }
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
