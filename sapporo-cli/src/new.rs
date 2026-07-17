use std::path::Path;

use crate::scaffold;

pub fn run(name: &str, verbose: bool) {
    let project_dir = Path::new(name);
    if project_dir.exists() {
        eprintln!("Error: directory '{}' already exists", name);
        std::process::exit(1);
    }

    scaffold::create_project(project_dir, name, verbose);

    println!("Created project '{}'", name);
    println!();
    println!("  cd {}", name);
    println!("  sapporo build       # compile to dist/{}.wasm", name);
    println!("  sapporo run         # start dev server + open browser");
}
