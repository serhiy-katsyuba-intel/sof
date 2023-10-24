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
#include "gna_model_db.h"

ErrorCode gna_model_db_init(gna_device *self)
{
	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_model_db_t *GnaModelDB = (gna_model_db_t *)&(self->GnaModelDB);

	GnaModelDB->buffer = (gna_model_internal *)&(self->GnaModelBuffer);
	GnaModelDB->db_next_alloc_idx = 0;
	GnaModelDB->num_free_elem = GNA_MODEL_DB_QUANTITY;

	/* fill buffer with 0 */
	memset(&(self->GnaModelBuffer), 0, GNA_MODEL_DB_SIZE);

	return ADSP_SUCCESS;
}

ErrorCode gna_model_db_alloc(const gna_device *self, gna_model_internal **model)
{
	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_model_db_t *GnaModelDB = (gna_model_db_t *)&(self->GnaModelDB);
	gna_model_internal *myModel = NULL;
	uint32_t idx;
	uint32_t i;
	ErrorCode ec = ADSP_GNA_DRV_FREELIST_ERROR;

	for (i = 0; (i < GNA_MODEL_DB_QUANTITY) && (myModel == NULL); i++) {
		idx = (GnaModelDB->db_next_alloc_idx + i) % GNA_MODEL_DB_QUANTITY;
		if (GnaModelDB->buffer[idx].state == GNA_DB_ENTRY_EMPTY) {
			/* found */
			GnaModelDB->num_free_elem--;
			myModel = &(GnaModelDB->buffer[idx]);
			myModel->use_cnt = 1; /* entry allocated by first user */
			myModel->state = GNA_DB_ENTRY_IN_USE;
			GnaModelDB->db_next_alloc_idx = (idx + 1) % GNA_MODEL_DB_QUANTITY;
			ec = ADSP_SUCCESS;
		}
	}
	*model = myModel;

	return ec;
}

ErrorCode gna_model_db_setup(const gna_device *self, gna_model_internal *model)
{
	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL((model->state != GNA_DB_ENTRY_EMPTY),
			  ADSP_GNA_ERROR);

	if (model->state == GNA_DB_ENTRY_INIT_DONE) {
		/* this model was init by other user, check UUID and increment use count */
		model->use_cnt++; /* new user for model */
	} else {
		/* setup new model */
		model->state = GNA_DB_ENTRY_INIT_DONE;
	}
	return ADSP_SUCCESS;
}

ErrorCode gna_model_db_free(const gna_device *self, gna_model_internal *model)
{
	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_model_db_t *GnaModelDB = (gna_model_db_t *)&(self->GnaModelDB);

	RETURN_EC_ON_FAIL((model->state != GNA_DB_ENTRY_EMPTY), ADSP_GNA_DRV_FREELIST_ERROR);

	RETURN_EC_ON_FAIL((model->use_cnt > 0), ADSP_GNA_DRV_FREELIST_ERROR);

	model->use_cnt--;

	if (model->use_cnt == 0) {
		GnaModelDB->num_free_elem++;
		/* clear entry */
		memset(model, 0, sizeof(gna_model_internal));
		model->state = GNA_DB_ENTRY_EMPTY;
	}

	return ADSP_SUCCESS;
}

ErrorCode gna_model_db_search_uuid(const gna_device *self, gna_model_internal **model,
				   const GNA_MODEL_UUID uuid)
{
	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	gna_model_db_t *GnaModelDB = (gna_model_db_t *)&(self->GnaModelDB);
	gna_model_internal *myModel = NULL;
	uint32_t idx;
	ErrorCode ec = ADSP_GNA_DRV_FREELIST_ERROR;

	for (idx = 0; (idx < GNA_MODEL_DB_QUANTITY) && (myModel == NULL); idx++) {
		if ((GnaModelDB->buffer[idx].state != GNA_DB_ENTRY_EMPTY) &&
		    (GnaModelDB->buffer[idx].model_id.model_global_id == uuid)) {
			/* found */
			myModel = &(GnaModelDB->buffer[idx]);
			ec = ADSP_SUCCESS;
		}
	}
	*model = myModel;

	return ec;
}

ErrorCode gna_model_db_search(const gna_device *self, gna_model_internal **model,
			      const gna_model_id *model_id)
{
	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	ErrorCode ec;

	ec = gna_model_db_search_uuid(self, model, model_id->model_global_id);

	return ec;
}
