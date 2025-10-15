/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_net.h>
#include <rte_vect.h>

#include "ice_rxtx.h"
#include "ice_rxtx_vec_common.h"

#define ICE_TX_CKSUM_OFFLOAD_MASK (RTE_MBUF_F_TX_IP_CKSUM |		 \
		RTE_MBUF_F_TX_L4_MASK |		 \
		RTE_MBUF_F_TX_TCP_SEG |		 \
		RTE_MBUF_F_TX_UDP_SEG |		 \
		RTE_MBUF_F_TX_OUTER_IP_CKSUM)

/**
 * The mbuf dynamic field pointer for protocol extraction metadata.
 */
#define ICE_DYNF_PROTO_XTR_METADATA(m, n) \
	RTE_MBUF_DYNFIELD((m), (n), uint32_t *)

static int
ice_monitor_callback(const uint64_t value,
		const uint64_t arg[RTE_POWER_MONITOR_OPAQUE_SZ] __rte_unused)
{
	const uint64_t m = rte_cpu_to_le_16(1 << ICE_RX_FLEX_DESC_STATUS0_DD_S);
	/*
	 * we expect the DD bit to be set to 1 if this descriptor was already
	 * written to.
	 */
	return (value & m) == m ? -1 : 0;
}

int
ice_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc)
{
	volatile union ci_rx_flex_desc *rxdp;
	struct ci_rx_queue *rxq = rx_queue;
	uint16_t desc;

	desc = rxq->rx_tail;
	rxdp = &rxq->rx_flex_ring[desc];
	/* watch for changes in status bit */
	pmc->addr = &rxdp->wb.status_error0;

	/* comparison callback */
	pmc->fn = ice_monitor_callback;

	/* register is 16-bit */
	pmc->size = sizeof(uint16_t);

	return 0;
}


static inline uint8_t
ice_proto_xtr_type_to_rxdid(uint8_t xtr_type)
{
	static uint8_t rxdid_map[] = {
		[PROTO_XTR_NONE]      = ICE_RXDID_COMMS_OVS,
		[PROTO_XTR_VLAN]      = ICE_RXDID_COMMS_AUX_VLAN,
		[PROTO_XTR_IPV4]      = ICE_RXDID_COMMS_AUX_IPV4,
		[PROTO_XTR_IPV6]      = ICE_RXDID_COMMS_AUX_IPV6,
		[PROTO_XTR_IPV6_FLOW] = ICE_RXDID_COMMS_AUX_IPV6_FLOW,
		[PROTO_XTR_TCP]       = ICE_RXDID_COMMS_AUX_TCP,
		[PROTO_XTR_IP_OFFSET] = ICE_RXDID_COMMS_AUX_IP_OFFSET,
	};

	return xtr_type < RTE_DIM(rxdid_map) ?
				rxdid_map[xtr_type] : ICE_RXDID_COMMS_OVS;
}

static inline void
ice_rxd_to_pkt_fields_by_comms_generic(__rte_unused struct ci_rx_queue *rxq,
				       struct rte_mbuf *mb,
				       volatile union ci_rx_flex_desc *rxdp)
{
	volatile struct ice_32b_rx_flex_desc_comms *desc =
			(volatile struct ice_32b_rx_flex_desc_comms *)rxdp;
	uint16_t stat_err = rte_le_to_cpu_16(desc->status_error0);

	if (likely(stat_err & (1 << ICE_RX_FLEX_DESC_STATUS0_RSS_VALID_S))) {
		mb->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		mb->hash.rss = rte_le_to_cpu_32(desc->rss_hash);
	}

#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	if (desc->flow_id != 0xFFFFFFFF) {
		mb->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
		mb->hash.fdir.hi = rte_le_to_cpu_32(desc->flow_id);
	}
#endif
}

static inline void
ice_rxd_to_pkt_fields_by_comms_ovs(__rte_unused struct ci_rx_queue *rxq,
				   struct rte_mbuf *mb,
				   volatile union ci_rx_flex_desc *rxdp)
{
	volatile struct ice_32b_rx_flex_desc_comms_ovs *desc =
			(volatile struct ice_32b_rx_flex_desc_comms_ovs *)rxdp;
#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	uint16_t stat_err;
#endif

	if (desc->flow_id != 0xFFFFFFFF) {
		mb->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
		mb->hash.fdir.hi = rte_le_to_cpu_32(desc->flow_id);
	}

#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	stat_err = rte_le_to_cpu_16(desc->status_error0);
	if (likely(stat_err & (1 << ICE_RX_FLEX_DESC_STATUS0_RSS_VALID_S))) {
		mb->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		mb->hash.rss = rte_le_to_cpu_32(desc->rss_hash);
	}
#endif
}

static inline void
ice_rxd_to_pkt_fields_by_comms_aux_v1(struct ci_rx_queue *rxq,
				      struct rte_mbuf *mb,
				      volatile union ci_rx_flex_desc *rxdp)
{
	volatile struct ice_32b_rx_flex_desc_comms *desc =
			(volatile struct ice_32b_rx_flex_desc_comms *)rxdp;
	uint16_t stat_err;

	stat_err = rte_le_to_cpu_16(desc->status_error0);
	if (likely(stat_err & (1 << ICE_RX_FLEX_DESC_STATUS0_RSS_VALID_S))) {
		mb->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		mb->hash.rss = rte_le_to_cpu_32(desc->rss_hash);
	}

#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	if (desc->flow_id != 0xFFFFFFFF) {
		mb->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
		mb->hash.fdir.hi = rte_le_to_cpu_32(desc->flow_id);
	}

	if (rxq->xtr_ol_flag) {
		uint32_t metadata = 0;

		stat_err = rte_le_to_cpu_16(desc->status_error1);

		if (stat_err & (1 << ICE_RX_FLEX_DESC_STATUS1_XTRMD4_VALID_S))
			metadata = rte_le_to_cpu_16(desc->flex_ts.flex.aux0);

		if (stat_err & (1 << ICE_RX_FLEX_DESC_STATUS1_XTRMD5_VALID_S))
			metadata |=
				rte_le_to_cpu_16(desc->flex_ts.flex.aux1) << 16;

		if (metadata) {
			mb->ol_flags |= rxq->xtr_ol_flag;

			*ICE_DYNF_PROTO_XTR_METADATA(mb, rxq->xtr_field_offs) = metadata;
		}
	}
#else
	RTE_SET_USED(rxq);
#endif
}

static inline void
ice_rxd_to_pkt_fields_by_comms_aux_v2(struct ci_rx_queue *rxq,
				      struct rte_mbuf *mb,
				      volatile union ci_rx_flex_desc *rxdp)
{
	volatile struct ice_32b_rx_flex_desc_comms *desc =
			(volatile struct ice_32b_rx_flex_desc_comms *)rxdp;
	uint16_t stat_err;

	stat_err = rte_le_to_cpu_16(desc->status_error0);
	if (likely(stat_err & (1 << ICE_RX_FLEX_DESC_STATUS0_RSS_VALID_S))) {
		mb->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		mb->hash.rss = rte_le_to_cpu_32(desc->rss_hash);
	}

#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	if (desc->flow_id != 0xFFFFFFFF) {
		mb->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
		mb->hash.fdir.hi = rte_le_to_cpu_32(desc->flow_id);
	}

	if (rxq->xtr_ol_flag) {
		uint32_t metadata = 0;

		if (desc->flex_ts.flex.aux0 != 0xFFFF)
			metadata = rte_le_to_cpu_16(desc->flex_ts.flex.aux0);
		else if (desc->flex_ts.flex.aux1 != 0xFFFF)
			metadata = rte_le_to_cpu_16(desc->flex_ts.flex.aux1);

		if (metadata) {
			mb->ol_flags |= rxq->xtr_ol_flag;

			*ICE_DYNF_PROTO_XTR_METADATA(mb, rxq->xtr_field_offs) = metadata;
		}
	}
#else
	RTE_SET_USED(rxq);
#endif
}

static const ice_rxd_to_pkt_fields_t rxd_to_pkt_fields_ops[] = {
	[ICE_RXDID_COMMS_AUX_VLAN] = ice_rxd_to_pkt_fields_by_comms_aux_v1,
	[ICE_RXDID_COMMS_AUX_IPV4] = ice_rxd_to_pkt_fields_by_comms_aux_v1,
	[ICE_RXDID_COMMS_AUX_IPV6] = ice_rxd_to_pkt_fields_by_comms_aux_v1,
	[ICE_RXDID_COMMS_AUX_IPV6_FLOW] = ice_rxd_to_pkt_fields_by_comms_aux_v1,
	[ICE_RXDID_COMMS_AUX_TCP] = ice_rxd_to_pkt_fields_by_comms_aux_v1,
	[ICE_RXDID_COMMS_AUX_IP_OFFSET] = ice_rxd_to_pkt_fields_by_comms_aux_v2,
	[ICE_RXDID_COMMS_GENERIC] = ice_rxd_to_pkt_fields_by_comms_generic,
	[ICE_RXDID_COMMS_OVS] = ice_rxd_to_pkt_fields_by_comms_ovs,
};

void
ice_select_rxd_to_pkt_fields_handler(struct ci_rx_queue *rxq, uint32_t rxdid)
{
	rxq->rxdid = rxdid;

	switch (rxdid) {
	case ICE_RXDID_COMMS_AUX_VLAN:
	case ICE_RXDID_COMMS_AUX_IPV4:
	case ICE_RXDID_COMMS_AUX_IPV6:
	case ICE_RXDID_COMMS_AUX_IPV6_FLOW:
	case ICE_RXDID_COMMS_AUX_TCP:
	case ICE_RXDID_COMMS_AUX_IP_OFFSET:
		break;
	case ICE_RXDID_COMMS_GENERIC:
		/* fallthrough */
	case ICE_RXDID_COMMS_OVS:
		break;

	default:
		/* update this according to the RXDID for PROTO_XTR_NONE */
		rxq->rxdid = ICE_RXDID_COMMS_OVS;
		break;
	}

	if (rxq->xtr_field_offs == -1)
		rxq->xtr_ol_flag = 0;
}

static int
ice_program_hw_rx_queue(struct ci_rx_queue *rxq)
{
	struct ice_vsi *vsi = rxq->ice_vsi;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_pf *pf = ICE_VSI_TO_PF(vsi);
	struct rte_eth_dev_data *dev_data = rxq->ice_vsi->adapter->pf.dev_data;
	struct ice_rlan_ctx rx_ctx;
	uint16_t buf_size;
	uint32_t rxdid = ICE_RXDID_COMMS_OVS;
	uint32_t regval;
	struct ice_adapter *ad = rxq->ice_vsi->adapter;
	uint32_t frame_size = dev_data->mtu + ICE_ETH_OVERHEAD;
	int err;

	/* Set buffer size as the head split is disabled. */
	buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mp) -
			      RTE_PKTMBUF_HEADROOM);
	rxq->rx_buf_len = RTE_ALIGN_FLOOR(buf_size, (1 << ICE_RLAN_CTX_DBUF_S));
	rxq->rx_buf_len = RTE_MIN(rxq->rx_buf_len, ICE_RX_MAX_DATA_BUF_SIZE);
	rxq->max_pkt_len =
		RTE_MIN((uint32_t)ICE_SUPPORT_CHAIN_NUM * rxq->rx_buf_len,
			frame_size);

	if (rxq->max_pkt_len <= RTE_ETHER_MIN_LEN ||
	    rxq->max_pkt_len > ICE_FRAME_SIZE_MAX) {
		PMD_DRV_LOG(ERR, "maximum packet length must "
			    "be larger than %u and smaller than %u",
			    (uint32_t)RTE_ETHER_MIN_LEN,
			    (uint32_t)ICE_FRAME_SIZE_MAX);
		return -EINVAL;
	}

	if (rxq->ts_flag == 0 && (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)) {
		/* Register mbuf field and flag for Rx timestamp */
		err = rte_mbuf_dyn_rx_timestamp_register(
				&rxq->ts_offset,
				&rxq->ts_flag);
		if (err) {
			PMD_DRV_LOG(ERR,
				"Cannot register mbuf field/flag for timestamp");
			return -EINVAL;
		}
	}

	memset(&rx_ctx, 0, sizeof(rx_ctx));

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) {
		uint32_t proto_hdr;
		proto_hdr = rxq->rxseg[0].proto_hdr;

		if (proto_hdr == RTE_PTYPE_UNKNOWN) {
			PMD_DRV_LOG(ERR, "Buffer split protocol must be configured");
			return -EINVAL;
		}

		switch (proto_hdr & RTE_PTYPE_L4_MASK) {
		case RTE_PTYPE_L4_TCP:
		case RTE_PTYPE_L4_UDP:
			rx_ctx.dtype = ICE_RX_DTYPE_HEADER_SPLIT;
			rx_ctx.hsplit_0 = ICE_RLAN_RX_HSPLIT_0_SPLIT_TCP_UDP;
			goto set_hsplit_finish;
		case RTE_PTYPE_L4_SCTP:
			rx_ctx.dtype = ICE_RX_DTYPE_HEADER_SPLIT;
			rx_ctx.hsplit_0 = ICE_RLAN_RX_HSPLIT_0_SPLIT_SCTP;
			goto set_hsplit_finish;
		}

		switch (proto_hdr & RTE_PTYPE_L3_MASK) {
		case RTE_PTYPE_L3_IPV4_EXT_UNKNOWN:
		case RTE_PTYPE_L3_IPV6_EXT_UNKNOWN:
			rx_ctx.dtype = ICE_RX_DTYPE_HEADER_SPLIT;
			rx_ctx.hsplit_0 = ICE_RLAN_RX_HSPLIT_0_SPLIT_IP;
			goto set_hsplit_finish;
		}

		switch (proto_hdr & RTE_PTYPE_L2_MASK) {
		case RTE_PTYPE_L2_ETHER:
			rx_ctx.dtype = ICE_RX_DTYPE_HEADER_SPLIT;
			rx_ctx.hsplit_0 = ICE_RLAN_RX_HSPLIT_0_SPLIT_L2;
			rx_ctx.hsplit_1 = ICE_RLAN_RX_HSPLIT_1_SPLIT_L2;
			goto set_hsplit_finish;
		}

		switch (proto_hdr & RTE_PTYPE_INNER_L4_MASK) {
		case RTE_PTYPE_INNER_L4_TCP:
		case RTE_PTYPE_INNER_L4_UDP:
			rx_ctx.dtype = ICE_RX_DTYPE_HEADER_SPLIT;
			rx_ctx.hsplit_0 = ICE_RLAN_RX_HSPLIT_0_SPLIT_TCP_UDP;
			goto set_hsplit_finish;
		case RTE_PTYPE_INNER_L4_SCTP:
			rx_ctx.dtype = ICE_RX_DTYPE_HEADER_SPLIT;
			rx_ctx.hsplit_0 = ICE_RLAN_RX_HSPLIT_0_SPLIT_SCTP;
			goto set_hsplit_finish;
		}

		switch (proto_hdr & RTE_PTYPE_INNER_L3_MASK) {
		case RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN:
		case RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN:
			rx_ctx.dtype = ICE_RX_DTYPE_HEADER_SPLIT;
			rx_ctx.hsplit_0 = ICE_RLAN_RX_HSPLIT_0_SPLIT_IP;
			goto set_hsplit_finish;
		}

		switch (proto_hdr & RTE_PTYPE_INNER_L2_MASK) {
		case RTE_PTYPE_INNER_L2_ETHER:
			rx_ctx.dtype = ICE_RX_DTYPE_HEADER_SPLIT;
			rx_ctx.hsplit_0 = ICE_RLAN_RX_HSPLIT_0_SPLIT_L2;
			goto set_hsplit_finish;
		}

		switch (proto_hdr & RTE_PTYPE_TUNNEL_MASK) {
		case RTE_PTYPE_TUNNEL_GRENAT:
			rx_ctx.dtype = ICE_RX_DTYPE_HEADER_SPLIT;
			rx_ctx.hsplit_1 = ICE_RLAN_RX_HSPLIT_1_SPLIT_ALWAYS;
			goto set_hsplit_finish;
		}

		PMD_DRV_LOG(ERR, "Buffer split protocol is not supported");
		return -EINVAL;

set_hsplit_finish:
		rxq->rx_hdr_len = ICE_RX_HDR_BUF_SIZE;
	} else {
		rxq->rx_hdr_len = 0;
		rx_ctx.dtype = 0; /* No Protocol Based Buffer Split mode */
	}

	rx_ctx.base = rxq->rx_ring_phys_addr / ICE_QUEUE_BASE_ADDR_UNIT;
	rx_ctx.qlen = rxq->nb_rx_desc;
	rx_ctx.dbuf = rxq->rx_buf_len >> ICE_RLAN_CTX_DBUF_S;
	rx_ctx.hbuf = rxq->rx_hdr_len >> ICE_RLAN_CTX_HBUF_S;
#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	rx_ctx.dsize = 1; /* 32B descriptors */
#endif
	rx_ctx.rxmax = rxq->max_pkt_len;
	/* TPH: Transaction Layer Packet (TLP) processing hints */
	rx_ctx.tphrdesc_ena = 1;
	rx_ctx.tphwdesc_ena = 1;
	rx_ctx.tphdata_ena = 1;
	rx_ctx.tphhead_ena = 1;
	/* Low Receive Queue Threshold defined in 64 descriptors units.
	 * When the number of free descriptors goes below the lrxqthresh,
	 * an immediate interrupt is triggered.
	 */
	rx_ctx.lrxqthresh = 2;
	/*default use 32 byte descriptor, vlan tag extract to L2TAG2(1st)*/
	rx_ctx.l2tsel = 1;
	rx_ctx.showiv = 0;
	rx_ctx.crcstrip = (rxq->crc_len == 0) ? 1 : 0;

	rxdid = ice_proto_xtr_type_to_rxdid(rxq->proto_xtr);

	PMD_DRV_LOG(DEBUG, "Port (%u) - Rx queue (%u) is set with RXDID : %u",
		    rxq->port_id, rxq->queue_id, rxdid);

	if (!(pf->supported_rxdid & RTE_BIT64(rxdid))) {
		PMD_DRV_LOG(ERR, "currently package doesn't support RXDID (%u)",
			    rxdid);
		return -EINVAL;
	}

	rxq->rxdid = rxdid;

	/* Enable Flexible Descriptors in the queue context which
	 * allows this driver to select a specific receive descriptor format
	 */
	regval = (rxdid << QRXFLXP_CNTXT_RXDID_IDX_S) &
		QRXFLXP_CNTXT_RXDID_IDX_M;

	/* increasing context priority to pick up profile ID;
	 * default is 0x01; setting to 0x03 to ensure profile
	 * is programming if prev context is of same priority
	 */
	regval |= (0x03 << QRXFLXP_CNTXT_RXDID_PRIO_S) &
		QRXFLXP_CNTXT_RXDID_PRIO_M;

	if (ad->ptp_ena || rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
		regval |= QRXFLXP_CNTXT_TS_M;

	ICE_WRITE_REG(hw, QRXFLXP_CNTXT(rxq->reg_idx), regval);

	err = ice_clear_rxq_ctx(hw, rxq->reg_idx);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to clear Lan Rx queue (%u) context",
			    rxq->queue_id);
		return -EINVAL;
	}
	err = ice_write_rxq_ctx(hw, &rx_ctx, rxq->reg_idx);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to write Lan Rx queue (%u) context",
			    rxq->queue_id);
		return -EINVAL;
	}

	/* Check if scattered RX needs to be used. */
	if (frame_size > buf_size)
		dev_data->scattered_rx = 1;

	rxq->qrx_tail = hw->hw_addr + QRX_TAIL(rxq->reg_idx);

	/* Init the Rx tail register*/
	ICE_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	return 0;
}

/* Allocate mbufs for all descriptors in rx queue */
static int
ice_alloc_rx_queue_mbufs(struct ci_rx_queue *rxq)
{
	struct ci_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		volatile union ci_rx_flex_desc *rxd;
		rxd = &rxq->rx_flex_ring[i];
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mp);

		if (unlikely(!mbuf)) {
			PMD_DRV_LOG(ERR, "Failed to allocate mbuf for RX");
			return -ENOMEM;
		}

		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->nb_segs = 1;
		mbuf->port = rxq->port_id;

		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

		if (!(rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
			rte_mbuf_refcnt_set(mbuf, 1);
			mbuf->next = NULL;
			rxd->read.hdr_addr = 0;
			rxd->read.pkt_addr = dma_addr;
		} else {
			struct rte_mbuf *mbuf_pay;
			mbuf_pay = rte_mbuf_raw_alloc(rxq->rxseg[1].mp);
			if (unlikely(!mbuf_pay)) {
				rte_pktmbuf_free(mbuf);
				PMD_DRV_LOG(ERR, "Failed to allocate payload mbuf for RX");
				return -ENOMEM;
			}

			mbuf_pay->next = NULL;
			mbuf_pay->data_off = RTE_PKTMBUF_HEADROOM;
			mbuf_pay->nb_segs = 1;
			mbuf_pay->port = rxq->port_id;
			mbuf->next = mbuf_pay;

			rxd->read.hdr_addr = dma_addr;
			/* The LS bit should be set to zero regardless of
			 * buffer split enablement.
			 */
			rxd->read.pkt_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf_pay));
		}

#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
		rxd->read.rsvd1 = 0;
		rxd->read.rsvd2 = 0;
#endif
		rxe[i].mbuf = mbuf;
	}

	return 0;
}

/* Free all mbufs for descriptors in rx queue */
static void
_ice_rx_queue_release_mbufs(struct ci_rx_queue *rxq)
{
	uint16_t i;

	if (!rxq || !rxq->sw_ring) {
		PMD_DRV_LOG(DEBUG, "Pointer to sw_ring is NULL");
		return;
	}

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (rxq->sw_ring[i].mbuf) {
			rte_pktmbuf_free(rxq->sw_ring[i].mbuf);
			rxq->sw_ring[i].mbuf = NULL;
		}
	}
	if (rxq->rx_nb_avail == 0)
		return;
	for (i = 0; i < rxq->rx_nb_avail; i++)
		rte_pktmbuf_free(rxq->rx_stage[rxq->rx_next_avail + i]);

	rxq->rx_nb_avail = 0;
}

/* turn on or off rx queue
 * @q_idx: queue index in pf scope
 * @on: turn on or off the queue
 */
static int
ice_switch_rx_queue(struct ice_hw *hw, uint16_t q_idx, bool on)
{
	uint32_t reg;
	uint16_t j;

	/* QRX_CTRL = QRX_ENA */
	reg = ICE_READ_REG(hw, QRX_CTRL(q_idx));

	if (on) {
		if (reg & QRX_CTRL_QENA_STAT_M)
			return 0; /* Already on, skip */
		reg |= QRX_CTRL_QENA_REQ_M;
	} else {
		if (!(reg & QRX_CTRL_QENA_STAT_M))
			return 0; /* Already off, skip */
		reg &= ~QRX_CTRL_QENA_REQ_M;
	}

	/* Write the register */
	ICE_WRITE_REG(hw, QRX_CTRL(q_idx), reg);
	/* Check the result. It is said that QENA_STAT
	 * follows the QENA_REQ not more than 10 use.
	 * TODO: need to change the wait counter later
	 */
	for (j = 0; j < ICE_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(ICE_CHK_Q_ENA_INTERVAL_US);
		reg = ICE_READ_REG(hw, QRX_CTRL(q_idx));
		if (on) {
			if ((reg & QRX_CTRL_QENA_REQ_M) &&
			    (reg & QRX_CTRL_QENA_STAT_M))
				break;
		} else {
			if (!(reg & QRX_CTRL_QENA_REQ_M) &&
			    !(reg & QRX_CTRL_QENA_STAT_M))
				break;
		}
	}

	/* Check if it is timeout */
	if (j >= ICE_CHK_Q_ENA_COUNT) {
		PMD_DRV_LOG(ERR, "Failed to %s rx queue[%u]",
			    (on ? "enable" : "disable"), q_idx);
		return -ETIMEDOUT;
	}

	return 0;
}

static inline int
ice_check_rx_burst_bulk_alloc_preconditions(struct ci_rx_queue *rxq)
{
	int ret = 0;

	if (!(rxq->rx_free_thresh >= ICE_RX_MAX_BURST)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->rx_free_thresh=%d, "
			     "ICE_RX_MAX_BURST=%d",
			     rxq->rx_free_thresh, ICE_RX_MAX_BURST);
		ret = -EINVAL;
	} else if (!(rxq->rx_free_thresh < rxq->nb_rx_desc)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->rx_free_thresh=%d, "
			     "rxq->nb_rx_desc=%d",
			     rxq->rx_free_thresh, rxq->nb_rx_desc);
		ret = -EINVAL;
	} else if (rxq->nb_rx_desc % rxq->rx_free_thresh != 0) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->nb_rx_desc=%d, "
			     "rxq->rx_free_thresh=%d",
			     rxq->nb_rx_desc, rxq->rx_free_thresh);
		ret = -EINVAL;
	}

	return ret;
}

/* reset fields in ci_rx_queue back to default */
static void
ice_reset_rx_queue(struct ci_rx_queue *rxq)
{
	unsigned int i;
	uint16_t len;

	if (!rxq) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq is NULL");
		return;
	}

	len = (uint16_t)(rxq->nb_rx_desc + ICE_RX_MAX_BURST);

	for (i = 0; i < len * sizeof(union ci_rx_flex_desc); i++)
		((volatile char *)rxq->rx_flex_ring)[i] = 0;

	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));
	for (i = 0; i < ICE_RX_MAX_BURST; ++i)
		rxq->sw_ring[rxq->nb_rx_desc + i].mbuf = &rxq->fake_mbuf;

	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);

	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;

	rxq->rxrearm_start = 0;
	rxq->rxrearm_nb = 0;
}

int
ice_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ci_rx_queue *rxq;
	int err;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	if (rx_queue_id >= dev->data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "RX queue %u is out of range %u",
			    rx_queue_id, dev->data->nb_rx_queues);
		return -EINVAL;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	if (!rxq || !rxq->q_set) {
		PMD_DRV_LOG(ERR, "RX queue %u not available or setup",
			    rx_queue_id);
		return -EINVAL;
	}

	if (dev->data->rx_queue_state[rx_queue_id] ==
		RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
		rxq->offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;
	err = ice_program_hw_rx_queue(rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to program RX queue %u",
			    rx_queue_id);
		return -EIO;
	}

	err = ice_alloc_rx_queue_mbufs(rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate RX queue mbuf");
		return -ENOMEM;
	}

	/* Init the RX tail register. */
	ICE_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	err = ice_switch_rx_queue(hw, rxq->reg_idx, true);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u on",
			    rx_queue_id);

		rxq->rx_rel_mbufs(rxq);
		ice_reset_rx_queue(rxq);
		return -EINVAL;
	}

	dev->data->rx_queue_state[rx_queue_id] =
		RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

int
ice_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ci_rx_queue *rxq;
	int err;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rx_queue_id < dev->data->nb_rx_queues) {
		rxq = dev->data->rx_queues[rx_queue_id];

		if (dev->data->rx_queue_state[rx_queue_id] ==
			RTE_ETH_QUEUE_STATE_STOPPED)
			return 0;

		err = ice_switch_rx_queue(hw, rxq->reg_idx, false);
		if (err) {
			PMD_DRV_LOG(ERR, "Failed to switch RX queue %u off",
				    rx_queue_id);
			return -EINVAL;
		}
		rxq->rx_rel_mbufs(rxq);
		ice_reset_rx_queue(rxq);
		dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_STOPPED;
	}

	return 0;
}

/**
 * ice_setup_txtime_ctx - setup a struct ice_txtime_ctx instance
 * @txq: The queue on which tstamp ring to configure
 * @txtime_ctx: Pointer to the Tx time queue context structure to be initialized
 * @txtime_ena: Tx time enable flag, set to true if Tx time should be enabled
 */
static int
ice_setup_txtime_ctx(struct ci_tx_queue *txq,
		     struct ice_txtime_ctx *txtime_ctx, bool txtime_ena)
{
	struct ice_vsi *vsi = txq->ice_vsi;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);

	txtime_ctx->base = txq->tsq->ts_mz->iova >> ICE_TX_CMPLTNQ_CTX_BASE_S;

	/* Tx time Queue Length */
	txtime_ctx->qlen = txq->tsq->nb_ts_desc;

	if (txtime_ena)
		txtime_ctx->txtime_ena_q = 1;

	/* PF number */
	txtime_ctx->pf_num = hw->pf_id;

	switch (vsi->type) {
	case ICE_VSI_PF:
		txtime_ctx->vmvf_type = ICE_TLAN_CTX_VMVF_TYPE_PF;
		break;
	default:
		PMD_DRV_LOG(ERR, "Unable to set VMVF type for VSI type %d", vsi->type);
		return -EINVAL;
	}

	/* make sure the context is associated with the right VSI */
	txtime_ctx->src_vsi = vsi->vsi_id;

	txtime_ctx->ts_res = ICE_TXTIME_CTX_RESOLUTION_128NS;
	txtime_ctx->drbell_mode_32 = ICE_TXTIME_CTX_DRBELL_MODE_32;
	txtime_ctx->ts_fetch_prof_id = ICE_TXTIME_CTX_FETCH_PROF_ID_0;

	return 0;
}

int
ice_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ci_tx_queue *txq;
	int err;
	struct ice_vsi *vsi;
	struct ice_hw *hw;
	struct ice_pf *pf;
	struct ice_aqc_add_tx_qgrp *txq_elem;
	struct ice_tlan_ctx tx_ctx;
	int buf_len;
	struct ice_adapter *ad = ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	u16 q_base, q_range, cgd_idx = 0;

	PMD_INIT_FUNC_TRACE();

	if (tx_queue_id >= dev->data->nb_tx_queues) {
		PMD_DRV_LOG(ERR, "TX queue %u is out of range %u",
			    tx_queue_id, dev->data->nb_tx_queues);
		return -EINVAL;
	}

	txq = dev->data->tx_queues[tx_queue_id];
	if (!txq || !txq->q_set) {
		PMD_DRV_LOG(ERR, "TX queue %u is not available or setup",
			    tx_queue_id);
		return -EINVAL;
	}

	if (dev->data->tx_queue_state[tx_queue_id] ==
		RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	buf_len = ice_struct_size(txq_elem, txqs, 1);
	txq_elem = ice_malloc(hw, buf_len);
	if (!txq_elem)
		return -ENOMEM;

	vsi = txq->ice_vsi;
	hw = ICE_VSI_TO_HW(vsi);
	pf = ICE_VSI_TO_PF(vsi);

	memset(&tx_ctx, 0, sizeof(tx_ctx));
	txq_elem->num_txqs = 1;
	txq_elem->txqs[0].txq_id = rte_cpu_to_le_16(txq->reg_idx);

	tx_ctx.base = txq->tx_ring_dma / ICE_QUEUE_BASE_ADDR_UNIT;
	tx_ctx.qlen = txq->nb_tx_desc;
	tx_ctx.pf_num = hw->pf_id;
	tx_ctx.vmvf_type = ICE_TLAN_CTX_VMVF_TYPE_PF;
	tx_ctx.src_vsi = vsi->vsi_id;
	tx_ctx.port_num = hw->port_info->lport;
	tx_ctx.tso_ena = 1; /* tso enable */
	tx_ctx.tso_qnum = txq->reg_idx; /* index for tso state structure */
	tx_ctx.legacy_int = 1; /* Legacy or Advanced Host Interface */
	tx_ctx.tsyn_ena = 1;

	/* Mirror RXQ<->CGD association to TXQ<->CGD */
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		q_base = rte_le_to_cpu_16(vsi->info.tc_mapping[i]) & ICE_AQ_VSI_TC_Q_OFFSET_M;
		q_range = 1 << ((rte_le_to_cpu_16(vsi->info.tc_mapping[i]) &
			ICE_AQ_VSI_TC_Q_NUM_M) >> ICE_AQ_VSI_TC_Q_NUM_S);

		if (q_base <= tx_queue_id && tx_queue_id < q_base + q_range)
			break;

		cgd_idx++;
	}

	if (cgd_idx >= ICE_MAX_TRAFFIC_CLASS) {
		PMD_DRV_LOG(ERR, "Bad queue mapping configuration");
		rte_free(txq_elem);
		return -EINVAL;
	}

	tx_ctx.cgd_num = cgd_idx;

	ice_set_ctx(hw, (uint8_t *)&tx_ctx, txq_elem->txqs[0].txq_ctx,
		    ice_tlan_ctx_info);

	/* Fix me, we assume TC always 0 here */
	err = ice_ena_vsi_txq(hw->port_info, vsi->idx, 0, tx_queue_id, 1,
			txq_elem, buf_len, NULL);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to add lan txq");
		rte_free(txq_elem);
		return -EIO;
	}
	/* store the schedule node id */
	txq->q_teid = txq_elem->txqs[0].q_teid;

	/* move the queue to correct position in hierarchy, if explicit hierarchy configured */
	if (pf->tm_conf.committed)
		if (ice_tm_setup_txq_node(pf, hw, tx_queue_id, txq->q_teid) != 0) {
			PMD_DRV_LOG(ERR, "Failed to set up txq traffic management node");
			rte_free(txq_elem);
			return -EIO;
		}

	/* record what kind of descriptor cleanup we need on teardown */
	txq->vector_tx = ad->tx_vec_allowed;

	if (txq->tsq != NULL && txq->tsq->ts_flag > 0) {
		struct ice_aqc_set_txtime_qgrp *ts_elem;
		struct ice_txtime_ctx txtime_ctx = { 0 };
		u8 ts_buf_len = ice_struct_size(ts_elem, txtimeqs, 1);

		ts_elem = ice_malloc(hw, ts_buf_len);
		ice_setup_txtime_ctx(txq, &txtime_ctx, true);
		ice_set_ctx(hw, (u8 *)&txtime_ctx,
				ts_elem->txtimeqs[0].txtime_ctx,
				ice_txtime_ctx_info);

		txq->qtx_tail = hw->hw_addr + E830_GLQTX_TXTIME_DBELL_LSB(txq->reg_idx);

		/* Init the Tx time tail register*/
		ICE_PCI_REG_WRITE(txq->qtx_tail, 0);

		err = ice_aq_set_txtimeq(hw, txq->reg_idx, 1, ts_elem, ts_buf_len, NULL);
		rte_free(ts_elem);
		if (err) {
			PMD_DRV_LOG(ERR, "Failed to set Tx Time queue context, error: %d", err);
			rte_free(txq_elem);
			return err;
		}
	} else {
		txq->qtx_tail = hw->hw_addr + QTX_COMM_DBELL(txq->reg_idx);

		/* Init the Tx tail register*/
		ICE_PCI_REG_WRITE(txq->qtx_tail, 0);
	}

	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	rte_free(txq_elem);
	return 0;
}

static int
ice_fdir_program_hw_rx_queue(struct ci_rx_queue *rxq)
{
	struct ice_vsi *vsi = rxq->ice_vsi;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	uint32_t rxdid = ICE_RXDID_LEGACY_1;
	struct ice_rlan_ctx rx_ctx;
	uint32_t regval;
	int err;

	rxq->rx_hdr_len = 0;
	rxq->rx_buf_len = 1024;

	memset(&rx_ctx, 0, sizeof(rx_ctx));

	rx_ctx.base = rxq->rx_ring_phys_addr / ICE_QUEUE_BASE_ADDR_UNIT;
	rx_ctx.qlen = rxq->nb_rx_desc;
	rx_ctx.dbuf = rxq->rx_buf_len >> ICE_RLAN_CTX_DBUF_S;
	rx_ctx.hbuf = rxq->rx_hdr_len >> ICE_RLAN_CTX_HBUF_S;
	rx_ctx.dtype = 0; /* No Buffer Split mode */
	rx_ctx.dsize = 1; /* 32B descriptors */
	rx_ctx.rxmax = ICE_ETH_MAX_LEN;
	/* TPH: Transaction Layer Packet (TLP) processing hints */
	rx_ctx.tphrdesc_ena = 1;
	rx_ctx.tphwdesc_ena = 1;
	rx_ctx.tphdata_ena = 1;
	rx_ctx.tphhead_ena = 1;
	/* Low Receive Queue Threshold defined in 64 descriptors units.
	 * When the number of free descriptors goes below the lrxqthresh,
	 * an immediate interrupt is triggered.
	 */
	rx_ctx.lrxqthresh = 2;
	/*default use 32 byte descriptor, vlan tag extract to L2TAG2(1st)*/
	rx_ctx.l2tsel = 1;
	rx_ctx.showiv = 0;
	rx_ctx.crcstrip = (rxq->crc_len == 0) ? 1 : 0;

	/* Enable Flexible Descriptors in the queue context which
	 * allows this driver to select a specific receive descriptor format
	 */
	regval = (rxdid << QRXFLXP_CNTXT_RXDID_IDX_S) &
		QRXFLXP_CNTXT_RXDID_IDX_M;

	/* increasing context priority to pick up profile ID;
	 * default is 0x01; setting to 0x03 to ensure profile
	 * is programming if prev context is of same priority
	 */
	regval |= (0x03 << QRXFLXP_CNTXT_RXDID_PRIO_S) &
		QRXFLXP_CNTXT_RXDID_PRIO_M;

	ICE_WRITE_REG(hw, QRXFLXP_CNTXT(rxq->reg_idx), regval);

	err = ice_clear_rxq_ctx(hw, rxq->reg_idx);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to clear Lan Rx queue (%u) context",
			    rxq->queue_id);
		return -EINVAL;
	}
	err = ice_write_rxq_ctx(hw, &rx_ctx, rxq->reg_idx);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to write Lan Rx queue (%u) context",
			    rxq->queue_id);
		return -EINVAL;
	}

	rxq->qrx_tail = hw->hw_addr + QRX_TAIL(rxq->reg_idx);

	/* Init the Rx tail register*/
	ICE_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	return 0;
}

int
ice_fdir_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ci_rx_queue *rxq;
	int err;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	rxq = pf->fdir.rxq;
	if (!rxq || !rxq->q_set) {
		PMD_DRV_LOG(ERR, "FDIR RX queue %u not available or setup",
			    rx_queue_id);
		return -EINVAL;
	}

	err = ice_fdir_program_hw_rx_queue(rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to program FDIR RX queue %u",
			    rx_queue_id);
		return -EIO;
	}

	/* Init the RX tail register. */
	ICE_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	err = ice_switch_rx_queue(hw, rxq->reg_idx, true);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch FDIR RX queue %u on",
			    rx_queue_id);

		ice_reset_rx_queue(rxq);
		return -EINVAL;
	}

	return 0;
}

int
ice_fdir_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ci_tx_queue *txq;
	int err;
	struct ice_vsi *vsi;
	struct ice_hw *hw;
	struct ice_aqc_add_tx_qgrp *txq_elem;
	struct ice_tlan_ctx tx_ctx;
	int buf_len;

	PMD_INIT_FUNC_TRACE();

	txq = pf->fdir.txq;
	if (!txq || !txq->q_set) {
		PMD_DRV_LOG(ERR, "FDIR TX queue %u is not available or setup",
			    tx_queue_id);
		return -EINVAL;
	}

	buf_len = ice_struct_size(txq_elem, txqs, 1);
	txq_elem = ice_malloc(hw, buf_len);
	if (!txq_elem)
		return -ENOMEM;

	vsi = txq->ice_vsi;
	hw = ICE_VSI_TO_HW(vsi);

	memset(&tx_ctx, 0, sizeof(tx_ctx));
	txq_elem->num_txqs = 1;
	txq_elem->txqs[0].txq_id = rte_cpu_to_le_16(txq->reg_idx);

	tx_ctx.base = txq->tx_ring_dma / ICE_QUEUE_BASE_ADDR_UNIT;
	tx_ctx.qlen = txq->nb_tx_desc;
	tx_ctx.pf_num = hw->pf_id;
	tx_ctx.vmvf_type = ICE_TLAN_CTX_VMVF_TYPE_PF;
	tx_ctx.src_vsi = vsi->vsi_id;
	tx_ctx.port_num = hw->port_info->lport;
	tx_ctx.tso_ena = 1; /* tso enable */
	tx_ctx.tso_qnum = txq->reg_idx; /* index for tso state structure */
	tx_ctx.legacy_int = 1; /* Legacy or Advanced Host Interface */

	ice_set_ctx(hw, (uint8_t *)&tx_ctx, txq_elem->txqs[0].txq_ctx,
		    ice_tlan_ctx_info);

	txq->qtx_tail = hw->hw_addr + QTX_COMM_DBELL(txq->reg_idx);

	/* Init the Tx tail register*/
	ICE_PCI_REG_WRITE(txq->qtx_tail, 0);

	/* Fix me, we assume TC always 0 here */
	err = ice_ena_vsi_txq(hw->port_info, vsi->idx, 0, tx_queue_id, 1,
			      txq_elem, buf_len, NULL);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to add FDIR txq");
		rte_free(txq_elem);
		return -EIO;
	}
	/* store the schedule node id */
	txq->q_teid = txq_elem->txqs[0].q_teid;

	rte_free(txq_elem);
	return 0;
}

static void
ice_reset_tx_queue(struct ci_tx_queue *txq)
{
	struct ci_tx_entry *txe;
	uint16_t i, prev, size;

	if (!txq) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	txe = txq->sw_ring;
	size = sizeof(struct ice_tx_desc) * txq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)txq->ice_tx_ring)[i] = 0;

	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile struct ice_tx_desc *txd = &txq->ice_tx_ring[i];

		txd->cmd_type_offset_bsz =
			rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DESC_DONE);
		txe[i].mbuf =  NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);
	txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

	txq->tx_tail = 0;
	txq->nb_tx_used = 0;

	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);

	if (txq->tsq != NULL && txq->tsq->ts_flag > 0) {
		for (i = 0; i < txq->tsq->nb_ts_desc; i++) {
			volatile struct ice_ts_desc *tsd = &txq->tsq->ice_ts_ring[i];
			tsd->tx_desc_idx_tstamp = 0;
		}

		txq->tsq->ts_tail = 0;
	}
}

int
ice_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ci_tx_queue *txq;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	uint16_t q_ids[1];
	uint32_t q_teids[1];
	uint16_t q_handle = tx_queue_id;
	int status;

	if (tx_queue_id >= dev->data->nb_tx_queues) {
		PMD_DRV_LOG(ERR, "TX queue %u is out of range %u",
			    tx_queue_id, dev->data->nb_tx_queues);
		return -EINVAL;
	}

	txq = dev->data->tx_queues[tx_queue_id];
	if (!txq) {
		PMD_DRV_LOG(ERR, "TX queue %u is not available",
			    tx_queue_id);
		return -EINVAL;
	}

	if (dev->data->tx_queue_state[tx_queue_id] ==
		RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;

	q_ids[0] = txq->reg_idx;
	q_teids[0] = txq->q_teid;

	if (txq->tsq != NULL && txq->tsq->ts_flag > 0) {
		struct ice_aqc_ena_dis_txtime_qgrp txtime_pg;

		dev->dev_ops->timesync_disable(dev);
		status = ice_aq_ena_dis_txtimeq(hw, q_ids[0], 1, 0, &txtime_pg, NULL);
		if (status != ICE_SUCCESS) {
			PMD_DRV_LOG(DEBUG, "Failed to disable Tx time queue");
			return -EINVAL;
		}
	}

	/* Fix me, we assume TC always 0 here */
	status = ice_dis_vsi_txq(hw->port_info, vsi->idx, 0, 1, &q_handle,
				q_ids, q_teids, ICE_NO_RESET, 0, NULL);
	if (status != ICE_SUCCESS) {
		PMD_DRV_LOG(DEBUG, "Failed to disable Lan Tx queue");
		return -EINVAL;
	}

	ci_txq_release_all_mbufs(txq, false);
	ice_reset_tx_queue(txq);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
ice_fdir_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ci_rx_queue *rxq;
	int err;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	rxq = pf->fdir.rxq;

	err = ice_switch_rx_queue(hw, rxq->reg_idx, false);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch FDIR RX queue %u off",
			    rx_queue_id);
		return -EINVAL;
	}
	rxq->rx_rel_mbufs(rxq);

	return 0;
}

int
ice_fdir_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ci_tx_queue *txq;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	uint16_t q_ids[1];
	uint32_t q_teids[1];
	uint16_t q_handle = tx_queue_id;
	int status;

	txq = pf->fdir.txq;
	if (!txq) {
		PMD_DRV_LOG(ERR, "TX queue %u is not available",
			    tx_queue_id);
		return -EINVAL;
	}
	if (txq->qtx_tail == NULL) {
		PMD_DRV_LOG(INFO, "TX queue %u not started", tx_queue_id);
		return 0;
	}
	vsi = txq->ice_vsi;

	q_ids[0] = txq->reg_idx;
	q_teids[0] = txq->q_teid;

	/* Fix me, we assume TC always 0 here */
	status = ice_dis_vsi_txq(hw->port_info, vsi->idx, 0, 1, &q_handle,
				 q_ids, q_teids, ICE_NO_RESET, 0, NULL);
	if (status != ICE_SUCCESS) {
		PMD_DRV_LOG(DEBUG, "Failed to disable Lan Tx queue");
		return -EINVAL;
	}

	ci_txq_release_all_mbufs(txq, false);
	txq->qtx_tail = NULL;

	return 0;
}

int
ice_rx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t queue_idx,
		   uint16_t nb_desc,
		   unsigned int socket_id,
		   const struct rte_eth_rxconf *rx_conf,
		   struct rte_mempool *mp)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct ci_rx_queue *rxq;
	const struct rte_memzone *rz;
	uint32_t ring_size, tlen;
	uint16_t len;
	int use_def_burst_func = 1;
	uint64_t offloads;
	uint16_t n_seg = rx_conf->rx_nseg;
	uint16_t i;

	if (nb_desc % ICE_ALIGN_RING_DESC != 0 ||
	    nb_desc > ICE_MAX_RING_DESC ||
	    nb_desc < ICE_MIN_RING_DESC) {
		PMD_INIT_LOG(ERR, "Number (%u) of receive descriptors is "
			     "invalid", nb_desc);
		return -EINVAL;
	}

	offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	if (mp)
		n_seg = 1;

	if (n_seg > 1 && !(offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
		PMD_INIT_LOG(ERR, "port %u queue index %u split offload not configured",
				dev->data->port_id, queue_idx);
		return -EINVAL;
	}

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx]) {
		ice_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Allocate the rx queue data structure */
	rxq = rte_zmalloc_socket(NULL,
				 sizeof(struct ci_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);

	if (!rxq) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for "
			     "rx queue data structure");
		return -ENOMEM;
	}

	rxq->rxseg_nb = n_seg;
	if (n_seg > 1) {
		for (i = 0; i < n_seg; i++)
			memcpy(&rxq->rxseg[i], &rx_conf->rx_seg[i].split,
				sizeof(struct rte_eth_rxseg_split));

		rxq->mp = rxq->rxseg[0].mp;
	} else {
		rxq->mp = mp;
	}

	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->offloads = offloads;

	rxq->reg_idx = vsi->base_queue + queue_idx;
	rxq->port_id = dev->data->port_id;
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;

	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->ice_vsi = vsi;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->proto_xtr = pf->proto_xtr != NULL ?
			 pf->proto_xtr[queue_idx] : PROTO_XTR_NONE;
	if (rxq->proto_xtr != PROTO_XTR_NONE &&
			ad->devargs.xtr_flag_offs[rxq->proto_xtr] != 0xff)
		rxq->xtr_ol_flag = 1ULL << ad->devargs.xtr_flag_offs[rxq->proto_xtr];
	rxq->xtr_field_offs = ad->devargs.xtr_field_offs;

	/* Allocate the maximum number of RX ring hardware descriptor. */
	len = ICE_MAX_NUM_DESC_BY_MAC(hw);

	/**
	 * Allocating a little more memory because vectorized/bulk_alloc Rx
	 * functions doesn't check boundaries each time.
	 */
	len += ICE_RX_MAX_BURST;

	/* Allocate the maximum number of RX ring hardware descriptor. */
	ring_size = sizeof(union ci_rx_flex_desc) * len;
	ring_size = RTE_ALIGN(ring_size, ICE_DMA_MEM_ALIGN);
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
				      ring_size, ICE_RING_BASE_ALIGN,
				      socket_id);
	if (!rz) {
		ice_rx_queue_release(rxq);
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for RX");
		return -ENOMEM;
	}

	rxq->mz = rz;
	/* Zero all the descriptors in the ring. */
	memset(rz->addr, 0, ring_size);

	rxq->rx_ring_phys_addr = rz->iova;
	rxq->rx_flex_ring = rz->addr;

	/* always reserve more for bulk alloc */
	len = (uint16_t)(nb_desc + ICE_RX_MAX_BURST);

	/* allocate extra entries for SW split buffer */
	tlen = ((rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) != 0) ?
		rxq->rx_free_thresh : 0;
	tlen += len;

	/* Allocate the software ring. */
	rxq->sw_ring = rte_zmalloc_socket(NULL,
					  sizeof(struct ci_rx_entry) * tlen,
					  RTE_CACHE_LINE_SIZE,
					  socket_id);
	if (!rxq->sw_ring) {
		ice_rx_queue_release(rxq);
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW ring");
		return -ENOMEM;
	}

	rxq->sw_split_buf = (tlen == len) ? NULL : rxq->sw_ring + len;

	ice_reset_rx_queue(rxq);
	rxq->q_set = true;
	dev->data->rx_queues[queue_idx] = rxq;
	rxq->rx_rel_mbufs = _ice_rx_queue_release_mbufs;

	use_def_burst_func = ice_check_rx_burst_bulk_alloc_preconditions(rxq);

	if (!use_def_burst_func) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
			     "satisfied. Rx Burst Bulk Alloc function will be "
			     "used on port=%d, queue=%d.",
			     rxq->port_id, rxq->queue_id);
	} else {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
			     "not satisfied, Scattered Rx is requested. "
			     "on port=%d, queue=%d.",
			     rxq->port_id, rxq->queue_id);
		ad->rx_bulk_alloc_allowed = false;
	}

	return 0;
}

void
ice_rx_queue_release(void *rxq)
{
	struct ci_rx_queue *q = (struct ci_rx_queue *)rxq;

	if (!q) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq is NULL");
		return;
	}

	if (q->rx_rel_mbufs != NULL)
		q->rx_rel_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(q);
}

/**
 * ice_calc_ts_ring_count - Calculate the number of timestamp descriptors
 * @hw: pointer to the hardware structure
 * @tx_desc_count: number of Tx descriptors in the ring
 *
 * Return: the number of timestamp descriptors
 */
static uint16_t
ice_calc_ts_ring_count(struct ice_hw *hw, u16 tx_desc_count)
{
	u16 prof = ICE_TXTIME_CTX_FETCH_PROF_ID_0;
	u16 max_fetch_desc = 0;
	u16 fetch;
	u32 reg;
	u16 i;

	for (i = 0; i < ICE_TXTIME_FETCH_PROFILE_CNT; i++) {
		reg = rd32(hw, E830_GLTXTIME_FETCH_PROFILE(prof, 0));
		fetch = FIELD_GET(E830_GLTXTIME_FETCH_PROFILE_FETCH_TS_DESC_M, reg);
		max_fetch_desc = max(fetch, max_fetch_desc);
	}

	if (!max_fetch_desc)
		max_fetch_desc = ICE_TXTIME_FETCH_TS_DESC_DFLT;

	max_fetch_desc = RTE_ALIGN(max_fetch_desc, ICE_REQ_DESC_MULTIPLE);

	return tx_desc_count + max_fetch_desc;
}

int
ice_tx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t queue_idx,
		   uint16_t nb_desc,
		   unsigned int socket_id,
		   const struct rte_eth_txconf *tx_conf)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct ci_tx_queue *txq;
	const struct rte_memzone *tz;
	uint32_t ring_size;
	uint16_t tx_rs_thresh, tx_free_thresh;
	uint64_t offloads;

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	if (nb_desc % ICE_ALIGN_RING_DESC != 0 ||
	    nb_desc > ICE_MAX_RING_DESC ||
	    nb_desc < ICE_MIN_RING_DESC) {
		PMD_INIT_LOG(ERR, "Number (%u) of transmit descriptors is "
			     "invalid", nb_desc);
		return -EINVAL;
	}

	/**
	 * The following two parameters control the setting of the RS bit on
	 * transmit descriptors. TX descriptors will have their RS bit set
	 * after txq->tx_rs_thresh descriptors have been used. The TX
	 * descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required to
	 * transmit a packet is greater than the number of free TX descriptors.
	 *
	 * The following constraints must be satisfied:
	 *  - tx_rs_thresh must be greater than 0.
	 *  - tx_rs_thresh must be less than the size of the ring minus 2.
	 *  - tx_rs_thresh must be less than or equal to tx_free_thresh.
	 *  - tx_rs_thresh must be a divisor of the ring size.
	 *  - tx_free_thresh must be greater than 0.
	 *  - tx_free_thresh must be less than the size of the ring minus 3.
	 *  - tx_free_thresh + tx_rs_thresh must not exceed nb_desc.
	 *
	 * One descriptor in the TX ring is used as a sentinel to avoid a H/W
	 * race condition, hence the maximum threshold constraints. When set
	 * to zero use default values.
	 */
	tx_free_thresh = (uint16_t)(tx_conf->tx_free_thresh ?
				    tx_conf->tx_free_thresh :
				    ICE_DEFAULT_TX_FREE_THRESH);
	/* force tx_rs_thresh to adapt an aggressive tx_free_thresh */
	tx_rs_thresh =
		(ICE_DEFAULT_TX_RSBIT_THRESH + tx_free_thresh > nb_desc) ?
			nb_desc - tx_free_thresh : ICE_DEFAULT_TX_RSBIT_THRESH;
	if (tx_conf->tx_rs_thresh)
		tx_rs_thresh = tx_conf->tx_rs_thresh;
	if (tx_rs_thresh + tx_free_thresh > nb_desc) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh + tx_free_thresh must not "
				"exceed nb_desc. (tx_rs_thresh=%u "
				"tx_free_thresh=%u nb_desc=%u port = %d queue=%d)",
				(unsigned int)tx_rs_thresh,
				(unsigned int)tx_free_thresh,
				(unsigned int)nb_desc,
				(int)dev->data->port_id,
				(int)queue_idx);
		return -EINVAL;
	}
	if (tx_rs_thresh >= (nb_desc - 2)) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be less than the "
			     "number of TX descriptors minus 2. "
			     "(tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}
	if (tx_free_thresh >= (nb_desc - 3)) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be less than the "
			     "tx_free_thresh must be less than the "
			     "number of TX descriptors minus 3. "
			     "(tx_free_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}
	if (tx_rs_thresh > tx_free_thresh) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be less than or "
			     "equal to tx_free_thresh. (tx_free_thresh=%u"
			     " tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}
	if ((nb_desc % tx_rs_thresh) != 0) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be a divisor of the "
			     "number of TX descriptors. (tx_rs_thresh=%u"
			     " port=%d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}
	if (tx_rs_thresh > 1 && tx_conf->tx_thresh.wthresh != 0) {
		PMD_INIT_LOG(ERR, "TX WTHRESH must be set to 0 if "
			     "tx_rs_thresh is greater than 1. "
			     "(tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx]) {
		ice_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket(NULL,
				 sizeof(struct ci_tx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (!txq) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for "
			     "tx queue structure");
		return -ENOMEM;
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct ice_tx_desc) * ICE_MAX_NUM_DESC_BY_MAC(hw);
	ring_size = RTE_ALIGN(ring_size, ICE_DMA_MEM_ALIGN);
	tz = rte_eth_dma_zone_reserve(dev, "ice_tx_ring", queue_idx,
				      ring_size, ICE_RING_BASE_ALIGN,
				      socket_id);
	if (!tz) {
		ice_tx_queue_release(txq);
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX");
		return -ENOMEM;
	}

	txq->mz = tz;
	txq->nb_tx_desc = nb_desc;
	txq->tx_rs_thresh = tx_rs_thresh;
	txq->tx_free_thresh = tx_free_thresh;
	txq->queue_id = queue_idx;

	txq->reg_idx = vsi->base_queue + queue_idx;
	txq->port_id = dev->data->port_id;
	txq->offloads = offloads;
	txq->ice_vsi = vsi;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	txq->tx_ring_dma = tz->iova;
	txq->ice_tx_ring = tz->addr;

	/* Allocate software ring */
	txq->sw_ring =
		rte_zmalloc_socket(NULL,
				   sizeof(struct ci_tx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (!txq->sw_ring) {
		ice_tx_queue_release(txq);
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW TX ring");
		return -ENOMEM;
	}

	if (vsi->type == ICE_VSI_PF && (offloads & RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP)) {
		if (hw->phy_model != ICE_PHY_E830) {
			ice_tx_queue_release(txq);
			PMD_INIT_LOG(ERR, "Tx Time Queue is not supported on this device");
			return -EINVAL;
		}

		txq->tsq = rte_zmalloc_socket(NULL, sizeof(struct ice_txtime),
				RTE_CACHE_LINE_SIZE, socket_id);
		if (!txq->tsq) {
			ice_tx_queue_release(txq);
			PMD_INIT_LOG(ERR, "Failed to allocate memory for tx time queue structure");
			return -ENOMEM;
		}

		int ret =
			rte_mbuf_dyn_tx_timestamp_register(&txq->tsq->ts_offset,
							 &txq->tsq->ts_flag);
		if (ret) {
			ice_tx_queue_release(txq);
			PMD_INIT_LOG(ERR, "Cannot register Tx mbuf field/flag for timestamp");
			return -EINVAL;
		}
		dev->dev_ops->timesync_enable(dev);

		txq->tsq->nb_ts_desc = ice_calc_ts_ring_count(ICE_VSI_TO_HW(vsi), txq->nb_tx_desc);
		ring_size = sizeof(struct ice_ts_desc) * txq->tsq->nb_ts_desc;
		ring_size = RTE_ALIGN(ring_size, ICE_DMA_MEM_ALIGN);
		const struct rte_memzone *ts_z = rte_eth_dma_zone_reserve(dev, "ice_tstamp_ring",
				queue_idx, ring_size, ICE_RING_BASE_ALIGN, socket_id);
		if (!ts_z) {
			ice_tx_queue_release(txq);
			PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX timestamp");
			return -ENOMEM;
		}
		txq->tsq->ts_mz = ts_z;
		txq->tsq->ice_ts_ring = ts_z->addr;
	} else {
		txq->tsq = NULL;
	}

	ice_reset_tx_queue(txq);
	txq->q_set = true;
	dev->data->tx_queues[queue_idx] = txq;
	ice_set_tx_function_flag(dev, txq);

	return 0;
}

void
ice_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	ice_rx_queue_release(dev->data->rx_queues[qid]);
}

void
ice_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	ice_tx_queue_release(dev->data->tx_queues[qid]);
}

void
ice_tx_queue_release(void *txq)
{
	struct ci_tx_queue *q = (struct ci_tx_queue *)txq;

	if (!q) {
		PMD_DRV_LOG(DEBUG, "Pointer to TX queue is NULL");
		return;
	}

	ci_txq_release_all_mbufs(q, false);
	rte_free(q->sw_ring);
	if (q->tsq) {
		rte_memzone_free(q->tsq->ts_mz);
		rte_free(q->tsq);
	}
	rte_memzone_free(q->mz);
	rte_free(q);
}

void
ice_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		 struct rte_eth_rxq_info *qinfo)
{
	struct ci_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mp;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;

	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.rx_drop_en = rxq->drop_en;
	qinfo->conf.rx_deferred_start = rxq->rx_deferred_start;
}

void
ice_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		 struct rte_eth_txq_info *qinfo)
{
	struct ci_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;

	qinfo->conf.tx_thresh.pthresh = ICE_DEFAULT_TX_PTHRESH;
	qinfo->conf.tx_thresh.hthresh = ICE_DEFAULT_TX_HTHRESH;
	qinfo->conf.tx_thresh.wthresh = ICE_DEFAULT_TX_WTHRESH;

	qinfo->conf.tx_free_thresh = txq->tx_free_thresh;
	qinfo->conf.tx_rs_thresh = txq->tx_rs_thresh;
	qinfo->conf.offloads = txq->offloads;
	qinfo->conf.tx_deferred_start = txq->tx_deferred_start;
}

int
ice_rx_queue_count(void *rx_queue)
{
#define ICE_RXQ_SCAN_INTERVAL 4
	volatile union ci_rx_flex_desc *rxdp;
	struct ci_rx_queue *rxq;
	uint16_t desc = 0;

	rxq = rx_queue;
	rxdp = &rxq->rx_flex_ring[rxq->rx_tail];
	while ((desc < rxq->nb_rx_desc) &&
	       rte_le_to_cpu_16(rxdp->wb.status_error0) &
	       (1 << ICE_RX_FLEX_DESC_STATUS0_DD_S)) {
		/**
		 * Check the DD bit of a rx descriptor of each 4 in a group,
		 * to avoid checking too frequently and downgrading performance
		 * too much.
		 */
		desc += ICE_RXQ_SCAN_INTERVAL;
		rxdp += ICE_RXQ_SCAN_INTERVAL;
		if (rxq->rx_tail + desc >= rxq->nb_rx_desc)
			rxdp = &rxq->rx_flex_ring[rxq->rx_tail + desc - rxq->nb_rx_desc];
	}

	return desc;
}

#define ICE_RX_FLEX_ERR0_BITS	\
	((1 << ICE_RX_FLEX_DESC_STATUS0_HBO_S) |	\
	 (1 << ICE_RX_FLEX_DESC_STATUS0_XSUM_IPE_S) |	\
	 (1 << ICE_RX_FLEX_DESC_STATUS0_XSUM_L4E_S) |	\
	 (1 << ICE_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S) |	\
	 (1 << ICE_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S) |	\
	 (1 << ICE_RX_FLEX_DESC_STATUS0_RXE_S))

/* Rx L3/L4 checksum */
static inline uint64_t
ice_rxd_error_to_pkt_flags(uint16_t stat_err0)
{
	uint64_t flags = 0;

	/* check if HW has decoded the packet and checksum */
	if (unlikely(!(stat_err0 & (1 << ICE_RX_FLEX_DESC_STATUS0_L3L4P_S))))
		return 0;

	if (likely(!(stat_err0 & ICE_RX_FLEX_ERR0_BITS))) {
		flags |= (RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			  RTE_MBUF_F_RX_L4_CKSUM_GOOD |
			  RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD);
		return flags;
	}

	if (unlikely(stat_err0 & (1 << ICE_RX_FLEX_DESC_STATUS0_XSUM_IPE_S)))
		flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

	if (unlikely(stat_err0 & (1 << ICE_RX_FLEX_DESC_STATUS0_XSUM_L4E_S)))
		flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;

	if (unlikely(stat_err0 & (1 << ICE_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S)))
		flags |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;

	if (unlikely(stat_err0 & (1 << ICE_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S)))
		flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD;

	return flags;
}

static inline void
ice_rxd_to_vlan_tci(struct rte_mbuf *mb, volatile union ci_rx_flex_desc *rxdp)
{
	if (rte_le_to_cpu_16(rxdp->wb.status_error0) &
	    (1 << ICE_RX_FLEX_DESC_STATUS0_L2TAG1P_S)) {
		mb->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		mb->vlan_tci =
			rte_le_to_cpu_16(rxdp->wb.l2tag1);
		PMD_RX_LOG(DEBUG, "Descriptor l2tag1: %u",
			   rte_le_to_cpu_16(rxdp->wb.l2tag1));
	} else {
		mb->vlan_tci = 0;
	}

#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	if (rte_le_to_cpu_16(rxdp->wb.status_error1) &
	    (1 << ICE_RX_FLEX_DESC_STATUS1_L2TAG2P_S)) {
		if ((mb->ol_flags & RTE_MBUF_F_RX_VLAN_STRIPPED) == 0) {
			mb->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		} else {
			/* if two tags, move Tag1 to outer tag field */
			mb->ol_flags |= RTE_MBUF_F_RX_QINQ_STRIPPED | RTE_MBUF_F_RX_QINQ;
			mb->vlan_tci_outer = mb->vlan_tci;
		}
		mb->vlan_tci = rte_le_to_cpu_16(rxdp->wb.l2tag2_2nd);
		PMD_RX_LOG(DEBUG, "Descriptor l2tag2_1: %u, l2tag2_2: %u",
			   rte_le_to_cpu_16(rxdp->wb.l2tag2_1st),
			   rte_le_to_cpu_16(rxdp->wb.l2tag2_2nd));
	} else {
		mb->vlan_tci_outer = 0;
	}
#endif
	PMD_RX_LOG(DEBUG, "Mbuf vlan_tci: %u, vlan_tci_outer: %u",
		   mb->vlan_tci, mb->vlan_tci_outer);
}

#define ICE_LOOK_AHEAD 8
#if (ICE_LOOK_AHEAD != 8)
#error "PMD ICE: ICE_LOOK_AHEAD must be 8\n"
#endif

#define ICE_PTP_TS_VALID 0x1

static inline int
ice_rx_scan_hw_ring(struct ci_rx_queue *rxq)
{
	volatile union ci_rx_flex_desc *rxdp;
	struct ci_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t stat_err0;
	uint16_t pkt_len, hdr_len;
	int32_t s[ICE_LOOK_AHEAD], nb_dd;
	int32_t i, j, nb_rx = 0;
	uint64_t pkt_flags = 0;
	uint32_t *ptype_tbl = rxq->ice_vsi->adapter->ptype_tbl;
#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	bool is_tsinit = false;
	uint64_t ts_ns;
	struct ice_vsi *vsi = rxq->ice_vsi;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_adapter *ad = rxq->ice_vsi->adapter;
#endif
	rxdp = &rxq->rx_flex_ring[rxq->rx_tail];
	rxep = &rxq->sw_ring[rxq->rx_tail];

	stat_err0 = rte_le_to_cpu_16(rxdp->wb.status_error0);

	/* Make sure there is at least 1 packet to receive */
	if (!(stat_err0 & (1 << ICE_RX_FLEX_DESC_STATUS0_DD_S)))
		return 0;

#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
		uint64_t sw_cur_time = rte_get_timer_cycles() / (rte_get_timer_hz() / 1000);

		if (unlikely(sw_cur_time - rxq->hw_time_update > 4))
			is_tsinit = 1;
	}
#endif

	/**
	 * Scan LOOK_AHEAD descriptors at a time to determine which
	 * descriptors reference packets that are ready to be received.
	 */
	for (i = 0; i < ICE_RX_MAX_BURST; i += ICE_LOOK_AHEAD,
	     rxdp += ICE_LOOK_AHEAD, rxep += ICE_LOOK_AHEAD) {
		/* Read desc statuses backwards to avoid race condition */
		for (j = ICE_LOOK_AHEAD - 1; j >= 0; j--)
			s[j] = rte_le_to_cpu_16(rxdp[j].wb.status_error0);

		rte_smp_rmb();

		/* Compute how many status bits were set */
		for (j = 0, nb_dd = 0; j < ICE_LOOK_AHEAD; j++)
			nb_dd += s[j] & (1 << ICE_RX_FLEX_DESC_STATUS0_DD_S);

		nb_rx += nb_dd;

		/* Translate descriptor info to mbuf parameters */
		for (j = 0; j < nb_dd; j++) {
			mb = rxep[j].mbuf;
			pkt_len = (rte_le_to_cpu_16(rxdp[j].wb.pkt_len) &
				   ICE_RX_FLX_DESC_PKT_LEN_M) - rxq->crc_len;
			mb->data_len = pkt_len;
			mb->pkt_len = pkt_len;

			if (!(rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
				pkt_len = (rte_le_to_cpu_16(rxdp[j].wb.pkt_len) &
					ICE_RX_FLX_DESC_PKT_LEN_M) - rxq->crc_len;
				mb->data_len = pkt_len;
				mb->pkt_len = pkt_len;
			} else {
				mb->nb_segs = (uint16_t)(mb->nb_segs + mb->next->nb_segs);
				mb->next->next = NULL;
				hdr_len = rte_le_to_cpu_16(rxdp[j].wb.hdr_len_sph_flex_flags1) &
						ICE_RX_FLEX_DESC_HEADER_LEN_M;
				pkt_len = (rte_le_to_cpu_16(rxdp[j].wb.pkt_len) &
					ICE_RX_FLX_DESC_PKT_LEN_M) - rxq->crc_len;
				mb->data_len = hdr_len;
				mb->pkt_len = hdr_len + pkt_len;
				mb->next->data_len = pkt_len;
#ifdef RTE_ETHDEV_DEBUG_RX
				rte_pktmbuf_dump(stdout, mb, rte_pktmbuf_pkt_len(mb));
#endif
			}

			mb->ol_flags = 0;
			stat_err0 = rte_le_to_cpu_16(rxdp[j].wb.status_error0);
			pkt_flags = ice_rxd_error_to_pkt_flags(stat_err0);
			mb->packet_type = ptype_tbl[ICE_RX_FLEX_DESC_PTYPE_M &
				rte_le_to_cpu_16(rxdp[j].wb.ptype_flex_flags0)];
			ice_rxd_to_vlan_tci(mb, &rxdp[j]);
			rxd_to_pkt_fields_ops[rxq->rxdid](rxq, mb, &rxdp[j]);
#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
			if (rxq->ts_flag > 0 &&
			    (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)) {
				rxq->time_high =
				rte_le_to_cpu_32(rxdp[j].wb.flex_ts.ts_high);
				if (unlikely(is_tsinit)) {
					ts_ns = ice_tstamp_convert_32b_64b(hw, ad, 1,
									   rxq->time_high);
					rxq->hw_time_low = (uint32_t)ts_ns;
					rxq->hw_time_high = (uint32_t)(ts_ns >> 32);
					is_tsinit = false;
				} else {
					if (rxq->time_high < rxq->hw_time_low)
						rxq->hw_time_high += 1;
					ts_ns = (uint64_t)rxq->hw_time_high << 32 | rxq->time_high;
					rxq->hw_time_low = rxq->time_high;
				}
				rxq->hw_time_update = rte_get_timer_cycles() /
						     (rte_get_timer_hz() / 1000);
				*RTE_MBUF_DYNFIELD(mb,
						   rxq->ts_offset,
						   rte_mbuf_timestamp_t *) = ts_ns;
				pkt_flags |= rxq->ts_flag;
			}

			if (ad->ptp_ena && ((mb->packet_type &
			    RTE_PTYPE_L2_MASK) == RTE_PTYPE_L2_ETHER_TIMESYNC)) {
				rxq->time_high =
				   rte_le_to_cpu_32(rxdp[j].wb.flex_ts.ts_high);
				mb->timesync = rxq->queue_id;
				pkt_flags |= RTE_MBUF_F_RX_IEEE1588_PTP;
				if (rxdp[j].wb.time_stamp_low &
				    ICE_PTP_TS_VALID)
					pkt_flags |=
						RTE_MBUF_F_RX_IEEE1588_TMST;
			}
#endif
			mb->ol_flags |= pkt_flags;
		}

		for (j = 0; j < ICE_LOOK_AHEAD; j++)
			rxq->rx_stage[i + j] = rxep[j].mbuf;

		if (nb_dd != ICE_LOOK_AHEAD)
			break;
	}

	/* Clear software ring entries */
	for (i = 0; i < nb_rx; i++)
		rxq->sw_ring[rxq->rx_tail + i].mbuf = NULL;

	PMD_RX_LOG(DEBUG, "ice_rx_scan_hw_ring: "
		   "port_id=%u, queue_id=%u, nb_rx=%d",
		   rxq->port_id, rxq->queue_id, nb_rx);

	return nb_rx;
}

static inline uint16_t
ice_rx_fill_from_stage(struct ci_rx_queue *rxq,
		       struct rte_mbuf **rx_pkts,
		       uint16_t nb_pkts)
{
	uint16_t i;
	struct rte_mbuf **stage = &rxq->rx_stage[rxq->rx_next_avail];

	nb_pkts = (uint16_t)RTE_MIN(nb_pkts, rxq->rx_nb_avail);

	for (i = 0; i < nb_pkts; i++)
		rx_pkts[i] = stage[i];

	rxq->rx_nb_avail = (uint16_t)(rxq->rx_nb_avail - nb_pkts);
	rxq->rx_next_avail = (uint16_t)(rxq->rx_next_avail + nb_pkts);

	return nb_pkts;
}

static inline int
ice_rx_alloc_bufs(struct ci_rx_queue *rxq)
{
	volatile union ci_rx_flex_desc *rxdp;
	struct ci_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t alloc_idx, i;
	uint64_t dma_addr;
	int diag, diag_pay;
	uint64_t pay_addr;

	/* Allocate buffers in bulk */
	alloc_idx = (uint16_t)(rxq->rx_free_trigger -
			       (rxq->rx_free_thresh - 1));
	rxep = &rxq->sw_ring[alloc_idx];
	diag = rte_mbuf_raw_alloc_bulk(rxq->mp, (void *)rxep,
				    rxq->rx_free_thresh);
	if (unlikely(diag != 0)) {
		PMD_RX_LOG(ERR, "Failed to get mbufs in bulk");
		return -ENOMEM;
	}

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) {
		diag_pay = rte_mbuf_raw_alloc_bulk(rxq->rxseg[1].mp,
				(void *)rxq->sw_split_buf, rxq->rx_free_thresh);
		if (unlikely(diag_pay != 0)) {
			rte_mbuf_raw_free_bulk(rxq->mp, (void *)rxep,
				    rxq->rx_free_thresh);
			PMD_RX_LOG(ERR, "Failed to get payload mbufs in bulk");
			return -ENOMEM;
		}
	}

	rxdp = &rxq->rx_flex_ring[alloc_idx];
	for (i = 0; i < rxq->rx_free_thresh; i++) {
		if (likely(i < (rxq->rx_free_thresh - 1)))
			/* Prefetch next mbuf */
			rte_prefetch0(rxep[i + 1].mbuf);

		mb = rxep[i].mbuf;
		rte_mbuf_refcnt_set(mb, 1);
		mb->data_off = RTE_PKTMBUF_HEADROOM;
		mb->nb_segs = 1;
		mb->port = rxq->port_id;
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mb));

		if (!(rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
			mb->next = NULL;
			rxdp[i].read.hdr_addr = 0;
			rxdp[i].read.pkt_addr = dma_addr;
		} else {
			mb->next = rxq->sw_split_buf[i].mbuf;
			pay_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mb->next));
			rxdp[i].read.hdr_addr = dma_addr;
			rxdp[i].read.pkt_addr = pay_addr;
		}
	}

	/* Update Rx tail register */
	ICE_PCI_REG_WRITE(rxq->qrx_tail, rxq->rx_free_trigger);

	rxq->rx_free_trigger =
		(uint16_t)(rxq->rx_free_trigger + rxq->rx_free_thresh);
	if (rxq->rx_free_trigger >= rxq->nb_rx_desc)
		rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);

	return 0;
}

static inline uint16_t
rx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct ci_rx_queue *rxq = (struct ci_rx_queue *)rx_queue;
	uint16_t nb_rx = 0;

	if (!nb_pkts)
		return 0;

	if (rxq->rx_nb_avail)
		return ice_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	nb_rx = (uint16_t)ice_rx_scan_hw_ring(rxq);
	rxq->rx_next_avail = 0;
	rxq->rx_nb_avail = nb_rx;
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_rx);

	if (rxq->rx_tail > rxq->rx_free_trigger) {
		if (ice_rx_alloc_bufs(rxq) != 0) {
			uint16_t i, j;

			rxq->ice_vsi->adapter->pf.dev_data->rx_mbuf_alloc_failed +=
				rxq->rx_free_thresh;
			PMD_RX_LOG(DEBUG, "Rx mbuf alloc failed for "
				   "port_id=%u, queue_id=%u",
				   rxq->port_id, rxq->queue_id);
			rxq->rx_nb_avail = 0;
			rxq->rx_tail = (uint16_t)(rxq->rx_tail - nb_rx);
			for (i = 0, j = rxq->rx_tail; i < nb_rx; i++, j++)
				rxq->sw_ring[j].mbuf = rxq->rx_stage[i];

			return 0;
		}
	}

	if (rxq->rx_tail >= rxq->nb_rx_desc)
		rxq->rx_tail = 0;

	if (rxq->rx_nb_avail)
		return ice_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	return 0;
}

static uint16_t
ice_recv_pkts_bulk_alloc(void *rx_queue,
			 struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	uint16_t nb_rx = 0;
	uint16_t n;
	uint16_t count;

	if (unlikely(nb_pkts == 0))
		return nb_rx;

	if (likely(nb_pkts <= ICE_RX_MAX_BURST))
		return rx_recv_pkts(rx_queue, rx_pkts, nb_pkts);

	while (nb_pkts) {
		n = RTE_MIN(nb_pkts, ICE_RX_MAX_BURST);
		count = rx_recv_pkts(rx_queue, &rx_pkts[nb_rx], n);
		nb_rx = (uint16_t)(nb_rx + count);
		nb_pkts = (uint16_t)(nb_pkts - count);
		if (count < n)
			break;
	}

	return nb_rx;
}

static uint16_t
ice_recv_scattered_pkts(void *rx_queue,
			struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct ci_rx_queue *rxq = rx_queue;
	volatile union ci_rx_flex_desc *rx_ring = rxq->rx_flex_ring;
	volatile union ci_rx_flex_desc *rxdp;
	union ci_rx_flex_desc rxd;
	struct ci_rx_entry *sw_ring = rxq->sw_ring;
	struct ci_rx_entry *rxe;
	struct rte_mbuf *first_seg = rxq->pkt_first_seg;
	struct rte_mbuf *last_seg = rxq->pkt_last_seg;
	struct rte_mbuf *nmb; /* new allocated mbuf */
	struct rte_mbuf *rxm; /* pointer to store old mbuf in SW ring */
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = 0;
	uint16_t rx_packet_len;
	uint16_t rx_stat_err0;
	uint64_t dma_addr;
	uint64_t pkt_flags;
	uint32_t *ptype_tbl = rxq->ice_vsi->adapter->ptype_tbl;
#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	bool is_tsinit = false;
	uint64_t ts_ns;
	struct ice_vsi *vsi = rxq->ice_vsi;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_adapter *ad = rxq->ice_vsi->adapter;

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
		uint64_t sw_cur_time = rte_get_timer_cycles() / (rte_get_timer_hz() / 1000);

		if (unlikely(sw_cur_time - rxq->hw_time_update > 4))
			is_tsinit = true;
	}
#endif

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		rx_stat_err0 = rte_le_to_cpu_16(rxdp->wb.status_error0);

		/* Check the DD bit first */
		if (!(rx_stat_err0 & (1 << ICE_RX_FLEX_DESC_STATUS0_DD_S)))
			break;

		/* allocate mbuf */
		nmb = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!nmb)) {
			rxq->ice_vsi->adapter->pf.dev_data->rx_mbuf_alloc_failed++;
			break;
		}
		rxd = *rxdp; /* copy descriptor in ring to temp variable*/

		nb_hold++;
		rxe = &sw_ring[rx_id]; /* get corresponding mbuf in SW ring */
		rx_id++;
		if (unlikely(rx_id == rxq->nb_rx_desc))
			rx_id = 0;

		/* Prefetch next mbuf */
		rte_prefetch0(sw_ring[rx_id].mbuf);

		/**
		 * When next RX descriptor is on a cache line boundary,
		 * prefetch the next 4 RX descriptors and next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));

		/* Set data buffer address and data length of the mbuf */
		rxdp->read.hdr_addr = 0;
		rxdp->read.pkt_addr = dma_addr;
		rx_packet_len = rte_le_to_cpu_16(rxd.wb.pkt_len) &
				ICE_RX_FLX_DESC_PKT_LEN_M;
		rxm->data_len = rx_packet_len;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;

		/**
		 * If this is the first buffer of the received packet, set the
		 * pointer to the first mbuf of the packet and initialize its
		 * context. Otherwise, update the total length and the number
		 * of segments of the current scattered packet, and update the
		 * pointer to the last mbuf of the current packet.
		 */
		if (!first_seg) {
			first_seg = rxm;
			first_seg->nb_segs = 1;
			first_seg->pkt_len = rx_packet_len;
		} else {
			first_seg->pkt_len =
				(uint16_t)(first_seg->pkt_len +
					   rx_packet_len);
			first_seg->nb_segs++;
			last_seg->next = rxm;
		}

		/**
		 * If this is not the last buffer of the received packet,
		 * update the pointer to the last mbuf of the current scattered
		 * packet and continue to parse the RX ring.
		 */
		if (!(rx_stat_err0 & (1 << ICE_RX_FLEX_DESC_STATUS0_EOF_S))) {
			last_seg = rxm;
			continue;
		}

		/**
		 * This is the last buffer of the received packet. If the CRC
		 * is not stripped by the hardware:
		 *  - Subtract the CRC length from the total packet length.
		 *  - If the last buffer only contains the whole CRC or a part
		 *  of it, free the mbuf associated to the last buffer. If part
		 *  of the CRC is also contained in the previous mbuf, subtract
		 *  the length of that CRC part from the data length of the
		 *  previous mbuf.
		 */
		rxm->next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt_len -= RTE_ETHER_CRC_LEN;
			if (rx_packet_len <= RTE_ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(rxm);
				first_seg->nb_segs--;
				last_seg->data_len =
					(uint16_t)(last_seg->data_len -
					(RTE_ETHER_CRC_LEN - rx_packet_len));
				last_seg->next = NULL;
			} else
				rxm->data_len = (uint16_t)(rx_packet_len -
							   RTE_ETHER_CRC_LEN);
		} else if (rx_packet_len == 0) {
			rte_pktmbuf_free_seg(rxm);
			first_seg->nb_segs--;
			last_seg->next = NULL;
		}

		first_seg->port = rxq->port_id;
		first_seg->ol_flags = 0;
		first_seg->packet_type = ptype_tbl[ICE_RX_FLEX_DESC_PTYPE_M &
			rte_le_to_cpu_16(rxd.wb.ptype_flex_flags0)];
		ice_rxd_to_vlan_tci(first_seg, &rxd);
		rxd_to_pkt_fields_ops[rxq->rxdid](rxq, first_seg, &rxd);
		pkt_flags = ice_rxd_error_to_pkt_flags(rx_stat_err0);
#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
		if (rxq->ts_flag > 0 &&
		    (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)) {
			rxq->time_high =
			   rte_le_to_cpu_32(rxd.wb.flex_ts.ts_high);
			if (unlikely(is_tsinit)) {
				ts_ns = ice_tstamp_convert_32b_64b(hw, ad, 1, rxq->time_high);
				rxq->hw_time_low = (uint32_t)ts_ns;
				rxq->hw_time_high = (uint32_t)(ts_ns >> 32);
				is_tsinit = false;
			} else {
				if (rxq->time_high < rxq->hw_time_low)
					rxq->hw_time_high += 1;
				ts_ns = (uint64_t)rxq->hw_time_high << 32 | rxq->time_high;
				rxq->hw_time_low = rxq->time_high;
			}
			rxq->hw_time_update = rte_get_timer_cycles() /
					     (rte_get_timer_hz() / 1000);
			*RTE_MBUF_DYNFIELD(first_seg,
					   (rxq->ts_offset),
					   rte_mbuf_timestamp_t *) = ts_ns;
			pkt_flags |= rxq->ts_flag;
		}

		if (ad->ptp_ena && ((first_seg->packet_type & RTE_PTYPE_L2_MASK)
		    == RTE_PTYPE_L2_ETHER_TIMESYNC)) {
			rxq->time_high =
			   rte_le_to_cpu_32(rxd.wb.flex_ts.ts_high);
			first_seg->timesync = rxq->queue_id;
			pkt_flags |= RTE_MBUF_F_RX_IEEE1588_PTP;
		}
#endif
		first_seg->ol_flags |= pkt_flags;
		/* Prefetch data of first segment, if configured to do so. */
		rte_prefetch0(RTE_PTR_ADD(first_seg->buf_addr,
					  first_seg->data_off));
		rx_pkts[nb_rx++] = first_seg;
		first_seg = NULL;
	}

	/* Record index of the next RX descriptor to probe. */
	rxq->rx_tail = rx_id;
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	/**
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register. Update the RDT with the value of the last processed RX
	 * descriptor minus 1, to guarantee that the RDT register is never
	 * equal to the RDH register, which creates a "full" ring situation
	 * from the hardware point of view.
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		rx_id = (uint16_t)(rx_id == 0 ?
				   (rxq->nb_rx_desc - 1) : (rx_id - 1));
		/* write TAIL register */
		ICE_PCI_REG_WC_WRITE(rxq->qrx_tail, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;

	/* return received packet in the burst */
	return nb_rx;
}

const uint32_t *
ice_dev_supported_ptypes_get(struct rte_eth_dev *dev, size_t *no_of_elements)
{
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	const uint32_t *ptypes;

	static const uint32_t ptypes_os[] = {
		/* refers to ice_get_default_pkt_type() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_TIMESYNC,
		RTE_PTYPE_L2_ETHER_LLDP,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_TUNNEL_GRENAT,
		RTE_PTYPE_TUNNEL_IP,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_ICMP,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
	};

	static const uint32_t ptypes_comms[] = {
		/* refers to ice_get_default_pkt_type() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_TIMESYNC,
		RTE_PTYPE_L2_ETHER_LLDP,
		RTE_PTYPE_L2_ETHER_ARP,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_TUNNEL_GRENAT,
		RTE_PTYPE_TUNNEL_IP,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_ICMP,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_TUNNEL_GTPC,
		RTE_PTYPE_TUNNEL_GTPU,
		RTE_PTYPE_L2_ETHER_PPPOE,
	};

	if (ad->active_pkg_type == ICE_PKG_TYPE_COMMS) {
		*no_of_elements = RTE_DIM(ptypes_comms);
		ptypes = ptypes_comms;
	} else {
		*no_of_elements = RTE_DIM(ptypes_os);
		ptypes = ptypes_os;
	}

	if (dev->rx_pkt_burst == ice_recv_pkts ||
	    dev->rx_pkt_burst == ice_recv_pkts_bulk_alloc ||
	    dev->rx_pkt_burst == ice_recv_scattered_pkts)
		return ptypes;

#ifdef RTE_ARCH_X86
	if (dev->rx_pkt_burst == ice_recv_pkts_vec ||
	    dev->rx_pkt_burst == ice_recv_scattered_pkts_vec ||
#ifdef CC_AVX512_SUPPORT
	    dev->rx_pkt_burst == ice_recv_pkts_vec_avx512 ||
	    dev->rx_pkt_burst == ice_recv_pkts_vec_avx512_offload ||
	    dev->rx_pkt_burst == ice_recv_scattered_pkts_vec_avx512 ||
	    dev->rx_pkt_burst == ice_recv_scattered_pkts_vec_avx512_offload ||
#endif
	    dev->rx_pkt_burst == ice_recv_pkts_vec_avx2 ||
	    dev->rx_pkt_burst == ice_recv_pkts_vec_avx2_offload ||
	    dev->rx_pkt_burst == ice_recv_scattered_pkts_vec_avx2 ||
	    dev->rx_pkt_burst == ice_recv_scattered_pkts_vec_avx2_offload)
		return ptypes;
#endif

	return NULL;
}

int
ice_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	volatile union ci_rx_flex_desc *rxdp;
	struct ci_rx_queue *rxq = rx_queue;
	uint32_t desc;

	if (unlikely(offset >= rxq->nb_rx_desc))
		return -EINVAL;

	if (offset >= rxq->nb_rx_desc - rxq->nb_rx_hold)
		return RTE_ETH_RX_DESC_UNAVAIL;

	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	rxdp = &rxq->rx_flex_ring[desc];
	if (rte_le_to_cpu_16(rxdp->wb.status_error0) &
	    (1 << ICE_RX_FLEX_DESC_STATUS0_DD_S))
		return RTE_ETH_RX_DESC_DONE;

	return RTE_ETH_RX_DESC_AVAIL;
}

int
ice_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct ci_tx_queue *txq = tx_queue;
	volatile uint64_t *status;
	uint64_t mask, expect;
	uint32_t desc;

	if (unlikely(offset >= txq->nb_tx_desc))
		return -EINVAL;

	desc = txq->tx_tail + offset;
	/* go to next desc that has the RS bit */
	desc = ((desc + txq->tx_rs_thresh - 1) / txq->tx_rs_thresh) *
		txq->tx_rs_thresh;
	if (desc >= txq->nb_tx_desc) {
		desc -= txq->nb_tx_desc;
		if (desc >= txq->nb_tx_desc)
			desc -= txq->nb_tx_desc;
	}

	status = &txq->ice_tx_ring[desc].cmd_type_offset_bsz;
	mask = rte_cpu_to_le_64(ICE_TXD_QW1_DTYPE_M);
	expect = rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DESC_DONE <<
				  ICE_TXD_QW1_DTYPE_S);
	if ((*status & mask) == expect)
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}

void
ice_free_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!dev->data->rx_queues[i])
			continue;
		ice_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (!dev->data->tx_queues[i])
			continue;
		ice_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

#define ICE_FDIR_NUM_TX_DESC  ICE_MIN_RING_DESC
#define ICE_FDIR_NUM_RX_DESC  ICE_MIN_RING_DESC

int
ice_fdir_setup_tx_resources(struct ice_pf *pf)
{
	struct ci_tx_queue *txq;
	const struct rte_memzone *tz = NULL;
	uint32_t ring_size;
	struct rte_eth_dev *dev;

	if (!pf) {
		PMD_DRV_LOG(ERR, "PF is not available");
		return -EINVAL;
	}

	dev = &rte_eth_devices[pf->adapter->pf.dev_data->port_id];

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("ice fdir tx queue",
				 sizeof(struct ci_tx_queue),
				 RTE_CACHE_LINE_SIZE,
				 SOCKET_ID_ANY);
	if (!txq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for "
			    "tx queue structure.");
		return -ENOMEM;
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct ice_tx_desc) * ICE_FDIR_NUM_TX_DESC;
	ring_size = RTE_ALIGN(ring_size, ICE_DMA_MEM_ALIGN);

	tz = rte_eth_dma_zone_reserve(dev, "fdir_tx_ring",
				      ICE_FDIR_QUEUE_ID, ring_size,
				      ICE_RING_BASE_ALIGN, SOCKET_ID_ANY);
	if (!tz) {
		ice_tx_queue_release(txq);
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for TX.");
		return -ENOMEM;
	}

	txq->mz = tz;
	txq->nb_tx_desc = ICE_FDIR_NUM_TX_DESC;
	txq->queue_id = ICE_FDIR_QUEUE_ID;
	txq->reg_idx = pf->fdir.fdir_vsi->base_queue;
	txq->ice_vsi = pf->fdir.fdir_vsi;

	txq->tx_ring_dma = tz->iova;
	txq->ice_tx_ring = (struct ice_tx_desc *)tz->addr;
	/*
	 * don't need to allocate software ring and reset for the fdir
	 * program queue just set the queue has been configured.
	 */
	txq->q_set = true;
	pf->fdir.txq = txq;


	return ICE_SUCCESS;
}

int
ice_fdir_setup_rx_resources(struct ice_pf *pf)
{
	struct ci_rx_queue *rxq;
	const struct rte_memzone *rz = NULL;
	uint32_t ring_size;
	struct rte_eth_dev *dev;

	if (!pf) {
		PMD_DRV_LOG(ERR, "PF is not available");
		return -EINVAL;
	}

	dev = &rte_eth_devices[pf->adapter->pf.dev_data->port_id];

	/* Allocate the RX queue data structure. */
	rxq = rte_zmalloc_socket("ice fdir rx queue",
				 sizeof(struct ci_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 SOCKET_ID_ANY);
	if (!rxq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for "
			    "rx queue structure.");
		return -ENOMEM;
	}

	/* Allocate RX hardware ring descriptors. */
	ring_size = sizeof(union ice_32byte_rx_desc) * ICE_FDIR_NUM_RX_DESC;
	ring_size = RTE_ALIGN(ring_size, ICE_DMA_MEM_ALIGN);

	rz = rte_eth_dma_zone_reserve(dev, "fdir_rx_ring",
				      ICE_FDIR_QUEUE_ID, ring_size,
				      ICE_RING_BASE_ALIGN, SOCKET_ID_ANY);
	if (!rz) {
		ice_rx_queue_release(rxq);
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for RX.");
		return -ENOMEM;
	}

	rxq->mz = rz;
	rxq->nb_rx_desc = ICE_FDIR_NUM_RX_DESC;
	rxq->queue_id = ICE_FDIR_QUEUE_ID;
	rxq->reg_idx = pf->fdir.fdir_vsi->base_queue;
	rxq->ice_vsi = pf->fdir.fdir_vsi;

	rxq->rx_ring_phys_addr = rz->iova;
	memset(rz->addr, 0, ICE_FDIR_NUM_RX_DESC *
	       sizeof(union ice_32byte_rx_desc));
	rxq->rx_flex_ring = (union ci_rx_flex_desc *)rz->addr;

	/*
	 * Don't need to allocate software ring and reset for the fdir
	 * rx queue, just set the queue has been configured.
	 */
	rxq->q_set = true;
	pf->fdir.rxq = rxq;

	rxq->rx_rel_mbufs = _ice_rx_queue_release_mbufs;

	return ICE_SUCCESS;
}

uint16_t
ice_recv_pkts(void *rx_queue,
	      struct rte_mbuf **rx_pkts,
	      uint16_t nb_pkts)
{
	struct ci_rx_queue *rxq = rx_queue;
	volatile union ci_rx_flex_desc *rx_ring = rxq->rx_flex_ring;
	volatile union ci_rx_flex_desc *rxdp;
	union ci_rx_flex_desc rxd;
	struct ci_rx_entry *sw_ring = rxq->sw_ring;
	struct ci_rx_entry *rxe;
	struct rte_mbuf *nmb; /* new allocated mbuf */
	struct rte_mbuf *nmb_pay; /* new allocated payload mbuf */
	struct rte_mbuf *rxm; /* pointer to store old mbuf in SW ring */
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = 0;
	uint16_t rx_packet_len;
	uint16_t rx_header_len;
	uint16_t rx_stat_err0;
	uint64_t dma_addr;
	uint64_t pkt_flags;
	uint32_t *ptype_tbl = rxq->ice_vsi->adapter->ptype_tbl;
#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	bool is_tsinit = false;
	uint64_t ts_ns;
	struct ice_vsi *vsi = rxq->ice_vsi;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_adapter *ad = rxq->ice_vsi->adapter;

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
		uint64_t sw_cur_time = rte_get_timer_cycles() / (rte_get_timer_hz() / 1000);

		if (unlikely(sw_cur_time - rxq->hw_time_update > 4))
			is_tsinit = 1;
	}
#endif

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		rx_stat_err0 = rte_le_to_cpu_16(rxdp->wb.status_error0);

		/* Check the DD bit first */
		if (!(rx_stat_err0 & (1 << ICE_RX_FLEX_DESC_STATUS0_DD_S)))
			break;

		/* allocate header mbuf */
		nmb = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!nmb)) {
			rxq->ice_vsi->adapter->pf.dev_data->rx_mbuf_alloc_failed++;
			break;
		}

		rxd = *rxdp; /* copy descriptor in ring to temp variable*/

		nb_hold++;
		rxe = &sw_ring[rx_id]; /* get corresponding mbuf in SW ring */
		rx_id++;
		if (unlikely(rx_id == rxq->nb_rx_desc))
			rx_id = 0;
		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));

		if (!(rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
			/**
			 * fill the read format of descriptor with physic address in
			 * new allocated mbuf: nmb
			 */
			rxdp->read.hdr_addr = 0;
			rxdp->read.pkt_addr = dma_addr;
		} else {
			/* allocate payload mbuf */
			nmb_pay = rte_mbuf_raw_alloc(rxq->rxseg[1].mp);
			if (unlikely(!nmb_pay)) {
				rxq->ice_vsi->adapter->pf.dev_data->rx_mbuf_alloc_failed++;
				rxe->mbuf = NULL;
				nb_hold--;
				if (unlikely(rx_id == 0))
					rx_id = rxq->nb_rx_desc;

				rx_id--;
				rte_pktmbuf_free(nmb);
				break;
			}

			nmb->next = nmb_pay;
			nmb_pay->next = NULL;

			/**
			 * fill the read format of descriptor with physic address in
			 * new allocated mbuf: nmb
			 */
			rxdp->read.hdr_addr = dma_addr;
			rxdp->read.pkt_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb_pay));
		}

		/* fill old mbuf with received descriptor: rxd */
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rte_prefetch0(RTE_PTR_ADD(rxm->buf_addr, RTE_PKTMBUF_HEADROOM));
		if (!(rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
			rxm->nb_segs = 1;
			rxm->next = NULL;
			/* calculate rx_packet_len of the received pkt */
			rx_packet_len = (rte_le_to_cpu_16(rxd.wb.pkt_len) &
					ICE_RX_FLX_DESC_PKT_LEN_M) - rxq->crc_len;
			rxm->data_len = rx_packet_len;
			rxm->pkt_len = rx_packet_len;
		} else {
			rxm->nb_segs = (uint16_t)(rxm->nb_segs + rxm->next->nb_segs);
			rxm->next->next = NULL;
			/* calculate rx_packet_len of the received pkt */
			rx_header_len = rte_le_to_cpu_16(rxd.wb.hdr_len_sph_flex_flags1) &
					ICE_RX_FLEX_DESC_HEADER_LEN_M;
			rx_packet_len = (rte_le_to_cpu_16(rxd.wb.pkt_len) &
					ICE_RX_FLX_DESC_PKT_LEN_M) - rxq->crc_len;
			rxm->data_len = rx_header_len;
			rxm->pkt_len = rx_header_len + rx_packet_len;
			rxm->next->data_len = rx_packet_len;

#ifdef RTE_ETHDEV_DEBUG_RX
			rte_pktmbuf_dump(stdout, rxm, rte_pktmbuf_pkt_len(rxm));
#endif
		}

		rxm->port = rxq->port_id;
		rxm->packet_type = ptype_tbl[ICE_RX_FLEX_DESC_PTYPE_M &
			rte_le_to_cpu_16(rxd.wb.ptype_flex_flags0)];
		ice_rxd_to_vlan_tci(rxm, &rxd);
		rxd_to_pkt_fields_ops[rxq->rxdid](rxq, rxm, &rxd);
		pkt_flags = ice_rxd_error_to_pkt_flags(rx_stat_err0);
#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
		if (rxq->ts_flag > 0 &&
		    (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)) {
			rxq->time_high =
			   rte_le_to_cpu_32(rxd.wb.flex_ts.ts_high);
			if (unlikely(is_tsinit)) {
				ts_ns = ice_tstamp_convert_32b_64b(hw, ad, 1, rxq->time_high);
				rxq->hw_time_low = (uint32_t)ts_ns;
				rxq->hw_time_high = (uint32_t)(ts_ns >> 32);
				is_tsinit = false;
			} else {
				if (rxq->time_high < rxq->hw_time_low)
					rxq->hw_time_high += 1;
				ts_ns = (uint64_t)rxq->hw_time_high << 32 | rxq->time_high;
				rxq->hw_time_low = rxq->time_high;
			}
			rxq->hw_time_update = rte_get_timer_cycles() /
					     (rte_get_timer_hz() / 1000);
			*RTE_MBUF_DYNFIELD(rxm,
					   (rxq->ts_offset),
					   rte_mbuf_timestamp_t *) = ts_ns;
			pkt_flags |= rxq->ts_flag;
		}

		if (ad->ptp_ena && ((rxm->packet_type & RTE_PTYPE_L2_MASK) ==
		    RTE_PTYPE_L2_ETHER_TIMESYNC)) {
			rxq->time_high =
			   rte_le_to_cpu_32(rxd.wb.flex_ts.ts_high);
			rxm->timesync = rxq->queue_id;
			pkt_flags |= RTE_MBUF_F_RX_IEEE1588_PTP;
		}
#endif
		rxm->ol_flags |= pkt_flags;
		/* copy old mbuf to rx_pkts */
		rx_pkts[nb_rx++] = rxm;
	}

	rxq->rx_tail = rx_id;
	/**
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the receive tail register of queue.
	 * Update that register with the value of the last processed RX
	 * descriptor minus 1.
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		rx_id = (uint16_t)(rx_id == 0 ?
				   (rxq->nb_rx_desc - 1) : (rx_id - 1));
		/* write TAIL register */
		ICE_PCI_REG_WC_WRITE(rxq->qrx_tail, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;

	/* return received packet in the burst */
	return nb_rx;
}

static inline void
ice_parse_tunneling_params(uint64_t ol_flags,
			    union ice_tx_offload tx_offload,
			    uint32_t *cd_tunneling)
{
	/* EIPT: External (outer) IP header type */
	if (ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM)
		*cd_tunneling |= ICE_TX_CTX_EIPT_IPV4;
	else if (ol_flags & RTE_MBUF_F_TX_OUTER_IPV4)
		*cd_tunneling |= ICE_TX_CTX_EIPT_IPV4_NO_CSUM;
	else if (ol_flags & RTE_MBUF_F_TX_OUTER_IPV6)
		*cd_tunneling |= ICE_TX_CTX_EIPT_IPV6;

	/* EIPLEN: External (outer) IP header length, in DWords */
	*cd_tunneling |= (tx_offload.outer_l3_len >> 2) <<
		ICE_TXD_CTX_QW0_EIPLEN_S;

	/* L4TUNT: L4 Tunneling Type */
	switch (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
	case RTE_MBUF_F_TX_TUNNEL_IPIP:
		/* for non UDP / GRE tunneling, set to 00b */
		break;
	case RTE_MBUF_F_TX_TUNNEL_VXLAN:
	case RTE_MBUF_F_TX_TUNNEL_VXLAN_GPE:
	case RTE_MBUF_F_TX_TUNNEL_GTP:
	case RTE_MBUF_F_TX_TUNNEL_GENEVE:
		*cd_tunneling |= ICE_TXD_CTX_UDP_TUNNELING;
		break;
	case RTE_MBUF_F_TX_TUNNEL_GRE:
		*cd_tunneling |= ICE_TXD_CTX_GRE_TUNNELING;
		break;
	default:
		PMD_TX_LOG(ERR, "Tunnel type not supported");
		return;
	}

	/* L4TUNLEN: L4 Tunneling Length, in Words
	 *
	 * We depend on app to set rte_mbuf.l2_len correctly.
	 * For IP in GRE it should be set to the length of the GRE
	 * header;
	 * For MAC in GRE or MAC in UDP it should be set to the length
	 * of the GRE or UDP headers plus the inner MAC up to including
	 * its last Ethertype.
	 * If MPLS labels exists, it should include them as well.
	 */
	*cd_tunneling |= (tx_offload.l2_len >> 1) <<
		ICE_TXD_CTX_QW0_NATLEN_S;

	/**
	 * Calculate the tunneling UDP checksum.
	 * Shall be set only if L4TUNT = 01b and EIPT is not zero
	 */
	if ((*cd_tunneling & ICE_TXD_CTX_QW0_EIPT_M) &&
			(*cd_tunneling & ICE_TXD_CTX_UDP_TUNNELING) &&
			(ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM))
		*cd_tunneling |= ICE_TXD_CTX_QW0_L4T_CS_M;
}

static inline void
ice_txd_enable_checksum(uint64_t ol_flags,
			uint32_t *td_cmd,
			uint32_t *td_offset,
			union ice_tx_offload tx_offload)
{
	/* Set MACLEN */
	if (!(ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK))
		*td_offset |= (tx_offload.l2_len >> 1)
			<< ICE_TX_DESC_LEN_MACLEN_S;

	/* Enable L3 checksum offloads */
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		*td_cmd |= ICE_TX_DESC_CMD_IIPT_IPV4_CSUM;
		*td_offset |= (tx_offload.l3_len >> 2) <<
			ICE_TX_DESC_LEN_IPLEN_S;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV4) {
		*td_cmd |= ICE_TX_DESC_CMD_IIPT_IPV4;
		*td_offset |= (tx_offload.l3_len >> 2) <<
			ICE_TX_DESC_LEN_IPLEN_S;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV6) {
		*td_cmd |= ICE_TX_DESC_CMD_IIPT_IPV6;
		*td_offset |= (tx_offload.l3_len >> 2) <<
			ICE_TX_DESC_LEN_IPLEN_S;
	}

	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		*td_cmd |= ICE_TX_DESC_CMD_L4T_EOFT_TCP;
		*td_offset |= (tx_offload.l4_len >> 2) <<
			      ICE_TX_DESC_LEN_L4_LEN_S;
		return;
	}

	if (ol_flags & RTE_MBUF_F_TX_UDP_SEG) {
		*td_cmd |= ICE_TX_DESC_CMD_L4T_EOFT_UDP;
		*td_offset |= (tx_offload.l4_len >> 2) <<
			      ICE_TX_DESC_LEN_L4_LEN_S;
		return;
	}

	/* Enable L4 checksum offloads */
	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		*td_cmd |= ICE_TX_DESC_CMD_L4T_EOFT_TCP;
		*td_offset |= (sizeof(struct rte_tcp_hdr) >> 2) <<
			      ICE_TX_DESC_LEN_L4_LEN_S;
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		*td_cmd |= ICE_TX_DESC_CMD_L4T_EOFT_SCTP;
		*td_offset |= (sizeof(struct rte_sctp_hdr) >> 2) <<
			      ICE_TX_DESC_LEN_L4_LEN_S;
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		*td_cmd |= ICE_TX_DESC_CMD_L4T_EOFT_UDP;
		*td_offset |= (sizeof(struct rte_udp_hdr) >> 2) <<
			      ICE_TX_DESC_LEN_L4_LEN_S;
		break;
	default:
		break;
	}
}

static inline int
ice_xmit_cleanup(struct ci_tx_queue *txq)
{
	struct ci_tx_entry *sw_ring = txq->sw_ring;
	volatile struct ice_tx_desc *txd = txq->ice_tx_ring;
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;

	/* Determine the last descriptor needing to be cleaned */
	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->tx_rs_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	/* Check to make sure the last descriptor to clean is done */
	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	if (!(txd[desc_to_clean_to].cmd_type_offset_bsz &
	    rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DESC_DONE))) {
		PMD_TX_LOG(DEBUG, "TX descriptor %4u is not done "
			   "(port=%d queue=%d) value=0x%"PRIx64,
			   desc_to_clean_to,
			   txq->port_id, txq->queue_id,
			   txd[desc_to_clean_to].cmd_type_offset_bsz);
		/* Failed to clean any descriptors */
		return -1;
	}

	/* Figure out how many descriptors will be cleaned */
	if (last_desc_cleaned > desc_to_clean_to)
		nb_tx_to_clean = (uint16_t)((nb_tx_desc - last_desc_cleaned) +
					    desc_to_clean_to);
	else
		nb_tx_to_clean = (uint16_t)(desc_to_clean_to -
					    last_desc_cleaned);

	/* The last descriptor to clean is done, so that means all the
	 * descriptors from the last descriptor that was cleaned
	 * up to the last descriptor with the RS bit set
	 * are done. Only reset the threshold descriptor.
	 */
	txd[desc_to_clean_to].cmd_type_offset_bsz = 0;

	/* Update the txq to reflect the last descriptor that was cleaned */
	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

	return 0;
}

/* Construct the tx flags */
static inline uint64_t
ice_build_ctob(uint32_t td_cmd,
	       uint32_t td_offset,
	       uint16_t size,
	       uint32_t td_tag)
{
	return rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DATA |
				((uint64_t)td_cmd << ICE_TXD_QW1_CMD_S) |
				((uint64_t)td_offset << ICE_TXD_QW1_OFFSET_S) |
				((uint64_t)size << ICE_TXD_QW1_TX_BUF_SZ_S) |
				((uint64_t)td_tag << ICE_TXD_QW1_L2TAG1_S));
}

/* Check if the context descriptor is needed for TX offloading */
static inline uint16_t
ice_calc_context_desc(uint64_t flags)
{
	static uint64_t mask = RTE_MBUF_F_TX_TCP_SEG |
		RTE_MBUF_F_TX_UDP_SEG |
		RTE_MBUF_F_TX_QINQ |
		RTE_MBUF_F_TX_OUTER_IP_CKSUM |
		RTE_MBUF_F_TX_TUNNEL_MASK |
		RTE_MBUF_F_TX_IEEE1588_TMST;

	return (flags & mask) ? 1 : 0;
}

/* set ice TSO context descriptor */
static inline uint64_t
ice_set_tso_ctx(struct rte_mbuf *mbuf, union ice_tx_offload tx_offload)
{
	uint64_t ctx_desc = 0;
	uint32_t cd_cmd, hdr_len, cd_tso_len;

	if (!tx_offload.l4_len) {
		PMD_TX_LOG(DEBUG, "L4 length set to 0");
		return ctx_desc;
	}

	hdr_len = tx_offload.l2_len + tx_offload.l3_len + tx_offload.l4_len;
	hdr_len += (mbuf->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) ?
		   tx_offload.outer_l2_len + tx_offload.outer_l3_len : 0;

	cd_cmd = ICE_TX_CTX_DESC_TSO;
	cd_tso_len = mbuf->pkt_len - hdr_len;
	ctx_desc |= ((uint64_t)cd_cmd << ICE_TXD_CTX_QW1_CMD_S) |
		    ((uint64_t)cd_tso_len << ICE_TXD_CTX_QW1_TSO_LEN_S) |
		    ((uint64_t)mbuf->tso_segsz << ICE_TXD_CTX_QW1_MSS_S);

	return ctx_desc;
}

/* HW requires that TX buffer size ranges from 1B up to (16K-1)B. */
#define ICE_MAX_DATA_PER_TXD \
	(ICE_TXD_QW1_TX_BUF_SZ_M >> ICE_TXD_QW1_TX_BUF_SZ_S)
/* Calculate the number of TX descriptors needed for each pkt */
static inline uint16_t
ice_calc_pkt_desc(struct rte_mbuf *tx_pkt)
{
	struct rte_mbuf *txd = tx_pkt;
	uint16_t count = 0;

	while (txd != NULL) {
		count += DIV_ROUND_UP(txd->data_len, ICE_MAX_DATA_PER_TXD);
		txd = txd->next;
	}

	return count;
}

uint16_t
ice_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct ci_tx_queue *txq;
	volatile struct ice_tx_desc *ice_tx_ring;
	volatile struct ice_tx_desc *txd;
	struct ci_tx_entry *sw_ring;
	struct ci_tx_entry *txe, *txn;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	uint32_t cd_tunneling_params;
	uint16_t tx_id;
	uint16_t ts_id = -1;
	uint16_t nb_tx;
	uint16_t nb_used;
	uint16_t nb_ctx;
	uint32_t td_cmd = 0;
	uint32_t td_offset = 0;
	uint32_t td_tag = 0;
	uint16_t tx_last;
	uint16_t slen;
	uint64_t buf_dma_addr;
	uint64_t ol_flags;
	union ice_tx_offload tx_offload = {0};

	txq = tx_queue;
	sw_ring = txq->sw_ring;
	ice_tx_ring = txq->ice_tx_ring;
	tx_id = txq->tx_tail;
	txe = &sw_ring[tx_id];

	if (txq->tsq != NULL && txq->tsq->ts_flag > 0)
		ts_id = txq->tsq->ts_tail;

	/* Check if the descriptor ring needs to be cleaned. */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		(void)ice_xmit_cleanup(txq);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = *tx_pkts++;

		td_cmd = 0;
		td_tag = 0;
		td_offset = 0;
		ol_flags = tx_pkt->ol_flags;
		tx_offload.l2_len = tx_pkt->l2_len;
		tx_offload.l3_len = tx_pkt->l3_len;
		tx_offload.outer_l2_len = tx_pkt->outer_l2_len;
		tx_offload.outer_l3_len = tx_pkt->outer_l3_len;
		tx_offload.l4_len = tx_pkt->l4_len;
		tx_offload.tso_segsz = tx_pkt->tso_segsz;
		/* Calculate the number of context descriptors needed. */
		nb_ctx = ice_calc_context_desc(ol_flags);

		/* The number of descriptors that must be allocated for
		 * a packet equals to the number of the segments of that
		 * packet plus the number of context descriptor if needed.
		 * Recalculate the needed tx descs when TSO enabled in case
		 * the mbuf data size exceeds max data size that hw allows
		 * per tx desc.
		 */
		if (ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG))
			nb_used = (uint16_t)(ice_calc_pkt_desc(tx_pkt) +
					     nb_ctx);
		else
			nb_used = (uint16_t)(tx_pkt->nb_segs + nb_ctx);
		tx_last = (uint16_t)(tx_id + nb_used - 1);

		/* Circular ring */
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t)(tx_last - txq->nb_tx_desc);

		if (nb_used > txq->nb_tx_free) {
			if (ice_xmit_cleanup(txq) != 0) {
				if (nb_tx == 0)
					return 0;
				goto end_of_tx;
			}
			if (unlikely(nb_used > txq->tx_rs_thresh)) {
				while (nb_used > txq->nb_tx_free) {
					if (ice_xmit_cleanup(txq) != 0) {
						if (nb_tx == 0)
							return 0;
						goto end_of_tx;
					}
				}
			}
		}

		/* Descriptor based VLAN insertion */
		if (ol_flags & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
			td_cmd |= ICE_TX_DESC_CMD_IL2TAG1;
			td_tag = tx_pkt->vlan_tci;
		}

		/* Fill in tunneling parameters if necessary */
		cd_tunneling_params = 0;
		if (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
			td_offset |= (tx_offload.outer_l2_len >> 1)
				<< ICE_TX_DESC_LEN_MACLEN_S;
			ice_parse_tunneling_params(ol_flags, tx_offload,
						   &cd_tunneling_params);
		}

		/* Enable checksum offloading */
		if (ol_flags & ICE_TX_CKSUM_OFFLOAD_MASK)
			ice_txd_enable_checksum(ol_flags, &td_cmd,
						&td_offset, tx_offload);

		if (nb_ctx) {
			/* Setup TX context descriptor if required */
			volatile struct ice_tx_ctx_desc *ctx_txd =
				(volatile struct ice_tx_ctx_desc *)
					&ice_tx_ring[tx_id];
			uint16_t cd_l2tag2 = 0;
			uint64_t cd_type_cmd_tso_mss = ICE_TX_DESC_DTYPE_CTX;

			txn = &sw_ring[txe->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);
			if (txe->mbuf) {
				rte_pktmbuf_free_seg(txe->mbuf);
				txe->mbuf = NULL;
			}

			if (ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG))
				cd_type_cmd_tso_mss |=
					ice_set_tso_ctx(tx_pkt, tx_offload);
			else if (ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST)
				cd_type_cmd_tso_mss |=
					((uint64_t)ICE_TX_CTX_DESC_TSYN <<
					ICE_TXD_CTX_QW1_CMD_S) |
					 (((uint64_t)txq->ice_vsi->adapter->ptp_tx_index <<
					 ICE_TXD_CTX_QW1_TSYN_S) & ICE_TXD_CTX_QW1_TSYN_M);

			ctx_txd->tunneling_params =
				rte_cpu_to_le_32(cd_tunneling_params);

			/* TX context descriptor based double VLAN insert */
			if (ol_flags & RTE_MBUF_F_TX_QINQ) {
				cd_l2tag2 = tx_pkt->vlan_tci_outer;
				cd_type_cmd_tso_mss |=
					((uint64_t)ICE_TX_CTX_DESC_IL2TAG2 <<
					 ICE_TXD_CTX_QW1_CMD_S);
			}
			ctx_txd->l2tag2 = rte_cpu_to_le_16(cd_l2tag2);
			ctx_txd->qw1 =
				rte_cpu_to_le_64(cd_type_cmd_tso_mss);

			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
		}
		m_seg = tx_pkt;

		do {
			txd = &ice_tx_ring[tx_id];
			txn = &sw_ring[txe->next_id];

			if (txe->mbuf)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/* Setup TX Descriptor */
			slen = m_seg->data_len;
			buf_dma_addr = rte_mbuf_data_iova(m_seg);

			while ((ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) &&
				unlikely(slen > ICE_MAX_DATA_PER_TXD)) {
				txd->buf_addr = rte_cpu_to_le_64(buf_dma_addr);
				txd->cmd_type_offset_bsz =
				rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DATA |
				((uint64_t)td_cmd << ICE_TXD_QW1_CMD_S) |
				((uint64_t)td_offset << ICE_TXD_QW1_OFFSET_S) |
				((uint64_t)ICE_MAX_DATA_PER_TXD <<
				 ICE_TXD_QW1_TX_BUF_SZ_S) |
				((uint64_t)td_tag << ICE_TXD_QW1_L2TAG1_S));

				buf_dma_addr += ICE_MAX_DATA_PER_TXD;
				slen -= ICE_MAX_DATA_PER_TXD;

				txe->last_id = tx_last;
				tx_id = txe->next_id;
				txe = txn;
				txd = &ice_tx_ring[tx_id];
				txn = &sw_ring[txe->next_id];
			}

			txd->buf_addr = rte_cpu_to_le_64(buf_dma_addr);
			txd->cmd_type_offset_bsz =
				rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DATA |
				((uint64_t)td_cmd << ICE_TXD_QW1_CMD_S) |
				((uint64_t)td_offset << ICE_TXD_QW1_OFFSET_S) |
				((uint64_t)slen << ICE_TXD_QW1_TX_BUF_SZ_S) |
				((uint64_t)td_tag << ICE_TXD_QW1_L2TAG1_S));

			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->next;
		} while (m_seg);

		/* fill the last descriptor with End of Packet (EOP) bit */
		td_cmd |= ICE_TX_DESC_CMD_EOP;
		txq->nb_tx_used = (uint16_t)(txq->nb_tx_used + nb_used);
		txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_used);

		/* set RS bit on the last descriptor of one packet */
		if (txq->nb_tx_used >= txq->tx_rs_thresh) {
			PMD_TX_LOG(DEBUG,
				   "Setting RS bit on TXD id="
				   "%4u (port=%d queue=%d)",
				   tx_last, txq->port_id, txq->queue_id);

			td_cmd |= ICE_TX_DESC_CMD_RS;

			/* Update txq RS bit counters */
			txq->nb_tx_used = 0;
		}
		txd->cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)td_cmd) <<
					 ICE_TXD_QW1_CMD_S);

		if (txq->tsq != NULL && txq->tsq->ts_flag > 0) {
			uint64_t txtime = *RTE_MBUF_DYNFIELD(tx_pkt,
					txq->tsq->ts_offset, uint64_t *);
			uint32_t tstamp = (uint32_t)(txtime % NS_PER_S) >>
						ICE_TXTIME_CTX_RESOLUTION_128NS;
			const uint32_t desc_tx_id = (tx_id == 0) ? txq->nb_tx_desc : tx_id;
			__le32 ts_desc = rte_cpu_to_le_32(FIELD_PREP(ICE_TXTIME_TX_DESC_IDX_M,
					desc_tx_id) | FIELD_PREP(ICE_TXTIME_STAMP_M, tstamp));
			txq->tsq->ice_ts_ring[ts_id].tx_desc_idx_tstamp = ts_desc;
			ts_id++;
			/* To prevent an MDD, when wrapping the tstamp
			 * ring create additional TS descriptors equal
			 * to the number of the fetch TS descriptors
			 * value. HW will merge the TS descriptors with
			 * the same timestamp value into a single
			 * descriptor.
			 */
			if (ts_id == txq->tsq->nb_ts_desc) {
				uint16_t fetch = txq->tsq->nb_ts_desc - txq->nb_tx_desc;
				ts_id = 0;
				for (; ts_id < fetch; ts_id++)
					txq->tsq->ice_ts_ring[ts_id].tx_desc_idx_tstamp = ts_desc;
			}
		}
	}
end_of_tx:
	/* update Tail register */
	if (txq->tsq != NULL && txq->tsq->ts_flag > 0) {
		ICE_PCI_REG_WRITE(txq->qtx_tail, ts_id);
		txq->tsq->ts_tail = ts_id;
	} else {
		ICE_PCI_REG_WRITE(txq->qtx_tail, tx_id);
	}
	txq->tx_tail = tx_id;

	return nb_tx;
}

static __rte_always_inline int
ice_tx_free_bufs(struct ci_tx_queue *txq)
{
	struct ci_tx_entry *txep;
	uint16_t i;

	if ((txq->ice_tx_ring[txq->tx_next_dd].cmd_type_offset_bsz &
	     rte_cpu_to_le_64(ICE_TXD_QW1_DTYPE_M)) !=
	    rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DESC_DONE))
		return 0;

	txep = &txq->sw_ring[txq->tx_next_dd - (txq->tx_rs_thresh - 1)];

	for (i = 0; i < txq->tx_rs_thresh; i++)
		rte_prefetch0((txep + i)->mbuf);

	if (txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
		for (i = 0; i < txq->tx_rs_thresh; ++i, ++txep) {
			rte_mempool_put(txep->mbuf->pool, txep->mbuf);
			txep->mbuf = NULL;
		}
	} else {
		for (i = 0; i < txq->tx_rs_thresh; ++i, ++txep) {
			rte_pktmbuf_free_seg(txep->mbuf);
			txep->mbuf = NULL;
		}
	}

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return txq->tx_rs_thresh;
}

static int
ice_tx_done_cleanup_full(struct ci_tx_queue *txq,
			uint32_t free_cnt)
{
	struct ci_tx_entry *swr_ring = txq->sw_ring;
	uint16_t i, tx_last, tx_id;
	uint16_t nb_tx_free_last;
	uint16_t nb_tx_to_clean;
	uint32_t pkt_cnt;

	/* Start free mbuf from the next of tx_tail */
	tx_last = txq->tx_tail;
	tx_id  = swr_ring[tx_last].next_id;

	if (txq->nb_tx_free == 0 && ice_xmit_cleanup(txq))
		return 0;

	nb_tx_to_clean = txq->nb_tx_free;
	nb_tx_free_last = txq->nb_tx_free;
	if (!free_cnt)
		free_cnt = txq->nb_tx_desc;

	/* Loop through swr_ring to count the amount of
	 * freeable mubfs and packets.
	 */
	for (pkt_cnt = 0; pkt_cnt < free_cnt; ) {
		for (i = 0; i < nb_tx_to_clean &&
			pkt_cnt < free_cnt &&
			tx_id != tx_last; i++) {
			if (swr_ring[tx_id].mbuf != NULL) {
				rte_pktmbuf_free_seg(swr_ring[tx_id].mbuf);
				swr_ring[tx_id].mbuf = NULL;

				/*
				 * last segment in the packet,
				 * increment packet count
				 */
				pkt_cnt += (swr_ring[tx_id].last_id == tx_id);
			}

			tx_id = swr_ring[tx_id].next_id;
		}

		if (txq->tx_rs_thresh > txq->nb_tx_desc -
			txq->nb_tx_free || tx_id == tx_last)
			break;

		if (pkt_cnt < free_cnt) {
			if (ice_xmit_cleanup(txq))
				break;

			nb_tx_to_clean = txq->nb_tx_free - nb_tx_free_last;
			nb_tx_free_last = txq->nb_tx_free;
		}
	}

	return (int)pkt_cnt;
}

#ifdef RTE_ARCH_X86
static int
ice_tx_done_cleanup_vec(struct ci_tx_queue *txq __rte_unused,
			uint32_t free_cnt __rte_unused)
{
	return -ENOTSUP;
}
#endif

static int
ice_tx_done_cleanup_simple(struct ci_tx_queue *txq,
			uint32_t free_cnt)
{
	int i, n, cnt;

	if (free_cnt == 0 || free_cnt > txq->nb_tx_desc)
		free_cnt = txq->nb_tx_desc;

	cnt = free_cnt - free_cnt % txq->tx_rs_thresh;

	for (i = 0; i < cnt; i += n) {
		if (txq->nb_tx_desc - txq->nb_tx_free < txq->tx_rs_thresh)
			break;

		n = ice_tx_free_bufs(txq);

		if (n == 0)
			break;
	}

	return i;
}

int
ice_tx_done_cleanup(void *txq, uint32_t free_cnt)
{
	struct ci_tx_queue *q = (struct ci_tx_queue *)txq;
	struct rte_eth_dev *dev = &rte_eth_devices[q->port_id];
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

#ifdef RTE_ARCH_X86
	if (ad->tx_vec_allowed)
		return ice_tx_done_cleanup_vec(q, free_cnt);
#endif
	if (ad->tx_simple_allowed)
		return ice_tx_done_cleanup_simple(q, free_cnt);
	else
		return ice_tx_done_cleanup_full(q, free_cnt);
}

/* Populate 4 descriptors with data from 4 mbufs */
static inline void
tx4(volatile struct ice_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t dma_addr;
	uint32_t i;

	for (i = 0; i < 4; i++, txdp++, pkts++) {
		dma_addr = rte_mbuf_data_iova(*pkts);
		txdp->buf_addr = rte_cpu_to_le_64(dma_addr);
		txdp->cmd_type_offset_bsz =
			ice_build_ctob((uint32_t)ICE_TD_CMD, 0,
				       (*pkts)->data_len, 0);
	}
}

/* Populate 1 descriptor with data from 1 mbuf */
static inline void
tx1(volatile struct ice_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t dma_addr;

	dma_addr = rte_mbuf_data_iova(*pkts);
	txdp->buf_addr = rte_cpu_to_le_64(dma_addr);
	txdp->cmd_type_offset_bsz =
		ice_build_ctob((uint32_t)ICE_TD_CMD, 0,
			       (*pkts)->data_len, 0);
}

static inline void
ice_tx_fill_hw_ring(struct ci_tx_queue *txq, struct rte_mbuf **pkts,
		    uint16_t nb_pkts)
{
	volatile struct ice_tx_desc *txdp = &txq->ice_tx_ring[txq->tx_tail];
	struct ci_tx_entry *txep = &txq->sw_ring[txq->tx_tail];
	const int N_PER_LOOP = 4;
	const int N_PER_LOOP_MASK = N_PER_LOOP - 1;
	int mainpart, leftover;
	int i, j;

	/**
	 * Process most of the packets in chunks of N pkts.  Any
	 * leftover packets will get processed one at a time.
	 */
	mainpart = nb_pkts & ((uint32_t)~N_PER_LOOP_MASK);
	leftover = nb_pkts & ((uint32_t)N_PER_LOOP_MASK);
	for (i = 0; i < mainpart; i += N_PER_LOOP) {
		/* Copy N mbuf pointers to the S/W ring */
		for (j = 0; j < N_PER_LOOP; ++j)
			(txep + i + j)->mbuf = *(pkts + i + j);
		tx4(txdp + i, pkts + i);
	}

	if (unlikely(leftover > 0)) {
		for (i = 0; i < leftover; ++i) {
			(txep + mainpart + i)->mbuf = *(pkts + mainpart + i);
			tx1(txdp + mainpart + i, pkts + mainpart + i);
		}
	}
}

static inline uint16_t
tx_xmit_pkts(struct ci_tx_queue *txq,
	     struct rte_mbuf **tx_pkts,
	     uint16_t nb_pkts)
{
	volatile struct ice_tx_desc *txr = txq->ice_tx_ring;
	uint16_t n = 0;

	/**
	 * Begin scanning the H/W ring for done descriptors when the number
	 * of available descriptors drops below tx_free_thresh. For each done
	 * descriptor, free the associated buffer.
	 */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		ice_tx_free_bufs(txq);

	/* Use available descriptor only */
	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(!nb_pkts))
		return 0;

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);
	if ((txq->tx_tail + nb_pkts) > txq->nb_tx_desc) {
		n = (uint16_t)(txq->nb_tx_desc - txq->tx_tail);
		ice_tx_fill_hw_ring(txq, tx_pkts, n);
		txr[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)ICE_TX_DESC_CMD_RS) <<
					 ICE_TXD_QW1_CMD_S);
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);
		txq->tx_tail = 0;
	}

	/* Fill hardware descriptor ring with mbuf data */
	ice_tx_fill_hw_ring(txq, tx_pkts + n, (uint16_t)(nb_pkts - n));
	txq->tx_tail = (uint16_t)(txq->tx_tail + (nb_pkts - n));

	/* Determine if RS bit needs to be set */
	if (txq->tx_tail > txq->tx_next_rs) {
		txr[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)ICE_TX_DESC_CMD_RS) <<
					 ICE_TXD_QW1_CMD_S);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
		if (txq->tx_next_rs >= txq->nb_tx_desc)
			txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);
	}

	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;

	/* Update the tx tail register */
	ICE_PCI_REG_WC_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

static uint16_t
ice_xmit_pkts_simple(void *tx_queue,
		     struct rte_mbuf **tx_pkts,
		     uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;

	if (likely(nb_pkts <= ICE_TX_MAX_BURST))
		return tx_xmit_pkts((struct ci_tx_queue *)tx_queue,
				    tx_pkts, nb_pkts);

	while (nb_pkts) {
		uint16_t ret, num = (uint16_t)RTE_MIN(nb_pkts,
						      ICE_TX_MAX_BURST);

		ret = tx_xmit_pkts((struct ci_tx_queue *)tx_queue,
				   &tx_pkts[nb_tx], num);
		nb_tx = (uint16_t)(nb_tx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < num)
			break;
	}

	return nb_tx;
}

static const struct ci_rx_path_info ice_rx_path_infos[] = {
	[ICE_RX_DEFAULT] = {
		.pkt_burst = ice_recv_pkts,
		.info = "Scalar",
		.features = {
			.rx_offloads = ICE_RX_SCALAR_OFFLOADS
		}
	},
	[ICE_RX_SCATTERED] = {
		.pkt_burst = ice_recv_scattered_pkts,
		.info = "Scalar Scattered",
		.features = {
			.rx_offloads = ICE_RX_SCALAR_OFFLOADS,
			.extra.scattered = true
		}
	},
	[ICE_RX_BULK_ALLOC] = {
		.pkt_burst = ice_recv_pkts_bulk_alloc,
		.info = "Scalar Bulk Alloc",
		.features = {
			.rx_offloads = ICE_RX_SCALAR_OFFLOADS,
			.extra.bulk_alloc = true
		}
	},
#ifdef RTE_ARCH_X86
	[ICE_RX_SSE] = {
		.pkt_burst = ice_recv_pkts_vec,
		.info = "Vector SSE",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOAD_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_128,
			.extra.bulk_alloc = true
		}
	},
	[ICE_RX_SSE_SCATTERED] = {
		.pkt_burst = ice_recv_scattered_pkts_vec,
		.info = "Vector SSE Scattered",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOAD_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_128,
			.extra.scattered = true,
			.extra.bulk_alloc = true
		}
	},
	[ICE_RX_AVX2] = {
		.pkt_burst = ice_recv_pkts_vec_avx2,
		.info = "Vector AVX2",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_256,
			.extra.bulk_alloc = true
		}
	},
	[ICE_RX_AVX2_SCATTERED] = {
		.pkt_burst = ice_recv_scattered_pkts_vec_avx2,
		.info = "Vector AVX2 Scattered",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_256,
			.extra.scattered = true,
			.extra.bulk_alloc = true
		}
	},
	[ICE_RX_AVX2_OFFLOAD] = {
		.pkt_burst = ice_recv_pkts_vec_avx2_offload,
		.info = "Offload Vector AVX2",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOAD_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_256,
			.extra.bulk_alloc = true
		}
	},
	[ICE_RX_AVX2_SCATTERED_OFFLOAD] = {
		.pkt_burst = ice_recv_scattered_pkts_vec_avx2_offload,
		.info = "Offload Vector AVX2 Scattered",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOAD_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_256,
			.extra.scattered = true,
			.extra.bulk_alloc = true
		}
	},
#ifdef CC_AVX512_SUPPORT
	[ICE_RX_AVX512] = {
		.pkt_burst = ice_recv_pkts_vec_avx512,
		.info = "Vector AVX512",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_512,
			.extra.bulk_alloc = true
		}
	},
	[ICE_RX_AVX512_SCATTERED] = {
		.pkt_burst = ice_recv_scattered_pkts_vec_avx512,
		.info = "Vector AVX512 Scattered",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_512,
			.extra.scattered = true,
			.extra.bulk_alloc = true
		}
	},
	[ICE_RX_AVX512_OFFLOAD] = {
		.pkt_burst = ice_recv_pkts_vec_avx512_offload,
		.info = "Offload Vector AVX512",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOAD_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_512,
			.extra.bulk_alloc = true
		}
	},
	[ICE_RX_AVX512_SCATTERED_OFFLOAD] = {
		.pkt_burst = ice_recv_scattered_pkts_vec_avx512_offload,
		.info = "Offload Vector AVX512 Scattered",
		.features = {
			.rx_offloads = ICE_RX_VECTOR_OFFLOAD_OFFLOADS,
			.simd_width = RTE_VECT_SIMD_512,
			.extra.scattered = true,
			.extra.bulk_alloc = true
		}
	},
#endif
#endif
};

void __rte_cold
ice_set_rx_function(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	enum rte_vect_max_simd rx_simd_width = RTE_VECT_SIMD_DISABLED;
	struct ci_rx_path_features req_features = {
		.rx_offloads = dev->data->dev_conf.rxmode.offloads,
		.simd_width = RTE_VECT_SIMD_DISABLED,
	};

	/* The primary process selects the rx path for all processes. */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		goto out;

#ifdef RTE_ARCH_X86
	if (ad->ptp_ena || !ad->rx_bulk_alloc_allowed) {
		rx_simd_width = RTE_VECT_SIMD_DISABLED;
	} else {
		rx_simd_width = ice_get_max_simd_bitwidth();
		if (rx_simd_width >= RTE_VECT_SIMD_128)
			if (ice_rx_vec_dev_check(dev) == -1)
				rx_simd_width = RTE_VECT_SIMD_DISABLED;
	}
#endif

	req_features.simd_width = rx_simd_width;
	if (dev->data->scattered_rx)
		req_features.extra.scattered = true;
	if (ad->rx_bulk_alloc_allowed)
		req_features.extra.bulk_alloc = true;

	ad->rx_func_type = ci_rx_path_select(req_features,
						&ice_rx_path_infos[0],
						RTE_DIM(ice_rx_path_infos),
						ICE_RX_DEFAULT);
#ifdef RTE_ARCH_X86
	int i;

	if (ice_rx_path_infos[ad->rx_func_type].features.simd_width >= RTE_VECT_SIMD_128)
		/* Vector function selected. Prepare the rxq accordingly. */
		for (i = 0; i < dev->data->nb_rx_queues; i++)
			if (dev->data->rx_queues[i])
				ice_rxq_vec_setup(dev->data->rx_queues[i]);
#endif

out:
	dev->rx_pkt_burst = ice_rx_path_infos[ad->rx_func_type].pkt_burst;
	PMD_DRV_LOG(NOTICE, "Using %s (port %d).",
			ice_rx_path_infos[ad->rx_func_type].info, dev->data->port_id);
}

int
ice_rx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		      struct rte_eth_burst_mode *mode)
{
	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	int ret = -EINVAL;
	unsigned int i;

	for (i = 0; i < RTE_DIM(ice_rx_path_infos); ++i) {
		if (pkt_burst == ice_rx_path_infos[i].pkt_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				 ice_rx_path_infos[i].info);
			ret = 0;
			break;
		}
	}

	return ret;
}

void __rte_cold
ice_set_tx_function_flag(struct rte_eth_dev *dev, struct ci_tx_queue *txq)
{
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	/* Use a simple Tx queue if possible (only fast free is allowed) */
	ad->tx_simple_allowed =
		(txq->offloads ==
		(txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) &&
		txq->tx_rs_thresh >= ICE_TX_MAX_BURST);

	if (ad->tx_simple_allowed)
		PMD_INIT_LOG(DEBUG, "Simple Tx can be enabled on Tx queue %u.",
			     txq->queue_id);
	else
		PMD_INIT_LOG(DEBUG,
			     "Simple Tx can NOT be enabled on Tx queue %u.",
			     txq->queue_id);
}

/*********************************************************************
 *
 *  TX prep functions
 *
 **********************************************************************/
/* The default values of TSO MSS */
#define ICE_MIN_TSO_MSS            64
#define ICE_MAX_TSO_MSS            9728
#define ICE_MAX_TSO_FRAME_SIZE     262144

/*Check for empty mbuf*/
static inline uint16_t
ice_check_empty_mbuf(struct rte_mbuf *tx_pkt)
{
	struct rte_mbuf *txd = tx_pkt;

	while (txd != NULL) {
		if (txd->data_len == 0)
			return -1;
		txd = txd->next;
	}

	return 0;
}

/* Tx mbuf check */
static uint16_t
ice_xmit_pkts_check(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct ci_tx_queue *txq = tx_queue;
	uint16_t idx;
	struct rte_mbuf *mb;
	bool pkt_error = false;
	uint16_t good_pkts = nb_pkts;
	const char *reason = NULL;
	struct ice_adapter *adapter = txq->ice_vsi->adapter;
	uint64_t ol_flags;

	for (idx = 0; idx < nb_pkts; idx++) {
		mb = tx_pkts[idx];
		ol_flags = mb->ol_flags;

		if ((adapter->devargs.mbuf_check & ICE_MBUF_CHECK_F_TX_MBUF) &&
		    (rte_mbuf_check(mb, 1, &reason) != 0)) {
			PMD_TX_LOG(ERR, "INVALID mbuf: %s", reason);
			pkt_error = true;
			break;
		}

		if ((adapter->devargs.mbuf_check & ICE_MBUF_CHECK_F_TX_SIZE) &&
		    (mb->data_len > mb->pkt_len ||
		     mb->data_len < ICE_TX_MIN_PKT_LEN ||
		     mb->data_len > ICE_FRAME_SIZE_MAX)) {
			PMD_TX_LOG(ERR, "INVALID mbuf: data_len (%u) is out of range, reasonable range (%d - %d)",
				mb->data_len, ICE_TX_MIN_PKT_LEN, ICE_FRAME_SIZE_MAX);
			pkt_error = true;
			break;
		}

		if (adapter->devargs.mbuf_check & ICE_MBUF_CHECK_F_TX_SEGMENT) {
			if (!(ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
				/**
				 * No TSO case: nb->segs, pkt_len to not exceed
				 * the limites.
				 */
				if (mb->nb_segs > ICE_TX_MTU_SEG_MAX) {
					PMD_TX_LOG(ERR, "INVALID mbuf: nb_segs (%d) exceeds HW limit, maximum allowed value is %d",
						mb->nb_segs, ICE_TX_MTU_SEG_MAX);
					pkt_error = true;
					break;
				}
				if (mb->pkt_len > ICE_FRAME_SIZE_MAX) {
					PMD_TX_LOG(ERR, "INVALID mbuf: pkt_len (%d) exceeds HW limit, maximum allowed value is %d",
						mb->nb_segs, ICE_FRAME_SIZE_MAX);
					pkt_error = true;
					break;
				}
			} else if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
				/** TSO case: tso_segsz, nb_segs, pkt_len not exceed
				 * the limits.
				 */
				if (mb->tso_segsz < ICE_MIN_TSO_MSS ||
				    mb->tso_segsz > ICE_MAX_TSO_MSS) {
					/**
					 * MSS outside the range are considered malicious
					 */
					PMD_TX_LOG(ERR, "INVALID mbuf: tso_segsz (%u) is out of range, reasonable range (%d - %u)",
						mb->tso_segsz, ICE_MIN_TSO_MSS, ICE_MAX_TSO_MSS);
					pkt_error = true;
					break;
				}
				if (mb->nb_segs > ((struct ci_tx_queue *)tx_queue)->nb_tx_desc) {
					PMD_TX_LOG(ERR, "INVALID mbuf: nb_segs out of ring length");
					pkt_error = true;
					break;
				}
			}
		}

		if (adapter->devargs.mbuf_check & ICE_MBUF_CHECK_F_TX_OFFLOAD) {
			if (ol_flags & ICE_TX_OFFLOAD_NOTSUP_MASK) {
				PMD_TX_LOG(ERR, "INVALID mbuf: TX offload is not supported");
				pkt_error = true;
				break;
			}

			if (!rte_validate_tx_offload(mb)) {
				PMD_TX_LOG(ERR, "INVALID mbuf: TX offload setup error");
				pkt_error = true;
				break;
			}
		}
	}

	if (pkt_error) {
		txq->mbuf_errors++;
		good_pkts = idx;
		if (good_pkts == 0)
			return 0;
	}

	return adapter->tx_pkt_burst(tx_queue, tx_pkts, good_pkts);
}

uint16_t
ice_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	      uint16_t nb_pkts)
{
	int i, ret;
	uint64_t ol_flags;
	struct rte_mbuf *m;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		if (!(ol_flags & RTE_MBUF_F_TX_TCP_SEG) &&
		    /**
		     * No TSO case: nb->segs, pkt_len to not exceed
		     * the limites.
		     */
		    (m->nb_segs > ICE_TX_MTU_SEG_MAX ||
		     m->pkt_len > ICE_FRAME_SIZE_MAX)) {
			rte_errno = EINVAL;
			return i;
		} else if (ol_flags & RTE_MBUF_F_TX_TCP_SEG &&
		    /** TSO case: tso_segsz, nb_segs, pkt_len not exceed
		     * the limits.
		     */
		    (m->tso_segsz < ICE_MIN_TSO_MSS ||
		     m->tso_segsz > ICE_MAX_TSO_MSS ||
		     m->nb_segs >
			((struct ci_tx_queue *)tx_queue)->nb_tx_desc ||
		     m->pkt_len > ICE_MAX_TSO_FRAME_SIZE)) {
			/**
			 * MSS outside the range are considered malicious
			 */
			rte_errno = EINVAL;
			return i;
		}

		if (m->pkt_len < ICE_TX_MIN_PKT_LEN) {
			rte_errno = EINVAL;
			return i;
		}

#ifdef RTE_ETHDEV_DEBUG_TX
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}

		if (ice_check_empty_mbuf(m) != 0) {
			rte_errno = EINVAL;
			return i;
		}
	}
	return i;
}

void __rte_cold
ice_set_tx_function(struct rte_eth_dev *dev)
{
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int mbuf_check = ad->devargs.mbuf_check;
#ifdef RTE_ARCH_X86
	struct ci_tx_queue *txq;
	int i;
	int tx_check_ret = -1;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ad->tx_simd_width = RTE_VECT_SIMD_DISABLED;
		tx_check_ret = ice_tx_vec_dev_check(dev);
		ad->tx_simd_width = ice_get_max_simd_bitwidth();
		if (tx_check_ret >= 0 &&
		    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128) {
			ad->tx_vec_allowed = true;

			if (ad->tx_simd_width < RTE_VECT_SIMD_256 &&
				tx_check_ret == ICE_VECTOR_OFFLOAD_PATH)
				ad->tx_vec_allowed = false;

			if (ad->tx_vec_allowed) {
				for (i = 0; i < dev->data->nb_tx_queues; i++) {
					txq = dev->data->tx_queues[i];
					if (txq && ice_txq_vec_setup(txq)) {
						ad->tx_vec_allowed = false;
						break;
					}
				}
			}
		} else {
			ad->tx_vec_allowed = false;
		}
	}

	if (ad->tx_vec_allowed) {
		dev->tx_pkt_prepare = NULL;
		if (ad->tx_simd_width == RTE_VECT_SIMD_512) {
#ifdef CC_AVX512_SUPPORT
			if (tx_check_ret == ICE_VECTOR_OFFLOAD_PATH) {
				PMD_DRV_LOG(NOTICE,
					    "Using AVX512 OFFLOAD Vector Tx (port %d).",
					    dev->data->port_id);
				dev->tx_pkt_burst =
					ice_xmit_pkts_vec_avx512_offload;
				dev->tx_pkt_prepare = ice_prep_pkts;
			} else {
				PMD_DRV_LOG(NOTICE,
					    "Using AVX512 Vector Tx (port %d).",
					    dev->data->port_id);
				dev->tx_pkt_burst = ice_xmit_pkts_vec_avx512;
			}
#endif
		} else {
			if (tx_check_ret == ICE_VECTOR_OFFLOAD_PATH) {
				PMD_DRV_LOG(NOTICE,
					    "Using AVX2 OFFLOAD Vector Tx (port %d).",
					    dev->data->port_id);
				dev->tx_pkt_burst =
					ice_xmit_pkts_vec_avx2_offload;
				dev->tx_pkt_prepare = ice_prep_pkts;
			} else {
				PMD_DRV_LOG(DEBUG, "Using %sVector Tx (port %d).",
					    ad->tx_simd_width == RTE_VECT_SIMD_256 ? "avx2 " : "",
					    dev->data->port_id);
				dev->tx_pkt_burst = ad->tx_simd_width == RTE_VECT_SIMD_256 ?
						    ice_xmit_pkts_vec_avx2 :
						    ice_xmit_pkts_vec;
			}
		}

		if (mbuf_check) {
			ad->tx_pkt_burst = dev->tx_pkt_burst;
			dev->tx_pkt_burst = ice_xmit_pkts_check;
		}
		return;
	}
#endif

	if (ad->tx_simple_allowed) {
		PMD_INIT_LOG(DEBUG, "Simple tx finally be used.");
		dev->tx_pkt_burst = ice_xmit_pkts_simple;
		dev->tx_pkt_prepare = NULL;
	} else {
		PMD_INIT_LOG(DEBUG, "Normal tx finally be used.");
		dev->tx_pkt_burst = ice_xmit_pkts;
		dev->tx_pkt_prepare = ice_prep_pkts;
	}

	if (mbuf_check) {
		ad->tx_pkt_burst = dev->tx_pkt_burst;
		dev->tx_pkt_burst = ice_xmit_pkts_check;
	}
}

static const struct {
	eth_tx_burst_t pkt_burst;
	const char *info;
} ice_tx_burst_infos[] = {
	{ ice_xmit_pkts_simple,   "Scalar Simple" },
	{ ice_xmit_pkts,          "Scalar" },
#ifdef RTE_ARCH_X86
#ifdef CC_AVX512_SUPPORT
	{ ice_xmit_pkts_vec_avx512, "Vector AVX512" },
	{ ice_xmit_pkts_vec_avx512_offload, "Offload Vector AVX512" },
#endif
	{ ice_xmit_pkts_vec_avx2,         "Vector AVX2" },
	{ ice_xmit_pkts_vec_avx2_offload, "Offload Vector AVX2" },
	{ ice_xmit_pkts_vec,              "Vector SSE" },
#endif
};

int
ice_tx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		      struct rte_eth_burst_mode *mode)
{
	eth_tx_burst_t pkt_burst = dev->tx_pkt_burst;
	int ret = -EINVAL;
	unsigned int i;

	for (i = 0; i < RTE_DIM(ice_tx_burst_infos); ++i) {
		if (pkt_burst == ice_tx_burst_infos[i].pkt_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				 ice_tx_burst_infos[i].info);
			ret = 0;
			break;
		}
	}

	return ret;
}

/* For each value it means, datasheet of hardware can tell more details
 *
 * @note: fix ice_dev_supported_ptypes_get() if any change here.
 */
static inline uint32_t
ice_get_default_pkt_type(uint16_t ptype)
{
	static const alignas(RTE_CACHE_LINE_SIZE) uint32_t type_table[ICE_MAX_PKT_TYPE] = {
		/* L2 types */
		/* [0] reserved */
		[1] = RTE_PTYPE_L2_ETHER,
		[2] = RTE_PTYPE_L2_ETHER_TIMESYNC,
		/* [3] - [5] reserved */
		[6] = RTE_PTYPE_L2_ETHER_LLDP,
		/* [7] - [10] reserved */
		[11] = RTE_PTYPE_L2_ETHER_ARP,
		/* [12] - [21] reserved */

		/* Non tunneled IPv4 */
		[22] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_FRAG,
		[23] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_NONFRAG,
		[24] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_UDP,
		/* [25] reserved */
		[26] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_TCP,
		[27] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_SCTP,
		[28] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_ICMP,

		/* IPv4 --> IPv4 */
		[29] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[30] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[31] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [32] reserved */
		[33] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[34] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[35] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> IPv6 */
		[36] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[37] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[38] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [39] reserved */
		[40] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[41] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[42] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GRE/Teredo/VXLAN */
		[43] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT,

		/* IPv4 --> GRE/Teredo/VXLAN --> IPv4 */
		[44] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[45] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[46] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [47] reserved */
		[48] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[49] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[50] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GRE/Teredo/VXLAN --> IPv6 */
		[51] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[52] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[53] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [54] reserved */
		[55] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[56] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[57] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GRE/Teredo/VXLAN --> MAC */
		[58] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER,

		/* IPv4 --> GRE/Teredo/VXLAN --> MAC --> IPv4 */
		[59] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[60] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[61] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [62] reserved */
		[63] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[64] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[65] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GRE/Teredo/VXLAN --> MAC --> IPv6 */
		[66] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[67] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[68] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [69] reserved */
		[70] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[71] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[72] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,
		/* [73] - [87] reserved */

		/* Non tunneled IPv6 */
		[88] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_FRAG,
		[89] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_NONFRAG,
		[90] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_UDP,
		/* [91] reserved */
		[92] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_TCP,
		[93] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_SCTP,
		[94] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_ICMP,

		/* IPv6 --> IPv4 */
		[95] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[96] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[97] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [98] reserved */
		[99] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[100] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[101] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> IPv6 */
		[102] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[103] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[104] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [105] reserved */
		[106] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[107] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[108] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GRE/Teredo/VXLAN */
		[109] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT,

		/* IPv6 --> GRE/Teredo/VXLAN --> IPv4 */
		[110] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[111] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[112] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [113] reserved */
		[114] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[115] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[116] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GRE/Teredo/VXLAN --> IPv6 */
		[117] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[118] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[119] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [120] reserved */
		[121] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[122] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[123] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GRE/Teredo/VXLAN --> MAC */
		[124] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER,

		/* IPv6 --> GRE/Teredo/VXLAN --> MAC --> IPv4 */
		[125] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[126] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[127] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [128] reserved */
		[129] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[130] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[131] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GRE/Teredo/VXLAN --> MAC --> IPv6 */
		[132] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[133] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[134] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [135] reserved */
		[136] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[137] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[138] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,
		/* [139] - [299] reserved */

		/* PPPoE */
		[300] = RTE_PTYPE_L2_ETHER_PPPOE,
		[301] = RTE_PTYPE_L2_ETHER_PPPOE,

		/* PPPoE --> IPv4 */
		[302] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_FRAG,
		[303] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_NONFRAG,
		[304] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[305] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_TCP,
		[306] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_SCTP,
		[307] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_ICMP,

		/* PPPoE --> IPv6 */
		[308] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_FRAG,
		[309] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_NONFRAG,
		[310] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[311] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_TCP,
		[312] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_SCTP,
		[313] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_ICMP,
		/* [314] - [324] reserved */

		/* IPv4/IPv6 --> GTPC/GTPU */
		[325] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPC,
		[326] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPC,
		[327] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPC,
		[328] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPC,
		[329] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU,
		[330] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU,

		/* IPv4 --> GTPU --> IPv4 */
		[331] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[332] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[333] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		[334] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[335] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GTPU --> IPv4 */
		[336] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[337] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[338] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		[339] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[340] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GTPU --> IPv6 */
		[341] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[342] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[343] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		[344] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[345] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GTPU --> IPv6 */
		[346] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[347] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[348] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		[349] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[350] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> UDP ECPRI */
		[372] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[373] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[374] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[375] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[376] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[377] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[378] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[379] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[380] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[381] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,

		/* IPV6 --> UDP ECPRI */
		[382] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[383] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[384] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[385] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[386] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[387] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[388] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[389] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[390] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[391] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		/* All others reserved */
	};

	return type_table[ptype];
}

void __rte_cold
ice_set_default_ptype_table(struct rte_eth_dev *dev)
{
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int i;

	for (i = 0; i < ICE_MAX_PKT_TYPE; i++)
		ad->ptype_tbl[i] = ice_get_default_pkt_type(i);
}

#define ICE_RX_PROG_STATUS_DESC_WB_QW1_PROGID_S	1
#define ICE_RX_PROG_STATUS_DESC_WB_QW1_PROGID_M	\
			(0x3UL << ICE_RX_PROG_STATUS_DESC_WB_QW1_PROGID_S)
#define ICE_RX_PROG_STATUS_DESC_WB_QW1_PROG_ADD 0
#define ICE_RX_PROG_STATUS_DESC_WB_QW1_PROG_DEL 0x1

#define ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_S	4
#define ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_M	\
	(1 << ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_S)
#define ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_PROF_S	5
#define ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_PROF_M	\
	(1 << ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_PROF_S)

/*
 * check the programming status descriptor in rx queue.
 * done after Programming Flow Director is programmed on
 * tx queue
 */
static inline int
ice_check_fdir_programming_status(struct ci_rx_queue *rxq)
{
	volatile union ice_32byte_rx_desc *rxdp;
	uint64_t qword1;
	uint32_t rx_status;
	uint32_t error;
	uint32_t id;
	int ret = -EAGAIN;

	rxdp = (volatile union ice_32byte_rx_desc *)&rxq->rx_flex_ring[rxq->rx_tail];
	qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
	rx_status = (qword1 & ICE_RXD_QW1_STATUS_M)
			>> ICE_RXD_QW1_STATUS_S;

	if (rx_status & (1 << ICE_RX_DESC_STATUS_DD_S)) {
		ret = 0;
		error = (qword1 & ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_M) >>
			ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_S;
		id = (qword1 & ICE_RX_PROG_STATUS_DESC_WB_QW1_PROGID_M) >>
			ICE_RX_PROG_STATUS_DESC_WB_QW1_PROGID_S;
		if (error) {
			if (id == ICE_RX_PROG_STATUS_DESC_WB_QW1_PROG_ADD)
				PMD_DRV_LOG(ERR, "Failed to add FDIR rule.");
			else if (id == ICE_RX_PROG_STATUS_DESC_WB_QW1_PROG_DEL)
				PMD_DRV_LOG(ERR, "Failed to remove FDIR rule.");
			ret = -EINVAL;
			goto err;
		}
		error = (qword1 & ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_PROF_M) >>
			ICE_RX_PROG_STATUS_DESC_WB_QW1_FAIL_PROF_S;
		if (error) {
			PMD_DRV_LOG(ERR, "Failed to create FDIR profile.");
			ret = -EINVAL;
		}
err:
		rxdp->wb.qword1.status_error_len = 0;
		rxq->rx_tail++;
		if (unlikely(rxq->rx_tail == rxq->nb_rx_desc))
			rxq->rx_tail = 0;
		if (rxq->rx_tail == 0)
			ICE_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
		else
			ICE_PCI_REG_WRITE(rxq->qrx_tail, rxq->rx_tail - 1);
	}

	return ret;
}

#define ICE_FDIR_MAX_WAIT_US 10000

int
ice_fdir_programming(struct ice_pf *pf, struct ice_fltr_desc *fdir_desc)
{
	struct ci_tx_queue *txq = pf->fdir.txq;
	struct ci_rx_queue *rxq = pf->fdir.rxq;
	volatile struct ice_fltr_desc *fdirdp;
	volatile struct ice_tx_desc *txdp;
	uint32_t td_cmd;
	uint16_t i;

	fdirdp = (volatile struct ice_fltr_desc *)
		(&txq->ice_tx_ring[txq->tx_tail]);
	fdirdp->qidx_compq_space_stat = fdir_desc->qidx_compq_space_stat;
	fdirdp->dtype_cmd_vsi_fdid = fdir_desc->dtype_cmd_vsi_fdid;

	txdp = &txq->ice_tx_ring[txq->tx_tail + 1];
	txdp->buf_addr = rte_cpu_to_le_64(pf->fdir.dma_addr);
	td_cmd = ICE_TX_DESC_CMD_EOP |
		ICE_TX_DESC_CMD_RS  |
		ICE_TX_DESC_CMD_DUMMY;

	txdp->cmd_type_offset_bsz =
		ice_build_ctob(td_cmd, 0, ICE_FDIR_PKT_LEN, 0);

	txq->tx_tail += 2;
	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;
	/* Update the tx tail register */
	ICE_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);
	for (i = 0; i < ICE_FDIR_MAX_WAIT_US; i++) {
		if ((txdp->cmd_type_offset_bsz &
		     rte_cpu_to_le_64(ICE_TXD_QW1_DTYPE_M)) ==
		    rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DESC_DONE))
			break;
		rte_delay_us(1);
	}
	if (i >= ICE_FDIR_MAX_WAIT_US) {
		PMD_DRV_LOG(ERR,
			    "Failed to program FDIR filter: time out to get DD on tx queue.");
		return -ETIMEDOUT;
	}

	for (; i < ICE_FDIR_MAX_WAIT_US; i++) {
		int ret;

		ret = ice_check_fdir_programming_status(rxq);
		if (ret == -EAGAIN)
			rte_delay_us(1);
		else
			return ret;
	}

	PMD_DRV_LOG(ERR,
		    "Failed to program FDIR filter: programming status reported.");
	return -ETIMEDOUT;


}
