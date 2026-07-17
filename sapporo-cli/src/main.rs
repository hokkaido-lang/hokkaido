mod add;
mod build;
mod init;
mod manifest;
mod new;
mod run;

use clap::{Parser, Subcommand};

#[derive(Parser)]
#[command(name = "sapporo", about = "Web app toolkit for Hokkaido")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Create a new sapporo project
    New {
        /// Project name
        name: String,
    },

    /// Initialize current directory as a sapporo project
    Init,

    /// Build the project to dist/
    Build {
        /// Force rebuild even if output exists
        #[arg(short, long)]
        force: bool,
    },

    /// Build and run a local dev server
    Run {
        /// Force rebuild
        #[arg(short, long)]
        force: bool,
    },

    /// Add a dependency
    Add {
        /// Package name(s)
        packages: Vec<String>,

        /// Install via npm instead of otaru
        #[arg(long)]
        npm: bool,
    },
}

fn main() {
    let cli = Cli::parse();

    match cli.command {
        Commands::New { name } => new::run(&name),
        Commands::Init => init::run(),
        Commands::Build { force } => build::run(force),
        Commands::Run { force } => run::run(force),
        Commands::Add { packages, npm } => add::run(&packages, npm),
    }
}
