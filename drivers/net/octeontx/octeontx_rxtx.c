/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_ethdev_driver.h>
#include <rte_ether.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>

#include "octeontx_ethdev.h"
#include "octeontx_rxtx.h"
#include "octeontx_logs.h"

uint16_t __rte_hot
octeontx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct octeontx_rxq *rxq;
	struct rte_event ev;
	size_t count;
	uint16_t valid_event;

	rxq = rx_queue;
	count = 0;
	while (count < nb_pkts) {
		valid_event = rte_event_dequeue_burst(rxq->evdev,
							rxq->ev_ports, &ev,
							1, 0);
		if (!valid_event)
			break;
		rx_pkts[count++] = ev.mbuf;
	}

	return count; /* return number of pkts received */
}

#define T(name, f3, f2, f1, f0, sz, flags)				\
static uint16_t __rte_noinline	__rte_hot				\
octeontx_xmit_pkts_ ##name(void *tx_queue,				\
			struct rte_mbuf **tx_pkts, uint16_t pkts)	\
{									\
	uint64_t cmd[(sz)];						\
									\
	return __octeontx_xmit_pkts(tx_queue, tx_pkts, pkts, cmd,	\
				    flags);				\
}

OCCTX_TX_FASTPATH_MODES
#undef T

void __rte_hot
octeontx_set_tx_function(struct rte_eth_dev *dev)
{
	struct octeontx_nic *nic = octeontx_pmd_priv(dev);

	const eth_tx_burst_t tx_burst_func[2][2][2][2] = {
#define T(name, f3, f2, f1, f0, sz, flags)			\
	[f3][f2][f1][f0] =  octeontx_xmit_pkts_ ##name,

OCCTX_TX_FASTPATH_MODES
#undef T
	};

	dev->tx_pkt_burst = tx_burst_func
		[!!(nic->tx_offload_flags & OCCTX_TX_OFFLOAD_MBUF_NOFF_F)]
		[!!(nic->tx_offload_flags & OCCTX_TX_OFFLOAD_OL3_OL4_CSUM_F)]
		[!!(nic->tx_offload_flags & OCCTX_TX_OFFLOAD_L3_L4_CSUM_F)]
		[!!(nic->tx_offload_flags & OCCTX_TX_MULTI_SEG_F)];
}
