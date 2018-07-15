/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2017-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_DP_RX_H
#define _SFC_DP_RX_H

#include <rte_mempool.h>
#include <rte_ethdev_driver.h>

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
	/** Maximum number of pushed Rx descriptors in the queue */
	unsigned int		max_fill_level;
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
	/** VI window size shift */
	unsigned int		vi_window_shift;
};

/**
 * Get Rx datapath specific device info.
 *
 * @param dev_info		Device info to be adjusted
 */
typedef void (sfc_dp_rx_get_dev_info_t)(struct rte_eth_dev_info *dev_info);

/**
 * Test if an Rx datapath supports specific mempool ops.
 *
 * @param pool			The name of the pool operations to test.
 *
 * @return Check status.
 * @retval	0		Best mempool ops choice.
 * @retval	1		Mempool ops are supported.
 * @retval	-ENOTSUP	Mempool ops not supported.
 */
typedef int (sfc_dp_rx_pool_ops_supported_t)(const char *pool);

/**
 * Get size of receive and event queue rings by the number of Rx
 * descriptors and mempool configuration.
 *
 * @param nb_rx_desc		Number of Rx descriptors
 * @param mb_pool		mbuf pool with Rx buffers
 * @param rxq_entries		Location for number of Rx ring entries
 * @param evq_entries		Location for number of event ring entries
 * @param rxq_max_fill_level	Location for maximum Rx ring fill level
 *
 * @return 0 or positive errno.
 */
typedef int (sfc_dp_rx_qsize_up_rings_t)(uint16_t nb_rx_desc,
					 struct rte_mempool *mb_pool,
					 unsigned int *rxq_entries,
					 unsigned int *evq_entries,
					 unsigned int *rxq_max_fill_level);

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
 * Packed stream receive event handler used during queue flush only.
 */
typedef bool (sfc_dp_rx_qrx_ps_ev_t)(struct sfc_dp_rxq *dp_rxq,
				     unsigned int id);

/**
 * Receive queue purge function called after queue flush.
 *
 * Should be used to free unused recevie buffers.
 */
typedef void (sfc_dp_rx_qpurge_t)(struct sfc_dp_rxq *dp_rxq);

/** Get packet types recognized/classified */
typedef const uint32_t * (sfc_dp_rx_supported_ptypes_get_t)(
				uint32_t tunnel_encaps);

/** Get number of pending Rx descriptors */
typedef unsigned int (sfc_dp_rx_qdesc_npending_t)(struct sfc_dp_rxq *dp_rxq);

/** Check Rx descriptor status */
typedef int (sfc_dp_rx_qdesc_status_t)(struct sfc_dp_rxq *dp_rxq,
				       uint16_t offset);

/** Receive datapath definition */
struct sfc_dp_rx {
	struct sfc_dp				dp;

	unsigned int				features;
#define SFC_DP_RX_FEAT_SCATTER			0x1
#define SFC_DP_RX_FEAT_MULTI_PROCESS		0x2
#define SFC_DP_RX_FEAT_TUNNELS			0x4
#define SFC_DP_RX_FEAT_FLOW_FLAG		0x8
#define SFC_DP_RX_FEAT_FLOW_MARK		0x10
#define SFC_DP_RX_FEAT_CHECKSUM			0x20
	sfc_dp_rx_get_dev_info_t		*get_dev_info;
	sfc_dp_rx_pool_ops_supported_t		*pool_ops_supported;
	sfc_dp_rx_qsize_up_rings_t		*qsize_up_rings;
	sfc_dp_rx_qcreate_t			*qcreate;
	sfc_dp_rx_qdestroy_t			*qdestroy;
	sfc_dp_rx_qstart_t			*qstart;
	sfc_dp_rx_qstop_t			*qstop;
	sfc_dp_rx_qrx_ev_t			*qrx_ev;
	sfc_dp_rx_qrx_ps_ev_t			*qrx_ps_ev;
	sfc_dp_rx_qpurge_t			*qpurge;
	sfc_dp_rx_supported_ptypes_get_t	*supported_ptypes_get;
	sfc_dp_rx_qdesc_npending_t		*qdesc_npending;
	sfc_dp_rx_qdesc_status_t		*qdesc_status;
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
extern struct sfc_dp_rx sfc_ef10_essb_rx;

#ifdef __cplusplus
}
#endif
#endif /* _SFC_DP_RX_H */
