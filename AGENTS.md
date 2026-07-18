# AGENTS

## Version Bumping

**Use the automated tool:** `./scripts/bump-version.sh`

The single source of truth is `versions.toml`. The script propagates versions into all files automatically.

```bash
./scripts/bump-version.sh --show              # View current versions
./scripts/bump-version.sh --set sapporo 0.3.0 # Bump a component + apply everywhere
./scripts/bump-version.sh --apply             # Re-apply versions.toml to all files
./scripts/bump-version.sh --commit            # Apply + git commit
```

### Hokkaido (C++ compiler)
- `default.nix` — `version = "..."` (root directory, always update)

### Sapporo CLI (Rust)
- `sapporo-cli/Cargo.toml` — `version = "..."`
- `sapporo-cli/default.nix` — `version = "..."`

### Otaru (Rust)
- `otaru/Cargo.toml` — `version = "..."`
- `otaru/default.nix` — `version = "..."`

### Cubical (Rust)
- `Cargo.toml` — `version = "..."` (root directory)

Keep Cargo.toml and default.nix versions in sync within each project.

### Version locations at a glance

| File | Field |
|------|-------|
| `default.nix` | `version = "0.20.4"` |
| `otaru/Cargo.toml` | `version = "0.7.1"` |
| `otaru/default.nix` | `version = "0.7.1"` |
| `sapporo-cli/Cargo.toml` | `version = "0.3.3"` |
| `sapporo-cli/default.nix` | `version = "0.3.3"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |
| `Cargo.toml` | `version = "0.3.0"` |

## tools
use nix when specific tool is not available
