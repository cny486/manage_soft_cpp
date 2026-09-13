cmake_minimum_required(VERSION 3.16)

project(ManageSoftCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(MANAGE_SOFT_BUILD_CLIENT "Build the desktop client" ON)
option(MANAGE_SOFT_BUILD_SERVER "Build the TCP backend server" ON)
option(MANAGE_SOFT_BUILD_UPDATER "Build the desktop updater helper" ON)

if(NOT MANAGE_SOFT_BUILD_CLIENT AND NOT MANAGE_SOFT_BUILD_SERVER)
    message(FATAL_ERROR "At least one of MANAGE_SOFT_BUILD_CLIENT or MANAGE_SOFT_BUILD_SERVER must be ON.")
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

if(MANAGE_SOFT_BUILD_CLIENT)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
endif()
find_package(ZLIB QUIET)

if(NOT ZLIB_FOUND)
    get_filename_component(MINGW_TOOLCHAIN_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(MINGW_TOOLCHAIN_ROOT "${MINGW_TOOLCHAIN_BIN_DIR}" DIRECTORY)

    find_path(MINGW_ZLIB_INCLUDE_DIR
        NAMES zlib.h
        HINTS
            "${MINGW_TOOLCHAIN_ROOT}/x86_64-w64-mingw32/include"
            "${MINGW_TOOLCHAIN_ROOT}/include"
            "${Qt${QT_VERSION_MAJOR}_DIR}/../../../include/QtZlib"
    )

    find_library(MINGW_ZLIB_LIBRARY
        NAMES z zlib libz
        HINTS
            "${MINGW_TOOLCHAIN_ROOT}/x86_64-w64-mingw32/lib"
            "${MINGW_TOOLCHAIN_ROOT}/lib"
    )

    if(MINGW_ZLIB_INCLUDE_DIR AND MINGW_ZLIB_LIBRARY)
        add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
        set_target_properties(ZLIB::ZLIB PROPERTIES
            IMPORTED_LOCATION "${MINGW_ZLIB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MINGW_ZLIB_INCLUDE_DIR}"
        )
        set(ZLIB_FOUND TRUE)
    else()
        message(FATAL_ERROR "Could not find ZLIB. Checked CMake defaults and MinGW toolchain paths under ${MINGW_TOOLCHAIN_ROOT}.")
    endif()
endif()

add_subdirectory(src/shared)

if(MANAGE_SOFT_BUILD_CLIENT)
    add_subdirectory(src/client)
    if(MANAGE_SOFT_BUILD_UPDATER)
        add_subdirectory(src/updater)
    endif()
endif()

if(MANAGE_SOFT_BUILD_SERVER)
    add_subdirectory(src/server)
endif()
