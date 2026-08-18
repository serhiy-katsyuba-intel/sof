#ifndef SOF_AUDIO_MODULE_ADAPTER_IADK_INFERENCE_SERVICE_INTERFACE_H
#define SOF_AUDIO_MODULE_ADAPTER_IADK_INFERENCE_SERVICE_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <module/module/system_service.h>

#include "array.h"
#include "system_agent_interface.h"
#include "system_error.h"

namespace intel_adsp
{
	class InferenceServiceInterfaceV1
	{
	public:
		struct Model {
			Model() : data(), size() {}
			Model(const uint8_t *model_data, size_t model_size) :
				data(model_data), size(model_size) {}

			static const size_t kModelAlignment = 64;
			const uint8_t *data;
			size_t size;
		};

		typedef void *ModelContext;
		static const size_t kModelContextAlignment = 64;

		struct ScalingFactors {
			ScalingFactors() : in(), out() {}
			ScalingFactors(float input, float output) : in(input), out(output) {}

			float in;
			float out;
		};

		typedef void *RequestContext;
		static const size_t kRequestContextAlignment = 64;

		struct InferenceBuffer {
			InferenceBuffer() : data(), size() {}
			InferenceBuffer(uint8_t *buffer_data, size_t buffer_size) :
				data(buffer_data), size(buffer_size) {}

			uint8_t *const data;
			size_t size;
		};

		struct ErrorCode : intel_adsp::ErrorCode {
			enum Enum {
				INVALID_MODEL = intel_adsp::ErrorCode::MaxValue + 1,
				UNSUPPORTED_MODEL,
				NO_USER_DATA,
				MODEL_NOT_ALIGNED,
			};

			static const Enum MinValue = INVALID_MODEL;
			static const Enum MaxValue = MODEL_NOT_ALIGNED;

			explicit ErrorCode(Type value) : intel_adsp::ErrorCode(value) {}
		};

		struct RequestStatus {
			enum Enum {
				SUCCESS = 0,
				PENDING = 1,
				ERROR,
			};

			typedef int Type;

			RequestStatus() : value_(SUCCESS) {}
			RequestStatus(Enum status) : value_(status) {}

			operator Enum()
			{
				return static_cast<Enum>(value_);
			}

		private:
			Type value_;
		};

		virtual uint32_t ModelGetContextSize(Model *model_data) const = 0;
		virtual ErrorCode::Type ModelInit(Model *model_data, ModelContext *model_context,
						  ModuleHandle *owning_module) = 0;
		virtual ScalingFactors ModelGetScalingFactors(ModelContext *model_context) const = 0;
		virtual ErrorCode::Type ModelGetUserMetadata(ModelContext *model_context,
							 Array<uint8_t> &metadata) const = 0;
		virtual uint32_t RequestGetContextSize(ModelContext *model_context) const = 0;
		virtual ErrorCode::Type RequestInit(ModelContext *model_context,
						    RequestContext *request_context) = 0;
		virtual InferenceBuffer RequestGetInput(RequestContext *request_context) const = 0;
		virtual InferenceBuffer RequestGetOutput(RequestContext *request_context) const = 0;
		virtual InferenceBuffer RequestGetState(RequestContext *request_context) const = 0;
		virtual ErrorCode::Type ModelGetRoData(ModelContext *model_context,
						   Array<const uint8_t> &ro_data) const = 0;
		virtual uint32_t ModelGetLdtNumber(ModelContext *model_context) const = 0;
		virtual ErrorCode::Type RequestStartAndYield(RequestContext *request_context) = 0;
		virtual ErrorCode::Type RequestStartAsync(RequestContext *request_context) = 0;
		virtual RequestStatus RequestQueryStatus(RequestContext *request_context) = 0;
		virtual ErrorCode::Type RequestReset(RequestContext *request_context) = 0;
		virtual ErrorCode::Type RequestRelease(RequestContext *request_context) = 0;
		virtual ErrorCode::Type ModelRelease(ModelContext *model_context) = 0;
	};

	class InferenceServiceInterfaceV2 : public InferenceServiceInterfaceV1
	{
	public:
		virtual ErrorCode::Type UpdateLayersRange(RequestContext *request_context,
							  uint32_t ldt_layer_start,
							  uint32_t layer_count) = 0;
	};

	using InferenceServiceInterface = InferenceServiceInterfaceV2;

	static_assert(InferenceServiceInterfaceV1::Model::kModelAlignment == 64,
		      "IES model alignment changed");
	static_assert(InferenceServiceInterfaceV1::kModelContextAlignment == 64,
		      "IES model context alignment changed");
	static_assert(InferenceServiceInterfaceV1::kRequestContextAlignment == 64,
		      "IES request context alignment changed");
	static_assert(InferenceServiceInterfaceV1::ErrorCode::INVALID_MODEL == 7,
		      "IES error values changed");
	static_assert(InferenceServiceInterfaceV1::ErrorCode::UNSUPPORTED_MODEL == 8,
		      "IES error values changed");
	static_assert(InferenceServiceInterfaceV1::ErrorCode::NO_USER_DATA == 9,
		      "IES error values changed");
	static_assert(InferenceServiceInterfaceV1::ErrorCode::MODEL_NOT_ALIGNED == 10,
		      "IES error values changed");
	static_assert(InferenceServiceInterfaceV1::RequestStatus::SUCCESS == 0,
		      "IES request status values changed");
	static_assert(InferenceServiceInterfaceV1::RequestStatus::PENDING == 1,
		      "IES request status values changed");
	static_assert(InferenceServiceInterfaceV1::RequestStatus::ERROR == 2,
		      "IES request status values changed");
	static_assert(intel_adsp::ErrorCode::NO_ERROR == 0, "common error values changed");
	static_assert(intel_adsp::ErrorCode::INVALID_PARAMETERS == 1,
		      "common error values changed");
	static_assert(intel_adsp::ErrorCode::BUSY == 4, "common error values changed");
	static_assert(intel_adsp::ErrorCode::FATAL_FAILURE == 6,
		      "common error values changed");
	static_assert(INTERFACE_ID_INFERENCE_SERVICE == 0x1001,
		      "IES interface ID changed");
	static_assert(INTERFACE_VERSION_INFERENCE_SERVICE_V1 == 0x1000,
		      "IES V1 version changed");
	static_assert(INTERFACE_VERSION_INFERENCE_SERVICE_V2 == 0x1001,
		      "IES V2 version changed");
	static_assert(INTERFACE_VERSION_INFERENCE_SERVICE ==
		      INTERFACE_VERSION_INFERENCE_SERVICE_V2,
		      "default IES version must be V2");
	static_assert(!__has_virtual_destructor(InferenceServiceInterfaceV1),
		      "IES V1 must not have a virtual destructor");
	static_assert(!__has_virtual_destructor(InferenceServiceInterfaceV2),
		      "IES V2 must not have a virtual destructor");

#ifdef __XTENSA__
	static_assert(sizeof(void *) == 4, "IES requires the ELF32 Xtensa ABI");
	static_assert(sizeof(size_t) == 4, "IES requires 32-bit size_t");
	static_assert(sizeof(InferenceServiceInterfaceV1::Model) == 8,
		      "IES Model layout changed");
	static_assert(alignof(InferenceServiceInterfaceV1::Model) == 4,
		      "IES Model alignment changed");
	static_assert(offsetof(InferenceServiceInterfaceV1::Model, data) == 0,
		      "IES Model::data offset changed");
	static_assert(offsetof(InferenceServiceInterfaceV1::Model, size) == 4,
		      "IES Model::size offset changed");
	static_assert(sizeof(InferenceServiceInterfaceV1::ScalingFactors) == 8,
		      "IES ScalingFactors layout changed");
	static_assert(alignof(InferenceServiceInterfaceV1::ScalingFactors) == 4,
		      "IES ScalingFactors alignment changed");
	static_assert(offsetof(InferenceServiceInterfaceV1::ScalingFactors, in) == 0,
		      "IES ScalingFactors::in offset changed");
	static_assert(offsetof(InferenceServiceInterfaceV1::ScalingFactors, out) == 4,
		      "IES ScalingFactors::out offset changed");
	static_assert(sizeof(InferenceServiceInterfaceV1::InferenceBuffer) == 8,
		      "IES InferenceBuffer layout changed");
	static_assert(alignof(InferenceServiceInterfaceV1::InferenceBuffer) == 4,
		      "IES InferenceBuffer alignment changed");
	static_assert(offsetof(InferenceServiceInterfaceV1::InferenceBuffer, data) == 0,
		      "IES InferenceBuffer::data offset changed");
	static_assert(offsetof(InferenceServiceInterfaceV1::InferenceBuffer, size) == 4,
		      "IES InferenceBuffer::size offset changed");
	static_assert(sizeof(InferenceServiceInterfaceV1::RequestStatus) == 4,
		      "IES RequestStatus layout changed");
	static_assert(alignof(InferenceServiceInterfaceV1::RequestStatus) == 4,
		      "IES RequestStatus alignment changed");
	static_assert(sizeof(Array<uint8_t>) == 8, "IES Array layout changed");
	static_assert(alignof(Array<uint8_t>) == 4, "IES Array alignment changed");
	static_assert(sizeof(InferenceServiceInterfaceV1) == 4,
		      "IES V1 object layout changed");
	static_assert(alignof(InferenceServiceInterfaceV1) == 4,
		      "IES V1 object alignment changed");
	static_assert(sizeof(InferenceServiceInterfaceV2) == 4,
		      "IES V2 object layout changed");
	static_assert(alignof(InferenceServiceInterfaceV2) == 4,
		      "IES V2 object alignment changed");
#endif
}

#endif /* SOF_AUDIO_MODULE_ADAPTER_IADK_INFERENCE_SERVICE_INTERFACE_H */