#!/bin/bash
# Stages the svn command-line client for the Linux AppImage into
# installer/3rdparty/svn/bin. Sourcerer puts 3rdparty/svn/bin on PATH
# (src/main.cpp).
#
# CMake builds svn when SOURCERER_BUILD_SVN_CLI is ON (lib/SvnRepo/CMakeLists.txt),
# with Subversion, APR, serf and SQLite linked in statically. Its remaining
# shared libraries are OpenSSL, which build-appimage.sh puts in usr/lib and
# AppRun adds to LD_LIBRARY_PATH, and libraries every Linux system has. The
# Windows package does the same with svn.exe next to its OpenSSL DLLs.
#
# Usage: build-svn-linux.sh [path-to-svn]
#   default: build/Release-Linux/SvnRepo/svn
# Environment:
#   OUTPUT_DIR   where the tree goes (default: installer/3rdparty/svn)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SVN_BIN="${1:-$PROJECT_ROOT/build/Release-Linux/SvnRepo/svn}"
OUTPUT_DIR="${OUTPUT_DIR:-$SCRIPT_DIR/svn}"

die() {
    echo "build-svn-linux.sh: $*" >&2
    exit 1
}

[[ -x "$SVN_BIN" ]] || die "svn was not built at $SVN_BIN. Configure with -DSOURCERER_BUILD_SVN_CLI=ON"

missing="$(ldd "$SVN_BIN" | grep "not found" || true)"
[[ -z "$missing" ]] || die "svn has unresolved libraries: $missing"

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/bin"
cp "$SVN_BIN" "$OUTPUT_DIR/bin/svn"
chmod 755 "$OUTPUT_DIR/bin/svn"

"$OUTPUT_DIR/bin/svn" --version --quiet >/dev/null || die "the staged svn does not run"
echo "Bundled svn $("$OUTPUT_DIR/bin/svn" --version --quiet) in $OUTPUT_DIR"
