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
#include <rte_tailq.h>
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

#include "testpmd.h"

#define MAX_PKT_RX_FLAGS 11
static const char *pkt_rx_flag_names[MAX_PKT_RX_FLAGS] = {
	"VLAN_PKT",
	"RSS_HASH",
	"PKT_RX_FDIR",
	"IP_CKSUM",
	"IP_CKSUM_BAD",

	"IPV4_HDR",
	"IPV4_HDR_EXT",
	"IPV6_HDR",
	"IPV6_HDR_EXT",

	"IEEE1588_PTP",
	"IEEE1588_TMST",
};

static inline void
print_ether_addr(const char *what, struct ether_addr *eth_addr)
{
	printf("%s%02X:%02X:%02X:%02X:%02X:%02X",
	       what,
	       eth_addr->addr_bytes[0],
	       eth_addr->addr_bytes[1],
	       eth_addr->addr_bytes[2],
	       eth_addr->addr_bytes[3],
	       eth_addr->addr_bytes[4],
	       eth_addr->addr_bytes[5]);
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
	uint16_t ol_flags;
	uint16_t nb_rx;
	uint16_t i;
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
		eth_hdr = (struct ether_hdr *) mb->pkt.data;
		eth_type = RTE_BE_TO_CPU_16(eth_hdr->ether_type);
		ol_flags = mb->ol_flags;
		print_ether_addr("  src=", &eth_hdr->s_addr);
		print_ether_addr(" - dst=", &eth_hdr->d_addr);
		printf(" - type=0x%04x - length=%u - nb_segs=%d",
		       eth_type, (unsigned) mb->pkt.pkt_len,
		       (int)mb->pkt.nb_segs);
		if (ol_flags & PKT_RX_RSS_HASH) {
			printf(" - RSS hash=0x%x", (unsigned) mb->pkt.hash.rss);
			printf(" - RSS queue=0x%x",(unsigned) fs->rx_queue);
		}
		else if (ol_flags & PKT_RX_FDIR)
			printf(" - FDIR hash=0x%x - FDIR id=0x%x ",
			       mb->pkt.hash.fdir.hash, mb->pkt.hash.fdir.id);
		if (ol_flags & PKT_RX_VLAN_PKT)
			printf(" - VLAN tci=0x%x",
				mb->pkt.vlan_macip.f.vlan_tci);
		printf("\n");
		if (ol_flags != 0) {
			int rxf;

			for (rxf = 0; rxf < MAX_PKT_RX_FLAGS; rxf++) {
				if (ol_flags & (1 << rxf))
					printf("  PKT_RX_%s\n",
					       pkt_rx_flag_names[rxf]);
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
