/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

/**
 * @file
 * Tx queues configuration for mlx4 driver.
 */

#include <assert.h>
#include <errno.h>
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

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include "mlx4.h"
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

	DEBUG("%p: freeing WRs", (void *)txq);
	while (elts_tail != elts_head) {
		struct txq_elt *elt = &(*elts)[elts_tail];

		assert(elt->buf != NULL);
		rte_pktmbuf_free(elt->buf);
		elt->buf = NULL;
		if (++elts_tail == RTE_DIM(*elts))
			elts_tail = 0;
	}
	txq->elts_tail = txq->elts_head;
}

struct txq_mp2mr_mbuf_check_data {
	int ret;
};

/**
 * Callback function for rte_mempool_obj_iter() to check whether a given
 * mempool object looks like a mbuf.
 *
 * @param[in] mp
 *   The mempool pointer
 * @param[in] arg
 *   Context data (struct mlx4_txq_mp2mr_mbuf_check_data). Contains the
 *   return value.
 * @param[in] obj
 *   Object address.
 * @param index
 *   Object index, unused.
 */
static void
mlx4_txq_mp2mr_mbuf_check(struct rte_mempool *mp, void *arg, void *obj,
			  uint32_t index)
{
	struct txq_mp2mr_mbuf_check_data *data = arg;
	struct rte_mbuf *buf = obj;

	(void)index;
	/*
	 * Check whether mbuf structure fits element size and whether mempool
	 * pointer is valid.
	 */
	if (sizeof(*buf) > mp->elt_size || buf->pool != mp)
		data->ret = -1;
}

/**
 * Iterator function for rte_mempool_walk() to register existing mempools and
 * fill the MP to MR cache of a Tx queue.
 *
 * @param[in] mp
 *   Memory Pool to register.
 * @param *arg
 *   Pointer to Tx queue structure.
 */
static void
mlx4_txq_mp2mr_iter(struct rte_mempool *mp, void *arg)
{
	struct txq *txq = arg;
	struct txq_mp2mr_mbuf_check_data data = {
		.ret = 0,
	};

	/* Register mempool only if the first element looks like a mbuf. */
	if (rte_mempool_obj_iter(mp, mlx4_txq_mp2mr_mbuf_check, &data) == 0 ||
			data.ret == -1)
		return;
	mlx4_txq_mp2mr(txq, mp);
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
	uint32_t sq_size = (uint32_t)dqp->rq.offset - (uint32_t)dqp->sq.offset;

	sq->buf = (uint8_t *)dqp->buf.buf + dqp->sq.offset;
	/* Total length, including headroom and spare WQEs. */
	sq->eob = sq->buf + sq_size;
	sq->head = 0;
	sq->tail = 0;
	sq->txbb_cnt =
		(dqp->sq.wqe_cnt << dqp->sq.wqe_shift) >> MLX4_TXBB_SHIFT;
	sq->txbb_cnt_mask = sq->txbb_cnt - 1;
	sq->db = dqp->sdb;
	sq->doorbell_qpn = dqp->doorbell_qpn;
	sq->headroom_txbbs =
		(2048 + (1 << dqp->sq.wqe_shift)) >> MLX4_TXBB_SHIFT;
	cq->buf = dcq->buf.buf;
	cq->cqe_cnt = dcq->cqe_cnt;
	cq->set_ci_db = dcq->set_ci_db;
	cq->cqe_64 = (dcq->cqe_size & 64) ? 1 : 0;
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
	struct txq_elt (*elts)[desc];
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

	(void)conf; /* Thresholds configuration (ignored). */
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
		.elts_comp = 0,
		/*
		 * Request send completion every MLX4_PMD_TX_PER_COMP_REQ
		 * packets or at least 4 times per ring.
		 */
		.elts_comp_cd =
			RTE_MIN(MLX4_PMD_TX_PER_COMP_REQ, desc / 4),
		.elts_comp_cd_init =
			RTE_MIN(MLX4_PMD_TX_PER_COMP_REQ, desc / 4),
		.csum = priv->hw_csum,
		.csum_l2tun = priv->hw_csum_l2tun,
		/* Enable Tx loopback for VF devices. */
		.lb = !!priv->vf,
		.bounce_buf = bounce_buf,
	};
	txq->cq = ibv_create_cq(priv->ctx, desc, NULL, NULL, 0);
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
	txq->qp = ibv_create_qp(priv->pd, &qp_init_attr);
	if (!txq->qp) {
		rte_errno = errno ? errno : EINVAL;
		ERROR("%p: QP creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	txq->max_inline = qp_init_attr.cap.max_inline_data;
	ret = ibv_modify_qp
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
	ret = ibv_modify_qp
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
	ret = ibv_modify_qp
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
	ret = mlx4dv_init_obj(&mlxdv, MLX4DV_OBJ_QP | MLX4DV_OBJ_CQ);
	if (ret) {
		rte_errno = EINVAL;
		ERROR("%p: failed to obtain information needed for"
		      " accessing the device queues", (void *)dev);
		goto error;
	}
	mlx4_txq_fill_dv_obj_info(txq, &mlxdv);
	/* Pre-register known mempools. */
	rte_mempool_walk(mlx4_txq_mp2mr_iter, txq);
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
		claim_zero(ibv_destroy_qp(txq->qp));
	if (txq->cq)
		claim_zero(ibv_destroy_cq(txq->cq));
	for (i = 0; i != RTE_DIM(txq->mp2mr); ++i) {
		if (!txq->mp2mr[i].mp)
			break;
		assert(txq->mp2mr[i].mr);
		mlx4_mr_put(txq->mp2mr[i].mr);
	}
	rte_free(txq);
}
