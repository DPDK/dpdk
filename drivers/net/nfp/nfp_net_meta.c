/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_net_meta.h"

#include "nfp_net_common.h"
#include "nfp_ipsec.h"
#include "nfp_logs.h"

enum nfp_net_meta_ipsec_layer {
	NFP_NET_META_IPSEC_SAIDX,       /**< Order of SA index in metadata */
	NFP_NET_META_IPSEC_SEQLOW,      /**< Order of Sequence Number (low 32bits) in metadata */
	NFP_NET_META_IPSEC_SEQHI,       /**< Order of Sequence Number (high 32bits) in metadata */
};

/* Parse the chained metadata from packet */
static bool
nfp_net_meta_parse_chained(uint8_t *meta_base,
		rte_be32_t meta_header,
		struct nfp_net_meta_parsed *meta)
{
	uint32_t meta_info;
	uint32_t vlan_info;
	uint8_t *meta_offset;

	meta_info = rte_be_to_cpu_32(meta_header);
	meta_offset = meta_base + 4;

	for (; meta_info != 0; meta_info >>= NFP_NET_META_FIELD_SIZE, meta_offset += 4) {
		switch (meta_info & NFP_NET_META_FIELD_MASK) {
		case NFP_NET_META_PORTID:
			meta->flags |= (1 << NFP_NET_META_PORTID);
			meta->port_id = rte_be_to_cpu_32(*(rte_be32_t *)meta_offset);
			break;
		case NFP_NET_META_HASH:
			meta->flags |= (1 << NFP_NET_META_HASH);
			/* Next field type is about the hash type */
			meta_info >>= NFP_NET_META_FIELD_SIZE;
			/* Hash value is in the data field */
			meta->hash = rte_be_to_cpu_32(*(rte_be32_t *)meta_offset);
			meta->hash_type = meta_info & NFP_NET_META_FIELD_MASK;
			break;
		case NFP_NET_META_VLAN:
			meta->flags |= (1 << NFP_NET_META_VLAN);
			vlan_info = rte_be_to_cpu_32(*(rte_be32_t *)meta_offset);
			meta->vlan[meta->vlan_layer].offload =
					vlan_info >> NFP_NET_META_VLAN_OFFLOAD;
			meta->vlan[meta->vlan_layer].tci =
					vlan_info & NFP_NET_META_VLAN_MASK;
			meta->vlan[meta->vlan_layer].tpid = NFP_NET_META_TPID(vlan_info);
			meta->vlan_layer++;
			break;
		case NFP_NET_META_IPSEC:
			meta->flags |= (1 << NFP_NET_META_IPSEC);
			meta->sa_idx = rte_be_to_cpu_32(*(rte_be32_t *)meta_offset);
			break;
		case NFP_NET_META_MARK:
			meta->flags |= (1 << NFP_NET_META_MARK);
			meta->mark_id = rte_be_to_cpu_32(*(rte_be32_t *)meta_offset);
			break;
		default:
			/* Unsupported metadata can be a performance issue */
			return false;
		}
	}

	return true;
}

/*
 * Parse the single metadata
 *
 * The RSS hash and hash-type are prepended to the packet data.
 * Get it from metadata area.
 */
static inline void
nfp_net_meta_parse_single(uint8_t *meta_base,
		rte_be32_t meta_header,
		struct nfp_net_meta_parsed *meta)
{
	meta->flags = 0;
	meta->flags |= (1 << NFP_NET_META_HASH);
	meta->hash_type = rte_be_to_cpu_32(meta_header);
	meta->hash = rte_be_to_cpu_32(*(rte_be32_t *)(meta_base + 4));
}

/* Set mbuf hash data based on the metadata info */
static void
nfp_net_meta_parse_hash(const struct nfp_net_meta_parsed *meta,
		struct nfp_net_rxq *rxq,
		struct rte_mbuf *mbuf)
{
	struct nfp_net_hw *hw = rxq->hw;

	if ((hw->super.ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0)
		return;

	if (((meta->flags >> NFP_NET_META_HASH) & 0x1) == 0)
		return;

	mbuf->hash.rss = meta->hash;
	mbuf->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
}

/* Set mbuf vlan_strip data based on metadata info */
static void
nfp_net_meta_parse_vlan(const struct nfp_net_meta_parsed *meta,
		struct nfp_net_rx_desc *rxd,
		struct nfp_net_rxq *rxq,
		struct rte_mbuf *mb)
{
	uint16_t flags;
	uint32_t ctrl = rxq->hw->super.ctrl;

	/* Skip if hardware don't support setting vlan. */
	if ((ctrl & (NFP_NET_CFG_CTRL_RXVLAN | NFP_NET_CFG_CTRL_RXVLAN_V2)) == 0)
		return;

	if (((meta->flags >> NFP_NET_META_VLAN) & 0x1) == 0)
		return;

	/*
	 * The firmware support two ways to send the VLAN info (with priority) :
	 * 1. Using the metadata when NFP_NET_CFG_CTRL_RXVLAN_V2 is set,
	 * 2. Using the descriptor when NFP_NET_CFG_CTRL_RXVLAN is set.
	 */
	if ((ctrl & NFP_NET_CFG_CTRL_RXVLAN_V2) != 0) {
		if (meta->vlan_layer > 0 && meta->vlan[0].offload != 0) {
			mb->vlan_tci = rte_cpu_to_le_32(meta->vlan[0].tci);
			mb->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		}
	} else if ((ctrl & NFP_NET_CFG_CTRL_RXVLAN) != 0) {
		flags = rte_le_to_cpu_16(rxd->rxd.flags);
		if ((flags & PCIE_DESC_RX_VLAN) != 0) {
			mb->vlan_tci = rte_cpu_to_le_32(rxd->rxd.offload_info);
			mb->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		}
	}
}

/*
 * Set mbuf qinq_strip data based on metadata info
 *
 * The out VLAN tci are prepended to the packet data.
 * Extract and decode it and set the mbuf fields.
 *
 * If both RTE_MBUF_F_RX_VLAN and NFP_NET_CFG_CTRL_RXQINQ are set, the 2 VLANs
 *   have been stripped by the hardware and their TCIs are saved in
 *   mbuf->vlan_tci (inner) and mbuf->vlan_tci_outer (outer).
 * If NFP_NET_CFG_CTRL_RXQINQ is set and RTE_MBUF_F_RX_VLAN is unset, only the
 *   outer VLAN is removed from packet data, but both tci are saved in
 *   mbuf->vlan_tci (inner) and mbuf->vlan_tci_outer (outer).
 *
 * qinq set & vlan set : meta->vlan_layer>=2, meta->vlan[0].offload=1, meta->vlan[1].offload=1
 * qinq set & vlan not set: meta->vlan_layer>=2, meta->vlan[1].offload=1,meta->vlan[0].offload=0
 * qinq not set & vlan set: meta->vlan_layer=1, meta->vlan[0].offload=1
 * qinq not set & vlan not set: meta->vlan_layer=0
 */
static void
nfp_net_meta_parse_qinq(const struct nfp_net_meta_parsed *meta,
		struct nfp_net_rxq *rxq,
		struct rte_mbuf *mb)
{
	struct nfp_hw *hw = &rxq->hw->super;

	if ((hw->ctrl & NFP_NET_CFG_CTRL_RXQINQ) == 0 ||
			(hw->cap & NFP_NET_CFG_CTRL_RXQINQ) == 0)
		return;

	if (((meta->flags >> NFP_NET_META_VLAN) & 0x1) == 0)
		return;

	if (meta->vlan_layer < NFP_NET_META_MAX_VLANS)
		return;

	if (meta->vlan[0].offload == 0)
		mb->vlan_tci = rte_cpu_to_le_16(meta->vlan[0].tci);

	mb->vlan_tci_outer = rte_cpu_to_le_16(meta->vlan[1].tci);
	PMD_RX_LOG(DEBUG, "Received outer vlan TCI is %u inner vlan TCI is %u.",
			mb->vlan_tci_outer, mb->vlan_tci);
	mb->ol_flags |= RTE_MBUF_F_RX_QINQ | RTE_MBUF_F_RX_QINQ_STRIPPED;
}

/*
 * Set mbuf IPsec Offload features based on metadata info.
 *
 * The IPsec Offload features is prepended to the mbuf ol_flags.
 * Extract and decode metadata info and set the mbuf ol_flags.
 */
static void
nfp_net_meta_parse_ipsec(struct nfp_net_meta_parsed *meta,
		struct nfp_net_rxq *rxq,
		struct rte_mbuf *mbuf)
{
	int offset;
	uint32_t sa_idx;
	struct nfp_net_hw *hw;
	struct nfp_tx_ipsec_desc_msg *desc_md;

	if (((meta->flags >> NFP_NET_META_IPSEC) & 0x1) == 0)
		return;

	hw = rxq->hw;
	sa_idx = meta->sa_idx;

	if (sa_idx >= NFP_NET_IPSEC_MAX_SA_CNT) {
		mbuf->ol_flags |= RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED;
	} else {
		mbuf->ol_flags |= RTE_MBUF_F_RX_SEC_OFFLOAD;
		offset = hw->ipsec_data->pkt_dynfield_offset;
		desc_md = RTE_MBUF_DYNFIELD(mbuf, offset, struct nfp_tx_ipsec_desc_msg *);
		desc_md->sa_idx = sa_idx;
		desc_md->enc = 0;
	}
}

static void
nfp_net_meta_parse_mark(const struct nfp_net_meta_parsed *meta,
		struct rte_mbuf *mbuf)
{
	if (((meta->flags >> NFP_NET_META_MARK) & 0x1) == 0)
		return;

	mbuf->hash.fdir.hi = meta->mark_id;
	mbuf->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
}

/* Parse the metadata from packet */
void
nfp_net_meta_parse(struct nfp_net_rx_desc *rxds,
		struct nfp_net_rxq *rxq,
		struct nfp_net_hw *hw,
		struct rte_mbuf *mb,
		struct nfp_net_meta_parsed *meta)
{
	uint16_t flags;
	uint8_t *meta_base;
	rte_be32_t meta_header;

	meta->flags = 0;
	flags = rte_le_to_cpu_16(rxds->rxd.flags);

	if (unlikely(NFP_DESC_META_LEN(rxds) == 0))
		return;

	meta_base = rte_pktmbuf_mtod_offset(mb, uint8_t *, -NFP_DESC_META_LEN(rxds));
	meta_header = *(rte_be32_t *)meta_base;

	switch (hw->meta_format) {
	case NFP_NET_METAFORMAT_CHAINED:
		if (nfp_net_meta_parse_chained(meta_base, meta_header, meta)) {
			nfp_net_meta_parse_hash(meta, rxq, mb);
			nfp_net_meta_parse_vlan(meta, rxds, rxq, mb);
			nfp_net_meta_parse_qinq(meta, rxq, mb);
			nfp_net_meta_parse_ipsec(meta, rxq, mb);
			nfp_net_meta_parse_mark(meta, mb);
		} else {
			PMD_RX_LOG(DEBUG, "RX chained metadata format is wrong!");
		}
		break;
	case NFP_NET_METAFORMAT_SINGLE:
		if ((flags & PCIE_DESC_RX_RSS) != 0) {
			nfp_net_meta_parse_single(meta_base, meta_header, meta);
			nfp_net_meta_parse_hash(meta, rxq, mb);
		}
		break;
	default:
		PMD_RX_LOG(DEBUG, "RX metadata do not exist.");
	}
}

void
nfp_net_meta_init_format(struct nfp_net_hw *hw,
		struct nfp_pf_dev *pf_dev)
{
	/*
	 * ABI 4.x and ctrl vNIC always use chained metadata, in other cases we allow use of
	 * single metadata if only RSS(v1) is supported by hw capability, and RSS(v2)
	 * also indicate that we are using chained metadata.
	 */
	if (pf_dev->ver.major == 4) {
		hw->meta_format = NFP_NET_METAFORMAT_CHAINED;
	} else if ((hw->super.cap & NFP_NET_CFG_CTRL_CHAIN_META) != 0) {
		hw->meta_format = NFP_NET_METAFORMAT_CHAINED;
		/*
		 * RSS is incompatible with chained metadata. hw->super.cap just represents
		 * firmware's ability rather than the firmware's configuration. We decide
		 * to reduce the confusion to allow us can use hw->super.cap to identify RSS later.
		 */
		hw->super.cap &= ~NFP_NET_CFG_CTRL_RSS;
	} else {
		hw->meta_format = NFP_NET_METAFORMAT_SINGLE;
	}
}

void
nfp_net_meta_set_vlan(struct nfp_net_meta_raw *meta_data,
		struct rte_mbuf *pkt,
		uint8_t layer)
{
	uint16_t tpid;
	uint16_t vlan_tci;

	tpid = RTE_ETHER_TYPE_VLAN;
	vlan_tci = pkt->vlan_tci;

	meta_data->data[layer] = tpid << 16 | vlan_tci;
}

void
nfp_net_meta_set_ipsec(struct nfp_net_meta_raw *meta_data,
		struct nfp_net_txq *txq,
		struct rte_mbuf *pkt,
		uint8_t layer,
		uint8_t ipsec_layer)
{
	int offset;
	struct nfp_net_hw *hw;
	struct nfp_tx_ipsec_desc_msg *desc_md;

	hw = txq->hw;
	offset = hw->ipsec_data->pkt_dynfield_offset;
	desc_md = RTE_MBUF_DYNFIELD(pkt, offset, struct nfp_tx_ipsec_desc_msg *);

	switch (ipsec_layer) {
	case NFP_NET_META_IPSEC_SAIDX:
		meta_data->data[layer] = desc_md->sa_idx;
		break;
	case NFP_NET_META_IPSEC_SEQLOW:
		meta_data->data[layer] = desc_md->esn.low;
		break;
	case NFP_NET_META_IPSEC_SEQHI:
		meta_data->data[layer] = desc_md->esn.hi;
		break;
	default:
		break;
	}
}
