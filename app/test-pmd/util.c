/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <stdio.h>

#include <rte_bitops.h>
#include <rte_net.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_vxlan.h>
#include <rte_ethdev.h>
#include <rte_flow.h>

#include "testpmd.h"

#define MAX_STRING_LEN 8192

#define MKDUMPSTR(buf, buf_size, cur_len, ...) \
do { \
	if (cur_len >= buf_size) \
		break; \
	cur_len += snprintf(buf + cur_len, buf_size - cur_len, __VA_ARGS__); \
} while (0)

static inline void
print_ether_addr(const char *what, const struct rte_ether_addr *eth_addr,
		 char print_buf[], size_t buf_size, size_t *cur_len)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	MKDUMPSTR(print_buf, buf_size, *cur_len, "%s%s", what, buf);
}

static inline bool
is_timestamp_enabled(const struct rte_mbuf *mbuf)
{
	static uint64_t timestamp_rx_dynflag;
	int timestamp_rx_dynflag_offset;

	if (timestamp_rx_dynflag == 0) {
		timestamp_rx_dynflag_offset = rte_mbuf_dynflag_lookup(
				RTE_MBUF_DYNFLAG_RX_TIMESTAMP_NAME, NULL);
		if (timestamp_rx_dynflag_offset < 0)
			return false;
		timestamp_rx_dynflag = RTE_BIT64(timestamp_rx_dynflag_offset);
	}

	return (mbuf->ol_flags & timestamp_rx_dynflag) != 0;
}

static inline rte_mbuf_timestamp_t
get_timestamp(const struct rte_mbuf *mbuf)
{
	static int timestamp_dynfield_offset = -1;

	if (timestamp_dynfield_offset < 0) {
		timestamp_dynfield_offset = rte_mbuf_dynfield_lookup(
				RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL);
		if (timestamp_dynfield_offset < 0)
			return 0;
	}

	return *RTE_MBUF_DYNFIELD(mbuf,
			timestamp_dynfield_offset, rte_mbuf_timestamp_t *);
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
	char print_buf[MAX_STRING_LEN];
	size_t buf_size = MAX_STRING_LEN;
	size_t cur_len = 0;

	if (!nb_pkts)
		return;
	MKDUMPSTR(print_buf, buf_size, cur_len,
		  "port %u/queue %u: %s %u packets\n", port_id, queue,
		  is_rx ? "received" : "sent", (unsigned int) nb_pkts);
	for (i = 0; i < nb_pkts; i++) {
		int ret;
		struct rte_flow_error error;
		struct rte_flow_restore_info info = { 0, };

		mb = pkts[i];
		eth_hdr = rte_pktmbuf_read(mb, 0, sizeof(_eth_hdr), &_eth_hdr);
		eth_type = RTE_BE_TO_CPU_16(eth_hdr->ether_type);
		packet_type = mb->packet_type;
		is_encapsulation = RTE_ETH_IS_TUNNEL_PKT(packet_type);
		ret = rte_flow_get_restore_info(port_id, mb, &info, &error);
		if (!ret) {
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  "restore info:");
			if (info.flags & RTE_FLOW_RESTORE_INFO_TUNNEL) {
				struct port_flow_tunnel *port_tunnel;

				port_tunnel = port_flow_locate_tunnel
					      (port_id, &info.tunnel);
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  " - tunnel");
				if (port_tunnel)
					MKDUMPSTR(print_buf, buf_size, cur_len,
						  " #%u", port_tunnel->id);
				else
					MKDUMPSTR(print_buf, buf_size, cur_len,
						  " %s", "-none-");
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  " type %s", port_flow_tunnel_type
					  (&info.tunnel));
			} else {
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  " - no tunnel info");
			}
			if (info.flags & RTE_FLOW_RESTORE_INFO_ENCAPSULATED)
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  " - outer header present");
			else
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  " - no outer header");
			if (info.flags & RTE_FLOW_RESTORE_INFO_GROUP_ID)
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  " - miss group %u", info.group_id);
			else
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  " - no miss group");
			MKDUMPSTR(print_buf, buf_size, cur_len, "\n");
		}
		print_ether_addr("  src=", &eth_hdr->s_addr,
				 print_buf, buf_size, &cur_len);
		print_ether_addr(" - dst=", &eth_hdr->d_addr,
				 print_buf, buf_size, &cur_len);
		MKDUMPSTR(print_buf, buf_size, cur_len,
			  " - type=0x%04x - length=%u - nb_segs=%d",
			  eth_type, (unsigned int) mb->pkt_len,
			  (int)mb->nb_segs);
		ol_flags = mb->ol_flags;
		if (ol_flags & PKT_RX_RSS_HASH) {
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - RSS hash=0x%x",
				  (unsigned int) mb->hash.rss);
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - RSS queue=0x%x", (unsigned int) queue);
		}
		if (ol_flags & PKT_RX_FDIR) {
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - FDIR matched ");
			if (ol_flags & PKT_RX_FDIR_ID)
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  "ID=0x%x", mb->hash.fdir.hi);
			else if (ol_flags & PKT_RX_FDIR_FLX)
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  "flex bytes=0x%08x %08x",
					  mb->hash.fdir.hi, mb->hash.fdir.lo);
			else
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  "hash=0x%x ID=0x%x ",
					  mb->hash.fdir.hash, mb->hash.fdir.id);
		}
		if (is_timestamp_enabled(mb))
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - timestamp %"PRIu64" ", get_timestamp(mb));
		if (ol_flags & PKT_RX_QINQ)
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - QinQ VLAN tci=0x%x, VLAN tci outer=0x%x",
				  mb->vlan_tci, mb->vlan_tci_outer);
		else if (ol_flags & PKT_RX_VLAN)
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - VLAN tci=0x%x", mb->vlan_tci);
		if (!is_rx && (ol_flags & PKT_TX_DYNF_METADATA))
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - Tx metadata: 0x%x",
				  *RTE_FLOW_DYNF_METADATA(mb));
		if (is_rx && (ol_flags & PKT_RX_DYNF_METADATA))
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - Rx metadata: 0x%x",
				  *RTE_FLOW_DYNF_METADATA(mb));
		for (dynf_index = 0; dynf_index < 64; dynf_index++) {
			if (dynf_names[dynf_index][0] != '\0')
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  " - dynf %s: %d",
					  dynf_names[dynf_index],
					  !!(ol_flags & (1UL << dynf_index)));
		}
		if (mb->packet_type) {
			rte_get_ptype_name(mb->packet_type, buf, sizeof(buf));
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - hw ptype: %s", buf);
		}
		sw_packet_type = rte_net_get_ptype(mb, &hdr_lens,
					RTE_PTYPE_ALL_MASK);
		rte_get_ptype_name(sw_packet_type, buf, sizeof(buf));
		MKDUMPSTR(print_buf, buf_size, cur_len, " - sw ptype: %s", buf);
		if (sw_packet_type & RTE_PTYPE_L2_MASK)
			MKDUMPSTR(print_buf, buf_size, cur_len, " - l2_len=%d",
				  hdr_lens.l2_len);
		if (sw_packet_type & RTE_PTYPE_L3_MASK)
			MKDUMPSTR(print_buf, buf_size, cur_len, " - l3_len=%d",
				  hdr_lens.l3_len);
		if (sw_packet_type & RTE_PTYPE_L4_MASK)
			MKDUMPSTR(print_buf, buf_size, cur_len, " - l4_len=%d",
				  hdr_lens.l4_len);
		if (sw_packet_type & RTE_PTYPE_TUNNEL_MASK)
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - tunnel_len=%d", hdr_lens.tunnel_len);
		if (sw_packet_type & RTE_PTYPE_INNER_L2_MASK)
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - inner_l2_len=%d", hdr_lens.inner_l2_len);
		if (sw_packet_type & RTE_PTYPE_INNER_L3_MASK)
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - inner_l3_len=%d", hdr_lens.inner_l3_len);
		if (sw_packet_type & RTE_PTYPE_INNER_L4_MASK)
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  " - inner_l4_len=%d", hdr_lens.inner_l4_len);
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
				MKDUMPSTR(print_buf, buf_size, cur_len,
					  " - VXLAN packet: packet type =%d, "
					  "Destination UDP port =%d, VNI = %d, "
					  "last_rsvd = %d", packet_type,
					  udp_port, vx_vni >> 8, vx_vni & 0xff);
			}
		}
		MKDUMPSTR(print_buf, buf_size, cur_len,
			  " - %s queue=0x%x", is_rx ? "Receive" : "Send",
			  (unsigned int) queue);
		MKDUMPSTR(print_buf, buf_size, cur_len, "\n");
		if (is_rx)
			rte_get_rx_ol_flag_list(mb->ol_flags, buf, sizeof(buf));
		else
			rte_get_tx_ol_flag_list(mb->ol_flags, buf, sizeof(buf));

		MKDUMPSTR(print_buf, buf_size, cur_len,
			  "  ol_flags: %s\n", buf);
		if (rte_mbuf_check(mb, 1, &reason) < 0)
			MKDUMPSTR(print_buf, buf_size, cur_len,
				  "INVALID mbuf: %s\n", reason);
		if (cur_len >= buf_size)
			printf("%s ...\n", print_buf);
		else
			printf("%s", print_buf);
		cur_len = 0;
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
		fprintf(stderr,
			"Error during getting device (port %u) info: %s\n",
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
		fprintf(stderr,
			"Error during %s promiscuous mode for port %u: %s\n",
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
		fprintf(stderr,
			"Error during %s all-multicast mode for port %u: %s\n",
			enable ? "enabling" : "disabling",
			port, rte_strerror(-ret));
}

int
eth_link_get_nowait_print_err(uint16_t port_id, struct rte_eth_link *link)
{
	int ret;

	ret = rte_eth_link_get_nowait(port_id, link);
	if (ret < 0)
		fprintf(stderr,
			"Device (port %u) link get (without wait) failed: %s\n",
			port_id, rte_strerror(-ret));

	return ret;
}

int
eth_macaddr_get_print_err(uint16_t port_id, struct rte_ether_addr *mac_addr)
{
	int ret;

	ret = rte_eth_macaddr_get(port_id, mac_addr);
	if (ret != 0)
		fprintf(stderr,
			"Error getting device (port %u) mac address: %s\n",
			port_id, rte_strerror(-ret));

	return ret;
}
