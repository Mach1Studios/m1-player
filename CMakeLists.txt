cmake_minimum_required(VERSION 3.15.0)

# Set policy for Xcode new build system compatibility with ExternalProject
if(POLICY CMP0114)
    cmake_policy(SET CMP0114 NEW)
endif()

# Force ARM64 architecture on Apple Silicon to match VLC installation
# This must be set BEFORE project() to ensure it applies to all dependencies
if(APPLE)
    # Note: To support Universal Binaries (x86_64 and arm64), VLC must be built for both architectures.
    # Currently build_vlc.sh builds for the host architecture only.
    # If you have universal VLC libs, you can change this to "arm64;x86_64"
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for macOS" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target" FORCE)
    message(STATUS "Building for ARM64 only to match VLC installation")
endif()

file(STRINGS VERSION CURRENT_VERSION)
project(M1-Player VERSION ${CURRENT_VERSION})

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

### IDE Generator pre-config ###
# Xcode: Disable automatic build scheme generation globally.
#        Instead, we explicitely specify which targets require schemes.
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
# Enable to build universal binaries on macOS, increasing build time
# This only affects local builds, GitHub actions always builds Universals
set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum macOS suppored" FORCE)
set(CMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET[arch=arm64] "11.0" CACHE STRING "arm 64 minimum deployment target" FORCE)
# IDEs:  Enable grouping of source files into folders in IDEs.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# IDEs:  Create a folder in the IDE with the JUCE Module code.
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

# Adds the Mach1 math
# (Please ensure this is added before the `m1_orientation_client` JUCE module)
add_subdirectory(Modules/m1_orientation_client/libs/m1-mathematics)

# add JUCE
add_subdirectory(JUCE)
# add custom modules
add_subdirectory(Modules)

# These are used to re-apply brackets for the JucePlugin_PreferredChannelConfigurations
set(LBRACKET_LITERAL "{")
set(RBRACKET_LITERAL "}")

set(CUSTOM_BUNDLE_ID "com.mach1.spatial.player")
set(CUSTOM_PLUGIN_CODE "M1PL")

# add the plugin targets
juce_add_gui_app(${CMAKE_PROJECT_NAME}
                VERSION ${CURRENT_VERSION}
                COMPANY_NAME "Mach1"
                COMPANY_WEBSITE "https://mach1.tech"
                COMPANY_EMAIL "whatsup@mach1.tech"
                ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
                ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
                PLUGIN_MANUFACTURER_CODE "Mac1"
                PLUGIN_CODE ${CUSTOM_PLUGIN_CODE}
                BUNDLE_ID ${CUSTOM_BUNDLE_ID}
                PLUGIN_NAME ${CMAKE_PROJECT_NAME}
                PRODUCT_NAME ${CMAKE_PROJECT_NAME}
                IS_SYNTH FALSE)

# Force ARM64 architecture for the JUCE target specifically
if(APPLE)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        OSX_ARCHITECTURES "arm64"
        XCODE_ATTRIBUTE_ARCHS "arm64"
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"
        XCODE_ATTRIBUTE_VALID_ARCHS "arm64"
    )
endif()

# add required flags
juce_generate_juce_header(${CMAKE_PROJECT_NAME})

target_compile_definitions(${CMAKE_PROJECT_NAME}
    PUBLIC 
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${CMAKE_PROJECT_NAME},JUCE_PROJECT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${CMAKE_PROJECT_NAME},JUCE_VERSION>"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Windows-specific definitions to match Mach1 SDK build configuration
if(WIN32 OR MSVC OR MINGW)
    message(STATUS "Adding M1_STATIC definition for Windows build")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE M1_STATIC)
endif()

# Set the C++ language standard requirenment for the "shared code" library target.
# Setting this to PUBLIC ensures that all dependent targets will inherit the specified C++ standard.
target_compile_features("${CMAKE_PROJECT_NAME}" PUBLIC cxx_std_17)
# Disable compiler extensions for the project targets (e.g. use -std=c++17 instead of -std=gnu++17).
get_property(project_targets DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
set_target_properties(${project_targets} PROPERTIES CXX_EXTENSIONS OFF)

# add the sources
add_subdirectory(Source)
add_subdirectory(Resources)

# Required for Linux happiness:
# See https://forum.juce.com/t/loading-pytorch-model-using-binarydata/39997/2
set_target_properties(Resources PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
set_target_properties(Resources PROPERTIES FOLDER "Targets")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Resources)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        juce::juce_audio_basics 
        juce::juce_audio_devices 
        juce::juce_audio_formats 
        juce::juce_audio_plugin_client 
        juce::juce_audio_processors 
        juce::juce_audio_utils 
        juce::juce_core 
        juce::juce_data_structures 
        juce::juce_events 
        juce::juce_graphics 
        juce::juce_gui_basics 
        juce::juce_gui_extra 
        juce::juce_opengl 
        juce::juce_osc 
        juce_murka 
        m1_orientation_client
        m1_mathematics
        juce_libvlc
        )

# definitions to replace the `JucePluginDefines.h`
set(JucePlugin_Name ${CMAKE_PROJECT_NAME})
set(JucePlugin_Desc ${CMAKE_PROJECT_NAME})

# link m1-sdk
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE M1Encode M1Decode M1Transcode)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE Modules/m1-sdk/libmach1spatial/api_common/include Modules/m1-sdk/libmach1spatial/api_decode/include Modules/m1-sdk/libmach1spatial/api_encode/include Modules/m1-sdk/libmach1spatial/api_transcode/include Modules/m1-sdk/libmach1spatial/deps)

# libVLC linking - find and link libVLC for the juce_libvlc module
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Modules/juce_libvlc/cmake")
find_package(LibVLC REQUIRED)
if(TARGET LibVLC::LibVLC)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE LibVLC::LibVLC)
    
    # Note: When building from source (LIBVLC_BUILD_FROM_SOURCE=ON), 
    # the LibVLC::LibVLC target already includes both libvlc and libvlccore.
    # Only look for system libvlccore when using system VLC.
    if(NOT LIBVLC_BUILD_FROM_SOURCE)
        # Also link libvlccore which is required on macOS with system VLC
        find_library(LibVLCCore_LIBRARY
            NAMES vlccore libvlccore
            PATHS /Applications/VLC.app/Contents/MacOS/lib
        )
        if(LibVLCCore_LIBRARY)
            message(STATUS "Found libvlccore: ${LibVLCCore_LIBRARY}")
            target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LibVLCCore_LIBRARY})
        else()
            message(WARNING "libvlccore not found - linking may fail")
        endif()
    endif()
    
    # Bundle VLC plugins into the app (replicates VLC.app structure)
    # This matches how VLC.app packages plugins in Contents/MacOS/plugins/
    if(LIBVLC_BUILD_FROM_SOURCE)
        set(VLC_INSTALL_DIR "${CMAKE_BINARY_DIR}/vlc-install")
        set(VLC_PLUGINS_SOURCE "${VLC_INSTALL_DIR}/lib/vlc/plugins")
        
        # Check if VLC plugins exist (will be .dylib after rebuild)
        if(EXISTS "${VLC_PLUGINS_SOURCE}")
            message(STATUS "VLC Plugin bundling:")
            message(STATUS "  Source: ${VLC_PLUGINS_SOURCE}")
            message(STATUS "  Destination: Contents/PlugIns/vlc/plugins/")
            
            # Create custom target to copy VLC plugins to app bundle
            # This runs after the app is built
            # NOTE: We use Contents/Resources/ instead of Contents/PlugIns/ because:
            # 1. plugins.dat is a data file, not code, and codesign fails on it in PlugIns/
            # 2. VLC plugins are dynamically loaded by libVLC, not macOS plugin system
            add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Bundling VLC plugins..."
                COMMAND ${CMAKE_COMMAND} -E make_directory 
                    "$<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_NAME}>/Resources/vlc"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${VLC_PLUGINS_SOURCE}"
                    "$<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_NAME}>/Resources/vlc/plugins"
                # Clean up static libraries (.a) and libtool files (.la) from the bundle
                # These shouldn't be in the bundle
                COMMAND find "$<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_NAME}>/Resources/vlc/plugins" -name "*.a" -delete
                COMMAND find "$<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_NAME}>/Resources/vlc/plugins" -name "*.la" -delete
                # Bundle VLC dependencies (FFmpeg, etc.) from Homebrew and fix their rpaths
                # This is required because VLC plugins link against Homebrew dylibs at absolute paths
                # which are blocked by macOS sandbox. We copy them into the bundle and rewrite paths.
                COMMAND ${CMAKE_COMMAND} -E echo "Bundling VLC dependencies..."
                COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/bundle_vlc_deps.sh" "$<TARGET_BUNDLE_DIR:${CMAKE_PROJECT_NAME}>"
                COMMAND ${CMAKE_COMMAND} -E echo "VLC plugins and dependencies bundled successfully"
                COMMENT "Copying VLC plugins to app bundle Resources and bundling dependencies"
            )
            
            # Generate plugins.dat cache if it doesn't exist
            # VLC requires this cache file to load plugins at runtime
            set(VLC_PLUGINS_CACHE "${VLC_PLUGINS_SOURCE}/plugins.dat")
            if(NOT EXISTS "${VLC_PLUGINS_CACHE}")
                set(VLC_CACHE_GEN "${VLC_INSTALL_DIR}/lib/vlc/vlc-cache-gen")
                if(EXISTS "${VLC_CACHE_GEN}")
                    message(STATUS "Generating VLC plugins cache...")
                    execute_process(
                        COMMAND ${CMAKE_COMMAND} -E env "DYLD_LIBRARY_PATH=${VLC_INSTALL_DIR}/lib" "${VLC_CACHE_GEN}" "${VLC_PLUGINS_SOURCE}"
                        RESULT_VARIABLE cache_result
                    )
                    if(cache_result EQUAL 0)
                        message(STATUS "VLC plugins cache generated successfully")
                    else()
                        message(WARNING "Failed to generate VLC plugins cache")
                    endif()
                endif()
            endif()
            
            # Also set VLC_PLUGIN_PATH at build time for testing from build directory
            # Note: When running from Xcode, use the source location; when running the app bundle,
            # VLCMediaPlayer.cpp will detect and use Contents/Resources/vlc/plugins
            set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
                XCODE_SCHEME_ENVIRONMENT "VLC_PLUGIN_PATH=${VLC_PLUGINS_SOURCE}"
            )
        else()
            message(STATUS "VLC plugins not found at ${VLC_PLUGINS_SOURCE}")
            message(STATUS "  Plugins will be bundled after VLC rebuild completes")
        endif()
    endif()
endif()

if(APPLE)
    ### setup info.plist and entitlements for macos
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Resources/${CMAKE_PROJECT_NAME}-Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME ${CMAKE_PROJECT_NAME}
        MACOSX_FRAMEWORK_IDENTIFIER ${CUSTOM_BUNDLE_ID}
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/${CMAKE_PROJECT_NAME}.entitlements"
        XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.utilities"
    )
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        "-framework Security"
        )
elseif(MSVC OR WIN32 OR MINGW)
    message(STATUS "Adding windows OS flag for httplib")
    add_compile_definitions(WIN32_LEAN_AND_MEAN)    
endif()

### IDE Generator post-config ###
# IDEs:  Move the "shared code" source group (main target) out of the "Targets" folder
#        to the top level.
set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES FOLDER "")
# IDEs:  Organise source file grouping.
get_target_property(project_sources "${CMAKE_PROJECT_NAME}" SOURCES)
#   If JuceHeader.h is generated, remove it from the source file list and handle it individually.
get_target_property(juce_library_code "${CMAKE_PROJECT_NAME}" JUCE_GENERATED_SOURCES_DIRECTORY)
set(juce_header "${juce_library_code}/JuceHeader.h")
list(REMOVE_ITEM project_sources "${juce_header}")

#   Place JuceHeader.h and the associated CMake Rule file into a "JUCE Library Code" folder.
source_group("JUCE Library Code" FILES "${juce_header}")
source_group("JUCE Library Code/CMake Rules" FILES "${juce_header}.rule")

#   Generate source groups that follow the organisation of source file directories.
source_group("" FILES ${project_sources})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} PREFIX "" FILES SOURCES)

#   Place Binary Data related source files into the root of their target folder.
if(TARGET Resources)
    get_target_property(project_assets Resources SOURCES)
    source_group("" FILES ${project_assets})
endif()

# Color our warnings and errors
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options(-fcolor-diagnostics)
endif()
