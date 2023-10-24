/******************************************************************************
 * INTEL CONFIDENTIAL
 * Copyright 2016 - 2017 Intel Corporation All Rights Reserved.
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

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_REGS_GNA_DESCRIPTOR_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_REGS_GNA_DESCRIPTOR_H_

/*
 * Size (in bytes) of GNA descriptor in MMU disabled mode.
 */
#define SIZE_OF_GNA_DESC_MMU_DIS 32

/*! GNA HW descriptor used in MMU disabled mode.
 *  MMU is disabled on embedded GNA.
 */
typedef union _GNA_DESCRIPTOR_MMU_DISABLED {
	uint8_t value[SIZE_OF_GNA_DESC_MMU_DIS];
	struct{
		uint32_t    labase;       /* 0000 - 0003 - Offset of Layer Descriptor */
		uint16_t    lacnt;        /* 0004 - 0005 - Number of layers */
		uint8_t     __res_6_7[2]; /* 0006 - 0007 (2B reserved) */
		uint32_t    maxaddr;      /* 0008 - 000B - Max address - hard to use without MMU */
		uint32_t    bar0;         /* 000C - 000f - BAR 0 */
		uint32_t    bar1;         /* 0010 - 0013 - BAR 1 */
		uint32_t    bar2;         /* 0014 - 0017 - BAR 2 */
#if CONFIG_INTEL_GNA34_6BAR
		uint32_t    bar3;         /* 0018 - 001B - BAR 3 */
		uint32_t    bar4;         /* 001C - 001F - BAR 4 */
#else /* 4 BARs */
		uint8_t     __res_1f_18[8]; /* 0018 - 001F (8B reserved) */
#endif  /* CONFIG_INTEL_GNA34_6BAR */
	} bits;
} GNA_DESC_MMU_DIS;                           /* GNA Base Descriptor */
BUILD_ASSERT(sizeof(GNA_DESC_MMU_DIS) == SIZE_OF_GNA_DESC_MMU_DIS,
	     "Worong size of GNA_DESC_MMU_DIS");

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_REGS_GNA_DESCRIPTOR_H_ */
