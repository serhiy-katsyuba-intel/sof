/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#ifndef __SOF_LIB_GNA_INSTANCE_H__
#define __SOF_LIB_GNA_INSTANCE_H__

#include <stdint.h>
#include <stdbool.h>
#include <sof/lib/gna/gna_request.h>
#include <sof/lib/gna/gna2-dev-versions.h>

#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>

/* GNA related constants */
#define MAX_TRACKED_MODELS 32
#define MAX_GNA_SCRATCH (MAX_TRACKED_MODELS * 1024)
#define GNA_HEADER_SIZE 64
#define IDX_NOT_FOUND 0xFFFF
#define GNA_SERVICE_UUID_OFFSET 0x00414E4700000000ULL

/* GNA HW versions */
#define GNA_35_VERSION Gna2DeviceVersion3_5
#define GNA_35E_VERSION Gna2DeviceVersionEmbedded3_5
#define GNA_36_VERSION Gna2DeviceVersion3_6
#define GNA_40_VERSION Gna2DeviceVersionEmbedded4_0

/**
 * @brief Structure representing a model reference in GNA.
 *
 * This structure holds information about a GNA model reference.
 */
struct model_ref {
	gna_model_id *model_id;	 /**< The ID of the GNA model. */
	const uint8_t *model_ro; /**< Pointer to read-only model data. */
	uint32_t references;	 /**< The number of references to the model. */
	uint8_t DCACHE_ALIGN gna_header[GNA_HEADER_SIZE]; /**< The GNA header. */
};

/**
 * @brief Configuration structure for a GNA instance.
 *
 * This structure holds the configuration parameters for a GNA instance.
 */
struct gna_instance_cfg {
	struct gna_model_ctx *model_ctx;     /**< Pointer to the GNA model context */
	struct gna_request_ctx *request_ctx; /**< Pointer to the GNA request context */
};

struct gna_instance_data {
	const struct device *dev;		   /**< Pointer to the GNA driver */
	struct model_ref refs[MAX_TRACKED_MODELS]; /**< Array of model references */
	uint8_t *common_scratch;		   /**< Ptr to common scratch memory */
	uint32_t common_scratch_size;	 	   /**< Size of the common scratch memory */
	struct list_item gna_model_list; 	   /**< Linked list of GNA models */

	struct gna_model_ctx *model_ctx;
	struct gna_request_ctx *request_ctx;

#ifdef __ZEPHYR__
	struct k_mutex lock;
#endif
};


/************************* GNA Model related methods **************************/

/**
 * Adds a GNA model to the GNA instance.
 *
 * @param gna The GNA instance data.
 * @return 0 on success, otherwise an error code.
 */
int gna_add_model(struct gna_instance_data *gna);

/**
 * Removes a GNA model from the GNA instance.
 *
 * @param gna The GNA instance data.
 * @return 0 on success, otherwise an error code.
 */
int gna_remove_model(struct gna_instance_data *gna);

/**
 * Parses the TLV (Type-Length-Value) data of a GNA model.
 *
 * @param gna The GNA instance data.
 * @return 0 on success, otherwise a status code.
 */
int32_t gna_model_parse_tlv(struct gna_instance_data *gna);

/**
 * Checks the hardware version of the GNA.
 *
 * @param gna_hw The GNA hardware version.
 * @return true if the hardware version matches, false otherwise.
 */
static bool gna_check_hw_version(uint32_t gna_hw);

/**
 * Gets the size of the common scratch memory used by the GNA instance.
 *
 * @param gna The GNA instance data.
 * @return The size of the common scratch memory in bytes.
 */
static size_t gna_get_used_common_scratch(struct gna_instance_data *gna);

/**
 * @brief Retrieves extra scratch size from model.
 *
 * This function retrieves the size of a GNA model scratch buffer from TLV.
 *
 * @param model_data The model data pointer.
 * @param model_size The size of the model data
 * @return The size of scratch buffer.
 */
size_t gna_model_get_extra_scratch(const uint8_t *model_data, size_t model_size);

/**
 * @brief Resize common scratch memory.
 *
 * This function resizes the common scratch memory used by the GNA instance.
 *
 * @param gna The GNA instance data.
 * @param old_size The old size of the common scratch memory.
 * @param new_size The new size of the common scratch memory.
 * @return 0 on success, a negative error code otherwise.
 */
int gna_set_common_scratch(struct gna_instance_data *gna, size_t old_size,
			   size_t new_size);

/*********************** GNA Request related methods *************************/

/**
 * @brief Get the size of a GNA request.
 *
 * This function returns the size of a GNA request for a given model context.
 *
 * @param model_ctx The model context.
 * @return The size of the GNA request.
 */
size_t gna_request_get_size(struct gna_model_ctx *model_ctx);

/**
 * @brief Initialize a GNA request.
 *
 * This function initializes a GNA request context.
 *
 * @param gna The GNA instance data.
 * @return 0 on success, a negative error code otherwise.
 */
int gna_request_init(struct gna_instance_data *gna);

/**
 * @brief Reset a GNA request.
 *
 * This function resets a GNA request context.
 *
 * @param gna The GNA instance data.
 * @return 0 on success, a negative error code otherwise.
 */
int gna_request_reset(struct gna_instance_data *gna);

/**
 * @brief Allocate buffers for a GNA request.
 *
 * This function allocates buffers for a GNA request context.
 *
 * @param gna The GNA instance data.
 * @return 0 on success, a negative error code otherwise.
 */
static int gna_request_allocate_buffs(struct gna_instance_data *gna);

/**
 * @brief Free buffers of a GNA request.
 *
 * This function frees the buffers allocated for a GNA request context.
 *
 * @param gna The GNA instance data.
 */
static void gna_request_free_buffs(struct gna_instance_data *gna);

/**
 * Starts a GNA request.
 *
 * This function starts the execution of a GNA request using the provided GNA instance
 * and request context.
 *
 * @param gna The GNA instance data.
 * @return 0 on success, negative error code on failure.
 */
int gna_request_start(struct gna_instance_data *gna);

/**
 * Registers a GNA request.
 *
 * This function registers a GNA request context for execution.
 *
 * @param req_ctx The GNA request context.
 * @param reg Set to true to register the request, false to unregister.
 */
static void gna_request_register(struct gna_request_ctx *req_ctx, bool reg);

/**
 * Gets the status of a GNA request.
 *
 * This function retrieves the status of a GNA request using the provided GNA instance
 * and request context.
 *
 * @param gna The GNA instance data.
 * @return The status of the GNA request.
 */
gna_request_status gna_request_get_status(struct gna_instance_data *gna);

/**
 * Callback function called when a GNA request is done.
 *
 * This function is called when a GNA request is completed. It provides the device,
 * context, request ID, status, and hardware status of the completed request.
 *
 * @param dev The device.
 * @param context The context.
 * @param request_id The request ID.
 * @param status The status of the request.
 * @param hw_status The hardware status of the request.
 */
static void gna_request_done_cb(const struct device *dev, void *context,
				uint32_t request_id, gna_request_status status,
				uint32_t hw_status);

/**
 * Callback function called to block a GNA request.
 *
 * This function is called to block a GNA request until it is completed.
 *
 * @param gna The GNA instance data.
 * @return True if the request should be blocked, false otherwise.
 */
static bool gna_request_block_cb(struct gna_instance_data *gna);

/**
 * Releases a GNA request.
 *
 * This function releases a GNA request context.
 *
 * @param gna The GNA instance data.
 * @return 0 on success, negative error code on failure.
 */
int gna_request_release(struct gna_instance_data *gna);

/**
 * Starts a GNA request and blocks until it is completed.
 *
 * This function starts the execution of a GNA request using the provided GNA instance
 * and request context, and blocks until the request is completed.
 *
 * @param gna The GNA instance data.
 * @return 0 on success, negative error code on failure.
 */
int gna_request_start_and_block(struct gna_instance_data *gna);

#endif /* __SOF_LIB_GNA_INSTANCE_H__ */
