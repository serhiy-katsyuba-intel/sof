/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Damian Nikodem <damian.nikodem@intel.com>
 */

#ifndef NLD_TRIGGER_GEN_API_
#define NLD_TRIGGER_GEN_API_

#include "trigger_audio_ip_generic_api.h"

/*
 * This header defines generic API for SED Audio IP.
 * Supported offload configuration types => None (DSP only)
 * Supported Trigger types => @see TriggerType:
 *          Noise Level Detection => @see TriggerTypeNld
 */

/*
 *              IP Generic API
 *        Nld Specific Types Definitions
 *          TriggerType: TriggerTypeNld
 * --------------------------------------------------------------------------------------------
 *         CONFIGURATION:
 * --------------------------------------------------------------------------------------------
 *
 * IMPORTANT: These structures are defined to meet sensing stack requirements and any change needs
 * confirmation by sensing stack arch
 */

struct nld_version {
	int32_t version; //!< Noise Level Detector version
};

/*
 * We assume some parameters will not be visible to user but will be part of algorithm tuning.
 * Tuning configuration parameters:
 * - time constant for A curve [ms]
 */

/*
 * Structure for storing result from Noise Level Detector
 * Sound level unit: dBA
 * (dBA levels are "A" weighted according to the "A" weighting curve to approximate the way the
 * human ear hears)
 */
struct nld_result {
	float min_sound_level_dBA; //!< minimum noise level from the given/configured min-max period
				   //!< [dBA]
	float max_sound_level_dBA; //!< maximum noise level from the given/configured min-max period
				   //!< [dBA]
};

/* End of sensing stack pamateres list. */

/*
 *              IP Generic API
 *        Nld Specific Types Definitions
 *          TriggerType: TriggerTypeNld
 * --------------------------------------------------------------------------------------------
 *         CONFIGURATION:
 * --------------------------------------------------------------------------------------------
 */

/*
 * structure storing output configuration to be set/get using NldSetTriggerConfig /
 * NldGetTriggerConfig & parameter id equal to TriggerConfigInParamConfig /
 * TriggerConfigOutParamConfig
 * @note single structure is passed as a parameter, not an array (trigger count is 1)
 * @see NldGetTriggerConfig
 * @see NldSetTriggerConfig
 * @see TriggerConfigOutputParameterId
 * @see TriggerConfigInputParameterId
 */
struct nld_config {
	uint8_t enabled;
};

enum config_input_parameter_min_max_period {
	ConfigInParamMinMax10ms = 10,
	ConfigInParamMinMax500ms = 500,
	ConfigInParamMinMax1000ms = 1000,
};

enum config_input_parameter_time_weighting_mode {
	ConfigInParamTimeWeight125ms = 125,
	ConfigInParamTimeWeight1000ms = 1000,
};

#endif // NLD_TRIGGER_GEN_API_
