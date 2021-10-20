/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <cryptodev_pmd.h>

#include "bcmfs_device.h"
#include "bcmfs_logs.h"
#include "bcmfs_qp.h"
#include "bcmfs_sym_pmd.h"
#include "bcmfs_sym_req.h"
#include "bcmfs_sym_session.h"
#include "bcmfs_sym_capabilities.h"

uint8_t cryptodev_bcmfs_driver_id;

static int bcmfs_sym_qp_release(struct rte_cryptodev *dev,
				uint16_t queue_pair_id);

static int
bcmfs_sym_dev_config(__rte_unused struct rte_cryptodev *dev,
		     __rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

static int
bcmfs_sym_dev_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

static void
bcmfs_sym_dev_stop(__rte_unused struct rte_cryptodev *dev)
{
}

static int
bcmfs_sym_dev_close(struct rte_cryptodev *dev)
{
	int i, ret;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = bcmfs_sym_qp_release(dev, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void
bcmfs_sym_dev_info_get(struct rte_cryptodev *dev,
		       struct rte_cryptodev_info *dev_info)
{
	struct bcmfs_sym_dev_private *internals = dev->data->dev_private;
	struct bcmfs_device *fsdev = internals->fsdev;

	if (dev_info != NULL) {
		dev_info->driver_id = cryptodev_bcmfs_driver_id;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->max_nb_queue_pairs = fsdev->max_hw_qps;
		/* No limit of number of sessions */
		dev_info->sym.max_nb_sessions = 0;
		dev_info->capabilities = bcmfs_sym_get_capabilities();
	}
}

static void
bcmfs_sym_stats_get(struct rte_cryptodev *dev,
		    struct rte_cryptodev_stats *stats)
{
	struct bcmfs_qp_stats bcmfs_stats = {0};
	struct bcmfs_sym_dev_private *bcmfs_priv;
	struct bcmfs_device *fsdev;

	if (stats == NULL || dev == NULL) {
		BCMFS_LOG(ERR, "invalid ptr: stats %p, dev %p", stats, dev);
		return;
	}
	bcmfs_priv = dev->data->dev_private;
	fsdev = bcmfs_priv->fsdev;

	bcmfs_qp_stats_get(fsdev->qps_in_use, fsdev->max_hw_qps, &bcmfs_stats);

	stats->enqueued_count = bcmfs_stats.enqueued_count;
	stats->dequeued_count = bcmfs_stats.dequeued_count;
	stats->enqueue_err_count = bcmfs_stats.enqueue_err_count;
	stats->dequeue_err_count = bcmfs_stats.dequeue_err_count;
}

static void
bcmfs_sym_stats_reset(struct rte_cryptodev *dev)
{
	struct bcmfs_sym_dev_private *bcmfs_priv;
	struct bcmfs_device *fsdev;

	if (dev == NULL) {
		BCMFS_LOG(ERR, "invalid cryptodev ptr %p", dev);
		return;
	}
	bcmfs_priv = dev->data->dev_private;
	fsdev = bcmfs_priv->fsdev;

	bcmfs_qp_stats_reset(fsdev->qps_in_use, fsdev->max_hw_qps);
}

static int
bcmfs_sym_qp_release(struct rte_cryptodev *dev, uint16_t queue_pair_id)
{
	struct bcmfs_sym_dev_private *bcmfs_private = dev->data->dev_private;
	struct bcmfs_qp *qp = (struct bcmfs_qp *)
			      (dev->data->queue_pairs[queue_pair_id]);

	BCMFS_LOG(DEBUG, "Release sym qp %u on device %d",
		  queue_pair_id, dev->data->dev_id);

	rte_mempool_free(qp->sr_mp);

	bcmfs_private->fsdev->qps_in_use[queue_pair_id] = NULL;

	return bcmfs_qp_release((struct bcmfs_qp **)
				&dev->data->queue_pairs[queue_pair_id]);
}

static void
spu_req_init(struct bcmfs_sym_request *sr, rte_iova_t iova __rte_unused)
{
	memset(sr, 0, sizeof(*sr));
	sr->fptr = iova + offsetof(struct bcmfs_sym_request, fmd);
	sr->optr = iova + offsetof(struct bcmfs_sym_request, omd);
	sr->dptr = iova + offsetof(struct bcmfs_sym_request, digest);
	sr->rptr = iova + offsetof(struct bcmfs_sym_request, resp);
}

static void
req_pool_obj_init(__rte_unused struct rte_mempool *mp,
		  __rte_unused void *opaque, void *obj,
		  __rte_unused unsigned int obj_idx)
{
	spu_req_init(obj, rte_mempool_virt2iova(obj));
}

static struct rte_mempool *
bcmfs_sym_req_pool_create(struct rte_cryptodev *cdev __rte_unused,
			  uint32_t nobjs, uint16_t qp_id,
			  int socket_id)
{
	char softreq_pool_name[RTE_RING_NAMESIZE];
	struct rte_mempool *mp;

	snprintf(softreq_pool_name, RTE_RING_NAMESIZE, "%s_%d",
		 "bcm_sym", qp_id);

	mp = rte_mempool_create(softreq_pool_name,
				RTE_ALIGN_MUL_CEIL(nobjs, 64),
				sizeof(struct bcmfs_sym_request),
				64, 0, NULL, NULL, req_pool_obj_init, NULL,
				socket_id, 0);
	if (mp == NULL)
		BCMFS_LOG(ERR, "Failed to create req pool, qid %d, err %d",
				qp_id, rte_errno);

	return mp;
}

static int
bcmfs_sym_qp_setup(struct rte_cryptodev *cdev, uint16_t qp_id,
		   const struct rte_cryptodev_qp_conf *qp_conf,
		   int socket_id)
{
	int ret = 0;
	struct bcmfs_qp *qp = NULL;
	struct bcmfs_qp_config bcmfs_qp_conf;

	struct bcmfs_qp **qp_addr =
			(struct bcmfs_qp **)&cdev->data->queue_pairs[qp_id];
	struct bcmfs_sym_dev_private *bcmfs_private = cdev->data->dev_private;
	struct bcmfs_device *fsdev = bcmfs_private->fsdev;


	/* If qp is already in use free ring memory and qp metadata. */
	if (*qp_addr != NULL) {
		ret = bcmfs_sym_qp_release(cdev, qp_id);
		if (ret < 0)
			return ret;
	}

	if (qp_id >= fsdev->max_hw_qps) {
		BCMFS_LOG(ERR, "qp_id %u invalid for this device", qp_id);
		return -EINVAL;
	}

	bcmfs_qp_conf.nb_descriptors = qp_conf->nb_descriptors;
	bcmfs_qp_conf.socket_id = socket_id;
	bcmfs_qp_conf.max_descs_req = BCMFS_CRYPTO_MAX_HW_DESCS_PER_REQ;
	bcmfs_qp_conf.iobase = BCMFS_QP_IOBASE_XLATE(fsdev->mmap_addr, qp_id);
	bcmfs_qp_conf.ops = fsdev->sym_hw_qp_ops;

	ret = bcmfs_qp_setup(qp_addr, qp_id, &bcmfs_qp_conf);
	if (ret != 0)
		return ret;

	qp = (struct bcmfs_qp *)*qp_addr;

	qp->sr_mp = bcmfs_sym_req_pool_create(cdev, qp_conf->nb_descriptors,
					      qp_id, socket_id);
	if (qp->sr_mp == NULL)
		return -ENOMEM;

	/* store a link to the qp in the bcmfs_device */
	bcmfs_private->fsdev->qps_in_use[qp_id] = *qp_addr;

	cdev->data->queue_pairs[qp_id] = qp;
	BCMFS_LOG(NOTICE, "queue %d setup done\n", qp_id);

	return 0;
}

static struct rte_cryptodev_ops crypto_bcmfs_ops = {
	/* Device related operations */
	.dev_configure          = bcmfs_sym_dev_config,
	.dev_start              = bcmfs_sym_dev_start,
	.dev_stop               = bcmfs_sym_dev_stop,
	.dev_close              = bcmfs_sym_dev_close,
	.dev_infos_get          = bcmfs_sym_dev_info_get,
	/* Stats Collection */
	.stats_get              = bcmfs_sym_stats_get,
	.stats_reset            = bcmfs_sym_stats_reset,
	/* Queue-Pair management */
	.queue_pair_setup       = bcmfs_sym_qp_setup,
	.queue_pair_release     = bcmfs_sym_qp_release,
	/* Crypto session related operations */
	.sym_session_get_size   = bcmfs_sym_session_get_private_size,
	.sym_session_configure  = bcmfs_sym_session_configure,
	.sym_session_clear      = bcmfs_sym_session_clear
};

/** Enqueue burst */
static uint16_t
bcmfs_sym_pmd_enqueue_op_burst(void *queue_pair,
			       struct rte_crypto_op **ops,
			       uint16_t nb_ops)
{
	int i, j;
	int retval;
	uint16_t enq = 0;
	struct bcmfs_sym_request *sreq;
	struct bcmfs_sym_session *sess;
	struct bcmfs_qp *qp = (struct bcmfs_qp *)queue_pair;

	if (nb_ops == 0)
		return 0;

	if (nb_ops > BCMFS_MAX_REQS_BUFF)
		nb_ops = BCMFS_MAX_REQS_BUFF;

	 /* We do not process more than available space */
	if (nb_ops >  (qp->nb_descriptors - qp->nb_pending_requests))
		nb_ops = qp->nb_descriptors - qp->nb_pending_requests;

	for (i = 0; i < nb_ops; i++) {
		sess = bcmfs_sym_get_session(ops[i]);
		if (unlikely(sess == NULL))
			goto enqueue_err;

		if (rte_mempool_get(qp->sr_mp, (void **)&sreq))
			goto enqueue_err;

		/* save rte_crypto_op */
		sreq->op = ops[i];

		/* save context */
		qp->infl_msgs[i] = &sreq->msgs;
		qp->infl_msgs[i]->ctx = (void *)sreq;

		/* pre process the request crypto h/w acceleration */
		retval = bcmfs_process_sym_crypto_op(ops[i], sess, sreq);
		if (unlikely(retval < 0))
			goto enqueue_err;
	}
	/* Send burst request to hw QP */
	enq = bcmfs_enqueue_op_burst(qp, (void **)qp->infl_msgs, i);

	for (j = enq; j < i; j++)
		rte_mempool_put(qp->sr_mp, qp->infl_msgs[j]->ctx);

	return enq;

enqueue_err:
	for (j = 0; j < i; j++)
		rte_mempool_put(qp->sr_mp, qp->infl_msgs[j]->ctx);

	return enq;
}

static void bcmfs_sym_set_request_status(struct rte_crypto_op *op,
					 struct bcmfs_sym_request *out)
{
	if (*out->resp == BCMFS_SYM_RESPONSE_SUCCESS)
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	else if (*out->resp == BCMFS_SYM_RESPONSE_HASH_TAG_ERROR)
		op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	else
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

static uint16_t
bcmfs_sym_pmd_dequeue_op_burst(void *queue_pair,
			       struct rte_crypto_op **ops,
			       uint16_t nb_ops)
{
	int i;
	uint16_t deq = 0;
	unsigned int pkts = 0;
	struct bcmfs_sym_request *sreq;
	struct bcmfs_qp *qp = queue_pair;

	if (nb_ops > BCMFS_MAX_REQS_BUFF)
		nb_ops = BCMFS_MAX_REQS_BUFF;

	deq = bcmfs_dequeue_op_burst(qp, (void **)qp->infl_msgs, nb_ops);
	/* get rte_crypto_ops */
	for (i = 0; i < deq; i++) {
		sreq = (struct bcmfs_sym_request *)qp->infl_msgs[i]->ctx;

		/* set the status based on the response from the crypto h/w */
		bcmfs_sym_set_request_status(sreq->op, sreq);

		ops[pkts++] = sreq->op;

		rte_mempool_put(qp->sr_mp, sreq);
	}

	return pkts;
}

/*
 * An rte_driver is needed in the registration of both the
 * device and the driver with cryptodev.
 */
static const char bcmfs_sym_drv_name[] = RTE_STR(CRYPTODEV_NAME_BCMFS_SYM_PMD);
static const struct rte_driver cryptodev_bcmfs_sym_driver = {
	.name = bcmfs_sym_drv_name,
	.alias = bcmfs_sym_drv_name
};

int
bcmfs_sym_dev_create(struct bcmfs_device *fsdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = rte_socket_id(),
		.private_data_size = sizeof(struct bcmfs_sym_dev_private)
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *cryptodev;
	struct bcmfs_sym_dev_private *internals;

	snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN, "%s_%s",
		 fsdev->name, "sym");

	/* Populate subset device to use in cryptodev device creation */
	fsdev->sym_rte_dev.driver = &cryptodev_bcmfs_sym_driver;
	fsdev->sym_rte_dev.numa_node = 0;
	fsdev->sym_rte_dev.devargs = NULL;

	cryptodev = rte_cryptodev_pmd_create(name,
					     &fsdev->sym_rte_dev,
					     &init_params);
	if (cryptodev == NULL)
		return -ENODEV;

	fsdev->sym_rte_dev.name = cryptodev->data->name;
	cryptodev->driver_id = cryptodev_bcmfs_driver_id;
	cryptodev->dev_ops = &crypto_bcmfs_ops;

	cryptodev->enqueue_burst = bcmfs_sym_pmd_enqueue_op_burst;
	cryptodev->dequeue_burst = bcmfs_sym_pmd_dequeue_op_burst;

	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
				   RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
				   RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;

	internals = cryptodev->data->dev_private;
	internals->fsdev = fsdev;
	fsdev->sym_dev = internals;

	internals->sym_dev_id = cryptodev->data->dev_id;
	internals->fsdev_capabilities = bcmfs_sym_get_capabilities();

	rte_cryptodev_pmd_probing_finish(cryptodev);

	BCMFS_LOG(DEBUG, "Created bcmfs-sym device %s as cryptodev instance %d",
		  cryptodev->data->name, internals->sym_dev_id);
	return 0;
}

int
bcmfs_sym_dev_destroy(struct bcmfs_device *fsdev)
{
	struct rte_cryptodev *cryptodev;

	if (fsdev == NULL)
		return -ENODEV;
	if (fsdev->sym_dev == NULL)
		return 0;

	/* free crypto device */
	cryptodev = rte_cryptodev_pmd_get_dev(fsdev->sym_dev->sym_dev_id);
	rte_cryptodev_pmd_destroy(cryptodev);
	fsdev->sym_rte_dev.name = NULL;
	fsdev->sym_dev = NULL;

	return 0;
}

static struct cryptodev_driver bcmfs_crypto_drv;
RTE_PMD_REGISTER_CRYPTO_DRIVER(bcmfs_crypto_drv,
			       cryptodev_bcmfs_sym_driver,
			       cryptodev_bcmfs_driver_id);
