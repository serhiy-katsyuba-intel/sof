#ifndef SOF_AUDIO_MODULE_ADAPTER_IADK_INFERENCE_SERVICE_PROVIDER_H
#define SOF_AUDIO_MODULE_ADAPTER_IADK_INFERENCE_SERVICE_PROVIDER_H

#include <inference_service_interface.h>

extern "C" intel_adsp::InferenceServiceInterfaceV1 *inference_service_provider_v1(void);
extern "C" intel_adsp::InferenceServiceInterfaceV2 *inference_service_provider_v2(void);

#endif /* SOF_AUDIO_MODULE_ADAPTER_IADK_INFERENCE_SERVICE_PROVIDER_H */