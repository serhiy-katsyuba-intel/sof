/******************************************************************************
 * INTEL CONFIDENTIAL
 * Copyright 2023 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to the
 * source code ("Material") are owned by Intel Corporation or its suppliers or
 * licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material may contain trade secrets and
 * proprietary and confidential information of Intel Corporation and its
 * suppliers and licensors, and is protected by worldwide copyright and trade
 * secret laws and treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted, distributed,
 * or disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery of
 * the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing
 *
 *******************************************************************************/
#ifndef ZEPHYR_INCLUDE_DRIVERS_INTEL_GNA34_H_
#define ZEPHYR_INCLUDE_DRIVERS_INTEL_GNA34_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#if CONFIG_INTEL_GNA34

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIGFW_ADSP_DCACHE_LINE_ALIGNMENT_SIZE 64

#ifndef DCACHE_ALIGN
#ifndef CONFIGFW_ADSP_DCACHE_LINE_ALIGNMENT_SIZE
#error "CONFIGFW_ADSP_DCACHE_LINE_ALIGNMENT_SIZE is not defined!"
#endif
#define DCACHE_ALIGN __aligned(CONFIGFW_ADSP_DCACHE_LINE_ALIGNMENT_SIZE)
#endif

#define GNA_DRIVER_0 "GNA_DRIVER_0"

/* TODO temp definition */
#define GNA_MODEL_UUID uint64_t

/*!
 * Alignments requirements (defined in bytes)
 * for GNA hardware & driver operational purposes.
 */
#define GNA_INPUTS_ALIGNMENT     CACHE_ALIGNMENT
#define GNA_OUTPUTS_ALIGNMENT    GNA_INPUTS_ALIGNMENT
#define GNA_DRV_BUFFER_ALIGNMENT GNA_INPUTS_ALIGNMENT
#define GNA_MODEL_ALIGNMENT      128
#define GNA_MODEL_MAX_LACNT      8191

#ifndef ErrorCode
typedef int ErrorCode;
#endif

/*!
 * \brief Header describing parameters of dumped model.
 *
 * Structured is partially filled by GNADumpXnn with parameters necessary for embedded,
 * other fields are populated by user as necessary.
 */

/*! GNA model ID */
/*!
 *  model_desc_id           - unique model ID assigned by GNA driver,
 *  model_ro_size           - size of RO data of the model,
 *  model_ldt_num_entries   - number of entries in LDT table,
 *  gna_ctx_ptr             - a pointer to location in which GNA Descriptor is located,
 *  model_ctx_ptr           - a pointer to private GNA model descriptor table,
 *  input_config_ptr        - a pointer to model config data,
 *  model_ro_ptr            - a pointer to read-only data of the GNA model,
 *  model_global_id         - global unique id of the model defined as UUID.
 *			      UUID is 128 bit global unique value of following format:
 *			      xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx where M indicates UUID version,
 *			      N - UUID variant.
 */
typedef struct _gna_model_id {
	uint32_t model_id;
	uint32_t model_ro_size;
	uint32_t model_ldt_num_entries;
	void *gna_ctx_ptr;
	void *model_ctx_ptr;
	void *input_config_ptr;
	void *model_ro_ptr;
	GNA_MODEL_UUID model_global_id;
} gna_model_id;

/*! Enum describing statues of GNA requests. */
typedef enum _gna_request_status {
	GNA_REQUEST_WAITING = 0,
	GNA_REQUEST_IN_PROGRESS = 1,
	GNA_REQUEST_COMPLETED = 2,
	GNA_REQUEST_STATUS_TIMEOUT = 3,
	GNA_REQUEST_ERRED = 4,
} gna_request_status;

/*! The GNA request structure. It shall be allocated by gna_init_request function. */
/*!
 *  request_id          - A unique ID of the request,
 *  model_id            - A unique ID of gna model,
 *  inputs              - A pointer to the memory where the inputs data to be processed
 *                        are being stored,
 *  outputs             - A pointer to the memory where the outputs data shall be placed,
 *  state               - A pointer to the memory where the state data shall be placed,
 *  model_state_size    - Size of state buffer,
 *  layer_offset        - An offset of the first layer from which GNA processing shall start.
 *			  This parameter value may vary from 0 to
 *			  (intel_gna_model_id_header→layer_count - 1),
 *  layers_count        - A number of layers to be processed by GNA,
 *			  starting from layer pointed by layer_offset.
 *			  This parameter value may vary from 1 to
 *			  (intel_gna_model_id_header→layer_count),
 *  submit_timestmp     - A timestamp, this value is set by the driver when request is submitted
 *			  for processing,
 *  start_timestmp      - A timestamp, this value is set by the driver when HW starts processing
 *			  the request,
 *  stop_timestmp       - A timestamp, this value is set by the driver when HW stops processing
 *			  the request,
 *  ptc_cycles          - GNA Performance Total Cycles (PTC) register value: cycles that the GNA
 *			  was in "Score in Progress" state
 *  psc_cycles          - GNA Performance Stall Cycles (PSC) register value: stall cycles that
 *			  the GNA had since last "Score in Progress" state.
 */
typedef struct _gna_request {
	uint32_t request_id;
	gna_model_id *model_id;
	void *inputs;
	void *outputs;
	void *state;
	uint32_t model_state_size;
	uint32_t layer_offset;
	uint32_t layers_count;
	uint64_t submit_timestmp;
	uint64_t start_timestmp;
	uint64_t stop_timestmp;
#if CONFIG_INTEL_GNA34_HW_STATS
	uint32_t ptc_cycles;
	uint32_t psc_cycles;
#endif
} gna_request;

/*! GNA capabilities structure */
/*!
 *  version      - GNA version,
 *  mmu_enabled  - Indicates whether MMU enabled,
 *  ae_supported - Indicates whether Autonomous Extension (AE) is supported,
 */
typedef struct _gna_capabilities {
	uint32_t version;
	uint32_t mmu_enabled;
	uint32_t ae_supported;
} gna_capabilities;

/*! The callback that will be executed once request is completed. */
/*!
 * \param dev        - A pointer to the device object,
 * \param context    - Context specified in gna_init_request,
 * \param request_id - GNA request that has been completed,
 * \param status     - status of the request
 * \param hw_status  - A dump of a GNA Status Register for a given request,
 */
typedef void (*pfn_gna_request_done)(const struct device *dev, void *context, uint32_t request_id,
				     gna_request_status status, uint32_t hw_status);

typedef ErrorCode (*gna_device_get_caps_fn)(const struct device *dev, gna_capabilities *caps);
typedef ErrorCode (*gna_init_model_fn)(const struct device *dev, gna_model_id **model_id,
				       void *model_ctx, void *gna_buffer,
				       GNA_MODEL_UUID model_global_id);
typedef ErrorCode (*gna_setup_model_fn)(const struct device *dev, gna_model_id *model_id,
					uint32_t model_ldt_num_entries, uint32_t model_ro_size,
					void *model);
typedef ErrorCode (*gna_destroy_model_fn)(const struct device *dev, const gna_model_id *model_id);
typedef ErrorCode (*gna_get_model_fn)(const struct device *dev, gna_model_id **model_id,
				      GNA_MODEL_UUID model_global_id);
typedef ErrorCode (*gna_init_request_fn)(const struct device *dev, gna_request *request,
					 const gna_model_id *model_id, void *state,
					 uint32_t model_state_size, uint32_t layer_offset,
					 uint32_t layer_count);
typedef ErrorCode (*gna_request_enqueue_fn)(const struct device *dev, const gna_request *request,
					    const void *input, uint32_t input_size, void *output,
					    uint32_t output_size,
					    pfn_gna_request_done request_callback,
					    void *request_ctx);
typedef ErrorCode (*gna_get_request_status_fn)(const struct device *dev, gna_request *request,
					       gna_request_status *status, uint32_t *hw_status);
typedef ErrorCode (*gna_abort_request_fn)(const struct device *dev, gna_request *request);
typedef ErrorCode (*gna_restore_fn)(const struct device *dev);

struct gna_driver_api {
	gna_device_get_caps_fn device_get_caps;
	gna_init_model_fn init_model;
	gna_setup_model_fn setup_model;
	gna_destroy_model_fn destroy_model;
	gna_get_model_fn get_model;
	gna_init_request_fn init_request;
	gna_request_enqueue_fn request_enqueue;
	gna_get_request_status_fn get_request_status;
	gna_abort_request_fn abort_request;
	gna_restore_fn restore;
};

/*! The function returns GNA device capabilities. */
/*!
 * \param dev    - A pointer to the device object,
 * \param caps   - GNA device capabilities will be returned via this parameter,
 */
static inline ErrorCode intel_gna34_device_get_caps(const struct device *dev,
						    gna_capabilities *caps)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->device_get_caps(dev, caps);
}

/*! Inits new GNA model. */
/*!
 * \param dev                - A pointer to the device object,
 * \param model_id           - return parameter,
 * \param model_ctx          - Pointer to model context data,
 * \param gna_buffer         - Pointer to buffer that contains GNA descriptor and scratch buffer,
 * \param model_global_id    - Global id of the model to be used by modules to access it
 */
static inline ErrorCode intel_gna34_init_model(const struct device *dev, gna_model_id **model_id,
					       void *model_ctx, void *gna_buffer,
					       GNA_MODEL_UUID model_global_id)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->init_model(dev, model_id, model_ctx, gna_buffer, model_global_id);
}

/*! Sets up the GNA model blob. */
/*!
 * \param dev                        - A pointer to the device object,
 * \param model_id                   - model id structure,
 * \param model_ldt_num_entries      - number of entries in LDT table,
 * \param model_ro_size              - size of RO data of the model,
 * \param model                      - A pointer to GNA buffer that contains LDT and RO data
 *
 */
static inline ErrorCode intel_gna34_setup_model(const struct device *dev, gna_model_id *model_id,
						uint32_t model_ldt_num_entries,
						uint32_t model_ro_size, void *model)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->setup_model(dev, model_id, model_ldt_num_entries, model_ro_size, model);
}

/*! The function completely removes specified GNA model. */
/*!
 * \param dev        - A pointer to the device object,
 * \param model_id   - model id structure
 */
static inline ErrorCode intel_gna34_destroy_model(const struct device *dev,
						  const gna_model_id *model_id)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->destroy_model(dev, model_id);
}

/*! The function returns pointer to model if model with defined model_global_id was already
 *  initialized..
 */
/*!
 * \param dev                - A pointer to the device object,
 * \param model_id           - Model that may be returned,
 * \param model_global_id    - Global id of the model to be used by modules to access it,
 */
static inline ErrorCode intel_gna34_get_model(const struct device *dev, gna_model_id **model_id,
					      GNA_MODEL_UUID model_global_id)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->get_model(dev, model_id, model_global_id);
}

/*! The function initializes request object with values given as parameters. */
/*!
 * It also assigns a unique ID for every request.
 * \param dev                - A pointer to the device object,
 * \param request            - output gna_request structure,
 * \param model_id           - gna model structure,
 * \param state              - A pointer to the memory where the state data shall be placed,
 * \param model_state_size`  - Size of state buffer,
 * \param layer_offset       - An offset of the first layer from which GNA processing shall start.
 *			       This parameter value may vary
 *			       from 0 to (intel_gna_model_id_header→layer_count - 1),
 * \param layers_count       - A number of layers to be processed by GNA,
 *			       starting from layer pointed by layer_offset.
 *			       This parameter value may vary from 1 to
 *			       (intel_gna_model_id_header->layer_count),
 */
static inline ErrorCode intel_gna34_init_request(const struct device *dev, gna_request *request,
						 const gna_model_id *model_id, void *state,
						 uint32_t model_state_size, uint32_t layer_offset,
						 uint32_t layer_count)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->init_request(dev, request, model_id, state, model_state_size, layer_offset,
				 layer_count);
}

/*! The function enqueues a GNA scoring request into the GNA request pool. */
/*!
 * It also triggers GNA scoring operation.
 * \param dev                - A pointer to the device object,
 * \param request            - GNA request initialized by gna_init_request function,
 * \param input              - A pointer to input data,
 * \param output             - A pointer to inference results,
 * \param request_callback   - Callback that should be executed once request is completed.
 *			       Note that callback is executed in the interrupt context,
 * \param request_ctx        - Context that should be passed to callback,
 */
static inline ErrorCode
intel_gna34_request_enqueue(const struct device *dev, const gna_request *request, const void *input,
			    uint32_t input_size, void *output, uint32_t output_size,
			    pfn_gna_request_done request_callback, void *request_ctx)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->request_enqueue(dev, request, input, input_size, output, output_size,
				    request_callback, request_ctx);
}

/*! The function obtains a current status of specified request. */
/*!
 * \param dev        - A pointer to the device object,
 * \param request    - A request for which results shall be obtained,
 * \param status     - Status of the request,
 * \param hw_status  - A dump of a GNA Status Register for a given request,
 */
static inline ErrorCode _impl_gna_api_get_request_status(const struct device *dev,
							 gna_request *request,
							 gna_request_status *status,
							 uint32_t *hw_status)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->get_request_status(dev, request, status, hw_status);
}

ErrorCode gna_api_get_request_status(const struct device *dev, gna_request *request,
				     gna_request_status *status, uint32_t *hw_status);

/*! The function aborts request if request already being processed by GNA or removes it from
 *  internal requests FIFO
 * \param dev        - A pointer to the device object,
 * \param request    - A request which shall be aborted,
 */
static inline ErrorCode intel_gna34_abort_request(const struct device *dev, gna_request *request)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->abort_request(dev, request);
}

static inline ErrorCode intel_gna34_restore(const struct device *dev)
{
	const struct gna_driver_api *api = (const struct gna_driver_api *)dev->api;

	return api->restore(dev);
}

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_INTEL_GNA34 */

#endif /* ZEPHYR_INCLUDE_DRIVERS_INTEL_GNA34_H_ */
