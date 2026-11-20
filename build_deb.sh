#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="blurred-files"
VERSION="0.7.0"
ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"
STAGE_DIR="pkg_deb"
OUT_DEB="${PKG_NAME}_${VERSION}_${ARCH}.deb"

rm -rf "$STAGE_DIR"
make clean
make PREFIX=/usr -j"$(nproc)"

# Install binary, bfiles.css, desktop entry, and icon into staging dir
install -Dm755 build/blurred_files "$STAGE_DIR/usr/bin/blurred_files"
install -Dm644 src/bfiles.css "$STAGE_DIR/usr/share/blurred_files/bfiles.css"
install -Dm644 blurred_files.desktop "$STAGE_DIR/usr/share/applications/blurred_files.desktop"
install -Dm644 src/blurred_files.svg "$STAGE_DIR/usr/share/icons/hicolor/scalable/apps/blurred_files.svg"

# Create DEBIAN control metadata
mkdir -p "$STAGE_DIR/DEBIAN"
chmod 0755 "$STAGE_DIR/DEBIAN"

cat > "$STAGE_DIR/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Depends: libgtk-4-1, libglib2.0-0, libcairo2, zip, unzip, tar, xdg-utils
Maintainer: v-borovkov <v-borovkov@://github.com>
Description: Fast, lightweight GTK4 file manager with superellipse UI
 A custom GTK4 file manager written in C featuring a superellipse silhouette,
 translucent Wayfire-blurred sidebar, and tactile Neumorphic header controls.
EOF

cat > "$STAGE_DIR/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
fi
EOF
chmod 0755 "$STAGE_DIR/DEBIAN/postinst"

dpkg-deb --build --root-owner-group "$STAGE_DIR" "$OUT_DEB"
rm -rf "$STAGE_DIR"
echo "Built $OUT_DEB successfully."