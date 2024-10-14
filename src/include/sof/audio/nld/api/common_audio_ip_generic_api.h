/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Damian Nikodem <damian.nikodem@intel.com>
 */

#ifndef AUDIO_IP_COMMON_GENERIC_API_
#define AUDIO_IP_COMMON_GENERIC_API_

#include <stdint.h>

/*
 *
 * INTRODUCTION
 *
 * This header defines types and constants for common API of Audio IP libraries.
 * API functions are specific for audio IP but use these common types and follow
 * common convention of usage and naming, described herein.
 *
 * While applying the API documentation to the specific Audio IP library, the string
 * "[LibPrefix]" should be replaced with specific Audio IP library abbreviation,
 * for example Wov, UlpWov etc.
 *
 */

/*
 *              Audio IP Generic API
 *          For Common Generic Functions
 * --------------------------------------------------------------------------------------------
 *   GENERIC:
 *            INITIALIZATION:
 *              [LibPrefix]GetDataSize
 *              [LibPrefix]Initialize
 *            CONFIGURATION: [ @see ConfigModelType ]
 *              [LibPrefix]SetConfiguration
 *              [LibPrefix]GetConfiguration
 *            RUNTIME:
 *              [LibPrefix]ProcessFrames
 * --------------------------------------------------------------------------------------------
 */

/*
 * Return status values for audio IP common generic API
 * Type: uint32_t
 * @note returned value 0 means success, non-zero means error
 * @note status values in range (0, 20> are reserved for audio IP common API errors (micsel,
 * history buffer etc.)
 * @note status values higher than 20 are reserved for audio IP trigger API errors
 * (WoV, NCA, ACA etc.)
 */
/* success */
#define AUDIO_IP_STATUS_SUCCESS 0U
/* new results not available due to insufficient input data or lack of events */
#define AUDIO_IP_STATUS_NO_DATA 1U
/* unspecified error */
#define AUDIO_IP_STATUS_GENERIC_ERROR 2U
/* given parameter id is not supported */
#define AUDIO_IP_STATUS_UNSUPPORTED_PARAM_ID 3U
/* given model type is not supported */
#define AUDIO_IP_STATUS_UNSUPPORTED_MODEL_TYPE 4U
/* parameter value is invalid */
#define AUDIO_IP_STATUS_INVALID_PARAM_VALUE 5U
/* parameter size is other than expected */
#define AUDIO_IP_STATUS_INVALID_PARAM_SIZE 6U
/* operation is not supported, operation sequence is invalid */
#define AUDIO_IP_STATUS_INVALID_OPERATION 7U
/* config blob format is invalid, unsupported or blob cannot be loaded */
#define AUDIO_IP_STATUS_INVALID_BLOB_FORMAT 8U
/* incomplete configuration, not enough parameters configured */
#define AUDIO_IP_STATUS_INCOMPLETE_CONFIGURATION 9U
/* given model type is not supported */
#define AUDIO_IP_STATUS_UNSUPPORTED_VERSION 10U
/* GNA lib version is not supported */
#define AUDIO_IP_STATUS_UNSUPPORTED_GNA_VERSION				11U
#define AUDIO_IP_STATUS_INSTANCE_NULL_POINTER				200U
#define AUDIO_IP_STATUS_NOT_ENOUGH_MEMORY_FOR_INSTANCE			201U
#define AUDIO_IP_STATUS_INSTANCE_BUFFER_NOT_ALIGNED			202U
#define AUDIO_IP_STATUS_INITIALIZATION_FAILED				203U
#define AUDIO_IP_STATUS_PARAM_VALUE_NULL_POINTER			204U
#define AUDIO_IP_STATUS_PARAM_SIZE_NULL_POINTER				205U
#define AUDIO_IP_STATUS_FAILED_TO_SET_LIBRARY_CONFIGURATION_PACKAGE	206U
#define AUDIO_IP_STATUS_TEMPORARY_BUFFER_INVALID_SIZE			207u
#define AUDIO_IP_STATUS_TEMPORARY_BUFFER_INVALID_ALIGNMENT		208u
#define AUDIO_IP_STATUS_INPUT_NULL_POINTER				209u
#define AUDIO_IP_STATUS_FAILED_TO_SET_TRIGGER_CONFIG			210u
#define AUDIO_IP_STATUS_TEMPORARY_BUFFER_OUT_POINTER_IS_NULL		211u
#define AUDIO_IP_STATUS_INVALID_FRAME_SIZE				212u
#define AUDIO_IP_STATUS_NUMBER_OF_FRAMES_PASSED_IS_ZERO			213u
#define AUDIO_IP_STATUS_DETECTION_RESULT_NULL_POINTER			214u
#define AUDIO_IP_STATUS_DETECTION_RESULT_SIZE_NULL_POINTER		215u
#define AUDIO_IP_STATUS_FAILED_TO_GET_TRIGGER_CONFIG			216u
#define AUDIO_IP_STATUS_INVALID_MEMORY_SIZE				217u

/* required alignment of GNA buffers */
#define GNA_BUFFER_REQUIRED_ALIGNMENT 64U

/* bit lengths of input samples */
#define SAMPLE_BIT_DEPTH_16 16U
#define SAMPLE_BIT_DEPTH_24 24U

#define SENSITIVITY_MINUS_26_DBFS_PER_PA -2600 // [mB = 0.001 dB]
#define SENSITIVITY_MINUS_8_DBFS_PER_PA -800   // [mB = 0.001 dB]

enum audio_ip_boolean {
	False = 0,
	True  = 1
};


/**
 *	Audio IP Generic API
 *	Common Generic Type Definitions
 * --------------------------------------------------------------------------------------------
 *	GENERIC:
 *		CONFIGURATION:
 *		enum ConfigModelType
 *		struct AudioIpMemoryRegion
 *		struct AudioIpFragmentedMemoryRegion
 *		enum AudioIpDataType
 *		struct AudioPcmFormatSignature
 *		[integer type] AudioIpFormatSignature
 *		enum ConfigInputParameterId
 *		enum ConfigOutputParameterId
 *		RUNTIME:
 *		[function type] ExecuteGnaRequestCallback
 *		struct detection_event_context
 * --------------------------------------------------------------------------------------------
 */

/*
 * Supported model types for ACE MTL platform.
 * @note Refers to ANNA (GNA) model associated with single ASRV on ANNA HW
 * @note Single Audio IP library may support zero, one or more model types.
 */
enum config_model_type {
	ConfigModelMicSel		= 0, /* microphone / channel selector */
	ConfigModelHistoryBuffer	= 1, /* history buffer */
	ConfigModelFrontEnd		= 2, /* low level (melbank or convolutional) front-end */
	ConfigModelKpd			= 3, /* key phrase detection */
	ConfigModelAcaCcl		= 4, /* acoustic context awareness - common
					      * convolutional front-end
					      */
	ConfigModelAcaAedImp		= 5, /* acoustic context awareness - acoustic event
					      * detector, impulsive events
					      */
	ConfigModelAcaAedCon		= 6, /* acoustic context awareness - acoustic event
					      * detector, continuous events
					      */
	ConfigModelAcaAsc		= 7, /* acoustic context awareness - acoustic scenes
					      * classifier
					      */
	ConfigModelAcaIsd		= 8, /* acoustic context awareness - instant speech
					      * detector
					      */
	ConfigModelGeneric		= 9, /* to be used when single configuration model is
					      * used for library
					      */
};

/*
 * Mutable Memory region, value type used for exchanging input / output configuration parameters:
 *  - [input] ConfigInParamBlob, ConfigInParamStateBuffer
 *  - [output] ConfigOutParamModelReadOnlyBuffer
 */
struct audio_ip_memory_region {
	void *buffer;	/* buffer pointer */
	uint32_t size;	/* buffer size in bytes */
};

/*
 * Non Mutable Memory region, value type used for exchanging input / output
 * configuration parameters:
 */
struct audio_ip_const_memory_region {
	const void *buffer;	/* buffer pointer */
	uint32_t size;		/* buffer size in bytes */
};

/**
 * Memory region divided into fragments, type used for exchanging input configuration parameter:
 *  - ConfigInParamExternalOutputBuffer
 * @note Defines ANNA External Input/Output Buffer
 */
struct audio_ip_fragmented_memory_region {
	void *buffer;			/* buffer pointer */
	uint32_t buffer_size;		/* buffer size in bytes */
	uint32_t fragment_count;	/* number of fragments; buffer_size should
					 * be divisible by fragment_count
					 */
};

/* Possible input / output data types exchanged between Audio IPs */
enum audio_ip_data_type {
	AudioDataPcm = 0,	/* raw audio in PCM format */
	AudioDataFeatures = 1,	/* features extracted from raw audio, storing signal
				 * characteristics important for detection of specific
				 * acoustic events
				 */
	AudioDataScores = 2,	/* scores storing detection confidence, calculated
				 * by neural network trained for detection of specific event,
				 * for example a keyphrase
				 */
	AudioDataOther = 3,	/* other */
};

/* structure defining audio PCM format */
struct audio_pcm_format_signature {
	uint8_t sampling_rate_khz;	/* audio sampling rate [kHz], for example: 16 for 16kHz */
	uint8_t sample_bit_depth;	/* audio sample bit width, for example:
					 * 16 for 16 bit samples
					 */
	uint8_t container_bit_depth;	/* audio sample container bit width, must be greater
					 * or equal to sample_bit_depth, example:
					 * 16 for 16 bit samples
					 */
	uint8_t channel_count;		/* number of audio channels, e.g.: 1 for mono signal */
};

/*
 * Generic type for unique identification of data format and for verifying compatibility of data
 * exchanged between IP's.
 * @note in/out format signature can be read using [LibPrefix]GetConfiguration with parameter ID
 * ConfigOutParamInputFormatSignature / ConfigOutParamOutputFormatSignature The purpose is to
 * verify input/output data format compatibility between IP's (configurations) according to
 * the following rule:
 * If output format signature of A is equal to input format signature of B then A -> B can be
 * bound as subsequent modules in a processing pipeline, given that other required parameters like
 * execution period and data size match.
 * @see ConfigOutputParameterId
 */
union audio_format_signature {
	uint32_t ip_format;
	uint32_t pcm_format;
};

/*
 * Supported configuration input parameters ID,
 * which can be set for a GNA model (or ASRV) using [LibPrefix]SetConfiguration
 * @note types of parameters are given in the comments below
 * @note in general for parameters that are single integer numbers, uint32_t type is used
 */
enum config_input_parameter_id {
	ConfigInParamChannelCount		= 0, /* parameter type: uint32_t */
	ConfigInParamInputBitDepth		= 1, /* configuration parameter to set bit length
						      * of input samples (16-bit vs 24-bit
						      * contained in the first 3 MSB of 32-bit
						      * variable): parameter type: uint32_t
						      */
	ConfigInParamSelectedChannel		= 2, /* MicSel specific,
						      * parameter type: uint32_t
						      */
	ConfigInParamGain			= 3, /* parameter type: uint32_t */
	ConfigInParamInputSizeMs		= 4, /* input data size [ms],
						      * parameter type: uint32_t
						      */
	ConfigInParamBlob			= 5, /* configuration FW blob,
						      * parameter type: AudioIpMemoryRegion
						      */
	ConfigInParamStateBuffer		= 6, /* service state buffer,
						      * parameter type: AudioIpMemoryRegion
						      */
	ConfigInParamExternalOutputBuffer	= 7, /* ANNA specific, external output buffer
						      * for ANNA,
						      * parameter type:
						      * AudioIpFragmentedMemoryRegion
						      */
	ConfigInParamInputBuffer		= 8, /* input buffer for GNA,
						      * parameter type: AudioIpMemoryRegion
						      */
	ConfigInParamOutputBuffer		= 9, /* output buffer for GNA,
						      * parameter type: AudioIpMemoryRegion
						      */
	ConfigInParamRequestCallbackFunction	= 10, /* request callback for GNA,
						       * parameter type:
						       * OffloadRequestCompletionCallback
						       */
	ConfigInParamRequestCallbackContext	= 11, /* request context for GNA,
						       * parameter type: pointer
						       */
	ConfigInParamInputScalingFactor		= 12, /* Factor for input scaling,
						       * parameter type: float
						       */
	ConfigInParamOutputScalingFactor	= 13, /* Factor for output scaling,
						       * parameter type: float
						       */
	ConfigInParamSubmit			= 14, /* Confirms end of algorithm configuration
						       * process, parameter type: bool
						       */
	ConfigInParamScalingFactorsArray	= 15, /* Sets scaling factors for GNA models */
	ConfigInParamInferenceContextArray	= 16, /* Sets inference context set for
						       * GNA models
						       */
	ConfigInParamMinMaxPeriod		= 17, /* Sound Level Meter param for min-max
						       * history period
						       */
	ConfigInParamTimeWeightPeriod		= 18, /* Sound Level Meter param for time
						       * weighting
						       */
	ConfigInParamSensitivity		= 19, /* Sensitivity expressed in miliBels
						       * (1 dB = 100 mB)
						       * parameter type: uint32_t
						       */
};

/*
 * Supported configuration output parameters ID,
 * which can be get for a GNA model (or ASRV) using [LibPrefix]GetConfiguration
 * @note types of parameters are given in the comments below
 * @note in general for parameters that are single integer numbers, uint32_t type is used
 */
enum config_output_parameter_id {
	ConfigOutParamInputFormatSignature = 6,	/* parameter type: AudioFormatSignature */
	ConfigOutParamOutputFormatSignature = 7, /* parameter type: AudioFormatSignature */
	ConfigOutParamReadIncrementCount = 10,	/* ANNA specific, read pointer shift per one
						 * inference (bytes), must be divisible by minimum
						 * fragment size
						 * (ANNA_EXT_BUF_MINIMUM_FRAGMENT_SIZE),
						 * parameter type: uint32_t
						 */
	ConfigOutParamExecutionPeriodMs = 11,	/* ANNA specific, how often inference should be
						 * executed [ms], parameter type: uint32_t
						 */
	ConfigOutParamGnaTlvModel = 12,		/* GNA specific, returns memory region with GNA TLV
						 * model, parameter type: AudioIpMemoryRegion
						 */
	ConfigOutParamGnaModelOffset = 13,	/* Factor for output scaling,
						 * parameter type: float
						 */
	ConfigOutParamChannelCount = 14,	/* parameter type: uint32_t */
	ConfigOutParamInputBitDepth = 15,	/* parameter type: uint32_t */
	ConfigOutParamSelectedChannel = 16,	/* MicSel specific, parameter type: uint32_t */
	ConfigOutParamGain = 17,		/* parameter type: uint32_t */
	ConfigOutParamSingleFrameSize = 18,	/* input data size [B], parameter type: uint32_t */
	ConfigOutParamModelsArray = 19,		/* NNet models binary blobs */
	ConfigOutParamErrorBacktraceBuffer = 20, /* Error backtrace buffer, used for
						  * debugging purposes
						  */
};

/**
 * Type definition for callback function which executes GNA request on behalf of library.
 * The callback function is called within [LibPrefix]ProcessFrames processing function.
 * The callback function is configured for given GNA model type. for example ConfigModelKPD.
 * @note The callback is passed to library in [LibPrefix]SetConfiguration with parameter ID
 * ConfigInParamRequestCallbackFunction
 * @note The request context needed for request execution is passed to library in
 * [LibPrefix]SetConfiguration with parameter ID ConfigInParamRequestCallbackContext
 * @see ConfigInputParameterId
 */
uint32_t (*ExecuteGnaRequestCallback)(void *request_context);

/**
 * Structure passed to the trigger on detection, can be derived from
 * anna_detection_context (input_fragment is omitted since it is not
 * needed by the trigger)
 */
struct detection_event_context {
	int32_t hw_status; //< snapshot of ASRV HW status register for the time when the detection
			   //event occurred
	uint16_t output_fragment; //< index of fragment in external output buffer, where inference
				  //result is
	// stored from the time when event detection occurred
};

// values of config_input_parameter_sensitivity_mode are in mB; 100 mB = 1 dB
enum config_input_parameter_sensitivity_mode {
	ConfigInParamSensitivityMinus26dB = -2600,
	ConfigInParamSensitivityMinus8dB = -800,
};

#endif // AUDIO_IP_COMMON_GENERIC_API_
