
# Signing key for rimage
###set(SIGNING_KEY "C:/work/key/mtl_private_key.pem")
set(SIGNING_KEY "/home/ilabuser/work/mtl_private_key.pem")

# Path to config toml for rimage
###set(TOML "C:/work/sof/rimage/config/mtl.toml")
set(TOML "/home/ilabuser/zephyrproject/sof/rimage/config/mtl.toml")

###########################################################

# This Loadable Modules Dev Kit root dir
set(LMDK_BASE ${CMAKE_CURRENT_LIST_DIR}/..)
cmake_path(ABSOLUTE_PATH LMDK_BASE NORMALIZE)

# thesofproject root dir
set(SOF_BASE ${LMDK_BASE}/..)
cmake_path(ABSOLUTE_PATH SOF_BASE NORMALIZE)

set(RIMAGE_INCLUDE_DIR ${SOF_BASE}/rimage/src/include)
cmake_path(ABSOLUTE_PATH RIMAGE_INCLUDE_DIR NORMALIZE)

# Path to rimage exec
set(RIMAGE_COMMAND ${SOF_BASE}/rimage/build/rimage)
