/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Damian Nikodem <damian.nikodem@intel.com>
 */

#ifndef ADSP_FW_NLD_CONFIG_BLOB_H
#define ADSP_FW_NLD_CONFIG_BLOB_H


/* ASCII:                                N L D */
#define NLD_CFG_BLOB_SIG            0x00787658

/*
 * NLD Configuration BLOB Header.
 */
struct nld_cfg_blob_header {
	/* Constant signature 0x00787658 */
	uint32_t signature;
	/* reserved blot header space */
	uint32_t rsvd0;
	/* reserved blot header space */
	uint32_t rsvd1;
	/* Size of config blob data (in bytes) */
	uint32_t config_blob_size;
};

#define NLD_NOTIFICATION_ID 0           /* ID of nld module notification */
#define NLD_INPUT_QUEUE_INDEX 0         /* Supported NLD queue index */
#define NLD_MAX_LEVELS 10               /* Number of threshold levels */
#define NLD_DBA_TO_MDB_MULTIPLIER 100   /* from dBA to value in 1/100th of dBA (mBA) */

/* Send notification when crossing threshold value (with histeresis) */
const uint16_t NOTIFY_THRESHOLD_CROSSING_MASK = 0x01;
/* Send notification periodically (with configured interval) */
const uint16_t NOTIFY_WITH_INTERVAL_MASK      = 0x02;
/* Send initial notification on fists valid process data after nodata block */
const uint16_t NOTIFY_INITIAL                 = 0x04;

enum error_detector_type {
	NO_DATA = 7,
	INVALID_REQUEST,
	PROCESSING_FAILED,
	DETECTOR_SHARE_FAILED,
	GET_INTERFACE_FAILED,
};

struct nld_threshold {
	/* threshold value in 1/100th of dBA (mBA) */
	uint16_t threshold_mBA;
	/* hysteresis value in 1/100th of dBA (mBA) which is a guardband around the threshold
	 * to prevent notification storm when metric values oscilate close to the threshold
	 */
	uint16_t hysteresis_mBA;
};

struct nld_notification {
	/* when to send notification (0 - disabled) */
	uint16_t notification_mask;
	/* limit on how often to send notification when threshold is being crossed */
	uint16_t threshold_min_interval;
	/* how often to send notifications when NOTIFY_WITH_INTERVAL is configured
	 * Note: interval can be rounded to processing frame size, e.g. 10ms
	 */
	uint32_t interval;
};


struct nld_scale {
	/* number of valid/configured thresholds in subsequent array
	 * Note: Number of sensitivity levels that can be reported is thresholds_count + 1.
	 * The lowest rage "0" is anything up to first threshold value and the last is
	 * anything above the last threshold value
	 */
	uint16_t thresholds_count;
	/* for custom scale the mBA sensitivity and histeresis levels
	 * for LASmin are set from this array, otherwise the values are ignored
	 */
	struct nld_threshold thresholds[NLD_MAX_LEVELS];
};


struct nld_ip_config {
	/* thresholds for noise level update notifications */
	struct nld_scale sensitivity_levels;
	/* time period of collecting statistics [ms] (alternatively it
	 * may be tuning param, not visible to user)
	 */
	uint16_t  measure_period_ms;
};


/*
 * We assume some parameters will not be visible to user but will be part of algorithm tuning.
 * Tuning configuration parameters:
 * - time constant for A curve [ms]
 */
static const struct nld_notification NLD_DEFAULT_NOTIFICATION = {
	/* default notification mask - threshold mechanism ON */
	NOTIFY_THRESHOLD_CROSSING_MASK,
	/* default threshold minimal interval period = 10 ms - same as processing size */
	10,
	/* default measure period = 1 s */
	1000,
};

struct nld_cfg_blob {
	/* Notification interval congiguration. */
	struct nld_notification notification;
	/* NLD lib configuration - measure level period, threshold count and thresholds table. */
	struct nld_ip_config config;
};

#endif /* ADSP_FW_NLD_CONFIG_BLOB_H */
