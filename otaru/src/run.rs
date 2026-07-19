use std::path::Path;
use std::process::Command;

use crate::utils::{has_build_targets, is_c_project, load_manifest};

pub fn run(file: Option<&str>, freestanding: bool, force: bool, release: bool) {
    if freestanding {
        eprintln!("Error: 'otaru run' does not support --freestanding mode.");
        eprintln!("Build with 'otaru build --freestanding <file>' and link manually.");
        std::process::exit(1);
    }

    if has_build_targets() || is_c_project() {
        let manifest = load_manifest();
        let is_web = manifest
            .build
            .as_ref()
            .map_or(false, |b| b.is_web());

        crate::cbuild::run(None, force, release, None);

        if is_web {
            run_web_server(&manifest);
        } else {
            run_project_binary();
        }
        return;
    }

    let binary = crate::build::compile_single_or_project(file, false, force, release, None);
    run_binary(&binary);
}

fn run_project_binary() {
    let manifest = load_manifest();

    if let Some(build_config) = &manifest.build {
        if build_config.kind == "wasm" {
            let wasm_path = format!("build/{}.wasm", manifest.package.name);
            run_wasm(&wasm_path);
            return;
        }

        if let Some(targets) = &build_config.targets {
            if let Some((name, config)) = targets.iter().find(|(_, c)| c.kind == "executable") {
                if config.kind == "wasm" {
                    run_wasm(&format!("build/{}.wasm", name));
                    return;
                }
                run_binary(&format!("build/{}", name));
                return;
            }
            eprintln!("Error: no executable target found");
            std::process::exit(1);
        }

        run_binary(&format!("build/{}", manifest.package.name));
    }
}

fn run_web_server(manifest: &crate::manifest::Manifest) {
    let build = manifest.build.as_ref().unwrap();
    let dist = build.dist_dir();
    let port = 8080;

    println!();
    println!("Starting dev server on http://localhost:{}", port);
    println!("Press Ctrl+C to stop.");
    println!();

    // Try to open browser
    let browser_cmd = if cfg!(target_os = "macos") {
        "open"
    } else if cfg!(target_os = "linux") {
        "xdg-open"
    } else {
        "start"
    };
    let url = format!("http://localhost:{}", port);
    let _ = Command::new(browser_cmd).arg(&url).spawn();

    // Start http server
    let status = Command::new("python3")
        .arg("-m")
        .arg("http.server")
        .arg(port.to_string())
        .arg("--directory")
        .arg(dist)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error starting server: {}", e);
            eprintln!();
            eprintln!("Manual start:");
            eprintln!("  cd {} && python3 -m http.server {}", dist, port);
            std::process::exit(1);
        });

    std::process::exit(status.code().unwrap_or(1));
}

fn run_wasm(wasm_path: &str) {
    if !Path::new(wasm_path).exists() {
        eprintln!("Error: {} not found. Run 'otaru build' first.", wasm_path);
        std::process::exit(1);
    }

    // Try wasmtime
    if let Ok(output) = Command::new("wasmtime").arg(wasm_path).output() {
        print!("{}", String::from_utf8_lossy(&output.stdout));
        eprint!("{}", String::from_utf8_lossy(&output.stderr));
        std::process::exit(output.status.code().unwrap_or(1));
    }

    // Try wasm3
    if let Ok(output) = Command::new("wasm3").arg("run").arg(wasm_path).output() {
        print!("{}", String::from_utf8_lossy(&output.stdout));
        eprint!("{}", String::from_utf8_lossy(&output.stderr));
        std::process::exit(output.status.code().unwrap_or(1));
    }

    // No runtime found
    eprintln!("No wasm runtime found (wasmtime, wasm3).");
    eprintln!();
    eprintln!("To run in browser:");
    eprintln!("  cd wasm32 && python3 -m http.server 8080");
    eprintln!("  open http://localhost:8080");
    eprintln!();
    eprintln!("To run from terminal, install wasmtime:");
    eprintln!("  curl https://wasmtime.dev/install.sh -sSf | bash");
    std::process::exit(1);
}

fn run_binary(binary: &str) {
    let run_status = Command::new(binary).status().unwrap_or_else(|e| {
        eprintln!("Error running '{}': {}", binary, e);
        std::process::exit(1);
    });
    std::process::exit(run_status.code().unwrap_or(1));
}
