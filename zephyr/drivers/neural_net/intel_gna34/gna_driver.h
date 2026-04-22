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

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_DRIVER_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_DRIVER_H_

#if CONFIG_INTEL_GNA34

#include <drivers/intel_gna34.h>
#include "regs_gna_descriptor.h"
#include "gna_sizeof.h"

#if CONFIG_INTEL_GNA34_SHARED
typedef struct _shmid_s {
	unit32_t	flags;
	uint32_t	type;
} shmid_s;
#endif /* CONFIG_INTEL_GNA34_SHARED */

typedef struct _gna_device {
#if CONFIG_INTEL_GNA34_SHARED
    /*  Shared memory descriptor. */
DCACHE_ALIGN
	shmid_s		shm;

DCACHE_ALIGN
#endif /* CONFIG_INTEL_GNA34_SHARED */
	/* GNA device data configuration. */
	uint32_t init_done;
	gna_capabilities capabilities;
	uint32_t int_buff_size;
	uint32_t ce_num;
	uint32_t ple_num;
	uint32_t afe_num;
	uint32_t registered_cores_mask;
	uint32_t cores_ie_mask;
	uint32_t current_core;
	uint32_t base_addr;
	int irq_no;
	uint32_t instance_no;

	/* queue and list headers */
	uint8_t GnaRqueue[SIZE_OF_GNA_RQUEUE_T] DCACHE_ALIGN;
	uint8_t GnaRequestDB[SIZE_OF_GNA_REQUEST_DB_T] DCACHE_ALIGN;
	uint8_t GnaModelDB[SIZE_OF_GNA_MODEL_DB_T] DCACHE_ALIGN;

	/* Buffers for data base */
	uint8_t GnaRqueueBuffer[GNA_RQUEUE_SIZE] DCACHE_ALIGN;
	uint8_t GnaRequestBuffer[GNA_REQUEST_DB_SIZE] DCACHE_ALIGN;
	uint8_t GnaModelBuffer[GNA_MODEL_DB_SIZE] DCACHE_ALIGN;

	uint32_t queue_processing_active;
#if defined(GNA_DRV_WA_POLLING) && (GNA_DRV_WA_POLLING == 1)
	struct device *gen_dev; /* generic device pointer */
#endif

#if CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR
/* Space for GNA descriptor */
DCACHE_ALIGN
	GNA_DESC_MMU_DIS	run_gna_descriptor;
#endif /* CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR */

#if CONFIG_INTEL_GNA34_SHARED
DCACHE_ALIGN
	uint32_t	_alignment[0];
#endif /* SUPPORTED(MULTI_CORE) */

} gna_device;

/*! Explicit function to turn the GNA/ANNA device power ON. */
/*!
 * \param self - A pointer to the GNA device object,
 */
ErrorCode gna_device_power_on(gna_device *self);

/*! Explicit function to turn the GNA/ANNA device power OFF. */
/*!
 * \param self - A pointer to the GNA device object,
 */
ErrorCode gna_device_power_off(gna_device *self);

/*! The function returns GNA device capabilities. */
/*!
 * \param dev    - A pointer to the device object,
 * \param caps   - GNA device capabilities will be returned via this parameter,
 */
ErrorCode gna_device_get_caps(const struct device *dev, gna_capabilities *caps);

/*! Initializes the GNA device. */
/*!
 * \param dev    - a pointer to the device object.
 *
 * \returns ErrorCode
 * \retval ADSP_SUCCESS If successfully initialized given gna_device.
 * \retval ADSP_GNA_ERROR If failed to initialize given gna_device.
 * \retval ADSP_ERROR_NULL_POINTER_AS_PARAM If NULL has been passed as one of the parameters.
 */
int gna_device_init(const struct device *dev);

/*! Inits new GNA model. */
/*!
 * \param dev                - A pointer to the device object,
 * \param model_id           - return parameter,
 * \param model_ctx          - Pointer to model context data,
 * \param gna_buffer         - Pointer to buffer that contains GNA descriptor and scratch buffer,
 * \param model_global_id    - Global id of the model to be used by modules to access it
 */
ErrorCode gna_init_model(const struct device *dev, gna_model_id **model_id, void *model_ctx,
			 void *gna_buffer, GNA_MODEL_UUID model_global_id);

/*! Sets up the GNA model blob. */
/*!
 * \param dev                        - A pointer to the device object,
 * \param model_id                   - model id structure,
 * \param model_ldt_num_entries      - number of entries in LDT table,
 * \param model_ro_size              - size of RO data of the model,
 * \param model                      - A pointer to GNA buffer that contains LDT and RO data
 *
 */
ErrorCode gna_setup_model(const struct device *dev, gna_model_id *model_id,
			  uint32_t model_ldt_num_entries, uint32_t model_ro_size, void *model);

/*! The function completely removes specified GNA model. */
/*!
 * \param dev        - A pointer to the device object,
 * \param model_id   - model id structure
 */
ErrorCode gna_destroy_model(const struct device *dev, const gna_model_id *model_id);

/*! The function returns pointer to model if model with defined model_global_id was already
 * initialized..
 */
/*!
 * \param dev                - A pointer to the device object,
 * \param model_id           - Model that may be returned,
 * \param model_global_id    - Global id of the model to be used by modules to access it,
 */
ErrorCode gna_get_model(const struct device *dev, gna_model_id **model_id,
			GNA_MODEL_UUID model_global_id);

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
 * \param layers_count       - A number of layers to be processed by GNA, starting from
 *			       layer pointed by layer_offset.
 *			       This parameter value may vary
 *			       from 1 to (intel_gna_model_id_header→layer_count),
 */
ErrorCode gna_init_request(const struct device *dev, gna_request *request,
			   const gna_model_id *model_id, void *state, uint32_t model_state_size,
			   uint32_t layer_offset, uint32_t layer_count);

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
ErrorCode gna_request_enqueue(const struct device *dev, const gna_request *request,
			      const void *input, uint32_t input_size, void *output,
			      uint32_t output_size, pfn_gna_request_done request_callback,
			      void *request_ctx);

/*! The function obtains a current status of specified request. */
/*!
 * \param dev        - A pointer to the device object,
 * \param request    - A request for which results shall be obtained,
 * \param status     - Status of the request,
 * \param hw_status  - A dump of a GNA Status Register for a given request,
 */
ErrorCode gna_get_request_status(const struct device *dev, gna_request *request,
				 gna_request_status *status, uint32_t *hw_status);

/*! The function aborts request if request already being processed by GNA or removes it from
 *  internal requests FIFO
 * \param dev        - A pointer to the device object,
 * \param request    - A pointer to request which shall be aborted,
 */
ErrorCode gna_abort_request(const struct device *dev, gna_request *request);

/*! The function restores GNA HW configuration after D3. */
/*!
 * \param dev        - A pointer to the device object,
 */
ErrorCode gna_device_restore(const struct device *dev);

#endif /* CONFIG_INTEL_GNA34 */

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_DRIVER_H_ */

/*! @} @} */
