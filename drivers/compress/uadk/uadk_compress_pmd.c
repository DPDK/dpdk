/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024-2025 Huawei Technologies Co.,Ltd. All rights reserved.
 * Copyright 2024-2025 Linaro ltd.
 */

#include <bus_vdev_driver.h>
#include <rte_compressdev_pmd.h>
#include <rte_malloc.h>

#include <uadk/wd_comp.h>
#include <uadk/wd_sched.h>

#include "uadk_compress_pmd_private.h"

#define UADK_COMP_DEF_CTXS    2
static char alg_name[8] = "deflate";

static const struct
rte_compressdev_capabilities uadk_compress_pmd_capabilities[] = {
	{   /* Deflate */
		.algo = RTE_COMP_ALGO_DEFLATE,
		.comp_feature_flags = RTE_COMP_FF_SHAREABLE_PRIV_XFORM |
				      RTE_COMP_FF_HUFFMAN_FIXED |
				      RTE_COMP_FF_HUFFMAN_DYNAMIC,
	},

	RTE_COMP_END_OF_CAPABILITIES_LIST()
};

static int
uadk_compress_pmd_config(struct rte_compressdev *dev,
			 struct rte_compressdev_config *config __rte_unused)
{
	struct uadk_compress_priv *priv = dev->data->dev_private;
	struct wd_ctx_params cparams = {0};
	struct wd_ctx_nums *ctx_set_num;
	int ret;

	if (priv->init)
		return 0;

	ctx_set_num = calloc(WD_DIR_MAX, sizeof(*ctx_set_num));
	if (!ctx_set_num) {
		UADK_LOG(ERR, "failed to alloc ctx_set_size!");
		return -WD_ENOMEM;
	}

	cparams.op_type_num = WD_DIR_MAX;
	cparams.ctx_set_num = ctx_set_num;

	for (int i = 0; i < WD_DIR_MAX; i++)
		ctx_set_num[i].async_ctx_num = UADK_COMP_DEF_CTXS;

	ret = wd_comp_init2_(alg_name, SCHED_POLICY_RR, TASK_MIX, &cparams);
	free(ctx_set_num);

	if (ret) {
		UADK_LOG(ERR, "failed to do comp init2!");
		return ret;
	}

	priv->init = true;

	return 0;
}

static int
uadk_compress_pmd_start(struct rte_compressdev *dev __rte_unused)
{
	return 0;
}

static void
uadk_compress_pmd_stop(struct rte_compressdev *dev __rte_unused)
{
}

static int
uadk_compress_pmd_close(struct rte_compressdev *dev)
{
	struct uadk_compress_priv *priv = dev->data->dev_private;

	if (priv->init) {
		wd_comp_uninit2();
		priv->init = false;
	}

	return 0;
}

static void
uadk_compress_pmd_stats_get(struct rte_compressdev *dev,
			    struct rte_compressdev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct uadk_compress_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;
		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

static void
uadk_compress_pmd_stats_reset(struct rte_compressdev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct uadk_compress_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}

static void
uadk_compress_pmd_info_get(struct rte_compressdev *dev,
			   struct rte_compressdev_info *dev_info)
{
	if (dev_info != NULL) {
		dev_info->driver_name = dev->device->driver->name;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = uadk_compress_pmd_capabilities;
	}
}

static int
uadk_compress_pmd_qp_release(struct rte_compressdev *dev, uint16_t qp_id)
{
	struct uadk_compress_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp != NULL) {
		rte_ring_free(qp->processed_pkts);
		rte_free(qp);
		dev->data->queue_pairs[qp_id] = NULL;
	}

	return 0;
}

static int
uadk_pmd_qp_set_unique_name(struct rte_compressdev *dev,
			    struct uadk_compress_qp *qp)
{
	unsigned int n = snprintf(qp->name, sizeof(qp->name),
				 "uadk_pmd_%u_qp_%u",
				 dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -EINVAL;

	return 0;
}

static struct rte_ring *
uadk_pmd_qp_create_processed_pkts_ring(struct uadk_compress_qp *qp,
				       unsigned int ring_size, int socket_id)
{
	struct rte_ring *r = qp->processed_pkts;

	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			UADK_LOG(INFO, "Reusing existing ring %s for processed packets",
				 qp->name);
			return r;
		}

		UADK_LOG(ERR, "Unable to reuse existing ring %s for processed packets",
			 qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
			       RING_F_EXACT_SZ);
}

static int
uadk_compress_pmd_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
			   uint32_t max_inflight_ops, int socket_id)
{
	struct uadk_compress_qp *qp = NULL;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		uadk_compress_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("uadk PMD Queue Pair", sizeof(*qp),
				RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return (-ENOMEM);

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	if (uadk_pmd_qp_set_unique_name(dev, qp))
		goto qp_setup_cleanup;

	qp->processed_pkts = uadk_pmd_qp_create_processed_pkts_ring(qp,
						max_inflight_ops, socket_id);
	if (qp->processed_pkts == NULL)
		goto qp_setup_cleanup;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));

	return 0;

qp_setup_cleanup:
	if (qp) {
		rte_free(qp);
		qp = NULL;
	}
	return -EINVAL;
}

static int
uadk_compress_pmd_xform_create(struct rte_compressdev *dev __rte_unused,
			       const struct rte_comp_xform *xform,
			       void **private_xform)
{
	struct wd_comp_sess_setup setup = {0};
	struct sched_params param = {0};
	struct uadk_compress_xform *xfrm;
	handle_t handle;

	if (xform == NULL) {
		UADK_LOG(ERR, "invalid xform struct");
		return -EINVAL;
	}

	xfrm = rte_malloc(NULL, sizeof(struct uadk_compress_xform), 0);
	if (xfrm == NULL)
		return -ENOMEM;

	switch (xform->type) {
	case RTE_COMP_COMPRESS:
		switch (xform->compress.algo) {
		case RTE_COMP_ALGO_NULL:
			break;
		case RTE_COMP_ALGO_DEFLATE:
			setup.alg_type = WD_DEFLATE;
			setup.win_sz = WD_COMP_WS_8K;
			setup.comp_lv = WD_COMP_L8;
			setup.op_type = WD_DIR_COMPRESS;
			param.type = setup.op_type;
			param.numa_id = -1;	/* choose nearby numa node */
			setup.sched_param = &param;
			break;
		default:
			goto err;
		}
		break;
	case RTE_COMP_DECOMPRESS:
		switch (xform->decompress.algo) {
		case RTE_COMP_ALGO_NULL:
			break;
		case RTE_COMP_ALGO_DEFLATE:
			setup.alg_type = WD_DEFLATE;
			setup.comp_lv = WD_COMP_L8;
			setup.op_type = WD_DIR_DECOMPRESS;
			param.type = setup.op_type;
			param.numa_id = -1;	/* choose nearby numa node */
			setup.sched_param = &param;
			break;
		default:
			goto err;
		}
		break;
	default:
		UADK_LOG(ERR, "Algorithm %u is not supported.", xform->type);
		goto err;
	}

	handle = wd_comp_alloc_sess(&setup);
	if (!handle)
		goto err;

	xfrm->handle = handle;
	xfrm->type = xform->type;
	*private_xform = xfrm;
	return 0;

err:
	rte_free(xfrm);
	return -EINVAL;
}

static int
uadk_compress_pmd_xform_free(struct rte_compressdev *dev __rte_unused, void *xform)
{
	if (!xform)
		return -EINVAL;

	wd_comp_free_sess(((struct uadk_compress_xform *)xform)->handle);
	rte_free(xform);

	return 0;
}

static struct rte_compressdev_ops uadk_compress_pmd_ops = {
		.dev_configure		= uadk_compress_pmd_config,
		.dev_start		= uadk_compress_pmd_start,
		.dev_stop		= uadk_compress_pmd_stop,
		.dev_close		= uadk_compress_pmd_close,
		.stats_get		= uadk_compress_pmd_stats_get,
		.stats_reset		= uadk_compress_pmd_stats_reset,
		.dev_infos_get		= uadk_compress_pmd_info_get,
		.queue_pair_setup	= uadk_compress_pmd_qp_setup,
		.queue_pair_release	= uadk_compress_pmd_qp_release,
		.private_xform_create	= uadk_compress_pmd_xform_create,
		.private_xform_free	= uadk_compress_pmd_xform_free,
};

static void *uadk_compress_pmd_async_cb(struct wd_comp_req *req,
					void *data __rte_unused)
{
	struct rte_comp_op *op = req->cb_param;
	uint16_t dst_len = rte_pktmbuf_data_len(op->m_dst);

	if (req->dst_len <= dst_len) {
		op->produced += req->dst_len;
		op->status = RTE_COMP_OP_STATUS_SUCCESS;
	} else  {
		op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED;
	}

	return NULL;
}

static uint16_t
uadk_compress_pmd_enqueue_burst_async(void *queue_pair,
				      struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct uadk_compress_qp *qp = queue_pair;
	struct uadk_compress_xform *xform;
	struct rte_comp_op *op;
	uint16_t enqd = 0;
	int i, ret = 0;

	for (i = 0; i < nb_ops; i++) {
		op = ops[i];

		if (op->op_type == RTE_COMP_OP_STATEFUL) {
			op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		} else {
			/* process stateless ops */
			xform = op->private_xform;
			if (xform) {
				struct wd_comp_req req = {0};
				uint16_t dst_len = rte_pktmbuf_data_len(op->m_dst);

				req.src = rte_pktmbuf_mtod(op->m_src, uint8_t *);
				req.src_len = op->src.length;
				req.dst = rte_pktmbuf_mtod(op->m_dst, uint8_t *);
				req.dst_len = dst_len;
				req.op_type = (enum wd_comp_op_type)xform->type;
				req.cb = uadk_compress_pmd_async_cb;
				req.cb_param = op;
				req.data_fmt = WD_FLAT_BUF;
				do {
					ret = wd_do_comp_async(xform->handle, &req);
				} while (ret == -WD_EBUSY);

				op->consumed += req.src_len;

				if (ret) {
					op->status = RTE_COMP_OP_STATUS_ERROR;
					break;
				}
			} else {
				op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
			}
		}

		/* Whatever is out of op, put it into completion queue with
		 * its status
		 */
		if (!ret)
			ret = rte_ring_enqueue(qp->processed_pkts, (void *)op);

		if (unlikely(ret)) {
			/* increment count if failed to enqueue op */
			qp->qp_stats.enqueue_err_count++;
		} else {
			qp->qp_stats.enqueued_count++;
			enqd++;
		}
	}

	return enqd;
}

static uint16_t
uadk_compress_pmd_dequeue_burst_async(void *queue_pair,
				      struct rte_comp_op **ops,
				      uint16_t nb_ops)
{
	struct uadk_compress_qp *qp = queue_pair;
	unsigned int nb_dequeued = 0;
	unsigned int recv = 0;
	int ret;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_pkts,
			(void **)ops, nb_ops, NULL);
	if (nb_dequeued == 0)
		return 0;

	do {
		ret = wd_comp_poll(nb_dequeued, &recv);
	} while (ret == -WD_EAGAIN);

	qp->qp_stats.dequeued_count += nb_dequeued;

	return recv;
}

static int
uadk_compress_probe(struct rte_vdev_device *vdev)
{
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id(),
	};
	struct rte_compressdev *compressdev;
	struct uacce_dev *udev;
	const char *name;

	udev = wd_get_accel_dev(alg_name);
	if (!udev)
		return -ENODEV;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	compressdev = rte_compressdev_pmd_create(name, &vdev->device,
			sizeof(struct uadk_compress_priv), &init_params);
	if (compressdev == NULL) {
		UADK_LOG(ERR, "driver %s: create failed", init_params.name);
		return -ENODEV;
	}

	compressdev->dev_ops = &uadk_compress_pmd_ops;
	compressdev->dequeue_burst = uadk_compress_pmd_dequeue_burst_async;
	compressdev->enqueue_burst = uadk_compress_pmd_enqueue_burst_async;
	compressdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;

	return 0;
}

static int
uadk_compress_remove(struct rte_vdev_device *vdev)
{
	struct rte_compressdev *compressdev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	compressdev = rte_compressdev_pmd_get_named_dev(name);
	if (compressdev == NULL)
		return -ENODEV;

	return rte_compressdev_pmd_destroy(compressdev);
}

static struct rte_vdev_driver uadk_compress_pmd = {
	.probe = uadk_compress_probe,
	.remove = uadk_compress_remove,
};

#define UADK_COMPRESS_DRIVER_NAME compress_uadk
RTE_PMD_REGISTER_VDEV(UADK_COMPRESS_DRIVER_NAME, uadk_compress_pmd);
RTE_LOG_REGISTER_DEFAULT(uadk_compress_logtype, INFO);
