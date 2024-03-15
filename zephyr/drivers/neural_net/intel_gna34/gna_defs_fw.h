/******************************************************************************
 * INTEL CONFIDENTIAL
 * Copyright 2023 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to the
 * source code ("Material") are owned by Intel Corporation or its suppliers or
 * licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors.
 * The Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery of
 * the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing.
 ******************************************************************************/

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_DEFS_FW_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_DEFS_FW_H_

#ifdef __cplusplus

#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END   }

#else

#define EXTERN_C_BEGIN
#define EXTERN_C_END

#endif /* __cplusplus */

#define ADDRESS(feature)  CONFIGFW_ADSP_##feature##_ADDRESS
#define REGISTER(feature) CONFIGFW_ADSP_##feature##_REGISTER
#define VERSION(feature)  CONFIGFW_ADSP_##feature##_VERSION

/*
 * GNA/ANNA VERSIONS
 */
#define CONFIGFW_ADSP_GNA_3_0_VERSION 30
#define CONFIGFW_ADSP_GNA_3_1_VERSION 31
#define CONFIGFW_ADSP_GNA_3_5_VERSION 35
#define CONFIGFW_ADSP_GNA_3_6_VERSION 36
#define CONFIGFW_ADSP_GNA_4_0_VERSION 40

/*
 * GNA HW DEFS
 */
#define CONFIGFW_ADSP_GNA_PRESENT       1
#define CONFIGFW_ADSP_GNA_CTRL_QUANTITY 1
#ifdef CONFIG_ACE_VERSION_1_5
#define CONFIGFW_ADSP_GNA_VERSION       CONFIGFW_ADSP_GNA_3_5_VERSION
#elif CONFIG_ACE_VERSION_2_0
#define CONFIGFW_ADSP_GNA_VERSION       CONFIGFW_ADSP_GNA_3_6_VERSION
#elif CONFIG_ACE_VERSION_3_0
#define CONFIGFW_ADSP_GNA_VERSION       CONFIGFW_ADSP_GNA_4_0_VERSION
#else
#error "Unsupported GNA version"
#endif
#define CONFIGFW_ADSP_GNA_6BAR_SUPPORT  0
#define CONFIGFW_ADSP_GNA_INSTANCE      0

/*
 * GNA FW DEFS
 */
#define CONFIGFW_ADSP_GNA_SUPPORT         CONFIGFW_ADSP_GNA_PRESENT
#define CONFIGFW_ADSP_GNA_HW_STAT_SUPPORT 1
#define CONFIGFW_ADSP_GNA_POLLING_MODE    0
#define CONFIGFW_ADSP_GNA_NMEMRFX         0
#define CONFIGFW_ADSP_GNA_LOAD_BARS       0
#define CONFIGFW_ADSP_GNA_PMQID           1
#define CONFIGFW_ADSP_GNA_DIS_ERCO        1
#define CONFIGFW_ADSP_GNA_DCG             1

#define ADSP_SUCCESS                       0
#define ADSP_ERROR_NULL_POINTER_AS_PARAM   1
#define ADSP_GNA_ERROR                     2
#define ADSP_GNA_DRV_FREELIST_ERROR        3
#define ADSP_ERROR_INVALID_PARAM           4
#define ADSP_GNA_DEV_NOT_INITIALIZED       5
#define ADSP_GNA_HW_NOT_COMPATIBLE         6
#define ADSP_GNA_MODEL_EXISTS              7
#define ADSP_GNA_MODEL_INVALID_LAYER_COUNT 8
#define ADSP_GNA_MODEL_NOT_FOUND           9
#define ADSP_GNA_REQUEST_NOT_EXISTS_ERROR  10
#define ADSP_GNA_DRV_QUEUE_ERROR           11
#define ADSP_GNA_REQUEST_EXISTS_ERROR      12

/* Registers for MTL/LNL/PTL platform
 * GNA IP registers, for controlling HW based DSP accelerator.
 */
#define CONFIGFW_ADSP_GNA_GNASTS_ADDRESS(addr)       (addr + 0x0000)
#define CONFIGFW_ADSP_GNA_GNACTL_ADDRESS(addr)       (addr + 0x0004)
#define CONFIGFW_ADSP_GNA_GNAMGM_ADDRESS(addr)       (addr + 0x0008)
#define CONFIGFW_ADSP_GNA_GNAPTC_ADDRESS(addr)       (addr + 0x000C)
#define CONFIGFW_ADSP_GNA_GNAPSC_ADDRESS(addr)       (addr + 0x0010)
#define CONFIGFW_ADSP_GNA_GNAISI_ADDRESS(addr)       (addr + 0x0014)
#define CONFIGFW_ADSP_GNA_GNAISVL_ADDRESS(addr)      (addr + 0x0018)
#define CONFIGFW_ADSP_GNA_GNAISVH_ADDRESS(addr)      (addr + 0x001C)
#define CONFIGFW_ADSP_GNA_GNABPL_ADDRESS(addr)       (addr + 0x0020)
#define CONFIGFW_ADSP_GNA_GNABPH_ADDRESS(addr)       (addr + 0x0024)
#define CONFIGFW_ADSP_GNA_GNARESERVED1_ADDRESS(addr) (addr + 0x0028)
#define CONFIGFW_ADSP_GNA_GNADSCBAR_ADDRESS(addr)    (addr + 0x0030)
#define CONFIGFW_ADSP_GNA_GNABLD_ADDRESS(addr)       (addr + 0x0034)
#define CONFIGFW_ADSP_GNA_GNARESERVED2_ADDRESS(addr) (addr + 0x0038)
#define CONFIGFW_ADSP_GNA_GNAOVR_ADDRESS(addr)       (addr + 0x0040)
#define CONFIGFW_ADSP_GNA_GNABAR0_ADDRESS(addr)      (addr + 0x0044)
#define CONFIGFW_ADSP_GNA_GNABAR1_ADDRESS(addr)      (addr + 0x0048)
#define CONFIGFW_ADSP_GNA_GNABAR2_ADDRESS(addr)      (addr + 0x004C)
#define CONFIGFW_ADSP_GNA_GNABAR3_ADDRESS(addr)      (addr + 0x0050)
#define CONFIGFW_ADSP_GNA_GNABAR4_ADDRESS(addr)      (addr + 0x0054)

#define CONFIGFW_ADSP_GNA_GNASTS_REGISTER(addr) \
	((volatile GNASTS_REG *)(CONFIGFW_ADSP_GNA_GNASTS_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNACTL_REGISTER(addr) \
	((volatile GNACTL_REG *)(CONFIGFW_ADSP_GNA_GNACTL_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNAMGM_REGISTER(addr) \
	((volatile GNAMGM_REG *)(CONFIGFW_ADSP_GNA_GNAMGM_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNAPTC_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNAPTC_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNAPSC_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNAPSC_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNAISI_REGISTER(addr) \
	((volatile GNAISI_REG *)(CONFIGFW_ADSP_GNA_GNAISI_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNAISVL_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNAISVL_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNAISVH_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNAISVH_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNABPL_REGISTER(addr) \
	((volatile GNABPL_REG *)(CONFIGFW_ADSP_GNA_GNABPL_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNABPH_REGISTER(addr) \
	((volatile GNABPH_REG *)(CONFIGFW_ADSP_GNA_GNABPH_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNADSCBAR_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNADSCBAR_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNABLD_REGISTER(addr) \
	((volatile GNABLD_REG *)(CONFIGFW_ADSP_GNA_GNABLD_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNAOVR_REGISTER(addr) \
	((volatile GNAOVR_REG *)(CONFIGFW_ADSP_GNA_GNAOVR_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNABAR0_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNABAR0_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNABAR1_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNABAR1_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNABAR2_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNABAR2_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNABAR3_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNABAR3_ADDRESS(addr)))
#define CONFIGFW_ADSP_GNA_GNABAR4_REGISTER(addr) \
	((volatile uint32_t *)(CONFIGFW_ADSP_GNA_GNABAR4_ADDRESS(addr)))

/*
 * ML SHIM registers, for general operation control (reset, power & clock gating, etc.).
 */
#define CONFIGFW_ADSP_MLSHIM_ADDRESS(addr)  ((addr) + 0x800)
#define CONFIGFW_ADSP_MLCAP_ADDRESS(addr)   (CONFIGFW_ADSP_MLSHIM_ADDRESS(addr) + 0x0000)
#define CONFIGFW_ADSP_MLCTL_ADDRESS(addr)   (CONFIGFW_ADSP_MLSHIM_ADDRESS(addr) + 0x0004)
#define CONFIGFW_ADSP_MLIPPTR_ADDRESS(addr) (CONFIGFW_ADSP_MLSHIM_ADDRESS(addr) + 0x0008)
#define CONFIGFW_ADSP_MLxSERCTL_ADDRESS(x)  (CONFIGFW_ADSP_MLSHIM_ADDRESS + 0x200 + 4 * x)

#define CONFIGFW_ADSP_MLCAP_REGISTER(addr) \
	((volatile MLCAP_REG *)(CONFIGFW_ADSP_MLCAP_ADDRESS(addr)))
#define CONFIGFW_ADSP_MLCTL_REGISTER(addr) \
	((volatile MLCTL_REG *)(CONFIGFW_ADSP_MLCTL_ADDRESS(addr)))
#define CONFIGFW_ADSP_MLIPPTR_REGISTER(addr) \
	((volatile MLIPPTR_REG *)(CONFIGFW_ADSP_MLIPPTR_ADDRESS(addr)))
#define CONFIGFW_ADSP_MLxSERCTL_REGISTER(i) \
	((volatile MLxSERCTL_REG *)(CONFIGFW_ADSP_MLxSERCTL_ADDRESS(i)))

/* this macro returns given def_ec when given ec != ADSP_SUCCESS. */
#define RETURN_EC_ON_ERROR(ec, def_ec) \
	if ((ec) != ADSP_SUCCESS) {\
		return def_ec;\
	}

/* this macro returns given ret_val when given boolean != true */
#define RETURN_EC_ON_FAIL(boolean, ret_val) \
	if (!(boolean)) {\
		return ret_val;\
	}

#define RETURN_ON_ERROR(ec) \
	if ((ec) != ADSP_SUCCESS) {\
		return ec;\
	}

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_DEFS_FW_H_ */
