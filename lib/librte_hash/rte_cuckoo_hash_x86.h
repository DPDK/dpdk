/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

/* rte_cuckoo_hash_x86.h
 * This file holds all x86 specific Cuckoo Hash functions
 */

/* Only tries to insert at one bucket (@prim_bkt) without trying to push
 * buckets around
 */
static inline unsigned
rte_hash_cuckoo_insert_mw_tm(struct rte_hash_bucket *prim_bkt,
		hash_sig_t sig, hash_sig_t alt_hash, uint32_t new_idx)
{
	unsigned i, status;
	unsigned try = 0;

	while (try < RTE_HASH_TSX_MAX_RETRY) {
		status = rte_xbegin();
		if (likely(status == RTE_XBEGIN_STARTED)) {
			/* Insert new entry if there is room in the primary
			* bucket.
			*/
			for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
				/* Check if slot is available */
				if (likely(prim_bkt->key_idx[i] == EMPTY_SLOT)) {
					prim_bkt->sig_current[i] = sig;
					prim_bkt->sig_alt[i] = alt_hash;
					prim_bkt->key_idx[i] = new_idx;
					break;
				}
			}
			rte_xend();

			if (i != RTE_HASH_BUCKET_ENTRIES)
				return 0;

			break; /* break off try loop if transaction commits */
		} else {
			/* If we abort we give up this cuckoo path. */
			try++;
			rte_pause();
		}
	}

	return -1;
}

/* Shift buckets along provided cuckoo_path (@leaf and @leaf_slot) and fill
 * the path head with new entry (sig, alt_hash, new_idx)
 */
static inline int
rte_hash_cuckoo_move_insert_mw_tm(const struct rte_hash *h,
			struct queue_node *leaf, uint32_t leaf_slot,
			hash_sig_t sig, hash_sig_t alt_hash, uint32_t new_idx)
{
	unsigned try = 0;
	unsigned status;
	uint32_t prev_alt_bkt_idx;

	struct queue_node *prev_node, *curr_node = leaf;
	struct rte_hash_bucket *prev_bkt, *curr_bkt = leaf->bkt;
	uint32_t prev_slot, curr_slot = leaf_slot;

	while (try < RTE_HASH_TSX_MAX_RETRY) {
		status = rte_xbegin();
		if (likely(status == RTE_XBEGIN_STARTED)) {
			while (likely(curr_node->prev != NULL)) {
				prev_node = curr_node->prev;
				prev_bkt = prev_node->bkt;
				prev_slot = curr_node->prev_slot;

				prev_alt_bkt_idx
					= prev_bkt->sig_alt[prev_slot]
					    & h->bucket_bitmask;

				if (unlikely(&h->buckets[prev_alt_bkt_idx]
					     != curr_bkt)) {
					rte_xabort(RTE_XABORT_CUCKOO_PATH_INVALIDED);
				}

				/* Need to swap current/alt sig to allow later
				 * Cuckoo insert to move elements back to its
				 * primary bucket if available
				 */
				curr_bkt->sig_alt[curr_slot] =
				    prev_bkt->sig_current[prev_slot];
				curr_bkt->sig_current[curr_slot] =
				    prev_bkt->sig_alt[prev_slot];
				curr_bkt->key_idx[curr_slot]
				    = prev_bkt->key_idx[prev_slot];

				curr_slot = prev_slot;
				curr_node = prev_node;
				curr_bkt = curr_node->bkt;
			}

			curr_bkt->sig_current[curr_slot] = sig;
			curr_bkt->sig_alt[curr_slot] = alt_hash;
			curr_bkt->key_idx[curr_slot] = new_idx;

			rte_xend();

			return 0;
		}

		/* If we abort we give up this cuckoo path, since most likely it's
		 * no longer valid as TSX detected data conflict
		 */
		try++;
		rte_pause();
	}

	return -1;
}

/*
 * Make space for new key, using bfs Cuckoo Search and Multi-Writer safe
 * Cuckoo
 */
static inline int
rte_hash_cuckoo_make_space_mw_tm(const struct rte_hash *h,
			struct rte_hash_bucket *bkt,
			hash_sig_t sig, hash_sig_t alt_hash,
			uint32_t new_idx)
{
	unsigned i;
	struct queue_node queue[RTE_HASH_BFS_QUEUE_MAX_LEN];
	struct queue_node *tail, *head;
	struct rte_hash_bucket *curr_bkt, *alt_bkt;

	tail = queue;
	head = queue + 1;
	tail->bkt = bkt;
	tail->prev = NULL;
	tail->prev_slot = -1;

	/* Cuckoo bfs Search */
	while (likely(tail != head && head <
					queue + RTE_HASH_BFS_QUEUE_MAX_LEN -
					RTE_HASH_BUCKET_ENTRIES)) {
		curr_bkt = tail->bkt;
		for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			if (curr_bkt->key_idx[i] == EMPTY_SLOT) {
				if (likely(rte_hash_cuckoo_move_insert_mw_tm(h,
						tail, i, sig,
						alt_hash, new_idx) == 0))
					return 0;
			}

			/* Enqueue new node and keep prev node info */
			alt_bkt = &(h->buckets[curr_bkt->sig_alt[i]
						    & h->bucket_bitmask]);
			head->bkt = alt_bkt;
			head->prev = tail;
			head->prev_slot = i;
			head++;
		}
		tail++;
	}

	return -ENOSPC;
}
