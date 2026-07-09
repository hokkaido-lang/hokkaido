use std::process::Command;

pub fn run(file: Option<&str>, freestanding: bool, force: bool, release: bool) {
    if freestanding {
        eprintln!("Error: 'otaru run' does not support --freestanding mode.");
        eprintln!("Build with 'otaru build --freestanding <file>' and link manually.");
        std::process::exit(1);
    }

    let binary = if let Some(f) = file {
        crate::build::compile_single_or_project(Some(f), false, force, release)
    } else {
        crate::build::compile_single_or_project(None, false, force, release)
    };

    let run_status = Command::new(&binary)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running '{}': {}", binary, e);
            std::process::exit(1);
        });

    std::process::exit(run_status.code().unwrap_or(1));
}
