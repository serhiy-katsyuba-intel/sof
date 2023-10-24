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

#include <errno.h>

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/dma.h>
#include <drivers/intel_gna34.h>
#include <soc.h>

#include "gna_defs_fw.h"

#include "gna_wa.h"
#include "gna_drv_internal.h"
#include "gna_request_db.h"


ErrorCode gna_request_db_init(gna_device *self) /* + reinit or update? */
{
	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	gna_request_db_t *GnaRequestDB = (gna_request_db_t *)&(self->GnaRequestDB);

	memset(&(self->GnaRequestBuffer), 0, GNA_REQUEST_DB_SIZE);
	GnaRequestDB->db_buffer = (gna_request_internal *)&(self->GnaRequestBuffer);
	GnaRequestDB->free_elem = GNA_REQUEST_DB_QUANTITY;
	GnaRequestDB->db_next_alloc_idx = 0;

	return ADSP_SUCCESS;
}

ErrorCode gna_request_db_alloc(const gna_device *self, gna_request_internal **pReqInt)
{

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(pReqInt != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	uint32_t idx;
	uint32_t i;
	ErrorCode ec = ADSP_GNA_DRV_FREELIST_ERROR;
	gna_request_db_t *GnaRequestDB = (gna_request_db_t *)&(self->GnaRequestDB);

	gna_request_internal *GnaRequestTable = GnaRequestDB->db_buffer;

	RETURN_EC_ON_FAIL(GnaRequestTable != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	*pReqInt = NULL;
	for (i = 0; ((i < GNA_REQUEST_DB_QUANTITY) && (*pReqInt == NULL)); i++) {
		idx = (GnaRequestDB->db_next_alloc_idx + i) % GNA_REQUEST_DB_QUANTITY;
		if (GnaRequestTable[idx].state == GNA_DB_ENTRY_EMPTY) {
			GnaRequestTable[idx].state = GNA_DB_ENTRY_IN_USE;
			*pReqInt = &GnaRequestTable[idx];
			ec = ADSP_SUCCESS;
			GnaRequestDB->db_next_alloc_idx = (idx + 1) % GNA_REQUEST_DB_QUANTITY;
			GnaRequestDB->free_elem--;
		}
	}

	return ec;
}

ErrorCode gna_request_db_setup(const gna_device *self, gna_request_internal *reqInt,
			       const gna_request *request)
{
	ErrorCode ec;
	gna_request_internal *pReqInt = NULL;

	RETURN_EC_ON_FAIL((self != NULL), ADSP_ERROR_INVALID_PARAM);
	RETURN_EC_ON_FAIL((reqInt != NULL), ADSP_ERROR_INVALID_PARAM);
	RETURN_EC_ON_FAIL((request != NULL), ADSP_ERROR_INVALID_PARAM);

	/* Search for existing internal request request */
	ec = gna_request_db_search(self, &pReqInt, request);
	RETURN_ON_ERROR(ec);

	/* request already exist */
	RETURN_EC_ON_FAIL((pReqInt == NULL), ADSP_GNA_REQUEST_EXISTS_ERROR);

	/* fill all required fields */
	reqInt->request = (gna_request *)request;
	/* Clear all execution specific fields - will be setup in gna_init_request() */
	reqInt->run_ctx = NULL;
	reqInt->my_ctx = NULL;
	reqInt->ctx_size = 0;
	reqInt->callbackFn = NULL;
	reqInt->context = NULL;
	reqInt->status = GNA_REQUEST_WAITING;
	reqInt->hw_status = 0;

	return ADSP_SUCCESS;
}

ErrorCode gna_request_db_free(const gna_device *self, gna_request_internal *reqInt)
{

	RETURN_EC_ON_FAIL((self != NULL), ADSP_ERROR_INVALID_PARAM);
	RETURN_EC_ON_FAIL((reqInt != NULL), ADSP_ERROR_INVALID_PARAM);

	gna_request_db_t *GnaRequestDB = (gna_request_db_t *)&(self->GnaRequestDB);

	/* change state */
	reqInt->request = NULL;
	reqInt->run_ctx = NULL;
	reqInt->my_ctx = NULL;
	reqInt->ctx_size = 0;
	reqInt->callbackFn = NULL;
	reqInt->context = NULL;
	reqInt->status = GNA_REQUEST_WAITING;
	reqInt->hw_status = 0;
	reqInt->state = GNA_DB_ENTRY_EMPTY;

	GnaRequestDB->free_elem++;

	return ADSP_SUCCESS;
}

ErrorCode gna_request_db_search_id(const gna_device *self, gna_request_internal **pReqInt,
				   const uint32_t request_id)
{
	int i;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(pReqInt != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	gna_request_db_t *GnaRequestDB = (gna_request_db_t *)&(self->GnaRequestDB);

	gna_request_internal *GnaRequestTable = GnaRequestDB->db_buffer;

	RETURN_EC_ON_FAIL(GnaRequestTable != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	*pReqInt = NULL;
	for (i = 0; ((i < GNA_REQUEST_DB_QUANTITY) && (*pReqInt == NULL)); i++) {
		if (GnaRequestTable[i].state != GNA_DB_ENTRY_EMPTY) {
			if (GnaRequestTable[i].request->request_id == request_id) {
				*pReqInt = &GnaRequestTable[i];
			}
		}
	}

	RETURN_EC_ON_FAIL(*pReqInt != NULL, ADSP_GNA_REQUEST_NOT_EXISTS_ERROR);

	return ADSP_SUCCESS;
}

ErrorCode gna_request_db_search(const gna_device *self, gna_request_internal **pReqInt,
				const gna_request *request)
{
	int i;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(pReqInt != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	gna_request_db_t *GnaRequestDB = (gna_request_db_t *)&(self->GnaRequestDB);

	gna_request_internal *GnaRequestTable = GnaRequestDB->db_buffer;

	RETURN_EC_ON_FAIL(GnaRequestTable != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	*pReqInt = NULL;
	for (i = 0; ((i < GNA_REQUEST_DB_QUANTITY) && (*pReqInt == NULL)); i++) {
		if (GnaRequestTable[i].state != GNA_DB_ENTRY_EMPTY) {
			if (GnaRequestTable[i].request == request) {
				*pReqInt = &GnaRequestTable[i];
			}
		}
	}

	return ADSP_SUCCESS;
}
