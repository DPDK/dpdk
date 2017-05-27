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

#ifndef _SFC_TX_H
#define _SFC_TX_H

#include <rte_mbuf.h>
#include <rte_ethdev.h>

#include "efx.h"

#include "sfc_dp_tx.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sfc_adapter;
struct sfc_evq;

/**
 * Software Tx descriptor information associated with hardware Tx
 * descriptor.
 */
struct sfc_efx_tx_sw_desc {
	struct rte_mbuf		*mbuf;
	uint8_t			*tsoh;	/* Buffer to store TSO header */
};

enum sfc_txq_state_bit {
	SFC_TXQ_INITIALIZED_BIT = 0,
#define SFC_TXQ_INITIALIZED	(1 << SFC_TXQ_INITIALIZED_BIT)
	SFC_TXQ_STARTED_BIT,
#define SFC_TXQ_STARTED		(1 << SFC_TXQ_STARTED_BIT)
	SFC_TXQ_FLUSHING_BIT,
#define SFC_TXQ_FLUSHING	(1 << SFC_TXQ_FLUSHING_BIT)
	SFC_TXQ_FLUSHED_BIT,
#define SFC_TXQ_FLUSHED		(1 << SFC_TXQ_FLUSHED_BIT)
	SFC_TXQ_FLUSH_FAILED_BIT,
#define SFC_TXQ_FLUSH_FAILED	(1 << SFC_TXQ_FLUSH_FAILED_BIT)
};

/**
 * Transmit queue control information. Not used on datapath.
 * Allocated on the socket specified on the queue setup.
 */
struct sfc_txq {
	unsigned int			state;
	unsigned int			hw_index;
	struct sfc_evq			*evq;
	efsys_mem_t			mem;
	struct sfc_dp_txq		*dp;
	efx_txq_t			*common;
	unsigned int			free_thresh;
	unsigned int			flags;
};

static inline unsigned int
sfc_txq_sw_index_by_hw_index(unsigned int hw_index)
{
	return hw_index;
}

static inline unsigned int
sfc_txq_sw_index(const struct sfc_txq *txq)
{
	return sfc_txq_sw_index_by_hw_index(txq->hw_index);
}

struct sfc_txq *sfc_txq_by_dp_txq(const struct sfc_dp_txq *dp_txq);

/**
 * Transmit queue information used on libefx-based data path.
 * Allocated on the socket specified on the queue setup.
 */
struct sfc_efx_txq {
	struct sfc_evq			*evq;
	struct sfc_efx_tx_sw_desc	*sw_ring;
	unsigned int			ptr_mask;
	efx_desc_t			*pend_desc;
	efx_txq_t			*common;
	unsigned int			added;
	unsigned int			pending;
	unsigned int			completed;
	unsigned int			free_thresh;
	uint16_t			hw_vlan_tci;
	uint16_t			dma_desc_size_max;

	unsigned int			hw_index;
	unsigned int			flags;
#define SFC_EFX_TXQ_FLAG_STARTED	0x1
#define SFC_EFX_TXQ_FLAG_RUNNING	0x2

	/* Datapath transmit queue anchor */
	struct sfc_dp_txq		dp;
};

static inline struct sfc_efx_txq *
sfc_efx_txq_by_dp_txq(struct sfc_dp_txq *dp_txq)
{
	return container_of(dp_txq, struct sfc_efx_txq, dp);
}

struct sfc_txq_info {
	unsigned int		entries;
	struct sfc_txq		*txq;
	boolean_t		deferred_start;
	boolean_t		deferred_started;
};

int sfc_tx_configure(struct sfc_adapter *sa);
void sfc_tx_close(struct sfc_adapter *sa);

int sfc_tx_qinit(struct sfc_adapter *sa, unsigned int sw_index,
		 uint16_t nb_tx_desc, unsigned int socket_id,
		 const struct rte_eth_txconf *tx_conf);
void sfc_tx_qfini(struct sfc_adapter *sa, unsigned int sw_index);

void sfc_tx_qflush_done(struct sfc_txq *txq);
int sfc_tx_qstart(struct sfc_adapter *sa, unsigned int sw_index);
void sfc_tx_qstop(struct sfc_adapter *sa, unsigned int sw_index);
int sfc_tx_start(struct sfc_adapter *sa);
void sfc_tx_stop(struct sfc_adapter *sa);

/* From 'sfc_tso.c' */
int sfc_efx_tso_alloc_tsoh_objs(struct sfc_efx_tx_sw_desc *sw_ring,
				unsigned int txq_entries,
				unsigned int socket_id);
void sfc_efx_tso_free_tsoh_objs(struct sfc_efx_tx_sw_desc *sw_ring,
				unsigned int txq_entries);
int sfc_efx_tso_do(struct sfc_efx_txq *txq, unsigned int idx,
		   struct rte_mbuf **in_seg, size_t *in_off, efx_desc_t **pend,
		   unsigned int *pkt_descs, size_t *pkt_len);

#ifdef __cplusplus
}
#endif
#endif	/* _SFC_TX_H */
