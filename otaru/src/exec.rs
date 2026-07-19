use std::process::Command;

use crate::utils::load_manifest;

pub fn list_scripts() {
    let manifest = load_manifest();
    if manifest.scripts.is_empty() {
        println!("No scripts defined in otaru.toml");
        println!();
        println!("Add scripts with:");
        println!("  [scripts]");
        println!("  test = \"./build/myapp --test\"");
        return;
    }
    println!("Available scripts:");
    for (name, cmd) in &manifest.scripts {
        println!("  {} = \"{}\"", name, cmd);
    }
    println!();
    println!("Run with: otaru exec <name>");
}

pub fn exec_script(name: &str, args: &[String]) {
    let manifest = load_manifest();

    let script = manifest.scripts.get(name).unwrap_or_else(|| {
        eprintln!("Error: script '{}' not found in otaru.toml", name);
        if !manifest.scripts.is_empty() {
            eprintln!();
            eprintln!("Available scripts:");
            for n in manifest.scripts.keys() {
                eprintln!("  {}", n);
            }
        }
        std::process::exit(1);
    });

    let mut cmd_str = script.clone();
    for (i, arg) in args.iter().enumerate() {
        let placeholder = format!("${}", i + 1);
        cmd_str = cmd_str.replace(&placeholder, arg);
    }

    eprintln!("$ {}", cmd_str);

    let status = Command::new("sh")
        .arg("-c")
        .arg(&cmd_str)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running script '{}': {}", name, e);
            std::process::exit(1);
        });

    std::process::exit(status.code().unwrap_or(1));
}

pub fn clean() {
    let _ = std::fs::remove_dir_all("build");
    println!("Cleaned build directory");
}
