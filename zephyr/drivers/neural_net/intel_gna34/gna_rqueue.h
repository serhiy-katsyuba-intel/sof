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
 * Request Queue support for GNA Driver
 * Internal driver function
 */

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_RQUEUE_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_RQUEUE_H_

typedef enum _gna_rqueue_elem_state {
	GNA_RQUEUE_EL_EMPTY = 0,
	GNA_RQUEUE_EL_WAIT = 1,
} gna_rqueue_elem_state;

typedef struct _gna_rqueue_elem_t {
	gna_request_internal *request;
	gna_rqueue_elem_state state;
} gna_rqueue_elem_t;
BUILD_ASSERT(sizeof(gna_rqueue_elem_t) == SIZE_OF_GNA_RQUEUE_ELEM_T,
	     "Wrong size of gna_rqueue_elem_t");

typedef struct _gna_rqueue_t {
	gna_rqueue_elem_t *rq_buffer;
	uint32_t rq_head_idx;
	uint32_t rq_tail_idx;
	uint32_t rq_elem;
} gna_rqueue_t;
BUILD_ASSERT(sizeof(gna_rqueue_t) == SIZE_OF_GNA_RQUEUE_T,
	     "Wrong size of gna_rqueue_t");

void gna_rqueue_lock(gna_device *self);
void gna_rqueue_unlock(gna_device *self);

ErrorCode gna_rqueue_init(gna_device *self);

ErrorCode gna_rqueue_push(gna_device *self, gna_request_internal *request);

ErrorCode gna_rqueue_pop(gna_device *self, gna_request_internal **pRequest);

ErrorCode gna_rqueue_get(gna_device *self, gna_request_internal **pRequest);

ErrorCode gna_rqueue_remove(gna_device *self, gna_request_internal *request);

bool gna_rqueue_full(gna_device *self);
bool gna_rqueue_empty(gna_device *self);

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_RQUEUE_H_ */
