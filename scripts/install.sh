#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"

echo "Building askdocs..."
cmake -S "$ROOT" -B "$ROOT/build"
cmake --build "$ROOT/build"

echo "Installing to $PREFIX/bin ..."
cmake --install "$ROOT/build" --prefix "$PREFIX"

ZSHRC="$HOME/.zshrc"
PATH_LINE='export PATH="$HOME/.local/bin:$PATH"'

if [[ -f "$ZSHRC" ]] && grep -q '\.local/bin' "$ZSHRC"; then
  echo "PATH already configured in $ZSHRC"
else
  echo "" >> "$ZSHRC"
  echo "# askdocs" >> "$ZSHRC"
  echo "$PATH_LINE" >> "$ZSHRC"
  echo "Added $PATH_LINE to $ZSHRC"
fi

echo ""
echo "Done. Run either:"
echo "  source ~/.zshrc"
echo "  askdocs path/to/file.cpp"
echo ""
echo "Docs are fetched online (cppreference, docs.python.org). Requires network."
echo "Disable with: export ASKDOCS_ONLINE=0"
