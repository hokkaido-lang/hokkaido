use std::fs;
use std::path::Path;

pub fn run(name: &str, web: bool, wasm: bool) {
    let project_dir = Path::new(name);
    if project_dir.exists() {
        eprintln!("Error: directory '{}' already exists", name);
        std::process::exit(1);
    }

    fs::create_dir_all(project_dir.join("src")).unwrap_or_else(|e| {
        eprintln!("Error creating project: {}", e);
        std::process::exit(1);
    });

    crate::template::create_project_files(project_dir, name, web, wasm);

    println!("Created project '{}'", name);
    println!("  cd {}", name);
    if web {
        println!("  otaru build            # compile to dist/{name}.wasm");
        println!("  otaru run              # build + dev server + browser");
    } else if wasm {
        println!("  otaru build            # compile to wasm32/<name>.wasm");
        println!("  otaru run              # run in browser (wasmtime or http server)");
    } else {
        println!("  otaru build");
    }
}
