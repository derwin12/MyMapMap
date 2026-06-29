#!/usr/bin/env bash
#
# sh_make_appimage.sh — package MyMapMap as a self-contained Linux AppImage.
#
# Bundles the built bin/mymapmap together with the Qt 6 runtime (including the
# Multimedia ffmpeg plugin needed for video playback and the video export /
# FPP Connect feature) using linuxdeploy + its Qt plugin.
#
# Run build_ubuntu.sh first so bin/mymapmap exists, then:
#   ./scripts/sh_make_appimage.sh
#
# Output: MyMapMap-<version>-x86_64.AppImage in the repo root.
#
set -euo pipefail
cd "$(dirname "$0")/.."

BIN="bin/mymapmap"
if [[ ! -x "$BIN" ]]; then
  echo "Error: $BIN not found. Build first (./build_ubuntu.sh)." >&2
  exit 1
fi

VERSION=$(grep -E '^VERSION = ' mymapmap.pro | awk '{print $3}' | tr -d '\r')
[[ -n "$VERSION" ]] || VERSION="0.0.0"

TOOLDIR="${TOOLDIR:-.appimage-tools}"
mkdir -p "$TOOLDIR"

fetch() { # url dest
  if [[ ! -f "$2" ]]; then
    echo "==> Downloading $(basename "$2")..."
    wget -q -O "$2" "$1"
  fi
  chmod +x "$2"
}

LD="$TOOLDIR/linuxdeploy-x86_64.AppImage"
LDQT="$TOOLDIR/linuxdeploy-plugin-qt-x86_64.AppImage"
fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" "$LD"
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" "$LDQT"

# Run the bundling tools without FUSE (CI runners usually lack it).
export APPIMAGE_EXTRACT_AND_RUN=1

# Icon: linuxdeploy expects the icon's basename to match the .desktop Icon= key.
ICON_SRC="resources/app_icons/mapmap.png"
ICON="$TOOLDIR/mymapmap.png"
cp "$ICON_SRC" "$ICON"

APPDIR="build-appimage/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR"

# Make sure the Qt plugin bundles the multimedia backend (ffmpeg) plugin.
export EXTRA_QT_PLUGINS="multimedia"
export OUTPUT="MyMapMap-${VERSION}-x86_64.AppImage"

echo "==> Building AppImage ($OUTPUT)..."
"$LD" --appdir "$APPDIR" \
  --executable "$BIN" \
  --desktop-file resources/linux/mymapmap.desktop \
  --icon-file "$ICON" \
  --plugin qt \
  --output appimage

echo ""
echo "Created $(pwd)/$OUTPUT"
