use std::path::Path;

pub fn run(name: &str, git: Option<&str>, path: Option<&str>) {
    let manifest_path = Path::new("otaru.toml");
    if !manifest_path.exists() {
        eprintln!("Error: otaru.toml not found");
        std::process::exit(1);
    }

    let mut manifest = crate::manifest::Manifest::load(manifest_path)
        .unwrap_or_else(|e| { eprintln!("{}", e); std::process::exit(1); });

    let dep = match (git, path) {
        (Some(url), None) => crate::manifest::Dependency::Detailed {
            git: Some(url.to_string()),
            path: None,
            tag: None,
            branch: None,
            version: Some("0.1.0".to_string()),
        },
        (None, Some(p)) => crate::manifest::Dependency::Detailed {
            git: None,
            path: Some(p.to_string()),
            tag: None,
            branch: None,
            version: None,
        },
        (None, None) => {
            // Try a registry lookup or just record it
            crate::manifest::Dependency::Simple("0.1.0".to_string())
        }
        _ => {
            eprintln!("Error: specify --git or --path, not both");
            std::process::exit(1);
        }
    };

    manifest.dependencies.insert(name.to_string(), dep);
    manifest.save(manifest_path)
        .unwrap_or_else(|e| { eprintln!("{}", e); std::process::exit(1); });

    println!("Added dependency '{}'", name);
}
