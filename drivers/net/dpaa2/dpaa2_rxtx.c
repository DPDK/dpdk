/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016 NXP.
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
 *     * Neither the name of Freescale Semiconductor, Inc nor the names of its
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

#include <time.h>
#include <net/if.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_dev.h>

#include <fslmc_logs.h>
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_dpio.h>
#include <dpaa2_hw_mempool.h>

#include "dpaa2_ethdev.h"
#include "base/dpaa2_hw_dpni_annot.h"

static inline uint32_t __attribute__((hot))
dpaa2_dev_rx_parse(uint64_t hw_annot_addr)
{
	uint32_t pkt_type = RTE_PTYPE_UNKNOWN;
	struct dpaa2_annot_hdr *annotation =
			(struct dpaa2_annot_hdr *)hw_annot_addr;

	PMD_RX_LOG(DEBUG, "annotation = 0x%lx   ", annotation->word4);

	if (BIT_ISSET_AT_POS(annotation->word3, L2_ARP_PRESENT)) {
		pkt_type = RTE_PTYPE_L2_ETHER_ARP;
		goto parse_done;
	} else if (BIT_ISSET_AT_POS(annotation->word3, L2_ETH_MAC_PRESENT)) {
		pkt_type = RTE_PTYPE_L2_ETHER;
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

static inline void __attribute__((hot))
dpaa2_dev_rx_offload(uint64_t hw_annot_addr, struct rte_mbuf *mbuf)
{
	struct dpaa2_annot_hdr *annotation =
		(struct dpaa2_annot_hdr *)hw_annot_addr;

	if (BIT_ISSET_AT_POS(annotation->word3,
			     L2_VLAN_1_PRESENT | L2_VLAN_N_PRESENT))
		mbuf->ol_flags |= PKT_RX_VLAN_PKT;

	if (BIT_ISSET_AT_POS(annotation->word8, DPAA2_ETH_FAS_L3CE))
		mbuf->ol_flags |= PKT_RX_IP_CKSUM_BAD;

	if (BIT_ISSET_AT_POS(annotation->word8, DPAA2_ETH_FAS_L4CE))
		mbuf->ol_flags |= PKT_RX_L4_CKSUM_BAD;
}

static inline struct rte_mbuf *__attribute__((hot))
eth_sg_fd_to_mbuf(const struct qbman_fd *fd)
{
	struct qbman_sge *sgt, *sge;
	dma_addr_t sg_addr;
	int i = 0;
	uint64_t fd_addr;
	struct rte_mbuf *first_seg, *next_seg, *cur_seg, *temp;

	fd_addr = (uint64_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd));

	/* Get Scatter gather table address */
	sgt = (struct qbman_sge *)(fd_addr + DPAA2_GET_FD_OFFSET(fd));

	sge = &sgt[i++];
	sg_addr = (uint64_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FLE_ADDR(sge));

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

	first_seg->packet_type = dpaa2_dev_rx_parse(
			 (uint64_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd))
			 + DPAA2_FD_PTA_SIZE);
	dpaa2_dev_rx_offload((uint64_t)DPAA2_IOVA_TO_VADDR(
			DPAA2_GET_FD_ADDR(fd)) +
			DPAA2_FD_PTA_SIZE, first_seg);
	rte_mbuf_refcnt_set(first_seg, 1);
	cur_seg = first_seg;
	while (!DPAA2_SG_IS_FINAL(sge)) {
		sge = &sgt[i++];
		sg_addr = (uint64_t)DPAA2_IOVA_TO_VADDR(
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

	/* Parse the packet */
	/* parse results are after the private - sw annotation area */
	mbuf->packet_type = dpaa2_dev_rx_parse(
			(uint64_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd))
			 + DPAA2_FD_PTA_SIZE);

	dpaa2_dev_rx_offload((uint64_t)DPAA2_IOVA_TO_VADDR(
			     DPAA2_GET_FD_ADDR(fd)) +
			     DPAA2_FD_PTA_SIZE, mbuf);

	mbuf->next = NULL;
	rte_mbuf_refcnt_set(mbuf, 1);

	PMD_RX_LOG(DEBUG, "to mbuf - mbuf =%p, mbuf->buf_addr =%p, off = %d,"
		"fd_off=%d fd =%lx, meta = %d  bpid =%d, len=%d\n",
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

	/* First Prepare FD to be transmited*/
	/* Resetting the buffer pool id and offset field*/
	fd->simple.bpid_offset = 0;

	temp = rte_pktmbuf_alloc(mbuf->pool);
	if (temp == NULL) {
		PMD_TX_LOG(ERR, "No memory to allocate S/G table");
		return -ENOMEM;
	}

	DPAA2_SET_FD_ADDR(fd, DPAA2_MBUF_VADDR_TO_IOVA(temp));
	DPAA2_SET_FD_LEN(fd, mbuf->pkt_len);
	DPAA2_SET_FD_OFFSET(fd, temp->data_off);
	DPAA2_SET_FD_BPID(fd, bpid);
	DPAA2_SET_FD_ASAL(fd, DPAA2_ASAL_VAL);
	DPAA2_FD_SET_FORMAT(fd, qbman_fd_sg);
	/*Set Scatter gather table and Scatter gather entries*/
	sgt = (struct qbman_sge *)(
			(uint64_t)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd))
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
	/*Resetting the buffer pool id and offset field*/
	fd->simple.bpid_offset = 0;

	DPAA2_SET_FD_ADDR(fd, DPAA2_MBUF_VADDR_TO_IOVA(mbuf));
	DPAA2_SET_FD_LEN(fd, mbuf->data_len);
	DPAA2_SET_FD_BPID(fd, bpid);
	DPAA2_SET_FD_OFFSET(fd, mbuf->data_off);
	DPAA2_SET_FD_ASAL(fd, DPAA2_ASAL_VAL);

	PMD_TX_LOG(DEBUG, "mbuf =%p, mbuf->buf_addr =%p, off = %d,"
		"fd_off=%d fd =%lx, meta = %d  bpid =%d, len=%d\n",
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
		PMD_TX_LOG(WARNING, "Unable to allocated DPAA2 buffer");
		rte_pktmbuf_free(mbuf);
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

	/*Resetting the buffer pool id and offset field*/
	fd->simple.bpid_offset = 0;

	DPAA2_SET_FD_ADDR(fd, DPAA2_MBUF_VADDR_TO_IOVA(m));
	DPAA2_SET_FD_LEN(fd, mbuf->data_len);
	DPAA2_SET_FD_BPID(fd, bpid);
	DPAA2_SET_FD_OFFSET(fd, mbuf->data_off);
	DPAA2_SET_FD_ASAL(fd, DPAA2_ASAL_VAL);

	PMD_TX_LOG(DEBUG, " mbuf %p BMAN buf addr %p",
		   (void *)mbuf, mbuf->buf_addr);

	PMD_TX_LOG(DEBUG, " fdaddr =%lx bpid =%d meta =%d off =%d, len =%d",
		   DPAA2_GET_FD_ADDR(fd),
		DPAA2_GET_FD_BPID(fd),
		rte_dpaa2_bpid_info[DPAA2_GET_FD_BPID(fd)].meta_data_size,
		DPAA2_GET_FD_OFFSET(fd),
		DPAA2_GET_FD_LEN(fd));
	/*free the original packet */
	rte_pktmbuf_free(mbuf);

	return 0;
}

uint16_t
dpaa2_dev_prefetch_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	/* Function receive frames for a given device and VQ*/
	struct dpaa2_queue *dpaa2_q = (struct dpaa2_queue *)queue;
	struct qbman_result *dq_storage;
	uint32_t fqid = dpaa2_q->fqid;
	int ret, num_rx = 0;
	uint8_t is_last = 0, status;
	struct qbman_swp *swp;
	const struct qbman_fd *fd[DPAA2_DQRR_RING_SIZE];
	struct qbman_pull_desc pulldesc;
	struct queue_storage_info_t *q_storage = dpaa2_q->q_storage;
	struct rte_eth_dev *dev = dpaa2_q->dev;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			RTE_LOG(ERR, PMD, "Failure in affining portal\n");
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;
	if (!q_storage->active_dqs) {
		q_storage->toggle = 0;
		dq_storage = q_storage->dq_storage[q_storage->toggle];
		qbman_pull_desc_clear(&pulldesc);
		qbman_pull_desc_set_numframes(&pulldesc,
					      (nb_pkts > DPAA2_DQRR_RING_SIZE) ?
					       DPAA2_DQRR_RING_SIZE : nb_pkts);
		qbman_pull_desc_set_fq(&pulldesc, fqid);
		qbman_pull_desc_set_storage(&pulldesc, dq_storage,
			(dma_addr_t)(DPAA2_VADDR_TO_IOVA(dq_storage)), 1);
		if (check_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index)) {
			while (!qbman_check_command_complete(swp,
			       get_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index)))
				;
			clear_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index);
		}
		while (1) {
			if (qbman_swp_pull(swp, &pulldesc)) {
				PMD_RX_LOG(WARNING, "VDQ command is not issued."
					   "QBMAN is busy\n");
				/* Portal was busy, try again */
				continue;
			}
			break;
		}
		q_storage->active_dqs = dq_storage;
		q_storage->active_dpio_id = DPAA2_PER_LCORE_DPIO->index;
		set_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index, dq_storage);
	}
	dq_storage = q_storage->active_dqs;
	/* Check if the previous issued command is completed.
	 * Also seems like the SWP is shared between the Ethernet Driver
	 * and the SEC driver.
	 */
	while (!qbman_check_command_complete(swp, dq_storage))
		;
	if (dq_storage == get_swp_active_dqs(q_storage->active_dpio_id))
		clear_swp_active_dqs(q_storage->active_dpio_id);
	while (!is_last) {
		/* Loop until the dq_storage is updated with
		 * new token by QBMAN
		 */
		while (!qbman_result_has_new_result(swp, dq_storage))
			;
		rte_prefetch0((void *)((uint64_t)(dq_storage + 1)));
		/* Check whether Last Pull command is Expired and
		 * setting Condition for Loop termination
		 */
		if (qbman_result_DQ_is_pull_complete(dq_storage)) {
			is_last = 1;
			/* Check for valid frame. */
			status = (uint8_t)qbman_result_DQ_flags(dq_storage);
			if (unlikely((status & QBMAN_DQ_STAT_VALIDFRAME) == 0))
				continue;
		}
		fd[num_rx] = qbman_result_DQ_fd(dq_storage);

		/* Prefetch Annotation address for the parse results */
		rte_prefetch0((void *)((uint64_t)DPAA2_GET_FD_ADDR(fd[num_rx])
				+ DPAA2_FD_PTA_SIZE + 16));

		if (unlikely(DPAA2_FD_GET_FORMAT(fd[num_rx]) == qbman_fd_sg))
			bufs[num_rx] = eth_sg_fd_to_mbuf(fd[num_rx]);
		else
			bufs[num_rx] = eth_fd_to_mbuf(fd[num_rx]);
		bufs[num_rx]->port = dev->data->port_id;

		if (dev->data->dev_conf.rxmode.hw_vlan_strip)
			rte_vlan_strip(bufs[num_rx]);

		dq_storage++;
		num_rx++;
	}

	if (check_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index)) {
		while (!qbman_check_command_complete(swp,
		       get_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index)))
			;
		clear_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index);
	}
	q_storage->toggle ^= 1;
	dq_storage = q_storage->dq_storage[q_storage->toggle];
	qbman_pull_desc_clear(&pulldesc);
	qbman_pull_desc_set_numframes(&pulldesc, DPAA2_DQRR_RING_SIZE);
	qbman_pull_desc_set_fq(&pulldesc, fqid);
	qbman_pull_desc_set_storage(&pulldesc, dq_storage,
			(dma_addr_t)(DPAA2_VADDR_TO_IOVA(dq_storage)), 1);
	/* Issue a volatile dequeue command. */
	while (1) {
		if (qbman_swp_pull(swp, &pulldesc)) {
			PMD_RX_LOG(WARNING, "VDQ command is not issued."
				   "QBMAN is busy\n");
			continue;
		}
		break;
	}
	q_storage->active_dqs = dq_storage;
	q_storage->active_dpio_id = DPAA2_PER_LCORE_DPIO->index;
	set_swp_active_dqs(DPAA2_PER_LCORE_DPIO->index, dq_storage);

	dpaa2_q->rx_pkts += num_rx;

	/* Return the total number of packets received to DPAA2 app */
	return num_rx;
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
	struct rte_eth_dev *dev = dpaa2_q->dev;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	if (unlikely(!DPAA2_PER_LCORE_DPIO)) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			RTE_LOG(ERR, PMD, "Failure in affining portal\n");
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	PMD_TX_LOG(DEBUG, "===> dev =%p, fqid =%d", dev, dpaa2_q->fqid);

	/*Prepare enqueue descriptor*/
	qbman_eq_desc_clear(&eqdesc);
	qbman_eq_desc_set_no_orp(&eqdesc, DPAA2_EQ_RESP_ERR_FQ);
	qbman_eq_desc_set_response(&eqdesc, 0, 0);
	qbman_eq_desc_set_qd(&eqdesc, priv->qdid,
			     dpaa2_q->flow_id, dpaa2_q->tc_index);

	/*Clear the unused FD fields before sending*/
	while (nb_pkts) {
		/*Check if the queue is congested*/
		retry_count = 0;
		if (qbman_result_SCN_state_in_mem(dpaa2_q->cscn)) {
			retry_count++;
			/* Retry for some time before giving up */
			if (retry_count > CONG_RETRY_COUNT)
				goto skip_tx;
		}

		frames_to_send = (nb_pkts >> 3) ? MAX_TX_RING_SLOTS : nb_pkts;

		for (loop = 0; loop < frames_to_send; loop++) {
			fd_arr[loop].simple.frc = 0;
			DPAA2_RESET_FD_CTRL((&fd_arr[loop]));
			DPAA2_SET_FD_FLC((&fd_arr[loop]), NULL);
			if (RTE_MBUF_DIRECT(*bufs)) {
				mp = (*bufs)->pool;
			} else {
				mi = rte_mbuf_from_indirect(*bufs);
				mp = mi->pool;
			}
			/* Not a hw_pkt pool allocated frame */
			if (!mp) {
				PMD_TX_LOG(ERR, "err: no bpool attached");
				goto skip_tx;
			}
			if (mp->ops_index != priv->bp_list->dpaa2_ops_index) {
				PMD_TX_LOG(ERR, "non hw offload bufffer ");
				/* alloc should be from the default buffer pool
				 * attached to this interface
				 */
				if (priv->bp_list) {
					bpid = priv->bp_list->buf_pool.bpid;
				} else {
					PMD_TX_LOG(ERR,
						   "err: no bpool attached");
					num_tx = 0;
					goto skip_tx;
				}
				if (unlikely((*bufs)->nb_segs > 1)) {
					PMD_TX_LOG(ERR, "S/G support not added"
						" for non hw offload buffer");
					goto skip_tx;
				}
				if (eth_copy_mbuf_to_fd(*bufs,
							&fd_arr[loop], bpid)) {
					bufs++;
					continue;
				}
			} else {
				bpid = mempool_to_bpid(mp);
				if (unlikely((*bufs)->nb_segs > 1)) {
					if (eth_mbuf_to_sg_fd(*bufs,
							&fd_arr[loop], bpid))
						goto skip_tx;
				} else {
					eth_mbuf_to_fd(*bufs,
						       &fd_arr[loop], bpid);
				}
			}
			bufs++;
		}
		loop = 0;
		while (loop < frames_to_send) {
			loop += qbman_swp_send_multiple(swp, &eqdesc,
					&fd_arr[loop], frames_to_send - loop);
		}

		num_tx += frames_to_send;
		dpaa2_q->tx_pkts += frames_to_send;
		nb_pkts -= frames_to_send;
	}
skip_tx:
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
