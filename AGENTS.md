# AGENTS

Instructions for AI agents working on this repository.

## Core Principles

1. **Always update docs** when changing functionality
2. **Always update versions** when releasing (use the automated tool)
3. **Keep READMEs accurate** — they're the first thing users see
4. **Test your changes** before committing

## Version Management

### Single Source of Truth: `versions.toml`

All versions live in `versions.toml`. Never edit version numbers directly in other files.

```bash
./scripts/bump-version.sh --show              # View current versions
./scripts/bump-version.sh --set sapporo 0.4.1 # Bump a component
./scripts/bump-version.sh --apply             # Re-apply versions.toml to all files
./scripts/bump-version.sh --commit            # Apply + git commit
```

### Components

| Component | Version | Files Updated |
|-----------|---------|---------------|
| hokkaido | 0.20.5 | `default.nix` |
| otaru | 0.7.1 | `otaru/Cargo.toml`, `otaru/default.nix`, `otaru/Cargo.lock` |
| sapporo | 0.4.0 | `sapporo-cli/Cargo.toml`, `sapporo-cli/default.nix`, `sapporo-cli/Cargo.lock` |
| cubical | 0.3.0 | `Cargo.toml`, `Cargo.lock` |

### When to Bump Versions

- **Bug fix**: Patch bump (0.7.1 → 0.7.2)
- **New feature**: Minor bump (0.7.1 → 0.8.0)
- **Breaking change**: Major bump (0.7.1 → 1.0.0)

## Documentation Updates

### Always Update These Files

When you change functionality, check and update:

1. **README.md** — Project overview, quick start, features
2. **Relevant component README** — `sapporo-cli/README.md`, etc.
3. **`sapporo/docs/*.md`** — API docs, examples, tutorials
4. **`AGENTS.md`** — This file (version table, workflows)

### Documentation Checklist

- [ ] API changes → Update `sapporo/docs/api.md`
- [ ] New examples → Update `sapporo/docs/examples.md`
- [ ] Architecture changes → Update `sapporo/docs/docs.md`
- [ ] CLI changes → Update component README
- [ ] Version bump → Run `./scripts/bump-version.sh --commit`

## Common Workflows

### Adding a New Feature

1. Implement the feature
2. Add tests if applicable
3. Update relevant docs (API, examples, README)
4. Run `./scripts/bump-version.sh --apply` if version bump needed
5. Commit with descriptive message

### Fixing a Bug

1. Fix the bug
2. Add regression test if applicable
3. Update docs if behavior changed
4. Commit with descriptive message

### Releasing a New Version

1. Ensure all changes are committed
2. Run `./scripts/bump-version.sh --set <component> <version>`
3. Run `./scripts/bump-version.sh --commit`
4. Push: `git push`
5. Create release tag: `git tag v<version>`
6. Push tag: `git push --tags`

## Project Structure

```
hokkaido/
├── default.nix          # Hokkaido compiler (C++)
├── Cargo.toml           # Cubical crate (Rust)
├── std/                 # Standard library
├── otaru/               # Package manager (Rust)
├── sapporo-cli/         # Web app toolkit CLI (Rust)
├── sapporo/             # Web app toolkit library
├── install.sh           # Official installer
├── versions.toml        # Single source of truth for versions
├── scripts/             # Automation scripts
└── AGENTS.md            # This file
```

## Tools

- **nix**: Use when specific tool is not available
- **cargo**: Rust package manager
- **./scripts/bump-version.sh**: Version management

## Code Style

- **Rust**: Follow standard rustfmt conventions
- **Shell**: Use `#!/usr/bin/env bash`, `set -euo pipefail`
- **Nix**: Follow nixpkgs conventions
- **Comments**: Minimal, only when necessary

## Testing

- Run `cargo test` for Rust components
- Test CLI tools manually after changes
- Verify examples work before committing
