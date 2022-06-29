/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation.
 * Copyright 2013-2014 6WIND S.A.
 */

#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_debug.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_flow.h>
#include <rte_mtr.h>
#include <rte_errno.h>
#ifdef RTE_NET_IXGBE
#include <rte_pmd_ixgbe.h>
#endif
#ifdef RTE_NET_I40E
#include <rte_pmd_i40e.h>
#endif
#ifdef RTE_NET_BNXT
#include <rte_pmd_bnxt.h>
#endif
#ifdef RTE_LIB_GRO
#include <rte_gro.h>
#endif
#include <rte_hexdump.h>

#include "testpmd.h"
#include "cmdline_mtr.h"

#define ETHDEV_FWVERS_LEN 32

#ifdef CLOCK_MONOTONIC_RAW /* Defined in glibc bits/time.h */
#define CLOCK_TYPE_ID CLOCK_MONOTONIC_RAW
#else
#define CLOCK_TYPE_ID CLOCK_MONOTONIC
#endif

#define NS_PER_SEC 1E9

static const struct {
	enum tx_pkt_split split;
	const char *name;
} tx_split_name[] = {
	{
		.split = TX_PKT_SPLIT_OFF,
		.name = "off",
	},
	{
		.split = TX_PKT_SPLIT_ON,
		.name = "on",
	},
	{
		.split = TX_PKT_SPLIT_RND,
		.name = "rand",
	},
};

const struct rss_type_info rss_type_table[] = {
	{ "all", RTE_ETH_RSS_ETH | RTE_ETH_RSS_VLAN | RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP |
		RTE_ETH_RSS_UDP | RTE_ETH_RSS_SCTP | RTE_ETH_RSS_L2_PAYLOAD |
		RTE_ETH_RSS_L2TPV3 | RTE_ETH_RSS_ESP | RTE_ETH_RSS_AH | RTE_ETH_RSS_PFCP |
		RTE_ETH_RSS_GTPU | RTE_ETH_RSS_ECPRI | RTE_ETH_RSS_MPLS},
	{ "none", 0 },
	{ "eth", RTE_ETH_RSS_ETH },
	{ "l2-src-only", RTE_ETH_RSS_L2_SRC_ONLY },
	{ "l2-dst-only", RTE_ETH_RSS_L2_DST_ONLY },
	{ "vlan", RTE_ETH_RSS_VLAN },
	{ "s-vlan", RTE_ETH_RSS_S_VLAN },
	{ "c-vlan", RTE_ETH_RSS_C_VLAN },
	{ "ipv4", RTE_ETH_RSS_IPV4 },
	{ "ipv4-frag", RTE_ETH_RSS_FRAG_IPV4 },
	{ "ipv4-tcp", RTE_ETH_RSS_NONFRAG_IPV4_TCP },
	{ "ipv4-udp", RTE_ETH_RSS_NONFRAG_IPV4_UDP },
	{ "ipv4-sctp", RTE_ETH_RSS_NONFRAG_IPV4_SCTP },
	{ "ipv4-other", RTE_ETH_RSS_NONFRAG_IPV4_OTHER },
	{ "ipv6", RTE_ETH_RSS_IPV6 },
	{ "ipv6-frag", RTE_ETH_RSS_FRAG_IPV6 },
	{ "ipv6-tcp", RTE_ETH_RSS_NONFRAG_IPV6_TCP },
	{ "ipv6-udp", RTE_ETH_RSS_NONFRAG_IPV6_UDP },
	{ "ipv6-sctp", RTE_ETH_RSS_NONFRAG_IPV6_SCTP },
	{ "ipv6-other", RTE_ETH_RSS_NONFRAG_IPV6_OTHER },
	{ "l2-payload", RTE_ETH_RSS_L2_PAYLOAD },
	{ "ipv6-ex", RTE_ETH_RSS_IPV6_EX },
	{ "ipv6-tcp-ex", RTE_ETH_RSS_IPV6_TCP_EX },
	{ "ipv6-udp-ex", RTE_ETH_RSS_IPV6_UDP_EX },
	{ "port", RTE_ETH_RSS_PORT },
	{ "vxlan", RTE_ETH_RSS_VXLAN },
	{ "geneve", RTE_ETH_RSS_GENEVE },
	{ "nvgre", RTE_ETH_RSS_NVGRE },
	{ "ip", RTE_ETH_RSS_IP },
	{ "udp", RTE_ETH_RSS_UDP },
	{ "tcp", RTE_ETH_RSS_TCP },
	{ "sctp", RTE_ETH_RSS_SCTP },
	{ "tunnel", RTE_ETH_RSS_TUNNEL },
	{ "l3-pre32", RTE_ETH_RSS_L3_PRE32 },
	{ "l3-pre40", RTE_ETH_RSS_L3_PRE40 },
	{ "l3-pre48", RTE_ETH_RSS_L3_PRE48 },
	{ "l3-pre56", RTE_ETH_RSS_L3_PRE56 },
	{ "l3-pre64", RTE_ETH_RSS_L3_PRE64 },
	{ "l3-pre96", RTE_ETH_RSS_L3_PRE96 },
	{ "l3-src-only", RTE_ETH_RSS_L3_SRC_ONLY },
	{ "l3-dst-only", RTE_ETH_RSS_L3_DST_ONLY },
	{ "l4-src-only", RTE_ETH_RSS_L4_SRC_ONLY },
	{ "l4-dst-only", RTE_ETH_RSS_L4_DST_ONLY },
	{ "esp", RTE_ETH_RSS_ESP },
	{ "ah", RTE_ETH_RSS_AH },
	{ "l2tpv3", RTE_ETH_RSS_L2TPV3 },
	{ "pfcp", RTE_ETH_RSS_PFCP },
	{ "pppoe", RTE_ETH_RSS_PPPOE },
	{ "gtpu", RTE_ETH_RSS_GTPU },
	{ "ecpri", RTE_ETH_RSS_ECPRI },
	{ "mpls", RTE_ETH_RSS_MPLS },
	{ "ipv4-chksum", RTE_ETH_RSS_IPV4_CHKSUM },
	{ "l4-chksum", RTE_ETH_RSS_L4_CHKSUM },
	{ NULL, 0 },
};

static const struct {
	enum rte_eth_fec_mode mode;
	const char *name;
} fec_mode_name[] = {
	{
		.mode = RTE_ETH_FEC_NOFEC,
		.name = "off",
	},
	{
		.mode = RTE_ETH_FEC_AUTO,
		.name = "auto",
	},
	{
		.mode = RTE_ETH_FEC_BASER,
		.name = "baser",
	},
	{
		.mode = RTE_ETH_FEC_RS,
		.name = "rs",
	},
};

static void
print_ethaddr(const char *name, struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

static void
nic_xstats_display_periodic(portid_t port_id)
{
	struct xstat_display_info *xstats_info;
	uint64_t *prev_values, *curr_values;
	uint64_t diff_value, value_rate;
	struct timespec cur_time;
	uint64_t *ids_supp;
	size_t ids_supp_sz;
	uint64_t diff_ns;
	unsigned int i;
	int rc;

	xstats_info = &ports[port_id].xstats_info;

	ids_supp_sz = xstats_info->ids_supp_sz;
	if (ids_supp_sz == 0)
		return;

	printf("\n");

	ids_supp = xstats_info->ids_supp;
	prev_values = xstats_info->prev_values;
	curr_values = xstats_info->curr_values;

	rc = rte_eth_xstats_get_by_id(port_id, ids_supp, curr_values,
				      ids_supp_sz);
	if (rc != (int)ids_supp_sz) {
		fprintf(stderr,
			"Failed to get values of %zu xstats for port %u - return code %d\n",
			ids_supp_sz, port_id, rc);
		return;
	}

	diff_ns = 0;
	if (clock_gettime(CLOCK_TYPE_ID, &cur_time) == 0) {
		uint64_t ns;

		ns = cur_time.tv_sec * NS_PER_SEC;
		ns += cur_time.tv_nsec;

		if (xstats_info->prev_ns != 0)
			diff_ns = ns - xstats_info->prev_ns;
		xstats_info->prev_ns = ns;
	}

	printf("%-31s%-17s%s\n", " ", "Value", "Rate (since last show)");
	for (i = 0; i < ids_supp_sz; i++) {
		diff_value = (curr_values[i] > prev_values[i]) ?
			     (curr_values[i] - prev_values[i]) : 0;
		prev_values[i] = curr_values[i];
		value_rate = diff_ns > 0 ?
				(double)diff_value / diff_ns * NS_PER_SEC : 0;

		printf("  %-25s%12"PRIu64" %15"PRIu64"\n",
		       xstats_display[i].name, curr_values[i], value_rate);
	}
}

void
nic_stats_display(portid_t port_id)
{
	static uint64_t prev_pkts_rx[RTE_MAX_ETHPORTS];
	static uint64_t prev_pkts_tx[RTE_MAX_ETHPORTS];
	static uint64_t prev_bytes_rx[RTE_MAX_ETHPORTS];
	static uint64_t prev_bytes_tx[RTE_MAX_ETHPORTS];
	static uint64_t prev_ns[RTE_MAX_ETHPORTS];
	struct timespec cur_time;
	uint64_t diff_pkts_rx, diff_pkts_tx, diff_bytes_rx, diff_bytes_tx,
								diff_ns;
	uint64_t mpps_rx, mpps_tx, mbps_rx, mbps_tx;
	struct rte_eth_stats stats;
	static const char *nic_stats_border = "########################";
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		print_valid_ports();
		return;
	}
	ret = rte_eth_stats_get(port_id, &stats);
	if (ret != 0) {
		fprintf(stderr,
			"%s: Error: failed to get stats (port %u): %d",
			__func__, port_id, ret);
		return;
	}
	printf("\n  %s NIC statistics for port %-2d %s\n",
	       nic_stats_border, port_id, nic_stats_border);

	printf("  RX-packets: %-10"PRIu64" RX-missed: %-10"PRIu64" RX-bytes:  "
	       "%-"PRIu64"\n", stats.ipackets, stats.imissed, stats.ibytes);
	printf("  RX-errors: %-"PRIu64"\n", stats.ierrors);
	printf("  RX-nombuf:  %-10"PRIu64"\n", stats.rx_nombuf);
	printf("  TX-packets: %-10"PRIu64" TX-errors: %-10"PRIu64" TX-bytes:  "
	       "%-"PRIu64"\n", stats.opackets, stats.oerrors, stats.obytes);

	diff_ns = 0;
	if (clock_gettime(CLOCK_TYPE_ID, &cur_time) == 0) {
		uint64_t ns;

		ns = cur_time.tv_sec * NS_PER_SEC;
		ns += cur_time.tv_nsec;

		if (prev_ns[port_id] != 0)
			diff_ns = ns - prev_ns[port_id];
		prev_ns[port_id] = ns;
	}

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

	printf("\n  Throughput (since last show)\n");
	printf("  Rx-pps: %12"PRIu64"          Rx-bps: %12"PRIu64"\n  Tx-pps: %12"
	       PRIu64"          Tx-bps: %12"PRIu64"\n", mpps_rx, mbps_rx * 8,
	       mpps_tx, mbps_tx * 8);

	if (xstats_display_num > 0)
		nic_xstats_display_periodic(port_id);

	printf("  %s############################%s\n",
	       nic_stats_border, nic_stats_border);
}

void
nic_stats_clear(portid_t port_id)
{
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		print_valid_ports();
		return;
	}

	ret = rte_eth_stats_reset(port_id);
	if (ret != 0) {
		fprintf(stderr,
			"%s: Error: failed to reset stats (port %u): %s",
			__func__, port_id, strerror(-ret));
		return;
	}

	ret = rte_eth_stats_get(port_id, &ports[port_id].stats);
	if (ret != 0) {
		if (ret < 0)
			ret = -ret;
		fprintf(stderr,
			"%s: Error: failed to get stats (port %u): %s",
			__func__, port_id, strerror(ret));
		return;
	}
	printf("\n  NIC statistics for port %d cleared\n", port_id);
}

void
nic_xstats_display(portid_t port_id)
{
	struct rte_eth_xstat *xstats;
	int cnt_xstats, idx_xstat;
	struct rte_eth_xstat_name *xstats_names;

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		print_valid_ports();
		return;
	}
	printf("###### NIC extended statistics for port %-2d\n", port_id);
	if (!rte_eth_dev_is_valid_port(port_id)) {
		fprintf(stderr, "Error: Invalid port number %i\n", port_id);
		return;
	}

	/* Get count */
	cnt_xstats = rte_eth_xstats_get_names(port_id, NULL, 0);
	if (cnt_xstats  < 0) {
		fprintf(stderr, "Error: Cannot get count of xstats\n");
		return;
	}

	/* Get id-name lookup table */
	xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * cnt_xstats);
	if (xstats_names == NULL) {
		fprintf(stderr, "Cannot allocate memory for xstats lookup\n");
		return;
	}
	if (cnt_xstats != rte_eth_xstats_get_names(
			port_id, xstats_names, cnt_xstats)) {
		fprintf(stderr, "Error: Cannot get xstats lookup\n");
		free(xstats_names);
		return;
	}

	/* Get stats themselves */
	xstats = malloc(sizeof(struct rte_eth_xstat) * cnt_xstats);
	if (xstats == NULL) {
		fprintf(stderr, "Cannot allocate memory for xstats\n");
		free(xstats_names);
		return;
	}
	if (cnt_xstats != rte_eth_xstats_get(port_id, xstats, cnt_xstats)) {
		fprintf(stderr, "Error: Unable to get xstats\n");
		free(xstats_names);
		free(xstats);
		return;
	}

	/* Display xstats */
	for (idx_xstat = 0; idx_xstat < cnt_xstats; idx_xstat++) {
		if (xstats_hide_zero && !xstats[idx_xstat].value)
			continue;
		printf("%s: %"PRIu64"\n",
			xstats_names[idx_xstat].name,
			xstats[idx_xstat].value);
	}
	free(xstats_names);
	free(xstats);
}

void
nic_xstats_clear(portid_t port_id)
{
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		print_valid_ports();
		return;
	}

	ret = rte_eth_xstats_reset(port_id);
	if (ret != 0) {
		fprintf(stderr,
			"%s: Error: failed to reset xstats (port %u): %s\n",
			__func__, port_id, strerror(-ret));
		return;
	}

	ret = rte_eth_stats_get(port_id, &ports[port_id].stats);
	if (ret != 0) {
		if (ret < 0)
			ret = -ret;
		fprintf(stderr, "%s: Error: failed to get stats (port %u): %s",
			__func__, port_id, strerror(ret));
		return;
	}
}

static const char *
get_queue_state_name(uint8_t queue_state)
{
	if (queue_state == RTE_ETH_QUEUE_STATE_STOPPED)
		return "stopped";
	else if (queue_state == RTE_ETH_QUEUE_STATE_STARTED)
		return "started";
	else if (queue_state == RTE_ETH_QUEUE_STATE_HAIRPIN)
		return "hairpin";
	else
		return "unknown";
}

void
rx_queue_infos_display(portid_t port_id, uint16_t queue_id)
{
	struct rte_eth_burst_mode mode;
	struct rte_eth_rxq_info qinfo;
	int32_t rc;
	static const char *info_border = "*********************";

	rc = rte_eth_rx_queue_info_get(port_id, queue_id, &qinfo);
	if (rc != 0) {
		fprintf(stderr,
			"Failed to retrieve information for port: %u, RX queue: %hu\nerror desc: %s(%d)\n",
			port_id, queue_id, strerror(-rc), rc);
		return;
	}

	printf("\n%s Infos for port %-2u, RX queue %-2u %s",
	       info_border, port_id, queue_id, info_border);

	printf("\nMempool: %s", (qinfo.mp == NULL) ? "NULL" : qinfo.mp->name);
	printf("\nRX prefetch threshold: %hhu", qinfo.conf.rx_thresh.pthresh);
	printf("\nRX host threshold: %hhu", qinfo.conf.rx_thresh.hthresh);
	printf("\nRX writeback threshold: %hhu", qinfo.conf.rx_thresh.wthresh);
	printf("\nRX free threshold: %hu", qinfo.conf.rx_free_thresh);
	printf("\nRX drop packets: %s",
		(qinfo.conf.rx_drop_en != 0) ? "on" : "off");
	printf("\nRX deferred start: %s",
		(qinfo.conf.rx_deferred_start != 0) ? "on" : "off");
	printf("\nRX scattered packets: %s",
		(qinfo.scattered_rx != 0) ? "on" : "off");
	printf("\nRx queue state: %s", get_queue_state_name(qinfo.queue_state));
	if (qinfo.rx_buf_size != 0)
		printf("\nRX buffer size: %hu", qinfo.rx_buf_size);
	printf("\nNumber of RXDs: %hu", qinfo.nb_desc);

	if (rte_eth_rx_burst_mode_get(port_id, queue_id, &mode) == 0)
		printf("\nBurst mode: %s%s",
		       mode.info,
		       mode.flags & RTE_ETH_BURST_FLAG_PER_QUEUE ?
				" (per queue)" : "");

	printf("\n");
}

void
tx_queue_infos_display(portid_t port_id, uint16_t queue_id)
{
	struct rte_eth_burst_mode mode;
	struct rte_eth_txq_info qinfo;
	int32_t rc;
	static const char *info_border = "*********************";

	rc = rte_eth_tx_queue_info_get(port_id, queue_id, &qinfo);
	if (rc != 0) {
		fprintf(stderr,
			"Failed to retrieve information for port: %u, TX queue: %hu\nerror desc: %s(%d)\n",
			port_id, queue_id, strerror(-rc), rc);
		return;
	}

	printf("\n%s Infos for port %-2u, TX queue %-2u %s",
	       info_border, port_id, queue_id, info_border);

	printf("\nTX prefetch threshold: %hhu", qinfo.conf.tx_thresh.pthresh);
	printf("\nTX host threshold: %hhu", qinfo.conf.tx_thresh.hthresh);
	printf("\nTX writeback threshold: %hhu", qinfo.conf.tx_thresh.wthresh);
	printf("\nTX RS threshold: %hu", qinfo.conf.tx_rs_thresh);
	printf("\nTX free threshold: %hu", qinfo.conf.tx_free_thresh);
	printf("\nTX deferred start: %s",
		(qinfo.conf.tx_deferred_start != 0) ? "on" : "off");
	printf("\nNumber of TXDs: %hu", qinfo.nb_desc);
	printf("\nTx queue state: %s", get_queue_state_name(qinfo.queue_state));

	if (rte_eth_tx_burst_mode_get(port_id, queue_id, &mode) == 0)
		printf("\nBurst mode: %s%s",
		       mode.info,
		       mode.flags & RTE_ETH_BURST_FLAG_PER_QUEUE ?
				" (per queue)" : "");

	printf("\n");
}

static int bus_match_all(const struct rte_bus *bus, const void *data)
{
	RTE_SET_USED(bus);
	RTE_SET_USED(data);
	return 0;
}

static void
device_infos_display_speeds(uint32_t speed_capa)
{
	printf("\n\tDevice speed capability:");
	if (speed_capa == RTE_ETH_LINK_SPEED_AUTONEG)
		printf(" Autonegotiate (all speeds)");
	if (speed_capa & RTE_ETH_LINK_SPEED_FIXED)
		printf(" Disable autonegotiate (fixed speed)  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_10M_HD)
		printf(" 10 Mbps half-duplex  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_10M)
		printf(" 10 Mbps full-duplex  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_100M_HD)
		printf(" 100 Mbps half-duplex  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_100M)
		printf(" 100 Mbps full-duplex  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_1G)
		printf(" 1 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_2_5G)
		printf(" 2.5 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_5G)
		printf(" 5 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_10G)
		printf(" 10 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_20G)
		printf(" 20 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_25G)
		printf(" 25 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_40G)
		printf(" 40 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_50G)
		printf(" 50 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_56G)
		printf(" 56 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_100G)
		printf(" 100 Gbps  ");
	if (speed_capa & RTE_ETH_LINK_SPEED_200G)
		printf(" 200 Gbps  ");
}

void
device_infos_display(const char *identifier)
{
	static const char *info_border = "*********************";
	struct rte_bus *start = NULL, *next;
	struct rte_dev_iterator dev_iter;
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_ether_addr mac_addr;
	struct rte_device *dev;
	struct rte_devargs da;
	portid_t port_id;
	struct rte_eth_dev_info dev_info;
	char devstr[128];

	memset(&da, 0, sizeof(da));
	if (!identifier)
		goto skip_parse;

	if (rte_devargs_parsef(&da, "%s", identifier)) {
		fprintf(stderr, "cannot parse identifier\n");
		return;
	}

skip_parse:
	while ((next = rte_bus_find(start, bus_match_all, NULL)) != NULL) {

		start = next;
		if (identifier && da.bus != next)
			continue;

		/* Skip buses that don't have iterate method */
		if (!next->dev_iterate)
			continue;

		snprintf(devstr, sizeof(devstr), "bus=%s", next->name);
		RTE_DEV_FOREACH(dev, devstr, &dev_iter) {

			if (!dev->driver)
				continue;
			/* Check for matching device if identifier is present */
			if (identifier &&
			    strncmp(da.name, dev->name, strlen(dev->name)))
				continue;
			printf("\n%s Infos for device %s %s\n",
			       info_border, dev->name, info_border);
			printf("Bus name: %s", dev->bus->name);
			printf("\nDriver name: %s", dev->driver->name);
			printf("\nDevargs: %s",
			       dev->devargs ? dev->devargs->args : "");
			printf("\nConnect to socket: %d", dev->numa_node);
			printf("\n");

			/* List ports with matching device name */
			RTE_ETH_FOREACH_DEV_OF(port_id, dev) {
				printf("\n\tPort id: %-2d", port_id);
				if (eth_macaddr_get_print_err(port_id,
							      &mac_addr) == 0)
					print_ethaddr("\n\tMAC address: ",
						      &mac_addr);
				rte_eth_dev_get_name_by_port(port_id, name);
				printf("\n\tDevice name: %s", name);
				if (rte_eth_dev_info_get(port_id, &dev_info) == 0)
					device_infos_display_speeds(dev_info.speed_capa);
				printf("\n");
			}
		}
	};
	rte_devargs_reset(&da);
}

static void
print_dev_capabilities(uint64_t capabilities)
{
	uint64_t single_capa;
	int begin;
	int end;
	int bit;

	if (capabilities == 0)
		return;

	begin = __builtin_ctzll(capabilities);
	end = sizeof(capabilities) * CHAR_BIT - __builtin_clzll(capabilities);

	single_capa = 1ULL << begin;
	for (bit = begin; bit < end; bit++) {
		if (capabilities & single_capa)
			printf(" %s",
			       rte_eth_dev_capability_name(single_capa));
		single_capa <<= 1;
	}
}

const char *
rsstypes_to_str(uint64_t rss_type)
{
	uint16_t i;

	for (i = 0; rss_type_table[i].str != NULL; i++) {
		if (rss_type_table[i].rss_type == rss_type)
			return rss_type_table[i].str;
	}

	return NULL;
}

void
port_infos_display(portid_t port_id)
{
	struct rte_port *port;
	struct rte_ether_addr mac_addr;
	struct rte_eth_link link;
	struct rte_eth_dev_info dev_info;
	int vlan_offload;
	struct rte_mempool * mp;
	static const char *info_border = "*********************";
	uint16_t mtu;
	char name[RTE_ETH_NAME_MAX_LEN];
	int ret;
	char fw_version[ETHDEV_FWVERS_LEN];

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		print_valid_ports();
		return;
	}
	port = &ports[port_id];
	ret = eth_link_get_nowait_print_err(port_id, &link);
	if (ret < 0)
		return;

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	printf("\n%s Infos for port %-2d %s\n",
	       info_border, port_id, info_border);
	if (eth_macaddr_get_print_err(port_id, &mac_addr) == 0)
		print_ethaddr("MAC address: ", &mac_addr);
	rte_eth_dev_get_name_by_port(port_id, name);
	printf("\nDevice name: %s", name);
	printf("\nDriver name: %s", dev_info.driver_name);

	if (rte_eth_dev_fw_version_get(port_id, fw_version,
						ETHDEV_FWVERS_LEN) == 0)
		printf("\nFirmware-version: %s", fw_version);
	else
		printf("\nFirmware-version: %s", "not available");

	if (dev_info.device->devargs && dev_info.device->devargs->args)
		printf("\nDevargs: %s", dev_info.device->devargs->args);
	printf("\nConnect to socket: %u", port->socket_id);

	if (port_numa[port_id] != NUMA_NO_CONFIG) {
		mp = mbuf_pool_find(port_numa[port_id], 0);
		if (mp)
			printf("\nmemory allocation on the socket: %d",
							port_numa[port_id]);
	} else
		printf("\nmemory allocation on the socket: %u",port->socket_id);

	printf("\nLink status: %s\n", (link.link_status) ? ("up") : ("down"));
	printf("Link speed: %s\n", rte_eth_link_speed_to_str(link.link_speed));
	printf("Link duplex: %s\n", (link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX) ?
	       ("full-duplex") : ("half-duplex"));
	printf("Autoneg status: %s\n", (link.link_autoneg == RTE_ETH_LINK_AUTONEG) ?
	       ("On") : ("Off"));

	if (!rte_eth_dev_get_mtu(port_id, &mtu))
		printf("MTU: %u\n", mtu);

	printf("Promiscuous mode: %s\n",
	       rte_eth_promiscuous_get(port_id) ? "enabled" : "disabled");
	printf("Allmulticast mode: %s\n",
	       rte_eth_allmulticast_get(port_id) ? "enabled" : "disabled");
	printf("Maximum number of MAC addresses: %u\n",
	       (unsigned int)(port->dev_info.max_mac_addrs));
	printf("Maximum number of MAC addresses of hash filtering: %u\n",
	       (unsigned int)(port->dev_info.max_hash_mac_addrs));

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);
	if (vlan_offload >= 0){
		printf("VLAN offload: \n");
		if (vlan_offload & RTE_ETH_VLAN_STRIP_OFFLOAD)
			printf("  strip on, ");
		else
			printf("  strip off, ");

		if (vlan_offload & RTE_ETH_VLAN_FILTER_OFFLOAD)
			printf("filter on, ");
		else
			printf("filter off, ");

		if (vlan_offload & RTE_ETH_VLAN_EXTEND_OFFLOAD)
			printf("extend on, ");
		else
			printf("extend off, ");

		if (vlan_offload & RTE_ETH_QINQ_STRIP_OFFLOAD)
			printf("qinq strip on\n");
		else
			printf("qinq strip off\n");
	}

	if (dev_info.hash_key_size > 0)
		printf("Hash key size in bytes: %u\n", dev_info.hash_key_size);
	if (dev_info.reta_size > 0)
		printf("Redirection table size: %u\n", dev_info.reta_size);
	if (!dev_info.flow_type_rss_offloads)
		printf("No RSS offload flow type is supported.\n");
	else {
		uint64_t rss_offload_types = dev_info.flow_type_rss_offloads;
		uint16_t i;

		printf("Supported RSS offload flow types:\n");
		for (i = 0; i < sizeof(rss_offload_types) * CHAR_BIT; i++) {
			uint64_t rss_offload = RTE_BIT64(i);
			if ((rss_offload_types & rss_offload) != 0) {
				const char *p = rsstypes_to_str(rss_offload);
				if (p)
					printf("  %s\n", p);
				else
					printf("  user defined %u\n",
					       i);
			}
		}
	}

	printf("Minimum size of RX buffer: %u\n", dev_info.min_rx_bufsize);
	printf("Maximum configurable length of RX packet: %u\n",
		dev_info.max_rx_pktlen);
	printf("Maximum configurable size of LRO aggregated packet: %u\n",
		dev_info.max_lro_pkt_size);
	if (dev_info.max_vfs)
		printf("Maximum number of VFs: %u\n", dev_info.max_vfs);
	if (dev_info.max_vmdq_pools)
		printf("Maximum number of VMDq pools: %u\n",
			dev_info.max_vmdq_pools);

	printf("Current number of RX queues: %u\n", dev_info.nb_rx_queues);
	printf("Max possible RX queues: %u\n", dev_info.max_rx_queues);
	printf("Max possible number of RXDs per queue: %hu\n",
		dev_info.rx_desc_lim.nb_max);
	printf("Min possible number of RXDs per queue: %hu\n",
		dev_info.rx_desc_lim.nb_min);
	printf("RXDs number alignment: %hu\n", dev_info.rx_desc_lim.nb_align);

	printf("Current number of TX queues: %u\n", dev_info.nb_tx_queues);
	printf("Max possible TX queues: %u\n", dev_info.max_tx_queues);
	printf("Max possible number of TXDs per queue: %hu\n",
		dev_info.tx_desc_lim.nb_max);
	printf("Min possible number of TXDs per queue: %hu\n",
		dev_info.tx_desc_lim.nb_min);
	printf("TXDs number alignment: %hu\n", dev_info.tx_desc_lim.nb_align);
	printf("Max segment number per packet: %hu\n",
		dev_info.tx_desc_lim.nb_seg_max);
	printf("Max segment number per MTU/TSO: %hu\n",
		dev_info.tx_desc_lim.nb_mtu_seg_max);

	printf("Device capabilities: 0x%"PRIx64"(", dev_info.dev_capa);
	print_dev_capabilities(dev_info.dev_capa);
	printf(" )\n");
	/* Show switch info only if valid switch domain and port id is set */
	if (dev_info.switch_info.domain_id !=
		RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID) {
		if (dev_info.switch_info.name)
			printf("Switch name: %s\n", dev_info.switch_info.name);

		printf("Switch domain Id: %u\n",
			dev_info.switch_info.domain_id);
		printf("Switch Port Id: %u\n",
			dev_info.switch_info.port_id);
		if ((dev_info.dev_capa & RTE_ETH_DEV_CAPA_RXQ_SHARE) != 0)
			printf("Switch Rx domain: %u\n",
			       dev_info.switch_info.rx_domain);
	}
}

void
port_summary_header_display(void)
{
	uint16_t port_number;

	port_number = rte_eth_dev_count_avail();
	printf("Number of available ports: %i\n", port_number);
	printf("%-4s %-17s %-12s %-14s %-8s %s\n", "Port", "MAC Address", "Name",
			"Driver", "Status", "Link");
}

void
port_summary_display(portid_t port_id)
{
	struct rte_ether_addr mac_addr;
	struct rte_eth_link link;
	struct rte_eth_dev_info dev_info;
	char name[RTE_ETH_NAME_MAX_LEN];
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		print_valid_ports();
		return;
	}

	ret = eth_link_get_nowait_print_err(port_id, &link);
	if (ret < 0)
		return;

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	rte_eth_dev_get_name_by_port(port_id, name);
	ret = eth_macaddr_get_print_err(port_id, &mac_addr);
	if (ret != 0)
		return;

	printf("%-4d " RTE_ETHER_ADDR_PRT_FMT " %-12s %-14s %-8s %s\n",
		port_id, RTE_ETHER_ADDR_BYTES(&mac_addr), name,
		dev_info.driver_name, (link.link_status) ? ("up") : ("down"),
		rte_eth_link_speed_to_str(link.link_speed));
}

void
port_eeprom_display(portid_t port_id)
{
	struct rte_dev_eeprom_info einfo;
	int ret;
	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		print_valid_ports();
		return;
	}

	int len_eeprom = rte_eth_dev_get_eeprom_length(port_id);
	if (len_eeprom < 0) {
		switch (len_eeprom) {
		case -ENODEV:
			fprintf(stderr, "port index %d invalid\n", port_id);
			break;
		case -ENOTSUP:
			fprintf(stderr, "operation not supported by device\n");
			break;
		case -EIO:
			fprintf(stderr, "device is removed\n");
			break;
		default:
			fprintf(stderr, "Unable to get EEPROM: %d\n",
				len_eeprom);
			break;
		}
		return;
	}

	einfo.offset = 0;
	einfo.length = len_eeprom;
	einfo.data = calloc(1, len_eeprom);
	if (!einfo.data) {
		fprintf(stderr,
			"Allocation of port %u eeprom data failed\n",
			port_id);
		return;
	}

	ret = rte_eth_dev_get_eeprom(port_id, &einfo);
	if (ret != 0) {
		switch (ret) {
		case -ENODEV:
			fprintf(stderr, "port index %d invalid\n", port_id);
			break;
		case -ENOTSUP:
			fprintf(stderr, "operation not supported by device\n");
			break;
		case -EIO:
			fprintf(stderr, "device is removed\n");
			break;
		default:
			fprintf(stderr, "Unable to get EEPROM: %d\n", ret);
			break;
		}
		free(einfo.data);
		return;
	}
	rte_hexdump(stdout, "hexdump", einfo.data, einfo.length);
	printf("Finish -- Port: %d EEPROM length: %d bytes\n", port_id, len_eeprom);
	free(einfo.data);
}

void
port_module_eeprom_display(portid_t port_id)
{
	struct rte_eth_dev_module_info minfo;
	struct rte_dev_eeprom_info einfo;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		print_valid_ports();
		return;
	}


	ret = rte_eth_dev_get_module_info(port_id, &minfo);
	if (ret != 0) {
		switch (ret) {
		case -ENODEV:
			fprintf(stderr, "port index %d invalid\n", port_id);
			break;
		case -ENOTSUP:
			fprintf(stderr, "operation not supported by device\n");
			break;
		case -EIO:
			fprintf(stderr, "device is removed\n");
			break;
		default:
			fprintf(stderr, "Unable to get module EEPROM: %d\n",
				ret);
			break;
		}
		return;
	}

	einfo.offset = 0;
	einfo.length = minfo.eeprom_len;
	einfo.data = calloc(1, minfo.eeprom_len);
	if (!einfo.data) {
		fprintf(stderr,
			"Allocation of port %u eeprom data failed\n",
			port_id);
		return;
	}

	ret = rte_eth_dev_get_module_eeprom(port_id, &einfo);
	if (ret != 0) {
		switch (ret) {
		case -ENODEV:
			fprintf(stderr, "port index %d invalid\n", port_id);
			break;
		case -ENOTSUP:
			fprintf(stderr, "operation not supported by device\n");
			break;
		case -EIO:
			fprintf(stderr, "device is removed\n");
			break;
		default:
			fprintf(stderr, "Unable to get module EEPROM: %d\n",
				ret);
			break;
		}
		free(einfo.data);
		return;
	}

	rte_hexdump(stdout, "hexdump", einfo.data, einfo.length);
	printf("Finish -- Port: %d MODULE EEPROM length: %d bytes\n", port_id, einfo.length);
	free(einfo.data);
}

int
port_id_is_invalid(portid_t port_id, enum print_warning warning)
{
	uint16_t pid;

	if (port_id == (portid_t)RTE_PORT_ALL)
		return 0;

	RTE_ETH_FOREACH_DEV(pid)
		if (port_id == pid)
			return 0;

	if (warning == ENABLED_WARN)
		fprintf(stderr, "Invalid port %d\n", port_id);

	return 1;
}

void print_valid_ports(void)
{
	portid_t pid;

	printf("The valid ports array is [");
	RTE_ETH_FOREACH_DEV(pid) {
		printf(" %d", pid);
	}
	printf(" ]\n");
}

static int
vlan_id_is_invalid(uint16_t vlan_id)
{
	if (vlan_id < 4096)
		return 0;
	fprintf(stderr, "Invalid vlan_id %d (must be < 4096)\n", vlan_id);
	return 1;
}

static int
port_reg_off_is_invalid(portid_t port_id, uint32_t reg_off)
{
	const struct rte_pci_device *pci_dev;
	const struct rte_bus *bus;
	uint64_t pci_len;

	if (reg_off & 0x3) {
		fprintf(stderr,
			"Port register offset 0x%X not aligned on a 4-byte boundary\n",
			(unsigned int)reg_off);
		return 1;
	}

	if (!ports[port_id].dev_info.device) {
		fprintf(stderr, "Invalid device\n");
		return 0;
	}

	bus = rte_bus_find_by_device(ports[port_id].dev_info.device);
	if (bus && !strcmp(bus->name, "pci")) {
		pci_dev = RTE_DEV_TO_PCI(ports[port_id].dev_info.device);
	} else {
		fprintf(stderr, "Not a PCI device\n");
		return 1;
	}

	pci_len = pci_dev->mem_resource[0].len;
	if (reg_off >= pci_len) {
		fprintf(stderr,
			"Port %d: register offset %u (0x%X) out of port PCI resource (length=%"PRIu64")\n",
			port_id, (unsigned int)reg_off, (unsigned int)reg_off,
			pci_len);
		return 1;
	}
	return 0;
}

static int
reg_bit_pos_is_invalid(uint8_t bit_pos)
{
	if (bit_pos <= 31)
		return 0;
	fprintf(stderr, "Invalid bit position %d (must be <= 31)\n", bit_pos);
	return 1;
}

#define display_port_and_reg_off(port_id, reg_off) \
	printf("port %d PCI register at offset 0x%X: ", (port_id), (reg_off))

static inline void
display_port_reg_value(portid_t port_id, uint32_t reg_off, uint32_t reg_v)
{
	display_port_and_reg_off(port_id, (unsigned)reg_off);
	printf("0x%08X (%u)\n", (unsigned)reg_v, (unsigned)reg_v);
}

void
port_reg_bit_display(portid_t port_id, uint32_t reg_off, uint8_t bit_x)
{
	uint32_t reg_v;


	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (port_reg_off_is_invalid(port_id, reg_off))
		return;
	if (reg_bit_pos_is_invalid(bit_x))
		return;
	reg_v = port_id_pci_reg_read(port_id, reg_off);
	display_port_and_reg_off(port_id, (unsigned)reg_off);
	printf("bit %d=%d\n", bit_x, (int) ((reg_v & (1 << bit_x)) >> bit_x));
}

void
port_reg_bit_field_display(portid_t port_id, uint32_t reg_off,
			   uint8_t bit1_pos, uint8_t bit2_pos)
{
	uint32_t reg_v;
	uint8_t  l_bit;
	uint8_t  h_bit;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (port_reg_off_is_invalid(port_id, reg_off))
		return;
	if (reg_bit_pos_is_invalid(bit1_pos))
		return;
	if (reg_bit_pos_is_invalid(bit2_pos))
		return;
	if (bit1_pos > bit2_pos)
		l_bit = bit2_pos, h_bit = bit1_pos;
	else
		l_bit = bit1_pos, h_bit = bit2_pos;

	reg_v = port_id_pci_reg_read(port_id, reg_off);
	reg_v >>= l_bit;
	if (h_bit < 31)
		reg_v &= ((1 << (h_bit - l_bit + 1)) - 1);
	display_port_and_reg_off(port_id, (unsigned)reg_off);
	printf("bits[%d, %d]=0x%0*X (%u)\n", l_bit, h_bit,
	       ((h_bit - l_bit) / 4) + 1, (unsigned)reg_v, (unsigned)reg_v);
}

void
port_reg_display(portid_t port_id, uint32_t reg_off)
{
	uint32_t reg_v;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (port_reg_off_is_invalid(port_id, reg_off))
		return;
	reg_v = port_id_pci_reg_read(port_id, reg_off);
	display_port_reg_value(port_id, reg_off, reg_v);
}

void
port_reg_bit_set(portid_t port_id, uint32_t reg_off, uint8_t bit_pos,
		 uint8_t bit_v)
{
	uint32_t reg_v;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (port_reg_off_is_invalid(port_id, reg_off))
		return;
	if (reg_bit_pos_is_invalid(bit_pos))
		return;
	if (bit_v > 1) {
		fprintf(stderr, "Invalid bit value %d (must be 0 or 1)\n",
			(int) bit_v);
		return;
	}
	reg_v = port_id_pci_reg_read(port_id, reg_off);
	if (bit_v == 0)
		reg_v &= ~(1 << bit_pos);
	else
		reg_v |= (1 << bit_pos);
	port_id_pci_reg_write(port_id, reg_off, reg_v);
	display_port_reg_value(port_id, reg_off, reg_v);
}

void
port_reg_bit_field_set(portid_t port_id, uint32_t reg_off,
		       uint8_t bit1_pos, uint8_t bit2_pos, uint32_t value)
{
	uint32_t max_v;
	uint32_t reg_v;
	uint8_t  l_bit;
	uint8_t  h_bit;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (port_reg_off_is_invalid(port_id, reg_off))
		return;
	if (reg_bit_pos_is_invalid(bit1_pos))
		return;
	if (reg_bit_pos_is_invalid(bit2_pos))
		return;
	if (bit1_pos > bit2_pos)
		l_bit = bit2_pos, h_bit = bit1_pos;
	else
		l_bit = bit1_pos, h_bit = bit2_pos;

	if ((h_bit - l_bit) < 31)
		max_v = (1 << (h_bit - l_bit + 1)) - 1;
	else
		max_v = 0xFFFFFFFF;

	if (value > max_v) {
		fprintf(stderr, "Invalid value %u (0x%x) must be < %u (0x%x)\n",
				(unsigned)value, (unsigned)value,
				(unsigned)max_v, (unsigned)max_v);
		return;
	}
	reg_v = port_id_pci_reg_read(port_id, reg_off);
	reg_v &= ~(max_v << l_bit); /* Keep unchanged bits */
	reg_v |= (value << l_bit); /* Set changed bits */
	port_id_pci_reg_write(port_id, reg_off, reg_v);
	display_port_reg_value(port_id, reg_off, reg_v);
}

void
port_reg_set(portid_t port_id, uint32_t reg_off, uint32_t reg_v)
{
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (port_reg_off_is_invalid(port_id, reg_off))
		return;
	port_id_pci_reg_write(port_id, reg_off, reg_v);
	display_port_reg_value(port_id, reg_off, reg_v);
}

static uint32_t
eth_dev_get_overhead_len(uint32_t max_rx_pktlen, uint16_t max_mtu)
{
	uint32_t overhead_len;

	if (max_mtu != UINT16_MAX && max_rx_pktlen > max_mtu)
		overhead_len = max_rx_pktlen - max_mtu;
	else
		overhead_len = RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;

	return overhead_len;
}

static int
eth_dev_validate_mtu(uint16_t port_id, uint16_t mtu)
{
	struct rte_eth_dev_info dev_info;
	uint32_t overhead_len;
	uint32_t frame_size;
	int ret;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	if (mtu < dev_info.min_mtu) {
		fprintf(stderr,
			"MTU (%u) < device min MTU (%u) for port_id %u\n",
			mtu, dev_info.min_mtu, port_id);
		return -EINVAL;
	}
	if (mtu > dev_info.max_mtu) {
		fprintf(stderr,
			"MTU (%u) > device max MTU (%u) for port_id %u\n",
			mtu, dev_info.max_mtu, port_id);
		return -EINVAL;
	}

	overhead_len = eth_dev_get_overhead_len(dev_info.max_rx_pktlen,
			dev_info.max_mtu);
	frame_size = mtu + overhead_len;
	if (frame_size > dev_info.max_rx_pktlen) {
		fprintf(stderr,
			"Frame size (%u) > device max frame size (%u) for port_id %u\n",
			frame_size, dev_info.max_rx_pktlen, port_id);
		return -EINVAL;
	}

	return 0;
}

void
port_mtu_set(portid_t port_id, uint16_t mtu)
{
	struct rte_port *port = &ports[port_id];
	int diag;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	diag = eth_dev_validate_mtu(port_id, mtu);
	if (diag != 0)
		return;

	if (port->need_reconfig == 0) {
		diag = rte_eth_dev_set_mtu(port_id, mtu);
		if (diag != 0) {
			fprintf(stderr, "Set MTU failed. diag=%d\n", diag);
			return;
		}
	}

	port->dev_conf.rxmode.mtu = mtu;
}

/* Generic flow management functions. */

static struct port_flow_tunnel *
port_flow_locate_tunnel_id(struct rte_port *port, uint32_t port_tunnel_id)
{
	struct port_flow_tunnel *flow_tunnel;

	LIST_FOREACH(flow_tunnel, &port->flow_tunnel_list, chain) {
		if (flow_tunnel->id == port_tunnel_id)
			goto out;
	}
	flow_tunnel = NULL;

out:
	return flow_tunnel;
}

const char *
port_flow_tunnel_type(struct rte_flow_tunnel *tunnel)
{
	const char *type;
	switch (tunnel->type) {
	default:
		type = "unknown";
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		type = "vxlan";
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		type = "gre";
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		type = "nvgre";
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		type = "geneve";
		break;
	}

	return type;
}

struct port_flow_tunnel *
port_flow_locate_tunnel(uint16_t port_id, struct rte_flow_tunnel *tun)
{
	struct rte_port *port = &ports[port_id];
	struct port_flow_tunnel *flow_tunnel;

	LIST_FOREACH(flow_tunnel, &port->flow_tunnel_list, chain) {
		if (!memcmp(&flow_tunnel->tunnel, tun, sizeof(*tun)))
			goto out;
	}
	flow_tunnel = NULL;

out:
	return flow_tunnel;
}

void port_flow_tunnel_list(portid_t port_id)
{
	struct rte_port *port = &ports[port_id];
	struct port_flow_tunnel *flt;

	LIST_FOREACH(flt, &port->flow_tunnel_list, chain) {
		printf("port %u tunnel #%u type=%s",
			port_id, flt->id, port_flow_tunnel_type(&flt->tunnel));
		if (flt->tunnel.tun_id)
			printf(" id=%" PRIu64, flt->tunnel.tun_id);
		printf("\n");
	}
}

void port_flow_tunnel_destroy(portid_t port_id, uint32_t tunnel_id)
{
	struct rte_port *port = &ports[port_id];
	struct port_flow_tunnel *flt;

	LIST_FOREACH(flt, &port->flow_tunnel_list, chain) {
		if (flt->id == tunnel_id)
			break;
	}
	if (flt) {
		LIST_REMOVE(flt, chain);
		free(flt);
		printf("port %u: flow tunnel #%u destroyed\n",
			port_id, tunnel_id);
	}
}

void port_flow_tunnel_create(portid_t port_id, const struct tunnel_ops *ops)
{
	struct rte_port *port = &ports[port_id];
	enum rte_flow_item_type	type;
	struct port_flow_tunnel *flt;

	if (!strcmp(ops->type, "vxlan"))
		type = RTE_FLOW_ITEM_TYPE_VXLAN;
	else if (!strcmp(ops->type, "gre"))
		type = RTE_FLOW_ITEM_TYPE_GRE;
	else if (!strcmp(ops->type, "nvgre"))
		type = RTE_FLOW_ITEM_TYPE_NVGRE;
	else if (!strcmp(ops->type, "geneve"))
		type = RTE_FLOW_ITEM_TYPE_GENEVE;
	else {
		fprintf(stderr, "cannot offload \"%s\" tunnel type\n",
			ops->type);
		return;
	}
	LIST_FOREACH(flt, &port->flow_tunnel_list, chain) {
		if (flt->tunnel.type == type)
			break;
	}
	if (!flt) {
		flt = calloc(1, sizeof(*flt));
		if (!flt) {
			fprintf(stderr, "failed to allocate port flt object\n");
			return;
		}
		flt->tunnel.type = type;
		flt->id = LIST_EMPTY(&port->flow_tunnel_list) ? 1 :
				  LIST_FIRST(&port->flow_tunnel_list)->id + 1;
		LIST_INSERT_HEAD(&port->flow_tunnel_list, flt, chain);
	}
	printf("port %d: flow tunnel #%u type %s\n",
		port_id, flt->id, ops->type);
}

/** Generate a port_flow entry from attributes/pattern/actions. */
static struct port_flow *
port_flow_new(const struct rte_flow_attr *attr,
	      const struct rte_flow_item *pattern,
	      const struct rte_flow_action *actions,
	      struct rte_flow_error *error)
{
	const struct rte_flow_conv_rule rule = {
		.attr_ro = attr,
		.pattern_ro = pattern,
		.actions_ro = actions,
	};
	struct port_flow *pf;
	int ret;

	ret = rte_flow_conv(RTE_FLOW_CONV_OP_RULE, NULL, 0, &rule, error);
	if (ret < 0)
		return NULL;
	pf = calloc(1, offsetof(struct port_flow, rule) + ret);
	if (!pf) {
		rte_flow_error_set
			(error, errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			 "calloc() failed");
		return NULL;
	}
	if (rte_flow_conv(RTE_FLOW_CONV_OP_RULE, &pf->rule, ret, &rule,
			  error) >= 0)
		return pf;
	free(pf);
	return NULL;
}

/** Print a message out of a flow error. */
static int
port_flow_complain(struct rte_flow_error *error)
{
	static const char *const errstrlist[] = {
		[RTE_FLOW_ERROR_TYPE_NONE] = "no error",
		[RTE_FLOW_ERROR_TYPE_UNSPECIFIED] = "cause unspecified",
		[RTE_FLOW_ERROR_TYPE_HANDLE] = "flow rule (handle)",
		[RTE_FLOW_ERROR_TYPE_ATTR_GROUP] = "group field",
		[RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY] = "priority field",
		[RTE_FLOW_ERROR_TYPE_ATTR_INGRESS] = "ingress field",
		[RTE_FLOW_ERROR_TYPE_ATTR_EGRESS] = "egress field",
		[RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER] = "transfer field",
		[RTE_FLOW_ERROR_TYPE_ATTR] = "attributes structure",
		[RTE_FLOW_ERROR_TYPE_ITEM_NUM] = "pattern length",
		[RTE_FLOW_ERROR_TYPE_ITEM_SPEC] = "item specification",
		[RTE_FLOW_ERROR_TYPE_ITEM_LAST] = "item specification range",
		[RTE_FLOW_ERROR_TYPE_ITEM_MASK] = "item specification mask",
		[RTE_FLOW_ERROR_TYPE_ITEM] = "specific pattern item",
		[RTE_FLOW_ERROR_TYPE_ACTION_NUM] = "number of actions",
		[RTE_FLOW_ERROR_TYPE_ACTION_CONF] = "action configuration",
		[RTE_FLOW_ERROR_TYPE_ACTION] = "specific action",
	};
	const char *errstr;
	char buf[32];
	int err = rte_errno;

	if ((unsigned int)error->type >= RTE_DIM(errstrlist) ||
	    !errstrlist[error->type])
		errstr = "unknown type";
	else
		errstr = errstrlist[error->type];
	fprintf(stderr, "%s(): Caught PMD error type %d (%s): %s%s: %s\n",
		__func__, error->type, errstr,
		error->cause ? (snprintf(buf, sizeof(buf), "cause: %p, ",
					 error->cause), buf) : "",
		error->message ? error->message : "(no stated reason)",
		rte_strerror(err));

	switch (error->type) {
	case RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER:
		fprintf(stderr, "The status suggests the use of \"transfer\" "
				"as the possible cause of the failure. Make "
				"sure that the flow in question and its "
				"indirect components (if any) are managed "
				"via \"transfer\" proxy port. Use command "
				"\"show port (port_id) flow transfer proxy\" "
				"to figure out the proxy port ID\n");
		break;
	default:
		break;
	}

	return -err;
}

static void
rss_config_display(struct rte_flow_action_rss *rss_conf)
{
	uint8_t i;

	if (rss_conf == NULL) {
		fprintf(stderr, "Invalid rule\n");
		return;
	}

	printf("RSS:\n"
	       " queues:");
	if (rss_conf->queue_num == 0)
		printf(" none");
	for (i = 0; i < rss_conf->queue_num; i++)
		printf(" %d", rss_conf->queue[i]);
	printf("\n");

	printf(" function: ");
	switch (rss_conf->func) {
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
		printf("default\n");
		break;
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
		printf("toeplitz\n");
		break;
	case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
		printf("simple_xor\n");
		break;
	case RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ:
		printf("symmetric_toeplitz\n");
		break;
	default:
		printf("Unknown function\n");
		return;
	}

	printf(" types:\n");
	if (rss_conf->types == 0) {
		printf("  none\n");
		return;
	}
	for (i = 0; rss_type_table[i].str; i++) {
		if ((rss_conf->types &
		    rss_type_table[i].rss_type) ==
		    rss_type_table[i].rss_type &&
		    rss_type_table[i].rss_type != 0)
			printf("  %s\n", rss_type_table[i].str);
	}
}

static struct port_indirect_action *
action_get_by_id(portid_t port_id, uint32_t id)
{
	struct rte_port *port;
	struct port_indirect_action **ppia;
	struct port_indirect_action *pia = NULL;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return NULL;
	port = &ports[port_id];
	ppia = &port->actions_list;
	while (*ppia) {
		if ((*ppia)->id == id) {
			pia = *ppia;
			break;
		}
		ppia = &(*ppia)->next;
	}
	if (!pia)
		fprintf(stderr,
			"Failed to find indirect action #%u on port %u\n",
			id, port_id);
	return pia;
}

static int
action_alloc(portid_t port_id, uint32_t id,
	     struct port_indirect_action **action)
{
	struct rte_port *port;
	struct port_indirect_action **ppia;
	struct port_indirect_action *pia = NULL;

	*action = NULL;
	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return -EINVAL;
	port = &ports[port_id];
	if (id == UINT32_MAX) {
		/* taking first available ID */
		if (port->actions_list) {
			if (port->actions_list->id == UINT32_MAX - 1) {
				fprintf(stderr,
					"Highest indirect action ID is already assigned, delete it first\n");
				return -ENOMEM;
			}
			id = port->actions_list->id + 1;
		} else {
			id = 0;
		}
	}
	pia = calloc(1, sizeof(*pia));
	if (!pia) {
		fprintf(stderr,
			"Allocation of port %u indirect action failed\n",
			port_id);
		return -ENOMEM;
	}
	ppia = &port->actions_list;
	while (*ppia && (*ppia)->id > id)
		ppia = &(*ppia)->next;
	if (*ppia && (*ppia)->id == id) {
		fprintf(stderr,
			"Indirect action #%u is already assigned, delete it first\n",
			id);
		free(pia);
		return -EINVAL;
	}
	pia->next = *ppia;
	pia->id = id;
	*ppia = pia;
	*action = pia;
	return 0;
}

/** Create indirect action */
int
port_action_handle_create(portid_t port_id, uint32_t id,
			  const struct rte_flow_indir_action_conf *conf,
			  const struct rte_flow_action *action)
{
	struct port_indirect_action *pia;
	int ret;
	struct rte_flow_error error;

	ret = action_alloc(port_id, id, &pia);
	if (ret)
		return ret;
	if (action->type == RTE_FLOW_ACTION_TYPE_AGE) {
		struct rte_flow_action_age *age =
			(struct rte_flow_action_age *)(uintptr_t)(action->conf);

		pia->age_type = ACTION_AGE_CONTEXT_TYPE_INDIRECT_ACTION;
		age->context = &pia->age_type;
	} else if (action->type == RTE_FLOW_ACTION_TYPE_CONNTRACK) {
		struct rte_flow_action_conntrack *ct =
		(struct rte_flow_action_conntrack *)(uintptr_t)(action->conf);

		memcpy(ct, &conntrack_context, sizeof(*ct));
	}
	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x22, sizeof(error));
	pia->handle = rte_flow_action_handle_create(port_id, conf, action,
						    &error);
	if (!pia->handle) {
		uint32_t destroy_id = pia->id;
		port_action_handle_destroy(port_id, 1, &destroy_id);
		return port_flow_complain(&error);
	}
	pia->type = action->type;
	printf("Indirect action #%u created\n", pia->id);
	return 0;
}

/** Destroy indirect action */
int
port_action_handle_destroy(portid_t port_id,
			   uint32_t n,
			   const uint32_t *actions)
{
	struct rte_port *port;
	struct port_indirect_action **tmp;
	uint32_t c = 0;
	int ret = 0;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return -EINVAL;
	port = &ports[port_id];
	tmp = &port->actions_list;
	while (*tmp) {
		uint32_t i;

		for (i = 0; i != n; ++i) {
			struct rte_flow_error error;
			struct port_indirect_action *pia = *tmp;

			if (actions[i] != pia->id)
				continue;
			/*
			 * Poisoning to make sure PMDs update it in case
			 * of error.
			 */
			memset(&error, 0x33, sizeof(error));

			if (pia->handle && rte_flow_action_handle_destroy(
					port_id, pia->handle, &error)) {
				ret = port_flow_complain(&error);
				continue;
			}
			*tmp = pia->next;
			printf("Indirect action #%u destroyed\n", pia->id);
			free(pia);
			break;
		}
		if (i == n)
			tmp = &(*tmp)->next;
		++c;
	}
	return ret;
}

int
port_action_handle_flush(portid_t port_id)
{
	struct rte_port *port;
	struct port_indirect_action **tmp;
	int ret = 0;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return -EINVAL;
	port = &ports[port_id];
	tmp = &port->actions_list;
	while (*tmp != NULL) {
		struct rte_flow_error error;
		struct port_indirect_action *pia = *tmp;

		/* Poisoning to make sure PMDs update it in case of error. */
		memset(&error, 0x44, sizeof(error));
		if (pia->handle != NULL &&
		    rte_flow_action_handle_destroy
					(port_id, pia->handle, &error) != 0) {
			printf("Indirect action #%u not destroyed\n", pia->id);
			ret = port_flow_complain(&error);
			tmp = &pia->next;
		} else {
			*tmp = pia->next;
			free(pia);
		}
	}
	return ret;
}

/** Get indirect action by port + id */
struct rte_flow_action_handle *
port_action_handle_get_by_id(portid_t port_id, uint32_t id)
{

	struct port_indirect_action *pia = action_get_by_id(port_id, id);

	return (pia) ? pia->handle : NULL;
}

/** Update indirect action */
int
port_action_handle_update(portid_t port_id, uint32_t id,
			  const struct rte_flow_action *action)
{
	struct rte_flow_error error;
	struct rte_flow_action_handle *action_handle;
	struct port_indirect_action *pia;
	const void *update;

	action_handle = port_action_handle_get_by_id(port_id, id);
	if (!action_handle)
		return -EINVAL;
	pia = action_get_by_id(port_id, id);
	if (!pia)
		return -EINVAL;
	switch (pia->type) {
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		update = action->conf;
		break;
	default:
		update = action;
		break;
	}
	if (rte_flow_action_handle_update(port_id, action_handle, update,
					  &error)) {
		return port_flow_complain(&error);
	}
	printf("Indirect action #%u updated\n", id);
	return 0;
}

int
port_action_handle_query(portid_t port_id, uint32_t id)
{
	struct rte_flow_error error;
	struct port_indirect_action *pia;
	union {
		struct rte_flow_query_count count;
		struct rte_flow_query_age age;
		struct rte_flow_action_conntrack ct;
	} query;

	pia = action_get_by_id(port_id, id);
	if (!pia)
		return -EINVAL;
	switch (pia->type) {
	case RTE_FLOW_ACTION_TYPE_AGE:
	case RTE_FLOW_ACTION_TYPE_COUNT:
		break;
	default:
		fprintf(stderr,
			"Indirect action %u (type: %d) on port %u doesn't support query\n",
			id, pia->type, port_id);
		return -ENOTSUP;
	}
	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x55, sizeof(error));
	memset(&query, 0, sizeof(query));
	if (rte_flow_action_handle_query(port_id, pia->handle, &query, &error))
		return port_flow_complain(&error);
	switch (pia->type) {
	case RTE_FLOW_ACTION_TYPE_AGE:
		printf("Indirect AGE action:\n"
		       " aged: %u\n"
		       " sec_since_last_hit_valid: %u\n"
		       " sec_since_last_hit: %" PRIu32 "\n",
		       query.age.aged,
		       query.age.sec_since_last_hit_valid,
		       query.age.sec_since_last_hit);
		break;
	case RTE_FLOW_ACTION_TYPE_COUNT:
		printf("Indirect COUNT action:\n"
		       " hits_set: %u\n"
		       " bytes_set: %u\n"
		       " hits: %" PRIu64 "\n"
		       " bytes: %" PRIu64 "\n",
		       query.count.hits_set,
		       query.count.bytes_set,
		       query.count.hits,
		       query.count.bytes);
		break;
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		printf("Conntrack Context:\n"
		       "  Peer: %u, Flow dir: %s, Enable: %u\n"
		       "  Live: %u, SACK: %u, CACK: %u\n"
		       "  Packet dir: %s, Liberal: %u, State: %u\n"
		       "  Factor: %u, Retrans: %u, TCP flags: %u\n"
		       "  Last Seq: %u, Last ACK: %u\n"
		       "  Last Win: %u, Last End: %u\n",
		       query.ct.peer_port,
		       query.ct.is_original_dir ? "Original" : "Reply",
		       query.ct.enable, query.ct.live_connection,
		       query.ct.selective_ack, query.ct.challenge_ack_passed,
		       query.ct.last_direction ? "Original" : "Reply",
		       query.ct.liberal_mode, query.ct.state,
		       query.ct.max_ack_window, query.ct.retransmission_limit,
		       query.ct.last_index, query.ct.last_seq,
		       query.ct.last_ack, query.ct.last_window,
		       query.ct.last_end);
		printf("  Original Dir:\n"
		       "    scale: %u, fin: %u, ack seen: %u\n"
		       " unacked data: %u\n    Sent end: %u,"
		       "    Reply end: %u, Max win: %u, Max ACK: %u\n",
		       query.ct.original_dir.scale,
		       query.ct.original_dir.close_initiated,
		       query.ct.original_dir.last_ack_seen,
		       query.ct.original_dir.data_unacked,
		       query.ct.original_dir.sent_end,
		       query.ct.original_dir.reply_end,
		       query.ct.original_dir.max_win,
		       query.ct.original_dir.max_ack);
		printf("  Reply Dir:\n"
		       "    scale: %u, fin: %u, ack seen: %u\n"
		       " unacked data: %u\n    Sent end: %u,"
		       "    Reply end: %u, Max win: %u, Max ACK: %u\n",
		       query.ct.reply_dir.scale,
		       query.ct.reply_dir.close_initiated,
		       query.ct.reply_dir.last_ack_seen,
		       query.ct.reply_dir.data_unacked,
		       query.ct.reply_dir.sent_end,
		       query.ct.reply_dir.reply_end,
		       query.ct.reply_dir.max_win,
		       query.ct.reply_dir.max_ack);
		break;
	default:
		fprintf(stderr,
			"Indirect action %u (type: %d) on port %u doesn't support query\n",
			id, pia->type, port_id);
		break;
	}
	return 0;
}

static struct port_flow_tunnel *
port_flow_tunnel_offload_cmd_prep(portid_t port_id,
				  const struct rte_flow_item *pattern,
				  const struct rte_flow_action *actions,
				  const struct tunnel_ops *tunnel_ops)
{
	int ret;
	struct rte_port *port;
	struct port_flow_tunnel *pft;
	struct rte_flow_error error;

	port = &ports[port_id];
	pft = port_flow_locate_tunnel_id(port, tunnel_ops->id);
	if (!pft) {
		fprintf(stderr, "failed to locate port flow tunnel #%u\n",
			tunnel_ops->id);
		return NULL;
	}
	if (tunnel_ops->actions) {
		uint32_t num_actions;
		const struct rte_flow_action *aptr;

		ret = rte_flow_tunnel_decap_set(port_id, &pft->tunnel,
						&pft->pmd_actions,
						&pft->num_pmd_actions,
						&error);
		if (ret) {
			port_flow_complain(&error);
			return NULL;
		}
		for (aptr = actions, num_actions = 1;
		     aptr->type != RTE_FLOW_ACTION_TYPE_END;
		     aptr++, num_actions++);
		pft->actions = malloc(
				(num_actions +  pft->num_pmd_actions) *
				sizeof(actions[0]));
		if (!pft->actions) {
			rte_flow_tunnel_action_decap_release(
					port_id, pft->actions,
					pft->num_pmd_actions, &error);
			return NULL;
		}
		rte_memcpy(pft->actions, pft->pmd_actions,
			   pft->num_pmd_actions * sizeof(actions[0]));
		rte_memcpy(pft->actions + pft->num_pmd_actions, actions,
			   num_actions * sizeof(actions[0]));
	}
	if (tunnel_ops->items) {
		uint32_t num_items;
		const struct rte_flow_item *iptr;

		ret = rte_flow_tunnel_match(port_id, &pft->tunnel,
					    &pft->pmd_items,
					    &pft->num_pmd_items,
					    &error);
		if (ret) {
			port_flow_complain(&error);
			return NULL;
		}
		for (iptr = pattern, num_items = 1;
		     iptr->type != RTE_FLOW_ITEM_TYPE_END;
		     iptr++, num_items++);
		pft->items = malloc((num_items + pft->num_pmd_items) *
				    sizeof(pattern[0]));
		if (!pft->items) {
			rte_flow_tunnel_item_release(
					port_id, pft->pmd_items,
					pft->num_pmd_items, &error);
			return NULL;
		}
		rte_memcpy(pft->items, pft->pmd_items,
			   pft->num_pmd_items * sizeof(pattern[0]));
		rte_memcpy(pft->items + pft->num_pmd_items, pattern,
			   num_items * sizeof(pattern[0]));
	}

	return pft;
}

static void
port_flow_tunnel_offload_cmd_release(portid_t port_id,
				     const struct tunnel_ops *tunnel_ops,
				     struct port_flow_tunnel *pft)
{
	struct rte_flow_error error;

	if (tunnel_ops->actions) {
		free(pft->actions);
		rte_flow_tunnel_action_decap_release(
			port_id, pft->pmd_actions,
			pft->num_pmd_actions, &error);
		pft->actions = NULL;
		pft->pmd_actions = NULL;
	}
	if (tunnel_ops->items) {
		free(pft->items);
		rte_flow_tunnel_item_release(port_id, pft->pmd_items,
					     pft->num_pmd_items,
					     &error);
		pft->items = NULL;
		pft->pmd_items = NULL;
	}
}

/** Add port meter policy */
int
port_meter_policy_add(portid_t port_id, uint32_t policy_id,
			const struct rte_flow_action *actions)
{
	struct rte_mtr_error error;
	const struct rte_flow_action *act = actions;
	const struct rte_flow_action *start;
	struct rte_mtr_meter_policy_params policy;
	uint32_t i = 0, act_n;
	int ret;

	for (i = 0; i < RTE_COLORS; i++) {
		for (act_n = 0, start = act;
			act->type != RTE_FLOW_ACTION_TYPE_END; act++)
			act_n++;
		if (act_n && act->type == RTE_FLOW_ACTION_TYPE_END)
			policy.actions[i] = start;
		else
			policy.actions[i] = NULL;
		act++;
	}
	ret = rte_mtr_meter_policy_add(port_id,
			policy_id,
			&policy, &error);
	if (ret)
		print_mtr_err_msg(&error);
	return ret;
}

/** Validate flow rule. */
int
port_flow_validate(portid_t port_id,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item *pattern,
		   const struct rte_flow_action *actions,
		   const struct tunnel_ops *tunnel_ops)
{
	struct rte_flow_error error;
	struct port_flow_tunnel *pft = NULL;
	int ret;

	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x11, sizeof(error));
	if (tunnel_ops->enabled) {
		pft = port_flow_tunnel_offload_cmd_prep(port_id, pattern,
							actions, tunnel_ops);
		if (!pft)
			return -ENOENT;
		if (pft->items)
			pattern = pft->items;
		if (pft->actions)
			actions = pft->actions;
	}
	ret = rte_flow_validate(port_id, attr, pattern, actions, &error);
	if (tunnel_ops->enabled)
		port_flow_tunnel_offload_cmd_release(port_id, tunnel_ops, pft);
	if (ret)
		return port_flow_complain(&error);
	printf("Flow rule validated\n");
	return 0;
}

/** Return age action structure if exists, otherwise NULL. */
static struct rte_flow_action_age *
age_action_get(const struct rte_flow_action *actions)
{
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_AGE:
			return (struct rte_flow_action_age *)
				(uintptr_t)actions->conf;
		default:
			break;
		}
	}
	return NULL;
}

/** Create flow rule. */
int
port_flow_create(portid_t port_id,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item *pattern,
		 const struct rte_flow_action *actions,
		 const struct tunnel_ops *tunnel_ops)
{
	struct rte_flow *flow;
	struct rte_port *port;
	struct port_flow *pf;
	uint32_t id = 0;
	struct rte_flow_error error;
	struct port_flow_tunnel *pft = NULL;
	struct rte_flow_action_age *age = age_action_get(actions);

	port = &ports[port_id];
	if (port->flow_list) {
		if (port->flow_list->id == UINT32_MAX) {
			fprintf(stderr,
				"Highest rule ID is already assigned, delete it first");
			return -ENOMEM;
		}
		id = port->flow_list->id + 1;
	}
	if (tunnel_ops->enabled) {
		pft = port_flow_tunnel_offload_cmd_prep(port_id, pattern,
							actions, tunnel_ops);
		if (!pft)
			return -ENOENT;
		if (pft->items)
			pattern = pft->items;
		if (pft->actions)
			actions = pft->actions;
	}
	pf = port_flow_new(attr, pattern, actions, &error);
	if (!pf)
		return port_flow_complain(&error);
	if (age) {
		pf->age_type = ACTION_AGE_CONTEXT_TYPE_FLOW;
		age->context = &pf->age_type;
	}
	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x22, sizeof(error));
	flow = rte_flow_create(port_id, attr, pattern, actions, &error);
	if (!flow) {
		if (tunnel_ops->enabled)
			port_flow_tunnel_offload_cmd_release(port_id,
							     tunnel_ops, pft);
		free(pf);
		return port_flow_complain(&error);
	}
	pf->next = port->flow_list;
	pf->id = id;
	pf->flow = flow;
	port->flow_list = pf;
	if (tunnel_ops->enabled)
		port_flow_tunnel_offload_cmd_release(port_id, tunnel_ops, pft);
	printf("Flow rule #%u created\n", pf->id);
	return 0;
}

/** Destroy a number of flow rules. */
int
port_flow_destroy(portid_t port_id, uint32_t n, const uint32_t *rule)
{
	struct rte_port *port;
	struct port_flow **tmp;
	uint32_t c = 0;
	int ret = 0;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return -EINVAL;
	port = &ports[port_id];
	tmp = &port->flow_list;
	while (*tmp) {
		uint32_t i;

		for (i = 0; i != n; ++i) {
			struct rte_flow_error error;
			struct port_flow *pf = *tmp;

			if (rule[i] != pf->id)
				continue;
			/*
			 * Poisoning to make sure PMDs update it in case
			 * of error.
			 */
			memset(&error, 0x33, sizeof(error));
			if (rte_flow_destroy(port_id, pf->flow, &error)) {
				ret = port_flow_complain(&error);
				continue;
			}
			printf("Flow rule #%u destroyed\n", pf->id);
			*tmp = pf->next;
			free(pf);
			break;
		}
		if (i == n)
			tmp = &(*tmp)->next;
		++c;
	}
	return ret;
}

/** Remove all flow rules. */
int
port_flow_flush(portid_t port_id)
{
	struct rte_flow_error error;
	struct rte_port *port;
	int ret = 0;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
		port_id == (portid_t)RTE_PORT_ALL)
		return -EINVAL;

	port = &ports[port_id];

	if (port->flow_list == NULL)
		return ret;

	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x44, sizeof(error));
	if (rte_flow_flush(port_id, &error)) {
		port_flow_complain(&error);
	}

	while (port->flow_list) {
		struct port_flow *pf = port->flow_list->next;

		free(port->flow_list);
		port->flow_list = pf;
	}
	return ret;
}

/** Dump flow rules. */
int
port_flow_dump(portid_t port_id, bool dump_all, uint32_t rule_id,
		const char *file_name)
{
	int ret = 0;
	FILE *file = stdout;
	struct rte_flow_error error;
	struct rte_port *port;
	struct port_flow *pflow;
	struct rte_flow *tmpFlow = NULL;
	bool found = false;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
		port_id == (portid_t)RTE_PORT_ALL)
		return -EINVAL;

	if (!dump_all) {
		port = &ports[port_id];
		pflow = port->flow_list;
		while (pflow) {
			if (rule_id != pflow->id) {
				pflow = pflow->next;
			} else {
				tmpFlow = pflow->flow;
				if (tmpFlow)
					found = true;
				break;
			}
		}
		if (found == false) {
			fprintf(stderr, "Failed to dump to flow %d\n", rule_id);
			return -EINVAL;
		}
	}

	if (file_name && strlen(file_name)) {
		file = fopen(file_name, "w");
		if (!file) {
			fprintf(stderr, "Failed to create file %s: %s\n",
				file_name, strerror(errno));
			return -errno;
		}
	}

	if (!dump_all)
		ret = rte_flow_dev_dump(port_id, tmpFlow, file, &error);
	else
		ret = rte_flow_dev_dump(port_id, NULL, file, &error);
	if (ret) {
		port_flow_complain(&error);
		fprintf(stderr, "Failed to dump flow: %s\n", strerror(-ret));
	} else
		printf("Flow dump finished\n");
	if (file_name && strlen(file_name))
		fclose(file);
	return ret;
}

/** Query a flow rule. */
int
port_flow_query(portid_t port_id, uint32_t rule,
		const struct rte_flow_action *action)
{
	struct rte_flow_error error;
	struct rte_port *port;
	struct port_flow *pf;
	const char *name;
	union {
		struct rte_flow_query_count count;
		struct rte_flow_action_rss rss_conf;
		struct rte_flow_query_age age;
	} query;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return -EINVAL;
	port = &ports[port_id];
	for (pf = port->flow_list; pf; pf = pf->next)
		if (pf->id == rule)
			break;
	if (!pf) {
		fprintf(stderr, "Flow rule #%u not found\n", rule);
		return -ENOENT;
	}
	ret = rte_flow_conv(RTE_FLOW_CONV_OP_ACTION_NAME_PTR,
			    &name, sizeof(name),
			    (void *)(uintptr_t)action->type, &error);
	if (ret < 0)
		return port_flow_complain(&error);
	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_COUNT:
	case RTE_FLOW_ACTION_TYPE_RSS:
	case RTE_FLOW_ACTION_TYPE_AGE:
		break;
	default:
		fprintf(stderr, "Cannot query action type %d (%s)\n",
			action->type, name);
		return -ENOTSUP;
	}
	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x55, sizeof(error));
	memset(&query, 0, sizeof(query));
	if (rte_flow_query(port_id, pf->flow, action, &query, &error))
		return port_flow_complain(&error);
	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_COUNT:
		printf("%s:\n"
		       " hits_set: %u\n"
		       " bytes_set: %u\n"
		       " hits: %" PRIu64 "\n"
		       " bytes: %" PRIu64 "\n",
		       name,
		       query.count.hits_set,
		       query.count.bytes_set,
		       query.count.hits,
		       query.count.bytes);
		break;
	case RTE_FLOW_ACTION_TYPE_RSS:
		rss_config_display(&query.rss_conf);
		break;
	case RTE_FLOW_ACTION_TYPE_AGE:
		printf("%s:\n"
		       " aged: %u\n"
		       " sec_since_last_hit_valid: %u\n"
		       " sec_since_last_hit: %" PRIu32 "\n",
		       name,
		       query.age.aged,
		       query.age.sec_since_last_hit_valid,
		       query.age.sec_since_last_hit);
		break;
	default:
		fprintf(stderr,
			"Cannot display result for action type %d (%s)\n",
			action->type, name);
		break;
	}
	return 0;
}

/** List simply and destroy all aged flows. */
void
port_flow_aged(portid_t port_id, uint8_t destroy)
{
	void **contexts;
	int nb_context, total = 0, idx;
	struct rte_flow_error error;
	enum age_action_context_type *type;
	union {
		struct port_flow *pf;
		struct port_indirect_action *pia;
	} ctx;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return;
	total = rte_flow_get_aged_flows(port_id, NULL, 0, &error);
	printf("Port %u total aged flows: %d\n", port_id, total);
	if (total < 0) {
		port_flow_complain(&error);
		return;
	}
	if (total == 0)
		return;
	contexts = malloc(sizeof(void *) * total);
	if (contexts == NULL) {
		fprintf(stderr, "Cannot allocate contexts for aged flow\n");
		return;
	}
	printf("%-20s\tID\tGroup\tPrio\tAttr\n", "Type");
	nb_context = rte_flow_get_aged_flows(port_id, contexts, total, &error);
	if (nb_context != total) {
		fprintf(stderr,
			"Port:%d get aged flows count(%d) != total(%d)\n",
			port_id, nb_context, total);
		free(contexts);
		return;
	}
	total = 0;
	for (idx = 0; idx < nb_context; idx++) {
		if (!contexts[idx]) {
			fprintf(stderr, "Error: get Null context in port %u\n",
				port_id);
			continue;
		}
		type = (enum age_action_context_type *)contexts[idx];
		switch (*type) {
		case ACTION_AGE_CONTEXT_TYPE_FLOW:
			ctx.pf = container_of(type, struct port_flow, age_type);
			printf("%-20s\t%" PRIu32 "\t%" PRIu32 "\t%" PRIu32
								 "\t%c%c%c\t\n",
			       "Flow",
			       ctx.pf->id,
			       ctx.pf->rule.attr->group,
			       ctx.pf->rule.attr->priority,
			       ctx.pf->rule.attr->ingress ? 'i' : '-',
			       ctx.pf->rule.attr->egress ? 'e' : '-',
			       ctx.pf->rule.attr->transfer ? 't' : '-');
			if (destroy && !port_flow_destroy(port_id, 1,
							  &ctx.pf->id))
				total++;
			break;
		case ACTION_AGE_CONTEXT_TYPE_INDIRECT_ACTION:
			ctx.pia = container_of(type,
					struct port_indirect_action, age_type);
			printf("%-20s\t%" PRIu32 "\n", "Indirect action",
			       ctx.pia->id);
			break;
		default:
			fprintf(stderr, "Error: invalid context type %u\n",
				port_id);
			break;
		}
	}
	printf("\n%d flows destroyed\n", total);
	free(contexts);
}

/** List flow rules. */
void
port_flow_list(portid_t port_id, uint32_t n, const uint32_t *group)
{
	struct rte_port *port;
	struct port_flow *pf;
	struct port_flow *list = NULL;
	uint32_t i;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return;
	port = &ports[port_id];
	if (!port->flow_list)
		return;
	/* Sort flows by group, priority and ID. */
	for (pf = port->flow_list; pf != NULL; pf = pf->next) {
		struct port_flow **tmp;
		const struct rte_flow_attr *curr = pf->rule.attr;

		if (n) {
			/* Filter out unwanted groups. */
			for (i = 0; i != n; ++i)
				if (curr->group == group[i])
					break;
			if (i == n)
				continue;
		}
		for (tmp = &list; *tmp; tmp = &(*tmp)->tmp) {
			const struct rte_flow_attr *comp = (*tmp)->rule.attr;

			if (curr->group > comp->group ||
			    (curr->group == comp->group &&
			     curr->priority > comp->priority) ||
			    (curr->group == comp->group &&
			     curr->priority == comp->priority &&
			     pf->id > (*tmp)->id))
				continue;
			break;
		}
		pf->tmp = *tmp;
		*tmp = pf;
	}
	printf("ID\tGroup\tPrio\tAttr\tRule\n");
	for (pf = list; pf != NULL; pf = pf->tmp) {
		const struct rte_flow_item *item = pf->rule.pattern;
		const struct rte_flow_action *action = pf->rule.actions;
		const char *name;

		printf("%" PRIu32 "\t%" PRIu32 "\t%" PRIu32 "\t%c%c%c\t",
		       pf->id,
		       pf->rule.attr->group,
		       pf->rule.attr->priority,
		       pf->rule.attr->ingress ? 'i' : '-',
		       pf->rule.attr->egress ? 'e' : '-',
		       pf->rule.attr->transfer ? 't' : '-');
		while (item->type != RTE_FLOW_ITEM_TYPE_END) {
			if ((uint32_t)item->type > INT_MAX)
				name = "PMD_INTERNAL";
			else if (rte_flow_conv(RTE_FLOW_CONV_OP_ITEM_NAME_PTR,
					  &name, sizeof(name),
					  (void *)(uintptr_t)item->type,
					  NULL) <= 0)
				name = "[UNKNOWN]";
			if (item->type != RTE_FLOW_ITEM_TYPE_VOID)
				printf("%s ", name);
			++item;
		}
		printf("=>");
		while (action->type != RTE_FLOW_ACTION_TYPE_END) {
			if ((uint32_t)action->type > INT_MAX)
				name = "PMD_INTERNAL";
			else if (rte_flow_conv(RTE_FLOW_CONV_OP_ACTION_NAME_PTR,
					  &name, sizeof(name),
					  (void *)(uintptr_t)action->type,
					  NULL) <= 0)
				name = "[UNKNOWN]";
			if (action->type != RTE_FLOW_ACTION_TYPE_VOID)
				printf(" %s", name);
			++action;
		}
		printf("\n");
	}
}

/** Restrict ingress traffic to the defined flow rules. */
int
port_flow_isolate(portid_t port_id, int set)
{
	struct rte_flow_error error;

	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x66, sizeof(error));
	if (rte_flow_isolate(port_id, set, &error))
		return port_flow_complain(&error);
	printf("Ingress traffic on port %u is %s to the defined flow rules\n",
	       port_id,
	       set ? "now restricted" : "not restricted anymore");
	return 0;
}

/*
 * RX/TX ring descriptors display functions.
 */
int
rx_queue_id_is_invalid(queueid_t rxq_id)
{
	if (rxq_id < nb_rxq)
		return 0;
	fprintf(stderr, "Invalid RX queue %d (must be < nb_rxq=%d)\n",
		rxq_id, nb_rxq);
	return 1;
}

int
tx_queue_id_is_invalid(queueid_t txq_id)
{
	if (txq_id < nb_txq)
		return 0;
	fprintf(stderr, "Invalid TX queue %d (must be < nb_txq=%d)\n",
		txq_id, nb_txq);
	return 1;
}

static int
get_rx_ring_size(portid_t port_id, queueid_t rxq_id, uint16_t *ring_size)
{
	struct rte_port *port = &ports[port_id];
	struct rte_eth_rxq_info rx_qinfo;
	int ret;

	ret = rte_eth_rx_queue_info_get(port_id, rxq_id, &rx_qinfo);
	if (ret == 0) {
		*ring_size = rx_qinfo.nb_desc;
		return ret;
	}

	if (ret != -ENOTSUP)
		return ret;
	/*
	 * If the rte_eth_rx_queue_info_get is not support for this PMD,
	 * ring_size stored in testpmd will be used for validity verification.
	 * When configure the rxq by rte_eth_rx_queue_setup with nb_rx_desc
	 * being 0, it will use a default value provided by PMDs to setup this
	 * rxq. If the default value is 0, it will use the
	 * RTE_ETH_DEV_FALLBACK_RX_RINGSIZE to setup this rxq.
	 */
	if (port->nb_rx_desc[rxq_id])
		*ring_size = port->nb_rx_desc[rxq_id];
	else if (port->dev_info.default_rxportconf.ring_size)
		*ring_size = port->dev_info.default_rxportconf.ring_size;
	else
		*ring_size = RTE_ETH_DEV_FALLBACK_RX_RINGSIZE;
	return 0;
}

static int
get_tx_ring_size(portid_t port_id, queueid_t txq_id, uint16_t *ring_size)
{
	struct rte_port *port = &ports[port_id];
	struct rte_eth_txq_info tx_qinfo;
	int ret;

	ret = rte_eth_tx_queue_info_get(port_id, txq_id, &tx_qinfo);
	if (ret == 0) {
		*ring_size = tx_qinfo.nb_desc;
		return ret;
	}

	if (ret != -ENOTSUP)
		return ret;
	/*
	 * If the rte_eth_tx_queue_info_get is not support for this PMD,
	 * ring_size stored in testpmd will be used for validity verification.
	 * When configure the txq by rte_eth_tx_queue_setup with nb_tx_desc
	 * being 0, it will use a default value provided by PMDs to setup this
	 * txq. If the default value is 0, it will use the
	 * RTE_ETH_DEV_FALLBACK_TX_RINGSIZE to setup this txq.
	 */
	if (port->nb_tx_desc[txq_id])
		*ring_size = port->nb_tx_desc[txq_id];
	else if (port->dev_info.default_txportconf.ring_size)
		*ring_size = port->dev_info.default_txportconf.ring_size;
	else
		*ring_size = RTE_ETH_DEV_FALLBACK_TX_RINGSIZE;
	return 0;
}

static int
rx_desc_id_is_invalid(portid_t port_id, queueid_t rxq_id, uint16_t rxdesc_id)
{
	uint16_t ring_size;
	int ret;

	ret = get_rx_ring_size(port_id, rxq_id, &ring_size);
	if (ret)
		return 1;

	if (rxdesc_id < ring_size)
		return 0;

	fprintf(stderr, "Invalid RX descriptor %u (must be < ring_size=%u)\n",
		rxdesc_id, ring_size);
	return 1;
}

static int
tx_desc_id_is_invalid(portid_t port_id, queueid_t txq_id, uint16_t txdesc_id)
{
	uint16_t ring_size;
	int ret;

	ret = get_tx_ring_size(port_id, txq_id, &ring_size);
	if (ret)
		return 1;

	if (txdesc_id < ring_size)
		return 0;

	fprintf(stderr, "Invalid TX descriptor %u (must be < ring_size=%u)\n",
		txdesc_id, ring_size);
	return 1;
}

static const struct rte_memzone *
ring_dma_zone_lookup(const char *ring_name, portid_t port_id, uint16_t q_id)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(mz_name, sizeof(mz_name), "eth_p%d_q%d_%s",
			port_id, q_id, ring_name);
	mz = rte_memzone_lookup(mz_name);
	if (mz == NULL)
		fprintf(stderr,
			"%s ring memory zoneof (port %d, queue %d) not found (zone name = %s\n",
			ring_name, port_id, q_id, mz_name);
	return mz;
}

union igb_ring_dword {
	uint64_t dword;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint32_t lo;
		uint32_t hi;
#else
		uint32_t hi;
		uint32_t lo;
#endif
	} words;
};

struct igb_ring_desc_32_bytes {
	union igb_ring_dword lo_dword;
	union igb_ring_dword hi_dword;
	union igb_ring_dword resv1;
	union igb_ring_dword resv2;
};

struct igb_ring_desc_16_bytes {
	union igb_ring_dword lo_dword;
	union igb_ring_dword hi_dword;
};

static void
ring_rxd_display_dword(union igb_ring_dword dword)
{
	printf("    0x%08X - 0x%08X\n", (unsigned)dword.words.lo,
					(unsigned)dword.words.hi);
}

static void
ring_rx_descriptor_display(const struct rte_memzone *ring_mz,
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
			   portid_t port_id,
#else
			   __rte_unused portid_t port_id,
#endif
			   uint16_t desc_id)
{
	struct igb_ring_desc_16_bytes *ring =
		(struct igb_ring_desc_16_bytes *)ring_mz->addr;
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
	int ret;
	struct rte_eth_dev_info dev_info;

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	if (strstr(dev_info.driver_name, "i40e") != NULL) {
		/* 32 bytes RX descriptor, i40e only */
		struct igb_ring_desc_32_bytes *ring =
			(struct igb_ring_desc_32_bytes *)ring_mz->addr;
		ring[desc_id].lo_dword.dword =
			rte_le_to_cpu_64(ring[desc_id].lo_dword.dword);
		ring_rxd_display_dword(ring[desc_id].lo_dword);
		ring[desc_id].hi_dword.dword =
			rte_le_to_cpu_64(ring[desc_id].hi_dword.dword);
		ring_rxd_display_dword(ring[desc_id].hi_dword);
		ring[desc_id].resv1.dword =
			rte_le_to_cpu_64(ring[desc_id].resv1.dword);
		ring_rxd_display_dword(ring[desc_id].resv1);
		ring[desc_id].resv2.dword =
			rte_le_to_cpu_64(ring[desc_id].resv2.dword);
		ring_rxd_display_dword(ring[desc_id].resv2);

		return;
	}
#endif
	/* 16 bytes RX descriptor */
	ring[desc_id].lo_dword.dword =
		rte_le_to_cpu_64(ring[desc_id].lo_dword.dword);
	ring_rxd_display_dword(ring[desc_id].lo_dword);
	ring[desc_id].hi_dword.dword =
		rte_le_to_cpu_64(ring[desc_id].hi_dword.dword);
	ring_rxd_display_dword(ring[desc_id].hi_dword);
}

static void
ring_tx_descriptor_display(const struct rte_memzone *ring_mz, uint16_t desc_id)
{
	struct igb_ring_desc_16_bytes *ring;
	struct igb_ring_desc_16_bytes txd;

	ring = (struct igb_ring_desc_16_bytes *)ring_mz->addr;
	txd.lo_dword.dword = rte_le_to_cpu_64(ring[desc_id].lo_dword.dword);
	txd.hi_dword.dword = rte_le_to_cpu_64(ring[desc_id].hi_dword.dword);
	printf("    0x%08X - 0x%08X / 0x%08X - 0x%08X\n",
			(unsigned)txd.lo_dword.words.lo,
			(unsigned)txd.lo_dword.words.hi,
			(unsigned)txd.hi_dword.words.lo,
			(unsigned)txd.hi_dword.words.hi);
}

void
rx_ring_desc_display(portid_t port_id, queueid_t rxq_id, uint16_t rxd_id)
{
	const struct rte_memzone *rx_mz;

	if (rx_desc_id_is_invalid(port_id, rxq_id, rxd_id))
		return;
	rx_mz = ring_dma_zone_lookup("rx_ring", port_id, rxq_id);
	if (rx_mz == NULL)
		return;
	ring_rx_descriptor_display(rx_mz, port_id, rxd_id);
}

void
tx_ring_desc_display(portid_t port_id, queueid_t txq_id, uint16_t txd_id)
{
	const struct rte_memzone *tx_mz;

	if (tx_desc_id_is_invalid(port_id, txq_id, txd_id))
		return;
	tx_mz = ring_dma_zone_lookup("tx_ring", port_id, txq_id);
	if (tx_mz == NULL)
		return;
	ring_tx_descriptor_display(tx_mz, txd_id);
}

void
fwd_lcores_config_display(void)
{
	lcoreid_t lc_id;

	printf("List of forwarding lcores:");
	for (lc_id = 0; lc_id < nb_cfg_lcores; lc_id++)
		printf(" %2u", fwd_lcores_cpuids[lc_id]);
	printf("\n");
}
void
rxtx_config_display(void)
{
	portid_t pid;
	queueid_t qid;

	printf("  %s packet forwarding%s packets/burst=%d\n",
	       cur_fwd_eng->fwd_mode_name,
	       retry_enabled == 0 ? "" : " with retry",
	       nb_pkt_per_burst);

	if (cur_fwd_eng == &tx_only_engine || cur_fwd_eng == &flow_gen_engine)
		printf("  packet len=%u - nb packet segments=%d\n",
				(unsigned)tx_pkt_length, (int) tx_pkt_nb_segs);

	printf("  nb forwarding cores=%d - nb forwarding ports=%d\n",
	       nb_fwd_lcores, nb_fwd_ports);

	RTE_ETH_FOREACH_DEV(pid) {
		struct rte_eth_rxconf *rx_conf = &ports[pid].rxq[0].conf;
		struct rte_eth_txconf *tx_conf = &ports[pid].txq[0].conf;
		uint16_t *nb_rx_desc = &ports[pid].nb_rx_desc[0];
		uint16_t *nb_tx_desc = &ports[pid].nb_tx_desc[0];
		struct rte_eth_rxq_info rx_qinfo;
		struct rte_eth_txq_info tx_qinfo;
		uint16_t rx_free_thresh_tmp;
		uint16_t tx_free_thresh_tmp;
		uint16_t tx_rs_thresh_tmp;
		uint16_t nb_rx_desc_tmp;
		uint16_t nb_tx_desc_tmp;
		uint64_t offloads_tmp;
		uint8_t pthresh_tmp;
		uint8_t hthresh_tmp;
		uint8_t wthresh_tmp;
		int32_t rc;

		/* per port config */
		printf("  port %d: RX queue number: %d Tx queue number: %d\n",
				(unsigned int)pid, nb_rxq, nb_txq);

		printf("    Rx offloads=0x%"PRIx64" Tx offloads=0x%"PRIx64"\n",
				ports[pid].dev_conf.rxmode.offloads,
				ports[pid].dev_conf.txmode.offloads);

		/* per rx queue config only for first queue to be less verbose */
		for (qid = 0; qid < 1; qid++) {
			rc = rte_eth_rx_queue_info_get(pid, qid, &rx_qinfo);
			if (rc) {
				nb_rx_desc_tmp = nb_rx_desc[qid];
				rx_free_thresh_tmp =
					rx_conf[qid].rx_free_thresh;
				pthresh_tmp = rx_conf[qid].rx_thresh.pthresh;
				hthresh_tmp = rx_conf[qid].rx_thresh.hthresh;
				wthresh_tmp = rx_conf[qid].rx_thresh.wthresh;
				offloads_tmp = rx_conf[qid].offloads;
			} else {
				nb_rx_desc_tmp = rx_qinfo.nb_desc;
				rx_free_thresh_tmp =
						rx_qinfo.conf.rx_free_thresh;
				pthresh_tmp = rx_qinfo.conf.rx_thresh.pthresh;
				hthresh_tmp = rx_qinfo.conf.rx_thresh.hthresh;
				wthresh_tmp = rx_qinfo.conf.rx_thresh.wthresh;
				offloads_tmp = rx_qinfo.conf.offloads;
			}

			printf("    RX queue: %d\n", qid);
			printf("      RX desc=%d - RX free threshold=%d\n",
				nb_rx_desc_tmp, rx_free_thresh_tmp);
			printf("      RX threshold registers: pthresh=%d hthresh=%d "
				" wthresh=%d\n",
				pthresh_tmp, hthresh_tmp, wthresh_tmp);
			printf("      RX Offloads=0x%"PRIx64, offloads_tmp);
			if (rx_conf->share_group > 0)
				printf(" share_group=%u share_qid=%u",
				       rx_conf->share_group,
				       rx_conf->share_qid);
			printf("\n");
		}

		/* per tx queue config only for first queue to be less verbose */
		for (qid = 0; qid < 1; qid++) {
			rc = rte_eth_tx_queue_info_get(pid, qid, &tx_qinfo);
			if (rc) {
				nb_tx_desc_tmp = nb_tx_desc[qid];
				tx_free_thresh_tmp =
					tx_conf[qid].tx_free_thresh;
				pthresh_tmp = tx_conf[qid].tx_thresh.pthresh;
				hthresh_tmp = tx_conf[qid].tx_thresh.hthresh;
				wthresh_tmp = tx_conf[qid].tx_thresh.wthresh;
				offloads_tmp = tx_conf[qid].offloads;
				tx_rs_thresh_tmp = tx_conf[qid].tx_rs_thresh;
			} else {
				nb_tx_desc_tmp = tx_qinfo.nb_desc;
				tx_free_thresh_tmp =
						tx_qinfo.conf.tx_free_thresh;
				pthresh_tmp = tx_qinfo.conf.tx_thresh.pthresh;
				hthresh_tmp = tx_qinfo.conf.tx_thresh.hthresh;
				wthresh_tmp = tx_qinfo.conf.tx_thresh.wthresh;
				offloads_tmp = tx_qinfo.conf.offloads;
				tx_rs_thresh_tmp = tx_qinfo.conf.tx_rs_thresh;
			}

			printf("    TX queue: %d\n", qid);
			printf("      TX desc=%d - TX free threshold=%d\n",
				nb_tx_desc_tmp, tx_free_thresh_tmp);
			printf("      TX threshold registers: pthresh=%d hthresh=%d "
				" wthresh=%d\n",
				pthresh_tmp, hthresh_tmp, wthresh_tmp);
			printf("      TX offloads=0x%"PRIx64" - TX RS bit threshold=%d\n",
				offloads_tmp, tx_rs_thresh_tmp);
		}
	}
}

void
port_rss_reta_info(portid_t port_id,
		   struct rte_eth_rss_reta_entry64 *reta_conf,
		   uint16_t nb_entries)
{
	uint16_t i, idx, shift;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	ret = rte_eth_dev_rss_reta_query(port_id, reta_conf, nb_entries);
	if (ret != 0) {
		fprintf(stderr,
			"Failed to get RSS RETA info, return code = %d\n",
			ret);
		return;
	}

	for (i = 0; i < nb_entries; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (!(reta_conf[idx].mask & (1ULL << shift)))
			continue;
		printf("RSS RETA configuration: hash index=%u, queue=%u\n",
					i, reta_conf[idx].reta[shift]);
	}
}

/*
 * Displays the RSS hash functions of a port, and, optionally, the RSS hash
 * key of the port.
 */
void
port_rss_hash_conf_show(portid_t port_id, int show_rss_key)
{
	struct rte_eth_rss_conf rss_conf = {0};
	uint8_t rss_key[RSS_HASH_KEY_LENGTH];
	uint64_t rss_hf;
	uint8_t i;
	int diag;
	struct rte_eth_dev_info dev_info;
	uint8_t hash_key_size;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	if (dev_info.hash_key_size > 0 &&
			dev_info.hash_key_size <= sizeof(rss_key))
		hash_key_size = dev_info.hash_key_size;
	else {
		fprintf(stderr,
			"dev_info did not provide a valid hash key size\n");
		return;
	}

	/* Get RSS hash key if asked to display it */
	rss_conf.rss_key = (show_rss_key) ? rss_key : NULL;
	rss_conf.rss_key_len = hash_key_size;
	diag = rte_eth_dev_rss_hash_conf_get(port_id, &rss_conf);
	if (diag != 0) {
		switch (diag) {
		case -ENODEV:
			fprintf(stderr, "port index %d invalid\n", port_id);
			break;
		case -ENOTSUP:
			fprintf(stderr, "operation not supported by device\n");
			break;
		default:
			fprintf(stderr, "operation failed - diag=%d\n", diag);
			break;
		}
		return;
	}
	rss_hf = rss_conf.rss_hf;
	if (rss_hf == 0) {
		printf("RSS disabled\n");
		return;
	}
	printf("RSS functions:\n ");
	for (i = 0; rss_type_table[i].str; i++) {
		if (rss_type_table[i].rss_type == 0)
			continue;
		if ((rss_hf & rss_type_table[i].rss_type) == rss_type_table[i].rss_type)
			printf("%s ", rss_type_table[i].str);
	}
	printf("\n");
	if (!show_rss_key)
		return;
	printf("RSS key:\n");
	for (i = 0; i < hash_key_size; i++)
		printf("%02X", rss_key[i]);
	printf("\n");
}

void
port_rss_hash_key_update(portid_t port_id, char rss_type[], uint8_t *hash_key,
			 uint8_t hash_key_len)
{
	struct rte_eth_rss_conf rss_conf;
	int diag;
	unsigned int i;

	rss_conf.rss_key = NULL;
	rss_conf.rss_key_len = 0;
	rss_conf.rss_hf = 0;
	for (i = 0; rss_type_table[i].str; i++) {
		if (!strcmp(rss_type_table[i].str, rss_type))
			rss_conf.rss_hf = rss_type_table[i].rss_type;
	}
	diag = rte_eth_dev_rss_hash_conf_get(port_id, &rss_conf);
	if (diag == 0) {
		rss_conf.rss_key = hash_key;
		rss_conf.rss_key_len = hash_key_len;
		diag = rte_eth_dev_rss_hash_update(port_id, &rss_conf);
	}
	if (diag == 0)
		return;

	switch (diag) {
	case -ENODEV:
		fprintf(stderr, "port index %d invalid\n", port_id);
		break;
	case -ENOTSUP:
		fprintf(stderr, "operation not supported by device\n");
		break;
	default:
		fprintf(stderr, "operation failed - diag=%d\n", diag);
		break;
	}
}

/*
 * Check whether a shared rxq scheduled on other lcores.
 */
static bool
fwd_stream_on_other_lcores(uint16_t domain_id, lcoreid_t src_lc,
			   portid_t src_port, queueid_t src_rxq,
			   uint32_t share_group, queueid_t share_rxq)
{
	streamid_t sm_id;
	streamid_t nb_fs_per_lcore;
	lcoreid_t  nb_fc;
	lcoreid_t  lc_id;
	struct fwd_stream *fs;
	struct rte_port *port;
	struct rte_eth_dev_info *dev_info;
	struct rte_eth_rxconf *rxq_conf;

	nb_fc = cur_fwd_config.nb_fwd_lcores;
	/* Check remaining cores. */
	for (lc_id = src_lc + 1; lc_id < nb_fc; lc_id++) {
		sm_id = fwd_lcores[lc_id]->stream_idx;
		nb_fs_per_lcore = fwd_lcores[lc_id]->stream_nb;
		for (; sm_id < fwd_lcores[lc_id]->stream_idx + nb_fs_per_lcore;
		     sm_id++) {
			fs = fwd_streams[sm_id];
			port = &ports[fs->rx_port];
			dev_info = &port->dev_info;
			rxq_conf = &port->rxq[fs->rx_queue].conf;
			if ((dev_info->dev_capa & RTE_ETH_DEV_CAPA_RXQ_SHARE)
			    == 0 || rxq_conf->share_group == 0)
				/* Not shared rxq. */
				continue;
			if (domain_id != port->dev_info.switch_info.domain_id)
				continue;
			if (rxq_conf->share_group != share_group)
				continue;
			if (rxq_conf->share_qid != share_rxq)
				continue;
			printf("Shared Rx queue group %u queue %hu can't be scheduled on different cores:\n",
			       share_group, share_rxq);
			printf("  lcore %hhu Port %hu queue %hu\n",
			       src_lc, src_port, src_rxq);
			printf("  lcore %hhu Port %hu queue %hu\n",
			       lc_id, fs->rx_port, fs->rx_queue);
			printf("Please use --nb-cores=%hu to limit number of forwarding cores\n",
			       nb_rxq);
			return true;
		}
	}
	return false;
}

/*
 * Check shared rxq configuration.
 *
 * Shared group must not being scheduled on different core.
 */
bool
pkt_fwd_shared_rxq_check(void)
{
	streamid_t sm_id;
	streamid_t nb_fs_per_lcore;
	lcoreid_t  nb_fc;
	lcoreid_t  lc_id;
	struct fwd_stream *fs;
	uint16_t domain_id;
	struct rte_port *port;
	struct rte_eth_dev_info *dev_info;
	struct rte_eth_rxconf *rxq_conf;

	if (rxq_share == 0)
		return true;
	nb_fc = cur_fwd_config.nb_fwd_lcores;
	/*
	 * Check streams on each core, make sure the same switch domain +
	 * group + queue doesn't get scheduled on other cores.
	 */
	for (lc_id = 0; lc_id < nb_fc; lc_id++) {
		sm_id = fwd_lcores[lc_id]->stream_idx;
		nb_fs_per_lcore = fwd_lcores[lc_id]->stream_nb;
		for (; sm_id < fwd_lcores[lc_id]->stream_idx + nb_fs_per_lcore;
		     sm_id++) {
			fs = fwd_streams[sm_id];
			/* Update lcore info stream being scheduled. */
			fs->lcore = fwd_lcores[lc_id];
			port = &ports[fs->rx_port];
			dev_info = &port->dev_info;
			rxq_conf = &port->rxq[fs->rx_queue].conf;
			if ((dev_info->dev_capa & RTE_ETH_DEV_CAPA_RXQ_SHARE)
			    == 0 || rxq_conf->share_group == 0)
				/* Not shared rxq. */
				continue;
			/* Check shared rxq not scheduled on remaining cores. */
			domain_id = port->dev_info.switch_info.domain_id;
			if (fwd_stream_on_other_lcores(domain_id, lc_id,
						       fs->rx_port,
						       fs->rx_queue,
						       rxq_conf->share_group,
						       rxq_conf->share_qid))
				return false;
		}
	}
	return true;
}

/*
 * Setup forwarding configuration for each logical core.
 */
static void
setup_fwd_config_of_each_lcore(struct fwd_config *cfg)
{
	streamid_t nb_fs_per_lcore;
	streamid_t nb_fs;
	streamid_t sm_id;
	lcoreid_t  nb_extra;
	lcoreid_t  nb_fc;
	lcoreid_t  nb_lc;
	lcoreid_t  lc_id;

	nb_fs = cfg->nb_fwd_streams;
	nb_fc = cfg->nb_fwd_lcores;
	if (nb_fs <= nb_fc) {
		nb_fs_per_lcore = 1;
		nb_extra = 0;
	} else {
		nb_fs_per_lcore = (streamid_t) (nb_fs / nb_fc);
		nb_extra = (lcoreid_t) (nb_fs % nb_fc);
	}

	nb_lc = (lcoreid_t) (nb_fc - nb_extra);
	sm_id = 0;
	for (lc_id = 0; lc_id < nb_lc; lc_id++) {
		fwd_lcores[lc_id]->stream_idx = sm_id;
		fwd_lcores[lc_id]->stream_nb = nb_fs_per_lcore;
		sm_id = (streamid_t) (sm_id + nb_fs_per_lcore);
	}

	/*
	 * Assign extra remaining streams, if any.
	 */
	nb_fs_per_lcore = (streamid_t) (nb_fs_per_lcore + 1);
	for (lc_id = 0; lc_id < nb_extra; lc_id++) {
		fwd_lcores[nb_lc + lc_id]->stream_idx = sm_id;
		fwd_lcores[nb_lc + lc_id]->stream_nb = nb_fs_per_lcore;
		sm_id = (streamid_t) (sm_id + nb_fs_per_lcore);
	}
}

static portid_t
fwd_topology_tx_port_get(portid_t rxp)
{
	static int warning_once = 1;

	RTE_ASSERT(rxp < cur_fwd_config.nb_fwd_ports);

	switch (port_topology) {
	default:
	case PORT_TOPOLOGY_PAIRED:
		if ((rxp & 0x1) == 0) {
			if (rxp + 1 < cur_fwd_config.nb_fwd_ports)
				return rxp + 1;
			if (warning_once) {
				fprintf(stderr,
					"\nWarning! port-topology=paired and odd forward ports number, the last port will pair with itself.\n\n");
				warning_once = 0;
			}
			return rxp;
		}
		return rxp - 1;
	case PORT_TOPOLOGY_CHAINED:
		return (rxp + 1) % cur_fwd_config.nb_fwd_ports;
	case PORT_TOPOLOGY_LOOP:
		return rxp;
	}
}

static void
simple_fwd_config_setup(void)
{
	portid_t i;

	cur_fwd_config.nb_fwd_ports = (portid_t) nb_fwd_ports;
	cur_fwd_config.nb_fwd_streams =
		(streamid_t) cur_fwd_config.nb_fwd_ports;

	/* reinitialize forwarding streams */
	init_fwd_streams();

	/*
	 * In the simple forwarding test, the number of forwarding cores
	 * must be lower or equal to the number of forwarding ports.
	 */
	cur_fwd_config.nb_fwd_lcores = (lcoreid_t) nb_fwd_lcores;
	if (cur_fwd_config.nb_fwd_lcores > cur_fwd_config.nb_fwd_ports)
		cur_fwd_config.nb_fwd_lcores =
			(lcoreid_t) cur_fwd_config.nb_fwd_ports;
	setup_fwd_config_of_each_lcore(&cur_fwd_config);

	for (i = 0; i < cur_fwd_config.nb_fwd_ports; i++) {
		fwd_streams[i]->rx_port   = fwd_ports_ids[i];
		fwd_streams[i]->rx_queue  = 0;
		fwd_streams[i]->tx_port   =
				fwd_ports_ids[fwd_topology_tx_port_get(i)];
		fwd_streams[i]->tx_queue  = 0;
		fwd_streams[i]->peer_addr = fwd_streams[i]->tx_port;
		fwd_streams[i]->retry_enabled = retry_enabled;
	}
}

/**
 * For the RSS forwarding test all streams distributed over lcores. Each stream
 * being composed of a RX queue to poll on a RX port for input messages,
 * associated with a TX queue of a TX port where to send forwarded packets.
 */
static void
rss_fwd_config_setup(void)
{
	portid_t   rxp;
	portid_t   txp;
	queueid_t  rxq;
	queueid_t  nb_q;
	streamid_t  sm_id;
	int start;
	int end;

	nb_q = nb_rxq;
	if (nb_q > nb_txq)
		nb_q = nb_txq;
	cur_fwd_config.nb_fwd_lcores = (lcoreid_t) nb_fwd_lcores;
	cur_fwd_config.nb_fwd_ports = nb_fwd_ports;
	cur_fwd_config.nb_fwd_streams =
		(streamid_t) (nb_q * cur_fwd_config.nb_fwd_ports);

	if (cur_fwd_config.nb_fwd_streams < cur_fwd_config.nb_fwd_lcores)
		cur_fwd_config.nb_fwd_lcores =
			(lcoreid_t)cur_fwd_config.nb_fwd_streams;

	/* reinitialize forwarding streams */
	init_fwd_streams();

	setup_fwd_config_of_each_lcore(&cur_fwd_config);

	if (proc_id > 0 && nb_q % num_procs != 0)
		printf("Warning! queue numbers should be multiple of processes, or packet loss will happen.\n");

	/**
	 * In multi-process, All queues are allocated to different
	 * processes based on num_procs and proc_id. For example:
	 * if supports 4 queues(nb_q), 2 processes(num_procs),
	 * the 0~1 queue for primary process.
	 * the 2~3 queue for secondary process.
	 */
	start = proc_id * nb_q / num_procs;
	end = start + nb_q / num_procs;
	rxp = 0;
	rxq = start;
	for (sm_id = 0; sm_id < cur_fwd_config.nb_fwd_streams; sm_id++) {
		struct fwd_stream *fs;

		fs = fwd_streams[sm_id];
		txp = fwd_topology_tx_port_get(rxp);
		fs->rx_port = fwd_ports_ids[rxp];
		fs->rx_queue = rxq;
		fs->tx_port = fwd_ports_ids[txp];
		fs->tx_queue = rxq;
		fs->peer_addr = fs->tx_port;
		fs->retry_enabled = retry_enabled;
		rxp++;
		if (rxp < nb_fwd_ports)
			continue;
		rxp = 0;
		rxq++;
		if (rxq >= end)
			rxq = start;
	}
}

static uint16_t
get_fwd_port_total_tc_num(void)
{
	struct rte_eth_dcb_info dcb_info;
	uint16_t total_tc_num = 0;
	unsigned int i;

	for (i = 0; i < nb_fwd_ports; i++) {
		(void)rte_eth_dev_get_dcb_info(fwd_ports_ids[i], &dcb_info);
		total_tc_num += dcb_info.nb_tcs;
	}

	return total_tc_num;
}

/**
 * For the DCB forwarding test, each core is assigned on each traffic class.
 *
 * Each core is assigned a multi-stream, each stream being composed of
 * a RX queue to poll on a RX port for input messages, associated with
 * a TX queue of a TX port where to send forwarded packets. All RX and
 * TX queues are mapping to the same traffic class.
 * If VMDQ and DCB co-exist, each traffic class on different POOLs share
 * the same core
 */
static void
dcb_fwd_config_setup(void)
{
	struct rte_eth_dcb_info rxp_dcb_info, txp_dcb_info;
	portid_t txp, rxp = 0;
	queueid_t txq, rxq = 0;
	lcoreid_t  lc_id;
	uint16_t nb_rx_queue, nb_tx_queue;
	uint16_t i, j, k, sm_id = 0;
	uint16_t total_tc_num;
	struct rte_port *port;
	uint8_t tc = 0;
	portid_t pid;
	int ret;

	/*
	 * The fwd_config_setup() is called when the port is RTE_PORT_STARTED
	 * or RTE_PORT_STOPPED.
	 *
	 * Re-configure ports to get updated mapping between tc and queue in
	 * case the queue number of the port is changed. Skip for started ports
	 * since modifying queue number and calling dev_configure need to stop
	 * ports first.
	 */
	for (pid = 0; pid < nb_fwd_ports; pid++) {
		if (port_is_started(pid) == 1)
			continue;

		port = &ports[pid];
		ret = rte_eth_dev_configure(pid, nb_rxq, nb_txq,
					    &port->dev_conf);
		if (ret < 0) {
			fprintf(stderr,
				"Failed to re-configure port %d, ret = %d.\n",
				pid, ret);
			return;
		}
	}

	cur_fwd_config.nb_fwd_lcores = (lcoreid_t) nb_fwd_lcores;
	cur_fwd_config.nb_fwd_ports = nb_fwd_ports;
	cur_fwd_config.nb_fwd_streams =
		(streamid_t) (nb_rxq * cur_fwd_config.nb_fwd_ports);
	total_tc_num = get_fwd_port_total_tc_num();
	if (cur_fwd_config.nb_fwd_lcores > total_tc_num)
		cur_fwd_config.nb_fwd_lcores = total_tc_num;

	/* reinitialize forwarding streams */
	init_fwd_streams();
	sm_id = 0;
	txp = 1;
	/* get the dcb info on the first RX and TX ports */
	(void)rte_eth_dev_get_dcb_info(fwd_ports_ids[rxp], &rxp_dcb_info);
	(void)rte_eth_dev_get_dcb_info(fwd_ports_ids[txp], &txp_dcb_info);

	for (lc_id = 0; lc_id < cur_fwd_config.nb_fwd_lcores; lc_id++) {
		fwd_lcores[lc_id]->stream_nb = 0;
		fwd_lcores[lc_id]->stream_idx = sm_id;
		for (i = 0; i < RTE_ETH_MAX_VMDQ_POOL; i++) {
			/* if the nb_queue is zero, means this tc is
			 * not enabled on the POOL
			 */
			if (rxp_dcb_info.tc_queue.tc_rxq[i][tc].nb_queue == 0)
				break;
			k = fwd_lcores[lc_id]->stream_nb +
				fwd_lcores[lc_id]->stream_idx;
			rxq = rxp_dcb_info.tc_queue.tc_rxq[i][tc].base;
			txq = txp_dcb_info.tc_queue.tc_txq[i][tc].base;
			nb_rx_queue = txp_dcb_info.tc_queue.tc_rxq[i][tc].nb_queue;
			nb_tx_queue = txp_dcb_info.tc_queue.tc_txq[i][tc].nb_queue;
			for (j = 0; j < nb_rx_queue; j++) {
				struct fwd_stream *fs;

				fs = fwd_streams[k + j];
				fs->rx_port = fwd_ports_ids[rxp];
				fs->rx_queue = rxq + j;
				fs->tx_port = fwd_ports_ids[txp];
				fs->tx_queue = txq + j % nb_tx_queue;
				fs->peer_addr = fs->tx_port;
				fs->retry_enabled = retry_enabled;
			}
			fwd_lcores[lc_id]->stream_nb +=
				rxp_dcb_info.tc_queue.tc_rxq[i][tc].nb_queue;
		}
		sm_id = (streamid_t) (sm_id + fwd_lcores[lc_id]->stream_nb);

		tc++;
		if (tc < rxp_dcb_info.nb_tcs)
			continue;
		/* Restart from TC 0 on next RX port */
		tc = 0;
		if (numa_support && (nb_fwd_ports <= (nb_ports >> 1)))
			rxp = (portid_t)
				(rxp + ((nb_ports >> 1) / nb_fwd_ports));
		else
			rxp++;
		if (rxp >= nb_fwd_ports)
			return;
		/* get the dcb information on next RX and TX ports */
		if ((rxp & 0x1) == 0)
			txp = (portid_t) (rxp + 1);
		else
			txp = (portid_t) (rxp - 1);
		rte_eth_dev_get_dcb_info(fwd_ports_ids[rxp], &rxp_dcb_info);
		rte_eth_dev_get_dcb_info(fwd_ports_ids[txp], &txp_dcb_info);
	}
}

static void
icmp_echo_config_setup(void)
{
	portid_t  rxp;
	queueid_t rxq;
	lcoreid_t lc_id;
	uint16_t  sm_id;

	if ((nb_txq * nb_fwd_ports) < nb_fwd_lcores)
		cur_fwd_config.nb_fwd_lcores = (lcoreid_t)
			(nb_txq * nb_fwd_ports);
	else
		cur_fwd_config.nb_fwd_lcores = (lcoreid_t) nb_fwd_lcores;
	cur_fwd_config.nb_fwd_ports = nb_fwd_ports;
	cur_fwd_config.nb_fwd_streams =
		(streamid_t) (nb_rxq * cur_fwd_config.nb_fwd_ports);
	if (cur_fwd_config.nb_fwd_streams < cur_fwd_config.nb_fwd_lcores)
		cur_fwd_config.nb_fwd_lcores =
			(lcoreid_t)cur_fwd_config.nb_fwd_streams;
	if (verbose_level > 0) {
		printf("%s fwd_cores=%d fwd_ports=%d fwd_streams=%d\n",
		       __FUNCTION__,
		       cur_fwd_config.nb_fwd_lcores,
		       cur_fwd_config.nb_fwd_ports,
		       cur_fwd_config.nb_fwd_streams);
	}

	/* reinitialize forwarding streams */
	init_fwd_streams();
	setup_fwd_config_of_each_lcore(&cur_fwd_config);
	rxp = 0; rxq = 0;
	for (lc_id = 0; lc_id < cur_fwd_config.nb_fwd_lcores; lc_id++) {
		if (verbose_level > 0)
			printf("  core=%d: \n", lc_id);
		for (sm_id = 0; sm_id < fwd_lcores[lc_id]->stream_nb; sm_id++) {
			struct fwd_stream *fs;
			fs = fwd_streams[fwd_lcores[lc_id]->stream_idx + sm_id];
			fs->rx_port = fwd_ports_ids[rxp];
			fs->rx_queue = rxq;
			fs->tx_port = fs->rx_port;
			fs->tx_queue = rxq;
			fs->peer_addr = fs->tx_port;
			fs->retry_enabled = retry_enabled;
			if (verbose_level > 0)
				printf("  stream=%d port=%d rxq=%d txq=%d\n",
				       sm_id, fs->rx_port, fs->rx_queue,
				       fs->tx_queue);
			rxq = (queueid_t) (rxq + 1);
			if (rxq == nb_rxq) {
				rxq = 0;
				rxp = (portid_t) (rxp + 1);
			}
		}
	}
}

void
fwd_config_setup(void)
{
	struct rte_port *port;
	portid_t pt_id;
	unsigned int i;

	cur_fwd_config.fwd_eng = cur_fwd_eng;
	if (strcmp(cur_fwd_eng->fwd_mode_name, "icmpecho") == 0) {
		icmp_echo_config_setup();
		return;
	}

	if ((nb_rxq > 1) && (nb_txq > 1)){
		if (dcb_config) {
			for (i = 0; i < nb_fwd_ports; i++) {
				pt_id = fwd_ports_ids[i];
				port = &ports[pt_id];
				if (!port->dcb_flag) {
					fprintf(stderr,
						"In DCB mode, all forwarding ports must be configured in this mode.\n");
					return;
				}
			}
			if (nb_fwd_lcores == 1) {
				fprintf(stderr,
					"In DCB mode,the nb forwarding cores should be larger than 1.\n");
				return;
			}

			dcb_fwd_config_setup();
		} else
			rss_fwd_config_setup();
	}
	else
		simple_fwd_config_setup();
}

static const char *
mp_alloc_to_str(uint8_t mode)
{
	switch (mode) {
	case MP_ALLOC_NATIVE:
		return "native";
	case MP_ALLOC_ANON:
		return "anon";
	case MP_ALLOC_XMEM:
		return "xmem";
	case MP_ALLOC_XMEM_HUGE:
		return "xmemhuge";
	case MP_ALLOC_XBUF:
		return "xbuf";
	default:
		return "invalid";
	}
}

void
pkt_fwd_config_display(struct fwd_config *cfg)
{
	struct fwd_stream *fs;
	lcoreid_t  lc_id;
	streamid_t sm_id;

	printf("%s packet forwarding%s - ports=%d - cores=%d - streams=%d - "
		"NUMA support %s, MP allocation mode: %s\n",
		cfg->fwd_eng->fwd_mode_name,
		retry_enabled == 0 ? "" : " with retry",
		cfg->nb_fwd_ports, cfg->nb_fwd_lcores, cfg->nb_fwd_streams,
		numa_support == 1 ? "enabled" : "disabled",
		mp_alloc_to_str(mp_alloc_type));

	if (retry_enabled)
		printf("TX retry num: %u, delay between TX retries: %uus\n",
			burst_tx_retry_num, burst_tx_delay_time);
	for (lc_id = 0; lc_id < cfg->nb_fwd_lcores; lc_id++) {
		printf("Logical Core %u (socket %u) forwards packets on "
		       "%d streams:",
		       fwd_lcores_cpuids[lc_id],
		       rte_lcore_to_socket_id(fwd_lcores_cpuids[lc_id]),
		       fwd_lcores[lc_id]->stream_nb);
		for (sm_id = 0; sm_id < fwd_lcores[lc_id]->stream_nb; sm_id++) {
			fs = fwd_streams[fwd_lcores[lc_id]->stream_idx + sm_id];
			printf("\n  RX P=%d/Q=%d (socket %u) -> TX "
			       "P=%d/Q=%d (socket %u) ",
			       fs->rx_port, fs->rx_queue,
			       ports[fs->rx_port].socket_id,
			       fs->tx_port, fs->tx_queue,
			       ports[fs->tx_port].socket_id);
			print_ethaddr("peer=",
				      &peer_eth_addrs[fs->peer_addr]);
		}
		printf("\n");
	}
	printf("\n");
}

void
set_fwd_eth_peer(portid_t port_id, char *peer_addr)
{
	struct rte_ether_addr new_peer_addr;
	if (!rte_eth_dev_is_valid_port(port_id)) {
		fprintf(stderr, "Error: Invalid port number %i\n", port_id);
		return;
	}
	if (rte_ether_unformat_addr(peer_addr, &new_peer_addr) < 0) {
		fprintf(stderr, "Error: Invalid ethernet address: %s\n",
			peer_addr);
		return;
	}
	peer_eth_addrs[port_id] = new_peer_addr;
}

int
set_fwd_lcores_list(unsigned int *lcorelist, unsigned int nb_lc)
{
	unsigned int i;
	unsigned int lcore_cpuid;
	int record_now;

	record_now = 0;
 again:
	for (i = 0; i < nb_lc; i++) {
		lcore_cpuid = lcorelist[i];
		if (! rte_lcore_is_enabled(lcore_cpuid)) {
			fprintf(stderr, "lcore %u not enabled\n", lcore_cpuid);
			return -1;
		}
		if (lcore_cpuid == rte_get_main_lcore()) {
			fprintf(stderr,
				"lcore %u cannot be masked on for running packet forwarding, which is the main lcore and reserved for command line parsing only\n",
				lcore_cpuid);
			return -1;
		}
		if (record_now)
			fwd_lcores_cpuids[i] = lcore_cpuid;
	}
	if (record_now == 0) {
		record_now = 1;
		goto again;
	}
	nb_cfg_lcores = (lcoreid_t) nb_lc;
	if (nb_fwd_lcores != (lcoreid_t) nb_lc) {
		printf("previous number of forwarding cores %u - changed to "
		       "number of configured cores %u\n",
		       (unsigned int) nb_fwd_lcores, nb_lc);
		nb_fwd_lcores = (lcoreid_t) nb_lc;
	}

	return 0;
}

int
set_fwd_lcores_mask(uint64_t lcoremask)
{
	unsigned int lcorelist[64];
	unsigned int nb_lc;
	unsigned int i;

	if (lcoremask == 0) {
		fprintf(stderr, "Invalid NULL mask of cores\n");
		return -1;
	}
	nb_lc = 0;
	for (i = 0; i < 64; i++) {
		if (! ((uint64_t)(1ULL << i) & lcoremask))
			continue;
		lcorelist[nb_lc++] = i;
	}
	return set_fwd_lcores_list(lcorelist, nb_lc);
}

void
set_fwd_lcores_number(uint16_t nb_lc)
{
	if (test_done == 0) {
		fprintf(stderr, "Please stop forwarding first\n");
		return;
	}
	if (nb_lc > nb_cfg_lcores) {
		fprintf(stderr,
			"nb fwd cores %u > %u (max. number of configured lcores) - ignored\n",
			(unsigned int) nb_lc, (unsigned int) nb_cfg_lcores);
		return;
	}
	nb_fwd_lcores = (lcoreid_t) nb_lc;
	printf("Number of forwarding cores set to %u\n",
	       (unsigned int) nb_fwd_lcores);
}

void
set_fwd_ports_list(unsigned int *portlist, unsigned int nb_pt)
{
	unsigned int i;
	portid_t port_id;
	int record_now;

	record_now = 0;
 again:
	for (i = 0; i < nb_pt; i++) {
		port_id = (portid_t) portlist[i];
		if (port_id_is_invalid(port_id, ENABLED_WARN))
			return;
		if (record_now)
			fwd_ports_ids[i] = port_id;
	}
	if (record_now == 0) {
		record_now = 1;
		goto again;
	}
	nb_cfg_ports = (portid_t) nb_pt;
	if (nb_fwd_ports != (portid_t) nb_pt) {
		printf("previous number of forwarding ports %u - changed to "
		       "number of configured ports %u\n",
		       (unsigned int) nb_fwd_ports, nb_pt);
		nb_fwd_ports = (portid_t) nb_pt;
	}
}

/**
 * Parse the user input and obtain the list of forwarding ports
 *
 * @param[in] list
 *   String containing the user input. User can specify
 *   in these formats 1,3,5 or 1-3 or 1-2,5 or 3,5-6.
 *   For example, if the user wants to use all the available
 *   4 ports in his system, then the input can be 0-3 or 0,1,2,3.
 *   If the user wants to use only the ports 1,2 then the input
 *   is 1,2.
 *   valid characters are '-' and ','
 * @param[out] values
 *   This array will be filled with a list of port IDs
 *   based on the user input
 *   Note that duplicate entries are discarded and only the first
 *   count entries in this array are port IDs and all the rest
 *   will contain default values
 * @param[in] maxsize
 *   This parameter denotes 2 things
 *   1) Number of elements in the values array
 *   2) Maximum value of each element in the values array
 * @return
 *   On success, returns total count of parsed port IDs
 *   On failure, returns 0
 */
static unsigned int
parse_port_list(const char *list, unsigned int *values, unsigned int maxsize)
{
	unsigned int count = 0;
	char *end = NULL;
	int min, max;
	int value, i;
	unsigned int marked[maxsize];

	if (list == NULL || values == NULL)
		return 0;

	for (i = 0; i < (int)maxsize; i++)
		marked[i] = 0;

	min = INT_MAX;

	do {
		/*Remove the blank spaces if any*/
		while (isblank(*list))
			list++;
		if (*list == '\0')
			break;
		errno = 0;
		value = strtol(list, &end, 10);
		if (errno || end == NULL)
			return 0;
		if (value < 0 || value >= (int)maxsize)
			return 0;
		while (isblank(*end))
			end++;
		if (*end == '-' && min == INT_MAX) {
			min = value;
		} else if ((*end == ',') || (*end == '\0')) {
			max = value;
			if (min == INT_MAX)
				min = value;
			for (i = min; i <= max; i++) {
				if (count < maxsize) {
					if (marked[i])
						continue;
					values[count] = i;
					marked[i] = 1;
					count++;
				}
			}
			min = INT_MAX;
		} else
			return 0;
		list = end + 1;
	} while (*end != '\0');

	return count;
}

void
parse_fwd_portlist(const char *portlist)
{
	unsigned int portcount;
	unsigned int portindex[RTE_MAX_ETHPORTS];
	unsigned int i, valid_port_count = 0;

	portcount = parse_port_list(portlist, portindex, RTE_MAX_ETHPORTS);
	if (!portcount)
		rte_exit(EXIT_FAILURE, "Invalid fwd port list\n");

	/*
	 * Here we verify the validity of the ports
	 * and thereby calculate the total number of
	 * valid ports
	 */
	for (i = 0; i < portcount && i < RTE_DIM(portindex); i++) {
		if (rte_eth_dev_is_valid_port(portindex[i])) {
			portindex[valid_port_count] = portindex[i];
			valid_port_count++;
		}
	}

	set_fwd_ports_list(portindex, valid_port_count);
}

void
set_fwd_ports_mask(uint64_t portmask)
{
	unsigned int portlist[64];
	unsigned int nb_pt;
	unsigned int i;

	if (portmask == 0) {
		fprintf(stderr, "Invalid NULL mask of ports\n");
		return;
	}
	nb_pt = 0;
	RTE_ETH_FOREACH_DEV(i) {
		if (! ((uint64_t)(1ULL << i) & portmask))
			continue;
		portlist[nb_pt++] = i;
	}
	set_fwd_ports_list(portlist, nb_pt);
}

void
set_fwd_ports_number(uint16_t nb_pt)
{
	if (nb_pt > nb_cfg_ports) {
		fprintf(stderr,
			"nb fwd ports %u > %u (number of configured ports) - ignored\n",
			(unsigned int) nb_pt, (unsigned int) nb_cfg_ports);
		return;
	}
	nb_fwd_ports = (portid_t) nb_pt;
	printf("Number of forwarding ports set to %u\n",
	       (unsigned int) nb_fwd_ports);
}

int
port_is_forwarding(portid_t port_id)
{
	unsigned int i;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return -1;

	for (i = 0; i < nb_fwd_ports; i++) {
		if (fwd_ports_ids[i] == port_id)
			return 1;
	}

	return 0;
}

void
set_nb_pkt_per_burst(uint16_t nb)
{
	if (nb > MAX_PKT_BURST) {
		fprintf(stderr,
			"nb pkt per burst: %u > %u (maximum packet per burst)  ignored\n",
			(unsigned int) nb, (unsigned int) MAX_PKT_BURST);
		return;
	}
	nb_pkt_per_burst = nb;
	printf("Number of packets per burst set to %u\n",
	       (unsigned int) nb_pkt_per_burst);
}

static const char *
tx_split_get_name(enum tx_pkt_split split)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(tx_split_name); i++) {
		if (tx_split_name[i].split == split)
			return tx_split_name[i].name;
	}
	return NULL;
}

void
set_tx_pkt_split(const char *name)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(tx_split_name); i++) {
		if (strcmp(tx_split_name[i].name, name) == 0) {
			tx_pkt_split = tx_split_name[i].split;
			return;
		}
	}
	fprintf(stderr, "unknown value: \"%s\"\n", name);
}

int
parse_fec_mode(const char *name, uint32_t *fec_capa)
{
	uint8_t i;

	for (i = 0; i < RTE_DIM(fec_mode_name); i++) {
		if (strcmp(fec_mode_name[i].name, name) == 0) {
			*fec_capa =
				RTE_ETH_FEC_MODE_TO_CAPA(fec_mode_name[i].mode);
			return 0;
		}
	}
	return -1;
}

void
show_fec_capability(unsigned int num, struct rte_eth_fec_capa *speed_fec_capa)
{
	unsigned int i, j;

	printf("FEC capabilities:\n");

	for (i = 0; i < num; i++) {
		printf("%s : ",
			rte_eth_link_speed_to_str(speed_fec_capa[i].speed));

		for (j = 0; j < RTE_DIM(fec_mode_name); j++) {
			if (RTE_ETH_FEC_MODE_TO_CAPA(j) &
						speed_fec_capa[i].capa)
				printf("%s ", fec_mode_name[j].name);
		}
		printf("\n");
	}
}

void
show_rx_pkt_offsets(void)
{
	uint32_t i, n;

	n = rx_pkt_nb_offs;
	printf("Number of offsets: %u\n", n);
	if (n) {
		printf("Segment offsets: ");
		for (i = 0; i != n - 1; i++)
			printf("%hu,", rx_pkt_seg_offsets[i]);
		printf("%hu\n", rx_pkt_seg_lengths[i]);
	}
}

void
set_rx_pkt_offsets(unsigned int *seg_offsets, unsigned int nb_offs)
{
	unsigned int i;

	if (nb_offs >= MAX_SEGS_BUFFER_SPLIT) {
		printf("nb segments per RX packets=%u >= "
		       "MAX_SEGS_BUFFER_SPLIT - ignored\n", nb_offs);
		return;
	}

	/*
	 * No extra check here, the segment length will be checked by PMD
	 * in the extended queue setup.
	 */
	for (i = 0; i < nb_offs; i++) {
		if (seg_offsets[i] >= UINT16_MAX) {
			printf("offset[%u]=%u > UINT16_MAX - give up\n",
			       i, seg_offsets[i]);
			return;
		}
	}

	for (i = 0; i < nb_offs; i++)
		rx_pkt_seg_offsets[i] = (uint16_t) seg_offsets[i];

	rx_pkt_nb_offs = (uint8_t) nb_offs;
}

void
show_rx_pkt_segments(void)
{
	uint32_t i, n;

	n = rx_pkt_nb_segs;
	printf("Number of segments: %u\n", n);
	if (n) {
		printf("Segment sizes: ");
		for (i = 0; i != n - 1; i++)
			printf("%hu,", rx_pkt_seg_lengths[i]);
		printf("%hu\n", rx_pkt_seg_lengths[i]);
	}
}

void
set_rx_pkt_segments(unsigned int *seg_lengths, unsigned int nb_segs)
{
	unsigned int i;

	if (nb_segs >= MAX_SEGS_BUFFER_SPLIT) {
		printf("nb segments per RX packets=%u >= "
		       "MAX_SEGS_BUFFER_SPLIT - ignored\n", nb_segs);
		return;
	}

	/*
	 * No extra check here, the segment length will be checked by PMD
	 * in the extended queue setup.
	 */
	for (i = 0; i < nb_segs; i++) {
		if (seg_lengths[i] >= UINT16_MAX) {
			printf("length[%u]=%u > UINT16_MAX - give up\n",
			       i, seg_lengths[i]);
			return;
		}
	}

	for (i = 0; i < nb_segs; i++)
		rx_pkt_seg_lengths[i] = (uint16_t) seg_lengths[i];

	rx_pkt_nb_segs = (uint8_t) nb_segs;
}

void
show_tx_pkt_segments(void)
{
	uint32_t i, n;
	const char *split;

	n = tx_pkt_nb_segs;
	split = tx_split_get_name(tx_pkt_split);

	printf("Number of segments: %u\n", n);
	printf("Segment sizes: ");
	for (i = 0; i != n - 1; i++)
		printf("%hu,", tx_pkt_seg_lengths[i]);
	printf("%hu\n", tx_pkt_seg_lengths[i]);
	printf("Split packet: %s\n", split);
}

static bool
nb_segs_is_invalid(unsigned int nb_segs)
{
	uint16_t ring_size;
	uint16_t queue_id;
	uint16_t port_id;
	int ret;

	RTE_ETH_FOREACH_DEV(port_id) {
		for (queue_id = 0; queue_id < nb_txq; queue_id++) {
			ret = get_tx_ring_size(port_id, queue_id, &ring_size);
			if (ret) {
				/* Port may not be initialized yet, can't say
				 * the port is invalid in this stage.
				 */
				continue;
			}
			if (ring_size < nb_segs) {
				printf("nb segments per TX packets=%u >= TX "
				       "queue(%u) ring_size=%u - txpkts ignored\n",
				       nb_segs, queue_id, ring_size);
				return true;
			}
		}
	}

	return false;
}

void
set_tx_pkt_segments(unsigned int *seg_lengths, unsigned int nb_segs)
{
	uint16_t tx_pkt_len;
	unsigned int i;

	/*
	 * For single segment settings failed check is ignored.
	 * It is a very basic capability to send the single segment
	 * packets, suppose it is always supported.
	 */
	if (nb_segs > 1 && nb_segs_is_invalid(nb_segs)) {
		fprintf(stderr,
			"Tx segment size(%u) is not supported - txpkts ignored\n",
			nb_segs);
		return;
	}

	if (nb_segs > RTE_MAX_SEGS_PER_PKT) {
		fprintf(stderr,
			"Tx segment size(%u) is bigger than max number of segment(%u)\n",
			nb_segs, RTE_MAX_SEGS_PER_PKT);
		return;
	}

	/*
	 * Check that each segment length is greater or equal than
	 * the mbuf data size.
	 * Check also that the total packet length is greater or equal than the
	 * size of an empty UDP/IP packet (sizeof(struct rte_ether_hdr) +
	 * 20 + 8).
	 */
	tx_pkt_len = 0;
	for (i = 0; i < nb_segs; i++) {
		if (seg_lengths[i] > mbuf_data_size[0]) {
			fprintf(stderr,
				"length[%u]=%u > mbuf_data_size=%u - give up\n",
				i, seg_lengths[i], mbuf_data_size[0]);
			return;
		}
		tx_pkt_len = (uint16_t)(tx_pkt_len + seg_lengths[i]);
	}
	if (tx_pkt_len < (sizeof(struct rte_ether_hdr) + 20 + 8)) {
		fprintf(stderr, "total packet length=%u < %d - give up\n",
				(unsigned) tx_pkt_len,
				(int)(sizeof(struct rte_ether_hdr) + 20 + 8));
		return;
	}

	for (i = 0; i < nb_segs; i++)
		tx_pkt_seg_lengths[i] = (uint16_t) seg_lengths[i];

	tx_pkt_length  = tx_pkt_len;
	tx_pkt_nb_segs = (uint8_t) nb_segs;
}

void
show_tx_pkt_times(void)
{
	printf("Interburst gap: %u\n", tx_pkt_times_inter);
	printf("Intraburst gap: %u\n", tx_pkt_times_intra);
}

void
set_tx_pkt_times(unsigned int *tx_times)
{
	tx_pkt_times_inter = tx_times[0];
	tx_pkt_times_intra = tx_times[1];
}

#ifdef RTE_LIB_GRO
void
setup_gro(const char *onoff, portid_t port_id)
{
	if (!rte_eth_dev_is_valid_port(port_id)) {
		fprintf(stderr, "invalid port id %u\n", port_id);
		return;
	}
	if (test_done == 0) {
		fprintf(stderr,
			"Before enable/disable GRO, please stop forwarding first\n");
		return;
	}
	if (strcmp(onoff, "on") == 0) {
		if (gro_ports[port_id].enable != 0) {
			fprintf(stderr,
				"Port %u has enabled GRO. Please disable GRO first\n",
				port_id);
			return;
		}
		if (gro_flush_cycles == GRO_DEFAULT_FLUSH_CYCLES) {
			gro_ports[port_id].param.gro_types = RTE_GRO_TCP_IPV4;
			gro_ports[port_id].param.max_flow_num =
				GRO_DEFAULT_FLOW_NUM;
			gro_ports[port_id].param.max_item_per_flow =
				GRO_DEFAULT_ITEM_NUM_PER_FLOW;
		}
		gro_ports[port_id].enable = 1;
	} else {
		if (gro_ports[port_id].enable == 0) {
			fprintf(stderr, "Port %u has disabled GRO\n", port_id);
			return;
		}
		gro_ports[port_id].enable = 0;
	}
}

void
setup_gro_flush_cycles(uint8_t cycles)
{
	if (test_done == 0) {
		fprintf(stderr,
			"Before change flush interval for GRO, please stop forwarding first.\n");
		return;
	}

	if (cycles > GRO_MAX_FLUSH_CYCLES || cycles <
			GRO_DEFAULT_FLUSH_CYCLES) {
		fprintf(stderr,
			"The flushing cycle be in the range of 1 to %u. Revert to the default value %u.\n",
			GRO_MAX_FLUSH_CYCLES, GRO_DEFAULT_FLUSH_CYCLES);
		cycles = GRO_DEFAULT_FLUSH_CYCLES;
	}

	gro_flush_cycles = cycles;
}

void
show_gro(portid_t port_id)
{
	struct rte_gro_param *param;
	uint32_t max_pkts_num;

	param = &gro_ports[port_id].param;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		fprintf(stderr, "Invalid port id %u.\n", port_id);
		return;
	}
	if (gro_ports[port_id].enable) {
		printf("GRO type: TCP/IPv4\n");
		if (gro_flush_cycles == GRO_DEFAULT_FLUSH_CYCLES) {
			max_pkts_num = param->max_flow_num *
				param->max_item_per_flow;
		} else
			max_pkts_num = MAX_PKT_BURST * GRO_MAX_FLUSH_CYCLES;
		printf("Max number of packets to perform GRO: %u\n",
				max_pkts_num);
		printf("Flushing cycles: %u\n", gro_flush_cycles);
	} else
		printf("Port %u doesn't enable GRO.\n", port_id);
}
#endif /* RTE_LIB_GRO */

#ifdef RTE_LIB_GSO
void
setup_gso(const char *mode, portid_t port_id)
{
	if (!rte_eth_dev_is_valid_port(port_id)) {
		fprintf(stderr, "invalid port id %u\n", port_id);
		return;
	}
	if (strcmp(mode, "on") == 0) {
		if (test_done == 0) {
			fprintf(stderr,
				"before enabling GSO, please stop forwarding first\n");
			return;
		}
		gso_ports[port_id].enable = 1;
	} else if (strcmp(mode, "off") == 0) {
		if (test_done == 0) {
			fprintf(stderr,
				"before disabling GSO, please stop forwarding first\n");
			return;
		}
		gso_ports[port_id].enable = 0;
	}
}
#endif /* RTE_LIB_GSO */

char*
list_pkt_forwarding_modes(void)
{
	static char fwd_modes[128] = "";
	const char *separator = "|";
	struct fwd_engine *fwd_eng;
	unsigned i = 0;

	if (strlen (fwd_modes) == 0) {
		while ((fwd_eng = fwd_engines[i++]) != NULL) {
			strncat(fwd_modes, fwd_eng->fwd_mode_name,
					sizeof(fwd_modes) - strlen(fwd_modes) - 1);
			strncat(fwd_modes, separator,
					sizeof(fwd_modes) - strlen(fwd_modes) - 1);
		}
		fwd_modes[strlen(fwd_modes) - strlen(separator)] = '\0';
	}

	return fwd_modes;
}

char*
list_pkt_forwarding_retry_modes(void)
{
	static char fwd_modes[128] = "";
	const char *separator = "|";
	struct fwd_engine *fwd_eng;
	unsigned i = 0;

	if (strlen(fwd_modes) == 0) {
		while ((fwd_eng = fwd_engines[i++]) != NULL) {
			if (fwd_eng == &rx_only_engine)
				continue;
			strncat(fwd_modes, fwd_eng->fwd_mode_name,
					sizeof(fwd_modes) -
					strlen(fwd_modes) - 1);
			strncat(fwd_modes, separator,
					sizeof(fwd_modes) -
					strlen(fwd_modes) - 1);
		}
		fwd_modes[strlen(fwd_modes) - strlen(separator)] = '\0';
	}

	return fwd_modes;
}

void
set_pkt_forwarding_mode(const char *fwd_mode_name)
{
	struct fwd_engine *fwd_eng;
	unsigned i;

	i = 0;
	while ((fwd_eng = fwd_engines[i]) != NULL) {
		if (! strcmp(fwd_eng->fwd_mode_name, fwd_mode_name)) {
			printf("Set %s packet forwarding mode%s\n",
			       fwd_mode_name,
			       retry_enabled == 0 ? "" : " with retry");
			cur_fwd_eng = fwd_eng;
			return;
		}
		i++;
	}
	fprintf(stderr, "Invalid %s packet forwarding mode\n", fwd_mode_name);
}

void
add_rx_dump_callbacks(portid_t portid)
{
	struct rte_eth_dev_info dev_info;
	uint16_t queue;
	int ret;

	if (port_id_is_invalid(portid, ENABLED_WARN))
		return;

	ret = eth_dev_info_get_print_err(portid, &dev_info);
	if (ret != 0)
		return;

	for (queue = 0; queue < dev_info.nb_rx_queues; queue++)
		if (!ports[portid].rx_dump_cb[queue])
			ports[portid].rx_dump_cb[queue] =
				rte_eth_add_rx_callback(portid, queue,
					dump_rx_pkts, NULL);
}

void
add_tx_dump_callbacks(portid_t portid)
{
	struct rte_eth_dev_info dev_info;
	uint16_t queue;
	int ret;

	if (port_id_is_invalid(portid, ENABLED_WARN))
		return;

	ret = eth_dev_info_get_print_err(portid, &dev_info);
	if (ret != 0)
		return;

	for (queue = 0; queue < dev_info.nb_tx_queues; queue++)
		if (!ports[portid].tx_dump_cb[queue])
			ports[portid].tx_dump_cb[queue] =
				rte_eth_add_tx_callback(portid, queue,
							dump_tx_pkts, NULL);
}

void
remove_rx_dump_callbacks(portid_t portid)
{
	struct rte_eth_dev_info dev_info;
	uint16_t queue;
	int ret;

	if (port_id_is_invalid(portid, ENABLED_WARN))
		return;

	ret = eth_dev_info_get_print_err(portid, &dev_info);
	if (ret != 0)
		return;

	for (queue = 0; queue < dev_info.nb_rx_queues; queue++)
		if (ports[portid].rx_dump_cb[queue]) {
			rte_eth_remove_rx_callback(portid, queue,
				ports[portid].rx_dump_cb[queue]);
			ports[portid].rx_dump_cb[queue] = NULL;
		}
}

void
remove_tx_dump_callbacks(portid_t portid)
{
	struct rte_eth_dev_info dev_info;
	uint16_t queue;
	int ret;

	if (port_id_is_invalid(portid, ENABLED_WARN))
		return;

	ret = eth_dev_info_get_print_err(portid, &dev_info);
	if (ret != 0)
		return;

	for (queue = 0; queue < dev_info.nb_tx_queues; queue++)
		if (ports[portid].tx_dump_cb[queue]) {
			rte_eth_remove_tx_callback(portid, queue,
				ports[portid].tx_dump_cb[queue]);
			ports[portid].tx_dump_cb[queue] = NULL;
		}
}

void
configure_rxtx_dump_callbacks(uint16_t verbose)
{
	portid_t portid;

#ifndef RTE_ETHDEV_RXTX_CALLBACKS
		TESTPMD_LOG(ERR, "setting rxtx callbacks is not enabled\n");
		return;
#endif

	RTE_ETH_FOREACH_DEV(portid)
	{
		if (verbose == 1 || verbose > 2)
			add_rx_dump_callbacks(portid);
		else
			remove_rx_dump_callbacks(portid);
		if (verbose >= 2)
			add_tx_dump_callbacks(portid);
		else
			remove_tx_dump_callbacks(portid);
	}
}

void
set_verbose_level(uint16_t vb_level)
{
	printf("Change verbose level from %u to %u\n",
	       (unsigned int) verbose_level, (unsigned int) vb_level);
	verbose_level = vb_level;
	configure_rxtx_dump_callbacks(verbose_level);
}

void
vlan_extend_set(portid_t port_id, int on)
{
	int diag;
	int vlan_offload;
	uint64_t port_rx_offloads = ports[port_id].dev_conf.rxmode.offloads;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);

	if (on) {
		vlan_offload |= RTE_ETH_VLAN_EXTEND_OFFLOAD;
		port_rx_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_EXTEND;
	} else {
		vlan_offload &= ~RTE_ETH_VLAN_EXTEND_OFFLOAD;
		port_rx_offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_EXTEND;
	}

	diag = rte_eth_dev_set_vlan_offload(port_id, vlan_offload);
	if (diag < 0) {
		fprintf(stderr,
			"rx_vlan_extend_set(port_pi=%d, on=%d) failed diag=%d\n",
			port_id, on, diag);
		return;
	}
	ports[port_id].dev_conf.rxmode.offloads = port_rx_offloads;
}

void
rx_vlan_strip_set(portid_t port_id, int on)
{
	int diag;
	int vlan_offload;
	uint64_t port_rx_offloads = ports[port_id].dev_conf.rxmode.offloads;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);

	if (on) {
		vlan_offload |= RTE_ETH_VLAN_STRIP_OFFLOAD;
		port_rx_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	} else {
		vlan_offload &= ~RTE_ETH_VLAN_STRIP_OFFLOAD;
		port_rx_offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	}

	diag = rte_eth_dev_set_vlan_offload(port_id, vlan_offload);
	if (diag < 0) {
		fprintf(stderr,
			"%s(port_pi=%d, on=%d) failed diag=%d\n",
			__func__, port_id, on, diag);
		return;
	}
	ports[port_id].dev_conf.rxmode.offloads = port_rx_offloads;
}

void
rx_vlan_strip_set_on_queue(portid_t port_id, uint16_t queue_id, int on)
{
	int diag;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	diag = rte_eth_dev_set_vlan_strip_on_queue(port_id, queue_id, on);
	if (diag < 0)
		fprintf(stderr,
			"%s(port_pi=%d, queue_id=%d, on=%d) failed diag=%d\n",
			__func__, port_id, queue_id, on, diag);
}

void
rx_vlan_filter_set(portid_t port_id, int on)
{
	int diag;
	int vlan_offload;
	uint64_t port_rx_offloads = ports[port_id].dev_conf.rxmode.offloads;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);

	if (on) {
		vlan_offload |= RTE_ETH_VLAN_FILTER_OFFLOAD;
		port_rx_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_FILTER;
	} else {
		vlan_offload &= ~RTE_ETH_VLAN_FILTER_OFFLOAD;
		port_rx_offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_FILTER;
	}

	diag = rte_eth_dev_set_vlan_offload(port_id, vlan_offload);
	if (diag < 0) {
		fprintf(stderr,
			"%s(port_pi=%d, on=%d) failed diag=%d\n",
			__func__, port_id, on, diag);
		return;
	}
	ports[port_id].dev_conf.rxmode.offloads = port_rx_offloads;
}

void
rx_vlan_qinq_strip_set(portid_t port_id, int on)
{
	int diag;
	int vlan_offload;
	uint64_t port_rx_offloads = ports[port_id].dev_conf.rxmode.offloads;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);

	if (on) {
		vlan_offload |= RTE_ETH_QINQ_STRIP_OFFLOAD;
		port_rx_offloads |= RTE_ETH_RX_OFFLOAD_QINQ_STRIP;
	} else {
		vlan_offload &= ~RTE_ETH_QINQ_STRIP_OFFLOAD;
		port_rx_offloads &= ~RTE_ETH_RX_OFFLOAD_QINQ_STRIP;
	}

	diag = rte_eth_dev_set_vlan_offload(port_id, vlan_offload);
	if (diag < 0) {
		fprintf(stderr, "%s(port_pi=%d, on=%d) failed diag=%d\n",
			__func__, port_id, on, diag);
		return;
	}
	ports[port_id].dev_conf.rxmode.offloads = port_rx_offloads;
}

int
rx_vft_set(portid_t port_id, uint16_t vlan_id, int on)
{
	int diag;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return 1;
	if (vlan_id_is_invalid(vlan_id))
		return 1;
	diag = rte_eth_dev_vlan_filter(port_id, vlan_id, on);
	if (diag == 0)
		return 0;
	fprintf(stderr,
		"rte_eth_dev_vlan_filter(port_pi=%d, vlan_id=%d, on=%d) failed diag=%d\n",
		port_id, vlan_id, on, diag);
	return -1;
}

void
rx_vlan_all_filter_set(portid_t port_id, int on)
{
	uint16_t vlan_id;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	for (vlan_id = 0; vlan_id < 4096; vlan_id++) {
		if (rx_vft_set(port_id, vlan_id, on))
			break;
	}
}

void
vlan_tpid_set(portid_t port_id, enum rte_vlan_type vlan_type, uint16_t tp_id)
{
	int diag;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	diag = rte_eth_dev_set_vlan_ether_type(port_id, vlan_type, tp_id);
	if (diag == 0)
		return;

	fprintf(stderr,
		"tx_vlan_tpid_set(port_pi=%d, vlan_type=%d, tpid=%d) failed diag=%d\n",
		port_id, vlan_type, tp_id, diag);
}

void
tx_vlan_set(portid_t port_id, uint16_t vlan_id)
{
	struct rte_eth_dev_info dev_info;
	int ret;

	if (vlan_id_is_invalid(vlan_id))
		return;

	if (ports[port_id].dev_conf.txmode.offloads &
	    RTE_ETH_TX_OFFLOAD_QINQ_INSERT) {
		fprintf(stderr, "Error, as QinQ has been enabled.\n");
		return;
	}

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	if ((dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_VLAN_INSERT) == 0) {
		fprintf(stderr,
			"Error: vlan insert is not supported by port %d\n",
			port_id);
		return;
	}

	tx_vlan_reset(port_id);
	ports[port_id].dev_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_VLAN_INSERT;
	ports[port_id].tx_vlan_id = vlan_id;
}

void
tx_qinq_set(portid_t port_id, uint16_t vlan_id, uint16_t vlan_id_outer)
{
	struct rte_eth_dev_info dev_info;
	int ret;

	if (vlan_id_is_invalid(vlan_id))
		return;
	if (vlan_id_is_invalid(vlan_id_outer))
		return;

	ret = eth_dev_info_get_print_err(port_id, &dev_info);
	if (ret != 0)
		return;

	if ((dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_QINQ_INSERT) == 0) {
		fprintf(stderr,
			"Error: qinq insert not supported by port %d\n",
			port_id);
		return;
	}

	tx_vlan_reset(port_id);
	ports[port_id].dev_conf.txmode.offloads |= (RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
						    RTE_ETH_TX_OFFLOAD_QINQ_INSERT);
	ports[port_id].tx_vlan_id = vlan_id;
	ports[port_id].tx_vlan_id_outer = vlan_id_outer;
}

void
tx_vlan_reset(portid_t port_id)
{
	ports[port_id].dev_conf.txmode.offloads &=
				~(RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
				  RTE_ETH_TX_OFFLOAD_QINQ_INSERT);
	ports[port_id].tx_vlan_id = 0;
	ports[port_id].tx_vlan_id_outer = 0;
}

void
tx_vlan_pvid_set(portid_t port_id, uint16_t vlan_id, int on)
{
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	rte_eth_dev_set_vlan_pvid(port_id, vlan_id, on);
}

void
set_qmap(portid_t port_id, uint8_t is_rx, uint16_t queue_id, uint8_t map_value)
{
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	if (is_rx ? (rx_queue_id_is_invalid(queue_id)) : (tx_queue_id_is_invalid(queue_id)))
		return;

	if (map_value >= RTE_ETHDEV_QUEUE_STAT_CNTRS) {
		fprintf(stderr, "map_value not in required range 0..%d\n",
			RTE_ETHDEV_QUEUE_STAT_CNTRS - 1);
		return;
	}

	if (!is_rx) { /* tx */
		ret = rte_eth_dev_set_tx_queue_stats_mapping(port_id, queue_id,
							     map_value);
		if (ret) {
			fprintf(stderr,
				"failed to set tx queue stats mapping.\n");
			return;
		}
	} else { /* rx */
		ret = rte_eth_dev_set_rx_queue_stats_mapping(port_id, queue_id,
							     map_value);
		if (ret) {
			fprintf(stderr,
				"failed to set rx queue stats mapping.\n");
			return;
		}
	}
}

void
set_xstats_hide_zero(uint8_t on_off)
{
	xstats_hide_zero = on_off;
}

void
set_record_core_cycles(uint8_t on_off)
{
	record_core_cycles = on_off;
}

void
set_record_burst_stats(uint8_t on_off)
{
	record_burst_stats = on_off;
}

#if defined(RTE_NET_I40E) || defined(RTE_NET_IXGBE)

static char*
flowtype_to_str(uint16_t flow_type)
{
	struct flow_type_info {
		char str[32];
		uint16_t ftype;
	};

	uint8_t i;
	static struct flow_type_info flowtype_str_table[] = {
		{"raw", RTE_ETH_FLOW_RAW},
		{"ipv4", RTE_ETH_FLOW_IPV4},
		{"ipv4-frag", RTE_ETH_FLOW_FRAG_IPV4},
		{"ipv4-tcp", RTE_ETH_FLOW_NONFRAG_IPV4_TCP},
		{"ipv4-udp", RTE_ETH_FLOW_NONFRAG_IPV4_UDP},
		{"ipv4-sctp", RTE_ETH_FLOW_NONFRAG_IPV4_SCTP},
		{"ipv4-other", RTE_ETH_FLOW_NONFRAG_IPV4_OTHER},
		{"ipv6", RTE_ETH_FLOW_IPV6},
		{"ipv6-frag", RTE_ETH_FLOW_FRAG_IPV6},
		{"ipv6-tcp", RTE_ETH_FLOW_NONFRAG_IPV6_TCP},
		{"ipv6-udp", RTE_ETH_FLOW_NONFRAG_IPV6_UDP},
		{"ipv6-sctp", RTE_ETH_FLOW_NONFRAG_IPV6_SCTP},
		{"ipv6-other", RTE_ETH_FLOW_NONFRAG_IPV6_OTHER},
		{"l2_payload", RTE_ETH_FLOW_L2_PAYLOAD},
		{"ipv6-ex", RTE_ETH_FLOW_IPV6_EX},
		{"ipv6-tcp-ex", RTE_ETH_FLOW_IPV6_TCP_EX},
		{"ipv6-udp-ex", RTE_ETH_FLOW_IPV6_UDP_EX},
		{"port", RTE_ETH_FLOW_PORT},
		{"vxlan", RTE_ETH_FLOW_VXLAN},
		{"geneve", RTE_ETH_FLOW_GENEVE},
		{"nvgre", RTE_ETH_FLOW_NVGRE},
		{"vxlan-gpe", RTE_ETH_FLOW_VXLAN_GPE},
		{"gtpu", RTE_ETH_FLOW_GTPU},
	};

	for (i = 0; i < RTE_DIM(flowtype_str_table); i++) {
		if (flowtype_str_table[i].ftype == flow_type)
			return flowtype_str_table[i].str;
	}

	return NULL;
}

static inline void
print_fdir_mask(struct rte_eth_fdir_masks *mask)
{
	printf("\n    vlan_tci: 0x%04x", rte_be_to_cpu_16(mask->vlan_tci_mask));

	if (fdir_conf.mode == RTE_FDIR_MODE_PERFECT_TUNNEL)
		printf(", mac_addr: 0x%02x, tunnel_type: 0x%01x,"
			" tunnel_id: 0x%08x",
			mask->mac_addr_byte_mask, mask->tunnel_type_mask,
			rte_be_to_cpu_32(mask->tunnel_id_mask));
	else if (fdir_conf.mode != RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		printf(", src_ipv4: 0x%08x, dst_ipv4: 0x%08x",
			rte_be_to_cpu_32(mask->ipv4_mask.src_ip),
			rte_be_to_cpu_32(mask->ipv4_mask.dst_ip));

		printf("\n    src_port: 0x%04x, dst_port: 0x%04x",
			rte_be_to_cpu_16(mask->src_port_mask),
			rte_be_to_cpu_16(mask->dst_port_mask));

		printf("\n    src_ipv6: 0x%08x,0x%08x,0x%08x,0x%08x",
			rte_be_to_cpu_32(mask->ipv6_mask.src_ip[0]),
			rte_be_to_cpu_32(mask->ipv6_mask.src_ip[1]),
			rte_be_to_cpu_32(mask->ipv6_mask.src_ip[2]),
			rte_be_to_cpu_32(mask->ipv6_mask.src_ip[3]));

		printf("\n    dst_ipv6: 0x%08x,0x%08x,0x%08x,0x%08x",
			rte_be_to_cpu_32(mask->ipv6_mask.dst_ip[0]),
			rte_be_to_cpu_32(mask->ipv6_mask.dst_ip[1]),
			rte_be_to_cpu_32(mask->ipv6_mask.dst_ip[2]),
			rte_be_to_cpu_32(mask->ipv6_mask.dst_ip[3]));
	}

	printf("\n");
}

static inline void
print_fdir_flex_payload(struct rte_eth_fdir_flex_conf *flex_conf, uint32_t num)
{
	struct rte_eth_flex_payload_cfg *cfg;
	uint32_t i, j;

	for (i = 0; i < flex_conf->nb_payloads; i++) {
		cfg = &flex_conf->flex_set[i];
		if (cfg->type == RTE_ETH_RAW_PAYLOAD)
			printf("\n    RAW:  ");
		else if (cfg->type == RTE_ETH_L2_PAYLOAD)
			printf("\n    L2_PAYLOAD:  ");
		else if (cfg->type == RTE_ETH_L3_PAYLOAD)
			printf("\n    L3_PAYLOAD:  ");
		else if (cfg->type == RTE_ETH_L4_PAYLOAD)
			printf("\n    L4_PAYLOAD:  ");
		else
			printf("\n    UNKNOWN PAYLOAD(%u):  ", cfg->type);
		for (j = 0; j < num; j++)
			printf("  %-5u", cfg->src_offset[j]);
	}
	printf("\n");
}

static inline void
print_fdir_flex_mask(struct rte_eth_fdir_flex_conf *flex_conf, uint32_t num)
{
	struct rte_eth_fdir_flex_mask *mask;
	uint32_t i, j;
	char *p;

	for (i = 0; i < flex_conf->nb_flexmasks; i++) {
		mask = &flex_conf->flex_mask[i];
		p = flowtype_to_str(mask->flow_type);
		printf("\n    %s:\t", p ? p : "unknown");
		for (j = 0; j < num; j++)
			printf(" %02x", mask->mask[j]);
	}
	printf("\n");
}

static inline void
print_fdir_flow_type(uint32_t flow_types_mask)
{
	int i;
	char *p;

	for (i = RTE_ETH_FLOW_UNKNOWN; i < RTE_ETH_FLOW_MAX; i++) {
		if (!(flow_types_mask & (1 << i)))
			continue;
		p = flowtype_to_str(i);
		if (p)
			printf(" %s", p);
		else
			printf(" unknown");
	}
	printf("\n");
}

static int
get_fdir_info(portid_t port_id, struct rte_eth_fdir_info *fdir_info,
		    struct rte_eth_fdir_stats *fdir_stat)
{
	int ret = -ENOTSUP;

#ifdef RTE_NET_I40E
	if (ret == -ENOTSUP) {
		ret = rte_pmd_i40e_get_fdir_info(port_id, fdir_info);
		if (!ret)
			ret = rte_pmd_i40e_get_fdir_stats(port_id, fdir_stat);
	}
#endif
#ifdef RTE_NET_IXGBE
	if (ret == -ENOTSUP) {
		ret = rte_pmd_ixgbe_get_fdir_info(port_id, fdir_info);
		if (!ret)
			ret = rte_pmd_ixgbe_get_fdir_stats(port_id, fdir_stat);
	}
#endif
	switch (ret) {
	case 0:
		break;
	case -ENOTSUP:
		fprintf(stderr, "\n FDIR is not supported on port %-2d\n",
			port_id);
		break;
	default:
		fprintf(stderr, "programming error: (%s)\n", strerror(-ret));
		break;
	}
	return ret;
}

void
fdir_get_infos(portid_t port_id)
{
	struct rte_eth_fdir_stats fdir_stat;
	struct rte_eth_fdir_info fdir_info;

	static const char *fdir_stats_border = "########################";

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&fdir_info, 0, sizeof(fdir_info));
	memset(&fdir_stat, 0, sizeof(fdir_stat));
	if (get_fdir_info(port_id, &fdir_info, &fdir_stat))
		return;

	printf("\n  %s FDIR infos for port %-2d     %s\n",
	       fdir_stats_border, port_id, fdir_stats_border);
	printf("  MODE: ");
	if (fdir_info.mode == RTE_FDIR_MODE_PERFECT)
		printf("  PERFECT\n");
	else if (fdir_info.mode == RTE_FDIR_MODE_PERFECT_MAC_VLAN)
		printf("  PERFECT-MAC-VLAN\n");
	else if (fdir_info.mode == RTE_FDIR_MODE_PERFECT_TUNNEL)
		printf("  PERFECT-TUNNEL\n");
	else if (fdir_info.mode == RTE_FDIR_MODE_SIGNATURE)
		printf("  SIGNATURE\n");
	else
		printf("  DISABLE\n");
	if (fdir_info.mode != RTE_FDIR_MODE_PERFECT_MAC_VLAN
		&& fdir_info.mode != RTE_FDIR_MODE_PERFECT_TUNNEL) {
		printf("  SUPPORTED FLOW TYPE: ");
		print_fdir_flow_type(fdir_info.flow_types_mask[0]);
	}
	printf("  FLEX PAYLOAD INFO:\n");
	printf("  max_len:       %-10"PRIu32"  payload_limit: %-10"PRIu32"\n"
	       "  payload_unit:  %-10"PRIu32"  payload_seg:   %-10"PRIu32"\n"
	       "  bitmask_unit:  %-10"PRIu32"  bitmask_num:   %-10"PRIu32"\n",
		fdir_info.max_flexpayload, fdir_info.flex_payload_limit,
		fdir_info.flex_payload_unit,
		fdir_info.max_flex_payload_segment_num,
		fdir_info.flex_bitmask_unit, fdir_info.max_flex_bitmask_num);
	printf("  MASK: ");
	print_fdir_mask(&fdir_info.mask);
	if (fdir_info.flex_conf.nb_payloads > 0) {
		printf("  FLEX PAYLOAD SRC OFFSET:");
		print_fdir_flex_payload(&fdir_info.flex_conf, fdir_info.max_flexpayload);
	}
	if (fdir_info.flex_conf.nb_flexmasks > 0) {
		printf("  FLEX MASK CFG:");
		print_fdir_flex_mask(&fdir_info.flex_conf, fdir_info.max_flexpayload);
	}
	printf("  guarant_count: %-10"PRIu32"  best_count:    %"PRIu32"\n",
	       fdir_stat.guarant_cnt, fdir_stat.best_cnt);
	printf("  guarant_space: %-10"PRIu32"  best_space:    %"PRIu32"\n",
	       fdir_info.guarant_spc, fdir_info.best_spc);
	printf("  collision:     %-10"PRIu32"  free:          %"PRIu32"\n"
	       "  maxhash:       %-10"PRIu32"  maxlen:        %"PRIu32"\n"
	       "  add:	         %-10"PRIu64"  remove:        %"PRIu64"\n"
	       "  f_add:         %-10"PRIu64"  f_remove:      %"PRIu64"\n",
	       fdir_stat.collision, fdir_stat.free,
	       fdir_stat.maxhash, fdir_stat.maxlen,
	       fdir_stat.add, fdir_stat.remove,
	       fdir_stat.f_add, fdir_stat.f_remove);
	printf("  %s############################%s\n",
	       fdir_stats_border, fdir_stats_border);
}

#endif /* RTE_NET_I40E || RTE_NET_IXGBE */

void
fdir_set_flex_mask(portid_t port_id, struct rte_eth_fdir_flex_mask *cfg)
{
	struct rte_port *port;
	struct rte_eth_fdir_flex_conf *flex_conf;
	int i, idx = 0;

	port = &ports[port_id];
	flex_conf = &port->dev_conf.fdir_conf.flex_conf;
	for (i = 0; i < RTE_ETH_FLOW_MAX; i++) {
		if (cfg->flow_type == flex_conf->flex_mask[i].flow_type) {
			idx = i;
			break;
		}
	}
	if (i >= RTE_ETH_FLOW_MAX) {
		if (flex_conf->nb_flexmasks < RTE_DIM(flex_conf->flex_mask)) {
			idx = flex_conf->nb_flexmasks;
			flex_conf->nb_flexmasks++;
		} else {
			fprintf(stderr,
				"The flex mask table is full. Can not set flex mask for flow_type(%u).",
				cfg->flow_type);
			return;
		}
	}
	rte_memcpy(&flex_conf->flex_mask[idx],
			 cfg,
			 sizeof(struct rte_eth_fdir_flex_mask));
}

void
fdir_set_flex_payload(portid_t port_id, struct rte_eth_flex_payload_cfg *cfg)
{
	struct rte_port *port;
	struct rte_eth_fdir_flex_conf *flex_conf;
	int i, idx = 0;

	port = &ports[port_id];
	flex_conf = &port->dev_conf.fdir_conf.flex_conf;
	for (i = 0; i < RTE_ETH_PAYLOAD_MAX; i++) {
		if (cfg->type == flex_conf->flex_set[i].type) {
			idx = i;
			break;
		}
	}
	if (i >= RTE_ETH_PAYLOAD_MAX) {
		if (flex_conf->nb_payloads < RTE_DIM(flex_conf->flex_set)) {
			idx = flex_conf->nb_payloads;
			flex_conf->nb_payloads++;
		} else {
			fprintf(stderr,
				"The flex payload table is full. Can not set flex payload for type(%u).",
				cfg->type);
			return;
		}
	}
	rte_memcpy(&flex_conf->flex_set[idx],
			 cfg,
			 sizeof(struct rte_eth_flex_payload_cfg));

}

void
set_vf_traffic(portid_t port_id, uint8_t is_rx, uint16_t vf, uint8_t on)
{
#ifdef RTE_NET_IXGBE
	int diag;

	if (is_rx)
		diag = rte_pmd_ixgbe_set_vf_rx(port_id, vf, on);
	else
		diag = rte_pmd_ixgbe_set_vf_tx(port_id, vf, on);

	if (diag == 0)
		return;
	fprintf(stderr,
		"rte_pmd_ixgbe_set_vf_%s for port_id=%d failed diag=%d\n",
		is_rx ? "rx" : "tx", port_id, diag);
	return;
#endif
	fprintf(stderr, "VF %s setting not supported for port %d\n",
		is_rx ? "Rx" : "Tx", port_id);
	RTE_SET_USED(vf);
	RTE_SET_USED(on);
}

int
set_queue_rate_limit(portid_t port_id, uint16_t queue_idx, uint16_t rate)
{
	int diag;
	struct rte_eth_link link;
	int ret;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return 1;
	ret = eth_link_get_nowait_print_err(port_id, &link);
	if (ret < 0)
		return 1;
	if (link.link_speed != RTE_ETH_SPEED_NUM_UNKNOWN &&
	    rate > link.link_speed) {
		fprintf(stderr,
			"Invalid rate value:%u bigger than link speed: %u\n",
			rate, link.link_speed);
		return 1;
	}
	diag = rte_eth_set_queue_rate_limit(port_id, queue_idx, rate);
	if (diag == 0)
		return diag;
	fprintf(stderr,
		"rte_eth_set_queue_rate_limit for port_id=%d failed diag=%d\n",
		port_id, diag);
	return diag;
}

int
set_vf_rate_limit(portid_t port_id, uint16_t vf, uint16_t rate, uint64_t q_msk)
{
	int diag = -ENOTSUP;

	RTE_SET_USED(vf);
	RTE_SET_USED(rate);
	RTE_SET_USED(q_msk);

#ifdef RTE_NET_IXGBE
	if (diag == -ENOTSUP)
		diag = rte_pmd_ixgbe_set_vf_rate_limit(port_id, vf, rate,
						       q_msk);
#endif
#ifdef RTE_NET_BNXT
	if (diag == -ENOTSUP)
		diag = rte_pmd_bnxt_set_vf_rate_limit(port_id, vf, rate, q_msk);
#endif
	if (diag == 0)
		return diag;

	fprintf(stderr,
		"%s for port_id=%d failed diag=%d\n",
		__func__, port_id, diag);
	return diag;
}

/*
 * Functions to manage the set of filtered Multicast MAC addresses.
 *
 * A pool of filtered multicast MAC addresses is associated with each port.
 * The pool is allocated in chunks of MCAST_POOL_INC multicast addresses.
 * The address of the pool and the number of valid multicast MAC addresses
 * recorded in the pool are stored in the fields "mc_addr_pool" and
 * "mc_addr_nb" of the "rte_port" data structure.
 *
 * The function "rte_eth_dev_set_mc_addr_list" of the PMDs API imposes
 * to be supplied a contiguous array of multicast MAC addresses.
 * To comply with this constraint, the set of multicast addresses recorded
 * into the pool are systematically compacted at the beginning of the pool.
 * Hence, when a multicast address is removed from the pool, all following
 * addresses, if any, are copied back to keep the set contiguous.
 */
#define MCAST_POOL_INC 32

static int
mcast_addr_pool_extend(struct rte_port *port)
{
	struct rte_ether_addr *mc_pool;
	size_t mc_pool_size;

	/*
	 * If a free entry is available at the end of the pool, just
	 * increment the number of recorded multicast addresses.
	 */
	if ((port->mc_addr_nb % MCAST_POOL_INC) != 0) {
		port->mc_addr_nb++;
		return 0;
	}

	/*
	 * [re]allocate a pool with MCAST_POOL_INC more entries.
	 * The previous test guarantees that port->mc_addr_nb is a multiple
	 * of MCAST_POOL_INC.
	 */
	mc_pool_size = sizeof(struct rte_ether_addr) * (port->mc_addr_nb +
						    MCAST_POOL_INC);
	mc_pool = (struct rte_ether_addr *) realloc(port->mc_addr_pool,
						mc_pool_size);
	if (mc_pool == NULL) {
		fprintf(stderr,
			"allocation of pool of %u multicast addresses failed\n",
			port->mc_addr_nb + MCAST_POOL_INC);
		return -ENOMEM;
	}

	port->mc_addr_pool = mc_pool;
	port->mc_addr_nb++;
	return 0;

}

static void
mcast_addr_pool_append(struct rte_port *port, struct rte_ether_addr *mc_addr)
{
	if (mcast_addr_pool_extend(port) != 0)
		return;
	rte_ether_addr_copy(mc_addr, &port->mc_addr_pool[port->mc_addr_nb - 1]);
}

static void
mcast_addr_pool_remove(struct rte_port *port, uint32_t addr_idx)
{
	port->mc_addr_nb--;
	if (addr_idx == port->mc_addr_nb) {
		/* No need to recompact the set of multicast addresses. */
		if (port->mc_addr_nb == 0) {
			/* free the pool of multicast addresses. */
			free(port->mc_addr_pool);
			port->mc_addr_pool = NULL;
		}
		return;
	}
	memmove(&port->mc_addr_pool[addr_idx],
		&port->mc_addr_pool[addr_idx + 1],
		sizeof(struct rte_ether_addr) * (port->mc_addr_nb - addr_idx));
}

int
mcast_addr_pool_destroy(portid_t port_id)
{
	struct rte_port *port;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return -EINVAL;
	port = &ports[port_id];

	if (port->mc_addr_nb != 0) {
		/* free the pool of multicast addresses. */
		free(port->mc_addr_pool);
		port->mc_addr_pool = NULL;
		port->mc_addr_nb = 0;
	}
	return 0;
}

static int
eth_port_multicast_addr_list_set(portid_t port_id)
{
	struct rte_port *port;
	int diag;

	port = &ports[port_id];
	diag = rte_eth_dev_set_mc_addr_list(port_id, port->mc_addr_pool,
					    port->mc_addr_nb);
	if (diag < 0)
		fprintf(stderr,
			"rte_eth_dev_set_mc_addr_list(port=%d, nb=%u) failed. diag=%d\n",
			port_id, port->mc_addr_nb, diag);

	return diag;
}

void
mcast_addr_add(portid_t port_id, struct rte_ether_addr *mc_addr)
{
	struct rte_port *port;
	uint32_t i;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	port = &ports[port_id];

	/*
	 * Check that the added multicast MAC address is not already recorded
	 * in the pool of multicast addresses.
	 */
	for (i = 0; i < port->mc_addr_nb; i++) {
		if (rte_is_same_ether_addr(mc_addr, &port->mc_addr_pool[i])) {
			fprintf(stderr,
				"multicast address already filtered by port\n");
			return;
		}
	}

	mcast_addr_pool_append(port, mc_addr);
	if (eth_port_multicast_addr_list_set(port_id) < 0)
		/* Rollback on failure, remove the address from the pool */
		mcast_addr_pool_remove(port, i);
}

void
mcast_addr_remove(portid_t port_id, struct rte_ether_addr *mc_addr)
{
	struct rte_port *port;
	uint32_t i;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	port = &ports[port_id];

	/*
	 * Search the pool of multicast MAC addresses for the removed address.
	 */
	for (i = 0; i < port->mc_addr_nb; i++) {
		if (rte_is_same_ether_addr(mc_addr, &port->mc_addr_pool[i]))
			break;
	}
	if (i == port->mc_addr_nb) {
		fprintf(stderr, "multicast address not filtered by port %d\n",
			port_id);
		return;
	}

	mcast_addr_pool_remove(port, i);
	if (eth_port_multicast_addr_list_set(port_id) < 0)
		/* Rollback on failure, add the address back into the pool */
		mcast_addr_pool_append(port, mc_addr);
}

void
port_dcb_info_display(portid_t port_id)
{
	struct rte_eth_dcb_info dcb_info;
	uint16_t i;
	int ret;
	static const char *border = "================";

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	ret = rte_eth_dev_get_dcb_info(port_id, &dcb_info);
	if (ret) {
		fprintf(stderr, "\n Failed to get dcb infos on port %-2d\n",
			port_id);
		return;
	}
	printf("\n  %s DCB infos for port %-2d  %s\n", border, port_id, border);
	printf("  TC NUMBER: %d\n", dcb_info.nb_tcs);
	printf("\n  TC :        ");
	for (i = 0; i < dcb_info.nb_tcs; i++)
		printf("\t%4d", i);
	printf("\n  Priority :  ");
	for (i = 0; i < dcb_info.nb_tcs; i++)
		printf("\t%4d", dcb_info.prio_tc[i]);
	printf("\n  BW percent :");
	for (i = 0; i < dcb_info.nb_tcs; i++)
		printf("\t%4d%%", dcb_info.tc_bws[i]);
	printf("\n  RXQ base :  ");
	for (i = 0; i < dcb_info.nb_tcs; i++)
		printf("\t%4d", dcb_info.tc_queue.tc_rxq[0][i].base);
	printf("\n  RXQ number :");
	for (i = 0; i < dcb_info.nb_tcs; i++)
		printf("\t%4d", dcb_info.tc_queue.tc_rxq[0][i].nb_queue);
	printf("\n  TXQ base :  ");
	for (i = 0; i < dcb_info.nb_tcs; i++)
		printf("\t%4d", dcb_info.tc_queue.tc_txq[0][i].base);
	printf("\n  TXQ number :");
	for (i = 0; i < dcb_info.nb_tcs; i++)
		printf("\t%4d", dcb_info.tc_queue.tc_txq[0][i].nb_queue);
	printf("\n");
}

uint8_t *
open_file(const char *file_path, uint32_t *size)
{
	int fd = open(file_path, O_RDONLY);
	off_t pkg_size;
	uint8_t *buf = NULL;
	int ret = 0;
	struct stat st_buf;

	if (size)
		*size = 0;

	if (fd == -1) {
		fprintf(stderr, "%s: Failed to open %s\n", __func__, file_path);
		return buf;
	}

	if ((fstat(fd, &st_buf) != 0) || (!S_ISREG(st_buf.st_mode))) {
		close(fd);
		fprintf(stderr, "%s: File operations failed\n", __func__);
		return buf;
	}

	pkg_size = st_buf.st_size;
	if (pkg_size < 0) {
		close(fd);
		fprintf(stderr, "%s: File operations failed\n", __func__);
		return buf;
	}

	buf = (uint8_t *)malloc(pkg_size);
	if (!buf) {
		close(fd);
		fprintf(stderr, "%s: Failed to malloc memory\n", __func__);
		return buf;
	}

	ret = read(fd, buf, pkg_size);
	if (ret < 0) {
		close(fd);
		fprintf(stderr, "%s: File read operation failed\n", __func__);
		close_file(buf);
		return NULL;
	}

	if (size)
		*size = pkg_size;

	close(fd);

	return buf;
}

int
save_file(const char *file_path, uint8_t *buf, uint32_t size)
{
	FILE *fh = fopen(file_path, "wb");

	if (fh == NULL) {
		fprintf(stderr, "%s: Failed to open %s\n", __func__, file_path);
		return -1;
	}

	if (fwrite(buf, 1, size, fh) != size) {
		fclose(fh);
		fprintf(stderr, "%s: File write operation failed\n", __func__);
		return -1;
	}

	fclose(fh);

	return 0;
}

int
close_file(uint8_t *buf)
{
	if (buf) {
		free((void *)buf);
		return 0;
	}

	return -1;
}

void
port_queue_region_info_display(portid_t port_id, void *buf)
{
#ifdef RTE_NET_I40E
	uint16_t i, j;
	struct rte_pmd_i40e_queue_regions *info =
		(struct rte_pmd_i40e_queue_regions *)buf;
	static const char *queue_region_info_stats_border = "-------";

	if (!info->queue_region_number)
		printf("there is no region has been set before");

	printf("\n	%s All queue region info for port=%2d %s",
			queue_region_info_stats_border, port_id,
			queue_region_info_stats_border);
	printf("\n	queue_region_number: %-14u \n",
			info->queue_region_number);

	for (i = 0; i < info->queue_region_number; i++) {
		printf("\n	region_id: %-14u queue_number: %-14u "
			"queue_start_index: %-14u \n",
			info->region[i].region_id,
			info->region[i].queue_num,
			info->region[i].queue_start_index);

		printf("  user_priority_num is	%-14u :",
					info->region[i].user_priority_num);
		for (j = 0; j < info->region[i].user_priority_num; j++)
			printf(" %-14u ", info->region[i].user_priority[j]);

		printf("\n	flowtype_num is  %-14u :",
				info->region[i].flowtype_num);
		for (j = 0; j < info->region[i].flowtype_num; j++)
			printf(" %-14u ", info->region[i].hw_flowtype[j]);
	}
#else
	RTE_SET_USED(port_id);
	RTE_SET_USED(buf);
#endif

	printf("\n\n");
}

void
show_macs(portid_t port_id)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_eth_dev_info dev_info;
	int32_t i, rc, num_macs = 0;

	if (eth_dev_info_get_print_err(port_id, &dev_info))
		return;

	struct rte_ether_addr addr[dev_info.max_mac_addrs];
	rc = rte_eth_macaddrs_get(port_id, addr, dev_info.max_mac_addrs);
	if (rc < 0)
		return;

	for (i = 0; i < rc; i++) {

		/* skip zero address */
		if (rte_is_zero_ether_addr(&addr[i]))
			continue;

		num_macs++;
	}

	printf("Number of MAC address added: %d\n", num_macs);

	for (i = 0; i < rc; i++) {

		/* skip zero address */
		if (rte_is_zero_ether_addr(&addr[i]))
			continue;

		rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, &addr[i]);
		printf("  %s\n", buf);
	}
}

void
show_mcast_macs(portid_t port_id)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *addr;
	struct rte_port *port;
	uint32_t i;

	port = &ports[port_id];

	printf("Number of Multicast MAC address added: %d\n", port->mc_addr_nb);

	for (i = 0; i < port->mc_addr_nb; i++) {
		addr = &port->mc_addr_pool[i];

		rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, addr);
		printf("  %s\n", buf);
	}
}
