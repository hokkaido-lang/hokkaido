use std::path::{Path, PathBuf};
use std::process::Command;

use crate::manifest::Build;
use crate::template;
use crate::utils::{collect_hk_files, ensure_build_dir, find_hokkaido, find_wasm_ld, load_manifest};

// =========================================================================
// LLVM integration
// =========================================================================

pub struct LlvmInfo {
    pub include_dirs: Vec<String>,
    pub cflags: Vec<String>,
    pub ldflags: Vec<String>,
    pub libraries: Vec<String>,
}

pub fn resolve_llvm(config: &Build) -> Option<LlvmInfo> {
    let llvm_bin = config.llvm_config.as_deref()?;

    let mut include_dirs = Vec::new();
    let mut cflags = Vec::new();
    let mut ldflags = Vec::new();
    let mut libraries = Vec::new();

    if let Ok(output) = Command::new(llvm_bin).arg("--cxxflags").output() {
        if output.status.success() {
            let flags = String::from_utf8_lossy(&output.stdout);
            for flag in flags.split_whitespace() {
                if flag == "-fno-exceptions" {
                    continue;
                }
                if let Some(path) = flag.strip_prefix("-I") {
                    include_dirs.push(path.to_string());
                } else {
                    cflags.push(flag.to_string());
                }
            }
        } else {
            eprintln!("Warning: {} --cxxflags failed", llvm_bin);
        }
    } else {
        eprintln!("Warning: could not run '{}' (is LLVM installed?)", llvm_bin);
        return None;
    }

    let mut link_args = vec!["--ldflags".to_string(), "--libs".to_string()];
    if let Some(components) = &config.llvm_components {
        link_args.extend(components.iter().cloned());
    }

    if let Ok(output) = Command::new(llvm_bin).args(&link_args).output() {
        if output.status.success() {
            parse_llvm_flags(&String::from_utf8_lossy(&output.stdout), &mut ldflags, &mut libraries);
        } else {
            eprintln!("Warning: {} --ldflags --libs failed", llvm_bin);
        }
    }

    if let Ok(output) = Command::new(llvm_bin)
        .arg("--system-libs")
        .args(&link_args)
        .output()
    {
        if output.status.success() {
            parse_llvm_flags(&String::from_utf8_lossy(&output.stdout), &mut ldflags, &mut libraries);
        }
    }

    Some(LlvmInfo {
        include_dirs,
        cflags,
        ldflags,
        libraries,
    })
}

fn parse_llvm_flags(output: &str, ldflags: &mut Vec<String>, libraries: &mut Vec<String>) {
    for flag in output.split_whitespace() {
        if let Some(path) = flag.strip_prefix("-L") {
            ldflags.push(format!("-L{}", path));
        } else if let Some(name) = flag.strip_prefix("-l") {
            if !libraries.contains(&name.to_string()) {
                libraries.push(name.to_string());
            }
        } else {
            ldflags.push(flag.to_string());
        }
    }
}

// =========================================================================
// Prebuild
// =========================================================================

pub fn run_prebuild(_config: &Build) {
    // prebuild feature was removed; this is a no-op placeholder
}

// =========================================================================
// Public entry point
// =========================================================================

pub fn run(file: Option<&str>, force: bool, release: bool, target: Option<&str>) {
    if let Some(f) = file {
        let path = Path::new(f);
        if !path.exists() {
            eprintln!("Error: file '{}' not found", f);
            std::process::exit(1);
        }
        let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
        if ext == "hk" {
            if is_wasm_project() {
                build_wasm_from_file(f, release);
                return;
            }
            eprintln!("Error: {} is a Hokkaido file. Use 'otaru build {}' without the C build system.", f, f);
            std::process::exit(1);
        }
        compile_single_file(f, force, release);
        return;
    }

    let manifest_path = Path::new("otaru.toml");
    if !manifest_path.exists() {
        eprintln!("Error: otaru.toml not found");
        std::process::exit(1);
    }

    let manifest = load_manifest();

    let build_config = manifest.build.as_ref().unwrap_or_else(|| {
        eprintln!("Error: no [build] section in otaru.toml");
        std::process::exit(1);
    });

    if let Some(target_name) = target {
        let targets = build_config.targets.as_ref().unwrap_or_else(|| {
            eprintln!("Error: target '{}' not found (no [[build.targets]] section)", target_name);
            std::process::exit(1);
        });
        let target_config = targets.get(target_name).unwrap_or_else(|| {
            eprintln!("Error: target '{}' not found", target_name);
            std::process::exit(1);
        });
        let merged = merge_target_config(build_config, target_config);
        build_target(target_name, &merged, force, release);
    } else if let Some(targets) = &build_config.targets {
        for (name, config) in targets {
            let merged = merge_target_config(build_config, config);
            build_target(name, &merged, force, release);
        }
    } else {
        let name = manifest.package.name.clone();
        build_target(&name, build_config, force, release);
    }
}

// =========================================================================
// Config merging
// =========================================================================

fn merge_target_config(parent: &Build, target: &Build) -> Build {
    let mut merged = target.clone();
    if merged.llvm_config.is_none() {
        merged.llvm_config = parent.llvm_config.clone();
    }
    if merged.llvm_components.is_none() {
        merged.llvm_components = parent.llvm_components.clone();
    }
    if merged.compiler == "cc" && parent.compiler != "cc" {
        merged.compiler = parent.compiler.clone();
    }
    merge_unique(&mut merged.include_dirs, &parent.include_dirs);
    merge_unique(&mut merged.cflags, &parent.cflags);
    merge_unique(&mut merged.ldflags, &parent.ldflags);
    merge_unique(&mut merged.link, &parent.link);
    merge_unique(&mut merged.libraries, &parent.libraries);
    merge_unique(&mut merged.lib_dirs, &parent.lib_dirs);
    merged
}

fn merge_unique(target: &mut Vec<String>, source: &[String]) {
    for item in source {
        if !target.contains(item) {
            target.push(item.clone());
        }
    }
}

// =========================================================================
// Project type detection
// =========================================================================

fn is_wasm_project() -> bool {
    load_manifest()
        .build
        .as_ref()
        .map_or(false, |b| b.kind == "wasm" || b.kind == "web")
}

// =========================================================================
// Wasm build pipeline (unified)
// =========================================================================

fn build_wasm_from_file(file: &str, release: bool) {
    let hokkaido = find_hokkaido();
    let wasm_ld = require_wasm_ld();
    ensure_build_dir();

    let path = Path::new(file);
    let stem = path.file_stem().unwrap_or_default().to_string_lossy();
    let output_base = format!("build/{}", stem);

    compile_hk_wasm(&hokkaido, file, &output_base, release);
    link_wasm(&wasm_ld, &output_base, release);
    copy_wasm_to_serving_dir(&output_base);
}

fn build_wasm_target(name: &str, config: &Build, release: bool) {
    let hokkaido = find_hokkaido();
    let wasm_ld = require_wasm_ld();

    let hk_files = if config.sources.is_empty() {
        collect_hk_files(Path::new("src"))
    } else {
        resolve_hk_sources(&config.sources)
    };

    if hk_files.is_empty() {
        eprintln!("Error: no .hk files found for wasm build");
        std::process::exit(1);
    }

    let entry = hk_files
        .iter()
        .find(|f| f.file_stem().map_or(false, |s| s == "main"))
        .unwrap_or(&hk_files[0]);

    let output_base = format!("build/{}", name);

    compile_hk_wasm(&hokkaido, &entry.to_string_lossy(), &output_base, release);
    link_wasm(&wasm_ld, &output_base, release);
    copy_wasm_to_serving_dir(&output_base);
}

// =========================================================================
// Web build pipeline (sapporo-style: dist/ with sapporo.js + index.html)
// =========================================================================

fn build_web_target(name: &str, config: &Build, release: bool) {
    let hokkaido = find_hokkaido();
    let wasm_ld = require_wasm_ld();
    let dist = config.dist_dir();

    let hk_files = if config.sources.is_empty() {
        collect_hk_files(Path::new("src"))
    } else {
        resolve_hk_sources(&config.sources)
    };

    if hk_files.is_empty() {
        eprintln!("Error: no .hk files found for web build");
        std::process::exit(1);
    }

    // Skip files inside the embedded sapporo/ directory (the library)
    let hk_files: Vec<_> = hk_files
        .into_iter()
        .filter(|f| !f.starts_with("sapporo/"))
        .collect();

    let _entry = hk_files
        .iter()
        .find(|f| f.file_stem().map_or(false, |s| s == "main"))
        .unwrap_or(&hk_files[0]);

    // Create dist directory
    let dist_path = Path::new(dist);
    std::fs::create_dir_all(dist_path).unwrap_or_else(|e| {
        eprintln!("Error creating {}: {}", dist, e);
        std::process::exit(1);
    });

    // Write embedded sapporo library files to project root for compiler import resolution
    template::write_sapporo_hk_to(Path::new("."));

    // Write sapporo.js to dist/
    template::write_sapporo_js_to(dist_path);

    // Copy index.html to dist/ if it exists at project root
    if Path::new("index.html").exists() {
        if let Err(e) = std::fs::copy("index.html", dist_path.join("index.html")) {
            eprintln!("Warning: could not copy index.html to {}: {}", dist, e);
        }
    }

    // Compile each .hk file to .o in dist/ (incremental — skip if .o is newer)
    let mut objects = Vec::new();
    let mut any_compiled = false;

    for file in &hk_files {
        let stem = file.file_stem().unwrap().to_string_lossy();
        let obj = dist_path.join(format!("{}.o", stem));

        if !release && obj.exists() {
            if let (Ok(obj_meta), Ok(src_meta)) =
                (std::fs::metadata(&obj), std::fs::metadata(file))
            {
                if let (Ok(obj_time), Ok(src_time)) = (obj_meta.modified(), src_meta.modified()) {
                    if obj_time >= src_time {
                        objects.push(obj);
                        continue;
                    }
                }
            }
        }

        let compile_target = dist_path.join(&*stem);
        print!("  Compiling {}...", file.display());

        let mut cmd = Command::new(&hokkaido);
        cmd.arg(file)
            .arg("-o")
            .arg(&compile_target)
            .arg("--target")
            .arg("wasm32-unknown-unknown");

        if release {
            cmd.arg("-O2");
        }

        let status = cmd.status().unwrap_or_else(|e| {
            eprintln!("Error running hokkaido: {}", e);
            std::process::exit(1);
        });

        if status.success() {
            println!(" ok");
            objects.push(obj);
            any_compiled = true;
        } else {
            println!(" FAILED");
            std::process::exit(1);
        }
    }

    // Skip relink if no recompilation and .wasm already exists
    let output_wasm = dist_path.join(format!("{}.wasm", name));
    if !any_compiled && output_wasm.exists() {
        println!();
        println!("Build up to date: {}", output_wasm.display());
        return;
    }

    // Link all .o files into .wasm
    let mut cmd = Command::new(&wasm_ld);
    cmd.arg("--no-entry")
        .arg("--export-all")
        .arg("--allow-undefined")
        .arg("-o")
        .arg(&output_wasm);

    for flag in &config.ldflags {
        cmd.arg(flag);
    }

    for obj in &objects {
        cmd.arg(obj);
    }

    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error running wasm-ld: {}", e);
        std::process::exit(1);
    });

    if !status.success() {
        eprintln!("Linking failed");
        std::process::exit(1);
    }

    println!();
    println!("Built: {}", output_wasm.display());
}

fn compile_hk_wasm(hokkaido: &str, file: &str, output_base: &str, release: bool) {
    let mut cmd = Command::new(hokkaido);
    cmd.arg(file)
        .arg("-o")
        .arg(output_base)
        .arg("--target")
        .arg("wasm32-unknown-unknown");
    if release {
        cmd.arg("-O2");
    }
    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error running hokkaido: {}", e);
        std::process::exit(1);
    });
    if !status.success() {
        eprintln!("Compilation failed");
        std::process::exit(1);
    }
}

fn link_wasm(wasm_ld: &str, output_base: &str, _release: bool) {
    let obj_path = format!("{}.o", output_base);
    let wasm_path = format!("{}.wasm", output_base);
    let status = Command::new(wasm_ld)
        .arg("--no-entry")
        .arg("--export=main")
        .arg("--allow-undefined")
        .arg("-o")
        .arg(&wasm_path)
        .arg(&obj_path)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running wasm-ld: {}", e);
            std::process::exit(1);
        });
    if !status.success() {
        eprintln!("Linking failed");
        std::process::exit(1);
    }
    println!("Built: {}", wasm_path);
}

fn copy_wasm_to_serving_dir(output_base: &str) {
    let wasm_path = format!("{}.wasm", output_base);
    let wasm32_main = Path::new("wasm32/main.wasm");
    if wasm32_main.parent().map_or(false, |p| p.is_dir()) {
        if let Err(e) = std::fs::copy(&wasm_path, wasm32_main) {
            eprintln!("Warning: could not copy wasm to wasm32/: {}", e);
        }
    }
}

fn resolve_hk_sources(patterns: &[String]) -> Vec<PathBuf> {
    let mut files = Vec::new();
    for pattern in patterns {
        match glob::glob(pattern) {
            Ok(paths) => {
                for path in paths.flatten() {
                    if path.is_file() && path.extension().map_or(false, |e| e == "hk") {
                        files.push(path);
                    }
                }
            }
            Err(e) => {
                eprintln!("Warning: invalid glob '{}': {}", pattern, e);
            }
        }
    }
    files.sort();
    files
}

fn require_wasm_ld() -> String {
    find_wasm_ld().unwrap_or_else(|| {
        eprintln!("Error: wasm-ld not found.");
        eprintln!("Install LLVM (e.g. nix-shell -p llvmPackages_19.lld) or add wasm-ld to PATH.");
        std::process::exit(1);
    })
}

// =========================================================================
// Build target dispatch
// =========================================================================

fn build_target(name: &str, config: &Build, force: bool, release: bool) {
    ensure_build_dir();

    if config.kind == "wasm" {
        build_wasm_target(name, config, release);
        return;
    }

    if config.is_web() {
        build_web_target(name, config, release);
        return;
    }

    run_prebuild(config);

    let effective_config = apply_llvm_config(config);
    let objects = compile_sources(name, &effective_config.sources, &effective_config, force, release);
    let link_config = build_link_config(config, &effective_config);

    let output = match config.kind.as_str() {
        "executable" => {
            let out = format!("build/{}", name);
            link_with_compiler(&objects, &out, &link_config, release, &[]);
            out
        }
        "staticlib" => {
            let out = format!("build/lib{}.a", name);
            link_staticlib(&objects, &out);
            out
        }
        "sharedlib" => {
            let out = format!("build/lib{}.so", name);
            link_with_compiler(&objects, &out, &link_config, release, &["-shared"]);
            out
        }
        "object" => {
            if objects.len() != 1 {
                eprintln!("Error: 'object' build type requires exactly one source file");
                std::process::exit(1);
            }
            objects[0].clone()
        }
        other => {
            eprintln!("Error: unknown build type '{}'", other);
            std::process::exit(1);
        }
    };

    println!("Built: {}", output);
}

fn apply_llvm_config(config: &Build) -> Build {
    let llvm = match resolve_llvm(config) {
        Some(info) => info,
        None => return config.clone(),
    };

    let mut effective = config.clone();
    merge_unique(&mut effective.include_dirs, &llvm.include_dirs);
    merge_unique(&mut effective.cflags, &llvm.cflags);
    effective
}

fn build_link_config(config: &Build, effective: &Build) -> Build {
    let llvm = resolve_llvm(config);
    let mut link_config = effective.clone();

    if let Some(ref info) = llvm {
        merge_unique(&mut link_config.ldflags, &info.ldflags);
        merge_unique(&mut link_config.link, &info.libraries);
    }

    link_config
}

fn compile_sources(name: &str, source_patterns: &[String], config: &Build, force: bool, release: bool) -> Vec<String> {
    let sources = resolve_sources(source_patterns);
    if sources.is_empty() {
        eprintln!("Error: no source files found for target '{}'", name);
        std::process::exit(1);
    }
    sources
        .iter()
        .map(|s| compile_source(s, config, force, release))
        .collect()
}

// =========================================================================
// Source resolution and compilation
// =========================================================================

pub fn resolve_sources(patterns: &[String]) -> Vec<PathBuf> {
    let mut sources = Vec::new();
    for pattern in patterns {
        match glob::glob(pattern) {
            Ok(paths) => {
                for path in paths.flatten() {
                    if path.is_file() {
                        sources.push(path);
                    }
                }
            }
            Err(e) => {
                eprintln!("Warning: invalid glob pattern '{}': {}", pattern, e);
            }
        }
    }
    sources.sort();
    sources
}

pub fn compile_source(source: &Path, config: &Build, force: bool, release: bool) -> String {
    let stem = source.file_stem().unwrap_or_default().to_string_lossy();
    let obj = format!("build/{}.o", stem);
    let dep = format!("build/{}.d", stem);

    if !force && Path::new(&obj).exists() {
        if let Ok(obj_meta) = std::fs::metadata(&obj) {
            if let Ok(src_meta) = std::fs::metadata(source) {
                if let (Ok(obj_time), Ok(src_time)) = (obj_meta.modified(), src_meta.modified()) {
                    if obj_time >= src_time {
                        if !Path::new(&dep).exists() || deps_are_up_to_date(&dep, obj_time) {
                            return obj;
                        }
                    }
                }
            }
        }
    }

    let mut cmd = Command::new(&config.compiler);
    cmd.arg("-c").arg(source).arg("-o").arg(&obj);

    for dir in &config.include_dirs {
        cmd.arg("-I").arg(dir);
    }
    for flag in &config.cflags {
        cmd.arg(flag);
    }
    if release {
        cmd.arg("-O2");
    } else {
        cmd.arg("-O0");
    }
    cmd.arg("-MMD").arg("-MF").arg(&dep);

    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error compiling {}: {}", source.display(), e);
        std::process::exit(1);
    });

    if !status.success() {
        eprintln!("Compilation failed: {}", source.display());
        std::process::exit(1);
    }

    println!("  Compiling {}", source.display());
    obj
}

pub fn deps_are_up_to_date(dep_file: &str, obj_time: std::time::SystemTime) -> bool {
    if let Ok(content) = std::fs::read_to_string(dep_file) {
        let content = content.replace("\\\n", " ");
        if let Some(deps_part) = content.split_once(':') {
            for dep in deps_part.1.split_whitespace() {
                if !dep.is_empty() {
                    if let Ok(meta) = std::fs::metadata(dep) {
                        if let Ok(dep_time) = meta.modified() {
                            if dep_time > obj_time {
                                return false;
                            }
                        }
                    }
                }
            }
        }
        true
    } else {
        false
    }
}

fn compile_single_file(file: &str, force: bool, release: bool) {
    let path = Path::new(file);
    let stem = path.file_stem().unwrap_or_default().to_string_lossy();

    ensure_build_dir();

    let obj = format!("build/{}.o", stem);
    let output = format!("build/{}", stem);

    if !force && Path::new(&obj).exists() {
        if let (Ok(obj_meta), Ok(src_meta)) =
            (std::fs::metadata(&obj), std::fs::metadata(path))
        {
            if let (Ok(obj_time), Ok(src_time)) = (obj_meta.modified(), src_meta.modified()) {
                if obj_time >= src_time {
                    println!("Cached: {} (unchanged)", file);
                    return;
                }
            }
        }
    }

    let compiler = detect_compiler(path);
    let mut cmd = Command::new(&compiler);
    cmd.arg("-c").arg(file).arg("-o").arg(&obj);
    if release {
        cmd.arg("-O2");
    } else {
        cmd.arg("-O0");
    }
    cmd.arg("-MMD")
        .arg("-MF")
        .arg(format!("build/{}.d", stem));

    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error compiling {}: {}", file, e);
        std::process::exit(1);
    });
    if !status.success() {
        eprintln!("Compilation failed: {}", file);
        std::process::exit(1);
    }

    let link_status = Command::new(&compiler)
        .arg(&obj)
        .arg("-o")
        .arg(&output)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error linking: {}", e);
            std::process::exit(1);
        });
    if !link_status.success() {
        eprintln!("Linking failed");
        std::process::exit(1);
    }

    println!("Built: {}", output);
}

fn detect_compiler(path: &Path) -> &'static str {
    match path.extension().and_then(|e| e.to_str()) {
        Some("cpp") | Some("cc") | Some("cxx") | Some("C") => "c++",
        _ => "cc",
    }
}

// =========================================================================
// Linking (unified)
// =========================================================================

fn link_with_compiler(
    objects: &[String],
    output: &str,
    config: &Build,
    release: bool,
    extra_args: &[&str],
) {
    let mut cmd = Command::new(&config.compiler);
    for arg in extra_args {
        cmd.arg(arg);
    }
    for obj in objects {
        cmd.arg(obj);
    }
    cmd.arg("-o").arg(output);
    for flag in &config.ldflags {
        cmd.arg(flag);
    }
    for lib in &config.link {
        cmd.arg(format!("-l{}", lib));
    }
    for lib in &config.libraries {
        let resolved = resolve_library(lib, &config.lib_dirs);
        cmd.arg(&resolved);
    }
    if release {
        cmd.arg("-O2");
    }

    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error linking {}: {}", output, e);
        std::process::exit(1);
    });
    if !status.success() {
        eprintln!("Linking failed: {}", output);
        std::process::exit(1);
    }
}

fn link_staticlib(objects: &[String], output: &str) {
    let mut cmd = Command::new("ar");
    cmd.arg("rcs").arg(output);
    for obj in objects {
        cmd.arg(obj);
    }
    let status = cmd.status().unwrap_or_else(|e| {
        eprintln!("Error creating static library {}: {}", output, e);
        std::process::exit(1);
    });
    if !status.success() {
        eprintln!("Failed to create static library: {}", output);
        std::process::exit(1);
    }
}

// =========================================================================
// Library resolution
// =========================================================================

pub fn resolve_library(name: &str, lib_dirs: &[String]) -> String {
    if Path::new(name).is_absolute() {
        if Path::new(name).exists() {
            return name.to_string();
        }
        eprintln!("Warning: library not found: {}", name);
        return name.to_string();
    }

    if name.contains('/') || name.contains('\\') {
        if Path::new(name).exists() {
            return name.to_string();
        }
        eprintln!("Warning: library not found: {}", name);
        return name.to_string();
    }

    let mut search_dirs: Vec<PathBuf> = lib_dirs.iter().map(|d| Path::new(d).to_path_buf()).collect();
    search_dirs.extend(get_system_lib_paths());

    for dir in &search_dirs {
        let candidate = dir.join(name);
        if candidate.exists() {
            return candidate.to_string_lossy().to_string();
        }
        let candidate = dir.join(format!("lib{}", name));
        if candidate.exists() {
            return candidate.to_string_lossy().to_string();
        }
    }

    eprintln!("Warning: library '{}' not found in search paths, passing to linker", name);
    name.to_string()
}

fn get_system_lib_paths() -> Vec<PathBuf> {
    let mut paths = Vec::new();

    for dir in &["/usr/lib", "/usr/local/lib", "/lib", "/lib64"] {
        let path = Path::new(dir);
        if path.is_dir() {
            paths.push(path.to_path_buf());
        }
    }

    if let Ok(library_path) = std::env::var("LIBRARY_PATH") {
        for dir in library_path.split(':') {
            if !dir.is_empty() {
                paths.push(Path::new(dir).to_path_buf());
            }
        }
    }

    if let Ok(ld_library_path) = std::env::var("LD_LIBRARY_PATH") {
        for dir in ld_library_path.split(':') {
            if !dir.is_empty() {
                paths.push(Path::new(dir).to_path_buf());
            }
        }
    }

    paths
}
