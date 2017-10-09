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
 * @param elts_n
 *   Number of elements to allocate.
 */
static void
txq_alloc_elts(struct mlx5_txq_ctrl *txq_ctrl, unsigned int elts_n)
{
	unsigned int i;

	for (i = 0; (i != elts_n); ++i)
		(*txq_ctrl->txq.elts)[i] = NULL;
	for (i = 0; (i != (1u << txq_ctrl->txq.wqe_n)); ++i) {
		volatile struct mlx5_wqe64 *wqe =
			(volatile struct mlx5_wqe64 *)
			txq_ctrl->txq.wqes + i;

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
 * Clean up a TX queue.
 *
 * Destroy objects, free allocated memory and reset the structure for reuse.
 *
 * @param txq_ctrl
 *   Pointer to TX queue structure.
 */
void
mlx5_txq_cleanup(struct mlx5_txq_ctrl *txq_ctrl)
{
	size_t i;

	DEBUG("cleaning up %p", (void *)txq_ctrl);
	txq_free_elts(txq_ctrl);
	if (txq_ctrl->qp != NULL)
		claim_zero(ibv_destroy_qp(txq_ctrl->qp));
	if (txq_ctrl->cq != NULL)
		claim_zero(ibv_destroy_cq(txq_ctrl->cq));
	for (i = 0; (i != RTE_DIM(txq_ctrl->txq.mp2mr)); ++i) {
		if (txq_ctrl->txq.mp2mr[i].mr == NULL)
			break;
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
txq_setup(struct mlx5_txq_ctrl *tmpl, struct mlx5_txq_ctrl *txq_ctrl)
{
	struct mlx5dv_qp qp;
	struct ibv_cq *ibcq = tmpl->cq;
	struct mlx5dv_cq cq_info;
	struct mlx5dv_obj obj;
	int ret = 0;

	qp.comp_mask = MLX5DV_QP_MASK_UAR_MMAP_OFFSET;
	obj.cq.in = ibcq;
	obj.cq.out = &cq_info;
	obj.qp.in = tmpl->qp;
	obj.qp.out = &qp;
	ret = mlx5dv_init_obj(&obj, MLX5DV_OBJ_CQ | MLX5DV_OBJ_QP);
	if (ret != 0) {
		return -EINVAL;
	}
	if (cq_info.cqe_size != RTE_CACHE_LINE_SIZE) {
		ERROR("Wrong MLX5_CQE_SIZE environment variable value: "
		      "it should be set to %u", RTE_CACHE_LINE_SIZE);
		return EINVAL;
	}
	tmpl->txq.cqe_n = log2above(cq_info.cqe_cnt);
	tmpl->txq.qp_num_8s = tmpl->qp->qp_num << 8;
	tmpl->txq.wqes = qp.sq.buf;
	tmpl->txq.wqe_n = log2above(qp.sq.wqe_cnt);
	tmpl->txq.qp_db = &qp.dbrec[MLX5_SND_DBR];
	tmpl->txq.bf_reg = qp.bf.reg;
	tmpl->txq.cq_db = cq_info.dbrec;
	tmpl->txq.cqes =
		(volatile struct mlx5_cqe (*)[])
		(uintptr_t)cq_info.buf;
	tmpl->txq.elts =
		(struct rte_mbuf *(*)[1 << tmpl->txq.elts_n])
		((uintptr_t)txq_ctrl + sizeof(*txq_ctrl));
	if (qp.comp_mask | MLX5DV_QP_MASK_UAR_MMAP_OFFSET) {
		tmpl->uar_mmap_offset = qp.uar_mmap_offset;
	} else {
		ERROR("Failed to retrieve UAR info, invalid libmlx5.so version");
		return EINVAL;
	}

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
mlx5_txq_ctrl_setup(struct rte_eth_dev *dev, struct mlx5_txq_ctrl *txq_ctrl,
		    uint16_t desc, unsigned int socket,
		    const struct rte_eth_txconf *conf)
{
	struct priv *priv = mlx5_get_priv(dev);
	struct mlx5_txq_ctrl tmpl = {
		.priv = priv,
		.socket = socket,
	};
	union {
		struct ibv_qp_init_attr_ex init;
		struct ibv_cq_init_attr_ex cq;
		struct ibv_qp_attr mod;
		struct ibv_cq_ex cq_attr;
	} attr;
	unsigned int cqe_n;
	const unsigned int max_tso_inline = ((MLX5_MAX_TSO_HEADER +
					     (RTE_CACHE_LINE_SIZE - 1)) /
					      RTE_CACHE_LINE_SIZE);
	int ret = 0;

	if (mlx5_getenv_int("MLX5_ENABLE_CQE_COMPRESSION")) {
		ret = ENOTSUP;
		ERROR("MLX5_ENABLE_CQE_COMPRESSION must never be set");
		goto error;
	}
	tmpl.txq.flags = conf->txq_flags;
	assert(desc > MLX5_TX_COMP_THRESH);
	tmpl.txq.elts_n = log2above(desc);
	if (priv->mps == MLX5_MPW_ENHANCED)
		tmpl.txq.mpw_hdr_dseg = priv->mpw_hdr_dseg;
	/* MRs will be registered in mp2mr[] later. */
	attr.cq = (struct ibv_cq_init_attr_ex){
		.comp_mask = 0,
	};
	cqe_n = ((desc / MLX5_TX_COMP_THRESH) - 1) ?
		((desc / MLX5_TX_COMP_THRESH) - 1) : 1;
	if (priv->mps == MLX5_MPW_ENHANCED)
		cqe_n += MLX5_TX_COMP_THRESH_INLINE_DIV;
	tmpl.cq = ibv_create_cq(priv->ctx,
				cqe_n,
				NULL, NULL, 0);
	if (tmpl.cq == NULL) {
		ret = ENOMEM;
		ERROR("%p: CQ creation failure: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	DEBUG("priv->device_attr.max_qp_wr is %d",
	      priv->device_attr.orig_attr.max_qp_wr);
	DEBUG("priv->device_attr.max_sge is %d",
	      priv->device_attr.orig_attr.max_sge);
	attr.init = (struct ibv_qp_init_attr_ex){
		/* CQ to be associated with the send queue. */
		.send_cq = tmpl.cq,
		/* CQ to be associated with the receive queue. */
		.recv_cq = tmpl.cq,
		.cap = {
			/* Max number of outstanding WRs. */
			.max_send_wr =
			 ((priv->device_attr.orig_attr.max_qp_wr < desc) ?
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
		/* Do *NOT* enable this, completions events are managed per
		 * TX burst. */
		.sq_sig_all = 0,
		.pd = priv->pd,
		.comp_mask = IBV_QP_INIT_ATTR_PD,
	};
	if (priv->txq_inline && (priv->txqs_n >= priv->txqs_inline)) {
		unsigned int ds_cnt;

		tmpl.txq.max_inline =
			((priv->txq_inline + (RTE_CACHE_LINE_SIZE - 1)) /
			 RTE_CACHE_LINE_SIZE);
		tmpl.txq.inline_en = 1;
		/* TSO and MPS can't be enabled concurrently. */
		assert(!priv->tso || !priv->mps);
		if (priv->mps == MLX5_MPW_ENHANCED) {
			tmpl.txq.inline_max_packet_sz =
				priv->inline_max_packet_sz;
			/* To minimize the size of data set, avoid requesting
			 * too large WQ.
			 */
			attr.init.cap.max_inline_data =
				((RTE_MIN(priv->txq_inline,
					  priv->inline_max_packet_sz) +
				  (RTE_CACHE_LINE_SIZE - 1)) /
				 RTE_CACHE_LINE_SIZE) * RTE_CACHE_LINE_SIZE;
		} else if (priv->tso) {
			int inline_diff = tmpl.txq.max_inline - max_tso_inline;

			/*
			 * Adjust inline value as Verbs aggregates
			 * tso_inline and txq_inline fields.
			 */
			attr.init.cap.max_inline_data = inline_diff > 0 ?
							inline_diff *
							RTE_CACHE_LINE_SIZE :
							0;
		} else {
			attr.init.cap.max_inline_data =
				tmpl.txq.max_inline * RTE_CACHE_LINE_SIZE;
		}
		/*
		 * Check if the inline size is too large in a way which
		 * can make the WQE DS to overflow.
		 * Considering in calculation:
		 *	WQE CTRL (1 DS)
		 *	WQE ETH  (1 DS)
		 *	Inline part (N DS)
		 */
		ds_cnt = 2 +
			(attr.init.cap.max_inline_data / MLX5_WQE_DWORD_SIZE);
		if (ds_cnt > MLX5_DSEG_MAX) {
			unsigned int max_inline = (MLX5_DSEG_MAX - 2) *
						   MLX5_WQE_DWORD_SIZE;

			max_inline = max_inline - (max_inline %
						   RTE_CACHE_LINE_SIZE);
			WARN("txq inline is too large (%d) setting it to "
			     "the maximum possible: %d\n",
			     priv->txq_inline, max_inline);
			tmpl.txq.max_inline = max_inline / RTE_CACHE_LINE_SIZE;
			attr.init.cap.max_inline_data = max_inline;
		}
	}
	if (priv->tso) {
		attr.init.max_tso_header =
			max_tso_inline * RTE_CACHE_LINE_SIZE;
		attr.init.comp_mask |= IBV_QP_INIT_ATTR_MAX_TSO_HEADER;
		tmpl.txq.max_inline = RTE_MAX(tmpl.txq.max_inline,
					      max_tso_inline);
		tmpl.txq.tso_en = 1;
	}
	if (priv->tunnel_en)
		tmpl.txq.tunnel_en = 1;
	tmpl.qp = ibv_create_qp_ex(priv->ctx, &attr.init);
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
	attr.mod = (struct ibv_qp_attr){
		/* Move the QP to this state. */
		.qp_state = IBV_QPS_INIT,
		/* Primary port number. */
		.port_num = priv->port
	};
	ret = ibv_modify_qp(tmpl.qp, &attr.mod,
			    (IBV_QP_STATE | IBV_QP_PORT));
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
	attr.mod = (struct ibv_qp_attr){
		.qp_state = IBV_QPS_RTR
	};
	ret = ibv_modify_qp(tmpl.qp, &attr.mod, IBV_QP_STATE);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_RTR failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	attr.mod.qp_state = IBV_QPS_RTS;
	ret = ibv_modify_qp(tmpl.qp, &attr.mod, IBV_QP_STATE);
	if (ret) {
		ERROR("%p: QP state to IBV_QPS_RTS failed: %s",
		      (void *)dev, strerror(ret));
		goto error;
	}
	/* Clean up txq in case we're reinitializing it. */
	DEBUG("%p: cleaning-up old txq just in case", (void *)txq_ctrl);
	mlx5_txq_cleanup(txq_ctrl);
	*txq_ctrl = tmpl;
	DEBUG("%p: txq updated with %p", (void *)txq_ctrl, (void *)&tmpl);
	/* Pre-register known mempools. */
	rte_mempool_walk(mlx5_txq_mp2mr_iter, txq_ctrl);
	assert(ret == 0);
	return 0;
error:
	mlx5_txq_cleanup(&tmpl);
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
	struct mlx5_txq_data *txq = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
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
		if (dev->data->dev_started) {
			priv_unlock(priv);
			return -EEXIST;
		}
		(*priv->txqs)[idx] = NULL;
		mlx5_txq_cleanup(txq_ctrl);
		/* Resize if txq size is changed. */
		if (txq_ctrl->txq.elts_n != log2above(desc)) {
			txq_ctrl = rte_realloc(txq_ctrl,
					       sizeof(*txq_ctrl) +
					       desc * sizeof(struct rte_mbuf *),
					       RTE_CACHE_LINE_SIZE);
			if (!txq_ctrl) {
				ERROR("%p: unable to reallocate queue index %u",
					(void *)dev, idx);
				priv_unlock(priv);
				return -ENOMEM;
			}
		}
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
	ret = mlx5_txq_ctrl_setup(dev, txq_ctrl, desc, socket, conf);
	if (ret)
		rte_free(txq_ctrl);
	else {
		txq_ctrl->txq.stats.idx = idx;
		DEBUG("%p: adding TX queue %p to list",
		      (void *)dev, (void *)txq_ctrl);
		(*priv->txqs)[idx] = &txq_ctrl->txq;
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
			(*priv->txqs)[i] = NULL;
			break;
		}
	mlx5_txq_cleanup(txq_ctrl);
	rte_free(txq_ctrl);
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
