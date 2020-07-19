/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_MR_H_
#define RTE_PMD_MLX5_COMMON_MR_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>


#include <rte_rwlock.h>
#include <rte_bitmap.h>
#include <rte_memory.h>

#include "mlx5_glue.h"
#include "mlx5_common_mp.h"

/* Size of per-queue MR cache array for linear search. */
#define MLX5_MR_CACHE_N 8
#define MLX5_MR_BTREE_CACHE_N 256

/* mlx5 PMD MR struct. */
struct mlx5_pmd_mr {
	uint32_t	     lkey;
	void		     *addr;
	size_t		     len;
	void		     *obj;  /* verbs mr object or devx umem object. */
};

/**
 * mr operations typedef
 */
typedef int (*mlx5_reg_mr_t)(void *pd, void *addr, size_t length,
			     struct mlx5_pmd_mr *pmd_mr);
typedef void (*mlx5_dereg_mr_t)(struct mlx5_pmd_mr *pmd_mr);

/* Memory Region object. */
struct mlx5_mr {
	LIST_ENTRY(mlx5_mr) mr; /**< Pointer to the prev/next entry. */
	struct mlx5_pmd_mr pmd_mr; /* PMD memory region. */
	const struct rte_memseg_list *msl;
	int ms_base_idx; /* Start index of msl->memseg_arr[]. */
	int ms_n; /* Number of memsegs in use. */
	uint32_t ms_bmp_n; /* Number of bits in memsegs bit-mask. */
	struct rte_bitmap *ms_bmp; /* Bit-mask of memsegs belonged to MR. */
};

/* Cache entry for Memory Region. */
struct mr_cache_entry {
	uintptr_t start; /* Start address of MR. */
	uintptr_t end; /* End address of MR. */
	uint32_t lkey; /* rte_cpu_to_be_32(lkey). */
} __rte_packed;

/* MR Cache table for Binary search. */
struct mlx5_mr_btree {
	uint16_t len; /* Number of entries. */
	uint16_t size; /* Total number of entries. */
	int overflow; /* Mark failure of table expansion. */
	struct mr_cache_entry (*table)[];
} __rte_packed;

/* Per-queue MR control descriptor. */
struct mlx5_mr_ctrl {
	uint32_t *dev_gen_ptr; /* Generation number of device to poll. */
	uint32_t cur_gen; /* Generation number saved to flush caches. */
	uint16_t mru; /* Index of last hit entry in top-half cache. */
	uint16_t head; /* Index of the oldest entry in top-half cache. */
	struct mr_cache_entry cache[MLX5_MR_CACHE_N]; /* Cache for top-half. */
	struct mlx5_mr_btree cache_bh; /* Cache for bottom-half. */
} __rte_packed;

LIST_HEAD(mlx5_mr_list, mlx5_mr);

/* Global per-device MR cache. */
struct mlx5_mr_share_cache {
	uint32_t dev_gen; /* Generation number to flush local caches. */
	rte_rwlock_t rwlock; /* MR cache Lock. */
	struct mlx5_mr_btree cache; /* Global MR cache table. */
	struct mlx5_mr_list mr_list; /* Registered MR list. */
	struct mlx5_mr_list mr_free_list; /* Freed MR list. */
	mlx5_reg_mr_t reg_mr_cb; /* Callback to reg_mr func */
	mlx5_dereg_mr_t dereg_mr_cb; /* Callback to dereg_mr func */
} __rte_packed;

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
mlx5_mr_lookup_lkey(struct mr_cache_entry *lkp_tbl, uint16_t *cached_idx,
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

__rte_internal
int mlx5_mr_btree_init(struct mlx5_mr_btree *bt, int n, int socket);
__rte_internal
void mlx5_mr_btree_free(struct mlx5_mr_btree *bt);
__rte_internal
void mlx5_mr_btree_dump(struct mlx5_mr_btree *bt __rte_unused);
__rte_internal
uint32_t mlx5_mr_addr2mr_bh(void *pd, struct mlx5_mp_id *mp_id,
			    struct mlx5_mr_share_cache *share_cache,
			    struct mlx5_mr_ctrl *mr_ctrl,
			    uintptr_t addr, unsigned int mr_ext_memseg_en);
__rte_internal
void mlx5_mr_release_cache(struct mlx5_mr_share_cache *mr_cache);
__rte_internal
void mlx5_mr_dump_cache(struct mlx5_mr_share_cache *share_cache __rte_unused);
__rte_internal
void mlx5_mr_rebuild_cache(struct mlx5_mr_share_cache *share_cache);
__rte_internal
void mlx5_mr_flush_local_cache(struct mlx5_mr_ctrl *mr_ctrl);
__rte_internal
int
mlx5_mr_insert_cache(struct mlx5_mr_share_cache *share_cache,
		     struct mlx5_mr *mr);
__rte_internal
uint32_t
mlx5_mr_lookup_cache(struct mlx5_mr_share_cache *share_cache,
		     struct mr_cache_entry *entry, uintptr_t addr);
__rte_internal
struct mlx5_mr *
mlx5_mr_lookup_list(struct mlx5_mr_share_cache *share_cache,
		    struct mr_cache_entry *entry, uintptr_t addr);
__rte_internal
struct mlx5_mr *
mlx5_create_mr_ext(void *pd, uintptr_t addr, size_t len, int socket_id,
		   mlx5_reg_mr_t reg_mr_cb);
__rte_internal
uint32_t
mlx5_mr_create_primary(void *pd,
		       struct mlx5_mr_share_cache *share_cache,
		       struct mr_cache_entry *entry, uintptr_t addr,
		       unsigned int mr_ext_memseg_en);
__rte_internal
int
mlx5_common_verbs_reg_mr(void *pd, void *addr, size_t length,
			 struct mlx5_pmd_mr *pmd_mr);
__rte_internal
void
mlx5_common_verbs_dereg_mr(struct mlx5_pmd_mr *pmd_mr);
#endif /* RTE_PMD_MLX5_COMMON_MR_H_ */
