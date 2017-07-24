/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   Copyright 2013-2014 6WIND S.A.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <sys/queue.h>

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
#include <rte_atomic.h>
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
#include <rte_errno.h>
#ifdef RTE_LIBRTE_IXGBE_PMD
#include <rte_pmd_ixgbe.h>
#endif
#ifdef RTE_LIBRTE_BNXT_PMD
#include <rte_pmd_bnxt.h>
#endif
#include <rte_gro.h>

#include "testpmd.h"

static char *flowtype_to_str(uint16_t flow_type);

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

struct rss_type_info {
	char str[32];
	uint64_t rss_type;
};

static const struct rss_type_info rss_type_table[] = {
	{ "ipv4", ETH_RSS_IPV4 },
	{ "ipv4-frag", ETH_RSS_FRAG_IPV4 },
	{ "ipv4-tcp", ETH_RSS_NONFRAG_IPV4_TCP },
	{ "ipv4-udp", ETH_RSS_NONFRAG_IPV4_UDP },
	{ "ipv4-sctp", ETH_RSS_NONFRAG_IPV4_SCTP },
	{ "ipv4-other", ETH_RSS_NONFRAG_IPV4_OTHER },
	{ "ipv6", ETH_RSS_IPV6 },
	{ "ipv6-frag", ETH_RSS_FRAG_IPV6 },
	{ "ipv6-tcp", ETH_RSS_NONFRAG_IPV6_TCP },
	{ "ipv6-udp", ETH_RSS_NONFRAG_IPV6_UDP },
	{ "ipv6-sctp", ETH_RSS_NONFRAG_IPV6_SCTP },
	{ "ipv6-other", ETH_RSS_NONFRAG_IPV6_OTHER },
	{ "l2-payload", ETH_RSS_L2_PAYLOAD },
	{ "ipv6-ex", ETH_RSS_IPV6_EX },
	{ "ipv6-tcp-ex", ETH_RSS_IPV6_TCP_EX },
	{ "ipv6-udp-ex", ETH_RSS_IPV6_UDP_EX },
	{ "port", ETH_RSS_PORT },
	{ "vxlan", ETH_RSS_VXLAN },
	{ "geneve", ETH_RSS_GENEVE },
	{ "nvgre", ETH_RSS_NVGRE },

};

static void
print_ethaddr(const char *name, struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

void
nic_stats_display(portid_t port_id)
{
	static uint64_t prev_pkts_rx[RTE_MAX_ETHPORTS];
	static uint64_t prev_pkts_tx[RTE_MAX_ETHPORTS];
	static uint64_t prev_cycles[RTE_MAX_ETHPORTS];
	uint64_t diff_pkts_rx, diff_pkts_tx, diff_cycles;
	uint64_t mpps_rx, mpps_tx;
	struct rte_eth_stats stats;
	struct rte_port *port = &ports[port_id];
	uint8_t i;
	portid_t pid;

	static const char *nic_stats_border = "########################";

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		printf("Valid port range is [0");
		RTE_ETH_FOREACH_DEV(pid)
			printf(", %d", pid);
		printf("]\n");
		return;
	}
	rte_eth_stats_get(port_id, &stats);
	printf("\n  %s NIC statistics for port %-2d %s\n",
	       nic_stats_border, port_id, nic_stats_border);

	if ((!port->rx_queue_stats_mapping_enabled) && (!port->tx_queue_stats_mapping_enabled)) {
		printf("  RX-packets: %-10"PRIu64" RX-missed: %-10"PRIu64" RX-bytes:  "
		       "%-"PRIu64"\n",
		       stats.ipackets, stats.imissed, stats.ibytes);
		printf("  RX-errors: %-"PRIu64"\n", stats.ierrors);
		printf("  RX-nombuf:  %-10"PRIu64"\n",
		       stats.rx_nombuf);
		printf("  TX-packets: %-10"PRIu64" TX-errors: %-10"PRIu64" TX-bytes:  "
		       "%-"PRIu64"\n",
		       stats.opackets, stats.oerrors, stats.obytes);
	}
	else {
		printf("  RX-packets:              %10"PRIu64"    RX-errors: %10"PRIu64
		       "    RX-bytes: %10"PRIu64"\n",
		       stats.ipackets, stats.ierrors, stats.ibytes);
		printf("  RX-errors:  %10"PRIu64"\n", stats.ierrors);
		printf("  RX-nombuf:               %10"PRIu64"\n",
		       stats.rx_nombuf);
		printf("  TX-packets:              %10"PRIu64"    TX-errors: %10"PRIu64
		       "    TX-bytes: %10"PRIu64"\n",
		       stats.opackets, stats.oerrors, stats.obytes);
	}

	if (port->rx_queue_stats_mapping_enabled) {
		printf("\n");
		for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
			printf("  Stats reg %2d RX-packets: %10"PRIu64
			       "    RX-errors: %10"PRIu64
			       "    RX-bytes: %10"PRIu64"\n",
			       i, stats.q_ipackets[i], stats.q_errors[i], stats.q_ibytes[i]);
		}
	}
	if (port->tx_queue_stats_mapping_enabled) {
		printf("\n");
		for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
			printf("  Stats reg %2d TX-packets: %10"PRIu64
			       "                             TX-bytes: %10"PRIu64"\n",
			       i, stats.q_opackets[i], stats.q_obytes[i]);
		}
	}

	diff_cycles = prev_cycles[port_id];
	prev_cycles[port_id] = rte_rdtsc();
	if (diff_cycles > 0)
		diff_cycles = prev_cycles[port_id] - diff_cycles;

	diff_pkts_rx = stats.ipackets - prev_pkts_rx[port_id];
	diff_pkts_tx = stats.opackets - prev_pkts_tx[port_id];
	prev_pkts_rx[port_id] = stats.ipackets;
	prev_pkts_tx[port_id] = stats.opackets;
	mpps_rx = diff_cycles > 0 ?
		diff_pkts_rx * rte_get_tsc_hz() / diff_cycles : 0;
	mpps_tx = diff_cycles > 0 ?
		diff_pkts_tx * rte_get_tsc_hz() / diff_cycles : 0;
	printf("\n  Throughput (since last show)\n");
	printf("  Rx-pps: %12"PRIu64"\n  Tx-pps: %12"PRIu64"\n",
			mpps_rx, mpps_tx);

	printf("  %s############################%s\n",
	       nic_stats_border, nic_stats_border);
}

void
nic_stats_clear(portid_t port_id)
{
	portid_t pid;

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		printf("Valid port range is [0");
		RTE_ETH_FOREACH_DEV(pid)
			printf(", %d", pid);
		printf("]\n");
		return;
	}
	rte_eth_stats_reset(port_id);
	printf("\n  NIC statistics for port %d cleared\n", port_id);
}

void
nic_xstats_display(portid_t port_id)
{
	struct rte_eth_xstat *xstats;
	int cnt_xstats, idx_xstat;
	struct rte_eth_xstat_name *xstats_names;

	printf("###### NIC extended statistics for port %-2d\n", port_id);
	if (!rte_eth_dev_is_valid_port(port_id)) {
		printf("Error: Invalid port number %i\n", port_id);
		return;
	}

	/* Get count */
	cnt_xstats = rte_eth_xstats_get_names(port_id, NULL, 0);
	if (cnt_xstats  < 0) {
		printf("Error: Cannot get count of xstats\n");
		return;
	}

	/* Get id-name lookup table */
	xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * cnt_xstats);
	if (xstats_names == NULL) {
		printf("Cannot allocate memory for xstats lookup\n");
		return;
	}
	if (cnt_xstats != rte_eth_xstats_get_names(
			port_id, xstats_names, cnt_xstats)) {
		printf("Error: Cannot get xstats lookup\n");
		free(xstats_names);
		return;
	}

	/* Get stats themselves */
	xstats = malloc(sizeof(struct rte_eth_xstat) * cnt_xstats);
	if (xstats == NULL) {
		printf("Cannot allocate memory for xstats\n");
		free(xstats_names);
		return;
	}
	if (cnt_xstats != rte_eth_xstats_get(port_id, xstats, cnt_xstats)) {
		printf("Error: Unable to get xstats\n");
		free(xstats_names);
		free(xstats);
		return;
	}

	/* Display xstats */
	for (idx_xstat = 0; idx_xstat < cnt_xstats; idx_xstat++)
		printf("%s: %"PRIu64"\n",
			xstats_names[idx_xstat].name,
			xstats[idx_xstat].value);
	free(xstats_names);
	free(xstats);
}

void
nic_xstats_clear(portid_t port_id)
{
	rte_eth_xstats_reset(port_id);
}

void
nic_stats_mapping_display(portid_t port_id)
{
	struct rte_port *port = &ports[port_id];
	uint16_t i;
	portid_t pid;

	static const char *nic_stats_mapping_border = "########################";

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		printf("Valid port range is [0");
		RTE_ETH_FOREACH_DEV(pid)
			printf(", %d", pid);
		printf("]\n");
		return;
	}

	if ((!port->rx_queue_stats_mapping_enabled) && (!port->tx_queue_stats_mapping_enabled)) {
		printf("Port id %d - either does not support queue statistic mapping or"
		       " no queue statistic mapping set\n", port_id);
		return;
	}

	printf("\n  %s NIC statistics mapping for port %-2d %s\n",
	       nic_stats_mapping_border, port_id, nic_stats_mapping_border);

	if (port->rx_queue_stats_mapping_enabled) {
		for (i = 0; i < nb_rx_queue_stats_mappings; i++) {
			if (rx_queue_stats_mappings[i].port_id == port_id) {
				printf("  RX-queue %2d mapped to Stats Reg %2d\n",
				       rx_queue_stats_mappings[i].queue_id,
				       rx_queue_stats_mappings[i].stats_counter_id);
			}
		}
		printf("\n");
	}


	if (port->tx_queue_stats_mapping_enabled) {
		for (i = 0; i < nb_tx_queue_stats_mappings; i++) {
			if (tx_queue_stats_mappings[i].port_id == port_id) {
				printf("  TX-queue %2d mapped to Stats Reg %2d\n",
				       tx_queue_stats_mappings[i].queue_id,
				       tx_queue_stats_mappings[i].stats_counter_id);
			}
		}
	}

	printf("  %s####################################%s\n",
	       nic_stats_mapping_border, nic_stats_mapping_border);
}

void
rx_queue_infos_display(portid_t port_id, uint16_t queue_id)
{
	struct rte_eth_rxq_info qinfo;
	int32_t rc;
	static const char *info_border = "*********************";

	rc = rte_eth_rx_queue_info_get(port_id, queue_id, &qinfo);
	if (rc != 0) {
		printf("Failed to retrieve information for port: %hhu, "
			"RX queue: %hu\nerror desc: %s(%d)\n",
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
	printf("\nNumber of RXDs: %hu", qinfo.nb_desc);
	printf("\n");
}

void
tx_queue_infos_display(portid_t port_id, uint16_t queue_id)
{
	struct rte_eth_txq_info qinfo;
	int32_t rc;
	static const char *info_border = "*********************";

	rc = rte_eth_tx_queue_info_get(port_id, queue_id, &qinfo);
	if (rc != 0) {
		printf("Failed to retrieve information for port: %hhu, "
			"TX queue: %hu\nerror desc: %s(%d)\n",
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
	printf("\nTX flags: %#x", qinfo.conf.txq_flags);
	printf("\nTX deferred start: %s",
		(qinfo.conf.tx_deferred_start != 0) ? "on" : "off");
	printf("\nNumber of TXDs: %hu", qinfo.nb_desc);
	printf("\n");
}

void
port_infos_display(portid_t port_id)
{
	struct rte_port *port;
	struct ether_addr mac_addr;
	struct rte_eth_link link;
	struct rte_eth_dev_info dev_info;
	int vlan_offload;
	struct rte_mempool * mp;
	static const char *info_border = "*********************";
	portid_t pid;
	uint16_t mtu;

	if (port_id_is_invalid(port_id, ENABLED_WARN)) {
		printf("Valid port range is [0");
		RTE_ETH_FOREACH_DEV(pid)
			printf(", %d", pid);
		printf("]\n");
		return;
	}
	port = &ports[port_id];
	rte_eth_link_get_nowait(port_id, &link);
	memset(&dev_info, 0, sizeof(dev_info));
	rte_eth_dev_info_get(port_id, &dev_info);
	printf("\n%s Infos for port %-2d %s\n",
	       info_border, port_id, info_border);
	rte_eth_macaddr_get(port_id, &mac_addr);
	print_ethaddr("MAC address: ", &mac_addr);
	printf("\nDriver name: %s", dev_info.driver_name);
	printf("\nConnect to socket: %u", port->socket_id);

	if (port_numa[port_id] != NUMA_NO_CONFIG) {
		mp = mbuf_pool_find(port_numa[port_id]);
		if (mp)
			printf("\nmemory allocation on the socket: %d",
							port_numa[port_id]);
	} else
		printf("\nmemory allocation on the socket: %u",port->socket_id);

	printf("\nLink status: %s\n", (link.link_status) ? ("up") : ("down"));
	printf("Link speed: %u Mbps\n", (unsigned) link.link_speed);
	printf("Link duplex: %s\n", (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
	       ("full-duplex") : ("half-duplex"));

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
		if (vlan_offload & ETH_VLAN_STRIP_OFFLOAD)
			printf("  strip on \n");
		else
			printf("  strip off \n");

		if (vlan_offload & ETH_VLAN_FILTER_OFFLOAD)
			printf("  filter on \n");
		else
			printf("  filter off \n");

		if (vlan_offload & ETH_VLAN_EXTEND_OFFLOAD)
			printf("  qinq(extend) on \n");
		else
			printf("  qinq(extend) off \n");
	}

	if (dev_info.hash_key_size > 0)
		printf("Hash key size in bytes: %u\n", dev_info.hash_key_size);
	if (dev_info.reta_size > 0)
		printf("Redirection table size: %u\n", dev_info.reta_size);
	if (!dev_info.flow_type_rss_offloads)
		printf("No flow type is supported.\n");
	else {
		uint16_t i;
		char *p;

		printf("Supported flow types:\n");
		for (i = RTE_ETH_FLOW_UNKNOWN + 1; i < RTE_ETH_FLOW_MAX;
								i++) {
			if (!(dev_info.flow_type_rss_offloads & (1ULL << i)))
				continue;
			p = flowtype_to_str(i);
			printf("  %s\n", (p ? p : "unknown"));
		}
	}

	printf("Max possible RX queues: %u\n", dev_info.max_rx_queues);
	printf("Max possible number of RXDs per queue: %hu\n",
		dev_info.rx_desc_lim.nb_max);
	printf("Min possible number of RXDs per queue: %hu\n",
		dev_info.rx_desc_lim.nb_min);
	printf("RXDs number alignment: %hu\n", dev_info.rx_desc_lim.nb_align);

	printf("Max possible TX queues: %u\n", dev_info.max_tx_queues);
	printf("Max possible number of TXDs per queue: %hu\n",
		dev_info.tx_desc_lim.nb_max);
	printf("Min possible number of TXDs per queue: %hu\n",
		dev_info.tx_desc_lim.nb_min);
	printf("TXDs number alignment: %hu\n", dev_info.tx_desc_lim.nb_align);
}

void
port_offload_cap_display(portid_t port_id)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	static const char *info_border = "************";

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);

	printf("\n%s Port %d supported offload features: %s\n",
		info_border, port_id, info_border);

	if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_VLAN_STRIP) {
		printf("VLAN stripped:                 ");
		if (dev->data->dev_conf.rxmode.hw_vlan_strip)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_QINQ_STRIP) {
		printf("Double VLANs stripped:         ");
		if (dev->data->dev_conf.rxmode.hw_vlan_extend)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_IPV4_CKSUM) {
		printf("RX IPv4 checksum:              ");
		if (dev->data->dev_conf.rxmode.hw_ip_checksum)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_UDP_CKSUM) {
		printf("RX UDP checksum:               ");
		if (dev->data->dev_conf.rxmode.hw_ip_checksum)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_TCP_CKSUM) {
		printf("RX TCP checksum:               ");
		if (dev->data->dev_conf.rxmode.hw_ip_checksum)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM)
		printf("RX Outer IPv4 checksum:        on");

	if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_TCP_LRO) {
		printf("Large receive offload:         ");
		if (dev->data->dev_conf.rxmode.enable_lro)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_VLAN_INSERT) {
		printf("VLAN insert:                   ");
		if (ports[port_id].tx_ol_flags &
		    TESTPMD_TX_OFFLOAD_INSERT_VLAN)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_QINQ_INSERT) {
		printf("Double VLANs insert:           ");
		if (ports[port_id].tx_ol_flags &
		    TESTPMD_TX_OFFLOAD_INSERT_QINQ)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) {
		printf("TX IPv4 checksum:              ");
		if (ports[port_id].tx_ol_flags & TESTPMD_TX_OFFLOAD_IP_CKSUM)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM) {
		printf("TX UDP checksum:               ");
		if (ports[port_id].tx_ol_flags & TESTPMD_TX_OFFLOAD_UDP_CKSUM)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) {
		printf("TX TCP checksum:               ");
		if (ports[port_id].tx_ol_flags & TESTPMD_TX_OFFLOAD_TCP_CKSUM)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_SCTP_CKSUM) {
		printf("TX SCTP checksum:              ");
		if (ports[port_id].tx_ol_flags & TESTPMD_TX_OFFLOAD_SCTP_CKSUM)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM) {
		printf("TX Outer IPv4 checksum:        ");
		if (ports[port_id].tx_ol_flags &
		    TESTPMD_TX_OFFLOAD_OUTER_IP_CKSUM)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_TSO) {
		printf("TX TCP segmentation:           ");
		if (ports[port_id].tso_segsz != 0)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_TSO) {
		printf("TX UDP segmentation:           ");
		if (ports[port_id].tso_segsz != 0)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_VXLAN_TNL_TSO) {
		printf("TSO for VXLAN tunnel packet:   ");
		if (ports[port_id].tunnel_tso_segsz)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_GRE_TNL_TSO) {
		printf("TSO for GRE tunnel packet:     ");
		if (ports[port_id].tunnel_tso_segsz)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPIP_TNL_TSO) {
		printf("TSO for IPIP tunnel packet:    ");
		if (ports[port_id].tunnel_tso_segsz)
			printf("on\n");
		else
			printf("off\n");
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_GENEVE_TNL_TSO) {
		printf("TSO for GENEVE tunnel packet:  ");
		if (ports[port_id].tunnel_tso_segsz)
			printf("on\n");
		else
			printf("off\n");
	}

}

int
port_id_is_invalid(portid_t port_id, enum print_warning warning)
{
	if (port_id == (portid_t)RTE_PORT_ALL)
		return 0;

	if (rte_eth_dev_is_valid_port(port_id))
		return 0;

	if (warning == ENABLED_WARN)
		printf("Invalid port %d\n", port_id);

	return 1;
}

static int
vlan_id_is_invalid(uint16_t vlan_id)
{
	if (vlan_id < 4096)
		return 0;
	printf("Invalid vlan_id %d (must be < 4096)\n", vlan_id);
	return 1;
}

static int
port_reg_off_is_invalid(portid_t port_id, uint32_t reg_off)
{
	uint64_t pci_len;

	if (reg_off & 0x3) {
		printf("Port register offset 0x%X not aligned on a 4-byte "
		       "boundary\n",
		       (unsigned)reg_off);
		return 1;
	}
	pci_len = ports[port_id].dev_info.pci_dev->mem_resource[0].len;
	if (reg_off >= pci_len) {
		printf("Port %d: register offset %u (0x%X) out of port PCI "
		       "resource (length=%"PRIu64")\n",
		       port_id, (unsigned)reg_off, (unsigned)reg_off,  pci_len);
		return 1;
	}
	return 0;
}

static int
reg_bit_pos_is_invalid(uint8_t bit_pos)
{
	if (bit_pos <= 31)
		return 0;
	printf("Invalid bit position %d (must be <= 31)\n", bit_pos);
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
		printf("Invalid bit value %d (must be 0 or 1)\n", (int) bit_v);
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
		printf("Invalid value %u (0x%x) must be < %u (0x%x)\n",
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

void
port_mtu_set(portid_t port_id, uint16_t mtu)
{
	int diag;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	diag = rte_eth_dev_set_mtu(port_id, mtu);
	if (diag == 0)
		return;
	printf("Set MTU failed. diag=%d\n", diag);
}

/* Generic flow management functions. */

/** Generate flow_item[] entry. */
#define MK_FLOW_ITEM(t, s) \
	[RTE_FLOW_ITEM_TYPE_ ## t] = { \
		.name = # t, \
		.size = s, \
	}

/** Information about known flow pattern items. */
static const struct {
	const char *name;
	size_t size;
} flow_item[] = {
	MK_FLOW_ITEM(END, 0),
	MK_FLOW_ITEM(VOID, 0),
	MK_FLOW_ITEM(INVERT, 0),
	MK_FLOW_ITEM(ANY, sizeof(struct rte_flow_item_any)),
	MK_FLOW_ITEM(PF, 0),
	MK_FLOW_ITEM(VF, sizeof(struct rte_flow_item_vf)),
	MK_FLOW_ITEM(PORT, sizeof(struct rte_flow_item_port)),
	MK_FLOW_ITEM(RAW, sizeof(struct rte_flow_item_raw)), /* +pattern[] */
	MK_FLOW_ITEM(ETH, sizeof(struct rte_flow_item_eth)),
	MK_FLOW_ITEM(VLAN, sizeof(struct rte_flow_item_vlan)),
	MK_FLOW_ITEM(IPV4, sizeof(struct rte_flow_item_ipv4)),
	MK_FLOW_ITEM(IPV6, sizeof(struct rte_flow_item_ipv6)),
	MK_FLOW_ITEM(ICMP, sizeof(struct rte_flow_item_icmp)),
	MK_FLOW_ITEM(UDP, sizeof(struct rte_flow_item_udp)),
	MK_FLOW_ITEM(TCP, sizeof(struct rte_flow_item_tcp)),
	MK_FLOW_ITEM(SCTP, sizeof(struct rte_flow_item_sctp)),
	MK_FLOW_ITEM(VXLAN, sizeof(struct rte_flow_item_vxlan)),
	MK_FLOW_ITEM(E_TAG, sizeof(struct rte_flow_item_e_tag)),
	MK_FLOW_ITEM(NVGRE, sizeof(struct rte_flow_item_nvgre)),
	MK_FLOW_ITEM(MPLS, sizeof(struct rte_flow_item_mpls)),
	MK_FLOW_ITEM(GRE, sizeof(struct rte_flow_item_gre)),
	MK_FLOW_ITEM(FUZZY, sizeof(struct rte_flow_item_fuzzy)),
};

/** Compute storage space needed by item specification. */
static void
flow_item_spec_size(const struct rte_flow_item *item,
		    size_t *size, size_t *pad)
{
	if (!item->spec) {
		*size = 0;
		goto empty;
	}
	switch (item->type) {
		union {
			const struct rte_flow_item_raw *raw;
		} spec;

	case RTE_FLOW_ITEM_TYPE_RAW:
		spec.raw = item->spec;
		*size = offsetof(struct rte_flow_item_raw, pattern) +
			spec.raw->length * sizeof(*spec.raw->pattern);
		break;
	default:
		*size = flow_item[item->type].size;
		break;
	}
empty:
	*pad = RTE_ALIGN_CEIL(*size, sizeof(double)) - *size;
}

/** Generate flow_action[] entry. */
#define MK_FLOW_ACTION(t, s) \
	[RTE_FLOW_ACTION_TYPE_ ## t] = { \
		.name = # t, \
		.size = s, \
	}

/** Information about known flow actions. */
static const struct {
	const char *name;
	size_t size;
} flow_action[] = {
	MK_FLOW_ACTION(END, 0),
	MK_FLOW_ACTION(VOID, 0),
	MK_FLOW_ACTION(PASSTHRU, 0),
	MK_FLOW_ACTION(MARK, sizeof(struct rte_flow_action_mark)),
	MK_FLOW_ACTION(FLAG, 0),
	MK_FLOW_ACTION(QUEUE, sizeof(struct rte_flow_action_queue)),
	MK_FLOW_ACTION(DROP, 0),
	MK_FLOW_ACTION(COUNT, 0),
	MK_FLOW_ACTION(DUP, sizeof(struct rte_flow_action_dup)),
	MK_FLOW_ACTION(RSS, sizeof(struct rte_flow_action_rss)), /* +queue[] */
	MK_FLOW_ACTION(PF, 0),
	MK_FLOW_ACTION(VF, sizeof(struct rte_flow_action_vf)),
};

/** Compute storage space needed by action configuration. */
static void
flow_action_conf_size(const struct rte_flow_action *action,
		      size_t *size, size_t *pad)
{
	if (!action->conf) {
		*size = 0;
		goto empty;
	}
	switch (action->type) {
		union {
			const struct rte_flow_action_rss *rss;
		} conf;

	case RTE_FLOW_ACTION_TYPE_RSS:
		conf.rss = action->conf;
		*size = offsetof(struct rte_flow_action_rss, queue) +
			conf.rss->num * sizeof(*conf.rss->queue);
		break;
	default:
		*size = flow_action[action->type].size;
		break;
	}
empty:
	*pad = RTE_ALIGN_CEIL(*size, sizeof(double)) - *size;
}

/** Generate a port_flow entry from attributes/pattern/actions. */
static struct port_flow *
port_flow_new(const struct rte_flow_attr *attr,
	      const struct rte_flow_item *pattern,
	      const struct rte_flow_action *actions)
{
	const struct rte_flow_item *item;
	const struct rte_flow_action *action;
	struct port_flow *pf = NULL;
	size_t tmp;
	size_t pad;
	size_t off1 = 0;
	size_t off2 = 0;
	int err = ENOTSUP;

store:
	item = pattern;
	if (pf)
		pf->pattern = (void *)&pf->data[off1];
	do {
		struct rte_flow_item *dst = NULL;

		if ((unsigned int)item->type >= RTE_DIM(flow_item) ||
		    !flow_item[item->type].name)
			goto notsup;
		if (pf)
			dst = memcpy(pf->data + off1, item, sizeof(*item));
		off1 += sizeof(*item);
		flow_item_spec_size(item, &tmp, &pad);
		if (item->spec) {
			if (pf)
				dst->spec = memcpy(pf->data + off2,
						   item->spec, tmp);
			off2 += tmp + pad;
		}
		if (item->last) {
			if (pf)
				dst->last = memcpy(pf->data + off2,
						   item->last, tmp);
			off2 += tmp + pad;
		}
		if (item->mask) {
			if (pf)
				dst->mask = memcpy(pf->data + off2,
						   item->mask, tmp);
			off2 += tmp + pad;
		}
		off2 = RTE_ALIGN_CEIL(off2, sizeof(double));
	} while ((item++)->type != RTE_FLOW_ITEM_TYPE_END);
	off1 = RTE_ALIGN_CEIL(off1, sizeof(double));
	action = actions;
	if (pf)
		pf->actions = (void *)&pf->data[off1];
	do {
		struct rte_flow_action *dst = NULL;

		if ((unsigned int)action->type >= RTE_DIM(flow_action) ||
		    !flow_action[action->type].name)
			goto notsup;
		if (pf)
			dst = memcpy(pf->data + off1, action, sizeof(*action));
		off1 += sizeof(*action);
		flow_action_conf_size(action, &tmp, &pad);
		if (action->conf) {
			if (pf)
				dst->conf = memcpy(pf->data + off2,
						   action->conf, tmp);
			off2 += tmp + pad;
		}
		off2 = RTE_ALIGN_CEIL(off2, sizeof(double));
	} while ((action++)->type != RTE_FLOW_ACTION_TYPE_END);
	if (pf != NULL)
		return pf;
	off1 = RTE_ALIGN_CEIL(off1, sizeof(double));
	tmp = RTE_ALIGN_CEIL(offsetof(struct port_flow, data), sizeof(double));
	pf = calloc(1, tmp + off1 + off2);
	if (pf == NULL)
		err = errno;
	else {
		*pf = (const struct port_flow){
			.size = tmp + off1 + off2,
			.attr = *attr,
		};
		tmp -= offsetof(struct port_flow, data);
		off2 = tmp + off1;
		off1 = tmp;
		goto store;
	}
notsup:
	rte_errno = err;
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
		[RTE_FLOW_ERROR_TYPE_ATTR] = "attributes structure",
		[RTE_FLOW_ERROR_TYPE_ITEM_NUM] = "pattern length",
		[RTE_FLOW_ERROR_TYPE_ITEM] = "specific pattern item",
		[RTE_FLOW_ERROR_TYPE_ACTION_NUM] = "number of actions",
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
	printf("Caught error type %d (%s): %s%s\n",
	       error->type, errstr,
	       error->cause ? (snprintf(buf, sizeof(buf), "cause: %p, ",
					error->cause), buf) : "",
	       error->message ? error->message : "(no stated reason)");
	return -err;
}

/** Validate flow rule. */
int
port_flow_validate(portid_t port_id,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item *pattern,
		   const struct rte_flow_action *actions)
{
	struct rte_flow_error error;

	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x11, sizeof(error));
	if (rte_flow_validate(port_id, attr, pattern, actions, &error))
		return port_flow_complain(&error);
	printf("Flow rule validated\n");
	return 0;
}

/** Create flow rule. */
int
port_flow_create(portid_t port_id,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item *pattern,
		 const struct rte_flow_action *actions)
{
	struct rte_flow *flow;
	struct rte_port *port;
	struct port_flow *pf;
	uint32_t id;
	struct rte_flow_error error;

	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x22, sizeof(error));
	flow = rte_flow_create(port_id, attr, pattern, actions, &error);
	if (!flow)
		return port_flow_complain(&error);
	port = &ports[port_id];
	if (port->flow_list) {
		if (port->flow_list->id == UINT32_MAX) {
			printf("Highest rule ID is already assigned, delete"
			       " it first");
			rte_flow_destroy(port_id, flow, NULL);
			return -ENOMEM;
		}
		id = port->flow_list->id + 1;
	} else
		id = 0;
	pf = port_flow_new(attr, pattern, actions);
	if (!pf) {
		int err = rte_errno;

		printf("Cannot allocate flow: %s\n", rte_strerror(err));
		rte_flow_destroy(port_id, flow, NULL);
		return -err;
	}
	pf->next = port->flow_list;
	pf->id = id;
	pf->flow = flow;
	port->flow_list = pf;
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

	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x44, sizeof(error));
	if (rte_flow_flush(port_id, &error)) {
		ret = port_flow_complain(&error);
		if (port_id_is_invalid(port_id, DISABLED_WARN) ||
		    port_id == (portid_t)RTE_PORT_ALL)
			return ret;
	}
	port = &ports[port_id];
	while (port->flow_list) {
		struct port_flow *pf = port->flow_list->next;

		free(port->flow_list);
		port->flow_list = pf;
	}
	return ret;
}

/** Query a flow rule. */
int
port_flow_query(portid_t port_id, uint32_t rule,
		enum rte_flow_action_type action)
{
	struct rte_flow_error error;
	struct rte_port *port;
	struct port_flow *pf;
	const char *name;
	union {
		struct rte_flow_query_count count;
	} query;

	if (port_id_is_invalid(port_id, ENABLED_WARN) ||
	    port_id == (portid_t)RTE_PORT_ALL)
		return -EINVAL;
	port = &ports[port_id];
	for (pf = port->flow_list; pf; pf = pf->next)
		if (pf->id == rule)
			break;
	if (!pf) {
		printf("Flow rule #%u not found\n", rule);
		return -ENOENT;
	}
	if ((unsigned int)action >= RTE_DIM(flow_action) ||
	    !flow_action[action].name)
		name = "unknown";
	else
		name = flow_action[action].name;
	switch (action) {
	case RTE_FLOW_ACTION_TYPE_COUNT:
		break;
	default:
		printf("Cannot query action type %d (%s)\n", action, name);
		return -ENOTSUP;
	}
	/* Poisoning to make sure PMDs update it in case of error. */
	memset(&error, 0x55, sizeof(error));
	memset(&query, 0, sizeof(query));
	if (rte_flow_query(port_id, pf->flow, action, &query, &error))
		return port_flow_complain(&error);
	switch (action) {
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
	default:
		printf("Cannot display result for action type %d (%s)\n",
		       action, name);
		break;
	}
	return 0;
}

/** List flow rules. */
void
port_flow_list(portid_t port_id, uint32_t n, const uint32_t group[n])
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

		if (n) {
			/* Filter out unwanted groups. */
			for (i = 0; i != n; ++i)
				if (pf->attr.group == group[i])
					break;
			if (i == n)
				continue;
		}
		tmp = &list;
		while (*tmp &&
		       (pf->attr.group > (*tmp)->attr.group ||
			(pf->attr.group == (*tmp)->attr.group &&
			 pf->attr.priority > (*tmp)->attr.priority) ||
			(pf->attr.group == (*tmp)->attr.group &&
			 pf->attr.priority == (*tmp)->attr.priority &&
			 pf->id > (*tmp)->id)))
			tmp = &(*tmp)->tmp;
		pf->tmp = *tmp;
		*tmp = pf;
	}
	printf("ID\tGroup\tPrio\tAttr\tRule\n");
	for (pf = list; pf != NULL; pf = pf->tmp) {
		const struct rte_flow_item *item = pf->pattern;
		const struct rte_flow_action *action = pf->actions;

		printf("%" PRIu32 "\t%" PRIu32 "\t%" PRIu32 "\t%c%c\t",
		       pf->id,
		       pf->attr.group,
		       pf->attr.priority,
		       pf->attr.ingress ? 'i' : '-',
		       pf->attr.egress ? 'e' : '-');
		while (item->type != RTE_FLOW_ITEM_TYPE_END) {
			if (item->type != RTE_FLOW_ITEM_TYPE_VOID)
				printf("%s ", flow_item[item->type].name);
			++item;
		}
		printf("=>");
		while (action->type != RTE_FLOW_ACTION_TYPE_END) {
			if (action->type != RTE_FLOW_ACTION_TYPE_VOID)
				printf(" %s", flow_action[action->type].name);
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
	printf("Invalid RX queue %d (must be < nb_rxq=%d)\n", rxq_id, nb_rxq);
	return 1;
}

int
tx_queue_id_is_invalid(queueid_t txq_id)
{
	if (txq_id < nb_txq)
		return 0;
	printf("Invalid TX queue %d (must be < nb_rxq=%d)\n", txq_id, nb_txq);
	return 1;
}

static int
rx_desc_id_is_invalid(uint16_t rxdesc_id)
{
	if (rxdesc_id < nb_rxd)
		return 0;
	printf("Invalid RX descriptor %d (must be < nb_rxd=%d)\n",
	       rxdesc_id, nb_rxd);
	return 1;
}

static int
tx_desc_id_is_invalid(uint16_t txdesc_id)
{
	if (txdesc_id < nb_txd)
		return 0;
	printf("Invalid TX descriptor %d (must be < nb_txd=%d)\n",
	       txdesc_id, nb_txd);
	return 1;
}

static const struct rte_memzone *
ring_dma_zone_lookup(const char *ring_name, uint8_t port_id, uint16_t q_id)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(mz_name, sizeof(mz_name), "%s_%s_%d_%d",
		 ports[port_id].dev_info.driver_name, ring_name, port_id, q_id);
	mz = rte_memzone_lookup(mz_name);
	if (mz == NULL)
		printf("%s ring memory zoneof (port %d, queue %d) not"
		       "found (zone name = %s\n",
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
			   uint8_t port_id,
#else
			   __rte_unused uint8_t port_id,
#endif
			   uint16_t desc_id)
{
	struct igb_ring_desc_16_bytes *ring =
		(struct igb_ring_desc_16_bytes *)ring_mz->addr;
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
	struct rte_eth_dev_info dev_info;

	memset(&dev_info, 0, sizeof(dev_info));
	rte_eth_dev_info_get(port_id, &dev_info);
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

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (rx_queue_id_is_invalid(rxq_id))
		return;
	if (rx_desc_id_is_invalid(rxd_id))
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

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (tx_queue_id_is_invalid(txq_id))
		return;
	if (tx_desc_id_is_invalid(txd_id))
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
	printf("  %s packet forwarding%s - CRC stripping %s - "
	       "packets/burst=%d\n", cur_fwd_eng->fwd_mode_name,
	       retry_enabled == 0 ? "" : " with retry",
	       rx_mode.hw_strip_crc ? "enabled" : "disabled",
	       nb_pkt_per_burst);

	if (cur_fwd_eng == &tx_only_engine || cur_fwd_eng == &flow_gen_engine)
		printf("  packet len=%u - nb packet segments=%d\n",
				(unsigned)tx_pkt_length, (int) tx_pkt_nb_segs);

	struct rte_eth_rxconf *rx_conf = &ports[0].rx_conf;
	struct rte_eth_txconf *tx_conf = &ports[0].tx_conf;

	printf("  nb forwarding cores=%d - nb forwarding ports=%d\n",
	       nb_fwd_lcores, nb_fwd_ports);
	printf("  RX queues=%d - RX desc=%d - RX free threshold=%d\n",
	       nb_rxq, nb_rxd, rx_conf->rx_free_thresh);
	printf("  RX threshold registers: pthresh=%d hthresh=%d wthresh=%d\n",
	       rx_conf->rx_thresh.pthresh, rx_conf->rx_thresh.hthresh,
	       rx_conf->rx_thresh.wthresh);
	printf("  TX queues=%d - TX desc=%d - TX free threshold=%d\n",
	       nb_txq, nb_txd, tx_conf->tx_free_thresh);
	printf("  TX threshold registers: pthresh=%d hthresh=%d wthresh=%d\n",
	       tx_conf->tx_thresh.pthresh, tx_conf->tx_thresh.hthresh,
	       tx_conf->tx_thresh.wthresh);
	printf("  TX RS bit threshold=%d - TXQ flags=0x%"PRIx32"\n",
	       tx_conf->tx_rs_thresh, tx_conf->txq_flags);
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
		printf("Failed to get RSS RETA info, return code = %d\n", ret);
		return;
	}

	for (i = 0; i < nb_entries; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (!(reta_conf[idx].mask & (1ULL << shift)))
			continue;
		printf("RSS RETA configuration: hash index=%u, queue=%u\n",
					i, reta_conf[idx].reta[shift]);
	}
}

/*
 * Displays the RSS hash functions of a port, and, optionaly, the RSS hash
 * key of the port.
 */
void
port_rss_hash_conf_show(portid_t port_id, char rss_info[], int show_rss_key)
{
	struct rte_eth_rss_conf rss_conf;
	uint8_t rss_key[RSS_HASH_KEY_LENGTH];
	uint64_t rss_hf;
	uint8_t i;
	int diag;
	struct rte_eth_dev_info dev_info;
	uint8_t hash_key_size;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	memset(&dev_info, 0, sizeof(dev_info));
	rte_eth_dev_info_get(port_id, &dev_info);
	if (dev_info.hash_key_size > 0 &&
			dev_info.hash_key_size <= sizeof(rss_key))
		hash_key_size = dev_info.hash_key_size;
	else {
		printf("dev_info did not provide a valid hash key size\n");
		return;
	}

	rss_conf.rss_hf = 0;
	for (i = 0; i < RTE_DIM(rss_type_table); i++) {
		if (!strcmp(rss_info, rss_type_table[i].str))
			rss_conf.rss_hf = rss_type_table[i].rss_type;
	}

	/* Get RSS hash key if asked to display it */
	rss_conf.rss_key = (show_rss_key) ? rss_key : NULL;
	rss_conf.rss_key_len = hash_key_size;
	diag = rte_eth_dev_rss_hash_conf_get(port_id, &rss_conf);
	if (diag != 0) {
		switch (diag) {
		case -ENODEV:
			printf("port index %d invalid\n", port_id);
			break;
		case -ENOTSUP:
			printf("operation not supported by device\n");
			break;
		default:
			printf("operation failed - diag=%d\n", diag);
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
	for (i = 0; i < RTE_DIM(rss_type_table); i++) {
		if (rss_hf & rss_type_table[i].rss_type)
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
			 uint hash_key_len)
{
	struct rte_eth_rss_conf rss_conf;
	int diag;
	unsigned int i;

	rss_conf.rss_key = NULL;
	rss_conf.rss_key_len = hash_key_len;
	rss_conf.rss_hf = 0;
	for (i = 0; i < RTE_DIM(rss_type_table); i++) {
		if (!strcmp(rss_type_table[i].str, rss_type))
			rss_conf.rss_hf = rss_type_table[i].rss_type;
	}
	diag = rte_eth_dev_rss_hash_conf_get(port_id, &rss_conf);
	if (diag == 0) {
		rss_conf.rss_key = hash_key;
		diag = rte_eth_dev_rss_hash_update(port_id, &rss_conf);
	}
	if (diag == 0)
		return;

	switch (diag) {
	case -ENODEV:
		printf("port index %d invalid\n", port_id);
		break;
	case -ENOTSUP:
		printf("operation not supported by device\n");
		break;
	default:
		printf("operation failed - diag=%d\n", diag);
		break;
	}
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

static void
simple_fwd_config_setup(void)
{
	portid_t i;
	portid_t j;
	portid_t inc = 2;

	if (port_topology == PORT_TOPOLOGY_CHAINED ||
	    port_topology == PORT_TOPOLOGY_LOOP) {
		inc = 1;
	} else if (nb_fwd_ports % 2) {
		printf("\nWarning! Cannot handle an odd number of ports "
		       "with the current port topology. Configuration "
		       "must be changed to have an even number of ports, "
		       "or relaunch application with "
		       "--port-topology=chained\n\n");
	}

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

	for (i = 0; i < cur_fwd_config.nb_fwd_ports; i = (portid_t) (i + inc)) {
		if (port_topology != PORT_TOPOLOGY_LOOP)
			j = (portid_t) ((i + 1) % cur_fwd_config.nb_fwd_ports);
		else
			j = i;
		fwd_streams[i]->rx_port   = fwd_ports_ids[i];
		fwd_streams[i]->rx_queue  = 0;
		fwd_streams[i]->tx_port   = fwd_ports_ids[j];
		fwd_streams[i]->tx_queue  = 0;
		fwd_streams[i]->peer_addr = j;
		fwd_streams[i]->retry_enabled = retry_enabled;

		if (port_topology == PORT_TOPOLOGY_PAIRED) {
			fwd_streams[j]->rx_port   = fwd_ports_ids[j];
			fwd_streams[j]->rx_queue  = 0;
			fwd_streams[j]->tx_port   = fwd_ports_ids[i];
			fwd_streams[j]->tx_queue  = 0;
			fwd_streams[j]->peer_addr = i;
			fwd_streams[j]->retry_enabled = retry_enabled;
		}
	}
}

/**
 * For the RSS forwarding test all streams distributed over lcores. Each stream
 * being composed of a RX queue to poll on a RX port for input messages,
 * associated with a TX queue of a TX port where to send forwarded packets.
 * All packets received on the RX queue of index "RxQj" of the RX port "RxPi"
 * are sent on the TX queue "TxQl" of the TX port "TxPk" according to the two
 * following rules:
 *    - TxPk = (RxPi + 1) if RxPi is even, (RxPi - 1) if RxPi is odd
 *    - TxQl = RxQj
 */
static void
rss_fwd_config_setup(void)
{
	portid_t   rxp;
	portid_t   txp;
	queueid_t  rxq;
	queueid_t  nb_q;
	streamid_t  sm_id;

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
	rxp = 0; rxq = 0;
	for (sm_id = 0; sm_id < cur_fwd_config.nb_fwd_streams; sm_id++) {
		struct fwd_stream *fs;

		fs = fwd_streams[sm_id];

		if ((rxp & 0x1) == 0)
			txp = (portid_t) (rxp + 1);
		else
			txp = (portid_t) (rxp - 1);
		/*
		 * if we are in loopback, simply send stuff out through the
		 * ingress port
		 */
		if (port_topology == PORT_TOPOLOGY_LOOP)
			txp = rxp;

		fs->rx_port = fwd_ports_ids[rxp];
		fs->rx_queue = rxq;
		fs->tx_port = fwd_ports_ids[txp];
		fs->tx_queue = rxq;
		fs->peer_addr = fs->tx_port;
		fs->retry_enabled = retry_enabled;
		rxq = (queueid_t) (rxq + 1);
		if (rxq < nb_q)
			continue;
		/*
		 * rxq == nb_q
		 * Restart from RX queue 0 on next RX port
		 */
		rxq = 0;
		if (numa_support && (nb_fwd_ports <= (nb_ports >> 1)))
			rxp = (portid_t)
				(rxp + ((nb_ports >> 1) / nb_fwd_ports));
		else
			rxp = (portid_t) (rxp + 1);
	}
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
	uint8_t tc = 0;

	cur_fwd_config.nb_fwd_lcores = (lcoreid_t) nb_fwd_lcores;
	cur_fwd_config.nb_fwd_ports = nb_fwd_ports;
	cur_fwd_config.nb_fwd_streams =
		(streamid_t) (nb_rxq * cur_fwd_config.nb_fwd_ports);

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
		for (i = 0; i < ETH_MAX_VMDQ_POOL; i++) {
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
	cur_fwd_config.fwd_eng = cur_fwd_eng;
	if (strcmp(cur_fwd_eng->fwd_mode_name, "icmpecho") == 0) {
		icmp_echo_config_setup();
		return;
	}
	if ((nb_rxq > 1) && (nb_txq > 1)){
		if (dcb_config)
			dcb_fwd_config_setup();
		else
			rss_fwd_config_setup();
	}
	else
		simple_fwd_config_setup();
}

void
pkt_fwd_config_display(struct fwd_config *cfg)
{
	struct fwd_stream *fs;
	lcoreid_t  lc_id;
	streamid_t sm_id;

	printf("%s packet forwarding%s - ports=%d - cores=%d - streams=%d - "
		"NUMA support %s, MP over anonymous pages %s\n",
		cfg->fwd_eng->fwd_mode_name,
		retry_enabled == 0 ? "" : " with retry",
		cfg->nb_fwd_ports, cfg->nb_fwd_lcores, cfg->nb_fwd_streams,
		numa_support == 1 ? "enabled" : "disabled",
		mp_anon != 0 ? "enabled" : "disabled");

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
			printf("lcore %u not enabled\n", lcore_cpuid);
			return -1;
		}
		if (lcore_cpuid == rte_get_master_lcore()) {
			printf("lcore %u cannot be masked on for running "
			       "packet forwarding, which is the master lcore "
			       "and reserved for command line parsing only\n",
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
		printf("Invalid NULL mask of cores\n");
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
	if (nb_lc > nb_cfg_lcores) {
		printf("nb fwd cores %u > %u (max. number of configured "
		       "lcores) - ignored\n",
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

void
set_fwd_ports_mask(uint64_t portmask)
{
	unsigned int portlist[64];
	unsigned int nb_pt;
	unsigned int i;

	if (portmask == 0) {
		printf("Invalid NULL mask of ports\n");
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
		printf("nb fwd ports %u > %u (number of configured "
		       "ports) - ignored\n",
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
		printf("nb pkt per burst: %u > %u (maximum packet per burst) "
		       " ignored\n",
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
	printf("unknown value: \"%s\"\n", name);
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

void
set_tx_pkt_segments(unsigned *seg_lengths, unsigned nb_segs)
{
	uint16_t tx_pkt_len;
	unsigned i;

	if (nb_segs >= (unsigned) nb_txd) {
		printf("nb segments per TX packets=%u >= nb_txd=%u - ignored\n",
		       nb_segs, (unsigned int) nb_txd);
		return;
	}

	/*
	 * Check that each segment length is greater or equal than
	 * the mbuf data sise.
	 * Check also that the total packet length is greater or equal than the
	 * size of an empty UDP/IP packet (sizeof(struct ether_hdr) + 20 + 8).
	 */
	tx_pkt_len = 0;
	for (i = 0; i < nb_segs; i++) {
		if (seg_lengths[i] > (unsigned) mbuf_data_size) {
			printf("length[%u]=%u > mbuf_data_size=%u - give up\n",
			       i, seg_lengths[i], (unsigned) mbuf_data_size);
			return;
		}
		tx_pkt_len = (uint16_t)(tx_pkt_len + seg_lengths[i]);
	}
	if (tx_pkt_len < (sizeof(struct ether_hdr) + 20 + 8)) {
		printf("total packet length=%u < %d - give up\n",
				(unsigned) tx_pkt_len,
				(int)(sizeof(struct ether_hdr) + 20 + 8));
		return;
	}

	for (i = 0; i < nb_segs; i++)
		tx_pkt_seg_lengths[i] = (uint16_t) seg_lengths[i];

	tx_pkt_length  = tx_pkt_len;
	tx_pkt_nb_segs = (uint8_t) nb_segs;
}

void
setup_gro(const char *mode, uint8_t port_id)
{
	if (!rte_eth_dev_is_valid_port(port_id)) {
		printf("invalid port id %u\n", port_id);
		return;
	}
	if (test_done == 0) {
		printf("Before enable/disable GRO,"
				" please stop forwarding first\n");
		return;
	}
	if (strcmp(mode, "on") == 0) {
		if (gro_ports[port_id].enable) {
			printf("port %u has enabled GRO\n", port_id);
			return;
		}
		gro_ports[port_id].enable = 1;
		gro_ports[port_id].param.gro_types = RTE_GRO_TCP_IPV4;

		if (gro_ports[port_id].param.max_flow_num == 0)
			gro_ports[port_id].param.max_flow_num =
				GRO_DEFAULT_FLOW_NUM;
		if (gro_ports[port_id].param.max_item_per_flow == 0)
			gro_ports[port_id].param.max_item_per_flow =
				GRO_DEFAULT_ITEM_NUM_PER_FLOW;
	} else {
		if (gro_ports[port_id].enable == 0) {
			printf("port %u has disabled GRO\n", port_id);
			return;
		}
		gro_ports[port_id].enable = 0;
	}
}

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
	printf("Invalid %s packet forwarding mode\n", fwd_mode_name);
}

void
set_verbose_level(uint16_t vb_level)
{
	printf("Change verbose level from %u to %u\n",
	       (unsigned int) verbose_level, (unsigned int) vb_level);
	verbose_level = vb_level;
}

void
vlan_extend_set(portid_t port_id, int on)
{
	int diag;
	int vlan_offload;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);

	if (on)
		vlan_offload |= ETH_VLAN_EXTEND_OFFLOAD;
	else
		vlan_offload &= ~ETH_VLAN_EXTEND_OFFLOAD;

	diag = rte_eth_dev_set_vlan_offload(port_id, vlan_offload);
	if (diag < 0)
		printf("rx_vlan_extend_set(port_pi=%d, on=%d) failed "
	       "diag=%d\n", port_id, on, diag);
}

void
rx_vlan_strip_set(portid_t port_id, int on)
{
	int diag;
	int vlan_offload;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);

	if (on)
		vlan_offload |= ETH_VLAN_STRIP_OFFLOAD;
	else
		vlan_offload &= ~ETH_VLAN_STRIP_OFFLOAD;

	diag = rte_eth_dev_set_vlan_offload(port_id, vlan_offload);
	if (diag < 0)
		printf("rx_vlan_strip_set(port_pi=%d, on=%d) failed "
	       "diag=%d\n", port_id, on, diag);
}

void
rx_vlan_strip_set_on_queue(portid_t port_id, uint16_t queue_id, int on)
{
	int diag;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	diag = rte_eth_dev_set_vlan_strip_on_queue(port_id, queue_id, on);
	if (diag < 0)
		printf("rx_vlan_strip_set_on_queue(port_pi=%d, queue_id=%d, on=%d) failed "
	       "diag=%d\n", port_id, queue_id, on, diag);
}

void
rx_vlan_filter_set(portid_t port_id, int on)
{
	int diag;
	int vlan_offload;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);

	if (on)
		vlan_offload |= ETH_VLAN_FILTER_OFFLOAD;
	else
		vlan_offload &= ~ETH_VLAN_FILTER_OFFLOAD;

	diag = rte_eth_dev_set_vlan_offload(port_id, vlan_offload);
	if (diag < 0)
		printf("rx_vlan_filter_set(port_pi=%d, on=%d) failed "
	       "diag=%d\n", port_id, on, diag);
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
	printf("rte_eth_dev_vlan_filter(port_pi=%d, vlan_id=%d, on=%d) failed "
	       "diag=%d\n",
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

	printf("tx_vlan_tpid_set(port_pi=%d, vlan_type=%d, tpid=%d) failed "
	       "diag=%d\n",
	       port_id, vlan_type, tp_id, diag);
}

void
tx_vlan_set(portid_t port_id, uint16_t vlan_id)
{
	int vlan_offload;
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (vlan_id_is_invalid(vlan_id))
		return;

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);
	if (vlan_offload & ETH_VLAN_EXTEND_OFFLOAD) {
		printf("Error, as QinQ has been enabled.\n");
		return;
	}

	tx_vlan_reset(port_id);
	ports[port_id].tx_ol_flags |= TESTPMD_TX_OFFLOAD_INSERT_VLAN;
	ports[port_id].tx_vlan_id = vlan_id;
}

void
tx_qinq_set(portid_t port_id, uint16_t vlan_id, uint16_t vlan_id_outer)
{
	int vlan_offload;
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	if (vlan_id_is_invalid(vlan_id))
		return;
	if (vlan_id_is_invalid(vlan_id_outer))
		return;

	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);
	if (!(vlan_offload & ETH_VLAN_EXTEND_OFFLOAD)) {
		printf("Error, as QinQ hasn't been enabled.\n");
		return;
	}

	tx_vlan_reset(port_id);
	ports[port_id].tx_ol_flags |= TESTPMD_TX_OFFLOAD_INSERT_QINQ;
	ports[port_id].tx_vlan_id = vlan_id;
	ports[port_id].tx_vlan_id_outer = vlan_id_outer;
}

void
tx_vlan_reset(portid_t port_id)
{
	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	ports[port_id].tx_ol_flags &= ~(TESTPMD_TX_OFFLOAD_INSERT_VLAN |
				TESTPMD_TX_OFFLOAD_INSERT_QINQ);
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
	uint16_t i;
	uint8_t existing_mapping_found = 0;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	if (is_rx ? (rx_queue_id_is_invalid(queue_id)) : (tx_queue_id_is_invalid(queue_id)))
		return;

	if (map_value >= RTE_ETHDEV_QUEUE_STAT_CNTRS) {
		printf("map_value not in required range 0..%d\n",
				RTE_ETHDEV_QUEUE_STAT_CNTRS - 1);
		return;
	}

	if (!is_rx) { /*then tx*/
		for (i = 0; i < nb_tx_queue_stats_mappings; i++) {
			if ((tx_queue_stats_mappings[i].port_id == port_id) &&
			    (tx_queue_stats_mappings[i].queue_id == queue_id)) {
				tx_queue_stats_mappings[i].stats_counter_id = map_value;
				existing_mapping_found = 1;
				break;
			}
		}
		if (!existing_mapping_found) { /* A new additional mapping... */
			tx_queue_stats_mappings[nb_tx_queue_stats_mappings].port_id = port_id;
			tx_queue_stats_mappings[nb_tx_queue_stats_mappings].queue_id = queue_id;
			tx_queue_stats_mappings[nb_tx_queue_stats_mappings].stats_counter_id = map_value;
			nb_tx_queue_stats_mappings++;
		}
	}
	else { /*rx*/
		for (i = 0; i < nb_rx_queue_stats_mappings; i++) {
			if ((rx_queue_stats_mappings[i].port_id == port_id) &&
			    (rx_queue_stats_mappings[i].queue_id == queue_id)) {
				rx_queue_stats_mappings[i].stats_counter_id = map_value;
				existing_mapping_found = 1;
				break;
			}
		}
		if (!existing_mapping_found) { /* A new additional mapping... */
			rx_queue_stats_mappings[nb_rx_queue_stats_mappings].port_id = port_id;
			rx_queue_stats_mappings[nb_rx_queue_stats_mappings].queue_id = queue_id;
			rx_queue_stats_mappings[nb_rx_queue_stats_mappings].stats_counter_id = map_value;
			nb_rx_queue_stats_mappings++;
		}
	}
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

static char *
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
		{"port", RTE_ETH_FLOW_PORT},
		{"vxlan", RTE_ETH_FLOW_VXLAN},
		{"geneve", RTE_ETH_FLOW_GENEVE},
		{"nvgre", RTE_ETH_FLOW_NVGRE},
	};

	for (i = 0; i < RTE_DIM(flowtype_str_table); i++) {
		if (flowtype_str_table[i].ftype == flow_type)
			return flowtype_str_table[i].str;
	}

	return NULL;
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

void
fdir_get_infos(portid_t port_id)
{
	struct rte_eth_fdir_stats fdir_stat;
	struct rte_eth_fdir_info fdir_info;
	int ret;

	static const char *fdir_stats_border = "########################";

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;
	ret = rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_FDIR);
	if (ret < 0) {
		printf("\n FDIR is not supported on port %-2d\n",
			port_id);
		return;
	}

	memset(&fdir_info, 0, sizeof(fdir_info));
	rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR,
			       RTE_ETH_FILTER_INFO, &fdir_info);
	memset(&fdir_stat, 0, sizeof(fdir_stat));
	rte_eth_dev_filter_ctrl(port_id, RTE_ETH_FILTER_FDIR,
			       RTE_ETH_FILTER_STATS, &fdir_stat);
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
			printf("The flex mask table is full. Can not set flex"
				" mask for flow_type(%u).", cfg->flow_type);
			return;
		}
	}
	(void)rte_memcpy(&flex_conf->flex_mask[idx],
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
			printf("The flex payload table is full. Can not set"
				" flex payload for type(%u).", cfg->type);
			return;
		}
	}
	(void)rte_memcpy(&flex_conf->flex_set[idx],
			 cfg,
			 sizeof(struct rte_eth_flex_payload_cfg));

}

void
set_vf_traffic(portid_t port_id, uint8_t is_rx, uint16_t vf, uint8_t on)
{
#ifdef RTE_LIBRTE_IXGBE_PMD
	int diag;

	if (is_rx)
		diag = rte_pmd_ixgbe_set_vf_rx(port_id, vf, on);
	else
		diag = rte_pmd_ixgbe_set_vf_tx(port_id, vf, on);

	if (diag == 0)
		return;
	printf("rte_pmd_ixgbe_set_vf_%s for port_id=%d failed diag=%d\n",
			is_rx ? "rx" : "tx", port_id, diag);
	return;
#endif
	printf("VF %s setting not supported for port %d\n",
			is_rx ? "Rx" : "Tx", port_id);
	RTE_SET_USED(vf);
	RTE_SET_USED(on);
}

int
set_queue_rate_limit(portid_t port_id, uint16_t queue_idx, uint16_t rate)
{
	int diag;
	struct rte_eth_link link;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return 1;
	rte_eth_link_get_nowait(port_id, &link);
	if (rate > link.link_speed) {
		printf("Invalid rate value:%u bigger than link speed: %u\n",
			rate, link.link_speed);
		return 1;
	}
	diag = rte_eth_set_queue_rate_limit(port_id, queue_idx, rate);
	if (diag == 0)
		return diag;
	printf("rte_eth_set_queue_rate_limit for port_id=%d failed diag=%d\n",
		port_id, diag);
	return diag;
}

int
set_vf_rate_limit(portid_t port_id, uint16_t vf, uint16_t rate, uint64_t q_msk)
{
	int diag = -ENOTSUP;

#ifdef RTE_LIBRTE_IXGBE_PMD
	if (diag == -ENOTSUP)
		diag = rte_pmd_ixgbe_set_vf_rate_limit(port_id, vf, rate,
						       q_msk);
#endif
#ifdef RTE_LIBRTE_BNXT_PMD
	if (diag == -ENOTSUP)
		diag = rte_pmd_bnxt_set_vf_rate_limit(port_id, vf, rate, q_msk);
#endif
	if (diag == 0)
		return diag;

	printf("set_vf_rate_limit for port_id=%d failed diag=%d\n",
		port_id, diag);
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
	struct ether_addr *mc_pool;
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
	mc_pool_size = sizeof(struct ether_addr) * (port->mc_addr_nb +
						    MCAST_POOL_INC);
	mc_pool = (struct ether_addr *) realloc(port->mc_addr_pool,
						mc_pool_size);
	if (mc_pool == NULL) {
		printf("allocation of pool of %u multicast addresses failed\n",
		       port->mc_addr_nb + MCAST_POOL_INC);
		return -ENOMEM;
	}

	port->mc_addr_pool = mc_pool;
	port->mc_addr_nb++;
	return 0;

}

static void
mcast_addr_pool_remove(struct rte_port *port, uint32_t addr_idx)
{
	port->mc_addr_nb--;
	if (addr_idx == port->mc_addr_nb) {
		/* No need to recompact the set of multicast addressses. */
		if (port->mc_addr_nb == 0) {
			/* free the pool of multicast addresses. */
			free(port->mc_addr_pool);
			port->mc_addr_pool = NULL;
		}
		return;
	}
	memmove(&port->mc_addr_pool[addr_idx],
		&port->mc_addr_pool[addr_idx + 1],
		sizeof(struct ether_addr) * (port->mc_addr_nb - addr_idx));
}

static void
eth_port_multicast_addr_list_set(uint8_t port_id)
{
	struct rte_port *port;
	int diag;

	port = &ports[port_id];
	diag = rte_eth_dev_set_mc_addr_list(port_id, port->mc_addr_pool,
					    port->mc_addr_nb);
	if (diag == 0)
		return;
	printf("rte_eth_dev_set_mc_addr_list(port=%d, nb=%u) failed. diag=%d\n",
	       port->mc_addr_nb, port_id, -diag);
}

void
mcast_addr_add(uint8_t port_id, struct ether_addr *mc_addr)
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
		if (is_same_ether_addr(mc_addr, &port->mc_addr_pool[i])) {
			printf("multicast address already filtered by port\n");
			return;
		}
	}

	if (mcast_addr_pool_extend(port) != 0)
		return;
	ether_addr_copy(mc_addr, &port->mc_addr_pool[i]);
	eth_port_multicast_addr_list_set(port_id);
}

void
mcast_addr_remove(uint8_t port_id, struct ether_addr *mc_addr)
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
		if (is_same_ether_addr(mc_addr, &port->mc_addr_pool[i]))
			break;
	}
	if (i == port->mc_addr_nb) {
		printf("multicast address not filtered by port %d\n", port_id);
		return;
	}

	mcast_addr_pool_remove(port, i);
	eth_port_multicast_addr_list_set(port_id);
}

void
port_dcb_info_display(uint8_t port_id)
{
	struct rte_eth_dcb_info dcb_info;
	uint16_t i;
	int ret;
	static const char *border = "================";

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return;

	ret = rte_eth_dev_get_dcb_info(port_id, &dcb_info);
	if (ret) {
		printf("\n Failed to get dcb infos on port %-2d\n",
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
open_ddp_package_file(const char *file_path, uint32_t *size)
{
	FILE *fh = fopen(file_path, "rb");
	uint32_t pkg_size;
	uint8_t *buf = NULL;
	int ret = 0;

	if (size)
		*size = 0;

	if (fh == NULL) {
		printf("%s: Failed to open %s\n", __func__, file_path);
		return buf;
	}

	ret = fseek(fh, 0, SEEK_END);
	if (ret < 0) {
		fclose(fh);
		printf("%s: File operations failed\n", __func__);
		return buf;
	}

	pkg_size = ftell(fh);

	buf = (uint8_t *)malloc(pkg_size);
	if (!buf) {
		fclose(fh);
		printf("%s: Failed to malloc memory\n",	__func__);
		return buf;
	}

	ret = fseek(fh, 0, SEEK_SET);
	if (ret < 0) {
		fclose(fh);
		printf("%s: File seek operation failed\n", __func__);
		close_ddp_package_file(buf);
		return NULL;
	}

	ret = fread(buf, 1, pkg_size, fh);
	if (ret < 0) {
		fclose(fh);
		printf("%s: File read operation failed\n", __func__);
		close_ddp_package_file(buf);
		return NULL;
	}

	if (size)
		*size = pkg_size;

	fclose(fh);

	return buf;
}

int
save_ddp_package_file(const char *file_path, uint8_t *buf, uint32_t size)
{
	FILE *fh = fopen(file_path, "wb");

	if (fh == NULL) {
		printf("%s: Failed to open %s\n", __func__, file_path);
		return -1;
	}

	if (fwrite(buf, 1, size, fh) != size) {
		fclose(fh);
		printf("%s: File write operation failed\n", __func__);
		return -1;
	}

	fclose(fh);

	return 0;
}

int
close_ddp_package_file(uint8_t *buf)
{
	if (buf) {
		free((void *)buf);
		return 0;
	}

	return -1;
}
