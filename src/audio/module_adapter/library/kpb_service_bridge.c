// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <rtos/userspace_helper.h>
#include <sof/audio/component.h>
#include <sof/audio/kpb.h>
#include <sof/audio/module_adapter/library/kpb_service.h>
#include <sof/lib/notifier.h>

struct kpb_service_client {
	struct kpb_event_data event;
	struct kpb_client client;
	size_t current_history_depth;
	size_t max_history_depth;
	bool in_use;
};

static APP_TASK_DATA struct kpb_service_client clients[KPB_MAX_NO_OF_CLIENTS];

static AdspErrorCode kpb_service_translate_error(int error)
{
	switch (error) {
	case 0:
	case -EINPROGRESS:
		return ADSP_NO_ERROR;
	case -EINVAL:
		return ADSP_INVALID_PARAMETERS;
	case -EBUSY:
		return ADSP_BUSY_RESOURCE;
	case -ENOMEM:
	case -ENOSPC:
		return ADSP_OUT_OF_MEMORY;
	default:
		return ADSP_FATAL_FAILURE;
	}
}

static bool kpb_service_client_is_valid(const struct kpb_service_client *client)
{
	int i;

	for (i = 0; i < KPB_MAX_NO_OF_CLIENTS; i++) {
		if (client == &clients[i])
			return clients[i].in_use;
	}

	return false;
}

static AdspErrorCode kpb_service_send_event(struct kpb_service_client *client,
					    enum kpb_event event_id)
{
	client->event.event_id = event_id;
	client->event.client_data = &client->client;
	client->event.status = -EINPROGRESS;

	notifier_event(client, NOTIFIER_ID_KPB_CLIENT_EVT, NOTIFIER_TARGET_CORE_ALL_MASK,
		       &client->event, sizeof(client->event));

	return kpb_service_translate_error(client->event.status);
}

AdspErrorCode kpb_service_register_client(size_t current_history_depth,
					  size_t max_history_depth, uint32_t output_pin,
					  struct kpb_service_client **client)
{
	struct kpb_service_client *new_client;
	AdspErrorCode error;

	if (!client || output_pin >= KPB_MAX_NO_OF_CLIENTS ||
	    current_history_depth > UINT32_MAX || max_history_depth > UINT32_MAX)
		return ADSP_INVALID_PARAMETERS;

	*client = NULL;
	new_client = &clients[output_pin];
	if (new_client->in_use)
		return ADSP_BUSY_RESOURCE;

	new_client->in_use = true;
	new_client->current_history_depth = current_history_depth;
	new_client->max_history_depth = max_history_depth;
	new_client->client.id = output_pin;
	new_client->client.drain_req = current_history_depth;
	new_client->client.state = KPB_CLIENT_UNREGISTERED;
	new_client->client.r_ptr = NULL;
	new_client->client.sink = NULL;
	error = kpb_service_send_event(new_client, KPB_EVENT_REGISTER_CLIENT);
	if (error != ADSP_NO_ERROR) {
		new_client->in_use = false;
		return error;
	}

	*client = new_client;
	return ADSP_NO_ERROR;
}

AdspErrorCode kpb_service_signal_detection(struct kpb_service_client *client,
					   uint32_t phrase_length)
{
	size_t history_depth;

	if (!kpb_service_client_is_valid(client))
		return ADSP_INVALID_PARAMETERS;

	history_depth = client->current_history_depth + phrase_length;
	if (history_depth < client->current_history_depth ||
	    history_depth > client->max_history_depth)
		history_depth = client->max_history_depth;

	client->client.drain_req = history_depth;
	return kpb_service_send_event(client, KPB_EVENT_BEGIN_DRAINING);
}

AdspErrorCode kpb_service_unregister_client(struct kpb_service_client *client)
{
	AdspErrorCode error;

	if (!kpb_service_client_is_valid(client))
		return ADSP_INVALID_PARAMETERS;
	error = kpb_service_send_event(client, KPB_EVENT_UNREGISTER_CLIENT);
	if (error != ADSP_NO_ERROR)
		return error;

	client->in_use = false;
	return ADSP_NO_ERROR;
}