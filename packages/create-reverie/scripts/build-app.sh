#!/usr/bin/env bash
# Build a Reverie app on Linux end to end: shim -> native binary -> assets.
#
#   scripts/build-app.sh [--name app] [--profile release|debug] [--dev]
#
# Output mirrors the Windows build: src-reverie/target/<profile>/<name> plus the
# bundled frontend under dist/. Requires `rev`, a C compiler, pkg-config, and
# the gtk+-3.0 / webkit2gtk-4.1 development packages.
set -euo pipefail

name="app"
profile="release"
dev=0
while [ $# -gt 0 ]; do
    case "$1" in
        --name) name="$2"; shift 2 ;;
        --profile) profile="$2"; shift 2 ;;
        --dev) dev=1; shift ;;
        -h|--help)
            echo "usage: build-app.sh [--name app] [--profile release|debug] [--dev]"
            exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 1 ;;
    esac
done

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
app_root="$(cd "$here/.." && pwd)"
shim="$app_root/shim"
src_reve="$app_root/src-reverie"
out_dir="$src_reve/target/$profile"
mkdir -p "$out_dir"

export REO_CC="${REO_CC:-cc}"
REV="${REV:-rev}"

# 1. C shim static library.
"$shim/linux/build.sh"

# 2. Generated configuration (dev mode points the window at the dev server).
if [ -f "$app_root/reverie.conf" ] && command -v node >/dev/null 2>&1; then
    if [ "$dev" = 1 ]; then
        node "$here/gen-config.mjs" --dev
    else
        node "$here/gen-config.mjs"
    fi
fi

# 3. Link flags. Only the libraries the shim references directly are listed;
#    their own dependencies come along as shared-library DT_NEEDED entries.
if pkg-config --exists webkit2gtk-4.1; then
    webkit="webkit2gtk-4.1"; jsc="javascriptcoregtk-4.1"
else
    webkit="webkit2gtk-4.0"; jsc="javascriptcoregtk-4.0"
fi

link_args=()
while IFS= read -r dir; do
    [ -n "$dir" ] && link_args+=(--lib-dir "${dir#-L}")
done < <(pkg-config --libs-only-L gtk+-3.0 "$webkit")
link_args+=(--lib-dir "$shim/linux/build")
for lib in gtk-3 "$webkit" "$jsc" gio-2.0 gobject-2.0 glib-2.0; do
    link_args+=(--link "$lib")
done

# 4. Native binary (RingEcho runtime).
"$REV" build "$src_reve/main.reo" "${link_args[@]}" -o "$out_dir/$name"

# 5. Frontend build output (for the reverie.local asset host), if present.
if [ -d "$app_root/dist" ]; then
    rm -rf "$out_dir/dist"
    cp -r "$app_root/dist" "$out_dir/dist"
fi

echo "built $out_dir/$name"
