/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_log.h>
#include <rte_tailq.h>
#include <rte_metrics.h>
#include <rte_metrics_telemetry.h>

#ifndef _RTE_TELEMETRY_INTERNAL_H_
#define _RTE_TELEMETRY_INTERNAL_H_

/* function types for the functions coming from metrics library */
typedef int32_t (*metrics_tel_reg_all_ethdev_t)(int *metrics_register_done,
		int *reg_index_list);

typedef int32_t (*metrics_tel_encode_json_format_t)(
		struct telemetry_encode_param *ep, char **json_buffer);

typedef int32_t (*metrics_tel_get_port_stats_ids_t)(
		struct telemetry_encode_param *ep);

typedef int32_t (*metrics_tel_get_ports_stats_json_t)(
		struct telemetry_encode_param *ep,
		int *reg_index, char **json_buffer);

typedef int32_t (*metrics_tel_extract_data_t)(struct telemetry_encode_param *ep,
		json_t *data);

struct metrics_functions {
	metrics_tel_reg_all_ethdev_t reg_all_ethdev;
	metrics_tel_encode_json_format_t encode_json_format;
	metrics_tel_get_port_stats_ids_t get_port_stats_ids;
	metrics_tel_get_ports_stats_json_t get_ports_stats_json;
	metrics_tel_extract_data_t extract_data;
};
extern const struct metrics_functions *metrics_fns;

/* API for use by metrics libraries to provide the functions to use */
__rte_experimental
void rte_telemetry_set_metrics_fns(const struct metrics_functions *fns);

/* Logging Macros */
extern int telemetry_log_level;

#define TELEMETRY_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ##level, telemetry_log_level, "%s(): "fmt "\n", \
		__func__, ##args)

#define TELEMETRY_LOG_ERR(fmt, args...) \
	TELEMETRY_LOG(ERR, fmt, ## args)

#define TELEMETRY_LOG_WARN(fmt, args...) \
	TELEMETRY_LOG(WARNING, fmt, ## args)

#define TELEMETRY_LOG_INFO(fmt, args...) \
	TELEMETRY_LOG(INFO, fmt, ## args)

#define MAX_METRICS 256

typedef struct telemetry_client {
	char *file_path;
	int fd;
	TAILQ_ENTRY(telemetry_client) client_list;
} telemetry_client;

typedef struct telemetry_impl {
	int accept_fd;
	int server_fd;
	pthread_t thread_id;
	int thread_status;
	uint32_t socket_id;
	int reg_index[RTE_MAX_ETHPORTS];
	int metrics_register_done;
	TAILQ_HEAD(, telemetry_client) client_list_head;
	struct telemetry_client *request_client;
	int register_fail_count;
} telemetry_impl;

enum rte_telemetry_parser_actions {
	ACTION_GET = 0,
	ACTION_DELETE = 2
};

int32_t
rte_telemetry_parse_client_message(struct telemetry_impl *telemetry, char *buf);

int32_t
rte_telemetry_send_error_response(struct telemetry_impl *telemetry,
	int error_type);

int32_t
rte_telemetry_register_client(struct telemetry_impl *telemetry,
	const char *client_path);

int32_t
rte_telemetry_unregister_client(struct telemetry_impl *telemetry,
	const char *client_path);

int32_t
rte_telemetry_send_ports_stats_values(struct telemetry_encode_param *ep,
	struct telemetry_impl *telemetry);

int32_t
rte_telemetry_socket_messaging_testing(int index, int socket);

int32_t
rte_telemetry_send_global_stats_values(struct telemetry_encode_param *ep,
	struct telemetry_impl *telemetry);

int32_t
rte_telemetry_parser_test(struct telemetry_impl *telemetry);

#endif
