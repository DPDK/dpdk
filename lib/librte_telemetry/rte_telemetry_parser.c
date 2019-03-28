/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>

#include <rte_metrics.h>
#include <rte_common.h>
#include <rte_ethdev.h>

#include "rte_telemetry_internal.h"

typedef int (*command_func)(struct telemetry_impl *, int, json_t *);

struct rte_telemetry_command {
	char *text;
	command_func fn;
} command;

static int32_t
rte_telemetry_command_clients(struct telemetry_impl *telemetry, int action,
	json_t *data)
{
	int ret;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Invalid telemetry argument");
		return -1;
	}

	if (action != ACTION_DELETE) {
		TELEMETRY_LOG_WARN("Invalid action for this command");
		goto einval_fail;
	}

	if (!json_is_object(data)) {
		TELEMETRY_LOG_WARN("Invalid data provided for this command");
		goto einval_fail;
	}

	json_t *client_path = json_object_get(data, "client_path");
	if (!json_is_string(client_path)) {
		TELEMETRY_LOG_WARN("Command value is not a string");
		goto einval_fail;
	}

	ret = rte_telemetry_unregister_client(telemetry,
			json_string_value(client_path));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not unregister client");
		goto einval_fail;
	}

	return 0;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;
}

static int32_t
rte_telemetry_command_ports(struct telemetry_impl *telemetry, int action,
	json_t *data)
{
	int ret;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Invalid telemetry argument");
		return -1;
	}

	if (!json_is_null(data)) {
		TELEMETRY_LOG_WARN("Data should be NULL JSON object for 'ports' command");
		goto einval_fail;
	}

	if (action != ACTION_GET) {
		TELEMETRY_LOG_WARN("Invalid action for this command");
		goto einval_fail;
	}

	return 0;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;
}

static int32_t
rte_telemetry_command_ports_details(struct telemetry_impl *telemetry,
	int action, json_t *data)
{
	json_t *value, *port_ids_json = json_object_get(data, "ports");
	uint64_t num_port_ids = json_array_size(port_ids_json);
	int ret, port_ids[num_port_ids];
	RTE_SET_USED(port_ids);
	size_t index;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Invalid telemetry argument");
		return -1;
	}

	if (action != ACTION_GET) {
		TELEMETRY_LOG_WARN("Invalid action for this command");
		goto einval_fail;
	}

	if (!json_is_object(data)) {
		TELEMETRY_LOG_WARN("Invalid data provided for this command");
		goto einval_fail;
	}

	if (!json_is_array(port_ids_json)) {
		TELEMETRY_LOG_WARN("Invalid Port ID array");
		goto einval_fail;
	}

	json_array_foreach(port_ids_json, index, value) {
		if (!json_is_integer(value)) {
			TELEMETRY_LOG_WARN("Port ID given is invalid");
			goto einval_fail;
		}
		port_ids[index] = json_integer_value(value);
	}

	return 0;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;
}

static int32_t
rte_telemetry_command_port_stats(struct telemetry_impl *telemetry, int action,
	json_t *data)
{
	int ret;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Invalid telemetry argument");
		return -1;
	}

	if (!json_is_null(data)) {
		TELEMETRY_LOG_WARN("Data should be NULL JSON object for 'port_stats' command");
		goto einval_fail;
	}

	if (action != ACTION_GET) {
		TELEMETRY_LOG_WARN("Invalid action for this command");
		goto einval_fail;
	}

	return 0;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;
}

static int32_t
rte_telemetry_stat_names_to_ids(struct telemetry_impl *telemetry,
	const char * const *stat_names, uint32_t *stat_ids,
	uint64_t num_stat_names)
{
	struct rte_metric_name *names;
	int ret, num_metrics;
	uint32_t i, k;

	if (stat_names == NULL) {
		TELEMETRY_LOG_WARN("Invalid stat_names argument");
		goto einval_fail;
	}

	if (num_stat_names <= 0) {
		TELEMETRY_LOG_WARN("Invalid num_stat_names argument");
		goto einval_fail;
	}

	num_metrics = rte_metrics_get_names(NULL, 0);
	if (num_metrics < 0) {
		TELEMETRY_LOG_ERR("Cannot get metrics count");
		goto eperm_fail;
	} else if (num_metrics == 0) {
		TELEMETRY_LOG_WARN("No metrics have been registered");
		goto eperm_fail;
	}

	names = malloc(sizeof(struct rte_metric_name) * num_metrics);
	if (names == NULL) {
		TELEMETRY_LOG_ERR("Cannot allocate memory for names");

		ret = rte_telemetry_send_error_response(telemetry, -ENOMEM);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");

		return -1;
	}

	ret = rte_metrics_get_names(names, num_metrics);
	if (ret < 0 || ret > num_metrics) {
		TELEMETRY_LOG_ERR("Cannot get metrics names");
		free(names);
		goto eperm_fail;
	}

	k = 0;
	for (i = 0; i < (uint32_t)num_stat_names; i++) {
		uint32_t j;
		for (j = 0; j < (uint32_t)num_metrics; j++) {
			if (strcmp(stat_names[i], names[j].name) == 0) {
				stat_ids[k] = j;
				k++;
				break;
			}
		}
	}

	if (k != num_stat_names) {
		TELEMETRY_LOG_WARN("Invalid stat names provided");
		free(names);
		goto einval_fail;
	}

	free(names);
	return 0;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;

eperm_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EPERM);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;
}

int32_t
rte_telemetry_command_ports_all_stat_values(struct telemetry_impl *telemetry,
	 int action, json_t *data)
{
	int ret, num_metrics, i, p;
	struct rte_metric_value *values;
	uint64_t num_port_ids = 0;
	uint32_t port_ids[RTE_MAX_ETHPORTS];

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Invalid telemetry argument");
		return -1;
	}

	if (action != ACTION_GET) {
		TELEMETRY_LOG_WARN("Invalid action for this command");
		ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	if (json_is_object(data)) {
		TELEMETRY_LOG_WARN("Invalid data provided for this command");
		ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	num_metrics = rte_metrics_get_values(0, NULL, 0);
	if (num_metrics < 0) {
		TELEMETRY_LOG_ERR("Cannot get metrics count");

		ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");

		return -1;
	} else if (num_metrics == 0) {
		TELEMETRY_LOG_ERR("No metrics to display (none have been registered)");

		ret = rte_telemetry_send_error_response(telemetry, -EPERM);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");

		return -1;
	}

	values = malloc(sizeof(struct rte_metric_value) * num_metrics);
	if (values == NULL) {
		TELEMETRY_LOG_ERR("Cannot allocate memory");
		ret = rte_telemetry_send_error_response(telemetry,
			 -ENOMEM);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	uint32_t stat_ids[num_metrics];

	RTE_ETH_FOREACH_DEV(p) {
		port_ids[num_port_ids] = p;
		num_port_ids++;
	}

	if (!num_port_ids) {
		TELEMETRY_LOG_WARN("No active ports");

		ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");

		goto fail;
	}

	ret = rte_metrics_get_values(port_ids[0], values, num_metrics);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not get stat values");
		goto fail;
	}
	for (i = 0; i < num_metrics; i++)
		stat_ids[i] = values[i].key;

	ret = rte_telemetry_send_ports_stats_values(stat_ids, num_metrics,
		port_ids, num_port_ids, telemetry);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Sending ports stats values failed");
		goto fail;
	}

	return 0;

fail:
	free(values);
	return -1;
}

int32_t
rte_telemetry_command_ports_stats_values_by_name(struct telemetry_impl
	*telemetry, int action, json_t *data)
{
	int ret;
	json_t *port_ids_json = json_object_get(data, "ports");
	json_t *stat_names_json = json_object_get(data, "stats");
	uint64_t num_port_ids = json_array_size(port_ids_json);
	uint64_t num_stat_names = json_array_size(stat_names_json);
	const char *stat_names[num_stat_names];
	uint32_t port_ids[num_port_ids], stat_ids[num_stat_names];
	size_t index;
	json_t *value;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Invalid telemetry argument");
		return -1;
	}

	if (action != ACTION_GET) {
		TELEMETRY_LOG_WARN("Invalid action for this command");
		ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	if (!json_is_object(data)) {
		TELEMETRY_LOG_WARN("Invalid data provided for this command");
		ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	if (!json_is_array(port_ids_json) ||
		 !json_is_array(stat_names_json)) {
		TELEMETRY_LOG_WARN("Invalid input data array(s)");
		ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	json_array_foreach(port_ids_json, index, value) {
		if (!json_is_integer(value)) {
			TELEMETRY_LOG_WARN("Port ID given is not valid");
			ret = rte_telemetry_send_error_response(telemetry,
				-EINVAL);
			if (ret < 0)
				TELEMETRY_LOG_ERR("Could not send error");
			return -1;
		}
		port_ids[index] = json_integer_value(value);
		ret = rte_telemetry_is_port_active(port_ids[index]);
		if (ret < 1) {
			ret = rte_telemetry_send_error_response(telemetry,
				-EINVAL);
			if (ret < 0)
				TELEMETRY_LOG_ERR("Could not send error");
			return -1;
		}
	}

	json_array_foreach(stat_names_json, index, value) {
		if (!json_is_string(value)) {
			TELEMETRY_LOG_WARN("Stat Name given is not a string");

			ret = rte_telemetry_send_error_response(telemetry,
					-EINVAL);
			if (ret < 0)
				TELEMETRY_LOG_ERR("Could not send error");

			return -1;
		}
		stat_names[index] = json_string_value(value);
	}

	ret = rte_telemetry_stat_names_to_ids(telemetry, stat_names, stat_ids,
		num_stat_names);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not convert stat names to IDs");
		return -1;
	}

	ret = rte_telemetry_send_ports_stats_values(stat_ids, num_stat_names,
		port_ids, num_port_ids, telemetry);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Sending ports stats values failed");
		return -1;
	}

	return 0;
}

static int32_t
rte_telemetry_parse_command(struct telemetry_impl *telemetry, int action,
	const char *command, json_t *data)
{
	int ret;
	uint32_t i;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Invalid telemetry argument");
		return -1;
	}

	struct rte_telemetry_command commands[] = {
		{
			.text = "clients",
			.fn = &rte_telemetry_command_clients
		},
		{
			.text = "ports",
			.fn = &rte_telemetry_command_ports
		},
		{
			.text = "ports_details",
			.fn = &rte_telemetry_command_ports_details
		},
		{
			.text = "port_stats",
			.fn = &rte_telemetry_command_port_stats
		},
		{
			.text = "ports_stats_values_by_name",
			.fn = &rte_telemetry_command_ports_stats_values_by_name
		},
		{
			.text = "ports_all_stat_values",
			.fn = &rte_telemetry_command_ports_all_stat_values
		}
	};

	const uint32_t num_commands = RTE_DIM(commands);

	for (i = 0; i < num_commands; i++) {
		if (strcmp(command, commands[i].text) == 0) {
			ret = commands[i].fn(telemetry, action, data);
			if (ret < 0) {
				TELEMETRY_LOG_ERR("Command Function for %s failed",
					commands[i].text);
				return -1;
			}
			return 0;
		}
	}

	TELEMETRY_LOG_WARN("\"%s\" command not found", command);

	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");

	return -1;
}

int32_t __rte_experimental
rte_telemetry_parse(struct telemetry_impl *telemetry, char *socket_rx_data)
{
	int ret, action_int;
	json_error_t error;
	json_t *root, *action, *command, *data;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Invalid telemetry argument");
		return -1;
	}

	root = json_loads(socket_rx_data, 0, &error);
	if (root == NULL) {
		TELEMETRY_LOG_WARN("Could not load JSON object from data passed in : %s",
				error.text);
		ret = rte_telemetry_send_error_response(telemetry, -EPERM);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -EPERM;
	} else if (!json_is_object(root)) {
		TELEMETRY_LOG_WARN("JSON Request is not a JSON object");
		json_decref(root);
		goto einval_fail;
	}

	action = json_object_get(root, "action");
	if (action == NULL) {
		TELEMETRY_LOG_WARN("Request does not have action field");
		goto einval_fail;
	} else if (!json_is_integer(action)) {
		TELEMETRY_LOG_WARN("Action value is not an integer");
		goto einval_fail;
	}

	command = json_object_get(root, "command");
	if (command == NULL) {
		TELEMETRY_LOG_WARN("Request does not have command field");
		goto einval_fail;
	} else if (!json_is_string(command)) {
		TELEMETRY_LOG_WARN("Command value is not a string");
		goto einval_fail;
	}

	action_int = json_integer_value(action);
	if (action_int != ACTION_GET && action_int != ACTION_DELETE) {
		TELEMETRY_LOG_WARN("Invalid action code");
		goto einval_fail;
	}

	const char *command_string = json_string_value(command);
	data = json_object_get(root, "data");
	if (data == NULL) {
		TELEMETRY_LOG_WARN("Request does not have data field");
		goto einval_fail;
	}

	ret = rte_telemetry_parse_command(telemetry, action_int, command_string,
		data);
	if (ret < 0) {
		TELEMETRY_LOG_WARN("Could not parse command");
		return -EINVAL;
	}

	return 0;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not send error");
		return -EPERM;
	}
	return -EINVAL;
}
