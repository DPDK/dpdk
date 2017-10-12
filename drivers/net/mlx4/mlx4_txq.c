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
#include "mlx4_autoconf.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

/**
 * Allocate Tx queue elements.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 * @param elts_n
 *   Number of elements to allocate.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mlx4_txq_alloc_elts(struct txq *txq, unsigned int elts_n)
{
	unsigned int i;
	struct txq_elt (*elts)[elts_n] =
		rte_calloc_socket("TXQ", 1, sizeof(*elts), 0, txq->socket);
	int ret = 0;

	if (elts == NULL) {
		ERROR("%p: can't allocate packets array", (void *)txq);
		ret = ENOMEM;
		goto error;
	}
	for (i = 0; (i != elts_n); ++i) {
		struct txq_elt *elt = &(*elts)[i];

		elt->buf = NULL;
	}
	DEBUG("%p: allocated and configured %u WRs", (void *)txq, elts_n);
	txq->elts_n = elts_n;
	txq->elts = elts;
	txq->elts_head = 0;
	txq->elts_tail = 0;
	txq->elts_comp = 0;
	/*
	 * Request send completion every MLX4_PMD_TX_PER_COMP_REQ packets or
	 * at least 4 times per ring.
	 */
	txq->elts_comp_cd_init =
		((MLX4_PMD_TX_PER_COMP_REQ < (elts_n / 4)) ?
		 MLX4_PMD_TX_PER_COMP_REQ : (elts_n / 4));
	txq->elts_comp_cd = txq->elts_comp_cd_init;
	assert(ret == 0);
	return 0;
error:
	rte_free(elts);
	DEBUG("%p: failed, freed everything", (void *)txq);
	assert(ret > 0);
	rte_errno = ret;
	return -rte_errno;
}

/**
 * Free Tx queue elements.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 */
static void
mlx4_txq_free_elts(struct txq *txq)
{
	unsigned int elts_n = txq->elts_n;
	unsigned int elts_head = txq->elts_head;
	unsigned int elts_tail = txq->elts_tail;
	struct txq_elt (*elts)[elts_n] = txq->elts;

	DEBUG("%p: freeing WRs", (void *)txq);
	txq->elts_n = 0;
	txq->elts_head = 0;
	txq->elts_tail = 0;
	txq->elts_comp = 0;
	txq->elts_comp_cd = 0;
	txq->elts_comp_cd_init = 0;
	txq->elts = NULL;
	if (elts == NULL)
		return;
	while (elts_tail != elts_head) {
		struct txq_elt *elt = &(*elts)[elts_tail];

		assert(elt->buf != NULL);
		rte_pktmbuf_free(elt->buf);
#ifndef NDEBUG
		/* Poisoning. */
		memset(elt, 0x77, sizeof(*elt));
#endif
		if (++elts_tail == elts_n)
			elts_tail = 0;
	}
	rte_free(elts);
}

/**
 * Clean up a Tx queue.
 *
 * Destroy objects, free allocated memory and reset the structure for reuse.
 *
 * @param txq
 *   Pointer to Tx queue structure.
 */
void
mlx4_txq_cleanup(struct txq *txq)
{
	size_t i;

	DEBUG("cleaning up %p", (void *)txq);
	mlx4_txq_free_elts(txq);
	if (txq->qp != NULL)
		claim_zero(ibv_destroy_qp(txq->qp));
	if (txq->cq != NULL)
		claim_zero(ibv_destroy_cq(txq->cq));
	for (i = 0; (i != RTE_DIM(txq->mp2mr)); ++i) {
		if (txq->mp2mr[i].mp == NULL)
			break;
		assert(txq->mp2mr[i].mr != NULL);
		claim_zero(ibv_dereg_mr(txq->mp2mr[i].mr));
	}
	memset(txq, 0, sizeof(*txq));
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
 * Configure a Tx queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param txq
 *   Pointer to Tx queue structure.
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
static int
mlx4_txq_setup(struct rte_eth_dev *dev, struct txq *txq, uint16_t desc,
	       unsigned int socket, const struct rte_eth_txconf *conf)
{
	struct priv *priv = dev->data->dev_private;
	struct txq tmpl = {
		.priv = priv,
		.socket = socket
	};
	union {
		struct ibv_qp_init_attr init;
		struct ibv_qp_attr mod;
	} attr;
	int ret;

	(void)conf; /* Thresholds configuration (ignored). */
	if (priv == NULL) {
		rte_errno = EINVAL;
		goto error;
	}
	if (desc == 0) {
		rte_errno = EINVAL;
		ERROR("%p: invalid number of Tx descriptors", (void *)dev);
		goto error;
	}
	/* MRs will be registered in mp2mr[] later. */
	tmpl.cq = ibv_create_cq(priv->ctx, desc, NULL, NULL, 0);
	if (tmpl.cq == NULL) {
		rte_errno = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	DEBUG("priv->device_attr.max_qp_wr is %d",
	      priv->device_attr.max_qp_wr);
	DEBUG("priv->device_attr.max_sge is %d",
	      priv->device_attr.max_sge);
	attr.init = (struct ibv_qp_init_attr){
		/* CQ to be associated with the send queue. */
		.send_cq = tmpl.cq,
		/* CQ to be associated with the receive queue. */
		.recv_cq = tmpl.cq,
		.cap = {
			/* Max number of outstanding WRs. */
			.max_send_wr = ((priv->device_attr.max_qp_wr < desc) ?
					priv->device_attr.max_qp_wr :
					desc),
			/* Max number of scatter/gather elements in a WR. */
			.max_send_sge = 1,
			.max_inline_data = MLX4_PMD_MAX_INLINE,
		},
		.qp_type = IBV_QPT_RAW_PACKET,
		/*
		 * Do *NOT* enable this, completions events are managed per
		 * Tx burst.
		 */
		.sq_sig_all = 0,
	};
	tmpl.qp = ibv_create_qp(priv->pd, &attr.init);
	if (tmpl.qp == NULL) {
		rte_errno = errno ? errno : EINVAL;
		ERROR("%p: QP creation failure: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	/* ibv_create_qp() updates this value. */
	tmpl.max_inline = attr.init.cap.max_inline_data;
	attr.mod = (struct ibv_qp_attr){
		/* Move the QP to this state. */
		.qp_state = IBV_QPS_INIT,
		/* Primary port number. */
		.port_num = priv->port
	};
	ret = ibv_modify_qp(tmpl.qp, &attr.mod, IBV_QP_STATE | IBV_QP_PORT);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_INIT failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	ret = mlx4_txq_alloc_elts(&tmpl, desc);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: TXQ allocation failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	attr.mod = (struct ibv_qp_attr){
		.qp_state = IBV_QPS_RTR
	};
	ret = ibv_modify_qp(tmpl.qp, &attr.mod, IBV_QP_STATE);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_RTR failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	attr.mod.qp_state = IBV_QPS_RTS;
	ret = ibv_modify_qp(tmpl.qp, &attr.mod, IBV_QP_STATE);
	if (ret) {
		rte_errno = ret;
		ERROR("%p: QP state to IBV_QPS_RTS failed: %s",
		      (void *)dev, strerror(rte_errno));
		goto error;
	}
	/* Clean up txq in case we're reinitializing it. */
	DEBUG("%p: cleaning-up old txq just in case", (void *)txq);
	mlx4_txq_cleanup(txq);
	*txq = tmpl;
	DEBUG("%p: txq updated with %p", (void *)txq, (void *)&tmpl);
	/* Pre-register known mempools. */
	rte_mempool_walk(mlx4_txq_mp2mr_iter, txq);
	return 0;
error:
	ret = rte_errno;
	mlx4_txq_cleanup(&tmpl);
	rte_errno = ret;
	assert(rte_errno > 0);
	return -rte_errno;
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
	struct txq *txq = dev->data->tx_queues[idx];
	int ret;

	DEBUG("%p: configuring queue %u for %u descriptors",
	      (void *)dev, idx, desc);
	if (idx >= dev->data->nb_tx_queues) {
		rte_errno = EOVERFLOW;
		ERROR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, idx, dev->data->nb_tx_queues);
		return -rte_errno;
	}
	if (txq != NULL) {
		DEBUG("%p: reusing already allocated queue index %u (%p)",
		      (void *)dev, idx, (void *)txq);
		if (priv->started) {
			rte_errno = EEXIST;
			return -rte_errno;
		}
		dev->data->tx_queues[idx] = NULL;
		mlx4_txq_cleanup(txq);
	} else {
		txq = rte_calloc_socket("TXQ", 1, sizeof(*txq), 0, socket);
		if (txq == NULL) {
			rte_errno = ENOMEM;
			ERROR("%p: unable to allocate queue index %u",
			      (void *)dev, idx);
			return -rte_errno;
		}
	}
	ret = mlx4_txq_setup(dev, txq, desc, socket, conf);
	if (ret) {
		rte_free(txq);
	} else {
		txq->stats.idx = idx;
		DEBUG("%p: adding Tx queue %p to list",
		      (void *)dev, (void *)txq);
		dev->data->tx_queues[idx] = txq;
	}
	return ret;
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
	mlx4_txq_cleanup(txq);
	rte_free(txq);
}
