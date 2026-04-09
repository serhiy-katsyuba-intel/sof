// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2024 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#include <errno.h>
#include <rtos/alloc.h>
#include <sof/lib/memory.h>
#include <sof/ipc/topology.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/lib/gna/gna_instance.h>
#include <sof/lib/uuid.h>
#include <sof/lib/gna/gna2-tlv-reader.h>
#include <sof/lib/gna/gna2-tlv.h>
#include <rtos/wait.h>
#if CONFIG_INTEL_GNA34_7BAR
#include <adsp_memory.h>
#endif


LOG_MODULE_REGISTER(intel_gna, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(intel_gna);

DECLARE_TR_CTX(intel_gna_tr, SOF_UUID(intel_gna_uuid), LOG_LEVEL_INFO);

#ifdef __ZEPHYR__

static void gna_lock(struct gna_instance_data *gna)
{
	k_mutex_lock(&gna->lock, K_FOREVER);
}

static void gna_unlock(struct gna_instance_data *gna)
{
	k_mutex_unlock(&gna->lock);
}

static void gna_lock_init(struct gna_instance_data *gna)
{
	k_mutex_init(&gna->lock);
}

#endif

static bool gna_check_hw_version(uint32_t gna_hw)
{
#if CONFIG_ACE_VERSION_1_5
	return gna_hw == GNA_35_VERSION || gna_hw == GNA_35E_VERSION;
#elif CONFIG_ACE_VERSION_2_0
	return gna_hw == GNA_36_VERSION;
#elif CONFIG_ACE_VERSION_3_0
	return gna_hw == GNA_40_VERSION;
#elif CONFIG_ACE_VERSION_4_0
	return gna_hw == GNA_45_VERSION;
#else
	return 0;
#endif
}

int32_t gna_model_parse_tlv(struct gna_instance_data *gna)
{
	struct gna_model_ctx *model_ctx = gna->model_ctx;
	uint32_t *value = NULL;
	uint32_t val_size = 0;
	Gna2TlvStatus status;
	int ret;

	tr_info(&intel_gna_tr, "Parsing model tlv data...");

	/* Verify GNA model*/
	status = Gna2TlvVerifyVersionAndCohesion(model_ctx->model_data,
						 model_ctx->model_size);
	if (status) {
		tr_err(&intel_gna_tr, "GNA model verification failed, status %d", status);
		return status;
	}

	/* Check GNA HW version*/
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeGnaHwVersion, &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t)) {
		tr_err(&intel_gna_tr, "GNA model tlv: hw version error! len %d val %x",
		       val_size, *value);
		return status;
	}

	tr_info(&intel_gna_tr, "GNA model tlv: hw version %d", *value);

	ret = gna_check_hw_version(*value);
	if (!ret) {
		tr_err(
		    &intel_gna_tr,
		    "GNA model version doesn't match platform hardware! model_hw_ver %d",
		    *value);
		return Gna2TlvStatusVersionNotSupported;
	}

	/* Retrieve GNA ro buffer address*/
#if CONFIG_INTEL_GNA34_7BAR
	/* For 7BAR: try separate LDT and RO TLV records first (native 7BAR model),
	 * then check combined L&RD record (backward compatible old model).
	 */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeLayerDescriptorArrayData, &val_size,
				    (void **)&value);
	if (status == Gna2TlvStatusSuccess && val_size > 0) {
		model_ctx->ldt = (uint8_t *)value;
		model_ctx->ldt_size = val_size;

		/* If model data is in IMR (L3), copy LDT to SRAM so GNA
		 * HW can quickly access it.
		 */
#if CONFIG_INTEL_GNA34_7BAR
		if ((uintptr_t)model_ctx->model_data >= L3_MEM_BASE_ADDR &&
		    (uintptr_t)model_ctx->model_data < L3_MEM_BASE_ADDR + L3_MEM_SIZE) {
			uint8_t *ldt_copy = rballoc_align(SOF_MEM_FLAG_USER,
							 model_ctx->ldt_size,
							 GNA_DRV_BUFFER_ALIGNMENT);
			if (!ldt_copy) {
				tr_err(&intel_gna_tr,
				       "GNA model: LDT SRAM copy alloc failed");
				return -ENOMEM;
			}
			memcpy_s(ldt_copy, model_ctx->ldt_size,
				 model_ctx->ldt, model_ctx->ldt_size);
			sys_cache_data_flush_and_invd_range(ldt_copy,
							   model_ctx->ldt_size);
			model_ctx->ldt = ldt_copy;
			model_ctx->ldt_allocated = true;
		}
#endif

		status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
					    Gna2TlvTypeReadOnlyData, &val_size,
					    (void **)&value);
		if (status) {
			tr_err(&intel_gna_tr, "GNA model tlv: ro buffer error! len %d",
			       val_size);
			return status;
		}

		/* Always set RO pointer - for 7BAR, BAR0 needs a valid address
		 * even if RO size is 0 (pointer into TLV blob is valid).
		 */
		model_ctx->ro = (uint8_t *)value;
		model_ctx->ro_size = val_size;
	} else {
		/* Old model with combined LDT+RO record */
		status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
					    Gna2TlvTypeLayerDescriptorAndRoArrayData,
					    &val_size, (void **)&value);
		if (status) {
			tr_err(&intel_gna_tr,
			       "GNA model tlv: ro buffer error! len %d",
			       val_size);
			return status;
		}

		if (val_size > 0) {
			model_ctx->ro = (uint8_t *)value;
			model_ctx->ro_size = val_size;
		}
	}
#else
	/* For pre-7BAR: LDT and RO data are combined in a single TLV record */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeLayerDescriptorAndRoArrayData, &val_size,
				    (void **)&value);
	if (status) {
		tr_err(&intel_gna_tr, "GNA model tlv: ro buffer error! len %d val %x",
		       val_size, *value);
		return status;
	}

	if (val_size > 0) {
		model_ctx->ro = (uint8_t *)value;
		model_ctx->ro_size = val_size;
	}
#endif

	/* Retrieve GNA initial state buffer address */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeStateData, &val_size, (void **)&value);
	if (status == Gna2TlvStatusSuccess) {
		if (val_size > 0) {
			model_ctx->initial_state_buffer = (uint8_t *)value;
			model_ctx->state_buffer_size = val_size;
		}
	} else {
		/* Model without inline state data, only state size */
		status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
					    Gna2TlvTypeStateSize, &val_size,
					    (void **)&value);
		if (status == Gna2TlvStatusSuccess && val_size == sizeof(uint32_t))
			model_ctx->state_buffer_size = *value;
	}

	tr_info(&intel_gna_tr, "GNA model tlv: state_buffer addr=0x%x, size=%d",
		(uint32_t)model_ctx->initial_state_buffer, model_ctx->state_buffer_size);

	/* Retrieve GNA scratch buffer size */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeScratchSize, &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t)) {
		tr_err(&intel_gna_tr,
		       "GNA model tlv: scratch buffer size error! len %d val %x",
		       val_size, *value);
		return status;
	}

	model_ctx->scratch_buffer_size = *value;
	model_ctx->scratch_ptr = gna->common_scratch;

	if (model_ctx->scratch_buffer_size > MAX_GNA_SCRATCH)
		model_ctx->scratch_ptr = model_ctx->gna_extra_scratch_buffer;

	tr_info(&intel_gna_tr, "GNA model tlv: scratch buffer size %d",
		model_ctx->scratch_buffer_size);

	/* Retrieve GNA input buffer size */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeInputBufferSize, &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t)) {
		tr_err(&intel_gna_tr,
		       "GNA model tlv: input buffer size error! len %d val %x", val_size,
		       *value);
		return status;
	}

	model_ctx->input_buffer_size = *value;

	tr_info(&intel_gna_tr, "GNA model tlv: input buffer size %d",
		model_ctx->input_buffer_size);

	/* Retrieve GNA output buffer size */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeOutputBufferSize, &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t)) {
		tr_err(&intel_gna_tr,
		       "GNA model tlv: output buffer size error! len %d val %x", val_size,
		       *value);
		return status;
	}

	model_ctx->output_buffer_size = *value;

	tr_info(&intel_gna_tr, "GNA model tlv: output buffer size %d",
		model_ctx->output_buffer_size);

	/* Retrieve GNA layer number */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeLayerNumber, &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t)) {
		tr_err(&intel_gna_tr, "GNA model tlv: layer number error! len %d val %x",
		       val_size, *value);
		return status;
	}

	model_ctx->ldt_number = *value;

	/* Retrieve GNA input scale factor */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeOVInputScaleFactor, &val_size, (void **)&value);
	if (!status) {
		model_ctx->input_scale_factor = *(float *)value;
	} else {
		model_ctx->input_scale_factor = 1;
		tr_info(&intel_gna_tr,
			"GNA model tlv: input_scale_factor using default value: 1.0");
	}

	/* Retrieve GNA output scale factor */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeOVOutputScaleFactor, &val_size,
				    (void **)&value);
	if (!status) {
		model_ctx->output_scale_factor = *(float *)value;
	} else {
		model_ctx->output_scale_factor = 1;
		tr_info(&intel_gna_tr,
			"GNA model tlv: output_scale_factor using default value: 1.0");
	}

	/* Retrieve GNA user data */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeUserData, &val_size, (void **)&value);
	if (status) {
		tr_err(&intel_gna_tr, "GNA model tlv: user data error! len %d val %x",
		       val_size, *value);
		return status;
	}

	if (val_size > 0) {
		model_ctx->user_data = (uint8_t *)value;
		model_ctx->user_data_size = val_size;
	}

	return 0;
}

int gna_add_model(struct gna_instance_data *gna)
{
	const uint64_t service_uuid_offset = GNA_SERVICE_UUID_OFFSET;
	struct gna_model_ctx *model_ctx = gna->model_ctx;

	/* TODO: optimize func, remove found */
	bool found = false;
	int free_model_id = IDX_NOT_FOUND;
	size_t curr_scratch_size, new_scratch_size;
	int ret;

	tr_info(&intel_gna_tr, "Adding GNA model");

	curr_scratch_size = ROUND_UP(gna_get_used_common_scratch(gna), CONFIG_MM_DRV_PAGE_SIZE);

	/* Add GNA model to models list*/
	list_item_append(&model_ctx->model_item, &gna->gna_model_list);

	new_scratch_size = ROUND_UP(gna_get_used_common_scratch(gna), CONFIG_MM_DRV_PAGE_SIZE);

	if (new_scratch_size > curr_scratch_size) {
		tr_info(&intel_gna_tr, "GNA model: common scratch buffer size increased");

		ret = gna_set_common_scratch(gna, curr_scratch_size, new_scratch_size);
		if (ret) {
			tr_err(&intel_gna_tr,
			       "GNA model: unable to allocate new common scratch buffer!");
			return ret;
		}
	}

	/* Check if model is already tracked */
	for (int i = 0; i < MAX_TRACKED_MODELS; ++i) {
		if (gna->refs[i].model_ro == gna_model_get_ro(model_ctx)) {
			tr_info(&intel_gna_tr, "GNA model refs: reusing tracking slot %d, refs %d",
				i, gna->refs[i].references);
			gna->refs[i].references += 1;
			model_ctx->model_id = gna->refs[i].model_id;
			found = true;
			break;
		}

		if (gna->refs[i].model_ro == NULL && free_model_id == IDX_NOT_FOUND)
			free_model_id = i;
	}

	/* Add new model to tracking list and setup using GNA driver */
	if (!found && free_model_id != IDX_NOT_FOUND) {
		uint64_t uuid;

		tr_info(&intel_gna_tr, "GNA model refs: reusing tracking slot %d, refs %d",
			free_model_id, gna->refs[free_model_id].references);

		gna->refs[free_model_id].model_ro = gna_model_get_ro(model_ctx);
		gna->refs[free_model_id].references += 1;
		memset(gna->refs[free_model_id].gna_header, 0,
		       sizeof(gna->refs[free_model_id].gna_header));

		uuid = service_uuid_offset + (uint32_t)model_ctx;
		ret = intel_gna34_init_model(gna->dev, &gna->refs[free_model_id].model_id,
					     gna->refs[free_model_id].gna_header,
					     model_ctx->scratch_ptr, uuid);
		if (ret) {
			tr_err(&intel_gna_tr, "Intel GNA: model initialization error! ret=%d", ret);
			return ret;
		}

#if CONFIG_INTEL_GNA34_7BAR
		gna->refs[free_model_id].model_id->model_ldt_ptr = model_ctx->ldt;
		gna->refs[free_model_id].model_id->model_ldt_size = model_ctx->ldt_size;
		gna->refs[free_model_id].model_id->model_scratch_size = model_ctx->scratch_buffer_size;
#endif
		ret = intel_gna34_setup_model(gna->dev, gna->refs[free_model_id].model_id,
					      model_ctx->ldt_number, model_ctx->ro_size,
					      gna_model_get_ro(model_ctx));
		if (ret) {
			tr_err(&intel_gna_tr, "Intel GNA: model setup error! ret=%d", ret);
			return ret;
		}

		model_ctx->model_id = gna->refs[free_model_id].model_id;
		found = true;
	}

	return 0;
}

static size_t gna_get_used_common_scratch(struct gna_instance_data *gna)
{
	size_t common_size = 0;
	struct list_item *item;
	size_t size;

	tr_info(&intel_gna_tr, "GNA model: getting used common scratch buffer size");

	list_for_item(item, &gna->gna_model_list) {
		struct gna_model_ctx *model_ctx =
		    list_item(item, struct gna_model_ctx, model_item);
		size = gna_is_scratch_shared(model_ctx)
			   ? gna_model_get_scratch_buff_size(model_ctx)
			   : 0;
		common_size = MAX(common_size, size);
	}
	return common_size;
}

size_t gna_model_get_extra_scratch(const uint8_t *model_data, size_t model_size)
{
	uint32_t *scratch_size = NULL;
	uint32_t size = 0;
	Gna2TlvStatus status;

	tr_info(&intel_gna_tr, "GNA model: getting extra scratch buffer size");

	status = Gna2TlvFindInArray(model_data, model_size, Gna2TlvTypeScratchSize, &size,
				    (void **)&scratch_size);
	if (status || size != sizeof(uint32_t))
		return 0;

	return *scratch_size < MAX_GNA_SCRATCH ? 0 : *scratch_size;
}

int gna_set_common_scratch(struct gna_instance_data *gna, size_t old_size, size_t new_size)
{
	uint8_t *new_scratch = NULL;

	tr_info(&intel_gna_tr, "GNA model: setting common scratch buffer size, old %d new %d",
		old_size, new_size);

	if (new_size > MAX_GNA_SCRATCH) {
		tr_err(&intel_gna_tr, "GNA common scratch buffer size too big!");
		return -EINVAL;
	}

	/* Reallocate new common scratch buffer */
	if (new_size) {
		new_scratch = rbrealloc_align(gna->common_scratch, SOF_MEM_FLAG_USER,
					      new_size, old_size, CONFIG_MM_DRV_PAGE_SIZE);
		if (!new_scratch) {
			tr_err(&intel_gna_tr, "GNA common scratch buffer allocation failed!");
			return -ENOMEM;
		}
	}

	gna->common_scratch = new_scratch;
	gna->common_scratch_size = new_size;

	return 0;
}

int gna_remove_model(struct gna_instance_data *gna)
{
	struct gna_model_ctx *model_ctx = gna->model_ctx;
	bool found = false;
	size_t curr_scratch, new_scratch;
	int ret;

	tr_info(&intel_gna_tr, "Removing GNA model");

	curr_scratch = ROUND_UP(gna_get_used_common_scratch(gna), CONFIG_MM_DRV_PAGE_SIZE);

	list_item_del(&model_ctx->model_item);

	for (int i = 0; i < MAX_TRACKED_MODELS; ++i) {
		if (gna->refs[i].model_ro == gna_model_get_ro(model_ctx)) {
			gna->refs[i].references -= 1;

			if (gna->refs[i].references == 0) {
				tr_info(&intel_gna_tr,
					"GNA model: removing model from tracking slot %d", i);
				gna->refs[i].model_ro = NULL;

				ret = intel_gna34_destroy_model(gna->dev, gna->refs[i].model_id);
				if (ret) {
					tr_err(&intel_gna_tr,
					       "Intel GNA: model destroy error! ret=%d", ret);
					return ret;
				}

				found = true;
				break;
			}
		}
	}

	new_scratch = ROUND_UP(gna_get_used_common_scratch(gna), CONFIG_MM_DRV_PAGE_SIZE);

#if CONFIG_INTEL_GNA34_7BAR
	if (model_ctx->ldt_allocated) {
		rfree(model_ctx->ldt);
		model_ctx->ldt = NULL;
		model_ctx->ldt_allocated = false;
	}
#endif

	if (new_scratch < curr_scratch) {
		tr_info(&intel_gna_tr, "GNA model: common scratch buffer size decreased");
		ret = gna_set_common_scratch(gna, curr_scratch, new_scratch);
		if (ret) {
			tr_err(
			    &intel_gna_tr,
			    "GNA model: unable to allocate new common scratch buffer!");
			return ret;
		}
	}

	return 0;
}

int gna_request_init(struct gna_instance_data *gna)
{
	int ret;

	if (!gna) {
		tr_err(&intel_gna_tr, "GNA data is NULL!");
		return -EINVAL;
	}

	tr_info(&intel_gna_tr, "GNA request initialization");

	/* Allocate GNA request related buffers */
	ret = gna_request_allocate_buffs(gna);
	if (ret) {
		tr_err(&intel_gna_tr, "GNA request buffers allocation failed!");
		return ret;
	}

	/* Reset GNA request */
	ret = gna_request_reset(gna);
	if (ret) {
		tr_err(&intel_gna_tr, "GNA request reset failed!");
		gna_request_free_buffs(gna);
		return ret;
	}

	return 0;
}

int gna_request_start(struct gna_instance_data *gna)
{
	struct gna_request_ctx *req_ctx = gna->request_ctx;
	int ret;

	tr_info(&intel_gna_tr, "Starting GNA request");

	/* Flush and invalidate buffers */
	sys_cache_data_flush_and_invd_range(req_ctx->state_buffer,
					    req_ctx->model->state_buffer_size);
	sys_cache_data_flush_and_invd_range(req_ctx->input_buffer,
					    req_ctx->model->input_buffer_size);
	sys_cache_data_flush_and_invd_range(req_ctx->output_buffer,
					    req_ctx->model->output_buffer_size);

	gna_request_register(req_ctx, true);

	req_ctx->cached_request_status = GNA_REQUEST_WAITING;

	memset(&req_ctx->gna_request, 0, sizeof(req_ctx->gna_request));

	ret = intel_gna34_init_request(
	    gna->dev, &req_ctx->gna_request, req_ctx->model->model_id, req_ctx->state_buffer,
	    req_ctx->model->state_buffer_size, 0, req_ctx->model->ldt_number);
	if (ret) {
		tr_err(&intel_gna_tr, "Intel GNA: request initialization error! ret=%d",
		       ret);
		goto error;
	}

	ret = intel_gna34_request_enqueue(
	    gna->dev, &req_ctx->gna_request, req_ctx->input_buffer,
	    req_ctx->model->input_buffer_size, req_ctx->output_buffer,
	    req_ctx->model->output_buffer_size, gna_request_done_cb, (void *)req_ctx);
	if (ret) {
		tr_err(&intel_gna_tr, "Intel GNA: request enqueue error! ret=%d", ret);
		goto error;
	}

	req_ctx->in_progress = true;

	req_ctx->requests_started++;

	tr_info(&intel_gna_tr, "Intel GNA: request enqueued");

	return 0;

error:
	req_ctx->cached_request_status = GNA_REQUEST_ERRED;
	return ret;
}

int gna_request_start_and_block(struct gna_instance_data *gna)
{
	int ret;

	tr_info(&intel_gna_tr, "Scheduling GNA request task");

	ret = gna_request_start(gna);
	if (ret) {
		tr_err(&intel_gna_tr, "GNA request start failed!");
		return ret;
	}

	/* Wait for GNA request completion */
	if (gna->request_ctx->in_progress) {
		while (gna_request_block_cb(gna))
			wait_delay(12);
	}

	return 0;
}

static void gna_request_done_cb(const struct device *dev, void *context,
				uint32_t request_id, gna_request_status status,
				uint32_t hw_status)
{
	struct gna_request_ctx *ctx = context;

	gna_request_register(ctx, false);

	ctx->cached_request_status = status;
	ctx->in_progress = false;

	tr_info(&intel_gna_tr,
		"done callback: request status=%d, hw_status=0x%08x, in_progress=%d",
		status, hw_status, ctx->in_progress);

	/* TODO: Disable power gating */
}

static bool gna_request_block_cb(struct gna_instance_data *gna)
{
	gna_request_status status = gna_request_get_status(gna);

	tr_info(&intel_gna_tr, "GNA Block CB: request status: %d", status);

	/* If true GNA task is blocked */
	return status == GNA_REQUEST_WAITING || status == GNA_REQUEST_IN_PROGRESS;
}

int gna_request_reset(struct gna_instance_data *gna)
{
	struct gna_request_ctx *req_ctx = gna->request_ctx;
	int ret;

	tr_info(&intel_gna_tr, "GNA request reset");

	if (req_ctx->model->state_buffer_size > 0) {
		ret = memcpy_s(req_ctx->state_buffer, req_ctx->model->state_buffer_size,
			       req_ctx->model->initial_state_buffer,
			       req_ctx->model->state_buffer_size);
		assert(!ret);
	}

	return 0;
}

int gna_request_release(struct gna_instance_data *gna)
{
	struct gna_request_ctx *req_ctx = gna->request_ctx;

	tr_info(&intel_gna_tr, "Releasing GNA request");

	if (req_ctx->in_progress) {
		while (gna_request_block_cb(gna))
			wait_delay(12);
	}

	/* Free GNA buffers */
	gna_request_free_buffs(gna);

	return 0;
}

static int gna_request_allocate_buffs(struct gna_instance_data *gna)
{
	struct gna_request_ctx *req_ctx = gna->request_ctx;
	int ret;

	tr_info(&intel_gna_tr, "GNA request buffers allocation");

	req_ctx->input_buffer = rballoc_align(SOF_MEM_FLAG_USER,
					      req_ctx->model->input_buffer_size,
					      GNA_DRV_BUFFER_ALIGNMENT);
	if (!req_ctx->input_buffer) {
		tr_err(&intel_gna_tr, "GNA request input_buffer allocation failed!");
		return -ENOMEM;
	}

	req_ctx->output_buffer = rballoc_align(SOF_MEM_FLAG_USER,
					       req_ctx->model->output_buffer_size,
					       GNA_DRV_BUFFER_ALIGNMENT);
	if (!req_ctx->output_buffer) {
		tr_err(&intel_gna_tr, "GNA request output_buffer allocation failed!");
		ret = -ENOMEM;
		goto in_err;
	}

	if (req_ctx->model->state_buffer_size) {
		req_ctx->state_buffer = rballoc_align(SOF_MEM_FLAG_USER,
						      req_ctx->model->state_buffer_size,
						      GNA_DRV_BUFFER_ALIGNMENT);
		if (!req_ctx->state_buffer) {
			tr_err(&intel_gna_tr, "GNA request state_buffer allocation failed!");
			ret = -ENOMEM;
			goto out_err;
		}
	}

	return 0;

out_err:
	rfree(req_ctx->output_buffer);
in_err:
	rfree(req_ctx->input_buffer);
	return ret;
}

static void gna_request_free_buffs(struct gna_instance_data *gna)
{
	struct gna_request_ctx *req_ctx = gna->request_ctx;

	tr_info(&intel_gna_tr, "GNA request buffers free");

	if (req_ctx->input_buffer)
		rfree(req_ctx->input_buffer);
	if (req_ctx->output_buffer)
		rfree(req_ctx->output_buffer);
	if (req_ctx->state_buffer)
		rfree(req_ctx->state_buffer);
}

gna_request_status gna_request_get_status(struct gna_instance_data *gna)
{
	struct gna_request_ctx *req_ctx = gna->request_ctx;
	gna_request_status status = GNA_REQUEST_ERRED;
	uint32_t hw_status = 0;
	int ret;

	if (req_ctx->in_progress) {
		ret = intel_gna34_get_request_status(gna->dev, &req_ctx->gna_request, &status,
						     &hw_status);
		if (ret) {
			tr_err(&intel_gna_tr, "Intel GNA: request get status error! ret=%d", ret);
			return status;
		}
	} else {
		status = req_ctx->cached_request_status;
	}

	tr_info(&intel_gna_tr, "GNA request status: %d", status);

	return status;
}

static void gna_request_register(struct gna_request_ctx *req_ctx, bool reg)
{
	req_ctx->model->active_requests += reg ? 1 : -1;
}

size_t gna_request_get_size(struct gna_model_ctx *model_ctx)
{
	return sizeof(struct gna_request_ctx) +
	       ROUND_UP(gna_model_get_input_buff_size(model_ctx),
			GNA_DRV_BUFFER_ALIGNMENT) +
	       ROUND_UP(gna_model_get_output_buff_size(model_ctx),
			GNA_DRV_BUFFER_ALIGNMENT) +
	       ROUND_UP(gna_model_get_state_buff_size(model_ctx),
			GNA_DRV_BUFFER_ALIGNMENT);
}
