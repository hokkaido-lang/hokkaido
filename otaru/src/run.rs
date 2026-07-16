use std::path::Path;
use std::process::Command;

pub fn run(file: Option<&str>, freestanding: bool, force: bool, release: bool) {
    if freestanding {
        eprintln!("Error: 'otaru run' does not support --freestanding mode.");
        eprintln!("Build with 'otaru build --freestanding <file>' and link manually.");
        std::process::exit(1);
    }

    if crate::cbuild::has_build_targets() || crate::cbuild::is_c_project() {
        crate::cbuild::run(None, force, release, None);

        let manifest_path = Path::new("otaru.toml");
        let manifest = crate::manifest::Manifest::load(manifest_path).unwrap_or_else(|e| {
            eprintln!("{}", e);
            std::process::exit(1);
        });

        if let Some(build_config) = &manifest.build {
            if build_config.kind == "wasm" {
                let wasm_path = format!("build/{}.wasm", manifest.package.name);
                run_wasm(&wasm_path);
                return;
            }

            if let Some(targets) = &build_config.targets {
                if let Some((name, config)) =
                    targets.iter().find(|(_, c)| c.kind == "executable")
                {
                    if config.kind == "wasm" {
                        let wasm_path = format!("build/{}.wasm", name);
                        run_wasm(&wasm_path);
                        return;
                    }
                    let binary = format!("build/{}", name);
                    run_binary(&binary);
                    return;
                }
                eprintln!("Error: no executable target found");
                std::process::exit(1);
            }
            let binary = format!("build/{}", manifest.package.name);
            run_binary(&binary);
            return;
        }
    }

    if crate::build::has_hk_files() {
        let binary = if let Some(f) = file {
            crate::build::compile_single_or_project(Some(f), false, force, release, None)
        } else {
            crate::build::compile_single_or_project(None, false, force, release, None)
        };
        run_binary(&binary);
        return;
    }

    let binary = if let Some(f) = file {
        crate::build::compile_single_or_project(Some(f), false, force, release, None)
    } else {
        crate::build::compile_single_or_project(None, false, force, release, None)
    };
    run_binary(&binary);
}

fn run_wasm(wasm_path: &str) {
    if !std::path::Path::new(wasm_path).exists() {
        eprintln!("Error: {} not found. Run 'otaru build' first.", wasm_path);
        std::process::exit(1);
    }

    // Try wasmtime — only bail out if the binary doesn't exist at all.
    // If wasmtime runs but the program returns non-zero, that's the program's exit code.
    if let Ok(output) = Command::new("wasmtime").arg(wasm_path).output() {
        print!("{}", String::from_utf8_lossy(&output.stdout));
        eprint!("{}", String::from_utf8_lossy(&output.stderr));
        std::process::exit(output.status.code().unwrap_or(1));
    }

    // Try wasm3
    if let Ok(output) = Command::new("wasm3")
        .arg("run")
        .arg(wasm_path)
        .output()
    {
        print!("{}", String::from_utf8_lossy(&output.stdout));
        eprint!("{}", String::from_utf8_lossy(&output.stderr));
        std::process::exit(output.status.code().unwrap_or(1));
    }

    // No runtime found — try to serve in browser
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
