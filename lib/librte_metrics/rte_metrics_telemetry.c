/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <jansson.h>

#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "rte_metrics.h"
#include "rte_metrics_telemetry.h"

int metrics_log_level;

/* Logging Macros */
#define METRICS_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ##level, metrics_log_level, "%s(): "fmt "\n", \
		__func__, ##args)

#define METRICS_LOG_ERR(fmt, args...) \
	METRICS_LOG(ERR, fmt, ## args)

#define METRICS_LOG_WARN(fmt, args...) \
	METRICS_LOG(WARNING, fmt, ## args)

static int32_t
rte_metrics_tel_is_port_active(int port_id)
{
	int ret;

	ret = rte_eth_find_next(port_id);
	if (ret == port_id)
		return 1;

	METRICS_LOG_ERR("port_id: %d is invalid, not active",
		port_id);

	return 0;
}

static int32_t
rte_metrics_tel_reg_port_ethdev_to_metrics(uint16_t port_id)
{
	int ret, num_xstats, ret_val, i;
	struct rte_eth_xstat *eth_xstats = NULL;
	struct rte_eth_xstat_name *eth_xstats_names = NULL;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		METRICS_LOG_ERR("port_id: %d is invalid", port_id);
		return -EINVAL;
	}

	num_xstats = rte_eth_xstats_get(port_id, NULL, 0);
	if (num_xstats < 0) {
		METRICS_LOG_ERR("rte_eth_xstats_get(%u) failed: %d",
				port_id, num_xstats);
		return -EPERM;
	}

	eth_xstats = malloc(sizeof(struct rte_eth_xstat) * num_xstats);
	if (eth_xstats == NULL) {
		METRICS_LOG_ERR("Failed to malloc memory for xstats");
		return -ENOMEM;
	}

	ret = rte_eth_xstats_get(port_id, eth_xstats, num_xstats);
	const char *xstats_names[num_xstats];
	eth_xstats_names = malloc(sizeof(struct rte_eth_xstat_name)
			* num_xstats);
	if (ret < 0 || ret > num_xstats) {
		METRICS_LOG_ERR("rte_eth_xstats_get(%u) len%i failed: %d",
				port_id, num_xstats, ret);
		ret_val = -EPERM;
		goto free_xstats;
	}

	if (eth_xstats_names == NULL) {
		METRICS_LOG_ERR("Failed to malloc memory for xstats_names");
		ret_val = -ENOMEM;
		goto free_xstats;
	}

	ret = rte_eth_xstats_get_names(port_id, eth_xstats_names, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		METRICS_LOG_ERR("rte_eth_xstats_get_names(%u) len%i failed: %d",
				port_id, num_xstats, ret);
		ret_val = -EPERM;
		goto free_xstats;
	}

	for (i = 0; i < num_xstats; i++)
		xstats_names[i] = eth_xstats_names[eth_xstats[i].id].name;

	ret_val = rte_metrics_reg_names(xstats_names, num_xstats);
	if (ret_val < 0) {
		METRICS_LOG_ERR("rte_metrics_reg_names failed - metrics may already be registered");
		ret_val = -1;
		goto free_xstats;
	}

	goto free_xstats;

free_xstats:
	free(eth_xstats);
	free(eth_xstats_names);
	return ret_val;
}

int32_t
rte_metrics_tel_reg_all_ethdev(int *metrics_register_done, int *reg_index_list)
{
	struct driver_index {
		const void *dev_ops;
		int reg_index;
	} drv_idx[RTE_MAX_ETHPORTS] = { {0} };
	int nb_drv_idx = 0;
	uint16_t pid;
	int ret;

	RTE_ETH_FOREACH_DEV(pid) {
		int i;
		/* Different device types have different numbers of stats, so
		 * first check if the stats for this type of device have
		 * already been registered
		 */
		for (i = 0; i < nb_drv_idx; i++) {
			if (rte_eth_devices[pid].dev_ops ==
					drv_idx[i].dev_ops) {
				reg_index_list[pid] = drv_idx[i].reg_index;
				break;
			}
		}
		if (i < nb_drv_idx)
			continue; /* we found a match, go to next port */

		/* No match, register a new set of xstats for this port */
		ret = rte_metrics_tel_reg_port_ethdev_to_metrics(pid);
		if (ret < 0) {
			METRICS_LOG_ERR("Failed to register ethdev metrics");
			return -1;
		}
		reg_index_list[pid] = ret;
		drv_idx[nb_drv_idx].dev_ops = rte_eth_devices[pid].dev_ops;
		drv_idx[nb_drv_idx].reg_index = ret;
		nb_drv_idx++;
	}

	*metrics_register_done = 1;
	return 0;
}

static int32_t
rte_metrics_tel_update_metrics_ethdev(uint16_t port_id, int reg_start_index)
{
	int ret, num_xstats, i;
	struct rte_eth_xstat *eth_xstats;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		METRICS_LOG_ERR("port_id: %d is invalid", port_id);
		return -EINVAL;
	}

	ret = rte_metrics_tel_is_port_active(port_id);
	if (ret < 1)
		return -EINVAL;

	num_xstats = rte_eth_xstats_get(port_id, NULL, 0);
	if (num_xstats < 0) {
		METRICS_LOG_ERR("rte_eth_xstats_get(%u) failed: %d", port_id,
				num_xstats);
		return -EPERM;
	}

	eth_xstats = malloc(sizeof(struct rte_eth_xstat) * num_xstats);
	if (eth_xstats == NULL) {
		METRICS_LOG_ERR("Failed to malloc memory for xstats");
		return -ENOMEM;
	}

	ret = rte_eth_xstats_get(port_id, eth_xstats, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		free(eth_xstats);
		METRICS_LOG_ERR("rte_eth_xstats_get(%u) len%i failed: %d",
				port_id, num_xstats, ret);
		return -EPERM;
	}

	uint64_t xstats_values[num_xstats];
	for (i = 0; i < num_xstats; i++)
		xstats_values[i] = eth_xstats[i].value;

	ret = rte_metrics_update_values(port_id, reg_start_index, xstats_values,
			num_xstats);
	if (ret < 0) {
		METRICS_LOG_ERR("Could not update metrics values");
		free(eth_xstats);
		return -EPERM;
	}

	free(eth_xstats);
	return 0;
}

static int
rte_metrics_tel_get_metrics(uint32_t port_id, struct rte_metric_value
	*metrics, struct rte_metric_name *names, int num_metrics)
{
	int ret, num_values;

	if (num_metrics < 0) {
		METRICS_LOG_ERR("Invalid metrics count");
		return -EINVAL;
	} else if (num_metrics == 0) {
		METRICS_LOG_ERR("No metrics to display (none have been registered)");
		return -EPERM;
	}

	if (metrics == NULL) {
		METRICS_LOG_ERR("Metrics must be initialised.");
		return -EINVAL;
	}

	if (names == NULL) {
		METRICS_LOG_ERR("Names must be initialised.");
		return -EINVAL;
	}

	ret = rte_metrics_get_names(names, num_metrics);
	if (ret < 0 || ret > num_metrics) {
		METRICS_LOG_ERR("Cannot get metrics names");
		return -EPERM;
	}

	num_values = rte_metrics_get_values(port_id, NULL, 0);
	ret = rte_metrics_get_values(port_id, metrics, num_values);
	if (ret < 0 || ret > num_values) {
		METRICS_LOG_ERR("Cannot get metrics values");
		return -EPERM;
	}

	return 0;
}

static int32_t
rte_metrics_tel_json_format_stat(json_t *stats, const char *metric_name,
	uint64_t metric_value)
{
	int ret;
	json_t *stat = json_object();

	if (stat == NULL) {
		METRICS_LOG_ERR("Could not create stat JSON object");
		return -EPERM;
	}

	ret = json_object_set_new(stat, "name", json_string(metric_name));
	if (ret < 0) {
		METRICS_LOG_ERR("Stat Name field cannot be set");
		return -EPERM;
	}

	ret = json_object_set_new(stat, "value", json_integer(metric_value));
	if (ret < 0) {
		METRICS_LOG_ERR("Stat Value field cannot be set");
		return -EPERM;
	}

	ret = json_array_append_new(stats, stat);
	if (ret < 0) {
		METRICS_LOG_ERR("Stat cannot be added to stats json array");
		return -EPERM;
	}

	return 0;
}

static int32_t
rte_metrics_tel_json_format_port(uint32_t port_id, json_t *ports,
	uint32_t *metric_ids, int num_metric_ids)
{
	struct rte_metric_value *metrics = 0;
	struct rte_metric_name *names = 0;
	int num_metrics, ret;
	json_t *port, *stats;
	int i;

	num_metrics = rte_metrics_get_names(NULL, 0);
	if (num_metrics < 0) {
		METRICS_LOG_ERR("Cannot get metrics count");
		goto einval_fail;
	} else if (num_metrics == 0) {
		METRICS_LOG_ERR("No metrics to display (none have been registered)");
		goto eperm_fail;
	}

	metrics = malloc(sizeof(struct rte_metric_value) * num_metrics);
	names = malloc(sizeof(struct rte_metric_name) * num_metrics);
	if (metrics == NULL || names == NULL) {
		METRICS_LOG_ERR("Cannot allocate memory");
		free(metrics);
		free(names);
		return -ENOMEM;
	}

	ret  = rte_metrics_tel_get_metrics(port_id, metrics, names,
			num_metrics);
	if (ret < 0) {
		free(metrics);
		free(names);
		METRICS_LOG_ERR("rte_metrics_tel_get_metrics failed");
		return ret;
	}

	port = json_object();
	stats = json_array();
	if (port == NULL || stats == NULL) {
		METRICS_LOG_ERR("Could not create port/stats JSON objects");
		goto eperm_fail;
	}

	ret = json_object_set_new(port, "port", json_integer(port_id));
	if (ret < 0) {
		METRICS_LOG_ERR("Port field cannot be set");
		goto eperm_fail;
	}

	for (i = 0; i < num_metric_ids; i++) {
		int metric_id = metric_ids[i];
		int metric_index = -1;
		int metric_name_key = -1;
		int32_t j;
		uint64_t metric_value;

		if (metric_id >= num_metrics) {
			METRICS_LOG_ERR("Metric_id: %d is not valid",
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
			METRICS_LOG_ERR("Could not get metric name/index");
			goto eperm_fail;
		}

		ret = rte_metrics_tel_json_format_stat(stats, metric_name,
				metric_value);
		if (ret < 0) {
			METRICS_LOG_ERR("Format stat with id: %u failed",
					metric_id);
			free(metrics);
			free(names);
			return ret;
		}
	}

	if (json_array_size(stats) == 0)
		ret = json_object_set_new(port, "stats", json_null());
	else
		ret = json_object_set_new(port, "stats", stats);

	if (ret < 0) {
		METRICS_LOG_ERR("Stats object cannot be set");
		goto eperm_fail;
	}

	ret = json_array_append_new(ports, port);
	if (ret < 0) {
		METRICS_LOG_ERR("Port object cannot be added to ports array");
		goto eperm_fail;
	}

	free(metrics);
	free(names);
	return 0;

eperm_fail:
	free(metrics);
	free(names);
	return -EPERM;

einval_fail:
	free(metrics);
	free(names);
	return -EINVAL;
}

int32_t
rte_metrics_tel_encode_json_format(struct telemetry_encode_param *ep,
		char **json_buffer)
{
	int ret;
	json_t *root, *ports;
	int i;
	uint32_t port_id;
	int num_port_ids;
	int num_metric_ids;

	ports = json_array();
	if (ports == NULL) {
		METRICS_LOG_ERR("Could not create ports JSON array");
		return -EPERM;
	}

	if (ep->type == PORT_STATS) {
		num_port_ids = ep->pp.num_port_ids;
		num_metric_ids = ep->pp.num_metric_ids;

		if (num_port_ids <= 0 || num_metric_ids <= 0) {
			METRICS_LOG_ERR("Please provide port and metric ids to query");
			return -EINVAL;
		}

		for (i = 0; i < num_port_ids; i++) {
			port_id = ep->pp.port_ids[i];
			if (!rte_eth_dev_is_valid_port(port_id)) {
				METRICS_LOG_ERR("Port: %d invalid",
						port_id);
				return -EINVAL;
			}
		}

		for (i = 0; i < num_port_ids; i++) {
			port_id = ep->pp.port_ids[i];
			ret = rte_metrics_tel_json_format_port(port_id,
					ports, &ep->pp.metric_ids[0],
					num_metric_ids);
			if (ret < 0) {
				METRICS_LOG_ERR("Format port in JSON failed");
				return ret;
			}
		}
	} else if (ep->type == GLOBAL_STATS) {
		/* Request Global Metrics */
		ret = rte_metrics_tel_json_format_port(RTE_METRICS_GLOBAL,
				ports, &ep->gp.metric_ids[0],
				ep->gp.num_metric_ids);
		if (ret < 0) {
			METRICS_LOG_ERR(" Request Global Metrics Failed");
			return ret;
		}
	} else {
		METRICS_LOG_ERR(" Invalid metrics type in encode params");
		return -EINVAL;
	}

	root = json_object();
	if (root == NULL) {
		METRICS_LOG_ERR("Could not create root JSON object");
		return -EPERM;
	}

	ret = json_object_set_new(root, "status_code",
		json_string("Status OK: 200"));
	if (ret < 0) {
		METRICS_LOG_ERR("Status code field cannot be set");
		return -EPERM;
	}

	ret = json_object_set_new(root, "data", ports);
	if (ret < 0) {
		METRICS_LOG_ERR("Data field cannot be set");
		return -EPERM;
	}

	*json_buffer = json_dumps(root, JSON_INDENT(2));
	json_decref(root);
	return 0;
}

int32_t
rte_metrics_tel_get_global_stats(struct telemetry_encode_param *ep)
{
	int num_metrics, ret, i;
	struct rte_metric_value *values;

	num_metrics = rte_metrics_get_values(RTE_METRICS_GLOBAL, NULL, 0);
	if (num_metrics < 0) {
		METRICS_LOG_ERR("Cannot get metrics count");
		return -EINVAL;
	} else if (num_metrics == 0) {
		METRICS_LOG_ERR("No metrics to display (none have been registered)");
		return -EPERM;
	}

	values = malloc(sizeof(struct rte_metric_value) * num_metrics);
	if (values == NULL) {
		METRICS_LOG_ERR("Cannot allocate memory");
		return -ENOMEM;
	}

	ret = rte_metrics_get_values(RTE_METRICS_GLOBAL, values, num_metrics);
	if (ret < 0) {
		METRICS_LOG_ERR("Could not get stat values");
		free(values);
		return -EINVAL;
	}
	for (i = 0; i < num_metrics; i++)
		ep->gp.metric_ids[i] = values[i].key;

	ep->gp.num_metric_ids = num_metrics;
	ep->type = GLOBAL_STATS;
	free(values);
	return 0;
}

int32_t
rte_metrics_tel_get_ports_stats_json(struct telemetry_encode_param *ep,
		int *reg_index, char **json_buffer)
{
	int ret, i;
	uint32_t port_id;

	for (i = 0; i < ep->pp.num_port_ids; i++) {
		port_id = ep->pp.port_ids[i];
		if (!rte_eth_dev_is_valid_port(port_id)) {
			METRICS_LOG_ERR("Port: %d invalid", port_id);
			return -EINVAL;
		}

		ret = rte_metrics_tel_update_metrics_ethdev(port_id,
				reg_index[i]);
		if (ret < 0) {
			METRICS_LOG_ERR("Failed to update ethdev metrics");
			return ret;
		}
	}

	ret = rte_metrics_tel_encode_json_format(ep, json_buffer);
	if (ret < 0) {
		METRICS_LOG_ERR("JSON encode function failed");
		return ret;
	}
	return 0;
}

int32_t
rte_metrics_tel_get_port_stats_ids(struct telemetry_encode_param *ep)
{
	int ret, num_metrics, i, p;
	struct rte_metric_value *values;
	uint64_t num_port_ids = 0;

	num_metrics = rte_metrics_get_values(0, NULL, 0);
	if (num_metrics < 0) {
		METRICS_LOG_ERR("Cannot get metrics count");
		return -EINVAL;
	} else if (num_metrics == 0) {
		METRICS_LOG_ERR("No metrics to display (none have been registered)");
		return -EPERM;
	}

	values = malloc(sizeof(struct rte_metric_value) * num_metrics);
	if (values == NULL) {
		METRICS_LOG_ERR("Cannot allocate memory");
		return -ENOMEM;
	}

	RTE_ETH_FOREACH_DEV(p) {
		ep->pp.port_ids[num_port_ids] = p;
		num_port_ids++;
	}

	if (!num_port_ids) {
		METRICS_LOG_ERR("No active ports");
		goto fail;
	}

	ret = rte_metrics_get_values(ep->pp.port_ids[0], values, num_metrics);
	if (ret < 0) {
		METRICS_LOG_ERR("Could not get stat values");
		goto fail;
	}
	for (i = 0; i < num_metrics; i++)
		ep->pp.metric_ids[i] = values[i].key;

	ep->pp.num_port_ids = num_port_ids;
	ep->pp.num_metric_ids = num_metrics;
	ep->type = PORT_STATS;
	return 0;

fail:
	free(values);
	return -EINVAL;
}

static int32_t
rte_metrics_tel_stat_names_to_ids(const char * const *stat_names,
	uint32_t *stat_ids, uint64_t num_stat_names)
{
	struct rte_metric_name *names;
	int ret, num_metrics;
	uint32_t i, k;

	if (stat_names == NULL) {
		METRICS_LOG_WARN("Invalid stat_names argument");
		return -EINVAL;
	}

	if (num_stat_names <= 0) {
		METRICS_LOG_WARN("Invalid num_stat_names argument");
		return -EINVAL;
	}

	num_metrics = rte_metrics_get_names(NULL, 0);
	if (num_metrics < 0) {
		METRICS_LOG_ERR("Cannot get metrics count");
		return -EPERM;
	} else if (num_metrics == 0) {
		METRICS_LOG_WARN("No metrics have been registered");
		return -EPERM;
	}

	names = malloc(sizeof(struct rte_metric_name) * num_metrics);
	if (names == NULL) {
		METRICS_LOG_ERR("Cannot allocate memory for names");
		return -ENOMEM;
	}

	ret = rte_metrics_get_names(names, num_metrics);
	if (ret < 0 || ret > num_metrics) {
		METRICS_LOG_ERR("Cannot get metrics names");
		free(names);
		return -EPERM;
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
		METRICS_LOG_WARN("Invalid stat names provided");
		free(names);
		return -EINVAL;
	}

	free(names);
	return 0;
}

int32_t
rte_metrics_tel_extract_data(struct telemetry_encode_param *ep, json_t *data)
{
	int ret;
	json_t *port_ids_json = json_object_get(data, "ports");
	json_t *stat_names_json = json_object_get(data, "stats");
	uint64_t num_stat_names = json_array_size(stat_names_json);
	const char *stat_names[num_stat_names];
	size_t index;
	json_t *value;

	memset(ep, 0, sizeof(*ep));
	ep->pp.num_port_ids = json_array_size(port_ids_json);
	ep->pp.num_metric_ids = num_stat_names;
	if (!json_is_object(data)) {
		METRICS_LOG_WARN("Invalid data provided for this command");
		return -EINVAL;
	}

	if (!json_is_array(port_ids_json) ||
		 !json_is_array(stat_names_json)) {
		METRICS_LOG_WARN("Invalid input data array(s)");
		return -EINVAL;
	}

	json_array_foreach(port_ids_json, index, value) {
		if (!json_is_integer(value)) {
			METRICS_LOG_WARN("Port ID given is not valid");
			return -EINVAL;
		}
		ep->pp.port_ids[index] = json_integer_value(value);
		ret = rte_metrics_tel_is_port_active(ep->pp.port_ids[index]);
		if (ret < 1)
			return -EINVAL;
	}

	json_array_foreach(stat_names_json, index, value) {
		if (!json_is_string(value)) {
			METRICS_LOG_WARN("Stat Name given is not a string");
			return -EINVAL;
		}
		stat_names[index] = json_string_value(value);
	}

	ret = rte_metrics_tel_stat_names_to_ids(stat_names, ep->pp.metric_ids,
			num_stat_names);
	if (ret < 0) {
		METRICS_LOG_ERR("Could not convert stat names to IDs");
		return ret;
	}

	ep->type = PORT_STATS;
	return 0;
}

RTE_INIT(metrics_ctor)
{
	metrics_log_level = rte_log_register("lib.metrics");
	if (metrics_log_level >= 0)
		rte_log_set_level(metrics_log_level, RTE_LOG_ERR);
}
