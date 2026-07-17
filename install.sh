#!/usr/bin/env bash
set -euo pipefail

REPO="hokkaido-lang/hokkaido"
INSTALL_DIR="${HOKKAIDO_INSTALL_DIR:-$HOME/.hokkaido}"

echo "Hokkaido Installer"
echo "=================="
echo ""

# Find latest release (try /releases/latest first, fall back to /releases)
echo "Fetching latest release..."
LATEST=$(curl -sf "https://api.github.com/repos/$REPO/releases/latest" | grep '"tag_name"' | head -1 | sed -E 's/.*"tag_name": *"([^"]+)".*/\1/' || true)

if [ -z "$LATEST" ]; then
    # No published release yet — look for tags
    LATEST=$(curl -sf "https://api.github.com/repos/$REPO/tags" | grep '"name"' | head -1 | sed -E 's/.*"name": *"([^"]+)".*/\1/' || true)
fi

if [ -z "$LATEST" ]; then
    echo "Error: no releases found at github.com/$REPO"
    echo ""
    echo "Build from source instead:"
    echo "  git clone https://github.com/$REPO"
    echo "  cd hokkaido"
    echo "  nix build .#hokkaido -o result"
    echo "  cp result/bin/hokkaido ~/.local/bin/"
    exit 1
fi

echo "Release: $LATEST"
ASSET_URL="https://github.com/$REPO/releases/download/$LATEST/hokkaido-linux-x86_64.tar.gz"

# Download
echo "Downloading $ASSET_URL ..."
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

HTTP_CODE=$(curl -sL -w '%{http_code}' -o "$TMPDIR/hokkaido.tar.gz" "$ASSET_URL")
if [ "$HTTP_CODE" != "200" ]; then
    echo "Error: download failed (HTTP $HTTP_CODE)"
    echo ""
    echo "The release $LATEST may not have a pre-built binary yet."
    echo "Build from source instead:"
    echo "  git clone https://github.com/$REPO"
    echo "  cd hokkaido"
    echo "  nix build .#hokkaido -o result"
    echo "  cp result/bin/hokkaido ~/.local/bin/"
    exit 1
fi

# Extract
echo "Installing to $INSTALL_DIR ..."
mkdir -p "$INSTALL_DIR"
tar xzf "$TMPDIR/hokkaido.tar.gz" -C "$INSTALL_DIR"

# Make binaries executable
chmod +x "$INSTALL_DIR/bin/"* 2>/dev/null || true

# Also export for this session
export PATH="$INSTALL_DIR/bin:$PATH"

echo ""
echo "Installed to $INSTALL_DIR/bin/"
echo ""
ls -1 "$INSTALL_DIR/bin/"
echo ""
