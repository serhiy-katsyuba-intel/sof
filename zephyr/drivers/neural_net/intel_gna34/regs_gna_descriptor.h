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
#if CONFIG_INTEL_GNA34_7BAR
#define SIZE_OF_GNA_DESC_MMU_DIS 64
#else
#define SIZE_OF_GNA_DESC_MMU_DIS 32
#endif /* CONFIG_INTEL_GNA34_7BAR */

/* MBAR access control privilege bits (GNA 4.5+ ACC_CTL) */
#define MBAR_READ_PRIVILEGES             1
#define MBAR_WRITE_PRIVILEGES            2
#define MBAR_READ_WRITE_PRIVILEGES       (MBAR_READ_PRIVILEGES | MBAR_WRITE_PRIVILEGES)
#define MBAR_EXECUTE_PRIVILEGES          4

#define MBAR_LDT_AREA_ACC_CTL            (MBAR_READ_PRIVILEGES | MBAR_EXECUTE_PRIVILEGES)
#define MBAR_RO_AREA_ACC_CTL             MBAR_READ_PRIVILEGES
#define MBAR_SCRATCH_ACC_CTL             MBAR_READ_WRITE_PRIVILEGES
#define MBAR_STATE_ACC_CTL               MBAR_READ_WRITE_PRIVILEGES
#define MBAR_INPUT_ACC_CTL               MBAR_READ_WRITE_PRIVILEGES
#define MBAR_OUTPUT_ACC_CTL              MBAR_READ_WRITE_PRIVILEGES

/* MBAR address must be 64B aligned */
#define MBAR_ADDR_MASK                   0xFFFFFFC0
#define MBAR_ACC_CTL_MASK                0x00000007

#if CONFIG_INTEL_GNA34_ACC_CTL
#define MBAR_VALUE_LDT(x)               (((x) & MBAR_ADDR_MASK) | MBAR_LDT_AREA_ACC_CTL)
#define MBAR_VALUE_RO(x)                (((x) & MBAR_ADDR_MASK) | MBAR_RO_AREA_ACC_CTL)
#define MBAR_VALUE_SCRATCH(x)           (((x) & MBAR_ADDR_MASK) | MBAR_SCRATCH_ACC_CTL)
#define MBAR_VALUE_STATE(x)             (((x) & MBAR_ADDR_MASK) | MBAR_STATE_ACC_CTL)
#define MBAR_VALUE_INPUT(x)             (((x) & MBAR_ADDR_MASK) | MBAR_INPUT_ACC_CTL)
#define MBAR_VALUE_OUTPUT(x)            (((x) & MBAR_ADDR_MASK) | MBAR_OUTPUT_ACC_CTL)
#else
#define MBAR_VALUE_LDT(x)               ((x) & MBAR_ADDR_MASK)
#define MBAR_VALUE_RO(x)                ((x) & MBAR_ADDR_MASK)
#define MBAR_VALUE_SCRATCH(x)           ((x) & MBAR_ADDR_MASK)
#define MBAR_VALUE_STATE(x)             ((x) & MBAR_ADDR_MASK)
#define MBAR_VALUE_INPUT(x)             ((x) & MBAR_ADDR_MASK)
#define MBAR_VALUE_OUTPUT(x)            ((x) & MBAR_ADDR_MASK)
#endif /* CONFIG_INTEL_GNA34_ACC_CTL */

/*! GNA HW descriptor used in MMU disabled mode.
 *  MMU is disabled on embedded GNA.
 */
typedef union _GNA_DESCRIPTOR_MMU_DISABLED {
	uint8_t value[SIZE_OF_GNA_DESC_MMU_DIS];
	struct{
		uint32_t    labase;       /* 0000 - 0003 - Offset of Layer Descriptor */
		uint16_t    lacnt;        /* 0004 - 0005 - Number of layers */
		uint8_t     __res_6_7[2]; /* 0006 - 0007 (2B reserved) */
#if !CONFIG_INTEL_GNA34_7BAR
		uint32_t    maxaddr;      /* 0008 - 000B - Max address */
#endif
		uint32_t    bar0;         /* BAR 0 */
		uint32_t    bar1;         /* BAR 1 */
		uint32_t    bar2;         /* BAR 2 */
#if CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR
		uint32_t    bar3;         /* BAR 3 */
		uint32_t    bar4;         /* BAR 4 */
#else /* 4 BARs */
		uint8_t     __res_1f_18[8]; /* (8B reserved) */
#endif  /* CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR */
#if CONFIG_INTEL_GNA34_7BAR
		uint32_t    bar5;         /* BAR 5 - LDT */
		uint32_t    mlmt0;        /* Memory limit for MBAR0 */
		uint32_t    mlmt1;        /* Memory limit for MBAR1 */
		uint32_t    mlmt2;        /* Memory limit for MBAR2 */
		uint32_t    mlmt3;        /* Memory limit for MBAR3 */
		uint32_t    mlmt4;        /* Memory limit for MBAR4 */
		uint32_t    mlmt5;        /* Memory limit for MBAR5 */
		uint32_t    __res_3c_3f[2]; /* reserved */
#endif /* CONFIG_INTEL_GNA34_7BAR */
	} bits;
} GNA_DESC_MMU_DIS;                           /* GNA Base Descriptor */
BUILD_ASSERT(sizeof(GNA_DESC_MMU_DIS) == SIZE_OF_GNA_DESC_MMU_DIS,
	     "Worong size of GNA_DESC_MMU_DIS");

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_REGS_GNA_DESCRIPTOR_H_ */
