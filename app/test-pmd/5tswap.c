/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2014-2020 Mellanox Technologies, Ltd
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_flow.h>

#include "macswap_common.h"
#include "testpmd.h"


static inline void
swap_mac(struct rte_ether_hdr *eth_hdr)
{
	struct rte_ether_addr addr;

	/* Swap dest and src mac addresses. */
	rte_ether_addr_copy(&eth_hdr->dst_addr, &addr);
	rte_ether_addr_copy(&eth_hdr->src_addr, &eth_hdr->dst_addr);
	rte_ether_addr_copy(&addr, &eth_hdr->src_addr);
}

static inline void
swap_ipv4(struct rte_ipv4_hdr *ipv4_hdr)
{
	rte_be32_t addr;

	/* Swap dest and src ipv4 addresses. */
	addr = ipv4_hdr->src_addr;
	ipv4_hdr->src_addr = ipv4_hdr->dst_addr;
	ipv4_hdr->dst_addr = addr;
}

static inline void
swap_ipv6(struct rte_ipv6_hdr *ipv6_hdr)
{
	uint8_t addr[16];

	/* Swap dest and src ipv6 addresses. */
	memcpy(&addr, &ipv6_hdr->src_addr, 16);
	memcpy(&ipv6_hdr->src_addr, &ipv6_hdr->dst_addr, 16);
	memcpy(&ipv6_hdr->dst_addr, &addr, 16);
}

static inline void
swap_tcp(struct rte_tcp_hdr *tcp_hdr)
{
	rte_be16_t port;

	/* Swap dest and src tcp port. */
	port = tcp_hdr->src_port;
	tcp_hdr->src_port = tcp_hdr->dst_port;
	tcp_hdr->dst_port = port;
}

static inline void
swap_udp(struct rte_udp_hdr *udp_hdr)
{
	rte_be16_t port;

	/* Swap dest and src udp port */
	port = udp_hdr->src_port;
	udp_hdr->src_port = udp_hdr->dst_port;
	udp_hdr->dst_port = port;
}

/*
 * 5 tuple swap forwarding mode: Swap the source and the destination of layers
 * 2,3,4. Swaps source and destination for MAC, IPv4/IPv6, UDP/TCP.
 * Parses each layer and swaps it. When the next layer doesn't match it stops.
 */
static void
pkt_burst_5tuple_swap(struct fwd_stream *fs)
{
	struct rte_mbuf  *pkts_burst[MAX_PKT_BURST];
	struct rte_port  *txp;
	struct rte_mbuf *mb;
	uint16_t next_proto;
	uint64_t ol_flags;
	uint16_t proto;
	uint16_t nb_rx;
	uint16_t nb_tx;
	uint32_t retry;

	int i;
	union {
		struct rte_ether_hdr *eth;
		struct rte_vlan_hdr *vlan;
		struct rte_ipv4_hdr *ipv4;
		struct rte_ipv6_hdr *ipv6;
		struct rte_tcp_hdr *tcp;
		struct rte_udp_hdr *udp;
		uint8_t *byte;
	} h;

	uint64_t start_tsc = 0;

	get_start_cycles(&start_tsc);

	/*
	 * Receive a burst of packets and forward them.
	 */
	nb_rx = rte_eth_rx_burst(fs->rx_port, fs->rx_queue, pkts_burst,
				 nb_pkt_per_burst);
	inc_rx_burst_stats(fs, nb_rx);
	if (unlikely(nb_rx == 0))
		return;

	fs->rx_packets += nb_rx;
	txp = &ports[fs->tx_port];
	ol_flags = ol_flags_init(txp->dev_conf.txmode.offloads);
	vlan_qinq_set(pkts_burst, nb_rx, ol_flags,
			txp->tx_vlan_id, txp->tx_vlan_id_outer);
	for (i = 0; i < nb_rx; i++) {
		if (likely(i < nb_rx - 1))
			rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[i+1],
					void *));
		mb = pkts_burst[i];
		h.eth = rte_pktmbuf_mtod(mb, struct rte_ether_hdr *);
		proto = h.eth->ether_type;
		swap_mac(h.eth);
		mb->l2_len = sizeof(struct rte_ether_hdr);
		h.eth++;
		while (proto == RTE_BE16(RTE_ETHER_TYPE_VLAN) ||
		       proto == RTE_BE16(RTE_ETHER_TYPE_QINQ)) {
			proto = h.vlan->eth_proto;
			h.vlan++;
			mb->l2_len += sizeof(struct rte_vlan_hdr);
		}
		if (proto == RTE_BE16(RTE_ETHER_TYPE_IPV4)) {
			swap_ipv4(h.ipv4);
			next_proto = h.ipv4->next_proto_id;
			mb->l3_len = rte_ipv4_hdr_len(h.ipv4);
			h.byte += mb->l3_len;
		} else if (proto == RTE_BE16(RTE_ETHER_TYPE_IPV6)) {
			swap_ipv6(h.ipv6);
			next_proto = h.ipv6->proto;
			h.ipv6++;
			mb->l3_len = sizeof(struct rte_ipv6_hdr);
		} else {
			mbuf_field_set(mb, ol_flags);
			continue;
		}
		if (next_proto == IPPROTO_UDP) {
			swap_udp(h.udp);
			mb->l4_len = sizeof(struct rte_udp_hdr);
		} else if (next_proto == IPPROTO_TCP) {
			swap_tcp(h.tcp);
			mb->l4_len = (h.tcp->data_off & 0xf0) >> 2;
		}
		mbuf_field_set(mb, ol_flags);
	}
	nb_tx = rte_eth_tx_burst(fs->tx_port, fs->tx_queue, pkts_burst, nb_rx);
	/*
	 * Retry if necessary
	 */
	if (unlikely(nb_tx < nb_rx) && fs->retry_enabled) {
		retry = 0;
		while (nb_tx < nb_rx && retry++ < burst_tx_retry_num) {
			rte_delay_us(burst_tx_delay_time);
			nb_tx += rte_eth_tx_burst(fs->tx_port, fs->tx_queue,
					&pkts_burst[nb_tx], nb_rx - nb_tx);
		}
	}
	fs->tx_packets += nb_tx;
	inc_tx_burst_stats(fs, nb_tx);
	if (unlikely(nb_tx < nb_rx)) {
		fs->fwd_dropped += (nb_rx - nb_tx);
		do {
			rte_pktmbuf_free(pkts_burst[nb_tx]);
		} while (++nb_tx < nb_rx);
	}
	get_end_cycles(fs, start_tsc);
}

static void
stream_init_5tuple_swap(struct fwd_stream *fs)
{
	bool rx_stopped, tx_stopped;

	rx_stopped = ports[fs->rx_port].rxq[fs->rx_queue].state ==
						RTE_ETH_QUEUE_STATE_STOPPED;
	tx_stopped = ports[fs->tx_port].txq[fs->tx_queue].state ==
						RTE_ETH_QUEUE_STATE_STOPPED;
	fs->disabled = rx_stopped || tx_stopped;
}

struct fwd_engine five_tuple_swap_fwd_engine = {
	.fwd_mode_name  = "5tswap",
	.port_fwd_begin = NULL,
	.port_fwd_end   = NULL,
	.stream_init    = stream_init_5tuple_swap,
	.packet_fwd     = pkt_burst_5tuple_swap,
};
