/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX4_MR_H_
#define RTE_PMD_MLX4_MR_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_eal_memconfig.h>
#include <rte_ethdev.h>
#include <rte_rwlock.h>
#include <rte_bitmap.h>

/* Size of per-queue MR cache array for linear search. */
#define MLX4_MR_CACHE_N 8

/* Size of MR cache table for binary search. */
#define MLX4_MR_BTREE_CACHE_N 256

/* Memory Region object. */
struct mlx4_mr {
	LIST_ENTRY(mlx4_mr) mr; /**< Pointer to the prev/next entry. */
	struct ibv_mr *ibv_mr; /* Verbs Memory Region. */
	const struct rte_memseg_list *msl;
	int ms_base_idx; /* Start index of msl->memseg_arr[]. */
	int ms_n; /* Number of memsegs in use. */
	uint32_t ms_bmp_n; /* Number of bits in memsegs bit-mask. */
	struct rte_bitmap *ms_bmp; /* Bit-mask of memsegs belonged to MR. */
};

/* Cache entry for Memory Region. */
struct mlx4_mr_cache {
	uintptr_t start; /* Start address of MR. */
	uintptr_t end; /* End address of MR. */
	uint32_t lkey; /* rte_cpu_to_be_32(ibv_mr->lkey). */
} __rte_packed;

/* MR Cache table for Binary search. */
struct mlx4_mr_btree {
	uint16_t len; /* Number of entries. */
	uint16_t size; /* Total number of entries. */
	int overflow; /* Mark failure of table expansion. */
	struct mlx4_mr_cache (*table)[];
} __rte_packed;

/* Per-queue MR control descriptor. */
struct mlx4_mr_ctrl {
	uint32_t *dev_gen_ptr; /* Generation number of device to poll. */
	uint32_t cur_gen; /* Generation number saved to flush caches. */
	uint16_t mru; /* Index of last hit entry in top-half cache. */
	uint16_t head; /* Index of the oldest entry in top-half cache. */
	struct mlx4_mr_cache cache[MLX4_MR_CACHE_N]; /* Cache for top-half. */
	struct mlx4_mr_btree cache_bh; /* Cache for bottom-half. */
} __rte_packed;

extern struct mlx4_dev_list  mlx4_mem_event_cb_list;
extern rte_rwlock_t mlx4_mem_event_rwlock;

/* First entry must be NULL for comparison. */
#define mlx4_mr_btree_len(bt) ((bt)->len - 1)

int mlx4_mr_btree_init(struct mlx4_mr_btree *bt, int n, int socket);
void mlx4_mr_btree_free(struct mlx4_mr_btree *bt);
void mlx4_mr_btree_dump(struct mlx4_mr_btree *bt);
uint32_t mlx4_mr_create_primary(struct rte_eth_dev *dev,
				struct mlx4_mr_cache *entry, uintptr_t addr);
void mlx4_mr_mem_event_cb(enum rte_mem_event event_type, const void *addr,
			  size_t len, void *arg);
int mlx4_mr_update_mp(struct rte_eth_dev *dev, struct mlx4_mr_ctrl *mr_ctrl,
		      struct rte_mempool *mp);
void mlx4_mr_dump_dev(struct rte_eth_dev *dev);
void mlx4_mr_release(struct rte_eth_dev *dev);

/**
 * Look up LKey from given lookup table by linear search. Firstly look up the
 * last-hit entry. If miss, the entire array is searched. If found, update the
 * last-hit index and return LKey.
 *
 * @param lkp_tbl
 *   Pointer to lookup table.
 * @param[in,out] cached_idx
 *   Pointer to last-hit index.
 * @param n
 *   Size of lookup table.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
static __rte_always_inline uint32_t
mlx4_mr_lookup_cache(struct mlx4_mr_cache *lkp_tbl, uint16_t *cached_idx,
		     uint16_t n, uintptr_t addr)
{
	uint16_t idx;

	if (likely(addr >= lkp_tbl[*cached_idx].start &&
		   addr < lkp_tbl[*cached_idx].end))
		return lkp_tbl[*cached_idx].lkey;
	for (idx = 0; idx < n && lkp_tbl[idx].start != 0; ++idx) {
		if (addr >= lkp_tbl[idx].start &&
		    addr < lkp_tbl[idx].end) {
			/* Found. */
			*cached_idx = idx;
			return lkp_tbl[idx].lkey;
		}
	}
	return UINT32_MAX;
}

#endif /* RTE_PMD_MLX4_MR_H_ */
