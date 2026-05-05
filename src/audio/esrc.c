/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#include <rtos/string.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/esrc.h>

void esrc_init(struct esrc *esrc, size_t channels)
{
	memset(esrc, 0, sizeof(*esrc));
	esrc->channels = channels;
}

void esrc_set_rate(struct esrc *esrc, uint32_t in_rate, uint32_t out_rate)
{
    if (out_rate > in_rate) {
        esrc->max_phase = ((uint64_t)in_rate << 32) / (out_rate - in_rate);
        memset(esrc->previous_sample_norm, 0, sizeof(esrc->previous_sample_norm));
    } else {
        assert(out_rate == in_rate);
        esrc->max_phase = 0;
    }
}

/*
phase_acc and max_phase should have great fractional precision hence uint64_t and << 32 (as we do not use float or double).
Phase increment is just 1 (i.e. 1 << 32).

Full-Speed feedback freaquency precision is 1 Hz (or max 1/16 = 0.0625 Hz if 14 bits fractional part is used)
Hi-Speed feedback frequency precision is also 1 Hz (or max 1/8 = 0.125 Hz if 16 bits fractional part is used)

Audio data is 32 bit.
*/

size_t esrc_process(struct esrc *esrc, const struct cir_buf_ptr *in,
		  struct cir_buf_ptr *out, size_t frames)
{
    if (esrc->max_phase == 0) {
        /* No resampling needed, just copy the data */
        cir_buf_copy(in->ptr, in->buf_start, in->buf_end, out->ptr,
		  out->buf_start, out->buf_end, sizeof(int32_t) * esrc->channels * frames);

          return 0;
    }

    const int32_t *in_ptr = in->ptr;
    int32_t *out_ptr = out->ptr;
    size_t added_frames = 0;

    /* we need esrc->max_phase to be uint64_t to keep precision when doing esrc->phase_acc rollover.
     * But we need max_phase_32 to be uint32_t to avoid overflow during multiplication.
     */
    uint32_t max_phase_32 = esrc->max_phase >> 32;

	while (frames--) {
        /* Same comment about precision as above for max_phase_32 applies here */
        uint32_t phase_acc_32 = esrc->phase_acc >> 32;

        for (size_t ch = 0; ch < esrc->channels; ch++) {
            in_ptr = cir_buf_wrap(in_ptr, in->buf_start, in->buf_end);
            out_ptr = cir_buf_wrap(out_ptr, out->buf_start, out->buf_end);

            int64_t sample_norm = ((int64_t)*in_ptr << 32) / max_phase_32;

            int64_t previous_sample_norm = esrc->previous_sample_norm[ch];
            esrc->previous_sample_norm[ch] = sample_norm;

            *out_ptr = (sample_norm * (max_phase_32 - phase_acc_32) + previous_sample_norm * phase_acc_32) >> 32;

            in_ptr++;
            out_ptr++;
        }

        esrc->phase_acc += (uint64_t)1 << 32;

        if (esrc->phase_acc >= esrc->max_phase) {
            esrc->phase_acc -= esrc->max_phase;
            added_frames++;

            /* Insert new frame here */
            for (size_t ch = 0; ch < esrc->channels; ch++) {
                out_ptr = cir_buf_wrap(out_ptr, out->buf_start, out->buf_end);

                *out_ptr = (esrc->previous_sample_norm[ch] * max_phase_32) >> 32;

                out_ptr++;
            }
        }
    }

    return added_frames;
}
