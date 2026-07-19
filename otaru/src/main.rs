pub mod add;
pub mod build;
pub mod cbuild;
pub mod exec;
pub mod init;
pub mod install;
pub mod manifest;
pub mod new;
pub mod run;
pub mod template;
pub mod utils;

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
        /// Create a web app project (sapporo DOM library, dist/ output, dev server)
        #[arg(long)]
        web: bool,
        /// Create a WebAssembly project template (legacy)
        #[arg(long)]
        wasm: bool,
    },
    /// Initialize the current directory as a Hokkaido project
    Init {
        /// Initialize as a web app project (sapporo DOM library, dist/ output, dev server)
        #[arg(long)]
        web: bool,
        /// Initialize as a WebAssembly project (legacy)
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
        /// Dependency name(s)
        packages: Vec<String>,
        /// Git repository URL
        #[arg(long)]
        git: Option<String>,
        /// Local path
        #[arg(long)]
        path: Option<String>,
        /// Install via npm instead of otaru
        #[arg(long)]
        npm: bool,
    },
    /// Install all dependencies
    Install,
    /// Clean build artifacts
    Clean,
}

fn main() {
    let cli = Cli::parse();

    match &cli.command {
        Command::New { name, web, wasm } => new::run(name, *web, *wasm),
        Command::Init { web, wasm } => init::run(*web, *wasm),
        Command::Build {
            file,
            freestanding,
            force,
            release,
            target,
            triple,
        } => {
            if utils::has_build_targets() || utils::is_c_project() {
                cbuild::run(file.as_deref(), *force, *release, target.as_deref());
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
        Command::Exec { name, args } => match name {
            None => exec::list_scripts(),
            Some(n) => exec::exec_script(n, args),
        },
        Command::Add {
            packages,
            git,
            path,
            npm,
        } => add::run(packages, git.as_deref(), path.as_deref(), *npm),
        Command::Install => install::run(),
        Command::Clean => exec::clean(),
    }
}
