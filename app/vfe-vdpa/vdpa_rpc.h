/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022, NVIDIA CORPORATION & AFFILIATES.
 */

#ifndef _VDPA_RPC_H_
#define _VDPA_RPC_H_

#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "jsonrpc-c.h"

#define MAX_PATH_LEN 128
#define MAX_IF_PATH_LEN 120
/* VDPA RPC */
#define VIRTNET_FEATURE_SZ	64
#define MAX_VDPA_SAMPLE_PORTS 1024
#define RTE_LOGTYPE_RPC RTE_LOGTYPE_USER1
#define VDPA_LOCAL_HOST "127.0.0.1"

enum {
	VDPA_RPC_PORT = 12190,
};

#define VDPA_RPC_JSON_EMPTY_SZ (4)
#define VDPA_RPC_PARAM_SZ (256)
#define MAX_JSON_STRING_LEN (20)

struct vdpa_rpc_context {
	struct jrpc_server	rpc_server;
	pthread_t		rpc_server_tid;
	pthread_mutex_t		rpc_lock;
};

int vdpa_rpc_start(struct vdpa_rpc_context *rpc_ctx);
void vdpa_rpc_stop(struct vdpa_rpc_context *rpc_ctx);
int vdpa_with_socket_path_start(const char *vf_name,
		const char *socket_file);
void vdpa_with_socket_path_stop(const char *vf_name);
int vdpa_get_socket_file_name(const char *vf_name,
		char *socket_file);
bool vdpa_socket_file_exists(const char *socket_file);
#endif /* _VDPA_RPC_H_ */
