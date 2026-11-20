#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="blurred-files"
VERSION="0.7.0"
RPMBUILD_DIR="$HOME/rpmbuild"

mkdir -p "$RPMBUILD_DIR"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Create source tarball from current working tree
tar --exclude='.git' \
    --exclude='build' \
    --exclude='pkg_deb' \
    --exclude='rpmbuild' \
    --exclude='*.deb' \
    --exclude='*.rpm' \
    --transform "s,^\.,${PKG_NAME}-${VERSION}," \
    -czf "$RPMBUILD_DIR/SOURCES/${PKG_NAME}-${VERSION}.tar.gz" .

cp blurred-files.spec "$RPMBUILD_DIR/SPECS/"
rpmbuild --define "_topdir $RPMBUILD_DIR" -ba "$RPMBUILD_DIR/SPECS/blurred-files.spec"

echo "Built RPM successfully in $RPMBUILD_DIR/RPMS/"