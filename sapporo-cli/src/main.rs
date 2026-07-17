mod add;
mod build;
mod init;
mod manifest;
mod new;
mod run;
mod scaffold;

use clap::{Parser, Subcommand};

#[derive(Parser)]
#[command(name = "sapporo", about = "Web app toolkit for Hokkaido")]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// Enable verbose output
    #[arg(short, long, global = true)]
    verbose: bool,
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
        Commands::New { name } => new::run(&name, cli.verbose),
        Commands::Init => init::run(cli.verbose),
        Commands::Build { force } => build::run(force, cli.verbose),
        Commands::Run { force } => run::run(force, cli.verbose),
        Commands::Add { packages, npm } => add::run(&packages, npm),
    }
}
