/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_DP_TX_H
#define _SFC_DP_TX_H

#include <rte_ethdev_driver.h>

#include "sfc_dp.h"
#include "sfc_debug.h"
#include "sfc_tso.h"

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

/** Datapath transmit queue descriptor number limitations */
struct sfc_dp_tx_hw_limits {
	unsigned int txq_max_entries;
	unsigned int txq_min_entries;
};

/**
 * Datapath transmit queue creation information.
 *
 * The structure is used just to pass information from control path to
 * datapath. It could be just function arguments, but it would be hardly
 * readable.
 */
struct sfc_dp_tx_qcreate_info {
	/** Maximum number of pushed Tx descriptors */
	unsigned int		max_fill_level;
	/** Minimum number of unused Tx descriptors to do reap */
	unsigned int		free_thresh;
	/** Offloads enabled on the transmit queue */
	uint64_t		offloads;
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
	/** VI window size shift */
	unsigned int		vi_window_shift;
	/**
	 * Maximum number of bytes into the packet the TCP header can start for
	 * the hardware to apply TSO packet edits.
	 */
	uint16_t		tso_tcp_header_offset_limit;
};

/**
 * Get Tx datapath specific device info.
 *
 * @param dev_info		Device info to be adjusted
 */
typedef void (sfc_dp_tx_get_dev_info_t)(struct rte_eth_dev_info *dev_info);

/**
 * Get size of transmit and event queue rings by the number of Tx
 * descriptors.
 *
 * @param nb_tx_desc		Number of Tx descriptors
 * @param txq_entries		Location for number of Tx ring entries
 * @param evq_entries		Location for number of event ring entries
 * @param txq_max_fill_level	Location for maximum Tx ring fill level
 *
 * @return 0 or positive errno.
 */
typedef int (sfc_dp_tx_qsize_up_rings_t)(uint16_t nb_tx_desc,
					 struct sfc_dp_tx_hw_limits *limits,
					 unsigned int *txq_entries,
					 unsigned int *evq_entries,
					 unsigned int *txq_max_fill_level);

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

/**
 * Check Tx descriptor status
 */
typedef int (sfc_dp_tx_qdesc_status_t)(struct sfc_dp_txq *dp_txq,
				       uint16_t offset);

/** Transmit datapath definition */
struct sfc_dp_tx {
	struct sfc_dp			dp;

	unsigned int			features;
#define SFC_DP_TX_FEAT_MULTI_PROCESS	0x1
	/**
	 * Tx offload capabilities supported by the datapath on device
	 * level only if HW/FW supports it.
	 */
	uint64_t			dev_offload_capa;
	/**
	 * Tx offload capabilities supported by the datapath per-queue
	 * if HW/FW supports it.
	 */
	uint64_t			queue_offload_capa;
	sfc_dp_tx_get_dev_info_t	*get_dev_info;
	sfc_dp_tx_qsize_up_rings_t	*qsize_up_rings;
	sfc_dp_tx_qcreate_t		*qcreate;
	sfc_dp_tx_qdestroy_t		*qdestroy;
	sfc_dp_tx_qstart_t		*qstart;
	sfc_dp_tx_qstop_t		*qstop;
	sfc_dp_tx_qtx_ev_t		*qtx_ev;
	sfc_dp_tx_qreap_t		*qreap;
	sfc_dp_tx_qdesc_status_t	*qdesc_status;
	eth_tx_prep_t			pkt_prepare;
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

/** Get Tx datapath ops by the datapath TxQ handle */
const struct sfc_dp_tx *sfc_dp_tx_by_dp_txq(const struct sfc_dp_txq *dp_txq);

static inline uint64_t
sfc_dp_tx_offload_capa(const struct sfc_dp_tx *dp_tx)
{
	return dp_tx->dev_offload_capa | dp_tx->queue_offload_capa;
}

static inline int
sfc_dp_tx_prepare_pkt(struct rte_mbuf *m,
			   uint32_t tso_tcp_header_offset_limit,
			   unsigned int max_fill_level,
			   unsigned int nb_tso_descs,
			   unsigned int nb_vlan_descs)
{
	unsigned int descs_required = m->nb_segs;

#ifdef RTE_LIBRTE_SFC_EFX_DEBUG
	int ret;

	ret = rte_validate_tx_offload(m);
	if (ret != 0) {
		/*
		 * Negative error code is returned by rte_validate_tx_offload(),
		 * but positive are used inside net/sfc PMD.
		 */
		SFC_ASSERT(ret < 0);
		return -ret;
	}
#endif

	if (m->ol_flags & PKT_TX_TCP_SEG) {
		unsigned int tcph_off = m->l2_len + m->l3_len;
		unsigned int header_len;

		switch (m->ol_flags & PKT_TX_TUNNEL_MASK) {
		case 0:
			break;
		case PKT_TX_TUNNEL_VXLAN:
			/* FALLTHROUGH */
		case PKT_TX_TUNNEL_GENEVE:
			if (!(m->ol_flags &
			      (PKT_TX_OUTER_IPV4 | PKT_TX_OUTER_IPV6)))
				return EINVAL;

			tcph_off += m->outer_l2_len + m->outer_l3_len;
		}

		header_len = tcph_off + m->l4_len;

		if (unlikely(tcph_off > tso_tcp_header_offset_limit))
			return EINVAL;

		descs_required += nb_tso_descs;

		/*
		 * Extra descriptor that is required when a packet header
		 * is separated from remaining content of the first segment.
		 */
		if (rte_pktmbuf_data_len(m) > header_len) {
			descs_required++;
		} else if (rte_pktmbuf_data_len(m) < header_len &&
			 unlikely(header_len > SFC_TSOH_STD_LEN)) {
			/*
			 * Header linearization is required and
			 * the header is too big to be linearized
			 */
			return EINVAL;
		}
	}

	/*
	 * The number of VLAN descriptors is added regardless of requested
	 * VLAN offload since VLAN is sticky and sending packet without VLAN
	 * insertion may require VLAN descriptor to reset the sticky to 0.
	 */
	descs_required += nb_vlan_descs;

	/*
	 * Max fill level must be sufficient to hold all required descriptors
	 * to send the packet entirely.
	 */
	if (descs_required > max_fill_level)
		return ENOBUFS;

	return 0;
}

extern struct sfc_dp_tx sfc_efx_tx;
extern struct sfc_dp_tx sfc_ef10_tx;
extern struct sfc_dp_tx sfc_ef10_simple_tx;

#ifdef __cplusplus
}
#endif
#endif /* _SFC_DP_TX_H */
