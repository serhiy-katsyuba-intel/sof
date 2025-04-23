# SPDX-License-Identifier: BSD-3-Clause

is_zephyr(it_is)
if(it_is) ###  Zephyr ###

zephyr_library_sources(nld.c)
zephyr_library_link_libraries_ifdef(CONFIG_PANTHERLAKE ${CMAKE_CURRENT_SOURCE_DIR}/lib/ptl/NldLib.a)

endif()
