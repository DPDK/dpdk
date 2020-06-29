/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_eventdev_pmd_pci.h>
#include <rte_kvargs.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_pci.h>

#include "otx2_evdev_stats.h"
#include "otx2_evdev.h"
#include "otx2_irq.h"
#include "otx2_tim_evdev.h"

static inline int
sso_get_msix_offsets(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint8_t nb_ports = dev->nb_event_ports * (dev->dual_ws ? 2 : 1);
	struct otx2_mbox *mbox = dev->mbox;
	struct msix_offset_rsp *msix_rsp;
	int i, rc;

	/* Get SSO and SSOW MSIX vector offsets */
	otx2_mbox_alloc_msg_msix_offset(mbox);
	rc = otx2_mbox_process_msg(mbox, (void *)&msix_rsp);

	for (i = 0; i < nb_ports; i++)
		dev->ssow_msixoff[i] = msix_rsp->ssow_msixoff[i];

	for (i = 0; i < dev->nb_event_queues; i++)
		dev->sso_msixoff[i] = msix_rsp->sso_msixoff[i];

	return rc;
}

void
sso_fastpath_fns_set(struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	/* Single WS modes */
	const event_dequeue_t ssogws_deq[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] = otx2_ssogws_deq_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_burst_t ssogws_deq_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] = otx2_ssogws_deq_burst_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_t ssogws_deq_timeout[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] = otx2_ssogws_deq_timeout_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_burst_t
		ssogws_deq_timeout_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_deq_timeout_burst_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_t ssogws_deq_seg[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] = otx2_ssogws_deq_seg_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_burst_t
		ssogws_deq_seg_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_deq_seg_burst_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_t ssogws_deq_seg_timeout[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_deq_seg_timeout_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_burst_t
		ssogws_deq_seg_timeout_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
				otx2_ssogws_deq_seg_timeout_burst_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};


	/* Dual WS modes */
	const event_dequeue_t ssogws_dual_deq[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] = otx2_ssogws_dual_deq_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_burst_t
		ssogws_dual_deq_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_dual_deq_burst_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_t ssogws_dual_deq_timeout[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_dual_deq_timeout_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_burst_t
		ssogws_dual_deq_timeout_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
	[f6][f5][f4][f3][f2][f1][f0] =					\
			otx2_ssogws_dual_deq_timeout_burst_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_t ssogws_dual_deq_seg[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] = otx2_ssogws_dual_deq_seg_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_burst_t
		ssogws_dual_deq_seg_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_dual_deq_seg_burst_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_t
		ssogws_dual_deq_seg_timeout[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_dual_deq_seg_timeout_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	const event_dequeue_burst_t
		ssogws_dual_deq_seg_timeout_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_dual_deq_seg_timeout_burst_ ##name,
SSO_RX_ADPTR_ENQ_FASTPATH_FUNC
#undef R
	};

	/* Tx modes */
	const event_tx_adapter_enqueue
		ssogws_tx_adptr_enq[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_tx_adptr_enq_ ## name,
SSO_TX_ADPTR_ENQ_FASTPATH_FUNC
#undef T
	};

	const event_tx_adapter_enqueue
		ssogws_tx_adptr_enq_seg[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_tx_adptr_enq_seg_ ## name,
SSO_TX_ADPTR_ENQ_FASTPATH_FUNC
#undef T
	};

	const event_tx_adapter_enqueue
		ssogws_dual_tx_adptr_enq[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_dual_tx_adptr_enq_ ## name,
SSO_TX_ADPTR_ENQ_FASTPATH_FUNC
#undef T
	};

	const event_tx_adapter_enqueue
		ssogws_dual_tx_adptr_enq_seg[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
		[f6][f5][f4][f3][f2][f1][f0] =				\
			otx2_ssogws_dual_tx_adptr_enq_seg_ ## name,
SSO_TX_ADPTR_ENQ_FASTPATH_FUNC
#undef T
	};

	event_dev->enqueue			= otx2_ssogws_enq;
	event_dev->enqueue_burst		= otx2_ssogws_enq_burst;
	event_dev->enqueue_new_burst		= otx2_ssogws_enq_new_burst;
	event_dev->enqueue_forward_burst	= otx2_ssogws_enq_fwd_burst;
	if (dev->rx_offloads & NIX_RX_MULTI_SEG_F) {
		event_dev->dequeue		= ssogws_deq_seg
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_SECURITY_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_CHECKSUM_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
		event_dev->dequeue_burst	= ssogws_deq_seg_burst
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_SECURITY_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_CHECKSUM_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
		if (dev->is_timeout_deq) {
			event_dev->dequeue	= ssogws_deq_seg_timeout
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_SECURITY_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_CHECKSUM_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
			event_dev->dequeue_burst	=
				ssogws_deq_seg_timeout_burst
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_SECURITY_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_CHECKSUM_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
		}
	} else {
		event_dev->dequeue			= ssogws_deq
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_SECURITY_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_CHECKSUM_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
		event_dev->dequeue_burst		= ssogws_deq_burst
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_SECURITY_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_CHECKSUM_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
		if (dev->is_timeout_deq) {
			event_dev->dequeue		= ssogws_deq_timeout
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_SECURITY_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_CHECKSUM_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
			event_dev->dequeue_burst	=
				ssogws_deq_timeout_burst
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_SECURITY_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_CHECKSUM_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
			[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
		}
	}

	if (dev->tx_offloads & NIX_TX_MULTI_SEG_F) {
		/* [SEC] [TSMP] [MBUF_NOFF] [VLAN] [OL3_L4_CSUM] [L3_L4_CSUM] */
		event_dev->txa_enqueue = ssogws_tx_adptr_enq_seg
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_SECURITY_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSO_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSTAMP_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_MBUF_NOFF_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_VLAN_QINQ_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_L3_L4_CSUM_F)];
	} else {
		event_dev->txa_enqueue = ssogws_tx_adptr_enq
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_SECURITY_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSO_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSTAMP_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_MBUF_NOFF_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_VLAN_QINQ_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)]
			[!!(dev->tx_offloads & NIX_TX_OFFLOAD_L3_L4_CSUM_F)];
	}

	if (dev->dual_ws) {
		event_dev->enqueue		= otx2_ssogws_dual_enq;
		event_dev->enqueue_burst	= otx2_ssogws_dual_enq_burst;
		event_dev->enqueue_new_burst	=
					otx2_ssogws_dual_enq_new_burst;
		event_dev->enqueue_forward_burst =
					otx2_ssogws_dual_enq_fwd_burst;

		if (dev->rx_offloads & NIX_RX_MULTI_SEG_F) {
			event_dev->dequeue	= ssogws_dual_deq_seg
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_SECURITY_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_TSTAMP_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_MARK_UPDATE_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_VLAN_STRIP_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_CHECKSUM_F)]
				[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
				[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
			event_dev->dequeue_burst = ssogws_dual_deq_seg_burst
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_SECURITY_F)]
				[!!(dev->rx_offloads & NIX_RX_OFFLOAD_TSTAMP_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_MARK_UPDATE_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_VLAN_STRIP_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_CHECKSUM_F)]
				[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
				[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
			if (dev->is_timeout_deq) {
				event_dev->dequeue	=
					ssogws_dual_deq_seg_timeout
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_SECURITY_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_TSTAMP_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_MARK_UPDATE_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_VLAN_STRIP_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_CHECKSUM_F)]
					[!!(dev->rx_offloads &
							NIX_RX_OFFLOAD_PTYPE_F)]
					[!!(dev->rx_offloads &
							NIX_RX_OFFLOAD_RSS_F)];
				event_dev->dequeue_burst =
					ssogws_dual_deq_seg_timeout_burst
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_SECURITY_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_TSTAMP_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_MARK_UPDATE_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_VLAN_STRIP_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_CHECKSUM_F)]
					[!!(dev->rx_offloads &
							NIX_RX_OFFLOAD_PTYPE_F)]
					[!!(dev->rx_offloads &
							NIX_RX_OFFLOAD_RSS_F)];
			}
		} else {
			event_dev->dequeue		= ssogws_dual_deq
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_SECURITY_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_TSTAMP_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_MARK_UPDATE_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_VLAN_STRIP_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_CHECKSUM_F)]
				[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
				[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
			event_dev->dequeue_burst	= ssogws_dual_deq_burst
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_SECURITY_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_TSTAMP_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_MARK_UPDATE_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_VLAN_STRIP_F)]
				[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_CHECKSUM_F)]
				[!!(dev->rx_offloads & NIX_RX_OFFLOAD_PTYPE_F)]
				[!!(dev->rx_offloads & NIX_RX_OFFLOAD_RSS_F)];
			if (dev->is_timeout_deq) {
				event_dev->dequeue	=
					ssogws_dual_deq_timeout
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_SECURITY_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_TSTAMP_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_MARK_UPDATE_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_VLAN_STRIP_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_CHECKSUM_F)]
					[!!(dev->rx_offloads &
							NIX_RX_OFFLOAD_PTYPE_F)]
					[!!(dev->rx_offloads &
							NIX_RX_OFFLOAD_RSS_F)];
				event_dev->dequeue_burst =
					ssogws_dual_deq_timeout_burst
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_SECURITY_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_TSTAMP_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_MARK_UPDATE_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_VLAN_STRIP_F)]
					[!!(dev->rx_offloads &
						NIX_RX_OFFLOAD_CHECKSUM_F)]
					[!!(dev->rx_offloads &
							NIX_RX_OFFLOAD_PTYPE_F)]
					[!!(dev->rx_offloads &
							NIX_RX_OFFLOAD_RSS_F)];
			}
		}

		if (dev->tx_offloads & NIX_TX_MULTI_SEG_F) {
		/* [SEC] [TSMP] [MBUF_NOFF] [VLAN] [OL3_L4_CSUM] [L3_L4_CSUM] */
			event_dev->txa_enqueue = ssogws_dual_tx_adptr_enq_seg
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_SECURITY_F)]
				[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSO_F)]
				[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSTAMP_F)]
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_MBUF_NOFF_F)]
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_VLAN_QINQ_F)]
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)]
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_L3_L4_CSUM_F)];
		} else {
			event_dev->txa_enqueue = ssogws_dual_tx_adptr_enq
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_SECURITY_F)]
				[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSO_F)]
				[!!(dev->tx_offloads & NIX_TX_OFFLOAD_TSTAMP_F)]
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_MBUF_NOFF_F)]
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_VLAN_QINQ_F)]
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)]
				[!!(dev->tx_offloads &
						NIX_TX_OFFLOAD_L3_L4_CSUM_F)];
		}
	}

	event_dev->txa_enqueue_same_dest = event_dev->txa_enqueue;
	rte_mb();
}

static void
otx2_sso_info_get(struct rte_eventdev *event_dev,
		  struct rte_event_dev_info *dev_info)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_OCTEONTX2_PMD);
	dev_info->min_dequeue_timeout_ns = dev->min_dequeue_timeout_ns;
	dev_info->max_dequeue_timeout_ns = dev->max_dequeue_timeout_ns;
	dev_info->max_event_queues = dev->max_event_queues;
	dev_info->max_event_queue_flows = (1ULL << 20);
	dev_info->max_event_queue_priority_levels = 8;
	dev_info->max_event_priority_levels = 1;
	dev_info->max_event_ports = dev->max_event_ports;
	dev_info->max_event_port_dequeue_depth = 1;
	dev_info->max_event_port_enqueue_depth = 1;
	dev_info->max_num_events =  dev->max_num_events;
	dev_info->event_dev_cap = RTE_EVENT_DEV_CAP_QUEUE_QOS |
					RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
					RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES |
					RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK |
					RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
					RTE_EVENT_DEV_CAP_NONSEQ_MODE;
}

static void
sso_port_link_modify(struct otx2_ssogws *ws, uint8_t queue, uint8_t enable)
{
	uintptr_t base = OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op);
	uint64_t val;

	val = queue;
	val |= 0ULL << 12; /* SET 0 */
	val |= 0x8000800080000000; /* Dont modify rest of the masks */
	val |= (uint64_t)enable << 14;   /* Enable/Disable Membership. */

	otx2_write64(val, base + SSOW_LF_GWS_GRPMSK_CHG);
}

static int
otx2_sso_port_link(struct rte_eventdev *event_dev, void *port,
		   const uint8_t queues[], const uint8_t priorities[],
		   uint16_t nb_links)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint8_t port_id = 0;
	uint16_t link;

	RTE_SET_USED(priorities);
	for (link = 0; link < nb_links; link++) {
		if (dev->dual_ws) {
			struct otx2_ssogws_dual *ws = port;

			port_id = ws->port;
			sso_port_link_modify((struct otx2_ssogws *)
					&ws->ws_state[0], queues[link], true);
			sso_port_link_modify((struct otx2_ssogws *)
					&ws->ws_state[1], queues[link], true);
		} else {
			struct otx2_ssogws *ws = port;

			port_id = ws->port;
			sso_port_link_modify(ws, queues[link], true);
		}
	}
	sso_func_trace("Port=%d nb_links=%d", port_id, nb_links);

	return (int)nb_links;
}

static int
otx2_sso_port_unlink(struct rte_eventdev *event_dev, void *port,
		     uint8_t queues[], uint16_t nb_unlinks)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint8_t port_id = 0;
	uint16_t unlink;

	for (unlink = 0; unlink < nb_unlinks; unlink++) {
		if (dev->dual_ws) {
			struct otx2_ssogws_dual *ws = port;

			port_id = ws->port;
			sso_port_link_modify((struct otx2_ssogws *)
					&ws->ws_state[0], queues[unlink],
					false);
			sso_port_link_modify((struct otx2_ssogws *)
					&ws->ws_state[1], queues[unlink],
					false);
		} else {
			struct otx2_ssogws *ws = port;

			port_id = ws->port;
			sso_port_link_modify(ws, queues[unlink], false);
		}
	}
	sso_func_trace("Port=%d nb_unlinks=%d", port_id, nb_unlinks);

	return (int)nb_unlinks;
}

static int
sso_hw_lf_cfg(struct otx2_mbox *mbox, enum otx2_sso_lf_type type,
	      uint16_t nb_lf, uint8_t attach)
{
	if (attach) {
		struct rsrc_attach_req *req;

		req = otx2_mbox_alloc_msg_attach_resources(mbox);
		switch (type) {
		case SSO_LF_GGRP:
			req->sso = nb_lf;
			break;
		case SSO_LF_GWS:
			req->ssow = nb_lf;
			break;
		default:
			return -EINVAL;
		}
		req->modify = true;
		if (otx2_mbox_process(mbox) < 0)
			return -EIO;
	} else {
		struct rsrc_detach_req *req;

		req = otx2_mbox_alloc_msg_detach_resources(mbox);
		switch (type) {
		case SSO_LF_GGRP:
			req->sso = true;
			break;
		case SSO_LF_GWS:
			req->ssow = true;
			break;
		default:
			return -EINVAL;
		}
		req->partial = true;
		if (otx2_mbox_process(mbox) < 0)
			return -EIO;
	}

	return 0;
}

static int
sso_lf_cfg(struct otx2_sso_evdev *dev, struct otx2_mbox *mbox,
	   enum otx2_sso_lf_type type, uint16_t nb_lf, uint8_t alloc)
{
	void *rsp;
	int rc;

	if (alloc) {
		switch (type) {
		case SSO_LF_GGRP:
			{
			struct sso_lf_alloc_req *req_ggrp;
			req_ggrp = otx2_mbox_alloc_msg_sso_lf_alloc(mbox);
			req_ggrp->hwgrps = nb_lf;
			}
			break;
		case SSO_LF_GWS:
			{
			struct ssow_lf_alloc_req *req_hws;
			req_hws = otx2_mbox_alloc_msg_ssow_lf_alloc(mbox);
			req_hws->hws = nb_lf;
			}
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (type) {
		case SSO_LF_GGRP:
			{
			struct sso_lf_free_req *req_ggrp;
			req_ggrp = otx2_mbox_alloc_msg_sso_lf_free(mbox);
			req_ggrp->hwgrps = nb_lf;
			}
			break;
		case SSO_LF_GWS:
			{
			struct ssow_lf_free_req *req_hws;
			req_hws = otx2_mbox_alloc_msg_ssow_lf_free(mbox);
			req_hws->hws = nb_lf;
			}
			break;
		default:
			return -EINVAL;
		}
	}

	rc = otx2_mbox_process_msg_tmo(mbox, (void **)&rsp, ~0);
	if (rc < 0)
		return rc;

	if (alloc && type == SSO_LF_GGRP) {
		struct sso_lf_alloc_rsp *rsp_ggrp = rsp;

		dev->xaq_buf_size = rsp_ggrp->xaq_buf_size;
		dev->xae_waes = rsp_ggrp->xaq_wq_entries;
		dev->iue = rsp_ggrp->in_unit_entries;
	}

	return 0;
}

static void
otx2_sso_port_release(void *port)
{
	rte_free(port);
}

static void
otx2_sso_queue_release(struct rte_eventdev *event_dev, uint8_t queue_id)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(queue_id);
}

static void
sso_clr_links(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	int i, j;

	for (i = 0; i < dev->nb_event_ports; i++) {
		if (dev->dual_ws) {
			struct otx2_ssogws_dual *ws;

			ws = event_dev->data->ports[i];
			for (j = 0; j < dev->nb_event_queues; j++) {
				sso_port_link_modify((struct otx2_ssogws *)
						&ws->ws_state[0], j, false);
				sso_port_link_modify((struct otx2_ssogws *)
						&ws->ws_state[1], j, false);
			}
		} else {
			struct otx2_ssogws *ws;

			ws = event_dev->data->ports[i];
			for (j = 0; j < dev->nb_event_queues; j++)
				sso_port_link_modify(ws, j, false);
		}
	}
}

static void
sso_restore_links(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint16_t *links_map;
	int i, j;

	for (i = 0; i < dev->nb_event_ports; i++) {
		links_map = event_dev->data->links_map;
		/* Point links_map to this port specific area */
		links_map += (i * RTE_EVENT_MAX_QUEUES_PER_DEV);
		if (dev->dual_ws) {
			struct otx2_ssogws_dual *ws;

			ws = event_dev->data->ports[i];
			for (j = 0; j < dev->nb_event_queues; j++) {
				if (links_map[j] == 0xdead)
					continue;
				sso_port_link_modify((struct otx2_ssogws *)
						&ws->ws_state[0], j, true);
				sso_port_link_modify((struct otx2_ssogws *)
						&ws->ws_state[1], j, true);
				sso_func_trace("Restoring port %d queue %d "
						"link", i, j);
			}
		} else {
			struct otx2_ssogws *ws;

			ws = event_dev->data->ports[i];
			for (j = 0; j < dev->nb_event_queues; j++) {
				if (links_map[j] == 0xdead)
					continue;
				sso_port_link_modify(ws, j, true);
				sso_func_trace("Restoring port %d queue %d "
						"link", i, j);
			}
		}
	}
}

static void
sso_set_port_ops(struct otx2_ssogws *ws, uintptr_t base)
{
	ws->tag_op		= base + SSOW_LF_GWS_TAG;
	ws->wqp_op		= base + SSOW_LF_GWS_WQP;
	ws->getwrk_op		= base + SSOW_LF_GWS_OP_GET_WORK;
	ws->swtp_op		= base + SSOW_LF_GWS_SWTP;
	ws->swtag_norm_op	= base + SSOW_LF_GWS_OP_SWTAG_NORM;
	ws->swtag_desched_op	= base + SSOW_LF_GWS_OP_SWTAG_DESCHED;
}

static int
sso_configure_dual_ports(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_mbox *mbox = dev->mbox;
	uint8_t vws = 0;
	uint8_t nb_lf;
	int i, rc;

	otx2_sso_dbg("Configuring event ports %d", dev->nb_event_ports);

	nb_lf = dev->nb_event_ports * 2;
	/* Ask AF to attach required LFs. */
	rc = sso_hw_lf_cfg(mbox, SSO_LF_GWS, nb_lf, true);
	if (rc < 0) {
		otx2_err("Failed to attach SSO GWS LF");
		return -ENODEV;
	}

	if (sso_lf_cfg(dev, mbox, SSO_LF_GWS, nb_lf, true) < 0) {
		sso_hw_lf_cfg(mbox, SSO_LF_GWS, nb_lf, false);
		otx2_err("Failed to init SSO GWS LF");
		return -ENODEV;
	}

	for (i = 0; i < dev->nb_event_ports; i++) {
		struct otx2_ssogws_dual *ws;
		uintptr_t base;

		if (event_dev->data->ports[i] != NULL) {
			ws = event_dev->data->ports[i];
		} else {
			/* Allocate event port memory */
			ws = rte_zmalloc_socket("otx2_sso_ws",
					sizeof(struct otx2_ssogws_dual),
					RTE_CACHE_LINE_SIZE,
					event_dev->data->socket_id);
		}
		if (ws == NULL) {
			otx2_err("Failed to alloc memory for port=%d", i);
			rc = -ENOMEM;
			break;
		}

		ws->port = i;
		base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | vws << 12);
		sso_set_port_ops((struct otx2_ssogws *)&ws->ws_state[0], base);
		vws++;

		base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | vws << 12);
		sso_set_port_ops((struct otx2_ssogws *)&ws->ws_state[1], base);
		vws++;

		event_dev->data->ports[i] = ws;
	}

	if (rc < 0) {
		sso_lf_cfg(dev, mbox, SSO_LF_GWS, nb_lf, false);
		sso_hw_lf_cfg(mbox, SSO_LF_GWS, nb_lf, false);
	}

	return rc;
}

static int
sso_configure_ports(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_mbox *mbox = dev->mbox;
	uint8_t nb_lf;
	int i, rc;

	otx2_sso_dbg("Configuring event ports %d", dev->nb_event_ports);

	nb_lf = dev->nb_event_ports;
	/* Ask AF to attach required LFs. */
	rc = sso_hw_lf_cfg(mbox, SSO_LF_GWS, nb_lf, true);
	if (rc < 0) {
		otx2_err("Failed to attach SSO GWS LF");
		return -ENODEV;
	}

	if (sso_lf_cfg(dev, mbox, SSO_LF_GWS, nb_lf, true) < 0) {
		sso_hw_lf_cfg(mbox, SSO_LF_GWS, nb_lf, false);
		otx2_err("Failed to init SSO GWS LF");
		return -ENODEV;
	}

	for (i = 0; i < nb_lf; i++) {
		struct otx2_ssogws *ws;
		uintptr_t base;

		/* Free memory prior to re-allocation if needed */
		if (event_dev->data->ports[i] != NULL) {
			ws = event_dev->data->ports[i];
			rte_free(ws);
			ws = NULL;
		}

		/* Allocate event port memory */
		ws = rte_zmalloc_socket("otx2_sso_ws",
					sizeof(struct otx2_ssogws),
					RTE_CACHE_LINE_SIZE,
					event_dev->data->socket_id);
		if (ws == NULL) {
			otx2_err("Failed to alloc memory for port=%d", i);
			rc = -ENOMEM;
			break;
		}

		ws->port = i;
		base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | i << 12);
		sso_set_port_ops(ws, base);

		event_dev->data->ports[i] = ws;
	}

	if (rc < 0) {
		sso_lf_cfg(dev, mbox, SSO_LF_GWS, nb_lf, false);
		sso_hw_lf_cfg(mbox, SSO_LF_GWS, nb_lf, false);
	}

	return rc;
}

static int
sso_configure_queues(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_mbox *mbox = dev->mbox;
	uint8_t nb_lf;
	int rc;

	otx2_sso_dbg("Configuring event queues %d", dev->nb_event_queues);

	nb_lf = dev->nb_event_queues;
	/* Ask AF to attach required LFs. */
	rc = sso_hw_lf_cfg(mbox, SSO_LF_GGRP, nb_lf, true);
	if (rc < 0) {
		otx2_err("Failed to attach SSO GGRP LF");
		return -ENODEV;
	}

	if (sso_lf_cfg(dev, mbox, SSO_LF_GGRP, nb_lf, true) < 0) {
		sso_hw_lf_cfg(mbox, SSO_LF_GGRP, nb_lf, false);
		otx2_err("Failed to init SSO GGRP LF");
		return -ENODEV;
	}

	return rc;
}

static int
sso_xaq_allocate(struct otx2_sso_evdev *dev)
{
	const struct rte_memzone *mz;
	struct npa_aura_s *aura;
	static int reconfig_cnt;
	char pool_name[RTE_MEMZONE_NAMESIZE];
	uint32_t xaq_cnt;
	int rc;

	if (dev->xaq_pool)
		rte_mempool_free(dev->xaq_pool);

	/*
	 * Allocate memory for Add work backpressure.
	 */
	mz = rte_memzone_lookup(OTX2_SSO_FC_NAME);
	if (mz == NULL)
		mz = rte_memzone_reserve_aligned(OTX2_SSO_FC_NAME,
						 OTX2_ALIGN +
						 sizeof(struct npa_aura_s),
						 rte_socket_id(),
						 RTE_MEMZONE_IOVA_CONTIG,
						 OTX2_ALIGN);
	if (mz == NULL) {
		otx2_err("Failed to allocate mem for fcmem");
		return -ENOMEM;
	}

	dev->fc_iova = mz->iova;
	dev->fc_mem = mz->addr;

	aura = (struct npa_aura_s *)((uintptr_t)dev->fc_mem + OTX2_ALIGN);
	memset(aura, 0, sizeof(struct npa_aura_s));

	aura->fc_ena = 1;
	aura->fc_addr = dev->fc_iova;
	aura->fc_hyst_bits = 0; /* Store count on all updates */

	/* Taken from HRM 14.3.3(4) */
	xaq_cnt = dev->nb_event_queues * OTX2_SSO_XAQ_CACHE_CNT;
	if (dev->xae_cnt)
		xaq_cnt += dev->xae_cnt / dev->xae_waes;
	else if (dev->adptr_xae_cnt)
		xaq_cnt += (dev->adptr_xae_cnt / dev->xae_waes) +
			(OTX2_SSO_XAQ_SLACK * dev->nb_event_queues);
	else
		xaq_cnt += (dev->iue / dev->xae_waes) +
			(OTX2_SSO_XAQ_SLACK * dev->nb_event_queues);

	otx2_sso_dbg("Configuring %d xaq buffers", xaq_cnt);
	/* Setup XAQ based on number of nb queues. */
	snprintf(pool_name, 30, "otx2_xaq_buf_pool_%d", reconfig_cnt);
	dev->xaq_pool = (void *)rte_mempool_create_empty(pool_name,
			xaq_cnt, dev->xaq_buf_size, 0, 0,
			rte_socket_id(), 0);

	if (dev->xaq_pool == NULL) {
		otx2_err("Unable to create empty mempool.");
		rte_memzone_free(mz);
		return -ENOMEM;
	}

	rc = rte_mempool_set_ops_byname(dev->xaq_pool,
					rte_mbuf_platform_mempool_ops(), aura);
	if (rc != 0) {
		otx2_err("Unable to set xaqpool ops.");
		goto alloc_fail;
	}

	rc = rte_mempool_populate_default(dev->xaq_pool);
	if (rc < 0) {
		otx2_err("Unable to set populate xaqpool.");
		goto alloc_fail;
	}
	reconfig_cnt++;
	/* When SW does addwork (enqueue) check if there is space in XAQ by
	 * comparing fc_addr above against the xaq_lmt calculated below.
	 * There should be a minimum headroom (OTX2_SSO_XAQ_SLACK / 2) for SSO
	 * to request XAQ to cache them even before enqueue is called.
	 */
	dev->xaq_lmt = xaq_cnt - (OTX2_SSO_XAQ_SLACK / 2 *
				  dev->nb_event_queues);
	dev->nb_xaq_cfg = xaq_cnt;

	return 0;
alloc_fail:
	rte_mempool_free(dev->xaq_pool);
	rte_memzone_free(mz);
	return rc;
}

static int
sso_ggrp_alloc_xaq(struct otx2_sso_evdev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct sso_hw_setconfig *req;

	otx2_sso_dbg("Configuring XAQ for GGRPs");
	req = otx2_mbox_alloc_msg_sso_hw_setconfig(mbox);
	req->npa_pf_func = otx2_npa_pf_func_get();
	req->npa_aura_id = npa_lf_aura_handle_to_aura(dev->xaq_pool->pool_id);
	req->hwgrps = dev->nb_event_queues;

	return otx2_mbox_process(mbox);
}

static void
sso_lf_teardown(struct otx2_sso_evdev *dev,
		enum otx2_sso_lf_type lf_type)
{
	uint8_t nb_lf;

	switch (lf_type) {
	case SSO_LF_GGRP:
		nb_lf = dev->nb_event_queues;
		break;
	case SSO_LF_GWS:
		nb_lf = dev->nb_event_ports;
		nb_lf *= dev->dual_ws ? 2 : 1;
		break;
	default:
		return;
	}

	sso_lf_cfg(dev, dev->mbox, lf_type, nb_lf, false);
	sso_hw_lf_cfg(dev->mbox, lf_type, nb_lf, false);
}

static int
otx2_sso_configure(const struct rte_eventdev *event_dev)
{
	struct rte_event_dev_config *conf = &event_dev->data->dev_conf;
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint32_t deq_tmo_ns;
	int rc;

	sso_func_trace();
	deq_tmo_ns = conf->dequeue_timeout_ns;

	if (deq_tmo_ns == 0)
		deq_tmo_ns = dev->min_dequeue_timeout_ns;

	if (deq_tmo_ns < dev->min_dequeue_timeout_ns ||
	    deq_tmo_ns > dev->max_dequeue_timeout_ns) {
		otx2_err("Unsupported dequeue timeout requested");
		return -EINVAL;
	}

	if (conf->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT)
		dev->is_timeout_deq = 1;

	dev->deq_tmo_ns = deq_tmo_ns;

	if (conf->nb_event_ports > dev->max_event_ports ||
	    conf->nb_event_queues > dev->max_event_queues) {
		otx2_err("Unsupported event queues/ports requested");
		return -EINVAL;
	}

	if (conf->nb_event_port_dequeue_depth > 1) {
		otx2_err("Unsupported event port deq depth requested");
		return -EINVAL;
	}

	if (conf->nb_event_port_enqueue_depth > 1) {
		otx2_err("Unsupported event port enq depth requested");
		return -EINVAL;
	}

	if (dev->configured) {
		sso_unregister_irqs(event_dev);
		/* Clear any prior port-queue mapping. */
		sso_clr_links(event_dev);
	}

	if (dev->nb_event_queues) {
		/* Finit any previous queues. */
		sso_lf_teardown(dev, SSO_LF_GGRP);
	}
	if (dev->nb_event_ports) {
		/* Finit any previous ports. */
		sso_lf_teardown(dev, SSO_LF_GWS);
	}

	dev->nb_event_queues = conf->nb_event_queues;
	dev->nb_event_ports = conf->nb_event_ports;

	if (dev->dual_ws)
		rc = sso_configure_dual_ports(event_dev);
	else
		rc = sso_configure_ports(event_dev);

	if (rc < 0) {
		otx2_err("Failed to configure event ports");
		return -ENODEV;
	}

	if (sso_configure_queues(event_dev) < 0) {
		otx2_err("Failed to configure event queues");
		rc = -ENODEV;
		goto teardown_hws;
	}

	if (sso_xaq_allocate(dev) < 0) {
		rc = -ENOMEM;
		goto teardown_hwggrp;
	}

	/* Restore any prior port-queue mapping. */
	sso_restore_links(event_dev);
	rc = sso_ggrp_alloc_xaq(dev);
	if (rc < 0) {
		otx2_err("Failed to alloc xaq to ggrp %d", rc);
		goto teardown_hwggrp;
	}

	rc = sso_get_msix_offsets(event_dev);
	if (rc < 0) {
		otx2_err("Failed to get msix offsets %d", rc);
		goto teardown_hwggrp;
	}

	rc = sso_register_irqs(event_dev);
	if (rc < 0) {
		otx2_err("Failed to register irq %d", rc);
		goto teardown_hwggrp;
	}

	dev->configured = 1;
	rte_mb();

	return 0;
teardown_hwggrp:
	sso_lf_teardown(dev, SSO_LF_GGRP);
teardown_hws:
	sso_lf_teardown(dev, SSO_LF_GWS);
	dev->nb_event_queues = 0;
	dev->nb_event_ports = 0;
	dev->configured = 0;
	return rc;
}

static void
otx2_sso_queue_def_conf(struct rte_eventdev *event_dev, uint8_t queue_id,
			struct rte_event_queue_conf *queue_conf)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(queue_id);

	queue_conf->nb_atomic_flows = (1ULL << 20);
	queue_conf->nb_atomic_order_sequences = (1ULL << 20);
	queue_conf->event_queue_cfg = RTE_EVENT_QUEUE_CFG_ALL_TYPES;
	queue_conf->priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
}

static int
otx2_sso_queue_setup(struct rte_eventdev *event_dev, uint8_t queue_id,
		     const struct rte_event_queue_conf *queue_conf)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct sso_grp_priority *req;
	int rc;

	sso_func_trace("Queue=%d prio=%d", queue_id, queue_conf->priority);

	req = otx2_mbox_alloc_msg_sso_grp_set_priority(dev->mbox);
	req->grp = queue_id;
	req->weight = 0xFF;
	req->affinity = 0xFF;
	/* Normalize <0-255> to <0-7> */
	req->priority = queue_conf->priority / 32;

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to set priority queue=%d", queue_id);
		return rc;
	}

	return 0;
}

static void
otx2_sso_port_def_conf(struct rte_eventdev *event_dev, uint8_t port_id,
		       struct rte_event_port_conf *port_conf)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);

	RTE_SET_USED(port_id);
	port_conf->new_event_threshold = dev->max_num_events;
	port_conf->dequeue_depth = 1;
	port_conf->enqueue_depth = 1;
}

static int
otx2_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
		    const struct rte_event_port_conf *port_conf)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uintptr_t grps_base[OTX2_SSO_MAX_VHGRP] = {0};
	uint64_t val;
	uint16_t q;

	sso_func_trace("Port=%d", port_id);
	RTE_SET_USED(port_conf);

	if (event_dev->data->ports[port_id] == NULL) {
		otx2_err("Invalid port Id %d", port_id);
		return -EINVAL;
	}

	for (q = 0; q < dev->nb_event_queues; q++) {
		grps_base[q] = dev->bar2 + (RVU_BLOCK_ADDR_SSO << 20 | q << 12);
		if (grps_base[q] == 0) {
			otx2_err("Failed to get grp[%d] base addr", q);
			return -EINVAL;
		}
	}

	/* Set get_work timeout for HWS */
	val = NSEC2USEC(dev->deq_tmo_ns) - 1;

	if (dev->dual_ws) {
		struct otx2_ssogws_dual *ws = event_dev->data->ports[port_id];

		rte_memcpy(ws->grps_base, grps_base,
			   sizeof(uintptr_t) * OTX2_SSO_MAX_VHGRP);
		ws->fc_mem = dev->fc_mem;
		ws->xaq_lmt = dev->xaq_lmt;
		ws->tstamp = dev->tstamp;
		otx2_write64(val, OTX2_SSOW_GET_BASE_ADDR(
			     ws->ws_state[0].getwrk_op) + SSOW_LF_GWS_NW_TIM);
		otx2_write64(val, OTX2_SSOW_GET_BASE_ADDR(
			     ws->ws_state[1].getwrk_op) + SSOW_LF_GWS_NW_TIM);
	} else {
		struct otx2_ssogws *ws = event_dev->data->ports[port_id];
		uintptr_t base = OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op);

		rte_memcpy(ws->grps_base, grps_base,
			   sizeof(uintptr_t) * OTX2_SSO_MAX_VHGRP);
		ws->fc_mem = dev->fc_mem;
		ws->xaq_lmt = dev->xaq_lmt;
		ws->tstamp = dev->tstamp;
		otx2_write64(val, base + SSOW_LF_GWS_NW_TIM);
	}

	otx2_sso_dbg("Port=%d ws=%p", port_id, event_dev->data->ports[port_id]);

	return 0;
}

static int
otx2_sso_timeout_ticks(struct rte_eventdev *event_dev, uint64_t ns,
		       uint64_t *tmo_ticks)
{
	RTE_SET_USED(event_dev);
	*tmo_ticks = NSEC2TICK(ns, rte_get_timer_hz());

	return 0;
}

static void
ssogws_dump(struct otx2_ssogws *ws, FILE *f)
{
	uintptr_t base = OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op);

	fprintf(f, "SSOW_LF_GWS Base addr   0x%" PRIx64 "\n", (uint64_t)base);
	fprintf(f, "SSOW_LF_GWS_LINKS       0x%" PRIx64 "\n",
		otx2_read64(base + SSOW_LF_GWS_LINKS));
	fprintf(f, "SSOW_LF_GWS_PENDWQP     0x%" PRIx64 "\n",
		otx2_read64(base + SSOW_LF_GWS_PENDWQP));
	fprintf(f, "SSOW_LF_GWS_PENDSTATE   0x%" PRIx64 "\n",
		otx2_read64(base + SSOW_LF_GWS_PENDSTATE));
	fprintf(f, "SSOW_LF_GWS_NW_TIM      0x%" PRIx64 "\n",
		otx2_read64(base + SSOW_LF_GWS_NW_TIM));
	fprintf(f, "SSOW_LF_GWS_TAG         0x%" PRIx64 "\n",
		otx2_read64(base + SSOW_LF_GWS_TAG));
	fprintf(f, "SSOW_LF_GWS_WQP         0x%" PRIx64 "\n",
		otx2_read64(base + SSOW_LF_GWS_TAG));
	fprintf(f, "SSOW_LF_GWS_SWTP        0x%" PRIx64 "\n",
		otx2_read64(base + SSOW_LF_GWS_SWTP));
	fprintf(f, "SSOW_LF_GWS_PENDTAG     0x%" PRIx64 "\n",
		otx2_read64(base + SSOW_LF_GWS_PENDTAG));
}

static void
ssoggrp_dump(uintptr_t base, FILE *f)
{
	fprintf(f, "SSO_LF_GGRP Base addr   0x%" PRIx64 "\n", (uint64_t)base);
	fprintf(f, "SSO_LF_GGRP_QCTL        0x%" PRIx64 "\n",
		otx2_read64(base + SSO_LF_GGRP_QCTL));
	fprintf(f, "SSO_LF_GGRP_XAQ_CNT     0x%" PRIx64 "\n",
		otx2_read64(base + SSO_LF_GGRP_XAQ_CNT));
	fprintf(f, "SSO_LF_GGRP_INT_THR     0x%" PRIx64 "\n",
		otx2_read64(base + SSO_LF_GGRP_INT_THR));
	fprintf(f, "SSO_LF_GGRP_INT_CNT     0x%" PRIX64 "\n",
		otx2_read64(base + SSO_LF_GGRP_INT_CNT));
	fprintf(f, "SSO_LF_GGRP_AQ_CNT      0x%" PRIX64 "\n",
		otx2_read64(base + SSO_LF_GGRP_AQ_CNT));
	fprintf(f, "SSO_LF_GGRP_AQ_THR      0x%" PRIX64 "\n",
		otx2_read64(base + SSO_LF_GGRP_AQ_THR));
	fprintf(f, "SSO_LF_GGRP_MISC_CNT    0x%" PRIx64 "\n",
		otx2_read64(base + SSO_LF_GGRP_MISC_CNT));
}

static void
otx2_sso_dump(struct rte_eventdev *event_dev, FILE *f)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint8_t queue;
	uint8_t port;

	fprintf(f, "[%s] SSO running in [%s] mode\n", __func__, dev->dual_ws ?
		"dual_ws" : "single_ws");
	/* Dump SSOW registers */
	for (port = 0; port < dev->nb_event_ports; port++) {
		if (dev->dual_ws) {
			struct otx2_ssogws_dual *ws =
				event_dev->data->ports[port];

			fprintf(f, "[%s] SSO dual workslot[%d] vws[%d] dump\n",
				__func__, port, 0);
			ssogws_dump((struct otx2_ssogws *)&ws->ws_state[0], f);
			fprintf(f, "[%s]SSO dual workslot[%d] vws[%d] dump\n",
				__func__, port, 1);
			ssogws_dump((struct otx2_ssogws *)&ws->ws_state[1], f);
		} else {
			fprintf(f, "[%s]SSO single workslot[%d] dump\n",
				__func__, port);
			ssogws_dump(event_dev->data->ports[port], f);
		}
	}

	/* Dump SSO registers */
	for (queue = 0; queue < dev->nb_event_queues; queue++) {
		fprintf(f, "[%s]SSO group[%d] dump\n", __func__, queue);
		if (dev->dual_ws) {
			struct otx2_ssogws_dual *ws = event_dev->data->ports[0];
			ssoggrp_dump(ws->grps_base[queue], f);
		} else {
			struct otx2_ssogws *ws = event_dev->data->ports[0];
			ssoggrp_dump(ws->grps_base[queue], f);
		}
	}
}

static void
otx2_handle_event(void *arg, struct rte_event event)
{
	struct rte_eventdev *event_dev = arg;

	if (event_dev->dev_ops->dev_stop_flush != NULL)
		event_dev->dev_ops->dev_stop_flush(event_dev->data->dev_id,
				event, event_dev->data->dev_stop_flush_arg);
}

static void
sso_qos_cfg(struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct sso_grp_qos_cfg *req;
	uint16_t i;

	for (i = 0; i < dev->qos_queue_cnt; i++) {
		uint8_t xaq_prcnt = dev->qos_parse_data[i].xaq_prcnt;
		uint8_t iaq_prcnt = dev->qos_parse_data[i].iaq_prcnt;
		uint8_t taq_prcnt = dev->qos_parse_data[i].taq_prcnt;

		if (dev->qos_parse_data[i].queue >= dev->nb_event_queues)
			continue;

		req = otx2_mbox_alloc_msg_sso_grp_qos_config(dev->mbox);
		req->xaq_limit = (dev->nb_xaq_cfg *
				  (xaq_prcnt ? xaq_prcnt : 100)) / 100;
		req->taq_thr = (SSO_HWGRP_IAQ_MAX_THR_MASK *
				(iaq_prcnt ? iaq_prcnt : 100)) / 100;
		req->iaq_thr = (SSO_HWGRP_TAQ_MAX_THR_MASK *
				(taq_prcnt ? taq_prcnt : 100)) / 100;
	}

	if (dev->qos_queue_cnt)
		otx2_mbox_process(dev->mbox);
}

static void
sso_cleanup(struct rte_eventdev *event_dev, uint8_t enable)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint16_t i;

	for (i = 0; i < dev->nb_event_ports; i++) {
		if (dev->dual_ws) {
			struct otx2_ssogws_dual *ws;

			ws = event_dev->data->ports[i];
			ssogws_reset((struct otx2_ssogws *)&ws->ws_state[0]);
			ssogws_reset((struct otx2_ssogws *)&ws->ws_state[1]);
			ws->swtag_req = 0;
			ws->vws = 0;
			ws->ws_state[0].cur_grp = 0;
			ws->ws_state[0].cur_tt = SSO_SYNC_EMPTY;
			ws->ws_state[1].cur_grp = 0;
			ws->ws_state[1].cur_tt = SSO_SYNC_EMPTY;
		} else {
			struct otx2_ssogws *ws;

			ws = event_dev->data->ports[i];
			ssogws_reset(ws);
			ws->swtag_req = 0;
			ws->cur_grp = 0;
			ws->cur_tt = SSO_SYNC_EMPTY;
		}
	}

	rte_mb();
	if (dev->dual_ws) {
		struct otx2_ssogws_dual *ws = event_dev->data->ports[0];
		struct otx2_ssogws temp_ws;

		memcpy(&temp_ws, &ws->ws_state[0],
		       sizeof(struct otx2_ssogws_state));
		for (i = 0; i < dev->nb_event_queues; i++) {
			/* Consume all the events through HWS0 */
			ssogws_flush_events(&temp_ws, i, ws->grps_base[i],
					    otx2_handle_event, event_dev);
			/* Enable/Disable SSO GGRP */
			otx2_write64(enable, ws->grps_base[i] +
				     SSO_LF_GGRP_QCTL);
		}
		ws->ws_state[0].cur_grp = 0;
		ws->ws_state[0].cur_tt = SSO_SYNC_EMPTY;
	} else {
		struct otx2_ssogws *ws = event_dev->data->ports[0];

		for (i = 0; i < dev->nb_event_queues; i++) {
			/* Consume all the events through HWS0 */
			ssogws_flush_events(ws, i, ws->grps_base[i],
					    otx2_handle_event, event_dev);
			/* Enable/Disable SSO GGRP */
			otx2_write64(enable, ws->grps_base[i] +
				     SSO_LF_GGRP_QCTL);
		}
		ws->cur_grp = 0;
		ws->cur_tt = SSO_SYNC_EMPTY;
	}

	/* reset SSO GWS cache */
	otx2_mbox_alloc_msg_sso_ws_cache_inv(dev->mbox);
	otx2_mbox_process(dev->mbox);
}

int
sso_xae_reconfigure(struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct rte_mempool *prev_xaq_pool;
	int rc = 0;

	if (event_dev->data->dev_started)
		sso_cleanup(event_dev, 0);

	prev_xaq_pool = dev->xaq_pool;
	dev->xaq_pool = NULL;
	rc = sso_xaq_allocate(dev);
	if (rc < 0) {
		otx2_err("Failed to alloc xaq pool %d", rc);
		rte_mempool_free(prev_xaq_pool);
		return rc;
	}
	rc = sso_ggrp_alloc_xaq(dev);
	if (rc < 0) {
		otx2_err("Failed to alloc xaq to ggrp %d", rc);
		rte_mempool_free(prev_xaq_pool);
		return rc;
	}

	rte_mempool_free(prev_xaq_pool);
	rte_mb();
	if (event_dev->data->dev_started)
		sso_cleanup(event_dev, 1);

	return 0;
}

static int
otx2_sso_start(struct rte_eventdev *event_dev)
{
	sso_func_trace();
	sso_qos_cfg(event_dev);
	sso_cleanup(event_dev, 1);
	sso_fastpath_fns_set(event_dev);

	return 0;
}

static void
otx2_sso_stop(struct rte_eventdev *event_dev)
{
	sso_func_trace();
	sso_cleanup(event_dev, 0);
	rte_mb();
}

static int
otx2_sso_close(struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint8_t all_queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint16_t i;

	if (!dev->configured)
		return 0;

	sso_unregister_irqs(event_dev);

	for (i = 0; i < dev->nb_event_queues; i++)
		all_queues[i] = i;

	for (i = 0; i < dev->nb_event_ports; i++)
		otx2_sso_port_unlink(event_dev, event_dev->data->ports[i],
				     all_queues, dev->nb_event_queues);

	sso_lf_teardown(dev, SSO_LF_GGRP);
	sso_lf_teardown(dev, SSO_LF_GWS);
	dev->nb_event_ports = 0;
	dev->nb_event_queues = 0;
	rte_mempool_free(dev->xaq_pool);
	rte_memzone_free(rte_memzone_lookup(OTX2_SSO_FC_NAME));

	return 0;
}

/* Initialize and register event driver with DPDK Application */
static struct rte_eventdev_ops otx2_sso_ops = {
	.dev_infos_get    = otx2_sso_info_get,
	.dev_configure    = otx2_sso_configure,
	.queue_def_conf   = otx2_sso_queue_def_conf,
	.queue_setup      = otx2_sso_queue_setup,
	.queue_release    = otx2_sso_queue_release,
	.port_def_conf    = otx2_sso_port_def_conf,
	.port_setup       = otx2_sso_port_setup,
	.port_release     = otx2_sso_port_release,
	.port_link        = otx2_sso_port_link,
	.port_unlink      = otx2_sso_port_unlink,
	.timeout_ticks    = otx2_sso_timeout_ticks,

	.eth_rx_adapter_caps_get  = otx2_sso_rx_adapter_caps_get,
	.eth_rx_adapter_queue_add = otx2_sso_rx_adapter_queue_add,
	.eth_rx_adapter_queue_del = otx2_sso_rx_adapter_queue_del,
	.eth_rx_adapter_start = otx2_sso_rx_adapter_start,
	.eth_rx_adapter_stop = otx2_sso_rx_adapter_stop,

	.eth_tx_adapter_caps_get = otx2_sso_tx_adapter_caps_get,
	.eth_tx_adapter_queue_add = otx2_sso_tx_adapter_queue_add,
	.eth_tx_adapter_queue_del = otx2_sso_tx_adapter_queue_del,

	.timer_adapter_caps_get = otx2_tim_caps_get,

	.xstats_get       = otx2_sso_xstats_get,
	.xstats_reset     = otx2_sso_xstats_reset,
	.xstats_get_names = otx2_sso_xstats_get_names,

	.dump             = otx2_sso_dump,
	.dev_start        = otx2_sso_start,
	.dev_stop         = otx2_sso_stop,
	.dev_close        = otx2_sso_close,
	.dev_selftest     = otx2_sso_selftest,
};

#define OTX2_SSO_XAE_CNT	"xae_cnt"
#define OTX2_SSO_SINGLE_WS	"single_ws"
#define OTX2_SSO_GGRP_QOS	"qos"
#define OTX2_SSO_SELFTEST	"selftest"

static void
parse_queue_param(char *value, void *opaque)
{
	struct otx2_sso_qos queue_qos = {0};
	uint8_t *val = (uint8_t *)&queue_qos;
	struct otx2_sso_evdev *dev = opaque;
	char *tok = strtok(value, "-");
	struct otx2_sso_qos *old_ptr;

	if (!strlen(value))
		return;

	while (tok != NULL) {
		*val = atoi(tok);
		tok = strtok(NULL, "-");
		val++;
	}

	if (val != (&queue_qos.iaq_prcnt + 1)) {
		otx2_err("Invalid QoS parameter expected [Qx-XAQ-TAQ-IAQ]");
		return;
	}

	dev->qos_queue_cnt++;
	old_ptr = dev->qos_parse_data;
	dev->qos_parse_data = rte_realloc(dev->qos_parse_data,
					  sizeof(struct otx2_sso_qos) *
					  dev->qos_queue_cnt, 0);
	if (dev->qos_parse_data == NULL) {
		dev->qos_parse_data = old_ptr;
		dev->qos_queue_cnt--;
		return;
	}
	dev->qos_parse_data[dev->qos_queue_cnt - 1] = queue_qos;
}

static void
parse_qos_list(const char *value, void *opaque)
{
	char *s = strdup(value);
	char *start = NULL;
	char *end = NULL;
	char *f = s;

	while (*s) {
		if (*s == '[')
			start = s;
		else if (*s == ']')
			end = s;

		if (start && start < end) {
			*end = 0;
			parse_queue_param(start + 1, opaque);
			s = end;
			start = end;
		}
		s++;
	}

	free(f);
}

static int
parse_sso_kvargs_dict(const char *key, const char *value, void *opaque)
{
	RTE_SET_USED(key);

	/* Dict format [Qx-XAQ-TAQ-IAQ][Qz-XAQ-TAQ-IAQ] use '-' cause ','
	 * isn't allowed. Everything is expressed in percentages, 0 represents
	 * default.
	 */
	parse_qos_list(value, opaque);

	return 0;
}

static void
sso_parse_devargs(struct otx2_sso_evdev *dev, struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	uint8_t single_ws = 0;

	if (devargs == NULL)
		return;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return;

	rte_kvargs_process(kvlist, OTX2_SSO_SELFTEST, &parse_kvargs_flag,
			   &dev->selftest);
	rte_kvargs_process(kvlist, OTX2_SSO_XAE_CNT, &parse_kvargs_value,
			   &dev->xae_cnt);
	rte_kvargs_process(kvlist, OTX2_SSO_SINGLE_WS, &parse_kvargs_flag,
			   &single_ws);
	rte_kvargs_process(kvlist, OTX2_SSO_GGRP_QOS, &parse_sso_kvargs_dict,
			   dev);
	otx2_parse_common_devargs(kvlist);
	dev->dual_ws = !single_ws;
	rte_kvargs_free(kvlist);
}

static int
otx2_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev,
				       sizeof(struct otx2_sso_evdev),
				       otx2_sso_init);
}

static int
otx2_sso_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, otx2_sso_fini);
}

static const struct rte_pci_id pci_sso_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_SSO_TIM_PF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_sso = {
	.id_table = pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = otx2_sso_probe,
	.remove = otx2_sso_remove,
};

int
otx2_sso_init(struct rte_eventdev *event_dev)
{
	struct free_rsrcs_rsp *rsrc_cnt;
	struct rte_pci_device *pci_dev;
	struct otx2_sso_evdev *dev;
	int rc;

	event_dev->dev_ops = &otx2_sso_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		sso_fastpath_fns_set(event_dev);
		return 0;
	}

	dev = sso_pmd_priv(event_dev);

	pci_dev = container_of(event_dev->dev, struct rte_pci_device, device);

	/* Initialize the base otx2_dev object */
	rc = otx2_dev_init(pci_dev, dev);
	if (rc < 0) {
		otx2_err("Failed to initialize otx2_dev rc=%d", rc);
		goto error;
	}

	/* Get SSO and SSOW MSIX rsrc cnt */
	otx2_mbox_alloc_msg_free_rsrc_cnt(dev->mbox);
	rc = otx2_mbox_process_msg(dev->mbox, (void *)&rsrc_cnt);
	if (rc < 0) {
		otx2_err("Unable to get free rsrc count");
		goto otx2_dev_uninit;
	}
	otx2_sso_dbg("SSO %d SSOW %d NPA %d provisioned", rsrc_cnt->sso,
		     rsrc_cnt->ssow, rsrc_cnt->npa);

	dev->max_event_ports = RTE_MIN(rsrc_cnt->ssow, OTX2_SSO_MAX_VHWS);
	dev->max_event_queues = RTE_MIN(rsrc_cnt->sso, OTX2_SSO_MAX_VHGRP);
	/* Grab the NPA LF if required */
	rc = otx2_npa_lf_init(pci_dev, dev);
	if (rc < 0) {
		otx2_err("Unable to init NPA lf. It might not be provisioned");
		goto otx2_dev_uninit;
	}

	dev->drv_inited = true;
	dev->is_timeout_deq = 0;
	dev->min_dequeue_timeout_ns = USEC2NSEC(1);
	dev->max_dequeue_timeout_ns = USEC2NSEC(0x3FF);
	dev->max_num_events = -1;
	dev->nb_event_queues = 0;
	dev->nb_event_ports = 0;

	if (!dev->max_event_ports || !dev->max_event_queues) {
		otx2_err("Not enough eventdev resource queues=%d ports=%d",
			 dev->max_event_queues, dev->max_event_ports);
		rc = -ENODEV;
		goto otx2_npa_lf_uninit;
	}

	dev->dual_ws = 1;
	sso_parse_devargs(dev, pci_dev->device.devargs);
	if (dev->dual_ws) {
		otx2_sso_dbg("Using dual workslot mode");
		dev->max_event_ports = dev->max_event_ports / 2;
	} else {
		otx2_sso_dbg("Using single workslot mode");
	}

	otx2_sso_pf_func_set(dev->pf_func);
	otx2_sso_dbg("Initializing %s max_queues=%d max_ports=%d",
		     event_dev->data->name, dev->max_event_queues,
		     dev->max_event_ports);
	if (dev->selftest) {
		event_dev->dev->driver = &pci_sso.driver;
		event_dev->dev_ops->dev_selftest();
	}

	otx2_tim_init(pci_dev, (struct otx2_dev *)dev);

	return 0;

otx2_npa_lf_uninit:
	otx2_npa_lf_fini();
otx2_dev_uninit:
	otx2_dev_fini(pci_dev, dev);
error:
	return rc;
}

int
otx2_sso_fini(struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct rte_pci_device *pci_dev;

	/* For secondary processes, nothing to be done */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = container_of(event_dev->dev, struct rte_pci_device, device);

	if (!dev->drv_inited)
		goto dev_fini;

	dev->drv_inited = false;
	otx2_npa_lf_fini();

dev_fini:
	if (otx2_npa_lf_active(dev)) {
		otx2_info("Common resource in use by other devices");
		return -EAGAIN;
	}

	otx2_tim_fini();
	otx2_dev_fini(pci_dev, dev);

	return 0;
}

RTE_PMD_REGISTER_PCI(event_octeontx2, pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_octeontx2, pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_octeontx2, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(event_octeontx2, OTX2_SSO_XAE_CNT "=<int>"
			      OTX2_SSO_SINGLE_WS "=1"
			      OTX2_SSO_GGRP_QOS "=<string>"
			      OTX2_SSO_SELFTEST "=1"
			      OTX2_NPA_LOCK_MASK "=<1-65535>");
