// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Damian Nikodem <damian.nikodem@intel.com>
 */

#include <stddef.h>
#include <sof/audio/nld/api/nld_lib_gen_api.h>
#include <sof/audio/nld/nld.h>
#include <ipc4/nld.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/audio/data_blob.h>
#include <sof/ipc/msg.h>
#include <ipc4/notification.h>
#include "../audio/copier/copier.h"
#include <sof/audio/component_ext.h>

LOG_MODULE_REGISTER(nld, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(nld);
DECLARE_TR_CTX(nld_comp_tr, SOF_UUID(nld_uuid), LOG_LEVEL_INFO);

int *__errno(void)
{ return &errno; }

/**
 * nld_initialize_algo - Initialize the Noise Level Detection (NLD) algorithm.
 * @data: Pointer to the data buffer used for initialization.
 * @size: Size of the data buffer.
 *
 * This function initializes the NLD algorithm using the provided data buffer and size.
 * It calls the `NldInitialize` function and returns the status of the initialization.
 *
 * Return: Status of the NLD IP initialization.
 */
static uint32_t nld_initialize_algo(uint8_t *data, size_t size)
{
	return NldInitialize(data, size);
}


/**
 * nld_get_algo_size - Get the size of the Noise Level Detection (NLD) algorithm data.
 *
 * This function retrieves the size of the data required by the NLD algorithm.
 * It calls the `NldGetDataSize` function and returns the size.
 *
 * Return: Size of the NLD IP algorithm data.
 */
static uint32_t nld_get_algo_size(void)
{
	return NldGetDataSize();
}


/**
 * nld_check_and_clear_result - Check and clear the noise level detection result.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function checks if a noise level has been detected by reading the `detected_`
 * field from the `result` structure within the `model`. It then clears the detection
 * result by setting the `detected_` field to `false`. The function returns the original
 * detection result.
 *
 * Return: `true` if noise was detected, `false` otherwise.
 */
bool nld_check_and_clear_result(struct intel_nld_data *model)
{
	bool output = model->result.detected;

	model->result.detected = false;
	return output;
}


/**
 * nld_check_and_clear_initial_notification - Check and clear the initial notification flag.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function checks if an initial notification has been triggered by reading the
 * `initial_notification_` field from the `result` structure within the `model`. It then
 * clears the initial notification flag by setting the `initial_notification_` field to `false`.
 * The function returns the original state of the initial notification flag.
 *
 * Return: `true` if the initial notification was triggered, `false` otherwise.
 */
bool nld_check_and_clear_initial_notification(struct intel_nld_data *model)
{
	bool output = model->result.initial_notification;

	model->result.initial_notification = false;
	return output;
}


/**
 * nld_check_and_clear_threshold_cross - Check and clear the threshold cross flag.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function checks if the noise level threshold has been crossed by reading the
 * `threshold_cross_` field from the `nld_ctx` structure within the `model`. It then
 * clears the threshold cross flag by setting the `threshold_cross_` field to `false`.
 * The function returns the original state of the threshold cross flag.
 *
 * Return: `true` if the threshold was crossed, `false` otherwise.
 */
bool nld_check_and_clear_threshold_cross(struct intel_nld_data *model)
{
	bool output = model->nld_ctx->inst_det.threshold_cross;

	model->nld_ctx->inst_det.threshold_cross = false;
	return output;
}


/**
 * nld_configure_algo - Configure the Noise Level Detection (NLD) algorithm.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function configures the NLD algorithm with the provided settings from the
 * `NldCfg` structure within the `model`. It sets the algorithm's bit depth,
 * minimum and maximum period, sensitivity, and trigger type. Additionally, it
 * retrieves and copies the configuration blob data into the model's context.
 *
 * Return: Status of the NLD configuration.
 */
static uint32_t nld_configure_algo(struct processing_module *mod)
{
	struct intel_nld_data *model = module_get_private_data(mod);
	struct nld_mod_cfg mod_cfg = model->nld_ctx->mod_cfg;
	uint32_t valid_bit_depth = mod_cfg.input_fmt.valid_bit_depth;
	uint32_t mix_max_period_ms = mod_cfg.min_max_period_ms;
	uint32_t sensitivity_mdb = mod_cfg.sensitivity_mdb;
	struct nld_config nld_config = mod_cfg.nld_config;
	int status;

	model->result.initial_notification = false;

	/* Set algo bit depth */
	status = NldSetConfiguration(model->nld_instance,
					      ConfigModelGeneric, ConfigInParamInputBitDepth,
					      &valid_bit_depth, sizeof(valid_bit_depth));

	if (status != 0)
		return status;

	/* Set algo min max period */
	status = NldSetConfiguration(model->nld_instance,
				     ConfigModelGeneric, ConfigInParamMinMaxPeriod,
				     (void *)&mix_max_period_ms, sizeof(uint32_t));

	if (status != 0)
		return status;

	/* Set algo sensitivity */
	status = NldSetConfiguration(model->nld_instance,
				     ConfigModelGeneric, ConfigInParamSensitivity,
				     (void *)&sensitivity_mdb, sizeof(uint32_t));

	if (status != 0)
		return status;

	/* Set algo trigger type */
	status = NldSetTriggerConfig(model->nld_instance,
				     TriggerTypeNld, TriggerConfigInParamConfig,
				     (void *)&nld_config, sizeof(struct nld_config));

	if (status != 0)
		return status;

	model->config_blob = comp_get_data_blob(model->model_handler,
						&model->config_blob_size,
						NULL);
	if (!model->config_blob)
		return -ENXIO;

	status = memcpy_s(&model->nld_ctx->nld_module_config.notification,
			  sizeof(struct nld_notification),
			  model->config_blob + sizeof(struct nld_cfg_blob_header),
			  sizeof(struct nld_notification));

	if (status != 0)
		return status;

	status = memcpy_s(&model->nld_ctx->nld_module_config.config,
			  sizeof(struct nld_ip_config),
			  model->config_blob + sizeof(struct nld_cfg_blob_header) +
			  sizeof(struct nld_notification),
			  sizeof(struct nld_ip_config));

	return status;
}


/**
 * nld_increase_threshold - Increase the noise level detection threshold.
 * @min_noise_level: The minimum noise level in mBA.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function increases the noise level detection threshold based on the provided
 * minimum noise level. It iterates through the sensitivity levels and updates the
 * current threshold index, increase threshold index, and decrease threshold index
 * if the minimum noise level is greater than the threshold to cross. It also sets
 * the threshold cross flag to true if the threshold is crossed.
 */
void nld_increase_threshold(const uint16_t min_noise_level, struct intel_nld_data *model)
{
	size_t max_threshold =
		model->nld_ctx->nld_module_config.config.sensitivity_levels.thresholds_count;

	for (int idx = model->nld_ctx->inst_det.increase_threshold_idx; idx < max_threshold;
	     idx++) {
		uint16_t current_threshold =
		    model->nld_ctx->nld_module_config.config.sensitivity_levels.thresholds[idx]
			.threshold_mBA;
		uint16_t threshold_to_cross =
		    current_threshold +
		    model->nld_ctx->nld_module_config.config.sensitivity_levels.thresholds[idx]
			.hysteresis_mBA;

		if (min_noise_level < threshold_to_cross ||
		    model->nld_ctx->inst_det.increase_threshold_idx > max_threshold)
			return;

		model->nld_ctx->inst_det.current_threshold_idx = idx;
		model->nld_ctx->inst_det.increase_threshold_idx = idx + 1;
		model->nld_ctx->inst_det.decrease_threshold_idx = idx;
		model->nld_ctx->inst_det.threshold_cross = true;
	}
}


/**
 * nld_decrease_threshold - Decrease the noise level detection threshold.
 * @min_noise_level: The minimum noise level in mBA.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function decreases the noise level detection threshold based on the provided
 * minimum noise level. It iterates through the sensitivity levels and updates the
 * current threshold index, increase threshold index, and decrease threshold index
 * if the minimum noise level is less than the threshold to cross. It also sets
 * the threshold cross flag to true if the threshold is crossed.
 */
void nld_decrease_threshold(const uint16_t min_noise_level, struct intel_nld_data *model)
{
	for (int idx = model->nld_ctx->inst_det.decrease_threshold_idx; idx >= 0; idx--) {
		uint16_t current_threshold =
		    model->nld_ctx->nld_module_config.config.sensitivity_levels.thresholds[idx]
			.threshold_mBA;
		uint16_t threshold_to_cross =
		    current_threshold -
		    model->nld_ctx->nld_module_config.config.sensitivity_levels.thresholds[idx]
			.hysteresis_mBA;

		if (min_noise_level > threshold_to_cross ||
		    model->nld_ctx->inst_det.decrease_threshold_idx < 0)
			return;

		model->nld_ctx->inst_det.current_threshold_idx = idx;
		model->nld_ctx->inst_det.increase_threshold_idx = idx;
		model->nld_ctx->inst_det.decrease_threshold_idx = idx - 1;
		model->nld_ctx->inst_det.threshold_cross = true;
	}
}


/**
 * nld_check_threshold - Check the noise level detection threshold.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function checks the noise level detection threshold based on the provided
 * minimum noise level. It compares the current noise level with the threshold and
 * increases or decreases the threshold accordingly.
 */
static uint32_t nld_check_threshold(struct intel_nld_data *model)
{
	uint16_t current_noise_level =
		model->result.cur_res.min_sound_level_dBA * NLD_DBA_TO_MDB_MULTIPLIER;
	uint16_t current_threshold =
		model->nld_ctx->nld_module_config.config.sensitivity_levels
		.thresholds[model->nld_ctx->inst_det.current_threshold_idx]
		.threshold_mBA;

	if (current_noise_level > current_threshold)
		nld_increase_threshold(current_noise_level, model);
	else if (current_noise_level < current_threshold)
		nld_decrease_threshold(current_noise_level, model);

	return 0;
}


/**
 * nld_check_threshold_notification - Check the threshold notification.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function checks the threshold notification based on the provided
 * minimum noise level. It compares the current noise level with the threshold and
 * increases or decreases the threshold accordingly. It also checks if the threshold
 * has been crossed and sends a notification if the threshold has been crossed.
 */
static void nld_check_threshold_notification(struct intel_nld_data *model)
{
	nld_check_threshold(model);

	model->nld_ctx->inst_det.threshold_frames_count++;

	if (model->nld_ctx->inst_det.threshold_frames_count >=
		model->nld_ctx->nld_module_config.notification.threshold_min_interval /
		NLD_MODULE_INPUT_FRAME_SIZE) {

		if (nld_check_and_clear_threshold_cross(model)) {
			model->result.detected = true;

			model->nld_ctx->inst_det.det_out.current_noise_level_mBA =
				model->result.cur_res.min_sound_level_dBA *
				NLD_DBA_TO_MDB_MULTIPLIER;
			model->nld_ctx->inst_det.det_out.crossed_threshold_level_mBA =
				model->nld_ctx->nld_module_config.config.sensitivity_levels
				.thresholds[model->nld_ctx->inst_det.current_threshold_idx]
				.threshold_mBA;
			model->nld_ctx->inst_det.det_out.notification_reason_mask |=
				NOTIFY_THRESHOLD_CROSSING_MASK;

			model->nld_ctx->inst_det.threshold_frames_count = 0;
		}
	}
}


/**
 * nld_check_periodic_notification - Check the periodic notification.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function checks the periodic notification based on the provided
 * minimum noise level. It compares the current noise level with the threshold and
 * increases or decreases the threshold accordingly. It also checks if the period
 * has passed and sends a notification if the period has passed.
 */
static void nld_check_periodic_notification(struct intel_nld_data *model)
{
	model->nld_ctx->inst_det.period_frames_count++;

	if (model->nld_ctx->inst_det.period_frames_count >=
	    model->nld_ctx->nld_module_config.notification.interval /
	    NLD_MODULE_INPUT_FRAME_SIZE) {
		model->result.detected = true;

		model->nld_ctx->inst_det.det_out.current_noise_level_mBA =
			model->result.cur_res.min_sound_level_dBA * NLD_DBA_TO_MDB_MULTIPLIER;
		model->nld_ctx->inst_det.det_out.notification_reason_mask |=
			NOTIFY_WITH_INTERVAL_MASK;

		model->nld_ctx->inst_det.period_frames_count = 0;
	}
}


/**
 * intel_nld_process_frames - Process the input frames using the Noise Level Detection (NLD)
 * algorithm.
 * @mod: Pointer to the processing module.
 * @input_frame: Pointer to the input frame.
 *
 * This function processes the input frames using the NLD algorithm. It calls the
 * `nld_process_frames` function and returns the status of the processing.
 *
 * Return: Status of the NLD processing.
 */
static uint32_t intel_nld_process_frames(struct processing_module *mod,
				   const int32_t *input_frame)
{
	struct intel_nld_data *model = module_get_private_data(mod);
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	enum error_detector_type dec = 0;

	uint32_t status = NldProcessFrames(model->nld_instance,
					   input_frame,
					   cfg->base_cfg.ibs,
					   model->frames_num);

	if (status == AUDIO_IP_STATUS_NO_DATA) {
		model->result.prev_res = model->result.cur_res;

		model->result.initial_notification = true;

		model->result.cur_res.max_sound_level_dBA = 0;
		model->result.cur_res.min_sound_level_dBA = 0;

		dec = NO_DATA;
	} else if (status == AUDIO_IP_STATUS_SUCCESS) {
		const void *value = NULL;
		uint32_t value_size = 0;

		model->result.prev_res = model->result.cur_res;

		status = NldGetTriggerResult(model->nld_instance,
					     TriggerTypeNld,
					     &value,
					     &value_size);

		if (status == PROCESSING_FAILED) {
			comp_err(mod->dev, "%s: data are available but NldGetTriggerResult failed",
				 __func__);
			return status;
		}

		const struct nld_result *result = value;

		model->result.cur_res.min_sound_level_dBA = result->min_sound_level_dBA;
		model->result.cur_res.max_sound_level_dBA = result->max_sound_level_dBA;

		/* Check if threshold is crossed */
		if (model->nld_ctx->nld_module_config.notification.notification_mask &
		    NOTIFY_THRESHOLD_CROSSING_MASK)
			nld_check_threshold_notification(model);

		/* Check if period have passed */
		if (model->nld_ctx->nld_module_config.notification.notification_mask &
		    NOTIFY_WITH_INTERVAL_MASK)
			nld_check_periodic_notification(model);

		uint16_t current_noise_level_mBA =
			result->min_sound_level_dBA * NLD_DBA_TO_MDB_MULTIPLIER;
		uint16_t crossed_threshold_level_mBA =
			model->nld_ctx->nld_module_config.config.sensitivity_levels
			.thresholds[model->nld_ctx->inst_det.current_threshold_idx].threshold_mBA;

		if (nld_check_and_clear_initial_notification(model)) {
			model->result.detected = true;

			model->nld_ctx->inst_det.det_out.current_noise_level_mBA =
				current_noise_level_mBA;
			model->nld_ctx->inst_det.det_out.crossed_threshold_level_mBA =
				crossed_threshold_level_mBA;
			model->nld_ctx->inst_det.det_out.notification_reason_mask |=
				NOTIFY_INITIAL;
		}
	} else {
		comp_err(mod->dev, "%s: NldProcessFrames failed with status %d",
			 __func__, status);
		dec = PROCESSING_FAILED;
	}

	return dec;
}


/**
 * nld_reset - Reset the Noise Level Detection (NLD) algorithm.
 * @model: Pointer to the intel_nld_data structure.
 *
 * This function resets the NLD algorithm by calling the `NldResetTriggerState` function
 * and setting the trigger type and runtime state to reset.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static uint32_t nld_reset(struct intel_nld_data *model)
{
	return NldResetTriggerState(model->nld_instance,
				    TriggerTypeNld,
				    TriggerRuntimeStateReset);
}


/**
 * find_dai_module - Iterate through the processing modules and perform actions.
 * @mod: Pointer to the processing module.
 * @posn: Pointer to the stream position structure.
 *
 * This function iterates through the processing modules in the pipeline, starting from
 * the given module. It traverses the source buffers to find the DAI component and then
 * iterates through the IPC component list to find all DAI components. For each DAI
 * component found, it retrieves the private data and performs necessary actions.
 */
static uint32_t find_dai_module(struct processing_module *mod, struct sof_ipc_stream_posn *posn)
{
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb;
	int pid;

	pid = dev_comp_pipe_id(dev);

	do {
		sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		dev = sourceb->source;

		if (dev == NULL)
			return -EINVAL;

	} while (dev_comp_type(dev) != SOF_COMP_DAI);

	mod = comp_mod(dev);
	dev = mod->dev;
	comp_position(dev, posn);

	posn->dai_posn = posn->dai_posn / ((mod->stream_params->sample_valid_bytes) *
			 mod->stream_params->channels);

	return 0;
}


/**
 * nld_ip_initialize - Initialize the Noise Level Detection (NLD) algorithm.
 * @mod: Pointer to the processing module.
 * @data: Pointer to the data buffer.
 * @size: Size of the data buffer.
 *
 * This function initializes the NLD algorithm using the provided data buffer and size.
 * It calls the `NldInitialize` function and returns the status of the initialization.
 *
 * Return: Status of the NLD IP initialization.
 */
static int nld_ip_initialize(struct processing_module *mod,
			     uint8_t *data,
			     size_t size)
{
	struct intel_nld_data *cd = module_get_private_data(mod);
	uint32_t nld_ip_status;
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	int ret;

	comp_dbg(mod->dev, "nld_ip_initialize()");

	cd->nld_algo_size = nld_get_algo_size();
	if (cd->nld_algo_size > size) {
		comp_err(mod->dev,
			 "NldGetDataSize error: nld_ip_size %d, alocated_size %d",
			 cd->nld_algo_size,  size);
		return -EINVAL;
	}


	nld_ip_status = nld_initialize_algo(data, cd->nld_algo_size);
	if (nld_ip_status != AUDIO_IP_STATUS_SUCCESS) {
		comp_err(mod->dev, "NldInitialize failed! ip_status: %d",
			 nld_ip_status);
		return -EINVAL;
	}

	cd->nld_ctx->mod_cfg.min_max_period_ms = NLD_DEFAULT_MEASURE_PERIOD_MS;
	cd->nld_ctx->mod_cfg.sensitivity_mdb = NLD_SENSITIVITY_MDB;
	cd->nld_ctx->mod_cfg.nld_config.enabled = 1;
	cd->nld_ctx->mod_cfg.input_fmt = cfg->base_cfg.audio_fmt;
	cd->nld_ctx->mod_cfg.frame_size_ms = NLD_MODULE_INPUT_FRAME_SIZE;

	ret = comp_init_data_blob(cd->model_handler,
				  CONFIG_BLOB_BUFFER_SIZE,
				  NULL);

	if (ret)
		comp_err(mod->dev, "init data blob failed!");

	return ret;
}


/**
 * nld_configure_algo_on_new_blob - Configure the NLD algorithm with a new configuration blob.
 * @mod: Pointer to the processing module.
 * @pos: Position of the configuration fragment.
 * @data_offset_size: Offset size of the data.
 * @fragment: Pointer to the configuration fragment.
 * @fragment_size: Size of the configuration fragment.
 *
 * This function configures the Noise Level Detection (NLD) algorithm using a new
 * configuration blob. It handles different positions of the configuration fragment
 * (first, single, last) and initializes the NLD instance if necessary. The function
 * sets the configuration data blob and calls the `nld_configure_algo` function if
 * the last fragment is received.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static uint32_t nld_configure_algo_on_new_blob(struct processing_module *mod,
					       enum module_cfg_fragment_position pos,
					       uint32_t data_offset_size,
					       const uint8_t *fragment,
					       size_t fragment_size)
{
	struct intel_nld_data *cd = module_get_private_data(mod);
	int ret, nld_ip_status;

	comp_dbg(mod->dev, "intel_nld_handle_blob()");

	if (pos == MODULE_CFG_FRAGMENT_SINGLE || pos == MODULE_CFG_FRAGMENT_FIRST) {
		nld_ip_status = nld_ip_initialize(mod, cd->nld_instance, NLD_INSTANCE_SIZE);
		if (nld_ip_status != AUDIO_IP_STATUS_SUCCESS) {
			comp_err(mod->dev, "NldInitialize failed! ip_status: %d",
				 nld_ip_status);
			return -EINVAL;
		}
	}

	ret = comp_data_blob_set(cd->model_handler, pos, data_offset_size,
				 fragment, fragment_size);

	if (ret) {
		comp_err(mod->dev, "failed to set blob!");
		return ret;
	}

	if (pos == MODULE_CFG_FRAGMENT_LAST || pos == MODULE_CFG_FRAGMENT_SINGLE)
		ret = nld_configure_algo(mod);

	return ret;
}


/**
 * intel_nld_apply_config - Apply the configuration to the Noise Level Detection (NLD) module.
 * @mod: Pointer to the processing module.
 *
 * This function applies the configuration to the NLD module. It checks the input format
 * configuration data and allocates memory for the NLD processing buffer. It sets the
 * basic parameters for the NLD processing buffer and initializes the NLD instance. The
 * function also initializes the data blob handler and sets the configuration blob data.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int intel_nld_apply_config(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	struct intel_nld_data *cd = module_get_private_data(mod);
	int ret;
	size_t nld_1ms_size = (cfg->base_cfg.audio_fmt.sampling_frequency / 1000) *
			       cfg->base_cfg.audio_fmt.channels_count *
			       (cfg->base_cfg.audio_fmt.depth >> 3);

	if (nld_1ms_size != NLD_16_ONE_MS_SIZE && nld_1ms_size != NLD_24_32_ONE_MS_SIZE) {
		comp_err(mod->dev, "Incorrect NLD Module input format configuration data!");
		return -EINVAL;
	}

	comp_dbg(mod->dev, "intel_nld_apply_config()");

	cd->ipc4_cfg = cfg->base_cfg;
	cd->config_blob = NULL;
	cd->config_blob_size = 0;
	cd->nld_ip_data_chunk = (cfg->base_cfg.ibs >> 1);
	cd->frame_size = (cfg->base_cfg.ibs >> 2);
	cd->frames_num = cd->nld_ip_data_chunk / cd->frame_size;

	/* component model data handler */
	cd->model_handler = comp_data_blob_handler_new_ext(mod->dev, true, NULL, NULL);
	if (!cd->model_handler) {
		ret = -ENOMEM;
		goto nld_fail;
	}

	ret = comp_init_data_blob(cd->model_handler,
				  CONFIG_BLOB_BUFFER_SIZE,
				  NULL);
	if (ret) {
		comp_err(mod->dev, "init data blob failed!");
		goto blob_fail;
	}

	return 0;

blob_fail:
comp_data_blob_handler_free(cd->model_handler);

nld_fail:
rfree(cd->nld_instance);
rfree(cd->nld_ctx);
rfree(cd);
return ret;
}


/**
 * intel_nld_init - Initialize the Noise Level Detection (NLD) module.
 * @mod: Pointer to the processing module.
 *
 * This function initializes the NLD module by setting up the necessary private data
 * and registering the Key Phrase Detected notification. It retrieves the private data
 * of the module, sets up the stream parameters, and registers the notification producer.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int intel_nld_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	size_t ipc_cfg_size = cfg->size;
	struct intel_nld_data *cd;
	uint32_t ret = 0;

	/* make sure data size is not bigger than config space */
	if (ipc_cfg_size != sizeof(struct ipc4_intel_nld_module_cfg)) {
		comp_err(mod->dev, "wrong ipc config size: %u", ipc_cfg_size);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_FLAG_USER, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	cd->nld_algo_size = nld_get_algo_size();

	/* allocate memory for nld ip instance */
	cd->nld_instance = rmalloc(SOF_MEM_FLAG_USER, cd->nld_algo_size);

	if (!cd->nld_instance) {
		rfree(cd);
		comp_err(mod->dev, "failed to allocate nld instance size: %d",
				NLD_INSTANCE_SIZE);
		return -ENOMEM;
	}

	cd->nld_ctx = rmalloc(SOF_MEM_FLAG_USER, sizeof(struct nld_ctx));

	if (!cd->nld_ctx) {
		rfree(cd->nld_instance);
		rfree(cd);
		comp_err(mod->dev, "failed to allocate nld ctx size");
		return -ENOMEM;
	}

	md->private = cd;

	/* Set stream direction for component*/
	mod->dev->direction = SOF_IPC_STREAM_CAPTURE;
	mod->dev->direction_set = true;
	mod->dev->state = COMP_STATE_READY;
	mod->dev->ipc_config.proc_domain = COMP_PROCESSING_DOMAIN_DP;

	ret = intel_nld_apply_config(mod);

	if (ret)
		return ret;

	return 0;
}


/**
 * intel_nld_set_large_config - Set a large configuration for the Noise Level Detection module.
 * @mod: Pointer to the processing module.
 * @config_id: Configuration ID.
 * @pos: Position of the configuration fragment.
 * @data_offset_size: Offset size of the data.
 * @fragment: Pointer to the configuration fragment.
 * @fragment_size: Size of the configuration fragment.
 * @response: Pointer to the response buffer.
 * @response_size: Size of the response buffer.
 *
 * This function sets a large configuration for the NLD module based on the provided
 * configuration ID. It handles different configuration IDs and calls the appropriate
 * configuration function. The function returns the status of the configuration.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int intel_nld_set_large_config(struct processing_module *mod,
				      uint32_t config_id,
				      enum module_cfg_fragment_position pos,
				      uint32_t data_offset_size,
				      const uint8_t *fragment,
				      size_t fragment_size,
				      uint8_t *response, size_t response_size)
{
	int ret;

	comp_dbg(mod->dev, "intel_nld_set_large_config()");

	switch (config_id) {
	case NLD_CONFIG_PARAM_NEWBLOB:
		ret = nld_configure_algo_on_new_blob(mod, pos, data_offset_size,
						     fragment, fragment_size);
		return ret;
	default:
		comp_err(mod->dev, "unknown config id %u", config_id);
		return -EINVAL;
	}

	return ret;
}


/**
 * intel_nld_prepare - Prepare the Noise Level Detection (NLD) module for processing.
 * @mod: Pointer to the processing module.
 * @sources: Array of source streams.
 * @num_of_sources: Number of source streams.
 * @sinks: Array of sink streams.
 * @num_of_sinks: Number of sink streams.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int intel_nld_prepare(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	return 0;
}


/**
 * nld_get_and_clear_detection_output - Retrieve and clear the noise level detection output.
 * @mod: Pointer to the processing module.
 *
 * This function retrieves the current noise level detection output from the module's
 * private data and then clears the detection output fields. It returns the retrieved
 * detection output.
 *
 * Return: The current noise level detection output.
 */
struct nld_output nld_get_and_clear_detection_output(struct processing_module *mod)
{
	struct intel_nld_data *cd = module_get_private_data(mod);
	struct nld_output output;

	output.current_noise_level_mBA =
		cd->nld_ctx->inst_det.det_out.current_noise_level_mBA;
	output.crossed_threshold_level_mBA =
		cd->nld_ctx->inst_det.det_out.crossed_threshold_level_mBA;
	output.notification_reason_mask =
		cd->nld_ctx->inst_det.det_out.notification_reason_mask;

	cd->nld_ctx->inst_det.det_out.current_noise_level_mBA = 0;
	cd->nld_ctx->inst_det.det_out.crossed_threshold_level_mBA = 0;
	cd->nld_ctx->inst_det.det_out.notification_reason_mask = 0;

	return output;
}


/**
 * nld_notification_init - Initialize a notification message for Noise Level Detection (NLD).
 * @mod: Pointer to the processing module.
 * @result: The result of the noise level detection.
 *
 * This function initializes an IPC message to notify the host about the noise level
 * detection result. It sets up the message header, fills in the payload with the
 * detection result and other relevant information, and returns the prepared message.
 *
 * Return: Pointer to the initialized IPC message, or NULL on failure.
 */
static struct ipc_msg *nld_notification_init(struct processing_module *mod,
					     struct nld_output result)
{
	static const uint32_t uuid_dword[4] = CONFIG_ADSP_NLD_DETECTION_UUID;
	struct intel_nld_data *cd = module_get_private_data(mod);
	struct ipc_msg msg_init;
	union ipc4_notification_header *primary =
		(union ipc4_notification_header *)&msg_init.header;
	struct ipc_msg *msg;
	struct detector_event_payload payload = {0};
	struct sof_ipc_stream_posn posn;
	uint64_t timer = 0;
	uint32_t ret;

	memset_s(&msg_init, sizeof(msg_init), 0, sizeof(msg_init));

	primary->r.notif_type = SOF_IPC4_MODULE_NOTIFICATION;
	primary->r.type = SOF_IPC4_GLB_NOTIFICATION;
	primary->r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	primary->r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;

	msg = ipc_msg_w_ext_init(msg_init.header,
				 msg_init.extension,
				 sizeof(struct detector_event_payload));

	if (!msg)
		return NULL;

	/* Find the DAI module that provides the audio stream
	 * The implementation of find_dai_module will be need
	 * to be updated when the pipeline 2.0 is implemented.
	 */
	ret = find_dai_module(mod, &posn);

	if (ret != 0)
		return NULL;

	timer = sof_cycle_get_64();

	payload.mod_id = mod->dev->ipc_config.id;
	payload.event_type = MODULE_NOTIFICATION_DETECTOR_EVENT;
	payload.event_data_size = sizeof(struct detector_event_payload);

	ret = memcpy_s(payload.event_guid, sizeof(payload.event_guid),
		       uuid_dword, sizeof(uuid_dword));

	if (ret != 0)
		return NULL;

	payload.current.timer_value = timer;
	payload.current.linear_link_position = posn.dai_posn;
	payload.event_start.timer_value = timer;
	payload.event_end.timer_value = timer;
	payload.result.structure_version.part.major = 0x1;
	payload.result.structure_version.part.minor = 0x0;
	payload.result.structure_version.part.hotfix = 0x0;
	payload.result.current_noise_level_mBA = result.current_noise_level_mBA;
	payload.result.crossed_threshold_level_mBA = result.crossed_threshold_level_mBA;
	payload.result.notification_reason_mask = result.notification_reason_mask;

	ret = memcpy_s(msg->tx_data, sizeof(struct detector_event_payload),
		 &payload, sizeof(struct detector_event_payload));

	if (ret != 0)
		return NULL;

	return msg;
}


/**
 * nld_detected_notify_host - Notify the host about a detected noise level change.
 * @mod: Pointer to the processing module.
 *
 * This function prepares and sends a notification to the host when a noise level
 * change is detected. It retrieves the private data of the module, logs the detection
 * event, and sends an IPC message to the host.
 */
static void nld_detected_notify_host(struct processing_module *mod)
{
	struct intel_nld_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "nosie_level_detected_notify_host()");

	ipc_msg_send(cd->msg, NULL, true);
}


/**
 * intel_nld_notify_host - Notify the host about the Noise Level Detection (NLD) result.
 * @mod: Pointer to the processing module.
 * @result: The result of the noise level detection.
 *
 * This function prepares and sends a notification to the host about the noise level
 * detection result. It initializes the notification message and sends it to the host.
 * If the initialization fails, it logs an error and returns an appropriate error code.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int intel_nld_notify_host(struct processing_module *mod,
				 struct nld_output result)
{
	struct intel_nld_data *cd = module_get_private_data(mod);
	bool detected = false;
	uint32_t event_size_ms;
	int ret;

	/* Prepare and send NLD notification here */
	cd->msg = nld_notification_init(mod, result);

	if (!cd->msg) {
		comp_err(mod->dev, "NLD notification init failed!");
		ret = -EINVAL;
		return ret;
	}

	nld_detected_notify_host(mod);

	return 0;
}


/**
 * intel_nld_process - Process the Noise Level Detection (NLD) for the given module.
 * @mod: Pointer to the processing module.
 * @sources: Array of source streams.
 * @num_of_sources: Number of source streams.
 * @sinks: Array of sink streams.
 * @num_of_sinks: Number of sink streams.
 *
 * This function processes the input audio data for noise level detection.
 * It fills the processing buffer with stream data, checks if there is enough
 * data to proceed with verification, and then processes the frames. If noise
 * is detected, it notifies the host with the detection output.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int intel_nld_process(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct intel_nld_data *cd = module_get_private_data(mod);
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	struct sof_source *input_stream = sources[0];
	uint32_t avail_bytes;
	uint8_t const *data_ptr;
	uint8_t const *data_begin;
	size_t data_size;
	const int32_t *frame;
	int ret;

	avail_bytes = source_get_data_available(input_stream);

	if (!avail_bytes)
		return 0;

	ret = source_get_data(input_stream, avail_bytes,
			      (void const **)&data_ptr,
			      (void const **)&data_begin, &data_size);

	if (ret)
		return ret;

	comp_info(mod->dev, "debug: data_size=%d, avail_bytes=%d",
			data_size, avail_bytes);

	frame = (const int32_t *)data_ptr;

	ret = intel_nld_process_frames(mod, frame);

	if (ret == 0) {
		if (nld_check_and_clear_result(cd)) {
			struct nld_output result =
				nld_get_and_clear_detection_output(mod);

			intel_nld_notify_host(mod, result);
		}
	}

	source_release_data(input_stream, avail_bytes);

	return 0;
}


/**
 * intel_nld_reset - Reset the Noise Level Detection (NLD) module.
 * @mod: Pointer to the processing module.
 *
 * This function resets the NLD module by calling the `nld_reset` function and
 * setting the processing buffer read and write pointers to the start address.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int intel_nld_reset(struct processing_module *mod)
{
	struct intel_nld_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "intel_nld_reset()");

	nld_reset(cd);

	return 0;
}


/**
 * intel_nld_free - Free the Noise Level Detection (NLD) module.
 * @mod: Pointer to the processing module.
 *
 * This function frees the NLD module by releasing the processing buffer and
 * the NLD instance. It also frees the private data of the module.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
static int intel_nld_free(struct processing_module *mod)
{
	struct intel_nld_data *cd = module_get_private_data(mod);
	int ret;

	comp_dbg(mod->dev, "intel_nld_free()");

	/* Free Intel NLD instance */
	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd->nld_instance);
	rfree(cd->nld_ctx);
	rfree(cd);

	return 0;
}

static struct module_interface nld_interface = {
	.init  = intel_nld_init,
	.prepare = intel_nld_prepare,
	.process = intel_nld_process,
	.set_configuration = intel_nld_set_large_config,
	.reset = intel_nld_reset,
	.free = intel_nld_free
};

DECLARE_MODULE_ADAPTER(nld_interface, nld_uuid, nld_comp_tr);
SOF_MODULE_INIT(nld, sys_comp_module_nld_interface_init);
