/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox.
 */

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mempool.h>
#include <rte_malloc.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_glue.h"

struct mlx5_check_mempool_data {
	int ret;
	char *start;
	char *end;
};

/* Called by mlx5_check_mempool() when iterating the memory chunks. */
static void
mlx5_check_mempool_cb(struct rte_mempool *mp,
		      void *opaque, struct rte_mempool_memhdr *memhdr,
		      unsigned int mem_idx)
{
	struct mlx5_check_mempool_data *data = opaque;

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
 *   Pointer to the start address of the mempool virtual memory area
 * @param[out] end
 *   Pointer to the end address of the mempool virtual memory area
 *
 * @return
 *   0 on success (mempool is virtually contiguous), -1 on error.
 */
static int mlx5_check_mempool(struct rte_mempool *mp, uintptr_t *start,
	uintptr_t *end)
{
	struct mlx5_check_mempool_data data;

	memset(&data, 0, sizeof(data));
	rte_mempool_mem_iter(mp, mlx5_check_mempool_cb, &data);
	*start = (uintptr_t)data.start;
	*end = (uintptr_t)data.end;

	return data.ret;
}

/**
 * Register a Memory Region (MR) <-> Memory Pool (MP) association in
 * txq->mp2mr[]. If mp2mr[] is full, remove an entry first.
 *
 * This function should only be called by txq_mp2mr().
 *
 * @param priv
 *   Pointer to private structure.
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] mp
 *   Memory Pool for which a Memory Region lkey must be returned.
 * @param idx
 *   Index of the next available entry.
 *
 * @return
 *   mr on success, NULL on failure.
 */
struct mlx5_mr*
priv_txq_mp2mr_reg(struct priv *priv, struct mlx5_txq_data *txq,
		   struct rte_mempool *mp, unsigned int idx)
{
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	struct mlx5_mr *mr;

	/* Add a new entry, register MR first. */
	DEBUG("%p: discovered new memory pool \"%s\" (%p)",
	      (void *)txq_ctrl, mp->name, (void *)mp);
	mr = priv_mr_get(priv, mp);
	if (mr == NULL) {
		if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
			DEBUG("Using unregistered mempool 0x%p(%s) in secondary process,"
			     " please create mempool before rte_eth_dev_start()",
			     (void *)mp, mp->name);
			return NULL;
		}
		mr = priv_mr_new(priv, mp);
	}
	if (unlikely(mr == NULL)) {
		DEBUG("%p: unable to configure MR, ibv_reg_mr() failed.",
		      (void *)txq_ctrl);
		return NULL;
	}
	if (unlikely(idx == RTE_DIM(txq->mp2mr))) {
		/* Table is full, remove oldest entry. */
		DEBUG("%p: MR <-> MP table full, dropping oldest entry.",
		      (void *)txq_ctrl);
		--idx;
		priv_mr_release(priv, txq->mp2mr[0]);
		memmove(&txq->mp2mr[0], &txq->mp2mr[1],
			(sizeof(txq->mp2mr) - sizeof(txq->mp2mr[0])));
	}
	/* Store the new entry. */
	txq_ctrl->txq.mp2mr[idx] = mr;
	DEBUG("%p: new MR lkey for MP \"%s\" (%p): 0x%08" PRIu32,
	      (void *)txq_ctrl, mp->name, (void *)mp,
	      txq_ctrl->txq.mp2mr[idx]->lkey);
	return mr;
}

/**
 * Register a Memory Region (MR) <-> Memory Pool (MP) association in
 * txq->mp2mr[]. If mp2mr[] is full, remove an entry first.
 *
 * This function should only be called by txq_mp2mr().
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] mp
 *   Memory Pool for which a Memory Region lkey must be returned.
 * @param idx
 *   Index of the next available entry.
 *
 * @return
 *   mr on success, NULL on failure.
 */
struct mlx5_mr*
mlx5_txq_mp2mr_reg(struct mlx5_txq_data *txq, struct rte_mempool *mp,
		   unsigned int idx)
{
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	struct mlx5_mr *mr;

	priv_lock(txq_ctrl->priv);
	mr = priv_txq_mp2mr_reg(txq_ctrl->priv, txq, mp, idx);
	priv_unlock(txq_ctrl->priv);
	return mr;
}

struct mlx5_mp2mr_mbuf_check_data {
	int ret;
};

/**
 * Callback function for rte_mempool_obj_iter() to check whether a given
 * mempool object looks like a mbuf.
 *
 * @param[in] mp
 *   The mempool pointer
 * @param[in] arg
 *   Context data (struct txq_mp2mr_mbuf_check_data). Contains the
 *   return value.
 * @param[in] obj
 *   Object address.
 * @param index
 *   Object index, unused.
 */
static void
txq_mp2mr_mbuf_check(struct rte_mempool *mp, void *arg, void *obj,
	uint32_t index __rte_unused)
{
	struct mlx5_mp2mr_mbuf_check_data *data = arg;
	struct rte_mbuf *buf = obj;

	/*
	 * Check whether mbuf structure fits element size and whether mempool
	 * pointer is valid.
	 */
	if (sizeof(*buf) > mp->elt_size || buf->pool != mp)
		data->ret = -1;
}

/**
 * Iterator function for rte_mempool_walk() to register existing mempools and
 * fill the MP to MR cache of a TX queue.
 *
 * @param[in] mp
 *   Memory Pool to register.
 * @param *arg
 *   Pointer to TX queue structure.
 */
void
mlx5_mp2mr_iter(struct rte_mempool *mp, void *arg)
{
	struct priv *priv = (struct priv *)arg;
	struct mlx5_mp2mr_mbuf_check_data data = {
		.ret = 0,
	};
	struct mlx5_mr *mr;

	/* Register mempool only if the first element looks like a mbuf. */
	if (rte_mempool_obj_iter(mp, txq_mp2mr_mbuf_check, &data) == 0 ||
			data.ret == -1)
		return;
	mr = priv_mr_get(priv, mp);
	if (mr) {
		priv_mr_release(priv, mr);
		return;
	}
	priv_mr_new(priv, mp);
}

/**
 * Register a new memory region from the mempool and store it in the memory
 * region list.
 *
 * @param  priv
 *   Pointer to private structure.
 * @param mp
 *   Pointer to the memory pool to register.
 * @return
 *   The memory region on success.
 */
struct mlx5_mr*
priv_mr_new(struct priv *priv, struct rte_mempool *mp)
{
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();
	uintptr_t start;
	uintptr_t end;
	unsigned int i;
	struct mlx5_mr *mr;

	mr = rte_zmalloc_socket(__func__, sizeof(*mr), 0, mp->socket_id);
	if (!mr) {
		DEBUG("unable to configure MR, ibv_reg_mr() failed.");
		return NULL;
	}
	if (mlx5_check_mempool(mp, &start, &end) != 0) {
		ERROR("mempool %p: not virtually contiguous",
		      (void *)mp);
		return NULL;
	}
	DEBUG("mempool %p area start=%p end=%p size=%zu",
	      (void *)mp, (void *)start, (void *)end,
	      (size_t)(end - start));
	/* Save original addresses for exact MR lookup. */
	mr->start = start;
	mr->end = end;
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
	mr->mr = mlx5_glue->reg_mr(priv->pd, (void *)start, end - start,
				   IBV_ACCESS_LOCAL_WRITE);
	mr->mp = mp;
	mr->lkey = rte_cpu_to_be_32(mr->mr->lkey);
	rte_atomic32_inc(&mr->refcnt);
	DEBUG("%p: new Memory Region %p refcnt: %d", (void *)priv,
	      (void *)mr, rte_atomic32_read(&mr->refcnt));
	LIST_INSERT_HEAD(&priv->mr, mr, next);
	return mr;
}

/**
 * Search the memory region object in the memory region list.
 *
 * @param  priv
 *   Pointer to private structure.
 * @param mp
 *   Pointer to the memory pool to register.
 * @return
 *   The memory region on success.
 */
struct mlx5_mr*
priv_mr_get(struct priv *priv, struct rte_mempool *mp)
{
	struct mlx5_mr *mr;

	assert(mp);
	if (LIST_EMPTY(&priv->mr))
		return NULL;
	LIST_FOREACH(mr, &priv->mr, next) {
		if (mr->mp == mp) {
			rte_atomic32_inc(&mr->refcnt);
			DEBUG("Memory Region %p refcnt: %d",
			      (void *)mr, rte_atomic32_read(&mr->refcnt));
			return mr;
		}
	}
	return NULL;
}

/**
 * Release the memory region object.
 *
 * @param  mr
 *   Pointer to memory region to release.
 *
 * @return
 *   0 on success, errno on failure.
 */
int
priv_mr_release(struct priv *priv, struct mlx5_mr *mr)
{
	(void)priv;
	assert(mr);
	DEBUG("Memory Region %p refcnt: %d",
	      (void *)mr, rte_atomic32_read(&mr->refcnt));
	if (rte_atomic32_dec_and_test(&mr->refcnt)) {
		claim_zero(mlx5_glue->dereg_mr(mr->mr));
		LIST_REMOVE(mr, next);
		rte_free(mr);
		return 0;
	}
	return EBUSY;
}

/**
 * Verify the flow list is empty
 *
 * @param priv
 *  Pointer to private structure.
 *
 * @return the number of object not released.
 */
int
priv_mr_verify(struct priv *priv)
{
	int ret = 0;
	struct mlx5_mr *mr;

	LIST_FOREACH(mr, &priv->mr, next) {
		DEBUG("%p: mr %p still referenced", (void *)priv,
		      (void *)mr);
		++ret;
	}
	return ret;
}
