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

# Set environment for Homebrew on macOS (Apple Silicon)
if [ "$(uname)" = "Darwin" ] && [ "$(uname -m)" = "arm64" ]; then
    export PATH="/opt/homebrew/bin:$PATH"
    export LDFLAGS="-L/opt/homebrew/lib -L/opt/homebrew/opt/gettext/lib $LDFLAGS"
    export CPPFLAGS="-I/opt/homebrew/include -I/opt/homebrew/opt/gettext/include $CPPFLAGS"
    export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/gettext/lib/pkgconfig:$PKG_CONFIG_PATH"
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

# Set build flags for macOS compatibility and SDK version
# This matches the CMake deployment target
export CFLAGS="-mmacosx-version-min=11.0 $CFLAGS"
export CXXFLAGS="-mmacosx-version-min=11.0 $CXXFLAGS"
export OBJCFLAGS="-mmacosx-version-min=11.0 $OBJCFLAGS"
export LDFLAGS="-mmacosx-version-min=11.0 $LDFLAGS"

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
# Build with static core libraries but shared plugins (hybrid approach)
# This matches how VLC.app is deployed and keeps binary size reasonable
# Set environment to prevent linking optional libraries
export OPENCORE_AMRNB_LIBS=""
export OPENCORE_AMRWB_LIBS=""

"$VLC_SOURCE/configure" \
    --prefix="$VLC_INSTALL" \
    --enable-static \
    --enable-shared \
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
    --disable-gnutls \
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
echo "Generating VLC Plugin Cache"
echo "========================================"
echo ""

# Generate plugins.dat cache file
# VLC requires this cache to properly load plugins at runtime
VLC_CACHE_GEN="$VLC_INSTALL/lib/vlc/vlc-cache-gen"
VLC_PLUGINS_DIR="$VLC_INSTALL/lib/vlc/plugins"

if [ -x "$VLC_CACHE_GEN" ] && [ -d "$VLC_PLUGINS_DIR" ]; then
    echo "Running vlc-cache-gen..."
    # Need to set library path so vlc-cache-gen can find libvlc
    DYLD_LIBRARY_PATH="$VLC_INSTALL/lib:$DYLD_LIBRARY_PATH" "$VLC_CACHE_GEN" "$VLC_PLUGINS_DIR"
    
    if [ -f "$VLC_PLUGINS_DIR/plugins.dat" ]; then
        echo "  Created: $VLC_PLUGINS_DIR/plugins.dat ($(du -h "$VLC_PLUGINS_DIR/plugins.dat" | cut -f1))"
    else
        echo "  Warning: plugins.dat was not created"
    fi
else
    echo "  Warning: vlc-cache-gen not found or plugins directory missing"
    echo "  VLC plugins may not load correctly"
fi

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
echo "Plugin cache:"
if [ -f "$VLC_PLUGINS_DIR/plugins.dat" ]; then
    echo "  $VLC_PLUGINS_DIR/plugins.dat"
else
    echo "  Warning: plugins.dat not found!"
fi
echo ""
echo "Next steps:"
echo "  1. Reconfigure CMake: make dev-player"
echo "  2. Build m1-player: cmake --build $1"
echo ""
