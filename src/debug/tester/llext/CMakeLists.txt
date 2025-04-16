# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

set(tester_sources
        ../tester.c
        ../tester_dummy_test.c
        ../tester_simple_dram_test.c
)

if(CONFIG_INFERENCE_SERVICE)
  list(APPEND tester_sources ../tester_gna_test.c)
endif()

sof_llext_build("tester"
        SOURCES ${tester_sources}
)
