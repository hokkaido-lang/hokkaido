pub mod manifest;
pub mod new;
pub mod build;
pub mod run;
pub mod add;
pub mod install;
pub mod std_embed;

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
    /// Build the current project
    Build,
    /// Build and run the current project
    Run,
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
        Command::Build => build::run(),
        Command::Run => run::run(),
        Command::Add { name, git, path } => add::run(name, git.as_deref(), path.as_deref()),
        Command::Install => install::run(),
        Command::Clean => clean(),
    }
}

fn clean() {
    let _ = std::fs::remove_dir_all("build");
    println!("Cleaned build directory");
}
