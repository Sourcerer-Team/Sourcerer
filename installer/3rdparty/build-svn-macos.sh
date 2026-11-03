#!/bin/bash
# Stages the svn command-line client for the macOS bundle into
# installer/3rdparty/svn/bin. Sourcerer puts 3rdparty/svn/bin on PATH
# (src/main.cpp), and build-macos.sh signs every Mach-O file in the tree.
#
# CMake builds svn when SOURCERER_BUILD_SVN_CLI is ON, linked against the
# shared Subversion, APR, serf, SQLite, ... libraries of the build (@rpath
# references into <build>/lib) and, for universal builds, a static OpenSSL
# (build-openssl-universal.sh). These and any other library from outside
# macOS, such as a Homebrew OpenSSL in a local single-architecture build, are
# copied to svn/lib and referenced through @loader_path, so the tree stays
# relocatable.
#
# Usage: build-svn-macos.sh <path-to-svn>
# Environment:
#   OUTPUT_DIR         where the tree goes (default: installer/3rdparty/svn)
#   SVN_LIB_DIR        the shared libraries of the build (default: <svn dir>/../lib)
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
SVN_LIB_DIR="${SVN_LIB_DIR:-$(dirname "$SVN_BIN")/../lib}"

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/bin" "$OUTPUT_DIR/lib"
cp "$SVN_BIN" "$OUTPUT_DIR/bin/svn"
chmod 755 "$OUTPUT_DIR/bin/svn"

# Libraries from outside macOS and the bundle: absolute paths and the @rpath
# libraries of the build.
foreign_libs() {
    otool -L "$1" | tail -n +2 | awk '{print $1}' |
        grep -vE '^(/usr/lib/|/System/Library/|@loader_path/|@executable_path/)' || true
}

# The file of a library reference.
lib_file() {
    case "$1" in
        @rpath/*) echo "$SVN_LIB_DIR/${1#@rpath/}" ;;
        *) echo "$1" ;;
    esac
}

# Copies each library from outside macOS next to svn and points the
# referencing binary at the copy; repeats for the copies' own dependencies.
bundle_libs() {
    local binary="$1" prefix="$2" dep name file
    for dep in $(foreign_libs "$binary"); do
        name="$(basename "$dep")"
        if [[ ! -f "$OUTPUT_DIR/lib/$name" ]]; then
            file="$(lib_file "$dep")"
            [[ -f "$file" ]] || die "cannot find $dep (looked for $file)"
            cp -L "$file" "$OUTPUT_DIR/lib/$name"
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
    # install_name_tool invalidated the signature, Apple silicon does not run
    # such binaries. build-macos.sh signs them again with the app's identity.
    codesign --force --sign - "$binary"
done < <(find "$OUTPUT_DIR" -type f -print0)

"$OUTPUT_DIR/bin/svn" --version --quiet >/dev/null || die "the staged svn does not run"
echo "Bundled svn $("$OUTPUT_DIR/bin/svn" --version --quiet) ($(lipo -archs "$OUTPUT_DIR/bin/svn")) in $OUTPUT_DIR"
