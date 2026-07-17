use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

pub const MANIFEST_FILE: &str = "sapporo.toml";

#[derive(Debug, Serialize, Deserialize)]
pub struct Manifest {
    pub package: Package,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub build: Option<BuildConfig>,

    #[serde(default, skip_serializing_if = "BTreeMap::is_empty")]
    pub dependencies: BTreeMap<String, Dependency>,

    #[serde(default, skip_serializing_if = "BTreeMap::is_empty")]
    pub scripts: BTreeMap<String, String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Package {
    pub name: String,
    pub version: String,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub authors: Vec<String>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct BuildConfig {
    /// Source directory (default: "src")
    #[serde(default = "default_sources")]
    pub sources: Vec<String>,

    /// Output directory (default: "dist")
    #[serde(default = "default_dist")]
    pub dist: String,

    /// Extra wasm-ld flags
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub ldflags: Vec<String>,

    /// Extra hokkaido compiler flags
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub cflags: Vec<String>,
}

fn default_sources() -> Vec<String> {
    vec!["src".to_string()]
}

fn default_dist() -> String {
    "dist".to_string()
}

impl Default for BuildConfig {
    fn default() -> Self {
        Self {
            sources: default_sources(),
            dist: default_dist(),
            ldflags: vec![],
            cflags: vec![],
        }
    }
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
        version: Option<String>,
    },
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
