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

work_dir="$build_dir/src"
rm -rf "$work_dir"
mkdir -p "$work_dir"
cp -R "$src_dir/." "$work_dir/"
find "$work_dir" -type f \( -name 'configure.*' -o -name '*.in' -o -name '*.am' -o -name '*.m4' -o -name '*.inc' -o -name '*.mk' -o -name '*.sh' \) -exec sed -i 's/\r$//' {} +

cd "$work_dir"
autoreconf --force --install --verbose -Wno-portability

cd "$build_dir"
sh "$work_dir/configure" --disable-shared --enable-static --prefix="$install_dir"

make -C include install prefix="$install_dir"
make -C common
make -C lib install prefix="$install_dir"
