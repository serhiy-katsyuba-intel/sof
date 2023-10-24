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

/*!
 * Internal driver function definitions
 */

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_DRV_INTERNAL_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_DRV_INTERNAL_H_

#include "gna_driver.h"

#include "gna_sizeof.h"
#include "gna_wa.h"

/*!< default values that marks gna memory as free */
#define FREE_SLAB_U32 0xFFFFFFFF
#define FREE_SLAB_U16 0xFFFF
#define FREE_SLAB_U8  0xFF

#define GNA_DESC_SIZE 32

typedef enum _gna_db_entry_state {
	GNA_DB_ENTRY_EMPTY = 0,
	GNA_DB_ENTRY_IN_USE = 1,
	GNA_DB_ENTRY_INIT_DONE = 2,
} gna_db_entry_state;

/* This request structure points to original request and contains information required by HW
 * Assumption: my_ctx contain pointer to updated GNA descriptor with in/out/state BAR set properly
 * so we can copy my_ctx to run_ctx, set GNA Descriptor BAR to run_ctx and start HW.
 */
typedef struct _gna_request_internal {
	gna_db_entry_state state;
	gna_request *request; /* original request pointer. */
	uint32_t core_id;     /* Core ID set when request is pushed to the queue */

	/* gna model info */
	void *run_ctx;     /* GNA descriptor buffer pointer */
	void *my_ctx;      /* pointer to pre-configured context - will be copied to run_ctx */
	uint32_t ctx_size; /* copy size */
	void *xnn_state;   /* xNN state buffer */
	uint32_t xnn_state_size;
	const void *input; /* input buffer */
	uint32_t input_size;
	void *output; /* output buffer */
	uint32_t output_size;

	/* GNA request callback parameters */
	pfn_gna_request_done callbackFn;
	void *context;
	gna_request_status status; /* current request status */
	uint32_t hw_status;        /* HW status updated by ISR */
} gna_request_internal;
BUILD_ASSERT(sizeof(gna_request_internal) == SIZE_OF_GNA_REQUEST_DB_ELEM_T,
	     "Wrong size of gna_request_internal");


typedef struct _gna_model_internal {
	gna_model_id model_id; /* local copy of model id structure -
				* must be first element (36/40)
				*/
	uint32_t use_cnt;      /* model can be reused - number of users -
				* remove only when this is 0
				*/
	gna_db_entry_state state;
	void *ro_space;
	void *in_space;
	void *out_space;
	void *state_space;
	void *ld_space;
} gna_model_internal;
BUILD_ASSERT(sizeof(gna_model_internal) == SIZE_OF_GNA_MODEL_DB_ELEM_T,
	     "Worong size of gna_model_internal");

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_DRV_INTERNAL_H_ */
