# SPDX-License-Identifier: Apache-2.0

zephyr_library()

add_subdirectory_ifdef(CONFIG_INTEL_GNA34 intel_gna34)
