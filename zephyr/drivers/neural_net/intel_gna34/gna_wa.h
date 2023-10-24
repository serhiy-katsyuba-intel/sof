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

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_WA_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_WA_H_

/* ---------- Cheat sheet for GNA driver -------------- */

/*
 * 1 - Enables cheat.
 * 0 - Disables cheat.
 * Deleting entry in this file means disabling cheat also.
 */

/*<! Enable to use gna driver in polling mode (without interrupt). */
#if defined(CONFIGFW_ADSP_GNA_POLLING_MODE) && (CONFIGFW_ADSP_GNA_POLLING_MODE == 1)
#define GNA_DRV_WA_POLLING 1
#else
#define GNA_DRV_WA_POLLING 0
#endif

/*
 * Set chicken bit 2 - GNACTL bit 25 - HAS 3.0 v1.24: Narrow-Mem Residue Fix (NMEMRFX)
 *                                   - HAS 3.5: Scratchpad (SCHTPD)
 *  TODO: check on which platform it should be used (CVF FPGA?, CVF HW?, ??)
 *
 *  Fund on CVF, WA works
 *
 *  Change Set:
 *  HSD1809652888 - CNN2D test 06.01.02.0064 causes GNA hang
 */
#if defined(CONFIGFW_ADSP_GNA_NMEMRFX) && (CONFIGFW_ADSP_GNA_NMEMRFX == 1)
#define GNA_DRV_WA_NMEMRFX 1
#else
#define GNA_DRV_WA_NMEMRFX 0
#endif

/*
 * Set chicken bit 1 - GNAOVR bit 16: Scoring Underrun/Overrun Error Check Disable (SUOECD)
 */
#define GNA_DRV_WA_CHICKEN_1 0

/*
 * Setup BARs
 *
 * Program BAR registers before inference - automatic pre-load not working
 *
 * Change Set:
 * HSD1808378926 - [ACE][GNA][LP-FW] Added workarounds & updates
 *                 for gna3.0 to work with adl fpga images >= 19ww33p7.
 */

#if defined(CONFIGFW_ADSP_GNA_LOAD_BARS) && (CONFIGFW_ADSP_GNA_LOAD_BARS == 1)
#define GNA_DRV_WA_LOAD_BARS 1
#else
#define GNA_DRV_WA_LOAD_BARS 0
#endif

/*
 * Set Quite Idle Disable
 *
 * TODO: Check if required - should be set on Embedded HW
 *       Should we clear this bit?
 *
 * Default: do not enable Quite Idle - do not clear this bit
 */
#if defined(CONFIGFW_ADSP_GNA_PMQID) && (CONFIGFW_ADSP_GNA_PMQID == 0)
#define GNA_DRV_WA_PMQID 0
#else
#define GNA_DRV_WA_PMQID 1
#endif

/*
 * OCP-M RD/WR Feature Disable
 * Only for MTL, not implemented on CVF
 *
 * PCR:
 * HSD22011624355
 *
 */
#if defined(CONFIGFW_ADSP_GNA_DIS_ERCO) && (CONFIGFW_ADSP_GNA_DIS_ERCO == 1)
#define GNA_DRV_WA_DIS_ERCO 1
#else
#define GNA_DRV_WA_DIS_ERCO 0
#endif

/*
 * Dynamic-Clock-Gates (DCGs) Enablement
 * CVF and MTL
 *
 * PCR:
 * HSD22011624355
 *
 */

#if defined(CONFIGFW_ADSP_GNA_DCG) && (CONFIGFW_ADSP_GNA_DCG == 1)
#define GNA_DRV_WA_DCG 1
#else
#define GNA_DRV_WA_DCG 0
#endif

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_WA_H_ */
