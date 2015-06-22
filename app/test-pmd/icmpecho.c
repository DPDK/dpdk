/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2013 6WIND
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_arp.h>
#include <rte_ip.h>
#include <rte_icmp.h>
#include <rte_string_fns.h>

#include "testpmd.h"

static const char *
arp_op_name(uint16_t arp_op)
{
	switch (arp_op ) {
	case ARP_OP_REQUEST:
		return "ARP Request";
	case ARP_OP_REPLY:
		return "ARP Reply";
	case ARP_OP_REVREQUEST:
		return "Reverse ARP Request";
	case ARP_OP_REVREPLY:
		return "Reverse ARP Reply";
	case ARP_OP_INVREQUEST:
		return "Peer Identify Request";
	case ARP_OP_INVREPLY:
		return "Peer Identify Reply";
	default:
		break;
	}
	return "Unkwown ARP op";
}

static const char *
ip_proto_name(uint16_t ip_proto)
{
	static const char * ip_proto_names[] = {
		"IP6HOPOPTS", /**< IP6 hop-by-hop options */
		"ICMP",       /**< control message protocol */
		"IGMP",       /**< group mgmt protocol */
		"GGP",        /**< gateway^2 (deprecated) */
		"IPv4",       /**< IPv4 encapsulation */

		"UNASSIGNED",
		"TCP",        /**< transport control protocol */
		"ST",         /**< Stream protocol II */
		"EGP",        /**< exterior gateway protocol */
		"PIGP",       /**< private interior gateway */

		"RCC_MON",    /**< BBN RCC Monitoring */
		"NVPII",      /**< network voice protocol*/
		"PUP",        /**< pup */
		"ARGUS",      /**< Argus */
		"EMCON",      /**< EMCON */

		"XNET",       /**< Cross Net Debugger */
		"CHAOS",      /**< Chaos*/
		"UDP",        /**< user datagram protocol */
		"MUX",        /**< Multiplexing */
		"DCN_MEAS",   /**< DCN Measurement Subsystems */

		"HMP",        /**< Host Monitoring */
		"PRM",        /**< Packet Radio Measurement */
		"XNS_IDP",    /**< xns idp */
		"TRUNK1",     /**< Trunk-1 */
		"TRUNK2",     /**< Trunk-2 */

		"LEAF1",      /**< Leaf-1 */
		"LEAF2",      /**< Leaf-2 */
		"RDP",        /**< Reliable Data */
		"IRTP",       /**< Reliable Transaction */
		"TP4",        /**< tp-4 w/ class negotiation */

		"BLT",        /**< Bulk Data Transfer */
		"NSP",        /**< Network Services */
		"INP",        /**< Merit Internodal */
		"SEP",        /**< Sequential Exchange */
		"3PC",        /**< Third Party Connect */

		"IDPR",       /**< InterDomain Policy Routing */
		"XTP",        /**< XTP */
		"DDP",        /**< Datagram Delivery */
		"CMTP",       /**< Control Message Transport */
		"TPXX",       /**< TP++ Transport */

		"ILTP",       /**< IL transport protocol */
		"IPv6_HDR",   /**< IP6 header */
		"SDRP",       /**< Source Demand Routing */
		"IPv6_RTG",   /**< IP6 routing header */
		"IPv6_FRAG",  /**< IP6 fragmentation header */

		"IDRP",       /**< InterDomain Routing*/
		"RSVP",       /**< resource reservation */
		"GRE",        /**< General Routing Encap. */
		"MHRP",       /**< Mobile Host Routing */
		"BHA",        /**< BHA */

		"ESP",        /**< IP6 Encap Sec. Payload */
		"AH",         /**< IP6 Auth Header */
		"INLSP",      /**< Integ. Net Layer Security */
		"SWIPE",      /**< IP with encryption */
		"NHRP",       /**< Next Hop Resolution */

		"UNASSIGNED",
		"UNASSIGNED",
		"UNASSIGNED",
		"ICMPv6",     /**< ICMP6 */
		"IPv6NONEXT", /**< IP6 no next header */

		"Ipv6DSTOPTS",/**< IP6 destination option */
		"AHIP",       /**< any host internal protocol */
		"CFTP",       /**< CFTP */
		"HELLO",      /**< "hello" routing protocol */
		"SATEXPAK",   /**< SATNET/Backroom EXPAK */

		"KRYPTOLAN",  /**< Kryptolan */
		"RVD",        /**< Remote Virtual Disk */
		"IPPC",       /**< Pluribus Packet Core */
		"ADFS",       /**< Any distributed FS */
		"SATMON",     /**< Satnet Monitoring */

		"VISA",       /**< VISA Protocol */
		"IPCV",       /**< Packet Core Utility */
		"CPNX",       /**< Comp. Prot. Net. Executive */
		"CPHB",       /**< Comp. Prot. HeartBeat */
		"WSN",        /**< Wang Span Network */

		"PVP",        /**< Packet Video Protocol */
		"BRSATMON",   /**< BackRoom SATNET Monitoring */
		"ND",         /**< Sun net disk proto (temp.) */
		"WBMON",      /**< WIDEBAND Monitoring */
		"WBEXPAK",    /**< WIDEBAND EXPAK */

		"EON",        /**< ISO cnlp */
		"VMTP",       /**< VMTP */
		"SVMTP",      /**< Secure VMTP */
		"VINES",      /**< Banyon VINES */
		"TTP",        /**< TTP */

		"IGP",        /**< NSFNET-IGP */
		"DGP",        /**< dissimilar gateway prot. */
		"TCF",        /**< TCF */
		"IGRP",       /**< Cisco/GXS IGRP */
		"OSPFIGP",    /**< OSPFIGP */

		"SRPC",       /**< Strite RPC protocol */
		"LARP",       /**< Locus Address Resoloution */
		"MTP",        /**< Multicast Transport */
		"AX25",       /**< AX.25 Frames */
		"4IN4",       /**< IP encapsulated in IP */

		"MICP",       /**< Mobile Int.ing control */
		"SCCSP",      /**< Semaphore Comm. security */
		"ETHERIP",    /**< Ethernet IP encapsulation */
		"ENCAP",      /**< encapsulation header */
		"AES",        /**< any private encr. scheme */

		"GMTP",       /**< GMTP */
		"IPCOMP",     /**< payload compression (IPComp) */
		"UNASSIGNED",
		"UNASSIGNED",
		"PIM",        /**< Protocol Independent Mcast */
	};

	if (ip_proto < sizeof(ip_proto_names) / sizeof(ip_proto_names[0]))
		return ip_proto_names[ip_proto];
	switch (ip_proto) {
#ifdef IPPROTO_PGM
	case IPPROTO_PGM:  /**< PGM */
		return "PGM";
#endif
	case IPPROTO_SCTP:  /**< Stream Control Transport Protocol */
		return "SCTP";
#ifdef IPPROTO_DIVERT
	case IPPROTO_DIVERT: /**< divert pseudo-protocol */
		return "DIVERT";
#endif
	case IPPROTO_RAW: /**< raw IP packet */
		return "RAW";
	default:
		break;
	}
	return "UNASSIGNED";
}

static void
ipv4_addr_to_dot(uint32_t be_ipv4_addr, char *buf)
{
	uint32_t ipv4_addr;

	ipv4_addr = rte_be_to_cpu_32(be_ipv4_addr);
	sprintf(buf, "%d.%d.%d.%d", (ipv4_addr >> 24) & 0xFF,
		(ipv4_addr >> 16) & 0xFF, (ipv4_addr >> 8) & 0xFF,
		ipv4_addr & 0xFF);
}

static void
ether_addr_dump(const char *what, const struct ether_addr *ea)
{
	char buf[ETHER_ADDR_FMT_SIZE];

	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, ea);
	if (what)
		printf("%s", what);
	printf("%s", buf);
}

static void
ipv4_addr_dump(const char *what, uint32_t be_ipv4_addr)
{
	char buf[16];

	ipv4_addr_to_dot(be_ipv4_addr, buf);
	if (what)
		printf("%s", what);
	printf("%s", buf);
}

static uint16_t
ipv4_hdr_cksum(struct ipv4_hdr *ip_h)
{
	uint16_t *v16_h;
	uint32_t ip_cksum;

	/*
	 * Compute the sum of successive 16-bit words of the IPv4 header,
	 * skipping the checksum field of the header.
	 */
	v16_h = (unaligned_uint16_t *) ip_h;
	ip_cksum = v16_h[0] + v16_h[1] + v16_h[2] + v16_h[3] +
		v16_h[4] + v16_h[6] + v16_h[7] + v16_h[8] + v16_h[9];

	/* reduce 32 bit checksum to 16 bits and complement it */
	ip_cksum = (ip_cksum & 0xffff) + (ip_cksum >> 16);
	ip_cksum = (ip_cksum & 0xffff) + (ip_cksum >> 16);
	ip_cksum = (~ip_cksum) & 0x0000FFFF;
	return (ip_cksum == 0) ? 0xFFFF : (uint16_t) ip_cksum;
}

#define is_multicast_ipv4_addr(ipv4_addr) \
	(((rte_be_to_cpu_32((ipv4_addr)) >> 24) & 0x000000FF) == 0xE0)

/*
 * Receive a burst of packets, lookup for ICMP echo requets, and, if any,
 * send back ICMP echo replies.
 */
static void
reply_to_icmp_echo_rqsts(struct fwd_stream *fs)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *pkt;
	struct ether_hdr *eth_h;
	struct vlan_hdr *vlan_h;
	struct arp_hdr  *arp_h;
	struct ipv4_hdr *ip_h;
	struct icmp_hdr *icmp_h;
	struct ether_addr eth_addr;
	uint32_t ip_addr;
	uint16_t nb_rx;
	uint16_t nb_tx;
	uint16_t nb_replies;
	uint16_t eth_type;
	uint16_t vlan_id;
	uint16_t arp_op;
	uint16_t arp_pro;
	uint32_t cksum;
	uint8_t  i;
	int l2_len;
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	uint64_t start_tsc;
	uint64_t end_tsc;
	uint64_t core_cycles;
#endif

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	start_tsc = rte_rdtsc();
#endif

	/*
	 * First, receive a burst of packets.
	 */
	nb_rx = rte_eth_rx_burst(fs->rx_port, fs->rx_queue, pkts_burst,
				 nb_pkt_per_burst);
	if (unlikely(nb_rx == 0))
		return;

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	fs->rx_burst_stats.pkt_burst_spread[nb_rx]++;
#endif
	fs->rx_packets += nb_rx;
	nb_replies = 0;
	for (i = 0; i < nb_rx; i++) {
		pkt = pkts_burst[i];
		eth_h = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
		eth_type = RTE_BE_TO_CPU_16(eth_h->ether_type);
		l2_len = sizeof(struct ether_hdr);
		if (verbose_level > 0) {
			printf("\nPort %d pkt-len=%u nb-segs=%u\n",
			       fs->rx_port, pkt->pkt_len, pkt->nb_segs);
			ether_addr_dump("  ETH:  src=", &eth_h->s_addr);
			ether_addr_dump(" dst=", &eth_h->d_addr);
		}
		if (eth_type == ETHER_TYPE_VLAN) {
			vlan_h = (struct vlan_hdr *)
				((char *)eth_h + sizeof(struct ether_hdr));
			l2_len  += sizeof(struct vlan_hdr);
			eth_type = rte_be_to_cpu_16(vlan_h->eth_proto);
			if (verbose_level > 0) {
				vlan_id = rte_be_to_cpu_16(vlan_h->vlan_tci)
					& 0xFFF;
				printf(" [vlan id=%u]", vlan_id);
			}
		}
		if (verbose_level > 0) {
			printf(" type=0x%04x\n", eth_type);
		}

		/* Reply to ARP requests */
		if (eth_type == ETHER_TYPE_ARP) {
			arp_h = (struct arp_hdr *) ((char *)eth_h + l2_len);
			arp_op = RTE_BE_TO_CPU_16(arp_h->arp_op);
			arp_pro = RTE_BE_TO_CPU_16(arp_h->arp_pro);
			if (verbose_level > 0) {
				printf("  ARP:  hrd=%d proto=0x%04x hln=%d "
				       "pln=%d op=%u (%s)\n",
				       RTE_BE_TO_CPU_16(arp_h->arp_hrd),
				       arp_pro, arp_h->arp_hln,
				       arp_h->arp_pln, arp_op,
				       arp_op_name(arp_op));
			}
			if ((RTE_BE_TO_CPU_16(arp_h->arp_hrd) !=
			     ARP_HRD_ETHER) ||
			    (arp_pro != ETHER_TYPE_IPv4) ||
			    (arp_h->arp_hln != 6) ||
			    (arp_h->arp_pln != 4)
			    ) {
				rte_pktmbuf_free(pkt);
				if (verbose_level > 0)
					printf("\n");
				continue;
			}
			if (verbose_level > 0) {
				ether_addr_copy(&arp_h->arp_data.arp_sha, &eth_addr);
				ether_addr_dump("        sha=", &eth_addr);
				ip_addr = arp_h->arp_data.arp_sip;
				ipv4_addr_dump(" sip=", ip_addr);
				printf("\n");
				ether_addr_copy(&arp_h->arp_data.arp_tha, &eth_addr);
				ether_addr_dump("        tha=", &eth_addr);
				ip_addr = arp_h->arp_data.arp_tip;
				ipv4_addr_dump(" tip=", ip_addr);
				printf("\n");
			}
			if (arp_op != ARP_OP_REQUEST) {
				rte_pktmbuf_free(pkt);
				continue;
			}

			/*
			 * Build ARP reply.
			 */

			/* Use source MAC address as destination MAC address. */
			ether_addr_copy(&eth_h->s_addr, &eth_h->d_addr);
			/* Set source MAC address with MAC address of TX port */
			ether_addr_copy(&ports[fs->tx_port].eth_addr,
					&eth_h->s_addr);

			arp_h->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);
			ether_addr_copy(&arp_h->arp_data.arp_tha, &eth_addr);
			ether_addr_copy(&arp_h->arp_data.arp_sha, &arp_h->arp_data.arp_tha);
			ether_addr_copy(&eth_h->s_addr, &arp_h->arp_data.arp_sha);

			/* Swap IP addresses in ARP payload */
			ip_addr = arp_h->arp_data.arp_sip;
			arp_h->arp_data.arp_sip = arp_h->arp_data.arp_tip;
			arp_h->arp_data.arp_tip = ip_addr;
			pkts_burst[nb_replies++] = pkt;
			continue;
		}

		if (eth_type != ETHER_TYPE_IPv4) {
			rte_pktmbuf_free(pkt);
			continue;
		}
		ip_h = (struct ipv4_hdr *) ((char *)eth_h + l2_len);
		if (verbose_level > 0) {
			ipv4_addr_dump("  IPV4: src=", ip_h->src_addr);
			ipv4_addr_dump(" dst=", ip_h->dst_addr);
			printf(" proto=%d (%s)\n",
			       ip_h->next_proto_id,
			       ip_proto_name(ip_h->next_proto_id));
		}

		/*
		 * Check if packet is a ICMP echo request.
		 */
		icmp_h = (struct icmp_hdr *) ((char *)ip_h +
					      sizeof(struct ipv4_hdr));
		if (! ((ip_h->next_proto_id == IPPROTO_ICMP) &&
		       (icmp_h->icmp_type == IP_ICMP_ECHO_REQUEST) &&
		       (icmp_h->icmp_code == 0))) {
			rte_pktmbuf_free(pkt);
			continue;
		}

		if (verbose_level > 0)
			printf("  ICMP: echo request seq id=%d\n",
			       rte_be_to_cpu_16(icmp_h->icmp_seq_nb));

		/*
		 * Prepare ICMP echo reply to be sent back.
		 * - switch ethernet source and destinations addresses,
		 * - use the request IP source address as the reply IP
		 *    destination address,
		 * - if the request IP destination address is a multicast
		 *   address:
		 *     - choose a reply IP source address different from the
		 *       request IP source address,
		 *     - re-compute the IP header checksum.
		 *   Otherwise:
		 *     - switch the request IP source and destination
		 *       addresses in the reply IP header,
		 *     - keep the IP header checksum unchanged.
		 * - set IP_ICMP_ECHO_REPLY in ICMP header.
		 * ICMP checksum is computed by assuming it is valid in the
		 * echo request and not verified.
		 */
		ether_addr_copy(&eth_h->s_addr, &eth_addr);
		ether_addr_copy(&eth_h->d_addr, &eth_h->s_addr);
		ether_addr_copy(&eth_addr, &eth_h->d_addr);
		ip_addr = ip_h->src_addr;
		if (is_multicast_ipv4_addr(ip_h->dst_addr)) {
			uint32_t ip_src;

			ip_src = rte_be_to_cpu_32(ip_addr);
			if ((ip_src & 0x00000003) == 1)
				ip_src = (ip_src & 0xFFFFFFFC) | 0x00000002;
			else
				ip_src = (ip_src & 0xFFFFFFFC) | 0x00000001;
			ip_h->src_addr = rte_cpu_to_be_32(ip_src);
			ip_h->dst_addr = ip_addr;
			ip_h->hdr_checksum = ipv4_hdr_cksum(ip_h);
		} else {
			ip_h->src_addr = ip_h->dst_addr;
			ip_h->dst_addr = ip_addr;
		}
		icmp_h->icmp_type = IP_ICMP_ECHO_REPLY;
		cksum = ~icmp_h->icmp_cksum & 0xffff;
		cksum += ~htons(IP_ICMP_ECHO_REQUEST << 8) & 0xffff;
		cksum += htons(IP_ICMP_ECHO_REPLY << 8);
		cksum = (cksum & 0xffff) + (cksum >> 16);
		cksum = (cksum & 0xffff) + (cksum >> 16);
		icmp_h->icmp_cksum = ~cksum;
		pkts_burst[nb_replies++] = pkt;
	}

	/* Send back ICMP echo replies, if any. */
	if (nb_replies > 0) {
		nb_tx = rte_eth_tx_burst(fs->tx_port, fs->tx_queue, pkts_burst,
					 nb_replies);
		fs->tx_packets += nb_tx;
#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
		fs->tx_burst_stats.pkt_burst_spread[nb_tx]++;
#endif
		if (unlikely(nb_tx < nb_replies)) {
			fs->fwd_dropped += (nb_replies - nb_tx);
			do {
				rte_pktmbuf_free(pkts_burst[nb_tx]);
			} while (++nb_tx < nb_replies);
		}
	}

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	end_tsc = rte_rdtsc();
	core_cycles = (end_tsc - start_tsc);
	fs->core_cycles = (uint64_t) (fs->core_cycles + core_cycles);
#endif
}

struct fwd_engine icmp_echo_engine = {
	.fwd_mode_name  = "icmpecho",
	.port_fwd_begin = NULL,
	.port_fwd_end   = NULL,
	.packet_fwd     = reply_to_icmp_echo_rqsts,
};
