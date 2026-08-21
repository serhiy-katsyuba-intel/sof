// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#include <rtos/userspace_helper.h>
#include <sof/audio/module_adapter/iadk/kpb_interface_v2.h>
#include <sof/audio/module_adapter/library/kpb_service.h>
#include <sof/audio/module_adapter/library/native_system_service.h>

namespace intel_adsp
{

struct KpbToken {
	const ModuleHandle *owner;
	void *kpb_client;
};

/** \brief SOF implementation of the KPB service v2 interface. */
class KpbService final : public KpbInterfaceV2 {
public:
	ErrorCode::Type RegisterKpbClient(const ModuleHandle& module_handle, KpbToken& token,
					 size_t current_history_depth, size_t max_history_depth,
					 uint32_t output_pin) const override
	{
		struct kpb_service_client *client;
		AdspErrorCode error = kpb_service_register_client(
								  current_history_depth,
								  max_history_depth,
								  output_pin, &client);

		if (error == ADSP_NO_ERROR) {
			token.owner = &module_handle;
			token.kpb_client = client;
		}

		return error;
	}

	ErrorCode::Type SignalDetection(KpbToken& token, void* data) const override
	{
		if (!token.owner || !token.kpb_client || !data)
			return ADSP_INVALID_PARAMETERS;

		return kpb_service_signal_detection(
			reinterpret_cast<struct kpb_service_client *>(token.kpb_client),
			*static_cast<uint32_t *>(data));
	}

	ErrorCode::Type UnregisterKpbClient(KpbToken& token) const override
	{
		if (!token.kpb_client) {
			token.owner = NULL;
			return ADSP_NO_ERROR;
		}

		AdspErrorCode error = kpb_service_unregister_client(
			reinterpret_cast<struct kpb_service_client *>(token.kpb_client));

		if (error == ADSP_NO_ERROR) {
			token.owner = NULL;
			token.kpb_client = NULL;
		}

		return error;
	}

	ErrorCode::Type RegisterKpbProducer(const ModuleHandle&) const override
	{
		return ADSP_NO_ERROR;
	}
};

static const APP_TASK_DATA KpbService kpb_service;

} /* namespace intel_adsp */

extern "C" struct system_service_iface *native_kpb_service_get_interface(void)
{
	return reinterpret_cast<struct system_service_iface *>(
		const_cast<intel_adsp::KpbService *>(&intel_adsp::kpb_service));
}