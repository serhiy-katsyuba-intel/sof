/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Damian Nikodem <damian.nikodem@intel.com>
 */

#ifndef NLD_LIB_GEN_API_
#define NLD_LIB_GEN_API_

#include "nld_trigger_gen_types.h"

enum nld_runtime_parameter_id { DummyParam_DONT_USE };

/**
 * This header defines generic API for NLD Audio IP.
 *
 * Supported offload configuration types => None (DSP only)
 *
 * Supported Trigger types => @see trigger_type:
 *          Noise Level Detection => @see trigger_typeNld
 */

/**
 *           NLD IP Generic API
 *               NLD Specific Functions
 * --------------------------------------------------------------------------------------------
 *   GENERIC:
 *            INITIALIZATION:
 *              NldGetDataSize
 *              NldInitialize
 *
 *   TRIGGER: [ trigger_type: trigger_typeNld ]
 *            CONFIGURATION:
 *              NldSetTriggerConfig
 *              NldGetTriggerConfig
 *            RUNTIME:
 *              NldProcessFrames
 *              NldHandleTriggerEvent
 *              NldGetTriggerResult
 *              NldResetTriggerState
 * --------------------------------------------------------------------------------------------
 */

uint32_t NldGetDataSize(void);

uint32_t NldInitialize(void *nld_data, uint32_t nld_data_size);

uint32_t NldSetConfiguration(void *nld_data, enum config_model_type model_type,
			     enum config_input_parameter_id param_id, const void *param_value,
			     uint32_t param_size);

uint32_t NldGetConfiguration(void *nld_data, enum config_model_type model_type,
			     enum config_output_parameter_id param_id, const void **out_param_value,
			     uint32_t *out_param_size);

uint32_t NldSetTriggerConfig(void *nld_data, enum trigger_type trigger_type,
			     enum trigger_config_input_parameter_id param_id,
			     const void *param_value,
			     uint32_t param_size);

uint32_t NldGetTriggerConfig(void *nld_data, enum trigger_type trigger_type,
			     enum trigger_config_output_parameter_id param_id,
			     const void **out_param_value, uint32_t *out_param_size);

uint32_t NldSetTriggerRuntimeParam(void *aca_data, enum trigger_type trigger_type,
				   enum nld_runtime_parameter_id param_id, const void *param_value,
				   uint32_t param_size);

uint32_t NldGetTriggerRuntimeParam(void *aca_data, enum trigger_type trigger_type,
				   enum nld_runtime_parameter_id param_id, const void **param_value,
				   uint32_t *param_size);

uint32_t NldProcessFrames(void *nld_data, const void *input, uint32_t frame_size,
			  uint32_t number_of_frames);

uint32_t NldGetTriggerResult(void *nld_data, enum trigger_type trigger_type,
			     const void **detection_result, uint32_t *result_size);

uint32_t NldResetTriggerState(void *nld_data, enum trigger_type trigger_type,
			      enum trigger_reset_type reset_type);

#endif // NLD_LIB_GEN_API_
