#!/bin/bash
# Stages the svn command-line client for the macOS bundle into
# installer/3rdparty/svn/bin. Sourcerer puts 3rdparty/svn/bin on PATH
# (src/main.cpp), and build-macos.sh signs every Mach-O file in the tree.
#
# CMake builds svn when SOURCERER_BUILD_SVN_CLI is ON, with Subversion, APR,
# serf, SQLite and, for universal builds, OpenSSL (build-openssl-universal.sh)
# linked in statically. A library from outside macOS, such as a Homebrew
# OpenSSL in a local single-architecture build, is copied to svn/lib and
# referenced through @loader_path, so the tree stays relocatable.
#
# Usage: build-svn-macos.sh <path-to-svn>
# Environment:
#   OUTPUT_DIR         where the tree goes (default: installer/3rdparty/svn)
#   REQUIRE_UNIVERSAL  1 to require arm64 and x86_64 in every binary

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$SCRIPT_DIR/svn}"
REQUIRE_UNIVERSAL="${REQUIRE_UNIVERSAL:-0}"

die() {
    echo "build-svn-macos.sh: $*" >&2
    exit 1
}

[[ $# -eq 1 ]] || die "usage: $0 <path-to-svn>"
SVN_BIN="$1"
[[ -x "$SVN_BIN" ]] || die "svn was not built at $SVN_BIN. Configure with -DSOURCERER_BUILD_SVN_CLI=ON"

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/bin" "$OUTPUT_DIR/lib"
cp "$SVN_BIN" "$OUTPUT_DIR/bin/svn"
chmod 755 "$OUTPUT_DIR/bin/svn"

foreign_libs() {
    otool -L "$1" | tail -n +2 | awk '{print $1}' |
        grep -vE '^(/usr/lib/|/System/Library/|@)' || true
}

# Copies each library from outside macOS next to svn and points the
# referencing binary at the copy; repeats for the copies' own dependencies.
bundle_libs() {
    local binary="$1" prefix="$2" dep name
    for dep in $(foreign_libs "$binary"); do
        name="$(basename "$dep")"
        if [[ ! -f "$OUTPUT_DIR/lib/$name" ]]; then
            cp -L "$dep" "$OUTPUT_DIR/lib/$name"
            chmod 644 "$OUTPUT_DIR/lib/$name"
            install_name_tool -id "@loader_path/$name" "$OUTPUT_DIR/lib/$name"
            bundle_libs "$OUTPUT_DIR/lib/$name" "@loader_path"
        fi
        install_name_tool -change "$dep" "$prefix/$name" "$binary"
    done
}
bundle_libs "$OUTPUT_DIR/bin/svn" "@loader_path/../lib"
rmdir "$OUTPUT_DIR/lib" 2>/dev/null || echo "Bundled with svn: $(ls "$OUTPUT_DIR/lib" | tr '\n' ' ')"

while IFS= read -r -d '' binary; do
    archs="$(lipo -archs "$binary")"
    if [[ "$REQUIRE_UNIVERSAL" == "1" && ! ("$archs" == *arm64* && "$archs" == *x86_64*) ]]; then
        die "${binary#"$OUTPUT_DIR"/} is not universal (has: $archs)"
    fi
    [[ -z "$(foreign_libs "$binary")" ]] || die "${binary#"$OUTPUT_DIR"/} still links outside the bundle"
done < <(find "$OUTPUT_DIR" -type f -print0)

"$OUTPUT_DIR/bin/svn" --version --quiet >/dev/null || die "the staged svn does not run"
echo "Bundled svn $("$OUTPUT_DIR/bin/svn" --version --quiet) ($(lipo -archs "$OUTPUT_DIR/bin/svn")) in $OUTPUT_DIR"
