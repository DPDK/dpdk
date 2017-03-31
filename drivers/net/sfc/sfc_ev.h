/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016-2017 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SFC_EV_H_
#define _SFC_EV_H_

#include <rte_ethdev.h>

#include "efx.h"

#include "sfc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of entries in the management event queue */
#define SFC_MGMT_EVQ_ENTRIES	(EFX_EVQ_MINNEVS)

struct sfc_adapter;
struct sfc_dp_rxq;
struct sfc_dp_txq;

enum sfc_evq_state {
	SFC_EVQ_UNINITIALIZED = 0,
	SFC_EVQ_INITIALIZED,
	SFC_EVQ_STARTING,
	SFC_EVQ_STARTED,

	SFC_EVQ_NSTATES
};

enum sfc_evq_type {
	SFC_EVQ_TYPE_MGMT = 0,
	SFC_EVQ_TYPE_RX,
	SFC_EVQ_TYPE_TX,

	SFC_EVQ_NTYPES
};

struct sfc_evq {
	/* Used on datapath */
	efx_evq_t			*common;
	const efx_ev_callbacks_t	*callbacks;
	unsigned int			read_ptr;
	boolean_t			exception;
	efsys_mem_t			mem;
	struct sfc_dp_rxq		*dp_rxq;
	struct sfc_dp_txq		*dp_txq;

	/* Not used on datapath */
	struct sfc_adapter		*sa;
	unsigned int			evq_index;
	enum sfc_evq_state		init_state;
	enum sfc_evq_type		type;
	unsigned int			entries;
};

/*
 * Functions below define event queue to transmit/receive queue and vice
 * versa mapping.
 * Own event queue is allocated for management, each Rx and each Tx queue.
 * Zero event queue is used for management events.
 * Rx event queues from 1 to RxQ number follow management event queue.
 * Tx event queues follow Rx event queues.
 */

static inline unsigned int
sfc_evq_index_by_rxq_sw_index(__rte_unused struct sfc_adapter *sa,
			      unsigned int rxq_sw_index)
{
	return 1 + rxq_sw_index;
}

static inline unsigned int
sfc_evq_index_by_txq_sw_index(struct sfc_adapter *sa, unsigned int txq_sw_index)
{
	return 1 + sa->eth_dev->data->nb_rx_queues + txq_sw_index;
}

int sfc_ev_attach(struct sfc_adapter *sa);
void sfc_ev_detach(struct sfc_adapter *sa);
int sfc_ev_start(struct sfc_adapter *sa);
void sfc_ev_stop(struct sfc_adapter *sa);

int sfc_ev_qinit(struct sfc_adapter *sa,
		 enum sfc_evq_type type, unsigned int type_index,
		 unsigned int entries, int socket_id, struct sfc_evq **evqp);
void sfc_ev_qfini(struct sfc_evq *evq);
int sfc_ev_qstart(struct sfc_evq *evq, unsigned int hw_index);
void sfc_ev_qstop(struct sfc_evq *evq);

int sfc_ev_qprime(struct sfc_evq *evq);
void sfc_ev_qpoll(struct sfc_evq *evq);

void sfc_ev_mgmt_qpoll(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_EV_H_ */
