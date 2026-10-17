cmake_minimum_required(VERSION 3.19)

project(Ra3Source
    VERSION 0.1.0
    DESCRIPTION "Red Alert 3 (ra3_1.0.game) reverse-engineering reconstruction"
    LANGUAGES CXX)

option(RA3_BUILD_TOOLS "Build developer tools" ON)
option(RA3_BUILD_TESTS "Build tests" ON)
option(RA3_ALLOW_NON_X86 "Allow targets other than 32-bit x86" OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT RA3_ALLOW_NON_X86)
    message(FATAL_ERROR
        "ra3_1.0.game is a 32-bit x86 program and reconstructed structures must keep its layout. "
        "Configure with the 'win32' preset (-A Win32) or set RA3_ALLOW_NON_X86=ON.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(RA3_DEFAULT_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/config/ra3source.ini"
    CACHE FILEPATH "Configuration file used when none is given on the command line or in RA3_CONFIG")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Ra3Targets)

add_subdirectory(src/core)
add_subdirectory(src/io)
add_subdirectory(src/text)
add_subdirectory(src/asset)
add_subdirectory(src/common)
add_subdirectory(src/math)
add_subdirectory(src/sim)
add_subdirectory(src/net)
add_subdirectory(src/render)
add_subdirectory(src/gameclient)
add_subdirectory(src/gamelogic)
add_subdirectory(src/audio)
add_subdirectory(src/hook)

if(RA3_BUILD_TOOLS)
    add_subdirectory(tools/bigcheck)
    add_subdirectory(tools/ra3tool)
    add_subdirectory(tools/ra3inject)
    add_subdirectory(tools/portcheck)
    add_subdirectory(tools/playercheck)
    add_subdirectory(tools/rendercheck)
    add_subdirectory(tools/fxpscheck)
    add_subdirectory(tools/skirmishcheck)
    add_subdirectory(tools/behaviorcheck)
    add_subdirectory(tools/stringcheck)
    add_subdirectory(tools/stringbase2check)
    add_subdirectory(tools/textcheck)
    add_subdirectory(tools/netcheck)
    add_subdirectory(tools/audioeventcheck)
endif()

if(RA3_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
