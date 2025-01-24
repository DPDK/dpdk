/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#ifndef _COMMON_INTEL_TX_H_
#define _COMMON_INTEL_TX_H_

#include <stdint.h>
#include <rte_mbuf.h>

/* forward declaration of the common intel (ci) queue structure */
struct ci_tx_queue;

/**
 * Structure associated with each descriptor of the TX ring of a TX queue.
 */
struct ci_tx_entry {
	struct rte_mbuf *mbuf; /* mbuf associated with TX desc, if any. */
	uint16_t next_id; /* Index of next descriptor in ring. */
	uint16_t last_id; /* Index of last scattered descriptor. */
};

/**
 * Structure associated with each descriptor of the TX ring of a TX queue in vector Tx.
 */
struct ci_tx_entry_vec {
	struct rte_mbuf *mbuf; /* mbuf associated with TX desc, if any. */
};

typedef void (*ice_tx_release_mbufs_t)(struct ci_tx_queue *txq);

struct ci_tx_queue {
	union { /* TX ring virtual address */
		volatile struct i40e_tx_desc *i40e_tx_ring;
		volatile struct iavf_tx_desc *iavf_tx_ring;
		volatile struct ice_tx_desc *ice_tx_ring;
		volatile union ixgbe_adv_tx_desc *ixgbe_tx_ring;
	};
	volatile uint8_t *qtx_tail;               /* register address of tail */
	union {
		struct ci_tx_entry *sw_ring; /* virtual address of SW ring */
		struct ci_tx_entry_vec *sw_ring_vec;
	};
	rte_iova_t tx_ring_dma;        /* TX ring DMA address */
	uint16_t nb_tx_desc;           /* number of TX descriptors */
	uint16_t tx_tail; /* current value of tail register */
	uint16_t nb_tx_used; /* number of TX desc used since RS bit set */
	/* index to last TX descriptor to have been cleaned */
	uint16_t last_desc_cleaned;
	/* Total number of TX descriptors ready to be allocated. */
	uint16_t nb_tx_free;
	/* Start freeing TX buffers if there are less free descriptors than
	 * this value.
	 */
	uint16_t tx_free_thresh;
	/* Number of TX descriptors to use before RS bit is set. */
	uint16_t tx_rs_thresh;
	uint8_t pthresh;   /**< Prefetch threshold register. */
	uint8_t hthresh;   /**< Host threshold register. */
	uint8_t wthresh;   /**< Write-back threshold reg. */
	uint16_t port_id;  /* Device port identifier. */
	uint16_t queue_id; /* TX queue index. */
	uint16_t reg_idx;
	uint64_t offloads;
	uint16_t tx_next_dd;
	uint16_t tx_next_rs;
	uint64_t mbuf_errors;
	bool tx_deferred_start; /* don't start this queue in dev start */
	bool q_set;             /* indicate if tx queue has been configured */
	union {                  /* the VSI this queue belongs to */
		struct i40e_vsi *i40e_vsi;
		struct iavf_vsi *iavf_vsi;
		struct ice_vsi *ice_vsi;
	};
	const struct rte_memzone *mz;

	union {
		struct { /* ICE driver specific values */
			ice_tx_release_mbufs_t tx_rel_mbufs;
			uint32_t q_teid; /* TX schedule node id. */
		};
		struct { /* I40E driver specific values */
			uint8_t dcb_tc;
		};
		struct { /* iavf driver specific values */
			uint16_t ipsec_crypto_pkt_md_offset;
			uint8_t rel_mbufs_type;
#define IAVF_TX_FLAGS_VLAN_TAG_LOC_L2TAG1 BIT(0)
#define IAVF_TX_FLAGS_VLAN_TAG_LOC_L2TAG2 BIT(1)
			uint8_t vlan_flag;
			uint8_t tc;
			bool use_ctx;  /* with ctx info, each pkt needs two descriptors */
		};
		struct { /* ixgbe specific values */
			const struct ixgbe_txq_ops *ops;
			struct ixgbe_advctx_info *ctx_cache;
			uint32_t ctx_curr;
#ifdef RTE_LIB_SECURITY
			uint8_t using_ipsec;  /**< indicates that IPsec TX feature is in use */
#endif
		};
	};
};

static __rte_always_inline void
ci_tx_backlog_entry(struct ci_tx_entry *txep, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	for (uint16_t i = 0; i < (int)nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

#endif /* _COMMON_INTEL_TX_H_ */
