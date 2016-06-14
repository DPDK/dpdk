/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2013 Tilera Corporation. All rights reserved.
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
 *     * Neither the name of Tilera Corporation nor the names of its
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
 *
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
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>

#include "testpmd.h"

/* hardcoded configuration (for now) */
static unsigned cfg_n_flows	= 1024;
static unsigned cfg_pkt_size	= 300;
static uint32_t cfg_ip_src	= IPv4(10, 254, 0, 0);
static uint32_t cfg_ip_dst	= IPv4(10, 253, 0, 0);
static uint16_t cfg_udp_src	= 1000;
static uint16_t cfg_udp_dst	= 1001;
static struct ether_addr cfg_ether_src	=
	{{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x00 }};
static struct ether_addr cfg_ether_dst	=
	{{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x01 }};

#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)

static inline uint16_t
ip_sum(const unaligned_uint16_t *hdr, int hdr_len)
{
	uint32_t sum = 0;

	while (hdr_len > 1)
	{
		sum += *hdr++;
		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);
		hdr_len -= 2;
	}

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

/*
 * Multi-flow generation mode.
 *
 * We originate a bunch of flows (varying destination IP addresses), and
 * terminate receive traffic.  Received traffic is simply discarded, but we
 * still do so in order to maintain traffic statistics.
 */
static void
pkt_burst_flow_gen(struct fwd_stream *fs)
{
	unsigned pkt_size = cfg_pkt_size - 4;	/* Adjust FCS */
	struct rte_mbuf  *pkts_burst[MAX_PKT_BURST];
	struct rte_mempool *mbp;
	struct rte_mbuf  *pkt;
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ip_hdr;
	struct udp_hdr *udp_hdr;
	uint16_t vlan_tci, vlan_tci_outer;
	uint16_t ol_flags;
	uint16_t nb_rx;
	uint16_t nb_tx;
	uint16_t nb_pkt;
	uint16_t i;
	uint32_t retry;
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	uint64_t start_tsc;
	uint64_t end_tsc;
	uint64_t core_cycles;
#endif
	static int next_flow = 0;

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	start_tsc = rte_rdtsc();
#endif

	/* Receive a burst of packets and discard them. */
	nb_rx = rte_eth_rx_burst(fs->rx_port, fs->rx_queue, pkts_burst,
				 nb_pkt_per_burst);
	fs->rx_packets += nb_rx;

	for (i = 0; i < nb_rx; i++)
		rte_pktmbuf_free(pkts_burst[i]);

	mbp = current_fwd_lcore()->mbp;
	vlan_tci = ports[fs->tx_port].tx_vlan_id;
	vlan_tci_outer = ports[fs->tx_port].tx_vlan_id_outer;
	ol_flags = ports[fs->tx_port].tx_ol_flags;

	for (nb_pkt = 0; nb_pkt < nb_pkt_per_burst; nb_pkt++) {
		pkt = rte_mbuf_raw_alloc(mbp);
		if (!pkt)
			break;

		pkt->data_len = pkt_size;
		pkt->next = NULL;

		/* Initialize Ethernet header. */
		eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
		ether_addr_copy(&cfg_ether_dst, &eth_hdr->d_addr);
		ether_addr_copy(&cfg_ether_src, &eth_hdr->s_addr);
		eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

		/* Initialize IP header. */
		ip_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
		memset(ip_hdr, 0, sizeof(*ip_hdr));
		ip_hdr->version_ihl	= IP_VHL_DEF;
		ip_hdr->type_of_service	= 0;
		ip_hdr->fragment_offset	= 0;
		ip_hdr->time_to_live	= IP_DEFTTL;
		ip_hdr->next_proto_id	= IPPROTO_UDP;
		ip_hdr->packet_id	= 0;
		ip_hdr->src_addr	= rte_cpu_to_be_32(cfg_ip_src);
		ip_hdr->dst_addr	= rte_cpu_to_be_32(cfg_ip_dst +
							   next_flow);
		ip_hdr->total_length	= RTE_CPU_TO_BE_16(pkt_size -
							   sizeof(*eth_hdr));
		ip_hdr->hdr_checksum	= ip_sum((unaligned_uint16_t *)ip_hdr,
						 sizeof(*ip_hdr));

		/* Initialize UDP header. */
		udp_hdr = (struct udp_hdr *)(ip_hdr + 1);
		udp_hdr->src_port	= rte_cpu_to_be_16(cfg_udp_src);
		udp_hdr->dst_port	= rte_cpu_to_be_16(cfg_udp_dst);
		udp_hdr->dgram_cksum	= 0; /* No UDP checksum. */
		udp_hdr->dgram_len	= RTE_CPU_TO_BE_16(pkt_size -
							   sizeof(*eth_hdr) -
							   sizeof(*ip_hdr));
		pkt->nb_segs		= 1;
		pkt->pkt_len		= pkt_size;
		pkt->ol_flags		= ol_flags;
		pkt->vlan_tci		= vlan_tci;
		pkt->vlan_tci_outer	= vlan_tci_outer;
		pkt->l2_len		= sizeof(struct ether_hdr);
		pkt->l3_len		= sizeof(struct ipv4_hdr);
		pkts_burst[nb_pkt]	= pkt;

		next_flow = (next_flow + 1) % cfg_n_flows;
	}

	nb_tx = rte_eth_tx_burst(fs->tx_port, fs->tx_queue, pkts_burst, nb_pkt);
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

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	fs->tx_burst_stats.pkt_burst_spread[nb_tx]++;
#endif
	if (unlikely(nb_tx < nb_pkt)) {
		/* Back out the flow counter. */
		next_flow -= (nb_pkt - nb_tx);
		while (next_flow < 0)
			next_flow += cfg_n_flows;

		do {
			rte_pktmbuf_free(pkts_burst[nb_tx]);
		} while (++nb_tx < nb_pkt);
	}
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	end_tsc = rte_rdtsc();
	core_cycles = (end_tsc - start_tsc);
	fs->core_cycles = (uint64_t) (fs->core_cycles + core_cycles);
#endif
}

struct fwd_engine flow_gen_engine = {
	.fwd_mode_name  = "flowgen",
	.port_fwd_begin = NULL,
	.port_fwd_end   = NULL,
	.packet_fwd     = pkt_burst_flow_gen,
};
