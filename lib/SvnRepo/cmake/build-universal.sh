#!/bin/sh
# Runs one of the autotools build scripts (build-apr.sh, build-apr-util.sh,
# build-cyrus-sasl.sh) once per macOS architecture and merges the resulting
# shared library with lipo.
#
# Apple's clang refuses to preprocess with several -arch flags, and configure
# preprocesses constantly, so a universal library cannot be built in a single
# configure run. Each architecture gets its own build directory and is
# installed into the same prefix one after the other, so the paths baked into
# the installed files (apr-1-config, .la files) stay valid. Only the library
# itself is merged.
#
# The non-native slice runs configure test programs, which needs Rosetta on
# Apple Silicon (softwareupdate --install-rosetta --agree-to-license).
#
# Usage:
#   build-universal.sh <archs> <deployment-target> <library> <script> <src> <build> <install> [args...]
#
#   archs              comma-separated list (arm64,x86_64), or "native"
#   deployment-target  MACOSX_DEPLOYMENT_TARGET for the build, or "default"
#   library            file name under <install>/lib to merge, e.g. libapr-1.0.dylib
#   script             the build script; it is called as
#                      sh <script> <src> <build>[/<arch>] <install> [args...]
set -e

archs=$1
deployment_target=$2
library=$3
script=$4
src_dir=$5
build_dir=$6
install_dir=$7
shift 7

if [ "$deployment_target" != default ]; then
    MACOSX_DEPLOYMENT_TARGET=$deployment_target
    export MACOSX_DEPLOYMENT_TARGET
fi

if [ "$archs" = native ]; then
    exec sh "$script" "$src_dir" "$build_dir" "$install_dir" "$@"
fi

base_cc=${CC:-cc}
arch_list=$(echo "$archs" | tr ',' ' ')
arch_count=$(echo "$arch_list" | wc -w | tr -d ' ')

if [ "$arch_count" -eq 1 ]; then
    CC="$base_cc -arch $arch_list" exec sh "$script" "$src_dir" "$build_dir" "$install_dir" "$@"
fi

slice_dir="$build_dir/slices"
rm -rf "$slice_dir"
mkdir -p "$slice_dir"

for arch in $arch_list; do
    echo "build-universal.sh: building $library for $arch"
    CC="$base_cc -arch $arch" sh "$script" "$src_dir" "$build_dir/$arch" "$install_dir" "$@"
    mkdir -p "$slice_dir/$arch"
    cp "$install_dir/lib/$library" "$slice_dir/$arch/$library"
done

set --
for arch in $arch_list; do
    set -- "$@" "$slice_dir/$arch/$library"
done
rm -f "$install_dir/lib/$library"
lipo -create "$@" -output "$install_dir/lib/$library"
lipo -info "$install_dir/lib/$library"

# The installed config scripts can point back into the build directory of the
# last architecture (build-apr.sh installs APR's uninstalled apr-1-config, so
# apr-util links <build>/<arch>/libapr-1.la and the dylib next to it). Put the
# universal library in place of every per-architecture copy so that whatever
# links against those copies gets all slices.
for arch in $arch_list; do
    find "$build_dir/$arch" -name "$library" -type f -exec cp "$install_dir/lib/$library" {} \;
done
