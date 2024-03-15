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

int inference_model_init(struct inference_model *model,
			 struct gna_instance_data *gna,
			 uint32_t model_ctx_size)
{
	struct gna_model_ctx *model_ctx = NULL;
	int32_t tlv_status;
	int ret;

	if (!gna) {
		tr_err(&inference_svc_tr, "GNA data is NULL!");
		return -EINVAL;
	}

	/* Allocate model context */
	model_ctx = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    model_ctx_size);
	if (!model_ctx) {
		tr_err(&inference_svc_tr, "GNA model context allocation failed!");
		return -ENOMEM;
	}

	/* Assign model to model context data */
	model_ctx->model_data = model->data;
	model_ctx->model_size = model->size;

	/* Assign model context to GNA instance */
	gna->model_ctx = model_ctx;

	tlv_status = gna_model_parse_tlv(gna);
	if (tlv_status) {
		tr_err(&inference_svc_tr, "GNA model parsing cfg failed! status=0x%x",
		       tlv_status);
		ret = -EINVAL;
		goto model_err;
	}

	ret = gna_add_model(gna);
	if (ret) {
		tr_err(&inference_svc_tr, "Unable to add GNA model!");
		goto model_err;
	}

	return ret;

model_err:
	rfree(gna->model_ctx);
	return ret;
}

int inference_model_release(struct gna_instance_data *gna)
{
	int ret;

	tr_info(&inference_svc_tr, "Inference model release");

	if (!gna)
		return -EINVAL;

	if (gna->model_ctx->active_requests) {
		tr_err(&inference_svc_tr, "GNA model - there are still active requests!");
		return -EBUSY;
	}

	ret = gna_remove_model(gna);
	if (ret) {
		tr_err(&inference_svc_tr, "Unable to remove GNA model!");
		return ret;
	}

	rfree(gna->model_ctx);

	return 0;
}

uint32_t inference_get_model_ctx_size(struct inference_model *model)
{
	return sizeof(struct gna_model_ctx) +
	       gna_model_get_extra_scratch(model->data, model->size);
}

int inference_request_release(struct gna_instance_data *gna)
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
