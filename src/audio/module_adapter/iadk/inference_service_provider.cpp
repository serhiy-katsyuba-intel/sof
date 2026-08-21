#include "inference_service_provider.h"

#include <errno.h>
#include <sof/lib/inference_service.h>
#include <stdint.h>

namespace
{
	using IesV1 = intel_adsp::InferenceServiceInterfaceV1;
	using intel_adsp::Array;

	IesV1::ErrorCode::Type MapError(int ret)
	{
		if (!ret)
			return IesV1::ErrorCode::NO_ERROR;
		if (ret == -EINVAL)
			return IesV1::ErrorCode::INVALID_PARAMETERS;
		return IesV1::ErrorCode::FATAL_FAILURE;
	}

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
		struct inference_model model;

		if (!model_data || !model_data->data || !model_data->size ||
		    (uintptr_t)model_data->data % Model::kModelAlignment ||
		    model_data->size > UINTPTR_MAX - (uintptr_t)model_data->data)
			return 0;

		model.data = model_data->data;
		model.size = model_data->size;
		return inference_get_model_ctx_size(&model);
	}

	IesV1::ErrorCode::Type InferenceServiceProvider::ModelInit(Model *model_data,
								      ModelContext *model_context,
								      intel_adsp::ModuleHandle *owning_module)
	{
		struct inference_model model;
		int ret;

		if (!model_data || !model_data->data || !model_data->size || !model_context ||
		    !owning_module)
			return ErrorCode::INVALID_PARAMETERS;

		if ((uintptr_t)model_data->data % Model::kModelAlignment)
			return ErrorCode::MODEL_NOT_ALIGNED;
		if ((uintptr_t)model_context % IesV1::kModelContextAlignment)
			return ErrorCode::INVALID_PARAMETERS;

		model.data = model_data->data;
		model.size = model_data->size;
		ret = inference_model_init(&model, NULL,
			reinterpret_cast<struct gna_model_ctx *>(model_context), owning_module);
		if (ret == -EBADMSG)
			return ErrorCode::INVALID_MODEL;
		return MapError(ret);
	}

	IesV1::ScalingFactors
	InferenceServiceProvider::ModelGetScalingFactors(ModelContext *model_context) const
	{
		struct scaling_factors factors;

		if (inference_get_model_scaling_factors(
			reinterpret_cast<struct gna_model_ctx *>(model_context), &factors))
			return ScalingFactors(0.0f, 0.0f);

		return ScalingFactors(factors.in, factors.out);
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::ModelGetUserMetadata(ModelContext *model_context,
							  Array<uint8_t> &metadata) const
	{
		uint8_t *data;
		size_t size;
		int ret;

		ret = inference_model_get_user_metadata(
			reinterpret_cast<struct gna_model_ctx *>(model_context), &data, &size);
		if (ret == -ENODATA)
			return ErrorCode::NO_USER_DATA;
		if (ret)
			return MapError(ret);

		metadata.Init(data, size);
		return ErrorCode::NO_ERROR;
	}

	uint32_t InferenceServiceProvider::RequestGetContextSize(ModelContext *model_context) const
	{
		return inference_get_request_ctx_size(
			reinterpret_cast<struct gna_model_ctx *>(model_context));
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestInit(ModelContext *model_context,
						RequestContext *request_context)
	{
		struct gna_model_ctx *model_ctx =
			reinterpret_cast<struct gna_model_ctx *>(model_context);
		int ret;

		if (!model_context || !request_context)
			return ErrorCode::INVALID_PARAMETERS;
		if ((uintptr_t)request_context % IesV1::kRequestContextAlignment)
			return ErrorCode::INVALID_PARAMETERS;

		ret = inference_request_init(model_ctx,
			reinterpret_cast<struct gna_request_ctx *>(request_context));
		if (!ret)
			return ErrorCode::NO_ERROR;
		return ret == -EINVAL ? ErrorCode::INVALID_PARAMETERS : ErrorCode::FATAL_FAILURE;
	}

	IesV1::InferenceBuffer
	InferenceServiceProvider::RequestGetInput(RequestContext *request_context) const
	{
		uint8_t *buffer;
		size_t size;

		if (inference_request_get_input(
			reinterpret_cast<struct gna_request_ctx *>(request_context),
			&buffer, &size))
			return InferenceBuffer(NULL, 0);

		return InferenceBuffer(buffer, size);
	}

	IesV1::InferenceBuffer
	InferenceServiceProvider::RequestGetOutput(RequestContext *request_context) const
	{
		uint8_t *buffer;
		size_t size;

		if (inference_request_get_output(
			reinterpret_cast<struct gna_request_ctx *>(request_context),
			&buffer, &size))
			return InferenceBuffer(NULL, 0);

		return InferenceBuffer(buffer, size);
	}

	IesV1::InferenceBuffer
	InferenceServiceProvider::RequestGetState(RequestContext *request_context) const
	{
		uint8_t *buffer;
		size_t size;

		if (inference_request_get_state(
			reinterpret_cast<struct gna_request_ctx *>(request_context),
			&buffer, &size))
			return InferenceBuffer(NULL, 0);

		return InferenceBuffer(buffer, size);
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::ModelGetRoData(ModelContext *model_context,
						   Array<const uint8_t> &ro_data) const
	{
		const uint8_t *data;
		size_t size;
		int ret;

		ret = inference_model_get_ro_data(
			reinterpret_cast<struct gna_model_ctx *>(model_context), &data, &size);
		if (ret)
			return MapError(ret);

		ro_data.Init(data, size);
		return ErrorCode::NO_ERROR;
	}

	uint32_t InferenceServiceProvider::ModelGetLdtNumber(ModelContext *model_context) const
	{
		return inference_model_get_ldt_number(
			reinterpret_cast<struct gna_model_ctx *>(model_context));
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestStartAndYield(RequestContext *request_context)
	{
		return MapError(inference_request_start_yield(
			reinterpret_cast<struct gna_request_ctx *>(request_context)));
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestStartAsync(RequestContext *request_context)
	{
		return MapError(inference_request_start_async(
			reinterpret_cast<struct gna_request_ctx *>(request_context)));
	}

	IesV1::RequestStatus
	InferenceServiceProvider::RequestQueryStatus(RequestContext *request_context)
	{
		switch (inference_request_query_status(
			reinterpret_cast<struct gna_request_ctx *>(request_context))) {
		case REQUEST_SUCCESS:
			return RequestStatus(RequestStatus::SUCCESS);
		case REQUEST_PENDING:
			return RequestStatus(RequestStatus::PENDING);
		default:
			return RequestStatus(RequestStatus::ERROR);
		}
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestReset(RequestContext *request_context)
	{
		return MapError(inference_request_reset(
			reinterpret_cast<struct gna_request_ctx *>(request_context)));
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::RequestRelease(RequestContext *request_context)
	{
		int ret = inference_request_release(
			reinterpret_cast<struct gna_request_ctx *>(request_context));

		return MapError(ret);
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::ModelRelease(ModelContext *model_context)
	{
		int ret = inference_model_release(
			reinterpret_cast<struct gna_model_ctx *>(model_context));

		return MapError(ret);
	}

	IesV1::ErrorCode::Type
	InferenceServiceProvider::UpdateLayersRange(RequestContext *request_context,
						      uint32_t ldt_layer_start,
						      uint32_t layer_count)
	{
		return MapError(inference_update_layers_range(
			reinterpret_cast<struct gna_request_ctx *>(request_context),
			ldt_layer_start, layer_count));
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

extern "C" struct system_service_iface *inference_service_provider_iface_v1(void)
{
	return reinterpret_cast<struct system_service_iface *>(inference_service_provider_v1());
}
extern "C" struct system_service_iface *inference_service_provider_iface_v2(void)
{
	return reinterpret_cast<struct system_service_iface *>(inference_service_provider_v2());
}
