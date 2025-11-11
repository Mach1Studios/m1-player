#!/bin/bash
# Build VLC static libraries for m1-player
# This script should be run AFTER CMake configuration
# VLC 3.0.22 uses autotools (configure/make)

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

# VLC 3.x requires autotools
for cmd in autoconf automake libtool pkg-config make; do
    command -v $cmd >/dev/null 2>&1 || {
        echo "   Error: $cmd not found"
        echo "   macOS: brew install $cmd"
        exit 1
    }
done

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

# Bootstrap if configure doesn't exist
if [ ! -f "$VLC_SOURCE/configure" ]; then
    echo "========================================"
    echo "Bootstrapping VLC"
    echo "========================================"
    echo ""
    cd "$VLC_SOURCE"
    
    # Fix Homebrew autoreconf perl path issue
    # Ensure we use the system perl instead of hardcoded perl5.30
    export PERL=/usr/bin/perl
    
    ./bootstrap
    cd "$SCRIPT_DIR"
    echo ""
fi

# Configure VLC with autotools
echo "========================================"
echo "Configuring VLC 3.0.22"
echo "========================================"
echo "Source: $VLC_SOURCE"
echo "Build:  $VLC_BUILD"
echo "Install: $VLC_INSTALL"
echo ""

# Create build directory
mkdir -p "$VLC_BUILD"
cd "$VLC_BUILD"

# Configure VLC
# These options match what we figured out for minimal playback-only build
"$VLC_SOURCE/configure" \
    --prefix="$VLC_INSTALL" \
    --enable-static \
    --disable-shared \
    --disable-vlc \
    --disable-vlm \
    --disable-sout \
    --disable-lua \
    --disable-qt \
    --disable-skins2 \
    --disable-screen \
    --disable-nls \
    --disable-update-check \
    --disable-sparkle \
    --disable-macosx \
    --disable-subtitle \
    --disable-css \
    --enable-avcodec \
    --enable-avformat \
    --enable-swscale

echo ""
echo "========================================"
echo "Building VLC (this will take 15-30 mins)"
echo "========================================"
echo ""

# Build VLC using all available CPU cores
make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

echo ""
echo "========================================"
echo "Installing VLC libraries"
echo "========================================"
echo ""

# Install to local directory
make install

cd "$SCRIPT_DIR"

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
echo "  1. Reconfigure CMake: make dev-player"
echo "  2. Build m1-player: cmake --build $1"
echo ""
