use std::path::Path;

use crate::scaffold;

pub fn run(verbose: bool) {
    if Path::new("sapporo.toml").exists() {
        eprintln!("Error: sapporo.toml already exists in this directory.");
        eprintln!("Use 'sapporo build' to build the project.");
        std::process::exit(1);
    }

    // Get project name from directory name
    let dir_name = std::env::current_dir()
        .ok()
        .and_then(|d| d.file_name().map(|n| n.to_string_lossy().to_string()))
        .unwrap_or_else(|| "my-app".to_string());

    scaffold::init_project(&dir_name, verbose);

    println!("Initialized sapporo project '{}'", dir_name);
    println!();
    println!("  sapporo build       # compile to dist/{}.wasm", dir_name);
    println!("  sapporo run         # start dev server + open browser");
}
