use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

#[derive(Debug, Serialize, Deserialize)]
pub struct Manifest {
    pub package: Package,
    #[serde(default)]
    pub dependencies: BTreeMap<String, Dependency>,
    #[serde(default)]
    pub build: Option<Build>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Package {
    pub name: String,
    pub version: String,
    #[serde(default)]
    pub authors: Vec<String>,
    #[serde(default = "default_edition")]
    pub edition: String,
}

fn default_edition() -> String {
    "2024".to_string()
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(untagged)]
pub enum Dependency {
    Simple(String),
    Detailed {
        #[serde(skip_serializing_if = "Option::is_none")]
        git: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        path: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        tag: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        branch: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        version: Option<String>,
    },
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Build {
    #[serde(default = "default_build_type", rename = "type")]
    pub kind: String,

    #[serde(default)]
    pub sources: Vec<String>,

    #[serde(default)]
    pub include_dirs: Vec<String>,

    #[serde(default = "default_compiler")]
    pub compiler: String,

    #[serde(default)]
    pub cflags: Vec<String>,

    #[serde(default)]
    pub ldflags: Vec<String>,

    #[serde(default)]
    pub link: Vec<String>,

    #[serde(default)]
    pub libraries: Vec<String>,

    #[serde(default)]
    pub lib_dirs: Vec<String>,

    #[serde(default)]
    pub targets: Option<BTreeMap<String, Build>>,
}

fn default_build_type() -> String {
    "executable".to_string()
}

fn default_compiler() -> String {
    "cc".to_string()
}

impl Manifest {
    pub fn load(path: &Path) -> Result<Self, String> {
        let content = fs::read_to_string(path)
            .map_err(|e| format!("Failed to read {}: {}", path.display(), e))?;
        toml::from_str(&content)
            .map_err(|e| format!("Failed to parse {}: {}", path.display(), e))
    }

    pub fn save(&self, path: &Path) -> Result<(), String> {
        let content = toml::to_string_pretty(self)
            .map_err(|e| format!("Failed to serialize manifest: {}", e))?;
        fs::write(path, content)
            .map_err(|e| format!("Failed to write {}: {}", path.display(), e))
    }
}
