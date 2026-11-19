#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="blurred-files"
VERSION="1.0.0"
ARCH="amd64"
if command -v dpkg >/dev/null 2>&1; then
    ARCH="$(dpkg --print-architecture)"
elif [ "$(uname -m)" = "aarch64" ]; then
    ARCH="arm64"
fi

STAGE_DIR="build/deb_pkg"
DEB_WORK="build/deb_work"

echo "==> Cleaning and building release binary for /usr..."
make clean
PREFIX=/usr make -j"$(nproc)"

echo "==> Staging files into $STAGE_DIR..."
rm -rf "$STAGE_DIR" "$DEB_WORK"
mkdir -p "$STAGE_DIR/DEBIAN" "$DEB_WORK"
DESTDIR="$(pwd)/$STAGE_DIR" PREFIX=/usr make install

cat <<EOF > "$STAGE_DIR/DEBIAN/control"
Package: $PKG_NAME
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Depends: libgtk-4-1, libglib2.0-0, libcairo2, libgdk-pixbuf-2.0-0
Recommends: zip, unzip, tar, xdg-utils
Maintainer: Custom Developer <dev@localhost>
Description: Blurred Files - Custom Superellipse Wayland File Manager
 A lightweight, GPU-accelerated GTK4 and Cairo file manager featuring a
 custom superellipse silhouette, frosted surfaces, drag-and-drop,
 archive extraction, and Wayland clipboard integration.
EOF

cat <<'EOF' > "$STAGE_DIR/DEBIAN/postinst"
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
EOF
chmod 0755 "$STAGE_DIR/DEBIAN/postinst"

cat <<'EOF' > "$STAGE_DIR/DEBIAN/postrm"
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
EOF
chmod 0755 "$STAGE_DIR/DEBIAN/postrm"

DEB_OUT="$(pwd)/${PKG_NAME}_${VERSION}_${ARCH}.deb"
rm -f "$DEB_OUT"

if command -v dpkg-deb >/dev/null 2>&1; then
    dpkg-deb --build --root-owner-group "$STAGE_DIR" "$DEB_OUT"
else
    echo "==> dpkg-deb not found; assembling .deb archive with tar + ar..."
    printf "2.0\n" > "$DEB_WORK/debian-binary"
    tar --owner=0 --group=0 -czf "$DEB_WORK/control.tar.gz" -C "$STAGE_DIR/DEBIAN" .
    tar --owner=0 --group=0 --exclude='./DEBIAN' -czf "$DEB_WORK/data.tar.gz" -C "$STAGE_DIR" .
    (cd "$DEB_WORK" && ar rcs "$DEB_OUT" debian-binary control.tar.gz data.tar.gz)
fi

echo "==> Created $(basename "$DEB_OUT")"
echo "    Install on Debian/Ubuntu with: sudo apt install ./$(basename "$DEB_OUT")"