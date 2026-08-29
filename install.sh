#!/bin/sh
# Hokkaido Installer
# Usage: curl -fsSL https://raw.githubusercontent.com/hokkaido-lang/hokkaido/main/install.sh | sh
set -e

REPO="hokkaido-lang/hokkaido"
INSTALL_DIR="${HOKKAIDO_INSTALL_DIR:-/usr/local}"
BINARY_DIR="${INSTALL_DIR}/bin"
SHARE_DIR="${INSTALL_DIR}/share/hokkaido"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { printf "${CYAN}%s${NC}\n" "$*"; }
ok()    { printf "${GREEN}%s${NC}\n" "$*"; }
warn()  { printf "${YELLOW}%s${NC}\n" "$*"; }
err()   { printf "${RED}%s${NC}\n" "$*" >&2; exit 1; }

# Detect OS and arch
detect_platform() {
    os="$(uname -s)"
    arch="$(uname -m)"

    case "$os" in
        Linux)  platform="linux-x86_64" ;;
        Darwin) platform="darwin-x86_64" ;;
        *)      err "Unsupported OS: $os" ;;
    esac

    case "$arch" in
        x86_64|amd64) ;; # already set
        arm64|aarch64) platform="linux-aarch64" ;;
        *)             err "Unsupported architecture: $arch" ;;
    esac

    info "Detected platform: $platform"
}

# Get latest release tag
get_latest_version() {
    if command -v gh >/dev/null 2>&1; then
        version=$(gh release list --repo "$REPO" --limit 1 --json tagName --jq '.[0].tagName' 2>/dev/null)
    fi

    if [ -z "$version" ]; then
        version=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" 2>/dev/null | grep '"tag_name"' | head -1 | cut -d'"' -f4)
    fi

    if [ -z "$version" ]; then
        err "Failed to detect latest version. Set HOKKAIDO_VERSION manually."
    fi

    info "Latest version: $version"
}

# Download and extract
download_and_extract() {
    version="${HOKKAIDO_VERSION:-$version}"
    tarball="hokkaido-${platform}.tar.gz"
    url="https://github.com/$REPO/releases/download/${version}/${tarball}"

    info "Downloading $tarball..."
    tmpdir=$(mktemp -d)
    trap "rm -rf $tmpdir" EXIT

    if ! curl -fSL "$url" -o "$tmpdir/$tarball" 2>/dev/null; then
        err "Download failed: $url"
    fi

    info "Extracting..."
    tar -xzf "$tmpdir/$tarball" -C "$tmpdir"

    # Check for required files
    if [ ! -d "$tmpdir/bin" ]; then
        err "Invalid archive: missing bin/ directory"
    fi

    # Install binaries
    install -d "$BINARY_DIR"
    for bin in "$tmpdir"/bin/*; do
        name=$(basename "$bin")
        info "  Installing $name -> $BINARY_DIR/$name"
        install -m 755 "$bin" "$BINARY_DIR/$name"
    done

    # Install std if present
    if [ -d "$tmpdir/share/std" ]; then
        install -d "$SHARE_DIR"
        info "  Installing std -> $SHARE_DIR/std"
        cp -r "$tmpdir/share/std" "$SHARE_DIR/std"
    fi
}

# Setup shell environment
setup_shell() {
    shell_name=$(basename "${SHELL:-/bin/sh}")

    case "$shell_name" in
        bash)
            rc="$HOME/.bashrc"
            profile="$HOME/.bash_profile"
            ;;
        zsh)
            rc="$HOME/.zshrc"
            profile="$HOME/.zprofile"
            ;;
        fish)
            rc="$HOME/.config/fish/config.fish"
            profile="$rc"
            ;;
        *)
            rc="$HOME/.profile"
            profile="$rc"
            ;;
    esac

    export_line="export HOKKAIDO_STD=\"${SHARE_DIR}/std\""

    # Check if already configured
    if grep -q "HOKKAIDO_STD" "$rc" 2>/dev/null; then
        ok "HOKKAIDO_STD already configured in $rc"
        return
    fi

    # Add to shell config
    {
        echo ""
        echo "# Hokkaido"
        echo "$export_line"
    } >> "$rc"

    ok "Added HOKKAIDO_STD to $rc"
    info "Run 'source $rc' or restart your shell"
}

# Print summary
print_summary() {
    echo ""
    ok "Hokkaido installed successfully!"
    echo ""
    info "Installed components:"
    command -v hokkaido >/dev/null && ok "  hokkaido  $(hokkaido --version 2>/dev/null || echo 'installed')"
    command -v otaru >/dev/null   && ok "  otaru     $(otaru --version 2>/dev/null || echo 'installed')"
    command -v hok-lsp >/dev/null && ok "  hok-lsp   $(hok-lsp --version 2>/dev/null || echo 'installed')"
    [ -d "$SHARE_DIR/std" ]      && ok "  std       $SHARE_DIR/std"
    echo ""
    info "Quick start:"
    info "  otaru new myproject"
    info "  otaru new my-webapp --web"
    echo ""
}

# Main
main() {
    echo ""
    info "=== Hokkaido Installer ==="
    echo ""

    detect_platform
    get_latest_version
    download_and_extract
    setup_shell
    print_summary
}

main "$@"
