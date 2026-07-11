use std::path::{Path, PathBuf};
use std::process::Command;

use crate::manifest::{Build, Manifest};

pub fn run(file: Option<&str>, force: bool, release: bool, target: Option<&str>) {
    if let Some(f) = file {
        let path = Path::new(f);
        if path.exists() {
            let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
            if ext == "hk" {
                eprintln!("Error: {} is a Hokkaido file. Use 'otaru build {}' without the C build system.", f, f);
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

    let manifest = Manifest::load(manifest_path)
        .unwrap_or_else(|e| {
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
        build_target(target_name, target_config, force, release);
    } else if let Some(targets) = &build_config.targets {
        for (name, config) in targets {
            build_target(name, config, force, release);
        }
    } else {
        let name = manifest.package.name.clone();
        build_target(&name, build_config, force, release);
    }
}

fn build_target(name: &str, config: &Build, force: bool, release: bool) {
    std::fs::create_dir_all("build").unwrap_or_else(|e| {
        eprintln!("Error creating build/ directory: {}", e);
        std::process::exit(1);
    });

    let sources = resolve_sources(&config.sources);
    if sources.is_empty() {
        eprintln!("Error: no source files found for target '{}'", name);
        std::process::exit(1);
    }

    let mut objects: Vec<String> = Vec::new();
    for source in &sources {
        let obj = compile_source(source, config, force, release);
        objects.push(obj);
    }

    let output = match config.kind.as_str() {
        "executable" => {
            let output = format!("build/{}", name);
            link_executable(&objects, &output, config, release);
            output
        }
        "staticlib" => {
            let output = format!("build/lib{}.a", name);
            link_staticlib(&objects, &output);
            output
        }
        "sharedlib" => {
            let output = format!("build/lib{}.so", name);
            link_sharedlib(&objects, &output, config, release);
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
                if let (Ok(obj_time), Ok(src_time)) = (obj_meta.modified(), src_meta.modified())
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
