/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2017 Solarflare Communications Inc.
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

#ifndef _SFC_DP_RX_H
#define _SFC_DP_RX_H

#include <rte_mempool.h>
#include <rte_ethdev.h>

#include "sfc_dp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generic receive queue information used on data path.
 * It must be kept as small as it is possible since it is built into
 * the structure used on datapath.
 */
struct sfc_dp_rxq {
	struct sfc_dp_queue	dpq;
};

/**
 * Datapath receive queue creation information.
 *
 * The structure is used just to pass information from control path to
 * datapath. It could be just function arguments, but it would be hardly
 * readable.
 */
struct sfc_dp_rx_qcreate_info {
	/** Memory pool to allocate Rx buffer from */
	struct rte_mempool	*refill_mb_pool;
	/** Minimum number of unused Rx descriptors to do refill */
	unsigned int		refill_threshold;
	/**
	 * Usable mbuf data space in accordance with alignment and
	 * padding requirements imposed by HW.
	 */
	unsigned int		buf_size;

	/**
	 * Maximum number of Rx descriptors completed in one Rx event.
	 * Just for sanity checks if datapath would like to do.
	 */
	unsigned int		batch_max;

	/** Pseudo-header size */
	unsigned int		prefix_size;

	/** Receive queue flags initializer */
	unsigned int		flags;
#define SFC_RXQ_FLAG_RSS_HASH	0x1

	/** Rx queue size */
	unsigned int		rxq_entries;
	/** DMA-mapped Rx descriptors ring */
	void			*rxq_hw_ring;

	/** Associated event queue size */
	unsigned int		evq_entries;
	/** Hardware event ring */
	void			*evq_hw_ring;

	/** The queue index in hardware (required to push right doorbell) */
	unsigned int		hw_index;
	/**
	 * Virtual address of the memory-mapped BAR to push Rx refill
	 * doorbell
	 */
	volatile void		*mem_bar;
};

/**
 * Allocate and initialize datapath receive queue.
 *
 * @param port_id	The port identifier
 * @param queue_id	The queue identifier
 * @param pci_addr	PCI function address
 * @param socket_id	Socket identifier to allocate memory
 * @param info		Receive queue information
 * @param dp_rxqp	Location for generic datapath receive queue pointer
 *
 * @return 0 or positive errno.
 */
typedef int (sfc_dp_rx_qcreate_t)(uint16_t port_id, uint16_t queue_id,
				  const struct rte_pci_addr *pci_addr,
				  int socket_id,
				  const struct sfc_dp_rx_qcreate_info *info,
				  struct sfc_dp_rxq **dp_rxqp);

/**
 * Free resources allocated for datapath recevie queue.
 */
typedef void (sfc_dp_rx_qdestroy_t)(struct sfc_dp_rxq *dp_rxq);

/**
 * Receive queue start callback.
 *
 * It handovers EvQ to the datapath.
 */
typedef int (sfc_dp_rx_qstart_t)(struct sfc_dp_rxq *dp_rxq,
				 unsigned int evq_read_ptr);

/**
 * Receive queue stop function called before flush.
 */
typedef void (sfc_dp_rx_qstop_t)(struct sfc_dp_rxq *dp_rxq,
				 unsigned int *evq_read_ptr);

/**
 * Receive event handler used during queue flush only.
 */
typedef bool (sfc_dp_rx_qrx_ev_t)(struct sfc_dp_rxq *dp_rxq, unsigned int id);

/**
 * Receive queue purge function called after queue flush.
 *
 * Should be used to free unused recevie buffers.
 */
typedef void (sfc_dp_rx_qpurge_t)(struct sfc_dp_rxq *dp_rxq);

/** Get packet types recognized/classified */
typedef const uint32_t * (sfc_dp_rx_supported_ptypes_get_t)(void);

/** Get number of pending Rx descriptors */
typedef unsigned int (sfc_dp_rx_qdesc_npending_t)(struct sfc_dp_rxq *dp_rxq);

/** Receive datapath definition */
struct sfc_dp_rx {
	struct sfc_dp				dp;

	unsigned int				features;
#define SFC_DP_RX_FEAT_SCATTER			0x1
#define SFC_DP_RX_FEAT_MULTI_PROCESS		0x2
	sfc_dp_rx_qcreate_t			*qcreate;
	sfc_dp_rx_qdestroy_t			*qdestroy;
	sfc_dp_rx_qstart_t			*qstart;
	sfc_dp_rx_qstop_t			*qstop;
	sfc_dp_rx_qrx_ev_t			*qrx_ev;
	sfc_dp_rx_qpurge_t			*qpurge;
	sfc_dp_rx_supported_ptypes_get_t	*supported_ptypes_get;
	sfc_dp_rx_qdesc_npending_t		*qdesc_npending;
	eth_rx_burst_t				pkt_burst;
};

static inline struct sfc_dp_rx *
sfc_dp_find_rx_by_name(struct sfc_dp_list *head, const char *name)
{
	struct sfc_dp *p = sfc_dp_find_by_name(head, SFC_DP_RX, name);

	return (p == NULL) ? NULL : container_of(p, struct sfc_dp_rx, dp);
}

static inline struct sfc_dp_rx *
sfc_dp_find_rx_by_caps(struct sfc_dp_list *head, unsigned int avail_caps)
{
	struct sfc_dp *p = sfc_dp_find_by_caps(head, SFC_DP_RX, avail_caps);

	return (p == NULL) ? NULL : container_of(p, struct sfc_dp_rx, dp);
}

extern struct sfc_dp_rx sfc_efx_rx;
extern struct sfc_dp_rx sfc_ef10_rx;

#ifdef __cplusplus
}
#endif
#endif /* _SFC_DP_RX_H */
