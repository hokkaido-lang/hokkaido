# sapporo CLI — DEPRECATED

> **This tool is deprecated.** All features have been merged into **otaru**.
>
> ```bash
> # Instead of: sapporo new myapp
> otaru new myapp --web
>
> # Instead of: sapporo build
> otaru build
>
> # Instead of: sapporo run
> otaru run
>
> # Instead of: sapporo add --npm lodash
> otaru add --npm lodash
> ```
>
> See [otaru/README.md](../otaru/README.md) for full documentation.

---

The rest of this file is preserved for historical reference.

## Installation

### From release

Download the latest release and add to your PATH:

```bash
tar xzf hokkaido-linux-x86_64.tar.gz
export PATH="$PWD/bin:$PATH"
```

The `sapporo` binary is bundled alongside `hokkaido` and `otaru`.

### From source

```bash
cd sapporo-cli
cargo build --release
cp target/release/sapporo ~/.local/bin/
```

## Quick start

```bash
# Create a new project
sapporo new my-app
cd my-app

# Build to dist/
sapporo build

# Start dev server and open browser
sapporo run
```

## Commands

### `sapporo new <name>`

Create a new project in a directory called `<name>`.

### `sapporo init`

Initialize the current directory as a sapporo project.

### `sapporo build`

Compile `.hk` sources to WebAssembly and package into `dist/`.

### `sapporo run`

Build the project and start a local HTTP server on port 8080. Opens the browser automatically.

### `sapporo add <package>`

Add a dependency to `sapporo.toml`.

```bash
sapporo add sapporo-lib        # Add a Hokkaido package
sapporo add lodash --npm       # Add an npm package
```

## License

Apache 2.0
