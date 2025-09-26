/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_txrx.h"
#include "nbl_include.h"

static int nbl_res_txrx_alloc_rings(void *priv, u16 tx_num, u16 rx_num, u16 queue_offset)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_txrx_mgt *txrx_mgt = res_mgt->txrx_mgt;

	txrx_mgt->tx_rings = rte_calloc("nbl_txrings", tx_num,
					sizeof(struct nbl_res_tx_ring *), 0);
	if (!txrx_mgt->tx_rings) {
		NBL_LOG(ERR, "Allocate the tx rings array failed");
		return -ENOMEM;
	}

	txrx_mgt->rx_rings = rte_calloc("nbl_rxrings", rx_num,
					sizeof(struct nbl_res_rx_ring *), 0);
	if (!txrx_mgt->tx_rings) {
		NBL_LOG(ERR, "Allocate the rx rings array failed");
		rte_free(txrx_mgt->tx_rings);
		return -ENOMEM;
	}

	txrx_mgt->queue_offset = queue_offset;

	return 0;
}

static void nbl_res_txrx_remove_rings(void *priv)
{
	struct nbl_resource_mgt *res_mgt = (struct nbl_resource_mgt *)priv;
	struct nbl_txrx_mgt *txrx_mgt = res_mgt->txrx_mgt;

	rte_free(txrx_mgt->tx_rings);
	rte_free(txrx_mgt->rx_rings);
}

static int nbl_res_txrx_start_tx_ring(void *priv,
				      struct nbl_start_tx_ring_param *param,
				      u64 *dma_addr)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(param);
	RTE_SET_USED(dma_addr);
	return 0;
}

static void nbl_res_txrx_stop_tx_ring(void *priv, u16 queue_idx)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(queue_idx);
}

static void nbl_res_txrx_release_txring(void *priv, u16 queue_idx)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(queue_idx);
}

static int nbl_res_txrx_start_rx_ring(void *priv,
				      struct nbl_start_rx_ring_param *param,
				      u64 *dma_addr)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(param);
	RTE_SET_USED(dma_addr);
	return 0;
}

static int nbl_res_alloc_rx_bufs(void *priv, u16 queue_idx)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(queue_idx);
	return 0;
}

static void nbl_res_txrx_stop_rx_ring(void *priv, u16 queue_idx)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(queue_idx);
}

static void nbl_res_txrx_release_rx_ring(void *priv, u16 queue_idx)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(queue_idx);
}

static void nbl_res_txrx_update_rx_ring(void *priv, u16 index)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(index);
}

/* NBL_TXRX_SET_OPS(ops_name, func)
 *
 * Use X Macros to reduce setup and remove codes.
 */
#define NBL_TXRX_OPS_TBL							\
do {										\
	NBL_TXRX_SET_OPS(alloc_rings, nbl_res_txrx_alloc_rings);		\
	NBL_TXRX_SET_OPS(remove_rings, nbl_res_txrx_remove_rings);		\
	NBL_TXRX_SET_OPS(start_tx_ring, nbl_res_txrx_start_tx_ring);		\
	NBL_TXRX_SET_OPS(stop_tx_ring, nbl_res_txrx_stop_tx_ring);		\
	NBL_TXRX_SET_OPS(release_tx_ring, nbl_res_txrx_release_txring);		\
	NBL_TXRX_SET_OPS(start_rx_ring, nbl_res_txrx_start_rx_ring);		\
	NBL_TXRX_SET_OPS(alloc_rx_bufs, nbl_res_alloc_rx_bufs);			\
	NBL_TXRX_SET_OPS(stop_rx_ring, nbl_res_txrx_stop_rx_ring);		\
	NBL_TXRX_SET_OPS(release_rx_ring, nbl_res_txrx_release_rx_ring);	\
	NBL_TXRX_SET_OPS(update_rx_ring, nbl_res_txrx_update_rx_ring);		\
} while (0)

/* Structure starts here, adding an op should not modify anything below */
static int nbl_txrx_setup_mgt(struct nbl_txrx_mgt **txrx_mgt)
{
	*txrx_mgt = rte_zmalloc("nbl_txrx_mgt", sizeof(struct nbl_txrx_mgt), 0);
	if (!*txrx_mgt)
		return -ENOMEM;

	return 0;
}

static void nbl_txrx_remove_mgt(struct nbl_txrx_mgt **txrx_mgt)
{
	rte_free(*txrx_mgt);
	*txrx_mgt = NULL;
}

int nbl_txrx_mgt_start(struct nbl_resource_mgt *res_mgt)
{
	struct nbl_txrx_mgt **txrx_mgt;

	txrx_mgt = &NBL_RES_MGT_TO_TXRX_MGT(res_mgt);

	return nbl_txrx_setup_mgt(txrx_mgt);
}

void nbl_txrx_mgt_stop(struct nbl_resource_mgt *res_mgt)
{
	struct nbl_txrx_mgt **txrx_mgt;

	txrx_mgt = &NBL_RES_MGT_TO_TXRX_MGT(res_mgt);

	if (!(*txrx_mgt))
		return;

	nbl_txrx_remove_mgt(txrx_mgt);
}

int nbl_txrx_setup_ops(struct nbl_resource_ops *res_ops)
{
#define NBL_TXRX_SET_OPS(name, func) do {res_ops->NBL_NAME(name) = func; ; } while (0)
	NBL_TXRX_OPS_TBL;
#undef  NBL_TXRX_SET_OPS

	return 0;
}

void nbl_txrx_remove_ops(struct nbl_resource_ops *res_ops)
{
#define NBL_TXRX_SET_OPS(name, func)		\
do {						\
	res_ops->NBL_NAME(name) = NULL;		\
	RTE_SET_USED(func);			\
} while (0)
	NBL_TXRX_OPS_TBL;
#undef  NBL_TXRX_SET_OPS
}
