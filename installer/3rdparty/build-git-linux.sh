#!/bin/bash
# Builds a relocatable git for the Linux AppImage into installer/3rdparty/git.
# Sourcerer puts 3rdparty/git/bin on PATH (src/main.cpp).
#
# RUNTIME_PREFIX makes git find libexec/git-core, its templates and etc/
# relative to its own binary, so the tree works from any directory. The
# shared libraries git needs beyond the C runtime (libcurl and its
# dependencies, OpenSSL, zlib, expat, ...) are copied to git/lib, and every
# binary carries an RPATH to that directory. RPATH, unlike RUNPATH, also
# applies to the dependencies of those libraries and wins over the
# LD_LIBRARY_PATH that the AppImage's AppRun sets.
#
# Needs gcc, make, curl and the -dev packages of libcurl, zlib, expat and
# OpenSSL (docker/Dockerfile.linux has them).
#
# Environment:
#   OUTPUT_DIR   where the tree goes (default: installer/3rdparty/git)
#   JOBS         parallel make jobs (default: nproc)

set -euo pipefail

GIT_VERSION="2.56.0"
GIT_SHA256="26c56c296b38c0695b26fa95f475f1d01704d2d38e73465ca30b0b2f5dc789d3"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$SCRIPT_DIR/git}"
JOBS="${JOBS:-$(nproc)}"

die() {
    echo "build-git-linux.sh: $*" >&2
    exit 1
}

archive="$SCRIPT_DIR/git-$GIT_VERSION.tar.xz"
if [[ -f "$archive" && "$(sha256sum "$archive" | cut -d' ' -f1)" == "$GIT_SHA256" ]]; then
    echo "Using cached $(basename "$archive")"
else
    echo "Downloading git $GIT_VERSION"
    curl -fL --retry 3 -o "$archive.part" \
        "https://mirrors.edge.kernel.org/pub/software/scm/git/git-$GIT_VERSION.tar.xz"
    [[ "$(sha256sum "$archive.part" | cut -d' ' -f1)" == "$GIT_SHA256" ]] || {
        rm -f "$archive.part"
        die "checksum mismatch for git-$GIT_VERSION.tar.xz"
    }
    mv "$archive.part" "$archive"
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
tar -xJf "$archive" -C "$work"

# The paths below are relative because of RUNTIME_PREFIX. "\$$ORIGIN" reaches
# the linker as a literal $ORIGIN through both make and the shell.
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
    INSTALL_SYMLINKS=YesPlease
    CFLAGS="-O2"
    LDFLAGS='-Wl,--disable-new-dtags -Wl,-rpath,\$$ORIGIN/../lib -Wl,-rpath,\$$ORIGIN/../../lib'
)
make -C "$work/git-$GIT_VERSION" -j"$JOBS" "${make_args[@]}" all
make -C "$work/git-$GIT_VERSION" "${make_args[@]}" DESTDIR="$work/stage" install

rm -rf "$OUTPUT_DIR"
mkdir -p "$(dirname "$OUTPUT_DIR")"
mv "$work/stage/usr" "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/lib"

# Libraries every Linux system has, from glibc and the compiler runtime.
is_system_lib() {
    case "$(basename "$1")" in
    linux-vdso.so* | ld-linux*.so* | libc.so.* | libm.so.* | libdl.so.* | \
        libpthread.so.* | librt.so.* | libresolv.so.* | libutil.so.* | \
        libgcc_s.so.* | libstdc++.so.*)
        return 0
        ;;
    esac
    return 1
}

while IFS= read -r -d '' binary; do
    [[ "$(head -c 4 "$binary" | tail -c 3)" == "ELF" ]] || continue
    while read -r name arrow path _; do
        [[ "$arrow" == "=>" ]] || continue
        [[ "$path" == "not" ]] && die "$(basename "$binary") needs $name, which was not found"
        is_system_lib "$path" && continue
        [[ -e "$OUTPUT_DIR/lib/$name" ]] || cp -L "$path" "$OUTPUT_DIR/lib/$name"
    done < <(ldd "$binary")
done < <(find "$OUTPUT_DIR/bin" "$OUTPUT_DIR/libexec" -type f -print0)

# Smoke test: git runs, finds its own libexec, and loads the bundled libcurl.
git_bin="$OUTPUT_DIR/bin/git"
"$git_bin" --version
[[ "$("$git_bin" --exec-path)" == "$OUTPUT_DIR/libexec/git-core" ]] ||
    die "git does not resolve its exec path relative to $OUTPUT_DIR"
curl_lib="$(ldd "$OUTPUT_DIR/libexec/git-core/git-remote-https" | awk '/libcurl/ {print $3}')"
[[ -n "$curl_lib" && "$(readlink -f "$curl_lib")" == "$(readlink -f "$OUTPUT_DIR/lib")"/libcurl* ]] ||
    die "git-remote-https does not load the bundled libcurl (loads: ${curl_lib:-none})"
echo "Bundled git $GIT_VERSION in $OUTPUT_DIR ($(ls "$OUTPUT_DIR/lib" | wc -l) libraries)"
