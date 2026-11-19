# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.22.1)

project(upicstudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(XEN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

add_link_options("-Wl,-z,max-page-size=16384")

add_library(xen_engine STATIC
        "${XEN_ROOT}/shared/src/engine.cpp"
        "${XEN_ROOT}/shared/src/serialize.cpp")
target_include_directories(xen_engine PUBLIC
        "${XEN_ROOT}/lib/include"
        "${XEN_ROOT}/shared/include")
target_compile_features(xen_engine PUBLIC cxx_std_17)

add_library(upic SHARED
        AudioOutput.cpp
        UpicJni.cpp)
target_include_directories(upic PRIVATE
        "${XEN_ROOT}/lib/include"
        "${XEN_ROOT}/shared/include")
target_compile_features(upic PRIVATE cxx_std_17)
target_link_libraries(upic
        xen_engine
        aaudio
        android
        log)
