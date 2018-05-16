/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

/**
 * @file
 * Tx queues configuration for mlx4 driver.
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include "mlx4.h"
#include "mlx4_glue.h"
#include "mlx4_prm.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

/**
 * Free Tx queue elements.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 */
static void
mlx4_txq_free_elts(struct txq *txq)
{
	unsigned int elts_head = txq->elts_head;
	unsigned int elts_tail = txq->elts_tail;
	struct txq_elt (*elts)[txq->elts_n] = txq->elts;
	unsigned int elts_m = txq->elts_n - 1;

	DEBUG("%p: freeing WRs", (void *)txq);
	while (elts_tail != elts_head) {
		struct txq_elt *elt = &(*elts)[elts_tail++ & elts_m];

		assert(elt->buf != NULL);
		rte_pktmbuf_free(elt->buf);
		elt->buf = NULL;
		elt->wqe = NULL;
	}
	txq->elts_tail = txq->elts_head;
}

/**
 * Retrieves information needed in order to directly access the Tx queue.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param mlxdv
 *   Pointer to device information for this Tx queue.
 */
static void
mlx4_txq_fill_dv_obj_info(struct txq *txq, struct mlx4dv_obj *mlxdv)
{
	struct mlx4_sq *sq = &txq->msq;
	struct mlx4_cq *cq = &txq->mcq;
	struct mlx4dv_qp *dqp = mlxdv->qp.out;
	struct mlx4dv_cq *dcq = mlxdv->cq.out;

	/* Total length, including headroom and spare WQEs. */
	sq->size = (uint32_t)dqp->rq.offset - (uint32_t)dqp->sq.offset;
	sq->buf = (uint8_t *)dqp->buf.buf + dqp->sq.offset;
	sq->eob = sq->buf + sq->size;
	uint32_t headroom_size = 2048 + (1 << dqp->sq.wqe_shift);
	/* Continuous headroom size bytes must always stay freed. */
	sq->remain_size = sq->size - headroom_size;
	sq->owner_opcode = MLX4_OPCODE_SEND | (0u << MLX4_SQ_OWNER_BIT);
	sq->stamp = rte_cpu_to_be_32(MLX4_SQ_STAMP_VAL |
				     (0u << MLX4_SQ_OWNER_BIT));
	sq->db = dqp->sdb;
	sq->doorbell_qpn = dqp->doorbell_qpn;
	cq->buf = dcq->buf.buf;
	cq->cqe_cnt = dcq->cqe_cnt;
	cq->set_ci_db = dcq->set_ci_db;
	cq->cqe_64 = (dcq->cqe_size & 64) ? 1 : 0;
}

/**
 * Returns the per-port supported offloads.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   Supported Tx offloads.
 */
uint64_t
mlx4_get_tx_port_offloads(struct priv *priv)
{
	uint64_t offloads = DEV_TX_OFFLOAD_MULTI_SEGS;

	if (priv->hw_csum) {
		offloads |= (DEV_TX_OFFLOAD_IPV4_CKSUM |
			     DEV_TX_OFFLOAD_UDP_CKSUM |
			     DEV_TX_OFFLOAD_TCP_CKSUM);
	}
	if (priv->hw_csum_l2tun)
		offloads |= DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM;
	return offloads;
}

/**
 * DPDK callback to configure a Tx queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Tx queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
int
mlx4_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket, const struct rte_eth_txconf *conf)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx4dv_obj mlxdv;
	struct mlx4dv_qp dv_qp;
	struct mlx4dv_cq dv_cq;
	struct txq_elt (*elts)[rte_align32pow2(desc)];
	struct ibv_qp_init_attr qp_init_attr;
	struct txq *txq;
	uint8_t *bounce_buf;
	struct mlx4_malloc_vec vec[] = {
		{
			.align = RTE_CACHE_LINE_SIZE,
			.size = sizeof(*txq),
			.addr = (void **)&txq,
		},
		{
			.align = RTE_CACHE_LINE_SIZE,
			.size = sizeof(*elts),
			.addr = (void **)&elts,
		},
		{
			.align = RTE_CACHE_LINE_SIZE,
			.size = MLX4_MAX_WQE_SIZE,
			.addr = (void **)&bounce_buf,
		},
	};
	int ret;
	uint64_t offloads;

	offloads = conf->offloads | dev->data->dev_conf.txmode.offloads;

	DEBUG("%p: configuring queue %u for %u descriptors",
	      (void *)dev, idx, desc);

	if (idx >= dev->data->nb_tx_queues) {
		rte_errno = EOVERFLOW;
		ERROR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, idx, dev->data->nb_tx_queues);
		return -rte_errno;
	}
	txq = dev->data->tx_queues[idx];
	if (txq) {
		rte_errno = EEXIST;
		DEBUG("%p: Tx queue %u already configured, release it first",
		      (void *)dev, idx);
		return -rte_errno;
	}
	if (!desc) {
		rte_errno = EINVAL;
		ERROR("%p: invalid number of Tx descriptors", (void *)dev);
		return -rte_errno;
	}
	if (desc != RTE_DIM(*elts)) {
		desc = RTE_DIM(*elts);
		WARN("%p: increased number of descriptors in Tx queue %u"
		     " to the next power of two (%u)",
		     (void *)dev, idx, desc);
	}
	/* Allocate and initialize Tx queue. */
	mlx4_zmallocv_socket("TXQ", vec, RTE_DIM(vec), socket);
	if (!txq) {
		ERROR("%p: unable to allocate queue index %u",
		      (void *)dev, idx);
		return -rte_errno;
	}
	*txq = (struct txq){
		.priv = priv,
		.stats = {
			.idx = idx,
		},
		.socket = socket,
		.elts_n = desc,
		.elts = elts,
		.elts_head = 0,
		.elts_tail = 0,
		/*
		 * Request send completion every MLX4_PMD_TX_PER_COMP_REQ
		 * packets or at least 4 times per ring.
		 */
		.elts_comp_cd =
			RTE_MIN(MLX4_PMD_TX_PER_COMP_REQ, desc / 4),
		.elts_comp_cd_init =
			RTE_MIN(MLX4_PMD_TX_PER_COMP_REQ, desc / 4),
		.csum = priv->hw_csum &&
			(offloads & (DEV_TX_OFFLOAD_IPV4_CKSUM |
					   DEV_TX_OFFLOAD_UDP_CKSUM |
					   DEV_TX_OFFLOAD_TCP_CKSUM)),
		.csum_l2tun = priv->hw_csum_l2tun &&
			      (offloads &
			       DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM),
		/* Enable Tx loopback for VF devices. */
		.lb = !!priv->vf,
		.bounce_buf = bounce_buf,
	};
	txq->cq = mlx4_glue->create_cq(priv->ctx, desc, NULL, NULL, 0);
	if (!txq->cq) {
		rte_errno = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	qp_init_attr = (struct ibv_qp_init_attr){
		.send_cq = txq->cq,
		.recv_cq = txq->cq,
		.cap = {
			.max_send_wr =
				RTE_MIN(priv->device_attr.max_qp_wr, desc),
			.max_send_sge = 1,
			.max_inline_data = MLX4_PMD_MAX_INLINE,
		},
		.qp_type = IBV_QPT_RAW_PACKET,
		/* No completion events must occur by default. */
		.sq_sig_all = 0,
	};
	txq->qp = mlx4_glue->create_qp(priv->pd, &qp_init_attr);
	if (!txq->qp) {
		rte_errno = errno ? errno : EINVAL;
		ERROR("%p: QP creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	txq->max_inline = qp_init_attr.cap.max_inline_data;
	ret = mlx4_glue->modify_qp
		(txq->qp,
		 &(struct ibv_qp_attr){
			.qp_state = IBV_QPS_INIT,
			.port_num = priv->port,
		 },
		 IBV_QP_STATE | IBV_QP_PORT);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_INIT failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	ret = mlx4_glue->modify_qp
		(txq->qp,
		 &(struct ibv_qp_attr){
			.qp_state = IBV_QPS_RTR,
		 },
		 IBV_QP_STATE);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_RTR failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	ret = mlx4_glue->modify_qp
		(txq->qp,
		 &(struct ibv_qp_attr){
			.qp_state = IBV_QPS_RTS,
		 },
		 IBV_QP_STATE);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_RTS failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	/* Retrieve device queue information. */
	mlxdv.cq.in = txq->cq;
	mlxdv.cq.out = &dv_cq;
	mlxdv.qp.in = txq->qp;
	mlxdv.qp.out = &dv_qp;
	ret = mlx4_glue->dv_init_obj(&mlxdv, MLX4DV_OBJ_QP | MLX4DV_OBJ_CQ);
	if (ret) {
		rte_errno = EINVAL;
		ERROR("%p: failed to obtain information needed for"
		      " accessing the device queues", (void *)dev);
		goto error;
	}
	mlx4_txq_fill_dv_obj_info(txq, &mlxdv);
	/* Save first wqe pointer in the first element. */
	(&(*txq->elts)[0])->wqe =
		(volatile struct mlx4_wqe_ctrl_seg *)txq->msq.buf;
	if (mlx4_mr_btree_init(&txq->mr_ctrl.cache_bh,
			       MLX4_MR_BTREE_CACHE_N, socket)) {
		/* rte_errno is already set. */
		goto error;
	}
	/* Save pointer of global generation number to check memory event. */
	txq->mr_ctrl.dev_gen_ptr = &priv->mr.dev_gen;
	DEBUG("%p: adding Tx queue %p to list", (void *)dev, (void *)txq);
	dev->data->tx_queues[idx] = txq;
	return 0;
error:
	dev->data->tx_queues[idx] = NULL;
	ret = rte_errno;
	mlx4_tx_queue_release(txq);
	rte_errno = ret;
	assert(rte_errno > 0);
	return -rte_errno;
}

/**
 * DPDK callback to release a Tx queue.
 *
 * @param dpdk_txq
 *   Generic Tx queue pointer.
 */
void
mlx4_tx_queue_release(void *dpdk_txq)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	struct priv *priv;
	unsigned int i;

	if (txq == NULL)
		return;
	priv = txq->priv;
	for (i = 0; i != priv->dev->data->nb_tx_queues; ++i)
		if (priv->dev->data->tx_queues[i] == txq) {
			DEBUG("%p: removing Tx queue %p from list",
			      (void *)priv->dev, (void *)txq);
			priv->dev->data->tx_queues[i] = NULL;
			break;
		}
	mlx4_txq_free_elts(txq);
	if (txq->qp)
		claim_zero(mlx4_glue->destroy_qp(txq->qp));
	if (txq->cq)
		claim_zero(mlx4_glue->destroy_cq(txq->cq));
	mlx4_mr_btree_free(&txq->mr_ctrl.cache_bh);
	rte_free(txq);
}
