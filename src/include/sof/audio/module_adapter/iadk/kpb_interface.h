/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef _KPB_INTERFACE_H_
#define _KPB_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>
#include "system_error.h"

namespace intel_adsp
{
class ModuleHandle;

struct KpbToken;

/** \brief KPB Service interface to be used by IADK modules. */
class KpbInterface {
public:
	virtual ErrorCode::Type RegisterKpbClient(const ModuleHandle& module_handle,
						 KpbToken& token,
						 size_t current_history_depth,
						 size_t max_hist_depth,
						 uint32_t output_pin) const = 0;
	virtual ErrorCode::Type SignalDetection(KpbToken& token, void* data) const = 0;
	virtual ErrorCode::Type SignalVoiceDetection(KpbToken& token, void* data) const = 0;
	virtual ErrorCode::Type SignalSilenceDetection(KpbToken& token, void* data) const = 0;
	virtual ErrorCode::Type UnregisterKpbClient(KpbToken& token) const = 0;
	virtual ErrorCode::Type RegisterWovProducer(const ModuleHandle& module_handle) const = 0;
	virtual ErrorCode::Type RegisterKpbProducer(const ModuleHandle& module_handle) const = 0;
	virtual ErrorCode::Type RequestWovConsumer(const ModuleHandle& module_handle) const = 0;
};

/** \brief KPB Service interface v2 to be used by IADK modules. */
class KpbInterfaceV2 {
public:
	virtual ErrorCode::Type RegisterKpbClient(const ModuleHandle& module_handle,
						 KpbToken& token,
						 size_t current_history_depth,
						 size_t max_hist_depth,
						 uint32_t output_pin) const = 0;
	virtual ErrorCode::Type SignalDetection(KpbToken& token, void* data) const = 0;
	virtual ErrorCode::Type UnregisterKpbClient(KpbToken& token) const = 0;
	virtual ErrorCode::Type RegisterKpbProducer(const ModuleHandle& module_handle) const = 0;
};

} /* namespace intel_adsp */

#endif /* _KPB_INTERFACE_H_ */
