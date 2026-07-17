#!/bin/sh
# build.sh — compile Sapporo example to WASM
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
LIB_DIR="$ROOT_DIR/sapporo"

# Find hokkaido
HOKKAIDO="hokkaido"
if ! command -v "$HOKKAIDO" >/dev/null 2>&1; then
  # Walk up from this script looking for build/hokkaido
  SEARCH_DIR="$SCRIPT_DIR"
    for i in 1 2 3 4 5 6; do
    if [ -x "$SEARCH_DIR/build/hokkaido" ]; then
      HOKKAIDO="$SEARCH_DIR/build/hokkaido"
      break
    fi
    SEARCH_DIR="$(dirname "$SEARCH_DIR")"
  done
  if [ "$HOKKAIDO" = "hokkaido" ]; then
    echo "Error: hokkaido not found. Add it to PATH."
    exit 1
  fi
fi

# Find wasm-ld
WASM_LD=""
for candidate in wasm-ld wasm-ld-21 wasm-ld-20 wasm-ld-19 wasm-ld-18 wasm-ld-17; do
  if command -v "$candidate" >/dev/null 2>&1; then
    WASM_LD="$candidate"
    break
  fi
done
# Search nix store if not found
if [ -z "$WASM_LD" ]; then
  WASM_LD="$(find /nix/store -name "wasm-ld" -type f 2>/dev/null | head -1)"
fi
if [ -z "$WASM_LD" ]; then
  echo "Error: wasm-ld not found."
  exit 1
fi

echo "Compiling $SCRIPT_DIR/main.hk"
"$HOKKAIDO" "$SCRIPT_DIR/main.hk" \
  -o "$SCRIPT_DIR/app" \
  --target wasm32-unknown-unknown

echo "Linking with wasm-ld"
"$WASM_LD" \
  --no-entry \
  --export=main \
  --allow-undefined \
  -o "$SCRIPT_DIR/app.wasm" \
  "$SCRIPT_DIR/app.o"

# Copy sapporo.js next to the HTML
cp "$ROOT_DIR/sapporo.js" "$SCRIPT_DIR/sapporo.js"

echo "Built: $SCRIPT_DIR/app.wasm"
echo ""
echo "To test:"
echo "  cd $SCRIPT_DIR && python3 -m http.server 8080"
echo "  open http://localhost:8080"
