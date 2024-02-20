/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_nfdk.h"

#include <bus_pci_driver.h>
#include <nfp_platform.h>
#include <rte_malloc.h>

#include "../flower/nfp_flower.h"
#include "../nfp_logs.h"

#define NFDK_TX_DESC_GATHER_MAX         17

/* Set TX CSUM offload flags in TX descriptor of nfdk */
static uint64_t
nfp_net_nfdk_tx_cksum(struct nfp_net_txq *txq,
		struct rte_mbuf *mb,
		uint64_t flags)
{
	uint64_t ol_flags;
	struct nfp_net_hw *hw = txq->hw;

	if ((hw->super.cap & NFP_NET_CFG_CTRL_TXCSUM) == 0)
		return flags;

	ol_flags = mb->ol_flags;

	/* Set TCP csum offload if TSO enabled. */
	if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) != 0)
		flags |= NFDK_DESC_TX_L4_CSUM;

	if ((ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) != 0)
		flags |= NFDK_DESC_TX_ENCAP;

	/* IPv6 does not need checksum */
	if ((ol_flags & RTE_MBUF_F_TX_IP_CKSUM) != 0)
		flags |= NFDK_DESC_TX_L3_CSUM;

	if ((ol_flags & RTE_MBUF_F_TX_L4_MASK) != 0)
		flags |= NFDK_DESC_TX_L4_CSUM;

	return flags;
}

/* Set TX descriptor for TSO of nfdk */
static uint64_t
nfp_net_nfdk_tx_tso(struct nfp_net_txq *txq,
		struct rte_mbuf *mb)
{
	uint8_t outer_len;
	uint64_t ol_flags;
	struct nfp_net_nfdk_tx_desc txd;
	struct nfp_net_hw *hw = txq->hw;

	txd.raw = 0;

	if ((hw->super.cap & NFP_NET_CFG_CTRL_LSO_ANY) == 0)
		return txd.raw;

	ol_flags = mb->ol_flags;
	if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) == 0)
		return txd.raw;

	txd.l3_offset = mb->l2_len;
	txd.l4_offset = mb->l2_len + mb->l3_len;
	txd.lso_meta_res = 0;
	txd.mss = rte_cpu_to_le_16(mb->tso_segsz);
	txd.lso_hdrlen = mb->l2_len + mb->l3_len + mb->l4_len;
	txd.lso_totsegs = (mb->pkt_len + mb->tso_segsz) / mb->tso_segsz;

	if ((ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) != 0) {
		outer_len = mb->outer_l2_len + mb->outer_l3_len;
		txd.l3_offset += outer_len;
		txd.l4_offset += outer_len;
		txd.lso_hdrlen += outer_len;
	}

	return txd.raw;
}

uint32_t
nfp_flower_nfdk_pkt_add_metadata(struct rte_mbuf *mbuf,
		uint32_t port_id)
{
	uint32_t header;
	char *meta_offset;

	meta_offset = rte_pktmbuf_prepend(mbuf, FLOWER_PKT_DATA_OFFSET);
	header = NFP_NET_META_PORTID << NFP_NET_META_NFDK_LENGTH | FLOWER_PKT_DATA_OFFSET;
	*(rte_be32_t *)meta_offset = rte_cpu_to_be_32(header);
	meta_offset += NFP_NET_META_HEADER_SIZE;
	*(rte_be32_t *)meta_offset = rte_cpu_to_be_32(port_id);

	return FLOWER_PKT_DATA_OFFSET;
}

static inline uint16_t
nfp_net_nfdk_headlen_to_segs(uint16_t headlen)
{
	/* First descriptor fits less data, so adjust for that */
	return DIV_ROUND_UP(headlen + NFDK_TX_MAX_DATA_PER_DESC - NFDK_TX_MAX_DATA_PER_HEAD,
			NFDK_TX_MAX_DATA_PER_DESC);
}

static inline void
nfp_net_nfdk_tx_close_block(struct nfp_net_txq *txq,
		uint32_t nop_slots)
{
	uint32_t i;
	uint32_t wr_p;

	wr_p = txq->wr_p;
	memset(&txq->ktxds[wr_p], 0, nop_slots * sizeof(struct nfp_net_nfdk_tx_desc));

	for (i = wr_p; i < nop_slots + wr_p; i++) {
		if (txq->txbufs[i].mbuf != NULL) {
			rte_pktmbuf_free_seg(txq->txbufs[i].mbuf);
			txq->txbufs[i].mbuf = NULL;
		}
	}

	txq->data_pending = 0;
	txq->wr_p = D_IDX(txq, wr_p + nop_slots);
}

int
nfp_net_nfdk_tx_maybe_close_block(struct nfp_net_txq *txq,
		struct rte_mbuf *pkt)
{
	uint16_t n_descs;
	uint32_t nop_slots;
	struct rte_mbuf *pkt_temp;

	/* Count address descriptor */
	pkt_temp = pkt;
	n_descs = nfp_net_nfdk_headlen_to_segs(pkt_temp->data_len);
	while (pkt_temp->next != NULL) {
		pkt_temp = pkt_temp->next;
		n_descs += DIV_ROUND_UP(pkt_temp->data_len, NFDK_TX_MAX_DATA_PER_DESC);
	}

	if (unlikely(n_descs > NFDK_TX_DESC_GATHER_MAX))
		return -EINVAL;

	/* Count TSO descriptor */
	if ((txq->hw->super.cap & NFP_NET_CFG_CTRL_LSO_ANY) != 0 &&
			(pkt->ol_flags & RTE_MBUF_F_TX_TCP_SEG) != 0)
		n_descs++;

	/* Don't count metadata descriptor, for the round down to work out */
	if (RTE_ALIGN_FLOOR(txq->wr_p, NFDK_TX_DESC_BLOCK_CNT) !=
			RTE_ALIGN_FLOOR(txq->wr_p + n_descs, NFDK_TX_DESC_BLOCK_CNT))
		goto close_block;

	if (txq->data_pending + pkt->pkt_len > NFDK_TX_MAX_DATA_PER_BLOCK)
		goto close_block;

	return 0;

close_block:
	nop_slots = D_BLOCK_CPL(txq->wr_p);
	nfp_net_nfdk_tx_close_block(txq, nop_slots);

	return nop_slots;
}

static int
nfp_net_nfdk_set_meta_data(struct rte_mbuf *pkt,
		struct nfp_net_txq *txq,
		uint64_t *metadata)
{
	char *meta;
	uint8_t layer = 0;
	uint32_t meta_type;
	uint32_t cap_extend;
	struct nfp_net_hw *hw;
	uint32_t header_offset;
	uint8_t vlan_layer = 0;
	uint8_t ipsec_layer = 0;
	struct nfp_net_meta_raw meta_data;

	memset(&meta_data, 0, sizeof(meta_data));
	hw = txq->hw;
	cap_extend = hw->super.cap_ext;

	if ((pkt->ol_flags & RTE_MBUF_F_TX_VLAN) != 0 &&
			(hw->super.ctrl & NFP_NET_CFG_CTRL_TXVLAN_V2) != 0) {
		if (meta_data.length == 0)
			meta_data.length = NFP_NET_META_HEADER_SIZE;
		meta_data.length += NFP_NET_META_FIELD_SIZE;
		meta_data.header |= NFP_NET_META_VLAN;
	}

	if ((pkt->ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) != 0 &&
			(cap_extend & NFP_NET_CFG_CTRL_IPSEC) != 0) {
		uint32_t ipsec_type = NFP_NET_META_IPSEC |
				NFP_NET_META_IPSEC << NFP_NET_META_FIELD_SIZE |
				NFP_NET_META_IPSEC << (2 * NFP_NET_META_FIELD_SIZE);
		if (meta_data.length == 0)
			meta_data.length = NFP_NET_META_FIELD_SIZE;
		uint8_t ipsec_offset = meta_data.length - NFP_NET_META_FIELD_SIZE;
		meta_data.header |= (ipsec_type << ipsec_offset);
		meta_data.length += 3 * NFP_NET_META_FIELD_SIZE;
	}

	if (meta_data.length == 0) {
		*metadata = 0;
		return 0;
	}

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
				return -EINVAL;
			}

			nfp_net_set_meta_vlan(&meta_data, pkt, layer);
			vlan_layer++;
			break;
		case NFP_NET_META_IPSEC:
			if (ipsec_layer > 2) {
				PMD_DRV_LOG(ERR, "At most 3 layers of ipsec is supported for now.");
				return -EINVAL;
			}

			nfp_net_set_meta_ipsec(&meta_data, txq, pkt, layer, ipsec_layer);
			ipsec_layer++;
			break;
		default:
			PMD_DRV_LOG(ERR, "The metadata type not supported");
			return -ENOTSUP;
		}

		memcpy(meta, &meta_data.data[layer], sizeof(meta_data.data[layer]));
	}

	*metadata = NFDK_DESC_TX_CHAIN_META;

	return 0;
}

uint16_t
nfp_net_nfdk_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	return nfp_net_nfdk_xmit_pkts_common(tx_queue, tx_pkts, nb_pkts, false);
}

uint16_t
nfp_net_nfdk_xmit_pkts_common(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts,
		bool repr_flag)
{
	uint32_t buf_idx;
	uint64_t dma_addr;
	uint32_t free_descs;
	uint32_t npkts = 0;
	struct rte_mbuf *pkt;
	struct nfp_net_hw *hw;
	struct rte_mbuf **lmbuf;
	struct nfp_net_txq *txq;
	uint32_t issued_descs = 0;
	struct rte_mbuf *temp_pkt;
	struct nfp_net_nfdk_tx_desc *ktxds;

	txq = tx_queue;
	hw = txq->hw;

	PMD_TX_LOG(DEBUG, "working for queue %hu at pos %d and %hu packets",
			txq->qidx, txq->wr_p, nb_pkts);

	if (nfp_net_nfdk_free_tx_desc(txq) < NFDK_TX_DESC_PER_SIMPLE_PKT * nb_pkts ||
			nfp_net_nfdk_txq_full(txq))
		nfp_net_tx_free_bufs(txq);

	free_descs = nfp_net_nfdk_free_tx_desc(txq);
	if (unlikely(free_descs == 0))
		return 0;

	PMD_TX_LOG(DEBUG, "queue: %hu. Sending %hu packets", txq->qidx, nb_pkts);

	/* Sending packets */
	while (npkts < nb_pkts && free_descs > 0) {
		int ret;
		int nop_descs;
		uint32_t type;
		uint32_t dma_len;
		uint32_t tmp_dlen;
		uint32_t dlen_type;
		uint32_t used_descs;
		uint64_t metadata = 0;

		pkt = *(tx_pkts + npkts);
		if (pkt == NULL)
			goto xmit_end;

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

		if (repr_flag) {
			metadata = NFDK_DESC_TX_CHAIN_META;
		} else {
			ret = nfp_net_nfdk_set_meta_data(pkt, txq, &metadata);
			if (unlikely(ret != 0))
				goto xmit_end;
		}

		if (unlikely(pkt->nb_segs > 1 &&
				(hw->super.cap & NFP_NET_CFG_CTRL_GATHER) == 0)) {
			PMD_TX_LOG(ERR, "Multisegment packet not supported");
			goto xmit_end;
		}

		/*
		 * Checksum and VLAN flags just in the first descriptor for a
		 * multisegment packet, but TSO info needs to be in all of them.
		 */
		dma_len = pkt->data_len;
		if ((hw->super.cap & NFP_NET_CFG_CTRL_LSO_ANY) != 0 &&
				(pkt->ol_flags & RTE_MBUF_F_TX_TCP_SEG) != 0) {
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
		if (dma_len > NFDK_DESC_TX_DMA_LEN_HEAD)
			tmp_dlen = NFDK_DESC_TX_DMA_LEN_HEAD;
		else
			tmp_dlen = dma_len;
		dlen_type = tmp_dlen | (NFDK_DESC_TX_TYPE_HEAD & (type << 12));
		ktxds->dma_len_type = rte_cpu_to_le_16(dlen_type);
		dma_addr = rte_mbuf_data_iova(pkt);
		ktxds->dma_addr_hi = rte_cpu_to_le_16(dma_addr >> 32);
		ktxds->dma_addr_lo = rte_cpu_to_le_32(dma_addr & 0xffffffff);
		ktxds++;

		/*
		 * Preserve the original dlen_type, this way below the EOP logic
		 * can use dlen_type.
		 */
		dma_len -= tmp_dlen;
		dma_addr += tmp_dlen + 1;

		/*
		 * The rest of the data (if any) will be in larger DMA descriptors
		 * and is handled with the dma_len loop.
		 */
		while (pkt != NULL) {
			if (*lmbuf != NULL)
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

			lmbuf = &txq->txbufs[buf_idx++].mbuf;
		}

		(ktxds - 1)->dma_len_type = rte_cpu_to_le_16(dlen_type | NFDK_DESC_TX_EOP);

		ktxds->raw = rte_cpu_to_le_64(nfp_net_nfdk_tx_cksum(txq, temp_pkt, metadata));
		ktxds++;

		if ((hw->super.cap & NFP_NET_CFG_CTRL_LSO_ANY) != 0 &&
				(temp_pkt->ol_flags & RTE_MBUF_F_TX_TCP_SEG) != 0) {
			ktxds->raw = rte_cpu_to_le_64(nfp_net_nfdk_tx_tso(txq, temp_pkt));
			ktxds++;
		}

		used_descs = ktxds - txq->ktxds - txq->wr_p;
		if (RTE_ALIGN_FLOOR(txq->wr_p, NFDK_TX_DESC_BLOCK_CNT) !=
				RTE_ALIGN_FLOOR(txq->wr_p + used_descs - 1,
						NFDK_TX_DESC_BLOCK_CNT)) {
			PMD_TX_LOG(INFO, "Used descs cross block boundary");
			goto xmit_end;
		}

		txq->wr_p = D_IDX(txq, txq->wr_p + used_descs);
		if (txq->wr_p % NFDK_TX_DESC_BLOCK_CNT)
			txq->data_pending += temp_pkt->pkt_len;
		else
			txq->data_pending = 0;

		issued_descs += used_descs;
		npkts++;
		free_descs = nfp_net_nfdk_free_tx_desc(txq);
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
	size_t size;
	uint32_t tx_desc_sz;
	uint16_t min_tx_desc;
	uint16_t max_tx_desc;
	struct nfp_net_hw *hw;
	uint16_t tx_free_thresh;
	struct nfp_net_txq *txq;
	const struct rte_memzone *tz;

	hw = nfp_net_get_hw(dev);

	nfp_net_tx_desc_limits(hw, &min_tx_desc, &max_tx_desc);

	/* Validating number of descriptors */
	tx_desc_sz = nb_desc * sizeof(struct nfp_net_nfdk_tx_desc);
	if ((NFDK_TX_DESC_PER_SIMPLE_PKT * tx_desc_sz) % NFP_ALIGN_RING_DESC != 0 ||
			(NFDK_TX_DESC_PER_SIMPLE_PKT * nb_desc) % NFDK_TX_DESC_BLOCK_CNT != 0 ||
			nb_desc > max_tx_desc || nb_desc < min_tx_desc) {
		PMD_DRV_LOG(ERR, "Wrong nb_desc value");
		return -EINVAL;
	}

	tx_free_thresh = tx_conf->tx_free_thresh;
	if (tx_free_thresh == 0)
		tx_free_thresh = DEFAULT_TX_FREE_THRESH;
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

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	size = sizeof(struct nfp_net_nfdk_tx_desc) * max_tx_desc *
			NFDK_TX_DESC_PER_SIMPLE_PKT;
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx, size,
			NFP_MEMZONE_ALIGN, socket_id);
	if (tz == NULL) {
		PMD_DRV_LOG(ERR, "Error allocating tx dma");
		nfp_net_tx_queue_release(dev, queue_idx);
		return -ENOMEM;
	}

	txq->tx_count = nb_desc * NFDK_TX_DESC_PER_SIMPLE_PKT;
	txq->tx_free_thresh = tx_free_thresh;

	/* Queue mapping based on firmware configuration */
	txq->qidx = queue_idx;
	txq->tx_qcidx = queue_idx * hw->stride_tx;
	txq->qcp_q = hw->tx_bar + NFP_QCP_QUEUE_OFF(txq->tx_qcidx);
	txq->port_id = dev->data->port_id;

	/* Saving physical and virtual addresses for the TX ring */
	txq->dma = tz->iova;
	txq->ktxds = tz->addr;

	/* Mbuf pointers array for referencing mbufs linked to TX descriptors */
	txq->txbufs = rte_zmalloc_socket("txq->txbufs",
			sizeof(*txq->txbufs) * txq->tx_count,
			RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->txbufs == NULL) {
		nfp_net_tx_queue_release(dev, queue_idx);
		return -ENOMEM;
	}

	nfp_net_reset_tx_queue(txq);

	dev->data->tx_queues[queue_idx] = txq;
	txq->hw = hw;

	/*
	 * Telling the HW about the physical address of the TX ring and number
	 * of descriptors in log2 format.
	 */
	nn_cfg_writeq(&hw->super, NFP_NET_CFG_TXR_ADDR(queue_idx), txq->dma);
	nn_cfg_writeb(&hw->super, NFP_NET_CFG_TXR_SZ(queue_idx), rte_log2_u32(txq->tx_count));

	return 0;
}
