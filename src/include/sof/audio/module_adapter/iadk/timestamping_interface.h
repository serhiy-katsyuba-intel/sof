#ifndef SOF_AUDIO_MODULE_ADAPTER_IADK_TIMESTAMPING_INTERFACE_H
#define SOF_AUDIO_MODULE_ADAPTER_IADK_TIMESTAMPING_INTERFACE_H

#include <stdint.h>

#include "system_error.h"

namespace intel_adsp
{
	class ModuleHandle;

	struct ModuleCounters {
		uint64_t timer_value;
		uint64_t linear_link_position;
		uint64_t gateway_total_processed_data;
		uint64_t module_total_processed_data;
	};

	class TimestampingInterface
	{
	public:
		virtual ErrorCode::Type GetCurrentPosition(const ModuleHandle &module_handle,
							   ModuleCounters &counters) const = 0;
		virtual ErrorCode::Type AdjustPosition(const ModuleHandle &module_handle,
						       ModuleCounters &counters,
						       int64_t ms_shift) const = 0;
	};
}

#endif /* SOF_AUDIO_MODULE_ADAPTER_IADK_TIMESTAMPING_INTERFACE_H */