#!/usr/bin/env bash
# Compile the GTK/WebKitGTK shim into shim/linux/build/libreverie_gtk.a.
#
#   shim/linux/build.sh
#
# Requires gtk+-3.0 and webkit2gtk-4.1 (or 4.0) development packages and a C
# compiler. Override the toolchain with CC / AR.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
out="$here/build"
mkdir -p "$out"

: "${CC:=cc}"
: "${AR:=ar}"
: "${PKG_CONFIG:=pkg-config}"

if "$PKG_CONFIG" --exists webkit2gtk-4.1; then
    webkit="webkit2gtk-4.1"
elif "$PKG_CONFIG" --exists webkit2gtk-4.0; then
    webkit="webkit2gtk-4.0"
else
    echo "error: need webkit2gtk-4.1 or webkit2gtk-4.0 (install libwebkit2gtk-4.1-dev)" >&2
    exit 1
fi

cflags=$("$PKG_CONFIG" --cflags gtk+-3.0 "$webkit")

"$CC" -O2 -fPIC -Wall -Wextra -c "$here/reverie_gtk.c" -o "$out/reverie_gtk.o" $cflags
"$AR" rcs "$out/libreverie_gtk.a" "$out/reverie_gtk.o"

echo "built $out/libreverie_gtk.a"
