/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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

#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_common.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5_utils.h"
#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"

/**
 * Allocate TX queue elements.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param elts_n
 *   Number of elements to allocate.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
txq_alloc_elts(struct txq *txq, unsigned int elts_n)
{
	unsigned int i;
	struct txq_elt (*elts)[elts_n] =
		rte_calloc_socket("TXQ", 1, sizeof(*elts), 0, txq->socket);
	linear_t (*elts_linear)[elts_n] =
		rte_calloc_socket("TXQ", 1, sizeof(*elts_linear), 0,
				  txq->socket);
	struct ibv_mr *mr_linear = NULL;
	int ret = 0;

	if ((elts == NULL) || (elts_linear == NULL)) {
		ERROR("%p: can't allocate packets array", (void *)txq);
		ret = ENOMEM;
		goto error;
	}
	mr_linear =
		ibv_reg_mr(txq->priv->pd, elts_linear, sizeof(*elts_linear),
			   (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));
	if (mr_linear == NULL) {
		ERROR("%p: unable to configure MR, ibv_reg_mr() failed",
		      (void *)txq);
		ret = EINVAL;
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
	/* Request send completion every MLX5_PMD_TX_PER_COMP_REQ packets or
	 * at least 4 times per ring. */
	txq->elts_comp_cd_init =
		((MLX5_PMD_TX_PER_COMP_REQ < (elts_n / 4)) ?
		 MLX5_PMD_TX_PER_COMP_REQ : (elts_n / 4));
	txq->elts_comp_cd = txq->elts_comp_cd_init;
	txq->elts_linear = elts_linear;
	txq->mr_linear = mr_linear;
	assert(ret == 0);
	return 0;
error:
	if (mr_linear != NULL)
		claim_zero(ibv_dereg_mr(mr_linear));

	rte_free(elts_linear);
	rte_free(elts);

	DEBUG("%p: failed, freed everything", (void *)txq);
	assert(ret > 0);
	return ret;
}

/**
 * Free TX queue elements.
 *
 * @param txq
 *   Pointer to TX queue structure.
 */
static void
txq_free_elts(struct txq *txq)
{
	unsigned int elts_n = txq->elts_n;
	unsigned int elts_head = txq->elts_head;
	unsigned int elts_tail = txq->elts_tail;
	struct txq_elt (*elts)[elts_n] = txq->elts;
	linear_t (*elts_linear)[elts_n] = txq->elts_linear;
	struct ibv_mr *mr_linear = txq->mr_linear;

	DEBUG("%p: freeing WRs", (void *)txq);
	txq->elts_n = 0;
	txq->elts_head = 0;
	txq->elts_tail = 0;
	txq->elts_comp = 0;
	txq->elts_comp_cd = 0;
	txq->elts_comp_cd_init = 0;
	txq->elts = NULL;
	txq->elts_linear = NULL;
	txq->mr_linear = NULL;
	if (mr_linear != NULL)
		claim_zero(ibv_dereg_mr(mr_linear));

	rte_free(elts_linear);
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
 * Clean up a TX queue.
 *
 * Destroy objects, free allocated memory and reset the structure for reuse.
 *
 * @param txq
 *   Pointer to TX queue structure.
 */
void
txq_cleanup(struct txq *txq)
{
	struct ibv_exp_release_intf_params params;
	size_t i;

	DEBUG("cleaning up %p", (void *)txq);
	txq_free_elts(txq);
	txq->poll_cnt = NULL;
#if MLX5_PMD_MAX_INLINE > 0
	txq->send_pending_inline = NULL;
#endif
	txq->send_flush = NULL;
	if (txq->if_qp != NULL) {
		assert(txq->priv != NULL);
		assert(txq->priv->ctx != NULL);
		assert(txq->qp != NULL);
		params = (struct ibv_exp_release_intf_params){
			.comp_mask = 0,
		};
		claim_zero(ibv_exp_release_intf(txq->priv->ctx,
						txq->if_qp,
						&params));
	}
	if (txq->if_cq != NULL) {
		assert(txq->priv != NULL);
		assert(txq->priv->ctx != NULL);
		assert(txq->cq != NULL);
		params = (struct ibv_exp_release_intf_params){
			.comp_mask = 0,
		};
		claim_zero(ibv_exp_release_intf(txq->priv->ctx,
						txq->if_cq,
						&params));
	}
	if (txq->qp != NULL)
		claim_zero(ibv_destroy_qp(txq->qp));
	if (txq->cq != NULL)
		claim_zero(ibv_destroy_cq(txq->cq));
	if (txq->rd != NULL) {
		struct ibv_exp_destroy_res_domain_attr attr = {
			.comp_mask = 0,
		};

		assert(txq->priv != NULL);
		assert(txq->priv->ctx != NULL);
		claim_zero(ibv_exp_destroy_res_domain(txq->priv->ctx,
						      txq->rd,
						      &attr));
	}
	for (i = 0; (i != RTE_DIM(txq->mp2mr)); ++i) {
		if (txq->mp2mr[i].mp == NULL)
			break;
		assert(txq->mp2mr[i].mr != NULL);
		claim_zero(ibv_dereg_mr(txq->mp2mr[i].mr));
	}
	memset(txq, 0, sizeof(*txq));
}

/**
 * Configure a TX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param txq
 *   Pointer to TX queue structure.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
txq_setup(struct rte_eth_dev *dev, struct txq *txq, uint16_t desc,
	  unsigned int socket, const struct rte_eth_txconf *conf)
{
	struct priv *priv = mlx5_get_priv(dev);
	struct txq tmpl = {
		.priv = priv,
		.socket = socket
	};
	union {
		struct ibv_exp_query_intf_params params;
		struct ibv_exp_qp_init_attr init;
		struct ibv_exp_res_domain_init_attr rd;
		struct ibv_exp_cq_init_attr cq;
		struct ibv_exp_qp_attr mod;
	} attr;
	enum ibv_exp_query_intf_status status;
	int ret = 0;

	(void)conf; /* Thresholds configuration (ignored). */
	if ((desc == 0) || (desc % MLX5_PMD_SGE_WR_N)) {
		ERROR("%p: invalid number of TX descriptors (must be a"
		      " multiple of %d)", (void *)dev, MLX5_PMD_SGE_WR_N);
		return EINVAL;
	}
	desc /= MLX5_PMD_SGE_WR_N;
	/* MRs will be registered in mp2mr[] later. */
	attr.rd = (struct ibv_exp_res_domain_init_attr){
		.comp_mask = (IBV_EXP_RES_DOMAIN_THREAD_MODEL |
			      IBV_EXP_RES_DOMAIN_MSG_MODEL),
		.thread_model = IBV_EXP_THREAD_SINGLE,
		.msg_model = IBV_EXP_MSG_HIGH_BW,
	};
	tmpl.rd = ibv_exp_create_res_domain(priv->ctx, &attr.rd);
	if (tmpl.rd == NULL) {
		ret = ENOMEM;
		ERROR("%p: RD creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.cq = (struct ibv_exp_cq_init_attr){
		.comp_mask = IBV_EXP_CQ_INIT_ATTR_RES_DOMAIN,
		.res_domain = tmpl.rd,
	};
	tmpl.cq = ibv_exp_create_cq(priv->ctx, desc, NULL, NULL, 0, &attr.cq);
	if (tmpl.cq == NULL) {
		ret = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	DEBUG("priv->device_attr.max_qp_wr is %d",
	      priv->device_attr.max_qp_wr);
	DEBUG("priv->device_attr.max_sge is %d",
	      priv->device_attr.max_sge);
	attr.init = (struct ibv_exp_qp_init_attr){
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
			.max_send_sge = ((priv->device_attr.max_sge <
					  MLX5_PMD_SGE_WR_N) ?
					 priv->device_attr.max_sge :
					 MLX5_PMD_SGE_WR_N),
#if MLX5_PMD_MAX_INLINE > 0
			.max_inline_data = MLX5_PMD_MAX_INLINE,
#endif
		},
		.qp_type = IBV_QPT_RAW_PACKET,
		/* Do *NOT* enable this, completions events are managed per
		 * TX burst. */
		.sq_sig_all = 0,
		.pd = priv->pd,
		.res_domain = tmpl.rd,
		.comp_mask = (IBV_EXP_QP_INIT_ATTR_PD |
			      IBV_EXP_QP_INIT_ATTR_RES_DOMAIN),
	};
	tmpl.qp = ibv_exp_create_qp(priv->ctx, &attr.init);
	if (tmpl.qp == NULL) {
		ret = (errno ? errno : EINVAL);
		ERROR("%p: QP creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
#if MLX5_PMD_MAX_INLINE > 0
	/* ibv_create_qp() updates this value. */
	tmpl.max_inline = attr.init.cap.max_inline_data;
#endif
	attr.mod = (struct ibv_exp_qp_attr){
		/* Move the QP to this state. */
		.qp_state = IBV_QPS_INIT,
		/* Primary port number. */
		.port_num = priv->port
	};
	ret = ibv_exp_modify_qp(tmpl.qp, &attr.mod,
				(IBV_EXP_QP_STATE | IBV_EXP_QP_PORT));
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_INIT failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	ret = txq_alloc_elts(&tmpl, desc);
	if (ret) {
		ERROR("%p: TXQ allocation failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.mod = (struct ibv_exp_qp_attr){
		.qp_state = IBV_QPS_RTR
	};
	ret = ibv_exp_modify_qp(tmpl.qp, &attr.mod, IBV_EXP_QP_STATE);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_RTR failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.mod.qp_state = IBV_QPS_RTS;
	ret = ibv_exp_modify_qp(tmpl.qp, &attr.mod, IBV_EXP_QP_STATE);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_RTS failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.params = (struct ibv_exp_query_intf_params){
		.intf_scope = IBV_EXP_INTF_GLOBAL,
		.intf = IBV_EXP_INTF_CQ,
		.obj = tmpl.cq,
	};
	tmpl.if_cq = ibv_exp_query_intf(priv->ctx, &attr.params, &status);
	if (tmpl.if_cq == NULL) {
		ret = EINVAL;
		ERROR("%p: CQ interface family query failed with status %d",
		      (void *)dev, status);
		goto error;
	}
	attr.params = (struct ibv_exp_query_intf_params){
		.intf_scope = IBV_EXP_INTF_GLOBAL,
		.intf = IBV_EXP_INTF_QP_BURST,
		.obj = tmpl.qp,
#ifdef HAVE_VERBS_VLAN_INSERTION
		.intf_version = 1,
#endif
#ifdef HAVE_EXP_QP_BURST_CREATE_ENABLE_MULTI_PACKET_SEND_WR
		/* Enable multi-packet send if supported. */
		.family_flags =
			(priv->mps ?
			 IBV_EXP_QP_BURST_CREATE_ENABLE_MULTI_PACKET_SEND_WR :
			 0),
#endif
	};
	tmpl.if_qp = ibv_exp_query_intf(priv->ctx, &attr.params, &status);
	if (tmpl.if_qp == NULL) {
		ret = EINVAL;
		ERROR("%p: QP interface family query failed with status %d",
		      (void *)dev, status);
		goto error;
	}
	/* Clean up txq in case we're reinitializing it. */
	DEBUG("%p: cleaning-up old txq just in case", (void *)txq);
	txq_cleanup(txq);
	*txq = tmpl;
	txq->poll_cnt = txq->if_cq->poll_cnt;
#if MLX5_PMD_MAX_INLINE > 0
	txq->send_pending_inline = txq->if_qp->send_pending_inline;
#ifdef HAVE_VERBS_VLAN_INSERTION
	txq->send_pending_inline_vlan = txq->if_qp->send_pending_inline_vlan;
#endif
#endif
#if MLX5_PMD_SGE_WR_N > 1
	txq->send_pending_sg_list = txq->if_qp->send_pending_sg_list;
#ifdef HAVE_VERBS_VLAN_INSERTION
	txq->send_pending_sg_list_vlan = txq->if_qp->send_pending_sg_list_vlan;
#endif
#endif
	txq->send_pending = txq->if_qp->send_pending;
#ifdef HAVE_VERBS_VLAN_INSERTION
	txq->send_pending_vlan = txq->if_qp->send_pending_vlan;
#endif
	txq->send_flush = txq->if_qp->send_flush;
	DEBUG("%p: txq updated with %p", (void *)txq, (void *)&tmpl);
	/* Pre-register known mempools. */
	rte_mempool_walk(txq_mp2mr_iter, txq);
	assert(ret == 0);
	return 0;
error:
	txq_cleanup(&tmpl);
	assert(ret > 0);
	return ret;
}

/**
 * DPDK callback to configure a TX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   TX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
mlx5_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket, const struct rte_eth_txconf *conf)
{
	struct priv *priv = dev->data->dev_private;
	struct txq *txq = (*priv->txqs)[idx];
	int ret;

	if (mlx5_is_secondary())
		return -E_RTE_SECONDARY;

	priv_lock(priv);
	DEBUG("%p: configuring queue %u for %u descriptors",
	      (void *)dev, idx, desc);
	if (idx >= priv->txqs_n) {
		ERROR("%p: queue index out of range (%u >= %u)",
		      (void *)dev, idx, priv->txqs_n);
		priv_unlock(priv);
		return -EOVERFLOW;
	}
	if (txq != NULL) {
		DEBUG("%p: reusing already allocated queue index %u (%p)",
		      (void *)dev, idx, (void *)txq);
		if (priv->started) {
			priv_unlock(priv);
			return -EEXIST;
		}
		(*priv->txqs)[idx] = NULL;
		txq_cleanup(txq);
	} else {
		txq = rte_calloc_socket("TXQ", 1, sizeof(*txq), 0, socket);
		if (txq == NULL) {
			ERROR("%p: unable to allocate queue index %u",
			      (void *)dev, idx);
			priv_unlock(priv);
			return -ENOMEM;
		}
	}
	ret = txq_setup(dev, txq, desc, socket, conf);
	if (ret)
		rte_free(txq);
	else {
		txq->stats.idx = idx;
		DEBUG("%p: adding TX queue %p to list",
		      (void *)dev, (void *)txq);
		(*priv->txqs)[idx] = txq;
		/* Update send callback. */
		dev->tx_pkt_burst = mlx5_tx_burst;
	}
	priv_unlock(priv);
	return -ret;
}

/**
 * DPDK callback to release a TX queue.
 *
 * @param dpdk_txq
 *   Generic TX queue pointer.
 */
void
mlx5_tx_queue_release(void *dpdk_txq)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	struct priv *priv;
	unsigned int i;

	if (mlx5_is_secondary())
		return;

	if (txq == NULL)
		return;
	priv = txq->priv;
	priv_lock(priv);
	for (i = 0; (i != priv->txqs_n); ++i)
		if ((*priv->txqs)[i] == txq) {
			DEBUG("%p: removing TX queue %p from list",
			      (void *)priv->dev, (void *)txq);
			(*priv->txqs)[i] = NULL;
			break;
		}
	txq_cleanup(txq);
	rte_free(txq);
	priv_unlock(priv);
}

/**
 * DPDK callback for TX in secondary processes.
 *
 * This function configures all queues from primary process information
 * if necessary before reverting to the normal TX burst callback.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx5_tx_burst_secondary_setup(void *dpdk_txq, struct rte_mbuf **pkts,
			      uint16_t pkts_n)
{
	struct txq *txq = dpdk_txq;
	struct priv *priv = mlx5_secondary_data_setup(txq->priv);
	struct priv *primary_priv;
	unsigned int index;

	if (priv == NULL)
		return 0;
	primary_priv =
		mlx5_secondary_data[priv->dev->data->port_id].primary_priv;
	/* Look for queue index in both private structures. */
	for (index = 0; index != priv->txqs_n; ++index)
		if (((*primary_priv->txqs)[index] == txq) ||
		    ((*priv->txqs)[index] == txq))
			break;
	if (index == priv->txqs_n)
		return 0;
	txq = (*priv->txqs)[index];
	return priv->dev->tx_pkt_burst(txq, pkts, pkts_n);
}
