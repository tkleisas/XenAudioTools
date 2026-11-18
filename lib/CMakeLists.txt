add_library(xen INTERFACE)
target_include_directories(xen INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(xen INTERFACE cxx_std_17)
