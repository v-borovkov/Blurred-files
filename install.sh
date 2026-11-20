#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
BINDIR="$PREFIX/bin"
DATADIR="$PREFIX/share/blurred_files"
APPDIR="$PREFIX/share/applications"

echo "==> Building blurred_files..."
make clean
PREFIX="$PREFIX" make -j"$(nproc)"

echo "==> Installing binary and assets to $PREFIX..."
sudo install -Dm755 build/blurred_files "$BINDIR/blurred_files"
sudo install -Dm644 src/bfiles.css /usr/local/share/blurred_files/bfiles.css

echo "==> Installing desktop launcher entry..."
sudo mkdir -p "$APPDIR"
cat <<EOF | sudo tee "$APPDIR/blurred_files.desktop" > /dev/null
[Desktop Entry]
Name=Blurred Files
Comment=Custom Superellipse Wayland File Manager
Exec=env GTK_A11Y=none $BINDIR/blurred_files
Icon=system-file-manager
Terminal=false
Type=Application
Categories=System;FileManager;Utility;
EOF

echo "==> Done! Launch 'blurred_files' from your terminal or app launcher."