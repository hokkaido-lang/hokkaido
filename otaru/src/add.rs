use std::path::Path;
use std::process::Command;

use crate::utils::load_manifest;

pub fn run(packages: &[String], git: Option<&str>, path: Option<&str>, npm: bool) {
    let manifest_path = Path::new("otaru.toml");
    if !manifest_path.exists() {
        eprintln!("Error: otaru.toml not found");
        std::process::exit(1);
    }

    let mut manifest = load_manifest();

    for name in packages {
        if manifest.dependencies.contains_key(name) {
            println!("'{}' already in dependencies", name);
            continue;
        }

        let dep = if npm {
            // Install via npm and record in manifest
            println!("Installing '{}' via npm...", name);
            let status = Command::new("npm").arg("install").arg(name).status();
            match status {
                Ok(s) if s.success() => {
                    crate::manifest::Dependency::Simple("npm".to_string())
                }
                _ => {
                    eprintln!("Failed to install '{}' via npm", name);
                    continue;
                }
            }
        } else if let Some(url) = git {
            crate::manifest::Dependency::Detailed {
                git: Some(url.to_string()),
                path: None,
                tag: None,
                branch: None,
                version: Some("0.1.0".to_string()),
            }
        } else if let Some(p) = path {
            crate::manifest::Dependency::Detailed {
                git: None,
                path: Some(p.to_string()),
                tag: None,
                branch: None,
                version: None,
            }
        } else {
            crate::manifest::Dependency::Simple("0.1.0".to_string())
        };

        manifest.dependencies.insert(name.to_string(), dep);
        println!("Added dependency '{}'", name);
    }

    manifest
        .save(manifest_path)
        .unwrap_or_else(|e| {
            eprintln!("{}", e);
            std::process::exit(1);
        });
}
