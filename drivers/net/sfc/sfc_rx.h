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

#ifndef _SFC_RX_H
#define _SFC_RX_H

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ethdev.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sfc_adapter;
struct sfc_evq;

/**
 * Software Rx descriptor information associated with hardware Rx
 * descriptor.
 */
struct sfc_rx_sw_desc {
	struct rte_mbuf		*mbuf;
	unsigned int		flags;
	unsigned int		size;
};

/** Receive queue state bits */
enum sfc_rxq_state_bit {
	SFC_RXQ_INITIALIZED_BIT = 0,
#define SFC_RXQ_INITIALIZED	(1 << SFC_RXQ_INITIALIZED_BIT)
};

/**
 * Receive queue information used on data path.
 * Allocated on the socket specified on the queue setup.
 */
struct sfc_rxq {
	/* Used on data path */
	struct sfc_evq		*evq;
	struct sfc_rx_sw_desc	*sw_desc;
	unsigned int		state;
	unsigned int		ptr_mask;

	/* Used on refill */
	uint16_t		buf_size;
	struct rte_mempool	*refill_mb_pool;
	efx_rxq_t		*common;
	efsys_mem_t		mem;

	/* Not used on data path */
	unsigned int		hw_index;
};

static inline unsigned int
sfc_rxq_sw_index_by_hw_index(unsigned int hw_index)
{
	return hw_index;
}

static inline unsigned int
sfc_rxq_sw_index(const struct sfc_rxq *rxq)
{
	return sfc_rxq_sw_index_by_hw_index(rxq->hw_index);
}

/**
 * Receive queue information used during setup/release only.
 * Allocated on the same socket as adapter data.
 */
struct sfc_rxq_info {
	unsigned int		max_entries;
	unsigned int		entries;
	efx_rxq_type_t		type;
	struct sfc_rxq		*rxq;
};

int sfc_rx_init(struct sfc_adapter *sa);
void sfc_rx_fini(struct sfc_adapter *sa);

int sfc_rx_qinit(struct sfc_adapter *sa, unsigned int rx_queue_id,
		 uint16_t nb_rx_desc, unsigned int socket_id,
		 const struct rte_eth_rxconf *rx_conf,
		 struct rte_mempool *mb_pool);
void sfc_rx_qfini(struct sfc_adapter *sa, unsigned int sw_index);

#ifdef __cplusplus
}
#endif
#endif  /* _SFC_RX_H */
