/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <ctype.h>
#include <stdlib.h>

#include <rte_kvargs.h>
#include <rte_telemetry.h>

#include "rte_ethdev.h"
#include "ethdev_driver.h"
#include "sff_telemetry.h"

static const struct {
	uint32_t capa;
	const char *name;
} rte_eth_fec_capa_name[] = {
	{ RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC),	"off"	},
	{ RTE_ETH_FEC_MODE_CAPA_MASK(AUTO),	"auto"	},
	{ RTE_ETH_FEC_MODE_CAPA_MASK(BASER),	"baser"	},
	{ RTE_ETH_FEC_MODE_CAPA_MASK(RS),	"rs"	},
	{ RTE_ETH_FEC_MODE_CAPA_MASK(LLRS),	"llrs"	},
};

static int
eth_dev_parse_port_params(const char *params, uint16_t *port_id,
		char **end_param, bool has_next)
{
	uint64_t pi;

	if (params == NULL || strlen(params) == 0 ||
		!isdigit(*params) || port_id == NULL)
		return -EINVAL;

	pi = strtoul(params, end_param, 0);
	if (**end_param != '\0' && !has_next)
		RTE_ETHDEV_LOG(NOTICE,
			"Extra parameters passed to ethdev telemetry command, ignoring\n");

	if (pi >= UINT16_MAX || !rte_eth_dev_is_valid_port(pi))
		return -EINVAL;

	*port_id = (uint16_t)pi;

	return 0;
}

static int
eth_dev_handle_port_list(const char *cmd __rte_unused,
		const char *params __rte_unused,
		struct rte_tel_data *d)
{
	int port_id;

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	RTE_ETH_FOREACH_DEV(port_id)
		rte_tel_data_add_array_int(d, port_id);
	return 0;
}

static void
eth_dev_add_port_queue_stats(struct rte_tel_data *d, uint64_t *q_stats,
		const char *stat_name)
{
	int q;
	struct rte_tel_data *q_data = rte_tel_data_alloc();
	if (q_data == NULL)
		return;
	rte_tel_data_start_array(q_data, RTE_TEL_UINT_VAL);
	for (q = 0; q < RTE_ETHDEV_QUEUE_STAT_CNTRS; q++)
		rte_tel_data_add_array_uint(q_data, q_stats[q]);
	rte_tel_data_add_dict_container(d, stat_name, q_data, 0);
}

static int
eth_dev_parse_hide_zero(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);

	if (value == NULL)
		return -1;

	if (strcmp(value, "true") == 0)
		*(bool *)extra_args = true;
	else if (strcmp(value, "false") == 0)
		*(bool *)extra_args = false;
	else
		return -1;

	return 0;
}

#define ADD_DICT_STAT(stats, s) rte_tel_data_add_dict_uint(d, #s, stats.s)

static int
eth_dev_handle_port_stats(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_eth_stats stats;
	uint16_t port_id;
	char *end_param;
	int ret;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	ret = rte_eth_stats_get(port_id, &stats);
	if (ret < 0)
		return -1;

	rte_tel_data_start_dict(d);
	ADD_DICT_STAT(stats, ipackets);
	ADD_DICT_STAT(stats, opackets);
	ADD_DICT_STAT(stats, ibytes);
	ADD_DICT_STAT(stats, obytes);
	ADD_DICT_STAT(stats, imissed);
	ADD_DICT_STAT(stats, ierrors);
	ADD_DICT_STAT(stats, oerrors);
	ADD_DICT_STAT(stats, rx_nombuf);
	eth_dev_add_port_queue_stats(d, stats.q_ipackets, "q_ipackets");
	eth_dev_add_port_queue_stats(d, stats.q_opackets, "q_opackets");
	eth_dev_add_port_queue_stats(d, stats.q_ibytes, "q_ibytes");
	eth_dev_add_port_queue_stats(d, stats.q_obytes, "q_obytes");
	eth_dev_add_port_queue_stats(d, stats.q_errors, "q_errors");

	return 0;
}

static int
eth_dev_handle_port_xstats(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	const char *const valid_keys[] = { "hide_zero", NULL };
	struct rte_eth_xstat *eth_xstats;
	struct rte_eth_xstat_name *xstat_names;
	struct rte_kvargs *kvlist;
	bool hide_zero = false;
	uint16_t port_id;
	char *end_param;
	int num_xstats;
	int i, ret;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, true);
	if (ret < 0)
		return ret;

	if (*end_param != '\0') {
		kvlist = rte_kvargs_parse(end_param, valid_keys);
		ret = rte_kvargs_process(kvlist, NULL, eth_dev_parse_hide_zero, &hide_zero);
		if (kvlist == NULL || ret != 0)
			RTE_ETHDEV_LOG(NOTICE,
				"Unknown extra parameters passed to ethdev telemetry command, ignoring\n");
		rte_kvargs_free(kvlist);
	}

	num_xstats = rte_eth_xstats_get(port_id, NULL, 0);
	if (num_xstats < 0)
		return -1;

	/* use one malloc for both names and stats */
	eth_xstats = malloc((sizeof(struct rte_eth_xstat) +
			sizeof(struct rte_eth_xstat_name)) * num_xstats);
	if (eth_xstats == NULL)
		return -1;
	xstat_names = (void *)&eth_xstats[num_xstats];

	ret = rte_eth_xstats_get_names(port_id, xstat_names, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		free(eth_xstats);
		return -1;
	}

	ret = rte_eth_xstats_get(port_id, eth_xstats, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		free(eth_xstats);
		return -1;
	}

	rte_tel_data_start_dict(d);
	for (i = 0; i < num_xstats; i++) {
		if (hide_zero && eth_xstats[i].value == 0)
			continue;
		rte_tel_data_add_dict_uint(d, xstat_names[i].name,
					   eth_xstats[i].value);
	}
	free(eth_xstats);
	return 0;
}

#ifndef RTE_EXEC_ENV_WINDOWS
static int
eth_dev_handle_port_dump_priv(const char *cmd __rte_unused,
			const char *params,
			struct rte_tel_data *d)
{
	char *buf, *end_param;
	uint16_t port_id;
	int ret;
	FILE *f;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	buf = calloc(RTE_TEL_MAX_SINGLE_STRING_LEN, sizeof(char));
	if (buf == NULL)
		return -ENOMEM;

	f = fmemopen(buf, RTE_TEL_MAX_SINGLE_STRING_LEN - 1, "w+");
	if (f == NULL) {
		free(buf);
		return -EINVAL;
	}

	ret = rte_eth_dev_priv_dump(port_id, f);
	fclose(f);
	if (ret == 0) {
		rte_tel_data_start_dict(d);
		rte_tel_data_string(d, buf);
	}

	free(buf);
	return 0;
}
#endif /* !RTE_EXEC_ENV_WINDOWS */

static int
eth_dev_handle_port_link_status(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	static const char *status_str = "status";
	struct rte_eth_link link;
	uint16_t port_id;
	char *end_param;
	int ret;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	ret = rte_eth_link_get_nowait(port_id, &link);
	if (ret < 0)
		return -1;

	rte_tel_data_start_dict(d);
	if (!link.link_status) {
		rte_tel_data_add_dict_string(d, status_str, "DOWN");
		return 0;
	}
	rte_tel_data_add_dict_string(d, status_str, "UP");
	rte_tel_data_add_dict_uint(d, "speed", link.link_speed);
	rte_tel_data_add_dict_string(d, "duplex",
			(link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX) ?
				"full-duplex" : "half-duplex");
	return 0;
}

static void
eth_dev_parse_rx_offloads(uint64_t offload, struct rte_tel_data *d)
{
	uint64_t i;

	rte_tel_data_start_array(d, RTE_TEL_STRING_VAL);
	for (i = 0; i < CHAR_BIT * sizeof(offload); i++) {
		if ((offload & RTE_BIT64(i)) != 0)
			rte_tel_data_add_array_string(d,
				rte_eth_dev_rx_offload_name(offload & RTE_BIT64(i)));
	}
}

static void
eth_dev_parse_tx_offloads(uint64_t offload, struct rte_tel_data *d)
{
	uint64_t i;

	rte_tel_data_start_array(d, RTE_TEL_STRING_VAL);
	for (i = 0; i < CHAR_BIT * sizeof(offload); i++) {
		if ((offload & RTE_BIT64(i)) != 0)
			rte_tel_data_add_array_string(d,
				rte_eth_dev_tx_offload_name(offload & RTE_BIT64(i)));
	}
}

static int
eth_dev_handle_port_info(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_tel_data *rx_offload, *tx_offload;
	struct rte_tel_data *rxq_state, *txq_state;
	char fw_version[RTE_TEL_MAX_STRING_LEN];
	char mac_addr[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_eth_dev *eth_dev;
	uint16_t port_id;
	char *end_param;
	int ret;
	int i;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	eth_dev = &rte_eth_devices[port_id];

	rxq_state = rte_tel_data_alloc();
	if (rxq_state == NULL)
		return -ENOMEM;

	txq_state = rte_tel_data_alloc();
	if (txq_state == NULL)
		goto free_rxq_state;

	rx_offload = rte_tel_data_alloc();
	if (rx_offload == NULL)
		goto free_txq_state;

	tx_offload = rte_tel_data_alloc();
	if (tx_offload == NULL)
		goto free_rx_offload;

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_string(d, "name", eth_dev->data->name);

	if (rte_eth_dev_fw_version_get(port_id, fw_version,
					 RTE_TEL_MAX_STRING_LEN) == 0)
		rte_tel_data_add_dict_string(d, "fw_version", fw_version);

	rte_tel_data_add_dict_int(d, "state", eth_dev->state);
	rte_tel_data_add_dict_int(d, "nb_rx_queues",
			eth_dev->data->nb_rx_queues);
	rte_tel_data_add_dict_int(d, "nb_tx_queues",
			eth_dev->data->nb_tx_queues);
	rte_tel_data_add_dict_int(d, "port_id", eth_dev->data->port_id);
	rte_tel_data_add_dict_int(d, "mtu", eth_dev->data->mtu);
	rte_tel_data_add_dict_uint(d, "rx_mbuf_size_min",
			eth_dev->data->min_rx_buf_size);
	rte_ether_format_addr(mac_addr, sizeof(mac_addr),
			eth_dev->data->mac_addrs);
	rte_tel_data_add_dict_string(d, "mac_addr", mac_addr);
	rte_tel_data_add_dict_int(d, "promiscuous",
			eth_dev->data->promiscuous);
	rte_tel_data_add_dict_int(d, "scattered_rx",
			eth_dev->data->scattered_rx);
	rte_tel_data_add_dict_int(d, "all_multicast",
			eth_dev->data->all_multicast);
	rte_tel_data_add_dict_int(d, "dev_started", eth_dev->data->dev_started);
	rte_tel_data_add_dict_int(d, "lro", eth_dev->data->lro);
	rte_tel_data_add_dict_int(d, "dev_configured",
			eth_dev->data->dev_configured);

	rte_tel_data_start_array(rxq_state, RTE_TEL_INT_VAL);
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
		rte_tel_data_add_array_int(rxq_state,
				eth_dev->data->rx_queue_state[i]);

	rte_tel_data_start_array(txq_state, RTE_TEL_INT_VAL);
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		rte_tel_data_add_array_int(txq_state,
				eth_dev->data->tx_queue_state[i]);

	rte_tel_data_add_dict_container(d, "rxq_state", rxq_state, 0);
	rte_tel_data_add_dict_container(d, "txq_state", txq_state, 0);
	rte_tel_data_add_dict_int(d, "numa_node", eth_dev->data->numa_node);
	rte_tel_data_add_dict_uint_hex(d, "dev_flags",
			eth_dev->data->dev_flags, 0);

	eth_dev_parse_rx_offloads(eth_dev->data->dev_conf.rxmode.offloads,
			rx_offload);
	rte_tel_data_add_dict_container(d, "rx_offloads", rx_offload, 0);
	eth_dev_parse_tx_offloads(eth_dev->data->dev_conf.txmode.offloads,
			tx_offload);
	rte_tel_data_add_dict_container(d, "tx_offloads", tx_offload, 0);

	rte_tel_data_add_dict_uint_hex(d, "ethdev_rss_hf",
			eth_dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf, 0);

	return 0;

free_rx_offload:
	rte_tel_data_free(rx_offload);
free_txq_state:
	rte_tel_data_free(txq_state);
free_rxq_state:
	rte_tel_data_free(rxq_state);

	return -ENOMEM;
}

static int
eth_dev_handle_port_macs(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	char mac_addr[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *eth_dev;
	uint16_t port_id;
	char *end_param;
	uint32_t i;
	int ret;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	eth_dev = &rte_eth_devices[port_id];
	rte_tel_data_start_array(d, RTE_TEL_STRING_VAL);
	for (i = 0; i < dev_info.max_mac_addrs; i++) {
		if (rte_is_zero_ether_addr(&eth_dev->data->mac_addrs[i]))
			continue;

		rte_ether_format_addr(mac_addr, sizeof(mac_addr),
			&eth_dev->data->mac_addrs[i]);
		rte_tel_data_add_array_string(d, mac_addr);
	}

	return 0;
}

static int
eth_dev_handle_port_flow_ctrl(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_eth_fc_conf fc_conf;
	uint16_t port_id;
	char *end_param;
	bool rx_fc_en;
	bool tx_fc_en;
	int ret;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	ret = rte_eth_dev_flow_ctrl_get(port_id, &fc_conf);
	if (ret != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Failed to get flow ctrl info, ret = %d\n", ret);
		return ret;
	}

	rx_fc_en = fc_conf.mode == RTE_ETH_FC_RX_PAUSE ||
		   fc_conf.mode == RTE_ETH_FC_FULL;
	tx_fc_en = fc_conf.mode == RTE_ETH_FC_TX_PAUSE ||
		   fc_conf.mode == RTE_ETH_FC_FULL;

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_uint_hex(d, "high_waterline", fc_conf.high_water, 0);
	rte_tel_data_add_dict_uint_hex(d, "low_waterline", fc_conf.low_water, 0);
	rte_tel_data_add_dict_uint_hex(d, "pause_time", fc_conf.pause_time, 0);
	rte_tel_data_add_dict_string(d, "send_xon", fc_conf.send_xon ? "on" : "off");
	rte_tel_data_add_dict_string(d, "mac_ctrl_frame_fwd",
			fc_conf.mac_ctrl_frame_fwd ? "on" : "off");
	rte_tel_data_add_dict_string(d, "rx_pause", rx_fc_en ? "on" : "off");
	rte_tel_data_add_dict_string(d, "tx_pause", tx_fc_en ? "on" : "off");
	rte_tel_data_add_dict_string(d, "autoneg", fc_conf.autoneg ? "on" : "off");

	return 0;
}

static int
ethdev_parse_queue_params(const char *params, bool is_rx,
		uint16_t *port_id, uint16_t *queue_id)
{
	struct rte_eth_dev *dev;
	const char *qid_param;
	uint16_t nb_queues;
	char *end_param;
	uint64_t qid;
	int ret;

	ret = eth_dev_parse_port_params(params, port_id, &end_param, true);
	if (ret < 0)
		return ret;

	dev = &rte_eth_devices[*port_id];
	nb_queues = is_rx ? dev->data->nb_rx_queues : dev->data->nb_tx_queues;
	if (nb_queues == 1 && *end_param == '\0')
		qid = 0;
	else {
		qid_param = strtok(end_param, ",");
		if (!qid_param || strlen(qid_param) == 0 || !isdigit(*qid_param))
			return -EINVAL;

		qid = strtoul(qid_param, &end_param, 0);
	}
	if (*end_param != '\0')
		RTE_ETHDEV_LOG(NOTICE,
			"Extra parameters passed to ethdev telemetry command, ignoring\n");

	if (qid >= UINT16_MAX)
		return -EINVAL;

	*queue_id = qid;
	return 0;
}

static int
eth_dev_add_burst_mode(uint16_t port_id, uint16_t queue_id,
			bool is_rx, struct rte_tel_data *d)
{
	struct rte_eth_burst_mode mode;
	int ret;

	if (is_rx)
		ret = rte_eth_rx_burst_mode_get(port_id, queue_id, &mode);
	else
		ret = rte_eth_tx_burst_mode_get(port_id, queue_id, &mode);

	if (ret == -ENOTSUP)
		return 0;

	if (ret != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Failed to get burst mode for port %u\n", port_id);
		return ret;
	}

	rte_tel_data_add_dict_uint(d, "burst_flags", mode.flags);
	rte_tel_data_add_dict_string(d, "burst_mode", mode.info);
	return 0;
}

static int
eth_dev_handle_port_rxq(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_eth_thresh *rx_thresh;
	struct rte_eth_rxconf *rxconf;
	struct rte_eth_rxq_info qinfo;
	struct rte_tel_data *offload;
	uint16_t port_id, queue_id;
	int ret;

	ret = ethdev_parse_queue_params(params, true, &port_id, &queue_id);
	if (ret != 0)
		return ret;

	ret = rte_eth_rx_queue_info_get(port_id, queue_id, &qinfo);
	if (ret != 0)
		return ret;

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_string(d, "mempool_name", qinfo.mp->name);
	rte_tel_data_add_dict_uint(d, "socket_id", qinfo.mp->socket_id);

	rx_thresh = &qinfo.conf.rx_thresh;
	rte_tel_data_add_dict_uint(d, "host_threshold", rx_thresh->hthresh);
	rte_tel_data_add_dict_uint(d, "prefetch_threshold", rx_thresh->pthresh);
	rte_tel_data_add_dict_uint(d, "writeback_threshold", rx_thresh->wthresh);

	rxconf = &qinfo.conf;
	rte_tel_data_add_dict_uint(d, "free_threshold", rxconf->rx_free_thresh);
	rte_tel_data_add_dict_string(d, "rx_drop_en",
			rxconf->rx_drop_en == 0 ? "off" : "on");
	rte_tel_data_add_dict_string(d, "deferred_start",
			rxconf->rx_deferred_start == 0 ? "off" : "on");
	rte_tel_data_add_dict_uint(d, "rx_nseg", rxconf->rx_nseg);
	rte_tel_data_add_dict_uint(d, "share_group", rxconf->share_group);
	rte_tel_data_add_dict_uint(d, "share_qid", rxconf->share_qid);

	offload = rte_tel_data_alloc();
	if (offload == NULL)
		return -ENOMEM;

	eth_dev_parse_rx_offloads(rxconf->offloads, offload);
	rte_tel_data_add_dict_container(d, "offloads", offload, 0);

	rte_tel_data_add_dict_uint(d, "rx_nmempool", rxconf->rx_nmempool);

	rte_tel_data_add_dict_string(d, "scattered_rx",
			qinfo.scattered_rx == 0 ? "off" : "on");
	rte_tel_data_add_dict_uint(d, "queue_state", qinfo.queue_state);
	rte_tel_data_add_dict_uint(d, "nb_desc", qinfo.nb_desc);
	rte_tel_data_add_dict_uint(d, "rx_buf_size", qinfo.rx_buf_size);
	rte_tel_data_add_dict_uint(d, "avail_thresh", qinfo.avail_thresh);

	ret = eth_dev_add_burst_mode(port_id, queue_id, true, d);
	if (ret != 0)
		rte_tel_data_free(offload);

	return ret;
}

static int
eth_dev_handle_port_txq(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_eth_thresh *tx_thresh;
	struct rte_eth_txconf *txconf;
	struct rte_eth_txq_info qinfo;
	struct rte_tel_data *offload;
	uint16_t port_id, queue_id;
	int ret;

	ret = ethdev_parse_queue_params(params, false, &port_id, &queue_id);
	if (ret != 0)
		return ret;

	ret = rte_eth_tx_queue_info_get(port_id, queue_id, &qinfo);
	if (ret != 0)
		return ret;

	rte_tel_data_start_dict(d);
	tx_thresh = &qinfo.conf.tx_thresh;
	txconf = &qinfo.conf;
	rte_tel_data_add_dict_uint(d, "host_threshold", tx_thresh->hthresh);
	rte_tel_data_add_dict_uint(d, "prefetch_threshold", tx_thresh->pthresh);
	rte_tel_data_add_dict_uint(d, "writeback_threshold", tx_thresh->wthresh);
	rte_tel_data_add_dict_uint(d, "rs_threshold", txconf->tx_rs_thresh);
	rte_tel_data_add_dict_uint(d, "free_threshold", txconf->tx_free_thresh);
	rte_tel_data_add_dict_string(d, "deferred_start",
			txconf->tx_deferred_start == 0 ? "off" : "on");

	offload = rte_tel_data_alloc();
	if (offload == NULL)
		return -ENOMEM;

	eth_dev_parse_tx_offloads(txconf->offloads, offload);
	rte_tel_data_add_dict_container(d, "offloads", offload, 0);

	rte_tel_data_add_dict_uint(d, "queue_state", qinfo.queue_state);
	rte_tel_data_add_dict_uint(d, "nb_desc", qinfo.nb_desc);

	ret = eth_dev_add_burst_mode(port_id, queue_id, false, d);
	if (ret != 0)
		rte_tel_data_free(offload);

	return 0;
}

static int
eth_dev_add_dcb_tc(struct rte_eth_dcb_info *dcb_info, struct rte_tel_data *d)
{
	struct rte_tel_data *tcds[RTE_ETH_DCB_NUM_TCS] = {NULL};
	struct rte_eth_dcb_tc_queue_mapping *tcq;
	char bw_percent[RTE_TEL_MAX_STRING_LEN];
	char name[RTE_TEL_MAX_STRING_LEN];
	struct rte_tel_data *tcd;
	uint32_t i;

	for (i = 0; i < dcb_info->nb_tcs; i++) {
		tcd = rte_tel_data_alloc();
		if (tcd == NULL) {
			while (i-- > 0)
				rte_tel_data_free(tcds[i]);
			return -ENOMEM;
		}

		tcds[i] = tcd;
		rte_tel_data_start_dict(tcd);

		rte_tel_data_add_dict_uint(tcd, "priority", dcb_info->prio_tc[i]);
		snprintf(bw_percent, RTE_TEL_MAX_STRING_LEN,
			"%u%%", dcb_info->tc_bws[i]);
		rte_tel_data_add_dict_string(tcd, "bw_percent", bw_percent);

		tcq = &dcb_info->tc_queue;
		rte_tel_data_add_dict_uint(tcd, "rxq_base", tcq->tc_rxq[0][i].base);
		rte_tel_data_add_dict_uint(tcd, "txq_base", tcq->tc_txq[0][i].base);
		rte_tel_data_add_dict_uint(tcd, "nb_rxq", tcq->tc_rxq[0][i].nb_queue);
		rte_tel_data_add_dict_uint(tcd, "nb_txq", tcq->tc_txq[0][i].nb_queue);

		snprintf(name, RTE_TEL_MAX_STRING_LEN, "tc%u", i);
		rte_tel_data_add_dict_container(d, name, tcd, 0);
	}

	return 0;
}

static int
eth_dev_add_dcb_info(uint16_t port_id, struct rte_tel_data *d)
{
	struct rte_eth_dcb_info dcb_info;
	int ret;

	ret = rte_eth_dev_get_dcb_info(port_id, &dcb_info);
	if (ret != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Failed to get dcb info, ret = %d\n", ret);
		return ret;
	}

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_uint(d, "tc_num", dcb_info.nb_tcs);

	if (dcb_info.nb_tcs > 0)
		return eth_dev_add_dcb_tc(&dcb_info, d);

	return 0;
}

static int
eth_dev_handle_port_dcb(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	uint16_t port_id;
	char *end_param;
	int ret;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	return eth_dev_add_dcb_info(port_id, d);
}

static int
eth_dev_add_rss_info(struct rte_eth_rss_conf *rss_conf, struct rte_tel_data *d)
{
	const uint32_t key_len = rss_conf->rss_key_len * 2 + 1;
	char *rss_key;
	char key[3]; /* FF\0 */
	uint32_t i;
	int ret;

	rss_key = malloc(key_len);
	if (rss_key == NULL)
		return -ENOMEM;

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_uint_hex(d, "rss_hf", rss_conf->rss_hf, 0);
	rte_tel_data_add_dict_uint(d, "rss_key_len", rss_conf->rss_key_len);

	memset(rss_key, 0, key_len);
	for (i = 0; i < rss_conf->rss_key_len; i++) {
		ret = snprintf(key, 3, "%02x", rss_conf->rss_key[i]);
		if (ret < 0)
			goto free_rss_key;
		strlcat(rss_key, key, key_len);
	}
	ret = rte_tel_data_add_dict_string(d, "rss_key", rss_key);

free_rss_key:
	free(rss_key);

	return ret;
}

static int
eth_dev_handle_port_rss_info(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rss_conf rss_conf;
	uint8_t key_len;
	uint16_t port_id;
	char *end_param;
	int ret;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Failed to get device info, ret = %d\n", ret);
		return ret;
	}

	key_len = dev_info.hash_key_size ? dev_info.hash_key_size : 40;
	rss_conf.rss_key_len = key_len;
	rss_conf.rss_key = malloc(key_len);
	if (rss_conf.rss_key == NULL)
		return -ENOMEM;

	ret = rte_eth_dev_rss_hash_conf_get(port_id, &rss_conf);
	if (ret != 0) {
		free(rss_conf.rss_key);
		return ret;
	}

	ret = eth_dev_add_rss_info(&rss_conf, d);
	free(rss_conf.rss_key);
	return ret;
}

static const char *
eth_dev_fec_capa_to_string(uint32_t fec_capa)
{
	uint32_t i;

	for (i = 0; i < RTE_DIM(rte_eth_fec_capa_name); i++) {
		if ((fec_capa & rte_eth_fec_capa_name[i].capa) != 0)
			return rte_eth_fec_capa_name[i].name;
	}

	return "unknown";
}

static void
eth_dev_fec_capas_to_string(uint32_t fec_capa, char *fec_name, uint32_t len)
{
	bool valid = false;
	size_t count = 0;
	uint32_t i;

	for (i = 0; i < RTE_DIM(rte_eth_fec_capa_name); i++) {
		if ((fec_capa & rte_eth_fec_capa_name[i].capa) != 0) {
			strlcat(fec_name, rte_eth_fec_capa_name[i].name, len);
			count = strlcat(fec_name, " ", len);
			valid = true;
		}
	}

	if (!valid)
		count = snprintf(fec_name, len, "unknown ");

	if (count >= len) {
		RTE_ETHDEV_LOG(WARNING, "FEC capa names may be truncated\n");
		count = len;
	}

	fec_name[count - 1] = '\0';
}

static int
eth_dev_get_fec_capability(uint16_t port_id, struct rte_tel_data *d)
{
	struct rte_eth_fec_capa *speed_fec_capa;
	char fec_name[RTE_TEL_MAX_STRING_LEN];
	char speed[RTE_TEL_MAX_STRING_LEN];
	uint32_t capa_num;
	uint32_t i, j;
	int ret;

	ret = rte_eth_fec_get_capability(port_id, NULL, 0);
	if (ret <= 0)
		return ret == 0 ? -EINVAL : ret;

	capa_num = ret;
	speed_fec_capa = calloc(capa_num, sizeof(struct rte_eth_fec_capa));
	if (speed_fec_capa == NULL)
		return -ENOMEM;

	ret = rte_eth_fec_get_capability(port_id, speed_fec_capa, capa_num);
	if (ret <= 0) {
		ret = ret == 0 ? -EINVAL : ret;
		goto out;
	}

	for (i = 0; i < capa_num; i++) {
		memset(fec_name, 0, RTE_TEL_MAX_STRING_LEN);
		eth_dev_fec_capas_to_string(speed_fec_capa[i].capa, fec_name,
					    RTE_TEL_MAX_STRING_LEN);

		memset(speed, 0, RTE_TEL_MAX_STRING_LEN);
		ret = snprintf(speed, RTE_TEL_MAX_STRING_LEN, "%s",
			rte_eth_link_speed_to_str(speed_fec_capa[i].speed));
		if (ret < 0)
			goto out;

		for (j = 0; j < strlen(speed); j++) {
			if (speed[j] == ' ')
				speed[j] = '_';
		}

		rte_tel_data_add_dict_string(d, speed, fec_name);
	}

out:
	free(speed_fec_capa);
	return ret > 0 ? 0 : ret;
}

static int
eth_dev_handle_port_fec(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_tel_data *fec_capas;
	uint32_t fec_mode;
	uint16_t port_id;
	char *end_param;
	int ret;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	ret = rte_eth_fec_get(port_id, &fec_mode);
	if (ret != 0)
		return ret;

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_string(d, "fec_mode",
				     eth_dev_fec_capa_to_string(fec_mode));

	fec_capas = rte_tel_data_alloc();
	if (fec_capas == NULL)
		return -ENOMEM;

	rte_tel_data_start_dict(fec_capas);
	ret = eth_dev_get_fec_capability(port_id, fec_capas);
	if (ret != 0) {
		rte_tel_data_free(fec_capas);
		return ret;
	}

	rte_tel_data_add_dict_container(d, "fec_capability", fec_capas, 0);
	return 0;
}

static int
eth_dev_add_vlan_id(int port_id, struct rte_tel_data *d)
{
	struct rte_tel_data *vlan_blks[64] = {NULL};
	uint16_t vlan_num, vidx, vbit, num_blks;
	char blk_name[RTE_TEL_MAX_STRING_LEN];
	struct rte_vlan_filter_conf *vfc;
	struct rte_tel_data *vlan_blk;
	struct rte_tel_data *vd;
	uint64_t bit_width;
	uint64_t vlan_id;

	vd = rte_tel_data_alloc();
	if (vd == NULL)
		return -ENOMEM;

	vfc = &rte_eth_devices[port_id].data->vlan_filter_conf;
	bit_width = CHAR_BIT * sizeof(uint64_t);
	vlan_num = 0;
	num_blks = 0;

	rte_tel_data_start_dict(vd);
	for (vidx = 0; vidx < RTE_DIM(vfc->ids); vidx++) {
		if (vfc->ids[vidx] == 0)
			continue;

		vlan_blk = rte_tel_data_alloc();
		if (vlan_blk == NULL)
			goto free_all;

		vlan_blks[num_blks] = vlan_blk;
		num_blks++;
		snprintf(blk_name, RTE_TEL_MAX_STRING_LEN, "vlan_%"PRIu64"_to_%"PRIu64"",
			 bit_width * vidx, bit_width * (vidx + 1) - 1);
		rte_tel_data_start_array(vlan_blk, RTE_TEL_UINT_VAL);
		rte_tel_data_add_dict_container(vd, blk_name, vlan_blk, 0);

		for (vbit = 0; vbit < bit_width; vbit++) {
			if ((vfc->ids[vidx] & RTE_BIT64(vbit)) == 0)
				continue;

			vlan_id = bit_width * vidx + vbit;
			rte_tel_data_add_array_uint(vlan_blk, vlan_id);
			vlan_num++;
		}
	}

	rte_tel_data_add_dict_uint(d, "vlan_num", vlan_num);
	rte_tel_data_add_dict_container(d, "vlan_ids", vd, 0);

	return 0;

free_all:
	while (num_blks-- > 0)
		rte_tel_data_free(vlan_blks[num_blks]);

	rte_tel_data_free(vd);
	return -ENOMEM;
}

static int
eth_dev_handle_port_vlan(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_eth_txmode *txmode;
	struct rte_eth_conf dev_conf;
	uint16_t port_id;
	int offload, ret;
	char *end_param;

	ret = eth_dev_parse_port_params(params, &port_id, &end_param, false);
	if (ret < 0)
		return ret;

	ret = rte_eth_dev_conf_get(port_id, &dev_conf);
	if (ret != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Failed to get device configuration, ret = %d\n", ret);
		return ret;
	}

	txmode = &dev_conf.txmode;
	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_uint(d, "pvid", txmode->pvid);
	rte_tel_data_add_dict_uint(d, "hw_vlan_reject_tagged",
		txmode->hw_vlan_reject_tagged);
	rte_tel_data_add_dict_uint(d, "hw_vlan_reject_untagged",
		txmode->hw_vlan_reject_untagged);
	rte_tel_data_add_dict_uint(d, "hw_vlan_insert_pvid",
		txmode->hw_vlan_insert_pvid);

	offload = rte_eth_dev_get_vlan_offload(port_id);
	rte_tel_data_add_dict_string(d, "VLAN_STRIP",
		((offload & RTE_ETH_VLAN_STRIP_OFFLOAD) != 0) ? "on" : "off");
	rte_tel_data_add_dict_string(d, "VLAN_EXTEND",
		((offload & RTE_ETH_VLAN_EXTEND_OFFLOAD) != 0) ? "on" : "off");
	rte_tel_data_add_dict_string(d, "QINQ_STRIP",
		((offload & RTE_ETH_QINQ_STRIP_OFFLOAD) != 0) ? "on" : "off");
	rte_tel_data_add_dict_string(d, "VLAN_FILTER",
		((offload & RTE_ETH_VLAN_FILTER_OFFLOAD) != 0) ? "on" : "off");

	return eth_dev_add_vlan_id(port_id, d);
}

RTE_INIT(ethdev_init_telemetry)
{
	rte_telemetry_register_cmd("/ethdev/list", eth_dev_handle_port_list,
			"Returns list of available ethdev ports. Takes no parameters");
	rte_telemetry_register_cmd("/ethdev/stats", eth_dev_handle_port_stats,
			"Returns the common stats for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/xstats", eth_dev_handle_port_xstats,
			"Returns the extended stats for a port. Parameters: int port_id,hide_zero=true|false(Optional for indicates hide zero xstats)");
#ifndef RTE_EXEC_ENV_WINDOWS
	rte_telemetry_register_cmd("/ethdev/dump_priv", eth_dev_handle_port_dump_priv,
			"Returns dump private information for a port. Parameters: int port_id");
#endif
	rte_telemetry_register_cmd("/ethdev/link_status",
			eth_dev_handle_port_link_status,
			"Returns the link status for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/info", eth_dev_handle_port_info,
			"Returns the device info for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/module_eeprom", eth_dev_handle_port_module_eeprom,
			"Returns module EEPROM info with SFF specs. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/macs", eth_dev_handle_port_macs,
			"Returns the MAC addresses for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/flow_ctrl", eth_dev_handle_port_flow_ctrl,
			"Returns flow ctrl info for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/rx_queue", eth_dev_handle_port_rxq,
			"Returns Rx queue info for a port. Parameters: int port_id, int queue_id (Optional if only one queue)");
	rte_telemetry_register_cmd("/ethdev/tx_queue", eth_dev_handle_port_txq,
			"Returns Tx queue info for a port. Parameters: int port_id, int queue_id (Optional if only one queue)");
	rte_telemetry_register_cmd("/ethdev/dcb", eth_dev_handle_port_dcb,
			"Returns DCB info for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/rss_info", eth_dev_handle_port_rss_info,
			"Returns RSS info for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/fec", eth_dev_handle_port_fec,
			"Returns FEC info for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/vlan", eth_dev_handle_port_vlan,
			"Returns VLAN info for a port. Parameters: int port_id");
}
