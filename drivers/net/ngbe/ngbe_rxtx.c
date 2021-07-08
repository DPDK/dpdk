/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <sys/queue.h>

#include <stdint.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "ngbe_logs.h"
#include "base/ngbe.h"
#include "ngbe_ethdev.h"
#include "ngbe_rxtx.h"

/**
 * ngbe_free_sc_cluster - free the not-yet-completed scattered cluster
 *
 * The "next" pointer of the last segment of (not-yet-completed) RSC clusters
 * in the sw_sc_ring is not set to NULL but rather points to the next
 * mbuf of this RSC aggregation (that has not been completed yet and still
 * resides on the HW ring). So, instead of calling for rte_pktmbuf_free() we
 * will just free first "nb_segs" segments of the cluster explicitly by calling
 * an rte_pktmbuf_free_seg().
 *
 * @m scattered cluster head
 */
static void
ngbe_free_sc_cluster(struct rte_mbuf *m)
{
	uint16_t i, nb_segs = m->nb_segs;
	struct rte_mbuf *next_seg;

	for (i = 0; i < nb_segs; i++) {
		next_seg = m->next;
		rte_pktmbuf_free_seg(m);
		m = next_seg;
	}
}

static void
ngbe_rx_queue_release_mbufs(struct ngbe_rx_queue *rxq)
{
	unsigned int i;

	if (rxq->sw_ring != NULL) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
		for (i = 0; i < rxq->rx_nb_avail; ++i) {
			struct rte_mbuf *mb;

			mb = rxq->rx_stage[rxq->rx_next_avail + i];
			rte_pktmbuf_free_seg(mb);
		}
		rxq->rx_nb_avail = 0;
	}

	if (rxq->sw_sc_ring != NULL)
		for (i = 0; i < rxq->nb_rx_desc; i++)
			if (rxq->sw_sc_ring[i].fbuf != NULL) {
				ngbe_free_sc_cluster(rxq->sw_sc_ring[i].fbuf);
				rxq->sw_sc_ring[i].fbuf = NULL;
			}
}

static void
ngbe_rx_queue_release(struct ngbe_rx_queue *rxq)
{
	if (rxq != NULL) {
		ngbe_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq->sw_sc_ring);
		rte_free(rxq);
	}
}

void
ngbe_dev_rx_queue_release(void *rxq)
{
	ngbe_rx_queue_release(rxq);
}

/*
 * Check if Rx Burst Bulk Alloc function can be used.
 * Return
 *        0: the preconditions are satisfied and the bulk allocation function
 *           can be used.
 *  -EINVAL: the preconditions are NOT satisfied and the default Rx burst
 *           function must be used.
 */
static inline int
check_rx_burst_bulk_alloc_preconditions(struct ngbe_rx_queue *rxq)
{
	int ret = 0;

	/*
	 * Make sure the following pre-conditions are satisfied:
	 *   rxq->rx_free_thresh >= RTE_PMD_NGBE_RX_MAX_BURST
	 *   rxq->rx_free_thresh < rxq->nb_rx_desc
	 *   (rxq->nb_rx_desc % rxq->rx_free_thresh) == 0
	 * Scattered packets are not supported.  This should be checked
	 * outside of this function.
	 */
	if (rxq->rx_free_thresh < RTE_PMD_NGBE_RX_MAX_BURST) {
		PMD_INIT_LOG(DEBUG,
			     "Rx Burst Bulk Alloc Preconditions: rxq->rx_free_thresh=%d, RTE_PMD_NGBE_RX_MAX_BURST=%d",
			     rxq->rx_free_thresh, RTE_PMD_NGBE_RX_MAX_BURST);
		ret = -EINVAL;
	} else if (rxq->rx_free_thresh >= rxq->nb_rx_desc) {
		PMD_INIT_LOG(DEBUG,
			     "Rx Burst Bulk Alloc Preconditions: rxq->rx_free_thresh=%d, rxq->nb_rx_desc=%d",
			     rxq->rx_free_thresh, rxq->nb_rx_desc);
		ret = -EINVAL;
	} else if ((rxq->nb_rx_desc % rxq->rx_free_thresh) != 0) {
		PMD_INIT_LOG(DEBUG,
			     "Rx Burst Bulk Alloc Preconditions: rxq->nb_rx_desc=%d, rxq->rx_free_thresh=%d",
			     rxq->nb_rx_desc, rxq->rx_free_thresh);
		ret = -EINVAL;
	}

	return ret;
}

/* Reset dynamic ngbe_rx_queue fields back to defaults */
static void
ngbe_reset_rx_queue(struct ngbe_adapter *adapter, struct ngbe_rx_queue *rxq)
{
	static const struct ngbe_rx_desc zeroed_desc = {
						{{0}, {0} }, {{0}, {0} } };
	unsigned int i;
	uint16_t len = rxq->nb_rx_desc;

	/*
	 * By default, the Rx queue setup function allocates enough memory for
	 * NGBE_RING_DESC_MAX.  The Rx Burst bulk allocation function requires
	 * extra memory at the end of the descriptor ring to be zero'd out.
	 */
	if (adapter->rx_bulk_alloc_allowed)
		/* zero out extra memory */
		len += RTE_PMD_NGBE_RX_MAX_BURST;

	/*
	 * Zero out HW ring memory. Zero out extra memory at the end of
	 * the H/W ring so look-ahead logic in Rx Burst bulk alloc function
	 * reads extra memory as zeros.
	 */
	for (i = 0; i < len; i++)
		rxq->rx_ring[i] = zeroed_desc;

	/*
	 * initialize extra software ring entries. Space for these extra
	 * entries is always allocated
	 */
	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));
	for (i = rxq->nb_rx_desc; i < len; ++i)
		rxq->sw_ring[i].mbuf = &rxq->fake_mbuf;

	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);
	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

int
ngbe_dev_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	const struct rte_memzone *rz;
	struct ngbe_rx_queue *rxq;
	struct ngbe_hw     *hw;
	uint16_t len;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		ngbe_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the Rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue",
				 sizeof(struct ngbe_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		return -ENOMEM;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->reg_idx = queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;

	/*
	 * Allocate Rx ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
				      RX_RING_SZ, NGBE_ALIGN, socket_id);
	if (rz == NULL) {
		ngbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	/*
	 * Zero init all the descriptors in the ring.
	 */
	memset(rz->addr, 0, RX_RING_SZ);

	rxq->rdt_reg_addr = NGBE_REG_ADDR(hw, NGBE_RXWP(rxq->reg_idx));
	rxq->rdh_reg_addr = NGBE_REG_ADDR(hw, NGBE_RXRP(rxq->reg_idx));

	rxq->rx_ring_phys_addr = TMZ_PADDR(rz);
	rxq->rx_ring = (struct ngbe_rx_desc *)TMZ_VADDR(rz);

	/*
	 * Certain constraints must be met in order to use the bulk buffer
	 * allocation Rx burst function. If any of Rx queues doesn't meet them
	 * the feature should be disabled for the whole port.
	 */
	if (check_rx_burst_bulk_alloc_preconditions(rxq)) {
		PMD_INIT_LOG(DEBUG,
			     "queue[%d] doesn't meet Rx Bulk Alloc preconditions - canceling the feature for the whole port[%d]",
			     rxq->queue_id, rxq->port_id);
		adapter->rx_bulk_alloc_allowed = false;
	}

	/*
	 * Allocate software ring. Allow for space at the end of the
	 * S/W ring to make sure look-ahead logic in bulk alloc Rx burst
	 * function does not access an invalid memory region.
	 */
	len = nb_desc;
	if (adapter->rx_bulk_alloc_allowed)
		len += RTE_PMD_NGBE_RX_MAX_BURST;

	rxq->sw_ring = rte_zmalloc_socket("rxq->sw_ring",
					  sizeof(struct ngbe_rx_entry) * len,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL) {
		ngbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	/*
	 * Always allocate even if it's not going to be needed in order to
	 * simplify the code.
	 *
	 * This ring is used in Scattered Rx cases and Scattered Rx may
	 * be requested in ngbe_dev_rx_init(), which is called later from
	 * dev_start() flow.
	 */
	rxq->sw_sc_ring =
		rte_zmalloc_socket("rxq->sw_sc_ring",
				  sizeof(struct ngbe_scattered_rx_entry) * len,
				  RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_sc_ring == NULL) {
		ngbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	PMD_INIT_LOG(DEBUG,
		     "sw_ring=%p sw_sc_ring=%p hw_ring=%p dma_addr=0x%" PRIx64,
		     rxq->sw_ring, rxq->sw_sc_ring, rxq->rx_ring,
		     rxq->rx_ring_phys_addr);

	dev->data->rx_queues[queue_idx] = rxq;

	ngbe_reset_rx_queue(adapter, rxq);

	return 0;
}

