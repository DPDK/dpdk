/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox
 */

/**
 * @file
 * Memory management functions for mlx4 driver.
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_spinlock.h>

#include "mlx4_glue.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

struct mlx4_check_mempool_data {
	int ret;
	char *start;
	char *end;
};

/**
 * Called by mlx4_check_mempool() when iterating the memory chunks.
 *
 * @param[in] mp
 *   Pointer to memory pool (unused).
 * @param[in, out] data
 *   Pointer to shared buffer with mlx4_check_mempool().
 * @param[in] memhdr
 *   Pointer to mempool chunk header.
 * @param mem_idx
 *   Mempool element index (unused).
 */
static void
mlx4_check_mempool_cb(struct rte_mempool *mp, void *opaque,
		      struct rte_mempool_memhdr *memhdr,
		      unsigned int mem_idx)
{
	struct mlx4_check_mempool_data *data = opaque;

	(void)mp;
	(void)mem_idx;
	/* It already failed, skip the next chunks. */
	if (data->ret != 0)
		return;
	/* It is the first chunk. */
	if (data->start == NULL && data->end == NULL) {
		data->start = memhdr->addr;
		data->end = data->start + memhdr->len;
		return;
	}
	if (data->end == memhdr->addr) {
		data->end += memhdr->len;
		return;
	}
	if (data->start == (char *)memhdr->addr + memhdr->len) {
		data->start -= memhdr->len;
		return;
	}
	/* Error, mempool is not virtually contiguous. */
	data->ret = -1;
}

/**
 * Check if a mempool can be used: it must be virtually contiguous.
 *
 * @param[in] mp
 *   Pointer to memory pool.
 * @param[out] start
 *   Pointer to the start address of the mempool virtual memory area.
 * @param[out] end
 *   Pointer to the end address of the mempool virtual memory area.
 *
 * @return
 *   0 on success (mempool is virtually contiguous), -1 on error.
 */
static int
mlx4_check_mempool(struct rte_mempool *mp, uintptr_t *start, uintptr_t *end)
{
	struct mlx4_check_mempool_data data;

	memset(&data, 0, sizeof(data));
	rte_mempool_mem_iter(mp, mlx4_check_mempool_cb, &data);
	*start = (uintptr_t)data.start;
	*end = (uintptr_t)data.end;
	return data.ret;
}

/**
 * Obtain a memory region from a memory pool.
 *
 * If a matching memory region already exists, it is returned with its
 * reference count incremented, otherwise a new one is registered.
 *
 * @param priv
 *   Pointer to private structure.
 * @param mp
 *   Pointer to memory pool.
 *
 * @return
 *   Memory region pointer, NULL in case of error and rte_errno is set.
 */
struct mlx4_mr *
mlx4_mr_get(struct priv *priv, struct rte_mempool *mp)
{
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();
	uintptr_t start;
	uintptr_t end;
	unsigned int i;
	struct mlx4_mr *mr;

	if (mlx4_check_mempool(mp, &start, &end) != 0) {
		rte_errno = EINVAL;
		ERROR("mempool %p: not virtually contiguous",
			(void *)mp);
		return NULL;
	}
	DEBUG("mempool %p area start=%p end=%p size=%zu",
	      (void *)mp, (void *)start, (void *)end,
	      (size_t)(end - start));
	/* Round start and end to page boundary if found in memory segments. */
	for (i = 0; (i < RTE_MAX_MEMSEG) && (ms[i].addr != NULL); ++i) {
		uintptr_t addr = (uintptr_t)ms[i].addr;
		size_t len = ms[i].len;
		unsigned int align = ms[i].hugepage_sz;

		if ((start > addr) && (start < addr + len))
			start = RTE_ALIGN_FLOOR(start, align);
		if ((end > addr) && (end < addr + len))
			end = RTE_ALIGN_CEIL(end, align);
	}
	DEBUG("mempool %p using start=%p end=%p size=%zu for MR",
	      (void *)mp, (void *)start, (void *)end,
	      (size_t)(end - start));
	rte_spinlock_lock(&priv->mr_lock);
	LIST_FOREACH(mr, &priv->mr, next)
		if (mp == mr->mp && start >= mr->start && end <= mr->end)
			break;
	if (mr) {
		++mr->refcnt;
		goto release;
	}
	mr = rte_malloc(__func__, sizeof(*mr), 0);
	if (!mr) {
		rte_errno = ENOMEM;
		goto release;
	}
	*mr = (struct mlx4_mr){
		.start = start,
		.end = end,
		.refcnt = 1,
		.priv = priv,
		.mr = mlx4_glue->reg_mr(priv->pd, (void *)start, end - start,
					IBV_ACCESS_LOCAL_WRITE),
		.mp = mp,
	};
	if (mr->mr) {
		mr->lkey = mr->mr->lkey;
		LIST_INSERT_HEAD(&priv->mr, mr, next);
	} else {
		rte_free(mr);
		mr = NULL;
		rte_errno = errno ? errno : EINVAL;
	}
release:
	rte_spinlock_unlock(&priv->mr_lock);
	return mr;
}

/**
 * Release a memory region.
 *
 * This function decrements its reference count and destroys it after
 * reaching 0.
 *
 * Note to avoid race conditions given this function may be used from the
 * data plane, it's extremely important that each user holds its own
 * reference.
 *
 * @param mr
 *   Memory region to release.
 */
void
mlx4_mr_put(struct mlx4_mr *mr)
{
	struct priv *priv = mr->priv;

	rte_spinlock_lock(&priv->mr_lock);
	assert(mr->refcnt);
	if (--mr->refcnt)
		goto release;
	LIST_REMOVE(mr, next);
	claim_zero(mlx4_glue->dereg_mr(mr->mr));
	rte_free(mr);
release:
	rte_spinlock_unlock(&priv->mr_lock);
}

/**
 * Add memory region (MR) <-> memory pool (MP) association to txq->mp2mr[].
 * If mp2mr[] is full, remove an entry first.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param[in] mp
 *   Memory pool for which a memory region lkey must be added.
 * @param[in] i
 *   Index in memory pool (MP) where to add memory region (MR).
 *
 * @return
 *   Added mr->lkey on success, (uint32_t)-1 on failure.
 */
uint32_t
mlx4_txq_add_mr(struct txq *txq, struct rte_mempool *mp, uint32_t i)
{
	struct mlx4_mr *mr;

	/* Add a new entry, register MR first. */
	DEBUG("%p: discovered new memory pool \"%s\" (%p)",
	      (void *)txq, mp->name, (void *)mp);
	mr = mlx4_mr_get(txq->priv, mp);
	if (unlikely(mr == NULL)) {
		DEBUG("%p: unable to configure MR, mlx4_mr_get() failed",
		      (void *)txq);
		return (uint32_t)-1;
	}
	if (unlikely(i == RTE_DIM(txq->mp2mr))) {
		/* Table is full, remove oldest entry. */
		DEBUG("%p: MR <-> MP table full, dropping oldest entry.",
		      (void *)txq);
		--i;
		mlx4_mr_put(txq->mp2mr[0].mr);
		memmove(&txq->mp2mr[0], &txq->mp2mr[1],
			(sizeof(txq->mp2mr) - sizeof(txq->mp2mr[0])));
	}
	/* Store the new entry. */
	txq->mp2mr[i].mp = mp;
	txq->mp2mr[i].mr = mr;
	txq->mp2mr[i].lkey = mr->lkey;
	DEBUG("%p: new MR lkey for MP \"%s\" (%p): 0x%08" PRIu32,
	      (void *)txq, mp->name, (void *)mp, txq->mp2mr[i].lkey);
	return txq->mp2mr[i].lkey;
}
