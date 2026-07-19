use std::io::{BufRead, Write};
use std::path::Path;
use std::process::Command;
use std::time::UNIX_EPOCH;

use crate::utils::{collect_hk_files, ensure_build_dir, find_hokkaido, find_std_dir};

pub fn run(file: Option<&str>, freestanding: bool, force: bool, release: bool, triple: Option<&str>) {
    if let Some(f) = file {
        compile_single(f, freestanding, force, release, triple);
    } else {
        compile_project(freestanding, force, release, triple);
    }
}

pub fn compile_single_or_project(
    file: Option<&str>,
    freestanding: bool,
    force: bool,
    release: bool,
    triple: Option<&str>,
) -> String {
    if let Some(f) = file {
        compile_single(f, freestanding, force, release, triple)
    } else {
        compile_project(freestanding, force, release, triple)
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
    tokens.push(file_metadata_token(Path::new(hokkaido_bin)));

    for f in &collect_hk_files(src_dir) {
        tokens.push(file_metadata_token(f));
    }

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

fn compile_single(file: &str, freestanding: bool, force: bool, release: bool, triple: Option<&str>) -> String {
    let path = Path::new(file);
    if !path.exists() {
        eprintln!("Error: file '{}' not found", file);
        std::process::exit(1);
    }

    let hokkaido = find_hokkaido();
    ensure_build_dir();

    let stem = path.file_stem().unwrap_or_default().to_string_lossy();
    let output_base = format!("build/{}", stem);
    let cache_key = compute_cache_key(path.parent().unwrap_or(Path::new(".")), &hokkaido);

    if !force {
        let cache_path = build_dir().join(format!(".{}.hkbuildcache.{}", stem, cache_suffix(release)));
        if let Some(cached) = load_cache(&cache_path) {
            if cached == cache_key {
                println!("Cached: {} (unchanged, skipping compilation)", file);
                if freestanding || triple.is_some() {
                    return output_base;
                }
                let obj_path = format!("{}.o", output_base);
                if Path::new(&obj_path).exists() {
                    link_with_clang(&[obj_path], &output_base, &None, release);
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
    if let Some(t) = triple {
        cmd.arg("--target").arg(t);
    }

    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error running hokkaido: {}", e);
        std::process::exit(1);
    });

    if !status.success() {
        eprintln!("Compilation failed");
        std::process::exit(1);
    }

    if !force {
        let cache_path = build_dir().join(format!(".{}.hkbuildcache.{}", stem, cache_suffix(release)));
        save_cache(&cache_path, &cache_key);
    }

    let obj_path = format!("{}.o", output_base);

    if freestanding || triple.is_some() {
        println!("Compiled {} -> {} (freestanding)", file, obj_path);
        if triple.is_some() {
            println!("Warning: cross-compiled object files cannot be linked with clang.");
            println!("Use wasm-ld or the appropriate linker for your target.");
        } else {
            println!("Warning: freestanding object files cannot be linked with clang.");
            println!("Use ld.lld directly or a custom linker script.");
        }
        return output_base;
    }

    link_with_clang(&[obj_path], &output_base, &None, release);
    println!("Built: {}", output_base);
    output_base
}

fn compile_project(freestanding: bool, force: bool, release: bool, triple: Option<&str>) -> String {
    let manifest_path = Path::new("otaru.toml");
    if !manifest_path.exists() {
        eprintln!("Error: otaru.toml not found (not in an otaru project)");
        eprintln!("Hint: compile a single file with: otaru build <file.hk>");
        std::process::exit(1);
    }

    let manifest = crate::utils::load_manifest();
    let build_config = manifest.build;

    let src_dir = Path::new("src");
    if !src_dir.is_dir() {
        eprintln!("Error: src/ directory not found");
        std::process::exit(1);
    }

    let hokkaido = find_hokkaido();
    ensure_build_dir();

    let binary = format!("build/{}", manifest.package.name);
    let cache_key = compute_cache_key(src_dir, &hokkaido);

    if !force {
        let cache_path = format!("build/.hkbuildcache.{}", cache_suffix(release));
        if let Some(cached) = load_cache(Path::new(&cache_path)) {
            if cached == cache_key {
                let obj_path = format!("{}.o", binary);
                if Path::new(&obj_path).exists() {
                    if !freestanding && triple.is_none() {
                        let mut all_objects = vec![obj_path.clone()];
                        if let Some(ref config) = build_config {
                            all_objects.extend(compile_extra_sources(config, force, release));
                        }
                        link_with_clang(&all_objects, &binary, &build_config, release);
                    }
                    println!("Built: {} (cached)", binary);
                    return binary;
                }
            }
        }
    }

    let hk_files = collect_hk_files(src_dir);
    if hk_files.is_empty() {
        eprintln!("Error: no .hk files found in src/");
        std::process::exit(1);
    }

    let entry = hk_files
        .iter()
        .find(|f| f.file_stem().map_or(false, |s| s == "main"))
        .unwrap_or(&hk_files[0]);

    let mut cmd = Command::new(&hokkaido);
    cmd.arg(entry).arg("-o").arg(&binary);
    cmd.arg(opt_flag(release));
    if freestanding {
        cmd.arg("--freestanding");
    }
    if let Some(t) = triple {
        cmd.arg("--target").arg(t);
    }

    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error running hokkaido: {}", e);
        std::process::exit(1);
    });

    if !status.success() {
        eprintln!("Compilation failed");
        std::process::exit(1);
    }

    if !force {
        let cache_path = format!("build/.hkbuildcache.{}", cache_suffix(release));
        save_cache(Path::new(&cache_path), &cache_key);
    }

    let obj_path = format!("{}.o", binary);

    if freestanding || triple.is_some() {
        println!("Compiled {} -> {} (freestanding)", entry.display(), obj_path);
        if triple.is_some() {
            println!("Warning: cross-compiled object files cannot be linked with clang.");
            println!("Use wasm-ld or the appropriate linker for your target.");
        } else {
            println!("Warning: freestanding object files cannot be linked with clang.");
            println!("Use ld.lld directly or a custom linker script.");
        }
        return binary;
    }

    let mut all_objects = vec![obj_path];
    if let Some(ref config) = build_config {
        all_objects.extend(compile_extra_sources(config, force, release));
    }

    link_with_clang(&all_objects, &binary, &build_config, release);
    println!("Built: {}", binary);
    binary
}

fn compile_extra_sources(config: &crate::manifest::Build, force: bool, release: bool) -> Vec<String> {
    if config.sources.is_empty() {
        return Vec::new();
    }
    let sources = crate::cbuild::resolve_sources(&config.sources);
    sources
        .iter()
        .map(|s| crate::cbuild::compile_source(s, config, force, release))
        .collect()
}

fn link_with_clang(
    obj_paths: &[String],
    output: &str,
    config: &Option<crate::manifest::Build>,
    release: bool,
) {
    let compiler = config
        .as_ref()
        .map(|c| c.compiler.as_str())
        .unwrap_or("clang");

    let mut cmd = Command::new(compiler);

    for obj in obj_paths {
        cmd.arg(obj);
    }

    cmd.arg("-o").arg(output);

    if let Some(config) = config {
        for flag in &config.ldflags {
            cmd.arg(flag);
        }
        for dir in &config.lib_dirs {
            cmd.arg(format!("-L{}", dir));
        }
        for lib in &config.link {
            cmd.arg(format!("-l{}", lib));
        }
        for lib in &config.libraries {
            let resolved = crate::cbuild::resolve_library(lib, &config.lib_dirs);
            cmd.arg(&resolved);
        }
        if release {
            cmd.arg("-O2");
        }
    }

    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error linking with {}: {} (is {} installed?)", output, e, compiler);
        std::process::exit(1);
    });

    if !status.success() {
        eprintln!("Linking failed: {}", output);
        std::process::exit(1);
    }
}

fn build_dir() -> std::path::PathBuf {
    std::path::PathBuf::from("build")
}
