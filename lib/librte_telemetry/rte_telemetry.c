/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <jansson.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_metrics.h>
#include <rte_option.h>
#include <rte_string_fns.h>

#include "rte_telemetry.h"
#include "rte_telemetry_internal.h"
#include "rte_telemetry_parser.h"
#include "rte_telemetry_parser_test.h"
#include "rte_telemetry_socket_tests.h"

#define BUF_SIZE 1024
#define ACTION_POST 1
#define SLEEP_TIME 10

#define SELFTEST_VALID_CLIENT "/var/run/dpdk/valid_client"
#define SELFTEST_INVALID_CLIENT "/var/run/dpdk/invalid_client"
#define SOCKET_TEST_CLIENT_PATH "/var/run/dpdk/client"

static telemetry_impl *static_telemetry;

struct telemetry_message_test {
	char *test_name;
	int (*test_func_ptr)(struct telemetry_impl *telemetry, int fd);
};

struct json_data {
	char *status_code;
	char *data;
	int port;
	char *stat_name;
	int stat_value;
};

static void
rte_telemetry_get_runtime_dir(char *socket_path, size_t size)
{
	snprintf(socket_path, size, "%s/telemetry", rte_eal_get_runtime_dir());
}

int32_t
rte_telemetry_is_port_active(int port_id)
{
	int ret;

	ret = rte_eth_find_next(port_id);
	if (ret == port_id)
		return 1;

	TELEMETRY_LOG_ERR("port_id: %d is invalid, not active",
		port_id);

	return 0;
}

static int32_t
rte_telemetry_update_metrics_ethdev(struct telemetry_impl *telemetry,
	uint16_t port_id, int reg_start_index)
{
	int ret, num_xstats, i;
	struct rte_eth_xstat *eth_xstats;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		TELEMETRY_LOG_ERR("port_id: %d is invalid", port_id);
		ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	ret = rte_telemetry_is_port_active(port_id);
	if (ret < 1) {
		ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	num_xstats = rte_eth_xstats_get(port_id, NULL, 0);
	if (num_xstats < 0) {
		TELEMETRY_LOG_ERR("rte_eth_xstats_get(%u) failed: %d", port_id,
				num_xstats);
		ret = rte_telemetry_send_error_response(telemetry, -EPERM);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	eth_xstats = malloc(sizeof(struct rte_eth_xstat) * num_xstats);
	if (eth_xstats == NULL) {
		TELEMETRY_LOG_ERR("Failed to malloc memory for xstats");
		ret = rte_telemetry_send_error_response(telemetry, -ENOMEM);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	ret = rte_eth_xstats_get(port_id, eth_xstats, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		free(eth_xstats);
		TELEMETRY_LOG_ERR("rte_eth_xstats_get(%u) len%i failed: %d",
				port_id, num_xstats, ret);
		ret = rte_telemetry_send_error_response(telemetry, -EPERM);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	uint64_t xstats_values[num_xstats];
	for (i = 0; i < num_xstats; i++)
		xstats_values[i] = eth_xstats[i].value;

	ret = rte_metrics_update_values(port_id, reg_start_index, xstats_values,
			num_xstats);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not update metrics values");
		ret = rte_telemetry_send_error_response(telemetry, -EPERM);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		free(eth_xstats);
		return -1;
	}

	free(eth_xstats);
	return 0;
}

int32_t
rte_telemetry_write_to_socket(struct telemetry_impl *telemetry,
	const char *json_string)
{
	int ret;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Could not initialise TELEMETRY_API");
		return -1;
	}

	if (telemetry->request_client == NULL) {
		TELEMETRY_LOG_ERR("No client has been chosen to write to");
		return -1;
	}

	if (json_string == NULL) {
		TELEMETRY_LOG_ERR("Invalid JSON string!");
		return -1;
	}

	ret = send(telemetry->request_client->fd,
			json_string, strlen(json_string), 0);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Failed to write to socket for client: %s",
				telemetry->request_client->file_path);
		return -1;
	}

	return 0;
}

int32_t
rte_telemetry_send_error_response(struct telemetry_impl *telemetry,
	int error_type)
{
	int ret;
	const char *status_code, *json_buffer;
	json_t *root;

	if (error_type == -EPERM)
		status_code = "Status Error: Unknown";
	else if (error_type == -EINVAL)
		status_code = "Status Error: Invalid Argument 404";
	else if (error_type == -ENOMEM)
		status_code = "Status Error: Memory Allocation Error";
	else {
		TELEMETRY_LOG_ERR("Invalid error type");
		return -EINVAL;
	}

	root = json_object();

	if (root == NULL) {
		TELEMETRY_LOG_ERR("Could not create root JSON object");
		return -EPERM;
	}

	ret = json_object_set_new(root, "status_code", json_string(status_code));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Status code field cannot be set");
		json_decref(root);
		return -EPERM;
	}

	ret = json_object_set_new(root, "data", json_null());
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Data field cannot be set");
		json_decref(root);
		return -EPERM;
	}

	json_buffer = json_dumps(root, 0);
	json_decref(root);

	ret = rte_telemetry_write_to_socket(telemetry, json_buffer);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not write to socket");
		return -EPERM;
	}

	return 0;
}

static int
rte_telemetry_get_metrics(struct telemetry_impl *telemetry, uint32_t port_id,
	struct rte_metric_value *metrics, struct rte_metric_name *names,
	int num_metrics)
{
	int ret, num_values;

	if (num_metrics < 0) {
		TELEMETRY_LOG_ERR("Invalid metrics count");
		goto einval_fail;
	} else if (num_metrics == 0) {
		TELEMETRY_LOG_ERR("No metrics to display (none have been registered)");
		goto eperm_fail;
	}

	if (metrics == NULL) {
		TELEMETRY_LOG_ERR("Metrics must be initialised.");
		goto einval_fail;
	}

	if (names == NULL) {
		TELEMETRY_LOG_ERR("Names must be initialised.");
		goto einval_fail;
	}

	ret = rte_metrics_get_names(names, num_metrics);
	if (ret < 0 || ret > num_metrics) {
		TELEMETRY_LOG_ERR("Cannot get metrics names");
		goto eperm_fail;
	}

	num_values = rte_metrics_get_values(port_id, NULL, 0);
	ret = rte_metrics_get_values(port_id, metrics, num_values);
	if (ret < 0 || ret > num_values) {
		TELEMETRY_LOG_ERR("Cannot get metrics values");
		goto eperm_fail;
	}

	return 0;

eperm_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EPERM);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;

}

static int32_t
rte_telemetry_json_format_stat(struct telemetry_impl *telemetry, json_t *stats,
	const char *metric_name, uint64_t metric_value)
{
	int ret;
	json_t *stat = json_object();

	if (stat == NULL) {
		TELEMETRY_LOG_ERR("Could not create stat JSON object");
		goto eperm_fail;
	}

	ret = json_object_set_new(stat, "name", json_string(metric_name));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Stat Name field cannot be set");
		goto eperm_fail;
	}

	ret = json_object_set_new(stat, "value", json_integer(metric_value));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Stat Value field cannot be set");
		goto eperm_fail;
	}

	ret = json_array_append_new(stats, stat);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Stat cannot be added to stats json array");
		goto eperm_fail;
	}

	return 0;

eperm_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EPERM);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;

}

static int32_t
rte_telemetry_json_format_port(struct telemetry_impl *telemetry,
	uint32_t port_id, json_t *ports, uint32_t *metric_ids,
	uint32_t num_metric_ids)
{
	struct rte_metric_value *metrics = 0;
	struct rte_metric_name *names = 0;
	int num_metrics, ret, err_ret;
	json_t *port, *stats;
	uint32_t i;

	num_metrics = rte_metrics_get_names(NULL, 0);
	if (num_metrics < 0) {
		TELEMETRY_LOG_ERR("Cannot get metrics count");
		goto einval_fail;
	} else if (num_metrics == 0) {
		TELEMETRY_LOG_ERR("No metrics to display (none have been registered)");
		goto eperm_fail;
	}

	metrics = malloc(sizeof(struct rte_metric_value) * num_metrics);
	names = malloc(sizeof(struct rte_metric_name) * num_metrics);
	if (metrics == NULL || names == NULL) {
		TELEMETRY_LOG_ERR("Cannot allocate memory");
		free(metrics);
		free(names);

		err_ret = rte_telemetry_send_error_response(telemetry, -ENOMEM);
		if (err_ret < 0)
			TELEMETRY_LOG_ERR("Could not send error");
		return -1;
	}

	ret  = rte_telemetry_get_metrics(telemetry, port_id, metrics, names,
		num_metrics);
	if (ret < 0) {
		free(metrics);
		free(names);
		TELEMETRY_LOG_ERR("rte_telemetry_get_metrics failed");
		return -1;
	}

	port = json_object();
	stats = json_array();
	if (port == NULL || stats == NULL) {
		TELEMETRY_LOG_ERR("Could not create port/stats JSON objects");
		goto eperm_fail;
	}

	ret = json_object_set_new(port, "port", json_integer(port_id));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Port field cannot be set");
		goto eperm_fail;
	}

	for (i = 0; i < num_metric_ids; i++) {
		int metric_id = metric_ids[i];
		int metric_index = -1;
		int metric_name_key = -1;
		int32_t j;
		uint64_t metric_value;

		if (metric_id >= num_metrics) {
			TELEMETRY_LOG_ERR("Metric_id: %d is not valid",
					metric_id);
			goto einval_fail;
		}

		for (j = 0; j < num_metrics; j++) {
			if (metrics[j].key == metric_id) {
				metric_name_key = metrics[j].key;
				metric_index = j;
				break;
			}
		}

		const char *metric_name = names[metric_name_key].name;
		metric_value = metrics[metric_index].value;

		if (metric_name_key < 0 || metric_index < 0) {
			TELEMETRY_LOG_ERR("Could not get metric name/index");
			goto eperm_fail;
		}

		ret = rte_telemetry_json_format_stat(telemetry, stats,
			metric_name, metric_value);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Format stat with id: %u failed",
					metric_id);
			free(metrics);
			free(names);
			return -1;
		}
	}

	if (json_array_size(stats) == 0)
		ret = json_object_set_new(port, "stats", json_null());
	else
		ret = json_object_set_new(port, "stats", stats);

	if (ret < 0) {
		TELEMETRY_LOG_ERR("Stats object cannot be set");
		goto eperm_fail;
	}

	ret = json_array_append_new(ports, port);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Port object cannot be added to ports array");
		goto eperm_fail;
	}

	free(metrics);
	free(names);
	return 0;

eperm_fail:
	free(metrics);
	free(names);
	ret = rte_telemetry_send_error_response(telemetry, -EPERM);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;

einval_fail:
	free(metrics);
	free(names);
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;
}

static int32_t
rte_telemetry_encode_json_format(struct telemetry_impl *telemetry,
	uint32_t *port_ids, uint32_t num_port_ids, uint32_t *metric_ids,
	uint32_t num_metric_ids, char **json_buffer)
{
	int ret;
	json_t *root, *ports;
	uint32_t i;

	if (num_port_ids <= 0 || num_metric_ids <= 0) {
		TELEMETRY_LOG_ERR("Please provide port and metric ids to query");
		goto einval_fail;
	}

	ports = json_array();
	if (ports == NULL) {
		TELEMETRY_LOG_ERR("Could not create ports JSON array");
		goto eperm_fail;
	}

	for (i = 0; i < num_port_ids; i++) {
		if (!rte_eth_dev_is_valid_port(port_ids[i])) {
			TELEMETRY_LOG_ERR("Port: %d invalid", port_ids[i]);
			goto einval_fail;
		}
	}

	for (i = 0; i < num_port_ids; i++) {
		ret = rte_telemetry_json_format_port(telemetry, port_ids[i],
			ports, metric_ids, num_metric_ids);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Format port in JSON failed");
			return -1;
		}
	}

	root = json_object();
	if (root == NULL) {
		TELEMETRY_LOG_ERR("Could not create root JSON object");
		goto eperm_fail;
	}

	ret = json_object_set_new(root, "status_code",
		json_string("Status OK: 200"));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Status code field cannot be set");
		goto eperm_fail;
	}

	ret = json_object_set_new(root, "data", ports);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Data field cannot be set");
		goto eperm_fail;
	}

	*json_buffer = json_dumps(root, JSON_INDENT(2));
	json_decref(root);
	return 0;

eperm_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EPERM);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;
}

int32_t
rte_telemetry_send_ports_stats_values(uint32_t *metric_ids, int num_metric_ids,
	uint32_t *port_ids, int num_port_ids, struct telemetry_impl *telemetry)
{
	int ret, i;
	char *json_buffer = NULL;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Invalid telemetry argument");
		return -1;
	}

	if (metric_ids == NULL) {
		TELEMETRY_LOG_ERR("Invalid metric_ids array");
		goto einval_fail;
	}

	if (num_metric_ids < 0) {
		TELEMETRY_LOG_ERR("Invalid num_metric_ids, must be positive");
		goto einval_fail;
	}

	if (port_ids == NULL) {
		TELEMETRY_LOG_ERR("Invalid port_ids array");
		goto einval_fail;
	}

	if (num_port_ids < 0) {
		TELEMETRY_LOG_ERR("Invalid num_port_ids, must be positive");
		goto einval_fail;
	}

	for (i = 0; i < num_port_ids; i++) {
		if (!rte_eth_dev_is_valid_port(port_ids[i])) {
			TELEMETRY_LOG_ERR("Port: %d invalid", port_ids[i]);
			goto einval_fail;
		}

		ret = rte_telemetry_update_metrics_ethdev(telemetry,
				port_ids[i], telemetry->reg_index[i]);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Failed to update ethdev metrics");
			return -1;
		}
	}

	ret = rte_telemetry_encode_json_format(telemetry, port_ids,
		num_port_ids, metric_ids, num_metric_ids, &json_buffer);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("JSON encode function failed");
		return -1;
	}

	ret = rte_telemetry_write_to_socket(telemetry, json_buffer);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not write to socket");
		return -1;
	}

	return 0;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -1;
}


static int32_t
rte_telemetry_reg_ethdev_to_metrics(uint16_t port_id)
{
	int ret, num_xstats, ret_val, i;
	struct rte_eth_xstat *eth_xstats = NULL;
	struct rte_eth_xstat_name *eth_xstats_names = NULL;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		TELEMETRY_LOG_ERR("port_id: %d is invalid", port_id);
		return -EINVAL;
	}

	num_xstats = rte_eth_xstats_get(port_id, NULL, 0);
	if (num_xstats < 0) {
		TELEMETRY_LOG_ERR("rte_eth_xstats_get(%u) failed: %d",
				port_id, num_xstats);
		return -EPERM;
	}

	eth_xstats = malloc(sizeof(struct rte_eth_xstat) * num_xstats);
	if (eth_xstats == NULL) {
		TELEMETRY_LOG_ERR("Failed to malloc memory for xstats");
		return -ENOMEM;
	}

	ret = rte_eth_xstats_get(port_id, eth_xstats, num_xstats);
	const char *xstats_names[num_xstats];
	eth_xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * num_xstats);
	if (ret < 0 || ret > num_xstats) {
		TELEMETRY_LOG_ERR("rte_eth_xstats_get(%u) len%i failed: %d",
				port_id, num_xstats, ret);
		ret_val = -EPERM;
		goto free_xstats;
	}

	if (eth_xstats_names == NULL) {
		TELEMETRY_LOG_ERR("Failed to malloc memory for xstats_names");
		ret_val = -ENOMEM;
		goto free_xstats;
	}

	ret = rte_eth_xstats_get_names(port_id, eth_xstats_names, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		TELEMETRY_LOG_ERR("rte_eth_xstats_get_names(%u) len%i failed: %d",
				port_id, num_xstats, ret);
		ret_val = -EPERM;
		goto free_xstats;
	}

	for (i = 0; i < num_xstats; i++)
		xstats_names[i] = eth_xstats_names[eth_xstats[i].id].name;

	ret_val = rte_metrics_reg_names(xstats_names, num_xstats);
	if (ret_val < 0) {
		TELEMETRY_LOG_ERR("rte_metrics_reg_names failed - metrics may already be registered");
		ret_val = -1;
		goto free_xstats;
	}

	goto free_xstats;

free_xstats:
	free(eth_xstats);
	free(eth_xstats_names);
	return ret_val;
}

static int32_t
rte_telemetry_initial_accept(struct telemetry_impl *telemetry)
{
	struct driver_index {
		const void *dev_ops;
		int reg_index;
	} drv_idx[RTE_MAX_ETHPORTS];
	int nb_drv_idx = 0;
	uint16_t pid;
	int ret;
	int selftest = 0;

	RTE_ETH_FOREACH_DEV(pid) {
		int i;
		/* Different device types have different numbers of stats, so
		 * first check if the stats for this type of device have
		 * already been registered
		 */
		for (i = 0; i < nb_drv_idx; i++) {
			if (rte_eth_devices[pid].dev_ops == drv_idx[i].dev_ops) {
				telemetry->reg_index[pid] = drv_idx[i].reg_index;
				break;
			}
		}
		if (i < nb_drv_idx)
			continue; /* we found a match, go to next port */

		/* No match, register a new set of xstats for this port */
		ret = rte_telemetry_reg_ethdev_to_metrics(pid);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Failed to register ethdev metrics");
			return -1;
		}
		telemetry->reg_index[pid] = ret;
		drv_idx[nb_drv_idx].dev_ops = rte_eth_devices[pid].dev_ops;
		drv_idx[nb_drv_idx].reg_index = ret;
		nb_drv_idx++;
	}

	telemetry->metrics_register_done = 1;
	if (selftest) {
		ret = rte_telemetry_socket_messaging_testing(telemetry->reg_index[0],
				telemetry->server_fd);
		if (ret < 0)
			return -1;

		ret = rte_telemetry_parser_test(telemetry);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Parser Tests Failed");
			return -1;
		}

		TELEMETRY_LOG_INFO("Success - All Parser Tests Passed");
	}

	return 0;
}

static int32_t
rte_telemetry_read_client(struct telemetry_impl *telemetry)
{
	char buf[BUF_SIZE];
	int ret, buffer_read;

	buffer_read = read(telemetry->accept_fd, buf, BUF_SIZE-1);

	if (buffer_read == -1) {
		TELEMETRY_LOG_ERR("Read error");
		return -1;
	} else if (buffer_read == 0) {
		goto close_socket;
	} else {
		buf[buffer_read] = '\0';
		ret = rte_telemetry_parse_client_message(telemetry, buf);
		if (ret < 0)
			TELEMETRY_LOG_WARN("Parse message failed");
		goto close_socket;
	}

close_socket:
	if (close(telemetry->accept_fd) < 0) {
		TELEMETRY_LOG_ERR("Close TELEMETRY socket failed");
		free(telemetry);
		return -EPERM;
	}
	telemetry->accept_fd = 0;

	return 0;
}

static int32_t
rte_telemetry_accept_new_client(struct telemetry_impl *telemetry)
{
	int ret;

	if (telemetry->accept_fd <= 0) {
		ret = listen(telemetry->server_fd, 1);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Listening error with server fd");
			return -1;
		}

		telemetry->accept_fd = accept(telemetry->server_fd, NULL, NULL);
		if (telemetry->accept_fd >= 0 &&
			telemetry->metrics_register_done == 0) {
			ret = rte_telemetry_initial_accept(telemetry);
			if (ret < 0) {
				TELEMETRY_LOG_ERR("Failed to run initial configurations/tests");
				return -1;
			}
		}
	} else {
		ret = rte_telemetry_read_client(telemetry);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Failed to read socket buffer");
			return -1;
		}
	}

	return 0;
}

static int32_t
rte_telemetry_read_client_sockets(struct telemetry_impl *telemetry)
{
	int ret;
	telemetry_client *client;
	char client_buf[BUF_SIZE];
	int bytes;

	TAILQ_FOREACH(client, &telemetry->client_list_head, client_list) {
		bytes = read(client->fd, client_buf, BUF_SIZE-1);

		if (bytes > 0) {
			client_buf[bytes] = '\0';
			telemetry->request_client = client;
			ret = rte_telemetry_parse(telemetry, client_buf);
			if (ret < 0) {
				TELEMETRY_LOG_WARN("Parse socket input failed: %i",
						ret);
				return -1;
			}
		}
	}

	return 0;
}

static int32_t
rte_telemetry_run(void *userdata)
{
	int ret;
	struct telemetry_impl *telemetry = userdata;

	if (telemetry == NULL) {
		TELEMETRY_LOG_WARN("TELEMETRY could not be initialised");
		return -1;
	}

	ret = rte_telemetry_accept_new_client(telemetry);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Accept and read new client failed");
		return -1;
	}

	ret = rte_telemetry_read_client_sockets(telemetry);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Client socket read failed");
		return -1;
	}

	return 0;
}

static void
*rte_telemetry_run_thread_func(void *userdata)
{
	int ret;
	struct telemetry_impl *telemetry = userdata;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("%s passed a NULL instance", __func__);
		pthread_exit(0);
	}

	while (telemetry->thread_status) {
		rte_telemetry_run(telemetry);
		ret = usleep(SLEEP_TIME);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Calling thread could not be put to sleep");
	}
	pthread_exit(0);
}

static int32_t
rte_telemetry_set_socket_nonblock(int fd)
{
	int flags;

	if (fd < 0) {
		TELEMETRY_LOG_ERR("Invalid fd provided");
		return -1;
	}

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		flags = 0;

	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int32_t
rte_telemetry_create_socket(struct telemetry_impl *telemetry)
{
	int ret;
	struct sockaddr_un addr;
	char socket_path[BUF_SIZE];

	if (telemetry == NULL)
		return -1;

	telemetry->server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (telemetry->server_fd == -1) {
		TELEMETRY_LOG_ERR("Failed to open socket");
		return -1;
	}

	ret  = rte_telemetry_set_socket_nonblock(telemetry->server_fd);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not set socket to NONBLOCK");
		goto close_socket;
	}

	addr.sun_family = AF_UNIX;
	rte_telemetry_get_runtime_dir(socket_path, sizeof(socket_path));
	strlcpy(addr.sun_path, socket_path, sizeof(addr.sun_path));
	unlink(socket_path);

	if (bind(telemetry->server_fd, (struct sockaddr *)&addr,
		sizeof(addr)) < 0) {
		TELEMETRY_LOG_ERR("Socket binding error");
		goto close_socket;
	}

	return 0;

close_socket:
	if (close(telemetry->server_fd) < 0) {
		TELEMETRY_LOG_ERR("Close TELEMETRY socket failed");
		return -EPERM;
	}

	return -1;
}

int32_t __rte_experimental
rte_telemetry_init()
{
	int ret;
	pthread_attr_t attr;
	const char *telemetry_ctrl_thread = "telemetry";

	if (static_telemetry) {
		TELEMETRY_LOG_WARN("TELEMETRY structure already initialised");
		return -EALREADY;
	}

	static_telemetry = calloc(1, sizeof(struct telemetry_impl));
	if (static_telemetry == NULL) {
		TELEMETRY_LOG_ERR("Memory could not be allocated");
		return -ENOMEM;
	}

	static_telemetry->socket_id = rte_socket_id();
	rte_metrics_init(static_telemetry->socket_id);

	ret = pthread_attr_init(&attr);
	if (ret != 0) {
		TELEMETRY_LOG_ERR("Pthread attribute init failed");
		return -EPERM;
	}

	ret = rte_telemetry_create_socket(static_telemetry);
	if (ret < 0) {
		ret = rte_telemetry_cleanup();
		if (ret < 0)
			TELEMETRY_LOG_ERR("TELEMETRY cleanup failed");
		return -EPERM;
	}
	TAILQ_INIT(&static_telemetry->client_list_head);

	ret = rte_ctrl_thread_create(&static_telemetry->thread_id,
		telemetry_ctrl_thread, &attr, rte_telemetry_run_thread_func,
		(void *)static_telemetry);
	static_telemetry->thread_status = 1;

	if (ret < 0) {
		ret = rte_telemetry_cleanup();
		if (ret < 0)
			TELEMETRY_LOG_ERR("TELEMETRY cleanup failed");
		return -EPERM;
	}

	return 0;
}

static int32_t
rte_telemetry_client_cleanup(struct telemetry_client *client)
{
	int ret;

	ret = close(client->fd);
	free(client->file_path);
	free(client);

	if (ret < 0) {
		TELEMETRY_LOG_ERR("Close client socket failed");
		return -EPERM;
	}

	return 0;
}

int32_t __rte_experimental
rte_telemetry_cleanup(void)
{
	int ret;
	struct telemetry_impl *telemetry = static_telemetry;
	telemetry_client *client, *temp_client;

	TAILQ_FOREACH_SAFE(client, &telemetry->client_list_head, client_list,
		temp_client) {
		TAILQ_REMOVE(&telemetry->client_list_head, client, client_list);
		ret = rte_telemetry_client_cleanup(client);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Client cleanup failed");
			return -EPERM;
		}
	}

	ret = close(telemetry->server_fd);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Close TELEMETRY socket failed");
		free(telemetry);
		return -EPERM;
	}

	telemetry->thread_status = 0;
	pthread_join(telemetry->thread_id, NULL);
	free(telemetry);
	static_telemetry = NULL;

	return 0;
}

int32_t
rte_telemetry_unregister_client(struct telemetry_impl *telemetry,
	const char *client_path)
{
	int ret;
	telemetry_client *client, *temp_client;

	if (telemetry == NULL) {
		TELEMETRY_LOG_WARN("TELEMETRY is not initialised");
		return -ENODEV;
	}

	if (client_path == NULL) {
		TELEMETRY_LOG_ERR("Invalid client path");
		goto einval_fail;
	}

	if (TAILQ_EMPTY(&telemetry->client_list_head)) {
		TELEMETRY_LOG_ERR("There are no clients currently registered");
		return -EPERM;
	}

	TAILQ_FOREACH_SAFE(client, &telemetry->client_list_head, client_list,
			temp_client) {
		if (strcmp(client_path, client->file_path) == 0) {
			TAILQ_REMOVE(&telemetry->client_list_head, client,
				client_list);
			ret = rte_telemetry_client_cleanup(client);

			if (ret < 0) {
				TELEMETRY_LOG_ERR("Client cleanup failed");
				return -EPERM;
			}

			return 0;
		}
	}

	TELEMETRY_LOG_WARN("Couldn't find client, possibly not registered yet.");
	return -1;

einval_fail:
	ret = rte_telemetry_send_error_response(telemetry, -EINVAL);
	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not send error");
	return -EINVAL;
}

int32_t
rte_telemetry_register_client(struct telemetry_impl *telemetry,
	const char *client_path)
{
	int ret, fd;
	struct sockaddr_un addrs;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Could not initialize TELEMETRY API");
		return -ENODEV;
	}

	if (client_path == NULL) {
		TELEMETRY_LOG_ERR("Invalid client path");
		return -EINVAL;
	}

	telemetry_client *client;
	TAILQ_FOREACH(client, &telemetry->client_list_head, client_list) {
		if (strcmp(client_path, client->file_path) == 0) {
			TELEMETRY_LOG_WARN("'%s' already registered",
					client_path);
			return -EINVAL;
		}
	}

	fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (fd == -1) {
		TELEMETRY_LOG_ERR("Client socket error");
		return -EACCES;
	}

	ret = rte_telemetry_set_socket_nonblock(fd);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not set socket to NONBLOCK");
		return -EPERM;
	}

	addrs.sun_family = AF_UNIX;
	strlcpy(addrs.sun_path, client_path, sizeof(addrs.sun_path));
	telemetry_client *new_client = malloc(sizeof(telemetry_client));
	new_client->file_path = strdup(client_path);
	new_client->fd = fd;

	if (connect(fd, (struct sockaddr *)&addrs, sizeof(addrs)) == -1) {
		TELEMETRY_LOG_ERR("TELEMETRY client connect to %s didn't work",
				client_path);
		ret = rte_telemetry_client_cleanup(new_client);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("Client cleanup failed");
			return -EPERM;
		}
		return -EINVAL;
	}

	TAILQ_INSERT_HEAD(&telemetry->client_list_head, new_client, client_list);

	return 0;
}

int32_t
rte_telemetry_parse_client_message(struct telemetry_impl *telemetry, char *buf)
{
	int ret, action_int;
	json_error_t error;
	json_t *root = json_loads(buf, 0, &error);

	if (root == NULL) {
		TELEMETRY_LOG_WARN("Could not load JSON object from data passed in : %s",
				error.text);
		goto fail;
	} else if (!json_is_object(root)) {
		TELEMETRY_LOG_WARN("JSON Request is not a JSON object");
		goto fail;
	}

	json_t *action = json_object_get(root, "action");
	if (action == NULL) {
		TELEMETRY_LOG_WARN("Request does not have action field");
		goto fail;
	} else if (!json_is_integer(action)) {
		TELEMETRY_LOG_WARN("Action value is not an integer");
		goto fail;
	}

	json_t *command = json_object_get(root, "command");
	if (command == NULL) {
		TELEMETRY_LOG_WARN("Request does not have command field");
		goto fail;
	} else if (!json_is_string(command)) {
		TELEMETRY_LOG_WARN("Command value is not a string");
		goto fail;
	}

	action_int = json_integer_value(action);
	if (action_int != ACTION_POST) {
		TELEMETRY_LOG_WARN("Invalid action code");
		goto fail;
	}

	if (strcmp(json_string_value(command), "clients") != 0) {
		TELEMETRY_LOG_WARN("Invalid command");
		goto fail;
	}

	json_t *data = json_object_get(root, "data");
	if (data == NULL) {
		TELEMETRY_LOG_WARN("Request does not have data field");
		goto fail;
	}

	json_t *client_path = json_object_get(data, "client_path");
	if (client_path == NULL) {
		TELEMETRY_LOG_WARN("Request does not have client_path field");
		goto fail;
	}

	if (!json_is_string(client_path)) {
		TELEMETRY_LOG_WARN("Client_path value is not a string");
		goto fail;
	}

	ret = rte_telemetry_register_client(telemetry,
			json_string_value(client_path));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not register client");
		telemetry->register_fail_count++;
		goto fail;
	}

	return 0;

fail:
	TELEMETRY_LOG_WARN("Client attempted to register with invalid message");
	json_decref(root);
	return -1;
}

int32_t
rte_telemetry_dummy_client_socket(const char *valid_client_path)
{
	int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	struct sockaddr_un addr = {0};

	if (sockfd < 0) {
		TELEMETRY_LOG_ERR("Test socket creation failure");
		return -1;
	}

	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, valid_client_path, sizeof(addr.sun_path));
	unlink(valid_client_path);

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		TELEMETRY_LOG_ERR("Test socket binding failure");
		return -1;
	}

	if (listen(sockfd, 1) < 0) {
		TELEMETRY_LOG_ERR("Listen failure");
		return -1;
	}

	return sockfd;
}

int32_t __rte_experimental
rte_telemetry_selftest(void)
{
	const char *invalid_client_path = SELFTEST_INVALID_CLIENT;
	const char *valid_client_path = SELFTEST_VALID_CLIENT;
	int ret, sockfd;

	TELEMETRY_LOG_INFO("Selftest");

	ret = rte_telemetry_init();
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Valid initialisation test failed");
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - Valid initialisation test passed");

	ret = rte_telemetry_init();
	if (ret != -EALREADY) {
		TELEMETRY_LOG_ERR("Invalid initialisation test failed");
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - Invalid initialisation test passed");

	ret = rte_telemetry_unregister_client(static_telemetry,
			invalid_client_path);
	if (ret != -EPERM) {
		TELEMETRY_LOG_ERR("Invalid unregister test failed");
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - Invalid unregister test passed");

	sockfd = rte_telemetry_dummy_client_socket(valid_client_path);
	if (sockfd < 0) {
		TELEMETRY_LOG_ERR("Test socket creation failed");
		return -1;
	}

	ret = rte_telemetry_register_client(static_telemetry, valid_client_path);
	if (ret != 0) {
		TELEMETRY_LOG_ERR("Valid register test failed: %i", ret);
		return -1;
	}

	accept(sockfd, NULL, NULL);
	TELEMETRY_LOG_INFO("Success - Valid register test passed");

	ret = rte_telemetry_register_client(static_telemetry, valid_client_path);
	if (ret != -EINVAL) {
		TELEMETRY_LOG_ERR("Invalid register test failed: %i", ret);
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - Invalid register test passed");

	ret = rte_telemetry_unregister_client(static_telemetry,
		invalid_client_path);
	if (ret != -1) {
		TELEMETRY_LOG_ERR("Invalid unregister test failed: %i", ret);
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - Invalid unregister test passed");

	ret = rte_telemetry_unregister_client(static_telemetry, valid_client_path);
	if (ret != 0) {
		TELEMETRY_LOG_ERR("Valid unregister test failed: %i", ret);
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - Valid unregister test passed");

	ret = rte_telemetry_cleanup();
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Cleanup test failed");
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - Valid cleanup test passed");

	return 0;
}

int32_t
rte_telemetry_socket_messaging_testing(int index, int socket)
{
	struct telemetry_impl *telemetry = calloc(1, sizeof(telemetry_impl));
	int fd, bad_send_fd, send_fd, bad_fd, bad_recv_fd, recv_fd, ret;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("Could not initialize Telemetry API");
		return -1;
	}

	telemetry->server_fd = socket;
	telemetry->reg_index[0] = index;
	TELEMETRY_LOG_INFO("Beginning Telemetry socket message Selftest");
	rte_telemetry_socket_test_setup(telemetry, &send_fd, &recv_fd);
	TELEMETRY_LOG_INFO("Register valid client test");

	ret = rte_telemetry_socket_register_test(telemetry, &fd, send_fd,
		recv_fd);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Register valid client test failed!");
		free(telemetry);
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - Register valid client test passed!");

	TELEMETRY_LOG_INFO("Register invalid/same client test");
	ret = rte_telemetry_socket_test_setup(telemetry, &bad_send_fd,
		&bad_recv_fd);
	ret = rte_telemetry_socket_register_test(telemetry, &bad_fd,
		bad_send_fd, bad_recv_fd);
	if (!ret) {
		TELEMETRY_LOG_ERR("Register invalid/same client test failed!");
		free(telemetry);
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - Register invalid/same client test passed!");

	ret = rte_telemetry_json_socket_message_test(telemetry, fd);
	if (ret < 0) {
		free(telemetry);
		return -1;
	}

	free(telemetry);
	return 0;
}

int32_t
rte_telemetry_socket_register_test(struct telemetry_impl *telemetry, int *fd,
	int send_fd, int recv_fd)
{
	int ret;
	char good_req_string[BUF_SIZE];

	snprintf(good_req_string, sizeof(good_req_string),
	"{\"action\":1,\"command\":\"clients\",\"data\":{\"client_path\""
		":\"%s\"}}", SOCKET_TEST_CLIENT_PATH);

	listen(recv_fd, 1);

	ret = send(send_fd, good_req_string, strlen(good_req_string), 0);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not send message over socket");
		return -1;
	}

	rte_telemetry_run(telemetry);

	if (telemetry->register_fail_count != 0)
		return -1;

	*fd = accept(recv_fd, NULL, NULL);

	return 0;
}

int32_t
rte_telemetry_socket_test_setup(struct telemetry_impl *telemetry, int *send_fd,
	int *recv_fd)
{
	int ret;
	const char *client_path = SOCKET_TEST_CLIENT_PATH;
	char socket_path[BUF_SIZE];
	struct sockaddr_un addr = {0};
	struct sockaddr_un addrs = {0};
	*send_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	*recv_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	listen(telemetry->server_fd, 5);
	addr.sun_family = AF_UNIX;
	rte_telemetry_get_runtime_dir(socket_path, sizeof(socket_path));
	strlcpy(addr.sun_path, socket_path, sizeof(addr.sun_path));

	ret = connect(*send_fd, (struct sockaddr *) &addr, sizeof(addr));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not connect socket");
		return -1;
	}

	telemetry->accept_fd = accept(telemetry->server_fd, NULL, NULL);

	addrs.sun_family = AF_UNIX;
	strlcpy(addrs.sun_path, client_path, sizeof(addrs.sun_path));
	unlink(client_path);

	ret = bind(*recv_fd, (struct sockaddr *)&addrs, sizeof(addrs));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not bind socket");
		return -1;
	}

	return 0;
}

static int32_t
rte_telemetry_stat_parse(char *buf, struct json_data *json_data_struct)
{
	json_error_t error;
	json_t *root = json_loads(buf, 0, &error);
	int arraylen, i;
	json_t *status, *dataArray, *port, *stats, *name, *value, *dataArrayObj,
	       *statsArrayObj;

	stats = NULL;
	port = NULL;
	name = NULL;

	if (buf == NULL) {
		TELEMETRY_LOG_ERR("JSON message is NULL");
		return -EINVAL;
	}

	if (root == NULL) {
		TELEMETRY_LOG_ERR("Could not load JSON object from data passed in : %s",
				error.text);
		return -EPERM;
	} else if (!json_is_object(root)) {
		TELEMETRY_LOG_ERR("JSON Request is not a JSON object");
		json_decref(root);
		return -EINVAL;
	}

	status = json_object_get(root, "status_code");
	if (!status) {
		TELEMETRY_LOG_ERR("Request does not have status field");
		return -EINVAL;
	} else if (!json_is_string(status)) {
		TELEMETRY_LOG_ERR("Status value is not a string");
		return -EINVAL;
	}

	json_data_struct->status_code = strdup(json_string_value(status));

	dataArray = json_object_get(root, "data");
	if (dataArray == NULL) {
		TELEMETRY_LOG_ERR("Request does not have data field");
		return -EINVAL;
	}

	arraylen = json_array_size(dataArray);
	if (arraylen == 0) {
		json_data_struct->data = "null";
		return -EINVAL;
	}

	for (i = 0; i < arraylen; i++) {
		dataArrayObj = json_array_get(dataArray, i);
		port = json_object_get(dataArrayObj, "port");
		stats = json_object_get(dataArrayObj, "stats");
	}

	if (port == NULL) {
		TELEMETRY_LOG_ERR("Request does not have port field");
		return -EINVAL;
	}

	if (!json_is_integer(port)) {
		TELEMETRY_LOG_ERR("Port value is not an integer");
		return -EINVAL;
	}

	json_data_struct->port = json_integer_value(port);

	if (stats == NULL) {
		TELEMETRY_LOG_ERR("Request does not have stats field");
		return -EINVAL;
	}

	arraylen = json_array_size(stats);
	for (i = 0; i < arraylen; i++) {
		statsArrayObj = json_array_get(stats, i);
		name = json_object_get(statsArrayObj, "name");
		value = json_object_get(statsArrayObj, "value");
	}

	if (name == NULL) {
		TELEMETRY_LOG_ERR("Request does not have name field");
		return -EINVAL;
	}

	if (!json_is_string(name)) {
		TELEMETRY_LOG_ERR("Stat name value is not a string");
		return -EINVAL;
	}

	json_data_struct->stat_name = strdup(json_string_value(name));

	if (value == NULL) {
		TELEMETRY_LOG_ERR("Request does not have value field");
		return -EINVAL;
	}

	if (!json_is_integer(value)) {
		TELEMETRY_LOG_ERR("Stat value is not an integer");
		return -EINVAL;
	}

	json_data_struct->stat_value = json_integer_value(value);

	return 0;
}

static void
rte_telemetry_free_test_data(struct json_data *data)
{
	free(data->status_code);
	free(data->stat_name);
	free(data);
}

int32_t
rte_telemetry_valid_json_test(struct telemetry_impl *telemetry, int fd)
{
	int ret;
	int port = 0;
	int value = 0;
	int fail_count = 0;
	int buffer_read = 0;
	char buf[BUF_SIZE];
	struct json_data *data_struct;
	errno = 0;
	const char *status = "Status OK: 200";
	const char *name = "rx_good_packets";
	const char *valid_json_message = "{\"action\":0,\"command\":"
	"\"ports_stats_values_by_name\",\"data\":{\"ports\""
	":[0],\"stats\":[\"rx_good_packets\"]}}";

	ret = send(fd, valid_json_message, strlen(valid_json_message), 0);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not send message over socket");
		return -1;
	}

	rte_telemetry_run(telemetry);
	buffer_read = recv(fd, buf, BUF_SIZE-1, 0);

	if (buffer_read == -1) {
		TELEMETRY_LOG_ERR("Read error");
		return -1;
	}

	buf[buffer_read] = '\0';
	data_struct = calloc(1, sizeof(struct json_data));
	ret = rte_telemetry_stat_parse(buf, data_struct);

	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not parse stats");
		fail_count++;
	}

	if (strcmp(data_struct->status_code, status) != 0) {
		TELEMETRY_LOG_ERR("Status code is invalid");
		fail_count++;
	}

	if (data_struct->port != port) {
		TELEMETRY_LOG_ERR("Port is invalid");
		fail_count++;
	}

	if (strcmp(data_struct->stat_name, name) != 0) {
		TELEMETRY_LOG_ERR("Stat name is invalid");
		fail_count++;
	}

	if (data_struct->stat_value != value) {
		TELEMETRY_LOG_ERR("Stat value is invalid");
		fail_count++;
	}

	rte_telemetry_free_test_data(data_struct);
	if (fail_count > 0)
		return -1;

	TELEMETRY_LOG_INFO("Success - Passed valid JSON message test passed");

	return 0;
}

int32_t
rte_telemetry_invalid_json_test(struct telemetry_impl *telemetry, int fd)
{
	int ret;
	char buf[BUF_SIZE];
	int fail_count = 0;
	const char *invalid_json = "{]";
	const char *status = "Status Error: Unknown";
	const char *data = "null";
	struct json_data *data_struct;
	int buffer_read = 0;
	errno = 0;

	ret = send(fd, invalid_json, strlen(invalid_json), 0);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not send message over socket");
		return -1;
	}

	rte_telemetry_run(telemetry);
	buffer_read = recv(fd, buf, BUF_SIZE-1, 0);

	if (buffer_read == -1) {
		TELEMETRY_LOG_ERR("Read error");
		return -1;
	}

	buf[buffer_read] = '\0';

	data_struct = calloc(1, sizeof(struct json_data));
	ret = rte_telemetry_stat_parse(buf, data_struct);

	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not parse stats");

	if (strcmp(data_struct->status_code, status) != 0) {
		TELEMETRY_LOG_ERR("Status code is invalid");
		fail_count++;
	}

	if (strcmp(data_struct->data, data) != 0) {
		TELEMETRY_LOG_ERR("Data status is invalid");
		fail_count++;
	}

	rte_telemetry_free_test_data(data_struct);
	if (fail_count > 0)
		return -1;

	TELEMETRY_LOG_INFO("Success - Passed invalid JSON message test");

	return 0;
}

int32_t
rte_telemetry_json_contents_test(struct telemetry_impl *telemetry, int fd)
{
	int ret;
	char buf[BUF_SIZE];
	int fail_count = 0;
	char *status = "Status Error: Invalid Argument 404";
	char *data = "null";
	struct json_data *data_struct;
	const char *invalid_contents = "{\"action\":0,\"command\":"
	"\"ports_stats_values_by_name\",\"data\":{\"ports\""
	":[0],\"stats\":[\"some_invalid_param\","
	"\"another_invalid_param\"]}}";
	int buffer_read = 0;
	errno = 0;

	ret = send(fd, invalid_contents, strlen(invalid_contents), 0);
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not send message over socket");
		return -1;
	}

	rte_telemetry_run(telemetry);
	buffer_read = recv(fd, buf, BUF_SIZE-1, 0);

	if (buffer_read == -1) {
		TELEMETRY_LOG_ERR("Read error");
		return -1;
	}

	buf[buffer_read] = '\0';
	data_struct = calloc(1, sizeof(struct json_data));
	ret = rte_telemetry_stat_parse(buf, data_struct);

	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not parse stats");

	if (strcmp(data_struct->status_code, status) != 0) {
		TELEMETRY_LOG_ERR("Status code is invalid");
		fail_count++;
	}

	if (strcmp(data_struct->data, data) != 0) {
		TELEMETRY_LOG_ERR("Data status is invalid");
		fail_count++;
	}

	rte_telemetry_free_test_data(data_struct);
	if (fail_count > 0)
		return -1;

	TELEMETRY_LOG_INFO("Success - Passed invalid JSON content test");

	return 0;
}

int32_t
rte_telemetry_json_empty_test(struct telemetry_impl *telemetry, int fd)
{
	int ret;
	char buf[BUF_SIZE];
	int fail_count = 0;
	const char *status = "Status Error: Invalid Argument 404";
	char *data = "null";
	struct json_data *data_struct;
	const char *empty_json  = "{}";
	int buffer_read = 0;
	errno = 0;

	ret = (send(fd, empty_json, strlen(empty_json), 0));
	if (ret < 0) {
		TELEMETRY_LOG_ERR("Could not send message over socket");
		return -1;
	}

	rte_telemetry_run(telemetry);
	buffer_read = recv(fd, buf, BUF_SIZE-1, 0);

	if (buffer_read == -1) {
		TELEMETRY_LOG_ERR("Read error");
		return -1;
	}

	buf[buffer_read] = '\0';
	data_struct = calloc(1, sizeof(struct json_data));
	ret = rte_telemetry_stat_parse(buf, data_struct);

	if (ret < 0)
		TELEMETRY_LOG_ERR("Could not parse stats");

	if (strcmp(data_struct->status_code, status) != 0) {
		TELEMETRY_LOG_ERR("Status code is invalid");
		fail_count++;
	}

	if (strcmp(data_struct->data, data) != 0) {
		TELEMETRY_LOG_ERR("Data status is invalid");
		fail_count++;
	}

	rte_telemetry_free_test_data(data_struct);

	if (fail_count > 0)
		return -1;

	TELEMETRY_LOG_INFO("Success - Passed JSON empty message test");

	return 0;
}

int32_t
rte_telemetry_json_socket_message_test(struct telemetry_impl *telemetry, int fd)
{
	uint16_t i;
	int ret, fail_count;

	fail_count = 0;
	struct telemetry_message_test socket_json_tests[] = {
		{.test_name = "Invalid JSON test",
			.test_func_ptr = rte_telemetry_invalid_json_test},
		{.test_name = "Valid JSON test",
			.test_func_ptr = rte_telemetry_valid_json_test},
		{.test_name = "JSON contents test",
			.test_func_ptr = rte_telemetry_json_contents_test},
		{.test_name = "JSON empty tests",
			.test_func_ptr = rte_telemetry_json_empty_test}
		};

#define NUM_TESTS RTE_DIM(socket_json_tests)

	for (i = 0; i < NUM_TESTS; i++) {
		TELEMETRY_LOG_INFO("%s", socket_json_tests[i].test_name);
		ret = (socket_json_tests[i].test_func_ptr)
			(telemetry, fd);
		if (ret < 0) {
			TELEMETRY_LOG_ERR("%s failed",
					socket_json_tests[i].test_name);
			fail_count++;
		}
	}

	if (fail_count > 0) {
		TELEMETRY_LOG_ERR("Failed %i JSON socket message test(s)",
				fail_count);
		return -1;
	}

	TELEMETRY_LOG_INFO("Success - All JSON tests passed");

	return 0;
}

int telemetry_log_level;

static struct rte_option option = {
	.name = "telemetry",
	.usage = "Enable telemetry backend",
	.cb = &rte_telemetry_init,
	.enabled = 0
};

RTE_INIT(rte_telemetry_register)
{
	telemetry_log_level = rte_log_register("lib.telemetry");
	if (telemetry_log_level >= 0)
		rte_log_set_level(telemetry_log_level, RTE_LOG_ERR);

	rte_option_register(&option);
}
