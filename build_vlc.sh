#!/bin/bash
# Build VLC static libraries for m1-player
# This script should be run AFTER CMake configuration

set -e  # Exit on error

# First argument is the CMake build directory (e.g., build-dev or build)
if [ -z "$1" ]; then
    echo "   Error: Build directory not specified"
    echo "   Usage: ./build_vlc.sh <cmake-build-dir>"
    echo "   Example: ./build_vlc.sh build-dev"
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CMAKE_BUILD_DIR="${SCRIPT_DIR}/$1"
VLC_SOURCE="${SCRIPT_DIR}/Modules/juce_libvlc/vlc"
VLC_BUILD="${CMAKE_BUILD_DIR}/vlc-build"
VLC_INSTALL="${CMAKE_BUILD_DIR}/vlc-install"

# Check if CMake build directory exists
if [ ! -d "$CMAKE_BUILD_DIR" ]; then
    echo "   Error: CMake build directory not found: $CMAKE_BUILD_DIR"
    echo "   Run CMake configure first: cmake -B $1"
    exit 1
fi

echo "========================================"
echo "Building VLC Static Libraries"
echo "========================================"
echo ""

# Check if VLC source exists
if [ ! -d "$VLC_SOURCE" ]; then
    echo "   Error: VLC source not found at: $VLC_SOURCE"
    echo "   Run: git submodule update --init --recursive"
    exit 1
fi

# Check for required tools
echo "Checking prerequisites..."
command -v meson >/dev/null 2>&1 || {
    echo "   Error: meson not found"
    echo "   Install: pip3 install meson"
    exit 1
}

command -v ninja >/dev/null 2>&1 || {
    echo "   Error: ninja not found"
    echo "   macOS: brew install ninja"
    exit 1
}

command -v pkg-config >/dev/null 2>&1 || {
    echo "   Error: pkg-config not found"
    echo "   macOS: brew install pkg-config"
    exit 1
}

# Check for modern bison (VLC 4.0 requires bison 3.0+)
if [ -x "/opt/homebrew/opt/bison/bin/bison" ]; then
    export PATH="/opt/homebrew/opt/bison/bin:$PATH"
    BISON_VER=$(/opt/homebrew/opt/bison/bin/bison --version | head -1 | awk '{print $4}')
    echo "Using Homebrew bison $BISON_VER"
else
    echo "Error: Modern bison not found"
    echo "   macOS: brew install bison"
    echo "   VLC 4.0 requires bison 3.0+ (system bison is too old)"
    exit 1
fi

# Check for FFmpeg (required by VLC)
if ! pkg-config --exists libavformat libavcodec libswscale; then
    echo "   Error: FFmpeg not found"
    echo "   macOS: brew install ffmpeg"
    echo "   Linux: sudo apt install libavcodec-dev libavformat-dev libswscale-dev"
    exit 1
fi

echo "All prerequisites found"
echo ""

# Check if already built
if [ -f "$VLC_INSTALL/lib/libvlc.a" ] && [ -f "$VLC_INSTALL/lib/libvlccore.a" ]; then
    echo "VLC static libraries already exist:"
    echo "  $(du -h "$VLC_INSTALL/lib/libvlc.a" | cut -f1) - $VLC_INSTALL/lib/libvlc.a"
    echo "  $(du -h "$VLC_INSTALL/lib/libvlccore.a" | cut -f1) - $VLC_INSTALL/lib/libvlccore.a"
    echo ""
    read -p "Rebuild? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Skipping build. Use existing libraries."
        exit 0
    fi
    echo "Cleaning previous build..."
    rm -rf "$VLC_BUILD" "$VLC_INSTALL"
fi

# Configure VLC with Meson
echo "========================================"
echo "Configuring VLC with Meson"
echo "========================================"
echo "Source: $VLC_SOURCE"
echo "Build:  $VLC_BUILD"
echo "Install: $VLC_INSTALL"
echo ""

meson setup "$VLC_BUILD" "$VLC_SOURCE" \
    --prefix="$VLC_INSTALL" \
    --libdir=lib \
    --includedir=include \
    --buildtype=release \
    --default-library=static \
    -Dvlc=false \
    -Dtests=disabled \
    -Dstream_outputs=false \
    -Dvideolan_manager=false \
    -Dqt=disabled \
    -Dlua=disabled \
    -Dskins2=disabled \
    -Dscreen=disabled \
    -Davcodec=enabled \
    -Davformat=enabled \
    -Dswscale=enabled

echo ""
echo "========================================"
echo "Building VLC (this will take 20-40 mins)"
echo "========================================"
echo ""

# Build VLC
meson compile -C "$VLC_BUILD"

echo ""
echo "========================================"
echo "Installing VLC libraries"
echo "========================================"
echo ""

# Install to local directory
meson install -C "$VLC_BUILD"

echo ""
echo "========================================"
echo "VLC Build Complete!"
echo "========================================"
echo ""
echo "Libraries installed to:"
echo "  $VLC_INSTALL/lib/libvlc.a ($(du -h "$VLC_INSTALL/lib/libvlc.a" | cut -f1))"
echo "  $VLC_INSTALL/lib/libvlccore.a ($(du -h "$VLC_INSTALL/lib/libvlccore.a" | cut -f1))"
echo ""
echo "Headers installed to:"
echo "  $VLC_INSTALL/include/vlc/"
echo ""
echo "Next steps:"
echo "  1. Configure CMake: cmake -B build"
echo "  2. Build m1-player: cmake --build build"
echo ""

