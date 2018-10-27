/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _RTE_TELEMETRY_PARSER_TEST_H_
#define _RTE_TELEMETRY_PARSER_TEST_H_

int32_t
rte_telemetry_parser_test(struct telemetry_impl *telemetry);

int32_t
rte_telemetry_format_port_stat_ids(int *port_ids, int num_port_ids,
	const char * const stat_names, int num_stat_names, json_t **data);

int32_t
rte_telemetry_create_json_request(int action, char *command,
	const char *client_path, int *port_ids, int num_port_ids,
	const char * const stat_names, int num_stat_names, char **request,
	int inv_choice);

int32_t
rte_telemetry_send_get_ports_and_stats_request(struct telemetry_impl *telemetry,
	int action_choice, char *command_choice, int inv_choice);

int32_t
rte_telemetry_send_get_ports_details_request(struct telemetry_impl *telemetry,
	int action_choice, int *port_ids, int num_port_ids, int inv_choice);

int32_t
rte_telemetry_send_stats_values_by_name_request(struct telemetry_impl
	*telemetry, int action_choice, int *port_ids, int num_port_ids,
	const char * const stat_names, int num_stat_names,
	int inv_choice);

int32_t
rte_telemetry_send_unreg_request(int action_choice, const char *client_path,
	int inv_choice);

#endif
