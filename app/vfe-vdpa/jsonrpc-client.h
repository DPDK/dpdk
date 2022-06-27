/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021, NVIDIA CORPORATION & AFFILIATES.
 */

#ifndef JSONRPC_CLIENT_H
#define JSONRPC_CLIENT_H

#include <stddef.h>
#include <rte_compat.h>
#include "cJSON.h"

#define KEY_PROTOCOL_VERSION "jsonrpc"
#define KEY_PROCEDURE_NAME "method"
#define KEY_ID "id"
#define KEY_PARAMETER "params"
#define KEY_AUTH "auth"
#define KEY_RESULT "result"
#define KEY_ERROR "error"
#define KEY_ERROR_CODE "code"
#define KEY_ERROR_MESSAGE "message"
#define KEY_ERROR_DATA "data"

struct jsonrpc_client {
	char ip[16];
	uint16_t port;
};

__rte_internal
cJSON *jsonrpc_client_call_method(struct jsonrpc_client *client, const char *name,
				char *param);
#endif
