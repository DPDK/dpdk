/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022-2023 Huawei Technologies Co.,Ltd. All rights reserved.
 * Copyright 2022-2023 Linaro ltd.
 */

#include <stdlib.h>

#include <bus_vdev_driver.h>
#include <cryptodev_pmd.h>
#include <rte_bus_vdev.h>

#include <uadk/wd_cipher.h>
#include <uadk/wd_digest.h>
#include <uadk/wd_sched.h>

#include "uadk_crypto_pmd_private.h"

static uint8_t uadk_cryptodev_driver_id;

static const struct rte_cryptodev_capabilities uadk_crypto_v2_capabilities[] = {
	/* End of capabilities */
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

/* Configure device */
static int
uadk_crypto_pmd_config(struct rte_cryptodev *dev __rte_unused,
		       struct rte_cryptodev_config *config __rte_unused)
{
	return 0;
}

/* Start device */
static int
uadk_crypto_pmd_start(struct rte_cryptodev *dev __rte_unused)
{
	return 0;
}

/* Stop device */
static void
uadk_crypto_pmd_stop(struct rte_cryptodev *dev __rte_unused)
{
}

/* Close device */
static int
uadk_crypto_pmd_close(struct rte_cryptodev *dev __rte_unused)
{
	return 0;
}

/* Get device statistics */
static void
uadk_crypto_pmd_stats_get(struct rte_cryptodev *dev,
			  struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct uadk_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;
		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/* Reset device statistics */
static void
uadk_crypto_pmd_stats_reset(struct rte_cryptodev *dev __rte_unused)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct uadk_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}

/* Get device info */
static void
uadk_crypto_pmd_info_get(struct rte_cryptodev *dev,
			 struct rte_cryptodev_info *dev_info)
{
	struct uadk_crypto_priv *priv = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->driver_name = dev->device->driver->name;
		dev_info->max_nb_queue_pairs = 128;
		/* No limit of number of sessions */
		dev_info->sym.max_nb_sessions = 0;
		dev_info->feature_flags = dev->feature_flags;

		if (priv->version == UADK_CRYPTO_V2)
			dev_info->capabilities = uadk_crypto_v2_capabilities;
	}
}

/* Release queue pair */
static int
uadk_crypto_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct uadk_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp) {
		rte_ring_free(qp->processed_pkts);
		rte_free(qp);
		dev->data->queue_pairs[qp_id] = NULL;
	}

	return 0;
}

/* set a unique name for the queue pair based on its name, dev_id and qp_id */
static int
uadk_pmd_qp_set_unique_name(struct rte_cryptodev *dev,
			    struct uadk_qp *qp)
{
	unsigned int n = snprintf(qp->name, sizeof(qp->name),
				  "uadk_crypto_pmd_%u_qp_%u",
				  dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -EINVAL;

	return 0;
}

/* Create a ring to place process packets on */
static struct rte_ring *
uadk_pmd_qp_create_processed_pkts_ring(struct uadk_qp *qp,
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
uadk_crypto_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
			 const struct rte_cryptodev_qp_conf *qp_conf,
			 int socket_id)
{
	struct uadk_qp *qp;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		uadk_crypto_pmd_qp_release(dev, qp_id);

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
				qp_conf->nb_descriptors, socket_id);
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

static struct rte_cryptodev_ops uadk_crypto_pmd_ops = {
		.dev_configure		= uadk_crypto_pmd_config,
		.dev_start		= uadk_crypto_pmd_start,
		.dev_stop		= uadk_crypto_pmd_stop,
		.dev_close		= uadk_crypto_pmd_close,
		.stats_get		= uadk_crypto_pmd_stats_get,
		.stats_reset		= uadk_crypto_pmd_stats_reset,
		.dev_infos_get		= uadk_crypto_pmd_info_get,
		.queue_pair_setup	= uadk_crypto_pmd_qp_setup,
		.queue_pair_release	= uadk_crypto_pmd_qp_release,
		.sym_session_get_size	= NULL,
		.sym_session_configure	= NULL,
		.sym_session_clear	= NULL,
};

static uint16_t
uadk_crypto_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
			  uint16_t nb_ops)
{
	struct uadk_qp *qp = queue_pair;
	struct rte_crypto_op *op;
	uint16_t enqd = 0;
	int i, ret;

	for (i = 0; i < nb_ops; i++) {
		op = ops[i];
		op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

		if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
			op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

		if (op->status != RTE_CRYPTO_OP_STATUS_ERROR) {
			ret = rte_ring_enqueue(qp->processed_pkts, (void *)op);
			if (ret < 0)
				goto enqueue_err;
			qp->qp_stats.enqueued_count++;
			enqd++;
		} else {
			/* increment count if failed to enqueue op */
			qp->qp_stats.enqueue_err_count++;
		}
	}

	return enqd;

enqueue_err:
	qp->qp_stats.enqueue_err_count++;
	return enqd;
}

static uint16_t
uadk_crypto_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
			  uint16_t nb_ops)
{
	struct uadk_qp *qp = queue_pair;
	unsigned int nb_dequeued;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_pkts,
			(void **)ops, nb_ops, NULL);
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

static int
uadk_cryptodev_probe(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.private_data_size = sizeof(struct uadk_crypto_priv),
		.max_nb_queue_pairs =
				RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS,
	};
	enum uadk_crypto_version version = UADK_CRYPTO_V2;
	struct uadk_crypto_priv *priv;
	struct rte_cryptodev *dev;
	struct uacce_dev *udev;
	const char *name;

	udev = wd_get_accel_dev("cipher");
	if (!udev)
		return -ENODEV;

	if (!strcmp(udev->api, "hisi_qm_v2"))
		version = UADK_CRYPTO_V2;

	free(udev);

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	dev = rte_cryptodev_pmd_create(name, &vdev->device, &init_params);
	if (dev == NULL) {
		UADK_LOG(ERR, "driver %s: create failed", init_params.name);
		return -ENODEV;
	}

	dev->dev_ops = &uadk_crypto_pmd_ops;
	dev->driver_id = uadk_cryptodev_driver_id;
	dev->dequeue_burst = uadk_crypto_dequeue_burst;
	dev->enqueue_burst = uadk_crypto_enqueue_burst;
	dev->feature_flags = RTE_CRYPTODEV_FF_HW_ACCELERATED;
	priv = dev->data->dev_private;
	priv->version = version;

	rte_cryptodev_pmd_probing_finish(dev);

	return 0;
}

static int
uadk_cryptodev_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_vdev_driver uadk_crypto_pmd = {
	.probe       = uadk_cryptodev_probe,
	.remove      = uadk_cryptodev_remove,
};

static struct cryptodev_driver uadk_crypto_drv;

#define UADK_CRYPTO_DRIVER_NAME crypto_uadk
RTE_PMD_REGISTER_VDEV(UADK_CRYPTO_DRIVER_NAME, uadk_crypto_pmd);
RTE_PMD_REGISTER_CRYPTO_DRIVER(uadk_crypto_drv, uadk_crypto_pmd.driver,
			       uadk_cryptodev_driver_id);
RTE_LOG_REGISTER_DEFAULT(uadk_crypto_logtype, INFO);
