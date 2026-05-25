/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

//#include <rtos/string.h>
//#include <sof/audio/audio_stream.h>
#include <zephyr/drivers/uaol.h>
#include <rtos/string.h>
#include <sof/tlv.h>

/* for stuff move from dai-zephyr.c */
#include <sof/audio/component_ext.h>
#include <sof/lib/dai-zephyr.h>


#include <sof/audio/uaol.h>

LOG_MODULE_REGISTER(uaol, CONFIG_SOF_LOG_LEVEL);

struct ipc4_uaol_link_capabilities {
	uint32_t input_streams_supported          : 4;
	uint32_t output_streams_supported         : 4;
	uint32_t bidirectional_streams_supported  : 5;
	uint32_t rsvd                             : 19;
	uint32_t max_tx_fifo_size;
	uint32_t max_rx_fifo_size;
} __packed __aligned(4);

struct ipc4_uaol_capabilities {
	uint32_t link_count;
	struct ipc4_uaol_link_capabilities link_caps[];
} __packed __aligned(4);

#define DEV_AND_COMMA(node) DEVICE_DT_GET(node),
static const struct device *uaol_devs[] = {
	DT_FOREACH_STATUS_OKAY(intel_adsp_uaol, DEV_AND_COMMA)
};

#if !CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY
__cold void tlv_value_set_uaol_caps(struct sof_tlv *tuple, uint32_t type)
{
	const size_t dev_count = ARRAY_SIZE(uaol_devs);
	struct uaol_capabilities dev_cap;
	struct ipc4_uaol_capabilities *caps = (struct ipc4_uaol_capabilities *)tuple->value;
	size_t caps_size = offsetof(struct ipc4_uaol_capabilities, link_caps[dev_count]);
	size_t i;
	int ret;

	assert_can_be_cold();

	memset(caps, 0, caps_size);

	caps->link_count = dev_count;
	for (i = 0; i < dev_count; i++) {
		ret = uaol_get_capabilities(uaol_devs[i], &dev_cap);
		if (ret)
			continue;

		caps->link_caps[i].input_streams_supported = dev_cap.input_streams;
		caps->link_caps[i].output_streams_supported = dev_cap.output_streams;
		caps->link_caps[i].bidirectional_streams_supported = dev_cap.bidirectional_streams;
		caps->link_caps[i].max_tx_fifo_size = dev_cap.max_tx_fifo_size;
		caps->link_caps[i].max_rx_fifo_size = dev_cap.max_rx_fifo_size;
	}

	tlv_value_set(tuple, type, caps_size, caps);
}
#endif /* CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY */

__cold int uaol_stream_id_to_hda_link_stream_id(int uaol_stream_id)
{
	size_t dev_count = ARRAY_SIZE(uaol_devs);
	size_t i;

	assert_can_be_cold();

	for (i = 0; i < dev_count; i++) {
		int hda_link_stream_id = uaol_get_mapped_hda_link_stream_id(uaol_devs[i],
									    uaol_stream_id);
		if (hda_link_stream_id >= 0)
			return hda_link_stream_id;
	}

	return -1;
}

const struct device *get_uaol_zdevice(int uaol_link_id)
{
	/* uaol_link_id is just an index for the device tree device */
	assert(uaol_link_id < ARRAY_SIZE(uaol_devs));
	return uaol_devs[uaol_link_id];
}

/************************************ moved from dai-zephyr.c **********************************/

int dai_get_uaol_stream_id(struct dai *dai, int *uaol_link_id, int *uaol_stream_id)
{
	const struct dai_properties *props;
	k_spinlock_key_t key;

	key = k_spin_lock(&dai->lock);

	props = dai_get_properties(dai->dev, 0, 0);
	*uaol_link_id = props->uaol_link_id;
	*uaol_stream_id = props->uaol_stream_id;

	k_spin_unlock(&dai->lock, key);

	return 0;
}

void process_uaol_feedback(struct comp_dev *dev, struct dai_data *dd)
{
	assert(dd && dd->uaol.fb_chan_idx >= 0 && dd->uaol.fb_dma_buf);
	///assert(dev->direction == SOF_IPC_STREAM_PLAYBACK);

	struct dma_status stat = {0};
	int ret = sof_dma_get_status(dd->dma, dd->uaol.fb_chan_idx, &stat);
	if (ret) {
		comp_err(dev, "Failed to get UAOL feedback DMA status: %d", ret);
		return;
	}

	if (stat.pending_length < 4) {
		/* TODO: That's a normal case, remove this comp_dbg() ??? */
		comp_dbg(dev, "Not enough data in UAOL feedback buffer: %d bytes", stat.pending_length);
		return;
	}

	assert(dd->uaol.fb_dma_buf_size >= 4);
	if (stat.read_position < 0 || stat.read_position > dd->uaol.fb_dma_buf_size - 4) {
		comp_err(dev, "Invalid read position in UAOL feedback buffer: %d", stat.read_position);
		return;
	}

	/* Use uncached pointer to read DMA buffer */
	assert(is_uncached(dd->uaol.fb_dma_buf));

	/* NOTE: Not all DMA drivers populate stat.write_position. Intel ACE HDA does. */
	assert(stat.write_position & 3 == 0);	/* 4-byte alignment check. */
	/* Read the last received 4 bytes (ignore older ones if any). */
	int last_4_bytes_pos = stat.write_position >= 4 ? (stat.write_position - 4) :
		(dd->uaol.fb_dma_buf_size - 4);
	uint32_t feedback_value = dd->uaol.fb_dma_buf[last_4_bytes_pos / 4];

	ret = sof_dma_reload(dd->dma, dd->uaol.fb_chan_idx, stat.pending_length);
	if (ret < 0) {
		comp_err(dev, "Failed to reload UAOL feedback DMA: %d, pending_length: %d",
		 ret, stat.pending_length);
		return;
	}

	comp_dbg(dev, "UAOL feedback value: %u", feedback_value);

	const struct device *uaol_zdev = get_uaol_zdevice(dd->uaol.link_id);
	int freq = uaol_interpret_feedback_value(uaol_zdev, dd->uaol.stream_id, feedback_value);

	if (freq < 0) {
		comp_err(dev, "Bad unexpected feedback freq value: %d", freq);
		return;
	}

	/* Let's limit the maximum drift to a reasonable value to prevent significant audio distortion
	 * when, for some reason, the reported drift is quite big.
	 */
	#define MAX_UAOL_DRIFT_HZ 6

	int drift = freq - dd->ipc_config.sampling_frequency;
	if (drift < -MAX_UAOL_DRIFT_HZ || drift > MAX_UAOL_DRIFT_HZ) {
		comp_warn(dev, "Too much/unreasonable UAOL feedback freq value: %d, drift: %d", freq, drift);
		return;
	}

	dd->uaol.feedback_drift = drift;
	if (dd->uaol.feedback_drift > 0)
		dsrc_set_rate(&dd->uaol.dsrc, dd->ipc_config.sampling_frequency, freq);
}

void adjust_uaol_rate(const struct dai_data *dd, bool increase)
{
	const struct device *uaol_zdev = get_uaol_zdevice(dd->uaol.link_id);
	int ret = uaol_adjust_rate(uaol_zdev, dd->uaol.stream_id, increase);

	if (ret != 0)
		comp_err(dd->dai_dev, "Failed to adjust UAOL rate: %d", ret);
}

int uaol_dma_buffer_copy_to(struct dai_data *dd, size_t bytes)
{
	int ret = 0;

	assert(dd->uaol.feedback_drift != 0);

	if (dd->uaol.feedback_drift > 0) {
		buffer_stream_invalidate(dd->local_buffer, bytes);

		struct cir_buf_ptr in = { dd->local_buffer->stream.addr,
			dd->local_buffer->stream.end_addr, dd->local_buffer->stream.r_ptr };
		assert(dd->uaol.dsrc_buf);
		struct cir_buf_ptr out = { dd->uaol.dsrc_buf->stream.addr,
			dd->uaol.dsrc_buf->stream.end_addr, dd->uaol.dsrc_buf->stream.w_ptr };
		size_t frames = bytes / audio_stream_frame_bytes(&dd->local_buffer->stream);

		size_t added_frames = dsrc_process(&dd->uaol.dsrc, &in, &out, frames);
		size_t added_bytes = added_frames * audio_stream_frame_bytes(&dd->local_buffer->stream);

		comp_update_buffer_consume(dd->local_buffer, bytes);
		audio_stream_produce(&dd->uaol.dsrc_buf->stream, bytes + added_bytes);

		if (added_frames)
			adjust_uaol_rate(dd, true);

		size_t extra_bytes = added_frames * audio_stream_frame_bytes(&dd->dma_buffer->stream);
		ret = dma_buffer_copy_to(dd->uaol.dsrc_buf, dd->dma_buffer,
				 dd->process, bytes + extra_bytes, dd->chmap);
	} else if (dd->uaol.feedback_drift < 0) {
		assert(dd->uaol.feedback_drift <= -1 && dd->uaol.feedback_drift >= -1000);
		dd->uaol.ms_since_last_adjustment++;
		if (-1000 / dd->uaol.feedback_drift >= dd->uaol.ms_since_last_adjustment) {
			dd->uaol.ms_since_last_adjustment = 0;
			adjust_uaol_rate(dd, false);
		}

		ret = dma_buffer_copy_to(dd->local_buffer, dd->dma_buffer,
				 dd->process, bytes, dd->chmap);
	} else {
		return -EINVAL;
	}

	return ret;
}
