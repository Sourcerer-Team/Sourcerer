#!/bin/bash
# Builds a universal (arm64 + x86_64) Sourcerer.app and packages it as a .dmg.
#
# Usage:
#   installer/macos/build-macos.sh
#
# Environment:
#   QT_PREFIX             Qt installation for macOS, e.g. ~/Qt/6.10.3/macos (required)
#   MACOSX_DEPLOYMENT_TARGET  Minimum macOS version (default 13.0; must not be
#                         lower than what the Qt version supports)
#   UNIVERSAL             1 (default) = arm64+x86_64 with a static universal OpenSSL,
#                         0 = native architecture only (uses OPENSSL_ROOT_DIR or Homebrew)
#   OPENSSL_ROOT_DIR      Used when UNIVERSAL=0
#   BUILD_DIR             CMake build directory (default <repo>/build/Release-macOS-dist)
#   OUTPUT_DMG            Result (default <repo>/installer/Sourcerer-Installer.dmg)
#
# Signing:
#   CODESIGN_IDENTITY     "-" (default) = ad-hoc signature. A "Developer ID
#                         Application: ..." identity enables the hardened runtime
#                         and secure timestamps.
#   NOTARIZE              1 = submit the DMG to Apple's notary service and staple the
#                         ticket (requires a Developer ID identity and
#                         APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

: "${QT_PREFIX:?QT_PREFIX must point to the Qt macOS kit, e.g. ~/Qt/6.10.3/macos}"
MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
UNIVERSAL="${UNIVERSAL:-1}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build/Release-macOS-dist}"
OUTPUT_DMG="${OUTPUT_DMG:-$REPO_ROOT/installer/Sourcerer-Installer.dmg}"
CODESIGN_IDENTITY="${CODESIGN_IDENTITY:--}"
NOTARIZE="${NOTARIZE:-0}"

VERSION="$(sed -n 's/.*APPLICATION_VERSION *= *"\([^"]*\)".*/\1/p' "$REPO_ROOT/component/ConfigManager/GitVersion.h.in")"
[[ -n "$VERSION" ]] || { echo "Cannot read APPLICATION_VERSION" >&2; exit 1; }

# Prefer CMake/Ninja from PATH, fall back to the Qt tools folder.
QT_TOOLS="$(dirname "$QT_PREFIX")/../Tools"
CMAKE="${CMAKE:-$(command -v cmake || true)}"
NINJA="${NINJA:-$(command -v ninja || true)}"
if [[ -z "$CMAKE" ]]; then CMAKE="$(ls -d "$QT_TOOLS"/CMake/CMake.app/Contents/bin/cmake 2>/dev/null | head -1)"; fi
if [[ -z "$NINJA" ]]; then NINJA="$(ls -d "$QT_TOOLS"/Ninja/ninja 2>/dev/null | head -1)"; fi
[[ -x "$CMAKE" && -x "$NINJA" ]] || { echo "cmake and ninja are required" >&2; exit 1; }

CMAKE_ARGS=(
    -S "$REPO_ROOT/src" -B "$BUILD_DIR" -G Ninja
    -DCMAKE_MAKE_PROGRAM="$NINJA"
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET"
    -DSOURCERER_BUILD_SVN_CLI=ON
    -Wno-dev
)

if [[ "$UNIVERSAL" == "1" ]]; then
    echo "== Building universal OpenSSL"
    MACOSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" "$SCRIPT_DIR/build-openssl-universal.sh"
    CMAKE_ARGS+=(
        -DOPENSSL_ROOT_DIR="$REPO_ROOT/build/openssl-universal"
        -DSOURCERER_MACOS_UNIVERSAL=ON
    )
elif [[ -n "${OPENSSL_ROOT_DIR:-}" ]]; then
    CMAKE_ARGS+=(-DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR")
fi

echo "== Configuring"
"$CMAKE" "${CMAKE_ARGS[@]}"
echo "== Building"
"$CMAKE" --build "$BUILD_DIR" --parallel "$(sysctl -n hw.ncpu)"

DIST_DIR="$BUILD_DIR/dist"
APP="$DIST_DIR/Sourcerer.app"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
ditto "$BUILD_DIR/Sourcerer.app" "$APP"

echo "== Bundling the shared libraries"
# The shared third-party libraries of the build, under the names they are
# referenced by (@rpath/<name>, found through @executable_path/../Frameworks).
# macdeployqt then finds them in the bundle.
mkdir -p "$APP/Contents/Frameworks"
for lib in "$BUILD_DIR"/lib/*.dylib; do
    [[ -L "$lib" ]] && continue
    name="$(basename "$(otool -D "$lib" | tail -n 1)")"
    cp "$lib" "$APP/Contents/Frameworks/$name"
done

echo "== Deploying Qt"
# The launcher is a second executable inside the bundle; without -executable it
# would keep pointing at the Qt installation of the build machine.
"$QT_PREFIX/bin/macdeployqt" "$APP" -executable="$APP/Contents/MacOS/SourcererLauncher"

# Bundle the JWT public key so Finder-launched apps can find it even though the
# current working directory is not the app's executable directory.
if [[ -f "$REPO_ROOT/keys/cloud_jwt_public_key.pem" ]]; then
    mkdir -p "$APP/Contents/Resources/keys"
    cp "$REPO_ROOT/keys/cloud_jwt_public_key.pem" "$APP/Contents/Resources/keys/"
fi

# Portable git with its dylibs, placed next to the executables so the app can
# put it on PATH at startup (see prependBundledGitToPath in src/main.cpp).
GIT_SRC="$REPO_ROOT/installer/3rdparty/git"
if [[ ! -x "$GIT_SRC/bin/git" ]]; then
    if [[ "$UNIVERSAL" == "1" ]]; then GIT_ARCHS="arm64 x86_64"; else GIT_ARCHS="$(uname -m)"; fi
    ARCHS="$GIT_ARCHS" "$REPO_ROOT/installer/3rdparty/build-git-macos.sh"
fi
mkdir -p "$APP/Contents/Resources/3rdparty"
ditto "$GIT_SRC" "$APP/Contents/Resources/3rdparty/git"

# Standalone svn client, built by the same universal CMake build and bundled the same way.
SVN_SRC="$REPO_ROOT/installer/3rdparty/svn"
REQUIRE_UNIVERSAL="$UNIVERSAL" OUTPUT_DIR="$SVN_SRC" \
    "$REPO_ROOT/installer/3rdparty/build-svn-macos.sh" "$BUILD_DIR/SvnRepo/svn"
mkdir -p "$APP/Contents/Resources/3rdparty"
ditto "$SVN_SRC" "$APP/Contents/Resources/3rdparty/svn"

# Portable Mercurial (relocatable CPython plus the Mercurial wheel), per arch and
# merged with lipo for universal builds.
HG_SRC="$REPO_ROOT/installer/3rdparty/hg"
if [[ ! -x "$HG_SRC/bin/hg" ]]; then
    if [[ "$UNIVERSAL" == "1" ]]; then
        HG_TMP="$(mktemp -d)"
        "$REPO_ROOT/installer/3rdparty/fetch-hg-unix.sh" "$HG_TMP/arm64" macos-arm64
        "$REPO_ROOT/installer/3rdparty/fetch-hg-unix.sh" "$HG_TMP/x86_64" macos-x86_64
        "$REPO_ROOT/installer/3rdparty/fetch-hg-unix.sh" --merge-macos "$HG_SRC" "$HG_TMP/arm64" "$HG_TMP/x86_64"
        rm -rf "$HG_TMP"
    elif [[ "$(uname -m)" == "arm64" ]]; then
        "$REPO_ROOT/installer/3rdparty/fetch-hg-unix.sh" "$HG_SRC" macos-arm64
    else
        "$REPO_ROOT/installer/3rdparty/fetch-hg-unix.sh" "$HG_SRC" macos-x86_64
    fi
fi
ditto "$HG_SRC" "$APP/Contents/Resources/3rdparty/hg"
cp "$REPO_ROOT/installer/3rdparty/hg-config/sourcerer.rc" \
    "$APP"/Contents/Resources/3rdparty/hg/python/lib/python3.*/site-packages/mercurial/defaultrc/

echo "== Signing ($CODESIGN_IDENTITY)"
SIGN_ARGS=(--force --sign "$CODESIGN_IDENTITY")
if [[ "$CODESIGN_IDENTITY" != "-" ]]; then
    SIGN_ARGS+=(--options runtime --timestamp)
fi
sign() { codesign "${SIGN_ARGS[@]}" "$@"; }

# Bundled git, svn and hg binaries and dylibs must be signed with the same
# identity as the app. The tools live in Contents/Resources, not Contents/MacOS:
# codesign treats everything in Contents/MacOS as nested code, so it rejects
# unsigned scripts there and takes directories like hg's python/lib/python3.13
# for malformed bundles. In Resources the app's signature seals the scripts and
# data as resources, and only the Mach-O files need signatures of their own.
while IFS= read -r -d '' bin; do
    if file -b "$bin" | grep -q "Mach-O"; then
        sign ${ENTITLEMENTS_ARGS[@]+"${ENTITLEMENTS_ARGS[@]}"} "$bin"
    fi
done < <(find "$APP/Contents/Resources/3rdparty/git" "$APP/Contents/Resources/3rdparty/svn" "$APP/Contents/Resources/3rdparty/hg" -type f -print0)

# Inside-out: loose libraries, then nested bundles (deepest first), then the
# executables and finally the application bundle itself. --deep is avoided on
# purpose, it signs in an unspecified order and cannot apply entitlements
# selectively.
while IFS= read -r -d '' file; do
    sign "$file"
done < <(find "$APP/Contents" -type f \( -name '*.dylib' -o -name '*.so' \) -print0)

while IFS= read -r bundle; do
    if [[ "$CODESIGN_IDENTITY" != "-" && "$bundle" == *.app ]]; then
        sign --entitlements "$SCRIPT_DIR/entitlements.plist" "$bundle"
    else
        sign "$bundle"
    fi
done < <(find "$APP/Contents" -type d \( -name '*.framework' -o -name '*.app' \) \
            | awk '{ print length($0) "\t" $0 }' | sort -rn | cut -f2-)

ENTITLEMENTS_ARGS=()
if [[ "$CODESIGN_IDENTITY" != "-" ]]; then
    ENTITLEMENTS_ARGS=(--entitlements "$SCRIPT_DIR/entitlements.plist")
fi
# (bash 3.2, the macOS default, treats expanding an empty array as an error
# under `set -u`, hence the ${arr[@]+...} form.)
sign ${ENTITLEMENTS_ARGS[@]+"${ENTITLEMENTS_ARGS[@]}"} "$APP/Contents/MacOS/SourcererLauncher"
sign ${ENTITLEMENTS_ARGS[@]+"${ENTITLEMENTS_ARGS[@]}"} "$APP/Contents/MacOS/Sourcerer"
sign ${ENTITLEMENTS_ARGS[@]+"${ENTITLEMENTS_ARGS[@]}"} "$APP"

echo "== Verifying"
codesign --verify --deep --strict --verbose=2 "$APP"
for exe in Sourcerer SourcererLauncher; do
    archs="$(lipo -archs "$APP/Contents/MacOS/$exe")"
    echo "$exe: $archs"
    if [[ "$UNIVERSAL" == "1" ]]; then
        [[ "$archs" == *arm64* && "$archs" == *x86_64* ]] || { echo "$exe is not universal" >&2; exit 1; }
    fi
done
if [[ "$UNIVERSAL" == "1" ]]; then
    git_archs="$(lipo -archs "$APP/Contents/Resources/3rdparty/git/bin/git")"
    echo "bundled git: $git_archs"
    [[ "$git_archs" == *arm64* && "$git_archs" == *x86_64* ]] || { echo "bundled git is not universal" >&2; exit 1; }
    svn_archs="$(lipo -archs "$APP/Contents/Resources/3rdparty/svn/bin/svn")"
    echo "bundled svn: $svn_archs"
    [[ "$svn_archs" == *arm64* && "$svn_archs" == *x86_64* ]] || { echo "bundled svn is not universal" >&2; exit 1; }
    hg_archs="$(lipo -archs "$APP/Contents/Resources/3rdparty/hg/python/bin/python3")"
    echo "bundled hg python: $hg_archs"
    [[ "$hg_archs" == *arm64* && "$hg_archs" == *x86_64* ]] || { echo "bundled hg python is not universal" >&2; exit 1; }
fi
BUNDLE_VERSION="$(plutil -extract CFBundleShortVersionString raw -o - "$APP/Contents/Info.plist")"
[[ "$BUNDLE_VERSION" == "$VERSION" ]] || { echo "Bundle version $BUNDLE_VERSION != $VERSION" >&2; exit 1; }
# Nothing may reference the build machine.
if otool -L "$APP/Contents/MacOS/Sourcerer" "$APP/Contents/MacOS/SourcererLauncher" \
        | grep -E "^[[:space:]]+(/opt/homebrew/|/usr/local/|/Users/|.*/Qt/)"; then
    echo "Executables reference libraries outside of the bundle" >&2
    exit 1
fi

# Smoke test: SourcererLauncher in helper mode only needs QtCore, so it proves
# that every architecture slice loads the bundled frameworks (the x86_64 slice
# runs under Rosetta on Apple Silicon). Exit code 64 = "bad arguments".
for arch in arm64 x86_64; do
    if ! lipo -archs "$APP/Contents/MacOS/SourcererLauncher" | grep -qw "$arch"; then
        continue
    fi
    set +e
    arch "-$arch" "$APP/Contents/MacOS/SourcererLauncher" --apply-update >/dev/null 2>&1
    status=$?
    set -e
    if [[ $status -eq 64 ]]; then
        echo "smoke test ($arch): ok"
    elif [[ "$arch" == "x86_64" && "$(uname -m)" == "arm64" && $status -eq 86 ]]; then
        echo "smoke test ($arch): skipped (Rosetta is not installed)"
    else
        echo "smoke test ($arch) failed with exit code $status" >&2
        exit 1
    fi
done

echo "== Creating $OUTPUT_DMG"
STAGE="$DIST_DIR/dmg"
rm -rf "$STAGE"
mkdir -p "$STAGE"
ditto "$APP" "$STAGE/Sourcerer.app"
ln -s /Applications "$STAGE/Applications"
rm -f "$OUTPUT_DMG"
hdiutil create -volname "Sourcerer" -srcfolder "$STAGE" -format UDZO -imagekey zlib-level=9 -ov "$OUTPUT_DMG"

if [[ "$CODESIGN_IDENTITY" != "-" ]]; then
    codesign --force --sign "$CODESIGN_IDENTITY" --timestamp "$OUTPUT_DMG"
fi

if [[ "$NOTARIZE" == "1" ]]; then
    : "${APPLE_ID:?}" "${APPLE_TEAM_ID:?}" "${APPLE_APP_PASSWORD:?}"
    [[ "$CODESIGN_IDENTITY" != "-" ]] || { echo "Notarization needs a Developer ID identity" >&2; exit 1; }
    echo "== Notarizing"
    xcrun notarytool submit "$OUTPUT_DMG" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
        --password "$APPLE_APP_PASSWORD" --wait
    xcrun stapler staple "$OUTPUT_DMG"
    xcrun stapler validate "$OUTPUT_DMG"
fi

echo "== Done: $OUTPUT_DMG (version $VERSION)"
ls -la "$OUTPUT_DMG"
