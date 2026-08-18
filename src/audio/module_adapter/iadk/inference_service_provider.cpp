#include "inference_service_provider.h"

namespace
{
	using IesV1 = intel_adsp::InferenceServiceInterfaceV1;
	using intel_adsp::Array;

	class InferenceServiceProvider final : public intel_adsp::InferenceServiceInterfaceV2
	{
	public:
		uint32_t ModelGetContextSize(Model *model_data) const override;
		ErrorCode::Type ModelInit(Model *model_data, ModelContext *model_context,
					  intel_adsp::ModuleHandle *owning_module) override;
		ScalingFactors ModelGetScalingFactors(ModelContext *model_context) const override;
		ErrorCode::Type ModelGetUserMetadata(ModelContext *model_context,
						 Array<uint8_t> &metadata) const override;
		uint32_t RequestGetContextSize(ModelContext *model_context) const override;
		ErrorCode::Type RequestInit(ModelContext *model_context,
					    RequestContext *request_context) override;
		InferenceBuffer RequestGetInput(RequestContext *request_context) const override;
		InferenceBuffer RequestGetOutput(RequestContext *request_context) const override;
		InferenceBuffer RequestGetState(RequestContext *request_context) const override;
		ErrorCode::Type ModelGetRoData(ModelContext *model_context,
					   Array<const uint8_t> &ro_data) const override;
		uint32_t ModelGetLdtNumber(ModelContext *model_context) const override;
		ErrorCode::Type RequestStartAndYield(RequestContext *request_context) override;
		ErrorCode::Type RequestStartAsync(RequestContext *request_context) override;
		RequestStatus RequestQueryStatus(RequestContext *request_context) override;
		ErrorCode::Type RequestReset(RequestContext *request_context) override;
		ErrorCode::Type RequestRelease(RequestContext *request_context) override;
		ErrorCode::Type ModelRelease(ModelContext *model_context) override;
		ErrorCode::Type UpdateLayersRange(RequestContext *request_context,
						  uint32_t ldt_layer_start,
						  uint32_t layer_count) override;
	};

	uint32_t InferenceServiceProvider::ModelGetContextSize(Model *model_data) const
	{
		return 0;
	}

	IesV1::ErrorCode::Type InferenceServiceProvider::ModelInit(Model *model_data,
								      ModelContext *model_context,
								      intel_adsp::ModuleHandle *owning_module)
	{
		return ErrorCode::FATAL_FAILURE;
	}

	IesV1::ScalingFactors
	InferenceServiceProvider::ModelGetScalingFactors(ModelContext *model_context) const
	{
		return ScalingFactors(0.0f, 0.0f);
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::ModelGetUserMetadata(ModelContext *model_context,
							  Array<uint8_t> &metadata) const
	{
		return ErrorCode::FATAL_FAILURE;
	}

	uint32_t InferenceServiceProvider::RequestGetContextSize(ModelContext *model_context) const
	{
		return 0;
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestInit(ModelContext *model_context,
						RequestContext *request_context)
	{
		return ErrorCode::FATAL_FAILURE;
	}

	IesV1::InferenceBuffer
	InferenceServiceProvider::RequestGetInput(RequestContext *request_context) const
	{
		return InferenceBuffer(NULL, 0);
	}

	IesV1::InferenceBuffer
	InferenceServiceProvider::RequestGetOutput(RequestContext *request_context) const
	{
		return InferenceBuffer(NULL, 0);
	}

	IesV1::InferenceBuffer
	InferenceServiceProvider::RequestGetState(RequestContext *request_context) const
	{
		return InferenceBuffer(NULL, 0);
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::ModelGetRoData(ModelContext *model_context,
						   Array<const uint8_t> &ro_data) const
	{
		return ErrorCode::FATAL_FAILURE;
	}

	uint32_t InferenceServiceProvider::ModelGetLdtNumber(ModelContext *model_context) const
	{
		return 0;
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestStartAndYield(RequestContext *request_context)
	{
		return ErrorCode::FATAL_FAILURE;
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestStartAsync(RequestContext *request_context)
	{
		return ErrorCode::FATAL_FAILURE;
	}

	IesV1::RequestStatus
	InferenceServiceProvider::RequestQueryStatus(RequestContext *request_context)
	{
		return RequestStatus(RequestStatus::ERROR);
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestReset(RequestContext *request_context)
	{
		return ErrorCode::FATAL_FAILURE;
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestRelease(RequestContext *request_context)
	{
		return ErrorCode::FATAL_FAILURE;
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::ModelRelease(ModelContext *model_context)
	{
		return ErrorCode::FATAL_FAILURE;
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::UpdateLayersRange(RequestContext *request_context,
						      uint32_t ldt_layer_start,
						      uint32_t layer_count)
	{
		return ErrorCode::FATAL_FAILURE;
	}

	const InferenceServiceProvider provider;
}

extern "C" intel_adsp::InferenceServiceInterfaceV1 *inference_service_provider_v1(void)
{
	const intel_adsp::InferenceServiceInterfaceV1 *provider_v1 = &provider;

	return const_cast<intel_adsp::InferenceServiceInterfaceV1 *>(provider_v1);
}

extern "C" intel_adsp::InferenceServiceInterfaceV2 *inference_service_provider_v2(void)
{
	const intel_adsp::InferenceServiceInterfaceV2 *provider_v2 = &provider;

	return const_cast<intel_adsp::InferenceServiceInterfaceV2 *>(provider_v2);
}