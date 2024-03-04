# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

# Fetch the latest pre-built libs
FetchContent_Declare(
  m1-sdk
  URL      https://github.com/Mach1Studios/m1-sdk/releases/download/0a420df/mach1spatial-libs.zip
)

FetchContent_GetProperties(m1-sdk)

if (NOT m1-sdk_POPULATED)
    FetchContent_Populate(m1-sdk)

    # Perform arbitrary actions on the m1-sdk project
    # Avoid `add_subdirectory()` until a CMakeFile.txt is added to this directory
    set(MACH1SPATIAL_LIBS_PATH "${m1-sdk_SOURCE_DIR}")
endif()

if(APPLE)
    set(MACH1SPATIAL_LIBS_UNIX_PATH ${MACH1SPATIAL_LIBS_PATH}/xcode)
else()
    set(MACH1SPATIAL_LIBS_UNIX_PATH ${MACH1SPATIAL_LIBS_PATH}/linux)
endif()

# link libraries
if(WIN32 OR MSVC OR MINGW)
    add_compile_definitions(M1_STATIC)
    message(STATUS "Adding windows OS flags")

    find_library(MACH1ENCODE_LIBRARY_RELEASE
        NAMES Mach1EncodeCAPI
        PATHS ${MACH1SPATIAL_LIBS_PATH}/vs-15-2017-x86_64/lib/Static/MD/Release
    )
    find_library(MACH1ENCODE_LIBRARY_DEBUG
        NAMES Mach1EncodeCAPId
        PATHS ${MACH1SPATIAL_LIBS_PATH}/vs-15-2017-x86_64/lib/Static/MD/Debug
    )
    SET(MACH1ENCODE_LIBRARY
        debug ${MACH1ENCODE_LIBRARY_DEBUG}
        optimized ${MACH1ENCODE_LIBRARY_RELEASE}
    )
    find_library(MACH1DECODE_LIBRARY_RELEASE
        NAMES Mach1DecodeCAPI
        PATHS ${MACH1SPATIAL_LIBS_PATH}/vs-15-2017-x86_64/lib/Static/MD/Release
    )
    find_library(MACH1DECODE_LIBRARY_DEBUG
        NAMES Mach1DecodeCAPId
        PATHS ${MACH1SPATIAL_LIBS_PATH}/vs-15-2017-x86_64/lib/Static/MD/Debug
    )
    SET(MACH1DECODE_LIBRARY
        debug ${MACH1DECODE_LIBRARY_DEBUG}
        optimized ${MACH1DECODE_LIBRARY_RELEASE}
    )
    find_library(MACH1TRANSCODE_LIBRARY_RELEASE
        NAMES Mach1TranscodeCAPI
        PATHS ${MACH1SPATIAL_LIBS_PATH}/vs-15-2017-x86_64/lib/Static/MD/Release
    )
    find_library(MACH1TRANSCODE_LIBRARY_DEBUG
        NAMES Mach1TranscodeCAPId
        PATHS ${MACH1SPATIAL_LIBS_PATH}/vs-15-2017-x86_64/lib/Static/MD/Debug
    )
    SET(MACH1TRANSCODE_LIBRARY
        debug ${MACH1TRANSCODE_LIBRARY_DEBUG}
        optimized ${MACH1TRANSCODE_LIBRARY_RELEASE}
    )
else()
    find_library(MACH1ENCODE_LIBRARY 
        NAMES Mach1EncodeCAPI
        PATHS ${MACH1SPATIAL_LIBS_UNIX_PATH}/lib
    )
    find_library(MACH1DECODE_LIBRARY 
        NAMES Mach1DecodeCAPI
        PATHS ${MACH1SPATIAL_LIBS_UNIX_PATH}/lib
    )
    find_library(MACH1TRANSCODE_LIBRARY 
        NAMES Mach1TranscodeCAPI
        PATHS ${MACH1SPATIAL_LIBS_UNIX_PATH}/lib
    )
endif()

# include headers
set(MACH1SPATIAL_INCLUDES ${MACH1SPATIAL_LIBS_PATH}/linux/include ${MACH1SPATIAL_LIBS_PATH}/linux/include/M1DSP ${MACH1SPATIAL_LIBS_PATH}/xcode/include ${MACH1SPATIAL_LIBS_PATH}/xcode/include/M1DSP ${MACH1SPATIAL_LIBS_PATH}/windows-x86/include ${MACH1SPATIAL_LIBS_PATH}/windows-x86/include/M1DSP)
include_directories(${MACH1SPATIAL_INCLUDES})
