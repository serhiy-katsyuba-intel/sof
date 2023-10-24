# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_INTEL_GNA34		gna_model_db.c)
zephyr_library_sources_ifdef(CONFIG_INTEL_GNA34     gna_request_db.c)
zephyr_library_sources_ifdef(CONFIG_INTEL_GNA34     gna_rqueue.c)
zephyr_library_sources_ifdef(CONFIG_INTEL_GNA34     gna.c)
