/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_LIB_GNA2_DEV_VERSIONS_H__
#define __SOF_LIB_GNA2_DEV_VERSIONS_H__

#include <stdint.h>
#include <rtos/panic.h>

typedef enum Gna2DeviceVersion {
	/**
	 * Gaussian Mixture Models device.
	 * A ::Gna2DeviceGenerationGmm generation device.
	 */
	Gna2DeviceVersionGMM = 0x01,
	/**
	 * GNA 0.9 device.
	 * A ::Gna2DeviceGeneration0_9 generation device.
	 */
	Gna2DeviceVersion0_9 = 0x09,
	/**
	 * GNA 1.0 device.
	 * A ::Gna2DeviceGeneration1_0 generation device.
	 */
	Gna2DeviceVersion1_0 = 0x10,
	/**
	 * GNA 2.0 device.
	 * A ::Gna2DeviceGeneration2_0 generation device.
	 */
	Gna2DeviceVersion2_0 = 0x20,
	/**
	 * GNA 3.0 device.
	 * A ::Gna2DeviceGeneration3_0 generation device.
	 */
	Gna2DeviceVersion3_0 = 0x30,
	/**
	 * GNA 3.5 device.
	 * A ::Gna2DeviceGeneration3_5 generation device.
	 */
	Gna2DeviceVersion3_5 = 0x35,
	/**
	 * GNA 3.6 device.
	 * A ::Gna2DeviceGeneration3_6 generation device.
	 */
	Gna2DeviceVersion3_6 = 0x36,
	/**
	 * GNA 1.0 embedded device.
	 * A ::Gna2DeviceGeneration1_0 generation device.
	 */
	Gna2DeviceVersionEmbedded1_0 = 0x10E,
	/**
	 * GNA 3.1 embedded device on PCH/ACE.
	 * A ::Gna2DeviceGeneration3_1 generation device.
	 */
	Gna2DeviceVersionEmbedded3_1 = 0x310E,
	/**
	 * GNA 3.5 embedded device on ACE.
	 * A ::Gna2DeviceGeneration3_5 generation device.
	 */
	Gna2DeviceVersionEmbedded3_5 = 0x35E,
	/**
	 * GNA 3.5 embedded device on ACE with Autonomous Extension.
	 * A ::Gna2DeviceGeneration3_5 generation device.
	 */
	Gna2DeviceVersionEmbeddedAE3_5 = 0x35A,
	/**
	 * GNA 3.6 embedded device on ACE.
	 * A ::Gna2DeviceGeneration3_6 generation device.
	 */
	Gna2DeviceVersionEmbedded3_6 = 0x36E,
	/**
	 * GNA 4.0 Lark (2CE) embedded device on ACE.
	 * A ::Gna2DeviceGeneration4_0 generation device.
	 */
	Gna2DeviceVersionEmbedded4_0 = 0x40E,
	/**
	 * GNA 4.0 Octopus (8CE) embedded device on ACE.
	 * A ::Gna2DeviceVersionEmbedded4_0_CE8 generation device.
	 */
	Gna2DeviceVersionEmbedded4_0_CE8 = 0x40E8,
	/**
	 * Value indicating no supported hardware device available.
	 * Software emulation (fall-back) will be used.
	 * @see Gna2RequestConfigEnableHardwareConsistency().
	 */
	Gna2DeviceVersionSoftwareEmulation = 0x00,
} gna2_device_version_t;

#endif /* __SOF_LIB_GNA2_DEV_VERSIONS_H__ */
