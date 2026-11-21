#!/usr/bin/env bash
set -euo pipefail

NIGHTMARE_DIR="/tmp/blurred-nightmare"

echo "=== [1/4] Building Blurred-files & Test Runner with ASan + UBSan ==="
make sanitize -j"$(nproc)"

echo "=== [2/4] Running Automated Tests under Sanitizers ==="
export ASAN_OPTIONS="halt_on_error=1:abort_on_error=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
make test

echo "=== [3/4] Generating Hostile Filesystem Fixture at $NIGHTMARE_DIR ==="
chmod -R u+w "$NIGHTMARE_DIR" 2>/dev/null || true
rm -rf "$NIGHTMARE_DIR"
mkdir -p "$NIGHTMARE_DIR/mass_files" "$NIGHTMARE_DIR/readonly_dir"

# Hostile filenames (spaces, quotes, newlines, leading dashes, unicode)
touch "$NIGHTMARE_DIR/file with spaces.txt"
touch "$NIGHTMARE_DIR/quote'and\"double.txt"
touch "$NIGHTMARE_DIR/"$'line\nbreak.txt'
touch "$NIGHTMARE_DIR/--leading-dash.txt"
touch "$NIGHTMARE_DIR/🔥🚀_ümläute_日本語.txt"

# Broken and circular symlinks
ln -s /nonexistent/path/target "$NIGHTMARE_DIR/broken_symlink"
ln -s loop_b "$NIGHTMARE_DIR/loop_a"
ln -s loop_a "$NIGHTMARE_DIR/loop_b"

# 5,000 files for grid/list sorting & async thumbnail stress
touch "$NIGHTMARE_DIR/mass_files/item_"{0001..5000}".txt"

# Corrupt / zero-byte media & archives
touch "$NIGHTMARE_DIR/empty.png" "$NIGHTMARE_DIR/empty.jpg" "$NIGHTMARE_DIR/empty.zip"
dd if=/dev/urandom of="$NIGHTMARE_DIR/corrupt_image.png" bs=1K count=64 status=none
dd if=/dev/urandom of="$NIGHTMARE_DIR/corrupt.tar.gz" bs=1K count=16 status=none

# Zip-Slip path traversal test archive (requires python3)
if command -v python3 >/dev/null 2>&1; then
    python3 -c '
import zipfile
with zipfile.ZipFile("/tmp/blurred-nightmare/zip_slip_test.zip", "w") as zf:
    zf.writestr("../../blurred_zip_slip_escaped.txt", "traversal test")
    zf.writestr("normal_inside.txt", "safe file")
'
fi

# Read-only trap directory
touch "$NIGHTMARE_DIR/readonly_dir/locked_file.txt"
chmod 555 "$NIGHTMARE_DIR/readonly_dir"

echo "=== [4/4] Launching build/blurred_files directly into $NIGHTMARE_DIR ==="
./build/blurred_files "$NIGHTMARE_DIR" || true

# Cleanup permissions so /tmp can be cleaned normally
chmod -R u+w "$NIGHTMARE_DIR" 2>/dev/null || true
echo "Done. Check /tmp/blurred_zip_slip_escaped.txt to verify Zip-Slip did NOT escape!"