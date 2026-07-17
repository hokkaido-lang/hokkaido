use std::path::Path;
use std::process::Command;

use crate::manifest::{Manifest, MANIFEST_FILE};

pub fn run(force: bool) {
    // Build first
    crate::build::run(force);

    let manifest = Manifest::load(Path::new(MANIFEST_FILE)).unwrap_or_else(|e| {
        eprintln!("{}", e);
        std::process::exit(1);
    });

    let build = manifest.build.unwrap_or_default();
    let dist = Path::new(&build.dist);
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
            eprintln!("  cd {} && python3 -m http.server {}", dist.display(), port);
            std::process::exit(1);
        });

    std::process::exit(status.code().unwrap_or(1));
}
