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
#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"

/**
 * Allocate TX queue elements.
 *
 * @param txq_ctrl
 *   Pointer to TX queue structure.
 * @param elts_n
 *   Number of elements to allocate.
 */
static void
txq_alloc_elts(struct txq_ctrl *txq_ctrl, unsigned int elts_n)
{
	unsigned int i;

	for (i = 0; (i != elts_n); ++i)
		(*txq_ctrl->txq.elts)[i] = NULL;
	for (i = 0; (i != txq_ctrl->txq.wqe_n); ++i) {
		volatile union mlx5_wqe *wqe = &(*txq_ctrl->txq.wqes)[i];

		memset((void *)(uintptr_t)wqe, 0x0, sizeof(*wqe));
	}
	DEBUG("%p: allocated and configured %u WRs", (void *)txq_ctrl, elts_n);
	txq_ctrl->txq.elts_head = 0;
	txq_ctrl->txq.elts_tail = 0;
	txq_ctrl->txq.elts_comp = 0;
}

/**
 * Free TX queue elements.
 *
 * @param txq_ctrl
 *   Pointer to TX queue structure.
 */
static void
txq_free_elts(struct txq_ctrl *txq_ctrl)
{
	unsigned int elts_n = txq_ctrl->txq.elts_n;
	unsigned int elts_head = txq_ctrl->txq.elts_head;
	unsigned int elts_tail = txq_ctrl->txq.elts_tail;
	struct rte_mbuf *(*elts)[elts_n] = txq_ctrl->txq.elts;

	DEBUG("%p: freeing WRs", (void *)txq_ctrl);
	txq_ctrl->txq.elts_head = 0;
	txq_ctrl->txq.elts_tail = 0;
	txq_ctrl->txq.elts_comp = 0;

	while (elts_tail != elts_head) {
		struct rte_mbuf *elt = (*elts)[elts_tail];

		assert(elt != NULL);
		rte_pktmbuf_free(elt);
#ifndef NDEBUG
		/* Poisoning. */
		memset(&(*elts)[elts_tail],
		       0x77,
		       sizeof((*elts)[elts_tail]));
#endif
		if (++elts_tail == elts_n)
			elts_tail = 0;
	}
}

/**
 * Clean up a TX queue.
 *
 * Destroy objects, free allocated memory and reset the structure for reuse.
 *
 * @param txq_ctrl
 *   Pointer to TX queue structure.
 */
void
txq_cleanup(struct txq_ctrl *txq_ctrl)
{
	struct ibv_exp_release_intf_params params;
	size_t i;

	DEBUG("cleaning up %p", (void *)txq_ctrl);
	txq_free_elts(txq_ctrl);
	if (txq_ctrl->if_qp != NULL) {
		assert(txq_ctrl->priv != NULL);
		assert(txq_ctrl->priv->ctx != NULL);
		assert(txq_ctrl->qp != NULL);
		params = (struct ibv_exp_release_intf_params){
			.comp_mask = 0,
		};
		claim_zero(ibv_exp_release_intf(txq_ctrl->priv->ctx,
						txq_ctrl->if_qp,
						&params));
	}
	if (txq_ctrl->if_cq != NULL) {
		assert(txq_ctrl->priv != NULL);
		assert(txq_ctrl->priv->ctx != NULL);
		assert(txq_ctrl->cq != NULL);
		params = (struct ibv_exp_release_intf_params){
			.comp_mask = 0,
		};
		claim_zero(ibv_exp_release_intf(txq_ctrl->priv->ctx,
						txq_ctrl->if_cq,
						&params));
	}
	if (txq_ctrl->qp != NULL)
		claim_zero(ibv_destroy_qp(txq_ctrl->qp));
	if (txq_ctrl->cq != NULL)
		claim_zero(ibv_destroy_cq(txq_ctrl->cq));
	if (txq_ctrl->rd != NULL) {
		struct ibv_exp_destroy_res_domain_attr attr = {
			.comp_mask = 0,
		};

		assert(txq_ctrl->priv != NULL);
		assert(txq_ctrl->priv->ctx != NULL);
		claim_zero(ibv_exp_destroy_res_domain(txq_ctrl->priv->ctx,
						      txq_ctrl->rd,
						      &attr));
	}
	for (i = 0; (i != RTE_DIM(txq_ctrl->txq.mp2mr)); ++i) {
		if (txq_ctrl->txq.mp2mr[i].mp == NULL)
			break;
		assert(txq_ctrl->txq.mp2mr[i].mr != NULL);
		claim_zero(ibv_dereg_mr(txq_ctrl->txq.mp2mr[i].mr));
	}
	memset(txq_ctrl, 0, sizeof(*txq_ctrl));
}

/**
 * Initialize TX queue.
 *
 * @param tmpl
 *   Pointer to TX queue control template.
 * @param txq_ctrl
 *   Pointer to TX queue control.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static inline int
txq_setup(struct txq_ctrl *tmpl, struct txq_ctrl *txq_ctrl)
{
	struct mlx5_qp *qp = to_mqp(tmpl->qp);
	struct ibv_cq *ibcq = tmpl->cq;
	struct mlx5_cq *cq = to_mxxx(cq, cq);

	if (cq->cqe_sz != RTE_CACHE_LINE_SIZE) {
		ERROR("Wrong MLX5_CQE_SIZE environment variable value: "
		      "it should be set to %u", RTE_CACHE_LINE_SIZE);
		return EINVAL;
	}
	tmpl->txq.cqe_n = ibcq->cqe + 1;
	tmpl->txq.qp_num_8s = qp->ctrl_seg.qp_num << 8;
	tmpl->txq.wqes =
		(volatile union mlx5_wqe (*)[])
		(uintptr_t)qp->gen_data.sqstart;
	tmpl->txq.wqe_n = qp->sq.wqe_cnt;
	tmpl->txq.qp_db = &qp->gen_data.db[MLX5_SND_DBR];
	tmpl->txq.bf_reg = qp->gen_data.bf->reg;
	tmpl->txq.bf_offset = qp->gen_data.bf->offset;
	tmpl->txq.bf_buf_size = qp->gen_data.bf->buf_size;
	tmpl->txq.cq_db = cq->dbrec;
	tmpl->txq.cqes =
		(volatile struct mlx5_cqe (*)[])
		(uintptr_t)cq->active_buf->buf;
	tmpl->txq.elts =
		(struct rte_mbuf *(*)[tmpl->txq.elts_n])
		((uintptr_t)txq_ctrl + sizeof(*txq_ctrl));
	return 0;
}

/**
 * Configure a TX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param txq_ctrl
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
txq_ctrl_setup(struct rte_eth_dev *dev, struct txq_ctrl *txq_ctrl,
	       uint16_t desc, unsigned int socket,
	       const struct rte_eth_txconf *conf)
{
	struct priv *priv = mlx5_get_priv(dev);
	struct txq_ctrl tmpl = {
		.priv = priv,
		.socket = socket,
	};
	union {
		struct ibv_exp_query_intf_params params;
		struct ibv_exp_qp_init_attr init;
		struct ibv_exp_res_domain_init_attr rd;
		struct ibv_exp_cq_init_attr cq;
		struct ibv_exp_qp_attr mod;
		struct ibv_exp_cq_attr cq_attr;
	} attr;
	enum ibv_exp_query_intf_status status;
	int ret = 0;

	if (mlx5_getenv_int("MLX5_ENABLE_CQE_COMPRESSION")) {
		ret = ENOTSUP;
		ERROR("MLX5_ENABLE_CQE_COMPRESSION must never be set");
		goto error;
	}
	(void)conf; /* Thresholds configuration (ignored). */
	assert(desc > MLX5_TX_COMP_THRESH);
	tmpl.txq.elts_n = desc;
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
	tmpl.cq = ibv_exp_create_cq(priv->ctx,
				    (((desc / MLX5_TX_COMP_THRESH) - 1) ?
				     ((desc / MLX5_TX_COMP_THRESH) - 1) : 1),
				    NULL, NULL, 0, &attr.cq);
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
			/*
			 * Max number of scatter/gather elements in a WR,
			 * must be 1 to prevent libmlx5 from trying to affect
			 * too much memory. TX gather is not impacted by the
			 * priv->device_attr.max_sge limit and will still work
			 * properly.
			 */
			.max_send_sge = 1,
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
	if (priv->txq_inline && priv->txqs_n >= priv->txqs_inline) {
		tmpl.txq.max_inline = priv->txq_inline;
		attr.init.cap.max_inline_data = tmpl.txq.max_inline;
	}
	tmpl.qp = ibv_exp_create_qp(priv->ctx, &attr.init);
	if (tmpl.qp == NULL) {
		ret = (errno ? errno : EINVAL);
		ERROR("%p: QP creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	DEBUG("TX queue capabilities: max_send_wr=%u, max_send_sge=%u,"
	      " max_inline_data=%u",
	      attr.init.cap.max_send_wr,
	      attr.init.cap.max_send_sge,
	      attr.init.cap.max_inline_data);
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
	ret = txq_setup(&tmpl, txq_ctrl);
	if (ret) {
		ERROR("%p: cannot initialize TX queue structure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	txq_alloc_elts(&tmpl, desc);
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
		.intf_version = 1,
		.obj = tmpl.qp,
		/* Enable multi-packet send if supported. */
		.family_flags =
			((priv->mps && !priv->sriov) ?
			 IBV_EXP_QP_BURST_CREATE_ENABLE_MULTI_PACKET_SEND_WR :
			 0),
	};
	tmpl.if_qp = ibv_exp_query_intf(priv->ctx, &attr.params, &status);
	if (tmpl.if_qp == NULL) {
		ret = EINVAL;
		ERROR("%p: QP interface family query failed with status %d",
		      (void *)dev, status);
		goto error;
	}
	/* Clean up txq in case we're reinitializing it. */
	DEBUG("%p: cleaning-up old txq just in case", (void *)txq_ctrl);
	txq_cleanup(txq_ctrl);
	*txq_ctrl = tmpl;
	DEBUG("%p: txq updated with %p", (void *)txq_ctrl, (void *)&tmpl);
	/* Pre-register known mempools. */
	rte_mempool_walk(txq_mp2mr_iter, txq_ctrl);
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
	struct txq_ctrl *txq_ctrl = container_of(txq, struct txq_ctrl, txq);
	int ret;

	if (mlx5_is_secondary())
		return -E_RTE_SECONDARY;

	priv_lock(priv);
	if (desc <= MLX5_TX_COMP_THRESH) {
		WARN("%p: number of descriptors requested for TX queue %u"
		     " must be higher than MLX5_TX_COMP_THRESH, using"
		     " %u instead of %u",
		     (void *)dev, idx, MLX5_TX_COMP_THRESH + 1, desc);
		desc = MLX5_TX_COMP_THRESH + 1;
	}
	if (!rte_is_power_of_2(desc)) {
		desc = 1 << log2above(desc);
		WARN("%p: increased number of descriptors in TX queue %u"
		     " to the next power of two (%d)",
		     (void *)dev, idx, desc);
	}
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
		txq_cleanup(txq_ctrl);
	} else {
		txq_ctrl =
			rte_calloc_socket("TXQ", 1,
					  sizeof(*txq_ctrl) +
					  desc * sizeof(struct rte_mbuf *),
					  0, socket);
		if (txq_ctrl == NULL) {
			ERROR("%p: unable to allocate queue index %u",
			      (void *)dev, idx);
			priv_unlock(priv);
			return -ENOMEM;
		}
	}
	ret = txq_ctrl_setup(dev, txq_ctrl, desc, socket, conf);
	if (ret)
		rte_free(txq_ctrl);
	else {
		txq_ctrl->txq.stats.idx = idx;
		DEBUG("%p: adding TX queue %p to list",
		      (void *)dev, (void *)txq_ctrl);
		(*priv->txqs)[idx] = &txq_ctrl->txq;
		/* Update send callback. */
		priv_select_tx_function(priv);
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
	struct txq_ctrl *txq_ctrl;
	struct priv *priv;
	unsigned int i;

	if (mlx5_is_secondary())
		return;

	if (txq == NULL)
		return;
	txq_ctrl = container_of(txq, struct txq_ctrl, txq);
	priv = txq_ctrl->priv;
	priv_lock(priv);
	for (i = 0; (i != priv->txqs_n); ++i)
		if ((*priv->txqs)[i] == txq) {
			DEBUG("%p: removing TX queue %p from list",
			      (void *)priv->dev, (void *)txq_ctrl);
			(*priv->txqs)[i] = NULL;
			break;
		}
	txq_cleanup(txq_ctrl);
	rte_free(txq_ctrl);
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
	struct txq_ctrl *txq_ctrl = container_of(txq, struct txq_ctrl, txq);
	struct priv *priv = mlx5_secondary_data_setup(txq_ctrl->priv);
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
