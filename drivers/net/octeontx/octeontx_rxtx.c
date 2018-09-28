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

uint16_t __hot
octeontx_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int count;
	struct octeontx_txq *txq = tx_queue;
	octeontx_dq_t *dq = &txq->dq;
	int res;

	count = 0;

	rte_cio_wmb();
	while (count < nb_pkts) {
		res = __octeontx_xmit_pkts(dq->lmtline_va, dq->ioreg_va,
					   dq->fc_status_va,
					   tx_pkts[count]);
		if (res < 0)
			break;

		count++;
	}

	return count; /* return number of pkts transmitted */
}

uint16_t __hot
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
