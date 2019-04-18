/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_prefetch.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_net.h>

#include "base/vmxnet3_defs.h"
#include "vmxnet3_ring.h"

#include "vmxnet3_logs.h"
#include "vmxnet3_ethdev.h"

#define	VMXNET3_TX_OFFLOAD_MASK	( \
		PKT_TX_VLAN_PKT | \
		PKT_TX_IPV6 |     \
		PKT_TX_IPV4 |     \
		PKT_TX_L4_MASK |  \
		PKT_TX_TCP_SEG)

#define	VMXNET3_TX_OFFLOAD_NOTSUP_MASK	\
	(PKT_TX_OFFLOAD_MASK ^ VMXNET3_TX_OFFLOAD_MASK)

static const uint32_t rxprod_reg[2] = {VMXNET3_REG_RXPROD, VMXNET3_REG_RXPROD2};

static int vmxnet3_post_rx_bufs(vmxnet3_rx_queue_t*, uint8_t);
static void vmxnet3_tq_tx_complete(vmxnet3_tx_queue_t *);
#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER_NOT_USED
static void vmxnet3_rxq_dump(struct vmxnet3_rx_queue *);
static void vmxnet3_txq_dump(struct vmxnet3_tx_queue *);
#endif

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER_NOT_USED
static void
vmxnet3_rxq_dump(struct vmxnet3_rx_queue *rxq)
{
	uint32_t avail = 0;

	if (rxq == NULL)
		return;

	PMD_RX_LOG(DEBUG,
		   "RXQ: cmd0 base : %p cmd1 base : %p comp ring base : %p.",
		   rxq->cmd_ring[0].base, rxq->cmd_ring[1].base, rxq->comp_ring.base);
	PMD_RX_LOG(DEBUG,
		   "RXQ: cmd0 basePA : 0x%lx cmd1 basePA : 0x%lx comp ring basePA : 0x%lx.",
		   (unsigned long)rxq->cmd_ring[0].basePA,
		   (unsigned long)rxq->cmd_ring[1].basePA,
		   (unsigned long)rxq->comp_ring.basePA);

	avail = vmxnet3_cmd_ring_desc_avail(&rxq->cmd_ring[0]);
	PMD_RX_LOG(DEBUG,
		   "RXQ:cmd0: size=%u; free=%u; next2proc=%u; queued=%u",
		   (uint32_t)rxq->cmd_ring[0].size, avail,
		   rxq->comp_ring.next2proc,
		   rxq->cmd_ring[0].size - avail);

	avail = vmxnet3_cmd_ring_desc_avail(&rxq->cmd_ring[1]);
	PMD_RX_LOG(DEBUG, "RXQ:cmd1 size=%u; free=%u; next2proc=%u; queued=%u",
		   (uint32_t)rxq->cmd_ring[1].size, avail, rxq->comp_ring.next2proc,
		   rxq->cmd_ring[1].size - avail);

}

static void
vmxnet3_txq_dump(struct vmxnet3_tx_queue *txq)
{
	uint32_t avail = 0;

	if (txq == NULL)
		return;

	PMD_TX_LOG(DEBUG, "TXQ: cmd base : %p comp ring base : %p data ring base : %p.",
		   txq->cmd_ring.base, txq->comp_ring.base, txq->data_ring.base);
	PMD_TX_LOG(DEBUG, "TXQ: cmd basePA : 0x%lx comp ring basePA : 0x%lx data ring basePA : 0x%lx.",
		   (unsigned long)txq->cmd_ring.basePA,
		   (unsigned long)txq->comp_ring.basePA,
		   (unsigned long)txq->data_ring.basePA);

	avail = vmxnet3_cmd_ring_desc_avail(&txq->cmd_ring);
	PMD_TX_LOG(DEBUG, "TXQ: size=%u; free=%u; next2proc=%u; queued=%u",
		   (uint32_t)txq->cmd_ring.size, avail,
		   txq->comp_ring.next2proc, txq->cmd_ring.size - avail);
}
#endif

static void
vmxnet3_tx_cmd_ring_release_mbufs(vmxnet3_cmd_ring_t *ring)
{
	while (ring->next2comp != ring->next2fill) {
		/* No need to worry about desc ownership, device is quiesced by now. */
		vmxnet3_buf_info_t *buf_info = ring->buf_info + ring->next2comp;

		if (buf_info->m) {
			rte_pktmbuf_free(buf_info->m);
			buf_info->m = NULL;
			buf_info->bufPA = 0;
			buf_info->len = 0;
		}
		vmxnet3_cmd_ring_adv_next2comp(ring);
	}
}

static void
vmxnet3_rx_cmd_ring_release_mbufs(vmxnet3_cmd_ring_t *ring)
{
	uint32_t i;

	for (i = 0; i < ring->size; i++) {
		/* No need to worry about desc ownership, device is quiesced by now. */
		vmxnet3_buf_info_t *buf_info = &ring->buf_info[i];

		if (buf_info->m) {
			rte_pktmbuf_free_seg(buf_info->m);
			buf_info->m = NULL;
			buf_info->bufPA = 0;
			buf_info->len = 0;
		}
		vmxnet3_cmd_ring_adv_next2comp(ring);
	}
}

static void
vmxnet3_cmd_ring_release(vmxnet3_cmd_ring_t *ring)
{
	rte_free(ring->buf_info);
	ring->buf_info = NULL;
}

void
vmxnet3_dev_tx_queue_release(void *txq)
{
	vmxnet3_tx_queue_t *tq = txq;

	if (tq != NULL) {
		/* Release mbufs */
		vmxnet3_tx_cmd_ring_release_mbufs(&tq->cmd_ring);
		/* Release the cmd_ring */
		vmxnet3_cmd_ring_release(&tq->cmd_ring);
		/* Release the memzone */
		rte_memzone_free(tq->mz);
		/* Release the queue */
		rte_free(tq);
	}
}

void
vmxnet3_dev_rx_queue_release(void *rxq)
{
	int i;
	vmxnet3_rx_queue_t *rq = rxq;

	if (rq != NULL) {
		/* Release mbufs */
		for (i = 0; i < VMXNET3_RX_CMDRING_SIZE; i++)
			vmxnet3_rx_cmd_ring_release_mbufs(&rq->cmd_ring[i]);

		/* Release both the cmd_rings */
		for (i = 0; i < VMXNET3_RX_CMDRING_SIZE; i++)
			vmxnet3_cmd_ring_release(&rq->cmd_ring[i]);

		/* Release the memzone */
		rte_memzone_free(rq->mz);

		/* Release the queue */
		rte_free(rq);
	}
}

static void
vmxnet3_dev_tx_queue_reset(void *txq)
{
	vmxnet3_tx_queue_t *tq = txq;
	struct vmxnet3_cmd_ring *ring = &tq->cmd_ring;
	struct vmxnet3_comp_ring *comp_ring = &tq->comp_ring;
	struct vmxnet3_data_ring *data_ring = &tq->data_ring;
	int size;

	if (tq != NULL) {
		/* Release the cmd_ring mbufs */
		vmxnet3_tx_cmd_ring_release_mbufs(&tq->cmd_ring);
	}

	/* Tx vmxnet rings structure initialization*/
	ring->next2fill = 0;
	ring->next2comp = 0;
	ring->gen = VMXNET3_INIT_GEN;
	comp_ring->next2proc = 0;
	comp_ring->gen = VMXNET3_INIT_GEN;

	size = sizeof(struct Vmxnet3_TxDesc) * ring->size;
	size += sizeof(struct Vmxnet3_TxCompDesc) * comp_ring->size;
	size += tq->txdata_desc_size * data_ring->size;

	memset(ring->base, 0, size);
}

static void
vmxnet3_dev_rx_queue_reset(void *rxq)
{
	int i;
	vmxnet3_rx_queue_t *rq = rxq;
	struct vmxnet3_hw *hw = rq->hw;
	struct vmxnet3_cmd_ring *ring0, *ring1;
	struct vmxnet3_comp_ring *comp_ring;
	struct vmxnet3_rx_data_ring *data_ring = &rq->data_ring;
	int size;

	/* Release both the cmd_rings mbufs */
	for (i = 0; i < VMXNET3_RX_CMDRING_SIZE; i++)
		vmxnet3_rx_cmd_ring_release_mbufs(&rq->cmd_ring[i]);

	ring0 = &rq->cmd_ring[0];
	ring1 = &rq->cmd_ring[1];
	comp_ring = &rq->comp_ring;

	/* Rx vmxnet rings structure initialization */
	ring0->next2fill = 0;
	ring1->next2fill = 0;
	ring0->next2comp = 0;
	ring1->next2comp = 0;
	ring0->gen = VMXNET3_INIT_GEN;
	ring1->gen = VMXNET3_INIT_GEN;
	comp_ring->next2proc = 0;
	comp_ring->gen = VMXNET3_INIT_GEN;

	size = sizeof(struct Vmxnet3_RxDesc) * (ring0->size + ring1->size);
	size += sizeof(struct Vmxnet3_RxCompDesc) * comp_ring->size;
	if (VMXNET3_VERSION_GE_3(hw) && rq->data_desc_size)
		size += rq->data_desc_size * data_ring->size;

	memset(ring0->base, 0, size);
}

void
vmxnet3_dev_clear_queues(struct rte_eth_dev *dev)
{
	unsigned i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct vmxnet3_tx_queue *txq = dev->data->tx_queues[i];

		if (txq != NULL) {
			txq->stopped = TRUE;
			vmxnet3_dev_tx_queue_reset(txq);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct vmxnet3_rx_queue *rxq = dev->data->rx_queues[i];

		if (rxq != NULL) {
			rxq->stopped = TRUE;
			vmxnet3_dev_rx_queue_reset(rxq);
		}
	}
}

static int
vmxnet3_unmap_pkt(uint16_t eop_idx, vmxnet3_tx_queue_t *txq)
{
	int completed = 0;
	struct rte_mbuf *mbuf;

	/* Release cmd_ring descriptor and free mbuf */
	RTE_ASSERT(txq->cmd_ring.base[eop_idx].txd.eop == 1);

	mbuf = txq->cmd_ring.buf_info[eop_idx].m;
	if (mbuf == NULL)
		rte_panic("EOP desc does not point to a valid mbuf");
	rte_pktmbuf_free(mbuf);

	txq->cmd_ring.buf_info[eop_idx].m = NULL;

	while (txq->cmd_ring.next2comp != eop_idx) {
		/* no out-of-order completion */
		RTE_ASSERT(txq->cmd_ring.base[txq->cmd_ring.next2comp].txd.cq == 0);
		vmxnet3_cmd_ring_adv_next2comp(&txq->cmd_ring);
		completed++;
	}

	/* Mark the txd for which tcd was generated as completed */
	vmxnet3_cmd_ring_adv_next2comp(&txq->cmd_ring);

	return completed + 1;
}

static void
vmxnet3_tq_tx_complete(vmxnet3_tx_queue_t *txq)
{
	int completed = 0;
	vmxnet3_comp_ring_t *comp_ring = &txq->comp_ring;
	struct Vmxnet3_TxCompDesc *tcd = (struct Vmxnet3_TxCompDesc *)
		(comp_ring->base + comp_ring->next2proc);

	while (tcd->gen == comp_ring->gen) {
		completed += vmxnet3_unmap_pkt(tcd->txdIdx, txq);

		vmxnet3_comp_ring_adv_next2proc(comp_ring);
		tcd = (struct Vmxnet3_TxCompDesc *)(comp_ring->base +
						    comp_ring->next2proc);
	}

	PMD_TX_LOG(DEBUG, "Processed %d tx comps & command descs.", completed);
}

uint16_t
vmxnet3_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts)
{
	int32_t ret;
	uint32_t i;
	uint64_t ol_flags;
	struct rte_mbuf *m;

	for (i = 0; i != nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		/* Non-TSO packet cannot occupy more than
		 * VMXNET3_MAX_TXD_PER_PKT TX descriptors.
		 */
		if ((ol_flags & PKT_TX_TCP_SEG) == 0 &&
				m->nb_segs > VMXNET3_MAX_TXD_PER_PKT) {
			rte_errno = -EINVAL;
			return i;
		}

		/* check that only supported TX offloads are requested. */
		if ((ol_flags & VMXNET3_TX_OFFLOAD_NOTSUP_MASK) != 0 ||
				(ol_flags & PKT_TX_L4_MASK) ==
				PKT_TX_SCTP_CKSUM) {
			rte_errno = -ENOTSUP;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
	}

	return i;
}

uint16_t
vmxnet3_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		  uint16_t nb_pkts)
{
	uint16_t nb_tx;
	vmxnet3_tx_queue_t *txq = tx_queue;
	struct vmxnet3_hw *hw = txq->hw;
	Vmxnet3_TxQueueCtrl *txq_ctrl = &txq->shared->ctrl;
	uint32_t deferred = rte_le_to_cpu_32(txq_ctrl->txNumDeferred);

	if (unlikely(txq->stopped)) {
		PMD_TX_LOG(DEBUG, "Tx queue is stopped.");
		return 0;
	}

	/* Free up the comp_descriptors aggressively */
	vmxnet3_tq_tx_complete(txq);

	nb_tx = 0;
	while (nb_tx < nb_pkts) {
		Vmxnet3_GenericDesc *gdesc;
		vmxnet3_buf_info_t *tbi;
		uint32_t first2fill, avail, dw2;
		struct rte_mbuf *txm = tx_pkts[nb_tx];
		struct rte_mbuf *m_seg = txm;
		int copy_size = 0;
		bool tso = (txm->ol_flags & PKT_TX_TCP_SEG) != 0;
		/* # of descriptors needed for a packet. */
		unsigned count = txm->nb_segs;

		avail = vmxnet3_cmd_ring_desc_avail(&txq->cmd_ring);
		if (count > avail) {
			/* Is command ring full? */
			if (unlikely(avail == 0)) {
				PMD_TX_LOG(DEBUG, "No free ring descriptors");
				txq->stats.tx_ring_full++;
				txq->stats.drop_total += (nb_pkts - nb_tx);
				break;
			}

			/* Command ring is not full but cannot handle the
			 * multi-segmented packet. Let's try the next packet
			 * in this case.
			 */
			PMD_TX_LOG(DEBUG, "Running out of ring descriptors "
				   "(avail %d needed %d)", avail, count);
			txq->stats.drop_total++;
			if (tso)
				txq->stats.drop_tso++;
			rte_pktmbuf_free(txm);
			nb_tx++;
			continue;
		}

		/* Drop non-TSO packet that is excessively fragmented */
		if (unlikely(!tso && count > VMXNET3_MAX_TXD_PER_PKT)) {
			PMD_TX_LOG(ERR, "Non-TSO packet cannot occupy more than %d tx "
				   "descriptors. Packet dropped.", VMXNET3_MAX_TXD_PER_PKT);
			txq->stats.drop_too_many_segs++;
			txq->stats.drop_total++;
			rte_pktmbuf_free(txm);
			nb_tx++;
			continue;
		}

		if (txm->nb_segs == 1 &&
		    rte_pktmbuf_pkt_len(txm) <= txq->txdata_desc_size) {
			struct Vmxnet3_TxDataDesc *tdd;

			/* Skip empty packets */
			if (unlikely(rte_pktmbuf_pkt_len(txm) == 0)) {
				txq->stats.drop_total++;
				rte_pktmbuf_free(txm);
				nb_tx++;
				continue;
			}

			tdd = (struct Vmxnet3_TxDataDesc *)
				((uint8 *)txq->data_ring.base +
				 txq->cmd_ring.next2fill *
				 txq->txdata_desc_size);
			copy_size = rte_pktmbuf_pkt_len(txm);
			rte_memcpy(tdd->data, rte_pktmbuf_mtod(txm, char *), copy_size);
		}

		/* use the previous gen bit for the SOP desc */
		dw2 = (txq->cmd_ring.gen ^ 0x1) << VMXNET3_TXD_GEN_SHIFT;
		first2fill = txq->cmd_ring.next2fill;
		do {
			/* Remember the transmit buffer for cleanup */
			tbi = txq->cmd_ring.buf_info + txq->cmd_ring.next2fill;

			/* NB: the following assumes that VMXNET3 maximum
			 * transmit buffer size (16K) is greater than
			 * maximum size of mbuf segment size.
			 */
			gdesc = txq->cmd_ring.base + txq->cmd_ring.next2fill;

			/* Skip empty segments */
			if (unlikely(m_seg->data_len == 0))
				continue;

			if (copy_size) {
				uint64 offset =
					(uint64)txq->cmd_ring.next2fill *
							txq->txdata_desc_size;
				gdesc->txd.addr =
					rte_cpu_to_le_64(txq->data_ring.basePA +
							 offset);
			} else {
				gdesc->txd.addr = rte_mbuf_data_iova(m_seg);
			}

			gdesc->dword[2] = dw2 | m_seg->data_len;
			gdesc->dword[3] = 0;

			/* move to the next2fill descriptor */
			vmxnet3_cmd_ring_adv_next2fill(&txq->cmd_ring);

			/* use the right gen for non-SOP desc */
			dw2 = txq->cmd_ring.gen << VMXNET3_TXD_GEN_SHIFT;
		} while ((m_seg = m_seg->next) != NULL);

		/* set the last buf_info for the pkt */
		tbi->m = txm;
		/* Update the EOP descriptor */
		gdesc->dword[3] |= VMXNET3_TXD_EOP | VMXNET3_TXD_CQ;

		/* Add VLAN tag if present */
		gdesc = txq->cmd_ring.base + first2fill;
		if (txm->ol_flags & PKT_TX_VLAN_PKT) {
			gdesc->txd.ti = 1;
			gdesc->txd.tci = txm->vlan_tci;
		}

		if (tso) {
			uint16_t mss = txm->tso_segsz;

			RTE_ASSERT(mss > 0);

			gdesc->txd.hlen = txm->l2_len + txm->l3_len + txm->l4_len;
			gdesc->txd.om = VMXNET3_OM_TSO;
			gdesc->txd.msscof = mss;

			deferred += (rte_pktmbuf_pkt_len(txm) - gdesc->txd.hlen + mss - 1) / mss;
		} else if (txm->ol_flags & PKT_TX_L4_MASK) {
			gdesc->txd.om = VMXNET3_OM_CSUM;
			gdesc->txd.hlen = txm->l2_len + txm->l3_len;

			switch (txm->ol_flags & PKT_TX_L4_MASK) {
			case PKT_TX_TCP_CKSUM:
				gdesc->txd.msscof = gdesc->txd.hlen + offsetof(struct tcp_hdr, cksum);
				break;
			case PKT_TX_UDP_CKSUM:
				gdesc->txd.msscof = gdesc->txd.hlen + offsetof(struct udp_hdr, dgram_cksum);
				break;
			default:
				PMD_TX_LOG(WARNING, "requested cksum offload not supported %#llx",
					   txm->ol_flags & PKT_TX_L4_MASK);
				abort();
			}
			deferred++;
		} else {
			gdesc->txd.hlen = 0;
			gdesc->txd.om = VMXNET3_OM_NONE;
			gdesc->txd.msscof = 0;
			deferred++;
		}

		/* flip the GEN bit on the SOP */
		rte_compiler_barrier();
		gdesc->dword[2] ^= VMXNET3_TXD_GEN;

		txq_ctrl->txNumDeferred = rte_cpu_to_le_32(deferred);
		nb_tx++;
	}

	PMD_TX_LOG(DEBUG, "vmxnet3 txThreshold: %u", rte_le_to_cpu_32(txq_ctrl->txThreshold));

	if (deferred >= rte_le_to_cpu_32(txq_ctrl->txThreshold)) {
		txq_ctrl->txNumDeferred = 0;
		/* Notify vSwitch that packets are available. */
		VMXNET3_WRITE_BAR0_REG(hw, (VMXNET3_REG_TXPROD + txq->queue_id * VMXNET3_REG_ALIGN),
				       txq->cmd_ring.next2fill);
	}

	return nb_tx;
}

static inline void
vmxnet3_renew_desc(vmxnet3_rx_queue_t *rxq, uint8_t ring_id,
		   struct rte_mbuf *mbuf)
{
	uint32_t val;
	struct vmxnet3_cmd_ring *ring = &rxq->cmd_ring[ring_id];
	struct Vmxnet3_RxDesc *rxd =
		(struct Vmxnet3_RxDesc *)(ring->base + ring->next2fill);
	vmxnet3_buf_info_t *buf_info = &ring->buf_info[ring->next2fill];

	if (ring_id == 0) {
		/* Usually: One HEAD type buf per packet
		 * val = (ring->next2fill % rxq->hw->bufs_per_pkt) ?
		 * VMXNET3_RXD_BTYPE_BODY : VMXNET3_RXD_BTYPE_HEAD;
		 */

		/* We use single packet buffer so all heads here */
		val = VMXNET3_RXD_BTYPE_HEAD;
	} else {
		/* All BODY type buffers for 2nd ring */
		val = VMXNET3_RXD_BTYPE_BODY;
	}

	/*
	 * Load mbuf pointer into buf_info[ring_size]
	 * buf_info structure is equivalent to cookie for virtio-virtqueue
	 */
	buf_info->m = mbuf;
	buf_info->len = (uint16_t)(mbuf->buf_len - RTE_PKTMBUF_HEADROOM);
	buf_info->bufPA = rte_mbuf_data_iova_default(mbuf);

	/* Load Rx Descriptor with the buffer's GPA */
	rxd->addr = buf_info->bufPA;

	/* After this point rxd->addr MUST not be NULL */
	rxd->btype = val;
	rxd->len = buf_info->len;
	/* Flip gen bit at the end to change ownership */
	rxd->gen = ring->gen;

	vmxnet3_cmd_ring_adv_next2fill(ring);
}
/*
 *  Allocates mbufs and clusters. Post rx descriptors with buffer details
 *  so that device can receive packets in those buffers.
 *  Ring layout:
 *      Among the two rings, 1st ring contains buffers of type 0 and type 1.
 *      bufs_per_pkt is set such that for non-LRO cases all the buffers required
 *      by a frame will fit in 1st ring (1st buf of type0 and rest of type1).
 *      2nd ring contains buffers of type 1 alone. Second ring mostly be used
 *      only for LRO.
 */
static int
vmxnet3_post_rx_bufs(vmxnet3_rx_queue_t *rxq, uint8_t ring_id)
{
	int err = 0;
	uint32_t i = 0;
	struct vmxnet3_cmd_ring *ring = &rxq->cmd_ring[ring_id];

	while (vmxnet3_cmd_ring_desc_avail(ring) > 0) {
		struct rte_mbuf *mbuf;

		/* Allocate blank mbuf for the current Rx Descriptor */
		mbuf = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(mbuf == NULL)) {
			PMD_RX_LOG(ERR, "Error allocating mbuf");
			rxq->stats.rx_buf_alloc_failure++;
			err = ENOMEM;
			break;
		}

		vmxnet3_renew_desc(rxq, ring_id, mbuf);
		i++;
	}

	/* Return error only if no buffers are posted at present */
	if (vmxnet3_cmd_ring_desc_avail(ring) >= (ring->size - 1))
		return -err;
	else
		return i;
}

/* MSS not provided by vmxnet3, guess one with available information */
static uint16_t
vmxnet3_guess_mss(struct vmxnet3_hw *hw, const Vmxnet3_RxCompDesc *rcd,
		struct rte_mbuf *rxm)
{
	uint32_t hlen, slen;
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	struct tcp_hdr *tcp_hdr;
	char *ptr;

	RTE_ASSERT(rcd->tcp);

	ptr = rte_pktmbuf_mtod(rxm, char *);
	slen = rte_pktmbuf_data_len(rxm);
	hlen = sizeof(struct ether_hdr);

	if (rcd->v4) {
		if (unlikely(slen < hlen + sizeof(struct ipv4_hdr)))
			return hw->mtu - sizeof(struct ipv4_hdr)
					- sizeof(struct tcp_hdr);

		ipv4_hdr = (struct ipv4_hdr *)(ptr + hlen);
		hlen += (ipv4_hdr->version_ihl & IPV4_HDR_IHL_MASK) *
				IPV4_IHL_MULTIPLIER;
	} else if (rcd->v6) {
		if (unlikely(slen < hlen + sizeof(struct ipv6_hdr)))
			return hw->mtu - sizeof(struct ipv6_hdr) -
					sizeof(struct tcp_hdr);

		ipv6_hdr = (struct ipv6_hdr *)(ptr + hlen);
		hlen += sizeof(struct ipv6_hdr);
		if (unlikely(ipv6_hdr->proto != IPPROTO_TCP)) {
			int frag;

			rte_net_skip_ip6_ext(ipv6_hdr->proto, rxm,
					&hlen, &frag);
		}
	}

	if (unlikely(slen < hlen + sizeof(struct tcp_hdr)))
		return hw->mtu - hlen - sizeof(struct tcp_hdr) +
				sizeof(struct ether_hdr);

	tcp_hdr = (struct tcp_hdr *)(ptr + hlen);
	hlen += (tcp_hdr->data_off & 0xf0) >> 2;

	if (rxm->udata64 > 1)
		return (rte_pktmbuf_pkt_len(rxm) - hlen +
				rxm->udata64 - 1) / rxm->udata64;
	else
		return hw->mtu - hlen + sizeof(struct ether_hdr);
}

/* Receive side checksum and other offloads */
static inline void
vmxnet3_rx_offload(struct vmxnet3_hw *hw, const Vmxnet3_RxCompDesc *rcd,
		struct rte_mbuf *rxm, const uint8_t sop)
{
	uint64_t ol_flags = rxm->ol_flags;
	uint32_t packet_type = rxm->packet_type;

	/* Offloads set in sop */
	if (sop) {
		/* Set packet type */
		packet_type |= RTE_PTYPE_L2_ETHER;

		/* Check large packet receive */
		if (VMXNET3_VERSION_GE_2(hw) &&
		    rcd->type == VMXNET3_CDTYPE_RXCOMP_LRO) {
			const Vmxnet3_RxCompDescExt *rcde =
					(const Vmxnet3_RxCompDescExt *)rcd;

			rxm->tso_segsz = rcde->mss;
			rxm->udata64 = rcde->segCnt;
			ol_flags |= PKT_RX_LRO;
		}
	} else { /* Offloads set in eop */
		/* Check for RSS */
		if (rcd->rssType != VMXNET3_RCD_RSS_TYPE_NONE) {
			ol_flags |= PKT_RX_RSS_HASH;
			rxm->hash.rss = rcd->rssHash;
		}

		/* Check for hardware stripped VLAN tag */
		if (rcd->ts) {
			ol_flags |= (PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED);
			rxm->vlan_tci = rte_le_to_cpu_16((uint16_t)rcd->tci);
		}

		/* Check packet type, checksum errors, etc. */
		if (rcd->cnc) {
			ol_flags |= PKT_RX_L4_CKSUM_UNKNOWN;
		} else {
			if (rcd->v4) {
				packet_type |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;

				if (rcd->ipc)
					ol_flags |= PKT_RX_IP_CKSUM_GOOD;
				else
					ol_flags |= PKT_RX_IP_CKSUM_BAD;

				if (rcd->tuc) {
					ol_flags |= PKT_RX_L4_CKSUM_GOOD;
					if (rcd->tcp)
						packet_type |= RTE_PTYPE_L4_TCP;
					else
						packet_type |= RTE_PTYPE_L4_UDP;
				} else {
					if (rcd->tcp) {
						packet_type |= RTE_PTYPE_L4_TCP;
						ol_flags |= PKT_RX_L4_CKSUM_BAD;
					} else if (rcd->udp) {
						packet_type |= RTE_PTYPE_L4_UDP;
						ol_flags |= PKT_RX_L4_CKSUM_BAD;
					}
				}
			} else if (rcd->v6) {
				packet_type |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;

				if (rcd->tuc) {
					ol_flags |= PKT_RX_L4_CKSUM_GOOD;
					if (rcd->tcp)
						packet_type |= RTE_PTYPE_L4_TCP;
					else
						packet_type |= RTE_PTYPE_L4_UDP;
				} else {
					if (rcd->tcp) {
						packet_type |= RTE_PTYPE_L4_TCP;
						ol_flags |= PKT_RX_L4_CKSUM_BAD;
					} else if (rcd->udp) {
						packet_type |= RTE_PTYPE_L4_UDP;
						ol_flags |= PKT_RX_L4_CKSUM_BAD;
					}
				}
			} else {
				packet_type |= RTE_PTYPE_UNKNOWN;
			}

			/* Old variants of vmxnet3 do not provide MSS */
			if ((ol_flags & PKT_RX_LRO) && rxm->tso_segsz == 0)
				rxm->tso_segsz = vmxnet3_guess_mss(hw,
						rcd, rxm);
		}
	}

	rxm->ol_flags = ol_flags;
	rxm->packet_type = packet_type;
}

/*
 * Process the Rx Completion Ring of given vmxnet3_rx_queue
 * for nb_pkts burst and return the number of packets received
 */
uint16_t
vmxnet3_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	uint16_t nb_rx;
	uint32_t nb_rxd, idx;
	uint8_t ring_idx;
	vmxnet3_rx_queue_t *rxq;
	Vmxnet3_RxCompDesc *rcd;
	vmxnet3_buf_info_t *rbi;
	Vmxnet3_RxDesc *rxd;
	struct rte_mbuf *rxm = NULL;
	struct vmxnet3_hw *hw;

	nb_rx = 0;
	ring_idx = 0;
	nb_rxd = 0;
	idx = 0;

	rxq = rx_queue;
	hw = rxq->hw;

	rcd = &rxq->comp_ring.base[rxq->comp_ring.next2proc].rcd;

	if (unlikely(rxq->stopped)) {
		PMD_RX_LOG(DEBUG, "Rx queue is stopped.");
		return 0;
	}

	while (rcd->gen == rxq->comp_ring.gen) {
		struct rte_mbuf *newm;

		if (nb_rx >= nb_pkts)
			break;

		newm = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(newm == NULL)) {
			PMD_RX_LOG(ERR, "Error allocating mbuf");
			rxq->stats.rx_buf_alloc_failure++;
			break;
		}

		idx = rcd->rxdIdx;
		ring_idx = vmxnet3_get_ring_idx(hw, rcd->rqID);
		rxd = (Vmxnet3_RxDesc *)rxq->cmd_ring[ring_idx].base + idx;
		RTE_SET_USED(rxd); /* used only for assert when enabled */
		rbi = rxq->cmd_ring[ring_idx].buf_info + idx;

		PMD_RX_LOG(DEBUG, "rxd idx: %d ring idx: %d.", idx, ring_idx);

		RTE_ASSERT(rcd->len <= rxd->len);
		RTE_ASSERT(rbi->m);

		/* Get the packet buffer pointer from buf_info */
		rxm = rbi->m;

		/* Clear descriptor associated buf_info to be reused */
		rbi->m = NULL;
		rbi->bufPA = 0;

		/* Update the index that we received a packet */
		rxq->cmd_ring[ring_idx].next2comp = idx;

		/* For RCD with EOP set, check if there is frame error */
		if (unlikely(rcd->eop && rcd->err)) {
			rxq->stats.drop_total++;
			rxq->stats.drop_err++;

			if (!rcd->fcs) {
				rxq->stats.drop_fcs++;
				PMD_RX_LOG(ERR, "Recv packet dropped due to frame err.");
			}
			PMD_RX_LOG(ERR, "Error in received packet rcd#:%d rxd:%d",
				   (int)(rcd - (struct Vmxnet3_RxCompDesc *)
					 rxq->comp_ring.base), rcd->rxdIdx);
			rte_pktmbuf_free_seg(rxm);
			if (rxq->start_seg) {
				struct rte_mbuf *start = rxq->start_seg;

				rxq->start_seg = NULL;
				rte_pktmbuf_free(start);
			}
			goto rcd_done;
		}

		/* Initialize newly received packet buffer */
		rxm->port = rxq->port_id;
		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = (uint16_t)rcd->len;
		rxm->data_len = (uint16_t)rcd->len;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->ol_flags = 0;
		rxm->vlan_tci = 0;
		rxm->packet_type = 0;

		/*
		 * If this is the first buffer of the received packet,
		 * set the pointer to the first mbuf of the packet
		 * Otherwise, update the total length and the number of segments
		 * of the current scattered packet, and update the pointer to
		 * the last mbuf of the current packet.
		 */
		if (rcd->sop) {
			RTE_ASSERT(rxd->btype == VMXNET3_RXD_BTYPE_HEAD);

			if (unlikely(rcd->len == 0)) {
				RTE_ASSERT(rcd->eop);

				PMD_RX_LOG(DEBUG,
					   "Rx buf was skipped. rxring[%d][%d])",
					   ring_idx, idx);
				rte_pktmbuf_free_seg(rxm);
				goto rcd_done;
			}

			if (vmxnet3_rx_data_ring(hw, rcd->rqID)) {
				uint8_t *rdd = rxq->data_ring.base +
					idx * rxq->data_desc_size;

				RTE_ASSERT(VMXNET3_VERSION_GE_3(hw));
				rte_memcpy(rte_pktmbuf_mtod(rxm, char *),
					   rdd, rcd->len);
			}

			rxq->start_seg = rxm;
			rxq->last_seg = rxm;
			vmxnet3_rx_offload(hw, rcd, rxm, 1);
		} else {
			struct rte_mbuf *start = rxq->start_seg;

			RTE_ASSERT(rxd->btype == VMXNET3_RXD_BTYPE_BODY);

			if (rxm->data_len) {
				start->pkt_len += rxm->data_len;
				start->nb_segs++;

				rxq->last_seg->next = rxm;
				rxq->last_seg = rxm;
			} else {
				rte_pktmbuf_free_seg(rxm);
			}
		}

		if (rcd->eop) {
			struct rte_mbuf *start = rxq->start_seg;

			vmxnet3_rx_offload(hw, rcd, start, 0);
			rx_pkts[nb_rx++] = start;
			rxq->start_seg = NULL;
		}

rcd_done:
		rxq->cmd_ring[ring_idx].next2comp = idx;
		VMXNET3_INC_RING_IDX_ONLY(rxq->cmd_ring[ring_idx].next2comp,
					  rxq->cmd_ring[ring_idx].size);

		/* It's time to renew descriptors */
		vmxnet3_renew_desc(rxq, ring_idx, newm);
		if (unlikely(rxq->shared->ctrl.updateRxProd)) {
			VMXNET3_WRITE_BAR0_REG(hw, rxprod_reg[ring_idx] + (rxq->queue_id * VMXNET3_REG_ALIGN),
					       rxq->cmd_ring[ring_idx].next2fill);
		}

		/* Advance to the next descriptor in comp_ring */
		vmxnet3_comp_ring_adv_next2proc(&rxq->comp_ring);

		rcd = &rxq->comp_ring.base[rxq->comp_ring.next2proc].rcd;
		nb_rxd++;
		if (nb_rxd > rxq->cmd_ring[0].size) {
			PMD_RX_LOG(ERR, "Used up quota of receiving packets,"
				   " relinquish control.");
			break;
		}
	}

	if (unlikely(nb_rxd == 0)) {
		uint32_t avail;
		for (ring_idx = 0; ring_idx < VMXNET3_RX_CMDRING_SIZE; ring_idx++) {
			avail = vmxnet3_cmd_ring_desc_avail(&rxq->cmd_ring[ring_idx]);
			if (unlikely(avail > 0)) {
				/* try to alloc new buf and renew descriptors */
				vmxnet3_post_rx_bufs(rxq, ring_idx);
			}
		}
		if (unlikely(rxq->shared->ctrl.updateRxProd)) {
			for (ring_idx = 0; ring_idx < VMXNET3_RX_CMDRING_SIZE; ring_idx++) {
				VMXNET3_WRITE_BAR0_REG(hw, rxprod_reg[ring_idx] + (rxq->queue_id * VMXNET3_REG_ALIGN),
						       rxq->cmd_ring[ring_idx].next2fill);
			}
		}
	}

	return nb_rx;
}

int
vmxnet3_dev_tx_queue_setup(struct rte_eth_dev *dev,
			   uint16_t queue_idx,
			   uint16_t nb_desc,
			   unsigned int socket_id,
			   const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	const struct rte_memzone *mz;
	struct vmxnet3_tx_queue *txq;
	struct vmxnet3_cmd_ring *ring;
	struct vmxnet3_comp_ring *comp_ring;
	struct vmxnet3_data_ring *data_ring;
	int size;

	PMD_INIT_FUNC_TRACE();

	txq = rte_zmalloc("ethdev_tx_queue", sizeof(struct vmxnet3_tx_queue),
			  RTE_CACHE_LINE_SIZE);
	if (txq == NULL) {
		PMD_INIT_LOG(ERR, "Can not allocate tx queue structure");
		return -ENOMEM;
	}

	txq->queue_id = queue_idx;
	txq->port_id = dev->data->port_id;
	txq->shared = NULL; /* set in vmxnet3_setup_driver_shared() */
	txq->hw = hw;
	txq->qid = queue_idx;
	txq->stopped = TRUE;
	txq->txdata_desc_size = hw->txdata_desc_size;

	ring = &txq->cmd_ring;
	comp_ring = &txq->comp_ring;
	data_ring = &txq->data_ring;

	/* Tx vmxnet ring length should be between 512-4096 */
	if (nb_desc < VMXNET3_DEF_TX_RING_SIZE) {
		PMD_INIT_LOG(ERR, "VMXNET3 Tx Ring Size Min: %u",
			     VMXNET3_DEF_TX_RING_SIZE);
		return -EINVAL;
	} else if (nb_desc > VMXNET3_TX_RING_MAX_SIZE) {
		PMD_INIT_LOG(ERR, "VMXNET3 Tx Ring Size Max: %u",
			     VMXNET3_TX_RING_MAX_SIZE);
		return -EINVAL;
	} else {
		ring->size = nb_desc;
		ring->size &= ~VMXNET3_RING_SIZE_MASK;
	}
	comp_ring->size = data_ring->size = ring->size;

	/* Tx vmxnet rings structure initialization*/
	ring->next2fill = 0;
	ring->next2comp = 0;
	ring->gen = VMXNET3_INIT_GEN;
	comp_ring->next2proc = 0;
	comp_ring->gen = VMXNET3_INIT_GEN;

	size = sizeof(struct Vmxnet3_TxDesc) * ring->size;
	size += sizeof(struct Vmxnet3_TxCompDesc) * comp_ring->size;
	size += txq->txdata_desc_size * data_ring->size;

	mz = rte_eth_dma_zone_reserve(dev, "txdesc", queue_idx, size,
				      VMXNET3_RING_BA_ALIGN, socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating queue descriptors zone");
		return -ENOMEM;
	}
	txq->mz = mz;
	memset(mz->addr, 0, mz->len);

	/* cmd_ring initialization */
	ring->base = mz->addr;
	ring->basePA = mz->iova;

	/* comp_ring initialization */
	comp_ring->base = ring->base + ring->size;
	comp_ring->basePA = ring->basePA +
		(sizeof(struct Vmxnet3_TxDesc) * ring->size);

	/* data_ring initialization */
	data_ring->base = (Vmxnet3_TxDataDesc *)(comp_ring->base + comp_ring->size);
	data_ring->basePA = comp_ring->basePA +
			(sizeof(struct Vmxnet3_TxCompDesc) * comp_ring->size);

	/* cmd_ring0 buf_info allocation */
	ring->buf_info = rte_zmalloc("tx_ring_buf_info",
				     ring->size * sizeof(vmxnet3_buf_info_t), RTE_CACHE_LINE_SIZE);
	if (ring->buf_info == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating tx_buf_info structure");
		return -ENOMEM;
	}

	/* Update the data portion with txq */
	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

int
vmxnet3_dev_rx_queue_setup(struct rte_eth_dev *dev,
			   uint16_t queue_idx,
			   uint16_t nb_desc,
			   unsigned int socket_id,
			   __rte_unused const struct rte_eth_rxconf *rx_conf,
			   struct rte_mempool *mp)
{
	const struct rte_memzone *mz;
	struct vmxnet3_rx_queue *rxq;
	struct vmxnet3_hw *hw = dev->data->dev_private;
	struct vmxnet3_cmd_ring *ring0, *ring1, *ring;
	struct vmxnet3_comp_ring *comp_ring;
	struct vmxnet3_rx_data_ring *data_ring;
	int size;
	uint8_t i;
	char mem_name[32];

	PMD_INIT_FUNC_TRACE();

	rxq = rte_zmalloc("ethdev_rx_queue", sizeof(struct vmxnet3_rx_queue),
			  RTE_CACHE_LINE_SIZE);
	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Can not allocate rx queue structure");
		return -ENOMEM;
	}

	rxq->mp = mp;
	rxq->queue_id = queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->shared = NULL; /* set in vmxnet3_setup_driver_shared() */
	rxq->hw = hw;
	rxq->qid1 = queue_idx;
	rxq->qid2 = queue_idx + hw->num_rx_queues;
	rxq->data_ring_qid = queue_idx + 2 * hw->num_rx_queues;
	rxq->data_desc_size = hw->rxdata_desc_size;
	rxq->stopped = TRUE;

	ring0 = &rxq->cmd_ring[0];
	ring1 = &rxq->cmd_ring[1];
	comp_ring = &rxq->comp_ring;
	data_ring = &rxq->data_ring;

	/* Rx vmxnet rings length should be between 256-4096 */
	if (nb_desc < VMXNET3_DEF_RX_RING_SIZE) {
		PMD_INIT_LOG(ERR, "VMXNET3 Rx Ring Size Min: 256");
		return -EINVAL;
	} else if (nb_desc > VMXNET3_RX_RING_MAX_SIZE) {
		PMD_INIT_LOG(ERR, "VMXNET3 Rx Ring Size Max: 4096");
		return -EINVAL;
	} else {
		ring0->size = nb_desc;
		ring0->size &= ~VMXNET3_RING_SIZE_MASK;
		ring1->size = ring0->size;
	}

	comp_ring->size = ring0->size + ring1->size;
	data_ring->size = ring0->size;

	/* Rx vmxnet rings structure initialization */
	ring0->next2fill = 0;
	ring1->next2fill = 0;
	ring0->next2comp = 0;
	ring1->next2comp = 0;
	ring0->gen = VMXNET3_INIT_GEN;
	ring1->gen = VMXNET3_INIT_GEN;
	comp_ring->next2proc = 0;
	comp_ring->gen = VMXNET3_INIT_GEN;

	size = sizeof(struct Vmxnet3_RxDesc) * (ring0->size + ring1->size);
	size += sizeof(struct Vmxnet3_RxCompDesc) * comp_ring->size;
	if (VMXNET3_VERSION_GE_3(hw) && rxq->data_desc_size)
		size += rxq->data_desc_size * data_ring->size;

	mz = rte_eth_dma_zone_reserve(dev, "rxdesc", queue_idx, size,
				      VMXNET3_RING_BA_ALIGN, socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating queue descriptors zone");
		return -ENOMEM;
	}
	rxq->mz = mz;
	memset(mz->addr, 0, mz->len);

	/* cmd_ring0 initialization */
	ring0->base = mz->addr;
	ring0->basePA = mz->iova;

	/* cmd_ring1 initialization */
	ring1->base = ring0->base + ring0->size;
	ring1->basePA = ring0->basePA + sizeof(struct Vmxnet3_RxDesc) * ring0->size;

	/* comp_ring initialization */
	comp_ring->base = ring1->base + ring1->size;
	comp_ring->basePA = ring1->basePA + sizeof(struct Vmxnet3_RxDesc) *
		ring1->size;

	/* data_ring initialization */
	if (VMXNET3_VERSION_GE_3(hw) && rxq->data_desc_size) {
		data_ring->base =
			(uint8_t *)(comp_ring->base + comp_ring->size);
		data_ring->basePA = comp_ring->basePA +
			sizeof(struct Vmxnet3_RxCompDesc) * comp_ring->size;
	}

	/* cmd_ring0-cmd_ring1 buf_info allocation */
	for (i = 0; i < VMXNET3_RX_CMDRING_SIZE; i++) {

		ring = &rxq->cmd_ring[i];
		ring->rid = i;
		snprintf(mem_name, sizeof(mem_name), "rx_ring_%d_buf_info", i);

		ring->buf_info = rte_zmalloc(mem_name,
					     ring->size * sizeof(vmxnet3_buf_info_t),
					     RTE_CACHE_LINE_SIZE);
		if (ring->buf_info == NULL) {
			PMD_INIT_LOG(ERR, "ERROR: Creating rx_buf_info structure");
			return -ENOMEM;
		}
	}

	/* Update the data portion with rxq */
	dev->data->rx_queues[queue_idx] = rxq;

	return 0;
}

/*
 * Initializes Receive Unit
 * Load mbufs in rx queue in advance
 */
int
vmxnet3_dev_rxtx_init(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;

	int i, ret;
	uint8_t j;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < hw->num_rx_queues; i++) {
		vmxnet3_rx_queue_t *rxq = dev->data->rx_queues[i];

		for (j = 0; j < VMXNET3_RX_CMDRING_SIZE; j++) {
			/* Passing 0 as alloc_num will allocate full ring */
			ret = vmxnet3_post_rx_bufs(rxq, j);
			if (ret <= 0) {
				PMD_INIT_LOG(ERR,
					     "ERROR: Posting Rxq: %d buffers ring: %d",
					     i, j);
				return -ret;
			}
			/*
			 * Updating device with the index:next2fill to fill the
			 * mbufs for coming packets.
			 */
			if (unlikely(rxq->shared->ctrl.updateRxProd)) {
				VMXNET3_WRITE_BAR0_REG(hw, rxprod_reg[j] + (rxq->queue_id * VMXNET3_REG_ALIGN),
						       rxq->cmd_ring[j].next2fill);
			}
		}
		rxq->stopped = FALSE;
		rxq->start_seg = NULL;
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct vmxnet3_tx_queue *txq = dev->data->tx_queues[i];

		txq->stopped = FALSE;
	}

	return 0;
}

static uint8_t rss_intel_key[40] = {
	0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
	0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
	0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
	0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
	0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA,
};

/*
 * Additional RSS configurations based on vmxnet v4+ APIs
 */
int
vmxnet3_v4_rss_configure(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	Vmxnet3_DriverShared *shared = hw->shared;
	Vmxnet3_CmdInfo *cmdInfo = &shared->cu.cmdInfo;
	struct rte_eth_rss_conf *port_rss_conf;
	uint64_t rss_hf;
	uint32_t ret;

	PMD_INIT_FUNC_TRACE();

	cmdInfo->setRSSFields = 0;
	port_rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;
	rss_hf = port_rss_conf->rss_hf &
		(VMXNET3_V4_RSS_MASK | VMXNET3_RSS_OFFLOAD_ALL);

	if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
		cmdInfo->setRSSFields |= VMXNET3_RSS_FIELDS_TCPIP4;
	if (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)
		cmdInfo->setRSSFields |= VMXNET3_RSS_FIELDS_TCPIP6;
	if (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP)
		cmdInfo->setRSSFields |= VMXNET3_RSS_FIELDS_UDPIP4;
	if (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP)
		cmdInfo->setRSSFields |= VMXNET3_RSS_FIELDS_UDPIP6;

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD,
			       VMXNET3_CMD_SET_RSS_FIELDS);
	ret = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_CMD);

	if (ret != VMXNET3_SUCCESS) {
		PMD_DRV_LOG(ERR, "Set RSS fields (v4) failed: %d", ret);
	}

	return ret;
}

/*
 * Configure RSS feature
 */
int
vmxnet3_rss_configure(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = dev->data->dev_private;
	struct VMXNET3_RSSConf *dev_rss_conf;
	struct rte_eth_rss_conf *port_rss_conf;
	uint64_t rss_hf;
	uint8_t i, j;

	PMD_INIT_FUNC_TRACE();

	dev_rss_conf = hw->rss_conf;
	port_rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;

	/* loading hashFunc */
	dev_rss_conf->hashFunc = VMXNET3_RSS_HASH_FUNC_TOEPLITZ;
	/* loading hashKeySize */
	dev_rss_conf->hashKeySize = VMXNET3_RSS_MAX_KEY_SIZE;
	/* loading indTableSize: Must not exceed VMXNET3_RSS_MAX_IND_TABLE_SIZE (128)*/
	dev_rss_conf->indTableSize = (uint16_t)(hw->num_rx_queues * 4);

	if (port_rss_conf->rss_key == NULL) {
		/* Default hash key */
		port_rss_conf->rss_key = rss_intel_key;
	}

	/* loading hashKey */
	memcpy(&dev_rss_conf->hashKey[0], port_rss_conf->rss_key,
	       dev_rss_conf->hashKeySize);

	/* loading indTable */
	for (i = 0, j = 0; i < dev_rss_conf->indTableSize; i++, j++) {
		if (j == dev->data->nb_rx_queues)
			j = 0;
		dev_rss_conf->indTable[i] = j;
	}

	/* loading hashType */
	dev_rss_conf->hashType = 0;
	rss_hf = port_rss_conf->rss_hf & VMXNET3_RSS_OFFLOAD_ALL;
	if (rss_hf & ETH_RSS_IPV4)
		dev_rss_conf->hashType |= VMXNET3_RSS_HASH_TYPE_IPV4;
	if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
		dev_rss_conf->hashType |= VMXNET3_RSS_HASH_TYPE_TCP_IPV4;
	if (rss_hf & ETH_RSS_IPV6)
		dev_rss_conf->hashType |= VMXNET3_RSS_HASH_TYPE_IPV6;
	if (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)
		dev_rss_conf->hashType |= VMXNET3_RSS_HASH_TYPE_TCP_IPV6;

	return VMXNET3_SUCCESS;
}
