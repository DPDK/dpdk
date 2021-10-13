/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_string_fns.h>
#ifdef RTE_LIB_TELEMETRY
#include <telemetry_internal.h>
#endif

#include "rte_metrics.h"
#include "rte_metrics_telemetry.h"

#ifdef RTE_HAS_JANSSON

struct telemetry_metrics_data tel_met_data;

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
rte_metrics_tel_reg_port_ethdev_to_metrics(uint16_t port_id)
{
	int ret,  num_xstats, i;
	struct rte_eth_xstat_name *eth_xstats_names;
	const char **xstats_names;

	num_xstats = rte_eth_xstats_get(port_id, NULL, 0);
	if (num_xstats < 0) {
		METRICS_LOG_ERR("rte_eth_xstats_get(%u) failed: %d",
				port_id, num_xstats);
		return -EPERM;
	}

	xstats_names = malloc(sizeof(*xstats_names) * num_xstats);
	eth_xstats_names = malloc(sizeof(struct rte_eth_xstat_name)
			* num_xstats);
	if (eth_xstats_names == NULL || xstats_names == NULL) {
		METRICS_LOG_ERR("Failed to malloc memory for xstats_names");
		ret = -ENOMEM;
		goto free_xstats;
	}

	if (rte_eth_xstats_get_names(port_id,
			eth_xstats_names, num_xstats) != num_xstats) {
		METRICS_LOG_ERR("rte_eth_xstats_get_names(%u) len %d failed",
				port_id, num_xstats);
		ret = -EPERM;
		goto free_xstats;
	}

	for (i = 0; i < num_xstats; i++)
		xstats_names[i] = eth_xstats_names[i].name;
	ret = rte_metrics_reg_names(xstats_names, num_xstats);
	if (ret < 0)
		METRICS_LOG_ERR("rte_metrics_reg_names failed - metrics may already be registered");

free_xstats:
	free(eth_xstats_names);
	free(xstats_names);
	return ret;
}

int32_t
rte_metrics_tel_reg_all_ethdev(int *metrics_register_done, int *reg_index_list)
{
	struct driver_index {
		const void *dev_ops;
		int reg_index;
	} drv_idx[RTE_MAX_ETHPORTS] = { {0} };
	int ret, nb_drv_idx = 0;
	uint16_t d;

	rte_metrics_init(rte_socket_id());
	RTE_ETH_FOREACH_DEV(d) {
		int i;
		/* Different device types have different numbers of stats, so
		 * first check if the stats for this type of device have
		 * already been registered
		 */
		for (i = 0; i < nb_drv_idx; i++) {
			if (rte_eth_devices[d].dev_ops == drv_idx[i].dev_ops) {
				reg_index_list[d] = drv_idx[i].reg_index;
				break;
			}
		}
		if (i < nb_drv_idx)
			continue; /* we found a match, go to next port */

		/* No match, register a new set of xstats for this port */
		ret = rte_metrics_tel_reg_port_ethdev_to_metrics(d);
		if (ret < 0) {
			METRICS_LOG_ERR("Failed to register ethdev to metrics");
			return ret;
		}
		reg_index_list[d] = ret;
		drv_idx[nb_drv_idx].dev_ops = rte_eth_devices[d].dev_ops;
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
	if (rte_metrics_update_values(port_id, reg_start_index, xstats_values,
			num_xstats) < 0) {
		METRICS_LOG_ERR("Could not update metrics values");
		free(eth_xstats);
		return -EPERM;
	}
	free(eth_xstats);
	return 0;
}

static int32_t
rte_metrics_tel_format_port(uint32_t pid, json_t *ports,
	uint32_t *metric_ids, int num_metric_ids)
{
	struct rte_metric_value *metrics = NULL;
	struct rte_metric_name *names = NULL;
	int num_metrics, i, ret = -EPERM; /* most error cases return EPERM */
	json_t *port, *stats;

	num_metrics = rte_metrics_get_names(NULL, 0);
	if (num_metrics < 0) {
		METRICS_LOG_ERR("Cannot get metrics count");
		return -EINVAL;
	} else if (num_metrics == 0) {
		METRICS_LOG_ERR("No metrics to display (none have been registered)");
		return -EPERM;
	}

	metrics = malloc(sizeof(struct rte_metric_value) * num_metrics);
	names = malloc(sizeof(struct rte_metric_name) * num_metrics);
	if (metrics == NULL || names == NULL) {
		METRICS_LOG_ERR("Cannot allocate memory");
		ret = -ENOMEM;
		goto fail;
	}

	if (rte_metrics_get_names(names, num_metrics) != num_metrics ||
			rte_metrics_get_values(pid, metrics, num_metrics)
				!= num_metrics) {
		METRICS_LOG_ERR("Error getting metrics");
		goto fail;
	}

	stats = json_array();
	if (stats == NULL) {
		METRICS_LOG_ERR("Could not create stats JSON object");
		goto fail;
	}

	for (i = 0; i < num_metrics; i++) {
		int32_t j;
		for (j = 0; j < num_metric_ids; j++)
			if (metrics[i].key == metric_ids[j])
				break;

		if (num_metric_ids > 0 && j == num_metric_ids)
			continue; /* can't find this id */

		json_t *stat = json_pack("{s,s,s,I}",
				"name", names[metrics[i].key].name,
				"value", metrics[i].value);
		if (stat == NULL || json_array_append_new(stats, stat) < 0) {
			METRICS_LOG_ERR("Format stat with id: %u failed",
					metrics[i].key);
			goto fail;
		}
	}

	port = json_pack("{s,i,s,o}", "port", pid, "stats",
			json_array_size(stats) ? stats : json_null());
	if (port == NULL || json_array_append_new(ports, port) < 0) {
		METRICS_LOG_ERR("Error creating port and adding to ports");
		goto fail;
	}

	free(metrics);
	free(names);
	return 0;

fail:
	free(metrics);
	free(names);
	return ret;
}

int32_t
rte_metrics_tel_encode_json_format(struct telemetry_encode_param *ep,
		char **json_buffer)
{
	json_t *root, *ports;
	int ret, i;

	ports = json_array();
	if (ports == NULL) {
		METRICS_LOG_ERR("Could not create ports JSON array");
		return -EPERM;
	}

	if (ep->type == PORT_STATS) {
		if (ep->pp.num_port_ids <= 0) {
			METRICS_LOG_ERR("Please provide port/metric ids");
			return -EINVAL;
		}

		for (i = 0; i < ep->pp.num_port_ids; i++) {
			ret = rte_metrics_tel_format_port(ep->pp.port_ids[i],
					ports, &ep->pp.metric_ids[0],
					ep->pp.num_metric_ids);
			if (ret < 0) {
				METRICS_LOG_ERR("Format port in JSON failed");
				return ret;
			}
		}
	} else if (ep->type == GLOBAL_STATS) {
		/* Request Global Metrics */
		ret = rte_metrics_tel_format_port(RTE_METRICS_GLOBAL,
				ports, NULL, 0);
		if (ret < 0) {
			METRICS_LOG_ERR("Request Global Metrics Failed");
			return ret;
		}
	} else {
		METRICS_LOG_ERR("Invalid metrics type in encode params");
		return -EINVAL;
	}

	root = json_pack("{s,s,s,o}", "status_code", "Status OK: 200",
			"data", ports);
	if (root == NULL) {
		METRICS_LOG_ERR("Root, Status or data field cannot be set");
		return -EPERM;
	}

	*json_buffer = json_dumps(root, JSON_INDENT(2));
	json_decref(root);
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
	int p, num_port_ids = 0;

	RTE_ETH_FOREACH_DEV(p) {
		ep->pp.port_ids[num_port_ids] = p;
		num_port_ids++;
	}

	if (!num_port_ids) {
		METRICS_LOG_ERR("No active ports");
		return -EINVAL;
	}

	ep->pp.num_port_ids = num_port_ids;
	ep->pp.num_metric_ids = 0;
	ep->type = PORT_STATS;
	return 0;
}

static int32_t
rte_metrics_tel_stat_names_to_ids(const char * const *stat_names,
	uint32_t *stat_ids, int num_stat_names)
{
	struct rte_metric_name *names;
	int num_metrics;
	int i, j, nb_stat_ids = 0;

	num_metrics = rte_metrics_get_names(NULL, 0);
	if (num_metrics <= 0) {
		METRICS_LOG_ERR("Error getting metrics count - no metrics may be registered");
		return -EPERM;
	}

	names = malloc(sizeof(struct rte_metric_name) * num_metrics);
	if (names == NULL) {
		METRICS_LOG_ERR("Cannot allocate memory for names");
		return -ENOMEM;
	}

	if (rte_metrics_get_names(names, num_metrics) != num_metrics) {
		METRICS_LOG_ERR("Cannot get metrics names");
		free(names);
		return -EPERM;
	}

	for (i = 0; i < num_stat_names; i++) {
		for (j = 0; j < num_metrics; j++) {
			if (strcmp(stat_names[i], names[j].name) == 0) {
				stat_ids[nb_stat_ids++] = j;
				break;
			}
		}
		if (j == num_metrics) {
			METRICS_LOG_WARN("Invalid stat name %s\n",
					stat_names[i]);
			free(names);
			return -EINVAL;
		}
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
	if (!json_is_object(data) || !json_is_array(port_ids_json) ||
			!json_is_array(stat_names_json)) {
		METRICS_LOG_WARN("Invalid data provided for this command");
		return -EINVAL;
	}

	json_array_foreach(port_ids_json, index, value) {
		if (!json_is_integer(value)) {
			METRICS_LOG_WARN("Port ID given is not valid");
			return -EINVAL;
		}
		ep->pp.port_ids[index] = json_integer_value(value);
		if (rte_eth_dev_is_valid_port(ep->pp.port_ids[index]) < 1)
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

static int
rte_metrics_tel_initial_metrics_setup(void)
{
	int ret;
	rte_metrics_init(rte_socket_id());

	if (!tel_met_data.metrics_register_done) {
		ret = rte_metrics_tel_reg_all_ethdev(
			&tel_met_data.metrics_register_done,
			tel_met_data.reg_index);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int
handle_ports_all_stats_values(const char *cmd __rte_unused,
		const char *params __rte_unused,
		char *buffer, int buf_len)
{
	struct telemetry_encode_param ep;
	int ret, used = 0;
	char *json_buffer = NULL;

	ret = rte_metrics_tel_initial_metrics_setup();
	if (ret < 0)
		return ret;

	memset(&ep, 0, sizeof(ep));
	ret = rte_metrics_tel_get_port_stats_ids(&ep);
	if (ret < 0)
		return ret;

	ret = rte_metrics_tel_get_ports_stats_json(&ep, tel_met_data.reg_index,
			&json_buffer);
	if (ret < 0)
		return ret;

	used += strlcpy(buffer, json_buffer, buf_len);
	return used;
}

static int
handle_global_stats_values(const char *cmd __rte_unused,
		const char *params __rte_unused,
		char *buffer, int buf_len)
{
	char *json_buffer = NULL;
	struct telemetry_encode_param ep = { .type = GLOBAL_STATS };
	int ret, used = 0;

	ret = rte_metrics_tel_initial_metrics_setup();
	if (ret < 0)
		return ret;

	ret = rte_metrics_tel_encode_json_format(&ep, &json_buffer);
	if (ret < 0) {
		METRICS_LOG_ERR("JSON encode function failed");
		return ret;
	}
	used += strlcpy(buffer, json_buffer, buf_len);
	return used;
}

static int
handle_ports_stats_values_by_name(const char *cmd __rte_unused,
		const char *params,
		char *buffer, int buf_len)
{
	char *json_buffer = NULL;
	struct telemetry_encode_param ep;
	int ret, used = 0;
	json_t *data;
	json_error_t error;

	ret = rte_metrics_tel_initial_metrics_setup();
	if (ret < 0)
		return ret;

	data = json_loads(params, 0, &error);
	if (!data) {
		METRICS_LOG_WARN("Could not load JSON object from data passed in : %s",
				error.text);
		return -EPERM;
	} else if (!json_is_object(data)) {
		METRICS_LOG_WARN("JSON Request data is not a JSON object");
		json_decref(data);
		return -EINVAL;
	}

	ret = rte_metrics_tel_extract_data(&ep, data);
	if (ret < 0) {
		METRICS_LOG_ERR("Extract data function failed");
		return ret;
	}

	ret = rte_metrics_tel_encode_json_format(&ep, &json_buffer);
	if (ret < 0) {
		METRICS_LOG_ERR("JSON encode function failed");
		return ret;
	}
	used += strlcpy(buffer, json_buffer, buf_len);
	return used;
}

RTE_LOG_REGISTER_DEFAULT(metrics_log_level, ERR);

RTE_INIT(metrics_ctor)
{
#ifdef RTE_LIB_TELEMETRY
	rte_telemetry_legacy_register("ports_all_stat_values", DATA_NOT_REQ,
			handle_ports_all_stats_values);
	rte_telemetry_legacy_register("global_stat_values", DATA_NOT_REQ,
			handle_global_stats_values);
	rte_telemetry_legacy_register("ports_stats_values_by_name", DATA_REQ,
			handle_ports_stats_values_by_name);
#endif
}

#else /* !RTE_HAS_JANSSON */

int32_t
rte_metrics_tel_reg_all_ethdev(int *metrics_register_done, int *reg_index_list)
{
	RTE_SET_USED(metrics_register_done);
	RTE_SET_USED(reg_index_list);

	return -ENOTSUP;
}

int32_t
rte_metrics_tel_encode_json_format(struct telemetry_encode_param *ep,
	char **json_buffer)
{
	RTE_SET_USED(ep);
	RTE_SET_USED(json_buffer);

	return -ENOTSUP;
}

int32_t
rte_metrics_tel_get_ports_stats_json(struct telemetry_encode_param *ep,
	int *reg_index, char **json_buffer)
{
	RTE_SET_USED(ep);
	RTE_SET_USED(reg_index);
	RTE_SET_USED(json_buffer);

	return -ENOTSUP;
}

int32_t
rte_metrics_tel_get_port_stats_ids(struct telemetry_encode_param *ep)
{
	RTE_SET_USED(ep);

	return -ENOTSUP;
}

int32_t
rte_metrics_tel_extract_data(struct telemetry_encode_param *ep, json_t *data)
{
	RTE_SET_USED(ep);
	RTE_SET_USED(data);

	return -ENOTSUP;
}

int32_t
rte_metrics_tel_get_global_stats(struct telemetry_encode_param *ep)
{
	RTE_SET_USED(ep);

	return -ENOTSUP;
}

#endif /* !RTE_HAS_JANSSON */
