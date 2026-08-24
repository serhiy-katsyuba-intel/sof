#include "timestamping_service_provider.h"

namespace
{
	class TimestampingServiceProvider final : public intel_adsp::TimestampingInterface
	{
	public:
		intel_adsp::ErrorCode::Type GetCurrentPosition(
			const intel_adsp::ModuleHandle &module_handle,
			intel_adsp::ModuleCounters &counters) const override;
		intel_adsp::ErrorCode::Type AdjustPosition(
			const intel_adsp::ModuleHandle &module_handle,
			intel_adsp::ModuleCounters &counters, int64_t ms_shift) const override;
	};

	intel_adsp::ErrorCode::Type TimestampingServiceProvider::GetCurrentPosition(
		const intel_adsp::ModuleHandle &module_handle,
		intel_adsp::ModuleCounters &counters) const
	{
		(void)module_handle;
		counters = {};
		return intel_adsp::ErrorCode::NO_ERROR;
	}

	intel_adsp::ErrorCode::Type TimestampingServiceProvider::AdjustPosition(
		const intel_adsp::ModuleHandle &module_handle,
		intel_adsp::ModuleCounters &counters, int64_t ms_shift) const
	{
		(void)module_handle;
		(void)counters;
		return ms_shift == 0 ? intel_adsp::ErrorCode::NO_ERROR :
			intel_adsp::ErrorCode::FATAL_FAILURE;
	}

	const TimestampingServiceProvider provider;
}

extern "C" intel_adsp::TimestampingInterface *timestamping_service_provider_v1(void)
{
	const intel_adsp::TimestampingInterface *provider_v1 = &provider;

	return const_cast<intel_adsp::TimestampingInterface *>(provider_v1);
}

extern "C" struct system_service_iface *timestamping_service_provider_iface_v1(void)
{
	return reinterpret_cast<struct system_service_iface *>(timestamping_service_provider_v1());
}