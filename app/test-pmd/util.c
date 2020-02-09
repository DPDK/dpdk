/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <stdio.h>

#include <rte_net.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_vxlan.h>
#include <rte_ethdev.h>
#include <rte_flow.h>

#include "testpmd.h"

static inline void
print_ether_addr(const char *what, const struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", what, buf);
}

static inline void
dump_pkt_burst(uint16_t port_id, uint16_t queue, struct rte_mbuf *pkts[],
	      uint16_t nb_pkts, int is_rx)
{
	struct rte_mbuf  *mb;
	const struct rte_ether_hdr *eth_hdr;
	struct rte_ether_hdr _eth_hdr;
	uint16_t eth_type;
	uint64_t ol_flags;
	uint16_t i, packet_type;
	uint16_t is_encapsulation;
	char buf[256];
	struct rte_net_hdr_lens hdr_lens;
	uint32_t sw_packet_type;
	uint16_t udp_port;
	uint32_t vx_vni;
	const char *reason;
	int dynf_index;

	if (!nb_pkts)
		return;
	printf("port %u/queue %u: %s %u packets\n",
		port_id, queue,
	       is_rx ? "received" : "sent",
	       (unsigned int) nb_pkts);
	for (i = 0; i < nb_pkts; i++) {
		mb = pkts[i];
		eth_hdr = rte_pktmbuf_read(mb, 0, sizeof(_eth_hdr), &_eth_hdr);
		eth_type = RTE_BE_TO_CPU_16(eth_hdr->ether_type);
		ol_flags = mb->ol_flags;
		packet_type = mb->packet_type;
		is_encapsulation = RTE_ETH_IS_TUNNEL_PKT(packet_type);

		print_ether_addr("  src=", &eth_hdr->s_addr);
		print_ether_addr(" - dst=", &eth_hdr->d_addr);
		printf(" - type=0x%04x - length=%u - nb_segs=%d",
		       eth_type, (unsigned int) mb->pkt_len,
		       (int)mb->nb_segs);
		if (ol_flags & PKT_RX_RSS_HASH) {
			printf(" - RSS hash=0x%x", (unsigned int) mb->hash.rss);
			printf(" - RSS queue=0x%x", (unsigned int) queue);
		}
		if (ol_flags & PKT_RX_FDIR) {
			printf(" - FDIR matched ");
			if (ol_flags & PKT_RX_FDIR_ID)
				printf("ID=0x%x",
				       mb->hash.fdir.hi);
			else if (ol_flags & PKT_RX_FDIR_FLX)
				printf("flex bytes=0x%08x %08x",
				       mb->hash.fdir.hi, mb->hash.fdir.lo);
			else
				printf("hash=0x%x ID=0x%x ",
				       mb->hash.fdir.hash, mb->hash.fdir.id);
		}
		if (ol_flags & PKT_RX_TIMESTAMP)
			printf(" - timestamp %"PRIu64" ", mb->timestamp);
		if (ol_flags & PKT_RX_QINQ)
			printf(" - QinQ VLAN tci=0x%x, VLAN tci outer=0x%x",
			       mb->vlan_tci, mb->vlan_tci_outer);
		else if (ol_flags & PKT_RX_VLAN)
			printf(" - VLAN tci=0x%x", mb->vlan_tci);
		if (!is_rx && (ol_flags & PKT_TX_DYNF_METADATA))
			printf(" - Tx metadata: 0x%x",
			       *RTE_FLOW_DYNF_METADATA(mb));
		if (is_rx && (ol_flags & PKT_RX_DYNF_METADATA))
			printf(" - Rx metadata: 0x%x",
			       *RTE_FLOW_DYNF_METADATA(mb));
		for (dynf_index = 0; dynf_index < 64; dynf_index++) {
			if (dynf_names[dynf_index][0] != '\0')
				printf(" - dynf %s: %d",
				       dynf_names[dynf_index],
				       !!(ol_flags & (1UL << dynf_index)));
		}
		if (mb->packet_type) {
			rte_get_ptype_name(mb->packet_type, buf, sizeof(buf));
			printf(" - hw ptype: %s", buf);
		}
		sw_packet_type = rte_net_get_ptype(mb, &hdr_lens,
					RTE_PTYPE_ALL_MASK);
		rte_get_ptype_name(sw_packet_type, buf, sizeof(buf));
		printf(" - sw ptype: %s", buf);
		if (sw_packet_type & RTE_PTYPE_L2_MASK)
			printf(" - l2_len=%d", hdr_lens.l2_len);
		if (sw_packet_type & RTE_PTYPE_L3_MASK)
			printf(" - l3_len=%d", hdr_lens.l3_len);
		if (sw_packet_type & RTE_PTYPE_L4_MASK)
			printf(" - l4_len=%d", hdr_lens.l4_len);
		if (sw_packet_type & RTE_PTYPE_TUNNEL_MASK)
			printf(" - tunnel_len=%d", hdr_lens.tunnel_len);
		if (sw_packet_type & RTE_PTYPE_INNER_L2_MASK)
			printf(" - inner_l2_len=%d", hdr_lens.inner_l2_len);
		if (sw_packet_type & RTE_PTYPE_INNER_L3_MASK)
			printf(" - inner_l3_len=%d", hdr_lens.inner_l3_len);
		if (sw_packet_type & RTE_PTYPE_INNER_L4_MASK)
			printf(" - inner_l4_len=%d", hdr_lens.inner_l4_len);
		if (is_encapsulation) {
			struct rte_ipv4_hdr *ipv4_hdr;
			struct rte_ipv6_hdr *ipv6_hdr;
			struct rte_udp_hdr *udp_hdr;
			uint8_t l2_len;
			uint8_t l3_len;
			uint8_t l4_len;
			uint8_t l4_proto;
			struct  rte_vxlan_hdr *vxlan_hdr;

			l2_len  = sizeof(struct rte_ether_hdr);

			/* Do not support ipv4 option field */
			if (RTE_ETH_IS_IPV4_HDR(packet_type)) {
				l3_len = sizeof(struct rte_ipv4_hdr);
				ipv4_hdr = rte_pktmbuf_mtod_offset(mb,
				struct rte_ipv4_hdr *,
				l2_len);
				l4_proto = ipv4_hdr->next_proto_id;
			} else {
				l3_len = sizeof(struct rte_ipv6_hdr);
				ipv6_hdr = rte_pktmbuf_mtod_offset(mb,
				struct rte_ipv6_hdr *,
				l2_len);
				l4_proto = ipv6_hdr->proto;
			}
			if (l4_proto == IPPROTO_UDP) {
				udp_hdr = rte_pktmbuf_mtod_offset(mb,
				struct rte_udp_hdr *,
				l2_len + l3_len);
				l4_len = sizeof(struct rte_udp_hdr);
				vxlan_hdr = rte_pktmbuf_mtod_offset(mb,
				struct rte_vxlan_hdr *,
				l2_len + l3_len + l4_len);
				udp_port = RTE_BE_TO_CPU_16(udp_hdr->dst_port);
				vx_vni = rte_be_to_cpu_32(vxlan_hdr->vx_vni);
				printf(" - VXLAN packet: packet type =%d, "
				       "Destination UDP port =%d, VNI = %d",
				       packet_type, udp_port, vx_vni >> 8);
			}
		}
		printf(" - %s queue=0x%x", is_rx ? "Receive" : "Send",
			(unsigned int) queue);
		printf("\n");
		rte_get_rx_ol_flag_list(mb->ol_flags, buf, sizeof(buf));
		printf("  ol_flags: %s\n", buf);
		if (rte_mbuf_check(mb, 1, &reason) < 0)
			printf("INVALID mbuf: %s\n", reason);
	}
}

uint16_t
dump_rx_pkts(uint16_t port_id, uint16_t queue, struct rte_mbuf *pkts[],
	     uint16_t nb_pkts, __rte_unused uint16_t max_pkts,
	     __rte_unused void *user_param)
{
	dump_pkt_burst(port_id, queue, pkts, nb_pkts, 1);
	return nb_pkts;
}

uint16_t
dump_tx_pkts(uint16_t port_id, uint16_t queue, struct rte_mbuf *pkts[],
	     uint16_t nb_pkts, __rte_unused void *user_param)
{
	dump_pkt_burst(port_id, queue, pkts, nb_pkts, 0);
	return nb_pkts;
}

uint16_t
tx_pkt_set_md(uint16_t port_id, __rte_unused uint16_t queue,
	      struct rte_mbuf *pkts[], uint16_t nb_pkts,
	      __rte_unused void *user_param)
{
	uint16_t i = 0;

	/*
	 * Add metadata value to every Tx packet,
	 * and set ol_flags accordingly.
	 */
	if (rte_flow_dynf_metadata_avail())
		for (i = 0; i < nb_pkts; i++) {
			*RTE_FLOW_DYNF_METADATA(pkts[i]) =
						ports[port_id].tx_metadata;
			pkts[i]->ol_flags |= PKT_TX_DYNF_METADATA;
		}
	return nb_pkts;
}

void
add_tx_md_callback(portid_t portid)
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
		if (!ports[portid].tx_set_md_cb[queue])
			ports[portid].tx_set_md_cb[queue] =
				rte_eth_add_tx_callback(portid, queue,
							tx_pkt_set_md, NULL);
}

void
remove_tx_md_callback(portid_t portid)
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
		if (ports[portid].tx_set_md_cb[queue]) {
			rte_eth_remove_tx_callback(portid, queue,
				ports[portid].tx_set_md_cb[queue]);
			ports[portid].tx_set_md_cb[queue] = NULL;
		}
}

uint16_t
tx_pkt_set_dynf(uint16_t port_id, __rte_unused uint16_t queue,
		struct rte_mbuf *pkts[], uint16_t nb_pkts,
		__rte_unused void *user_param)
{
	uint16_t i = 0;

	if (ports[port_id].mbuf_dynf)
		for (i = 0; i < nb_pkts; i++)
			pkts[i]->ol_flags |= ports[port_id].mbuf_dynf;
	return nb_pkts;
}

void
add_tx_dynf_callback(portid_t portid)
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
		if (!ports[portid].tx_set_dynf_cb[queue])
			ports[portid].tx_set_dynf_cb[queue] =
				rte_eth_add_tx_callback(portid, queue,
							tx_pkt_set_dynf, NULL);
}

void
remove_tx_dynf_callback(portid_t portid)
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
		if (ports[portid].tx_set_dynf_cb[queue]) {
			rte_eth_remove_tx_callback(portid, queue,
				ports[portid].tx_set_dynf_cb[queue]);
			ports[portid].tx_set_dynf_cb[queue] = NULL;
		}
}

int
eth_dev_info_get_print_err(uint16_t port_id,
					struct rte_eth_dev_info *dev_info)
{
	int ret;

	ret = rte_eth_dev_info_get(port_id, dev_info);
	if (ret != 0)
		printf("Error during getting device (port %u) info: %s\n",
				port_id, strerror(-ret));

	return ret;
}

void
eth_set_promisc_mode(uint16_t port, int enable)
{
	int ret;

	if (enable)
		ret = rte_eth_promiscuous_enable(port);
	else
		ret = rte_eth_promiscuous_disable(port);

	if (ret != 0)
		printf("Error during %s promiscuous mode for port %u: %s\n",
			enable ? "enabling" : "disabling",
			port, rte_strerror(-ret));
}

void
eth_set_allmulticast_mode(uint16_t port, int enable)
{
	int ret;

	if (enable)
		ret = rte_eth_allmulticast_enable(port);
	else
		ret = rte_eth_allmulticast_disable(port);

	if (ret != 0)
		printf("Error during %s all-multicast mode for port %u: %s\n",
			enable ? "enabling" : "disabling",
			port, rte_strerror(-ret));
}

int
eth_link_get_nowait_print_err(uint16_t port_id, struct rte_eth_link *link)
{
	int ret;

	ret = rte_eth_link_get_nowait(port_id, link);
	if (ret < 0)
		printf("Device (port %u) link get (without wait) failed: %s\n",
			port_id, rte_strerror(-ret));

	return ret;
}

int
eth_macaddr_get_print_err(uint16_t port_id, struct rte_ether_addr *mac_addr)
{
	int ret;

	ret = rte_eth_macaddr_get(port_id, mac_addr);
	if (ret != 0)
		printf("Error getting device (port %u) mac address: %s\n",
				port_id, rte_strerror(-ret));

	return ret;
}
