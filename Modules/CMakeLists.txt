juce_add_modules(
        juce_murka
        m1_orientation_client)

add_subdirectory(foleys_video_engine) # used to fetch and compile dependencies

# Mach1 Spatial SDK
# Use these options to block dependencies we do not need
set(BUILD_M1DECODEPOSITIONAL OFF)
set(BUILD_MACH1SPATIAL_COMBINED_LIB OFF) # We do not need the combined lib

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/Modules/m1-sdk/cmake) # Prepend the global CMake modules directory
add_subdirectory(m1-sdk/libmach1spatial)