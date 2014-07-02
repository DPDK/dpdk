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

#include <sys/queue.h>

#include <endian.h>
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
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_prefetch.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_string_fns.h>
#include <rte_errno.h>

#include "vmxnet3/vmxnet3_defs.h"
#include "vmxnet3_ring.h"

#include "vmxnet3_logs.h"
#include "vmxnet3_ethdev.h"


#define RTE_MBUF_DATA_DMA_ADDR(mb) \
	(uint64_t) ((mb)->buf_physaddr + (uint64_t)((char *)((mb)->pkt.data) - \
	(char *)(mb)->buf_addr))

#define RTE_MBUF_DATA_DMA_ADDR_DEFAULT(mb) \
	(uint64_t) ((mb)->buf_physaddr + RTE_PKTMBUF_HEADROOM)

static uint32_t rxprod_reg[2] = {VMXNET3_REG_RXPROD, VMXNET3_REG_RXPROD2};

static inline int vmxnet3_post_rx_bufs(vmxnet3_rx_queue_t* , uint8_t);
static inline void vmxnet3_tq_tx_complete(vmxnet3_tx_queue_t *);
#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER_NOT_USED
static void vmxnet3_rxq_dump(struct vmxnet3_rx_queue *);
static void vmxnet3_txq_dump(struct vmxnet3_tx_queue *);
#endif

static inline struct rte_mbuf *
rte_rxmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;

	m = __rte_mbuf_raw_alloc(mp);
	__rte_mbuf_sanity_check_raw(m, RTE_MBUF_PKT, 0);
	return (m);
}

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER_NOT_USED
static void
vmxnet3_rxq_dump(struct vmxnet3_rx_queue *rxq)
{
	uint32_t avail = 0;
	if (rxq == NULL)
		return;

	PMD_RX_LOG(DEBUG, "RXQ: cmd0 base : 0x%p cmd1 base : 0x%p comp ring base : 0x%p.\n",
		        rxq->cmd_ring[0].base, rxq->cmd_ring[1].base, rxq->comp_ring.base);
	PMD_RX_LOG(DEBUG, "RXQ: cmd0 basePA : 0x%lx cmd1 basePA : 0x%lx comp ring basePA : 0x%lx.\n",
				(unsigned long)rxq->cmd_ring[0].basePA, (unsigned long)rxq->cmd_ring[1].basePA,
		        (unsigned long)rxq->comp_ring.basePA);

	avail = vmxnet3_cmd_ring_desc_avail(&rxq->cmd_ring[0]);
	PMD_RX_LOG(DEBUG, "RXQ:cmd0: size=%u; free=%u; next2proc=%u; queued=%u\n",
		    (uint32_t)rxq->cmd_ring[0].size, avail, rxq->comp_ring.next2proc,
		    rxq->cmd_ring[0].size - avail);

	avail = vmxnet3_cmd_ring_desc_avail(&rxq->cmd_ring[1]);
	PMD_RX_LOG(DEBUG, "RXQ:cmd1 size=%u; free=%u; next2proc=%u; queued=%u\n",
			(uint32_t)rxq->cmd_ring[1].size, avail, rxq->comp_ring.next2proc,
			rxq->cmd_ring[1].size - avail);

}

static void
vmxnet3_txq_dump(struct vmxnet3_tx_queue *txq)
{
	uint32_t avail = 0;
	if (txq == NULL)
		return;

	PMD_TX_LOG(DEBUG, "TXQ: cmd base : 0x%p comp ring base : 0x%p.\n",
		                txq->cmd_ring.base, txq->comp_ring.base);
	PMD_TX_LOG(DEBUG, "TXQ: cmd basePA : 0x%lx comp ring basePA : 0x%lx.\n",
		                (unsigned long)txq->cmd_ring.basePA, (unsigned long)txq->comp_ring.basePA);

	avail = vmxnet3_cmd_ring_desc_avail(&txq->cmd_ring);
	PMD_TX_LOG(DEBUG, "TXQ: size=%u; free=%u; next2proc=%u; queued=%u\n",
			(uint32_t)txq->cmd_ring.size, avail,
			txq->comp_ring.next2proc, txq->cmd_ring.size - avail);
}
#endif

static inline void
vmxnet3_cmd_ring_release(vmxnet3_cmd_ring_t *ring)
{
	while (ring->next2comp != ring->next2fill) {
		/* No need to worry about tx desc ownership, device is quiesced by now. */
		vmxnet3_buf_info_t *buf_info = ring->buf_info + ring->next2comp;
		if(buf_info->m) {
			rte_pktmbuf_free(buf_info->m);
			buf_info->m = NULL;
			buf_info->bufPA = 0;
			buf_info->len = 0;
		}
		vmxnet3_cmd_ring_adv_next2comp(ring);
	}
	rte_free(ring->buf_info);
}

void
vmxnet3_dev_tx_queue_release(void *txq)
{
	vmxnet3_tx_queue_t *tq = txq;
	if (txq != NULL) {
		/* Release the cmd_ring */
		vmxnet3_cmd_ring_release(&tq->cmd_ring);
	}
}

void
vmxnet3_dev_rx_queue_release(void *rxq)
{
	int i;
	vmxnet3_rx_queue_t *rq = rxq;
	if (rxq != NULL) {
		/* Release both the cmd_rings */
		for (i = 0; i < VMXNET3_RX_CMDRING_SIZE; i++)
			vmxnet3_cmd_ring_release(&rq->cmd_ring[i]);
	}
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
			vmxnet3_dev_tx_queue_release(txq);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct vmxnet3_rx_queue *rxq = dev->data->rx_queues[i];
		if(rxq != NULL) {
			rxq->stopped = TRUE;
			vmxnet3_dev_rx_queue_release(rxq);
		}
	}
}

static inline void
vmxnet3_tq_tx_complete(vmxnet3_tx_queue_t *txq)
{
   int completed = 0;
   struct rte_mbuf *mbuf;
   vmxnet3_comp_ring_t *comp_ring = &txq->comp_ring;
   struct Vmxnet3_TxCompDesc *tcd = (struct Vmxnet3_TxCompDesc *)
                                    (comp_ring->base + comp_ring->next2proc);

   while (tcd->gen == comp_ring->gen) {

	   /* Release cmd_ring descriptor and free mbuf */
#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER
	    VMXNET3_ASSERT(txq->cmd_ring.base[tcd->txdIdx].txd.eop == 1);
#endif
	    mbuf = txq->cmd_ring.buf_info[tcd->txdIdx].m;
		if (unlikely(mbuf == NULL))
			rte_panic("EOP desc does not point to a valid mbuf");
		else
			rte_pktmbuf_free(mbuf);


		txq->cmd_ring.buf_info[tcd->txdIdx].m = NULL;
		/* Mark the txd for which tcd was generated as completed */
		vmxnet3_cmd_ring_adv_next2comp(&txq->cmd_ring);

		vmxnet3_comp_ring_adv_next2proc(comp_ring);
		tcd = (struct Vmxnet3_TxCompDesc *)(comp_ring->base +
										  comp_ring->next2proc);
		completed++;
   }

   PMD_TX_LOG(DEBUG, "Processed %d tx comps & command descs.\n", completed);
}

uint16_t
vmxnet3_xmit_pkts( void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	uint16_t nb_tx;
	Vmxnet3_TxDesc *txd = NULL;
	vmxnet3_buf_info_t *tbi = NULL;
	struct vmxnet3_hw *hw;
	struct rte_mbuf *txm;
	vmxnet3_tx_queue_t *txq = tx_queue;

	hw = txq->hw;

	if(txq->stopped) {
		PMD_TX_LOG(DEBUG, "Tx queue is stopped.\n");
		return 0;
	}

	/* Free up the comp_descriptors aggressively */
	vmxnet3_tq_tx_complete(txq);

	nb_tx = 0;
	while(nb_tx < nb_pkts) {

		if(vmxnet3_cmd_ring_desc_avail(&txq->cmd_ring)) {

			txm = tx_pkts[nb_tx];
			/* Don't support scatter packets yet, free them if met */
			if (txm->pkt.nb_segs != 1) {
				PMD_TX_LOG(DEBUG, "Don't support scatter packets yet, drop!\n");
				rte_pktmbuf_free(tx_pkts[nb_tx]);
				txq->stats.drop_total++;

				nb_tx++;
				continue;
			}

			/* Needs to minus ether header len */
			if(txm->pkt.data_len > (hw->cur_mtu + ETHER_HDR_LEN)) {
				PMD_TX_LOG(DEBUG, "Packet data_len higher than MTU\n");
				rte_pktmbuf_free(tx_pkts[nb_tx]);
				txq->stats.drop_total++;

				nb_tx++;
				continue;
			}

			txd = (Vmxnet3_TxDesc *)(txq->cmd_ring.base + txq->cmd_ring.next2fill);

			/* Fill the tx descriptor */
			tbi = txq->cmd_ring.buf_info + txq->cmd_ring.next2fill;
			tbi->bufPA = RTE_MBUF_DATA_DMA_ADDR(txm);
			txd->addr = tbi->bufPA;
			txd->len = txm->pkt.data_len;

			/* Mark the last descriptor as End of Packet. */
			txd->cq = 1;
			txd->eop = 1;

			/* Record current mbuf for freeing it later in tx complete */
#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER
			VMXNET3_ASSERT(txm);
#endif
			tbi->m = txm;

			/* Set the offloading mode to default */
			txd->hlen = 0;
			txd->om = VMXNET3_OM_NONE;
			txd->msscof = 0;

			/* finally flip the GEN bit of the SOP desc  */
			txd->gen = txq->cmd_ring.gen;
			txq->shared->ctrl.txNumDeferred++;

			/* move to the next2fill descriptor */
			vmxnet3_cmd_ring_adv_next2fill(&txq->cmd_ring);
			nb_tx++;

		} else {
			PMD_TX_LOG(DEBUG, "No free tx cmd desc(s)\n");
			txq->stats.drop_total += (nb_pkts - nb_tx);
			break;
		}
	}

	PMD_TX_LOG(DEBUG, "vmxnet3 txThreshold: %u", txq->shared->ctrl.txThreshold);

	if (txq->shared->ctrl.txNumDeferred >= txq->shared->ctrl.txThreshold) {

		txq->shared->ctrl.txNumDeferred = 0;
		/* Notify vSwitch that packets are available. */
		VMXNET3_WRITE_BAR0_REG(hw, (VMXNET3_REG_TXPROD + txq->queue_id * VMXNET3_REG_ALIGN),
				txq->cmd_ring.next2fill);
	}

	return (nb_tx);
}

/*
 *  Allocates mbufs and clusters. Post rx descriptors with buffer details
 *  so that device can receive packets in those buffers.
 *	Ring layout:
 *      Among the two rings, 1st ring contains buffers of type 0 and type1.
 *      bufs_per_pkt is set such that for non-LRO cases all the buffers required
 *      by a frame will fit in 1st ring (1st buf of type0 and rest of type1).
 *      2nd ring contains buffers of type 1 alone. Second ring mostly be used
 *      only for LRO.
 *
 */
static inline int
vmxnet3_post_rx_bufs(vmxnet3_rx_queue_t* rxq, uint8_t ring_id)
{
   int err = 0;
   uint32_t i = 0, val = 0;
   struct vmxnet3_cmd_ring *ring = &rxq->cmd_ring[ring_id];

   while (vmxnet3_cmd_ring_desc_avail(ring) > 0) {

		struct Vmxnet3_RxDesc *rxd;
		struct rte_mbuf *mbuf;
		vmxnet3_buf_info_t *buf_info = &ring->buf_info[ring->next2fill];
		rxd = (struct Vmxnet3_RxDesc *)(ring->base + ring->next2fill);

		if (ring->rid == 0) {
			 /* Usually: One HEAD type buf per packet
			   * val = (ring->next2fill % rxq->hw->bufs_per_pkt) ?
			   * VMXNET3_RXD_BTYPE_BODY : VMXNET3_RXD_BTYPE_HEAD;
			   */

			/* We use single packet buffer so all heads here */
			val = VMXNET3_RXD_BTYPE_HEAD;
		} else {
			/* All BODY type buffers for 2nd ring; which won't be used at all by ESXi */
			val = VMXNET3_RXD_BTYPE_BODY;
		}

		/* Allocate blank mbuf for the current Rx Descriptor */
		mbuf = rte_rxmbuf_alloc(rxq->mp);
		if (mbuf == NULL) {
			PMD_RX_LOG(ERR, "Error allocating mbuf in %s\n", __func__);
			rxq->stats.rx_buf_alloc_failure++;
			err = ENOMEM;
			break;
		}

		/*
		 * Load mbuf pointer into buf_info[ring_size]
		 * buf_info structure is equivalent to cookie for virtio-virtqueue
		 */
		buf_info->m = mbuf;
		buf_info->len = (uint16_t)(mbuf->buf_len -
			RTE_PKTMBUF_HEADROOM);
		buf_info->bufPA = RTE_MBUF_DATA_DMA_ADDR_DEFAULT(mbuf);

		/* Load Rx Descriptor with the buffer's GPA */
		rxd->addr = buf_info->bufPA;

		/* After this point rxd->addr MUST not be NULL */
		rxd->btype = val;
		rxd->len = buf_info->len;
		/* Flip gen bit at the end to change ownership */
		rxd->gen = ring->gen;

		vmxnet3_cmd_ring_adv_next2fill(ring);
		i++;
   }

   /* Return error only if no buffers are posted at present */
   if (vmxnet3_cmd_ring_desc_avail(ring) >= (ring->size -1))
      return -err;
   else
      return i;
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

	if(rxq->stopped) {
		PMD_RX_LOG(DEBUG, "Rx queue is stopped.\n");
		return 0;
	}

	while (rcd->gen == rxq->comp_ring.gen) {

		if(nb_rx >= nb_pkts)
			break;
		idx = rcd->rxdIdx;
		ring_idx = (uint8_t)((rcd->rqID == rxq->qid1) ? 0 : 1);
		rxd = (Vmxnet3_RxDesc *)rxq->cmd_ring[ring_idx].base + idx;
		rbi = rxq->cmd_ring[ring_idx].buf_info + idx;

		if(rcd->sop !=1 || rcd->eop != 1) {
			rte_pktmbuf_free_seg(rbi->m);

			PMD_RX_LOG(DEBUG, "Packet spread across multiple buffers\n)");
			goto rcd_done;

		} else {

			PMD_RX_LOG(DEBUG, "rxd idx: %d ring idx: %d.\n", idx, ring_idx);

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER
			VMXNET3_ASSERT(rcd->len <= rxd->len);
			VMXNET3_ASSERT(rbi->m);
#endif
			if (rcd->len == 0) {
				PMD_RX_LOG(DEBUG, "Rx buf was skipped. rxring[%d][%d]\n)",
							 ring_idx, idx);
#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER
				VMXNET3_ASSERT(rcd->sop && rcd->eop);
#endif
				rte_pktmbuf_free_seg(rbi->m);

				goto rcd_done;
			}

			/* Assuming a packet is coming in a single packet buffer */
			if (rxd->btype != VMXNET3_RXD_BTYPE_HEAD) {
				PMD_RX_LOG(DEBUG, "Alert : Misbehaving device, incorrect "
						  " buffer type used. iPacket dropped.\n");
				rte_pktmbuf_free_seg(rbi->m);
				goto rcd_done;
			}
#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER
			VMXNET3_ASSERT(rxd->btype == VMXNET3_RXD_BTYPE_HEAD);
#endif
			/* Get the packet buffer pointer from buf_info */
			rxm = rbi->m;

			/* Clear descriptor associated buf_info to be reused */
			rbi->m = NULL;
			rbi->bufPA = 0;

			/* Update the index that we received a packet */
			rxq->cmd_ring[ring_idx].next2comp = idx;

			/* For RCD with EOP set, check if there is frame error */
			if (rcd->err) {
				rxq->stats.drop_total++;
				rxq->stats.drop_err++;

				if(!rcd->fcs) {
					rxq->stats.drop_fcs++;
					PMD_RX_LOG(ERR, "Recv packet dropped due to frame err.\n");
				}
				PMD_RX_LOG(ERR, "Error in received packet rcd#:%d rxd:%d\n",
						 (int)(rcd - (struct Vmxnet3_RxCompDesc *)
							   rxq->comp_ring.base), rcd->rxdIdx);
				rte_pktmbuf_free_seg(rxm);

				goto rcd_done;
			}

			/* Check for hardware stripped VLAN tag */
			if (rcd->ts) {

				PMD_RX_LOG(ERR, "Received packet with vlan ID: %d.\n",
						 rcd->tci);
				rxm->ol_flags = PKT_RX_VLAN_PKT;

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER
				VMXNET3_ASSERT(rxm &&
					rte_pktmbuf_mtod(rxm, void *));
#endif
				//Copy vlan tag in packet buffer
				rxm->pkt.vlan_macip.f.vlan_tci =
					rte_le_to_cpu_16((uint16_t)rcd->tci);

			} else
				rxm->ol_flags = 0;

			/* Initialize newly received packet buffer */
			rxm->pkt.in_port = rxq->port_id;
			rxm->pkt.nb_segs = 1;
			rxm->pkt.next = NULL;
			rxm->pkt.pkt_len = (uint16_t)rcd->len;
			rxm->pkt.data_len = (uint16_t)rcd->len;
			rxm->pkt.in_port = rxq->port_id;
			rxm->pkt.vlan_macip.f.vlan_tci = 0;
			rxm->pkt.data = (char *)rxm->buf_addr + RTE_PKTMBUF_HEADROOM;

			rx_pkts[nb_rx++] = rxm;

rcd_done:
			rxq->cmd_ring[ring_idx].next2comp = idx;
			VMXNET3_INC_RING_IDX_ONLY(rxq->cmd_ring[ring_idx].next2comp, rxq->cmd_ring[ring_idx].size);

			/* It's time to allocate some new buf and renew descriptors */
			vmxnet3_post_rx_bufs(rxq, ring_idx);
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
						 " relinquish control.\n");
				break;
			}
		}
	}

	return (nb_rx);
}

/*
 * Create memzone for device rings. malloc can't be used as the physical address is
 * needed. If the memzone is already created, then this function returns a ptr
 * to the old one.
 */
static const struct rte_memzone *
ring_dma_zone_reserve(struct rte_eth_dev *dev, const char *ring_name,
		      uint16_t queue_id, uint32_t ring_size, int socket_id)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(z_name, sizeof(z_name), "%s_%s_%d_%d",
			dev->driver->pci_drv.name, ring_name,
			dev->data->port_id, queue_id);

	mz = rte_memzone_lookup(z_name);
	if (mz)
		return mz;

	return rte_memzone_reserve_aligned(z_name, ring_size,
			socket_id, 0, VMXNET3_RING_BA_ALIGN);
}

int
vmxnet3_dev_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 __attribute__((unused)) const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *mz;
	struct vmxnet3_tx_queue *txq;
	struct vmxnet3_hw     *hw;
    struct vmxnet3_cmd_ring *ring;
    struct vmxnet3_comp_ring *comp_ring;
    int size;

	PMD_INIT_FUNC_TRACE();
	hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if ((tx_conf->txq_flags & ETH_TXQ_FLAGS_NOMULTSEGS) !=
		ETH_TXQ_FLAGS_NOMULTSEGS) {
		PMD_INIT_LOG(ERR, "TX Multi segment not support yet\n");
		return (-EINVAL);
	}

	if ((tx_conf->txq_flags & ETH_TXQ_FLAGS_NOOFFLOADS) !=
		ETH_TXQ_FLAGS_NOOFFLOADS) {
		PMD_INIT_LOG(ERR, "TX not support offload function yet\n");
		return (-EINVAL);
	}

	txq = rte_zmalloc("ethdev_tx_queue", sizeof(struct vmxnet3_tx_queue), CACHE_LINE_SIZE);
	if (txq == NULL) {
		PMD_INIT_LOG(ERR, "Can not allocate tx queue structure\n");
		return (-ENOMEM);
	}

	txq->queue_id = queue_idx;
	txq->port_id = dev->data->port_id;
	txq->shared = &hw->tqd_start[queue_idx];
    txq->hw = hw;
    txq->qid = queue_idx;
    txq->stopped = TRUE;

    ring = &txq->cmd_ring;
    comp_ring = &txq->comp_ring;

    /* Tx vmxnet ring length should be between 512-4096 */
    if (nb_desc < VMXNET3_DEF_TX_RING_SIZE) {
		PMD_INIT_LOG(ERR, "VMXNET3 Tx Ring Size Min: %u\n",
					VMXNET3_DEF_TX_RING_SIZE);
		return -EINVAL;
	} else if (nb_desc > VMXNET3_TX_RING_MAX_SIZE) {
		PMD_INIT_LOG(ERR, "VMXNET3 Tx Ring Size Max: %u\n",
					VMXNET3_TX_RING_MAX_SIZE);
		return -EINVAL;
    } else {
		ring->size = nb_desc;
		ring->size &= ~VMXNET3_RING_SIZE_MASK;
    }
    comp_ring->size = ring->size;

    /* Tx vmxnet rings structure initialization*/
    ring->next2fill = 0;
    ring->next2comp = 0;
    ring->gen = VMXNET3_INIT_GEN;
    comp_ring->next2proc = 0;
    comp_ring->gen = VMXNET3_INIT_GEN;

    size = sizeof(struct Vmxnet3_TxDesc) * ring->size;
    size += sizeof(struct Vmxnet3_TxCompDesc) * comp_ring->size;

    mz = ring_dma_zone_reserve(dev, "txdesc", queue_idx, size, socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating queue descriptors zone\n");
		return (-ENOMEM);
	}
	memset(mz->addr, 0, mz->len);

	/* cmd_ring initialization */
	ring->base = mz->addr;
	ring->basePA = mz->phys_addr;

	/* comp_ring initialization */
    comp_ring->base = ring->base + ring->size;
    comp_ring->basePA = ring->basePA +
				(sizeof(struct Vmxnet3_TxDesc) * ring->size);

    /* cmd_ring0 buf_info allocation */
	ring->buf_info = rte_zmalloc("tx_ring_buf_info",
				ring->size * sizeof(vmxnet3_buf_info_t), CACHE_LINE_SIZE);
	if (ring->buf_info == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating tx_buf_info structure\n");
		return (-ENOMEM);
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
			 __attribute__((unused)) const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	const struct rte_memzone *mz;
	struct vmxnet3_rx_queue *rxq;
	struct vmxnet3_hw     *hw;
	struct vmxnet3_cmd_ring *ring0, *ring1, *ring;
	struct vmxnet3_comp_ring *comp_ring;
	int size;
	uint8_t i;
	char mem_name[32];
	uint16_t buf_size;
	struct rte_pktmbuf_pool_private *mbp_priv;

	PMD_INIT_FUNC_TRACE();
	hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	mbp_priv = (struct rte_pktmbuf_pool_private *)
				rte_mempool_get_priv(mp);
	buf_size = (uint16_t) (mbp_priv->mbuf_data_room_size -
				   RTE_PKTMBUF_HEADROOM);

	if (dev->data->dev_conf.rxmode.max_rx_pkt_len > buf_size) {
		PMD_INIT_LOG(ERR, "buf_size = %u, max_pkt_len = %u, "
				"VMXNET3 don't support scatter packets yet\n",
				buf_size, dev->data->dev_conf.rxmode.max_rx_pkt_len);
		return (-EINVAL);
	}

	rxq = rte_zmalloc("ethdev_rx_queue", sizeof(struct vmxnet3_rx_queue), CACHE_LINE_SIZE);
	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Can not allocate rx queue structure\n");
		return (-ENOMEM);
	}

	rxq->mp = mp;
	rxq->queue_id = queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->shared = &hw->rqd_start[queue_idx];
	rxq->hw = hw;
	rxq->qid1 = queue_idx;
	rxq->qid2 = queue_idx + hw->num_rx_queues;
	rxq->stopped = TRUE;

	ring0 = &rxq->cmd_ring[0];
	ring1 = &rxq->cmd_ring[1];
	comp_ring = &rxq->comp_ring;

	/* Rx vmxnet rings length should be between 256-4096 */
	if(nb_desc < VMXNET3_DEF_RX_RING_SIZE) {
		PMD_INIT_LOG(ERR, "VMXNET3 Rx Ring Size Min: 256\n");
		return -EINVAL;
	} else if(nb_desc > VMXNET3_RX_RING_MAX_SIZE) {
		PMD_INIT_LOG(ERR, "VMXNET3 Rx Ring Size Max: 4096\n");
		return -EINVAL;
	} else {
		ring0->size = nb_desc;
		ring0->size &= ~VMXNET3_RING_SIZE_MASK;
		ring1->size = ring0->size;
	}

	comp_ring->size = ring0->size + ring1->size;

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

	mz = ring_dma_zone_reserve(dev, "rxdesc", queue_idx, size, socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating queue descriptors zone\n");
		return (-ENOMEM);
	}
	memset(mz->addr, 0, mz->len);

	/* cmd_ring0 initialization */
	ring0->base = mz->addr;
	ring0->basePA = mz->phys_addr;

	/* cmd_ring1 initialization */
	ring1->base = ring0->base + ring0->size;
	ring1->basePA = ring0->basePA + sizeof(struct Vmxnet3_RxDesc) * ring0->size;

	/* comp_ring initialization */
	comp_ring->base = ring1->base +  ring1->size;
	comp_ring->basePA = ring1->basePA + sizeof(struct Vmxnet3_RxDesc) *
					   ring1->size;

	/* cmd_ring0-cmd_ring1 buf_info allocation */
	for(i = 0; i < VMXNET3_RX_CMDRING_SIZE; i++) {

	  ring = &rxq->cmd_ring[i];
	  ring->rid = i;
	  snprintf(mem_name, sizeof(mem_name), "rx_ring_%d_buf_info", i);

	  ring->buf_info = rte_zmalloc(mem_name, ring->size * sizeof(vmxnet3_buf_info_t), CACHE_LINE_SIZE);
	  if (ring->buf_info == NULL) {
		  PMD_INIT_LOG(ERR, "ERROR: Creating rx_buf_info structure\n");
		  return (-ENOMEM);
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
	struct vmxnet3_hw *hw;
	int i, ret;
	uint8_t j;

	PMD_INIT_FUNC_TRACE();
	hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	for (i = 0; i < hw->num_rx_queues; i++) {

		vmxnet3_rx_queue_t *rxq = dev->data->rx_queues[i];
		for(j = 0;j < VMXNET3_RX_CMDRING_SIZE;j++) {
			/* Passing 0 as alloc_num will allocate full ring */
			ret = vmxnet3_post_rx_bufs(rxq, j);
			if (ret <= 0) {
			  PMD_INIT_LOG(ERR, "ERROR: Posting Rxq: %d buffers ring: %d\n", i, j);
			  return (-ret);
			}
			/* Updating device with the index:next2fill to fill the mbufs for coming packets */
			if (unlikely(rxq->shared->ctrl.updateRxProd)) {
				VMXNET3_WRITE_BAR0_REG(hw, rxprod_reg[j] + (rxq->queue_id * VMXNET3_REG_ALIGN),
						rxq->cmd_ring[j].next2fill);
			}
		}
		rxq->stopped = FALSE;
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
 * Configure RSS feature
 */
int
vmxnet3_rss_configure(struct rte_eth_dev *dev)
{
#define VMXNET3_RSS_OFFLOAD_ALL ( \
		ETH_RSS_IPV4 | \
		ETH_RSS_IPV4_TCP | \
		ETH_RSS_IPV6 | \
		ETH_RSS_IPV6_TCP)

	struct vmxnet3_hw *hw;
	struct VMXNET3_RSSConf *dev_rss_conf;
	struct rte_eth_rss_conf *port_rss_conf;
	uint64_t rss_hf;
	uint8_t i, j;

	PMD_INIT_FUNC_TRACE();
	hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	dev_rss_conf = hw->rss_conf;
	port_rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;

	/* loading hashFunc */
	dev_rss_conf->hashFunc = VMXNET3_RSS_HASH_FUNC_TOEPLITZ;
	/* loading hashKeySize */
	dev_rss_conf->hashKeySize = VMXNET3_RSS_MAX_KEY_SIZE;
	/* loading indTableSize : Must not exceed VMXNET3_RSS_MAX_IND_TABLE_SIZE (128)*/
	dev_rss_conf->indTableSize = (uint16_t)(hw->num_rx_queues * 4);

	if (port_rss_conf->rss_key == NULL) {
		/* Default hash key */
		port_rss_conf->rss_key = rss_intel_key;
	}

	/* loading hashKey */
	memcpy(&dev_rss_conf->hashKey[0], port_rss_conf->rss_key, dev_rss_conf->hashKeySize);

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
	if (rss_hf & ETH_RSS_IPV4_TCP)
		dev_rss_conf->hashType |= VMXNET3_RSS_HASH_TYPE_TCP_IPV4;
	if (rss_hf & ETH_RSS_IPV6)
		dev_rss_conf->hashType |= VMXNET3_RSS_HASH_TYPE_IPV6;
	if (rss_hf & ETH_RSS_IPV6_TCP)
		dev_rss_conf->hashType |= VMXNET3_RSS_HASH_TYPE_TCP_IPV6;

	return VMXNET3_SUCCESS;
}

/*
 * Configure VLAN Filter feature
 */
int
vmxnet3_vlan_configure(struct rte_eth_dev *dev)
{
	uint8_t i;
	struct vmxnet3_hw *hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t *vf_table = hw->shared->devRead.rxFilterConf.vfTable;

	PMD_INIT_FUNC_TRACE();

	/* Verify if this tag is already set */
	for (i = 0; i < VMXNET3_VFT_SIZE; i++) {
		/* Filter all vlan tags out by default */
		vf_table[i] = 0;
		/* To-Do: Provide another routine in dev_ops for user config */

		PMD_INIT_LOG(DEBUG, "Registering VLAN portid: %"PRIu8" tag %u\n",
					dev->data->port_id, vf_table[i]);
	}

	return VMXNET3_SUCCESS;
}
