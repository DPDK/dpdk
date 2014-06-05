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
#include <rte_memcpy.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_sctp.h>
#include <rte_prefetch.h>
#include <rte_string_fns.h>
#include "testpmd.h"



#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)

static inline uint16_t
get_16b_sum(uint16_t *ptr16, uint32_t nr)
{
	uint32_t sum = 0;
	while (nr > 1)
	{
		sum +=*ptr16;
		nr -= sizeof(uint16_t);
		ptr16++;
		if (sum > UINT16_MAX)
			sum -= UINT16_MAX;
	}

	/* If length is in odd bytes */
	if (nr)
		sum += *((uint8_t*)ptr16);

	sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
	sum &= 0x0ffff;
	return (uint16_t)sum;
}

static inline uint16_t
get_ipv4_cksum(struct ipv4_hdr *ipv4_hdr)
{
	uint16_t cksum;
	cksum = get_16b_sum((uint16_t*)ipv4_hdr, sizeof(struct ipv4_hdr));
	return (uint16_t)((cksum == 0xffff)?cksum:~cksum);
}


static inline uint16_t
get_ipv4_psd_sum (struct ipv4_hdr * ip_hdr)
{
	/* Pseudo Header for IPv4/UDP/TCP checksum */
	union ipv4_psd_header {
		struct {
			uint32_t src_addr; /* IP address of source host. */
			uint32_t dst_addr; /* IP address of destination host(s). */
			uint8_t  zero;     /* zero. */
			uint8_t  proto;    /* L4 protocol type. */
			uint16_t len;      /* L4 length. */
		} __attribute__((__packed__));
		uint16_t u16_arr[0];
	} psd_hdr;

	psd_hdr.src_addr = ip_hdr->src_addr;
	psd_hdr.dst_addr = ip_hdr->dst_addr;
	psd_hdr.zero     = 0;
	psd_hdr.proto    = ip_hdr->next_proto_id;
	psd_hdr.len      = rte_cpu_to_be_16((uint16_t)(rte_be_to_cpu_16(ip_hdr->total_length)
				- sizeof(struct ipv4_hdr)));
	return get_16b_sum(psd_hdr.u16_arr, sizeof(psd_hdr));
}

static inline uint16_t
get_ipv6_psd_sum (struct ipv6_hdr * ip_hdr)
{
	/* Pseudo Header for IPv6/UDP/TCP checksum */
	union ipv6_psd_header {
		struct {
			uint8_t src_addr[16]; /* IP address of source host. */
			uint8_t dst_addr[16]; /* IP address of destination host(s). */
			uint32_t len;         /* L4 length. */
			uint32_t proto;       /* L4 protocol - top 3 bytes must be zero */
		} __attribute__((__packed__));

		uint16_t u16_arr[0]; /* allow use as 16-bit values with safe aliasing */
	} psd_hdr;

	rte_memcpy(&psd_hdr.src_addr, ip_hdr->src_addr,
			sizeof(ip_hdr->src_addr) + sizeof(ip_hdr->dst_addr));
	psd_hdr.len       = ip_hdr->payload_len;
	psd_hdr.proto     = (ip_hdr->proto << 24);

	return get_16b_sum(psd_hdr.u16_arr, sizeof(psd_hdr));
}

static inline uint16_t
get_ipv4_udptcp_checksum(struct ipv4_hdr *ipv4_hdr, uint16_t *l4_hdr)
{
	uint32_t cksum;
	uint32_t l4_len;

	l4_len = rte_be_to_cpu_16(ipv4_hdr->total_length) - sizeof(struct ipv4_hdr);

	cksum = get_16b_sum(l4_hdr, l4_len);
	cksum += get_ipv4_psd_sum(ipv4_hdr);

	cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);
	cksum = (~cksum) & 0xffff;
	if (cksum == 0)
		cksum = 0xffff;
	return (uint16_t)cksum;

}

static inline uint16_t
get_ipv6_udptcp_checksum(struct ipv6_hdr *ipv6_hdr, uint16_t *l4_hdr)
{
	uint32_t cksum;
	uint32_t l4_len;

	l4_len = rte_be_to_cpu_16(ipv6_hdr->payload_len);

	cksum = get_16b_sum(l4_hdr, l4_len);
	cksum += get_ipv6_psd_sum(ipv6_hdr);

	cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);
	cksum = (~cksum) & 0xffff;
	if (cksum == 0)
		cksum = 0xffff;

	return (uint16_t)cksum;
}


/*
 * Forwarding of packets. Change the checksum field with HW or SW methods
 * The HW/SW method selection depends on the ol_flags on every packet
 */
static void
pkt_burst_checksum_forward(struct fwd_stream *fs)
{
	struct rte_mbuf  *pkts_burst[MAX_PKT_BURST];
	struct rte_port  *txp;
	struct rte_mbuf  *mb;
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr  *ipv4_hdr;
	struct ipv6_hdr  *ipv6_hdr;
	struct udp_hdr   *udp_hdr;
	struct tcp_hdr   *tcp_hdr;
	struct sctp_hdr  *sctp_hdr;

	uint16_t nb_rx;
	uint16_t nb_tx;
	uint16_t i;
	uint16_t ol_flags;
	uint16_t pkt_ol_flags;
	uint16_t tx_ol_flags;
	uint16_t l4_proto;
	uint16_t eth_type;
	uint8_t  l2_len;
	uint8_t  l3_len;

	uint32_t rx_bad_ip_csum;
	uint32_t rx_bad_l4_csum;

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	uint64_t start_tsc;
	uint64_t end_tsc;
	uint64_t core_cycles;
#endif

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	start_tsc = rte_rdtsc();
#endif

	/*
	 * Receive a burst of packets and forward them.
	 */
	nb_rx = rte_eth_rx_burst(fs->rx_port, fs->rx_queue, pkts_burst,
				 nb_pkt_per_burst);
	if (unlikely(nb_rx == 0))
		return;

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	fs->rx_burst_stats.pkt_burst_spread[nb_rx]++;
#endif
	fs->rx_packets += nb_rx;
	rx_bad_ip_csum = 0;
	rx_bad_l4_csum = 0;

	txp = &ports[fs->tx_port];
	tx_ol_flags = txp->tx_ol_flags;

	for (i = 0; i < nb_rx; i++) {

		mb = pkts_burst[i];
		l2_len  = sizeof(struct ether_hdr);
		pkt_ol_flags = mb->ol_flags;
		ol_flags = (uint16_t) (pkt_ol_flags & (~PKT_TX_L4_MASK));

		eth_hdr = (struct ether_hdr *) mb->pkt.data;
		eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);
		if (eth_type == ETHER_TYPE_VLAN) {
			/* Only allow single VLAN label here */
			l2_len  += sizeof(struct vlan_hdr);
			 eth_type = rte_be_to_cpu_16(*(uint16_t *)
				((uintptr_t)&eth_hdr->ether_type +
				sizeof(struct vlan_hdr)));
		}

		/* Update the L3/L4 checksum error packet count  */
		rx_bad_ip_csum += (uint16_t) ((pkt_ol_flags & PKT_RX_IP_CKSUM_BAD) != 0);
		rx_bad_l4_csum += (uint16_t) ((pkt_ol_flags & PKT_RX_L4_CKSUM_BAD) != 0);

		/*
		 * Try to figure out L3 packet type by SW.
		 */
		if ((pkt_ol_flags & (PKT_RX_IPV4_HDR | PKT_RX_IPV4_HDR_EXT |
				PKT_RX_IPV6_HDR | PKT_RX_IPV6_HDR_EXT)) == 0) {
			if (eth_type == ETHER_TYPE_IPv4)
				pkt_ol_flags |= PKT_RX_IPV4_HDR;
			else if (eth_type == ETHER_TYPE_IPv6)
				pkt_ol_flags |= PKT_RX_IPV6_HDR;
		}

		/*
		 * Simplify the protocol parsing
		 * Assuming the incoming packets format as
		 *      Ethernet2 + optional single VLAN
		 *      + ipv4 or ipv6
		 *      + udp or tcp or sctp or others
		 */
		if (pkt_ol_flags & PKT_RX_IPV4_HDR) {

			/* Do not support ipv4 option field */
			l3_len = sizeof(struct ipv4_hdr) ;

			ipv4_hdr = (struct ipv4_hdr *) (rte_pktmbuf_mtod(mb,
					unsigned char *) + l2_len);

			l4_proto = ipv4_hdr->next_proto_id;

			/* Do not delete, this is required by HW*/
			ipv4_hdr->hdr_checksum = 0;

			if (tx_ol_flags & 0x1) {
				/* HW checksum */
				ol_flags |= PKT_TX_IP_CKSUM;
			}
			else {
				ol_flags |= PKT_TX_IPV4;
				/* SW checksum calculation */
				ipv4_hdr->src_addr++;
				ipv4_hdr->hdr_checksum = get_ipv4_cksum(ipv4_hdr);
			}

			if (l4_proto == IPPROTO_UDP) {
				udp_hdr = (struct udp_hdr*) (rte_pktmbuf_mtod(mb,
						unsigned char *) + l2_len + l3_len);
				if (tx_ol_flags & 0x2) {
					/* HW Offload */
					ol_flags |= PKT_TX_UDP_CKSUM;
					/* Pseudo header sum need be set properly */
					udp_hdr->dgram_cksum = get_ipv4_psd_sum(ipv4_hdr);
				}
				else {
					/* SW Implementation, clear checksum field first */
					udp_hdr->dgram_cksum = 0;
					udp_hdr->dgram_cksum = get_ipv4_udptcp_checksum(ipv4_hdr,
							(uint16_t*)udp_hdr);
				}
			}
			else if (l4_proto == IPPROTO_TCP){
				tcp_hdr = (struct tcp_hdr*) (rte_pktmbuf_mtod(mb,
						unsigned char *) + l2_len + l3_len);
				if (tx_ol_flags & 0x4) {
					ol_flags |= PKT_TX_TCP_CKSUM;
					tcp_hdr->cksum = get_ipv4_psd_sum(ipv4_hdr);
				}
				else {
					tcp_hdr->cksum = 0;
					tcp_hdr->cksum = get_ipv4_udptcp_checksum(ipv4_hdr,
							(uint16_t*)tcp_hdr);
				}
			}
			else if (l4_proto == IPPROTO_SCTP) {
				sctp_hdr = (struct sctp_hdr*) (rte_pktmbuf_mtod(mb,
						unsigned char *) + l2_len + l3_len);

				if (tx_ol_flags & 0x8) {
					ol_flags |= PKT_TX_SCTP_CKSUM;
					sctp_hdr->cksum = 0;

					/* Sanity check, only number of 4 bytes supported */
					if ((rte_be_to_cpu_16(ipv4_hdr->total_length) % 4) != 0)
						printf("sctp payload must be a multiple "
							"of 4 bytes for checksum offload");
				}
				else {
					sctp_hdr->cksum = 0;
					/* CRC32c sample code available in RFC3309 */
				}
			}
			/* End of L4 Handling*/
		}
		else if (pkt_ol_flags & PKT_RX_IPV6_HDR) {

			ipv6_hdr = (struct ipv6_hdr *) (rte_pktmbuf_mtod(mb,
					unsigned char *) + l2_len);
			l3_len = sizeof(struct ipv6_hdr) ;
			l4_proto = ipv6_hdr->proto;
			ol_flags |= PKT_TX_IPV6;

			if (l4_proto == IPPROTO_UDP) {
				udp_hdr = (struct udp_hdr*) (rte_pktmbuf_mtod(mb,
						unsigned char *) + l2_len + l3_len);
				if (tx_ol_flags & 0x2) {
					/* HW Offload */
					ol_flags |= PKT_TX_UDP_CKSUM;
					udp_hdr->dgram_cksum = get_ipv6_psd_sum(ipv6_hdr);
				}
				else {
					/* SW Implementation */
					/* checksum field need be clear first */
					udp_hdr->dgram_cksum = 0;
					udp_hdr->dgram_cksum = get_ipv6_udptcp_checksum(ipv6_hdr,
							(uint16_t*)udp_hdr);
				}
			}
			else if (l4_proto == IPPROTO_TCP) {
				tcp_hdr = (struct tcp_hdr*) (rte_pktmbuf_mtod(mb,
						unsigned char *) + l2_len + l3_len);
				if (tx_ol_flags & 0x4) {
					ol_flags |= PKT_TX_TCP_CKSUM;
					tcp_hdr->cksum = get_ipv6_psd_sum(ipv6_hdr);
				}
				else {
					tcp_hdr->cksum = 0;
					tcp_hdr->cksum = get_ipv6_udptcp_checksum(ipv6_hdr,
							(uint16_t*)tcp_hdr);
				}
			}
			else if (l4_proto == IPPROTO_SCTP) {
				sctp_hdr = (struct sctp_hdr*) (rte_pktmbuf_mtod(mb,
						unsigned char *) + l2_len + l3_len);

				if (tx_ol_flags & 0x8) {
					ol_flags |= PKT_TX_SCTP_CKSUM;
					sctp_hdr->cksum = 0;
					/* Sanity check, only number of 4 bytes supported by HW */
					if ((rte_be_to_cpu_16(ipv6_hdr->payload_len) % 4) != 0)
						printf("sctp payload must be a multiple "
							"of 4 bytes for checksum offload");
				}
				else {
					/* CRC32c sample code available in RFC3309 */
					sctp_hdr->cksum = 0;
				}
			} else {
				printf("Test flow control for 1G PMD \n");
			}
			/* End of L6 Handling*/
		}
		else {
			l3_len = 0;
			printf("Unhandled packet type: %#hx\n", eth_type);
		}

		/* Combine the packet header write. VLAN is not consider here */
		mb->pkt.vlan_macip.f.l2_len = l2_len;
		mb->pkt.vlan_macip.f.l3_len = l3_len;
		mb->ol_flags = ol_flags;
	}
	nb_tx = rte_eth_tx_burst(fs->tx_port, fs->tx_queue, pkts_burst, nb_rx);
	fs->tx_packets += nb_tx;
	fs->rx_bad_ip_csum += rx_bad_ip_csum;
	fs->rx_bad_l4_csum += rx_bad_l4_csum;

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	fs->tx_burst_stats.pkt_burst_spread[nb_tx]++;
#endif
	if (unlikely(nb_tx < nb_rx)) {
		fs->fwd_dropped += (nb_rx - nb_tx);
		do {
			rte_pktmbuf_free(pkts_burst[nb_tx]);
		} while (++nb_tx < nb_rx);
	}
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	end_tsc = rte_rdtsc();
	core_cycles = (end_tsc - start_tsc);
	fs->core_cycles = (uint64_t) (fs->core_cycles + core_cycles);
#endif
}


struct fwd_engine csum_fwd_engine = {
	.fwd_mode_name  = "csum",
	.port_fwd_begin = NULL,
	.port_fwd_end   = NULL,
	.packet_fwd     = pkt_burst_checksum_forward,
};

