/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#ifndef _COMMON_INTEL_TX_H_
#define _COMMON_INTEL_TX_H_

#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_vect.h>

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
		volatile struct idpf_base_tx_desc *idpf_tx_ring;
		volatile union ixgbe_adv_tx_desc *ixgbe_tx_ring;
	};
	volatile uint8_t *qtx_tail;               /* register address of tail */
	union {
		struct ci_tx_entry *sw_ring; /* virtual address of SW ring */
		struct ci_tx_entry_vec *sw_ring_vec;
	};
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
	uint16_t port_id;  /* Device port identifier. */
	uint16_t queue_id; /* TX queue index. */
	uint16_t reg_idx;
	uint16_t tx_next_dd;
	uint16_t tx_next_rs;
	/* Mempool pointer for fast release of mbufs.
	 * NULL if disabled, UINTPTR_MAX if enabled and not yet known.
	 * Set at first use (if enabled and not yet known).
	 */
	struct rte_mempool *fast_free_mp;
	uint64_t offloads;
	uint64_t mbuf_errors;
	rte_iova_t tx_ring_dma;        /* TX ring DMA address */
	bool tx_deferred_start; /* don't start this queue in dev start */
	bool q_set;             /* indicate if tx queue has been configured */
	bool vector_tx;         /* port is using vector TX */
	union {                  /* the VSI this queue belongs to */
		struct i40e_vsi *i40e_vsi;
		struct iavf_vsi *iavf_vsi;
		struct ice_vsi *ice_vsi;
	};
	const struct rte_memzone *mz;

	union {
		struct { /* ICE driver specific values */
			struct ice_txtime *tsq; /* Tx Time based queue */
			uint32_t q_teid; /* TX schedule node id. */
		};
		struct { /* I40E driver specific values */
			uint8_t dcb_tc;
		};
		struct { /* iavf driver specific values */
			uint16_t ipsec_crypto_pkt_md_offset;
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
			uint8_t pthresh;   /**< Prefetch threshold register. */
			uint8_t hthresh;   /**< Host threshold register. */
			uint8_t wthresh;   /**< Write-back threshold reg. */
			uint8_t using_ipsec;  /**< indicates that IPsec TX feature is in use */
			uint8_t is_vf;   /**< indicates that this is a VF queue */
			uint8_t vf_ctx_initialized; /**< VF context descriptors initialized */
		};
		struct { /* idpf specific values */
				volatile union {
						struct idpf_flex_tx_sched_desc *desc_ring;
						struct idpf_splitq_tx_compl_desc *compl_ring;
				};
				struct ci_tx_queue *complq;
				void **txqs;   /*only valid for split queue mode*/
				uint32_t tx_start_qid;
				uint16_t sw_nb_desc;
				uint16_t sw_tail;
				uint16_t rs_compl_count;
				uint8_t expected_gen_id;
		};
	};
};

struct ci_tx_path_features {
	uint32_t tx_offloads;
	enum rte_vect_max_simd simd_width;
	bool simple_tx;
	bool ctx_desc;
	bool disabled;
	bool single_queue;
};

struct ci_tx_path_info {
	eth_tx_burst_t pkt_burst;
	const char *info;
	struct ci_tx_path_features features;
	eth_tx_prep_t pkt_prep;
};

static __rte_always_inline void
ci_tx_backlog_entry(struct ci_tx_entry *txep, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	for (uint16_t i = 0; i < (int)nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

static __rte_always_inline void
ci_tx_backlog_entry_vec(struct ci_tx_entry_vec *txep, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	for (uint16_t i = 0; i < nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

#define IETH_VPMD_TX_MAX_FREE_BUF 64

typedef int (*ci_desc_done_fn)(struct ci_tx_queue *txq, uint16_t idx);

static __rte_always_inline int
ci_tx_free_bufs_vec(struct ci_tx_queue *txq, ci_desc_done_fn desc_done, bool ctx_descs)
{
	int nb_free = 0;
	struct rte_mbuf *free[IETH_VPMD_TX_MAX_FREE_BUF];
	struct rte_mbuf *m;

	/* check DD bits on threshold descriptor */
	if (!desc_done(txq, txq->tx_next_dd))
		return 0;

	const uint32_t n = txq->tx_rs_thresh >> ctx_descs;

	/* first buffer to free from S/W ring is at index
	 * tx_next_dd - (tx_rs_thresh - 1)
	 */
	struct ci_tx_entry_vec *txep = txq->sw_ring_vec;
	txep += (txq->tx_next_dd >> ctx_descs) - (n - 1);

	/* is fast-free enabled? */
	struct rte_mempool *mp =
			likely(txq->fast_free_mp != (void *)UINTPTR_MAX) ?
			txq->fast_free_mp :
			(txq->fast_free_mp = txep[0].mbuf->pool);

	if (mp != NULL && (n & 31) == 0) {
		void **cache_objs;
		struct rte_mempool_cache *cache = rte_mempool_default_cache(mp, rte_lcore_id());

		if (cache == NULL)
			goto normal;

		cache_objs = &cache->objs[cache->len];

		if (n > RTE_MEMPOOL_CACHE_MAX_SIZE) {
			rte_mempool_ops_enqueue_bulk(mp, (void *)txep, n);
			goto done;
		}

		/* The cache follows the following algorithm
		 *   1. Add the objects to the cache
		 *   2. Anything greater than the cache min value (if it
		 *   crosses the cache flush threshold) is flushed to the ring.
		 */
		/* Add elements back into the cache */
		uint32_t copied = 0;
		/* n is multiple of 32 */
		while (copied < n) {
			memcpy(&cache_objs[copied], &txep[copied], 32 * sizeof(void *));
			copied += 32;
		}
		cache->len += n;

		if (cache->len >= cache->flushthresh) {
			rte_mempool_ops_enqueue_bulk(mp, &cache->objs[cache->size],
					cache->len - cache->size);
			cache->len = cache->size;
		}
		goto done;
	}

normal:
	m = rte_pktmbuf_prefree_seg(txep[0].mbuf);
	if (likely(m)) {
		free[0] = m;
		nb_free = 1;
		for (uint32_t i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (likely(m)) {
				if (likely(m->pool == free[0]->pool)) {
					free[nb_free++] = m;
				} else {
					rte_mbuf_raw_free_bulk(free[0]->pool, free, nb_free);
					free[0] = m;
					nb_free = 1;
				}
			}
		}
		rte_mbuf_raw_free_bulk(free[0]->pool, free, nb_free);
	} else {
		for (uint32_t i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (m)
				rte_mempool_put(m->pool, m);
		}
	}

done:
	/* buffers were freed, update counters */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return txq->tx_rs_thresh;
}

static inline void
ci_txq_release_all_mbufs(struct ci_tx_queue *txq, bool use_ctx)
{
	if (unlikely(!txq || !txq->sw_ring))
		return;

	if (!txq->vector_tx) {
		for (uint16_t i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
		return;
	}

	/**
	 *  vPMD tx will not set sw_ring's mbuf to NULL after free,
	 *  so determining buffers to free is a little more complex.
	 */
	const uint16_t start = (txq->tx_next_dd - txq->tx_rs_thresh + 1) >> use_ctx;
	const uint16_t nb_desc = txq->nb_tx_desc >> use_ctx;
	const uint16_t end = txq->tx_tail >> use_ctx;

	uint16_t i = start;
	if (end < i) {
		for (; i < nb_desc; i++)
			rte_pktmbuf_free_seg(txq->sw_ring_vec[i].mbuf);
		i = 0;
	}
	for (; i < end; i++)
		rte_pktmbuf_free_seg(txq->sw_ring_vec[i].mbuf);
	memset(txq->sw_ring_vec, 0, sizeof(txq->sw_ring_vec[0]) * nb_desc);
}

/**
 * Select the best matching Tx path based on features
 *
 * @param req_features
 *   The requested features for the Tx path
 * @param infos
 *   Array of information about the available Tx paths
 * @param num_paths
 *   Number of available paths in the infos array
 * @param default_path
 *   Index of the default path to use if no suitable path is found
 *
 * @return
 *   The packet burst function index that best matches the requested features,
 *   or default_path if no suitable path is found
 */
static inline int
ci_tx_path_select(const struct ci_tx_path_features *req_features,
			const struct ci_tx_path_info *infos,
			size_t num_paths,
			int default_path)
{
	int idx = default_path;
	const struct ci_tx_path_features *chosen_path_features = NULL;

	for (unsigned int i = 0; i < num_paths; i++) {
		const struct ci_tx_path_features *path_features = &infos[i].features;

		/* Do not select a path with a NULL pkt_burst function. */
		if (infos[i].pkt_burst == NULL)
			continue;

		/* Do not select a disabled tx path. */
		if (path_features->disabled)
			continue;

		/* Do not use a simple tx path if not requested. */
		if (path_features->simple_tx && !req_features->simple_tx)
			continue;

		/* If a context descriptor is requested, ensure the path supports it. */
		if (!path_features->ctx_desc && req_features->ctx_desc)
			continue;

		/* If requested, ensure the path supports single queue TX. */
		if (path_features->single_queue != req_features->single_queue)
			continue;

		/* Ensure the path supports the requested TX offloads. */
		if ((path_features->tx_offloads & req_features->tx_offloads) !=
				req_features->tx_offloads)
			continue;

		/* Ensure the path's SIMD width is compatible with the requested width. */
		if (path_features->simd_width > req_features->simd_width)
			continue;

		/* Do not select the path if it is less suitable than the chosen path. */
		if (chosen_path_features != NULL) {
			/* Do not select paths with lower SIMD width than the chosen path. */
			if (path_features->simd_width < chosen_path_features->simd_width)
				continue;
			/* Do not select paths with more offloads enabled than the chosen path if
			 * the SIMD widths are the same.
			 */
			if (path_features->simd_width == chosen_path_features->simd_width &&
					rte_popcount32(path_features->tx_offloads) >
					rte_popcount32(chosen_path_features->tx_offloads))
				continue;

			/* Don't use a context descriptor unless necessary */
			if (path_features->ctx_desc && !chosen_path_features->ctx_desc)
				continue;
		}

		/* Finally, select the path since it has met all the requirements. */
		idx = i;
		chosen_path_features = &infos[idx].features;
	}

	return idx;
}

#endif /* _COMMON_INTEL_TX_H_ */
