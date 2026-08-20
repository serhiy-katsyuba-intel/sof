/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef _KPB_INTERFACE_V2_H_
#define _KPB_INTERFACE_V2_H_

#include <stddef.h>
#include <stdint.h>
#include "system_error.h"

namespace intel_adsp
{
class ModuleHandle;

struct KpbToken;

/**
 * \brief KPB Service interface v2 for processing modules.
 *
 * Retrieve this interface through SystemService::GetInterface() using the KPB
 * service identifier and version 2.
 */
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

#endif /* _KPB_INTERFACE_V2_H_ */