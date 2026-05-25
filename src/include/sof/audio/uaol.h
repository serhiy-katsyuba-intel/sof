/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_UAOL_H__
#define __SOF_AUDIO_UAOL_H__

//#include <stddef.h>
#include <stdint.h>
#include <sof/lib/memory.h>
//#include <sof/platform.h>

struct sof_tlv;
struct device;

#if !CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY
__cold void tlv_value_set_uaol_caps(struct sof_tlv *tuple, uint32_t type);
#endif /* CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY */

__cold int uaol_stream_id_to_hda_link_stream_id(int uaol_stream_id);

const struct device *get_uaol_zdevice(int uaol_link_id);

/************************************ moved from dai-zephyr.c **********************************/

struct dai;

int dai_get_uaol_stream_id(struct dai *dai, int *uaol_link_id, int *uaol_stream_id);

#endif /* __SOF_AUDIO_UAOL_H__ */
