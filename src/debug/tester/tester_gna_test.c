// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>
//	   Ievgen Ganakov <ievgen.ganakov@intel.com>

#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/lib/gna/gna_model.h>
#include <sof/lib/gna/gna_request.h>
#include <sof/lib/memory.h>
#include <rtos/wait.h>

#if !(CONFIG_GNA_HW_VER_35 || CONFIG_GNA_HW_VER_36 || CONFIG_GNA_HW_VER_40 || \
      CONFIG_GNA_HW_VER_45 || CONFIG_GNA_HW_VER_46)
#error "GNA tester: no built-in model for the configured GNA HW version"
#endif

#include "gna_test/gna0_model.h"
#include "gna_test/gna0_test_data.h"
#include "gna_test/gna1_model.h"
#include "gna_test/gna1_test_data.h"
#include "tester_gna_test.h"

LOG_MODULE_REGISTER(tester_gna, CONFIG_SOF_LOG_LEVEL);

/*
 * This is a simple test case for GNA inference
 * The test case will run a GNA inference request
 * and verify the output buffer against a reference buffer.
 */

static int gna_test_prepare_request(struct processing_module *mod,
				    struct gna_test_data *gna_data)
{
	struct comp_dev *dev = mod->dev;
	uint32_t request_ctx_size, model_in_buff_size;
	int ret;

	request_ctx_size = inference_get_request_ctx_size(gna_data->gna->model_ctx);
	if (request_ctx_size > GNA_REQUEST_CTX_MEM_MAX_SIZE) {
		comp_err(dev, "Request context size is too big!");
		return -EINVAL;
	}

	comp_info(mod->dev, "Inference request initialization. Request ctx size: %d",
		  request_ctx_size);

	ret = inference_request_init(gna_data->gna, request_ctx_size);
	if (ret) {
		comp_err(dev, "Failed to initialize GNA request");
		return -EINVAL;
	}

	model_in_buff_size = gna_model_get_input_buff_size(gna_data->gna->model_ctx);
	if (gna_data->builtin_data->in_buff_size > model_in_buff_size) {
		comp_err(dev, "Builtin input buffer size is too big!");
		return -EINVAL;
	}

	comp_info(dev, "Model ctx buffer sizes: input=%d, output=%d, state=%d",
		  model_in_buff_size, gna_model_get_output_buff_size(gna_data->gna->model_ctx),
		  gna_model_get_state_buff_size(gna_data->gna->model_ctx));
	comp_info(dev, "GNA buffers: model=%p, input=%p, output=%p, scratch=%p",
		  (const void *)gna_data->gna->model_ctx->model_data,
		  (void *)gna_data->gna->request_ctx->input_buffer,
		  (void *)gna_data->gna->request_ctx->output_buffer,
		  (void *)gna_data->gna->model_ctx->scratch_ptr);

	ret = memcpy_s(gna_data->gna->request_ctx->input_buffer, model_in_buff_size,
		       gna_data->builtin_data->in_buff,
		       gna_data->builtin_data->in_buff_size);
	assert(!ret);

	return 0;
}

static inline const uint8_t *gna_test_get_ref_buff(struct gna_test_data *gna_data)
{
	return gna_data->builtin_data->ref_buff;
}

static int gna_test_verify(struct processing_module *mod, struct gna_test_data *gna_data)
{
	const uint8_t *out_buff = gna_request_get_output_buff(gna_data->gna->request_ctx);
	const uint8_t *ref_buff = gna_test_get_ref_buff(gna_data);
	struct comp_dev *dev = mod->dev;

	for (size_t i = 0; i < gna_data->builtin_data->ref_buff_size; i++) {
		if (out_buff[i] != ref_buff[i]) {
			comp_err(dev, "Output buffer verification failed at idx %d, %x != %x",
				 i, out_buff[i], ref_buff[i]);
			return -EINVAL;
		}
	}

	comp_info(dev, "Output buffer verification passed");

	return 0;
}

static int gna_test_unload_model_cleanup(struct processing_module *mod,
					 struct gna_test_data *gna_data)
{
	int ret = 0;

	if (gna_data->gna->model_ctx) {
		ret = inference_model_release(gna_data->gna);
		if (ret)
			comp_err(mod->dev, "Failed to release GNA model");
	}

	inference_free(gna_data->gna);

	return ret;
}

static int gna_test_execute_async(struct processing_module *mod,
				  struct gna_test_data *gna_data)
{
	struct comp_dev *dev = mod->dev;
	int request_status = REQUEST_PENDING;
	int retries;
	int ret;

	comp_info(dev, "Executing GNA test in async mode");

	ret = gna_test_prepare_request(mod, gna_data);
	if (ret) {
		comp_err(mod->dev, "Failed to prepare GNA request");
		return ret;
	}

	comp_info(mod->dev, "Starting infernece request in async mode");
	ret = inference_request_start_async(gna_data->gna);
	if (ret) {
		comp_err(dev, "Failed to start GNA request");
		goto release;
	}

	for (retries = GNA_REQUEST_POLL_RETRIES; retries > 0; retries--) {
		request_status = inference_request_query_status(gna_data->gna);
		if (request_status != REQUEST_PENDING)
			break;

		wait_delay_us(GNA_REQUEST_POLL_INTERVAL_US);
	}

	if (request_status != REQUEST_SUCCESS) {
		comp_err(dev, "GNA request not completed, status %d, retries left %d",
			 request_status, retries);
		ret = -EIO;
		goto release;
	}

	comp_info(mod->dev, "Verifying GNA request and cleaning up");
	ret = gna_test_verify(mod, gna_data);
	if (ret)
		comp_err(mod->dev, "Failed to verify GNA request");

release:
	inference_request_release(gna_data->gna);

	return ret;
}

static int gna_test_execute_yield(struct processing_module *mod,
				  struct gna_test_data *gna_data)
{
	struct comp_dev *dev = mod->dev;
	int request_status;
	int ret;

	comp_info(dev, "Executing GNA test in yield mode");

	ret = gna_test_prepare_request(mod, gna_data);
	if (ret) {
		comp_err(dev, "Failed to prepare GNA request");
		return ret;
	}

	comp_info(dev, "Starting inference request in yield mode");
	ret = inference_request_start_yield(gna_data->gna);
	if (ret) {
		comp_err(dev, "Failed to start GNA request in yield mode");
		goto release;
	}

	request_status = inference_request_query_status(gna_data->gna);
	if (request_status != REQUEST_SUCCESS) {
		comp_err(dev, "GNA yield request completed with status %d", request_status);
		ret = -EIO;
		goto release;
	}

	comp_info(dev, "Verifying GNA yield request and cleaning up");
	ret = gna_test_verify(mod, gna_data);
	if (ret)
		comp_err(dev, "Failed to verify GNA yield request");

release:
	inference_request_release(gna_data->gna);

	return ret;
}

static int gna_test_load_model(struct processing_module *mod,
			       struct gna_test_data *gna_data)
{
	struct comp_dev *dev = mod->dev;
	uint32_t model_ctx_size;
	int ret;

	if (!IS_ALIGNED(gna_data->gna_model.size, gna_data->gna_model.model_alignment)) {
		comp_err(dev, "Model is not aligned!");
		return -EINVAL;
	}

	model_ctx_size = inference_get_model_ctx_size(&gna_data->gna_model);
	if (model_ctx_size > GNA_MODEL_CTX_MEM_MAX_SIZE) {
		comp_err(dev, "Model context size is too big!");
		return -EINVAL;
	}

	comp_info(mod->dev, "Inference model initialization. Model ctx size: %d",
		  model_ctx_size);

	ret = inference_model_init(&gna_data->gna_model, gna_data->gna, model_ctx_size,
				   gna_data->gna_dev_instance);
	if (ret) {
		comp_err(dev, "Failed to initialize GNA model");
		return -EINVAL;
	}

	return 0;
}

static int gna_test_select_instance(struct processing_module *mod,
				    const struct inference_model *model,
				    uint32_t preferred, uint32_t *instance)
{
	struct comp_dev *dev = mod->dev;
	uint32_t count = inference_get_device_count();
	uint32_t model_ver = inference_model_get_required_hw_version(model);
	uint32_t i;

	if (!count || !model_ver) {
		comp_err(dev, "No GNA device (count %d) or unknown model HW version (%d)",
			 count, model_ver);
		return -ENODEV;
	}

	comp_info(dev, "GNA instances: %d, model requires HW version %d", count, model_ver);

	/* Start from the instance suggested by the IPC model_id and wrap around, so that
	 * model_id maps 1:1 to the instance whenever the platform has enough of them.
	 */
	for (i = 0; i < count; i++) {
		uint32_t idx = (preferred + i) % count;
		const struct device *gna_dev = inference_get_device_by_instance(idx);
		gna_capabilities caps;

		if (!gna_dev || intel_gna34_device_get_caps(gna_dev, &caps))
			continue;

		comp_info(dev, "GNA instance %d: HW version %d", idx, caps.version);

		if (caps.version == model_ver) {
			*instance = idx;
			return 0;
		}
	}

	comp_err(dev, "No GNA instance matches model HW version %d", model_ver);

	return -ENOTSUP;
}

static int tester_gna_test(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	uint32_t passed_iter;
	int cleanup_ret;
	int ret;

	const struct gna_builtin_data gna_models[GNA_BUILTIN_MODELS_COUNT] = {
		{gna0_model_bin, gna0_model_bin_len, gna0_input_bin, gna0_input_bin_len,
		 gna0_ref_bin, gna0_ref_bin_len},
		{gna1_model_bin, gna1_model_bin_len, gna1_input_bin, gna1_input_bin_len,
		 gna1_ref_bin, gna1_ref_bin_len}};

	struct gna_test_ipc_data *ipc_data = (struct gna_test_ipc_data *)cfg->init_data;

	if (cfg->size != sizeof(struct gna_test_ipc_data)) {
		comp_err(dev, "Invalid GNA IPC data size");
		return -EINVAL;
	}

	if (ipc_data->model_id >= GNA_BUILTIN_MODELS_COUNT) {
		comp_err(dev, "Invalid GNA model ID");
		return -EINVAL;
	}

	struct gna_test_data test_data = {
		.gna = NULL,
		.model_ctx = NULL,
		.request_ctx = NULL,
		.builtin_data = &gna_models[ipc_data->model_id],
		.test_mode = ipc_data->test_mode,
		.iterations = ipc_data->test_iterations,
		.gna_model = {GNA_MODEL_MEMORY_ALIGNMENT,
				gna_models[ipc_data->model_id].model,
				gna_models[ipc_data->model_id].model_size}};

	ret = gna_test_select_instance(mod, &test_data.gna_model, ipc_data->model_id,
				       &test_data.gna_dev_instance);
	if (ret) {
		comp_err(dev, "Failed to select GNA instance");
		return ret;
	}

	comp_info(dev, "Initializing IES for GNA test");

	test_data.gna = inference_init();
	if (!test_data.gna) {
		comp_err(dev, "Failed to initialize IES");
		return -EINVAL;
	}

	comp_info(dev, "Loading GNA model %d on instance %d", ipc_data->model_id,
		  test_data.gna_dev_instance);

	ret = gna_test_load_model(mod, &test_data);
	if (ret) {
		comp_err(dev, "Failed to load GNA model");
		goto cleanup;
	}

	for (passed_iter = 0; passed_iter < test_data.iterations; passed_iter++) {
		comp_info(dev, "Running GNA test iteration %d", passed_iter);

		switch (test_data.test_mode) {
		case async:
			ret = gna_test_execute_async(mod, &test_data);
			break;
		case yield:
			ret = gna_test_execute_yield(mod, &test_data);
			break;
		default:
			comp_err(dev, "Unsupported GNA test mode %d", test_data.test_mode);
			ret = -EINVAL;
			break;
		}

		if (ret) {
			comp_err(dev, "Failed to execute GNA test mode %d", test_data.test_mode);
			goto cleanup;
		}
	}

cleanup:
	comp_info(dev, "Unload GNA model and cleanup");
	cleanup_ret = gna_test_unload_model_cleanup(mod, &test_data);

	return ret ? ret : cleanup_ret;
}

static int gna_test_case_init(struct processing_module *mod, void **ctx)
{
	return tester_gna_test(mod);
}

static int gna_test_free(void *ctx, struct processing_module *mod)
{
	rfree(ctx);
	return 0;
}

const struct tester_test_case_interface tester_interface_gna_test = {
	.init = gna_test_case_init,
	.free = gna_test_free
};
