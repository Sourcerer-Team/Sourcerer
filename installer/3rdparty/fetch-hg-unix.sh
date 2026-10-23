#!/bin/bash
# Portable Mercurial for the Linux AppImage and the macOS bundle: a relocatable
# CPython from python-build-standalone with the Mercurial wheel unpacked into
# its site-packages, and bin/hg, a shell launcher that runs
# "python/bin/python3 -I -m mercurial". The Windows package bundles the same
# Mercurial and Python versions, with hg-launcher/hg.c as its launcher.
#
# Usage:
#   fetch-hg-unix.sh <output-dir> linux|macos-arm64|macos-x86_64
#   fetch-hg-unix.sh --merge-macos <output-dir> <arm64-dir> <x86_64-dir>
#
# Downloads are checked against pinned SHA-256 sums and cached next to this
# script. Only <output-dir>/python and <output-dir>/bin are replaced.

set -euo pipefail

PYTHON_RELEASE="20261001"
PYTHON_VERSION="3.13.16"
PYTHON_MINOR="3.13"
HG_VERSION="7.2.4"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CACHE_DIR="$SCRIPT_DIR"

die() {
    echo "fetch-hg-unix.sh: $*" >&2
    exit 1
}

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    else
        shasum -a 256 "$1" | cut -d' ' -f1
    fi
}

# Downloads <url> to the cache as <file> unless a copy with <sha256> is there.
fetch() {
    local url="$1" file="$2" sha256="$3"
    local path="$CACHE_DIR/$file"
    if [[ -f "$path" && "$(sha256_of "$path")" == "$sha256" ]]; then
        echo "Using cached $file" >&2
    else
        echo "Downloading $file" >&2
        curl -fL --retry 3 -o "$path.part" "$url"
        [[ "$(sha256_of "$path.part")" == "$sha256" ]] || {
            rm -f "$path.part"
            die "checksum mismatch for $file"
        }
        mv "$path.part" "$path"
    fi
    echo "$path"
}

write_launcher() {
    local out="$1"
    mkdir -p "$out/bin"
    cat >"$out/bin/hg" <<'EOF'
#!/bin/sh
# Relocatable hg: runs Mercurial on the bundled Python next to this directory.
# -I keeps PYTHONHOME, PYTHONPATH and the user site-packages out of it.
here=$(cd "$(dirname "$0")" && pwd)
exec "$here/../python/bin/python3" -I -m mercurial "$@"
EOF
    chmod 755 "$out/bin/hg"
}

fetch_platform() {
    local out="$1" platform="$2"
    local triple python_sha wheel wheel_url wheel_sha
    case "$platform" in
    linux)
        triple="x86_64-unknown-linux-gnu"
        python_sha="b55f90dfb060e998a47e4a811dae46aaa27b923244044bbcf88de5e57a5fa4c7"
        wheel="mercurial-$HG_VERSION-cp313-cp313-manylinux_2_17_x86_64.manylinux2014_x86_64.manylinux_2_28_x86_64.whl"
        wheel_url="https://files.pythonhosted.org/packages/7a/8c/09072956ebd5ea250d43818ac2c688deb96939b5ed16beac99e07764c5a4/$wheel"
        wheel_sha="75a2277a5691d28fa2a5a791e033f0c2b72a09b9ac9b83ebd8d3dd736c114729"
        ;;
    macos-arm64 | macos-x86_64)
        if [[ "$platform" == "macos-arm64" ]]; then
            triple="aarch64-apple-darwin"
            python_sha="ca8aa2715d2c89a213b6fbf85f76ae0be7e8e8796f9c61f770d8154caf53bcec"
        else
            triple="x86_64-apple-darwin"
            python_sha="c8aa409e0c26a49d14ef64bf5fcddd79e003750481f44aab3517c55b467932d1"
        fi
        # One universal2 wheel serves both architectures.
        wheel="mercurial-$HG_VERSION-cp313-cp313-macosx_10_13_universal2.whl"
        wheel_url="https://files.pythonhosted.org/packages/13/a2/9c409c04f2b26324369c62d47cc4eb56eb4c53bdf041ddb0484a97beb830/$wheel"
        wheel_sha="1600b1a10358716c4a18c4c428003a1ab8b8da01148b100d70ae3e7ba744eca3"
        ;;
    *)
        die "unknown platform '$platform' (expected linux, macos-arm64 or macos-x86_64)"
        ;;
    esac

    local python_file="cpython-$PYTHON_VERSION+$PYTHON_RELEASE-$triple-install_only.tar.gz"
    local python_url="https://github.com/astral-sh/python-build-standalone/releases/download/$PYTHON_RELEASE/${python_file//+/%2B}"
    local python_archive wheel_archive
    python_archive="$(fetch "$python_url" "$python_file" "$python_sha")"
    wheel_archive="$(fetch "$wheel_url" "$wheel" "$wheel_sha")"

    mkdir -p "$out"
    rm -rf "$out/python" "$out/bin"
    # The archive holds a single top-level "python" directory.
    tar -xzf "$python_archive" -C "$out"
    [[ -x "$out/python/bin/python3" ]] || die "no python/bin/python3 in $python_file"

    # A wheel is a zip that is installed by unpacking it into site-packages,
    # the same way the Windows package does. unzip is used when present
    # because a macOS host may not be able to run the other architecture.
    local site_packages="$out/python/lib/python$PYTHON_MINOR/site-packages"
    if command -v unzip >/dev/null 2>&1; then
        unzip -q -o "$wheel_archive" -d "$site_packages"
    else
        "$out/python/bin/python3" -I -m zipfile -e "$wheel_archive" "$site_packages"
    fi
    [[ -f "$site_packages/mercurial/__main__.py" ]] || die "Mercurial not found in $wheel"

    write_launcher "$out"

    # Smoke test when this host can run the result.
    if "$out/python/bin/python3" -I -c '' 2>/dev/null; then
        local version
        version="$("$out/bin/hg" version -T '{ver}')" || die "bundled hg does not run"
        [[ "$version" == "$HG_VERSION" ]] || die "bundled hg reports $version, expected $HG_VERSION"
        echo "Bundled hg $version ($platform) in $out" >&2
    else
        echo "Bundled hg ($platform) in $out, not run on this host" >&2
    fi
}

is_mach_o() {
    file -b "$1" | grep -q "Mach-O"
}

# Merges an arm64 and an x86_64 tree into one universal tree: thin Mach-O files
# are joined with lipo, files that are already universal (the Mercurial wheel)
# and all other files are copied from the arm64 tree.
merge_macos() {
    local out="$1" arm="$2" intel="$3"
    [[ -x "$arm/python/bin/python3" ]] || die "$arm is not an arm64 Mercurial tree"
    [[ -x "$intel/python/bin/python3" ]] || die "$intel is not an x86_64 Mercurial tree"

    mkdir -p "$out"
    rm -rf "$out/python" "$out/bin"
    cp -a "$arm/python" "$out/python"

    local rel archs
    while IFS= read -r -d '' file; do
        rel="${file#"$arm"/}"
        is_mach_o "$file" || continue
        archs="$(lipo -archs "$file")"
        [[ "$archs" == *x86_64* ]] && continue
        [[ -f "$intel/$rel" ]] || die "no x86_64 counterpart for $rel"
        lipo -create "$file" "$intel/$rel" -output "$out/$rel"
    done < <(find "$arm/python" -type f -print0)

    # Files that exist only in the x86_64 tree.
    while IFS= read -r -d '' file; do
        rel="${file#"$intel"/}"
        if [[ ! -e "$out/$rel" && ! -L "$out/$rel" ]]; then
            mkdir -p "$(dirname "$out/$rel")"
            cp -a "$file" "$out/$rel"
        fi
    done < <(find "$intel/python" \( -type f -o -type l \) -print0)

    write_launcher "$out"
    echo "Universal bundled hg: $(lipo -archs "$out/python/bin/python3") in $out" >&2
}

if [[ "${1:-}" == "--merge-macos" ]]; then
    [[ $# -eq 4 ]] || die "usage: $0 --merge-macos <output-dir> <arm64-dir> <x86_64-dir>"
    merge_macos "$2" "$3" "$4"
else
    [[ $# -eq 2 ]] || die "usage: $0 <output-dir> linux|macos-arm64|macos-x86_64"
    fetch_platform "$1" "$2"
fi
