/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <rte_bitops.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>

#include "ethdev_priv.h"
#include "module_api.h"

static const char
cmd_ethdev_mtu_help[] = "ethdev <ethdev_name> mtu <mtu_sz>";

static const char
cmd_ethdev_prom_mode_help[] = "ethdev <ethdev_name> promiscuous <on/off>";

static const char
cmd_ethdev_help[] = "ethdev <ethdev_name> rxq <n_queues> txq <n_queues> <mempool_name>";

static const char
cmd_ethdev_stats_help[] = "ethdev <ethdev_name> stats";

static const char
cmd_ethdev_show_help[] = "ethdev <ethdev_name> show";

static const char
cmd_ethdev_ip4_addr_help[] = "ethdev <ethdev_name> ip4 addr add <ip> netmask <mask>";

static const char
cmd_ethdev_ip6_addr_help[] = "ethdev <ethdev_name> ip6 addr add <ip> netmask <mask>";

static const char
cmd_ethdev_forward_help[] = "ethdev forward <tx_dev_name> <rx_dev_name>";

static struct rte_eth_conf port_conf_default = {
	.link_speeds = 0,
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
		.mtu = 9000 - (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN), /* Jumbo frame MTU */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_key_len = 40,
			.rss_hf = 0,
		},
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
	.lpbk_mode = 0,
};

uint32_t enabled_port_mask;
static struct ethdev_head eth_node = TAILQ_HEAD_INITIALIZER(eth_node);


static struct ethdev*
ethdev_port_by_id(uint16_t port_id)
{
	struct ethdev *port;

	TAILQ_FOREACH(port, &eth_node, next) {
		if (port->config.port_id == port_id)
			return port;
	}
	return NULL;
}

int16_t
ethdev_txport_by_rxport_get(uint16_t portid_rx)
{
	int portid = -EINVAL;
	struct ethdev *port;

	port = ethdev_port_by_id(portid_rx);
	if (port)
		portid = port->tx_port_id;

	return portid;
}

void *
ethdev_mempool_list_by_portid(uint16_t portid)
{
	struct ethdev *port;

	if (portid >= RTE_MAX_ETHPORTS)
		return NULL;

	port = ethdev_port_by_id(portid);
	if (port)
		return &(port->config.rx.mp);
	else
		return NULL;
}

int16_t
ethdev_portid_by_ip4(uint32_t ip, uint32_t mask)
{
	int portid = -EINVAL;
	struct ethdev *port;

	TAILQ_FOREACH(port, &eth_node, next) {
		if (mask == 0) {
			if ((port->ip4_addr.ip & port->ip4_addr.mask) == (ip & port->ip4_addr.mask))
				return port->config.port_id;
		} else {
			if ((port->ip4_addr.ip & port->ip4_addr.mask) == (ip & mask))
				return port->config.port_id;
		}
	}

	return portid;
}

int16_t
ethdev_portid_by_ip6(struct rte_ipv6_addr *ip, struct rte_ipv6_addr *mask)
{
	struct ethdev *port;

	TAILQ_FOREACH(port, &eth_node, next) {
		uint8_t depth = rte_ipv6_mask_depth(&port->ip6_addr.mask);
		if (mask != NULL)
			depth = RTE_MAX(depth, rte_ipv6_mask_depth(mask));
		if (rte_ipv6_addr_eq_prefix(&port->ip6_addr.ip, ip, depth))
			return port->config.port_id;
	}

	return -EINVAL;
}

void
ethdev_list_clean(void)
{
	struct ethdev *port;

	while (!TAILQ_EMPTY(&eth_node)) {
		port = TAILQ_FIRST(&eth_node);
		TAILQ_REMOVE(&eth_node, port, next);
	}
}

void
ethdev_stop(void)
{
	uint16_t portid;
	int rc;

	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("Closing port %d...", portid);
		rc = rte_eth_dev_stop(portid);
		if (rc != 0)
			printf("Failed to stop port %u: %s\n",
					portid, rte_strerror(-rc));
		rte_eth_dev_close(portid);
		printf(" Done\n");
	}

	ethdev_list_clean();
	route_ip4_list_clean();
	route_ip6_list_clean();
	neigh4_list_clean();
	neigh6_list_clean();
	printf("Bye...\n");
}

void
ethdev_start(void)
{
	uint16_t portid;
	int rc;

	RTE_ETH_FOREACH_DEV(portid)
	{
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		rc = rte_eth_dev_start(portid);
		if (rc < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, port=%d\n", rc, portid);
	}
}

static int
ethdev_show(const char *name)
{
	uint16_t mtu = 0, port_id = 0;
	struct rte_eth_dev_info info;
	struct rte_eth_stats stats;
	struct rte_ether_addr addr;
	struct rte_eth_link link;
	uint32_t length;
	int rc;

	rc = rte_eth_dev_get_port_by_name(name, &port_id);
	if (rc < 0)
		return rc;

	rc = rte_eth_dev_info_get(port_id, &info);
	if (rc < 0)
		return rc;

	rc = rte_eth_link_get(port_id, &link);
	if (rc < 0)
		return rc;

	rc = rte_eth_stats_get(port_id, &stats);
	if (rc < 0)
		return rc;

	rc = rte_eth_macaddr_get(port_id, &addr);
	if (rc < 0)
		return rc;

	rte_eth_dev_get_mtu(port_id, &mtu);

	length = strlen(conn->msg_out);
	conn->msg_out += length;
	snprintf(conn->msg_out, conn->msg_out_len_max,
		 "%s: flags=<%s> mtu %u\n"
		 "\tether " RTE_ETHER_ADDR_PRT_FMT " rxqueues %u txqueues %u\n"
		 "\tport# %u  speed %s\n"
		 "\tRX packets %" PRIu64"  bytes %" PRIu64"\n"
		 "\tRX errors %" PRIu64"  missed %" PRIu64"  no-mbuf %" PRIu64"\n"
		 "\tTX packets %" PRIu64"  bytes %" PRIu64"\n"
		 "\tTX errors %" PRIu64"\n\n",
		 name,
		 link.link_status ? "UP" : "DOWN",
		 mtu,
		 RTE_ETHER_ADDR_BYTES(&addr),
		 info.nb_rx_queues,
		 info.nb_tx_queues,
		 port_id,
		 rte_eth_link_speed_to_str(link.link_speed),
		 stats.ipackets,
		 stats.ibytes,
		 stats.ierrors,
		 stats.imissed,
		 stats.rx_nombuf,
		 stats.opackets,
		 stats.obytes,
		 stats.oerrors);

	length = strlen(conn->msg_out);
	conn->msg_out_len_max -= length;
	return 0;
}

static int
ethdev_ip4_addr_add(const char *name, struct ipv4_addr_config *config)
{
	struct ethdev *eth_hdl;
	uint16_t portid = 0;
	int rc;

	rc = rte_eth_dev_get_port_by_name(name, &portid);
	if (rc < 0)
		return rc;

	eth_hdl = ethdev_port_by_id(portid);

	if (eth_hdl) {
		eth_hdl->ip4_addr.ip = config->ip;
		eth_hdl->ip4_addr.mask = config->mask;
		return 0;
	}

	rc = -EINVAL;
	return rc;
}

static int
ethdev_ip6_addr_add(const char *name, struct ipv6_addr_config *config)
{
	struct ethdev *eth_hdl;
	uint16_t portid = 0;
	int rc;

	rc = rte_eth_dev_get_port_by_name(name, &portid);
	if (rc < 0)
		return rc;

	eth_hdl = ethdev_port_by_id(portid);

	if (eth_hdl) {
		eth_hdl->ip6_addr.ip = config->ip;
		eth_hdl->ip6_addr.mask = config->mask;
		return 0;
	}
	rc = -EINVAL;
	return rc;
}

static int
ethdev_prom_mode_config(const char *name, bool enable)
{
	struct ethdev *eth_hdl;
	uint16_t portid = 0;
	int rc;

	rc = rte_eth_dev_get_port_by_name(name, &portid);
	if (rc < 0)
		return rc;

	eth_hdl = ethdev_port_by_id(portid);

	if (eth_hdl) {
		if (enable)
			rc = rte_eth_promiscuous_enable(portid);
		else
			rc = rte_eth_promiscuous_disable(portid);
		if (rc < 0)
			return rc;

		eth_hdl->config.promiscuous = enable;
		return 0;
	}

	rc = -EINVAL;
	return rc;
}

static int
ethdev_mtu_config(const char *name, uint32_t mtu)
{
	struct ethdev *eth_hdl;
	uint16_t portid = 0;
	int rc;

	rc = rte_eth_dev_get_port_by_name(name, &portid);
	if (rc < 0)
		return rc;

	eth_hdl = ethdev_port_by_id(portid);

	if (eth_hdl) {
		rc = rte_eth_dev_set_mtu(portid, mtu);
		if (rc < 0)
			return rc;

		eth_hdl->config.mtu = mtu;
		return 0;
	}

	rc = -EINVAL;
	return rc;
}

static int
ethdev_process(const char *name, struct ethdev_config *params)
{
	struct rte_eth_dev_info port_info;
	struct rte_eth_conf port_conf;
	struct ethdev_rss_config *rss;
	struct rte_mempool *mempool;
	struct ethdev *ethdev_port;
	struct rte_ether_addr smac;
	uint16_t port_id = 0;
	int numa_node, rc;
	uint32_t i;

	/* Check input params */
	if (!name || !name[0] || !params || !params->rx.n_queues || !params->rx.queue_size ||
	    !params->tx.n_queues || !params->tx.queue_size)
		return -EINVAL;

	rc = rte_eth_dev_get_port_by_name(name, &port_id);
	if (rc)
		return -EINVAL;

	if (!ethdev_port_by_id(port_id)) {
		ethdev_port = malloc(sizeof(struct ethdev));
		if (!ethdev_port)
			return -EINVAL;
	} else {
		return 0;
	}

	rc = rte_eth_dev_info_get(port_id, &port_info);
	if (rc) {
		rc = -EINVAL;
		goto exit;
	}

	mempool = rte_mempool_lookup(params->rx.mempool_name);
	if (!mempool) {
		rc =  -EINVAL;
		goto exit;
	}

	params->rx.mp = mempool;

	rss = params->rx.rss;
	if (rss) {
		if (!port_info.reta_size || port_info.reta_size > RTE_ETH_RSS_RETA_SIZE_512) {
			rc = -EINVAL;
			goto exit;
		}

		if (!rss->n_queues || rss->n_queues >= ETHDEV_RXQ_RSS_MAX) {
			rc = -EINVAL;
			goto exit;
		}

		for (i = 0; i < rss->n_queues; i++)
			if (rss->queue_id[i] >= port_info.max_rx_queues) {
				rc = -EINVAL;
				goto exit;
			}
	}

	/* Port */
	port_conf = port_conf_default;
	if (rss) {
		uint64_t rss_hf = RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP;

		port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
		port_conf.rx_adv_conf.rss_conf.rss_hf = rss_hf & port_info.flow_type_rss_offloads;
	}

	numa_node = rte_eth_dev_socket_id(port_id);
	if (numa_node == SOCKET_ID_ANY)
		numa_node = 0;

	if (params->mtu)
		port_conf.rxmode.mtu = params->mtu;

	rc = rte_eth_dev_configure(port_id, params->rx.n_queues, params->tx.n_queues,
				       &port_conf);
	if (rc < 0) {
		rc = -EINVAL;
		goto exit;
	}

	rc = rte_eth_macaddr_get(port_id, &smac);
	if (rc < 0) {
		rc = -EINVAL;
		goto exit;
	}

	printf("Port_id = %d srcmac = %x:%x:%x:%x:%x:%x\n", port_id,
		smac.addr_bytes[0], smac.addr_bytes[1],
		smac.addr_bytes[2], smac.addr_bytes[3],
		smac.addr_bytes[4], smac.addr_bytes[5]);

	/* Port RX */
	for (i = 0; i < params->rx.n_queues; i++) {
		rc = rte_eth_rx_queue_setup(port_id, i, params->rx.queue_size, numa_node, NULL,
			mempool);
		if (rc < 0) {
			rc = -EINVAL;
			goto exit;
		}
	}

	/* Port TX */
	for (i = 0; i < params->tx.n_queues; i++) {
		rc = rte_eth_tx_queue_setup(port_id, i, params->tx.queue_size, numa_node, NULL);
		if (rc < 0) {
			rc = -EINVAL;
			goto exit;
		}
	}

	memcpy(&ethdev_port->config, params, sizeof(struct ethdev_config));
	memcpy(ethdev_port->config.dev_name, name, strlen(name));
	ethdev_port->config.port_id = port_id;
	enabled_port_mask |= RTE_BIT32(port_id);

	TAILQ_INSERT_TAIL(&eth_node, ethdev_port, next);
	return 0;
exit:
	free(ethdev_port);
	return rc;

}

static int
ethdev_stats_show(const char *name)
{
	uint64_t diff_pkts_rx, diff_pkts_tx, diff_bytes_rx, diff_bytes_tx;
	static uint64_t prev_pkts_rx[RTE_MAX_ETHPORTS];
	static uint64_t prev_pkts_tx[RTE_MAX_ETHPORTS];
	static uint64_t prev_bytes_rx[RTE_MAX_ETHPORTS];
	static uint64_t prev_bytes_tx[RTE_MAX_ETHPORTS];
	static uint64_t prev_cycles[RTE_MAX_ETHPORTS];
	uint64_t mpps_rx, mpps_tx, mbps_rx, mbps_tx;
	uint64_t diff_ns, diff_cycles, curr_cycles;
	struct rte_eth_stats stats;
	static const char *nic_stats_border = "########################";
	uint16_t port_id, len;
	int rc;

	rc = rte_eth_dev_get_port_by_name(name, &port_id);
	if (rc < 0)
		return rc;

	rc = rte_eth_stats_get(port_id, &stats);
	if (rc != 0) {
		fprintf(stderr,
			"%s: Error: failed to get stats (port %u): %d",
			__func__, port_id, rc);
		return rc;
	}

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max,
		 "\n  %s NIC statistics for port %-2d %s\n"
		 "  RX-packets: %-10"PRIu64" RX-missed: %-10"PRIu64" RX-bytes:  ""%-"PRIu64"\n"
		 "  RX-errors: %-"PRIu64"\n"
		 "  RX-nombuf:  %-10"PRIu64"\n"
		 "  TX-packets: %-10"PRIu64" TX-errors: %-10"PRIu64" TX-bytes:  ""%-"PRIu64"\n",
		 nic_stats_border, port_id, nic_stats_border, stats.ipackets, stats.imissed,
		 stats.ibytes, stats.ierrors, stats.rx_nombuf, stats.opackets, stats.oerrors,
		 stats.obytes);

	len = strlen(conn->msg_out) - len;
	conn->msg_out_len_max -= len;

	diff_ns = 0;
	diff_cycles = 0;

	curr_cycles = rte_rdtsc();
	if (prev_cycles[port_id] != 0)
		diff_cycles = curr_cycles - prev_cycles[port_id];

	prev_cycles[port_id] = curr_cycles;
	diff_ns = diff_cycles > 0 ?
		diff_cycles * (1 / (double)rte_get_tsc_hz()) * NS_PER_SEC : 0;

	diff_pkts_rx = (stats.ipackets > prev_pkts_rx[port_id]) ?
		(stats.ipackets - prev_pkts_rx[port_id]) : 0;
	diff_pkts_tx = (stats.opackets > prev_pkts_tx[port_id]) ?
		(stats.opackets - prev_pkts_tx[port_id]) : 0;
	prev_pkts_rx[port_id] = stats.ipackets;
	prev_pkts_tx[port_id] = stats.opackets;
	mpps_rx = diff_ns > 0 ?
		(double)diff_pkts_rx / diff_ns * NS_PER_SEC : 0;
	mpps_tx = diff_ns > 0 ?
		(double)diff_pkts_tx / diff_ns * NS_PER_SEC : 0;

	diff_bytes_rx = (stats.ibytes > prev_bytes_rx[port_id]) ?
		(stats.ibytes - prev_bytes_rx[port_id]) : 0;
	diff_bytes_tx = (stats.obytes > prev_bytes_tx[port_id]) ?
		(stats.obytes - prev_bytes_tx[port_id]) : 0;
	prev_bytes_rx[port_id] = stats.ibytes;
	prev_bytes_tx[port_id] = stats.obytes;
	mbps_rx = diff_ns > 0 ?
		(double)diff_bytes_rx / diff_ns * NS_PER_SEC : 0;
	mbps_tx = diff_ns > 0 ?
		(double)diff_bytes_tx / diff_ns * NS_PER_SEC : 0;

	len = strlen(conn->msg_out);
	snprintf(conn->msg_out + len, conn->msg_out_len_max,
		 "\n  Throughput (since last show)\n"
		 "  Rx-pps: %12"PRIu64"          Rx-bps: %12"PRIu64"\n  Tx-pps: %12"
		 PRIu64"          Tx-bps: %12"PRIu64"\n"
		 "  %s############################%s\n",
		 mpps_rx, mbps_rx * 8, mpps_tx, mbps_tx * 8, nic_stats_border, nic_stats_border);
	return 0;
}

void
cmd_ethdev_dev_mtu_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
			  void *data __rte_unused)
{
	struct cmd_ethdev_dev_mtu_result *res = parsed_result;
	int rc = -EINVAL;

	rc = ethdev_mtu_config(res->dev, res->size);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->ethdev);
}

void
cmd_ethdev_dev_promiscuous_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
				  void *data __rte_unused)
{
	struct cmd_ethdev_dev_promiscuous_result *res = parsed_result;
	bool enable = false;
	int rc = -EINVAL;

	if (!strcmp(res->enable, "on"))
		enable = true;

	rc = ethdev_prom_mode_config(res->dev, enable);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->ethdev);
}

void
cmd_ethdev_dev_ip4_addr_add_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
				   void *data __rte_unused)
{
	struct cmd_ethdev_dev_ip4_addr_add_result *res = parsed_result;
	struct ipv4_addr_config config;
	int rc = -EINVAL;

	config.ip = rte_be_to_cpu_32(res->ip.addr.ipv4.s_addr);
	config.mask = rte_be_to_cpu_32(res->mask.addr.ipv4.s_addr);

	rc = ethdev_ip4_addr_add(res->dev, &config);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->ethdev);
}

void
cmd_ethdev_dev_ip6_addr_add_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
				   void *data __rte_unused)
{
	struct cmd_ethdev_dev_ip6_addr_add_result *res = parsed_result;
	struct ipv6_addr_config config = {
		.ip = res->ip.addr.ipv6,
		.mask = res->mask.addr.ipv6,
	};
	int rc;

	rc = ethdev_ip6_addr_add(res->dev, &config);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->ethdev);
}

void
cmd_ethdev_dev_show_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
			   void *data __rte_unused)
{
	struct cmd_ethdev_dev_show_result *res = parsed_result;
	int rc = -EINVAL;

	rc = ethdev_show(res->dev);
	if (rc < 0)
		printf(MSG_ARG_INVALID, res->dev);
}

void
cmd_ethdev_dev_stats_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
			    void *data __rte_unused)
{
	struct cmd_ethdev_dev_stats_result *res = parsed_result;
	int rc = -EINVAL;

	rc = ethdev_stats_show(res->dev);
	if (rc < 0)
		printf(MSG_ARG_INVALID, res->dev);
}

void
cmd_ethdev_parsed(void *parsed_result, __rte_unused struct cmdline *cl, void *data __rte_unused)
{
	struct cmd_ethdev_result *res = parsed_result;
	struct ethdev_config config;
	int rc;

	memset(&config, 0, sizeof(struct ethdev_config));
	config.rx.n_queues = res->nb_rxq;
	config.rx.queue_size = ETHDEV_RX_DESC_DEFAULT;
	memcpy(config.rx.mempool_name, res->mempool, strlen(res->mempool));

	config.tx.n_queues = res->nb_txq;
	config.tx.queue_size = ETHDEV_TX_DESC_DEFAULT;

	config.mtu = port_conf_default.rxmode.mtu;

	rc = ethdev_process(res->dev, &config);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->ethdev);
}

void
cmd_help_ethdev_parsed(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
		       __rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		 "----------------------------- ethdev command help -----------------------------",
		 cmd_ethdev_help, cmd_ethdev_ip4_addr_help, cmd_ethdev_ip6_addr_help,
		 cmd_ethdev_forward_help, cmd_ethdev_prom_mode_help, cmd_ethdev_mtu_help,
		 cmd_ethdev_stats_help, cmd_ethdev_show_help);

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}

static int
ethdev_forward_config(char *tx_dev, char *rx_dev)
{
	uint16_t portid_rx = 0;
	uint16_t portid_tx = 0;
	struct ethdev *port;
	int rc = -EINVAL;

	rc = rte_eth_dev_get_port_by_name(tx_dev, &portid_tx);
	if (rc < 0)
		return rc;

	rc = rte_eth_dev_get_port_by_name(rx_dev, &portid_rx);
	if (rc < 0)
		return rc;

	port = ethdev_port_by_id(portid_rx);
	if (port) {
		port->tx_port_id = portid_tx;
		rc = 0;
	} else {
		rc = -EINVAL;
	}

	return rc;
}

void
cmd_ethdev_forward_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
			  void *data __rte_unused)
{
	struct cmd_ethdev_forward_result *res = parsed_result;
	int rc = -EINVAL;

	rc = ethdev_forward_config(res->tx_dev, res->rx_dev);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->ethdev);
}
