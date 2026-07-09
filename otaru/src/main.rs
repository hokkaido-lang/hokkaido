pub mod manifest;
pub mod new;
pub mod build;
pub mod run;
pub mod add;
pub mod install;

use clap::{Parser, Subcommand};

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
    },
    /// Build the current project or a single file
    Build {
        /// Input .hk file (optional — if omitted, builds the project in src/)
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
        Command::New { name } => new::run(name),
        Command::Build { file, freestanding, force, release } => {
            build::run(file.as_deref(), *freestanding, *force, *release);
        }
        Command::Run { file, freestanding, force, release } => {
            run::run(file.as_deref(), *freestanding, *force, *release);
        }
        Command::Add { name, git, path } => add::run(name, git.as_deref(), path.as_deref()),
        Command::Install => install::run(),
        Command::Clean => clean(),
    }
}

fn clean() {
    let _ = std::fs::remove_dir_all("build");
    println!("Cleaned build directory");
}
