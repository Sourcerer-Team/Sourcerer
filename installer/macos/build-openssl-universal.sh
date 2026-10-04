#!/bin/bash
# Builds a static, universal (arm64 + x86_64) OpenSSL for macOS.
#
# Usage:
#   installer/macos/build-openssl-universal.sh
#
# Environment:
#   OPENSSL_VERSION             OpenSSL version to build (default 3.5.4)
#   OPENSSL_SHA256              Optional SHA-256 of the source tarball
#   MACOSX_DEPLOYMENT_TARGET    Minimum macOS version (default 13.0)
#   OUTPUT_DIR                  Install prefix (default <repo>/build/openssl-universal)
#
# Then configure Sourcerer with:
#   -DOPENSSL_ROOT_DIR=<OUTPUT_DIR> -DSOURCERER_MACOS_UNIVERSAL=ON

set -euo pipefail

OPENSSL_VERSION="${OPENSSL_VERSION:-3.5.4}"
MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$REPO_ROOT/build/openssl-universal}"
WORK_DIR="${WORK_DIR:-$REPO_ROOT/build/openssl-work}"

STAMP="$OUTPUT_DIR/.version"
WANTED="$OPENSSL_VERSION-$MACOSX_DEPLOYMENT_TARGET"
if [[ -f "$STAMP" && "$(cat "$STAMP")" == "$WANTED" ]]; then
    echo "OpenSSL $WANTED already built in $OUTPUT_DIR"
    exit 0
fi

mkdir -p "$WORK_DIR"
TARBALL="$WORK_DIR/openssl-$OPENSSL_VERSION.tar.gz"
if [[ ! -f "$TARBALL" ]]; then
    curl -fL --retry 3 -o "$TARBALL" \
        "https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz"
fi

if [[ -n "${OPENSSL_SHA256:-}" ]]; then
    echo "$OPENSSL_SHA256  $TARBALL" | shasum -a 256 -c -
fi

JOBS="$(sysctl -n hw.ncpu)"
declare -a ARCH_PREFIXES=()

build_arch() {
    local target="$1"   # OpenSSL target
    local name="$2"     # arm64 | x86_64
    local src="$WORK_DIR/src-$name"
    local prefix="$WORK_DIR/install-$name"

    rm -rf "$src" "$prefix"
    mkdir -p "$src"
    tar -xzf "$TARBALL" -C "$src" --strip-components=1

    (
        cd "$src"
        ./Configure "$target" no-shared no-tests no-docs no-apps \
            --prefix="$prefix" --openssldir=/etc/ssl \
            "-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET"
        make -j"$JOBS"
        make install_sw
    )
    ARCH_PREFIXES+=("$prefix")
}

build_arch darwin64-arm64-cc arm64
build_arch darwin64-x86_64-cc x86_64

ARM="$WORK_DIR/install-arm64"
X64="$WORK_DIR/install-x86_64"

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/lib"
cp -R "$ARM/include" "$OUTPUT_DIR/include"

# opensslconf.h is generated per architecture, but the 3.x series keeps the
# arch-specific parts in configuration.h guarded by the same macros, so the
# arm64 copy is sufficient for both. Verify the two copies are equivalent
# apart from build-time details before relying on that.
if ! diff -q <(grep -v -E 'OPENSSL_CONFIGURED_API|Generated on|BUILD_METADATA' "$ARM/include/openssl/configuration.h") \
              <(grep -v -E 'OPENSSL_CONFIGURED_API|Generated on|BUILD_METADATA' "$X64/include/openssl/configuration.h") >/dev/null; then
    echo "warning: configuration.h differs between architectures:" >&2
    diff <(cat "$ARM/include/openssl/configuration.h") <(cat "$X64/include/openssl/configuration.h") >&2 || true
fi

for lib in libssl.a libcrypto.a; do
    lipo -create "$ARM/lib/$lib" "$X64/lib/$lib" -output "$OUTPUT_DIR/lib/$lib"
    lipo -archs "$OUTPUT_DIR/lib/$lib"
done

echo "$WANTED" > "$STAMP"
echo "Universal OpenSSL installed to $OUTPUT_DIR"
