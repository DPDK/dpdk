/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <bus_pci_driver.h>
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
#define MLX5_COMP_NUM_SUP_ALGO 4

struct mlx5_compress_devarg_params {
	uint32_t log_block_sz;
};

struct mlx5_compress_xform {
	LIST_ENTRY(mlx5_compress_xform) next;
	enum rte_comp_xform_type type;
	enum rte_comp_checksum_type csum_type;
	uint32_t opcode;
	uint32_t gga_ctrl1; /* BE. */
};

struct mlx5_compress_priv {
	TAILQ_ENTRY(mlx5_compress_priv) next;
	struct rte_compressdev *compressdev;
	struct mlx5_common_device *cdev; /* Backend mlx5 device. */
	struct mlx5_uar uar;
	struct rte_compressdev_config dev_config;
	struct rte_compressdev_capabilities caps[MLX5_COMP_NUM_SUP_ALGO];
	LIST_HEAD(xform_list, mlx5_compress_xform) xform_list;
	rte_spinlock_t xform_sl;
	uint32_t log_block_sz;
	uint32_t crc32_opaq_offs;
};

struct mlx5_compress_qp {
	uint16_t qp_id;
	uint16_t entries_n;
	uint16_t pi;
	uint16_t ci;
	struct mlx5_mr_ctrl mr_ctrl;
	int socket_id;
	struct mlx5_devx_cq cq;
	struct mlx5_devx_qp qp;
	struct mlx5_pmd_mr opaque_mr;
	struct rte_comp_op **ops;
	struct mlx5_compress_priv *priv;
	struct rte_compressdev_stats stats;
};

TAILQ_HEAD(mlx5_compress_privs, mlx5_compress_priv) mlx5_compress_priv_list =
				TAILQ_HEAD_INITIALIZER(mlx5_compress_priv_list);
static pthread_mutex_t priv_list_lock = PTHREAD_MUTEX_INITIALIZER;

int mlx5_compress_logtype;

static void
mlx5_compress_dev_info_get(struct rte_compressdev *dev,
			   struct rte_compressdev_info *info)
{
	if (dev != NULL && info != NULL) {
		struct mlx5_compress_priv *priv = dev->data->dev_private;

		info->max_nb_queue_pairs = MLX5_COMPRESS_MAX_QPS;
		info->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;
		info->capabilities = priv->caps;
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

	if (qp->qp.qp != NULL)
		mlx5_devx_qp_destroy(&qp->qp);
	if (qp->cq.cq != NULL)
		mlx5_devx_cq_destroy(&qp->cq);
	if (qp->opaque_mr.obj != NULL) {
		void *opaq = qp->opaque_mr.addr;

		mlx5_common_verbs_dereg_mr(&qp->opaque_mr);
		rte_free(opaq);
	}
	mlx5_mr_btree_free(&qp->mr_ctrl.cache_bh);
	rte_free(qp);
	dev->data->queue_pairs[qp_id] = NULL;
	return 0;
}

static void
mlx5_compress_init_qp(struct mlx5_compress_qp *qp)
{
	volatile struct mlx5_gga_wqe *restrict wqe =
				    (volatile struct mlx5_gga_wqe *)qp->qp.wqes;
	volatile union mlx5_gga_compress_opaque *opaq = qp->opaque_mr.addr;
	const uint32_t sq_ds = rte_cpu_to_be_32((qp->qp.qp->id << 8) | 4u);
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
	struct mlx5_hca_attr *attr = &priv->cdev->config.hca_attr;
	struct mlx5_compress_qp *qp;
	struct mlx5_devx_cq_attr cq_attr = {
		.uar_page_id = mlx5_os_get_devx_uar_page_id(priv->uar.obj),
	};
	struct mlx5_devx_qp_attr qp_attr = {
		.pd = priv->cdev->pdn,
		.uar_index = mlx5_os_get_devx_uar_page_id(priv->uar.obj),
		.user_index = qp_id,
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
	if (mlx5_mr_ctrl_init(&qp->mr_ctrl, &priv->cdev->mr_scache.dev_gen,
			      priv->dev_config.socket_id)) {
		DRV_LOG(ERR, "Cannot allocate MR Btree for qp %u.",
			(uint32_t)qp_id);
		rte_errno = ENOMEM;
		goto err;
	}
	opaq_buf = rte_calloc(__func__, (size_t)1 << log_ops_n,
			      sizeof(union mlx5_gga_compress_opaque),
			      sizeof(union mlx5_gga_compress_opaque));
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
	if (mlx5_common_verbs_reg_mr(priv->cdev->pd, opaq_buf, qp->entries_n *
					sizeof(union mlx5_gga_compress_opaque),
							 &qp->opaque_mr) != 0) {
		rte_free(opaq_buf);
		DRV_LOG(ERR, "Failed to register opaque MR.");
		rte_errno = ENOMEM;
		goto err;
	}
	ret = mlx5_devx_cq_create(priv->cdev->ctx, &qp->cq, log_ops_n, &cq_attr,
				  socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create CQ.");
		goto err;
	}
	qp_attr.cqn = qp->cq.cq->id;
	qp_attr.ts_format = mlx5_ts_format_conv(attr->qp_ts_format);
	qp_attr.num_of_receive_wqes = 0;
	qp_attr.num_of_send_wqbbs = RTE_BIT32(log_ops_n);
	qp_attr.mmo = attr->mmo_compress_qp_en || attr->mmo_dma_qp_en ||
		      attr->decomp_lz4_checksum_en ||
		      attr->decomp_lz4_no_checksum_en ||
		      attr->decomp_deflate_v1_en || attr->decomp_deflate_v2_en;
	ret = mlx5_devx_qp_create(priv->cdev->ctx, &qp->qp,
					qp_attr.num_of_send_wqbbs *
					MLX5_WQE_SIZE, &qp_attr, socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create QP.");
		goto err;
	}
	mlx5_compress_init_qp(qp);
	ret = mlx5_devx_qp2rts(&qp->qp, 0);
	if (ret)
		goto err;
	DRV_LOG(INFO, "QP %u: SQN=0x%X CQN=0x%X entries num = %u",
		(uint32_t)qp_id, qp->qp.qp->id, qp->cq.cq->id, qp->entries_n);
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
mlx5_compress_xform_validate(const struct rte_comp_xform *xform,
			     const struct mlx5_hca_attr *attr)
{
	switch (xform->type) {
	case RTE_COMP_COMPRESS:
		if (xform->compress.algo == RTE_COMP_ALGO_NULL &&
		    !attr->mmo_dma_qp_en && !attr->mmo_dma_sq_en) {
			DRV_LOG(ERR, "Not enough capabilities to support DMA operation, maybe old FW/OFED version?");
			return -ENOTSUP;
		} else if (!attr->mmo_compress_qp_en &&
			   !attr->mmo_compress_sq_en) {
			DRV_LOG(ERR, "Not enough capabilities to support compress operation.");
			return -ENOTSUP;
		}
		if (xform->compress.algo == RTE_COMP_ALGO_LZ4) {
			DRV_LOG(ERR, "LZ4 compression is not supported.");
			return -ENOTSUP;
		}
		if (xform->compress.level == RTE_COMP_LEVEL_NONE) {
			DRV_LOG(ERR, "Non-compressed block is not supported.");
			return -ENOTSUP;
		}
		if (xform->compress.hash_algo != RTE_COMP_HASH_ALGO_NONE) {
			DRV_LOG(ERR, "SHA is not supported.");
			return -ENOTSUP;
		}
		if (xform->compress.chksum == RTE_COMP_CHECKSUM_XXHASH32) {
			DRV_LOG(ERR, "xxHash32 checksum isn't supported in compress operation.");
			return -ENOTSUP;
		}
		break;
	case RTE_COMP_DECOMPRESS:
		switch (xform->decompress.algo) {
		case RTE_COMP_ALGO_NULL:
			if (!attr->mmo_dma_qp_en && !attr->mmo_dma_sq_en) {
				DRV_LOG(ERR, "Not enough capabilities to support DMA operation, maybe old FW/OFED version?");
				return -ENOTSUP;
			}
			break;
		case RTE_COMP_ALGO_DEFLATE:
			if (!attr->decomp_deflate_v1_en &&
			    !attr->decomp_deflate_v2_en &&
			    !attr->mmo_decompress_sq_en) {
				DRV_LOG(ERR, "Not enough capabilities to support decompress DEFLATE algorithm, maybe old FW/OFED version?");
				return -ENOTSUP;
			}
			switch (xform->decompress.chksum) {
			case RTE_COMP_CHECKSUM_NONE:
			case RTE_COMP_CHECKSUM_CRC32:
			case RTE_COMP_CHECKSUM_ADLER32:
			case RTE_COMP_CHECKSUM_CRC32_ADLER32:
				break;
			case RTE_COMP_CHECKSUM_XXHASH32:
			default:
				DRV_LOG(ERR, "DEFLATE algorithm doesn't support %u checksum.",
					xform->decompress.chksum);
				return -ENOTSUP;
			}
			break;
		case RTE_COMP_ALGO_LZ4:
			if (!attr->decomp_lz4_no_checksum_en &&
			    !attr->decomp_lz4_checksum_en) {
				DRV_LOG(ERR, "Not enough capabilities to support decompress LZ4 algorithm, maybe old FW/OFED version?");
				return -ENOTSUP;
			}
			if (xform->decompress.lz4.flags &
			    RTE_COMP_LZ4_FLAG_BLOCK_CHECKSUM) {
				if (!attr->decomp_lz4_checksum_en) {
					DRV_LOG(ERR, "Not enough capabilities to support decompress LZ4 block with checksum param, maybe old FW/OFED version?");
					return -ENOTSUP;
				}
			} else {
				if (!attr->decomp_lz4_no_checksum_en) {
					DRV_LOG(ERR, "Not enough capabilities to support decompress LZ4 block without checksum param, maybe old FW/OFED version?");
					return -ENOTSUP;
				}
			}
			if (xform->decompress.chksum !=
			    RTE_COMP_CHECKSUM_XXHASH32 &&
			    xform->decompress.chksum !=
			    RTE_COMP_CHECKSUM_NONE) {
				DRV_LOG(ERR, "LZ4 algorithm supports only xxHash32 checksum.");
				return -ENOTSUP;
			}
			break;
		default:
			DRV_LOG(ERR, "Algorithm %u is not supported.",
				xform->decompress.algo);
			return -ENOTSUP;
		}
		if (xform->decompress.hash_algo != RTE_COMP_HASH_ALGO_NONE) {
			DRV_LOG(ERR, "SHA is not supported.");
			return -ENOTSUP;
		}
		break;
	default:
		DRV_LOG(ERR, "Xform type should be compress/decompress");
		return -ENOTSUP;
	}
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
	int ret;

	ret = mlx5_compress_xform_validate(xform, &priv->cdev->config.hca_attr);
	if (ret < 0)
		return ret;
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
			size = priv->log_block_sz;
			xfrm->gga_ctrl1 += size <<
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
			xfrm->gga_ctrl1 += WQE_GGA_DECOMP_DEFLATE <<
						     WQE_GGA_DECOMP_TYPE_OFFSET;
			break;
		case RTE_COMP_ALGO_LZ4:
			xfrm->opcode += MLX5_OPC_MOD_MMO_DECOMP <<
							WQE_CSEG_OPC_MOD_OFFSET;
			xfrm->gga_ctrl1 += WQE_GGA_DECOMP_LZ4 <<
						     WQE_GGA_DECOMP_TYPE_OFFSET;
			if (xform->decompress.lz4.flags &
			    RTE_COMP_LZ4_FLAG_BLOCK_CHECKSUM)
				xfrm->gga_ctrl1 +=
				      MLX5_GGA_DECOMP_LZ4_BLOCK_WITH_CHECKSUM <<
						   WQE_GGA_DECOMP_PARAMS_OFFSET;
			else
				xfrm->gga_ctrl1 +=
				      MLX5_GGA_DECOMP_LZ4_BLOCK_WITHOUT_CHECKSUM
						<< WQE_GGA_DECOMP_PARAMS_OFFSET;
			if (xform->decompress.lz4.flags &
			    RTE_COMP_LZ4_FLAG_BLOCK_INDEPENDENCE)
				xfrm->gga_ctrl1 += 1u <<
					WQE_GGA_DECOMP_BLOCK_INDEPENDENT_OFFSET;
			break;
		default:
			goto err;
		}
		xfrm->csum_type = xform->decompress.chksum;
		break;
	default:
		DRV_LOG(ERR, "Operation %u is not supported.", xform->type);
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
	struct mlx5_compress_priv *priv = dev->data->dev_private;

	return mlx5_dev_mempool_subscribe(priv->cdev);
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

static __rte_always_inline uint32_t
mlx5_compress_dseg_set(struct mlx5_compress_qp *qp,
		       volatile struct mlx5_wqe_dseg *restrict dseg,
		       struct rte_mbuf *restrict mbuf,
		       uint32_t offset, uint32_t len)
{
	uintptr_t addr = rte_pktmbuf_mtod_offset(mbuf, uintptr_t, offset);

	dseg->bcount = rte_cpu_to_be_32(len);
	dseg->lkey = mlx5_mr_mb2mr(&qp->mr_ctrl, mbuf);
	dseg->pbuf = rte_cpu_to_be_64(addr);
	return dseg->lkey;
}

static uint16_t
mlx5_compress_enqueue_burst(void *queue_pair, struct rte_comp_op **ops,
			    uint16_t nb_ops)
{
	struct mlx5_compress_qp *qp = queue_pair;
	volatile struct mlx5_gga_wqe *wqes = (volatile struct mlx5_gga_wqe *)
							      qp->qp.wqes, *wqe;
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
	mlx5_doorbell_ring(&qp->priv->uar.bf_db, *(volatile uint64_t *)wqe,
			   qp->pi, &qp->qp.db_rec[MLX5_SND_DBR],
			   !qp->priv->uar.dbnc);
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
	for (i = 0; i < sizeof(union mlx5_gga_compress_opaque) >> 2; i += 4)
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
								    qp->qp.wqes;
	volatile union mlx5_gga_compress_opaque *opaq = qp->opaque_mr.addr;

	volatile uint32_t *synd_word = RTE_PTR_ADD(cqe, MLX5_ERROR_CQE_SYNDROME_OFFSET);
	switch (*synd_word) {
	case MLX5_GGA_COMP_OUT_OF_SPACE_SYNDROME_BE:
		op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
		DRV_LOG(DEBUG, "OUT OF SPACE error, output is bigger than dst buffer.");
		break;
	case MLX5_GGA_COMP_MISSING_BFINAL_SYNDROME_BE:
		DRV_LOG(DEBUG, "The last compressed block missed the B-final flag; maybe the compressed data is not complete or garbaged?");
		/* fallthrough */
	default:
		op->status = RTE_COMP_OP_STATUS_ERROR;
	}
	op->consumed = 0;
	op->produced = 0;
	op->output_chksum = 0;
	op->debug_status = rte_be_to_cpu_32(opaq[idx].syndrome) |
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
	volatile union mlx5_gga_compress_opaque *opaq = qp->opaque_mr.addr;
	struct rte_comp_op *restrict op;
	const unsigned int cq_size = qp->entries_n;
	const unsigned int mask = cq_size - 1;
	uint32_t idx;
	uint32_t next_idx = qp->ci & mask;
	const uint16_t max = RTE_MIN((uint16_t)(qp->pi - qp->ci), nb_ops);
	uint32_t crc32_idx = qp->priv->crc32_opaq_offs;
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
						    (opaq[idx].data[crc32_idx]);
				break;
			case RTE_COMP_CHECKSUM_ADLER32:
				op->output_chksum = (uint64_t)rte_be_to_cpu_32
						(opaq[idx].data[crc32_idx + 1]);
				break;
			case RTE_COMP_CHECKSUM_CRC32_ADLER32:
				op->output_chksum = (uint64_t)rte_be_to_cpu_32
						   (opaq[idx].data[crc32_idx]) |
						     ((uint64_t)rte_be_to_cpu_32
					 (opaq[idx].data[crc32_idx + 1]) << 32);
				break;
			case RTE_COMP_CHECKSUM_XXHASH32:
				op->output_chksum = (uint64_t)rte_be_to_cpu_32
						    (opaq[idx].v2.xxh32);
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

static int
mlx5_compress_args_check_handler(const char *key, const char *val, void *opaque)
{
	struct mlx5_compress_devarg_params *devarg_prms = opaque;

	if (strcmp(key, "log-block-size") == 0) {
		errno = 0;
		devarg_prms->log_block_sz = (uint32_t)strtoul(val, NULL, 10);
		if (errno) {
			DRV_LOG(WARNING, "%s: \"%s\" is an invalid integer.",
				key, val);
			return -errno;
		}
	}
	return 0;
}

static int
mlx5_compress_handle_devargs(struct mlx5_kvargs_ctrl *mkvlist,
			     struct mlx5_compress_devarg_params *devarg_prms,
			     struct mlx5_hca_attr *att)
{
	const char **params = (const char *[]){
		"log-block-size",
		NULL,
	};

	devarg_prms->log_block_sz = MLX5_GGA_COMP_LOG_BLOCK_SIZE_MAX;
	if (mkvlist == NULL)
		return 0;
	if (mlx5_kvargs_process(mkvlist, params,
				mlx5_compress_args_check_handler,
				devarg_prms) != 0) {
		DRV_LOG(ERR, "Devargs handler function Failed.");
		rte_errno = EINVAL;
		return -1;
	}
	if (devarg_prms->log_block_sz > MLX5_GGA_COMP_LOG_BLOCK_SIZE_MAX ||
	    devarg_prms->log_block_sz < att->compress_min_block_size) {
		DRV_LOG(WARNING, "Log block size provided is out of range("
			"%u); default it to %u.",
			devarg_prms->log_block_sz,
			MLX5_GGA_COMP_LOG_BLOCK_SIZE_MAX);
		devarg_prms->log_block_sz = MLX5_GGA_COMP_LOG_BLOCK_SIZE_MAX;
	}
	return 0;
}

static void
mlx5_compress_fill_caps(struct mlx5_compress_priv *priv,
			const struct mlx5_hca_attr *attr)
{
	struct rte_compressdev_capabilities caps[] = {
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
			.algo = RTE_COMP_ALGO_LZ4,
			.comp_feature_flags = RTE_COMP_FF_XXHASH32_CHECKSUM |
					RTE_COMP_FF_SHAREABLE_PRIV_XFORM |
					RTE_COMP_FF_LZ4_BLOCK_INDEPENDENCE,
			.window_size = {.min = 1, .max = 15, .increment = 1},
		},
		RTE_COMP_END_OF_CAPABILITIES_LIST()
	};
	priv->caps[0] = caps[0];
	priv->caps[1] = caps[1];
	if (attr->decomp_lz4_checksum_en || attr->decomp_lz4_no_checksum_en) {
		priv->caps[2] = caps[2];
		if (attr->decomp_lz4_checksum_en)
			priv->caps[2].comp_feature_flags |=
					RTE_COMP_FF_LZ4_BLOCK_WITH_CHECKSUM;
		priv->caps[3] = caps[3];
	} else
		priv->caps[2] = caps[3];
}

static int
mlx5_compress_dev_probe(struct mlx5_common_device *cdev,
			struct mlx5_kvargs_ctrl *mkvlist)
{
	struct rte_compressdev *compressdev;
	struct mlx5_compress_priv *priv;
	struct mlx5_hca_attr *attr = &cdev->config.hca_attr;
	struct mlx5_compress_devarg_params devarg_prms = {0};
	struct rte_compressdev_pmd_init_params init_params = {
		.name = "",
		.socket_id = cdev->dev->numa_node,
	};
	const char *ibdev_name = mlx5_os_get_ctx_device_name(cdev->ctx);
	uint32_t crc32_opaq_offset;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		DRV_LOG(ERR, "Non-primary process type is not supported.");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	if (!attr->decomp_lz4_checksum_en && !attr->decomp_lz4_no_checksum_en &&
	    !attr->decomp_deflate_v1_en && !attr->decomp_deflate_v2_en &&
	    !attr->mmo_decompress_sq_en && !attr->mmo_compress_qp_en &&
	    !attr->mmo_compress_sq_en && !attr->mmo_dma_qp_en &&
	    !attr->mmo_dma_sq_en) {
		DRV_LOG(ERR, "Not enough capabilities to support compress operations, maybe old FW/OFED version?");
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	mlx5_compress_handle_devargs(mkvlist, &devarg_prms, attr);
	compressdev = rte_compressdev_pmd_create(ibdev_name, cdev->dev,
						 sizeof(*priv), &init_params);
	if (compressdev == NULL) {
		DRV_LOG(ERR, "Failed to create device \"%s\".", ibdev_name);
		return -ENODEV;
	}
	DRV_LOG(INFO,
		"Compress device %s was created successfully.", ibdev_name);
	compressdev->dev_ops = &mlx5_compress_ops;
	compressdev->dequeue_burst = mlx5_compress_dequeue_burst;
	compressdev->enqueue_burst = mlx5_compress_enqueue_burst;
	compressdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;
	priv = compressdev->data->dev_private;
	priv->log_block_sz = devarg_prms.log_block_sz;
	if (attr->decomp_deflate_v2_en || attr->decomp_lz4_checksum_en ||
	    attr->decomp_lz4_no_checksum_en)
		crc32_opaq_offset = offsetof(union mlx5_gga_compress_opaque,
					     v2.crc32);
	else
		crc32_opaq_offset = offsetof(union mlx5_gga_compress_opaque,
					     v1.crc32);
	MLX5_ASSERT((crc32_opaq_offset % 4) == 0);
	priv->crc32_opaq_offs = crc32_opaq_offset / 4;
	priv->cdev = cdev;
	priv->compressdev = compressdev;
	mlx5_compress_fill_caps(priv, attr);
	if (mlx5_devx_uar_prepare(cdev, &priv->uar) != 0) {
		rte_compressdev_pmd_destroy(priv->compressdev);
		return -1;
	}
	pthread_mutex_lock(&priv_list_lock);
	TAILQ_INSERT_TAIL(&mlx5_compress_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	return 0;
}

static int
mlx5_compress_dev_remove(struct mlx5_common_device *cdev)
{
	struct mlx5_compress_priv *priv = NULL;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &mlx5_compress_priv_list, next)
		if (priv->compressdev->device == cdev->dev)
			break;
	if (priv)
		TAILQ_REMOVE(&mlx5_compress_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	if (priv) {
		mlx5_devx_uar_release(&priv->uar);
		rte_compressdev_pmd_destroy(priv->compressdev);
	}
	return 0;
}

static const struct rte_pci_id mlx5_compress_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_BLUEFIELD2)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_BLUEFIELD3)
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
