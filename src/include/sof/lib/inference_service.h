/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#ifndef __SOF_LIB_INFERENCE_SERVICE_H__
#define __SOF_LIB_INFERENCE_SERVICE_H__

#include <stdint.h>
#include <sof/lib/gna/gna_instance.h>

/*!
 * @brief Enum of error code value which can be reported by a Inference Service
 */
enum inference_service_error {
	/*!< Reports that the given model is invalid */
	INVALID_MODEL = 7,
	/*!< Reports that the given model is unsupported */
	UNSUPPORTED_MODEL,
	/*!< Reports that the given model does not contains user data */
	NO_USER_DATA,
};

/*! @brief enumeration values of request status */
enum request_status_code {
	/*!< Request was processed without errors. */
	REQUEST_SUCCESS = 0,
	/*!< Request is still pending processing. */
	REQUEST_PENDING = 1,
	/*!< Inference failed with internal error. */
	REQUEST_ERROR,
};

/**!
 * @brief Structure representing neural network model in memory.
 */
struct inference_model {
	/*!< required alignment of model buffer */
	size_t model_alignment; /* TODO: has to be set to 64 */
	/*!< model data */
	const uint8_t *data;
	/*!< model size */
	size_t size;
};

/*!
 * @brief structure that contains input and output model scaling factors.
 */
struct scaling_factors {
	/*!< model input scaling factor */
	float in;
	/*!< model output scaling factor */
	float out;
};

/*!
 * @brief Structure representing neural network request buffer in memory.
 */
struct inference_buffer {
	/*!< data stream buffer */
	uint8_t *const data;
	/*!<size indicator about the data in the buffer */
	size_t size;
};

/**
 * @brief Initializes the GNA inference service.
 *
 * This function initializes the GNA inference service and returns a pointer to the
 * gna_instance_data structure.
 *
 * @return A pointer to the gna_instance_data structure.
 */
struct gna_instance_data *inference_init(void);

/**
 * @brief Frees the resources used by the GNA inference service.
 *
 * This function frees the resources used by the GNA inference service, including the
 * gna_instance_data, gna_model_ctx, and gna_request_ctx structures.
 *
 * @param gna The pointer to the gna_instance_data structure.
 */
void inference_free(struct gna_instance_data *gna);

/* ----------- model loading flow ----------- */

/*! @brief Retrieves size of Model Context that needs to be allocated by module
 * instance.
 *
 * @param model pointer to neural network model.
 * @returns object size in bytes.
 */
uint32_t inference_get_model_ctx_size(struct inference_model *model);

/*! @brief Initializes Model Context instance in provided memory.
 *
 * @param model pointer to neural network model.
 * @param gna pointer to GNA instance data.
 * @param model_ctx_size size of model context.
 * @returns 0 on success, an error code otherwise.
 */
int inference_model_init(struct inference_model *model,
			 struct gna_instance_data *gna,
			 uint32_t model_ctx_size);

/*! @brief Retrieves scaling factors defined by neural network model
 *
 * This function retrieves the scaling factors defined by the neural network model.
 *
 * @param model_ctx Pointer to the neural network model context.
 * @param factors Pointer to the scaling factors structure.
 * @return 0 on success, an error code otherwise.
 */
int inference_get_model_scaling_factors(struct gna_model_ctx *model_ctx,
					struct scaling_factors *factors);

/*! @brief Retrieves size of Request Context that needs to be allocated by module
 * instance.
 *
 * @param model_ctx pointer to neural network model context.
 * @returns object size in bytes.
 */
uint32_t inference_get_request_ctx_size(struct gna_model_ctx *model_ctx);

/*! @brief Initializes Request Context instance in provided memory.
 *
 * @param gna pointer to GNA instance data.
 * @param request_ctx_size size of request context.
 * @returns 0 on success, an error code otherwise.
 */
int inference_request_init(struct gna_instance_data *gna, uint32_t request_ctx_size);

/* ----------- model process flow ----------- */

/*! @brief Starts inference and blocks until it is finished.
 *
 * @param gna pointer to GNA instance data
 * @returns 0 on success, an error code otherwise
 */
int inference_request_start_yield(struct gna_instance_data *gna);

/*! @brief Starts inference and returns.
 *
 * See \ref RequestQueryStatus for querying request status
 * @param gna pointer to GNA instance data
 * @returns 0 on success, an error code otherwise
 */
int inference_request_start_async(struct gna_instance_data *gna);

/**
 * @brief Checks the status of an inference request.
 *
 * @param gna The GNA instance data.
 * @return The status of the inference request.
 */
int inference_request_query_status(struct gna_instance_data *gna);

/*! @brief Resets request to its initial state.
 * State buffer is restored, input/output are not changed.
 *
 * @param gna The GNA instance data.
 * @returns 0 on success, an error code otherwise
 */
int inference_request_reset(struct gna_instance_data *gna);

/* ----------- model unload flow ----------- */

/*! @brief Releases inference request object
 *
 * Will block if request is still processing.
 *
 * @param gna The GNA instance data.
 * @returns 0 on success, an error code otherwise
 */
int inference_request_release(struct gna_instance_data *gna);

/*! @brief Releases neural network model context
 *
 * Should be called only if there is no request enqueued.
 *
 * @param gna The GNA instance data.
 * @returns 0 on success, an error code otherwise
 */
int inference_model_release(struct gna_instance_data *gna);

#endif /* __SOF_LIB_INFERENCE_SERVICE_H__ */
