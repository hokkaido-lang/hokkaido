use std::path::{Path, PathBuf};
use std::process::Command;

use crate::manifest::{Build, Manifest};

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
            eprintln!(
                "Warning: {} --cxxflags failed",
                llvm_bin
            );
        }
    } else {
        eprintln!(
            "Warning: could not run '{}' (is LLVM installed?)",
            llvm_bin
        );
        return None;
    }

    let mut link_args = vec!["--ldflags".to_string(), "--libs".to_string()];
    if let Some(components) = &config.llvm_components {
        link_args.extend(components.iter().cloned());
    }

    if let Ok(output) = Command::new(llvm_bin).args(&link_args).output() {
        if output.status.success() {
            let flags = String::from_utf8_lossy(&output.stdout);
            for flag in flags.split_whitespace() {
                if let Some(path) = flag.strip_prefix("-L") {
                    ldflags.push(format!("-L{}", path));
                } else if let Some(name) = flag.strip_prefix("-l") {
                    libraries.push(name.to_string());
                } else {
                    ldflags.push(flag.to_string());
                }
            }
        } else {
            eprintln!(
                "Warning: {} --ldflags --libs failed",
                llvm_bin
            );
        }
    }

    if let Ok(output) = Command::new(llvm_bin)
        .arg("--system-libs")
        .args(&link_args)
        .output()
    {
        if output.status.success() {
            let flags = String::from_utf8_lossy(&output.stdout);
            for flag in flags.split_whitespace() {
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
    }

    Some(LlvmInfo {
        include_dirs,
        cflags,
        ldflags,
        libraries,
    })
}

pub fn run_prebuild(config: &Build) {
    if let Some(cmd) = &config.prebuild {
        eprintln!("$ {}", cmd);
        let status = Command::new("sh")
            .arg("-c")
            .arg(cmd)
            .status()
            .unwrap_or_else(|e| {
                eprintln!("Error running prebuild command: {}", e);
                std::process::exit(1);
            });
        if !status.success() {
            eprintln!(
                "Prebuild command failed (exit code: {})",
                status.code().unwrap_or(-1)
            );
            std::process::exit(1);
        }
    }
}

pub fn run(file: Option<&str>, force: bool, release: bool, target: Option<&str>) {
    if let Some(f) = file {
        let path = Path::new(f);
        if path.exists() {
            let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
            if ext == "hk" {
                eprintln!(
                    "Error: {} is a Hokkaido file. Use 'otaru build {}' without the C build system.",
                    f, f
                );
                std::process::exit(1);
            }
            compile_single_file(f, force, release);
            return;
        }
        eprintln!("Error: file '{}' not found", f);
        std::process::exit(1);
    }

    let manifest_path = Path::new("otaru.toml");
    if !manifest_path.exists() {
        eprintln!("Error: otaru.toml not found");
        std::process::exit(1);
    }

    let manifest = Manifest::load(manifest_path).unwrap_or_else(|e| {
        eprintln!("{}", e);
        std::process::exit(1);
    });

    let build_config = manifest.build.as_ref().unwrap_or_else(|| {
        eprintln!("Error: no [build] section in otaru.toml");
        std::process::exit(1);
    });

    if let Some(target_name) = target {
        let targets = build_config.targets.as_ref().unwrap_or_else(|| {
            eprintln!(
                "Error: target '{}' not found (no [[build.targets]] section)",
                target_name
            );
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

fn merge_target_config(parent: &Build, target: &Build) -> Build {
    let mut merged = target.clone();
    if merged.llvm_config.is_none() {
        merged.llvm_config = parent.llvm_config.clone();
    }
    if merged.llvm_components.is_none() {
        merged.llvm_components = parent.llvm_components.clone();
    }
    if merged.prebuild.is_none() {
        merged.prebuild = parent.prebuild.clone();
    }
    if merged.compiler == "cc" && parent.compiler != "cc" {
        merged.compiler = parent.compiler.clone();
    }
    for dir in &parent.include_dirs {
        if !merged.include_dirs.contains(dir) {
            merged.include_dirs.push(dir.clone());
        }
    }
    for flag in &parent.cflags {
        if !merged.cflags.contains(flag) {
            merged.cflags.push(flag.clone());
        }
    }
    for flag in &parent.ldflags {
        if !merged.ldflags.contains(flag) {
            merged.ldflags.push(flag.clone());
        }
    }
    for lib in &parent.link {
        if !merged.link.contains(lib) {
            merged.link.push(lib.clone());
        }
    }
    for lib in &parent.libraries {
        if !merged.libraries.contains(lib) {
            merged.libraries.push(lib.clone());
        }
    }
    for dir in &parent.lib_dirs {
        if !merged.lib_dirs.contains(dir) {
            merged.lib_dirs.push(dir.clone());
        }
    }
    merged
}

fn build_target(name: &str, config: &Build, force: bool, release: bool) {
    std::fs::create_dir_all("build").unwrap_or_else(|e| {
        eprintln!("Error creating build/ directory: {}", e);
        std::process::exit(1);
    });

    run_prebuild(config);

    let llvm = resolve_llvm(config);

    let mut extra_cflags = Vec::new();
    let mut extra_include_dirs = Vec::new();
    if let Some(ref info) = llvm {
        extra_include_dirs.extend(info.include_dirs.iter().cloned());
        extra_cflags.extend(info.cflags.iter().cloned());
    }

    let mut effective_config = config.clone();
    for dir in &extra_include_dirs {
        if !effective_config.include_dirs.contains(dir) {
            effective_config.include_dirs.push(dir.clone());
        }
    }
    for flag in &extra_cflags {
        if !effective_config.cflags.contains(flag) {
            effective_config.cflags.push(flag.clone());
        }
    }

    let sources = resolve_sources(&config.sources);
    if sources.is_empty() {
        eprintln!("Error: no source files found for target '{}'", name);
        std::process::exit(1);
    }

    let mut objects: Vec<String> = Vec::new();
    for source in &sources {
        let obj = compile_source(source, &effective_config, force, release);
        objects.push(obj);
    }

    let mut effective_ldflags = config.ldflags.clone();
    let mut effective_link = config.link.clone();
    let effective_libraries = config.libraries.clone();
    let effective_lib_dirs = config.lib_dirs.clone();

    if let Some(ref info) = llvm {
        for flag in &info.ldflags {
            if !effective_ldflags.contains(flag) {
                effective_ldflags.push(flag.clone());
            }
        }
        for lib in &info.libraries {
            if !effective_link.contains(lib) {
                effective_link.push(lib.clone());
            }
        }
    }

    let mut link_config = effective_config.clone();
    link_config.ldflags = effective_ldflags;
    link_config.link = effective_link;
    link_config.libraries = effective_libraries;
    link_config.lib_dirs = effective_lib_dirs;

    let output = match config.kind.as_str() {
        "executable" => {
            let output = format!("build/{}", name);
            link_executable(&objects, &output, &link_config, release);
            output
        }
        "staticlib" => {
            let output = format!("build/lib{}.a", name);
            link_staticlib(&objects, &output);
            output
        }
        "sharedlib" => {
            let output = format!("build/lib{}.so", name);
            link_sharedlib(&objects, &output, &link_config, release);
            output
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

fn compile_single_file(file: &str, force: bool, release: bool) {
    let path = Path::new(file);
    let stem = path.file_stem().unwrap_or_default().to_string_lossy();

    std::fs::create_dir_all("build").unwrap_or_else(|e| {
        eprintln!("Error creating build/ directory: {}", e);
        std::process::exit(1);
    });

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
                if let (Ok(obj_time), Ok(src_time)) =
                    (obj_meta.modified(), src_meta.modified())
                {
                    if obj_time >= src_time {
                        if Path::new(&dep).exists() {
                            if deps_are_up_to_date(&dep, obj_time) {
                                return obj;
                            }
                        } else {
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

fn link_executable(objects: &[String], output: &str, config: &Build, release: bool) {
    let mut cmd = Command::new(&config.compiler);

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

fn link_sharedlib(objects: &[String], output: &str, config: &Build, release: bool) {
    let mut cmd = Command::new(&config.compiler);
    cmd.arg("-shared");

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
        eprintln!("Error creating shared library {}: {}", output, e);
        std::process::exit(1);
    });

    if !status.success() {
        eprintln!("Failed to create shared library: {}", output);
        std::process::exit(1);
    }
}

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

    let mut search_dirs: Vec<PathBuf> = Vec::new();

    for dir in lib_dirs {
        search_dirs.push(Path::new(dir).to_path_buf());
    }

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

    eprintln!(
        "Warning: library '{}' not found in search paths, passing to linker",
        name
    );
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

pub fn is_c_project() -> bool {
    let manifest_path = Path::new("otaru.toml");
    if manifest_path.exists() {
        if let Ok(manifest) = Manifest::load(manifest_path) {
            return manifest.build.is_some();
        }
    }
    false
}

pub fn has_build_targets() -> bool {
    let manifest_path = Path::new("otaru.toml");
    if manifest_path.exists() {
        if let Ok(manifest) = Manifest::load(manifest_path) {
            if let Some(build) = &manifest.build {
                return build.targets.is_some();
            }
        }
    }
    false
}
