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
#include "gna_rqueue.h"

void gna_rqueue_lock(gna_device *self)
{
}

void gna_rqueue_unlock(gna_device *self)
{
}

ErrorCode gna_rqueue_init(gna_device *self)
{
	gna_rqueue_lock(self);
	gna_rqueue_t *GnaRqueue = (gna_rqueue_t *)&(self->GnaRqueue);

	GnaRqueue->rq_buffer = (gna_rqueue_elem_t *)&(self->GnaRqueueBuffer);
	GnaRqueue->rq_head_idx = GnaRqueue->rq_tail_idx = 0;
	GnaRqueue->rq_elem = 0;

	/* fill buffer with 0 */
	memset(&(self->GnaRqueueBuffer), 0, GNA_RQUEUE_SIZE);

	gna_rqueue_unlock(self);

	return ADSP_SUCCESS;
}

ErrorCode gna_rqueue_push(gna_device *self, gna_request_internal *request)
{
	gna_rqueue_elem_t *queue_head;
	gna_rqueue_t *GnaRqueue = (gna_rqueue_t *)&(self->GnaRqueue);

	if (gna_rqueue_full(self)) {
		return ADSP_GNA_DRV_QUEUE_ERROR;
	}

	/* lock queue for changes */
	gna_rqueue_lock(self);

	queue_head = &(GnaRqueue->rq_buffer[GnaRqueue->rq_head_idx]);
	GnaRqueue->rq_head_idx = (GnaRqueue->rq_head_idx + 1) % GNA_RQUEUE_QUANTITY;
	GnaRqueue->rq_elem++;

	/* unlock queue and fill entry */
	gna_rqueue_unlock(self);

	queue_head->request = request;
	queue_head->request->core_id = 0; /* TODO: arch_cpu_get_current_cpu_id(); */
	queue_head->state = GNA_RQUEUE_EL_WAIT;

	return ADSP_SUCCESS;
}

ErrorCode gna_rqueue_pop(gna_device *self, gna_request_internal **pRequest)
{
	gna_rqueue_elem_t *queue_tail;
	bool found = false;
	gna_rqueue_t *GnaRqueue = (gna_rqueue_t *)&(self->GnaRqueue);

	if (gna_rqueue_empty(self)) {
		*pRequest = NULL;
		return ADSP_GNA_DRV_QUEUE_ERROR;
	}

	gna_rqueue_lock(self);
	do {
		queue_tail = &(GnaRqueue->rq_buffer[GnaRqueue->rq_tail_idx]);
		if ((queue_tail->state != GNA_RQUEUE_EL_EMPTY) && !found) {
			found = true;
			*pRequest = queue_tail->request;
			queue_tail->state = GNA_RQUEUE_EL_EMPTY; /* clean this entry */
		}

		GnaRqueue->rq_tail_idx = (GnaRqueue->rq_tail_idx + 1) % GNA_RQUEUE_QUANTITY;
		GnaRqueue->rq_elem--;

	} while (!gna_rqueue_empty(self) && !found);

	/* now clean next entries if EMPTY */
	queue_tail = &(GnaRqueue->rq_buffer[GnaRqueue->rq_tail_idx]);
	while (!gna_rqueue_empty(self) && (queue_tail->state == GNA_RQUEUE_EL_EMPTY)) {
		GnaRqueue->rq_tail_idx = (GnaRqueue->rq_tail_idx + 1) % GNA_RQUEUE_QUANTITY;
		GnaRqueue->rq_elem--;
		queue_tail = &(GnaRqueue->rq_buffer[GnaRqueue->rq_tail_idx]);
	};
	gna_rqueue_unlock(self);

	if (!found) {
		*pRequest = NULL;
		return ADSP_GNA_DRV_QUEUE_ERROR;
	}

	return ADSP_SUCCESS;
}

ErrorCode gna_rqueue_get(gna_device *self, gna_request_internal **pRequest)
{
	gna_rqueue_elem_t *queue_tail;
	gna_rqueue_t *GnaRqueue = (gna_rqueue_t *)&(self->GnaRqueue);

	if (gna_rqueue_empty(self)) {
		*pRequest = NULL;
		return ADSP_GNA_DRV_QUEUE_ERROR;
	}

	queue_tail = &(GnaRqueue->rq_buffer[GnaRqueue->rq_tail_idx]);

	*pRequest = queue_tail->request;

	return ADSP_SUCCESS;
}

ErrorCode gna_rqueue_remove(gna_device *self, gna_request_internal *request)
{
	gna_rqueue_elem_t *queue_elem;
	uint32_t elem_idx;
	uint32_t elem_cnt;
	bool found = false;
	gna_rqueue_t *GnaRqueue = (gna_rqueue_t *)&(self->GnaRqueue);

	if (gna_rqueue_empty(self)) {
		return ADSP_GNA_DRV_QUEUE_ERROR;
	}

	gna_rqueue_lock(self);

	elem_idx = GnaRqueue->rq_tail_idx;
	/* search for request */
	for (elem_cnt = GnaRqueue->rq_elem; ((elem_cnt > 0) && !found); elem_cnt--) {
		queue_elem = &(GnaRqueue->rq_buffer[elem_idx]);
		if ((queue_elem->state != GNA_RQUEUE_EL_EMPTY) &&
		    (queue_elem->request == request)) {
			found = true;
		} else {
			elem_idx = (elem_idx + 1) % GNA_RQUEUE_QUANTITY;
		}
	}

	if (found) {
		queue_elem->state = GNA_RQUEUE_EL_EMPTY;
		if (elem_idx == GnaRqueue->rq_tail_idx) {
			/* Tail is removed, check other empty entries */
			do {
				GnaRqueue->rq_tail_idx =
					(GnaRqueue->rq_tail_idx + 1) % GNA_RQUEUE_QUANTITY;
				GnaRqueue->rq_elem--;
				queue_elem = &(GnaRqueue->rq_buffer[GnaRqueue->rq_tail_idx]);
			} while (!gna_rqueue_empty(self) &&
				 (queue_elem->state == GNA_RQUEUE_EL_EMPTY));
		}
	}

	gna_rqueue_unlock(self);
	return ADSP_SUCCESS;
}

bool gna_rqueue_full(gna_device *self)
{
	gna_rqueue_t *GnaRqueue = (gna_rqueue_t *)&(self->GnaRqueue);

	return (GnaRqueue->rq_elem >= GNA_RQUEUE_QUANTITY);
}

bool gna_rqueue_empty(gna_device *self)
{
	gna_rqueue_t *GnaRqueue = (gna_rqueue_t *)&(self->GnaRqueue);

	return (GnaRqueue->rq_elem == 0);
}
