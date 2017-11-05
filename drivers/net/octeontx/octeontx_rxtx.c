/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium, Inc. 2017. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>

#include "octeontx_ethdev.h"
#include "octeontx_rxtx.h"
#include "octeontx_logs.h"


static __rte_always_inline uint16_t __hot
__octeontx_xmit_pkts(void *lmtline_va, void *ioreg_va, int64_t *fc_status_va,
			struct rte_mbuf *tx_pkt)
{
	uint64_t cmd_buf[4];
	uint16_t gaura_id;

	if (unlikely(*((volatile int64_t *)fc_status_va) < 0))
		return -ENOSPC;

	/* Get the gaura Id */
	gaura_id = octeontx_fpa_bufpool_gpool((uintptr_t)tx_pkt->pool->pool_id);

	/* Setup PKO_SEND_HDR_S */
	cmd_buf[0] = tx_pkt->data_len & 0xffff;
	cmd_buf[1] = 0x0;

	/* Set don't free bit if reference count > 1 */
	if (rte_mbuf_refcnt_read(tx_pkt) > 1)
		cmd_buf[0] |= (1ULL << 58); /* SET DF */

	/* Setup PKO_SEND_GATHER_S */
	cmd_buf[(1 << 1) | 1] = rte_mbuf_data_iova(tx_pkt);
	cmd_buf[(1 << 1) | 0] = PKO_SEND_GATHER_SUBDC |
				PKO_SEND_GATHER_LDTYPE(0x1ull) |
				PKO_SEND_GATHER_GAUAR((long)gaura_id) |
				tx_pkt->data_len;

	octeontx_reg_lmtst(lmtline_va, ioreg_va, cmd_buf, PKO_CMD_SZ);

	return 0;
}

uint16_t __hot
octeontx_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int count;
	struct octeontx_txq *txq = tx_queue;
	octeontx_dq_t *dq = &txq->dq;
	int res;

	count = 0;

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
