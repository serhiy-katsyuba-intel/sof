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
 * Model List/Data Base - keep track for all registered models
 * Internal driver function
 */

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_REQUEST_DB_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_REQUEST_DB_H_

typedef struct _gna_request_db_t {
	gna_request_internal *db_buffer;
	uint32_t db_next_alloc_idx;
	uint32_t free_elem;
} gna_request_db_t;
BUILD_ASSERT(sizeof(gna_request_db_t) == SIZE_OF_GNA_REQUEST_DB_T,
	     "Wrong size of gna_request_db_t");

ErrorCode gna_request_db_init(gna_device *self);
ErrorCode gna_request_db_alloc(const gna_device *self, gna_request_internal **pReqInt);
ErrorCode gna_request_db_setup(const gna_device *self, gna_request_internal *reqInt,
			       const gna_request *request);
ErrorCode gna_request_db_free(const gna_device *self, gna_request_internal *reqInt);
ErrorCode gna_request_db_search_id(const gna_device *self, gna_request_internal **pReqInt,
				   const uint32_t request_id);
ErrorCode gna_request_db_search(const gna_device *self, gna_request_internal **pReqInt,
				const gna_request *request);

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_REQUEST_DB_H_ */
