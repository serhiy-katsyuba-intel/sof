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
	return gna_hw == GNA_35_VERSION;
#elif CONFIG_ACE_VERSION_2_0
	return gna_hw ==  GNA_36_VERSION;
#elif CONFIG_ACE_VERSION_3_0
	return gna_hw ==  GNA_40_VERSION;
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

	ret = gna_check_hw_version(*value);
	if (!ret) {
		tr_err(
		    &intel_gna_tr,
		    "GNA model version doesn't match platform hardware! model_hw_ver %d",
		    *value);
		return Gna2TlvStatusVersionNotSupported;
	}

	/* Retrieve GNA ro buffer address*/
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

	/* Retrieve GNA initial state buffer address */
	status = Gna2TlvFindInArray(model_ctx->model_data, model_ctx->model_size,
				    Gna2TlvTypeStateData, &val_size, (void **)&value);
	if (status) {
		tr_err(&intel_gna_tr,
		       "GNA model tlv: innitial state buffer error! len %d val %x",
		       val_size, *value);
		return status;
	}

	if (val_size > 0) {
		model_ctx->state_buffer = (uint8_t *)value;
		model_ctx->state_buffer_size = val_size;
	}

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

	curr_scratch_size = ROUND_UP(gna_get_used_common_scratch(gna), PAGE_SIZE);

	/* Add GNA model to models list*/
	list_item_append(&model_ctx->model_item, &gna->gna_model_list);

	new_scratch_size = ROUND_UP(gna_get_used_common_scratch(gna), PAGE_SIZE);

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

	status = Gna2TlvFindInArray(model_data, model_size, Gna2TlvTypeScratchSize, &size,
				    (void **)&scratch_size);
	if (status || size != sizeof(uint32_t))
		return 0;

	return *scratch_size < MAX_GNA_SCRATCH ? 0 : *scratch_size;
}

int gna_set_common_scratch(struct gna_instance_data *gna, size_t old_size, size_t new_size)
{
	uint8_t *new_scratch = NULL;

	if (new_size > MAX_GNA_SCRATCH) {
		tr_err(&intel_gna_tr, "GNA common scratch buffer size too big!");
		return -EINVAL;
	}

	/* Reallocate new common scratch buffer */
	if (new_size) {
		new_scratch = rbrealloc_align(gna->common_scratch, 0, SOF_MEM_CAPS_RAM,
					      new_size, old_size, PLATFORM_PAGE_ALIGN);
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

	curr_scratch = ROUND_UP(gna_get_used_common_scratch(gna), PAGE_SIZE);

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

	new_scratch = ROUND_UP(gna_get_used_common_scratch(gna), PAGE_SIZE);

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
