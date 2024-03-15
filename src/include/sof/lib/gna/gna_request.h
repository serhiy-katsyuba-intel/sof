/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#ifndef __SOF_LIB_GNA_REQUEST_H__
#define __SOF_LIB_GNA_REQUEST_H__

#include <sof/lib/gna/gna_model.h>

/**
 * @brief Structure representing a GNA request context.
 *
 * This structure holds information related to a GNA request.
 */
struct gna_request_ctx {
	struct gna_model_ctx *model; /**< Pointer to the GNA model context */
	gna_request gna_request;     /**< The GNA request */ /* TODO: change field to request*/
	bool in_progress;	     /**< Flag indicating if the request is in progress */
	gna_request_status cached_request_status; /**< Cached request status */

	/* GNA request task data */
	struct task gna_request_task;		   /**< GNA request task */

	uint8_t *input_buffer;	/**< Pointer to the input buffer */
	uint8_t *output_buffer; /**< Pointer to the output buffer */
	uint8_t *state_buffer;	/**< Pointer to the state buffer */

	uint32_t DCACHE_ALIGN pad[0]; /**< Padding */
};

static inline uint8_t *gna_request_get_input_buff(struct gna_request_ctx *ctx)
{
	return ctx->input_buffer;
}

static inline uint8_t *gna_request_get_output_buff(struct gna_request_ctx *ctx)
{
	return ctx->output_buffer;
}

static inline uint8_t *gna_request_get_state_buff(struct gna_request_ctx *ctx)
{
	return ctx->state_buffer;
}

#endif /* __SOF_LIB_GNA_REQUEST_H__ */
