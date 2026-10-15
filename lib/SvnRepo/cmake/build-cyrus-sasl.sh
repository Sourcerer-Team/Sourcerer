#!/bin/sh
set -e

src_dir=$1
build_dir=$2
install_dir=$3

mkdir -p "$build_dir"
mkdir -p "$install_dir/include"
mkdir -p "$install_dir/lib"
mkdir -p "$install_dir/bin"

command -v autoreconf >/dev/null 2>&1 || {
    echo "autoreconf is required to build cyrus-sasl" >&2
    exit 1
}
command -v make >/dev/null 2>&1 || {
    echo "make is required to build cyrus-sasl" >&2
    exit 1
}

cd "$src_dir"
autoreconf --force --install --verbose -Wno-portability

cd "$build_dir"
sh "$src_dir/configure" --disable-shared --enable-static --prefix="$install_dir"

make -C include install prefix="$install_dir"
make -C common
make -C lib install prefix="$install_dir"
