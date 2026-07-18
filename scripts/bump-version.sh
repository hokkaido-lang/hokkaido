#!/usr/bin/env bash
#
# bump-version.sh — Update version numbers across the entire project.
#
# Reads versions from versions.toml (single source of truth) and updates:
#   - default.nix (root)
#   - Cargo.toml / default.nix (per component)
#   - Cargo.lock (per component, by running cargo check)
#   - AGENTS.md version table
#
# Usage:
#   ./scripts/bump-version.sh --apply           Read versions.toml, update all files
#   ./scripts/bump-version.sh --set <component> <version>  Set a version and apply
#   ./scripts/bump-version.sh --show            Show current versions
#   ./scripts/bump-version.sh --commit          Apply + git commit

set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

VERSIONS_TOML="versions.toml"
ROOT_DIR="."

# ── Helpers ──────────────────────────────────────────────────────────────────

die() { echo "ERROR: $*" >&2; exit 1; }

# Read a value from versions.toml: get_version <component>
get_version() {
    local component="$1"
    # Parse simple key = "value" TOML (no nesting needed for our flat file)
    grep -A1 "^\[$component\]" "$VERSIONS_TOML" | grep 'version' | sed 's/.*"\(.*\)".*/\1/'
}

# sed in-place, cross-platform (GNU & BSD)
sed_inplace() {
    if sed --version 2>/dev/null | grep -q GNU; then
        sed -i "$@"
    else
        sed -i '' "$@"
    fi
}

# ── Apply a single component ─────────────────────────────────────────────────

apply_component() {
    local component="$1"
    local version="$2"
    local files_updated=0

    echo "  [$component] -> $version"

    case "$component" in
        hokkaido)
            # Root default.nix
            if [ -f "$ROOT_DIR/default.nix" ]; then
                sed_inplace 's/version = "[^"]*"/version = "'"$version"'"/' "$ROOT_DIR/default.nix"
                echo "    updated default.nix"
                files_updated=$((files_updated + 1))
            fi
            ;;

        otaru)
            # otaru/Cargo.toml
            if [ -f "otaru/Cargo.toml" ]; then
                sed_inplace '/^\[package\]/,/version/{s/version = "[^"]*"/version = "'"$version"'"/}' "otaru/Cargo.toml"
                echo "    updated otaru/Cargo.toml"
                files_updated=$((files_updated + 1))
            fi
            # otaru/default.nix
            if [ -f "otaru/default.nix" ]; then
                sed_inplace 's/version = "[^"]*"/version = "'"$version"'"/' "otaru/default.nix"
                echo "    updated otaru/default.nix"
                files_updated=$((files_updated + 1))
            fi
            # Regenerate Cargo.lock
            if [ -f "otaru/Cargo.lock" ]; then
                (cd otaru && cargo check 2>/dev/null) && echo "    updated otaru/Cargo.lock" || echo "    (cargo check skipped)"
                files_updated=$((files_updated + 1))
            fi
            ;;

        sapporo)
            # sapporo-cli/Cargo.toml
            if [ -f "sapporo-cli/Cargo.toml" ]; then
                sed_inplace '/^\[package\]/,/version/{s/version = "[^"]*"/version = "'"$version"'"/}' "sapporo-cli/Cargo.toml"
                echo "    updated sapporo-cli/Cargo.toml"
                files_updated=$((files_updated + 1))
            fi
            # sapporo-cli/default.nix
            if [ -f "sapporo-cli/default.nix" ]; then
                sed_inplace 's/version = "[^"]*"/version = "'"$version"'"/' "sapporo-cli/default.nix"
                echo "    updated sapporo-cli/default.nix"
                files_updated=$((files_updated + 1))
            fi
            # Regenerate Cargo.lock
            if [ -f "sapporo-cli/Cargo.lock" ]; then
                (cd sapporo-cli && cargo check 2>/dev/null) && echo "    updated sapporo-cli/Cargo.lock" || echo "    (cargo check skipped)"
                files_updated=$((files_updated + 1))
            fi
            ;;

        cubical)
            # Root Cargo.toml
            if [ -f "Cargo.toml" ]; then
                sed_inplace '/^\[package\]/,/version/{s/version = "[^"]*"/version = "'"$version"'"/}' "Cargo.toml"
                echo "    updated Cargo.toml"
                files_updated=$((files_updated + 1))
            fi
            # Regenerate root Cargo.lock
            if [ -f "Cargo.lock" ]; then
                cargo check 2>/dev/null && echo "    updated Cargo.lock" || echo "    (cargo check skipped)"
                files_updated=$((files_updated + 1))
            fi
            ;;

        *)
            echo "    WARNING: unknown component '$component', skipping"
            return
            ;;
    esac

    echo "    ($files_updated files)"
}

# ── Update AGENTS.md version table ───────────────────────────────────────────

update_agents_md() {
    local hokkaido_ver otaru_ver sapporo_ver cubical_ver
    hokkaido_ver=$(get_version hokkaido)
    otaru_ver=$(get_version otaru)
    sapporo_ver=$(get_version sapporo)
    cubical_ver=$(get_version cubical)

    if [ ! -f "AGENTS.md" ]; then
        echo "  WARNING: AGENTS.md not found, skipping"
        return
    fi

    # Update the version table (format: | component | version | files |)
    sed_inplace 's/| hokkaido | [0-9.]* |/| hokkaido | '"$hokkaido_ver"' |/' "AGENTS.md"
    sed_inplace 's/| otaru | [0-9.]* |/| otaru | '"$otaru_ver"' |/' "AGENTS.md"
    sed_inplace 's/| sapporo | [0-9.]* |/| sapporo | '"$sapporo_ver"' |/' "AGENTS.md"
    sed_inplace 's/| cubical | [0-9.]* |/| cubical | '"$cubical_ver"' |/' "AGENTS.md"

    echo "  updated AGENTS.md"
}

# ── Commands ─────────────────────────────────────────────────────────────────

cmd_apply() {
    echo "Applying versions from $VERSIONS_TOML..."
    echo

    for component in hokkaido otaru sapporo cubical; do
        local version
        version=$(get_version "$component")
        if [ -z "$version" ]; then
            echo "  WARNING: no version found for $component in $VERSIONS_TOML"
            continue
        fi
        apply_component "$component" "$version"
    done

    echo
    echo "Updating AGENTS.md..."
    update_agents_md

    echo
    echo "Done. All versions updated."
}

cmd_set() {
    local component="${1:-}"
    local version="${2:-}"

    [ -z "$component" ] && die "Usage: $0 --set <component> <version>"
    [ -z "$version" ] && die "Usage: $0 --set <component> <version>"

    # Validate semver-ish format
    if ! echo "$version" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+'; then
        die "Version must be semver-like (e.g. 1.2.3), got: $version"
    fi

    # Update versions.toml
    if grep -q "^\[$component\]" "$VERSIONS_TOML"; then
        sed_inplace '/^\['"$component"'\]/,/version/{s/version = "[^"]*"/version = "'"$version"'"/}' "$VERSIONS_TOML"
    else
        # Append new section
        echo "" >> "$VERSIONS_TOML"
        echo "[$component]" >> "$VERSIONS_TOML"
        echo "version = \"$version\"" >> "$VERSIONS_TOML"
    fi

    echo "Set $component = $version in $VERSIONS_TOML"
    echo
    cmd_apply
}

cmd_show() {
    echo "Current versions (from $VERSIONS_TOML):"
    echo
    printf "  %-12s %s\n" "Component" "Version"
    printf "  %-12s %s\n" "---------" "-------"
    for component in hokkaido otaru sapporo cubical; do
        local version
        version=$(get_version "$component")
        printf "  %-12s %s\n" "$component" "$version"
    done
    echo
    echo "Files managed:"
    printf "  %-30s %s\n" "File" "Field"
    printf "  %-30s %s\n" "----" "-----"
    printf "  %-30s %s\n" "default.nix" "hokkaido"
    printf "  %-30s %s\n" "Cargo.toml" "cubical"
    printf "  %-30s %s\n" "otaru/Cargo.toml" "otaru"
    printf "  %-30s %s\n" "otaru/default.nix" "otaru"
    printf "  %-30s %s\n" "sapporo-cli/Cargo.toml" "sapporo"
    printf "  %-30s %s\n" "sapporo-cli/default.nix" "sapporo"
    printf "  %-30s %s\n" "AGENTS.md" "all (version table)"
}

cmd_commit() {
    cmd_apply
    echo
    echo "Committing changes..."
    git add -A
    git diff --cached --quiet && echo "Nothing to commit." && return 0
    git commit -m "chore: bump versions

$(for c in hokkaido otaru sapporo cubical; do
    v=$(get_version "$c")
    echo "- $c: $v"
done)"
    echo
    echo "Committed. Don't forget to push and create a release tag:"
    echo "  git push"
    echo "  git tag v\$(grep '^version' versions.toml | head -1 | sed 's/.*\"\(.*\)\".*/\1/')"
}

# ── Main ─────────────────────────────────────────────────────────────────────

usage() {
    cat <<EOF
Usage: $0 <command> [args]

Commands:
  --apply                 Read versions.toml and update all files
  --set <component> <v>   Set a component version, update versions.toml, then apply
  --show                  Show current versions and managed files
  --commit                Apply + git commit

Components: hokkaido, otaru, sapporo, cubical

Examples:
  $0 --show
  $0 --set sapporo 0.3.0
  $0 --set hokkaido 0.20.0
  $0 --commit

The single source of truth is versions.toml. This script propagates those
values into default.nix, Cargo.toml, Cargo.lock, and AGENTS.md.
EOF
}

case "${1:-}" in
    --apply)  cmd_apply ;;
    --set)    cmd_set "${2:-}" "${3:-}" ;;
    --show)   cmd_show ;;
    --commit) cmd_commit ;;
    -h|--help|"") usage ;;
    *) die "Unknown command: $1 (try --help)" ;;
esac
