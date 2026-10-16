#!/bin/bash
# End-to-end test of the macOS updater (SourcererLauncher + MacUpdater).
#
# It installs a copy of a built Sourcerer.app, stages a fake "version 99.0.0"
# update DMG exactly where the application would stage a real one, runs the
# launcher of the installed copy and checks the result. Must be run from a
# normal Terminal (needs hdiutil and a window server).
#
# Usage:
#   installer/macos/test-updater.sh [--elevated] [--tamper] [path/to/Sourcerer.app]
#
#   (default)   bundle in a user-writable folder    -> unprivileged swap
#   --elevated  bundle in /Applications, root-owned -> administrator prompt
#               (uses sudo to set the test up and to clean it up)
#   --tamper    DMG whose app fails signature check -> update must be rejected
#
# The staged DMG is placed in ~/Library/Application Support/Sourcerer/updates,
# which is where the real application stages updates; it is removed afterwards.

set -euo pipefail

ELEVATED=0
TAMPER=0
APP=""
for arg in "$@"; do
    case "$arg" in
    --elevated) ELEVATED=1 ;;
    --tamper) TAMPER=1 ;;
    *) APP="$arg" ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
APP="${APP:-$REPO_ROOT/build/Release-macOS/Sourcerer.app}"
[[ -d "$APP" ]] || { echo "No application bundle at $APP" >&2; exit 2; }

WORK="$REPO_ROOT/build/updater-e2e"
STAGING="$HOME/Library/Application Support/Sourcerer/updates"
RAN="$WORK/ran.txt"
NEW_VERSION="99.0.0"

rm -rf "$WORK"
mkdir -p "$WORK/new" "$STAGING"

if [[ $ELEVATED -eq 1 ]]; then
    INSTALLED="/Applications/Sourcerer.app"
    if [[ -e "$INSTALLED" ]]; then
        echo "$INSTALLED already exists; refusing to overwrite it." >&2
        exit 2
    fi
else
    mkdir -p "$WORK/installed"
    INSTALLED="$WORK/installed/Sourcerer.app"
fi

cleanup() {
    rm -f "$STAGING"/Sourcerer-Update-e2e.dmg
    if [[ $ELEVATED -eq 1 ]]; then
        sudo rm -rf "/Applications/Sourcerer.app" "/Applications/.Sourcerer.app.new" "/Applications/.Sourcerer.app.old"
    fi
}
trap cleanup EXIT

echo "== Installing the application under test: $INSTALLED"
if [[ $ELEVATED -eq 1 ]]; then
    sudo ditto "$APP" "$INSTALLED"
    sudo chown -R root:wheel "$INSTALLED"
else
    ditto "$APP" "$INSTALLED"
fi
INSTALLED_VERSION="$(plutil -extract CFBundleShortVersionString raw -o - "$INSTALLED/Contents/Info.plist")"

# A stand-in for the main executable that only records how it was started, so
# that the test does not open the real GUI. The real launcher stays in place.
echo "== Building the fake executable"
cat > "$WORK/fake.c" <<EOF
#include <stdio.h>
int main(int c, char **v) {
  FILE *f = fopen("$RAN", "a");
  if (f) { fprintf(f, "%s\n", v[0]); for (int i = 1; i < c; i++) fprintf(f, "  arg %s\n", v[i]); fclose(f); }
  return 0;
}
EOF
clang -arch arm64 -arch x86_64 -mmacosx-version-min=13.0 "$WORK/fake.c" -o "$WORK/fake"

if [[ $ELEVATED -eq 1 ]]; then
    sudo cp "$WORK/fake" "$INSTALLED/Contents/MacOS/Sourcerer"
    sudo codesign --force --deep -s - "$INSTALLED"
else
    cp "$WORK/fake" "$INSTALLED/Contents/MacOS/Sourcerer"
    codesign --force --deep -s - "$INSTALLED"
fi

echo "== Building the fake update $NEW_VERSION"
NEW_APP="$WORK/new/Sourcerer.app"
mkdir -p "$NEW_APP/Contents/MacOS"
cp "$WORK/fake" "$NEW_APP/Contents/MacOS/Sourcerer"
cp "$WORK/fake" "$NEW_APP/Contents/MacOS/SourcererLauncher"
cat > "$NEW_APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>www.sourcererapp.com</string>
<key>CFBundleExecutable</key><string>Sourcerer</string>
<key>CFBundleShortVersionString</key><string>$NEW_VERSION</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
</dict></plist>
EOF
codesign --force --deep -s - "$NEW_APP"
if [[ $TAMPER -eq 1 ]]; then
    echo "garbage" >> "$NEW_APP/Contents/MacOS/Sourcerer"
fi

hdiutil create -quiet -volname Sourcerer -srcfolder "$WORK/new" -format UDZO -ov "$WORK/update.dmg"
cp "$WORK/update.dmg" "$STAGING/Sourcerer-Update-e2e.dmg"

echo "== Running the launcher of the installed application (installed version $INSTALLED_VERSION)"
"$INSTALLED/Contents/MacOS/SourcererLauncher" || true

# The launcher starts the application through LaunchServices; give it a moment.
for _ in 1 2 3 4 5 6 7 8 9 10; do
    [[ -f "$RAN" ]] && break
    sleep 1
done

RESULT_VERSION="$(plutil -extract CFBundleShortVersionString raw -o - "$INSTALLED/Contents/Info.plist")"
echo "== Version after update: $RESULT_VERSION"
echo "== Application was started with:"
cat "$RAN" 2>/dev/null || echo "(it was not started)"

fail() { echo "FAIL: $1"; exit 1; }

if [[ $TAMPER -eq 1 ]]; then
    [[ "$RESULT_VERSION" == "$INSTALLED_VERSION" ]] || fail "a tampered update was installed"
    grep -q -- "--skip-update" "$RAN" || fail "the application was not started with --skip-update"
    [[ ! -e "$STAGING/Sourcerer-Update-e2e.dmg" ]] || fail "the rejected update was not discarded"
else
    [[ "$RESULT_VERSION" == "$NEW_VERSION" ]] || fail "the update was not installed"
    codesign --verify --deep --strict "$INSTALLED" || fail "installed bundle has an invalid signature"
    [[ ! -e "$(dirname "$INSTALLED")/.Sourcerer.app.old" ]] || fail "old bundle was left behind"
    [[ ! -e "$(dirname "$INSTALLED")/.Sourcerer.app.new" ]] || fail "temporary bundle was left behind"
    [[ ! -e "$STAGING/Sourcerer-Update-e2e.dmg" ]] || fail "the staged update was not removed"
    if [[ $ELEVATED -eq 1 ]]; then
        [[ "$(stat -f %Su "$INSTALLED")" == "root" ]] || fail "installed bundle is no longer owned by root"
    fi
fi
echo "PASS"
