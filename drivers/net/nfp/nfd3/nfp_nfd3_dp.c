/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include <ethdev_driver.h>
#include <bus_pci_driver.h>
#include <rte_malloc.h>

#include "../nfp_logs.h"
#include "../nfp_common.h"
#include "../nfp_rxtx.h"
#include "../flower/nfp_flower.h"
#include "../flower/nfp_flower_cmsg.h"
#include "nfp_nfd3.h"

uint32_t
nfp_flower_nfd3_pkt_add_metadata(struct rte_mbuf *mbuf,
		uint32_t port_id)
{
	char *meta_offset;

	meta_offset = rte_pktmbuf_prepend(mbuf, FLOWER_PKT_DATA_OFFSET);
	*(rte_be32_t *)meta_offset = rte_cpu_to_be_32(NFP_NET_META_PORTID);
	meta_offset += NFP_NET_META_HEADER_SIZE;
	*(rte_be32_t *)meta_offset = rte_cpu_to_be_32(port_id);

	return FLOWER_PKT_DATA_OFFSET;
}

/*
 * nfp_net_nfd3_tx_vlan() - Set vlan info in the nfd3 tx desc
 *
 * If enable NFP_NET_CFG_CTRL_TXVLAN_V2
 *	Vlan_info is stored in the meta and
 *	is handled in the nfp_net_nfd3_set_meta_vlan()
 * else if enable NFP_NET_CFG_CTRL_TXVLAN
 *	Vlan_info is stored in the tx_desc and
 *	is handled in the nfp_net_nfd3_tx_vlan()
 */
static inline void
nfp_net_nfd3_tx_vlan(struct nfp_net_txq *txq,
		struct nfp_net_nfd3_tx_desc *txd,
		struct rte_mbuf *mb)
{
	struct nfp_net_hw *hw = txq->hw;

	if ((hw->cap & NFP_NET_CFG_CTRL_TXVLAN_V2) != 0 ||
			(hw->cap & NFP_NET_CFG_CTRL_TXVLAN) == 0)
		return;

	if ((mb->ol_flags & RTE_MBUF_F_TX_VLAN) != 0) {
		txd->flags |= NFD3_DESC_TX_VLAN;
		txd->vlan = mb->vlan_tci;
	}
}

static inline void
nfp_net_nfd3_set_meta_data(struct nfp_net_meta_raw *meta_data,
		struct nfp_net_txq *txq,
		struct rte_mbuf *pkt)
{
	char *meta;
	uint8_t layer = 0;
	uint32_t meta_info;
	struct nfp_net_hw *hw;
	uint8_t vlan_layer = 0;

	hw = txq->hw;

	if ((pkt->ol_flags & RTE_MBUF_F_TX_VLAN) != 0 &&
			(hw->ctrl & NFP_NET_CFG_CTRL_TXVLAN_V2) != 0) {
		if (meta_data->length == 0)
			meta_data->length = NFP_NET_META_HEADER_SIZE;
		meta_data->length += NFP_NET_META_FIELD_SIZE;
		meta_data->header |= NFP_NET_META_VLAN;
	}

	if (meta_data->length == 0)
		return;

	meta_info = meta_data->header;
	meta_data->header = rte_cpu_to_be_32(meta_data->header);
	meta = rte_pktmbuf_prepend(pkt, meta_data->length);
	memcpy(meta, &meta_data->header, sizeof(meta_data->header));
	meta += NFP_NET_META_HEADER_SIZE;

	for (; meta_info != 0; meta_info >>= NFP_NET_META_FIELD_SIZE, layer++,
			meta += NFP_NET_META_FIELD_SIZE) {
		switch (meta_info & NFP_NET_META_FIELD_MASK) {
		case NFP_NET_META_VLAN:
			if (vlan_layer > 0) {
				PMD_DRV_LOG(ERR, "At most 1 layers of vlan is supported");
				return;
			}
			nfp_net_set_meta_vlan(meta_data, pkt, layer);
			vlan_layer++;
			break;
		default:
			PMD_DRV_LOG(ERR, "The metadata type not supported");
			return;
		}

		memcpy(meta, &meta_data->data[layer], sizeof(meta_data->data[layer]));
	}
}

uint16_t
nfp_net_nfd3_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	return nfp_net_nfd3_xmit_pkts_common(tx_queue, tx_pkts, nb_pkts, false);
}

uint16_t
nfp_net_nfd3_xmit_pkts_common(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts,
		bool repr_flag)
{
	int i;
	int pkt_size;
	int dma_size;
	uint8_t offset;
	uint64_t dma_addr;
	uint16_t free_descs;
	uint16_t issued_descs;
	struct rte_mbuf *pkt;
	struct nfp_net_hw *hw;
	struct rte_mbuf **lmbuf;
	struct nfp_net_txq *txq;
	struct nfp_net_nfd3_tx_desc txd;
	struct nfp_net_nfd3_tx_desc *txds;

	txq = tx_queue;
	hw = txq->hw;
	txds = &txq->txds[txq->wr_p];

	PMD_TX_LOG(DEBUG, "working for queue %hu at pos %d and %hu packets",
			txq->qidx, txq->wr_p, nb_pkts);

	if (nfp_net_nfd3_free_tx_desc(txq) < NFD3_TX_DESC_PER_PKT * nb_pkts ||
			nfp_net_nfd3_txq_full(txq))
		nfp_net_tx_free_bufs(txq);

	free_descs = nfp_net_nfd3_free_tx_desc(txq);
	if (unlikely(free_descs == 0))
		return 0;

	pkt = *tx_pkts;

	issued_descs = 0;
	PMD_TX_LOG(DEBUG, "queue: %hu. Sending %hu packets", txq->qidx, nb_pkts);

	/* Sending packets */
	for (i = 0; i < nb_pkts && free_descs > 0; i++) {
		/* Grabbing the mbuf linked to the current descriptor */
		lmbuf = &txq->txbufs[txq->wr_p].mbuf;
		/* Warming the cache for releasing the mbuf later on */
		RTE_MBUF_PREFETCH_TO_FREE(*lmbuf);

		pkt = *(tx_pkts + i);

		if (!repr_flag) {
			struct nfp_net_meta_raw meta_data;
			memset(&meta_data, 0, sizeof(meta_data));
			nfp_net_nfd3_set_meta_data(&meta_data, txq, pkt);
			offset = meta_data.length;
		} else {
			offset = FLOWER_PKT_DATA_OFFSET;
		}

		if (unlikely(pkt->nb_segs > 1 &&
				(hw->cap & NFP_NET_CFG_CTRL_GATHER) == 0)) {
			PMD_TX_LOG(ERR, "Multisegment packet not supported");
			goto xmit_end;
		}

		/* Checking if we have enough descriptors */
		if (unlikely(pkt->nb_segs > free_descs))
			goto xmit_end;

		/*
		 * Checksum and VLAN flags just in the first descriptor for a
		 * multisegment packet, but TSO info needs to be in all of them.
		 */
		txd.data_len = pkt->pkt_len;
		nfp_net_nfd3_tx_tso(txq, &txd, pkt);
		nfp_net_nfd3_tx_cksum(txq, &txd, pkt);
		nfp_net_nfd3_tx_vlan(txq, &txd, pkt);

		/*
		 * mbuf data_len is the data in one segment and pkt_len data
		 * in the whole packet. When the packet is just one segment,
		 * then data_len = pkt_len
		 */
		pkt_size = pkt->pkt_len;

		while (pkt != NULL && free_descs > 0) {
			/* Copying TSO, VLAN and cksum info */
			*txds = txd;

			/* Releasing mbuf used by this descriptor previously */
			if (*lmbuf != NULL)
				rte_pktmbuf_free_seg(*lmbuf);

			/*
			 * Linking mbuf with descriptor for being released
			 * next time descriptor is used
			 */
			*lmbuf = pkt;

			dma_size = pkt->data_len;
			dma_addr = rte_mbuf_data_iova(pkt);

			/* Filling descriptors fields */
			txds->dma_len = dma_size;
			txds->data_len = txd.data_len;
			txds->dma_addr_hi = (dma_addr >> 32) & 0xff;
			txds->dma_addr_lo = (dma_addr & 0xffffffff);
			free_descs--;

			txq->wr_p++;
			if (unlikely(txq->wr_p == txq->tx_count)) /* wrapping */
				txq->wr_p = 0;

			pkt_size -= dma_size;

			/*
			 * Making the EOP, packets with just one segment
			 * the priority
			 */
			if (likely(pkt_size == 0))
				txds->offset_eop = NFD3_DESC_TX_EOP;
			else
				txds->offset_eop = 0;

			/* Set the meta_len */
			txds->offset_eop |= offset;

			pkt = pkt->next;
			/* Referencing next free TX descriptor */
			txds = &txq->txds[txq->wr_p];
			lmbuf = &txq->txbufs[txq->wr_p].mbuf;
			issued_descs++;
		}
	}

xmit_end:
	/* Increment write pointers. Force memory write before we let HW know */
	rte_wmb();
	nfp_qcp_ptr_add(txq->qcp_q, NFP_QCP_WRITE_PTR, issued_descs);

	return i;
}

int
nfp_net_nfd3_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf)
{
	int ret;
	size_t size;
	uint32_t tx_desc_sz;
	uint16_t min_tx_desc;
	uint16_t max_tx_desc;
	struct nfp_net_hw *hw;
	struct nfp_net_txq *txq;
	uint16_t tx_free_thresh;
	const struct rte_memzone *tz;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	ret = nfp_net_tx_desc_limits(hw, &min_tx_desc, &max_tx_desc);
	if (ret != 0)
		return ret;

	/* Validating number of descriptors */
	tx_desc_sz = nb_desc * sizeof(struct nfp_net_nfd3_tx_desc);
	if ((NFD3_TX_DESC_PER_PKT * tx_desc_sz) % NFP_ALIGN_RING_DESC != 0 ||
			nb_desc > max_tx_desc || nb_desc < min_tx_desc) {
		PMD_DRV_LOG(ERR, "Wrong nb_desc value");
		return -EINVAL;
	}

	tx_free_thresh = (tx_conf->tx_free_thresh != 0) ?
			tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH;
	if (tx_free_thresh > nb_desc) {
		PMD_DRV_LOG(ERR, "tx_free_thresh must be less than the number of TX "
				"descriptors. (tx_free_thresh=%u port=%d queue=%d)",
				tx_free_thresh, dev->data->port_id, queue_idx);
		return -EINVAL;
	}

	/*
	 * Free memory prior to re-allocation if needed. This is the case after
	 * calling nfp_net_stop().
	 */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		PMD_TX_LOG(DEBUG, "Freeing memory prior to re-allocation %d",
				queue_idx);
		nfp_net_tx_queue_release(dev, queue_idx);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocating tx queue data structure */
	txq = rte_zmalloc_socket("ethdev TX queue", sizeof(struct nfp_net_txq),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL) {
		PMD_DRV_LOG(ERR, "Error allocating tx dma");
		return -ENOMEM;
	}

	dev->data->tx_queues[queue_idx] = txq;

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	size = sizeof(struct nfp_net_nfd3_tx_desc) * NFD3_TX_DESC_PER_PKT * max_tx_desc;
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx, size,
			NFP_MEMZONE_ALIGN, socket_id);
	if (tz == NULL) {
		PMD_DRV_LOG(ERR, "Error allocating tx dma");
		nfp_net_tx_queue_release(dev, queue_idx);
		dev->data->tx_queues[queue_idx] = NULL;
		return -ENOMEM;
	}

	txq->tx_count = nb_desc * NFD3_TX_DESC_PER_PKT;
	txq->tx_free_thresh = tx_free_thresh;

	/* queue mapping based on firmware configuration */
	txq->qidx = queue_idx;
	txq->tx_qcidx = queue_idx * hw->stride_tx;
	txq->qcp_q = hw->tx_bar + NFP_QCP_QUEUE_OFF(txq->tx_qcidx);
	txq->port_id = dev->data->port_id;

	/* Saving physical and virtual addresses for the TX ring */
	txq->dma = tz->iova;
	txq->txds = tz->addr;

	/* mbuf pointers array for referencing mbufs linked to TX descriptors */
	txq->txbufs = rte_zmalloc_socket("txq->txbufs",
			sizeof(*txq->txbufs) * txq->tx_count,
			RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->txbufs == NULL) {
		nfp_net_tx_queue_release(dev, queue_idx);
		dev->data->tx_queues[queue_idx] = NULL;
		return -ENOMEM;
	}

	nfp_net_reset_tx_queue(txq);

	txq->hw = hw;

	/*
	 * Telling the HW about the physical address of the TX ring and number
	 * of descriptors in log2 format
	 */
	nn_cfg_writeq(hw, NFP_NET_CFG_TXR_ADDR(queue_idx), txq->dma);
	nn_cfg_writeb(hw, NFP_NET_CFG_TXR_SZ(queue_idx), rte_log2_u32(txq->tx_count));

	return 0;
}
