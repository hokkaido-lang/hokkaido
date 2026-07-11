use std::path::Path;
use std::process::Command;

pub fn run(file: Option<&str>, freestanding: bool, force: bool, release: bool) {
    if freestanding {
        eprintln!("Error: 'otaru run' does not support --freestanding mode.");
        eprintln!("Build with 'otaru build --freestanding <file>' and link manually.");
        std::process::exit(1);
    }

    if crate::build::has_hk_files() {
        let binary = if let Some(f) = file {
            crate::build::compile_single_or_project(Some(f), false, force, release)
        } else {
            crate::build::compile_single_or_project(None, false, force, release)
        };
        run_binary(&binary);
        return;
    }

    if crate::cbuild::is_c_project() {
        crate::cbuild::run(None, force, release, None);

        let manifest_path = Path::new("otaru.toml");
        let manifest = crate::manifest::Manifest::load(manifest_path)
            .unwrap_or_else(|e| {
                eprintln!("{}", e);
                std::process::exit(1);
            });

        if let Some(build_config) = &manifest.build {
            if let Some(targets) = &build_config.targets {
                if let Some((name, _config)) =
                    targets.iter().find(|(_, c)| c.kind == "executable")
                {
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

    let binary = if let Some(f) = file {
        crate::build::compile_single_or_project(Some(f), false, force, release)
    } else {
        crate::build::compile_single_or_project(None, false, force, release)
    };
    run_binary(&binary);
}

fn run_binary(binary: &str) {
    let run_status = Command::new(binary)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running '{}': {}", binary, e);
            std::process::exit(1);
        });

    std::process::exit(run_status.code().unwrap_or(1));
}
