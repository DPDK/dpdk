/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016-2018 NXP
 *
 */

#include <time.h>
#include <net/if.h>

#include <rte_mbuf.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_dev.h>

#include <rte_fslmc.h>
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_dpio.h>
#include <dpaa2_hw_mempool.h>

#include "dpaa2_pmd_logs.h"
#include "dpaa2_ethdev.h"
#include "base/dpaa2_hw_dpni_annot.h"

static inline uint32_t __attribute__((hot))
dpaa2_dev_rx_parse_slow(struct rte_mbuf *mbuf,
			struct dpaa2_annot_hdr *annotation);

#define DPAA2_MBUF_TO_CONTIG_FD(_mbuf, _fd, _bpid)  do { \
	DPAA2_SET_FD_ADDR(_fd, DPAA2_MBUF_VADDR_TO_IOVA(_mbuf)); \
	DPAA2_SET_FD_LEN(_fd, _mbuf->data_len); \
	DPAA2_SET_ONLY_FD_BPID(_fd, _bpid); \
	DPAA2_SET_FD_OFFSET(_fd, _mbuf->data_off); \
	DPAA2_SET_FD_FRC(_fd, 0);		\
	DPAA2_RESET_FD_CTRL(_fd);		\
	DPAA2_RESET_FD_FLC(_fd);		\
} while (0)

static inline void __attribute__((hot))
dpaa2_dev_rx_parse_new(struct rte_mbuf *m, const struct qbman_fd *fd)
{
	struct dpaa2_annot_hdr *annotation;
	uint16_t frc = DPAA2_GET_FD_FRC_PARSE_SUM(fd);

	m->packet_type = RTE_PTYPE_UNKNOWN;
	switch (frc) {
	case DPAA2_PKT_TYPE_ETHER:
		m->packet_type = RTE_PTYPE_L2_ETHER;
		break;
	case DPAA2_PKT_TYPE_IPV4:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4;
		break;
	case DPAA2_PKT_TYPE_IPV6:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6;
		break;
	case DPAA2_PKT_TYPE_IPV4_EXT:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4_EXT;
		break;
	case DPAA2_PKT_TYPE_IPV6_EXT:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6_EXT;
		break;
	case DPAA2_PKT_TYPE_IPV4_TCP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP;
		break;
	case DPAA2_PKT_TYPE_IPV6_TCP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP;
		break;
	case DPAA2_PKT_TYPE_IPV4_UDP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP;
		break;
	case DPAA2_PKT_TYPE_IPV6_UDP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP;
		break;
	case DPAA2_PKT_TYPE_IPV4_SCTP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_SCTP;
		break;
	case DPAA2_PKT_TYPE_IPV6_SCTP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_SCTP;
		break;
	case DPAA2_PKT_TYPE_IPV4_ICMP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_ICMP;
		break;
	case DPAA2_PKT_TYPE_IPV6_ICMP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
			RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_ICMP;
		break;
	default:
		m->packet_type = dpaa2_dev_rx_parse_slow(m,
		  (void *)((size_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd))
			 + DPAA2_FD_PTA_SIZE));
	}
	m->hash.rss = fd->simple.flc_hi;
	m->ol_flags |= PKT_RX_RSS_HASH;

	if (dpaa2_enable_ts == PMD_DPAA2_ENABLE_TS) {
		annotation = (struct dpaa2_annot_hdr *)
			((size_t)DPAA2_IOVA_TO_VADDR(
			DPAA2_GET_FD_ADDR(fd)) + DPAA2_FD_PTA_SIZE);
		m->timestamp = annotation->word2;
		m->ol_flags |= PKT_RX_TIMESTAMP;
		DPAA2_PMD_DP_DEBUG("pkt timestamp:0x%" PRIx64 "", m->timestamp);
	}

	DPAA2_PMD_DP_DEBUG("HW frc = 0x%x\t packet type =0x%x "
		"ol_flags =0x%" PRIx64 "",
		frc, m->packet_type, m->ol_flags);
}

static inline uint32_t __attribute__((hot))
dpaa2_dev_rx_parse_slow(struct rte_mbuf *mbuf,
			struct dpaa2_annot_hdr *annotation)
{
	uint32_t pkt_type = RTE_PTYPE_UNKNOWN;
	uint16_t *vlan_tci;

	DPAA2_PMD_DP_DEBUG("(slow parse)annotation(3)=0x%" PRIx64 "\t"
			"(4)=0x%" PRIx64 "\t",
			annotation->word3, annotation->word4);

	if (BIT_ISSET_AT_POS(annotation->word3, L2_VLAN_1_PRESENT)) {
		vlan_tci = rte_pktmbuf_mtod_offset(mbuf, uint16_t *,
			(VLAN_TCI_OFFSET_1(annotation->word5) >> 16));
		mbuf->vlan_tci = rte_be_to_cpu_16(*vlan_tci);
		mbuf->ol_flags |= PKT_RX_VLAN;
		pkt_type |= RTE_PTYPE_L2_ETHER_VLAN;
	} else if (BIT_ISSET_AT_POS(annotation->word3, L2_VLAN_N_PRESENT)) {
		vlan_tci = rte_pktmbuf_mtod_offset(mbuf, uint16_t *,
			(VLAN_TCI_OFFSET_1(annotation->word5) >> 16));
		mbuf->vlan_tci = rte_be_to_cpu_16(*vlan_tci);
		mbuf->ol_flags |= PKT_RX_VLAN | PKT_RX_QINQ;
		pkt_type |= RTE_PTYPE_L2_ETHER_QINQ;
	}

	if (BIT_ISSET_AT_POS(annotation->word3, L2_ARP_PRESENT)) {
		pkt_type |= RTE_PTYPE_L2_ETHER_ARP;
		goto parse_done;
	} else if (BIT_ISSET_AT_POS(annotation->word3, L2_ETH_MAC_PRESENT)) {
		pkt_type |= RTE_PTYPE_L2_ETHER;
	} else {
		goto parse_done;
	}

	if (BIT_ISSET_AT_POS(annotation->word4, L3_IPV4_1_PRESENT |
			     L3_IPV4_N_PRESENT)) {
		pkt_type |= RTE_PTYPE_L3_IPV4;
		if (BIT_ISSET_AT_POS(annotation->word4, L3_IP_1_OPT_PRESENT |
			L3_IP_N_OPT_PRESENT))
			pkt_type |= RTE_PTYPE_L3_IPV4_EXT;

	} else if (BIT_ISSET_AT_POS(annotation->word4, L3_IPV6_1_PRESENT |
		  L3_IPV6_N_PRESENT)) {
		pkt_type |= RTE_PTYPE_L3_IPV6;
		if (BIT_ISSET_AT_POS(annotation->word4, L3_IP_1_OPT_PRESENT |
		    L3_IP_N_OPT_PRESENT))
			pkt_type |= RTE_PTYPE_L3_IPV6_EXT;
	} else {
		goto parse_done;
	}

	if (BIT_ISSET_AT_POS(annotation->word8, DPAA2_ETH_FAS_L3CE))
		mbuf->ol_flags |= PKT_RX_IP_CKSUM_BAD;
	else if (BIT_ISSET_AT_POS(annotation->word8, DPAA2_ETH_FAS_L4CE))
		mbuf->ol_flags |= PKT_RX_L4_CKSUM_BAD;

	if (BIT_ISSET_AT_POS(annotation->word4, L3_IP_1_FIRST_FRAGMENT |
	    L3_IP_1_MORE_FRAGMENT |
	    L3_IP_N_FIRST_FRAGMENT |
	    L3_IP_N_MORE_FRAGMENT)) {
		pkt_type |= RTE_PTYPE_L4_FRAG;
		goto parse_done;
	} else {
		pkt_type |= RTE_PTYPE_L4_NONFRAG;
	}

	if (BIT_ISSET_AT_POS(annotation->word4, L3_PROTO_UDP_PRESENT))
		pkt_type |= RTE_PTYPE_L4_UDP;

	else if (BIT_ISSET_AT_POS(annotation->word4, L3_PROTO_TCP_PRESENT))
		pkt_type |= RTE_PTYPE_L4_TCP;

	else if (BIT_ISSET_AT_POS(annotation->word4, L3_PROTO_SCTP_PRESENT))
		pkt_type |= RTE_PTYPE_L4_SCTP;

	else if (BIT_ISSET_AT_POS(annotation->word4, L3_PROTO_ICMP_PRESENT))
		pkt_type |= RTE_PTYPE_L4_ICMP;

	else if (BIT_ISSET_AT_POS(annotation->word4, L3_IP_UNKNOWN_PROTOCOL))
		pkt_type |= RTE_PTYPE_UNKNOWN;

parse_done:
	return pkt_type;
}

static inline uint32_t __attribute__((hot))
dpaa2_dev_rx_parse(struct rte_mbuf *mbuf, void *hw_annot_addr)
{
	struct dpaa2_annot_hdr *annotation =
			(struct dpaa2_annot_hdr *)hw_annot_addr;

	DPAA2_PMD_DP_DEBUG("(fast parse) Annotation = 0x%" PRIx64 "\t",
			   annotation->word4);

	if (BIT_ISSET_AT_POS(annotation->word8, DPAA2_ETH_FAS_L3CE))
		mbuf->ol_flags |= PKT_RX_IP_CKSUM_BAD;
	else if (BIT_ISSET_AT_POS(annotation->word8, DPAA2_ETH_FAS_L4CE))
		mbuf->ol_flags |= PKT_RX_L4_CKSUM_BAD;

	mbuf->ol_flags |= PKT_RX_TIMESTAMP;
	mbuf->timestamp = annotation->word2;
	DPAA2_PMD_DP_DEBUG("pkt timestamp: 0x%" PRIx64 "", mbuf->timestamp);

	/* Check detailed parsing requirement */
	if (annotation->word3 & 0x7FFFFC3FFFF)
		return dpaa2_dev_rx_parse_slow(mbuf, annotation);

	/* Return some common types from parse processing */
	switch (annotation->word4) {
	case DPAA2_L3_IPv4:
		return RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4;
	case DPAA2_L3_IPv6:
		return  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6;
	case DPAA2_L3_IPv4_TCP:
		return  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 |
				RTE_PTYPE_L4_TCP;
	case DPAA2_L3_IPv4_UDP:
		return  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 |
				RTE_PTYPE_L4_UDP;
	case DPAA2_L3_IPv6_TCP:
		return  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6 |
				RTE_PTYPE_L4_TCP;
	case DPAA2_L3_IPv6_UDP:
		return  RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6 |
				RTE_PTYPE_L4_UDP;
	default:
		break;
	}

	return dpaa2_dev_rx_parse_slow(mbuf, annotation);
}

static inline struct rte_mbuf *__attribute__((hot))
eth_sg_fd_to_mbuf(const struct qbman_fd *fd)
{
	struct qbman_sge *sgt, *sge;
	size_t sg_addr, fd_addr;
	int i = 0;
	struct rte_mbuf *first_seg, *next_seg, *cur_seg, *temp;

	fd_addr = (size_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd));

	/* Get Scatter gather table address */
	sgt = (struct qbman_sge *)(fd_addr + DPAA2_GET_FD_OFFSET(fd));

	sge = &sgt[i++];
	sg_addr = (size_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FLE_ADDR(sge));

	/* First Scatter gather entry */
	first_seg = DPAA2_INLINE_MBUF_FROM_BUF(sg_addr,
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size);
	/* Prepare all the metadata for first segment */
	first_seg->buf_addr = (uint8_t *)sg_addr;
	first_seg->ol_flags = 0;
	first_seg->data_off = DPAA2_GET_FLE_OFFSET(sge);
	first_seg->data_len = sge->length  & 0x1FFFF;
	first_seg->pkt_len = DPAA2_GET_FD_LEN(fd);
	first_seg->nb_segs = 1;
	first_seg->next = NULL;
	if (dpaa2_svr_family == SVR_LX2160A)
		dpaa2_dev_rx_parse_new(first_seg, fd);
	else
		first_seg->packet_type = dpaa2_dev_rx_parse(first_seg,
			(void *)((size_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd))
			 + DPAA2_FD_PTA_SIZE));

	rte_mbuf_refcnt_set(first_seg, 1);
	cur_seg = first_seg;
	while (!DPAA2_SG_IS_FINAL(sge)) {
		sge = &sgt[i++];
		sg_addr = (size_t)DPAA2_IOVA_TO_VADDR(
				DPAA2_GET_FLE_ADDR(sge));
		next_seg = DPAA2_INLINE_MBUF_FROM_BUF(sg_addr,
			rte_dpaa2_bpid_info[DPAA2_GET_FLE_BPID(sge)].meta_data_size);
		next_seg->buf_addr  = (uint8_t *)sg_addr;
		next_seg->data_off  = DPAA2_GET_FLE_OFFSET(sge);
		next_seg->data_len  = sge->length  & 0x1FFFF;
		first_seg->nb_segs += 1;
		rte_mbuf_refcnt_set(next_seg, 1);
		cur_seg->next = next_seg;
		next_seg->next = NULL;
		cur_seg = next_seg;
	}
	temp = DPAA2_INLINE_MBUF_FROM_BUF(fd_addr,
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size);
	rte_mbuf_refcnt_set(temp, 1);
	rte_pktmbuf_free_seg(temp);

	return (void *)first_seg;
}

static inline struct rte_mbuf *__attribute__((hot))
eth_fd_to_mbuf(const struct qbman_fd *fd)
{
	struct rte_mbuf *mbuf = DPAA2_INLINE_MBUF_FROM_BUF(
		DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd)),
		     rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size);

	/* need to repopulated some of the fields,
	 * as they may have changed in last transmission
	 */
	mbuf->nb_segs = 1;
	mbuf->ol_flags = 0;
	mbuf->data_off = DPAA2_GET_FD_OFFSET(fd);
	mbuf->data_len = DPAA2_GET_FD_LEN(fd);
	mbuf->pkt_len = mbuf->data_len;
	mbuf->next = NULL;
	rte_mbuf_refcnt_set(mbuf, 1);

	/* Parse the packet */
	/* parse results for LX2 are there in FRC field of FD.
	 * For other DPAA2 platforms , parse results are after
	 * the private - sw annotation area
	 */

	if (dpaa2_svr_family == SVR_LX2160A)
		dpaa2_dev_rx_parse_new(mbuf, fd);
	else
		mbuf->packet_type = dpaa2_dev_rx_parse(mbuf,
			(void *)((size_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd))
			 + DPAA2_FD_PTA_SIZE));

	DPAA2_PMD_DP_DEBUG("to mbuf - mbuf =%p, mbuf->buf_addr =%p, off = %d,"
		"fd_off=%d fd =%" PRIx64 ", meta = %d  bpid =%d, len=%d\n",
		mbuf, mbuf->buf_addr, mbuf->data_off,
		DPAA2_GET_FD_OFFSET(fd), DPAA2_GET_FD_ADDR(fd),
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size,
		DPAA2_GET_FD_BPID(fd), DPAA2_GET_FD_LEN(fd));

	return mbuf;
}

static int __attribute__ ((noinline)) __attribute__((hot))
eth_mbuf_to_sg_fd(struct rte_mbuf *mbuf,
		  struct qbman_fd *fd, uint16_t bpid)
{
	struct rte_mbuf *cur_seg = mbuf, *prev_seg, *mi, *temp;
	struct qbman_sge *sgt, *sge = NULL;
	int i;

	temp = rte_pktmbuf_alloc(mbuf->pool);
	if (temp == NULL) {
		DPAA2_PMD_DP_DEBUG("No memory to allocate S/G table\n");
		return -ENOMEM;
	}

	DPAA2_SET_FD_ADDR(fd, DPAA2_MBUF_VADDR_TO_IOVA(temp));
	DPAA2_SET_FD_LEN(fd, mbuf->pkt_len);
	DPAA2_SET_ONLY_FD_BPID(fd, bpid);
	DPAA2_SET_FD_OFFSET(fd, temp->data_off);
	DPAA2_SET_FD_ASAL(fd, DPAA2_ASAL_VAL);
	DPAA2_FD_SET_FORMAT(fd, qbman_fd_sg);
	/*Set Scatter gather table and Scatter gather entries*/
	sgt = (struct qbman_sge *)(
			(size_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd))
			+ DPAA2_GET_FD_OFFSET(fd));

	for (i = 0; i < mbuf->nb_segs; i++) {
		sge = &sgt[i];
		/*Resetting the buffer pool id and offset field*/
		sge->fin_bpid_offset = 0;
		DPAA2_SET_FLE_ADDR(sge, DPAA2_MBUF_VADDR_TO_IOVA(cur_seg));
		DPAA2_SET_FLE_OFFSET(sge, cur_seg->data_off);
		sge->length = cur_seg->data_len;
		if (RTE_MBUF_DIRECT(cur_seg)) {
			if (rte_mbuf_refcnt_read(cur_seg) > 1) {
				/* If refcnt > 1, invalid bpid is set to ensure
				 * buffer is not freed by HW
				 */
				DPAA2_SET_FLE_IVP(sge);
				rte_mbuf_refcnt_update(cur_seg, -1);
			} else
				DPAA2_SET_FLE_BPID(sge,
						mempool_to_bpid(cur_seg->pool));
			cur_seg = cur_seg->next;
		} else {
			/* Get owner MBUF from indirect buffer */
			mi = rte_mbuf_from_indirect(cur_seg);
			if (rte_mbuf_refcnt_read(mi) > 1) {
				/* If refcnt > 1, invalid bpid is set to ensure
				 * owner buffer is not freed by HW
				 */
				DPAA2_SET_FLE_IVP(sge);
			} else {
				DPAA2_SET_FLE_BPID(sge,
						   mempool_to_bpid(mi->pool));
				rte_mbuf_refcnt_update(mi, 1);
			}
			prev_seg = cur_seg;
			cur_seg = cur_seg->next;
			prev_seg->next = NULL;
			rte_pktmbuf_free(prev_seg);
		}
	}
	DPAA2_SG_SET_FINAL(sge, true);
	return 0;
}

static void
eth_mbuf_to_fd(struct rte_mbuf *mbuf,
	       struct qbman_fd *fd, uint16_t bpid) __attribute__((unused));

static void __attribute__ ((noinline)) __attribute__((hot))
eth_mbuf_to_fd(struct rte_mbuf *mbuf,
	       struct qbman_fd *fd, uint16_t bpid)
{
	DPAA2_MBUF_TO_CONTIG_FD(mbuf, fd, bpid);

	DPAA2_PMD_DP_DEBUG("mbuf =%p, mbuf->buf_addr =%p, off = %d,"
		"fd_off=%d fd =%" PRIx64 ", meta = %d  bpid =%d, len=%d\n",
		mbuf, mbuf->buf_addr, mbuf->data_off,
		DPAA2_GET_FD_OFFSET(fd), DPAA2_GET_FD_ADDR(fd),
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size,
		DPAA2_GET_FD_BPID(fd), DPAA2_GET_FD_LEN(fd));
	if (RTE_MBUF_DIRECT(mbuf)) {
		if (rte_mbuf_refcnt_read(mbuf) > 1) {
			DPAA2_SET_FD_IVP(fd);
			rte_mbuf_refcnt_update(mbuf, -1);
		}
	} else {
		struct rte_mbuf *mi;

		mi = rte_mbuf_from_indirect(mbuf);
		if (rte_mbuf_refcnt_read(mi) > 1)
			DPAA2_SET_FD_IVP(fd);
		else
			rte_mbuf_refcnt_update(mi, 1);
		rte_pktmbuf_free(mbuf);
	}
}

static inline int __attribute__((hot))
eth_copy_mbuf_to_fd(struct rte_mbuf *mbuf,
		    struct qbman_fd *fd, uint16_t bpid)
{
	struct rte_mbuf *m;
	void *mb = NULL;

	if (rte_dpaa2_mbuf_alloc_bulk(
		rte_dpaa2_bpid_info[bpid].bp_list->mp, &mb, 1)) {
		DPAA2_PMD_DP_DEBUG("Unable to allocated DPAA2 buffer\n");
		return -1;
	}
	m = (struct rte_mbuf *)mb;
	memcpy((char *)m->buf_addr + mbuf->data_off,
	       (void *)((char *)mbuf->buf_addr + mbuf->data_off),
		mbuf->pkt_len);

	/* Copy required fields */
	m->data_off = mbuf->data_off;
	m->ol_flags = mbuf->ol_flags;
	m->packet_type = mbuf->packet_type;
	m->tx_offload = mbuf->tx_offload;

	DPAA2_MBUF_TO_CONTIG_FD(m, fd, bpid);

	DPAA2_PMD_DP_DEBUG(
		"mbuf: %p, BMAN buf addr: %p, fdaddr: %" PRIx64 ", bpid: %d,"
		" meta: %d, off: %d, len: %d\n",
		(void *)mbuf,
		mbuf->buf_addr,
		DPAA2_GET_FD_ADDR(fd),
		DPAA2_GET_FD_BPID(fd),
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size,
		DPAA2_GET_FD_OFFSET(fd),
		DPAA2_GET_FD_LEN(fd));

return 0;
}

/* This function assumes that caller will be keep the same value for nb_pkts
 * across calls per queue, if that is not the case, better use non-prefetch
 * version of rx call.
 * It will return the packets as requested in previous call without honoring
 * the current nb_pkts or bufs space.
 */
uint16_t
dpaa2_dev_prefetch_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	/* Function receive frames for a given device and VQ*/
	struct dpaa2_queue *dpaa2_q = (struct dpaa2_queue *)queue;
	struct qbman_result *dq_storage, *dq_storage1 = NULL;
	uint32_t fqid = dpaa2_q->fqid;
	int ret, num_rx = 0, pull_size;
	uint8_t pending, status;
	struct qbman_swp *swp;
	const struct qbman_fd *fd, *next_fd;
	struct qbman_pull_desc pulldesc;
	struct queue_storage_info_t *q_storage = dpaa2_q->q_storage;
	struct rte_eth_dev_data *eth_data = dpaa2_q->eth_data;

	if (unlikely(!DPAA2_PER_LCORE_ETHRX_DPIO)) {
		ret = dpaa2_affine_qbman_ethrx_swp();
		if (ret) {
			DPAA2_PMD_ERR("Failure in affining portal");
			return 0;
		}
	}

	if (unlikely(!rte_dpaa2_bpid_info &&
		     rte_eal_process_type() == RTE_PROC_SECONDARY))
		rte_dpaa2_bpid_info = dpaa2_q->bp_array;

	swp = DPAA2_PER_LCORE_ETHRX_PORTAL;
	pull_size = (nb_pkts > dpaa2_dqrr_size) ? dpaa2_dqrr_size : nb_pkts;
	if (unlikely(!q_storage->active_dqs)) {
		q_storage->toggle = 0;
		dq_storage = q_storage->dq_storage[q_storage->toggle];
		q_storage->last_num_pkts = pull_size;
		qbman_pull_desc_clear(&pulldesc);
		qbman_pull_desc_set_numframes(&pulldesc,
					      q_storage->last_num_pkts);
		qbman_pull_desc_set_fq(&pulldesc, fqid);
		qbman_pull_desc_set_storage(&pulldesc, dq_storage,
			(uint64_t)(DPAA2_VADDR_TO_IOVA(dq_storage)), 1);
		if (check_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index)) {
			while (!qbman_check_command_complete(
			       get_swp_active_dqs(
			       DPAA2_PER_LCORE_ETHRX_DPIO->index)))
				;
			clear_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index);
		}
		while (1) {
			if (qbman_swp_pull(swp, &pulldesc)) {
				DPAA2_PMD_DP_DEBUG("VDQ command is not issued."
						  " QBMAN is busy (1)\n");
				/* Portal was busy, try again */
				continue;
			}
			break;
		}
		q_storage->active_dqs = dq_storage;
		q_storage->active_dpio_id = DPAA2_PER_LCORE_ETHRX_DPIO->index;
		set_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index,
				   dq_storage);
	}

	dq_storage = q_storage->active_dqs;
	rte_prefetch0((void *)(size_t)(dq_storage));
	rte_prefetch0((void *)(size_t)(dq_storage + 1));

	/* Prepare next pull descriptor. This will give space for the
	 * prefething done on DQRR entries
	 */
	q_storage->toggle ^= 1;
	dq_storage1 = q_storage->dq_storage[q_storage->toggle];
	qbman_pull_desc_clear(&pulldesc);
	qbman_pull_desc_set_numframes(&pulldesc, pull_size);
	qbman_pull_desc_set_fq(&pulldesc, fqid);
	qbman_pull_desc_set_storage(&pulldesc, dq_storage1,
		(uint64_t)(DPAA2_VADDR_TO_IOVA(dq_storage1)), 1);

	/* Check if the previous issued command is completed.
	 * Also seems like the SWP is shared between the Ethernet Driver
	 * and the SEC driver.
	 */
	while (!qbman_check_command_complete(dq_storage))
		;
	if (dq_storage == get_swp_active_dqs(q_storage->active_dpio_id))
		clear_swp_active_dqs(q_storage->active_dpio_id);

	pending = 1;

	do {
		/* Loop until the dq_storage is updated with
		 * new token by QBMAN
		 */
		while (!qbman_check_new_result(dq_storage))
			;
		rte_prefetch0((void *)((size_t)(dq_storage + 2)));
		/* Check whether Last Pull command is Expired and
		 * setting Condition for Loop termination
		 */
		if (qbman_result_DQ_is_pull_complete(dq_storage)) {
			pending = 0;
			/* Check for valid frame. */
			status = qbman_result_DQ_flags(dq_storage);
			if (unlikely((status & QBMAN_DQ_STAT_VALIDFRAME) == 0))
				continue;
		}
		fd = qbman_result_DQ_fd(dq_storage);

		if (dpaa2_svr_family != SVR_LX2160A) {
			next_fd = qbman_result_DQ_fd(dq_storage + 1);
			/* Prefetch Annotation address for the parse results */
			rte_prefetch0((void *)(size_t)(DPAA2_GET_FD_ADDR(
				      next_fd) + DPAA2_FD_PTA_SIZE + 16));
		}

		if (unlikely(DPAA2_FD_GET_FORMAT(fd) == qbman_fd_sg))
			bufs[num_rx] = eth_sg_fd_to_mbuf(fd);
		else
			bufs[num_rx] = eth_fd_to_mbuf(fd);
		bufs[num_rx]->port = eth_data->port_id;

		if (eth_data->dev_conf.rxmode.offloads &
				DEV_RX_OFFLOAD_VLAN_STRIP)
			rte_vlan_strip(bufs[num_rx]);

		dq_storage++;
		num_rx++;
	} while (pending);

	if (check_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index)) {
		while (!qbman_check_command_complete(
		       get_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index)))
			;
		clear_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index);
	}
	/* issue a volatile dequeue command for next pull */
	while (1) {
		if (qbman_swp_pull(swp, &pulldesc)) {
			DPAA2_PMD_DP_DEBUG("VDQ command is not issued."
					  "QBMAN is busy (2)\n");
			continue;
		}
		break;
	}
	q_storage->active_dqs = dq_storage1;
	q_storage->active_dpio_id = DPAA2_PER_LCORE_ETHRX_DPIO->index;
	set_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index, dq_storage1);

	dpaa2_q->rx_pkts += num_rx;

	return num_rx;
}

void __attribute__((hot))
dpaa2_dev_process_parallel_event(struct qbman_swp *swp,
				 const struct qbman_fd *fd,
				 const struct qbman_result *dq,
				 struct dpaa2_queue *rxq,
				 struct rte_event *ev)
{
	rte_prefetch0((void *)(size_t)(DPAA2_GET_FD_ADDR(fd) +
		DPAA2_FD_PTA_SIZE + 16));

	ev->flow_id = rxq->ev.flow_id;
	ev->sub_event_type = rxq->ev.sub_event_type;
	ev->event_type = RTE_EVENT_TYPE_ETHDEV;
	ev->op = RTE_EVENT_OP_NEW;
	ev->sched_type = rxq->ev.sched_type;
	ev->queue_id = rxq->ev.queue_id;
	ev->priority = rxq->ev.priority;

	ev->mbuf = eth_fd_to_mbuf(fd);

	qbman_swp_dqrr_consume(swp, dq);
}

void __attribute__((hot))
dpaa2_dev_process_atomic_event(struct qbman_swp *swp __attribute__((unused)),
			       const struct qbman_fd *fd,
			       const struct qbman_result *dq,
			       struct dpaa2_queue *rxq,
			       struct rte_event *ev)
{
	uint8_t dqrr_index;

	rte_prefetch0((void *)(size_t)(DPAA2_GET_FD_ADDR(fd) +
		DPAA2_FD_PTA_SIZE + 16));

	ev->flow_id = rxq->ev.flow_id;
	ev->sub_event_type = rxq->ev.sub_event_type;
	ev->event_type = RTE_EVENT_TYPE_ETHDEV;
	ev->op = RTE_EVENT_OP_NEW;
	ev->sched_type = rxq->ev.sched_type;
	ev->queue_id = rxq->ev.queue_id;
	ev->priority = rxq->ev.priority;

	ev->mbuf = eth_fd_to_mbuf(fd);

	dqrr_index = qbman_get_dqrr_idx(dq);
	ev->mbuf->seqn = dqrr_index + 1;
	DPAA2_PER_LCORE_DQRR_SIZE++;
	DPAA2_PER_LCORE_DQRR_HELD |= 1 << dqrr_index;
	DPAA2_PER_LCORE_DQRR_MBUF(dqrr_index) = ev->mbuf;
}

void __attribute__((hot))
dpaa2_dev_process_ordered_event(struct qbman_swp *swp,
				const struct qbman_fd *fd,
				const struct qbman_result *dq,
				struct dpaa2_queue *rxq,
				struct rte_event *ev)
{
	rte_prefetch0((void *)(size_t)(DPAA2_GET_FD_ADDR(fd) +
		DPAA2_FD_PTA_SIZE + 16));

	ev->flow_id = rxq->ev.flow_id;
	ev->sub_event_type = rxq->ev.sub_event_type;
	ev->event_type = RTE_EVENT_TYPE_ETHDEV;
	ev->op = RTE_EVENT_OP_NEW;
	ev->sched_type = rxq->ev.sched_type;
	ev->queue_id = rxq->ev.queue_id;
	ev->priority = rxq->ev.priority;

	ev->mbuf = eth_fd_to_mbuf(fd);

	ev->mbuf->seqn = DPAA2_ENQUEUE_FLAG_ORP;
	ev->mbuf->seqn |= qbman_result_DQ_odpid(dq) << DPAA2_EQCR_OPRID_SHIFT;
	ev->mbuf->seqn |= qbman_result_DQ_seqnum(dq) << DPAA2_EQCR_SEQNUM_SHIFT;

	qbman_swp_dqrr_consume(swp, dq);
}

/*
 * Callback to handle sending packets through WRIOP based interface
 */
uint16_t
dpaa2_dev_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	/* Function to transmit the frames to given device and VQ*/
	uint32_t loop, retry_count;
	int32_t ret;
	struct qbman_fd fd_arr[MAX_TX_RING_SLOTS];
	struct rte_mbuf *mi;
	uint32_t frames_to_send;
	struct rte_mempool *mp;
	struct qbman_eq_desc eqdesc;
	struct dpaa2_queue *dpaa2_q = (struct dpaa2_queue *)queue;
	struct qbman_swp *swp;
	uint16_t num_tx = 0;
	uint16_t bpid;
	struct rte_eth_dev_data *eth_data = dpaa2_q->eth_data;
	struct dpaa2_dev_priv *priv = eth_data->dev_private;
	uint32_t flags[MAX_TX_RING_SLOTS] = {0};

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_PMD_ERR("Failure in affining portal");
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	DPAA2_PMD_DP_DEBUG("===> eth_data =%p, fqid =%d\n",
			eth_data, dpaa2_q->fqid);

	/*Prepare enqueue descriptor*/
	qbman_eq_desc_clear(&eqdesc);
	qbman_eq_desc_set_no_orp(&eqdesc, DPAA2_EQ_RESP_ERR_FQ);
	qbman_eq_desc_set_qd(&eqdesc, priv->qdid,
			     dpaa2_q->flow_id, dpaa2_q->tc_index);
	/*Clear the unused FD fields before sending*/
	while (nb_pkts) {
		/*Check if the queue is congested*/
		retry_count = 0;
		while (qbman_result_SCN_state(dpaa2_q->cscn)) {
			retry_count++;
			/* Retry for some time before giving up */
			if (retry_count > CONG_RETRY_COUNT)
				goto skip_tx;
		}

		frames_to_send = (nb_pkts > dpaa2_eqcr_size) ?
			dpaa2_eqcr_size : nb_pkts;

		for (loop = 0; loop < frames_to_send; loop++) {
			if ((*bufs)->seqn) {
				uint8_t dqrr_index = (*bufs)->seqn - 1;

				flags[loop] = QBMAN_ENQUEUE_FLAG_DCA |
						dqrr_index;
				DPAA2_PER_LCORE_DQRR_SIZE--;
				DPAA2_PER_LCORE_DQRR_HELD &= ~(1 << dqrr_index);
				(*bufs)->seqn = DPAA2_INVALID_MBUF_SEQN;
			}

			if (likely(RTE_MBUF_DIRECT(*bufs))) {
				mp = (*bufs)->pool;
				/* Check the basic scenario and set
				 * the FD appropriately here itself.
				 */
				if (likely(mp && mp->ops_index ==
				    priv->bp_list->dpaa2_ops_index &&
				    (*bufs)->nb_segs == 1 &&
				    rte_mbuf_refcnt_read((*bufs)) == 1)) {
					if (unlikely(((*bufs)->ol_flags
						& PKT_TX_VLAN_PKT) ||
						(eth_data->dev_conf.txmode.offloads
						& DEV_TX_OFFLOAD_VLAN_INSERT))) {
						ret = rte_vlan_insert(bufs);
						if (ret)
							goto send_n_return;
					}
					DPAA2_MBUF_TO_CONTIG_FD((*bufs),
					&fd_arr[loop], mempool_to_bpid(mp));
					bufs++;
					continue;
				}
			} else {
				mi = rte_mbuf_from_indirect(*bufs);
				mp = mi->pool;
			}
			/* Not a hw_pkt pool allocated frame */
			if (unlikely(!mp || !priv->bp_list)) {
				DPAA2_PMD_ERR("Err: No buffer pool attached");
				goto send_n_return;
			}

			if (unlikely(((*bufs)->ol_flags & PKT_TX_VLAN_PKT) ||
				(eth_data->dev_conf.txmode.offloads
				& DEV_TX_OFFLOAD_VLAN_INSERT))) {
				int ret = rte_vlan_insert(bufs);
				if (ret)
					goto send_n_return;
			}
			if (mp->ops_index != priv->bp_list->dpaa2_ops_index) {
				DPAA2_PMD_WARN("Non DPAA2 buffer pool");
				/* alloc should be from the default buffer pool
				 * attached to this interface
				 */
				bpid = priv->bp_list->buf_pool.bpid;

				if (unlikely((*bufs)->nb_segs > 1)) {
					DPAA2_PMD_ERR("S/G support not added"
						" for non hw offload buffer");
					goto send_n_return;
				}
				if (eth_copy_mbuf_to_fd(*bufs,
							&fd_arr[loop], bpid)) {
					goto send_n_return;
				}
				/* free the original packet */
				rte_pktmbuf_free(*bufs);
			} else {
				bpid = mempool_to_bpid(mp);
				if (unlikely((*bufs)->nb_segs > 1)) {
					if (eth_mbuf_to_sg_fd(*bufs,
							&fd_arr[loop], bpid))
						goto send_n_return;
				} else {
					eth_mbuf_to_fd(*bufs,
						       &fd_arr[loop], bpid);
				}
			}
			bufs++;
		}
		loop = 0;
		while (loop < frames_to_send) {
			loop += qbman_swp_enqueue_multiple(swp, &eqdesc,
					&fd_arr[loop], &flags[loop],
					frames_to_send - loop);
		}

		num_tx += frames_to_send;
		nb_pkts -= frames_to_send;
	}
	dpaa2_q->tx_pkts += num_tx;
	return num_tx;

send_n_return:
	/* send any already prepared fd */
	if (loop) {
		unsigned int i = 0;

		while (i < loop) {
			i += qbman_swp_enqueue_multiple(swp, &eqdesc,
							&fd_arr[i],
							&flags[loop],
							loop - i);
		}
		num_tx += loop;
	}
skip_tx:
	dpaa2_q->tx_pkts += num_tx;
	return num_tx;
}

void
dpaa2_dev_free_eqresp_buf(uint16_t eqresp_ci)
{
	struct dpaa2_dpio_dev *dpio_dev = DPAA2_PER_LCORE_DPIO;
	struct qbman_fd *fd;
	struct rte_mbuf *m;

	fd = qbman_result_eqresp_fd(&dpio_dev->eqresp[eqresp_ci]);
	m = eth_fd_to_mbuf(fd);
	rte_pktmbuf_free(m);
}

static void
dpaa2_set_enqueue_descriptor(struct dpaa2_queue *dpaa2_q,
			     struct rte_mbuf *m,
			     struct qbman_eq_desc *eqdesc)
{
	struct rte_eth_dev_data *eth_data = dpaa2_q->eth_data;
	struct dpaa2_dev_priv *priv = eth_data->dev_private;
	struct dpaa2_dpio_dev *dpio_dev = DPAA2_PER_LCORE_DPIO;
	struct eqresp_metadata *eqresp_meta;
	uint16_t orpid, seqnum;
	uint8_t dq_idx;

	qbman_eq_desc_set_qd(eqdesc, priv->qdid, dpaa2_q->flow_id,
			     dpaa2_q->tc_index);

	if (m->seqn & DPAA2_ENQUEUE_FLAG_ORP) {
		orpid = (m->seqn & DPAA2_EQCR_OPRID_MASK) >>
			DPAA2_EQCR_OPRID_SHIFT;
		seqnum = (m->seqn & DPAA2_EQCR_SEQNUM_MASK) >>
			DPAA2_EQCR_SEQNUM_SHIFT;

		if (!priv->en_loose_ordered) {
			qbman_eq_desc_set_orp(eqdesc, 1, orpid, seqnum, 0);
			qbman_eq_desc_set_response(eqdesc, (uint64_t)
				DPAA2_VADDR_TO_IOVA(&dpio_dev->eqresp[
				dpio_dev->eqresp_pi]), 1);
			qbman_eq_desc_set_token(eqdesc, 1);

			eqresp_meta = &dpio_dev->eqresp_meta[
				dpio_dev->eqresp_pi];
			eqresp_meta->dpaa2_q = dpaa2_q;
			eqresp_meta->mp = m->pool;

			dpio_dev->eqresp_pi + 1 < MAX_EQ_RESP_ENTRIES ?
				dpio_dev->eqresp_pi++ :
				(dpio_dev->eqresp_pi = 0);
		} else {
			qbman_eq_desc_set_orp(eqdesc, 0, orpid, seqnum, 0);
		}
	} else {
		dq_idx = m->seqn - 1;
		qbman_eq_desc_set_dca(eqdesc, 1, dq_idx, 0);
		DPAA2_PER_LCORE_DQRR_SIZE--;
		DPAA2_PER_LCORE_DQRR_HELD &= ~(1 << dq_idx);
	}
	m->seqn = DPAA2_INVALID_MBUF_SEQN;
}

/* Callback to handle sending ordered packets through WRIOP based interface */
uint16_t
dpaa2_dev_tx_ordered(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	/* Function to transmit the frames to given device and VQ*/
	struct dpaa2_queue *dpaa2_q = (struct dpaa2_queue *)queue;
	struct rte_eth_dev_data *eth_data = dpaa2_q->eth_data;
	struct dpaa2_dev_priv *priv = eth_data->dev_private;
	struct dpaa2_queue *order_sendq = (struct dpaa2_queue *)priv->tx_vq[0];
	struct qbman_fd fd_arr[MAX_TX_RING_SLOTS];
	struct rte_mbuf *mi;
	struct rte_mempool *mp;
	struct qbman_eq_desc eqdesc[MAX_TX_RING_SLOTS];
	struct qbman_swp *swp;
	uint32_t frames_to_send, num_free_eq_desc;
	uint32_t loop, retry_count;
	int32_t ret;
	uint16_t num_tx = 0;
	uint16_t bpid;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_PMD_ERR("Failure in affining portal");
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	DPAA2_PMD_DP_DEBUG("===> eth_data =%p, fqid =%d\n",
			   eth_data, dpaa2_q->fqid);

	/* This would also handle normal and atomic queues as any type
	 * of packet can be enqueued when ordered queues are being used.
	 */
	while (nb_pkts) {
		/*Check if the queue is congested*/
		retry_count = 0;
		while (qbman_result_SCN_state(dpaa2_q->cscn)) {
			retry_count++;
			/* Retry for some time before giving up */
			if (retry_count > CONG_RETRY_COUNT)
				goto skip_tx;
		}

		frames_to_send = (nb_pkts > dpaa2_eqcr_size) ?
			dpaa2_eqcr_size : nb_pkts;

		if (!priv->en_loose_ordered) {
			if ((*bufs)->seqn & DPAA2_ENQUEUE_FLAG_ORP) {
				num_free_eq_desc = dpaa2_free_eq_descriptors();
				if (num_free_eq_desc < frames_to_send)
					frames_to_send = num_free_eq_desc;
			}
		}

		for (loop = 0; loop < frames_to_send; loop++) {
			/*Prepare enqueue descriptor*/
			qbman_eq_desc_clear(&eqdesc[loop]);

			if ((*bufs)->seqn) {
				/* Use only queue 0 for Tx in case of atomic/
				 * ordered packets as packets can get unordered
				 * when being tranmitted out from the interface
				 */
				dpaa2_set_enqueue_descriptor(order_sendq,
							     (*bufs),
							     &eqdesc[loop]);
			} else {
				qbman_eq_desc_set_no_orp(&eqdesc[loop],
							 DPAA2_EQ_RESP_ERR_FQ);
				qbman_eq_desc_set_qd(&eqdesc[loop], priv->qdid,
						     dpaa2_q->flow_id,
						     dpaa2_q->tc_index);
			}

			if (likely(RTE_MBUF_DIRECT(*bufs))) {
				mp = (*bufs)->pool;
				/* Check the basic scenario and set
				 * the FD appropriately here itself.
				 */
				if (likely(mp && mp->ops_index ==
				    priv->bp_list->dpaa2_ops_index &&
				    (*bufs)->nb_segs == 1 &&
				    rte_mbuf_refcnt_read((*bufs)) == 1)) {
					if (unlikely((*bufs)->ol_flags
						& PKT_TX_VLAN_PKT)) {
					  ret = rte_vlan_insert(bufs);
					  if (ret)
						goto send_n_return;
					}
					DPAA2_MBUF_TO_CONTIG_FD((*bufs),
						&fd_arr[loop],
						mempool_to_bpid(mp));
					bufs++;
					continue;
				}
			} else {
				mi = rte_mbuf_from_indirect(*bufs);
				mp = mi->pool;
			}
			/* Not a hw_pkt pool allocated frame */
			if (unlikely(!mp || !priv->bp_list)) {
				DPAA2_PMD_ERR("Err: No buffer pool attached");
				goto send_n_return;
			}

			if (mp->ops_index != priv->bp_list->dpaa2_ops_index) {
				DPAA2_PMD_WARN("Non DPAA2 buffer pool");
				/* alloc should be from the default buffer pool
				 * attached to this interface
				 */
				bpid = priv->bp_list->buf_pool.bpid;

				if (unlikely((*bufs)->nb_segs > 1)) {
					DPAA2_PMD_ERR(
						"S/G not supp for non hw offload buffer");
					goto send_n_return;
				}
				if (eth_copy_mbuf_to_fd(*bufs,
							&fd_arr[loop], bpid)) {
					goto send_n_return;
				}
				/* free the original packet */
				rte_pktmbuf_free(*bufs);
			} else {
				bpid = mempool_to_bpid(mp);
				if (unlikely((*bufs)->nb_segs > 1)) {
					if (eth_mbuf_to_sg_fd(*bufs,
							      &fd_arr[loop],
							      bpid))
						goto send_n_return;
				} else {
					eth_mbuf_to_fd(*bufs,
						       &fd_arr[loop], bpid);
				}
			}
			bufs++;
		}
		loop = 0;
		while (loop < frames_to_send) {
			loop += qbman_swp_enqueue_multiple_desc(swp,
					&eqdesc[loop], &fd_arr[loop],
					frames_to_send - loop);
		}

		num_tx += frames_to_send;
		nb_pkts -= frames_to_send;
	}
	dpaa2_q->tx_pkts += num_tx;
	return num_tx;

send_n_return:
	/* send any already prepared fd */
	if (loop) {
		unsigned int i = 0;

		while (i < loop) {
			i += qbman_swp_enqueue_multiple_desc(swp, &eqdesc[loop],
							&fd_arr[i], loop - i);
		}
		num_tx += loop;
	}
skip_tx:
	dpaa2_q->tx_pkts += num_tx;
	return num_tx;
}

/**
 * Dummy DPDK callback for TX.
 *
 * This function is used to temporarily replace the real callback during
 * unsafe control operations on the queue, or in case of error.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
dummy_dev_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	(void)queue;
	(void)bufs;
	(void)nb_pkts;
	return 0;
}

#if defined(RTE_TOOLCHAIN_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#elif defined(RTE_TOOLCHAIN_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#endif

/* This function loopbacks all the received packets.*/
uint16_t
dpaa2_dev_loopback_rx(void *queue,
		      struct rte_mbuf **bufs __rte_unused,
		      uint16_t nb_pkts)
{
	/* Function receive frames for a given device and VQ*/
	struct dpaa2_queue *dpaa2_q = (struct dpaa2_queue *)queue;
	struct qbman_result *dq_storage, *dq_storage1 = NULL;
	uint32_t fqid = dpaa2_q->fqid;
	int ret, num_rx = 0, num_tx = 0, pull_size;
	uint8_t pending, status;
	struct qbman_swp *swp;
	struct qbman_fd *fd[DPAA2_LX2_DQRR_RING_SIZE];
	struct qbman_pull_desc pulldesc;
	struct qbman_eq_desc eqdesc;
	struct queue_storage_info_t *q_storage = dpaa2_q->q_storage;
	struct rte_eth_dev_data *eth_data = dpaa2_q->eth_data;
	struct dpaa2_dev_priv *priv = eth_data->dev_private;
	struct dpaa2_queue *tx_q = priv->tx_vq[0];
	/* todo - currently we are using 1st TX queue only for loopback*/

	if (unlikely(!DPAA2_PER_LCORE_ETHRX_DPIO)) {
		ret = dpaa2_affine_qbman_ethrx_swp();
		if (ret) {
			DPAA2_PMD_ERR("Failure in affining portal");
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_ETHRX_PORTAL;
	pull_size = (nb_pkts > dpaa2_dqrr_size) ? dpaa2_dqrr_size : nb_pkts;
	if (unlikely(!q_storage->active_dqs)) {
		q_storage->toggle = 0;
		dq_storage = q_storage->dq_storage[q_storage->toggle];
		q_storage->last_num_pkts = pull_size;
		qbman_pull_desc_clear(&pulldesc);
		qbman_pull_desc_set_numframes(&pulldesc,
					      q_storage->last_num_pkts);
		qbman_pull_desc_set_fq(&pulldesc, fqid);
		qbman_pull_desc_set_storage(&pulldesc, dq_storage,
			(size_t)(DPAA2_VADDR_TO_IOVA(dq_storage)), 1);
		if (check_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index)) {
			while (!qbman_check_command_complete(
			       get_swp_active_dqs(
			       DPAA2_PER_LCORE_ETHRX_DPIO->index)))
				;
			clear_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index);
		}
		while (1) {
			if (qbman_swp_pull(swp, &pulldesc)) {
				DPAA2_PMD_DP_DEBUG(
					"VDQ command not issued.QBMAN busy\n");
				/* Portal was busy, try again */
				continue;
			}
			break;
		}
		q_storage->active_dqs = dq_storage;
		q_storage->active_dpio_id = DPAA2_PER_LCORE_ETHRX_DPIO->index;
		set_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index,
				   dq_storage);
	}

	dq_storage = q_storage->active_dqs;
	rte_prefetch0((void *)(size_t)(dq_storage));
	rte_prefetch0((void *)(size_t)(dq_storage + 1));

	/* Prepare next pull descriptor. This will give space for the
	 * prefething done on DQRR entries
	 */
	q_storage->toggle ^= 1;
	dq_storage1 = q_storage->dq_storage[q_storage->toggle];
	qbman_pull_desc_clear(&pulldesc);
	qbman_pull_desc_set_numframes(&pulldesc, pull_size);
	qbman_pull_desc_set_fq(&pulldesc, fqid);
	qbman_pull_desc_set_storage(&pulldesc, dq_storage1,
		(size_t)(DPAA2_VADDR_TO_IOVA(dq_storage1)), 1);

	/*Prepare enqueue descriptor*/
	qbman_eq_desc_clear(&eqdesc);
	qbman_eq_desc_set_no_orp(&eqdesc, DPAA2_EQ_RESP_ERR_FQ);
	qbman_eq_desc_set_response(&eqdesc, 0, 0);
	qbman_eq_desc_set_fq(&eqdesc, tx_q->fqid);

	/* Check if the previous issued command is completed.
	 * Also seems like the SWP is shared between the Ethernet Driver
	 * and the SEC driver.
	 */
	while (!qbman_check_command_complete(dq_storage))
		;
	if (dq_storage == get_swp_active_dqs(q_storage->active_dpio_id))
		clear_swp_active_dqs(q_storage->active_dpio_id);

	pending = 1;

	do {
		/* Loop until the dq_storage is updated with
		 * new token by QBMAN
		 */
		while (!qbman_check_new_result(dq_storage))
			;
		rte_prefetch0((void *)((size_t)(dq_storage + 2)));
		/* Check whether Last Pull command is Expired and
		 * setting Condition for Loop termination
		 */
		if (qbman_result_DQ_is_pull_complete(dq_storage)) {
			pending = 0;
			/* Check for valid frame. */
			status = qbman_result_DQ_flags(dq_storage);
			if (unlikely((status & QBMAN_DQ_STAT_VALIDFRAME) == 0))
				continue;
		}
		fd[num_rx] = (struct qbman_fd *)qbman_result_DQ_fd(dq_storage);

		dq_storage++;
		num_rx++;
	} while (pending);

	while (num_tx < num_rx) {
		num_tx += qbman_swp_enqueue_multiple_fd(swp, &eqdesc,
				&fd[num_tx], 0, num_rx - num_tx);
	}

	if (check_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index)) {
		while (!qbman_check_command_complete(
		       get_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index)))
			;
		clear_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index);
	}
	/* issue a volatile dequeue command for next pull */
	while (1) {
		if (qbman_swp_pull(swp, &pulldesc)) {
			DPAA2_PMD_DP_DEBUG("VDQ command is not issued."
					  "QBMAN is busy (2)\n");
			continue;
		}
		break;
	}
	q_storage->active_dqs = dq_storage1;
	q_storage->active_dpio_id = DPAA2_PER_LCORE_ETHRX_DPIO->index;
	set_swp_active_dqs(DPAA2_PER_LCORE_ETHRX_DPIO->index, dq_storage1);

	dpaa2_q->rx_pkts += num_rx;
	dpaa2_q->tx_pkts += num_tx;

	return 0;
}
#if defined(RTE_TOOLCHAIN_GCC)
#pragma GCC diagnostic pop
#elif defined(RTE_TOOLCHAIN_CLANG)
#pragma clang diagnostic pop
#endif
