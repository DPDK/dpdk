/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright 2014 6WIND S.A.
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

/* We cannot use rte_cpu_to_be_16() on a constant in a switch/case */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#define _htons(x) ((uint16_t)((((x) & 0x00ffU) << 8) | (((x) & 0xff00U) >> 8)))
#else
#define _htons(x) (x)
#endif

static uint16_t
get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags)
{
	if (ethertype == _htons(ETHER_TYPE_IPv4))
		return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
	else /* assume ethertype == ETHER_TYPE_IPv6 */
		return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}

static uint16_t
get_udptcp_checksum(void *l3_hdr, void *l4_hdr, uint16_t ethertype)
{
	if (ethertype == _htons(ETHER_TYPE_IPv4))
		return rte_ipv4_udptcp_cksum(l3_hdr, l4_hdr);
	else /* assume ethertype == ETHER_TYPE_IPv6 */
		return rte_ipv6_udptcp_cksum(l3_hdr, l4_hdr);
}

/*
 * Parse an ethernet header to fill the ethertype, l2_len, l3_len and
 * ipproto. This function is able to recognize IPv4/IPv6 with one optional vlan
 * header. The l4_len argument is only set in case of TCP (useful for TSO).
 */
static void
parse_ethernet(struct ether_hdr *eth_hdr, uint16_t *ethertype, uint16_t *l2_len,
	uint16_t *l3_len, uint8_t *l4_proto, uint16_t *l4_len)
{
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	struct tcp_hdr *tcp_hdr;

	*l2_len = sizeof(struct ether_hdr);
	*ethertype = eth_hdr->ether_type;

	if (*ethertype == _htons(ETHER_TYPE_VLAN)) {
		struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);

		*l2_len  += sizeof(struct vlan_hdr);
		*ethertype = vlan_hdr->eth_proto;
	}

	switch (*ethertype) {
	case _htons(ETHER_TYPE_IPv4):
		ipv4_hdr = (struct ipv4_hdr *) ((char *)eth_hdr + *l2_len);
		*l3_len = (ipv4_hdr->version_ihl & 0x0f) * 4;
		*l4_proto = ipv4_hdr->next_proto_id;
		break;
	case _htons(ETHER_TYPE_IPv6):
		ipv6_hdr = (struct ipv6_hdr *) ((char *)eth_hdr + *l2_len);
		*l3_len = sizeof(struct ipv6_hdr);
		*l4_proto = ipv6_hdr->proto;
		break;
	default:
		*l3_len = 0;
		*l4_proto = 0;
		break;
	}

	if (*l4_proto == IPPROTO_TCP) {
		tcp_hdr = (struct tcp_hdr *)((char *)eth_hdr +
			*l2_len + *l3_len);
		*l4_len = (tcp_hdr->data_off & 0xf0) >> 2;
	} else
		*l4_len = 0;
}

/* modify the IPv4 or IPv4 source address of a packet */
static void
change_ip_addresses(void *l3_hdr, uint16_t ethertype)
{
	struct ipv4_hdr *ipv4_hdr = l3_hdr;
	struct ipv6_hdr *ipv6_hdr = l3_hdr;

	if (ethertype == _htons(ETHER_TYPE_IPv4)) {
		ipv4_hdr->src_addr =
			rte_cpu_to_be_32(rte_be_to_cpu_32(ipv4_hdr->src_addr) + 1);
	} else if (ethertype == _htons(ETHER_TYPE_IPv6)) {
		ipv6_hdr->src_addr[15] = ipv6_hdr->src_addr[15] + 1;
	}
}

/* if possible, calculate the checksum of a packet in hw or sw,
 * depending on the testpmd command line configuration */
static uint64_t
process_inner_cksums(void *l3_hdr, uint16_t ethertype, uint16_t l3_len,
	uint8_t l4_proto, uint16_t tso_segsz, uint16_t testpmd_ol_flags)
{
	struct ipv4_hdr *ipv4_hdr = l3_hdr;
	struct udp_hdr *udp_hdr;
	struct tcp_hdr *tcp_hdr;
	struct sctp_hdr *sctp_hdr;
	uint64_t ol_flags = 0;

	if (ethertype == _htons(ETHER_TYPE_IPv4)) {
		ipv4_hdr = l3_hdr;
		ipv4_hdr->hdr_checksum = 0;

		if (tso_segsz != 0 && l4_proto == IPPROTO_TCP) {
			ol_flags |= PKT_TX_IP_CKSUM;
		} else {
			if (testpmd_ol_flags & TESTPMD_TX_OFFLOAD_IP_CKSUM)
				ol_flags |= PKT_TX_IP_CKSUM;
			else {
				ipv4_hdr->hdr_checksum =
					rte_ipv4_cksum(ipv4_hdr);
				ol_flags |= PKT_TX_IPV4;
			}
		}
	} else if (ethertype == _htons(ETHER_TYPE_IPv6))
		ol_flags |= PKT_TX_IPV6;
	else
		return 0; /* packet type not supported, nothing to do */

	if (l4_proto == IPPROTO_UDP) {
		udp_hdr = (struct udp_hdr *)((char *)l3_hdr + l3_len);
		/* do not recalculate udp cksum if it was 0 */
		if (udp_hdr->dgram_cksum != 0) {
			udp_hdr->dgram_cksum = 0;
			if (testpmd_ol_flags & TESTPMD_TX_OFFLOAD_UDP_CKSUM) {
				ol_flags |= PKT_TX_UDP_CKSUM;
				udp_hdr->dgram_cksum = get_psd_sum(l3_hdr,
					ethertype, ol_flags);
			} else {
				udp_hdr->dgram_cksum =
					get_udptcp_checksum(l3_hdr, udp_hdr,
						ethertype);
			}
		}
	} else if (l4_proto == IPPROTO_TCP) {
		tcp_hdr = (struct tcp_hdr *)((char *)l3_hdr + l3_len);
		tcp_hdr->cksum = 0;
		if (tso_segsz != 0) {
			ol_flags |= PKT_TX_TCP_SEG;
			tcp_hdr->cksum = get_psd_sum(l3_hdr, ethertype, ol_flags);
		} else if (testpmd_ol_flags & TESTPMD_TX_OFFLOAD_TCP_CKSUM) {
			ol_flags |= PKT_TX_TCP_CKSUM;
			tcp_hdr->cksum = get_psd_sum(l3_hdr, ethertype, ol_flags);
		} else {
			tcp_hdr->cksum =
				get_udptcp_checksum(l3_hdr, tcp_hdr, ethertype);
		}
	} else if (l4_proto == IPPROTO_SCTP) {
		sctp_hdr = (struct sctp_hdr *)((char *)l3_hdr + l3_len);
		sctp_hdr->cksum = 0;
		/* sctp payload must be a multiple of 4 to be
		 * offloaded */
		if ((testpmd_ol_flags & TESTPMD_TX_OFFLOAD_SCTP_CKSUM) &&
			((ipv4_hdr->total_length & 0x3) == 0)) {
			ol_flags |= PKT_TX_SCTP_CKSUM;
		} else {
			/* XXX implement CRC32c, example available in
			 * RFC3309 */
		}
	}

	return ol_flags;
}

/* Calculate the checksum of outer header (only vxlan is supported,
 * meaning IP + UDP). The caller already checked that it's a vxlan
 * packet */
static uint64_t
process_outer_cksums(void *outer_l3_hdr, uint16_t outer_ethertype,
	uint16_t outer_l3_len, uint16_t testpmd_ol_flags)
{
	struct ipv4_hdr *ipv4_hdr = outer_l3_hdr;
	struct ipv6_hdr *ipv6_hdr = outer_l3_hdr;
	struct udp_hdr *udp_hdr;
	uint64_t ol_flags = 0;

	if (testpmd_ol_flags & TESTPMD_TX_OFFLOAD_VXLAN_CKSUM)
		ol_flags |= PKT_TX_UDP_TUNNEL_PKT;

	if (outer_ethertype == _htons(ETHER_TYPE_IPv4)) {
		ipv4_hdr->hdr_checksum = 0;

		if (testpmd_ol_flags & TESTPMD_TX_OFFLOAD_VXLAN_CKSUM)
			ol_flags |= PKT_TX_OUTER_IP_CKSUM;
		else
			ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);
	} else if (testpmd_ol_flags & TESTPMD_TX_OFFLOAD_VXLAN_CKSUM)
		ol_flags |= PKT_TX_OUTER_IPV6;

	udp_hdr = (struct udp_hdr *)((char *)outer_l3_hdr + outer_l3_len);
	/* do not recalculate udp cksum if it was 0 */
	if (udp_hdr->dgram_cksum != 0) {
		udp_hdr->dgram_cksum = 0;
		if (outer_ethertype == _htons(ETHER_TYPE_IPv4))
			udp_hdr->dgram_cksum =
				rte_ipv4_udptcp_cksum(ipv4_hdr, udp_hdr);
		else
			udp_hdr->dgram_cksum =
				rte_ipv6_udptcp_cksum(ipv6_hdr, udp_hdr);
	}

	return ol_flags;
}

/*
 * Receive a burst of packets, and for each packet:
 *  - parse packet, and try to recognize a supported packet type (1)
 *  - if it's not a supported packet type, don't touch the packet, else:
 *  - modify the IPs in inner headers and in outer headers if any
 *  - reprocess the checksum of all supported layers. This is done in SW
 *    or HW, depending on testpmd command line configuration
 *  - if TSO is enabled in testpmd command line, also flag the mbuf for TCP
 *    segmentation offload (this implies HW TCP checksum)
 * Then transmit packets on the output port.
 *
 * (1) Supported packets are:
 *   Ether / (vlan) / IP|IP6 / UDP|TCP|SCTP .
 *   Ether / (vlan) / outer IP|IP6 / outer UDP / VxLAN / Ether / IP|IP6 /
 *           UDP|TCP|SCTP
 *
 * The testpmd command line for this forward engine sets the flags
 * TESTPMD_TX_OFFLOAD_* in ports[tx_port].tx_ol_flags. They control
 * wether a checksum must be calculated in software or in hardware. The
 * IP, UDP, TCP and SCTP flags always concern the inner layer.  The
 * VxLAN flag concerns the outer IP (if packet is recognized as a vxlan packet).
 */
static void
pkt_burst_checksum_forward(struct fwd_stream *fs)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_port *txp;
	struct rte_mbuf *m;
	struct ether_hdr *eth_hdr;
	void *l3_hdr = NULL, *outer_l3_hdr = NULL; /* can be IPv4 or IPv6 */
	struct udp_hdr *udp_hdr;
	uint16_t nb_rx;
	uint16_t nb_tx;
	uint16_t i;
	uint64_t ol_flags;
	uint16_t testpmd_ol_flags;
	uint8_t l4_proto, l4_tun_len = 0;
	uint16_t ethertype = 0, outer_ethertype = 0;
	uint16_t l2_len = 0, l3_len = 0, l4_len = 0;
	uint16_t outer_l2_len = 0, outer_l3_len = 0;
	uint16_t tso_segsz;
	int tunnel = 0;
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

	/* receive a burst of packet */
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
	testpmd_ol_flags = txp->tx_ol_flags;
	tso_segsz = txp->tso_segsz;

	for (i = 0; i < nb_rx; i++) {

		ol_flags = 0;
		tunnel = 0;
		l4_tun_len = 0;
		m = pkts_burst[i];

		/* Update the L3/L4 checksum error packet statistics */
		rx_bad_ip_csum += ((m->ol_flags & PKT_RX_IP_CKSUM_BAD) != 0);
		rx_bad_l4_csum += ((m->ol_flags & PKT_RX_L4_CKSUM_BAD) != 0);

		/* step 1: dissect packet, parsing optional vlan, ip4/ip6, vxlan
		 * and inner headers */

		eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
		parse_ethernet(eth_hdr, &ethertype, &l2_len, &l3_len,
			&l4_proto, &l4_len);
		l3_hdr = (char *)eth_hdr + l2_len;

		/* check if it's a supported tunnel (only vxlan for now) */
		if (l4_proto == IPPROTO_UDP) {
			udp_hdr = (struct udp_hdr *)((char *)l3_hdr + l3_len);

			/* check udp destination port, 4789 is the default
			 * vxlan port (rfc7348) */
			if (udp_hdr->dst_port == _htons(4789)) {
				l4_tun_len = ETHER_VXLAN_HLEN;
				tunnel = 1;

			/* currently, this flag is set by i40e only if the
			 * packet is vxlan */
			} else if (m->ol_flags & (PKT_RX_TUNNEL_IPV4_HDR |
					PKT_RX_TUNNEL_IPV6_HDR))
				tunnel = 1;

			if (tunnel == 1) {
				outer_ethertype = ethertype;
				outer_l2_len = l2_len;
				outer_l3_len = l3_len;
				outer_l3_hdr = l3_hdr;

				eth_hdr = (struct ether_hdr *)((char *)udp_hdr +
					sizeof(struct udp_hdr) +
					sizeof(struct vxlan_hdr));

				parse_ethernet(eth_hdr, &ethertype, &l2_len,
					&l3_len, &l4_proto, &l4_len);
				l3_hdr = (char *)eth_hdr + l2_len;
			}
		}

		/* step 2: change all source IPs (v4 or v6) so we need
		 * to recompute the chksums even if they were correct */

		change_ip_addresses(l3_hdr, ethertype);
		if (tunnel == 1)
			change_ip_addresses(outer_l3_hdr, outer_ethertype);

		/* step 3: depending on user command line configuration,
		 * recompute checksum either in software or flag the
		 * mbuf to offload the calculation to the NIC. If TSO
		 * is configured, prepare the mbuf for TCP segmentation. */

		/* process checksums of inner headers first */
		ol_flags |= process_inner_cksums(l3_hdr, ethertype,
			l3_len, l4_proto, tso_segsz, testpmd_ol_flags);

		/* Then process outer headers if any. Note that the software
		 * checksum will be wrong if one of the inner checksums is
		 * processed in hardware. */
		if (tunnel == 1) {
			ol_flags |= process_outer_cksums(outer_l3_hdr,
				outer_ethertype, outer_l3_len, testpmd_ol_flags);
		}

		/* step 4: fill the mbuf meta data (flags and header lengths) */

		if (tunnel == 1) {
			if (testpmd_ol_flags & TESTPMD_TX_OFFLOAD_VXLAN_CKSUM) {
				m->outer_l2_len = outer_l2_len;
				m->outer_l3_len = outer_l3_len;
				m->l2_len = l4_tun_len + l2_len;
				m->l3_len = l3_len;
			}
			else {
				/* if we don't do vxlan cksum in hw,
				   outer checksum will be wrong because
				   we changed the ip, but it shows that
				   we can process the inner header cksum
				   in the nic */
				m->l2_len = outer_l2_len + outer_l3_len +
					sizeof(struct udp_hdr) +
					sizeof(struct vxlan_hdr) + l2_len;
				m->l3_len = l3_len;
				m->l4_len = l4_len;
			}
		} else {
			/* this is only useful if an offload flag is
			 * set, but it does not hurt to fill it in any
			 * case */
			m->l2_len = l2_len;
			m->l3_len = l3_len;
			m->l4_len = l4_len;
		}
		m->tso_segsz = tso_segsz;
		m->ol_flags = ol_flags;

		/* if verbose mode is enabled, dump debug info */
		if (verbose_level > 0) {
			struct {
				uint64_t flag;
				uint64_t mask;
			} tx_flags[] = {
				{ PKT_TX_IP_CKSUM, PKT_TX_IP_CKSUM },
				{ PKT_TX_UDP_CKSUM, PKT_TX_L4_MASK },
				{ PKT_TX_TCP_CKSUM, PKT_TX_L4_MASK },
				{ PKT_TX_SCTP_CKSUM, PKT_TX_L4_MASK },
				{ PKT_TX_UDP_TUNNEL_PKT, PKT_TX_UDP_TUNNEL_PKT },
				{ PKT_TX_IPV4, PKT_TX_IPV4 },
				{ PKT_TX_IPV6, PKT_TX_IPV6 },
				{ PKT_TX_OUTER_IP_CKSUM, PKT_TX_OUTER_IP_CKSUM },
				{ PKT_TX_OUTER_IPV4, PKT_TX_OUTER_IPV4 },
				{ PKT_TX_OUTER_IPV6, PKT_TX_OUTER_IPV6 },
				{ PKT_TX_TCP_SEG, PKT_TX_TCP_SEG },
			};
			unsigned j;
			const char *name;

			printf("-----------------\n");
			/* dump rx parsed packet info */
			printf("rx: l2_len=%d ethertype=%x l3_len=%d "
				"l4_proto=%d l4_len=%d\n",
				l2_len, rte_be_to_cpu_16(ethertype),
				l3_len, l4_proto, l4_len);
			if (tunnel == 1)
				printf("rx: outer_l2_len=%d outer_ethertype=%x "
					"outer_l3_len=%d\n", outer_l2_len,
					rte_be_to_cpu_16(outer_ethertype),
					outer_l3_len);
			/* dump tx packet info */
			if ((testpmd_ol_flags & (TESTPMD_TX_OFFLOAD_IP_CKSUM |
						TESTPMD_TX_OFFLOAD_UDP_CKSUM |
						TESTPMD_TX_OFFLOAD_TCP_CKSUM |
						TESTPMD_TX_OFFLOAD_SCTP_CKSUM)) ||
				tso_segsz != 0)
				printf("tx: m->l2_len=%d m->l3_len=%d "
					"m->l4_len=%d\n",
					m->l2_len, m->l3_len, m->l4_len);
			if ((tunnel == 1) &&
				(testpmd_ol_flags & TESTPMD_TX_OFFLOAD_VXLAN_CKSUM))
				printf("tx: m->outer_l2_len=%d m->outer_l3_len=%d\n",
					m->outer_l2_len, m->outer_l3_len);
			if (tso_segsz != 0)
				printf("tx: m->tso_segsz=%d\n", m->tso_segsz);
			printf("tx: flags=");
			for (j = 0; j < sizeof(tx_flags)/sizeof(*tx_flags); j++) {
				name = rte_get_tx_ol_flag_name(tx_flags[j].flag);
				if ((m->ol_flags & tx_flags[j].mask) ==
					tx_flags[j].flag)
					printf("%s ", name);
			}
			printf("\n");
		}
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

