#!/usr/bin/env bash
#
# build_ubuntu.sh — build MyMapMap on Ubuntu / Debian.
#
# The Linux counterpart of build_msvc2022.bat. Installs the Qt 6 build
# dependencies (unless --no-deps is given), then configures and builds with
# CMake + Ninja. The resulting binary is bin/mymapmap.
#
# Usage:
#   ./build_ubuntu.sh            # install deps (sudo), then build
#   ./build_ubuntu.sh --no-deps  # skip apt, just configure + build
#
set -euo pipefail
cd "$(dirname "$0")"

INSTALL_DEPS=1
for arg in "$@"; do
  case "$arg" in
    --no-deps) INSTALL_DEPS=0 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

if [[ "$INSTALL_DEPS" == "1" ]]; then
  echo "==> Installing build dependencies (sudo apt-get)..."
  sudo apt-get update
  sudo apt-get install -y \
    build-essential cmake ninja-build pkg-config \
    qt6-base-dev qt6-base-private-dev \
    qt6-multimedia-dev qt6-tools-dev \
    libqt6opengl6-dev libqt6openglwidgets6 \
    libgl1-mesa-dev
  # Optional: the Qt HttpServer module enables the in-app MCP server.
  # Not packaged on every release; ignore failure.
  sudo apt-get install -y libqt6httpserver6-dev 2>/dev/null || \
    echo "    (qt6-httpserver dev package unavailable — MCP server will be disabled)"
fi

echo "==> Configuring with CMake (Ninja, Release)..."
cmake -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release

echo "==> Building..."
cmake --build build-linux

echo ""
echo "Build complete: $(pwd)/bin/mymapmap"
echo "Package as an AppImage with: ./scripts/sh_make_appimage.sh"
