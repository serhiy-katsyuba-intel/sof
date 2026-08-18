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
#include <drivers/intel_gna34.h>
#if CONFIG_INTEL_GNA34_7BAR
#include <adsp_memory.h>
#endif


LOG_MODULE_REGISTER(intel_gna, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(intel_gna);

DECLARE_TR_CTX(intel_gna_tr, SOF_UUID(intel_gna_uuid), LOG_LEVEL_INFO);

void gna_lock(struct gna_instance_data *gna)
{
	k_mutex_lock(&gna->lock, K_FOREVER);
}

void gna_unlock(struct gna_instance_data *gna)
{
	k_mutex_unlock(&gna->lock);
}

static void gna_lock_init(struct gna_instance_data *gna)
{
	k_mutex_init(&gna->lock);
}

bool gna_is_scratch_shared(struct gna_model_ctx *model_ctx)
{
	return model_ctx && model_ctx->backend &&
		model_ctx->scratch_ptr == model_ctx->backend->common_scratch;
}

void gna_instance_init(struct gna_instance_data *gna)
{
	if (!gna)
		return;

	list_init(&gna->gna_model_list);
	list_init(&gna->gna_request_list);
	gna_lock_init(gna);
}
EXPORT_SYMBOL(gna_instance_init);

/**
 * Convert GNA lib device version enum to ACE HW version number.
 * Model TLV stores lib enum (e.g. 0x45E), driver caps store ACE number (e.g. 45).
 */
uint32_t gna_lib_to_ace_version(uint32_t lib_ver)
{
	switch (lib_ver) {
	case Gna2DeviceVersionEmbedded3_5:
	case Gna2DeviceVersionEmbeddedAE3_5:
		return 35;
	case Gna2DeviceVersionEmbedded3_6:
		return 36;
	case Gna2DeviceVersionEmbedded4_0:
	case Gna2DeviceVersionEmbedded4_0_CE8:
		return 40;
	case Gna2DeviceVersionEmbedded4_5:
		return 45;
	case Gna2DeviceVersionEmbedded4_6:
		return 46;
	default:
		return 0;
	}
}

/**
 * Check if model HW version matches the actual GNA device.
 * Uses runtime caps.version from the device.
 */
static bool gna_check_hw_version(uint32_t model_lib_ver, uint32_t dev_ace_ver)
{
	uint32_t model_ace_ver = gna_lib_to_ace_version(model_lib_ver);

	return model_ace_ver == dev_ace_ver;
}

int32_t gna_model_parse_tlv(struct gna_instance_data *gna,
			    struct gna_model_ctx *model_ctx)
{
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

	/* Get runtime HW version from device */
	gna_capabilities caps;
	ErrorCode ec = intel_gna34_device_get_caps(gna->dev, &caps);

	if (ec != 0) {
		tr_err(&intel_gna_tr, "Failed to get GNA device caps, ec %d", ec);
		return Gna2TlvStatusVersionNotSupported;
	}

	/* Check GNA HW version*/
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeGnaHwVersion, &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t) || !value) {
		tr_err(&intel_gna_tr, "GNA model tlv: hw version error! status %d len %d",
		       status, val_size);
		return status ? status : Gna2TlvStatusTlvReadError;
	}

	tr_info(&intel_gna_tr, "GNA model tlv: hw version 0x%x, device version %d",
		*value, caps.version);

	ret = gna_check_hw_version(*value, caps.version);
	if (!ret) {
		tr_err(
		    &intel_gna_tr,
		    "GNA model version doesn't match device! model_hw_ver 0x%x dev_ver %d",
		    *value, caps.version);
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

		model_ctx->ro = (uint8_t *)value;
		model_ctx->ro_size = val_size;
	}
#else
	/* For pre-7BAR: LDT and RO data are combined in a single TLV record */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeLayerDescriptorAndRoArrayData, &val_size,
				    (void **)&value);
	if (status) {
		tr_err(&intel_gna_tr, "GNA model tlv: ro buffer error! len %d val %x",
		       val_size, value ? *value : 0);
		return status;
	}

	model_ctx->ro = (uint8_t *)value;
	model_ctx->ro_size = val_size;
#endif

	/* Retrieve GNA initial state buffer address */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeStateData, &val_size, (void **)&value);
	if (status == Gna2TlvStatusSuccess) {
		if (val_size > 0) {
			model_ctx->initial_state_buffer = (uint8_t *)value;
			model_ctx->state_buffer_size = val_size;
		}
	} else if (status == Gna2TlvStatusNotFound) {
		/* Model without inline state data, only state size */
		status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
					    Gna2TlvTypeStateSize, &val_size,
					    (void **)&value);
		if (status == Gna2TlvStatusSuccess) {
			if (val_size != sizeof(uint32_t) || !value) {
				tr_err(&intel_gna_tr,
				       "GNA model tlv: state buffer size error! len %d",
				       val_size);
				return Gna2TlvStatusTlvReadError;
			}
			model_ctx->state_buffer_size = *value;
		} else if (status != Gna2TlvStatusNotFound) {
			return status;
		}
	} else {
		return status;
	}

	tr_info(&intel_gna_tr, "GNA model tlv: state_buffer addr=0x%x, size=%d",
		(uint32_t)model_ctx->initial_state_buffer, model_ctx->state_buffer_size);

	/* Retrieve GNA scratch buffer size */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeScratchSize, &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t) || !value) {
		tr_err(&intel_gna_tr,
		       "GNA model tlv: scratch buffer size error! status %d len %d",
		       status, val_size);
		return status ? status : Gna2TlvStatusTlvReadError;
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
	if (status || val_size != sizeof(uint32_t) || !value) {
		tr_err(&intel_gna_tr,
		       "GNA model tlv: input buffer size error! status %d len %d", status,
		       val_size);
		return status ? status : Gna2TlvStatusTlvReadError;
	}

	model_ctx->input_buffer_size = *value;

	tr_info(&intel_gna_tr, "GNA model tlv: input buffer size %d",
		model_ctx->input_buffer_size);

	/* Retrieve GNA output buffer size */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeOutputBufferSize, &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t) || !value) {
		tr_err(&intel_gna_tr,
		       "GNA model tlv: output buffer size error! status %d len %d", status,
		       val_size);
		return status ? status : Gna2TlvStatusTlvReadError;
	}

	model_ctx->output_buffer_size = *value;

	tr_info(&intel_gna_tr, "GNA model tlv: output buffer size %d",
		model_ctx->output_buffer_size);

	/* Retrieve GNA layer number */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeLayerNumber, &val_size, (void **)&value);
	if (status || val_size != sizeof(uint32_t) || !value) {
		tr_err(&intel_gna_tr, "GNA model tlv: layer number error! status %d len %d",
		       status, val_size);
		return status ? status : Gna2TlvStatusTlvReadError;
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
	if (status == Gna2TlvStatusSuccess) {
		model_ctx->user_data = (uint8_t *)value;
		model_ctx->user_data_size = val_size;
	} else if (status != Gna2TlvStatusNotFound) {
		tr_err(&intel_gna_tr, "GNA model tlv: user data error! status %d len %d",
		       status, val_size);
		return status;
	}

	return 0;
}

int gna_add_model(struct gna_instance_data *gna, struct gna_model_ctx *model_ctx)
{
	const uint64_t service_uuid_offset = GNA_SERVICE_UUID_OFFSET;

	bool found = false;
	int free_model_id = IDX_NOT_FOUND;
	uint32_t tracked_models = 0;
	struct model_ref *model_ref = NULL;
	int ret = 0;

	if (!gna || !model_ctx || !gna_model_get_ro(model_ctx))
		return -EINVAL;

	gna_lock(gna);

	tr_info(&intel_gna_tr, "Adding GNA model");

	if (gna_is_scratch_shared(model_ctx) &&
	    MAX(gna_get_used_common_scratch(gna),
		(size_t)model_ctx->scratch_buffer_size) > gna->common_scratch_size) {
		tr_err(&intel_gna_tr, "GNA model: common scratch buffer is too small");
		ret = -ENOMEM;
		goto add_model_unlock;
	}

	/* Check if model is already tracked */
	for (int i = 0; i < MAX_TRACKED_MODELS; ++i) {
		if (gna->refs[i].model_ro &&
		    gna->refs[i].model_ro == gna_model_get_ro(model_ctx)) {
			model_ref = &gna->refs[i];
			tr_info(&intel_gna_tr, "GNA model refs: reusing tracking slot %d, refs %d",
				i, gna->refs[i].references);
			gna->refs[i].references += 1;
			model_ctx->model_id = gna->refs[i].model_id;
			found = true;
			break;
		}

		if (gna->refs[i].model_ro)
			tracked_models++;
		else if (free_model_id == IDX_NOT_FOUND)
			free_model_id = i;
	}

	if (found)
		goto add_model_publish;

	if (tracked_models >= CONFIG_INTEL_GNA34_MAX_MODELS ||
	    free_model_id == IDX_NOT_FOUND) {
		tr_err(&intel_gna_tr, "GNA model: no free model tracking slot");
		ret = -ENOSPC;
		goto add_model_unlock;
	}

	/* Add new model to tracking list and setup using GNA driver */
	{
		uint64_t uuid;

		tr_info(&intel_gna_tr, "GNA model refs: reusing tracking slot %d, refs %d",
			free_model_id, gna->refs[free_model_id].references);

		model_ref = &gna->refs[free_model_id];
		model_ref->model_ro = gna_model_get_ro(model_ctx);
		model_ref->model_id = NULL;
		model_ref->references = 1;
		memset(model_ref->gna_header, 0, sizeof(model_ref->gna_header));

		uuid = service_uuid_offset + (uint32_t)model_ctx;
		ret = intel_gna34_init_model(gna->dev, &model_ref->model_id,
					     model_ref->gna_header, model_ctx->scratch_ptr, uuid);
		if (ret) {
			tr_err(&intel_gna_tr, "Intel GNA: model initialization error! ret=%d", ret);
			goto add_model_rollback;
		}
		if (!model_ref->model_id) {
			ret = -EIO;
			goto add_model_rollback;
		}

#if CONFIG_INTEL_GNA34_7BAR
		model_ref->model_id->model_ldt_ptr = model_ctx->ldt;
		model_ref->model_id->model_ldt_size = model_ctx->ldt_size;
		model_ref->model_id->model_scratch_size = model_ctx->scratch_buffer_size;
#endif
		ret = intel_gna34_setup_model(gna->dev, model_ref->model_id,
					      model_ctx->ldt_number, model_ctx->ro_size,
					      gna_model_get_ro(model_ctx));
		if (ret) {
			tr_err(&intel_gna_tr, "Intel GNA: model setup error! ret=%d", ret);
			goto add_model_rollback;
		}

		model_ctx->model_id = model_ref->model_id;
		found = true;
	}

	add_model_publish:
	list_item_append(&model_ctx->model_item, &gna->gna_model_list);
	ret = 0;
	goto add_model_unlock;

add_model_rollback:
	if (model_ref->model_id) {
		int destroy_ret = intel_gna34_destroy_model(gna->dev, model_ref->model_id);

		if (destroy_ret) {
			tr_err(&intel_gna_tr,
			       "Intel GNA: model rollback destroy error! ret=%d", destroy_ret);
			ret = destroy_ret;
			goto add_model_unlock;
		}
	}
	model_ref->model_id = NULL;
	model_ref->model_ro = NULL;
	model_ref->references = 0;
	memset(model_ref->gna_header, 0, sizeof(model_ref->gna_header));

add_model_unlock:
	gna_unlock(gna);
	return ret;
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

	if (*scratch_size <= MAX_GNA_SCRATCH)
		return 0;

	return ROUND_UP(*scratch_size, GNA_DRV_BUFFER_ALIGNMENT);
}

int gna_remove_model(struct gna_instance_data *gna, struct gna_model_ctx *model_ctx)
{
	struct model_ref *model_ref = NULL;
	int ret;

	if (!gna || !model_ctx || !gna_model_get_ro(model_ctx))
		return -EINVAL;

	gna_lock(gna);

	tr_info(&intel_gna_tr, "Removing GNA model");

	for (int i = 0; i < MAX_TRACKED_MODELS; ++i) {
		if (gna->refs[i].model_ro &&
		    gna->refs[i].model_ro == gna_model_get_ro(model_ctx)) {
			model_ref = &gna->refs[i];
			if (!model_ref->references) {
				ret = -EINVAL;
				goto remove_model_unlock;
			}
			if (model_ref->references == 1) {
				tr_info(&intel_gna_tr,
					"GNA model: removing model from tracking slot %d", i);

				ret = intel_gna34_destroy_model(gna->dev, model_ref->model_id);
				if (ret) {
					tr_err(&intel_gna_tr,
					       "Intel GNA: model destroy error! ret=%d", ret);
					goto remove_model_unlock;
				}

				model_ref->model_id = NULL;
				model_ref->model_ro = NULL;
				model_ref->references = 0;
			} else {
				model_ref->references--;
			}
			break;
		}
	}

	if (!model_ref) {
		ret = -ENOENT;
		goto remove_model_unlock;
	}

	list_item_del(&model_ctx->model_item);

#if CONFIG_INTEL_GNA34_7BAR
	if (model_ctx->ldt_allocated) {
		rfree(model_ctx->ldt);
		model_ctx->ldt = NULL;
		model_ctx->ldt_allocated = false;
	}
#endif

	ret = 0;

remove_model_unlock:
	gna_unlock(gna);
	return ret;
}

int gna_request_init(struct gna_instance_data *gna, struct gna_request_ctx *req_ctx)
{
	int ret;

	if (!gna || !req_ctx || !req_ctx->model) {
		tr_err(&intel_gna_tr, "GNA data is NULL!");
		return -EINVAL;
	}

	tr_info(&intel_gna_tr, "GNA request initialization");

	req_ctx->completion = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
				      sizeof(*req_ctx->completion));
	if (!req_ctx->completion) {
		tr_err(&intel_gna_tr, "GNA completion allocation failed!");
		return -ENOMEM;
	}

	ret = k_sem_init(req_ctx->completion, 0, 1);
	if (ret)
		goto free_completion;

	req_ctx->cached_request_status = GNA_REQUEST_ERRED;
	req_ctx->input_buffer_size = req_ctx->model->input_buffer_size;
	req_ctx->output_buffer_size = req_ctx->model->output_buffer_size;
	req_ctx->state_buffer_size = req_ctx->model->state_buffer_size;

	/* Allocate GNA request related buffers */
	ret = gna_request_allocate_buffs(gna, req_ctx);
	if (ret) {
		tr_err(&intel_gna_tr, "GNA request buffers allocation failed!");
		goto free_completion;
	}

	/* Reset GNA request */
	ret = gna_request_reset(gna, req_ctx);
	if (ret) {
		tr_err(&intel_gna_tr, "GNA request reset failed!");
		gna_request_free_buffs(req_ctx);
		goto free_completion;
	}

	gna_lock(gna);
	list_item_append(&req_ctx->request_item, &gna->gna_request_list);
	gna_unlock(gna);

	return 0;

free_completion:
	rfree(req_ctx->completion);
	req_ctx->completion = NULL;
	return ret;
}

int gna_request_start(struct gna_instance_data *gna, struct gna_request_ctx *req_ctx)
{
	int ret;
	bool request_registered = false;

	if (!gna || !req_ctx || !req_ctx->model || !req_ctx->completion)
		return -EINVAL;

	if (req_ctx->in_progress)
		return -EBUSY;

	tr_info(&intel_gna_tr, "Starting GNA request");

	k_sem_reset(req_ctx->completion);

	/* Flush and invalidate buffers */
	sys_cache_data_flush_and_invd_range(req_ctx->state_buffer,
					    req_ctx->model->state_buffer_size);
	sys_cache_data_flush_and_invd_range(req_ctx->input_buffer,
					    req_ctx->model->input_buffer_size);
	sys_cache_data_flush_and_invd_range(req_ctx->output_buffer,
					    req_ctx->model->output_buffer_size);

	gna_request_register(req_ctx, true);
	request_registered = true;

	req_ctx->cached_request_status = GNA_REQUEST_WAITING;
	req_ctx->in_progress = true;

	memset(&req_ctx->gna_request, 0, sizeof(req_ctx->gna_request));

	ret = intel_gna34_init_request(
	    gna->dev, &req_ctx->gna_request, req_ctx->model->model_id, req_ctx->state_buffer,
	    req_ctx->model->state_buffer_size, req_ctx->ldt_layer_start, req_ctx->layer_count);
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

	req_ctx->requests_started++;

	tr_info(&intel_gna_tr, "Intel GNA: request enqueued");

	return 0;

error:
	if (request_registered) {
		req_ctx->in_progress = false;
		gna_request_register(req_ctx, false);
	}
	req_ctx->cached_request_status = GNA_REQUEST_ERRED;
	k_sem_give(req_ctx->completion);
	return ret;
}

int gna_request_start_and_block(struct gna_instance_data *gna,
				struct gna_request_ctx *req_ctx)
{
	int ret;

	if (!gna || !req_ctx)
		return -EINVAL;

	tr_info(&intel_gna_tr, "Scheduling GNA request task");

	ret = gna_request_start(gna, req_ctx);
	if (ret) {
		tr_err(&intel_gna_tr, "GNA request start failed!");
		return ret;
	}
	ret = k_sem_take(req_ctx->completion, K_FOREVER);
	if (ret)
		return ret;

	return 0;
}

static void gna_request_done_cb(const struct device *dev, void *context,
				uint32_t request_id, gna_request_status status,
				uint32_t hw_status)
{
	struct gna_request_ctx *ctx = context;

	/* GNA wrote the results behind the core's back, drop any cached lines. */
	if (ctx->output_buffer_size)
		sys_cache_data_invd_range(ctx->output_buffer, ctx->output_buffer_size);
	if (ctx->state_buffer_size)
		sys_cache_data_invd_range(ctx->state_buffer, ctx->state_buffer_size);

	ctx->cached_request_status = status;
	ctx->in_progress = false;
	gna_request_register(ctx, false);
	k_sem_give(ctx->completion);

	tr_info(&intel_gna_tr,
		"done callback: request status=%d, hw_status=0x%08x, in_progress=%d",
		status, hw_status, ctx->in_progress);

	/* TODO: Disable power gating */
}

int gna_request_reset(struct gna_instance_data *gna, struct gna_request_ctx *req_ctx)
{
	int ret;

	if (!gna || !req_ctx || !req_ctx->model)
		return -EINVAL;

	tr_info(&intel_gna_tr, "GNA request reset");

	if (req_ctx->model->state_buffer_size > 0) {
		if (!req_ctx->state_buffer)
			return -EINVAL;

		if (req_ctx->model->initial_state_buffer) {
			ret = memcpy_s(req_ctx->state_buffer, req_ctx->model->state_buffer_size,
				       req_ctx->model->initial_state_buffer,
				       req_ctx->model->state_buffer_size);
			if (ret)
				return ret;
		} else {
			memset(req_ctx->state_buffer, 0, req_ctx->model->state_buffer_size);
		}
	}

	return 0;
}

int gna_request_release(struct gna_instance_data *gna, struct gna_request_ctx *req_ctx)
{
	int ret;

	if (!gna || !req_ctx || !req_ctx->model)
		return -EINVAL;


	tr_info(&intel_gna_tr, "Releasing GNA request");

	if (req_ctx->in_progress) {
		ret = k_sem_take(req_ctx->completion, K_FOREVER);
		if (ret)
			return ret;
	}

	/* Free GNA buffers */
	gna_request_free_buffs(req_ctx);

	gna_lock(gna);
	list_item_del(&req_ctx->request_item);
	gna_unlock(gna);

	rfree(req_ctx->completion);
	req_ctx->completion = NULL;

	return 0;
}

static int gna_request_allocate_buffs(struct gna_instance_data *gna,
					      struct gna_request_ctx *req_ctx)
{
	uintptr_t tail;

	tr_info(&intel_gna_tr, "GNA request buffers allocation");

	if (!gna || !req_ctx || !req_ctx->model)
		return -EINVAL;

	tail = ROUND_UP((uintptr_t)req_ctx + sizeof(*req_ctx), GNA_DRV_BUFFER_ALIGNMENT);
	req_ctx->input_buffer = (uint8_t *)tail;
	tail += ROUND_UP(req_ctx->model->input_buffer_size, GNA_DRV_BUFFER_ALIGNMENT);
	req_ctx->output_buffer = (uint8_t *)tail;
	tail += ROUND_UP(req_ctx->model->output_buffer_size, GNA_DRV_BUFFER_ALIGNMENT);
	req_ctx->state_buffer = req_ctx->model->state_buffer_size ? (uint8_t *)tail : NULL;

	/* GNA only writes the scored elements, the alignment padding must not expose
	 * stale caller-owned context data to the client.
	 */
	memset(req_ctx->output_buffer, 0,
	       ROUND_UP(req_ctx->model->output_buffer_size, GNA_DRV_BUFFER_ALIGNMENT));

	return 0;
}

static void gna_request_free_buffs(struct gna_request_ctx *req_ctx)
{
	tr_info(&intel_gna_tr, "GNA request buffers free");

	req_ctx->input_buffer = NULL;
	req_ctx->output_buffer = NULL;
	req_ctx->state_buffer = NULL;
	req_ctx->input_buffer_size = 0;
	req_ctx->output_buffer_size = 0;
	req_ctx->state_buffer_size = 0;
}

gna_request_status gna_request_get_status(struct gna_instance_data *gna,
					  struct gna_request_ctx *req_ctx)
{
	gna_request_status status = GNA_REQUEST_ERRED;
	uint32_t hw_status = 0;
	int ret;

	if (!gna || !req_ctx || !req_ctx->model)
		return status;

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
	if (reg)
		atomic_inc(&req_ctx->model->active_requests);
	else
		atomic_dec(&req_ctx->model->active_requests);
}

size_t gna_request_get_size(struct gna_model_ctx *model_ctx)
{
	if (!model_ctx)
		return 0;

	return ROUND_UP(sizeof(struct gna_request_ctx), GNA_DRV_BUFFER_ALIGNMENT) +
	       ROUND_UP(gna_model_get_input_buff_size(model_ctx),
			GNA_DRV_BUFFER_ALIGNMENT) +
	       ROUND_UP(gna_model_get_output_buff_size(model_ctx),
			GNA_DRV_BUFFER_ALIGNMENT) +
	       ROUND_UP(gna_model_get_state_buff_size(model_ctx),
			GNA_DRV_BUFFER_ALIGNMENT);
}
