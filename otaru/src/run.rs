use std::process::Command;

pub fn run() {
    let binary = crate::build::compile_and_link();

    let run_status = Command::new(&binary)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running '{}': {}", binary, e);
            std::process::exit(1);
        });

    std::process::exit(run_status.code().unwrap_or(1));
}
