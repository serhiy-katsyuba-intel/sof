
cmake_minimum_required(VERSION 3.14)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../../cmake/xtensa.cmake")

project(sample)
set(MODULES_LIST sample sample2)

include(../../cmake/build.cmake)



# This is just to test linking with external library
add_library(lib2 STATIC lib2.c)
###target_link_libraries(sample_library PRIVATE lib2)
target_link_libraries(sample PUBLIC lib2)