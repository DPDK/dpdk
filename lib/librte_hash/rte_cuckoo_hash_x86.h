/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
				if (likely(prim_bkt->signatures[i].sig ==
						NULL_SIGNATURE)) {
					prim_bkt->signatures[i].current = sig;
					prim_bkt->signatures[i].alt = alt_hash;
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
					= prev_bkt->signatures[prev_slot].alt
					    & h->bucket_bitmask;

				if (unlikely(&h->buckets[prev_alt_bkt_idx]
					     != curr_bkt)) {
					rte_xabort(RTE_XABORT_CUCKOO_PATH_INVALIDED);
				}

				/* Need to swap current/alt sig to allow later
				 * Cuckoo insert to move elements back to its
				 * primary bucket if available
				 */
				curr_bkt->signatures[curr_slot].alt =
				    prev_bkt->signatures[prev_slot].current;
				curr_bkt->signatures[curr_slot].current =
				    prev_bkt->signatures[prev_slot].alt;
				curr_bkt->key_idx[curr_slot]
				    = prev_bkt->key_idx[prev_slot];

				curr_slot = prev_slot;
				curr_node = prev_node;
				curr_bkt = curr_node->bkt;
			}

			curr_bkt->signatures[curr_slot].current = sig;
			curr_bkt->signatures[curr_slot].alt = alt_hash;
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
					queue + RTE_HASH_BFS_QUEUE_MAX_LEN - 4)) {
		curr_bkt = tail->bkt;
		for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			if (curr_bkt->signatures[i].sig == NULL_SIGNATURE) {
				if (likely(rte_hash_cuckoo_move_insert_mw_tm(h,
						tail, i, sig,
						alt_hash, new_idx) == 0))
					return 0;
			}

			/* Enqueue new node and keep prev node info */
			alt_bkt = &(h->buckets[curr_bkt->signatures[i].alt
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
