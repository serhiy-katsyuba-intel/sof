#ifndef SOF_AUDIO_MODULE_ADAPTER_IADK_TIMESTAMPING_SERVICE_PROVIDER_H
#define SOF_AUDIO_MODULE_ADAPTER_IADK_TIMESTAMPING_SERVICE_PROVIDER_H

#include <timestamping_interface.h>

extern "C" intel_adsp::TimestampingInterface *timestamping_service_provider_v1(void);
extern "C" struct system_service_iface *timestamping_service_provider_iface_v1(void);

#endif /* SOF_AUDIO_MODULE_ADAPTER_IADK_TIMESTAMPING_SERVICE_PROVIDER_H */