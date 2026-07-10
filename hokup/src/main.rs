use std::io::Write;
use std::path::{Path, PathBuf};

use clap::{Parser, Subcommand};
use flate2::read::GzDecoder;
use serde::Deserialize;
use tar::Archive;

const REPO: &str = "jihoo/hokkaido";
const DEFAULT_INSTALL_DIR: &str = ".hokkaido";

#[derive(Parser)]
#[command(
    name = "hokup",
    version,
    about = "Hokkaido toolchain installer — like rustup, but for hokkaido"
)]
struct Cli {
    #[command(subcommand)]
    command: Option<Command>,
}

#[derive(Subcommand)]
enum Command {
    /// Install hokkaido toolchain (default command)
    Install {
        /// Installation directory (default: ~/.hokkaido)
        #[arg(short, long)]
        path: Option<PathBuf>,
    },
    /// Update hokkaido toolchain to the latest version
    Update,
    /// Uninstall hokkaido toolchain
    Uninstall,
    /// Show installed version info
    Info,
}

#[derive(Deserialize)]
struct GitHubRelease {
    tag_name: String,
    assets: Vec<GitHubAsset>,
}

#[derive(Deserialize)]
struct GitHubAsset {
    name: String,
    browser_download_url: String,
}

fn detect_platform() -> Result<(String, String), String> {
    let os = std::env::consts::OS;
    let arch = std::env::consts::ARCH;

    let os_str = match os {
        "linux" => "linux",
        "macos" => "macos",
        _ => return Err(format!("Unsupported OS: {os}")),
    };

    let arch_str = match arch {
        "x86_64" => "x86_64",
        "aarch64" => "aarch64",
        _ => return Err(format!("Unsupported architecture: {arch}")),
    };

    Ok((os_str.to_string(), arch_str.to_string()))
}

fn get_asset_suffix() -> Result<String, String> {
    let (os, arch) = detect_platform()?;
    Ok(format!("{os}-{arch}"))
}

fn get_install_dir(custom_path: Option<PathBuf>) -> PathBuf {
    custom_path.unwrap_or_else(|| {
        let home = dirs::home_dir().expect("Could not determine home directory");
        home.join(DEFAULT_INSTALL_DIR)
    })
}

fn get_bin_dir(install_dir: &Path) -> PathBuf {
    install_dir.join("bin")
}

fn get_latest_release() -> Result<GitHubRelease, String> {
    let url = format!("https://api.github.com/repos/{REPO}/releases/latest");
    let resp: GitHubRelease = ureq::get(&url)
        .header("Accept", "application/vnd.github+json")
        .call()
        .map_err(|e| format!("Failed to fetch latest release: {e}"))?
        .into_body()
        .read_json()
        .map_err(|e| format!("Failed to parse release info: {e}"))?;
    Ok(resp)
}

fn find_asset<'a>(release: &'a GitHubRelease, suffix: &str) -> Result<&'a GitHubAsset, String> {
    let pattern = format!("hokkaido-{suffix}.tar.gz");
    release
        .assets
        .iter()
        .find(|a| a.name == pattern)
        .ok_or_else(|| {
            format!(
                "No asset found for platform '{suffix}' in release {}. \
                 Available assets: {}",
                release.tag_name,
                release
                    .assets
                    .iter()
                    .map(|a| a.name.as_str())
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        })
}

fn download_and_extract(url: &str, install_dir: &Path) -> Result<(), String> {
    println!("  Downloading {url} ...");

    let body: Vec<u8>;
    let response = ureq::get(url)
        .call()
        .map_err(|e| format!("Download failed: {e}"))?;

    let mut reader = response.into_body();
    body = reader
        .read_to_vec()
        .map_err(|e| format!("Failed to read download body: {e}"))?;

    println!("  Downloaded {} bytes", body.len());

    let bin_dir = get_bin_dir(install_dir);
    let share_dir = install_dir.join("share");
    std::fs::create_dir_all(&bin_dir)
        .map_err(|e| format!("Failed to create {}: {e}", bin_dir.display()))?;
    std::fs::create_dir_all(&share_dir)
        .map_err(|e| format!("Failed to create {}: {e}", share_dir.display()))?;

    println!("  Extracting to {} ...", install_dir.display());
    let decoder = GzDecoder::new(&body[..]);
    let mut archive = Archive::new(decoder);

    // The tarball has top-level bin/ and share/ directories.
    // We want to strip the prefix and place them under install_dir.
    archive
        .unpack(install_dir)
        .map_err(|e| format!("Extraction failed: {e}"))?;

    // Make binaries executable
    for entry in std::fs::read_dir(&bin_dir)
        .map_err(|e| format!("Failed to read {}: {e}", bin_dir.display()))?
    {
        let entry = entry.map_err(|e| format!("Dir entry error: {e}"))?;
        let path = entry.path();
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let mut perms = std::fs::metadata(&path)
                .map_err(|e| format!("Failed to stat {}: {e}", path.display()))?
                .permissions();
            perms.set_mode(0o755);
            std::fs::set_permissions(&path, perms)
                .map_err(|e| format!("Failed to chmod {}: {e}", path.display()))?;
        }
    }

    Ok(())
}

fn setup_path(install_dir: &Path) -> Result<(), String> {
    let bin_dir = get_bin_dir(install_dir);
    let bin_str = bin_dir.to_string_lossy().to_string();

    // Check if already in PATH
    if let Ok(path) = std::env::var("PATH") {
        for dir in path.split(':') {
            if Path::new(dir) == bin_dir {
                println!("  {} is already in PATH", bin_dir.display());
                return Ok(());
            }
        }
    }

    // Determine which shell profile to modify
    let profile = detect_shell_profile();
    if let Some(profile_path) = profile {
        let profile_str = profile_path.to_string_lossy().to_string();

        // Check if already configured
        if profile_path.exists() {
            if let Ok(contents) = std::fs::read_to_string(&profile_path) {
                if contents.contains(&bin_str) {
                    println!("  PATH already configured in {profile_str}");
                    return Ok(());
                }
            }
        }

        // Append PATH configuration
        let export_line = format!("\n# Hokkaido toolchain\nexport PATH=\"{bin_str}:$PATH\"\n");
        std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open(&profile_path)
            .map_err(|e| format!("Failed to open {profile_str}: {e}"))?
            .write_all(export_line.as_bytes())
            .map_err(|e| format!("Failed to write to {profile_str}: {e}"))?;

        println!("  Added {bin_str} to PATH in {profile_str}");
        println!("  Run `source {profile_str}` or restart your shell to apply");
    } else {
        println!(
            "  Could not detect shell profile. Please add {} to your PATH manually.",
            bin_dir.display()
        );
    }

    Ok(())
}

fn detect_shell_profile() -> Option<PathBuf> {
    let home = dirs::home_dir()?;

    // Check SHELL env var
    if let Ok(shell) = std::env::var("SHELL") {
        if shell.contains("zsh") {
            return Some(home.join(".zshrc"));
        }
        if shell.contains("bash") {
            // Prefer .bashrc over .bash_profile
            let bashrc = home.join(".bashrc");
            if bashrc.exists() {
                return Some(bashrc);
            }
            return Some(home.join(".bash_profile"));
        }
        if shell.contains("fish") {
            return Some(
                home.join(".config")
                    .join("fish")
                    .join("config.fish"),
            );
        }
    }

    // Fallback: try common profiles
    for name in &[".zshrc", ".bashrc", ".bash_profile", ".profile"] {
        let path = home.join(name);
        if path.exists() {
            return Some(path);
        }
    }

    None
}

fn print_welcome(install_dir: &Path, version: &str) {
    let bin_dir = get_bin_dir(install_dir);
    println!();
    println!("  Hokkaido {version} installed successfully!");
    println!();
    println!("  Installed to: {}", install_dir.display());
    println!("  Binaries:");
    println!("    {} - Hokkaido compiler", bin_dir.join("hokkaido").display());
    println!("    {} - Hokkaido LSP server", bin_dir.join("hok-lsp").display());
    println!("    {} - Hokkaido package manager", bin_dir.join("otaru").display());
    println!();
    println!("  To get started:");
    println!("    export HOKKAIDO_HOME={}", install_dir.display());
    println!("    otaru new my_project");
    println!("    cd my_project && otaru run");
    println!();
}

fn get_current_version(install_dir: &Path) -> Option<String> {
    let version_file = install_dir.join(".version");
    std::fs::read_to_string(version_file).ok().map(|s| s.trim().to_string())
}

fn save_version(install_dir: &Path, version: &str) -> Result<(), String> {
    let version_file = install_dir.join(".version");
    std::fs::write(&version_file, version)
        .map_err(|e| format!("Failed to write version file: {e}"))
}

fn do_install(path: Option<PathBuf>) -> Result<(), String> {
    let install_dir = get_install_dir(path);
    let suffix = get_asset_suffix()?;

    println!("  hokup — Hokkaido toolchain installer");
    println!();
    println!("  Platform: {suffix}");
    println!("  Install directory: {}", install_dir.display());
    println!();

    // Check for existing installation
    if install_dir.exists() {
        if let Some(ver) = get_current_version(&install_dir) {
            println!("  Existing installation found (v{ver}). Updating...");
        }
    }

    // Fetch latest release
    println!("  Fetching latest release info...");
    let release = get_latest_release()?;
    let version = release.tag_name.trim_start_matches('v');
    println!("  Latest version: v{version}");

    // Find the right asset
    let asset = find_asset(&release, &suffix)?;

    // Create install directory
    std::fs::create_dir_all(&install_dir)
        .map_err(|e| format!("Failed to create {}: {e}", install_dir.display()))?;

    // Download and extract
    download_and_extract(&asset.browser_download_url, &install_dir)?;

    // Save version
    save_version(&install_dir, version)?;

    // Setup PATH
    setup_path(&install_dir)?;

    // Print welcome
    print_welcome(&install_dir, version);

    Ok(())
}

fn do_update() -> Result<(), String> {
    let install_dir = get_install_dir(None);

    if !install_dir.exists() {
        println!("  No existing installation found. Run `hokup install` first.");
        return Ok(());
    }

    println!("  Checking for updates...");
    let release = get_latest_release()?;
    let new_version = release.tag_name.trim_start_matches('v');

    if let Some(current) = get_current_version(&install_dir) {
        if current == new_version {
            println!("  Already up to date (v{current}).");
            return Ok(());
        }
        println!("  Updating from v{current} to v{new_version}...");
    }

    let suffix = get_asset_suffix()?;
    let asset = find_asset(&release, &suffix)?;
    download_and_extract(&asset.browser_download_url, &install_dir)?;
    save_version(&install_dir, new_version)?;

    println!();
    println!("  Updated to v{new_version}!");
    Ok(())
}

fn do_uninstall() -> Result<(), String> {
    let install_dir = get_install_dir(None);

    if !install_dir.exists() {
        println!("  No installation found at {}", install_dir.display());
        return Ok(());
    }

    println!("  Removing {} ...", install_dir.display());
    std::fs::remove_dir_all(&install_dir)
        .map_err(|e| format!("Failed to remove {}: {e}", install_dir.display()))?;

    println!();
    println!("  Hokkaido has been uninstalled.");
    println!("  You may want to remove the PATH entry from your shell profile.");

    Ok(())
}

fn do_info() -> Result<(), String> {
    let install_dir = get_install_dir(None);

    if !install_dir.exists() {
        println!("  No installation found at {}", install_dir.display());
        return Ok(());
    }

    println!("  Hokkaido installation info:");
    println!("  Location: {}", install_dir.display());
    println!(
        "  Version: {}",
        get_current_version(&install_dir).unwrap_or_else(|| "unknown".to_string())
    );

    let bin_dir = get_bin_dir(&install_dir);
    if bin_dir.exists() {
        println!("  Binaries:");
        for entry in std::fs::read_dir(&bin_dir)
            .map_err(|e| format!("Failed to read bin dir: {e}"))?
        {
            let entry = entry.map_err(|e| format!("Dir entry error: {e}"))?;
            println!("    {}", entry.file_name().to_string_lossy());
        }
    }

    Ok(())
}

fn main() {
    let cli = Cli::parse();

    let result = match cli.command {
        Some(Command::Install { path }) => do_install(path),
        None => do_install(None),
        Some(Command::Update) => do_update(),
        Some(Command::Uninstall) => do_uninstall(),
        Some(Command::Info) => do_info(),
    };

    if let Err(e) = result {
        eprintln!("Error: {e}");
        std::process::exit(1);
    }
}
