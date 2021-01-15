/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifdef RTE_LIB_TELEMETRY
#include <jansson.h>
#else
#define json_t void *
#endif

#include "rte_metrics.h"

#ifndef _RTE_METRICS_TELEMETRY_H_
#define _RTE_METRICS_TELEMETRY_H_


enum rte_telemetry_stats_type {
	PORT_STATS = 0,
	GLOBAL_STATS = 1
};

struct telemetry_encode_param {
	enum rte_telemetry_stats_type type;
	struct port_param {
		int num_metric_ids;
		uint32_t metric_ids[RTE_METRICS_MAX_METRICS];
		int num_port_ids;
		uint32_t port_ids[RTE_MAX_ETHPORTS];
	} pp;
};

struct telemetry_metrics_data {
	int reg_index[RTE_MAX_ETHPORTS];
	int metrics_register_done;
};

__rte_experimental
int32_t rte_metrics_tel_reg_all_ethdev(int *metrics_register_done,
		int *reg_index_list);

__rte_experimental
int32_t
rte_metrics_tel_encode_json_format(struct telemetry_encode_param *ep,
		char **json_buffer);

__rte_experimental
int32_t
rte_metrics_tel_get_global_stats(struct telemetry_encode_param *ep);

__rte_experimental
int32_t
rte_metrics_tel_get_port_stats_ids(struct telemetry_encode_param *ep);

__rte_experimental
int32_t
rte_metrics_tel_get_ports_stats_json(struct telemetry_encode_param *ep,
		int *reg_index, char **json_buffer);

__rte_experimental
int32_t
rte_metrics_tel_extract_data(struct telemetry_encode_param *ep, json_t *data);

#endif
