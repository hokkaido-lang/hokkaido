pub mod manifest;
pub mod new;
pub mod build;
pub mod run;
pub mod add;
pub mod install;
pub mod cbuild;

use clap::{Parser, Subcommand};
use std::path::Path;

#[derive(Parser)]
#[command(name = "otaru", version, about = "Hokkaido package manager and project manager")]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Create a new Hokkaido project
    New {
        /// Project name
        name: String,
        /// Create a WebAssembly project template
        #[arg(long)]
        wasm: bool,
    },
    /// Build the current project or a single file
    Build {
        /// Input file (optional — if omitted, builds the project)
        file: Option<String>,
        /// Freestanding mode (no CRT/libc, main becomes ELF entry point)
        #[arg(long)]
        freestanding: bool,
        /// Force rebuild, ignoring cached artifacts
        #[arg(long, short)]
        force: bool,
        /// Build in release mode (with optimizations, equivalent to -O2)
        #[arg(long, short = 'r')]
        release: bool,
        /// Build a specific target (for projects with [[build.targets]])
        #[arg(long, short)]
        target: Option<String>,
        /// Target triple for cross-compilation (e.g., wasm32-unknown-wasi)
        #[arg(long)]
        triple: Option<String>,
    },
    /// Build and run the current project or a single file
    Run {
        /// Input .hk file (optional — if omitted, runs the project in src/)
        file: Option<String>,
        /// Freestanding mode (no CRT/libc, main becomes ELF entry point)
        #[arg(long)]
        freestanding: bool,
        /// Force rebuild, ignoring cached artifacts
        #[arg(long, short)]
        force: bool,
        /// Build in release mode (with optimizations, equivalent to -O2)
        #[arg(long, short = 'r')]
        release: bool,
    },
    /// Run a named shell script from otaru.toml
    Exec {
        /// Script name (omit to list all scripts)
        name: Option<String>,
        /// Arguments to pass to the script ($1, $2, etc.)
        #[arg(trailing_var_arg = true)]
        args: Vec<String>,
    },
    /// Add a dependency
    Add {
        /// Dependency name
        name: String,
        /// Git repository URL
        #[arg(long)]
        git: Option<String>,
        /// Local path
        #[arg(long)]
        path: Option<String>,
    },
    /// Install all dependencies
    Install,
    /// Clean build artifacts
    Clean,
}

fn main() {
    let cli = Cli::parse();

    match &cli.command {
        Command::New { name, wasm } => new::run(name, *wasm),
        Command::Build {
            file,
            freestanding,
            force,
            release,
            target,
            triple,
        } => {
            if cbuild::has_build_targets() || cbuild::is_c_project() {
                cbuild::run(file.as_deref(), *force, *release, target.as_deref());
            } else if build::has_hk_files() {
                build::run(file.as_deref(), *freestanding, *force, *release, triple.as_deref());
            } else {
                build::run(file.as_deref(), *freestanding, *force, *release, triple.as_deref());
            }
        }
        Command::Run {
            file,
            freestanding,
            force,
            release,
        } => {
            run::run(file.as_deref(), *freestanding, *force, *release);
        }
        Command::Exec { name, args } => {
            match name {
                None => list_scripts(),
                Some(n) => exec_script(n, args),
            }
        }
        Command::Add {
            name,
            git,
            path,
        } => add::run(name, git.as_deref(), path.as_deref()),
        Command::Install => install::run(),
        Command::Clean => clean(),
    }
}

fn clean() {
    let _ = std::fs::remove_dir_all("build");
    println!("Cleaned build directory");
}

fn load_scripts() -> std::collections::BTreeMap<String, String> {
    let manifest_path = Path::new("otaru.toml");
    if !manifest_path.exists() {
        eprintln!("Error: otaru.toml not found");
        std::process::exit(1);
    }
    let manifest = manifest::Manifest::load(manifest_path)
        .unwrap_or_else(|e| {
            eprintln!("{}", e);
            std::process::exit(1);
        });
    manifest.scripts
}

fn list_scripts() {
    let scripts = load_scripts();
    if scripts.is_empty() {
        println!("No scripts defined in otaru.toml");
        println!();
        println!("Add scripts with:");
        println!("  [scripts]");
        println!("  test = \"./build/myapp --test\"");
        return;
    }
    println!("Available scripts:");
    for (name, cmd) in &scripts {
        println!("  {} = \"{}\"", name, cmd);
    }
    println!();
    println!("Run with: otaru exec <name>");
}

fn exec_script(name: &str, args: &[String]) {
    let scripts = load_scripts();

    let script = scripts.get(name).unwrap_or_else(|| {
        eprintln!("Error: script '{}' not found in otaru.toml", name);
        if !scripts.is_empty() {
            eprintln!();
            eprintln!("Available scripts:");
            for (n, _) in &scripts {
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

    let status = std::process::Command::new("sh")
        .arg("-c")
        .arg(&cmd_str)
        .status()
        .unwrap_or_else(|e| {
            eprintln!("Error running script '{}': {}", name, e);
            std::process::exit(1);
        });

    std::process::exit(status.code().unwrap_or(1));
}
