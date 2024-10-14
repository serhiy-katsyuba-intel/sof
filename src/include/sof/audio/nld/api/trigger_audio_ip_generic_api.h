/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Damian Nikodem <damian.nikodem@intel.com>
 */

#ifndef AUDIO_IP_TRIGGER_GENERIC_API_
#define AUDIO_IP_TRIGGER_GENERIC_API_

#include "common_audio_ip_generic_api.h"

/**
 * --------------------------------------------------------------------------------------------
 *
 * INTRODUCTION
 *
 * This header defines generic API of Audio Trigger IP libraries.
 * (see EXPLANATION OF TERMS AND CONVENTIONS USED for definition of Trigger IP)
 * API functions are specific for audio IP but use common types and follow
 * common convention of usage and naming, described herein.
 *
 * While applying the API documentation to the specific Audio IP library, the string
 * "[LibPrefix]" should be replaced with specific Audio IP library abbreviation,
 * for example Wov, Nca etc.
 *
 * --------------------------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------------------------
 *
 * EXPLANATION OF TERMS AND CONVENTIONS USED
 *
 *
 * Audio Trigger IP
 *
 * Generic word "Audio Trigger" means here both acoustic event type and associated
 * Audio IP (Intellectual Property ~ algorithm + FW/HW implementation) that is
 * capable of detecting such acoustic events and triggering the detection
 * mechanism resulting in HW interrupt notifying and/or waking up DSP / CPU or other.
 *
 *  Examples of triggers in Intel Audio IP portfolio are:
 * >> WoV (Wake on Voice) IP detecting spoken keyphrase acoustic event (keyphrase detector),
 * >> NCA (Noise Context Awareness) IP detecting noise level change event (noise change detector),
 * >> ACA (Acoustic Context Awareness) IP detecting glass break acoustic event (glass break
 * detector)
 * >> ACA (Acoustic Context Awareness) IP detecting baby cry acoustic event (baby cry detector)
 *
 *
 * Trigger Types
 *
 * Specifically single Audio IP (library) may be capable of detecting one or more acoustic
 * event type (referred as "Trigger Type"), depending on run-time configuration loaded, for example:
 * >> algorithm triggering on keyphrase spoken (Trigger Type: Kpd / keyphrase detector)
 * >> algorithm triggering on acoustic events (Trigger Type: AcaAed / acoustic context awareness,
 * acoustic events detection)
 * >> algorithm triggering on voice activity start / stop (Trigger Type: Vad / voice activity
 * detection)
 *
 *
 * Multiple Triggers, Trigger Index
 *
 * Within a specific run-time configuration loaded by Audio IP, multiple triggers of given Trigger
 * Type may exist. For example WoV IP with "Trigger Type: KPD (keyphrase detector)" may be capable
 * of triggering (detecting) on two or more keyphrases at the same time (Alexa, Cortana or other,
 * depending on run-time configuration). Multiple keyphrases may generate an event independently
 * from each other. Such multiple triggers of the same type are referred in Audio IP API by "Trigger
 * Index", starting from 0. The number of triggers of the same type is reffered as "Trigger Count".
 * Configuration and results are get / set in the API for all triggers of given type in single call,
 * using arrays of size equal to trigger count and array element C type specific to trigger type,
 * usually defined as a separate structure with variable size.
 *
 *
 * Trigger GUID
 *
 * Each trigger may have associated unique identifier ("Trigger GUID") which can derived from Audio
 * IP. Trigger GUID allows API user to uniquely identify the associated acoustic event trigger. For
 * example with "Trigger Type: KPD" the associated GUID determines keyphrase (Alexa, Cortana, etc.),
 * language (English, German, etc.), location (US, GB, etc.).
 * The GUID interpretation is not a subject of this documentation. See FW/SW architecture
 * specification for more details.
 *
 * --------------------------------------------------------------------------------------------
 */

/**
 * --------------------------------------------------------------------------------------------
 *              Audio IP Generic API
 *           For Trigger Generic Functions
 *
 *   TRIGGER: [ @see TriggerType ]
 *            CONFIGURATION:
 *              [LibPrefix]set_trigger_config
 *              [LibPrefix]get_trigger_config
 *            RUNTIME:
 *              [LibPrefix]set_trigger_runtime_param
 *              [LibPrefix]get_trigger_runtime_param
 *              [LibPrefix]handle_trigger_event
 *              [LibPrefix]get_trigger_result
 *              [LibPrefix]reset_trigger_state
 * --------------------------------------------------------------------------------------------
 */

/**
 * Return status values specific for Audio IP triggers
 * Type: uint32_t
 * @note status values higher than 20 are reserved for audio IP trigger API errors
 */
#define AUDIO_IP_STATUS_UNSUPPORTED_TRIGGER_TYPE 20U /* given trigger type is not supported */
#define AUDIO_IP_STATUS_INVALID_TRIGGER_INDEX 21U    /* given trigger index is not valid */

/**
 *              Audio IP Generic API
 *        Trigger Generic Types Definitions
 * --------------------------------------------------------------------------------------------
 *   GENERIC:
 *            CONFIGURATION:
 *              enum trigger_type
 *              enum trigger_config_input_parameter_id
 *              enum trigger_config_output_parameter_id
 *              struct trigger_guid
 *            RUNTIME:
 *              enum trigger_reset_type
 * --------------------------------------------------------------------------------------------
 */
/**
 * Supported trigger types for ACE MCF platform.
 */
enum trigger_type {
	TriggerTypeKpd = 0, /* Trigger Type: keyphrase detector */
	TriggerTypeVad = 1, /* Trigger Type: voice activity detection, for purpose of gating WoV */
	TriggerTypeAad = 2, /* Trigger Type: acoustic activity detection,
			     * for purpose of gating ACA
			     */
	TriggerTypeNld = 3, /* Trigger Type: noise level detection */
	TriggerTypeAcaAed =
	    4, /* Trigger Type: acoustic context awareness - acoustic event detector */
	TriggerTypeAcaAsc =
	    5, /* Trigger Type: acoustic context awareness - acoustic scenes classifier */
	TriggerTypeAcaIsd =
	    6, /* Trigger Type: acoustic context awareness - instant speech detector */
	TriggerTypeSend =
	    7, /* Trigger Type: Sound Energy Detection - impulsive sound level measurement */
};

/**
 * Supported trigger config input parameters ID,
 * which can be set using [LibPrefix]SetTriggerConfig
 */
enum trigger_config_input_parameter_id {
	TriggerConfigInParamConfig =
	    0, /* configuration - parameter type: array of structures specific for
		* trigger type, array size is triggers count
		*/
	TriggerConfigInParamInterval = 1,
	TriggerConfigInParamDebugConfig = 2, /* debug configuration  - parameter type: array of
					      * structures specific for trigger type, array
					      * size is triggers count, use only in debug mode
					      */
};

/**
 * Supported trigger config output parameters ID,
 * which can be get using [LibPrefix]GetTriggerConfig
 */
enum trigger_config_output_parameter_id {
	TriggerConfigOutParamIsTriggerTypeSupported =
	    0, /* parameter type: uint32_t, value: true (1) if trigger
		* type is supported by Audio IP, false (0) otherwise
		*/
	TriggerConfigOutParamTriggerCount =
	    1, /* parameter type: uint32_t, value: number of triggers of given
		* type within loaded run-time configuration
		*/
	TriggerConfigOutParamTriggerUUID =
	    2, /* parameter type: array of triggers UUID for given trigger type,
		* @see TriggerUUID, array size is triggers count
		*/
	TriggerConfigOutParamConfig =
	    3, /* parameter type: array of enabled configuration structures specific for
		* trigger type, array size is triggers count
		*/
	TriggerConfigOutParamDebugConfig =
	    4, /* parameter type: array of debug configuration structures
		* specific for trigger type, array size
		* is triggers count, use only in debug mode
		*/
	TriggerConfigOutParamVersion = 5, /* version of trigger algorithm,
					   * parameter type: int32_t
					   */
	TriggerConfigOutParamInterval =
	    6, /* reporting interval (sensor-specific), parameter type: uint32_t */
	TriggerConfigOutParamSupportedConfig =
	    7, /* parameter type: array of possible configuration structures specific for
		* trigger type, array size is triggers count
		*/
	TriggerConfigOutParamTriggerGUID =
	    8, /* parameter type: array of triggers GUID for given trigger type,
		* @see TriggerGUID, array size is triggers count
		*/
	TriggerConfigOutParamIsActive =
	    9, /* parameter type: bool, value: true (1) if trigger type is active
		* in current instance, false (0) otherwise
		*/
};

/**
 * supported reset types, to be used in ResetTriggerState
 */
enum trigger_reset_type {
	/* resets only runtime state (reverts processing history), does not impact trigger
	 * configuration
	 */
	TriggerRuntimeStateReset = 0,
	/* resets total state to the initial state, both trigger configuration and runtime state */
	TriggerTotalReset = 1,
	/* re-arming detected triggers (resets only those triggers for which detection occurred)
	 * MM: pls specify
	 */
	TriggerDetectedReset = 2,
};

#endif /* AUDIO_IP_TRIGGER_GENERIC_API_ */
