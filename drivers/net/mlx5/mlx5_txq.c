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
#include <unistd.h>
#include <sys/mman.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_common.h>

#include "mlx5_utils.h"
#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"

/**
 * Allocate TX queue elements.
 *
 * @param txq_ctrl
 *   Pointer to TX queue structure.
 */
void
txq_alloc_elts(struct mlx5_txq_ctrl *txq_ctrl)
{
	const unsigned int elts_n = 1 << txq_ctrl->txq.elts_n;
	unsigned int i;

	for (i = 0; (i != elts_n); ++i)
		(*txq_ctrl->txq.elts)[i] = NULL;
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
txq_free_elts(struct mlx5_txq_ctrl *txq_ctrl)
{
	const uint16_t elts_n = 1 << txq_ctrl->txq.elts_n;
	const uint16_t elts_m = elts_n - 1;
	uint16_t elts_head = txq_ctrl->txq.elts_head;
	uint16_t elts_tail = txq_ctrl->txq.elts_tail;
	struct rte_mbuf *(*elts)[elts_n] = txq_ctrl->txq.elts;

	DEBUG("%p: freeing WRs", (void *)txq_ctrl);
	txq_ctrl->txq.elts_head = 0;
	txq_ctrl->txq.elts_tail = 0;
	txq_ctrl->txq.elts_comp = 0;

	while (elts_tail != elts_head) {
		struct rte_mbuf *elt = (*elts)[elts_tail & elts_m];

		assert(elt != NULL);
		rte_pktmbuf_free_seg(elt);
#ifndef NDEBUG
		/* Poisoning. */
		memset(&(*elts)[elts_tail & elts_m],
		       0x77,
		       sizeof((*elts)[elts_tail & elts_m]));
#endif
		++elts_tail;
	}
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
	struct mlx5_txq_data *txq = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	int ret = 0;

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
	if (!mlx5_priv_txq_releasable(priv, idx)) {
		ret = EBUSY;
		ERROR("%p: unable to release queue index %u",
		      (void *)dev, idx);
		goto out;
	}
	mlx5_priv_txq_release(priv, idx);
	txq_ctrl = mlx5_priv_txq_new(priv, idx, desc, socket, conf);
	if (!txq_ctrl) {
		ERROR("%p: unable to allocate queue index %u",
		      (void *)dev, idx);
		ret = ENOMEM;
		goto out;
	}
	DEBUG("%p: adding TX queue %p to list",
	      (void *)dev, (void *)txq_ctrl);
	(*priv->txqs)[idx] = &txq_ctrl->txq;
out:
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
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	struct mlx5_txq_ctrl *txq_ctrl;
	struct priv *priv;
	unsigned int i;

	if (mlx5_is_secondary())
		return;

	if (txq == NULL)
		return;
	txq_ctrl = container_of(txq, struct mlx5_txq_ctrl, txq);
	priv = txq_ctrl->priv;
	priv_lock(priv);
	for (i = 0; (i != priv->txqs_n); ++i)
		if ((*priv->txqs)[i] == txq) {
			DEBUG("%p: removing TX queue %p from list",
			      (void *)priv->dev, (void *)txq_ctrl);
			mlx5_priv_txq_release(priv, i);
			break;
		}
	priv_unlock(priv);
}


/**
 * Map locally UAR used in Tx queues for BlueFlame doorbell.
 *
 * @param[in] priv
 *   Pointer to private structure.
 * @param fd
 *   Verbs file descriptor to map UAR pages.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_tx_uar_remap(struct priv *priv, int fd)
{
	unsigned int i, j;
	uintptr_t pages[priv->txqs_n];
	unsigned int pages_n = 0;
	uintptr_t uar_va;
	void *addr;
	struct mlx5_txq_data *txq;
	struct mlx5_txq_ctrl *txq_ctrl;
	int already_mapped;
	size_t page_size = sysconf(_SC_PAGESIZE);

	memset(pages, 0, priv->txqs_n * sizeof(uintptr_t));
	/*
	 * As rdma-core, UARs are mapped in size of OS page size.
	 * Use aligned address to avoid duplicate mmap.
	 * Ref to libmlx5 function: mlx5_init_context()
	 */
	for (i = 0; i != priv->txqs_n; ++i) {
		txq = (*priv->txqs)[i];
		txq_ctrl = container_of(txq, struct mlx5_txq_ctrl, txq);
		uar_va = (uintptr_t)txq_ctrl->txq.bf_reg;
		uar_va = RTE_ALIGN_FLOOR(uar_va, page_size);
		already_mapped = 0;
		for (j = 0; j != pages_n; ++j) {
			if (pages[j] == uar_va) {
				already_mapped = 1;
				break;
			}
		}
		if (already_mapped)
			continue;
		pages[pages_n++] = uar_va;
		addr = mmap((void *)uar_va, page_size,
			    PROT_WRITE, MAP_FIXED | MAP_SHARED, fd,
			    txq_ctrl->uar_mmap_offset);
		if (addr != (void *)uar_va) {
			ERROR("call to mmap failed on UAR for txq %d\n", i);
			return -1;
		}
	}
	return 0;
}

/**
 * Create the Tx queue Verbs object.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   Queue index in DPDK Rx queue array
 *
 * @return
 *   The Verbs object initialised if it can be created.
 */
struct mlx5_txq_ibv*
mlx5_priv_txq_ibv_new(struct priv *priv, uint16_t idx)
{
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq_data, struct mlx5_txq_ctrl, txq);
	struct mlx5_txq_ibv tmpl;
	struct mlx5_txq_ibv *txq_ibv;
	union {
		struct ibv_qp_init_attr_ex init;
		struct ibv_cq_init_attr_ex cq;
		struct ibv_qp_attr mod;
		struct ibv_cq_ex cq_attr;
	} attr;
	unsigned int cqe_n;
	struct mlx5dv_qp qp = { .comp_mask = MLX5DV_QP_MASK_UAR_MMAP_OFFSET };
	struct mlx5dv_cq cq_info;
	struct mlx5dv_obj obj;
	const int desc = 1 << txq_data->elts_n;
	int ret = 0;

	assert(txq_data);
	if (mlx5_getenv_int("MLX5_ENABLE_CQE_COMPRESSION")) {
		ERROR("MLX5_ENABLE_CQE_COMPRESSION must never be set");
		goto error;
	}
	memset(&tmpl, 0, sizeof(struct mlx5_txq_ibv));
	/* MRs will be registered in mp2mr[] later. */
	attr.cq = (struct ibv_cq_init_attr_ex){
		.comp_mask = 0,
	};
	cqe_n = ((desc / MLX5_TX_COMP_THRESH) - 1) ?
		((desc / MLX5_TX_COMP_THRESH) - 1) : 1;
	if (priv->mps == MLX5_MPW_ENHANCED)
		cqe_n += MLX5_TX_COMP_THRESH_INLINE_DIV;
	tmpl.cq = ibv_create_cq(priv->ctx, cqe_n, NULL, NULL, 0);
	if (tmpl.cq == NULL) {
		ERROR("%p: CQ creation failure", (void *)txq_ctrl);
		goto error;
	}
	attr.init = (struct ibv_qp_init_attr_ex){
		/* CQ to be associated with the send queue. */
		.send_cq = tmpl.cq,
		/* CQ to be associated with the receive queue. */
		.recv_cq = tmpl.cq,
		.cap = {
			/* Max number of outstanding WRs. */
			.max_send_wr =
				((priv->device_attr.orig_attr.max_qp_wr <
				  desc) ?
				 priv->device_attr.orig_attr.max_qp_wr :
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
		/*
		 * Do *NOT* enable this, completions events are managed per
		 * Tx burst.
		 */
		.sq_sig_all = 0,
		.pd = priv->pd,
		.comp_mask = IBV_QP_INIT_ATTR_PD,
	};
	if (txq_data->inline_en)
		attr.init.cap.max_inline_data = txq_ctrl->max_inline_data;
	if (txq_data->tso_en) {
		attr.init.max_tso_header = txq_ctrl->max_tso_header;
		attr.init.comp_mask |= IBV_QP_INIT_ATTR_MAX_TSO_HEADER;
	}
	tmpl.qp = ibv_create_qp_ex(priv->ctx, &attr.init);
	if (tmpl.qp == NULL) {
		ERROR("%p: QP creation failure", (void *)txq_ctrl);
		goto error;
	}
	attr.mod = (struct ibv_qp_attr){
		/* Move the QP to this state. */
		.qp_state = IBV_QPS_INIT,
		/* Primary port number. */
		.port_num = priv->port
	};
	ret = ibv_modify_qp(tmpl.qp, &attr.mod, (IBV_QP_STATE | IBV_QP_PORT));
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_INIT failed", (void *)txq_ctrl);
		goto error;
	}
	attr.mod = (struct ibv_qp_attr){
		.qp_state = IBV_QPS_RTR
	};
	ret = ibv_modify_qp(tmpl.qp, &attr.mod, IBV_QP_STATE);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_RTR failed", (void *)txq_ctrl);
		goto error;
	}
	attr.mod.qp_state = IBV_QPS_RTS;
	ret = ibv_modify_qp(tmpl.qp, &attr.mod, IBV_QP_STATE);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_RTS failed", (void *)txq_ctrl);
		goto error;
	}
	txq_ibv = rte_calloc_socket(__func__, 1, sizeof(struct mlx5_txq_ibv), 0,
				    txq_ctrl->socket);
	if (!txq_ibv) {
		ERROR("%p: cannot allocate memory", (void *)txq_ctrl);
		goto error;
	}
	obj.cq.in = tmpl.cq;
	obj.cq.out = &cq_info;
	obj.qp.in = tmpl.qp;
	obj.qp.out = &qp;
	ret = mlx5dv_init_obj(&obj, MLX5DV_OBJ_CQ | MLX5DV_OBJ_QP);
	if (ret != 0)
		goto error;
	if (cq_info.cqe_size != RTE_CACHE_LINE_SIZE) {
		ERROR("Wrong MLX5_CQE_SIZE environment variable value: "
		      "it should be set to %u", RTE_CACHE_LINE_SIZE);
		goto error;
	}
	txq_data->cqe_n = log2above(cq_info.cqe_cnt);
	txq_data->qp_num_8s = tmpl.qp->qp_num << 8;
	txq_data->wqes = qp.sq.buf;
	txq_data->wqe_n = log2above(qp.sq.wqe_cnt);
	txq_data->qp_db = &qp.dbrec[MLX5_SND_DBR];
	txq_data->bf_reg = qp.bf.reg;
	txq_data->cq_db = cq_info.dbrec;
	txq_data->cqes =
		(volatile struct mlx5_cqe (*)[])
		(uintptr_t)cq_info.buf;
	txq_data->cq_ci = 0;
	txq_data->cq_pi = 0;
	txq_data->wqe_ci = 0;
	txq_data->wqe_pi = 0;
	txq_ibv->qp = tmpl.qp;
	txq_ibv->cq = tmpl.cq;
	rte_atomic32_inc(&txq_ibv->refcnt);
	if (qp.comp_mask & MLX5DV_QP_MASK_UAR_MMAP_OFFSET) {
		txq_ctrl->uar_mmap_offset = qp.uar_mmap_offset;
	} else {
		ERROR("Failed to retrieve UAR info, invalid libmlx5.so version");
		goto error;
	}
	DEBUG("%p: Verbs Tx queue %p: refcnt %d", (void *)priv,
	      (void *)txq_ibv, rte_atomic32_read(&txq_ibv->refcnt));
	LIST_INSERT_HEAD(&priv->txqsibv, txq_ibv, next);
	return txq_ibv;
error:
	if (tmpl.cq)
		claim_zero(ibv_destroy_cq(tmpl.cq));
	if (tmpl.qp)
		claim_zero(ibv_destroy_qp(tmpl.qp));
	return NULL;
}

/**
 * Get an Tx queue Verbs object.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   Queue index in DPDK Rx queue array
 *
 * @return
 *   The Verbs object if it exists.
 */
struct mlx5_txq_ibv*
mlx5_priv_txq_ibv_get(struct priv *priv, uint16_t idx)
{
	struct mlx5_txq_ctrl *txq_ctrl;

	if (idx >= priv->txqs_n)
		return NULL;
	if (!(*priv->txqs)[idx])
		return NULL;
	txq_ctrl = container_of((*priv->txqs)[idx], struct mlx5_txq_ctrl, txq);
	if (txq_ctrl->ibv) {
		rte_atomic32_inc(&txq_ctrl->ibv->refcnt);
		DEBUG("%p: Verbs Tx queue %p: refcnt %d", (void *)priv,
		      (void *)txq_ctrl->ibv,
		      rte_atomic32_read(&txq_ctrl->ibv->refcnt));
	}
	return txq_ctrl->ibv;
}

/**
 * Release an Tx verbs queue object.
 *
 * @param priv
 *   Pointer to private structure.
 * @param txq_ibv
 *   Verbs Tx queue object.
 *
 * @return
 *   0 on success, errno on failure.
 */
int
mlx5_priv_txq_ibv_release(struct priv *priv, struct mlx5_txq_ibv *txq_ibv)
{
	(void)priv;
	assert(txq_ibv);
	DEBUG("%p: Verbs Tx queue %p: refcnt %d", (void *)priv,
	      (void *)txq_ibv, rte_atomic32_read(&txq_ibv->refcnt));
	if (rte_atomic32_dec_and_test(&txq_ibv->refcnt)) {
		claim_zero(ibv_destroy_qp(txq_ibv->qp));
		claim_zero(ibv_destroy_cq(txq_ibv->cq));
		LIST_REMOVE(txq_ibv, next);
		rte_free(txq_ibv);
		return 0;
	}
	return EBUSY;
}

/**
 * Return true if a single reference exists on the object.
 *
 * @param priv
 *   Pointer to private structure.
 * @param txq_ibv
 *   Verbs Tx queue object.
 */
int
mlx5_priv_txq_ibv_releasable(struct priv *priv, struct mlx5_txq_ibv *txq_ibv)
{
	(void)priv;
	assert(txq_ibv);
	return (rte_atomic32_read(&txq_ibv->refcnt) == 1);
}

/**
 * Verify the Verbs Tx queue list is empty
 *
 * @param priv
 *  Pointer to private structure.
 *
 * @return the number of object not released.
 */
int
mlx5_priv_txq_ibv_verify(struct priv *priv)
{
	int ret = 0;
	struct mlx5_txq_ibv *txq_ibv;

	LIST_FOREACH(txq_ibv, &priv->txqsibv, next) {
		DEBUG("%p: Verbs Tx queue %p still referenced", (void *)priv,
		      (void *)txq_ibv);
		++ret;
	}
	return ret;
}

/**
 * Create a DPDK Tx queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   TX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *  Thresholds parameters.
 *
 * @return
 *   A DPDK queue object on success.
 */
struct mlx5_txq_ctrl*
mlx5_priv_txq_new(struct priv *priv, uint16_t idx, uint16_t desc,
		  unsigned int socket,
		  const struct rte_eth_txconf *conf)
{
	const unsigned int max_tso_inline =
		((MLX5_MAX_TSO_HEADER + (RTE_CACHE_LINE_SIZE - 1)) /
		 RTE_CACHE_LINE_SIZE);
	struct mlx5_txq_ctrl *tmpl;

	tmpl = rte_calloc_socket("TXQ", 1,
				 sizeof(*tmpl) +
				 desc * sizeof(struct rte_mbuf *),
				 0, socket);
	if (!tmpl)
		return NULL;
	assert(desc > MLX5_TX_COMP_THRESH);
	tmpl->txq.flags = conf->txq_flags;
	tmpl->priv = priv;
	tmpl->socket = socket;
	tmpl->txq.elts_n = log2above(desc);
	if (priv->mps == MLX5_MPW_ENHANCED)
		tmpl->txq.mpw_hdr_dseg = priv->mpw_hdr_dseg;
	/* MRs will be registered in mp2mr[] later. */
	DEBUG("priv->device_attr.max_qp_wr is %d",
	      priv->device_attr.orig_attr.max_qp_wr);
	DEBUG("priv->device_attr.max_sge is %d",
	      priv->device_attr.orig_attr.max_sge);
	if (priv->txq_inline && (priv->txqs_n >= priv->txqs_inline)) {
		unsigned int ds_cnt;

		tmpl->txq.max_inline =
			((priv->txq_inline + (RTE_CACHE_LINE_SIZE - 1)) /
			 RTE_CACHE_LINE_SIZE);
		tmpl->txq.inline_en = 1;
		/* TSO and MPS can't be enabled concurrently. */
		assert(!priv->tso || !priv->mps);
		if (priv->mps == MLX5_MPW_ENHANCED) {
			tmpl->txq.inline_max_packet_sz =
				priv->inline_max_packet_sz;
			/* To minimize the size of data set, avoid requesting
			 * too large WQ.
			 */
			tmpl->max_inline_data =
				((RTE_MIN(priv->txq_inline,
					  priv->inline_max_packet_sz) +
				  (RTE_CACHE_LINE_SIZE - 1)) /
				 RTE_CACHE_LINE_SIZE) * RTE_CACHE_LINE_SIZE;
		} else if (priv->tso) {
			int inline_diff = tmpl->txq.max_inline - max_tso_inline;

			/*
			 * Adjust inline value as Verbs aggregates
			 * tso_inline and txq_inline fields.
			 */
			tmpl->max_inline_data = inline_diff > 0 ?
					       inline_diff *
					       RTE_CACHE_LINE_SIZE :
					       0;
		} else {
			tmpl->max_inline_data =
				tmpl->txq.max_inline * RTE_CACHE_LINE_SIZE;
		}
		/*
		 * Check if the inline size is too large in a way which
		 * can make the WQE DS to overflow.
		 * Considering in calculation:
		 *      WQE CTRL (1 DS)
		 *      WQE ETH  (1 DS)
		 *      Inline part (N DS)
		 */
		ds_cnt = 2 + (tmpl->txq.max_inline / MLX5_WQE_DWORD_SIZE);
		if (ds_cnt > MLX5_DSEG_MAX) {
			unsigned int max_inline = (MLX5_DSEG_MAX - 2) *
						  MLX5_WQE_DWORD_SIZE;

			max_inline = max_inline - (max_inline %
						   RTE_CACHE_LINE_SIZE);
			WARN("txq inline is too large (%d) setting it to "
			     "the maximum possible: %d\n",
			     priv->txq_inline, max_inline);
			tmpl->txq.max_inline = max_inline / RTE_CACHE_LINE_SIZE;
		}
	}
	if (priv->tso) {
		tmpl->max_tso_header = max_tso_inline * RTE_CACHE_LINE_SIZE;
		tmpl->txq.max_inline = RTE_MAX(tmpl->txq.max_inline,
					       max_tso_inline);
		tmpl->txq.tso_en = 1;
	}
	if (priv->tunnel_en)
		tmpl->txq.tunnel_en = 1;
	tmpl->txq.elts =
		(struct rte_mbuf *(*)[1 << tmpl->txq.elts_n])(tmpl + 1);
	tmpl->txq.stats.idx = idx;
	rte_atomic32_inc(&tmpl->refcnt);
	DEBUG("%p: Tx queue %p: refcnt %d", (void *)priv,
	      (void *)tmpl, rte_atomic32_read(&tmpl->refcnt));
	LIST_INSERT_HEAD(&priv->txqsctrl, tmpl, next);
	return tmpl;
}

/**
 * Get a Tx queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   TX queue index.
 *
 * @return
 *   A pointer to the queue if it exists.
 */
struct mlx5_txq_ctrl*
mlx5_priv_txq_get(struct priv *priv, uint16_t idx)
{
	struct mlx5_txq_ctrl *ctrl = NULL;

	if ((*priv->txqs)[idx]) {
		ctrl = container_of((*priv->txqs)[idx], struct mlx5_txq_ctrl,
				    txq);
		unsigned int i;

		mlx5_priv_txq_ibv_get(priv, idx);
		for (i = 0; i != MLX5_PMD_TX_MP_CACHE; ++i) {
			struct mlx5_mr *mr = NULL;

			(void)mr;
			if (ctrl->txq.mp2mr[i]) {
				mr = priv_mr_get(priv, ctrl->txq.mp2mr[i]->mp);
				assert(mr);
			}
		}
		rte_atomic32_inc(&ctrl->refcnt);
		DEBUG("%p: Tx queue %p: refcnt %d", (void *)priv,
		      (void *)ctrl, rte_atomic32_read(&ctrl->refcnt));
	}
	return ctrl;
}

/**
 * Release a Tx queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   TX queue index.
 *
 * @return
 *   0 on success, errno on failure.
 */
int
mlx5_priv_txq_release(struct priv *priv, uint16_t idx)
{
	unsigned int i;
	struct mlx5_txq_ctrl *txq;

	if (!(*priv->txqs)[idx])
		return 0;
	txq = container_of((*priv->txqs)[idx], struct mlx5_txq_ctrl, txq);
	DEBUG("%p: Tx queue %p: refcnt %d", (void *)priv,
	      (void *)txq, rte_atomic32_read(&txq->refcnt));
	if (txq->ibv) {
		int ret;

		ret = mlx5_priv_txq_ibv_release(priv, txq->ibv);
		if (!ret)
			txq->ibv = NULL;
	}
	for (i = 0; i != MLX5_PMD_TX_MP_CACHE; ++i) {
		if (txq->txq.mp2mr[i]) {
			priv_mr_release(priv, txq->txq.mp2mr[i]);
			txq->txq.mp2mr[i] = NULL;
		}
	}
	if (rte_atomic32_dec_and_test(&txq->refcnt)) {
		txq_free_elts(txq);
		LIST_REMOVE(txq, next);
		rte_free(txq);
		(*priv->txqs)[idx] = NULL;
		return 0;
	}
	return EBUSY;
}

/**
 * Verify if the queue can be released.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   TX queue index.
 *
 * @return
 *   1 if the queue can be released.
 */
int
mlx5_priv_txq_releasable(struct priv *priv, uint16_t idx)
{
	struct mlx5_txq_ctrl *txq;

	if (!(*priv->txqs)[idx])
		return -1;
	txq = container_of((*priv->txqs)[idx], struct mlx5_txq_ctrl, txq);
	return (rte_atomic32_read(&txq->refcnt) == 1);
}

/**
 * Verify the Tx Queue list is empty
 *
 * @param priv
 *  Pointer to private structure.
 *
 * @return the number of object not released.
 */
int
mlx5_priv_txq_verify(struct priv *priv)
{
	struct mlx5_txq_ctrl *txq;
	int ret = 0;

	LIST_FOREACH(txq, &priv->txqsctrl, next) {
		DEBUG("%p: Tx Queue %p still referenced", (void *)priv,
		      (void *)txq);
		++ret;
	}
	return ret;
}
