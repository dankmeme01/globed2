cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(globed2 VERSION 1.0.1)

# set ios archs
if (${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    message(STATUS "building for ios")
    unset(CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")
    set(CMAKE_SYSTEM_NAME "iOS")
else()
    message(STATUS "building for mac")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

option(GLOBED_NATIVE_ARCH "Native architecture build" OFF)

# on windows use precompiled, on other platforms compile libsodium
if (WIN32)
    option(GLOBED_COMPILE_LIBS "Don't use precompiled libraries and compile manually" OFF)
else()
    option(GLOBED_COMPILE_LIBS "Don't use precompiled libraries and compile manually" ON)
endif()

# Check for debug build
option(ENABLE_DEBUG "Debug mode" OFF)
option(GLOBED_RELEASE "Release build" OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}asdf" STREQUAL "asdf" OR ENABLE_DEBUG)
    set(GLOBED_IS_DEBUG ON)
endif()

# Debug/Release options
if (GLOBED_IS_DEBUG)
    add_compile_definitions(GLOBED_DEBUG=1)
    add_compile_definitions(GEODE_DEBUG=1)
    add_compile_definitions(ASP_ENABLE_DEBUG=1)
elseif (GLOBED_RELEASE)
    # Enable LTO in release (2.5x less binary size, costs only a few extra seconds of build time)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# add thingies depending on the current system
if (ANDROID)
    file(GLOB_RECURSE OS_SOURCES "src/platform/os/android/*.cpp" "src/platform/arch/arm/*.cpp")
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS")
    file(GLOB_RECURSE OS_SOURCES "src/platform/os/ios/*.cpp" "src/platform/arch/arm/*.cpp")
elseif (APPLE)
    file(GLOB_RECURSE OS_SOURCES "src/platform/os/mac/*.cpp" "src/platform/arch/x86/*.cpp" "src/platform/arch/arm/*.cpp")
elseif (WIN32)
    file(GLOB_RECURSE OS_SOURCES "src/platform/os/windows/*.cpp" "src/platform/arch/x86/*.cpp")
else()
    message(FATAL_ERROR "Unsupported operating system")
endif()

# source files
file(GLOB_RECURSE SOURCES
	src/audio/*.cpp
	src/crypto/*.cpp
	src/data/*.cpp
    src/globed/*.cpp
	src/game/*.cpp
	src/hooks/*.cpp
	src/managers/*.cpp
	src/net/*.cpp
	src/ui/*.cpp
	src/util/*.cpp
	src/main.cpp
    ${OS_SOURCES}
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Windows - do stuff
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(WIN32_LEAN_AND_MEAN=1) # geode moment
    if (NOT MSVC)
        set(GLOBED_WINDOWS_CLANG ON)
    endif()

    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# export some symbols
target_compile_definitions(${PROJECT_NAME} PRIVATE GLOBED_EXPORTING)

if (CMAKE_HOST_SYSTEM MATCHES "Linux" AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLOBED_LINUX_COMPILATION=1)
endif()

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# done so you can include root files with <file.hpp>
target_include_directories(${PROJECT_NAME} PRIVATE src/)
target_include_directories(${PROJECT_NAME} PRIVATE libs/)

# our favorite libraries
CPMAddPackage("gh:camila314/uibuilder#ba63097")
CPMAddPackage(
    NAME Boost
    VERSION 1.84.0
    URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
    URL_HASH SHA256=2e64e5d79a738d0fa6fb546c6e5c2bd28f88d268a2a080546f74e5ff98f29d0e
    OPTIONS "BOOST_ENABLE_CMAKE ON" "BOOST_INCLUDE_LIBRARIES describe" # escape with \\\;
)
CPMAddPackage("gh:dankmeme01/asp2#782a4fa")

# asp defines
if (WIN32)
    if (GLOBED_IS_DEBUG)
        # debug
        target_compile_definitions(asp PRIVATE _HAS_ITERATOR_DEBUGGING=0)
    endif()

    # thingy
    target_compile_definitions(asp PRIVATE ASP_ENABLE_FORMAT=1)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ASP_ENABLE_FORMAT=1)
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/globed-codegen")
target_include_directories("${PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/globed-codegen")

include(cmake/baked_resources_gen.cmake)
generate_baked_resources_header("${CMAKE_CURRENT_SOURCE_DIR}/embedded-resources.json" "${CMAKE_CURRENT_BINARY_DIR}/globed-codegen/embedded_resources.hpp")

if (GLOBED_NATIVE_ARCH)
    # Apply native architecture
    add_compile_options("-march=native")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wno-deprecated-declarations")
    # target_compile_options(${PROJECT_NAME} PRIVATE "-ftime-trace")
    # target_compile_options(asp PRIVATE "-ftime-trace")
    # if (WIN32)
    #     target_compile_options(${PROJECT_NAME} PRIVATE "-Wno-vla-cxx-extension")
    # endif()
endif()

target_link_libraries(${PROJECT_NAME} UIBuilder Boost::describe asp)

if (GLOBED_COMPILE_LIBS)
    CPMAddPackage("gh:dankmeme01/libsodium-cmake#226abba")
    CPMAddPackage("gh:xiph/opus#v1.5.2")

    # disable some warnings
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(sodium PRIVATE "-Wno-inaccessible-base" "-Wno-pointer-sign" "-Wno-user-defined-warnings")
    endif()

    target_link_libraries(${PROJECT_NAME} sodium opus)
else()
    # download headers (binaries based off of 1.0.20 too)
    CPMAddPackage(
        NAME sodium
        GIT_REPOSITORY "https://github.com/jedisct1/libsodium.git"
        GIT_TAG "1.0.20-RELEASE"
    )
    CPMAddPackage(
        NAME opus
        GIT_REPOSITORY "https://github.com/xiph/opus.git"
        GIT_TAG "v1.5.2"
        DOWNLOAD_ONLY YES
    )

    add_library(sodium STATIC IMPORTED)
    set_target_properties(sodium PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/sodium/libsodium.lib")

    add_library(opus STATIC IMPORTED)
    set_target_properties(opus PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/opus/opus.lib")

    target_compile_definitions(${PROJECT_NAME} PRIVATE SODIUM_STATIC=1)
    target_include_directories(${PROJECT_NAME} PRIVATE "${sodium_SOURCE_DIR}/src/libsodium/include")
    target_include_directories(${PROJECT_NAME} PRIVATE "${opus_SOURCE_DIR}/include")

    set(SODIUM_VERSION_IN "${CMAKE_CURRENT_SOURCE_DIR}/libs/sodium/version.h")
    set(SODIUM_VERSION_OUT "${sodium_SOURCE_DIR}/src/libsodium/include/sodium/")

    if (NOT EXISTS "${SODIUM_VERSION_OUT}/version.h")
        message(STATUS "Copied version.h to ${SODIUM_VERSION_OUT}")
        file(COPY "${SODIUM_VERSION_IN}" DESTINATION "${SODIUM_VERSION_OUT}")
    else()
        file(READ "${SODIUM_VERSION_IN}" CONTENT_IN)
        file(READ "${SODIUM_VERSION_OUT}/version.h" CONTENT_OUT)

        if (NOT "${CONTENT_IN}" STREQUAL "${CONTENT_OUT}")
            message(STATUS "Copied version.h to ${SODIUM_VERSION_OUT}")
            file(COPY "${SODIUM_VERSION_IN}" DESTINATION "${SODIUM_VERSION_OUT}")
        endif()
    endif()

    target_link_libraries(${PROJECT_NAME} sodium opus)
endif()

# Windows - link to winsock and wmi
if (WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

# Apple - link to iokit
if (APPLE)
    find_library(IOKIT_LIBRARY IOKit)
    target_link_libraries(${PROJECT_NAME} ${IOKIT_LIBRARY})
endif()

# link to libcurl

CPMAddPackage(
    NAME curl
    GIT_REPOSITORY "https://github.com/curl/curl.git"
    GIT_TAG "curl-8_8_0"
    DOWNLOAD_ONLY YES
)

add_library(curl STATIC IMPORTED)

if (ANDROID)
    if (ANDROID_ABI STREQUAL "arm64-v8a")
        set(LIB_PLATFORM "android64")
    else()
        set(LIB_PLATFORM "android32")
    endif()
elseif (WIN32)
    set(LIB_PLATFORM "win64")
else()
    set(LIB_PLATFORM "macos")
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE CURL_STATICLIB=1)

# we are kinda leeching off of geode but we can always snatch those libraries in the future
if (WIN32)
    set_target_properties(curl PROPERTIES IMPORTED_LOCATION "$ENV{GEODE_SDK}/loader/include/link/${LIB_PLATFORM}/libcurl.lib")
else()
    set_target_properties(curl PROPERTIES IMPORTED_LOCATION "$ENV{GEODE_SDK}/loader/include/link/${LIB_PLATFORM}/libcurl.a")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE "${curl_SOURCE_DIR}/include")

target_link_libraries(${PROJECT_NAME} curl)

# mac needs this for curl
if (APPLE)
	target_link_libraries(${PROJECT_NAME} z)
elseif (WIN32)
    # windows needs this ig?
    target_link_libraries(${PROJECT_NAME} crypt32)
endif()

setup_geode_mod(${PROJECT_NAME})
