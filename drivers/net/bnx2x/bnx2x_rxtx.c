/*
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 *
 * Copyright (c) 2015 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.bnx2x_pmd for copyright and licensing details.
 */

#include "bnx2x.h"
#include "bnx2x_rxtx.h"

static const struct rte_memzone *
ring_dma_zone_reserve(struct rte_eth_dev *dev, const char *ring_name,
		      uint16_t queue_id, uint32_t ring_size, int socket_id)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(z_name, sizeof(z_name), "%s_%s_%d_%d",
			dev->device->driver->name, ring_name,
			dev->data->port_id, queue_id);

	mz = rte_memzone_lookup(z_name);
	if (mz)
		return mz;

	return rte_memzone_reserve_aligned(z_name, ring_size, socket_id, 0, BNX2X_PAGE_SIZE);
}

static void
bnx2x_rx_queue_release(struct bnx2x_rx_queue *rx_queue)
{
	uint16_t i;
	struct rte_mbuf **sw_ring;

	if (NULL != rx_queue) {

		sw_ring = rx_queue->sw_ring;
		if (NULL != sw_ring) {
			for (i = 0; i < rx_queue->nb_rx_desc; i++) {
				if (NULL != sw_ring[i])
					rte_pktmbuf_free(sw_ring[i]);
			}
			rte_free(sw_ring);
		}
		rte_free(rx_queue);
	}
}

void
bnx2x_dev_rx_queue_release(void *rxq)
{
	bnx2x_rx_queue_release(rxq);
}

int
bnx2x_dev_rx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx,
		       uint16_t nb_desc,
		       unsigned int socket_id,
		       __rte_unused const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp)
{
	uint16_t j, idx;
	const struct rte_memzone *dma;
	struct bnx2x_rx_queue *rxq;
	uint32_t dma_size;
	struct rte_mbuf *mbuf;
	struct bnx2x_softc *sc = dev->data->dev_private;
	struct bnx2x_fastpath *fp = &sc->fp[queue_idx];
	struct eth_rx_cqe_next_page *nextpg;
	phys_addr_t *rx_bd;
	phys_addr_t busaddr;

	/* First allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue", sizeof(struct bnx2x_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (NULL == rxq) {
		PMD_INIT_LOG(ERR, "rte_zmalloc for rxq failed!");
		return -ENOMEM;
	}
	rxq->sc = sc;
	rxq->mb_pool = mp;
	rxq->queue_id = queue_idx;
	rxq->port_id = dev->data->port_id;

	rxq->nb_rx_pages = 1;
	while (USABLE_RX_BD(rxq) < nb_desc)
		rxq->nb_rx_pages <<= 1;

	rxq->nb_rx_desc  = TOTAL_RX_BD(rxq);
	sc->rx_ring_size = USABLE_RX_BD(rxq);
	rxq->nb_cq_pages = RCQ_BD_PAGES(rxq);

	PMD_INIT_LOG(DEBUG, "fp[%02d] req_bd=%u, usable_bd=%lu, "
		       "total_bd=%lu, rx_pages=%u, cq_pages=%u",
		       queue_idx, nb_desc, (unsigned long)USABLE_RX_BD(rxq),
		       (unsigned long)TOTAL_RX_BD(rxq), rxq->nb_rx_pages,
		       rxq->nb_cq_pages);

	/* Allocate RX ring hardware descriptors */
	dma_size = rxq->nb_rx_desc * sizeof(struct eth_rx_bd);
	dma = ring_dma_zone_reserve(dev, "hw_ring", queue_idx, dma_size, socket_id);
	if (NULL == dma) {
		PMD_RX_LOG(ERR, "ring_dma_zone_reserve for rx_ring failed!");
		bnx2x_rx_queue_release(rxq);
		return -ENOMEM;
	}
	fp->rx_desc_mapping = rxq->rx_ring_phys_addr = (uint64_t)dma->phys_addr;
	rxq->rx_ring = (uint64_t*)dma->addr;
	memset((void *)rxq->rx_ring, 0, dma_size);

	/* Link the RX chain pages. */
	for (j = 1; j <= rxq->nb_rx_pages; j++) {
		rx_bd = &rxq->rx_ring[TOTAL_RX_BD_PER_PAGE * j - 2];
		busaddr = rxq->rx_ring_phys_addr + BNX2X_PAGE_SIZE * (j % rxq->nb_rx_pages);
		*rx_bd = busaddr;
	}

	/* Allocate software ring */
	dma_size = rxq->nb_rx_desc * sizeof(struct bnx2x_rx_entry);
	rxq->sw_ring = rte_zmalloc_socket("sw_ring", dma_size,
					  RTE_CACHE_LINE_SIZE,
					  socket_id);
	if (NULL == rxq->sw_ring) {
		PMD_RX_LOG(ERR, "rte_zmalloc for sw_ring failed!");
		bnx2x_rx_queue_release(rxq);
		return -ENOMEM;
	}

	/* Initialize software ring entries */
	for (idx = 0; idx < rxq->nb_rx_desc; idx = NEXT_RX_BD(idx)) {
		mbuf = rte_mbuf_raw_alloc(mp);
		if (NULL == mbuf) {
			PMD_RX_LOG(ERR, "RX mbuf alloc failed queue_id=%u, idx=%d",
				   (unsigned)rxq->queue_id, idx);
			bnx2x_rx_queue_release(rxq);
			return -ENOMEM;
		}
		rxq->sw_ring[idx] = mbuf;
		rxq->rx_ring[idx] = mbuf->buf_physaddr;
	}
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
	rxq->rx_bd_head = 0;
	rxq->rx_bd_tail = rxq->nb_rx_desc;

	/* Allocate CQ chain. */
	dma_size = BNX2X_RX_CHAIN_PAGE_SZ * rxq->nb_cq_pages;
	dma = ring_dma_zone_reserve(dev, "bnx2x_rcq", queue_idx, dma_size, socket_id);
	if (NULL == dma) {
		PMD_RX_LOG(ERR, "RCQ  alloc failed");
		return -ENOMEM;
	}
	fp->rx_comp_mapping = rxq->cq_ring_phys_addr = (uint64_t)dma->phys_addr;
	rxq->cq_ring = (union eth_rx_cqe*)dma->addr;

	/* Link the CQ chain pages. */
	for (j = 1; j <= rxq->nb_cq_pages; j++) {
		nextpg = &rxq->cq_ring[TOTAL_RCQ_ENTRIES_PER_PAGE * j - 1].next_page_cqe;
		busaddr = rxq->cq_ring_phys_addr + BNX2X_PAGE_SIZE * (j % rxq->nb_cq_pages);
		nextpg->addr_hi = rte_cpu_to_le_32(U64_HI(busaddr));
		nextpg->addr_lo = rte_cpu_to_le_32(U64_LO(busaddr));
	}
	rxq->rx_cq_head = 0;
	rxq->rx_cq_tail = TOTAL_RCQ_ENTRIES(rxq);

	dev->data->rx_queues[queue_idx] = rxq;
	if (!sc->rx_queues) sc->rx_queues = dev->data->rx_queues;

	return 0;
}

static void
bnx2x_tx_queue_release(struct bnx2x_tx_queue *tx_queue)
{
	uint16_t i;
	struct rte_mbuf **sw_ring;

	if (NULL != tx_queue) {

		sw_ring = tx_queue->sw_ring;
		if (NULL != sw_ring) {
			for (i = 0; i < tx_queue->nb_tx_desc; i++) {
				if (NULL != sw_ring[i])
					rte_pktmbuf_free(sw_ring[i]);
			}
			rte_free(sw_ring);
		}
		rte_free(tx_queue);
	}
}

void
bnx2x_dev_tx_queue_release(void *txq)
{
	bnx2x_tx_queue_release(txq);
}

static uint16_t
bnx2x_xmit_pkts(void *p_txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct bnx2x_tx_queue *txq;
	struct bnx2x_softc *sc;
	struct bnx2x_fastpath *fp;
	uint16_t nb_tx_pkts;
	uint16_t nb_pkt_sent = 0;
	uint32_t ret;

	txq = p_txq;
	sc = txq->sc;
	fp = &sc->fp[txq->queue_id];

	if ((unlikely((txq->nb_tx_desc - txq->nb_tx_avail) >
				txq->tx_free_thresh)))
		bnx2x_txeof(sc, fp);

	nb_tx_pkts = RTE_MIN(nb_pkts, txq->nb_tx_avail / BDS_PER_TX_PKT);
	if (unlikely(nb_tx_pkts == 0))
		return 0;

	while (nb_tx_pkts--) {
		struct rte_mbuf *m = *tx_pkts++;
		assert(m != NULL);
		ret = bnx2x_tx_encap(txq, m);
		fp->tx_db.data.prod += ret;
		nb_pkt_sent++;
	}

	bnx2x_update_fp_sb_idx(fp);
	mb();
	DOORBELL(sc, txq->queue_id, fp->tx_db.raw);
	mb();

	if ((txq->nb_tx_desc - txq->nb_tx_avail) >
				txq->tx_free_thresh)
		bnx2x_txeof(sc, fp);

	return nb_pkt_sent;
}

int
bnx2x_dev_tx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx,
		       uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	uint16_t i;
	unsigned int tsize;
	const struct rte_memzone *tz;
	struct bnx2x_tx_queue *txq;
	struct eth_tx_next_bd *tx_n_bd;
	uint64_t busaddr;
	struct bnx2x_softc *sc = dev->data->dev_private;
	struct bnx2x_fastpath *fp = &sc->fp[queue_idx];

	/* First allocate the tx queue data structure */
	txq = rte_zmalloc("ethdev TX queue", sizeof(struct bnx2x_tx_queue),
			  RTE_CACHE_LINE_SIZE);
	if (txq == NULL)
		return -ENOMEM;
	txq->sc = sc;

	txq->nb_tx_pages = 1;
	while (USABLE_TX_BD(txq) < nb_desc)
		txq->nb_tx_pages <<= 1;

	txq->nb_tx_desc  = TOTAL_TX_BD(txq);
	sc->tx_ring_size = TOTAL_TX_BD(txq);

	txq->tx_free_thresh = tx_conf->tx_free_thresh ?
		tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH;
	txq->tx_free_thresh = min(txq->tx_free_thresh,
				  txq->nb_tx_desc - BDS_PER_TX_PKT);

	PMD_INIT_LOG(DEBUG, "fp[%02d] req_bd=%u, thresh=%u, usable_bd=%lu, "
		     "total_bd=%lu, tx_pages=%u",
		     queue_idx, nb_desc, txq->tx_free_thresh,
		     (unsigned long)USABLE_TX_BD(txq),
		     (unsigned long)TOTAL_TX_BD(txq), txq->nb_tx_pages);

	/* Allocate TX ring hardware descriptors */
	tsize = txq->nb_tx_desc * sizeof(union eth_tx_bd_types);
	tz = ring_dma_zone_reserve(dev, "tx_hw_ring", queue_idx, tsize, socket_id);
	if (tz == NULL) {
		bnx2x_tx_queue_release(txq);
		return -ENOMEM;
	}
	fp->tx_desc_mapping = txq->tx_ring_phys_addr = (uint64_t)tz->phys_addr;
	txq->tx_ring = (union eth_tx_bd_types *) tz->addr;
	memset(txq->tx_ring, 0, tsize);

	/* Allocate software ring */
	tsize = txq->nb_tx_desc * sizeof(struct rte_mbuf *);
	txq->sw_ring = rte_zmalloc("tx_sw_ring", tsize,
				   RTE_CACHE_LINE_SIZE);
	if (txq->sw_ring == NULL) {
		bnx2x_tx_queue_release(txq);
		return -ENOMEM;
	}

	/* PMD_DRV_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%"PRIx64,
	   txq->sw_ring, txq->tx_ring, txq->tx_ring_phys_addr); */

	/* Link TX pages */
	for (i = 1; i <= txq->nb_tx_pages; i++) {
		tx_n_bd = &txq->tx_ring[TOTAL_TX_BD_PER_PAGE * i - 1].next_bd;
		busaddr = txq->tx_ring_phys_addr + BNX2X_PAGE_SIZE * (i % txq->nb_tx_pages);
		tx_n_bd->addr_hi = rte_cpu_to_le_32(U64_HI(busaddr));
		tx_n_bd->addr_lo = rte_cpu_to_le_32(U64_LO(busaddr));
		/* PMD_DRV_LOG(DEBUG, "link tx page %lu", (TOTAL_TX_BD_PER_PAGE * i - 1)); */
	}

	txq->queue_id = queue_idx;
	txq->port_id = dev->data->port_id;
	txq->tx_pkt_tail = 0;
	txq->tx_pkt_head = 0;
	txq->tx_bd_tail = 0;
	txq->tx_bd_head = 0;
	txq->nb_tx_avail = txq->nb_tx_desc;
	dev->tx_pkt_burst = bnx2x_xmit_pkts;
	dev->data->tx_queues[queue_idx] = txq;
	if (!sc->tx_queues) sc->tx_queues = dev->data->tx_queues;

	return 0;
}

static inline void
bnx2x_upd_rx_prod_fast(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
		uint16_t rx_bd_prod, uint16_t rx_cq_prod)
{
	union ustorm_eth_rx_producers rx_prods;

	rx_prods.prod.bd_prod  = rx_bd_prod;
	rx_prods.prod.cqe_prod = rx_cq_prod;

	REG_WR(sc, fp->ustorm_rx_prods_offset, rx_prods.raw_data[0]);
}

static uint16_t
bnx2x_recv_pkts(void *p_rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct bnx2x_rx_queue *rxq = p_rxq;
	struct bnx2x_softc *sc = rxq->sc;
	struct bnx2x_fastpath *fp = &sc->fp[rxq->queue_id];
	uint32_t nb_rx = 0;
	uint16_t hw_cq_cons, sw_cq_cons, sw_cq_prod;
	uint16_t bd_cons, bd_prod;
	struct rte_mbuf *new_mb;
	uint16_t rx_pref;
	struct eth_fast_path_rx_cqe *cqe_fp;
	uint16_t len, pad;
	struct rte_mbuf *rx_mb = NULL;

	hw_cq_cons = le16toh(*fp->rx_cq_cons_sb);
	if ((hw_cq_cons & USABLE_RCQ_ENTRIES_PER_PAGE) ==
			USABLE_RCQ_ENTRIES_PER_PAGE) {
		++hw_cq_cons;
	}

	bd_cons = rxq->rx_bd_head;
	bd_prod = rxq->rx_bd_tail;
	sw_cq_cons = rxq->rx_cq_head;
	sw_cq_prod = rxq->rx_cq_tail;

	if (sw_cq_cons == hw_cq_cons)
		return 0;

	while (nb_rx < nb_pkts && sw_cq_cons != hw_cq_cons) {

		bd_prod &= MAX_RX_BD(rxq);
		bd_cons &= MAX_RX_BD(rxq);

		cqe_fp = &rxq->cq_ring[sw_cq_cons & MAX_RX_BD(rxq)].fast_path_cqe;

		if (unlikely(CQE_TYPE_SLOW(cqe_fp->type_error_flags & ETH_FAST_PATH_RX_CQE_TYPE))) {
			PMD_RX_LOG(ERR, "slowpath event during traffic processing");
			break;
		}

		if (unlikely(cqe_fp->type_error_flags & ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG)) {
			PMD_RX_LOG(ERR, "flags 0x%x rx packet %u",
					cqe_fp->type_error_flags, sw_cq_cons);
			goto next_rx;
		}

		len = cqe_fp->pkt_len_or_gro_seg_len;
		pad = cqe_fp->placement_offset;

		new_mb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (unlikely(!new_mb)) {
			PMD_RX_LOG(ERR, "mbuf alloc fail fp[%02d]", fp->index);
			rte_eth_devices[rxq->port_id].data->
					rx_mbuf_alloc_failed++;
			goto next_rx;
		}

		rx_mb = rxq->sw_ring[bd_cons];
		rxq->sw_ring[bd_cons] = new_mb;
		rxq->rx_ring[bd_prod] = new_mb->buf_physaddr;

		rx_pref = NEXT_RX_BD(bd_cons) & MAX_RX_BD(rxq);
		rte_prefetch0(rxq->sw_ring[rx_pref]);
		if ((rx_pref & 0x3) == 0) {
			rte_prefetch0(&rxq->rx_ring[rx_pref]);
			rte_prefetch0(&rxq->sw_ring[rx_pref]);
		}

		rx_mb->data_off = pad;
		rx_mb->nb_segs = 1;
		rx_mb->next = NULL;
		rx_mb->pkt_len = rx_mb->data_len = len;
		rx_mb->port = rxq->port_id;
		rte_prefetch1(rte_pktmbuf_mtod(rx_mb, void *));

		/*
		 * If we received a packet with a vlan tag,
		 * attach that information to the packet.
		 */
		if (cqe_fp->pars_flags.flags & PARSING_FLAGS_VLAN) {
			rx_mb->vlan_tci = cqe_fp->vlan_tag;
			rx_mb->ol_flags |= PKT_RX_VLAN_PKT;
		}

		rx_pkts[nb_rx] = rx_mb;
		nb_rx++;

		/* limit spinning on the queue */
		if (unlikely(nb_rx == sc->rx_budget)) {
			PMD_RX_LOG(ERR, "Limit spinning on the queue");
			break;
		}

next_rx:
		bd_cons    = NEXT_RX_BD(bd_cons);
		bd_prod    = NEXT_RX_BD(bd_prod);
		sw_cq_prod = NEXT_RCQ_IDX(sw_cq_prod);
		sw_cq_cons = NEXT_RCQ_IDX(sw_cq_cons);
	}
	rxq->rx_bd_head = bd_cons;
	rxq->rx_bd_tail = bd_prod;
	rxq->rx_cq_head = sw_cq_cons;
	rxq->rx_cq_tail = sw_cq_prod;

	bnx2x_upd_rx_prod_fast(sc, fp, bd_prod, sw_cq_prod);

	return nb_rx;
}

int
bnx2x_dev_rx_init(struct rte_eth_dev *dev)
{
	dev->rx_pkt_burst = bnx2x_recv_pkts;

	return 0;
}

void
bnx2x_dev_clear_queues(struct rte_eth_dev *dev)
{
	uint8_t i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct bnx2x_tx_queue *txq = dev->data->tx_queues[i];
		if (txq != NULL) {
			bnx2x_tx_queue_release(txq);
			dev->data->tx_queues[i] = NULL;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct bnx2x_rx_queue *rxq = dev->data->rx_queues[i];
		if (rxq != NULL) {
			bnx2x_rx_queue_release(rxq);
			dev->data->rx_queues[i] = NULL;
		}
	}
}
