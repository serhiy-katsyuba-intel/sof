
cmake_minimum_required(VERSION 3.14)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../../cmake/xtensa.cmake")

project(up_down_mixer)

set(MODULES_LIST up_down_mixer module_example2)

# TODO: Move it somewhere???
add_definitions(-DMAJOR_IADSP_API_VERSION=5)
add_definitions(-DMIDDLE_IADSP_API_VERSION=0)
add_definitions(-DMINOR_IADSP_API_VERSION=0)

include(../../cmake/build.cmake)
