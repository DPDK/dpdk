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
	{	/* AES ECB */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_ECB,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* AES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_CBC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* AES XTS */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_XTS,
				.block_size = 1,
				.key_size = {
					.min = 32,
					.max = 64,
					.increment = 32
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* DES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_DES_CBC,
				.block_size = 8,
				.key_size = {
					.min = 8,
					.max = 8,
					.increment = 0
				},
				.iv_size = {
					.min = 8,
					.max = 8,
					.increment = 0
				}
			}, }
		}, }
	},
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
uadk_crypto_pmd_close(struct rte_cryptodev *dev)
{
	struct uadk_crypto_priv *priv = dev->data->dev_private;

	if (priv->env_cipher_init) {
		wd_cipher_env_uninit();
		priv->env_cipher_init = false;
	}

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

static unsigned int
uadk_crypto_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct uadk_crypto_session);
}

static enum uadk_chain_order
uadk_get_chain_order(const struct rte_crypto_sym_xform *xform)
{
	enum uadk_chain_order res = UADK_CHAIN_NOT_SUPPORTED;

	if (xform != NULL) {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
			if (xform->next == NULL)
				res = UADK_CHAIN_ONLY_CIPHER;
		}
	}

	return res;
}

static int
uadk_set_session_cipher_parameters(struct rte_cryptodev *dev,
				   struct uadk_crypto_session *sess,
				   struct rte_crypto_sym_xform *xform)
{
	struct uadk_crypto_priv *priv = dev->data->dev_private;
	struct rte_crypto_cipher_xform *cipher = &xform->cipher;
	struct wd_cipher_sess_setup setup = {0};
	struct sched_params params = {0};
	int ret;

	if (!priv->env_cipher_init) {
		ret = wd_cipher_env_init(NULL);
		if (ret < 0)
			return -EINVAL;
		priv->env_cipher_init = true;
	}

	sess->cipher.direction = cipher->op;
	sess->iv.offset = cipher->iv.offset;
	sess->iv.length = cipher->iv.length;

	switch (cipher->algo) {
	/* Cover supported cipher algorithms */
	case RTE_CRYPTO_CIPHER_AES_CTR:
		setup.alg = WD_CIPHER_AES;
		setup.mode = WD_CIPHER_CTR;
		sess->cipher.req.out_bytes = 64;
		break;
	case RTE_CRYPTO_CIPHER_AES_ECB:
		setup.alg = WD_CIPHER_AES;
		setup.mode = WD_CIPHER_ECB;
		sess->cipher.req.out_bytes = 16;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		setup.alg = WD_CIPHER_AES;
		setup.mode = WD_CIPHER_CBC;
		if (cipher->key.length == 16)
			sess->cipher.req.out_bytes = 16;
		else
			sess->cipher.req.out_bytes = 64;
		break;
	case RTE_CRYPTO_CIPHER_AES_XTS:
		setup.alg = WD_CIPHER_AES;
		setup.mode = WD_CIPHER_XTS;
		if (cipher->key.length == 16)
			sess->cipher.req.out_bytes = 32;
		else
			sess->cipher.req.out_bytes = 512;
		break;
	default:
		ret = -ENOTSUP;
		goto env_uninit;
	}

	params.numa_id = -1;	/* choose nearby numa node */
	setup.sched_param = &params;
	sess->handle_cipher = wd_cipher_alloc_sess(&setup);
	if (!sess->handle_cipher) {
		UADK_LOG(ERR, "uadk failed to alloc session!\n");
		ret = -EINVAL;
		goto env_uninit;
	}

	ret = wd_cipher_set_key(sess->handle_cipher, cipher->key.data, cipher->key.length);
	if (ret) {
		wd_cipher_free_sess(sess->handle_cipher);
		UADK_LOG(ERR, "uadk failed to set key!\n");
		ret = -EINVAL;
		goto env_uninit;
	}

	return 0;

env_uninit:
	wd_cipher_env_uninit();
	priv->env_cipher_init = false;
	return ret;
}

static int
uadk_crypto_sym_session_configure(struct rte_cryptodev *dev,
				  struct rte_crypto_sym_xform *xform,
				  struct rte_cryptodev_sym_session *session)
{
	struct rte_crypto_sym_xform *cipher_xform = NULL;
	struct uadk_crypto_session *sess = CRYPTODEV_GET_SYM_SESS_PRIV(session);
	int ret;

	if (unlikely(!sess)) {
		UADK_LOG(ERR, "Session not available");
		return -EINVAL;
	}

	sess->chain_order = uadk_get_chain_order(xform);
	switch (sess->chain_order) {
	case UADK_CHAIN_ONLY_CIPHER:
		cipher_xform = xform;
		break;
	default:
		return -ENOTSUP;
	}

	if (cipher_xform) {
		ret = uadk_set_session_cipher_parameters(dev, sess, cipher_xform);
		if (ret != 0) {
			UADK_LOG(ERR,
				"Invalid/unsupported cipher parameters");
			return ret;
		}
	}

	return 0;
}

static void
uadk_crypto_sym_session_clear(struct rte_cryptodev *dev __rte_unused,
			      struct rte_cryptodev_sym_session *session)
{
	struct uadk_crypto_session *sess = CRYPTODEV_GET_SYM_SESS_PRIV(session);

	if (unlikely(sess == NULL)) {
		UADK_LOG(ERR, "Session not available");
		return;
	}

	if (sess->handle_cipher) {
		wd_cipher_free_sess(sess->handle_cipher);
		sess->handle_cipher = 0;
	}
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
		.sym_session_get_size	= uadk_crypto_sym_session_get_size,
		.sym_session_configure	= uadk_crypto_sym_session_configure,
		.sym_session_clear	= uadk_crypto_sym_session_clear,
};

static void
uadk_process_cipher_op(struct rte_crypto_op *op,
		       struct uadk_crypto_session *sess,
		       struct rte_mbuf *msrc, struct rte_mbuf *mdst)
{
	uint32_t off = op->sym->cipher.data.offset;
	int ret;

	if (!sess) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return;
	}

	sess->cipher.req.src = rte_pktmbuf_mtod_offset(msrc, uint8_t *, off);
	sess->cipher.req.in_bytes = op->sym->cipher.data.length;
	sess->cipher.req.dst = rte_pktmbuf_mtod_offset(mdst, uint8_t *, off);
	sess->cipher.req.out_buf_bytes = sess->cipher.req.in_bytes;
	sess->cipher.req.iv_bytes = sess->iv.length;
	sess->cipher.req.iv = rte_crypto_op_ctod_offset(op, uint8_t *,
							sess->iv.offset);
	if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		sess->cipher.req.op_type = WD_CIPHER_ENCRYPTION;
	else
		sess->cipher.req.op_type = WD_CIPHER_DECRYPTION;

	do {
		ret = wd_do_cipher_sync(sess->handle_cipher, &sess->cipher.req);
	} while (ret == -WD_EBUSY);

	if (ret)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

static uint16_t
uadk_crypto_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
			  uint16_t nb_ops)
{
	struct uadk_qp *qp = queue_pair;
	struct uadk_crypto_session *sess = NULL;
	struct rte_mbuf *msrc, *mdst;
	struct rte_crypto_op *op;
	uint16_t enqd = 0;
	int i, ret;

	for (i = 0; i < nb_ops; i++) {
		op = ops[i];
		op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		msrc = op->sym->m_src;
		mdst = op->sym->m_dst ? op->sym->m_dst : op->sym->m_src;

		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			if (likely(op->sym->session != NULL))
				sess = CRYPTODEV_GET_SYM_SESS_PRIV(
					op->sym->session);
		}

		switch (sess->chain_order) {
		case UADK_CHAIN_ONLY_CIPHER:
			uadk_process_cipher_op(op, sess, msrc, mdst);
			break;
		default:
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			break;
		}

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
	dev->feature_flags = RTE_CRYPTODEV_FF_HW_ACCELERATED |
			     RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO;
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
