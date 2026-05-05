/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_ESRC_H__
#define __SOF_AUDIO_ESRC_H__

#include <stddef.h>
#include <stdint.h>

/// TODO: move it later to a more generic header!!!
struct cir_buf_ptr {
	void *buf_start;
	void *buf_end;
	void *ptr;
};

struct esrc {
	size_t channels;
	int64_t previous_sample_norm[8];
	uint64_t phase_acc;
	uint64_t max_phase;
};

void esrc_init(struct esrc *esrc, size_t channels);
void esrc_set_rate(struct esrc *esrc, uint32_t in_rate, uint32_t out_rate);
size_t esrc_process(struct esrc *esrc, const struct cir_buf_ptr *in,
		  struct cir_buf_ptr *out, size_t frames);

#endif /* __SOF_AUDIO_ESRC_H__ */
