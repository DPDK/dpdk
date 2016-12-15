/*-
 * Copyright (c) 2016 Solarflare Communications Inc.
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

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of entries in the management event queue */
#define SFC_MGMT_EVQ_ENTRIES	(EFX_EVQ_MINNEVS)

struct sfc_adapter;
struct sfc_rxq;
struct sfc_txq;

enum sfc_evq_state {
	SFC_EVQ_UNINITIALIZED = 0,
	SFC_EVQ_INITIALIZED,
	SFC_EVQ_STARTING,
	SFC_EVQ_STARTED,

	SFC_EVQ_NSTATES
};

struct sfc_evq {
	/* Used on datapath */
	efx_evq_t		*common;
	unsigned int		read_ptr;
	boolean_t		exception;
	efsys_mem_t		mem;
	struct sfc_rxq		*rxq;
	struct sfc_txq		*txq;

	/* Not used on datapath */
	struct sfc_adapter	*sa;
	unsigned int		evq_index;
	enum sfc_evq_state	init_state;
};

struct sfc_evq_info {
	/* Maximum number of EVQ entries taken into account when buffer
	 * table space is allocated.
	 */
	unsigned int		max_entries;
	/* Real number of EVQ entries, less or equal to max_entries */
	unsigned int		entries;
	/* Event queue creation flags */
	uint32_t		flags;
	/* NUMA-aware EVQ data structure used on datapath */
	struct sfc_evq		*evq;
};

/*
 * Functions below define event queue to transmit/receive queue and vice
 * versa mapping.
 */

static inline unsigned int
sfc_ev_qcount(struct sfc_adapter *sa)
{
	const struct rte_eth_dev_data *dev_data = sa->eth_dev->data;

	/*
	 * One management EVQ for global events.
	 * Own EVQ for each Tx and Rx queue.
	 */
	return 1 + dev_data->nb_rx_queues + dev_data->nb_tx_queues;
}

static inline unsigned int
sfc_evq_max_entries(struct sfc_adapter *sa, unsigned int sw_index)
{
	unsigned int max_entries;

	if (sw_index == sa->mgmt_evq_index)
		max_entries = SFC_MGMT_EVQ_ENTRIES;
	else if (sw_index <= sa->eth_dev->data->nb_rx_queues)
		max_entries = EFX_RXQ_MAXNDESCS;
	else
		max_entries = efx_nic_cfg_get(sa->nic)->enc_txq_max_ndescs;

	return max_entries;
}

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

int sfc_ev_init(struct sfc_adapter *sa);
void sfc_ev_fini(struct sfc_adapter *sa);
int sfc_ev_start(struct sfc_adapter *sa);
void sfc_ev_stop(struct sfc_adapter *sa);

int sfc_ev_qinit(struct sfc_adapter *sa, unsigned int sw_index,
		 unsigned int entries, int socket_id);
void sfc_ev_qfini(struct sfc_adapter *sa, unsigned int sw_index);
int sfc_ev_qstart(struct sfc_adapter *sa, unsigned int sw_index);
void sfc_ev_qstop(struct sfc_adapter *sa, unsigned int sw_index);

int sfc_ev_qprime(struct sfc_evq *evq);
void sfc_ev_qpoll(struct sfc_evq *evq);

void sfc_ev_mgmt_qpoll(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_EV_H_ */
