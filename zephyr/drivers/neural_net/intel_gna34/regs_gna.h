/******************************************************************************
 * INTEL CONFIDENTIAL
 * Copyright 2018 Intel Corporation All Rights Reserved.
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

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_REGS_GNA_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_REGS_GNA_H_

/*! --------------- ML IP registers - GNA --------------- */
/**
 * Status Register.
 */
typedef union _GNASTS_REG {
	uint32_t value;
	struct {
		uint32_t scr_completed: 1;     /* 00:00 ROV    - scoring completed */
		uint32_t susp_bp_match: 1;     /* 01:01 ROV    - suspended breakpoint match */
		uint32_t susp_pause: 1;        /* 02:02 ROV    - suspended due to pause */
		uint32_t comp_stats_valid: 1;  /* 03:03 ROV    - compute statistics valid */
		uint32_t ocp_mmu_err: 1;       /* 04:04 ROV    - OCP error: MMU req */
		uint32_t ocp_dma_err: 1;       /* 05:05 ROV    - OCP error: DMA req */
		uint32_t ocp_ucomp_err: 1;     /* 06:06 ROV    - OCP error: unexpected completion */
		uint32_t va_oor_err: 1;        /* 07:07 ROV    - VA out of range */
		uint32_t hw_par_oor_err: 1;    /* 08:08 ROV    - parameter out of range */
		uint32_t __res_14_9: 6;        /* 09:14 RO     - reserved */
		uint32_t recv_err_response: 1; /* 15:15 RW1C/V - received error response
						*		 on OCP I/F
						*/
		uint32_t hw_out_full: 1;       /* 16:16 RW1C/V - output buffer is currently full */
		uint32_t score_saturated: 1;   /* 17:17 RW1C/V - score has reached the saturation */
		uint32_t __res_29_18: 12;      /* 18:29 RO     - reserved */
		uint32_t aeip: 1;              /* 30:30 ROV    - Autonomous Extension in progress */
		uint32_t intr_status: 1;       /* 31:31 ROV    - interrupt status */
	} bits;
} GNASTS_REG;
BUILD_ASSERT(sizeof(GNASTS_REG) == 4, "Wrong size of GNASTS_REG");

/**
 * Control Register.
 */
typedef union _GNACTL_REG {
	uint32_t value;
	struct {
		uint32_t start_accel: 1;     /* 00:00 RW1S/V - start accelerator */
		uint32_t __res_1: 1;         /* 01:01 RW     - reserved, old active list enable */
		uint32_t abort_clr_accel: 1; /* 02:02 WO     - abort/clear accelerator */
		uint32_t pause_accel: 1;     /* 03:03 RW1S/V - pause execution */
		uint32_t resume_accel: 1;    /* 04:04 RW1S/V - resume execution */
		uint32_t gna_mode: 2;        /* 05:06 RW     - GNA operation mode
					      *		       (0:reserved (old GMM), 1:xNN)
					      */
		uint32_t mmu_bar_preload: 1; /* 07:07 RW     - MMU BARs are preloaded - don't load
					      *                from GNA descriptor
					      */
		uint32_t comp_int_en: 1;     /* 08:08 RW     - completion interrupt enable */
		uint32_t bp_pause_int_en: 1; /* 09:09 RW     - breakpoint pause interrupt enable */
		uint32_t err_int_en: 1;      /* 10:10 RW     - error interrupt enable */
		uint32_t npos_int_en: 1;     /* 11:11 RW     - Non-Posted Response
					      *		       Interrupt Enable
					      */
		uint32_t comp_stats_en: 4;   /* 12:15 RW     - compute statistics enable */
		uint32_t pm_ovr_power_on: 1; /* 16:16 RW     - pwr mgmt override power on */
		uint32_t pm_ovr_clock_on: 1; /* 17:17 RW     - pwr mgmt override force clck on */
		uint32_t pm_quite_idle_dis: 1; /* 18:18 RW   - pwr mgmt quite-idle disable
						*	       NOTE: available on SKL ONLY.
						*/
		uint32_t __res_29_19: 11;      /* 19:29 RW     - reserved, scratchpad */
		uint32_t auto_ext_en: 1;       /* 30:30 RW     - Autonomous Extension Enable */
		uint32_t intr_disable: 1;      /* 31:31 RW     - interrupt disable */
	} bits;
} GNACTL_REG;
BUILD_ASSERT(sizeof(GNACTL_REG) == 4, "Wrong size of GNACTL_REG");

/**
 * Management Control.
 */
typedef union _GNAMGM_REG {
	uint32_t value;
	struct {
		uint32_t max_outs_trans: 8; /* 00:07 RW - max outstanding transaction control,
					     *            0 = infinite
					     */
		uint32_t __res_8: 1;        /* 08:08 RO - Reserved */
		uint32_t rd_cmd_ovr: 1;     /* 09:09 RW - Enable Read Command Overlap (ERCO) */
		uint32_t __res_31_10: 22;   /* 10:31 RO - reserved */
	} bits;
} GNAMGM_REG;
BUILD_ASSERT(sizeof(GNAMGM_REG) == 4, "Wrong size of GNAMGM_REG");

/**
 * Internal State Index.
 */
typedef union _GNAISI_REG {
	uint32_t value;
	struct {
		uint32_t int_state_idx: 11; /* 00:10 RW - index of internal GNA module status */
		uint32_t __res_31_11: 21;   /* 11:31 RO - reserved */
	} bits;
} GNAISI_REG;
BUILD_ASSERT(sizeof(GNAISI_REG) == 4, "Wrong size of GNAISI_REG");

/*
 * Break Point Setup Low
 */
typedef union _GNABPL_REG {
	uint32_t value;

	/* xNN Mode Break point setup */
	struct {
		/* low */
		uint32_t output_num: 16; /* 00:15 RW - output number */
		uint32_t input_num: 8;   /* 16:23 RW - input number */
		uint32_t in_iter_num: 8; /* 24:31 RW - input iteration number */
	} xnn;

} GNABPL_REG;
BUILD_ASSERT(sizeof(GNABPL_REG) == 4, "Wrong size of GNABPL_REG");

/*
 * Break Point Setup High
 */
typedef union _GNABPH_REG {
	uint32_t value;

	/* xNN Mode Break point setup */
	struct {
		/* high */
		uint32_t group_num: 3;    /* 00:02 RW - group number */
		uint32_t layer_num: 13;   /* 03:15 RW - layer number */
		uint32_t __res_18_16: 3;  /* 16:18 RW - reserved must write 0 */
		uint32_t __res_30_19: 12; /* 19:30 RO - (reserved) */
		uint32_t xnn_debug_en: 1; /* 31:31 RW - Break Point Enabled */
	} xnn;
} GNABPH_REG;
BUILD_ASSERT(sizeof(GNABPH_REG) == 4, "Wrong size of GNABPH_REG");

typedef union _GNAOVR_REG {
	uint32_t value;
	struct {
		uint32_t __res_0: 1;             /* 00:00 RO - reserved */
		uint32_t __res_1: 1;             /* 01:01 RO - reserved - old partition
						  *	       clock gating enable
						  */
		uint32_t gm_bb_host_dcgen: 1;    /* 02:02 RW - host interface
						  *	       clock gating enable
						  */
		uint32_t gm_bb_ra_dcgen: 1;      /* 03:03 RW - register access
						  *	       clock gating enable
						  */
		uint32_t gm_bb_dma_dcgen: 1;     /* 04:04 RW - dma engine clock gating enable */
		uint32_t gm_bb_gnac_dcgen: 1;    /* 05:05 RW - GNA core clock gating enable */
		uint32_t gm_bb_inl2ocp_dcgen: 1; /* 06:06 RW - enables ggm_core_clk to be gated */
		uint32_t __res_15_07: 9;         /* 07:15 RO - reserved */
		uint32_t scr_under_over_err_chk_dis: 1; /* 16:16 RW - scoring underrun/overrun
							 *            error check disable
							 */
		uint32_t __res_31_17: 15;               /* 17:31 RO - reserved */
	} bits;
} GNAOVR_REG;
BUILD_ASSERT(sizeof(GNAOVR_REG) == 4, "Wrong size of GNAOVR_REG");

/*!
 * Specifies GNA build register with version and parameters.
 */
typedef union _GNABLD_REG {
	uint32_t value;
	struct {
		uint32_t gna_in_buf_size: 8; /* 00:07 RO   - GNA Input Buffer Size (KB) */
		uint32_t gna_ce_num: 4;      /* 08:11 RO   - GNA CE  numbers */
		uint32_t gna_ple_num: 4;     /* 12:15 RO   - GNA PLE numbers */
		uint32_t gna_afe_num: 4;     /* 16:19 RO   - GNA AFE numbers */
		uint32_t __res_21_20: 2;     /* 20:21 RO   - reserved */
		uint32_t gna_ae_pres: 1;     /* 22:22 RO   - GNA Autonomous Extension present */
		uint32_t gna_mmu_pres: 1;    /* 23:23 RO   - GNA MMU present */
		uint32_t gna_ver_num: 8;     /* 24:31 RO   - GNA version number */
	} bits;
} GNABLD_REG;
BUILD_ASSERT(sizeof(GNABLD_REG) == 4, "Wrong size of GNABLD_REG");

/*! --------------- ML SHIM registers ---------------*/
typedef union _MLCAP {
	uint32_t value;
	struct {
		uint32_t asc: 5;     /* 4:0   - RO - Number of Autonomous Services Supported.*/
		uint32_t rsvd26: 22; /* 26:5  - RO - Reserved. */
		uint32_t osel: 1;    /* 27    - RO - Owner Select (indication). */
		uint32_t rsvd31: 4;  /* 31:28 - RO - Reserved. */
	} bits;
} MLCAP_REG;
BUILD_ASSERT(sizeof(MLCAP_REG) == 4, "Wrong size of MLCAP_REG");

typedef union _MLCTL {
	uint32_t value;
	struct {
		uint32_t spa: 1;     /* 0:0   - RW - Set Power Active. */
		uint32_t rsvd7: 7;   /* 7:1   - RO - Reserved. */
		uint32_t cpa: 1;     /* 8:8   - RO - Current Power Active. */
		uint32_t rsvd23: 15; /* 23: 9 - RO - Reserved. */
		uint32_t osel: 2;    /* 25:24 - RW - Owner Select 0x - host CPU + DSP,
				      *              10- host CPU, 11 - DSP
				      */
		uint32_t fcg: 1;     /* 26    - RW - Force Clock Gating. */
		uint32_t rsvd29: 3;  /* 29:27 - RO - Reserved. */
		uint32_t dcgd: 1;    /* 30    - RW - Dynamic Clock Gating Disable. */
		uint32_t icgd: 1;    /* 31    - RW - Idle Clock Gating Disable. */
	} bits;
} MLCTL_REG;
BUILD_ASSERT(sizeof(MLCTL_REG) == 4, "Wrong size of MLCTL_REG");

typedef union _MLIPPTR {
	uint32_t value;
	struct {
		uint32_t ptr: 21; /* 20:0  - RO - Contains the offset to the IP (MLIP_ADDRESS). */
		uint32_t ver: 3;  /* 23:21 - RO - Indicates the version of the IP. */
		uint32_t rsvd: 8; /* 31:24 - RO - Reserved. */
	} bits;
} MLIPPTR_REG;
BUILD_ASSERT(sizeof(MLIPPTR_REG) == 4, "Wrong size of MLIPPTR_REG");

typedef union _MLxSERCTL_REG {
	uint32_t value;
	struct {
		uint32_t asre: 1;   /* 0:0   - RW - Auto ISR enabled */
		uint32_t idhr: 7;   /* 7:1   - RW - DMA request number for input data */
		uint32_t rsvd8: 7;  /* 14:8  - RO - Reserved */
		uint32_t hde: 1;    /* 15:15 - RW - History buffer DMA enabled */
		uint32_t hdhr: 7;   /* 22:16 - RW - DMA request number for History buffer */
		uint32_t rsvd23: 9; /* 31:23 - RO - Reserved. */
	} bits;
} MLxSERCTL_REG;
BUILD_ASSERT(sizeof(MLxSERCTL_REG) == 4, "Wrong size of MLxSERCTL_REG");

/*!
 * Specifies ML configuration.
 */
typedef struct _MLSHIM_REGS {
	MLCAP_REG cap;     /* 0000 - 0003 (004 B) - RO - Capability register. */
	MLCTL_REG ctl;     /* 0004 - 0007 (004 B) - RW - Control register. */
	MLIPPTR_REG ipptr; /* 0008 - 000B (004 B) - RO - IP Pointer & Version register. */
} MLSHIM_REGS;
BUILD_ASSERT(sizeof(MLSHIM_REGS) == 12, "Wrong size of MLSHIM_REGS");

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_REGS_GNA_H_ */
