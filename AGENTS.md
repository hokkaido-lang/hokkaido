# AGENTS

## Version Bumping

After modifying code and updating the project, update version numbers in the relevant files. The root `default.nix` must always be updated for any change.

### Hokkaido (C++ compiler)
- `default.nix` — `version = "..."` (root directory, always update)

### Sapporo CLI (Rust)
- `sapporo-cli/Cargo.toml` — `version = "..."`
- `sapporo-cli/default.nix` — `version = "..."`

### Otaru (Rust)
- `otaru/Cargo.toml` — `version = "..."`
- `otaru/default.nix` — `version = "..."`

Keep Cargo.toml and default.nix versions in sync within each project.

### Version locations at a glance

| File | Field |
|------|-------|
| `default.nix` | `version = "0.19.3"` |
| `otaru/Cargo.toml` | `version = "0.7.0"` |
| `otaru/default.nix` | `version = "0.7.0"` |
| `sapporo-cli/Cargo.toml` | `version = "0.2.3"` |
| `sapporo-cli/default.nix` | `version = "0.2.3"` |
