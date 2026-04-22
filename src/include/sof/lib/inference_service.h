/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#ifndef __SOF_LIB_INFERENCE_SERVICE_H__
#define __SOF_LIB_INFERENCE_SERVICE_H__

#include <stdbool.h>
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

/*! @brief Opaque handle representing an HPP client */
struct hpp_client_handle {
	void *handle;
};

/*! @brief Enumeration of request parameter types for get/set */
enum inference_request_param_type {
	/*!< Get CPC for given LDT subset; out: uint32_t, in: ldt_params */
	INFERENCE_PARAM_MODEL_ICPC_PER_LAYER_SUBSET = 0,
	/*!< Set LDT layer start and count; in: ldt_params */
	INFERENCE_PARAM_REQUEST_LAYER_SUBSET,
	/*!< Set max CPC value in request; in: uint32_t */
	INFERENCE_PARAM_REQUEST_WORST_CASE_ICPC,
	/*!< Indicates last request in request queue; in: bool */
	INFERENCE_PARAM_REQUEST_LAST_IN_AUDIO_FRAME,
	/*!< Get/set priority value (LOW=0, NORMAL=1, HIGH=2); uint32_t */
	INFERENCE_PARAM_REQUEST_PRIORITY,
};

/*! @brief LDT layer subset parameters */
struct inference_ldt_params {
	/*!< Layer from which inference should start */
	uint32_t start;
	/*!< Number of layers to perform inference on */
	uint32_t count;
};

/*! @brief Source of model data for extended model init */
enum inference_model_source {
	/*!< Model provided as raw pointer */
	INFERENCE_MODEL_RAW_POINTER = 1,
	/*!< Model loaded from FTLM module by GUID */
	INFERENCE_MODEL_FTLM_MODULE = 2,
};

/*! @brief Extended model initialization parameters */
struct inference_model_cfg {
	/*!< Size of this structure in bytes */
	size_t cb;
	/*!< Source of model data */
	enum inference_model_source model_source;
	/*!< Model data source */
	union {
		/*!< Pointer to model data (when source is RAW_POINTER) */
		struct inference_model *model_data;
		/*!< GUID of FTLM module (when source is FTLM_MODULE) */
		uint32_t ftlm_module_guid[4];
	} model;
	/*!< GNA instance data */
	struct gna_instance_data *gna;
	/*!< Model context size */
	uint32_t model_ctx_size;
	/*!< GNA device instance ID (0 = default) */
	uint32_t gna_dev_instance;
	/*!< Private scratch buffer (NULL for common scratch) */
	uint8_t *private_scratch;
	/*!< Private scratch buffer size (0 for auto) */
	size_t private_scratch_size;
};

/*! @brief Chained buffer descriptor for pipelined inference */
struct inference_chained_buffer {
	/*!< Input buffer */
	struct inference_buffer input;
	/*!< Output buffer */
	struct inference_buffer output;
	/*!< Pointer to next chained buffer or NULL */
	struct inference_chained_buffer *next;
};

/*! @brief Extended request initialization parameters */
struct inference_request_cfg {
	/*!< Size of this structure in bytes */
	size_t cb;
	/*!< GNA instance data */
	struct gna_instance_data *gna;
	/*!< Request context size */
	uint32_t request_ctx_size;
	/*!< Associated model context (must be already initialized) */
	struct gna_model_ctx *model_ctx;
	/*!< Optional chained buffer descriptor (NULL if not used) */
	struct inference_chained_buffer *chain;
};

/*! @brief Extended HPP client registration parameters */
struct inference_hpp_client_cfg {
	/*!< Size of this structure in bytes */
	size_t cb;
	/*!< Output: HPP client handle */
	struct hpp_client_handle *client_id;
	/*!< Total instruction CPC budget for all requests */
	uint32_t total_icpc;
	/*!< GNA device instance ID (0 = default) */
	uint32_t gna_dev_instance;
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

/*! @brief Gets a GNA device by instance index.
 *
 * @param instance Device instance index.
 * @returns Pointer to the device, or NULL if invalid.
 */
const struct device *inference_get_device_by_instance(uint32_t instance);

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

/*! @brief Initializes Model Context on a specific GNA device instance.
 *
 * @param model pointer to neural network model.
 * @param gna pointer to GNA instance data.
 * @param model_ctx_size size of model context.
 * @param gna_dev_instance GNA device instance index.
 * @returns 0 on success, an error code otherwise.
 */
int inference_model_init(struct inference_model *model,
			 struct gna_instance_data *gna,
			 uint32_t model_ctx_size,
			 uint32_t gna_dev_instance);

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

/* ----------- V1 accessors ----------- */

/*! @brief Retrieves read-only data from neural network model.
 *
 * @param model_ctx Pointer to the model context.
 * @param ro_data Output pointer to the read-only data.
 * @param ro_size Output size of the read-only data in bytes.
 * @returns 0 on success, an error code otherwise.
 */
int inference_model_get_ro_data(struct gna_model_ctx *model_ctx,
				const uint8_t **ro_data, size_t *ro_size);

/*! @brief Retrieves the number of layer descriptors in the model.
 *
 * @param model_ctx Pointer to the model context.
 * @returns Number of layer descriptors.
 */
uint32_t inference_model_get_ldt_number(struct gna_model_ctx *model_ctx);

/*! @brief Retrieves user metadata from the model.
 *
 * @param model_ctx Pointer to the model context.
 * @param metadata Output pointer to metadata.
 * @param metadata_size Output size of metadata in bytes.
 * @returns 0 on success, an error code otherwise.
 */
int inference_model_get_user_metadata(struct gna_model_ctx *model_ctx,
				      uint8_t **metadata, size_t *metadata_size);

/* ----------- V2 layer range ----------- */

/*! @brief Updates the range of layers to execute during inference.
 *
 * @param gna The GNA instance data.
 * @param ldt_layer_start First layer index to execute.
 * @param layer_count Number of layers to execute.
 * @returns 0 on success, an error code otherwise.
 */
int inference_update_layers_range(struct gna_instance_data *gna,
				  uint32_t ldt_layer_start, uint32_t layer_count);

/* ----------- V3 HPP and parameters ----------- */

/*! @brief Gets a parameter from the request context.
 *
 * @param gna The GNA instance data.
 * @param type Parameter type to retrieve.
 * @param out_value Output buffer for the parameter value.
 * @param out_size Size of the output buffer.
 * @param in_value Optional input parameter (e.g. ldt_params for CPC query).
 * @param in_size Size of the input parameter.
 * @returns 0 on success, an error code otherwise.
 */
int inference_request_get_parameter(struct gna_instance_data *gna,
				    enum inference_request_param_type type,
				    void *out_value, uint32_t out_size,
				    const void *in_value, uint32_t in_size);

/*! @brief Sets a parameter on the request context.
 *
 * @param gna The GNA instance data.
 * @param type Parameter type to set.
 * @param in_value Input parameter value.
 * @param in_size Size of the input parameter.
 * @returns 0 on success, an error code otherwise.
 */
int inference_request_set_parameter(struct gna_instance_data *gna,
				    enum inference_request_param_type type,
				    const void *in_value, uint32_t in_size);

/*! @brief Registers an HPP (High Priority Processing) client.
 *
 * @param client_id Output: HPP client handle.
 * @param total_icpc Total instruction CPC budget.
 * @param gna_dev_instance GNA device instance ID (0 = default).
 * @returns 0 on success, an error code otherwise.
 */
int inference_register_hpp_client(struct hpp_client_handle *client_id,
				 uint32_t total_icpc,
				 uint32_t gna_dev_instance);

/*! @brief Unregisters an HPP client.
 *
 * @param client_id HPP client handle to unregister.
 * @returns 0 on success, an error code otherwise.
 */
int inference_unregister_hpp_client(struct hpp_client_handle client_id);

/*! @brief Starts inference synchronously via HPP.
 *
 * @param gna The GNA instance data.
 * @returns 0 on success, an error code otherwise.
 */
int inference_request_start_hpp_sync(struct gna_instance_data *gna);

/*! @brief Starts inference asynchronously via HPP.
 *
 * @param gna The GNA instance data.
 * @returns 0 on success, an error code otherwise.
 */
int inference_request_start_hpp_async(struct gna_instance_data *gna);

/* ----------- V4 extended API ----------- */

/*! @brief Retrieves model context size with private scratch consideration.
 *
 * @param model Pointer to the model.
 * @param private_scratch If true, excludes inline scratch from size.
 * @returns Model context size in bytes.
 */
uint32_t inference_get_model_ctx_size_ex(struct inference_model *model,
					 bool private_scratch);

/*! @brief Initializes model using extended configuration.
 *
 * @param cfg Pointer to the model configuration parameters.
 * @returns 0 on success, an error code otherwise.
 */
int inference_model_init_ex(const struct inference_model_cfg *cfg);

/*! @brief Initializes request using extended configuration.
 *
 * @param cfg Pointer to the request configuration parameters.
 * @returns 0 on success, an error code otherwise.
 */
int inference_request_init_ex(const struct inference_request_cfg *cfg);

/*! @brief Registers HPP client using extended configuration.
 *
 * @param cfg Pointer to the HPP client configuration parameters.
 * @returns 0 on success, an error code otherwise.
 */
int inference_register_hpp_client_ex(const struct inference_hpp_client_cfg *cfg);

/*! @brief Starts inference using parameters from request context.
 *
 * Priority and sync mode are taken from request context settings.
 *
 * @param gna The GNA instance data.
 * @returns 0 on success, an error code otherwise.
 */
int inference_request_start_ex(struct gna_instance_data *gna);

#endif /* __SOF_LIB_INFERENCE_SERVICE_H__ */
