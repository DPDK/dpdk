/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>
#include <rte_flow.h>

#include "testpmd.h"

#define UDP_SRC_PORT 1024
#define UDP_DST_PORT 1024

#define IP_SRC_ADDR ((192U << 24) | (168 << 16) | (0 << 8) | 1)
#define IP_DST_ADDR ((192U << 24) | (168 << 16) | (0 << 8) | 2)

#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)

static struct ipv4_hdr  pkt_ip_hdr;  /**< IP header of transmitted packets. */
static struct udp_hdr pkt_udp_hdr; /**< UDP header of transmitted packets. */

static void
copy_buf_to_pkt_segs(void* buf, unsigned len, struct rte_mbuf *pkt,
		     unsigned offset)
{
	struct rte_mbuf *seg;
	void *seg_buf;
	unsigned copy_len;

	seg = pkt;
	while (offset >= seg->data_len) {
		offset -= seg->data_len;
		seg = seg->next;
	}
	copy_len = seg->data_len - offset;
	seg_buf = rte_pktmbuf_mtod_offset(seg, char *, offset);
	while (len > copy_len) {
		rte_memcpy(seg_buf, buf, (size_t) copy_len);
		len -= copy_len;
		buf = ((char*) buf + copy_len);
		seg = seg->next;
		seg_buf = rte_pktmbuf_mtod(seg, char *);
		copy_len = seg->data_len;
	}
	rte_memcpy(seg_buf, buf, (size_t) len);
}

static inline void
copy_buf_to_pkt(void* buf, unsigned len, struct rte_mbuf *pkt, unsigned offset)
{
	if (offset + len <= pkt->data_len) {
		rte_memcpy(rte_pktmbuf_mtod_offset(pkt, char *, offset),
			buf, (size_t) len);
		return;
	}
	copy_buf_to_pkt_segs(buf, len, pkt, offset);
}

static void
setup_pkt_udp_ip_headers(struct ipv4_hdr *ip_hdr,
			 struct udp_hdr *udp_hdr,
			 uint16_t pkt_data_len)
{
	uint16_t *ptr16;
	uint32_t ip_cksum;
	uint16_t pkt_len;

	/*
	 * Initialize UDP header.
	 */
	pkt_len = (uint16_t) (pkt_data_len + sizeof(struct udp_hdr));
	udp_hdr->src_port = rte_cpu_to_be_16(UDP_SRC_PORT);
	udp_hdr->dst_port = rte_cpu_to_be_16(UDP_DST_PORT);
	udp_hdr->dgram_len      = RTE_CPU_TO_BE_16(pkt_len);
	udp_hdr->dgram_cksum    = 0; /* No UDP checksum. */

	/*
	 * Initialize IP header.
	 */
	pkt_len = (uint16_t) (pkt_len + sizeof(struct ipv4_hdr));
	ip_hdr->version_ihl   = IP_VHL_DEF;
	ip_hdr->type_of_service   = 0;
	ip_hdr->fragment_offset = 0;
	ip_hdr->time_to_live   = IP_DEFTTL;
	ip_hdr->next_proto_id = IPPROTO_UDP;
	ip_hdr->packet_id = 0;
	ip_hdr->total_length   = RTE_CPU_TO_BE_16(pkt_len);
	ip_hdr->src_addr = rte_cpu_to_be_32(IP_SRC_ADDR);
	ip_hdr->dst_addr = rte_cpu_to_be_32(IP_DST_ADDR);

	/*
	 * Compute IP header checksum.
	 */
	ptr16 = (unaligned_uint16_t*) ip_hdr;
	ip_cksum = 0;
	ip_cksum += ptr16[0]; ip_cksum += ptr16[1];
	ip_cksum += ptr16[2]; ip_cksum += ptr16[3];
	ip_cksum += ptr16[4];
	ip_cksum += ptr16[6]; ip_cksum += ptr16[7];
	ip_cksum += ptr16[8]; ip_cksum += ptr16[9];

	/*
	 * Reduce 32 bit checksum to 16 bits and complement it.
	 */
	ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) +
		(ip_cksum & 0x0000FFFF);
	if (ip_cksum > 65535)
		ip_cksum -= 65535;
	ip_cksum = (~ip_cksum) & 0x0000FFFF;
	if (ip_cksum == 0)
		ip_cksum = 0xFFFF;
	ip_hdr->hdr_checksum = (uint16_t) ip_cksum;
}

/*
 * Transmit a burst of multi-segments packets.
 */
static void
pkt_burst_transmit(struct fwd_stream *fs)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_port *txp;
	struct rte_mbuf *pkt;
	struct rte_mbuf *pkt_seg;
	struct rte_mempool *mbp;
	struct ether_hdr eth_hdr;
	uint16_t nb_tx;
	uint16_t nb_pkt;
	uint16_t vlan_tci, vlan_tci_outer;
	uint32_t retry;
	uint64_t ol_flags = 0;
	uint8_t  i;
	uint64_t tx_offloads;
#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	uint64_t start_tsc;
	uint64_t end_tsc;
	uint64_t core_cycles;
#endif
	uint32_t nb_segs, pkt_len;

#ifdef RTE_TEST_PMD_RECORD_CORE_CYCLES
	start_tsc = rte_rdtsc();
#endif

	mbp = current_fwd_lcore()->mbp;
	txp = &ports[fs->tx_port];
	tx_offloads = txp->dev_conf.txmode.offloads;
	vlan_tci = txp->tx_vlan_id;
	vlan_tci_outer = txp->tx_vlan_id_outer;
	if (tx_offloads	& DEV_TX_OFFLOAD_VLAN_INSERT)
		ol_flags = PKT_TX_VLAN_PKT;
	if (tx_offloads & DEV_TX_OFFLOAD_QINQ_INSERT)
		ol_flags |= PKT_TX_QINQ_PKT;
	if (tx_offloads & DEV_TX_OFFLOAD_MACSEC_INSERT)
		ol_flags |= PKT_TX_MACSEC;
	for (nb_pkt = 0; nb_pkt < nb_pkt_per_burst; nb_pkt++) {
		pkt = rte_mbuf_raw_alloc(mbp);
		if (pkt == NULL) {
		nomore_mbuf:
			if (nb_pkt == 0)
				return;
			break;
		}

		/*
		 * Using raw alloc is good to improve performance,
		 * but some consumers may use the headroom and so
		 * decrement data_off. We need to make sure it is
		 * reset to default value.
		 */
		rte_pktmbuf_reset_headroom(pkt);
		pkt->data_len = tx_pkt_seg_lengths[0];
		pkt_seg = pkt;
		if (tx_pkt_split == TX_PKT_SPLIT_RND)
			nb_segs = random() % tx_pkt_nb_segs + 1;
		else
			nb_segs = tx_pkt_nb_segs;
		pkt_len = pkt->data_len;
		for (i = 1; i < nb_segs; i++) {
			pkt_seg->next = rte_mbuf_raw_alloc(mbp);
			if (pkt_seg->next == NULL) {
				pkt->nb_segs = i;
				rte_pktmbuf_free(pkt);
				goto nomore_mbuf;
			}
			pkt_seg = pkt_seg->next;
			pkt_seg->data_len = tx_pkt_seg_lengths[i];
			pkt_len += pkt_seg->data_len;
		}
		pkt_seg->next = NULL; /* Last segment of packet. */

		/*
		 * Initialize Ethernet header.
		 */
		ether_addr_copy(&peer_eth_addrs[fs->peer_addr],&eth_hdr.d_addr);
		ether_addr_copy(&ports[fs->tx_port].eth_addr, &eth_hdr.s_addr);
		eth_hdr.ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

		/*
		 * Copy headers in first packet segment(s).
		 */
		copy_buf_to_pkt(&eth_hdr, sizeof(eth_hdr), pkt, 0);
		copy_buf_to_pkt(&pkt_ip_hdr, sizeof(pkt_ip_hdr), pkt,
				sizeof(struct ether_hdr));
		copy_buf_to_pkt(&pkt_udp_hdr, sizeof(pkt_udp_hdr), pkt,
				sizeof(struct ether_hdr) +
				sizeof(struct ipv4_hdr));

		/*
		 * Complete first mbuf of packet and append it to the
		 * burst of packets to be transmitted.
		 */
		pkt->nb_segs = nb_segs;
		pkt->pkt_len = pkt_len;
		pkt->ol_flags = ol_flags;
		pkt->vlan_tci = vlan_tci;
		pkt->vlan_tci_outer = vlan_tci_outer;
		pkt->l2_len = sizeof(struct ether_hdr);
		pkt->l3_len = sizeof(struct ipv4_hdr);
		pkts_burst[nb_pkt] = pkt;
	}
	nb_tx = rte_eth_tx_burst(fs->tx_port, fs->tx_queue, pkts_burst, nb_pkt);
	/*
	 * Retry if necessary
	 */
	if (unlikely(nb_tx < nb_pkt) && fs->retry_enabled) {
		retry = 0;
		while (nb_tx < nb_pkt && retry++ < burst_tx_retry_num) {
			rte_delay_us(burst_tx_delay_time);
			nb_tx += rte_eth_tx_burst(fs->tx_port, fs->tx_queue,
					&pkts_burst[nb_tx], nb_pkt - nb_tx);
		}
	}
	fs->tx_packets += nb_tx;

#ifdef RTE_TEST_PMD_RECORD_BURST_STATS
	fs->tx_burst_stats.pkt_burst_spread[nb_tx]++;
#endif
	if (unlikely(nb_tx < nb_pkt)) {
		if (verbose_level > 0 && fs->fwd_dropped == 0)
			printf("port %d tx_queue %d - drop "
			       "(nb_pkt:%u - nb_tx:%u)=%u packets\n",
			       fs->tx_port, fs->tx_queue,
			       (unsigned) nb_pkt, (unsigned) nb_tx,
			       (unsigned) (nb_pkt - nb_tx));
		fs->fwd_dropped += (nb_pkt - nb_tx);
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

static void
tx_only_begin(__attribute__((unused)) portid_t pi)
{
	uint16_t pkt_data_len;

	pkt_data_len = (uint16_t) (tx_pkt_length - (sizeof(struct ether_hdr) +
						    sizeof(struct ipv4_hdr) +
						    sizeof(struct udp_hdr)));
	setup_pkt_udp_ip_headers(&pkt_ip_hdr, &pkt_udp_hdr, pkt_data_len);
}

struct fwd_engine tx_only_engine = {
	.fwd_mode_name  = "txonly",
	.port_fwd_begin = tx_only_begin,
	.port_fwd_end   = NULL,
	.packet_fwd     = pkt_burst_transmit,
};
