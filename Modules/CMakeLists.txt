# LibVLC Configuration Options
# Choose one of the following methods:
#
# Method 1: Build libVLC from source (static linking)
set(LIBVLC_BUILD_FROM_SOURCE ON CACHE BOOL "Build libVLC from source")
set(LIBVLC_STATIC ON CACHE BOOL "Link libVLC statically")
#
# Method 2: Use custom libVLC installation (dynamic linking):
# set(LIBVLC_ROOT "/opt/vlc-custom" CACHE PATH "Root directory of custom libVLC installation")
#
# Method 3: Use custom libVLC with static linking:
# set(LIBVLC_ROOT "/opt/vlc-static" CACHE PATH "Root directory of custom libVLC installation")
# set(LIBVLC_STATIC ON CACHE BOOL "Link libVLC statically")
#
# Method 4: Use system-installed libVLC (default, dynamic linking):
# set(LIBVLC_BUILD_FROM_SOURCE OFF CACHE BOOL "Build libVLC from source")
# (No other configuration needed)
#
# Via CMake command line:
#    cmake -DLIBVLC_BUILD_FROM_SOURCE=ON -DLIBVLC_STATIC=ON ..
#    cmake -DLIBVLC_ROOT=/path/to/vlc -DLIBVLC_STATIC=ON ..

# Add FindLibVLC.cmake to module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/juce_libvlc/cmake")

juce_add_modules(
        juce_murka
        m1_orientation_client
        juce_libvlc)

# Mach1 Spatial SDK
# Use these options to block dependencies we do not need
set(BUILD_M1DECODEPOSITIONAL OFF)
set(BUILD_MACH1SPATIAL_COMBINED_LIB OFF) # We do not need the combined lib

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/Modules/m1-sdk/cmake) # Prepend the global CMake modules directory
add_subdirectory(m1-sdk/libmach1spatial)
