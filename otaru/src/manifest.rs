use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

#[derive(Debug, Serialize, Deserialize)]
pub struct Manifest {
    pub package: Package,
    #[serde(default, skip_serializing_if = "BTreeMap::is_empty")]
    pub dependencies: BTreeMap<String, Dependency>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub build: Option<Build>,
    #[serde(default, skip_serializing_if = "BTreeMap::is_empty")]
    pub scripts: BTreeMap<String, String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Package {
    pub name: String,
    pub version: String,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
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

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub sources: Vec<String>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub include_dirs: Vec<String>,

    #[serde(default = "default_compiler", skip_serializing_if = "is_default_compiler")]
    pub compiler: String,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub cflags: Vec<String>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub ldflags: Vec<String>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub link: Vec<String>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub libraries: Vec<String>,

    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub lib_dirs: Vec<String>,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub targets: Option<BTreeMap<String, Build>>,

    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub prebuild: Option<String>,

    #[serde(default, skip_serializing_if = "Option::is_none", rename = "llvm-config")]
    pub llvm_config: Option<String>,

    #[serde(default, skip_serializing_if = "Option::is_none", rename = "llvm-components")]
    pub llvm_components: Option<Vec<String>>,
}

fn is_default_compiler(s: &str) -> bool {
    s == "cc"
}

impl Default for Build {
    fn default() -> Self {
        Self {
            kind: default_build_type(),
            sources: vec![],
            include_dirs: vec![],
            compiler: default_compiler(),
            cflags: vec![],
            ldflags: vec![],
            link: vec![],
            libraries: vec![],
            lib_dirs: vec![],
            targets: None,
            prebuild: None,
            llvm_config: None,
            llvm_components: None,
        }
    }
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
