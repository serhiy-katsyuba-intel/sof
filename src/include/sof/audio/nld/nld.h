/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Damian Nikodem <damian.nikodem@intel.com>
 */

#ifndef _ADSP_FW_NLD_MODULE_H
#define _ADSP_FW_NLD_MODULE_H


#include <stdint.h>
#include <ipc4/base-config.h>
#include <ipc4/nld.h>
#include <sof/trace/trace.h>
#include "api/nld_trigger_gen_types.h"


/* Generic event_id valud for Detector Event */
#define MODULE_NOTIFICATION_DETECTOR_EVENT (0xDE7EC7ED)
#define CONFIG_ADSP_NLD_DETECTION_UUID { 0x2426EC71, 0x4551E8A0, 0x674D2993, 0xF2BFC1E5 }

static const size_t QUEUE_LENGTH = 3;

enum sampling_frequency {
	FS_8000HZ   = 8000,
	FS_11025HZ  = 11025,
	FS_12000HZ  = 12000,	/* Mp3, AAC, SRC only. */
	FS_16000HZ  = 16000,
	FS_18900HZ  = 18900,	/* SRC only for 44100 */
	FS_22050HZ  = 22050,
	FS_24000HZ  = 24000,	/* Mp3, AAC, SRC only. */
	FS_32000HZ  = 32000,
	FS_37800HZ  = 37800,	/* SRC only for 44100 */
	FS_44100HZ  = 44100,
	FS_48000HZ  = 48000,	/* Default. */
	FS_64000HZ  = 64000,	/* AAC, SRC only. */
	FS_88200HZ  = 88200,	/* AAC, SRC only. */
	FS_96000HZ  = 96000,	/* AAC, SRC only. */
	FS_176400HZ = 176400,	/* SRC only. */
	FS_192000HZ = 192000,	/* SRC only. */
	FS_INVALID
};

enum bit_depth {
	DEPTH_8BIT  = 8,	/* 8 bits depth */
	DEPTH_16BIT = 16,	/* 16 bits depth */
	DEPTH_24BIT = 24,	/* 24 bits depth - Default */
	DEPTH_32BIT = 32,	/* 32 bits depth */
	DEPTH_64BIT = 64,	/* 64 bits depth */
	DEPTH_INVALID
};

/* Total number of input pins. */
static const size_t NLD_MODULE_INPUT_PINS_COUNT		= 1;
/* Total number of output pins. */
static const size_t NLD_MODULE_OUTPUT_PINS_COUNT	= 1;
/* Supported frame size in ms. */
static const size_t NLD_MODULE_INPUT_FRAME_SIZE		= 10;
/* Supported frequency in Hz. */
static const size_t NLD_MODULE_SUPPORTED_FREQUENCY	= FS_16000HZ;
/* Supported input channels. */
static const size_t NLD_MODULE_SUPPORTED_INPUT_CHANNELS	= 1;
/* Supported input sample size in bytes. */
static const size_t NLD_MODULE_16_16_SAMPLE_SIZE	= DEPTH_16BIT >> 3;
static const size_t NLD_MODULE_24_32_SAMPLE_SIZE	= DEPTH_32BIT >> 3;

#define NLD_NOTIFICATION_ID 0		/* ID of nld module notification */
#define NLD_INPUT_QUEUE_INDEX 0		/* Supported NLD queue index */
#define NLD_MAX_LEVELS 10		/* Number of threshold levels */
#define NLD_DBA_TO_MDB_MULTIPLIER 100	/* from dBA to value in 1/100th of dBA (mBA) */

/* size in bytes of 1ms signal processed by noise level detector module */
#define NLD_16_ONE_MS_SIZE (NLD_MODULE_SUPPORTED_INPUT_CHANNELS * \
			   (NLD_MODULE_SUPPORTED_FREQUENCY/1000) * \
			   NLD_MODULE_16_16_SAMPLE_SIZE)

#define NLD_24_32_ONE_MS_SIZE (NLD_MODULE_SUPPORTED_INPUT_CHANNELS * \
			      (NLD_MODULE_SUPPORTED_FREQUENCY/1000) * \
			      NLD_MODULE_24_32_SAMPLE_SIZE)

/* NLD block processing size = 10ms * 1ch * 16kHz * 2bytes (16 bit depth) */
#define NLD_16_PROCESSING_SIZE (NLD_MODULE_INPUT_FRAME_SIZE * NLD_16_ONE_MS_SIZE)

/* NLD block processing size = 10ms * 1ch * 16kHz * 4bytes (24 bit in 32 container depth) */
#define NLD_24_32_PROCESSING_SIZE (NLD_MODULE_INPUT_FRAME_SIZE * NLD_24_32_ONE_MS_SIZE)

#define PAGE_SIZE			   4096
#define NLD_INSTANCE_SIZE		   (7 * PAGE_SIZE)
#define CONFIG_BLOB_BUFFER_PAGES	   45
#define CONFIG_BLOB_BUFFER_SIZE		   (CONFIG_BLOB_BUFFER_PAGES * PAGE_SIZE)


#define NLD_SENSITIVITY_MDB               -2600 /* SENSITIVITY_MINUS_26_DBFS_PER_PA */


struct ipc4_intel_nld_module_cfg {
	struct ipc4_base_module_cfg base_cfg;
};


static const struct nld_scale NLD_DEFAULT_THRESHOLDS = {
	8,		    /* thresholds_count */
	{                   /* threshold_mBA   histeresis_mBA */
		{   0,    0},   /*  0 dBA          0 dBA */
		{3000,  200},   /* 30 dBA          2 dBA */
		{4000,  200},   /* 40 dBA          2 dBA */
		{5000,  200},   /* 50 dBA          2 dBA */
		{6000,  200},   /* 60 dBA          2 dBA */
		{7000,  200},   /* 70 dBA          2 dBA */
		{8000,  200},   /* 80 dBA          2 dBA */
		{9000,  200}    /* 90 dBA          2 dBA */
	}
};

/* ConfigInParamMinMax1000ms */
static const uint16_t NLD_DEFAULT_MEASURE_PERIOD_MS = 1000;

enum nld_params {
	/* NLD processing set param - message based on enum NldTypes */
	NLD_CONFIG_PARAM_NEWBLOB = 2,
};


union nld_output_version {
	uint16_t full;
	struct {
		/* Major Version */
		uint16_t major : 4;
		/* Minor Version */
		uint16_t minor : 4;
		/* Hot fix Version */
		uint16_t hotfix : 4;
		/* Reserved */
		uint16_t reserved : 4;
	} part;
};



/*
 * Structure for storing result from Noise Level Detector
 * Metric: LASmin (time for min calculation set by measure_period_ms)
 * Sound level unit: 1/100 dBA - mBA
 * (mBA levels are "A" weighted according to the "A" weighting curve
 * to approximate the way the human ear hears)
 */
struct nld_output {
	/* bit mask for notification reason */
	uint16_t notification_reason_mask;
	/* version of this notification structure */
	union nld_output_version structure_version;
	/* crossed threshold in 1/100th of dBA (mBA) */
	uint16_t crossed_threshold_level_mBA;
	/* current noise level (LASmin) [1/100th of dBA - mBA] */
	uint16_t current_noise_level_mBA;
} __packed __aligned(4);


struct module_counters {
	/* DSP Timer value in wallclock ticks */
	uint64_t timer_value;
	/* Link Gateway position in sample groups */
	uint64_t linear_link_position;
	/* Event position counter in Gateway in sample groups */
	uint64_t gateway_total_processed_data;
	/* Event position counter in detector module in sample groups */
	uint64_t module_total_processed_data;
} __packed __aligned(4);

/*
 * detector_event_payload structure describes layout for detection events
 * from modules
 */
struct detector_event_payload {
	/* Module ID */
	uint32_t mod_id;
	/* Event type */
	uint32_t event_type;
	/* Event data size */
	uint32_t event_data_size;
	/* Event specific GUID */
	uint32_t event_guid[4];
	/* Counters' value at the moment of sending notification to HOST. */
	struct module_counters current;
	/* Counters' value at the moment the event appeared in audio stream. */
	struct module_counters event_start;
	/* [Optional] Counters' value at the end of the reported event */
	struct module_counters event_end;
	/* Optional event data */
	struct nld_output result;
} __packed __aligned(4);

struct processing_buffer {
	/* size of NLD processing buffer */
	size_t size;
	/* buffer start address */
	uint8_t *start_addr;
	/* buffer end address */
	uint8_t *end_addr;
	/* buffer write pointer */
	uint8_t *w_ptr;
	/* buffer read pointer */
	uint8_t *r_ptr;
};

struct nld_mod_cfg {
	/* Default configuration struct for NLD algo lib */
	struct nld_config nld_config;

	/* Time period of collecting statistics [ms]
	 * (alternatively it may be tuning param, not visible to user)
	 */
	enum config_input_parameter_min_max_period min_max_period_ms;

	/* Sensitivity level of input in mDb - specific for platform  */
	uint32_t sensitivity_mdb;

	/* Detector audio frame size in millisecond */
	size_t frame_size_ms;

	/* Detector audio input format */
	struct ipc4_audio_format input_fmt;
};

struct detector_result {
	bool initial_notification;

	/* Indicator of detection in algo processing */
	bool detected;

	/* Previous algo result */
	struct nld_result prev_res;

	/* Current algo result */
	struct nld_result cur_res;
};

struct nld_instance_detector {
	/* Indicator of threshold mechanism trigger shot. Define if detection should be triggered */
	bool threshold_cross;

	/* Current noise level detector threshold index */
	size_t current_threshold_idx;

	/* Noise level detector threshold index of threshold to cross during signal increasing */
	int8_t decrease_threshold_idx;

	/* Noise level detector threshold index of threshold to cross during signal decreasing */
	int8_t increase_threshold_idx;

	/* Number of frames processed by threshold notification mechanism. */
	uint16_t threshold_frames_count;
	/* Number of frames processed by periodic notification mechanism. */
	uint16_t period_frames_count;

	/* Flag for initial notification after nodata segment */
	bool initial_notification;

	/* Detection output used by notification mechanism */
	struct nld_output det_out;
};


struct nld_ctx {
	/* Noise level detector config */
	struct nld_cfg_blob nld_module_config;

	struct nld_mod_cfg mod_cfg;

	struct nld_instance_detector inst_det;
};


/* Intel NLD component private data. */
struct intel_nld_data {
	struct ipc4_base_module_cfg ipc4_cfg;
	struct comp_data_blob_handler *model_handler;
	uint8_t *config_blob;
	size_t config_blob_size;

	uint32_t nld_ip_data_chunk;
	uint32_t frame_size;
	uint32_t frames_num;

	/* Notifications related data */
	struct ipc_msg *msg;	/*  host notification */

	/* AMS related data*/
	uint32_t nld_uuid_id;

	/* NLD module instance*/
	uint8_t *nld_instance;
	size_t nld_algo_size;
	struct detector_result result;
	struct nld_ctx *nld_ctx;
};

void sys_comp_module_nld_interface_init(void);

#endif // _ADSP_FW_NLD_MODULE_H
