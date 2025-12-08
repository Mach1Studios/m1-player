#!/bin/bash
# Bundle VLC dependencies into the app bundle
# This script copies all Homebrew dylibs that VLC plugins depend on
# and fixes their install names to use @loader_path

set -e

# First argument is the app bundle path
APP_BUNDLE="$1"

if [ -z "$APP_BUNDLE" ] || [ ! -d "$APP_BUNDLE" ]; then
    echo "Error: App bundle path not specified or doesn't exist"
    echo "Usage: $0 <path-to.app>"
    exit 1
fi

echo "========================================"
echo "Bundling VLC Dependencies"
echo "========================================"
echo "App Bundle: $APP_BUNDLE"
echo ""

# Directories
VLC_PLUGINS_DIR="$APP_BUNDLE/Contents/Resources/vlc/plugins"
FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"
LIBS_DIR="$APP_BUNDLE/Contents/Resources/vlc/lib"

if [ ! -d "$VLC_PLUGINS_DIR" ]; then
    echo "Error: VLC plugins not found at $VLC_PLUGINS_DIR"
    exit 1
fi

# Create lib directory for dependencies
mkdir -p "$LIBS_DIR"

echo "Scanning VLC plugins for Homebrew dependencies..."

# Detect Homebrew prefix based on architecture
if [ "$(uname -m)" = "arm64" ]; then
    HOMEBREW_PREFIX="/opt/homebrew"
else
    HOMEBREW_PREFIX="/usr/local"
fi
echo "Using Homebrew prefix: $HOMEBREW_PREFIX"
echo ""

# Find all unique Homebrew library dependencies
HOMEBREW_DEPS=$(find "$VLC_PLUGINS_DIR" -name "*.dylib" -exec otool -L {} \; 2>/dev/null | \
    grep -E "^\s+$HOMEBREW_PREFIX" | \
    awk '{print $1}' | \
    sort -u)

if [ -z "$HOMEBREW_DEPS" ]; then
    echo "No Homebrew dependencies found."
    exit 0
fi

echo "Found Homebrew dependencies:"
echo "$HOMEBREW_DEPS" | while read dep; do
    echo "  - $(basename "$dep")"
done
echo ""

# Function to get dependencies of a library
get_deps() {
    local lib="$1"
    
    if [ ! -f "$lib" ]; then
        return
    fi
    
    # Get Homebrew dependencies of this library
    otool -L "$lib" 2>/dev/null | \
        grep -E "^\s+$HOMEBREW_PREFIX" | \
        awk '{print $1}'
}

# Create a temporary file to collect all dependencies
DEPS_FILE=$(mktemp)
trap "rm -f $DEPS_FILE" EXIT

# Get all direct dependencies
echo "$HOMEBREW_DEPS" > "$DEPS_FILE"

# Recursively resolve transitive dependencies (multiple passes)
echo "Resolving transitive dependencies..."
MAX_PASSES=5
for pass in $(seq 1 $MAX_PASSES); do
    PREV_COUNT=$(wc -l < "$DEPS_FILE" | tr -d ' ')
    
    # Get dependencies of all current dependencies
    for dep in $(cat "$DEPS_FILE"); do
        if [ -f "$dep" ]; then
            get_deps "$dep" >> "$DEPS_FILE"
        fi
    done
    
    # Remove duplicates
    sort -u "$DEPS_FILE" -o "$DEPS_FILE"
    
    NEW_COUNT=$(wc -l < "$DEPS_FILE" | tr -d ' ')
    
    if [ "$NEW_COUNT" -eq "$PREV_COUNT" ]; then
        echo "  Pass $pass: No new dependencies found (total: $NEW_COUNT)"
        break
    else
        echo "  Pass $pass: Found $((NEW_COUNT - PREV_COUNT)) new dependencies (total: $NEW_COUNT)"
    fi
done

# Get unique list
ALL_DEPS=$(cat "$DEPS_FILE")

# Copy each dependency to the lib directory
echo "Copying dependencies to app bundle..."
for dep in $ALL_DEPS; do
    if [ -f "$dep" ]; then
        libname=$(basename "$dep")
        dest="$LIBS_DIR/$libname"
        
        if [ ! -f "$dest" ]; then
            echo "  Copying: $libname"
            cp "$dep" "$dest"
            chmod 755 "$dest"
        fi
    else
        echo "  Warning: $dep not found"
    fi
done

echo ""
echo "Fixing install names..."

# Fix install names in the copied libraries
# Need to handle all Homebrew paths, not just the ones in our deps list
for lib in "$LIBS_DIR"/*.dylib; do
    if [ -f "$lib" ]; then
        libname=$(basename "$lib")
        
        # Change the library's own ID to @loader_path
        install_name_tool -id "@loader_path/$libname" "$lib" 2>/dev/null || true
        
        # Get all Homebrew references in this library and fix them
        HOMEBREW_REFS=$(otool -L "$lib" 2>/dev/null | grep -E "^\s+$HOMEBREW_PREFIX" | awk '{print $1}')
        for dep in $HOMEBREW_REFS; do
            depname=$(basename "$dep")
            install_name_tool -change "$dep" "@loader_path/$depname" "$lib" 2>/dev/null || true
        done
        
        # Also fix any references using Cellar path format
        CELLAR_REFS=$(otool -L "$lib" 2>/dev/null | grep -E "^\s+$HOMEBREW_PREFIX/Cellar" | awk '{print $1}')
        for dep in $CELLAR_REFS; do
            depname=$(basename "$dep")
            install_name_tool -change "$dep" "@loader_path/$depname" "$lib" 2>/dev/null || true
        done
    fi
done

# Fix install names in VLC plugins
echo "Fixing plugin references..."
find "$VLC_PLUGINS_DIR" -name "*.dylib" | while read plugin; do
    pluginname=$(basename "$plugin")
    
    # Fix references to Homebrew libraries
    for dep in $ALL_DEPS; do
        depname=$(basename "$dep")
        # VLC plugins need to reference libs relative to their location
        # Plugins are in vlc/plugins/category/, libs are in vlc/lib/
        install_name_tool -change "$dep" "@loader_path/../../lib/$depname" "$plugin" 2>/dev/null || true
    done
done

echo ""
echo "Verifying fixes..."

# Verify that no Homebrew references remain in plugins
REMAINING=$(find "$VLC_PLUGINS_DIR" -name "*.dylib" -exec otool -L {} \; 2>/dev/null | \
    grep -E "^\s+$HOMEBREW_PREFIX" | head -5)

if [ -n "$REMAINING" ]; then
    echo "Warning: Some Homebrew references still remain:"
    echo "$REMAINING"
else
    echo "All Homebrew references have been fixed!"
fi

# Count bundled libraries
LIB_COUNT=$(ls -1 "$LIBS_DIR"/*.dylib 2>/dev/null | wc -l | tr -d ' ')

echo ""
echo "Re-signing modified libraries..."

# After modifying dylibs with install_name_tool, their code signatures become invalid
# We need to re-sign them for the app to launch outside of Xcode
# Use ad-hoc signing (no identity required, works for local testing)

# Sign the bundled libraries
LIB_SIGN_COUNT=0
for lib in "$LIBS_DIR"/*.dylib; do
    if [ -f "$lib" ]; then
        codesign --force --sign - "$lib" 2>/dev/null && LIB_SIGN_COUNT=$((LIB_SIGN_COUNT + 1)) || echo "  Warning: Failed to sign $(basename "$lib")"
    fi
done
echo "  Signed $LIB_SIGN_COUNT libraries in vlc/lib/"

# Sign the VLC plugins
PLUGIN_SIGN_COUNT=0
for plugin in $(find "$VLC_PLUGINS_DIR" -name "*.dylib"); do
    codesign --force --sign - "$plugin" 2>/dev/null && PLUGIN_SIGN_COUNT=$((PLUGIN_SIGN_COUNT + 1)) || echo "  Warning: Failed to sign $(basename "$plugin")"
done
echo "  Signed $PLUGIN_SIGN_COUNT plugins in vlc/plugins/"

# Sign the main VLC libraries if they exist in Frameworks or MacOS
FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"
MACOS_DIR="$APP_BUNDLE/Contents/MacOS"

VLC_CORE_SIGNED=0
for vlclib in libvlc libvlccore; do
    for dir in "$FRAMEWORKS_DIR" "$MACOS_DIR"; do
        for ext in dylib 5.dylib 9.dylib; do
            libpath="$dir/$vlclib.$ext"
            if [ -f "$libpath" ]; then
                codesign --force --sign - "$libpath" 2>/dev/null && VLC_CORE_SIGNED=$((VLC_CORE_SIGNED + 1)) && echo "  Signed $vlclib.$ext"
            fi
        done
    done
done
if [ $VLC_CORE_SIGNED -gt 0 ]; then
    echo "  Signed $VLC_CORE_SIGNED VLC core libraries"
fi

# Add @executable_path/../Frameworks to rpath if not already present
# This allows the executable to find VLC libraries in the bundle
EXECUTABLE="$APP_BUNDLE/Contents/MacOS/$(basename "$APP_BUNDLE" .app)"
if [ -f "$EXECUTABLE" ]; then
    # Check if the Frameworks rpath already exists
    if ! otool -l "$EXECUTABLE" 2>/dev/null | grep -q "@executable_path/../Frameworks"; then
        echo "  Adding Frameworks rpath to executable..."
        install_name_tool -add_rpath "@executable_path/../Frameworks" "$EXECUTABLE" 2>/dev/null || true
        codesign --force --sign - "$EXECUTABLE" 2>/dev/null || true
    fi
fi

echo ""
echo "Regenerating VLC plugins cache..."

# Delete the old plugins.dat as it references old paths
rm -f "$VLC_PLUGINS_DIR/plugins.dat"

# Try to regenerate the plugins cache
# Note: vlc-cache-gen might not be available in the bundle, so we skip if not found
# VLC will regenerate the cache on first run if it's missing
VLC_CACHE_GEN=""
# Check common locations
if [ -f "/Applications/VLC.app/Contents/MacOS/plugins/vlc-cache-gen" ]; then
    VLC_CACHE_GEN="/Applications/VLC.app/Contents/MacOS/plugins/vlc-cache-gen"
elif [ -f "$APP_BUNDLE/../vlc-install/lib/vlc/vlc-cache-gen" ]; then
    VLC_CACHE_GEN="$APP_BUNDLE/../vlc-install/lib/vlc/vlc-cache-gen"
fi

if [ -n "$VLC_CACHE_GEN" ] && [ -x "$VLC_CACHE_GEN" ]; then
    # Need to set library path for cache generator
    DYLD_LIBRARY_PATH="$LIBS_DIR:$DYLD_LIBRARY_PATH" "$VLC_CACHE_GEN" "$VLC_PLUGINS_DIR" 2>/dev/null || \
        echo "  Warning: Cache regeneration failed (VLC will regenerate on first run)"
    if [ -f "$VLC_PLUGINS_DIR/plugins.dat" ]; then
        echo "  Plugins cache regenerated successfully"
    fi
else
    echo "  Note: vlc-cache-gen not found - VLC will regenerate cache on first run"
    echo "  This may cause a brief delay on first playback"
fi

echo ""
echo "========================================"
echo "Done! Bundled $LIB_COUNT libraries"
echo "========================================"

