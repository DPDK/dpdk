/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_bus_pci.h>
#include <rte_spinlock.h>
#include <rte_comp.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common_os.h>
#include <mlx5_common_devx.h>
#include <mlx5_common_mr.h>
#include <mlx5_prm.h>

#include "mlx5_compress_utils.h"

#define MLX5_COMPRESS_DRIVER_NAME mlx5_compress
#define MLX5_COMPRESS_MAX_QPS 1024
#define MLX5_COMP_MAX_WIN_SIZE_CONF 6u

struct mlx5_compress_xform {
	LIST_ENTRY(mlx5_compress_xform) next;
	enum rte_comp_xform_type type;
	enum rte_comp_checksum_type csum_type;
	uint32_t opcode;
	uint32_t gga_ctrl1; /* BE. */
};

struct mlx5_compress_priv {
	TAILQ_ENTRY(mlx5_compress_priv) next;
	struct ibv_context *ctx; /* Device context. */
	struct rte_compressdev *cdev;
	void *uar;
	uint32_t pdn; /* Protection Domain number. */
	uint8_t min_block_size;
	uint8_t sq_ts_format; /* Whether SQ supports timestamp formats. */
	/* Minimum huffman block size supported by the device. */
	struct ibv_pd *pd;
	struct rte_compressdev_config dev_config;
	LIST_HEAD(xform_list, mlx5_compress_xform) xform_list;
	rte_spinlock_t xform_sl;
	struct mlx5_mr_share_cache mr_scache; /* Global shared MR cache. */
	volatile uint64_t *uar_addr;
#ifndef RTE_ARCH_64
	rte_spinlock_t uar32_sl;
#endif /* RTE_ARCH_64 */
};

struct mlx5_compress_qp {
	uint16_t qp_id;
	uint16_t entries_n;
	uint16_t pi;
	uint16_t ci;
	struct mlx5_mr_ctrl mr_ctrl;
	int socket_id;
	struct mlx5_devx_cq cq;
	struct mlx5_devx_sq sq;
	struct mlx5_pmd_mr opaque_mr;
	struct rte_comp_op **ops;
	struct mlx5_compress_priv *priv;
	struct rte_compressdev_stats stats;
};

TAILQ_HEAD(mlx5_compress_privs, mlx5_compress_priv) mlx5_compress_priv_list =
				TAILQ_HEAD_INITIALIZER(mlx5_compress_priv_list);
static pthread_mutex_t priv_list_lock = PTHREAD_MUTEX_INITIALIZER;

int mlx5_compress_logtype;

static const struct rte_compressdev_capabilities mlx5_caps[] = {
	{
		.algo = RTE_COMP_ALGO_NULL,
		.comp_feature_flags = RTE_COMP_FF_ADLER32_CHECKSUM |
				      RTE_COMP_FF_CRC32_CHECKSUM |
				      RTE_COMP_FF_CRC32_ADLER32_CHECKSUM |
				      RTE_COMP_FF_SHAREABLE_PRIV_XFORM,
	},
	{
		.algo = RTE_COMP_ALGO_DEFLATE,
		.comp_feature_flags = RTE_COMP_FF_ADLER32_CHECKSUM |
				      RTE_COMP_FF_CRC32_CHECKSUM |
				      RTE_COMP_FF_CRC32_ADLER32_CHECKSUM |
				      RTE_COMP_FF_SHAREABLE_PRIV_XFORM |
				      RTE_COMP_FF_HUFFMAN_FIXED |
				      RTE_COMP_FF_HUFFMAN_DYNAMIC,
		.window_size = {.min = 10, .max = 15, .increment = 1},
	},
	{
		.algo = RTE_COMP_ALGO_LIST_END,
	}
};

static void
mlx5_compress_dev_info_get(struct rte_compressdev *dev,
			   struct rte_compressdev_info *info)
{
	RTE_SET_USED(dev);
	if (info != NULL) {
		info->max_nb_queue_pairs = MLX5_COMPRESS_MAX_QPS;
		info->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;
		info->capabilities = mlx5_caps;
	}
}

static int
mlx5_compress_dev_configure(struct rte_compressdev *dev,
			    struct rte_compressdev_config *config)
{
	struct mlx5_compress_priv *priv;

	if (dev == NULL || config == NULL)
		return -EINVAL;
	priv = dev->data->dev_private;
	priv->dev_config = *config;
	return 0;
}

static int
mlx5_compress_dev_close(struct rte_compressdev *dev)
{
	RTE_SET_USED(dev);
	return 0;
}

static int
mlx5_compress_qp_release(struct rte_compressdev *dev, uint16_t qp_id)
{
	struct mlx5_compress_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp->sq.sq != NULL)
		mlx5_devx_sq_destroy(&qp->sq);
	if (qp->cq.cq != NULL)
		mlx5_devx_cq_destroy(&qp->cq);
	if (qp->opaque_mr.obj != NULL) {
		void *opaq = qp->opaque_mr.addr;

		mlx5_common_verbs_dereg_mr(&qp->opaque_mr);
		if (opaq != NULL)
			rte_free(opaq);
	}
	mlx5_mr_btree_free(&qp->mr_ctrl.cache_bh);
	rte_free(qp);
	dev->data->queue_pairs[qp_id] = NULL;
	return 0;
}

static void
mlx5_compress_init_sq(struct mlx5_compress_qp *qp)
{
	volatile struct mlx5_gga_wqe *restrict wqe =
				    (volatile struct mlx5_gga_wqe *)qp->sq.wqes;
	volatile struct mlx5_gga_compress_opaque *opaq = qp->opaque_mr.addr;
	const uint32_t sq_ds = rte_cpu_to_be_32((qp->sq.sq->id << 8) | 4u);
	const uint32_t flags = RTE_BE32(MLX5_COMP_ALWAYS <<
					MLX5_COMP_MODE_OFFSET);
	const uint32_t opaq_lkey = rte_cpu_to_be_32(qp->opaque_mr.lkey);
	int i;

	/* All the next fields state should stay constant. */
	for (i = 0; i < qp->entries_n; ++i, ++wqe) {
		wqe->sq_ds = sq_ds;
		wqe->flags = flags;
		wqe->opaque_lkey = opaq_lkey;
		wqe->opaque_vaddr = rte_cpu_to_be_64
						((uint64_t)(uintptr_t)&opaq[i]);
	}
}

static int
mlx5_compress_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		       uint32_t max_inflight_ops, int socket_id)
{
	struct mlx5_compress_priv *priv = dev->data->dev_private;
	struct mlx5_compress_qp *qp;
	struct mlx5_devx_cq_attr cq_attr = {
		.uar_page_id = mlx5_os_get_devx_uar_page_id(priv->uar),
	};
	struct mlx5_devx_create_sq_attr sq_attr = {
		.user_index = qp_id,
		.wq_attr = (struct mlx5_devx_wq_attr){
			.pd = priv->pdn,
			.uar_page = mlx5_os_get_devx_uar_page_id(priv->uar),
		},
	};
	struct mlx5_devx_modify_sq_attr modify_attr = {
		.state = MLX5_SQC_STATE_RDY,
	};
	uint32_t log_ops_n = rte_log2_u32(max_inflight_ops);
	uint32_t alloc_size = sizeof(*qp);
	void *opaq_buf;
	int ret;

	alloc_size = RTE_ALIGN(alloc_size, RTE_CACHE_LINE_SIZE);
	alloc_size += sizeof(struct rte_comp_op *) * (1u << log_ops_n);
	qp = rte_zmalloc_socket(__func__, alloc_size, RTE_CACHE_LINE_SIZE,
				socket_id);
	if (qp == NULL) {
		DRV_LOG(ERR, "Failed to allocate qp memory.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	dev->data->queue_pairs[qp_id] = qp;
	if (mlx5_mr_btree_init(&qp->mr_ctrl.cache_bh, MLX5_MR_BTREE_CACHE_N,
			       priv->dev_config.socket_id)) {
		DRV_LOG(ERR, "Cannot allocate MR Btree for qp %u.",
			(uint32_t)qp_id);
		rte_errno = ENOMEM;
		goto err;
	}
	opaq_buf = rte_calloc(__func__, (size_t)1 << log_ops_n,
			      sizeof(struct mlx5_gga_compress_opaque),
			      sizeof(struct mlx5_gga_compress_opaque));
	if (opaq_buf == NULL) {
		DRV_LOG(ERR, "Failed to allocate opaque memory.");
		rte_errno = ENOMEM;
		goto err;
	}
	qp->entries_n = 1 << log_ops_n;
	qp->socket_id = socket_id;
	qp->qp_id = qp_id;
	qp->priv = priv;
	qp->ops = (struct rte_comp_op **)RTE_ALIGN((uintptr_t)(qp + 1),
						   RTE_CACHE_LINE_SIZE);
	if (mlx5_common_verbs_reg_mr(priv->pd, opaq_buf, qp->entries_n *
					sizeof(struct mlx5_gga_compress_opaque),
							 &qp->opaque_mr) != 0) {
		rte_free(opaq_buf);
		DRV_LOG(ERR, "Failed to register opaque MR.");
		rte_errno = ENOMEM;
		goto err;
	}
	ret = mlx5_devx_cq_create(priv->ctx, &qp->cq, log_ops_n, &cq_attr,
				  socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create CQ.");
		goto err;
	}
	sq_attr.cqn = qp->cq.cq->id;
	sq_attr.ts_format = mlx5_ts_format_conv(priv->sq_ts_format);
	ret = mlx5_devx_sq_create(priv->ctx, &qp->sq, log_ops_n, &sq_attr,
				  socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create SQ.");
		goto err;
	}
	mlx5_compress_init_sq(qp);
	ret = mlx5_devx_cmd_modify_sq(qp->sq.sq, &modify_attr);
	if (ret != 0) {
		DRV_LOG(ERR, "Can't change SQ state to ready.");
		goto err;
	}
	/* Save pointer of global generation number to check memory event. */
	qp->mr_ctrl.dev_gen_ptr = &priv->mr_scache.dev_gen;
	DRV_LOG(INFO, "QP %u: SQN=0x%X CQN=0x%X entries num = %u",
		(uint32_t)qp_id, qp->sq.sq->id, qp->cq.cq->id, qp->entries_n);
	return 0;
err:
	mlx5_compress_qp_release(dev, qp_id);
	return -1;
}

static int
mlx5_compress_xform_free(struct rte_compressdev *dev, void *xform)
{
	struct mlx5_compress_priv *priv = dev->data->dev_private;

	rte_spinlock_lock(&priv->xform_sl);
	LIST_REMOVE((struct mlx5_compress_xform *)xform, next);
	rte_spinlock_unlock(&priv->xform_sl);
	rte_free(xform);
	return 0;
}

static int
mlx5_compress_xform_create(struct rte_compressdev *dev,
			   const struct rte_comp_xform *xform,
			   void **private_xform)
{
	struct mlx5_compress_priv *priv = dev->data->dev_private;
	struct mlx5_compress_xform *xfrm;
	uint32_t size;

	if (xform->type == RTE_COMP_COMPRESS && xform->compress.level ==
							  RTE_COMP_LEVEL_NONE) {
		DRV_LOG(ERR, "Non-compressed block is not supported.");
		return -ENOTSUP;
	}
	if ((xform->type == RTE_COMP_COMPRESS && xform->compress.hash_algo !=
	     RTE_COMP_HASH_ALGO_NONE) || (xform->type == RTE_COMP_DECOMPRESS &&
		      xform->decompress.hash_algo != RTE_COMP_HASH_ALGO_NONE)) {
		DRV_LOG(ERR, "SHA is not supported.");
		return -ENOTSUP;
	}
	xfrm = rte_zmalloc_socket(__func__, sizeof(*xfrm), 0,
						    priv->dev_config.socket_id);
	if (xfrm == NULL)
		return -ENOMEM;
	xfrm->opcode = MLX5_OPCODE_MMO;
	xfrm->type = xform->type;
	switch (xform->type) {
	case RTE_COMP_COMPRESS:
		switch (xform->compress.algo) {
		case RTE_COMP_ALGO_NULL:
			xfrm->opcode += MLX5_OPC_MOD_MMO_DMA <<
							WQE_CSEG_OPC_MOD_OFFSET;
			break;
		case RTE_COMP_ALGO_DEFLATE:
			size = 1 << xform->compress.window_size;
			size /= MLX5_GGA_COMP_WIN_SIZE_UNITS;
			xfrm->gga_ctrl1 += RTE_MIN(rte_log2_u32(size),
					 MLX5_COMP_MAX_WIN_SIZE_CONF) <<
						WQE_GGA_COMP_WIN_SIZE_OFFSET;
			switch (xform->compress.level) {
			case RTE_COMP_LEVEL_PMD_DEFAULT:
				size = MLX5_GGA_COMP_LOG_BLOCK_SIZE_MAX;
				break;
			case RTE_COMP_LEVEL_MAX:
				size = priv->min_block_size;
				break;
			default:
				size = RTE_MAX(MLX5_GGA_COMP_LOG_BLOCK_SIZE_MAX
					+ 1 - xform->compress.level,
					priv->min_block_size);
			}
			xfrm->gga_ctrl1 += RTE_MIN(size,
					    MLX5_GGA_COMP_LOG_BLOCK_SIZE_MAX) <<
						 WQE_GGA_COMP_BLOCK_SIZE_OFFSET;
			xfrm->opcode += MLX5_OPC_MOD_MMO_COMP <<
							WQE_CSEG_OPC_MOD_OFFSET;
			size = xform->compress.deflate.huffman ==
						      RTE_COMP_HUFFMAN_DYNAMIC ?
					    MLX5_GGA_COMP_LOG_DYNAMIC_SIZE_MAX :
					     MLX5_GGA_COMP_LOG_DYNAMIC_SIZE_MIN;
			xfrm->gga_ctrl1 += size <<
					       WQE_GGA_COMP_DYNAMIC_SIZE_OFFSET;
			break;
		default:
			goto err;
		}
		xfrm->csum_type = xform->compress.chksum;
		break;
	case RTE_COMP_DECOMPRESS:
		switch (xform->decompress.algo) {
		case RTE_COMP_ALGO_NULL:
			xfrm->opcode += MLX5_OPC_MOD_MMO_DMA <<
							WQE_CSEG_OPC_MOD_OFFSET;
			break;
		case RTE_COMP_ALGO_DEFLATE:
			xfrm->opcode += MLX5_OPC_MOD_MMO_DECOMP <<
							WQE_CSEG_OPC_MOD_OFFSET;
			break;
		default:
			goto err;
		}
		xfrm->csum_type = xform->decompress.chksum;
		break;
	default:
		DRV_LOG(ERR, "Algorithm %u is not supported.", xform->type);
		goto err;
	}
	DRV_LOG(DEBUG, "New xform: gga ctrl1 = 0x%08X opcode = 0x%08X csum "
		"type = %d.", xfrm->gga_ctrl1, xfrm->opcode, xfrm->csum_type);
	xfrm->gga_ctrl1 = rte_cpu_to_be_32(xfrm->gga_ctrl1);
	rte_spinlock_lock(&priv->xform_sl);
	LIST_INSERT_HEAD(&priv->xform_list, xfrm, next);
	rte_spinlock_unlock(&priv->xform_sl);
	*private_xform = xfrm;
	return 0;
err:
	rte_free(xfrm);
	return -ENOTSUP;
}

static void
mlx5_compress_dev_stop(struct rte_compressdev *dev)
{
	RTE_SET_USED(dev);
}

static int
mlx5_compress_dev_start(struct rte_compressdev *dev)
{
	RTE_SET_USED(dev);
	return 0;
}

static void
mlx5_compress_stats_get(struct rte_compressdev *dev,
		struct rte_compressdev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct mlx5_compress_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;
		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

static void
mlx5_compress_stats_reset(struct rte_compressdev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct mlx5_compress_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}

static struct rte_compressdev_ops mlx5_compress_ops = {
	.dev_configure		= mlx5_compress_dev_configure,
	.dev_start		= mlx5_compress_dev_start,
	.dev_stop		= mlx5_compress_dev_stop,
	.dev_close		= mlx5_compress_dev_close,
	.dev_infos_get		= mlx5_compress_dev_info_get,
	.stats_get		= mlx5_compress_stats_get,
	.stats_reset		= mlx5_compress_stats_reset,
	.queue_pair_setup	= mlx5_compress_qp_setup,
	.queue_pair_release	= mlx5_compress_qp_release,
	.private_xform_create	= mlx5_compress_xform_create,
	.private_xform_free	= mlx5_compress_xform_free,
	.stream_create		= NULL,
	.stream_free		= NULL,
};

/**
 * Query LKey from a packet buffer for QP. If not found, add the mempool.
 *
 * @param priv
 *   Pointer to the priv object.
 * @param addr
 *   Search key.
 * @param mr_ctrl
 *   Pointer to per-queue MR control structure.
 * @param ol_flags
 *   Mbuf offload features.
 *
 * @return
 *   Searched LKey on success, UINT32_MAX on no match.
 */
static __rte_always_inline uint32_t
mlx5_compress_addr2mr(struct mlx5_compress_priv *priv, uintptr_t addr,
		      struct mlx5_mr_ctrl *mr_ctrl, uint64_t ol_flags)
{
	uint32_t lkey;

	/* Check generation bit to see if there's any change on existing MRs. */
	if (unlikely(*mr_ctrl->dev_gen_ptr != mr_ctrl->cur_gen))
		mlx5_mr_flush_local_cache(mr_ctrl);
	/* Linear search on MR cache array. */
	lkey = mlx5_mr_lookup_lkey(mr_ctrl->cache, &mr_ctrl->mru,
				   MLX5_MR_CACHE_N, addr);
	if (likely(lkey != UINT32_MAX))
		return lkey;
	/* Take slower bottom-half on miss. */
	return mlx5_mr_addr2mr_bh(priv->pd, 0, &priv->mr_scache, mr_ctrl, addr,
				  !!(ol_flags & EXT_ATTACHED_MBUF));
}

static __rte_always_inline uint32_t
mlx5_compress_dseg_set(struct mlx5_compress_qp *qp,
		       volatile struct mlx5_wqe_dseg *restrict dseg,
		       struct rte_mbuf *restrict mbuf,
		       uint32_t offset, uint32_t len)
{
	uintptr_t addr = rte_pktmbuf_mtod_offset(mbuf, uintptr_t, offset);

	dseg->bcount = rte_cpu_to_be_32(len);
	dseg->lkey = mlx5_compress_addr2mr(qp->priv, addr, &qp->mr_ctrl,
					   mbuf->ol_flags);
	dseg->pbuf = rte_cpu_to_be_64(addr);
	return dseg->lkey;
}

/*
 * Provide safe 64bit store operation to mlx5 UAR region for both 32bit and
 * 64bit architectures.
 */
static __rte_always_inline void
mlx5_compress_uar_write(uint64_t val, struct mlx5_compress_priv *priv)
{
#ifdef RTE_ARCH_64
	*priv->uar_addr = val;
#else /* !RTE_ARCH_64 */
	rte_spinlock_lock(&priv->uar32_sl);
	*(volatile uint32_t *)priv->uar_addr = val;
	rte_io_wmb();
	*((volatile uint32_t *)priv->uar_addr + 1) = val >> 32;
	rte_spinlock_unlock(&priv->uar32_sl);
#endif
}

static uint16_t
mlx5_compress_enqueue_burst(void *queue_pair, struct rte_comp_op **ops,
			    uint16_t nb_ops)
{
	struct mlx5_compress_qp *qp = queue_pair;
	volatile struct mlx5_gga_wqe *wqes = (volatile struct mlx5_gga_wqe *)
							      qp->sq.wqes, *wqe;
	struct mlx5_compress_xform *xform;
	struct rte_comp_op *op;
	uint16_t mask = qp->entries_n - 1;
	uint16_t remain = qp->entries_n - (qp->pi - qp->ci);
	uint16_t idx;
	bool invalid;

	if (remain < nb_ops)
		nb_ops = remain;
	else
		remain = nb_ops;
	if (unlikely(remain == 0))
		return 0;
	do {
		idx = qp->pi & mask;
		wqe = &wqes[idx];
		rte_prefetch0(&wqes[(qp->pi + 1) & mask]);
		op = *ops++;
		xform = op->private_xform;
		/*
		 * Check operation arguments and error cases:
		 *   - Operation type must be state-less.
		 *   - Compress operation flush flag must be FULL or FINAL.
		 *   - Source and destination buffers must be mapped internally.
		 */
		invalid = op->op_type != RTE_COMP_OP_STATELESS ||
					    (xform->type == RTE_COMP_COMPRESS &&
					  op->flush_flag < RTE_COMP_FLUSH_FULL);
		if (unlikely(invalid ||
			     (mlx5_compress_dseg_set(qp, &wqe->gather,
						     op->m_src,
						     op->src.offset,
						     op->src.length) ==
								  UINT32_MAX) ||
			     (mlx5_compress_dseg_set(qp, &wqe->scatter,
						op->m_dst,
						op->dst.offset,
						rte_pktmbuf_pkt_len(op->m_dst) -
							      op->dst.offset) ==
								 UINT32_MAX))) {
			op->status = invalid ? RTE_COMP_OP_STATUS_INVALID_ARGS :
						       RTE_COMP_OP_STATUS_ERROR;
			nb_ops -= remain;
			if (unlikely(nb_ops == 0))
				return 0;
			break;
		}
		wqe->gga_ctrl1 = xform->gga_ctrl1;
		wqe->opcode = rte_cpu_to_be_32(xform->opcode + (qp->pi << 8));
		qp->ops[idx] = op;
		qp->pi++;
	} while (--remain);
	qp->stats.enqueued_count += nb_ops;
	rte_io_wmb();
	qp->sq.db_rec[MLX5_SND_DBR] = rte_cpu_to_be_32(qp->pi);
	rte_wmb();
	mlx5_compress_uar_write(*(volatile uint64_t *)wqe, qp->priv);
	rte_wmb();
	return nb_ops;
}

static void
mlx5_compress_dump_err_objs(volatile uint32_t *cqe, volatile uint32_t *wqe,
			     volatile uint32_t *opaq)
{
	size_t i;

	DRV_LOG(ERR, "Error cqe:");
	for (i = 0; i < sizeof(struct mlx5_err_cqe) >> 2; i += 4)
		DRV_LOG(ERR, "%08X %08X %08X %08X", cqe[i], cqe[i + 1],
			cqe[i + 2], cqe[i + 3]);
	DRV_LOG(ERR, "\nError wqe:");
	for (i = 0; i < sizeof(struct mlx5_gga_wqe) >> 2; i += 4)
		DRV_LOG(ERR, "%08X %08X %08X %08X", wqe[i], wqe[i + 1],
			wqe[i + 2], wqe[i + 3]);
	DRV_LOG(ERR, "\nError opaq:");
	for (i = 0; i < sizeof(struct mlx5_gga_compress_opaque) >> 2; i += 4)
		DRV_LOG(ERR, "%08X %08X %08X %08X", opaq[i], opaq[i + 1],
			opaq[i + 2], opaq[i + 3]);
}

static void
mlx5_compress_cqe_err_handle(struct mlx5_compress_qp *qp,
			     struct rte_comp_op *op)
{
	const uint32_t idx = qp->ci & (qp->entries_n - 1);
	volatile struct mlx5_err_cqe *cqe = (volatile struct mlx5_err_cqe *)
							      &qp->cq.cqes[idx];
	volatile struct mlx5_gga_wqe *wqes = (volatile struct mlx5_gga_wqe *)
								    qp->sq.wqes;
	volatile struct mlx5_gga_compress_opaque *opaq = qp->opaque_mr.addr;

	op->status = RTE_COMP_OP_STATUS_ERROR;
	op->consumed = 0;
	op->produced = 0;
	op->output_chksum = 0;
	op->debug_status = rte_be_to_cpu_32(opaq[idx].syndrom) |
			      ((uint64_t)rte_be_to_cpu_32(cqe->syndrome) << 32);
	mlx5_compress_dump_err_objs((volatile uint32_t *)cqe,
				 (volatile uint32_t *)&wqes[idx],
				 (volatile uint32_t *)&opaq[idx]);
	qp->stats.dequeue_err_count++;
}

static uint16_t
mlx5_compress_dequeue_burst(void *queue_pair, struct rte_comp_op **ops,
			    uint16_t nb_ops)
{
	struct mlx5_compress_qp *qp = queue_pair;
	volatile struct mlx5_compress_xform *restrict xform;
	volatile struct mlx5_cqe *restrict cqe;
	volatile struct mlx5_gga_compress_opaque *opaq = qp->opaque_mr.addr;
	struct rte_comp_op *restrict op;
	const unsigned int cq_size = qp->entries_n;
	const unsigned int mask = cq_size - 1;
	uint32_t idx;
	uint32_t next_idx = qp->ci & mask;
	const uint16_t max = RTE_MIN((uint16_t)(qp->pi - qp->ci), nb_ops);
	uint16_t i = 0;
	int ret;

	if (unlikely(max == 0))
		return 0;
	do {
		idx = next_idx;
		next_idx = (qp->ci + 1) & mask;
		rte_prefetch0(&qp->cq.cqes[next_idx]);
		rte_prefetch0(qp->ops[next_idx]);
		op = qp->ops[idx];
		cqe = &qp->cq.cqes[idx];
		ret = check_cqe(cqe, cq_size, qp->ci);
		/*
		 * Be sure owner read is done before any other cookie field or
		 * opaque field.
		 */
		rte_io_rmb();
		if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN)) {
			if (likely(ret == MLX5_CQE_STATUS_HW_OWN))
				break;
			mlx5_compress_cqe_err_handle(qp, op);
		} else {
			xform = op->private_xform;
			op->status = RTE_COMP_OP_STATUS_SUCCESS;
			op->consumed = op->src.length;
			op->produced = rte_be_to_cpu_32(cqe->byte_cnt);
			MLX5_ASSERT(cqe->byte_cnt ==
				    opaq[idx].scattered_length);
			switch (xform->csum_type) {
			case RTE_COMP_CHECKSUM_CRC32:
				op->output_chksum = (uint64_t)rte_be_to_cpu_32
						    (opaq[idx].crc32);
				break;
			case RTE_COMP_CHECKSUM_ADLER32:
				op->output_chksum = (uint64_t)rte_be_to_cpu_32
					    (opaq[idx].adler32) << 32;
				break;
			case RTE_COMP_CHECKSUM_CRC32_ADLER32:
				op->output_chksum = (uint64_t)rte_be_to_cpu_32
							     (opaq[idx].crc32) |
						     ((uint64_t)rte_be_to_cpu_32
						     (opaq[idx].adler32) << 32);
				break;
			default:
				break;
			}
		}
		ops[i++] = op;
		qp->ci++;
	} while (i < max);
	if (likely(i != 0)) {
		rte_io_wmb();
		qp->cq.db_rec[0] = rte_cpu_to_be_32(qp->ci);
		qp->stats.dequeued_count += i;
	}
	return i;
}

static void
mlx5_compress_hw_global_release(struct mlx5_compress_priv *priv)
{
	if (priv->pd != NULL) {
		claim_zero(mlx5_glue->dealloc_pd(priv->pd));
		priv->pd = NULL;
	}
	if (priv->uar != NULL) {
		mlx5_glue->devx_free_uar(priv->uar);
		priv->uar = NULL;
	}
}

static int
mlx5_compress_pd_create(struct mlx5_compress_priv *priv)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5dv_obj obj;
	struct mlx5dv_pd pd_info;
	int ret;

	priv->pd = mlx5_glue->alloc_pd(priv->ctx);
	if (priv->pd == NULL) {
		DRV_LOG(ERR, "Failed to allocate PD.");
		return errno ? -errno : -ENOMEM;
	}
	obj.pd.in = priv->pd;
	obj.pd.out = &pd_info;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_PD);
	if (ret != 0) {
		DRV_LOG(ERR, "Fail to get PD object info.");
		mlx5_glue->dealloc_pd(priv->pd);
		priv->pd = NULL;
		return -errno;
	}
	priv->pdn = pd_info.pdn;
	return 0;
#else
	(void)priv;
	DRV_LOG(ERR, "Cannot get pdn - no DV support.");
	return -ENOTSUP;
#endif /* HAVE_IBV_FLOW_DV_SUPPORT */
}

static int
mlx5_compress_hw_global_prepare(struct mlx5_compress_priv *priv)
{
	if (mlx5_compress_pd_create(priv) != 0)
		return -1;
	priv->uar = mlx5_devx_alloc_uar(priv->ctx, -1);
	if (priv->uar == NULL || mlx5_os_get_devx_uar_reg_addr(priv->uar) ==
	    NULL) {
		rte_errno = errno;
		claim_zero(mlx5_glue->dealloc_pd(priv->pd));
		DRV_LOG(ERR, "Failed to allocate UAR.");
		return -1;
	}
	priv->uar_addr = mlx5_os_get_devx_uar_reg_addr(priv->uar);
	MLX5_ASSERT(priv->uar_addr);
#ifndef RTE_ARCH_64
	rte_spinlock_init(&priv->uar32_sl);
#endif /* RTE_ARCH_64 */
	return 0;
}

/**
 * Callback for memory event.
 *
 * @param event_type
 *   Memory event type.
 * @param addr
 *   Address of memory.
 * @param len
 *   Size of memory.
 */
static void
mlx5_compress_mr_mem_event_cb(enum rte_mem_event event_type, const void *addr,
			      size_t len, void *arg __rte_unused)
{
	struct mlx5_compress_priv *priv;

	/* Must be called from the primary process. */
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	switch (event_type) {
	case RTE_MEM_EVENT_FREE:
		pthread_mutex_lock(&priv_list_lock);
		/* Iterate all the existing mlx5 devices. */
		TAILQ_FOREACH(priv, &mlx5_compress_priv_list, next)
			mlx5_free_mr_by_addr(&priv->mr_scache,
					     priv->ctx->device->name,
					     addr, len);
		pthread_mutex_unlock(&priv_list_lock);
		break;
	case RTE_MEM_EVENT_ALLOC:
	default:
		break;
	}
}

static int
mlx5_compress_dev_probe(struct rte_device *dev)
{
	struct ibv_device *ibv;
	struct rte_compressdev *cdev;
	struct ibv_context *ctx;
	struct mlx5_compress_priv *priv;
	struct mlx5_hca_attr att = { 0 };
	struct rte_compressdev_pmd_init_params init_params = {
		.name = "",
		.socket_id = dev->numa_node,
	};

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		DRV_LOG(ERR, "Non-primary process type is not supported.");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	ibv = mlx5_os_get_ibv_dev(dev);
	if (ibv == NULL)
		return -rte_errno;
	ctx = mlx5_glue->dv_open_device(ibv);
	if (ctx == NULL) {
		DRV_LOG(ERR, "Failed to open IB device \"%s\".", ibv->name);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	if (mlx5_devx_cmd_query_hca_attr(ctx, &att) != 0 ||
	    att.mmo_compress_en == 0 || att.mmo_decompress_en == 0 ||
	    att.mmo_dma_en == 0) {
		DRV_LOG(ERR, "Not enough capabilities to support compress "
			"operations, maybe old FW/OFED version?");
		claim_zero(mlx5_glue->close_device(ctx));
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	cdev = rte_compressdev_pmd_create(ibv->name, dev,
					  sizeof(*priv), &init_params);
	if (cdev == NULL) {
		DRV_LOG(ERR, "Failed to create device \"%s\".", ibv->name);
		claim_zero(mlx5_glue->close_device(ctx));
		return -ENODEV;
	}
	DRV_LOG(INFO,
		"Compress device %s was created successfully.", ibv->name);
	cdev->dev_ops = &mlx5_compress_ops;
	cdev->dequeue_burst = mlx5_compress_dequeue_burst;
	cdev->enqueue_burst = mlx5_compress_enqueue_burst;
	cdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;
	priv = cdev->data->dev_private;
	priv->ctx = ctx;
	priv->cdev = cdev;
	priv->min_block_size = att.compress_min_block_size;
	priv->sq_ts_format = att.sq_ts_format;
	if (mlx5_compress_hw_global_prepare(priv) != 0) {
		rte_compressdev_pmd_destroy(priv->cdev);
		claim_zero(mlx5_glue->close_device(priv->ctx));
		return -1;
	}
	if (mlx5_mr_btree_init(&priv->mr_scache.cache,
			     MLX5_MR_BTREE_CACHE_N * 2, rte_socket_id()) != 0) {
		DRV_LOG(ERR, "Failed to allocate shared cache MR memory.");
		mlx5_compress_hw_global_release(priv);
		rte_compressdev_pmd_destroy(priv->cdev);
		claim_zero(mlx5_glue->close_device(priv->ctx));
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->mr_scache.reg_mr_cb = mlx5_common_verbs_reg_mr;
	priv->mr_scache.dereg_mr_cb = mlx5_common_verbs_dereg_mr;
	/* Register callback function for global shared MR cache management. */
	if (TAILQ_EMPTY(&mlx5_compress_priv_list))
		rte_mem_event_callback_register("MLX5_MEM_EVENT_CB",
						mlx5_compress_mr_mem_event_cb,
						NULL);
	pthread_mutex_lock(&priv_list_lock);
	TAILQ_INSERT_TAIL(&mlx5_compress_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	return 0;
}

static int
mlx5_compress_dev_remove(struct rte_device *dev)
{
	struct mlx5_compress_priv *priv = NULL;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &mlx5_compress_priv_list, next)
		if (priv->cdev->device == dev)
			break;
	if (priv)
		TAILQ_REMOVE(&mlx5_compress_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	if (priv) {
		if (TAILQ_EMPTY(&mlx5_compress_priv_list))
			rte_mem_event_callback_unregister("MLX5_MEM_EVENT_CB",
							  NULL);
		mlx5_mr_release_cache(&priv->mr_scache);
		mlx5_compress_hw_global_release(priv);
		rte_compressdev_pmd_destroy(priv->cdev);
		claim_zero(mlx5_glue->close_device(priv->ctx));
	}
	return 0;
}

static const struct rte_pci_id mlx5_compress_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6DXBF)
	},
	{
		.vendor_id = 0
	}
};

static struct mlx5_class_driver mlx5_compress_driver = {
	.drv_class = MLX5_CLASS_COMPRESS,
	.name = RTE_STR(MLX5_COMPRESS_DRIVER_NAME),
	.id_table = mlx5_compress_pci_id_map,
	.probe = mlx5_compress_dev_probe,
	.remove = mlx5_compress_dev_remove,
};

RTE_INIT(rte_mlx5_compress_init)
{
	mlx5_common_init();
	if (mlx5_glue != NULL)
		mlx5_class_driver_register(&mlx5_compress_driver);
}

RTE_LOG_REGISTER_DEFAULT(mlx5_compress_logtype, NOTICE)
RTE_PMD_EXPORT_NAME(MLX5_COMPRESS_DRIVER_NAME, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(MLX5_COMPRESS_DRIVER_NAME, mlx5_compress_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(MLX5_COMPRESS_DRIVER_NAME, "* ib_uverbs & mlx5_core & mlx5_ib");
