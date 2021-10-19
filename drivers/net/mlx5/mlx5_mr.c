/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#include <rte_eal_memconfig.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_rwlock.h>

#include <mlx5_common_mp.h>
#include <mlx5_common_mr.h>

#include "mlx5.h"
#include "mlx5_mr.h"
#include "mlx5_rxtx.h"
#include "mlx5_rx.h"
#include "mlx5_tx.h"

/**
 * Callback for memory event. This can be called from both primary and secondary
 * process.
 *
 * @param event_type
 *   Memory event type.
 * @param addr
 *   Address of memory.
 * @param len
 *   Size of memory.
 */
void
mlx5_mr_mem_event_cb(enum rte_mem_event event_type, const void *addr,
		     size_t len, void *arg __rte_unused)
{
	struct mlx5_dev_ctx_shared *sh;
	struct mlx5_dev_list *dev_list = &mlx5_shared_data->mem_event_cb_list;

	/* Must be called from the primary process. */
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	switch (event_type) {
	case RTE_MEM_EVENT_FREE:
		rte_rwlock_write_lock(&mlx5_shared_data->mem_event_rwlock);
		/* Iterate all the existing mlx5 devices. */
		LIST_FOREACH(sh, dev_list, mem_event_cb)
			mlx5_free_mr_by_addr(&sh->share_cache,
					     sh->ibdev_name, addr, len);
		rte_rwlock_write_unlock(&mlx5_shared_data->mem_event_rwlock);
		break;
	case RTE_MEM_EVENT_ALLOC:
	default:
		break;
	}
}

/**
 * Bottom-half of LKey search on Tx.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param addr
 *   Search key.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
static uint32_t
mlx5_tx_addr2mr_bh(struct mlx5_txq_data *txq, uintptr_t addr)
{
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	struct mlx5_mr_ctrl *mr_ctrl = &txq->mr_ctrl;
	struct mlx5_priv *priv = txq_ctrl->priv;

	return mlx5_mr_addr2mr_bh(priv->sh->cdev->pd, &priv->mp_id,
				  &priv->sh->share_cache, mr_ctrl, addr,
				  priv->sh->cdev->config.mr_ext_memseg_en);
}

/**
 * Bottom-half of LKey search on Tx. If it can't be searched in the memseg
 * list, register the mempool of the mbuf as externally allocated memory.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param mb
 *   Pointer to mbuf.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
uint32_t
mlx5_tx_mb2mr_bh(struct mlx5_txq_data *txq, struct rte_mbuf *mb)
{
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	struct mlx5_mr_ctrl *mr_ctrl = &txq->mr_ctrl;
	struct mlx5_priv *priv = txq_ctrl->priv;
	uintptr_t addr = (uintptr_t)mb->buf_addr;
	uint32_t lkey;

	if (priv->sh->cdev->config.mr_mempool_reg_en) {
		struct rte_mempool *mp = NULL;
		struct mlx5_mprq_buf *buf;

		if (!RTE_MBUF_HAS_EXTBUF(mb)) {
			mp = mlx5_mb2mp(mb);
		} else if (mb->shinfo->free_cb == mlx5_mprq_buf_free_cb) {
			/* Recover MPRQ mempool. */
			buf = mb->shinfo->fcb_opaque;
			mp = buf->mp;
		}
		if (mp != NULL) {
			lkey = mlx5_mr_mempool2mr_bh(&priv->sh->share_cache,
						     mr_ctrl, mp, addr);
			/*
			 * Lookup can only fail on invalid input, e.g. "addr"
			 * is not from "mp" or "mp" has RTE_MEMPOOL_F_NON_IO set.
			 */
			if (lkey != UINT32_MAX)
				return lkey;
		}
		/* Fallback for generic mechanism in corner cases. */
	}
	return mlx5_tx_addr2mr_bh(txq, addr);
}

/**
 * Finds the first ethdev that match the device.
 * The existence of multiple ethdev per pci device is only with representors.
 * On such case, it is enough to get only one of the ports as they all share
 * the same ibv context.
 *
 * @param dev
 *   Pointer to the device.
 *
 * @return
 *   Pointer to the ethdev if found, NULL otherwise.
 */
static struct rte_eth_dev *
dev_to_eth_dev(struct rte_device *dev)
{
	uint16_t port_id;

	port_id = rte_eth_find_next_of(0, dev);
	if (port_id == RTE_MAX_ETHPORTS)
		return NULL;
	return &rte_eth_devices[port_id];
}

/**
 * Callback to DMA map external memory to a device.
 *
 * @param rte_dev
 *   Pointer to the generic device.
 * @param addr
 *   Starting virtual address of memory to be mapped.
 * @param iova
 *   Starting IOVA address of memory to be mapped.
 * @param len
 *   Length of memory segment being mapped.
 *
 * @return
 *   0 on success, negative value on error.
 */
int
mlx5_net_dma_map(struct rte_device *rte_dev, void *addr,
		 uint64_t iova __rte_unused, size_t len)
{
	struct rte_eth_dev *dev;
	struct mlx5_mr *mr;
	struct mlx5_priv *priv;
	struct mlx5_dev_ctx_shared *sh;

	dev = dev_to_eth_dev(rte_dev);
	if (!dev) {
		DRV_LOG(WARNING, "unable to find matching ethdev "
				 "to device %s", rte_dev->name);
		rte_errno = ENODEV;
		return -1;
	}
	priv = dev->data->dev_private;
	sh = priv->sh;
	mr = mlx5_create_mr_ext(sh->cdev->pd, (uintptr_t)addr, len,
				SOCKET_ID_ANY, sh->share_cache.reg_mr_cb);
	if (!mr) {
		DRV_LOG(WARNING,
			"port %u unable to dma map", dev->data->port_id);
		rte_errno = EINVAL;
		return -1;
	}
	rte_rwlock_write_lock(&sh->share_cache.rwlock);
	LIST_INSERT_HEAD(&sh->share_cache.mr_list, mr, mr);
	/* Insert to the global cache table. */
	mlx5_mr_insert_cache(&sh->share_cache, mr);
	rte_rwlock_write_unlock(&sh->share_cache.rwlock);
	return 0;
}

/**
 * Callback to DMA unmap external memory to a device.
 *
 * @param rte_dev
 *   Pointer to the generic device.
 * @param addr
 *   Starting virtual address of memory to be unmapped.
 * @param iova
 *   Starting IOVA address of memory to be unmapped.
 * @param len
 *   Length of memory segment being unmapped.
 *
 * @return
 *   0 on success, negative value on error.
 */
int
mlx5_net_dma_unmap(struct rte_device *rte_dev, void *addr,
		   uint64_t iova __rte_unused, size_t len __rte_unused)
{
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;
	struct mlx5_dev_ctx_shared *sh;
	struct mlx5_mr *mr;
	struct mr_cache_entry entry;

	dev = dev_to_eth_dev(rte_dev);
	if (!dev) {
		DRV_LOG(WARNING, "unable to find matching ethdev to device %s",
			rte_dev->name);
		rte_errno = ENODEV;
		return -1;
	}
	priv = dev->data->dev_private;
	sh = priv->sh;
	rte_rwlock_write_lock(&sh->share_cache.rwlock);
	mr = mlx5_mr_lookup_list(&sh->share_cache, &entry, (uintptr_t)addr);
	if (!mr) {
		rte_rwlock_write_unlock(&sh->share_cache.rwlock);
		DRV_LOG(WARNING, "address 0x%" PRIxPTR " wasn't registered to device %s",
			(uintptr_t)addr, rte_dev->name);
		rte_errno = EINVAL;
		return -1;
	}
	LIST_REMOVE(mr, mr);
	DRV_LOG(DEBUG, "port %u remove MR(%p) from list", dev->data->port_id,
	      (void *)mr);
	mlx5_mr_free(mr, sh->share_cache.dereg_mr_cb);
	mlx5_mr_rebuild_cache(&sh->share_cache);
	/*
	 * No explicit wmb is needed after updating dev_gen due to
	 * store-release ordering in unlock that provides the
	 * implicit barrier at the software visible level.
	 */
	++sh->share_cache.dev_gen;
	DRV_LOG(DEBUG, "broadcasting local cache flush, gen=%d",
	      sh->share_cache.dev_gen);
	rte_rwlock_write_unlock(&sh->share_cache.rwlock);
	return 0;
}
