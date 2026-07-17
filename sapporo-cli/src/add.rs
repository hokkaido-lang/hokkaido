use std::process::Command;

use crate::manifest::{Dependency, Manifest, MANIFEST_FILE};

pub fn run(packages: &[String], npm: bool) {
    let mut manifest = Manifest::load(std::path::Path::new(MANIFEST_FILE)).unwrap_or_else(|e| {
        eprintln!("{}", e);
        std::process::exit(1);
    });

    for pkg in packages {
        if manifest.dependencies.contains_key(pkg) {
            println!("  '{}' already in dependencies", pkg);
            continue;
        }

        if npm {
            // Add as npm dependency and run npm install
            println!("  Installing '{}' via npm...", pkg);
            let status = Command::new("npm")
                .arg("install")
                .arg(pkg)
                .status();
            match status {
                Ok(s) if s.success() => {
                    manifest.dependencies.insert(
                        pkg.clone(),
                        Dependency::Simple("npm".to_string()),
                    );
                    println!("  Added '{}'", pkg);
                }
                _ => {
                    eprintln!("  Failed to install '{}' via npm", pkg);
                }
            }
        } else {
            // Add as otaru/Hokkaido dependency
            println!("  Adding '{}'...", pkg);
            manifest
                .dependencies
                .insert(pkg.clone(), Dependency::Simple("latest".to_string()));
            println!("  Added '{}'", pkg);
        }
    }

    manifest
        .save(std::path::Path::new(MANIFEST_FILE))
        .unwrap_or_else(|e| {
            eprintln!("{}", e);
            std::process::exit(1);
        });

    // Install dependencies via otaru if available
    if !npm && !manifest.dependencies.is_empty() {
        if let Ok(_) = Command::new("otaru").arg("--version").output() {
            println!();
            println!("Installing dependencies via otaru...");
            let status = Command::new("otaru").arg("install").status();
            if let Ok(s) = status {
                if s.success() {
                    println!("Dependencies installed.");
                } else {
                    eprintln!("Failed to install some dependencies.");
                }
            }
        }
    }
}
