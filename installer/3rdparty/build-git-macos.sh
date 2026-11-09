#!/bin/bash
# Builds a relocatable git for the macOS bundle into installer/3rdparty/git,
# as one universal build when ARCHS names more than one architecture.
# Sourcerer puts 3rdparty/git/bin on PATH (src/main.cpp), and build-macos.sh
# signs every Mach-O file in the tree.
#
# RUNTIME_PREFIX makes git find libexec/git-core, its templates and etc/
# relative to its own binary. git links only libraries of macOS itself
# (libcurl, zlib, expat, iconv and CommonCrypto), which are universal, so one
# compile with several -arch flags gives universal binaries. Homebrew and
# MacPorts are kept out of the build, and the result is checked for it.
#
# Environment:
#   ARCHS                     architectures, e.g. "arm64 x86_64" (default: uname -m)
#   MACOSX_DEPLOYMENT_TARGET  minimum macOS version (default 13.0)
#   OUTPUT_DIR                where the tree goes (default: installer/3rdparty/git)
#   JOBS                      parallel make jobs (default: sysctl hw.ncpu)

set -euo pipefail

GIT_VERSION="2.56.0"
GIT_SHA256="26c56c296b38c0695b26fa95f475f1d01704d2d38e73465ca30b0b2f5dc789d3"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCHS="${ARCHS:-$(uname -m)}"
MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
OUTPUT_DIR="${OUTPUT_DIR:-$SCRIPT_DIR/git}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

die() {
    echo "build-git-macos.sh: $*" >&2
    exit 1
}

archive="$SCRIPT_DIR/git-$GIT_VERSION.tar.xz"
if [[ -f "$archive" && "$(shasum -a 256 "$archive" | cut -d' ' -f1)" == "$GIT_SHA256" ]]; then
    echo "Using cached $(basename "$archive")"
else
    echo "Downloading git $GIT_VERSION"
    curl -fL --retry 3 -o "$archive.part" \
        "https://mirrors.edge.kernel.org/pub/software/scm/git/git-$GIT_VERSION.tar.xz"
    [[ "$(shasum -a 256 "$archive.part" | cut -d' ' -f1)" == "$GIT_SHA256" ]] || {
        rm -f "$archive.part"
        die "checksum mismatch for git-$GIT_VERSION.tar.xz"
    }
    mv "$archive.part" "$archive"
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
tar -xJf "$archive" -C "$work"

arch_flags=""
for arch in $ARCHS; do
    arch_flags="$arch_flags -arch $arch"
done
version_flag="-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET"

# Only the system toolchain and the system curl-config, never Homebrew's.
export PATH="/usr/bin:/bin:/usr/sbin:/sbin"
export MACOSX_DEPLOYMENT_TARGET

# The paths below are relative because of RUNTIME_PREFIX.
make_args=(
    prefix=/usr
    RUNTIME_PREFIX=YesPlease
    gitexecdir=libexec/git-core
    template_dir=share/git-core/templates
    sysconfdir=etc
    NO_GETTEXT=YesPlease
    NO_TCLTK=YesPlease
    NO_PERL=YesPlease
    NO_PYTHON=YesPlease
    NO_RUST=YesPlease
    NO_OPENSSL=YesPlease
    NO_FINK=YesPlease
    NO_DARWIN_PORTS=YesPlease
    INSTALL_SYMLINKS=YesPlease
    CC=clang
    CURL_CONFIG=/usr/bin/curl-config
    CFLAGS="-O2$arch_flags $version_flag"
    LDFLAGS="$arch_flags $version_flag"
)
make -C "$work/git-$GIT_VERSION" -j"$JOBS" "${make_args[@]}" all
make -C "$work/git-$GIT_VERSION" "${make_args[@]}" DESTDIR="$work/stage" install

rm -rf "$OUTPUT_DIR"
mkdir -p "$(dirname "$OUTPUT_DIR")"
mv "$work/stage/usr" "$OUTPUT_DIR"

# Every binary must hold every requested architecture and link only
# libraries of macOS itself.
while IFS= read -r -d '' binary; do
    file -b "$binary" | grep -q "Mach-O" || continue
    archs="$(lipo -archs "$binary")"
    for arch in $ARCHS; do
        [[ " $archs " == *" $arch "* ]] || die "${binary#"$OUTPUT_DIR"/} lacks $arch (has: $archs)"
    done
    foreign="$(otool -L "$binary" | tail -n +2 | awk '{print $1}' |
        grep -vE '^(/usr/lib/|/System/Library/)' || true)"
    [[ -z "$foreign" ]] || die "${binary#"$OUTPUT_DIR"/} links libraries outside macOS: $foreign"
done < <(find "$OUTPUT_DIR/bin" "$OUTPUT_DIR/libexec" -type f -print0)

git_bin="$OUTPUT_DIR/bin/git"
if "$git_bin" --version; then
    [[ "$("$git_bin" --exec-path)" == "$OUTPUT_DIR/libexec/git-core" ]] ||
        die "git does not resolve its exec path relative to $OUTPUT_DIR"
fi
echo "Bundled git $GIT_VERSION ($(lipo -archs "$git_bin")) in $OUTPUT_DIR"
