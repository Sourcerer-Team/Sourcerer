#!/bin/bash
# Stages the svn command-line client for the Linux AppImage into
# installer/3rdparty/svn/bin. Sourcerer puts 3rdparty/svn/bin on PATH
# (src/main.cpp).
#
# CMake builds svn when SOURCERER_BUILD_SVN_CLI is ON (lib/SvnRepo/CMakeLists.txt),
# linked against the shared Subversion, APR, serf, SQLite, ... libraries of the
# build (build/Release-Linux/lib). The ones svn needs are copied to svn/lib and
# found through an $ORIGIN RPATH, so the tree stays relocatable. Its remaining
# shared libraries are OpenSSL, which build-appimage.sh puts in usr/lib and
# AppRun adds to LD_LIBRARY_PATH, and libraries every Linux system has. The
# Windows package does the same with svn.exe next to its DLLs.
#
# Usage: build-svn-linux.sh [path-to-svn]
#   default: build/Release-Linux/SvnRepo/svn
# Environment:
#   OUTPUT_DIR   where the tree goes (default: installer/3rdparty/svn)
#   SVN_LIB_DIR  the shared libraries of the build (default: <svn dir>/../lib)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SVN_BIN="${1:-$PROJECT_ROOT/build/Release-Linux/SvnRepo/svn}"
OUTPUT_DIR="${OUTPUT_DIR:-$SCRIPT_DIR/svn}"
SVN_LIB_DIR="${SVN_LIB_DIR:-$(dirname "$SVN_BIN")/../lib}"

die() {
    echo "build-svn-linux.sh: $*" >&2
    exit 1
}

[[ -x "$SVN_BIN" ]] || die "svn was not built at $SVN_BIN. Configure with -DSOURCERER_BUILD_SVN_CLI=ON"
[[ -d "$SVN_LIB_DIR" ]] || die "the shared libraries of the build are not at $SVN_LIB_DIR"
command -v patchelf >/dev/null || die "patchelf is required"

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/bin" "$OUTPUT_DIR/lib"
cp "$SVN_BIN" "$OUTPUT_DIR/bin/svn"
chmod 755 "$OUTPUT_DIR/bin/svn"

needed_libs() {
    objdump -p "$1" | awk '$1 == "NEEDED" { print $2 }'
}

# Copies the libraries of the build that a binary needs to svn/lib, and
# theirs. System libraries are looked up as usual.
bundle_libs() {
    local binary="$1" dep
    for dep in $(needed_libs "$binary"); do
        if [[ -e "$SVN_LIB_DIR/$dep" && ! -e "$OUTPUT_DIR/lib/$dep" ]]; then
            cp -L "$SVN_LIB_DIR/$dep" "$OUTPUT_DIR/lib/$dep"
            chmod 644 "$OUTPUT_DIR/lib/$dep"
            patchelf --set-rpath '$ORIGIN' "$OUTPUT_DIR/lib/$dep"
            bundle_libs "$OUTPUT_DIR/lib/$dep"
        fi
    done
}
bundle_libs "$OUTPUT_DIR/bin/svn"
patchelf --set-rpath '$ORIGIN/../lib' "$OUTPUT_DIR/bin/svn"
echo "Bundled with svn: $(ls "$OUTPUT_DIR/lib" | tr '\n' ' ')"

missing="$(LD_LIBRARY_PATH= ldd "$OUTPUT_DIR/bin/svn" | grep "not found" || true)"
[[ -z "$missing" ]] || die "svn has unresolved libraries: $missing"

"$OUTPUT_DIR/bin/svn" --version --quiet >/dev/null || die "the staged svn does not run"
echo "Bundled svn $("$OUTPUT_DIR/bin/svn" --version --quiet) in $OUTPUT_DIR"
