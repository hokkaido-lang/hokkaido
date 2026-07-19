use std::fs;
use std::path::Path;

pub fn run(web: bool, wasm: bool) {
    let cwd = std::env::current_dir().unwrap_or_else(|e| {
        eprintln!("Error getting current directory: {}", e);
        std::process::exit(1);
    });
    let name = cwd
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or("myapp");

    if Path::new("otaru.toml").exists() {
        eprintln!("Error: otaru.toml already exists in the current directory");
        std::process::exit(1);
    }

    fs::create_dir_all("src").unwrap_or_else(|e| {
        eprintln!("Error creating src/: {}", e);
        std::process::exit(1);
    });

    crate::template::create_project_files(Path::new("."), name, web, wasm);

    println!("Initialized project '{}'", name);
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
