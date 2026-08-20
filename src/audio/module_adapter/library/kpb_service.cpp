// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#include <rtos/userspace_helper.h>
#include <sof/audio/module_adapter/iadk/kpb_interface_v2.h>
#include <sof/audio/module_adapter/library/native_system_service.h>

namespace intel_adsp
{

/** \brief Placeholder implementation of the KPB service v2 interface. */
class KpbService final : public KpbInterfaceV2 {
public:
	ErrorCode::Type RegisterKpbClient(const ModuleHandle&, KpbToken&, size_t, size_t,
					 uint32_t) const override
	{
		return ADSP_SERVICE_UNAVAILABLE;
	}

	ErrorCode::Type SignalDetection(KpbToken&, void*) const override
	{
		return ADSP_SERVICE_UNAVAILABLE;
	}

	ErrorCode::Type UnregisterKpbClient(KpbToken&) const override
	{
		return ADSP_SERVICE_UNAVAILABLE;
	}

	ErrorCode::Type RegisterKpbProducer(const ModuleHandle&) const override
	{
		return ADSP_SERVICE_UNAVAILABLE;
	}
};

static const APP_TASK_DATA KpbService kpb_service;

} /* namespace intel_adsp */

extern "C" struct system_service_iface *native_kpb_service_get_interface(void)
{
	return reinterpret_cast<struct system_service_iface *>(
		const_cast<intel_adsp::KpbService *>(&intel_adsp::kpb_service));
}