cmake_minimum_required(VERSION 3.24)

set(XEN_VERSION "" CACHE STRING "Project version (defaults to the nearest git tag or 0.1.0)")

if(XEN_VERSION STREQUAL "")
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE XEN_GIT_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(NOT XEN_GIT_TAG STREQUAL "")
            string(REGEX REPLACE "^v" "" XEN_VERSION "${XEN_GIT_TAG}")
        endif()
    endif()
endif()

if(XEN_VERSION STREQUAL "")
    set(XEN_VERSION "0.1.0")
endif()

project(XenAudioTools VERSION ${XEN_VERSION} LANGUAGES C CXX)

message(STATUS "XenAudioTools version: ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(XEN_BUILD_VST3 "Build the JUCE VST3 instrument" ON)
option(XEN_BUILD_APP "Build the standalone UPIC Studio application" ON)
option(XEN_BUILD_TESTS "Build the unit tests" ON)
option(XEN_BUILD_TOOLS "Build preset and helper tools" ON)

set(XEN_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ans-1964/third_party/JUCE"
    CACHE PATH "JUCE source directory")

if(XEN_BUILD_VST3 OR XEN_BUILD_APP)
    if(NOT EXISTS "${XEN_JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE not found at ${XEN_JUCE_DIR}. Set -DXEN_JUCE_DIR=<path>.")
    endif()
endif()

enable_testing()

add_subdirectory(lib)
add_subdirectory(shared)

if(XEN_BUILD_VST3 OR XEN_BUILD_APP)
    add_subdirectory("${XEN_JUCE_DIR}" JUCE)
    add_subdirectory(ui)
endif()

if(XEN_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(XEN_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(XEN_BUILD_VST3)
    add_subdirectory(vst3)
endif()

if(XEN_BUILD_APP)
    add_subdirectory(app)
endif()
