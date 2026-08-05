// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2024 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#include <errno.h>
#include <sof/ipc/topology.h>
#include <sof/lib/gna/gna2-tlv.h>
#include <sof/lib/gna/gna2-tlv-reader.h>
#include <sof/lib/inference_service.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/common.h>
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

const struct device *inference_get_device_by_instance(uint32_t instance)
{
	if (instance >= ARRAY_SIZE(gna_z_dev)) {
		tr_err(&inference_svc_tr, "GNA device instance %u out of range (max %zu)",
		       instance, ARRAY_SIZE(gna_z_dev));
		return NULL;
	}

	return gna_z_dev[instance];
}
EXPORT_SYMBOL(inference_get_device_by_instance);

uint32_t inference_get_device_count(void)
{
	return ARRAY_SIZE(gna_z_dev);
}
EXPORT_SYMBOL(inference_get_device_count);
#endif /* CONFIG_ZEPHYR_NATIVE_DRIVERS */

uint32_t inference_model_get_required_hw_version(const struct inference_model *model)
{
	uint32_t *value = NULL;
	uint32_t val_size = 0;
	Gna2TlvStatus status;

	if (!model || !model->data)
		return 0;

	status = Gna2TlvFindInArray(model->data, model->size, Gna2TlvTypeGnaHwVersion,
				    &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t))
		return 0;

	return gna_lib_to_ace_version(*value);
}
EXPORT_SYMBOL(inference_model_get_required_hw_version);

struct gna_instance_data *inference_init(void)
{
	struct gna_instance_data *gna = NULL;
	uint8_t *common_scratch = NULL;

	tr_info(&inference_svc_tr, "Inference service init");

	const struct device *gna_dev = gna_get_zephyr_device();

	if (!gna_dev) {
		tr_err(&inference_svc_tr, "GNA device not found!");
		return NULL;
	}

	/* Allocate memory for the GNA context and initialize it. */
	gna = rzalloc(SOF_MEM_FLAG_USER, sizeof(struct gna_instance_data));
	if (!gna) {
		tr_err(&inference_svc_tr, "GNA context allocation failed!");
		return gna;
	}

	gna->dev = gna_dev;

	/* Initialize GNA models list */
	list_init(&gna->gna_model_list);

	/* Allocate common scratch buffer */
	common_scratch = rballoc_align(SOF_MEM_FLAG_USER, MAX_GNA_SCRATCH, CONFIG_MM_DRV_PAGE_SIZE);
	if (!common_scratch) {
		tr_err(&inference_svc_tr, "GNA common scratch allocation failed!");
		rfree(gna);
		return NULL;
	}

	gna->common_scratch = common_scratch;
	gna->common_scratch_size = MAX_GNA_SCRATCH;

	return gna;
}
EXPORT_SYMBOL(inference_init);

int inference_model_init(struct inference_model *model,
			 struct gna_instance_data *gna,
			 uint32_t model_ctx_size,
			 uint32_t gna_dev_instance)
{
	struct gna_model_ctx *model_ctx = NULL;
	const struct device *dev;
	int32_t tlv_status;
	int ret;

	tr_info(&inference_svc_tr, "Inference model init, instance %u", gna_dev_instance);

	if (!gna) {
		tr_err(&inference_svc_tr, "GNA data is NULL!");
		return -EINVAL;
	}

	dev = inference_get_device_by_instance(gna_dev_instance);
	if (!dev) {
		tr_err(&inference_svc_tr, "GNA device instance %u not available",
		       gna_dev_instance);
		return -ENODEV;
	}

	gna->dev = dev;

	/* Allocate model context */
	model_ctx = rzalloc(SOF_MEM_FLAG_USER, model_ctx_size);
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
	gna->model_ctx = NULL;
	return ret;
}
EXPORT_SYMBOL(inference_model_init);

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
	gna->model_ctx = NULL;

	return 0;
}
EXPORT_SYMBOL(inference_model_release);

uint32_t inference_get_model_ctx_size(struct inference_model *model)
{
	return sizeof(struct gna_model_ctx) +
	       gna_model_get_extra_scratch(model->data, model->size);
}
EXPORT_SYMBOL(inference_get_model_ctx_size);

int inference_request_init(struct gna_instance_data *gna, uint32_t request_ctx_size)
{
	struct gna_request_ctx *request_ctx = NULL;
	struct gna_model_ctx *model_ctx = gna->model_ctx;
	int ret;

	tr_info(&inference_svc_tr, "Inference request init");

	/* Allocate GNA request context data*/
	request_ctx = rzalloc(SOF_MEM_FLAG_USER, request_ctx_size);
	if (!request_ctx) {
		tr_err(&inference_svc_tr, "GNA request context allocation failed!");
		return -ENOMEM;
	}
	request_ctx->model = model_ctx;

	/* TODO: Refactor to use gna->request_ctx */
	gna->request_ctx = request_ctx;

	/* Initialize GNA request */
	ret = gna_request_init(gna);
	if (ret) {
		tr_err(&inference_svc_tr, "GNA request initialization failed!");
		goto req_err;
	}

	return ret;

req_err:
	rfree(gna->request_ctx);
	gna->request_ctx = NULL;
	return ret;
}
EXPORT_SYMBOL(inference_request_init);

int inference_request_start_async(struct gna_instance_data *gna)
{
	int ret;

	tr_info(&inference_svc_tr, "Inference request start async");

	if (!gna)
		return -EINVAL;

	ret = gna_request_start(gna);
	if (ret) {
		tr_err(&inference_svc_tr, "GNA request start async failed!");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(inference_request_start_async);

int inference_request_start_yield(struct gna_instance_data *gna)
{
	int ret;

	tr_info(&inference_svc_tr, "Inference request start and yield");

	if (!gna)
		return -EINVAL;

	ret = gna_request_start_and_block(gna);
	if (ret) {
		tr_err(&inference_svc_tr, "GNA request start and yield failed!");
		return ret;
	}

	gna->request_ctx->requests_completed++;

	tr_info(&inference_svc_tr, "Inference request completed! ret=%d, started=%d, completed=%d",
		ret, gna->request_ctx->requests_started, gna->request_ctx->requests_completed);

	return 0;
}
EXPORT_SYMBOL(inference_request_start_yield);

int inference_request_query_status(struct gna_instance_data *gna)
{
	gna_request_status status = gna_request_get_status(gna);

	tr_info(&inference_svc_tr, "Inference request query status=%d", status);

	switch (status) {
	case GNA_REQUEST_COMPLETED:
		return REQUEST_SUCCESS;
	case GNA_REQUEST_WAITING:
	case GNA_REQUEST_IN_PROGRESS:
		return REQUEST_PENDING;
	case GNA_REQUEST_STATUS_TIMEOUT:
	case GNA_REQUEST_ERRED:
	default:
		return REQUEST_ERROR;
	}
}
EXPORT_SYMBOL(inference_request_query_status);

int inference_get_model_scaling_factors(struct gna_model_ctx *model_ctx,
					struct scaling_factors *factors)
{
	if (!model_ctx || !factors)
		return -EINVAL;

	factors->in = model_ctx->input_scale_factor;
	factors->out = model_ctx->output_scale_factor;

	tr_info(&inference_svc_tr,
		"Inference get model scaling factors: in_factor=%d, out_factor=%d",
		(uint32_t)factors->in, (uint32_t)factors->out);

	return 0;
}
EXPORT_SYMBOL(inference_get_model_scaling_factors);

uint32_t inference_get_request_ctx_size(struct gna_model_ctx *model_ctx)
{
	if (!model_ctx) {
		tr_err(&inference_svc_tr, "Invalid model context ptr!");
		return 0;
	}

	return gna_request_get_size(model_ctx);
}
EXPORT_SYMBOL(inference_get_request_ctx_size);

int inference_request_reset(struct gna_instance_data *gna)
{
	if (!gna)
		return -EINVAL;

	tr_info(&inference_svc_tr, "Inference request reset");

	return gna_request_reset(gna);
}
EXPORT_SYMBOL(inference_request_reset);

int inference_request_release(struct gna_instance_data *gna)
{
	tr_info(&inference_svc_tr, "Inference request release");

	if (!gna)
		return -EINVAL;

	gna_request_release(gna);

	rfree(gna->request_ctx);
	gna->request_ctx = NULL;

	return 0;
}
EXPORT_SYMBOL(inference_request_release);

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
EXPORT_SYMBOL(inference_free);

/* ----------- V1 accessors ----------- */

int inference_model_get_ro_data(struct gna_model_ctx *model_ctx,
				const uint8_t **ro_data, size_t *ro_size)
{
	if (!model_ctx || !ro_data || !ro_size)
		return -EINVAL;

	*ro_data = model_ctx->ro;
	*ro_size = model_ctx->ro_size;
	return 0;
}
EXPORT_SYMBOL(inference_model_get_ro_data);

uint32_t inference_model_get_ldt_number(struct gna_model_ctx *model_ctx)
{
	if (!model_ctx)
		return 0;

	return model_ctx->ldt_number;
}
EXPORT_SYMBOL(inference_model_get_ldt_number);

int inference_model_get_user_metadata(struct gna_model_ctx *model_ctx,
				      uint8_t **metadata, size_t *metadata_size)
{
	if (!model_ctx || !metadata || !metadata_size)
		return -EINVAL;

	if (!model_ctx->user_data)
		return -ENODATA;

	*metadata = model_ctx->user_data;
	*metadata_size = model_ctx->user_data_size;

	return 0;
}
EXPORT_SYMBOL(inference_model_get_user_metadata);

/* ----------- V2 layer range ----------- */

int inference_update_layers_range(struct gna_instance_data *gna,
				  uint32_t ldt_layer_start, uint32_t layer_count)
{
	if (!gna || !gna->request_ctx)
		return -EINVAL;

	gna->request_ctx->ldt_layer_start = ldt_layer_start;
	gna->request_ctx->layer_count = layer_count;

	return 0;
}
EXPORT_SYMBOL(inference_update_layers_range);

/* ----------- V3 HPP and parameters ----------- */

int inference_request_get_parameter(struct gna_instance_data *gna,
				    enum inference_request_param_type type,
				    void *out_value, uint32_t out_size,
				    const void *in_value, uint32_t in_size)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_request_get_parameter);

int inference_request_set_parameter(struct gna_instance_data *gna,
				    enum inference_request_param_type type,
				    const void *in_value, uint32_t in_size)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_request_set_parameter);

int inference_register_hpp_client(struct hpp_client_handle *client_id,
				 uint32_t total_icpc,
				 uint32_t gna_dev_instance)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_register_hpp_client);

int inference_unregister_hpp_client(struct hpp_client_handle client_id)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_unregister_hpp_client);

int inference_request_start_hpp_sync(struct gna_instance_data *gna)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_request_start_hpp_sync);

int inference_request_start_hpp_async(struct gna_instance_data *gna)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_request_start_hpp_async);

/* ----------- V4 extended API ----------- */

uint32_t inference_get_model_ctx_size_ex(struct inference_model *model,
					 bool private_scratch)
{
	/* TODO: account for private scratch */
	return inference_get_model_ctx_size(model);
}
EXPORT_SYMBOL(inference_get_model_ctx_size_ex);

int inference_model_init_ex(const struct inference_model_cfg *cfg)
{
	if (!cfg || !cfg->gna)
		return -EINVAL;

	if (!cfg->model.model_data)
		return -EINVAL;

	return inference_model_init(cfg->model.model_data, cfg->gna,
				   cfg->model_ctx_size,
				   cfg->gna_dev_instance);
}
EXPORT_SYMBOL(inference_model_init_ex);

int inference_request_init_ex(const struct inference_request_cfg *cfg)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_request_init_ex);

int inference_register_hpp_client_ex(const struct inference_hpp_client_cfg *cfg)
{
	if (!cfg)
		return -EINVAL;

	return inference_register_hpp_client(cfg->client_id, cfg->total_icpc,
					     cfg->gna_dev_instance);
}
EXPORT_SYMBOL(inference_register_hpp_client_ex);

int inference_request_start_ex(struct gna_instance_data *gna)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_request_start_ex);
