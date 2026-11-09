#!/bin/sh
set -e

src_dir=$1
build_dir=$2
install_dir=$3
apr_install_dir=$4
apr_src_dir=$5
expat_src_dir=$6
expat_build_dir=$7

mkdir -p "$build_dir"
mkdir -p "$install_dir/include"
mkdir -p "$install_dir/lib"
mkdir -p "$install_dir/bin"
mkdir -p "$install_dir/build"

work_dir="$build_dir/src"
rm -rf "$work_dir"
mkdir -p "$work_dir"
cp -R "$src_dir/." "$work_dir/"
find "$work_dir" -type f \( -name buildconf -o -name 'configure.*' -o -name '*.in' -o -name '*.am' -o -name '*.m4' -o -name '*.inc' -o -name '*.mk' -o -name '*.sh' -o -name '*.hw' -o -name '*.hwc' -o -name config.layout \) -exec perl -pi -e 's/\r$//' {} +
# The sources may be checked out without executable bits (e.g. from Windows),
# but buildconf runs helper scripts such as build/buildcheck.sh directly.
find "$work_dir" -type f -exec sh -c 'for f; do head -c 2 "$f" | grep -q "^#!" && chmod +x "$f"; done; true' sh {} +

apr_build_copy="$build_dir/apr-src"
rm -rf "$apr_build_copy"
mkdir -p "$apr_build_copy"
cp -R "$apr_src_dir/build" "$apr_build_copy/"
find "$apr_build_copy/build" -type f -exec perl -pi -e 's/\r$//' {} +
find "$apr_build_copy/build" -type f -exec sh -c 'for f; do head -c 2 "$f" | grep -q "^#!" && chmod +x "$f"; done; true' sh {} +

cd "$work_dir"
sh ./buildconf --with-apr="$apr_build_copy"
cd "$build_dir"
CPPFLAGS="${CPPFLAGS} -I${expat_src_dir}/lib"
LDFLAGS="${LDFLAGS} -L${expat_build_dir}"
export CPPFLAGS
export LDFLAGS
sh "$work_dir/configure" --enable-shared --disable-static --prefix="$install_dir" --with-apr="$apr_install_dir"
perl -pi -e "s|^INCLUDES = |INCLUDES = -I${expat_src_dir}/lib |" "$build_dir/Makefile"
perl -pi -e "s|^APRUTIL_EXPORT_INCLUDES=\"\"|APRUTIL_EXPORT_INCLUDES=\"-I${expat_src_dir}/lib\"|" "$build_dir/export_vars.sh"

cat > "$build_dir/libtool" <<'EOF'
#!/bin/sh
# Minimal libtool replacement. Libraries that are installed (linked with
# -rpath) become shared libraries named like libtool names them, with the
# static archive next to them; everything else only gets the archive.
set -e
mode=
while [ $# -gt 0 ]; do
    case "$1" in
        --silent|--quiet)
            shift
            ;;
        --tag=*)
            shift
            ;;
        --mode=*)
            mode=${1#--mode=}
            shift
            ;;
        *)
            break
            ;;
    esac
done

case "$mode" in
    compile)
        compiler=$1
        shift
        out=
        args=
        while [ $# -gt 0 ]; do
            case "$1" in
                -o)
                    out=$2
                    shift 2
                    ;;
                *)
                    args="$args '$1'"
                    shift
                    ;;
            esac
        done
        out_o=${out%.lo}.o
        eval set -- "$args"
        "$compiler" "$@" -fPIC -o "$out_o"
        touch "$out"
        ;;
    link)
        linker=$1
        shift
        out=
        objs=
        libs=
        rpath=
        version=0:0:0
        while [ $# -gt 0 ]; do
            case "$1" in
                -o)
                    out=$2
                    shift 2
                    ;;
                -rpath)
                    rpath=$2
                    shift 2
                    ;;
                -release)
                    shift 2
                    ;;
                -version-info)
                    version=$2
                    shift 2
                    ;;
                *.lo)
                    objs="$objs ${1%.lo}.o"
                    shift
                    ;;
                *.la)
                    # A library it depends on: link its shared library, or
                    # its archive when it has none.
                    dep=$(sed -n "s/^dlname='\(.*\)'$/\1/p" "$1")
                    [ -n "$dep" ] || dep=$(sed -n "s/^old_library='\(.*\)'$/\1/p" "$1")
                    libs="$libs $(dirname "$1")/$dep"
                    shift
                    ;;
                -l*|-L*|-pthread|-Wl,*)
                    libs="$libs $1"
                    shift
                    ;;
                -framework)
                    libs="$libs $1 $2"
                    shift 2
                    ;;
                *)
                    shift
                    ;;
            esac
        done
        base=${out%.la}
        archive=${base}.a
        eval set -- $objs
        ar cru "$archive" "$@"
        ranlib "$archive"
        dlname=
        if [ "$base" != "$out" ] && [ -n "$rpath" ]; then
            name=$(basename "$base")
            current=${version%%:*}
            revision=${version#*:}
            revision=${revision%%:*}
            age=${version##*:}
            major=$((current - age))
            if [ "$(uname -s)" = Darwin ]; then
                dlname=$name.$major.dylib
                "$linker" -dynamiclib -o "$(dirname "$out")/$dlname" "$@" $libs \
                    -install_name "@rpath/$dlname" \
                    -compatibility_version $((current + 1)) \
                    -current_version $((current + 1)).$revision \
                    -Wl,-rpath,@loader_path
            else
                dlname=$name.so.$major
                "$linker" -shared -o "$(dirname "$out")/$dlname" "$@" $libs \
                    -Wl,-soname,"$dlname" -Wl,-rpath,'$ORIGIN'
            fi
        fi
        cat > "$out" <<LA
# libtool archive generated by custom wrapper
dlname='$dlname'
library_names='$dlname'
old_library='$(basename "$archive")'
installed=no
shouldnotlink=no
LA
        ;;
    install)
        shift
        if [ $# -gt 0 ] && [ "${1#-}" = "$1" ]; then
            shift
        fi
        src=
        dest=
        while [ $# -gt 0 ]; do
            case "$1" in
                -c)
                    shift
                    ;;
                -m|-o|-t)
                    shift 2
                    ;;
                -*)
                    shift
                    ;;
                *)
                    if [ -z "$src" ]; then
                        src=$1
                    else
                        dest=$1
                    fi
                    shift
                    ;;
            esac
        done
        base=${src%.la}
        mkdir -p "$dest"
        cp "$src" "$dest/$(basename "$src")"
        if [ -f "$base.a" ]; then
            cp "$base.a" "$dest/$(basename "$base.a")"
        fi
        dlname=$(sed -n "s/^dlname='\(.*\)'$/\1/p" "$src" 2>/dev/null || true)
        if [ -n "$dlname" ]; then
            cp "$(dirname "$src")/$dlname" "$dest/$dlname"
            # The name the linker looks for with -l.
            if [ "$(uname -s)" = Darwin ]; then
                ln -sf "$dlname" "$dest/$(basename "$base").dylib"
            else
                ln -sf "$dlname" "$dest/$(basename "$base").so"
            fi
        fi
        ;;
    *)
        exit 0
        ;;
esac
EOF
chmod +x "$build_dir/libtool"

for generated in "$build_dir/Makefile" "$build_dir/apu-1-config" "$build_dir/build/rules.mk" "$build_dir/libtool"; do
    if [ -f "$generated" ]; then
        perl -0pi -e 's/\r\n/\n/g; s/\r/\n/g' "$generated"
    fi
done

make install

perl -0pe 's/@apu_have_sqlite3_10@/1/g; s/@apu_have_crypto_10@/0/g; s/@apu_have_apr_iconv_10@/0/g' "$work_dir/include/apu.hwc" > "$install_dir/include/apu.h"
perl -0pe 's/@apr_has_ldap_10@/0/g' "$work_dir/include/apr_ldap.hwc" > "$install_dir/include/apr_ldap.h"
cp "$work_dir/include/apu_want.hw" "$install_dir/include/apu_want.h"
cp "$work_dir/include/private/apu_config.hw" "$install_dir/include/apu_config.h"
cp "$work_dir/include/private/apu_select_dbm.hw" "$install_dir/include/apu_select_dbm.h"

cp "$build_dir/build/rules.mk" "$install_dir/build/rules.mk"
for generated in "$install_dir/bin/apu-1-config" "$install_dir/build/rules.mk" "$install_dir/lib/libaprutil-1.la"; do
    if [ -f "$generated" ]; then
        perl -0pi -e 's/\r\n/\n/g; s/\r/\n/g' "$generated"
    fi
done
