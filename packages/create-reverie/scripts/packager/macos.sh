#!/usr/bin/env bash
# macOS packager: .app bundle + .dmg. Run on macOS.
# Expects src-reverie/target/release/<name> (a macOS binary, from the macos
# backend) and, for production, target/release/dist (the built frontend).
set -euo pipefail

NAME="${1:-app}"
VERSION="${2:-0.1.0}"
BUNDLE_ID="${BUNDLE_ID:-top.ringaire.$NAME}"
HERE="$(cd "$(dirname "$0")" && pwd)"
APP_ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$APP_ROOT/src-reverie/target/release"
BIN="$OUT/$NAME"

[ -f "$BIN" ] || { echo "missing $BIN; build the macOS backend first (rev build ...)"; exit 1; }

APP="$APP_ROOT/src-reverie/target/macos/$NAME.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cp "$BIN" "$APP/Contents/MacOS/$NAME"
chmod +x "$APP/Contents/MacOS/$NAME"
if [ -d "$OUT/dist" ]; then
    cp -R "$OUT/dist" "$APP/Contents/Resources/dist"
fi

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>$NAME</string>
  <key>CFBundleDisplayName</key><string>$NAME</string>
  <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleExecutable</key><string>$NAME</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSPrincipalClass</key><string>NSApplication</string>
</dict>
</plist>
PLIST

echo "packaged (app): $APP"

if command -v hdiutil >/dev/null 2>&1; then
    hdiutil create -volname "$NAME" -srcfolder "$APP" -ov -format UDZO "$OUT/$NAME.dmg"
    echo "packaged (dmg): $OUT/$NAME.dmg"
fi
