#!/usr/bin/env bash
# Linux packager: AppImage + .deb + .rpm. Run on Linux.
# Expects src-reverie/target/release/<name> (a Linux binary, from the linux
# backend) and, for production, target/release/dist (the built frontend).
set -euo pipefail

NAME="${1:-app}"
VERSION="${2:-0.1.0}"
ARCH="${ARCH:-amd64}"
HERE="$(cd "$(dirname "$0")" && pwd)"
APP_ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$APP_ROOT/src-reverie/target/release"
BIN="$OUT/$NAME"

[ -f "$BIN" ] || { echo "missing $BIN; build the Linux backend first (rev build ...)"; exit 1; }

# ---- AppImage ----
if command -v appimagetool >/dev/null 2>&1; then
    APPDIR="$APP_ROOT/src-reverie/target/AppDir"
    rm -rf "$APPDIR"
    mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib/$NAME"
    cp "$BIN" "$APPDIR/usr/bin/$NAME"
    [ -d "$OUT/dist" ] && cp -R "$OUT/dist" "$APPDIR/usr/lib/$NAME/dist"
    cat > "$APPDIR/$NAME.desktop" <<DESKTOP
[Desktop Entry]
Name=$NAME
Exec=usr/bin/$NAME
Type=Application
Categories=Utility;
DESKTOP
    printf '' > "$APPDIR/$NAME.png" 2>/dev/null || true
    appimagetool "$APPDIR" "$OUT/$NAME.AppImage" || echo "appimagetool failed (icon?)"
    echo "packaged (AppImage): $OUT/$NAME.AppImage"
else
    echo "appimagetool not found; skipping AppImage"
fi

# ---- .deb ----
if command -v dpkg-deb >/dev/null 2>&1; then
    DEB="$APP_ROOT/src-reverie/target/deb"
    rm -rf "$DEB"
    mkdir -p "$DEB/DEBIAN" "$DEB/usr/bin"
    cp "$BIN" "$DEB/usr/bin/$NAME"
    if [ -d "$OUT/dist" ]; then
        mkdir -p "$DEB/usr/lib/$NAME"
        cp -R "$OUT/dist" "$DEB/usr/lib/$NAME/dist"
    fi
    cat > "$DEB/DEBIAN/control" <<CONTROL
Package: $NAME
Version: $VERSION
Architecture: $ARCH
Maintainer: Reverie
Description: Reverie desktop app
CONTROL
    dpkg-deb --build "$DEB" "$OUT/$NAME.deb"
    echo "packaged (deb): $OUT/$NAME.deb"
else
    echo "dpkg-deb not found; skipping .deb"
fi

# ---- .rpm ----
if command -v rpmbuild >/dev/null 2>&1; then
    TOP="$APP_ROOT/src-reverie/target/rpm"
    rm -rf "$TOP"
    mkdir -p "$TOP"/{BUILD,RPMS,SOURCES,SPECS,SRPMS} "$TOP/$NAME-1.0/usr/bin"
    cp "$BIN" "$TOP/$NAME-1.0/usr/bin/$NAME"
    if [ -d "$OUT/dist" ]; then
        mkdir -p "$TOP/$NAME-1.0/usr/lib/$NAME"
        cp -R "$OUT/dist" "$TOP/$NAME-1.0/usr/lib/$NAME/dist"
    fi
    (cd "$TOP" && tar czf "SOURCES/$NAME-1.0.tar.gz" "$NAME-1.0")
    cat > "$TOP/SPECS/$NAME.spec" <<SPEC
Name: $NAME
Version: $VERSION
Release: 1
Summary: Reverie desktop app
License: MIT
BuildArch: x86_64
%description
Reverie desktop app.
%prep
%setup -q -n $NAME-1.0
%install
mkdir -p %{buildroot}/usr/bin %{buildroot}/usr/lib/$NAME
cp usr/bin/$NAME %{buildroot}/usr/bin/$NAME
[ -d usr/lib/$NAME/dist ] && cp -R usr/lib/$NAME/dist %{buildroot}/usr/lib/$NAME/dist || true
%files
/usr/bin/$NAME
/usr/lib/$NAME
SPEC
    rpmbuild --define "_topdir $TOP" -bb "$TOP/SPECS/$NAME.spec"
    find "$TOP/RPMS" -name '*.rpm' -exec cp {} "$OUT/" \;
    echo "packaged (rpm): $OUT/$NAME.rpm"
else
    echo "rpmbuild not found; skipping .rpm"
fi
