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
#include "../nfpcore/nfp_mip.h"
#include "../nfpcore/nfp_rtsym.h"
#include "nfp_nfdk.h"

static inline int
nfp_net_nfdk_headlen_to_segs(unsigned int headlen)
{
	return DIV_ROUND_UP(headlen +
			NFDK_TX_MAX_DATA_PER_DESC -
			NFDK_TX_MAX_DATA_PER_HEAD,
			NFDK_TX_MAX_DATA_PER_DESC);
}

static int
nfp_net_nfdk_tx_maybe_close_block(struct nfp_net_txq *txq, struct rte_mbuf *pkt)
{
	unsigned int n_descs, wr_p, i, nop_slots;
	struct rte_mbuf *pkt_temp;

	pkt_temp = pkt;
	n_descs = nfp_net_nfdk_headlen_to_segs(pkt_temp->data_len);
	while (pkt_temp->next) {
		pkt_temp = pkt_temp->next;
		n_descs += DIV_ROUND_UP(pkt_temp->data_len, NFDK_TX_MAX_DATA_PER_DESC);
	}

	if (unlikely(n_descs > NFDK_TX_DESC_GATHER_MAX))
		return -EINVAL;

	/* Under count by 1 (don't count meta) for the round down to work out */
	n_descs += !!(pkt->ol_flags & RTE_MBUF_F_TX_TCP_SEG);

	if (round_down(txq->wr_p, NFDK_TX_DESC_BLOCK_CNT) !=
			round_down(txq->wr_p + n_descs, NFDK_TX_DESC_BLOCK_CNT))
		goto close_block;

	if ((uint32_t)txq->data_pending + pkt->pkt_len > NFDK_TX_MAX_DATA_PER_BLOCK)
		goto close_block;

	return 0;

close_block:
	wr_p = txq->wr_p;
	nop_slots = D_BLOCK_CPL(wr_p);

	memset(&txq->ktxds[wr_p], 0, nop_slots * sizeof(struct nfp_net_nfdk_tx_desc));
	for (i = wr_p; i < nop_slots + wr_p; i++) {
		if (txq->txbufs[i].mbuf) {
			rte_pktmbuf_free_seg(txq->txbufs[i].mbuf);
			txq->txbufs[i].mbuf = NULL;
		}
	}
	txq->data_pending = 0;
	txq->wr_p = D_IDX(txq, txq->wr_p + nop_slots);

	return nop_slots;
}

static void
nfp_net_nfdk_set_meta_data(struct rte_mbuf *pkt,
		struct nfp_net_txq *txq,
		uint64_t *metadata)
{
	char *meta;
	uint8_t layer = 0;
	uint32_t meta_type;
	struct nfp_net_hw *hw;
	uint32_t header_offset;
	uint8_t vlan_layer = 0;
	struct nfp_net_meta_raw meta_data;

	memset(&meta_data, 0, sizeof(meta_data));
	hw = txq->hw;

	if ((pkt->ol_flags & RTE_MBUF_F_TX_VLAN) != 0 &&
			(hw->ctrl & NFP_NET_CFG_CTRL_TXVLAN_V2) != 0) {
		if (meta_data.length == 0)
			meta_data.length = NFP_NET_META_HEADER_SIZE;
		meta_data.length += NFP_NET_META_FIELD_SIZE;
		meta_data.header |= NFP_NET_META_VLAN;
	}

	if (meta_data.length == 0)
		return;

	meta_type = meta_data.header;
	header_offset = meta_type << NFP_NET_META_NFDK_LENGTH;
	meta_data.header = header_offset | meta_data.length;
	meta_data.header = rte_cpu_to_be_32(meta_data.header);
	meta = rte_pktmbuf_prepend(pkt, meta_data.length);
	memcpy(meta, &meta_data.header, sizeof(meta_data.header));
	meta += NFP_NET_META_HEADER_SIZE;

	for (; meta_type != 0; meta_type >>= NFP_NET_META_FIELD_SIZE, layer++,
			meta += NFP_NET_META_FIELD_SIZE) {
		switch (meta_type & NFP_NET_META_FIELD_MASK) {
		case NFP_NET_META_VLAN:
			if (vlan_layer > 0) {
				PMD_DRV_LOG(ERR, "At most 1 layers of vlan is supported");
				return;
			}
			nfp_net_set_meta_vlan(&meta_data, pkt, layer);
			vlan_layer++;
			break;
		default:
			PMD_DRV_LOG(ERR, "The metadata type not supported");
			return;
		}

		memcpy(meta, &meta_data.data[layer], sizeof(meta_data.data[layer]));
	}

	*metadata = NFDK_DESC_TX_CHAIN_META;
}

uint16_t
nfp_net_nfdk_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	uint32_t buf_idx;
	uint64_t dma_addr;
	uint16_t free_descs;
	uint32_t npkts = 0;
	uint64_t metadata = 0;
	uint16_t issued_descs = 0;
	struct nfp_net_txq *txq;
	struct nfp_net_hw *hw;
	struct nfp_net_nfdk_tx_desc *ktxds;
	struct rte_mbuf *pkt, *temp_pkt;
	struct rte_mbuf **lmbuf;

	txq = tx_queue;
	hw = txq->hw;

	PMD_TX_LOG(DEBUG, "working for queue %u at pos %d and %u packets",
		txq->qidx, txq->wr_p, nb_pkts);

	if ((nfp_net_nfdk_free_tx_desc(txq) < NFDK_TX_DESC_PER_SIMPLE_PKT *
			nb_pkts) || (nfp_net_nfdk_txq_full(txq)))
		nfp_net_tx_free_bufs(txq);

	free_descs = (uint16_t)nfp_net_nfdk_free_tx_desc(txq);
	if (unlikely(free_descs == 0))
		return 0;

	PMD_TX_LOG(DEBUG, "queue: %u. Sending %u packets", txq->qidx, nb_pkts);
	/* Sending packets */
	while ((npkts < nb_pkts) && free_descs) {
		uint32_t type, dma_len, dlen_type, tmp_dlen;
		int nop_descs, used_descs;

		pkt = *(tx_pkts + npkts);
		nop_descs = nfp_net_nfdk_tx_maybe_close_block(txq, pkt);
		if (nop_descs < 0)
			goto xmit_end;

		issued_descs += nop_descs;
		ktxds = &txq->ktxds[txq->wr_p];
		/* Grabbing the mbuf linked to the current descriptor */
		buf_idx = txq->wr_p;
		lmbuf = &txq->txbufs[buf_idx++].mbuf;
		/* Warming the cache for releasing the mbuf later on */
		RTE_MBUF_PREFETCH_TO_FREE(*lmbuf);

		temp_pkt = pkt;
		nfp_net_nfdk_set_meta_data(pkt, txq, &metadata);

		if (unlikely(pkt->nb_segs > 1 &&
				!(hw->cap & NFP_NET_CFG_CTRL_GATHER))) {
			PMD_INIT_LOG(ERR, "Multisegment packet not supported");
			goto xmit_end;
		}

		/*
		 * Checksum and VLAN flags just in the first descriptor for a
		 * multisegment packet, but TSO info needs to be in all of them.
		 */

		dma_len = pkt->data_len;
		if ((hw->cap & NFP_NET_CFG_CTRL_LSO_ANY) &&
				(pkt->ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
			type = NFDK_DESC_TX_TYPE_TSO;
		} else if (pkt->next == NULL && dma_len <= NFDK_TX_MAX_DATA_PER_HEAD) {
			type = NFDK_DESC_TX_TYPE_SIMPLE;
		} else {
			type = NFDK_DESC_TX_TYPE_GATHER;
		}

		/* Implicitly truncates to chunk in below logic */
		dma_len -= 1;

		/*
		 * We will do our best to pass as much data as we can in descriptor
		 * and we need to make sure the first descriptor includes whole
		 * head since there is limitation in firmware side. Sometimes the
		 * value of 'dma_len & NFDK_DESC_TX_DMA_LEN_HEAD' will be less
		 * than packet head len.
		 */
		dlen_type = (dma_len > NFDK_DESC_TX_DMA_LEN_HEAD ?
				NFDK_DESC_TX_DMA_LEN_HEAD : dma_len) |
			(NFDK_DESC_TX_TYPE_HEAD & (type << 12));
		ktxds->dma_len_type = rte_cpu_to_le_16(dlen_type);
		dma_addr = rte_mbuf_data_iova(pkt);
		PMD_TX_LOG(DEBUG, "Working with mbuf at dma address:"
				"%" PRIx64 "", dma_addr);
		ktxds->dma_addr_hi = rte_cpu_to_le_16(dma_addr >> 32);
		ktxds->dma_addr_lo = rte_cpu_to_le_32(dma_addr & 0xffffffff);
		ktxds++;

		/*
		 * Preserve the original dlen_type, this way below the EOP logic
		 * can use dlen_type.
		 */
		tmp_dlen = dlen_type & NFDK_DESC_TX_DMA_LEN_HEAD;
		dma_len -= tmp_dlen;
		dma_addr += tmp_dlen + 1;

		/*
		 * The rest of the data (if any) will be in larger DMA descriptors
		 * and is handled with the dma_len loop.
		 */
		while (pkt) {
			if (*lmbuf)
				rte_pktmbuf_free_seg(*lmbuf);
			*lmbuf = pkt;
			while (dma_len > 0) {
				dma_len -= 1;
				dlen_type = NFDK_DESC_TX_DMA_LEN & dma_len;

				ktxds->dma_len_type = rte_cpu_to_le_16(dlen_type);
				ktxds->dma_addr_hi = rte_cpu_to_le_16(dma_addr >> 32);
				ktxds->dma_addr_lo = rte_cpu_to_le_32(dma_addr & 0xffffffff);
				ktxds++;

				dma_len -= dlen_type;
				dma_addr += dlen_type + 1;
			}

			if (pkt->next == NULL)
				break;

			pkt = pkt->next;
			dma_len = pkt->data_len;
			dma_addr = rte_mbuf_data_iova(pkt);
			PMD_TX_LOG(DEBUG, "Working with mbuf at dma address:"
				"%" PRIx64 "", dma_addr);

			lmbuf = &txq->txbufs[buf_idx++].mbuf;
		}

		(ktxds - 1)->dma_len_type = rte_cpu_to_le_16(dlen_type | NFDK_DESC_TX_EOP);

		ktxds->raw = rte_cpu_to_le_64(nfp_net_nfdk_tx_cksum(txq, temp_pkt, metadata));
		ktxds++;

		if ((hw->cap & NFP_NET_CFG_CTRL_LSO_ANY) &&
				(temp_pkt->ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
			ktxds->raw = rte_cpu_to_le_64(nfp_net_nfdk_tx_tso(txq, temp_pkt));
			ktxds++;
		}

		used_descs = ktxds - txq->ktxds - txq->wr_p;
		if (round_down(txq->wr_p, NFDK_TX_DESC_BLOCK_CNT) !=
			round_down(txq->wr_p + used_descs - 1, NFDK_TX_DESC_BLOCK_CNT)) {
			PMD_INIT_LOG(INFO, "Used descs cross block boundary");
			goto xmit_end;
		}

		txq->wr_p = D_IDX(txq, txq->wr_p + used_descs);
		if (txq->wr_p % NFDK_TX_DESC_BLOCK_CNT)
			txq->data_pending += temp_pkt->pkt_len;
		else
			txq->data_pending = 0;

		issued_descs += used_descs;
		npkts++;
		free_descs = (uint16_t)nfp_net_nfdk_free_tx_desc(txq);
	}

xmit_end:
	/* Increment write pointers. Force memory write before we let HW know */
	rte_wmb();
	nfp_qcp_ptr_add(txq->qcp_q, NFP_QCP_WRITE_PTR, issued_descs);

	return npkts;
}

int
nfp_net_nfdk_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf)
{
	int ret;
	uint16_t min_tx_desc;
	uint16_t max_tx_desc;
	const struct rte_memzone *tz;
	struct nfp_net_txq *txq;
	uint16_t tx_free_thresh;
	struct nfp_net_hw *hw;
	uint32_t tx_desc_sz;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	ret = nfp_net_tx_desc_limits(hw, &min_tx_desc, &max_tx_desc);
	if (ret != 0)
		return ret;

	/* Validating number of descriptors */
	tx_desc_sz = nb_desc * sizeof(struct nfp_net_nfdk_tx_desc);
	if ((NFDK_TX_DESC_PER_SIMPLE_PKT * tx_desc_sz) % NFP_ALIGN_RING_DESC != 0 ||
	    (NFDK_TX_DESC_PER_SIMPLE_PKT * nb_desc) % NFDK_TX_DESC_BLOCK_CNT != 0 ||
	     nb_desc > max_tx_desc || nb_desc < min_tx_desc) {
		PMD_DRV_LOG(ERR, "Wrong nb_desc value");
		return -EINVAL;
	}

	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
				tx_conf->tx_free_thresh :
				DEFAULT_TX_FREE_THRESH);

	if (tx_free_thresh > (nb_desc)) {
		PMD_DRV_LOG(ERR,
			"tx_free_thresh must be less than the number of TX "
			"descriptors. (tx_free_thresh=%u port=%d "
			"queue=%d)", (unsigned int)tx_free_thresh,
			dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/*
	 * Free memory prior to re-allocation if needed. This is the case after
	 * calling nfp_net_stop
	 */
	if (dev->data->tx_queues[queue_idx]) {
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

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
				sizeof(struct nfp_net_nfdk_tx_desc) *
				NFDK_TX_DESC_PER_SIMPLE_PKT *
				max_tx_desc, NFP_MEMZONE_ALIGN,
				socket_id);
	if (tz == NULL) {
		PMD_DRV_LOG(ERR, "Error allocating tx dma");
		nfp_net_tx_queue_release(dev, queue_idx);
		return -ENOMEM;
	}

	txq->tx_count = nb_desc * NFDK_TX_DESC_PER_SIMPLE_PKT;
	txq->tx_free_thresh = tx_free_thresh;
	txq->tx_pthresh = tx_conf->tx_thresh.pthresh;
	txq->tx_hthresh = tx_conf->tx_thresh.hthresh;
	txq->tx_wthresh = tx_conf->tx_thresh.wthresh;

	/* queue mapping based on firmware configuration */
	txq->qidx = queue_idx;
	txq->tx_qcidx = queue_idx * hw->stride_tx;
	txq->qcp_q = hw->tx_bar + NFP_QCP_QUEUE_OFF(txq->tx_qcidx);

	txq->port_id = dev->data->port_id;

	/* Saving physical and virtual addresses for the TX ring */
	txq->dma = (uint64_t)tz->iova;
	txq->ktxds = (struct nfp_net_nfdk_tx_desc *)tz->addr;

	/* mbuf pointers array for referencing mbufs linked to TX descriptors */
	txq->txbufs = rte_zmalloc_socket("txq->txbufs",
				sizeof(*txq->txbufs) * txq->tx_count,
				RTE_CACHE_LINE_SIZE, socket_id);

	if (txq->txbufs == NULL) {
		nfp_net_tx_queue_release(dev, queue_idx);
		return -ENOMEM;
	}
	PMD_TX_LOG(DEBUG, "txbufs=%p hw_ring=%p dma_addr=0x%" PRIx64,
		txq->txbufs, txq->ktxds, (unsigned long)txq->dma);

	nfp_net_reset_tx_queue(txq);

	dev->data->tx_queues[queue_idx] = txq;
	txq->hw = hw;
	/*
	 * Telling the HW about the physical address of the TX ring and number
	 * of descriptors in log2 format
	 */
	nn_cfg_writeq(hw, NFP_NET_CFG_TXR_ADDR(queue_idx), txq->dma);
	nn_cfg_writeb(hw, NFP_NET_CFG_TXR_SZ(queue_idx), rte_log2_u32(txq->tx_count));

	return 0;
}
