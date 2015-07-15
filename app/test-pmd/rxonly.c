/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "testpmd.h"

static inline void
print_ether_addr(const char *what, struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", what, buf);
}

/*
 * Received a burst of packets.
 */
static void
pkt_burst_receive(struct fwd_stream *fs)
{
	struct rte_mbuf  *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf  *mb;
	struct ether_hdr *eth_hdr;
	uint16_t eth_type;
	uint64_t ol_flags;
	uint16_t nb_rx;
	uint16_t i, packet_type;
#ifdef RTE_NEXT_ABI
	uint16_t is_encapsulation;
#else
	uint64_t is_encapsulation;
#endif

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	uint64_t start_tsc;
	uint64_t end_tsc;
	uint64_t core_cycles;
#endif

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	start_tsc = rte_rdtsc();
#endif

	/*
	 * Receive a burst of packets.
	 */
	nb_rx = rte_eth_rx_burst(fs->rx_port, fs->rx_queue, pkts_burst,
				 nb_pkt_per_burst);
	if (unlikely(nb_rx == 0))
		return;

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	fs->rx_burst_stats.pkt_burst_spread[nb_rx]++;
#endif
	fs->rx_packets += nb_rx;

	/*
	 * Dump each received packet if verbose_level > 0.
	 */
	if (verbose_level > 0)
		printf("port %u/queue %u: received %u packets\n",
		       (unsigned) fs->rx_port,
		       (unsigned) fs->rx_queue,
		       (unsigned) nb_rx);
	for (i = 0; i < nb_rx; i++) {
		mb = pkts_burst[i];
		if (verbose_level == 0) {
			rte_pktmbuf_free(mb);
			continue;
		}
		eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr *);
		eth_type = RTE_BE_TO_CPU_16(eth_hdr->ether_type);
		ol_flags = mb->ol_flags;
		packet_type = mb->packet_type;

#ifdef RTE_NEXT_ABI
		is_encapsulation = RTE_ETH_IS_TUNNEL_PKT(packet_type);
#else
		is_encapsulation = ol_flags & (PKT_RX_TUNNEL_IPV4_HDR |
				PKT_RX_TUNNEL_IPV6_HDR);
#endif

		print_ether_addr("  src=", &eth_hdr->s_addr);
		print_ether_addr(" - dst=", &eth_hdr->d_addr);
		printf(" - type=0x%04x - length=%u - nb_segs=%d",
		       eth_type, (unsigned) mb->pkt_len,
		       (int)mb->nb_segs);
		if (ol_flags & PKT_RX_RSS_HASH) {
			printf(" - RSS hash=0x%x", (unsigned) mb->hash.rss);
			printf(" - RSS queue=0x%x",(unsigned) fs->rx_queue);
		} else if (ol_flags & PKT_RX_FDIR) {
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
		if (ol_flags & PKT_RX_VLAN_PKT)
			printf(" - VLAN tci=0x%x", mb->vlan_tci);
		if (ol_flags & PKT_RX_QINQ_PKT)
			printf(" - QinQ VLAN tci=0x%x, VLAN tci outer=0x%x",
					mb->vlan_tci, mb->vlan_tci_outer);
#ifdef RTE_NEXT_ABI
		if (mb->packet_type) {
			uint32_t ptype;

			/* (outer) L2 packet type */
			ptype = mb->packet_type & RTE_PTYPE_L2_MASK;
			switch (ptype) {
			case RTE_PTYPE_L2_ETHER:
				printf(" - (outer) L2 type: ETHER");
				break;
			case RTE_PTYPE_L2_ETHER_TIMESYNC:
				printf(" - (outer) L2 type: ETHER_Timesync");
				break;
			case RTE_PTYPE_L2_ETHER_ARP:
				printf(" - (outer) L2 type: ETHER_ARP");
				break;
			case RTE_PTYPE_L2_ETHER_LLDP:
				printf(" - (outer) L2 type: ETHER_LLDP");
				break;
			default:
				printf(" - (outer) L2 type: Unknown");
				break;
			}

			/* (outer) L3 packet type */
			ptype = mb->packet_type & RTE_PTYPE_L3_MASK;
			switch (ptype) {
			case RTE_PTYPE_L3_IPV4:
				printf(" - (outer) L3 type: IPV4");
				break;
			case RTE_PTYPE_L3_IPV4_EXT:
				printf(" - (outer) L3 type: IPV4_EXT");
				break;
			case RTE_PTYPE_L3_IPV6:
				printf(" - (outer) L3 type: IPV6");
				break;
			case RTE_PTYPE_L3_IPV4_EXT_UNKNOWN:
				printf(" - (outer) L3 type: IPV4_EXT_UNKNOWN");
				break;
			case RTE_PTYPE_L3_IPV6_EXT:
				printf(" - (outer) L3 type: IPV6_EXT");
				break;
			case RTE_PTYPE_L3_IPV6_EXT_UNKNOWN:
				printf(" - (outer) L3 type: IPV6_EXT_UNKNOWN");
				break;
			default:
				printf(" - (outer) L3 type: Unknown");
				break;
			}

			/* (outer) L4 packet type */
			ptype = mb->packet_type & RTE_PTYPE_L4_MASK;
			switch (ptype) {
			case RTE_PTYPE_L4_TCP:
				printf(" - (outer) L4 type: TCP");
				break;
			case RTE_PTYPE_L4_UDP:
				printf(" - (outer) L4 type: UDP");
				break;
			case RTE_PTYPE_L4_FRAG:
				printf(" - (outer) L4 type: L4_FRAG");
				break;
			case RTE_PTYPE_L4_SCTP:
				printf(" - (outer) L4 type: SCTP");
				break;
			case RTE_PTYPE_L4_ICMP:
				printf(" - (outer) L4 type: ICMP");
				break;
			case RTE_PTYPE_L4_NONFRAG:
				printf(" - (outer) L4 type: L4_NONFRAG");
				break;
			default:
				printf(" - (outer) L4 type: Unknown");
				break;
			}

			/* packet tunnel type */
			ptype = mb->packet_type & RTE_PTYPE_TUNNEL_MASK;
			switch (ptype) {
			case RTE_PTYPE_TUNNEL_IP:
				printf(" - Tunnel type: IP");
				break;
			case RTE_PTYPE_TUNNEL_GRE:
				printf(" - Tunnel type: GRE");
				break;
			case RTE_PTYPE_TUNNEL_VXLAN:
				printf(" - Tunnel type: VXLAN");
				break;
			case RTE_PTYPE_TUNNEL_NVGRE:
				printf(" - Tunnel type: NVGRE");
				break;
			case RTE_PTYPE_TUNNEL_GENEVE:
				printf(" - Tunnel type: GENEVE");
				break;
			case RTE_PTYPE_TUNNEL_GRENAT:
				printf(" - Tunnel type: GRENAT");
				break;
			default:
				printf(" - Tunnel type: Unknown");
				break;
			}

			/* inner L2 packet type */
			ptype = mb->packet_type & RTE_PTYPE_INNER_L2_MASK;
			switch (ptype) {
			case RTE_PTYPE_INNER_L2_ETHER:
				printf(" - Inner L2 type: ETHER");
				break;
			case RTE_PTYPE_INNER_L2_ETHER_VLAN:
				printf(" - Inner L2 type: ETHER_VLAN");
				break;
			default:
				printf(" - Inner L2 type: Unknown");
				break;
			}

			/* inner L3 packet type */
			ptype = mb->packet_type & RTE_PTYPE_INNER_L3_MASK;
			switch (ptype) {
			case RTE_PTYPE_INNER_L3_IPV4:
				printf(" - Inner L3 type: IPV4");
				break;
			case RTE_PTYPE_INNER_L3_IPV4_EXT:
				printf(" - Inner L3 type: IPV4_EXT");
				break;
			case RTE_PTYPE_INNER_L3_IPV6:
				printf(" - Inner L3 type: IPV6");
				break;
			case RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN:
				printf(" - Inner L3 type: IPV4_EXT_UNKNOWN");
				break;
			case RTE_PTYPE_INNER_L3_IPV6_EXT:
				printf(" - Inner L3 type: IPV6_EXT");
				break;
			case RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN:
				printf(" - Inner L3 type: IPV6_EXT_UNKNOWN");
				break;
			default:
				printf(" - Inner L3 type: Unknown");
				break;
			}

			/* inner L4 packet type */
			ptype = mb->packet_type & RTE_PTYPE_INNER_L4_MASK;
			switch (ptype) {
			case RTE_PTYPE_INNER_L4_TCP:
				printf(" - Inner L4 type: TCP");
				break;
			case RTE_PTYPE_INNER_L4_UDP:
				printf(" - Inner L4 type: UDP");
				break;
			case RTE_PTYPE_INNER_L4_FRAG:
				printf(" - Inner L4 type: L4_FRAG");
				break;
			case RTE_PTYPE_INNER_L4_SCTP:
				printf(" - Inner L4 type: SCTP");
				break;
			case RTE_PTYPE_INNER_L4_ICMP:
				printf(" - Inner L4 type: ICMP");
				break;
			case RTE_PTYPE_INNER_L4_NONFRAG:
				printf(" - Inner L4 type: L4_NONFRAG");
				break;
			default:
				printf(" - Inner L4 type: Unknown");
				break;
			}
			printf("\n");
		} else
			printf("Unknown packet type\n");
#endif /* RTE_NEXT_ABI */
		if (is_encapsulation) {
			struct ipv4_hdr *ipv4_hdr;
			struct ipv6_hdr *ipv6_hdr;
			struct udp_hdr *udp_hdr;
			uint8_t l2_len;
			uint8_t l3_len;
			uint8_t l4_len;
			uint8_t l4_proto;
			struct  vxlan_hdr *vxlan_hdr;

			l2_len  = sizeof(struct ether_hdr);

			 /* Do not support ipv4 option field */
#ifdef RTE_NEXT_ABI
			if (RTE_ETH_IS_IPV4_HDR(packet_type)) {
#else
			if (ol_flags & PKT_RX_TUNNEL_IPV4_HDR) {
#endif
				l3_len = sizeof(struct ipv4_hdr);
				ipv4_hdr = rte_pktmbuf_mtod_offset(mb,
								   struct ipv4_hdr *,
								   l2_len);
				l4_proto = ipv4_hdr->next_proto_id;
			} else {
				l3_len = sizeof(struct ipv6_hdr);
				ipv6_hdr = rte_pktmbuf_mtod_offset(mb,
								   struct ipv6_hdr *,
								   l2_len);
				l4_proto = ipv6_hdr->proto;
			}
			if (l4_proto == IPPROTO_UDP) {
				udp_hdr = rte_pktmbuf_mtod_offset(mb,
								  struct udp_hdr *,
								  l2_len + l3_len);
				l4_len = sizeof(struct udp_hdr);
				vxlan_hdr = rte_pktmbuf_mtod_offset(mb,
								    struct vxlan_hdr *,
								    l2_len + l3_len + l4_len);

				printf(" - VXLAN packet: packet type =%d, "
					"Destination UDP port =%d, VNI = %d",
					packet_type, RTE_BE_TO_CPU_16(udp_hdr->dst_port),
					rte_be_to_cpu_32(vxlan_hdr->vx_vni) >> 8);
			}
		}
		printf(" - Receive queue=0x%x", (unsigned) fs->rx_queue);
		printf("\n");
		if (ol_flags != 0) {
			unsigned rxf;
			const char *name;

			for (rxf = 0; rxf < sizeof(mb->ol_flags) * 8; rxf++) {
				if ((ol_flags & (1ULL << rxf)) == 0)
					continue;
				name = rte_get_rx_ol_flag_name(1ULL << rxf);
				if (name == NULL)
					continue;
				printf("  %s\n", name);
			}
		}
		rte_pktmbuf_free(mb);
	}

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	end_tsc = rte_rdtsc();
	core_cycles = (end_tsc - start_tsc);
	fs->core_cycles = (uint64_t) (fs->core_cycles + core_cycles);
#endif
}

struct fwd_engine rx_only_engine = {
	.fwd_mode_name  = "rxonly",
	.port_fwd_begin = NULL,
	.port_fwd_end   = NULL,
	.packet_fwd     = pkt_burst_receive,
};
