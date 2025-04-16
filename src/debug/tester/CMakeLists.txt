# SPDX-License-Identifier: BSD-3-Clause

set(base_files
  tester.c
  tester_dummy_test.c
  tester_simple_dram_test.c
)

if(CONFIG_INFERENCE_SERVICE)
  list(APPEND base_files tester_gna_test.c)
endif()

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  if(CONFIG_COMP_TESTER STREQUAL "m" AND DEFINED CONFIG_LLEXT)

    add_subdirectory(llext ${PROJECT_BINARY_DIR}/tester_llext)
    add_dependencies(app tester)

  elseif(CONFIG_COMP_TESTER STREQUAL "y")

    zephyr_library_sources(${base_files})

  endif()

endif()
