// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2024 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#include <errno.h>
#include <sof/ipc/topology.h>
#include <sof/lib/gna/gna2-tlv.h>
#include <sof/lib/inference_service.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(inference_svc, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(inference_svc);

DECLARE_TR_CTX(inference_svc_tr, SOF_UUID(inference_svc_uuid), LOG_LEVEL_INFO);

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
#include <zephyr/device.h>

#define GET_DEVICE_LIST(node) DEVICE_DT_GET(node),

const struct device *gna_z_dev[] = {
#if CONFIG_INTEL_GNA34
	DT_FOREACH_STATUS_OKAY(intel_gna34, GET_DEVICE_LIST)
#endif
};

static const struct device *gna_get_zephyr_device(void)
{
	for (int i = 0; i < ARRAY_SIZE(gna_z_dev); i++) {
		if (gna_z_dev[i])
			return gna_z_dev[i];
	}

	return NULL;
}
#endif /* CONFIG_ZEPHYR_NATIVE_DRIVERS */

struct gna_instance_data *inference_init(void)
{
	struct gna_instance_data *gna = NULL;
	uint8_t *common_scratch = NULL;

	const struct device *gna_dev = gna_get_zephyr_device();

	if (!gna_dev) {
		tr_err(&inference_svc_tr, "GNA device not found!");
		return NULL;
	}

	/* Allocate memory for the GNA context and initialize it. */
	gna = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      sizeof(struct gna_instance_data));
	if (!gna) {
		tr_err(&inference_svc_tr, "GNA context allocation failed!");
		return gna;
	}

	gna->dev = gna_dev;

	/* Initialize GNA models list */
	list_init(&gna->gna_model_list);

	/* Allocate common scratch buffer */
	common_scratch = rballoc_align(0, SOF_MEM_CAPS_RAM, MAX_GNA_SCRATCH,
				       PLATFORM_PAGE_ALIGN);
	if (!common_scratch) {
		tr_err(&inference_svc_tr, "GNA common scratch allocation failed!");
		rfree(gna);
		return NULL;
	}

	gna->common_scratch = common_scratch;
	gna->common_scratch_size = MAX_GNA_SCRATCH;

	return gna;
}


int inference_request_release(struct gna_instance_data *gna)
{
	return 0;
}

int inference_model_release(struct gna_instance_data *gna)
{
	return 0;
}

void inference_free(struct gna_instance_data *gna)
{
	int ret;

	tr_info(&inference_svc_tr, "Inference free");

	if (gna->request_ctx) {
		ret = inference_request_release(gna);
		if (ret)
			tr_err(&inference_svc_tr, "Failed to release request context");
	}
	if (gna->model_ctx) {
		ret = inference_model_release(gna);
		if (ret)
			tr_err(&inference_svc_tr, "Failed to release model context");
	}
	if (gna)
		rfree(gna);
}
