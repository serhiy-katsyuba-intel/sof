// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2024 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#include <errno.h>
#include <sof/ipc/topology.h>
#include <sof/lib/gna/gna_instance.h>
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

static void inference_model_context_cleanup(struct gna_model_ctx *model_ctx)
{
#if CONFIG_INTEL_GNA34_7BAR
	if (model_ctx && model_ctx->ldt_allocated && model_ctx->ldt) {
		rfree(model_ctx->ldt);
		model_ctx->ldt = NULL;
		model_ctx->ldt_allocated = false;
	}
#else
	(void)model_ctx;
#endif
}

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
#include <zephyr/device.h>
#include <zephyr/init.h>

#define GET_DEVICE_LIST(node) DEVICE_DT_GET(node),

const struct device *gna_z_dev[] = {
#if CONFIG_INTEL_GNA34
	DT_FOREACH_STATUS_OKAY(intel_gna34, GET_DEVICE_LIST)
#endif
};

struct gna_service_registry {
	struct gna_instance_data *instances;
	uint32_t count;
};

static struct gna_service_registry *gna_registry;

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

struct gna_instance_data *inference_get_instance(uint32_t instance)
{
	if (!gna_registry || instance >= gna_registry->count)
		return NULL;

	return &gna_registry->instances[instance];
}
EXPORT_SYMBOL(inference_get_instance);

static struct gna_instance_data *inference_context_backend(const void *context,
						   bool request_context)
{
	struct list_item *item;

	if (!context || !gna_registry)
		return NULL;

	for (uint32_t i = 0; i < gna_registry->count; i++) {
		struct gna_instance_data *gna = &gna_registry->instances[i];
		struct list_item *contexts = request_context ? &gna->gna_request_list :
			&gna->gna_model_list;

		gna_lock(gna);
		list_for_item(item, contexts) {
			void *candidate = request_context ?
				(void *)list_item(item, struct gna_request_ctx, request_item) :
				(void *)list_item(item, struct gna_model_ctx, model_item);

			if (candidate == context) {
				gna_unlock(gna);
				return gna;
			}
		}
		gna_unlock(gna);
	}

	return NULL;
}
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

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
/*
 * The backend registry is created once during firmware startup and lives for the
 * whole uptime.
 */
static int inference_service_init(void)
{
	struct gna_service_registry *registry;
	struct gna_instance_data *instances;
	uint32_t count;

	count = inference_get_device_count();
	if (!count) {
		tr_info(&inference_svc_tr, "No GNA device present, inference service disabled");
		return 0;
	}

	registry = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT, sizeof(*registry));
	if (!registry) {
		tr_err(&inference_svc_tr, "GNA registry allocation failed!");
		return -ENOMEM;
	}

	/* Backend instances embed a k_mutex, they must not live in cached memory. */
	instances = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
			    count * sizeof(*instances));
	if (!instances) {
		tr_err(&inference_svc_tr, "GNA backend registry allocation failed!");
		rfree(registry);
		return -ENOMEM;
	}

	registry->instances = instances;
	registry->count = count;

	for (uint32_t i = 0; i < count; i++) {
		struct gna_instance_data *gna = &instances[i];

		gna->dev = gna_z_dev[i];
		gna->dev_instance = i;
		gna_instance_init(gna);
		gna->common_scratch = rballoc_align(SOF_MEM_FLAG_USER, MAX_GNA_SCRATCH,
						   CONFIG_MM_DRV_PAGE_SIZE);
		if (!gna->common_scratch) {
			tr_err(&inference_svc_tr,
			       "GNA common scratch allocation failed for instance %u", i);
			for (uint32_t j = 0; j < i; j++)
				rfree(instances[j].common_scratch);
			rfree(instances);
			rfree(registry);
			return -ENOMEM;
		}

		gna->common_scratch_size = MAX_GNA_SCRATCH;
	}

	gna_registry = registry;
	tr_info(&inference_svc_tr, "Inference service ready, %u GNA instances", count);

	return 0;
}

SYS_INIT(inference_service_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
#endif /* CONFIG_ZEPHYR_NATIVE_DRIVERS */

int inference_model_init(struct inference_model *model,
			 struct gna_instance_data *gna,
			 struct gna_model_ctx *model_ctx,
			 const void *owner)
{
	int32_t tlv_status;
	uint32_t model_ctx_size;
	int ret;
	uint32_t gna_dev_instance;

	if (!model || !model->data || !model->size || !model_ctx) {
		tr_err(&inference_svc_tr, "Invalid GNA model or context");
		return -EINVAL;
	}

	/* Extended init will replace instance 0 for owner-based selection. */
	if (!gna)
		gna = inference_get_instance(0);
	if (!gna)
		return -ENODEV;

	gna_dev_instance = gna->dev_instance;
	tr_info(&inference_svc_tr, "Inference model init, instance %u", gna_dev_instance);

	if (inference_get_instance(gna_dev_instance) != gna) {
		tr_err(&inference_svc_tr, "GNA backend does not match instance %u",
		       gna_dev_instance);
		return -ENODEV;
	}

	model_ctx_size = inference_get_model_ctx_size(model);

	memset(model_ctx, 0, model_ctx_size);
	model_ctx->model_data = model->data;
	model_ctx->model_size = model->size;
	model_ctx->owner = owner;
	model_ctx->backend = gna;
	model_ctx->gna_dev_instance = gna_dev_instance;

	tlv_status = gna_model_parse_tlv(gna, model_ctx);
	if (tlv_status) {
		tr_err(&inference_svc_tr, "GNA model parsing cfg failed! status=0x%x",
		       tlv_status);
		ret = tlv_status == -ENOMEM ? -ENOMEM : -EBADMSG;
		goto model_err;
	}

	ret = gna_add_model(gna, model_ctx);
	if (ret) {
		tr_err(&inference_svc_tr, "Unable to add GNA model!");
		goto model_err;
	}

	return ret;

model_err:
	inference_model_context_cleanup(model_ctx);
	memset(model_ctx, 0, model_ctx_size);
	return ret;
}
EXPORT_SYMBOL(inference_model_init);

int inference_model_release(struct gna_model_ctx *model_ctx)
{
	struct gna_instance_data *gna = inference_context_backend(model_ctx, false);
	int ret;

	tr_info(&inference_svc_tr, "Inference model release");

	if (!gna)
		return -EINVAL;

	if (atomic_get(&model_ctx->active_requests)) {
		tr_err(&inference_svc_tr, "GNA model - there are still active requests!");
		return -EBUSY;
	}

	ret = gna_remove_model(gna, model_ctx);
	if (ret) {
		tr_err(&inference_svc_tr, "Unable to remove GNA model!");
		return ret;
	}

	model_ctx->model_id = NULL;
	model_ctx->owner = NULL;
	model_ctx->backend = NULL;
	model_ctx->model_data = NULL;
	model_ctx->model_size = 0;

	return 0;
}
EXPORT_SYMBOL(inference_model_release);

uint32_t inference_get_model_ctx_size(struct inference_model *model)
{
	if (!model || !model->data || !model->size)
		return 0;

	return sizeof(struct gna_model_ctx) +
	       gna_model_get_extra_scratch(model->data, model->size);
}
EXPORT_SYMBOL(inference_get_model_ctx_size);

int inference_request_init(struct gna_model_ctx *model_ctx,
			   struct gna_request_ctx *request_ctx)
{
	struct gna_instance_data *gna = inference_context_backend(model_ctx, false);
	uint32_t request_ctx_size;
	int ret;

	tr_info(&inference_svc_tr, "Inference request init");

	if (!gna || !request_ctx) {
		tr_err(&inference_svc_tr, "Invalid GNA request context");
		return -EINVAL;
	}

	request_ctx_size = gna_request_get_size(model_ctx);

	memset(request_ctx, 0, request_ctx_size);
	request_ctx->model = model_ctx;

	/* Initialize GNA request */
	ret = gna_request_init(gna, request_ctx);
	if (ret) {
		tr_err(&inference_svc_tr, "GNA request initialization failed!");
		goto req_err;
	}

	return ret;

req_err:
	memset(request_ctx, 0, request_ctx_size);
	return ret;
}
EXPORT_SYMBOL(inference_request_init);

static int inference_request_get_buffer(struct gna_request_ctx *request_ctx,
					uint8_t **buffer, size_t *buffer_size,
					uint8_t *request_buffer, size_t request_size)
{
	if (!request_ctx || !buffer || !buffer_size || (!request_buffer && request_size))
		return -EINVAL;

	*buffer = request_buffer;
	*buffer_size = request_size;
	return 0;
}

int inference_request_get_input(struct gna_request_ctx *request_ctx,
				uint8_t **buffer, size_t *buffer_size)
{
	if (!inference_context_backend(request_ctx, true))
		return -EINVAL;

	return inference_request_get_buffer(request_ctx, buffer, buffer_size,
					   request_ctx->input_buffer,
					   request_ctx->model->input_buffer_size);
}
EXPORT_SYMBOL(inference_request_get_input);

int inference_request_get_output(struct gna_request_ctx *request_ctx,
				 uint8_t **buffer, size_t *buffer_size)
{
	if (!inference_context_backend(request_ctx, true))
		return -EINVAL;

	return inference_request_get_buffer(request_ctx, buffer, buffer_size,
					   request_ctx->output_buffer,
					   request_ctx->model->output_buffer_size);
}
EXPORT_SYMBOL(inference_request_get_output);

int inference_request_get_state(struct gna_request_ctx *request_ctx,
				uint8_t **buffer, size_t *buffer_size)
{
	if (!inference_context_backend(request_ctx, true))
		return -EINVAL;

	return inference_request_get_buffer(request_ctx, buffer, buffer_size,
					   request_ctx->state_buffer,
					   request_ctx->model->state_buffer_size);
}
EXPORT_SYMBOL(inference_request_get_state);

int inference_request_start_async(struct gna_request_ctx *request_ctx)
{
	struct gna_instance_data *gna = inference_context_backend(request_ctx, true);
	int ret;

	tr_info(&inference_svc_tr, "Inference request start async");

	if (!gna)
		return -EINVAL;

	ret = gna_request_start(gna, request_ctx);
	if (ret) {
		tr_err(&inference_svc_tr, "GNA request start async failed!");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(inference_request_start_async);

int inference_request_start_yield(struct gna_request_ctx *request_ctx)
{
	struct gna_instance_data *gna = inference_context_backend(request_ctx, true);
	int ret;

	tr_info(&inference_svc_tr, "Inference request start and yield");

	if (!gna)
		return -EINVAL;

	ret = gna_request_start_and_block(gna, request_ctx);
	if (ret) {
		tr_err(&inference_svc_tr, "GNA request start and yield failed!");
		return ret;
	}

	request_ctx->requests_completed++;

	tr_info(&inference_svc_tr, "Inference request completed! ret=%d, started=%d, completed=%d",
		ret, request_ctx->requests_started, request_ctx->requests_completed);

	return 0;
}
EXPORT_SYMBOL(inference_request_start_yield);

int inference_request_query_status(struct gna_request_ctx *request_ctx)
{
	struct gna_instance_data *gna = inference_context_backend(request_ctx, true);
	gna_request_status status;

	if (!gna)
		return REQUEST_ERROR;

	status = gna_request_get_status(gna, request_ctx);

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
	if (!inference_context_backend(model_ctx, false) || !factors)
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
	if (!inference_context_backend(model_ctx, false)) {
		tr_err(&inference_svc_tr, "Invalid model context ptr!");
		return 0;
	}

	return gna_request_get_size(model_ctx);
}
EXPORT_SYMBOL(inference_get_request_ctx_size);

int inference_request_reset(struct gna_request_ctx *request_ctx)
{
	struct gna_instance_data *gna = inference_context_backend(request_ctx, true);

	if (!gna)
		return -EINVAL;

	tr_info(&inference_svc_tr, "Inference request reset");

	return gna_request_reset(gna, request_ctx);
}
EXPORT_SYMBOL(inference_request_reset);

int inference_request_release(struct gna_request_ctx *request_ctx)
{
	struct gna_instance_data *gna = inference_context_backend(request_ctx, true);
	int ret;

	tr_info(&inference_svc_tr, "Inference request release");

	if (!gna)
		return -EINVAL;

	ret = gna_request_release(gna, request_ctx);
	if (ret)
		return ret;

	request_ctx->model = NULL;
	request_ctx->in_progress = false;
	return 0;
}
EXPORT_SYMBOL(inference_request_release);

/* ----------- V1 accessors ----------- */

int inference_model_get_ro_data(struct gna_model_ctx *model_ctx,
				const uint8_t **ro_data, size_t *ro_size)
{
	if (!inference_context_backend(model_ctx, false) || !ro_data || !ro_size)
		return -EINVAL;

	*ro_data = model_ctx->ro;
	*ro_size = model_ctx->ro_size;
	return 0;
}
EXPORT_SYMBOL(inference_model_get_ro_data);

uint32_t inference_model_get_ldt_number(struct gna_model_ctx *model_ctx)
{
	if (!inference_context_backend(model_ctx, false))
		return 0;

	return model_ctx->ldt_number;
}
EXPORT_SYMBOL(inference_model_get_ldt_number);

int inference_model_get_user_metadata(struct gna_model_ctx *model_ctx,
				      uint8_t **metadata, size_t *metadata_size)
{
	if (!inference_context_backend(model_ctx, false) || !metadata || !metadata_size)
		return -EINVAL;

	if (!model_ctx->user_data)
		return -ENODATA;

	*metadata = model_ctx->user_data;
	*metadata_size = model_ctx->user_data_size;

	return 0;
}
EXPORT_SYMBOL(inference_model_get_user_metadata);

/* ----------- V2 layer range ----------- */

int inference_update_layers_range(struct gna_request_ctx *request_ctx,
				  uint32_t ldt_layer_start, uint32_t layer_count)
{
	if (!inference_context_backend(request_ctx, true))
		return -EINVAL;
	if (!layer_count || ldt_layer_start >= request_ctx->model->ldt_number ||
	    layer_count > request_ctx->model->ldt_number - ldt_layer_start)
		return -EINVAL;
	if (request_ctx->in_progress)
		return -EBUSY;

	request_ctx->ldt_layer_start = ldt_layer_start;
	request_ctx->layer_count = layer_count;

	return 0;
}
EXPORT_SYMBOL(inference_update_layers_range);

/* ----------- V3 HPP and parameters ----------- */

int inference_request_get_parameter(struct gna_instance_data *gna,
				    struct gna_request_ctx *request_ctx,
				    enum inference_request_param_type type,
				    void *out_value, uint32_t out_size,
				    const void *in_value, uint32_t in_size)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_request_get_parameter);

int inference_request_set_parameter(struct gna_instance_data *gna,
				    struct gna_request_ctx *request_ctx,
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

int inference_request_start_hpp_sync(struct gna_instance_data *gna,
				     struct gna_request_ctx *request_ctx)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_request_start_hpp_sync);

int inference_request_start_hpp_async(struct gna_instance_data *gna,
				      struct gna_request_ctx *request_ctx)
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
	if (!cfg || !cfg->gna || !cfg->model_ctx)
		return -EINVAL;

	if (!cfg->model.model_data)
		return -EINVAL;
	if (cfg->gna->dev_instance != cfg->gna_dev_instance)
		return -ENODEV;

	return inference_model_init(cfg->model.model_data, cfg->gna, cfg->model_ctx, NULL);
}
EXPORT_SYMBOL(inference_model_init_ex);

int inference_request_init_ex(const struct inference_request_cfg *cfg)
{
	if (!cfg || !cfg->gna || !cfg->model_ctx || !cfg->request_ctx)
		return -EINVAL;
	if (inference_context_backend(cfg->model_ctx, false) != cfg->gna)
		return -EINVAL;

	return inference_request_init(cfg->model_ctx, cfg->request_ctx);
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

int inference_request_start_ex(struct gna_instance_data *gna,
				       struct gna_request_ctx *request_ctx)
{
	return -EINVAL;
}
EXPORT_SYMBOL(inference_request_start_ex);
