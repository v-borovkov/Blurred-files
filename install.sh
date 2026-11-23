#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"

echo "==> Building blurred_files..."
make clean
make PREFIX="$PREFIX" -j"$(nproc)"

echo "==> Installing binary, theme, icon, and desktop entry to $PREFIX..."
sudo make install PREFIX="$PREFIX"

echo "==> Done! Launch 'blurred_files' from your terminal or app launcher."