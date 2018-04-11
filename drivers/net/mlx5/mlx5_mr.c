/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
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
mlx5_check_mempool_cb(struct rte_mempool *mp __rte_unused,
		      void *opaque, struct rte_mempool_memhdr *memhdr,
		      unsigned int mem_idx __rte_unused)
{
	struct mlx5_check_mempool_data *data = opaque;

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
static int
mlx5_check_mempool(struct rte_mempool *mp, uintptr_t *start,
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
 * @param txq
 *   Pointer to TX queue structure.
 * @param[in] mp
 *   Memory Pool for which a Memory Region lkey must be returned.
 * @param idx
 *   Index of the next available entry.
 *
 * @return
 *   mr on success, NULL on failure and rte_errno is set.
 */
struct mlx5_mr *
mlx5_txq_mp2mr_reg(struct mlx5_txq_data *txq, struct rte_mempool *mp,
		   unsigned int idx)
{
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	struct rte_eth_dev *dev;
	struct mlx5_mr *mr;

	rte_spinlock_lock(&txq_ctrl->priv->mr_lock);
	/* Add a new entry, register MR first. */
	DRV_LOG(DEBUG, "port %u discovered new memory pool \"%s\" (%p)",
		txq_ctrl->priv->dev->data->port_id, mp->name, (void *)mp);
	dev = txq_ctrl->priv->dev;
	mr = mlx5_mr_get(dev, mp);
	if (mr == NULL) {
		if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
			DRV_LOG(DEBUG,
				"port %u using unregistered mempool 0x%p(%s)"
				" in secondary process, please create mempool"
				" before rte_eth_dev_start()",
				txq_ctrl->priv->dev->data->port_id,
				(void *)mp, mp->name);
			rte_spinlock_unlock(&txq_ctrl->priv->mr_lock);
			rte_errno = ENOTSUP;
			return NULL;
		}
		mr = mlx5_mr_new(dev, mp);
	}
	if (unlikely(mr == NULL)) {
		DRV_LOG(DEBUG,
			"port %u unable to configure memory region,"
			" ibv_reg_mr() failed.",
			txq_ctrl->priv->dev->data->port_id);
		rte_spinlock_unlock(&txq_ctrl->priv->mr_lock);
		return NULL;
	}
	if (unlikely(idx == RTE_DIM(txq->mp2mr))) {
		/* Table is full, remove oldest entry. */
		DRV_LOG(DEBUG,
			"port %u memory region <-> memory pool table full, "
			" dropping oldest entry",
			txq_ctrl->priv->dev->data->port_id);
		--idx;
		mlx5_mr_release(txq->mp2mr[0]);
		memmove(&txq->mp2mr[0], &txq->mp2mr[1],
			(sizeof(txq->mp2mr) - sizeof(txq->mp2mr[0])));
	}
	/* Store the new entry. */
	txq_ctrl->txq.mp2mr[idx] = mr;
	DRV_LOG(DEBUG,
		"port %u new memory region lkey for MP \"%s\" (%p): 0x%08"
		PRIu32,
		txq_ctrl->priv->dev->data->port_id, mp->name, (void *)mp,
		txq_ctrl->txq.mp2mr[idx]->lkey);
	rte_spinlock_unlock(&txq_ctrl->priv->mr_lock);
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
	mr = mlx5_mr_get(priv->dev, mp);
	if (mr) {
		mlx5_mr_release(mr);
		return;
	}
	mr = mlx5_mr_new(priv->dev, mp);
	if (!mr)
		DRV_LOG(ERR, "port %u cannot create memory region: %s",
			priv->dev->data->port_id, strerror(rte_errno));
}

/**
 * Register a new memory region from the mempool and store it in the memory
 * region list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mp
 *   Pointer to the memory pool to register.
 *
 * @return
 *   The memory region on success, NULL on failure and rte_errno is set.
 */
struct mlx5_mr *
mlx5_mr_new(struct rte_eth_dev *dev, struct rte_mempool *mp)
{
	struct priv *priv = dev->data->dev_private;
	const struct rte_memseg *ms;
	uintptr_t start;
	uintptr_t end;
	struct mlx5_mr *mr;

	mr = rte_zmalloc_socket(__func__, sizeof(*mr), 0, mp->socket_id);
	if (!mr) {
		DRV_LOG(DEBUG,
			"port %u unable to configure memory region,"
			" ibv_reg_mr() failed.",
			dev->data->port_id);
		rte_errno = ENOMEM;
		return NULL;
	}
	if (mlx5_check_mempool(mp, &start, &end) != 0) {
		DRV_LOG(ERR, "port %u mempool %p: not virtually contiguous",
			dev->data->port_id, (void *)mp);
		rte_errno = ENOMEM;
		return NULL;
	}
	DRV_LOG(DEBUG, "port %u mempool %p area start=%p end=%p size=%zu",
		dev->data->port_id, (void *)mp, (void *)start, (void *)end,
		(size_t)(end - start));
	/* Save original addresses for exact MR lookup. */
	mr->start = start;
	mr->end = end;

	/* Round start and end to page boundary if found in memory segments. */
	ms = rte_mem_virt2memseg((void *)start);
	if (ms != NULL)
		start = RTE_ALIGN_FLOOR(start, ms->hugepage_sz);
	ms = rte_mem_virt2memseg((void *)end);
	if (ms != NULL)
		end = RTE_ALIGN_CEIL(end, ms->hugepage_sz);

	DRV_LOG(DEBUG,
		"port %u mempool %p using start=%p end=%p size=%zu for memory"
		" region",
		dev->data->port_id, (void *)mp, (void *)start, (void *)end,
		(size_t)(end - start));
	mr->mr = mlx5_glue->reg_mr(priv->pd, (void *)start, end - start,
				   IBV_ACCESS_LOCAL_WRITE);
	if (!mr->mr) {
		rte_errno = ENOMEM;
		return NULL;
	}
	mr->mp = mp;
	mr->lkey = rte_cpu_to_be_32(mr->mr->lkey);
	rte_atomic32_inc(&mr->refcnt);
	DRV_LOG(DEBUG, "port %u new memory Region %p refcnt: %d",
		dev->data->port_id, (void *)mr, rte_atomic32_read(&mr->refcnt));
	LIST_INSERT_HEAD(&priv->mr, mr, next);
	return mr;
}

/**
 * Search the memory region object in the memory region list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mp
 *   Pointer to the memory pool to register.
 *
 * @return
 *   The memory region on success.
 */
struct mlx5_mr *
mlx5_mr_get(struct rte_eth_dev *dev, struct rte_mempool *mp)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_mr *mr;

	assert(mp);
	if (LIST_EMPTY(&priv->mr))
		return NULL;
	LIST_FOREACH(mr, &priv->mr, next) {
		if (mr->mp == mp) {
			rte_atomic32_inc(&mr->refcnt);
			DRV_LOG(DEBUG, "port %u memory region %p refcnt: %d",
				dev->data->port_id, (void *)mr,
				rte_atomic32_read(&mr->refcnt));
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
 *   1 while a reference on it exists, 0 when freed.
 */
int
mlx5_mr_release(struct mlx5_mr *mr)
{
	assert(mr);
	DRV_LOG(DEBUG, "memory region %p refcnt: %d", (void *)mr,
		rte_atomic32_read(&mr->refcnt));
	if (rte_atomic32_dec_and_test(&mr->refcnt)) {
		claim_zero(mlx5_glue->dereg_mr(mr->mr));
		LIST_REMOVE(mr, next);
		rte_free(mr);
		return 0;
	}
	return 1;
}

/**
 * Verify the flow list is empty
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The number of object not released.
 */
int
mlx5_mr_verify(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int ret = 0;
	struct mlx5_mr *mr;

	LIST_FOREACH(mr, &priv->mr, next) {
		DRV_LOG(DEBUG, "port %u memory region %p still referenced",
			dev->data->port_id, (void *)mr);
		++ret;
	}
	return ret;
}
