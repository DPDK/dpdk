/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_txrx.h"
#include "nbl_include.h"

static int nbl_res_txrx_alloc_rings(void *priv, u16 tx_num, u16 rx_num, u16 queue_offset)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_txrx_mgt *txrx_mgt = res_mgt->txrx_mgt;

	txrx_mgt->tx_rings = rte_calloc("nbl_txrings", tx_num,
					sizeof(struct nbl_res_tx_ring *), 0);
	if (!txrx_mgt->tx_rings) {
		NBL_LOG(ERR, "Allocate the tx rings array failed");
		return -ENOMEM;
	}

	txrx_mgt->rx_rings = rte_calloc("nbl_rxrings", rx_num,
					sizeof(struct nbl_res_rx_ring *), 0);
	if (!txrx_mgt->tx_rings) {
		NBL_LOG(ERR, "Allocate the rx rings array failed");
		rte_free(txrx_mgt->tx_rings);
		return -ENOMEM;
	}

	txrx_mgt->queue_offset = queue_offset;

	return 0;
}

static void nbl_res_txrx_remove_rings(void *priv)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_txrx_mgt *txrx_mgt = res_mgt->txrx_mgt;

	rte_free(txrx_mgt->tx_rings);
	rte_free(txrx_mgt->rx_rings);
}

static void nbl_res_txrx_stop_tx_ring(void *priv, u16 queue_idx)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_res_tx_ring *tx_ring = NBL_RES_MGT_TO_TX_RING(res_mgt, queue_idx);
	int i;

	if (!tx_ring)
		return;

	for (i = 0; i < tx_ring->nb_desc; i++) {
		if (tx_ring->tx_entry[i].mbuf != NULL) {
			rte_pktmbuf_free_seg(tx_ring->tx_entry[i].mbuf);
			memset(&tx_ring->tx_entry[i], 0, sizeof(*tx_ring->tx_entry));
		}
		tx_ring->desc[i].flags = 0;
	}

	tx_ring->avail_used_flags = NBL_PACKED_DESC_F_AVAIL_BIT;
	tx_ring->used_wrap_counter = 1;
	tx_ring->next_to_clean = NBL_TX_RS_THRESH - 1;
	tx_ring->next_to_use = 0;
	tx_ring->vq_free_cnt = tx_ring->nb_desc;
}

static void nbl_res_txrx_release_tx_ring(void *priv, u16 queue_idx)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_txrx_mgt *txrx_mgt = NBL_RES_MGT_TO_TXRX_MGT(res_mgt);
	struct nbl_res_tx_ring *tx_ring = NBL_RES_MGT_TO_TX_RING(res_mgt, queue_idx);
	if (!tx_ring)
		return;
	rte_free(tx_ring->tx_entry);
	rte_free(tx_ring);
	txrx_mgt->tx_rings[queue_idx] = NULL;
}

static inline u16 nbl_prep_tx_ehdr_leonis(union nbl_tx_extend_head *head, struct rte_mbuf *mbuf)
{
	RTE_SET_USED(head);
	RTE_SET_USED(mbuf);
	return 0;
}

static int nbl_res_txrx_start_tx_ring(void *priv,
				      struct nbl_start_tx_ring_param *param,
				      u64 *dma_addr)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_txrx_mgt *txrx_mgt = NBL_RES_MGT_TO_TXRX_MGT(res_mgt);
	struct nbl_res_tx_ring *tx_ring = NBL_RES_MGT_TO_TX_RING(res_mgt, param->queue_idx);
	const struct rte_eth_dev *eth_dev = NBL_RES_MGT_TO_ETH_DEV(res_mgt);
	struct nbl_common_info *common = NBL_RES_MGT_TO_COMMON(res_mgt);
	const struct nbl_hw_ops *hw_ops = NBL_RES_MGT_TO_HW_OPS(res_mgt);
	const struct rte_memzone *memzone;
	const struct rte_memzone *net_hdr_mz;
	char vq_hdr_name[NBL_VQ_HDR_NAME_MAXSIZE];
	struct nbl_tx_ehdr_leonis ext_hdr = {0};
	uint64_t offloads;
	u32 size;

	offloads = param->conf->offloads | eth_dev->data->dev_conf.txmode.offloads;

	if (eth_dev->data->tx_queues[param->queue_idx] != NULL) {
		NBL_LOG(WARNING, "re-setup an already allocated tx queue");
		nbl_res_txrx_stop_tx_ring(priv, param->queue_idx);
		eth_dev->data->tx_queues[param->queue_idx] = NULL;
	}

	tx_ring = rte_zmalloc("nbl_txring", sizeof(*tx_ring), RTE_CACHE_LINE_SIZE);
	if (!tx_ring) {
		NBL_LOG(ERR, "allocate tx queue data structure failed");
		return -ENOMEM;
	}
	memset(&tx_ring->default_hdr, 0, sizeof(tx_ring->default_hdr));
	switch (param->product) {
	case NBL_LEONIS_TYPE:
		tx_ring->exthdr_len = sizeof(struct nbl_tx_ehdr_leonis);
		tx_ring->prep_tx_ehdr = nbl_prep_tx_ehdr_leonis;
		ext_hdr.fwd = NBL_TX_FWD_TYPE_NORMAL;
		memcpy(&tx_ring->default_hdr, &ext_hdr, sizeof(struct nbl_tx_ehdr_leonis));
		break;
	default:
		tx_ring->exthdr_len = sizeof(union nbl_tx_extend_head);
		break;
	};

	tx_ring->tx_entry = rte_calloc("nbl_tx_entry",
				       param->nb_desc, sizeof(*tx_ring->tx_entry), 0);
	if (!tx_ring->tx_entry) {
		NBL_LOG(ERR, "allocate tx queue %d software ring failed", param->queue_idx);
		goto alloc_tx_entry_failed;
	}

	/* Alloc twice memory, and second half is used to back up the desc for desc checking */
	size = RTE_ALIGN_CEIL(sizeof(tx_ring->desc[0]) * param->nb_desc, 4096);
	memzone = rte_eth_dma_zone_reserve(eth_dev, "tx_ring", param->queue_idx,
					   size, RTE_CACHE_LINE_SIZE,
					   param->socket_id);
	if (memzone == NULL) {
		NBL_LOG(ERR, "reserve dma zone for tx ring failed");
		goto alloc_dma_zone_failed;
	}

	/* if has no memory to put extend header, apply for new memory */
	size = param->nb_desc * NBL_TX_HEADER_LEN;
	snprintf(vq_hdr_name, sizeof(vq_hdr_name), "port%d_vq%d_hdr",
			eth_dev->data->port_id, param->queue_idx);
	net_hdr_mz = rte_memzone_reserve_aligned(vq_hdr_name, size,
						 param->socket_id,
						 RTE_MEMZONE_IOVA_CONTIG,
						 RTE_CACHE_LINE_SIZE);
	if (net_hdr_mz == NULL) {
		if (rte_errno == EEXIST)
			net_hdr_mz = rte_memzone_lookup(vq_hdr_name);
		if (net_hdr_mz == NULL) {
			NBL_LOG(ERR, "reserve net_hdr_mz dma zone for tx ring failed");
			goto reserve_net_hdr_mz_failed;
		}
	}

	tx_ring->product = param->product;
	tx_ring->nb_desc = param->nb_desc;
	tx_ring->vq_free_cnt = param->nb_desc;
	tx_ring->queue_id = param->queue_idx;
	tx_ring->notify_qid =
		(res_mgt->res_info.base_qid + txrx_mgt->queue_offset + param->queue_idx) * 2 + 1;
	tx_ring->ring_phys_addr = (u64)NBL_DMA_ADDRESS_FULL_TRANSLATE(common, memzone->iova);
	tx_ring->avail_used_flags = NBL_PACKED_DESC_F_AVAIL_BIT;
	tx_ring->used_wrap_counter = 1;
	tx_ring->next_to_clean = NBL_TX_RS_THRESH - 1;
	tx_ring->next_to_use = 0;
	tx_ring->desc = (struct nbl_packed_desc *)memzone->addr;
	tx_ring->net_hdr_mz = net_hdr_mz;
	tx_ring->eth_dev = eth_dev;
	tx_ring->dma_set_msb = common->dma_set_msb;
	tx_ring->dma_limit_msb = common->dma_limit_msb;
	tx_ring->notify = hw_ops->get_tail_ptr(NBL_RES_MGT_TO_HW_PRIV(res_mgt));
	tx_ring->offloads = offloads;
	tx_ring->common = common;

	eth_dev->data->tx_queues[param->queue_idx] = tx_ring;

	txrx_mgt->tx_rings[param->queue_idx] = tx_ring;
	txrx_mgt->tx_ring_num++;

	*dma_addr = tx_ring->ring_phys_addr;

	return 0;

reserve_net_hdr_mz_failed:
	rte_memzone_free(memzone);
alloc_dma_zone_failed:
	rte_free(tx_ring->tx_entry);
	tx_ring->tx_entry = NULL;
	tx_ring->size = 0;
alloc_tx_entry_failed:
	rte_free(tx_ring);
	return -ENOMEM;
}

static void nbl_res_txrx_stop_rx_ring(void *priv, u16 queue_idx)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_res_rx_ring *rx_ring =
			NBL_RES_MGT_TO_RX_RING(res_mgt, queue_idx);
	u16 i;

	if (!rx_ring)
		return;
	if (rx_ring->rx_entry != NULL) {
		for (i = 0; i < rx_ring->nb_desc; i++) {
			if (rx_ring->rx_entry[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rx_ring->rx_entry[i].mbuf);
				rx_ring->rx_entry[i].mbuf = NULL;
			}
			rx_ring->desc[i].flags = 0;
		}

		for (i = rx_ring->nb_desc; i < rx_ring->nb_desc + NBL_DESC_PER_LOOP_VEC_MAX; i++)
			rx_ring->desc[i].flags = 0;
	}

	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;
}

static int nbl_res_txrx_start_rx_ring(void *priv,
				      struct nbl_start_rx_ring_param *param,
				      u64 *dma_addr)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_txrx_mgt *txrx_mgt = NBL_RES_MGT_TO_TXRX_MGT(res_mgt);
	struct nbl_res_rx_ring *rx_ring = NBL_RES_MGT_TO_RX_RING(res_mgt, param->queue_idx);
	const struct rte_eth_dev *eth_dev = NBL_RES_MGT_TO_ETH_DEV(res_mgt);
	const struct nbl_hw_ops *hw_ops = NBL_RES_MGT_TO_HW_OPS(res_mgt);
	struct nbl_common_info *common = NBL_RES_MGT_TO_COMMON(res_mgt);
	const struct rte_memzone *memzone;
	u32 size;

	if (eth_dev->data->rx_queues[param->queue_idx] != NULL) {
		NBL_LOG(WARNING, "re-setup an already allocated rx queue");
		nbl_res_txrx_stop_rx_ring(priv, param->queue_idx);
		eth_dev->data->rx_queues[param->queue_idx] = NULL;
	}

	rx_ring = rte_zmalloc_socket("nbl_rxring", sizeof(*rx_ring),
				 RTE_CACHE_LINE_SIZE, param->socket_id);
	if (rx_ring == NULL) {
		NBL_LOG(ERR, "allocate rx queue data structure failed");
		return -ENOMEM;
	}

	size = sizeof(rx_ring->rx_entry[0]) * (param->nb_desc + NBL_DESC_PER_LOOP_VEC_MAX);
	rx_ring->rx_entry = rte_zmalloc_socket("rxq rx_entry", size,
					       RTE_CACHE_LINE_SIZE,
					       param->socket_id);
	if (rx_ring->rx_entry == NULL) {
		NBL_LOG(ERR, "allocate rx queue %d software ring failed", param->queue_idx);
		goto alloc_rx_entry_failed;
	}

	size = sizeof(rx_ring->desc[0]) * (param->nb_desc + NBL_DESC_PER_LOOP_VEC_MAX);
	memzone = rte_eth_dma_zone_reserve(eth_dev, "rx_ring", param->queue_idx,
					   size, RTE_CACHE_LINE_SIZE,
					   param->socket_id);
	if (memzone == NULL) {
		NBL_LOG(ERR, "reserve dma zone for rx ring failed");
		goto alloc_dma_zone_failed;
	}

	rx_ring->product = param->product;
	rx_ring->mempool = param->mempool;
	rx_ring->nb_desc = param->nb_desc;
	rx_ring->queue_id = param->queue_idx;
	rx_ring->notify_qid =
		(res_mgt->res_info.base_qid + txrx_mgt->queue_offset + param->queue_idx) * 2;
	rx_ring->ring_phys_addr = NBL_DMA_ADDRESS_FULL_TRANSLATE(common, memzone->iova);
	rx_ring->desc = (struct nbl_packed_desc *)memzone->addr;
	rx_ring->port_id = eth_dev->data->port_id;
	rx_ring->eth_dev = eth_dev;
	rx_ring->dma_set_msb = common->dma_set_msb;
	rx_ring->dma_limit_msb = common->dma_limit_msb;
	rx_ring->common = common;
	rx_ring->notify = hw_ops->get_tail_ptr(NBL_RES_MGT_TO_HW_PRIV(res_mgt));

	switch (param->product) {
	case NBL_LEONIS_TYPE:
		rx_ring->exthdr_len = sizeof(struct nbl_rx_ehdr_leonis);
		break;
	default:
		rx_ring->exthdr_len = sizeof(union nbl_rx_extend_head);
	};

	eth_dev->data->rx_queues[param->queue_idx] = rx_ring;

	txrx_mgt->rx_rings[param->queue_idx] = rx_ring;
	txrx_mgt->rx_ring_num++;

	*dma_addr = rx_ring->ring_phys_addr;

	return 0;

alloc_dma_zone_failed:
	rte_free(rx_ring->rx_entry);
	rx_ring->rx_entry = NULL;
	rx_ring->size = 0;
alloc_rx_entry_failed:
	rte_free(rx_ring);
	return -ENOMEM;
}

static int nbl_res_alloc_rx_bufs(void *priv, u16 queue_idx)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_res_rx_ring *rxq = NBL_RES_MGT_TO_RX_RING(res_mgt, queue_idx);
	struct nbl_rx_entry *rx_entry = rxq->rx_entry;
	volatile struct nbl_packed_desc *rx_desc;
	struct nbl_rx_entry *rxe;
	struct rte_mbuf *mbuf;
	u64 dma_addr;
	int i;
	u32 frame_size = rxq->eth_dev->data->mtu + NBL_ETH_OVERHEAD + rxq->exthdr_len;
	u16 buf_length;

	rxq->avail_used_flags = NBL_PACKED_DESC_F_AVAIL_BIT | NBL_PACKED_DESC_F_WRITE_BIT;
	rxq->used_wrap_counter = 1;

	for (i = 0; i < rxq->nb_desc; i++) {
		mbuf = rte_mbuf_raw_alloc(rxq->mempool);
		if (mbuf == NULL) {
			NBL_LOG(ERR, "RX mbuf alloc failed for queue %u", rxq->queue_id);
			return -ENOMEM;
		}
		dma_addr = NBL_DMA_ADDRESS_FULL_TRANSLATE(rxq, rte_mbuf_data_iova_default(mbuf));
		rx_desc = &rxq->desc[i];
		rxe = &rx_entry[i];
		rx_desc->addr = dma_addr;
		rx_desc->len = mbuf->buf_len - RTE_PKTMBUF_HEADROOM;
		rx_desc->flags = rxq->avail_used_flags;
		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		rxe->mbuf = mbuf;
	}

	rxq->next_to_clean = 0;
	rxq->next_to_use = 0;
	rxq->vq_free_cnt = 0;
	rxq->avail_used_flags ^= NBL_PACKED_DESC_F_AVAIL_USED;

	buf_length = rte_pktmbuf_data_room_size(rxq->mempool) - RTE_PKTMBUF_HEADROOM;
	if (buf_length >= NBL_BUF_LEN_16K) {
		rxq->buf_length = NBL_BUF_LEN_16K;
	} else if (buf_length >= NBL_BUF_LEN_8K) {
		rxq->buf_length = NBL_BUF_LEN_8K;
	} else if (buf_length >= NBL_BUF_LEN_4K) {
		rxq->buf_length = NBL_BUF_LEN_4K;
	} else if (buf_length >= NBL_BUF_LEN_2K) {
		rxq->buf_length = NBL_BUF_LEN_2K;
	} else {
		NBL_LOG(ERR, "mempool mbuf length should be at least 2kB, but current value is %u",
			buf_length);
		nbl_res_txrx_stop_rx_ring(res_mgt, queue_idx);
		return -EINVAL;
	}

	if (frame_size > rxq->buf_length)
		rxq->eth_dev->data->scattered_rx = 1;

	rxq->buf_length = rxq->buf_length - RTE_PKTMBUF_HEADROOM;

	return 0;
}

static void nbl_res_txrx_release_rx_ring(void *priv, u16 queue_idx)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_txrx_mgt *txrx_mgt = NBL_RES_MGT_TO_TXRX_MGT(res_mgt);
	struct nbl_res_rx_ring *rx_ring =
			NBL_RES_MGT_TO_RX_RING(res_mgt, queue_idx);
	if (!rx_ring)
		return;

	rte_free(rx_ring->rx_entry);
	rte_free(rx_ring);
	txrx_mgt->rx_rings[queue_idx] = NULL;
}

static void nbl_res_txrx_update_rx_ring(void *priv, u16 index)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	const struct nbl_hw_ops *hw_ops = NBL_RES_MGT_TO_HW_OPS(res_mgt);
	struct nbl_res_rx_ring *rx_ring = NBL_RES_MGT_TO_RX_RING(res_mgt, index);

	hw_ops->update_tail_ptr(NBL_RES_MGT_TO_HW_PRIV(res_mgt),
				 rx_ring->notify_qid,
				 ((!!(rx_ring->avail_used_flags & NBL_PACKED_DESC_F_AVAIL_BIT)) |
				 rx_ring->next_to_use));
}

/* NBL_TXRX_SET_OPS(ops_name, func)
 *
 * Use X Macros to reduce setup and remove codes.
 */
#define NBL_TXRX_OPS_TBL							\
do {										\
	NBL_TXRX_SET_OPS(alloc_rings, nbl_res_txrx_alloc_rings);		\
	NBL_TXRX_SET_OPS(remove_rings, nbl_res_txrx_remove_rings);		\
	NBL_TXRX_SET_OPS(start_tx_ring, nbl_res_txrx_start_tx_ring);		\
	NBL_TXRX_SET_OPS(stop_tx_ring, nbl_res_txrx_stop_tx_ring);		\
	NBL_TXRX_SET_OPS(release_tx_ring, nbl_res_txrx_release_tx_ring);	\
	NBL_TXRX_SET_OPS(start_rx_ring, nbl_res_txrx_start_rx_ring);		\
	NBL_TXRX_SET_OPS(alloc_rx_bufs, nbl_res_alloc_rx_bufs);			\
	NBL_TXRX_SET_OPS(stop_rx_ring, nbl_res_txrx_stop_rx_ring);		\
	NBL_TXRX_SET_OPS(release_rx_ring, nbl_res_txrx_release_rx_ring);	\
	NBL_TXRX_SET_OPS(update_rx_ring, nbl_res_txrx_update_rx_ring);		\
} while (0)

/* Structure starts here, adding an op should not modify anything below */
static int nbl_txrx_setup_mgt(struct nbl_txrx_mgt **txrx_mgt)
{
	*txrx_mgt = rte_zmalloc("nbl_txrx_mgt", sizeof(struct nbl_txrx_mgt), 0);
	if (!*txrx_mgt)
		return -ENOMEM;

	return 0;
}

static void nbl_txrx_remove_mgt(struct nbl_txrx_mgt **txrx_mgt)
{
	rte_free(*txrx_mgt);
	*txrx_mgt = NULL;
}

int nbl_txrx_mgt_start(struct nbl_resource_mgt *res_mgt)
{
	struct nbl_txrx_mgt **txrx_mgt;

	txrx_mgt = &NBL_RES_MGT_TO_TXRX_MGT(res_mgt);

	return nbl_txrx_setup_mgt(txrx_mgt);
}

void nbl_txrx_mgt_stop(struct nbl_resource_mgt *res_mgt)
{
	struct nbl_txrx_mgt **txrx_mgt;

	txrx_mgt = &NBL_RES_MGT_TO_TXRX_MGT(res_mgt);

	if (!(*txrx_mgt))
		return;

	nbl_txrx_remove_mgt(txrx_mgt);
}

int nbl_txrx_setup_ops(struct nbl_resource_ops *res_ops)
{
#define NBL_TXRX_SET_OPS(name, func) do {res_ops->NBL_NAME(name) = func; ; } while (0)
	NBL_TXRX_OPS_TBL;
#undef  NBL_TXRX_SET_OPS

	return 0;
}

void nbl_txrx_remove_ops(struct nbl_resource_ops *res_ops)
{
#define NBL_TXRX_SET_OPS(name, func)		\
do {						\
	res_ops->NBL_NAME(name) = NULL;		\
	RTE_SET_USED(func);			\
} while (0)
	NBL_TXRX_OPS_TBL;
#undef  NBL_TXRX_SET_OPS
}
