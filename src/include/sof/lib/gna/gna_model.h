/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#ifndef __SOF_LIB_GNA_MODEL_H__
#define __SOF_LIB_GNA_MODEL_H__

#include <stdint.h>
#include <sof/list.h>
#include <drivers/intel_gna34.h>

/**
 * @brief Structure representing a GNA model context.
 *
 * This structure holds information about a GNA model.
 *
 * The `gna_extra_scratch_buffer` is a flexible array member used for additional
 * scratch buffer space.
 */
struct gna_model_ctx {
	const uint8_t *model_data;   /**< Pointer to the model data */
	size_t model_size;	     /**< Size of the model data in bytes */
	gna_model_id *model_id;	     /**< Pointer to the model ID */
	struct list_item model_item; /**< Model item in a list */

	uint8_t *user_data;	      /**< Pointer to user data */
	uint8_t *ro;		      /**< Pointer to the read-only (ro) buffer */
	uint8_t *initial_state_buffer;/**< Pointer to the initial state buffer */
	uint8_t *scratch_ptr;	      /**< Pointer to the scratch buffer */
	uint32_t ro_size;	      /**< Size of the read-only (ro) buffer in bytes */
	uint32_t input_buffer_size;   /**< Size of the input buffer in bytes */
	uint32_t output_buffer_size;  /**< Size of the output buffer in bytes */
	uint32_t state_buffer_size;   /**< Size of the state buffer in bytes */
	uint32_t scratch_buffer_size; /**< Size of the scratch buffer in bytes */
	uint32_t user_data_size;      /**< Size of the user data in bytes */
	uint32_t ldt_number;	      /**< LDT number */

#if CONFIG_INTEL_GNA34_7BAR
	uint8_t *ldt;		      /**< Pointer to the LDT buffer (7BAR) */
	uint32_t ldt_size;	      /**< Size of the LDT buffer in bytes */
#endif

	float input_scale_factor;  /**< Input scale factor */
	float output_scale_factor; /**< Output scale factor */

	uint32_t active_requests; /**< Number of active requests */

	uint8_t DCACHE_ALIGN gna_extra_scratch_buffer[0]; /**< Flexible array member for additional
							     scratch buffer space */
};

static inline bool gna_is_scratch_shared(struct gna_model_ctx *model_ctx)
{
	return model_ctx->scratch_ptr == model_ctx->gna_extra_scratch_buffer;
}

static inline size_t gna_model_get_input_buff_size(struct gna_model_ctx *model_ctx)
{
	return model_ctx->input_buffer_size;
}

static inline size_t gna_model_get_output_buff_size(struct gna_model_ctx *model_ctx)
{
	return model_ctx->output_buffer_size;
}

static inline size_t gna_model_get_state_buff_size(struct gna_model_ctx *model_ctx)
{
	return model_ctx->state_buffer_size;
}

static inline size_t gna_model_get_scratch_buff_size(struct gna_model_ctx *model_ctx)
{
	return model_ctx->scratch_buffer_size;
}

static inline size_t gna_model_get_ro_buff_size(struct gna_model_ctx *model_ctx)
{
	return model_ctx->ro_size;
}

static inline uint8_t *gna_model_get_ro(struct gna_model_ctx *model_ctx)
{
	return model_ctx->ro;
}

#endif /* __SOF_LIB_GNA_MODEL_H__ */
