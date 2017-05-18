/*-
 *   BSD LICENSE
 *
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

#ifndef _SFC_DP_TX_H
#define _SFC_DP_TX_H

#include <rte_ethdev.h>

#include "sfc_dp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generic transmit queue information used on data path.
 * It must be kept as small as it is possible since it is built into
 * the structure used on datapath.
 */
struct sfc_dp_txq {
	struct sfc_dp_queue	dpq;
};

/**
 * Datapath transmit queue creation information.
 *
 * The structure is used just to pass information from control path to
 * datapath. It could be just function arguments, but it would be hardly
 * readable.
 */
struct sfc_dp_tx_qcreate_info {
	/** Minimum number of unused Tx descriptors to do reap */
	unsigned int		free_thresh;
	/** Transmit queue configuration flags */
	unsigned int		flags;
	/** Tx queue size */
	unsigned int		txq_entries;
	/** Maximum size of data in the DMA descriptor */
	uint16_t		dma_desc_size_max;
	/** DMA-mapped Tx descriptors ring */
	void			*txq_hw_ring;
	/** Associated event queue size */
	unsigned int		evq_entries;
	/** Hardware event ring */
	void			*evq_hw_ring;
	/** The queue index in hardware (required to push right doorbell) */
	unsigned int		hw_index;
	/** Virtual address of the memory-mapped BAR to push Tx doorbell */
	volatile void		*mem_bar;
};

/**
 * Allocate and initialize datapath transmit queue.
 *
 * @param port_id	The port identifier
 * @param queue_id	The queue identifier
 * @param pci_addr	PCI function address
 * @param socket_id	Socket identifier to allocate memory
 * @param info		Tx queue details wrapped in structure
 * @param dp_txqp	Location for generic datapath transmit queue pointer
 *
 * @return 0 or positive errno.
 */
typedef int (sfc_dp_tx_qcreate_t)(uint16_t port_id, uint16_t queue_id,
				  const struct rte_pci_addr *pci_addr,
				  int socket_id,
				  const struct sfc_dp_tx_qcreate_info *info,
				  struct sfc_dp_txq **dp_txqp);

/**
 * Free resources allocated for datapath transmit queue.
 */
typedef void (sfc_dp_tx_qdestroy_t)(struct sfc_dp_txq *dp_txq);

/**
 * Transmit queue start callback.
 *
 * It handovers EvQ to the datapath.
 */
typedef int (sfc_dp_tx_qstart_t)(struct sfc_dp_txq *dp_txq,
				 unsigned int evq_read_ptr,
				 unsigned int txq_desc_index);

/**
 * Transmit queue stop function called before the queue flush.
 *
 * It returns EvQ to the control path.
 */
typedef void (sfc_dp_tx_qstop_t)(struct sfc_dp_txq *dp_txq,
				 unsigned int *evq_read_ptr);

/**
 * Transmit event handler used during queue flush only.
 */
typedef bool (sfc_dp_tx_qtx_ev_t)(struct sfc_dp_txq *dp_txq, unsigned int id);

/**
 * Transmit queue function called after the queue flush.
 */
typedef void (sfc_dp_tx_qreap_t)(struct sfc_dp_txq *dp_txq);

/** Transmit datapath definition */
struct sfc_dp_tx {
	struct sfc_dp			dp;

	unsigned int			features;
#define SFC_DP_TX_FEAT_VLAN_INSERT	0x1
#define SFC_DP_TX_FEAT_TSO		0x2
#define SFC_DP_TX_FEAT_MULTI_SEG	0x4
#define SFC_DP_TX_FEAT_MULTI_PROCESS	0x8
	sfc_dp_tx_qcreate_t		*qcreate;
	sfc_dp_tx_qdestroy_t		*qdestroy;
	sfc_dp_tx_qstart_t		*qstart;
	sfc_dp_tx_qstop_t		*qstop;
	sfc_dp_tx_qtx_ev_t		*qtx_ev;
	sfc_dp_tx_qreap_t		*qreap;
	eth_tx_burst_t			pkt_burst;
};

static inline struct sfc_dp_tx *
sfc_dp_find_tx_by_name(struct sfc_dp_list *head, const char *name)
{
	struct sfc_dp *p = sfc_dp_find_by_name(head, SFC_DP_TX, name);

	return (p == NULL) ? NULL : container_of(p, struct sfc_dp_tx, dp);
}

static inline struct sfc_dp_tx *
sfc_dp_find_tx_by_caps(struct sfc_dp_list *head, unsigned int avail_caps)
{
	struct sfc_dp *p = sfc_dp_find_by_caps(head, SFC_DP_TX, avail_caps);

	return (p == NULL) ? NULL : container_of(p, struct sfc_dp_tx, dp);
}

extern struct sfc_dp_tx sfc_efx_tx;
extern struct sfc_dp_tx sfc_ef10_tx;
extern struct sfc_dp_tx sfc_ef10_simple_tx;

#ifdef __cplusplus
}
#endif
#endif /* _SFC_DP_TX_H */
