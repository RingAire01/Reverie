#!/usr/bin/env bash
# Reverie Packager (macOS / Linux): release build + host bundle formats.
#
#   NAME=myapp VERSION=0.1.0 scripts/package-app.sh
#
# macOS -> .app + .dmg;  Linux -> AppImage + .deb + .rpm.
set -euo pipefail

NAME="${NAME:-app}"
VERSION="${VERSION:-0.1.0}"
HERE="$(cd "$(dirname "$0")" && pwd)"
APP_ROOT="$(cd "$HERE/.." && pwd)"
TARGET="$APP_ROOT/src-reverie/target/release"

mkdir -p "$TARGET"

# Release build (requires the host backend; skipped if `rev` is unavailable).
if command -v rev >/dev/null 2>&1; then
    rev build "$APP_ROOT/src-reverie/main.reo" -o "$TARGET/$NAME" || true
fi

case "$(uname -s)" in
    Darwin) exec "$HERE/packager/macos.sh" "$NAME" "$VERSION" ;;
    Linux)  exec "$HERE/packager/linux.sh" "$NAME" "$VERSION" ;;
    *) echo "unsupported host: $(uname -s)"; exit 1 ;;
esac
