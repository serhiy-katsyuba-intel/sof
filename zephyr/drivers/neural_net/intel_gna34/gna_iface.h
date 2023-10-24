/******************************************************************************
 * INTEL CONFIDENTIAL
 * Copyright 2023 Intel Corporation All Rights Reserved.
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

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_IFACE_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_IFACE_H_

#if CONFIG_INTEL_GNA34
#include "regs_gna.h"
EXTERN_C_BEGIN

static void xmp_spin(void)
{
	volatile int a = 0;

	a = a;
}

/*
 * HAL interface of GNA - AON ML
 */

typedef enum {
	GNACOMP_STAT_OFF = 0,
	GNACOMP_STAT_TOTAL_STALL_CYCLES = 1,
	GNACOMP_STAT_WAIT_FOR_DMA = 2,
	GNACOMP_STAT_WAIT_FOR_MMU = 3
} gnacomp_stat_t;

typedef enum {
	GNAMODE_GMM = 0,
	GNAMODE_XNN = 1
} gnamode_t;

typedef enum {
	GNAOSEL_HOSTCPU_DSP = 0x0,
	GNAOSEL_RESERVED = 0x1,
	GNAOSEL_HOSTCPU = 0x2,
	GNAOSEL_DSP = 0x3
} gnaosel_t;

/*
 * GNA HAL API
 */

/*! Get GNA Version */
static inline uint32_t adsphal_gna_get_version(uint32_t base_addr)
{
	volatile GNABLD_REG bld_reg = *(REGISTER(GNA_GNABLD)(base_addr));
	return bld_reg.bits.gna_ver_num;
}

/*! Get GNA Internal buffer size (KB) */
static inline uint32_t adsphal_gna_get_internal_buffer_size(uint32_t base_addr)
{
	volatile GNABLD_REG bld_reg = *(REGISTER(GNA_GNABLD)(base_addr));
	return bld_reg.bits.gna_in_buf_size;
}

/*! Get GNA CE number */
static inline uint32_t adsphal_gna_get_ce_num(uint32_t base_addr)
{
	volatile GNABLD_REG bld_reg = *(REGISTER(GNA_GNABLD)(base_addr));
	return bld_reg.bits.gna_ce_num;
}

/*! Get GNA PLE number */
static inline uint32_t adsphal_gna_get_ple_num(uint32_t base_addr)
{
	volatile GNABLD_REG bld_reg = *(REGISTER(GNA_GNABLD)(base_addr));
	return bld_reg.bits.gna_ple_num;
}

/*! Get GNA AFE number */
static inline uint32_t adsphal_gna_get_afe_num(uint32_t base_addr)
{
	volatile GNABLD_REG bld_reg = *(REGISTER(GNA_GNABLD)(base_addr));
	return bld_reg.bits.gna_afe_num;
}

/*! Get AE Support (ANNA) */
static inline bool adsphal_gna_get_ae_support(uint32_t base_addr)
{
	volatile GNABLD_REG bld_reg = *(REGISTER(GNA_GNABLD)(base_addr));
	return bld_reg.bits.gna_ae_pres;
}

/*! Get MMU present */
static inline bool adsphal_gna_get_mmu_present(uint32_t base_addr)
{
	volatile GNABLD_REG bld_reg = *(REGISTER(GNA_GNABLD)(base_addr));
	return bld_reg.bits.gna_mmu_pres;
}

/*! Sets Descriptor Base Address. */
static inline void adsphal_gna_set_desc_base(uint32_t base_addr, uint32_t addr)
{
	*(REGISTER(GNA_GNADSCBAR)(base_addr)) = addr;
}

/*! Sets MBAR0. */
static inline void adsphal_gna_set_mbar0(uint32_t base_addr, uint32_t value)
{
	*(REGISTER(GNA_GNABAR0)(base_addr)) = value;
}

/*! Sets MBAR1. */
static inline void adsphal_gna_set_mbar1(uint32_t base_addr, uint32_t value)
{
	*(REGISTER(GNA_GNABAR1)(base_addr)) = value;
}

/*! Sets MBAR2. */
static inline void adsphal_gna_set_mbar2(uint32_t base_addr, uint32_t value)
{
	*(REGISTER(GNA_GNABAR2)(base_addr)) = value;
}

#if CONFIG_INTEL_GNA34_6BAR
/*! Sets MBAR3. */
static inline void adsphal_gna_set_mbar3(uint32_t base_addr, uint32_t value)
{
	*(REGISTER(GNA_GNABAR3)(base_addr)) = value;
}

/*! Sets MBAR4. */
static inline void adsphal_gna_set_mbar4(uint32_t base_addr, uint32_t value)
{
	*(REGISTER(GNA_GNABAR4)(base_addr)) = value;
}
#endif /* CONFIG_INTEL_GNA34_6BAR */

/*! Turns ON GNA QuiteIdle feature */
static inline void adsphal_gna_set_quiteidle_on(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.pm_quite_idle_dis = 0;
}

/*! Turns OFF GNA QuiteIdle feature */
static inline void adsphal_gna_set_quiteidle_off(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.pm_quite_idle_dis = 1;
}

/*! Turns ON global gna interrupts. This permits for interrupts. */
static inline void adsphal_gna_set_interrupts_on(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.intr_disable = 0;
}

/*! Turns OFF global gna interrupts. This prohibits interrupts. */
static inline void adsphal_gna_set_interrupts_off(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.intr_disable = 1;
}

/*!
 * Turns ON interrupt for scoring completion.
 * \attention Global interrupts shall be on for this interrupt to take effect.
 * \see adsphal_gna_set_interrupts_on()
 */
static inline void adsphal_gna_set_completion_int_on(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.comp_int_en = 1;
}

/* ! Turns OFF interrupt for scoring completion. */
static inline void adsphal_gna_set_completion_int_off(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.comp_int_en = 0;
}

/*!
 * Turns ON interrupt for execution stopped due to an error.
 * \attention Global interrupts shall be on for this interrupt to take effect.
 * \see adsphal_gna_set_interrupts_on()
 */
static inline void adsphal_gna_set_error_int_on(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.err_int_en = 1;
}

/*! Turns OFF interrupt for execution stopped due to an error. */
static inline void adsphal_gna_set_error_int_off(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.err_int_en = 0;
}

/*! Sets GNA mode. */
static inline void adsphal_gna_set_gnamode(uint32_t base_addr, gnamode_t mode)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.gna_mode =
		GNAMODE_XNN; /* only XNN is supported on 3.x */
}

/*! Starts GNA accelerator. */
static inline void adsphal_gna_start_acceleration(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.start_accel = 1;
}

/*! Stops (& clears) GNA accelerator. */
static inline void adsphal_gna_abort_clear_acceleration(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.abort_clr_accel = 1;
}

/*! Pause GNA accelerator. */
static inline void adsphal_gna_pause_acceleration(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.pause_accel = 1;
}

/*! Resume GNA accelerator from pause state. */
static inline void adsphal_gna_resume_acceleration(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.resume_accel = 1;
}

/*
 *! Set GNA BAR fw preload on.
 *  \BAR registers must be programmed by fw before gna execution.
 */
static inline void adsphal_gna_bar_fw_preload_on(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.mmu_bar_preload = 1;
}

/*
 *! Set GNA BAR fw preload off.
 *  \BAR registers are loaded from GNA descriptor by gna hw at start of execution.
 */
static inline void adsphal_gna_bar_fw_preload_off(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.mmu_bar_preload = 0;
}

/*! Enables compute statistics with a given statistics mode. */
static inline void adsphal_gna_enable_compute_stats(uint32_t base_addr, gnacomp_stat_t mode)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.comp_stats_en = mode;
}

/*! Get current execution status. */
static inline uint32_t adsphal_gna_get_exec_active(uint32_t base_addr)
{
	return (REGISTER(GNA_GNACTL)(base_addr))->bits.start_accel;
}

/*!< Set Power Management Force Power On */
static inline void adsphal_gna_set_pm_force_power_on(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.pm_ovr_power_on = 1;
	while ((REGISTER(GNA_GNACTL)(base_addr))->bits.pm_ovr_power_on != 1) {
		xmp_spin();
	}
}

/*!< Clear Power Management Force Power On */
static inline void adsphal_gna_clr_pm_force_power_on(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.pm_ovr_power_on = 0;
	while ((REGISTER(GNA_GNACTL)(base_addr))->bits.pm_ovr_power_on != 0) {
		xmp_spin();
	}
}

/*!< Set Power Management Force Clock On */
static inline void adsphal_gna_set_pm_force_clock_on(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.pm_ovr_clock_on = 1;
	while ((REGISTER(GNA_GNACTL)(base_addr))->bits.pm_ovr_clock_on != 1) {
		xmp_spin();
	}
}

/*!< Clear Power Management Force Clock On */
static inline void adsphal_gna_clr_pm_force_clock_on(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->bits.pm_ovr_clock_on = 0;
	while ((REGISTER(GNA_GNACTL)(base_addr))->bits.pm_ovr_clock_on != 0) {
		xmp_spin();
	}
}

/* Get AEIP - AE In Progress */
static inline bool adsphal_gna_get_aeip(uint32_t base_addr)
{
	return (REGISTER(GNA_GNASTS)(base_addr))->bits.aeip;
}

/*! Gets GNA status register value. */
static inline uint32_t adsphal_gna_get_gna_status(uint32_t base_addr)
{
	return (REGISTER(GNA_GNASTS)(base_addr))->value;
}

/*!
 * Checks GNA status, based on provided status register dump.
 * \returns Error Code.
 * \retval ADSP_SUCCESS If there were no errors indicated in status register dump.
 * \retval ADSP_GNA_ERROR If there were errors indicated in status register dump.
 */
static inline ErrorCode adsphal_gna_check_status(const uint32_t *stsreg_dump)
{
	ErrorCode ec;
	GNASTS_REG *sts = (GNASTS_REG *)stsreg_dump;

	if (1 == sts->bits.ocp_mmu_err || 1 == sts->bits.ocp_dma_err ||
	    1 == sts->bits.ocp_ucomp_err || 1 == sts->bits.va_oor_err ||
	    1 == sts->bits.hw_par_oor_err || 1 == sts->bits.recv_err_response) {
		ec = ADSP_GNA_ERROR;
	} else {
		ec = ADSP_SUCCESS;
	}

	return ec;
}

/*!
 * Gets GNA Performance Total Cycles (PTC) register value.
 * This indicates the number of cycles that the GNA was in "Score in Progress" state.
 */
static inline uint32_t adsphal_gna_get_perf_total_cycles(uint32_t base_addr)
{
	return *(REGISTER(GNA_GNAPTC)(base_addr));
}

/*!
 * Gets GNA Performance Stall Cycles (PSC) register value.
 * This indicates the number of stall cycles that the GNA had since last "Score in Progress" state.
 */
static inline uint32_t adsphal_gna_get_perf_stall_cycles(uint32_t base_addr)
{
	return *(REGISTER(GNA_GNAPSC)(base_addr));
}

static inline void adsphal_gna_clear_device(uint32_t base_addr)
{
	(REGISTER(GNA_GNASTS)(base_addr))->bits.recv_err_response = 0;
	(REGISTER(GNA_GNASTS)(base_addr))->bits.score_saturated = 0;
	(REGISTER(GNA_GNASTS)(base_addr))->bits.hw_out_full = 0;

	adsphal_gna_abort_clear_acceleration(base_addr);
}

/*! Gets GNA scoring status (0 - scoring not completed, 1 - scoring completed). */
static inline bool adsphal_gna_get_acceleration_status(uint32_t base_addr)
{
	return (REGISTER(GNA_GNASTS)(base_addr))->bits.scr_completed;
}

/*! Gets GNA interrupt status (0 - no interrupt pending, 1 - interrupt pending). */
static inline bool adsphal_gna_get_interrupt_status(uint32_t base_addr)
{
	return (REGISTER(GNA_GNASTS)(base_addr))->bits.intr_status;
}

/*! Gets max outstanding transactions */
static inline uint32_t
adsphal_gna_get_max_outs_trans(uint32_t base_addr)
{
	return (REGISTER(GNA_GNAMGM)(base_addr))->bits.max_outs_trans;
}

/*! Set max outstanding transactions
 *  For GNA versions >= 3.5 0 = infinite
 */
static inline void adsphal_gna_set_max_outs_trans(uint32_t base_addr, uint32_t max_ot)
{
	(REGISTER(GNA_GNAMGM)(base_addr))->bits.max_outs_trans = max_ot;
}

/*! Get Enable Read Command Overlap (ERCO) */
static inline bool adsphal_gna_get_erco(uint32_t base_addr)
{
	return (bool)((REGISTER(GNA_GNAMGM)(base_addr))->bits.rd_cmd_ovr);
}

/*! Set Enable Read Command Overlap (ERCO) */
static inline void adsphal_gna_set_erco(uint32_t base_addr)
{
	(REGISTER(GNA_GNAMGM)(base_addr))->bits.rd_cmd_ovr = 1;
}

/*! Clear Enable Read Command Overlap (ERCO) */
static inline void adsphal_gna_clr_erco(uint32_t base_addr)
{
	(REGISTER(GNA_GNAMGM)(base_addr))->bits.rd_cmd_ovr = 0;
}

/* Sets max outstanding transactions (MAXOUTS) */
static inline void adsphal_gna_set_maxouts(uint32_t base_addr, uint8_t maxouts)
{
	(REGISTER(GNA_GNAMGM)(base_addr))->bits.max_outs_trans = maxouts;
}

/*! Get max outstanding transactions (MAXOUTS) */
static inline uint8_t adsphal_gna_get_maxouts(uint32_t base_addr)
{
	return (uint8_t)((REGISTER(GNA_GNAMGM)(base_addr))->bits.max_outs_trans);
}

/*! Gets max outstanding transactions */
static inline uint32_t
adsphal_gna_get_ovr_val(uint32_t base_addr)
{
	return (REGISTER(GNA_GNAOVR)(base_addr))->value;
}

/*! Set max outstanding transactions */
static inline void adsphal_gna_set_ovr_val(uint32_t base_addr, uint32_t val)
{
	(REGISTER(GNA_GNAOVR)(base_addr))->value = val;
}

/*! Turns ON chicken bit no. 1. */
static inline void adsphal_gna_set_chicken_no1(uint32_t base_addr)
{
	(REGISTER(GNA_GNAOVR)(base_addr))->bits.scr_under_over_err_chk_dis = 1;
	while ((REGISTER(GNA_GNAOVR)(base_addr))->bits.scr_under_over_err_chk_dis != 1)
		;
}

/*! Clears chicken bit no. 1. */
static inline void adsphal_gna_clear_chicken_no1(uint32_t base_addr)
{
	(REGISTER(GNA_GNAOVR)(base_addr))->bits.scr_under_over_err_chk_dis = 0;
	while ((REGISTER(GNA_GNAOVR)(base_addr))->bits.scr_under_over_err_chk_dis != 0) {
	};
}

/**
 * Turns ON chicken bit no. 2.
 * This bit is essential for CNN2D if they causes GNA hang.
 * Narrow-Mem Residue Fix (NMEMRFX)
 */
static inline void adsphal_gna_set_nmemrfx(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->value |= BIT(25);
	while (!(((REGISTER(GNA_GNACTL)(base_addr))->value) & BIT(25)))
		;
}

#define adsphal_gna_set_chicken_no2 adsphal_gna_set_nmemrfx

/** Clears chicken bit no. 2. */
static inline void adsphal_gna_clear_nmemrfx(uint32_t base_addr)
{
	(REGISTER(GNA_GNACTL)(base_addr))->value &= ~BIT(25);
	while (((REGISTER(GNA_GNACTL)(base_addr))->value) & BIT(25))
		;
}

#define adsphal_gna_clear_chicken_no2 adsphal_gna_clear_nmemrfx

/* HAL for accessing SHIM registers
 *!< Sets gna ownership.
 */
static inline void adsphal_ml_set_ownership(uint32_t base_addr, gnaosel_t osel)
{
	MLCTL_REG regs;

	regs = *(REGISTER(MLCTL)(base_addr));
	regs.bits.osel = osel;
	*(REGISTER(MLCTL)(base_addr)) = regs;
}

/*
 * Is autonomous extension present in ML.
 * ML power needs to be ON to perform this function.
 */
static inline uint32_t adsphal_gna_is_ae_present(uint32_t base_addr)
{
	return (REGISTER(GNA_GNABLD)(base_addr))->bits.gna_ae_pres;
}

/* Get number of autonomous services supported. */
static inline uint32_t adsphal_ml_get_asc(uint32_t base_addr)
{
	return 0;
}

/*!< Sets AON power ON. Blocks until power state reaches ON state. */
static inline void adsphal_ml_set_power_on(uint32_t base_addr)
{
	MLCTL_REG regs;

	regs = *(REGISTER(MLCTL)(base_addr));
	regs.bits.spa = 1;
	*(REGISTER(MLCTL)(base_addr)) = regs;

	while (regs.bits.cpa == 0) {
		xmp_spin();
		regs = *(REGISTER(MLCTL)(base_addr));
	}
}

/*!< Sets AON power OFF. Blocks until power state reaches OFF state. */
static inline void adsphal_ml_set_power_off(uint32_t base_addr)
{
	(REGISTER(MLCTL)(base_addr))->bits.spa = 0;
	while ((REGISTER(MLCTL)(base_addr))->bits.cpa == 1) {
		xmp_spin();
	}
}

static inline void adsphal_ml_set_icgd(uint32_t base_addr, int icgd)
{
	MLCTL_REG regs;

	regs = *(REGISTER(MLCTL)(base_addr));
	regs.bits.icgd = (icgd) ? 1 : 0;
	*(REGISTER(MLCTL)(base_addr)) = regs;
}

static inline void adsphal_ml_set_dcgd(uint32_t base_addr, int dcgd)
{
	MLCTL_REG regs;

	regs = *(REGISTER(MLCTL)(base_addr));
	regs.bits.dcgd = (dcgd) ? 1 : 0;
	*(REGISTER(MLCTL)(base_addr)) = regs;
}

EXTERN_C_END

#endif /* CONFIG_INTEL_GNA34 */
#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_IFACE_H_ */
