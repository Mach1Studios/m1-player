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
echo "Building VLC Libraries"
echo "========================================"
echo ""

# Detect architecture - support cross-compilation via Rosetta 2
HOST_ARCH="$(uname -m)"
ROSETTA_MODE=false

# Check if running under Rosetta (x86_64 process on ARM64 hardware)
if [ "$HOST_ARCH" = "x86_64" ] && [ "$(uname)" = "Darwin" ]; then
    if sysctl -n sysctl.proc_translated 2>/dev/null | grep -q 1; then
        ROSETTA_MODE=true
        echo "Running under Rosetta 2 (cross-compiling for Intel)"
    fi
fi

echo "Target architecture: $HOST_ARCH"
if [ "$ROSETTA_MODE" = true ]; then
    echo "  Cross-compilation mode: ARM64 Mac -> x86_64 binary"
fi
echo "NOTE: VLC will be built for $HOST_ARCH only."
echo "      For distribution, build on each target architecture separately."
echo ""

# Check if VLC source exists
if [ ! -d "$VLC_SOURCE" ]; then
    echo "   Error: VLC source not found at: $VLC_SOURCE"
    echo "   Run: git submodule update --init --recursive"
    exit 1
fi

# Set environment for Homebrew on macOS
if [ "$(uname)" = "Darwin" ]; then
    if [ "$HOST_ARCH" = "arm64" ]; then
        # Apple Silicon - Homebrew at /opt/homebrew
        HOMEBREW_PREFIX="/opt/homebrew"
    else
        # Intel (or Rosetta) - Homebrew at /usr/local
        HOMEBREW_PREFIX="/usr/local"
    fi
    
    # Verify Homebrew exists for target architecture
    if [ ! -d "$HOMEBREW_PREFIX/bin" ]; then
        echo ""
        echo "ERROR: Homebrew not found at $HOMEBREW_PREFIX"
        if [ "$ROSETTA_MODE" = true ]; then
            echo ""
            echo "To build for Intel on Apple Silicon, install x86_64 Homebrew first:"
            echo "  arch -x86_64 /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
            echo ""
            echo "Then install required dependencies:"
            echo "  arch -x86_64 /usr/local/bin/brew install autoconf automake libtool pkg-config"
            echo "  arch -x86_64 /usr/local/bin/brew install ffmpeg@6 libass flac mpg123 libvpx x264 x265"
            echo "  arch -x86_64 /usr/local/bin/brew install dav1d aom opus libvorbis theora speex libogg libpng jpeg-turbo"
        fi
        exit 1
    fi
    
    echo "Using Homebrew at: $HOMEBREW_PREFIX"
    export PATH="$HOMEBREW_PREFIX/bin:$PATH"
    export LDFLAGS="-L$HOMEBREW_PREFIX/lib -L$HOMEBREW_PREFIX/opt/gettext/lib $LDFLAGS"
    export CPPFLAGS="-I$HOMEBREW_PREFIX/include -I$HOMEBREW_PREFIX/opt/gettext/include $CPPFLAGS"
    export PKG_CONFIG_PATH="$HOMEBREW_PREFIX/lib/pkgconfig:$HOMEBREW_PREFIX/opt/gettext/lib/pkgconfig:$PKG_CONFIG_PATH"
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

patch_objc_libtool_tags() {
    local makefile

    for makefile in "$VLC_BUILD/modules/Makefile" "$VLC_BUILD/bin/Makefile" "$VLC_BUILD/test/Makefile"; do
        [ -f "$makefile" ] || continue

        /usr/bin/python3 - "$makefile" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
text = path.read_text()
updated = re.sub(
    r'(?<!--tag=CC )--mode=compile \$\(OBJC\)',
    '--tag=CC --mode=compile $(OBJC)',
    text,
)
updated = re.sub(
    r'(?<!--tag=CC )--mode=link \$\((OBJCLD|OBJC)\)',
    lambda match: f'--tag=CC --mode=link $({match.group(1)})',
    updated,
)

if updated != text:
    path.write_text(updated)
    print(f"Patched Objective-C libtool tags in {path}")
PY
    done
}

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
if [ "$(uname -m)" = "arm64" ]; then
    MACOS_MIN_VERSION="11.0"
else
    MACOS_MIN_VERSION="10.14"
fi
export CFLAGS="-mmacosx-version-min=$MACOS_MIN_VERSION $CFLAGS"
export CXXFLAGS="-mmacosx-version-min=$MACOS_MIN_VERSION $CXXFLAGS"
export OBJCFLAGS="-mmacosx-version-min=$MACOS_MIN_VERSION $OBJCFLAGS"
export LDFLAGS="-mmacosx-version-min=$MACOS_MIN_VERSION $LDFLAGS"

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

if [ "$(uname)" = "Darwin" ]; then
    patch_objc_libtool_tags
fi

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
