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

#define MAX_ALG_NAME 64
#define UADK_CIPHER_DEF_CTXS    2

static uint8_t uadk_cryptodev_driver_id;

static const struct rte_cryptodev_capabilities uadk_crypto_v2_capabilities[] = {
	{	/* MD5 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_MD5_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 16,
					.increment = 1
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* MD5 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_MD5,
				.block_size = 64,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA1 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 20,
					.increment = 1
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA1 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA1,
				.block_size = 64,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 20,
					.max = 20,
					.increment = 0
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA224 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA224_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 28,
					.increment = 1
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA224 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA224,
				.block_size = 64,
					.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 1,
					.max = 28,
					.increment = 1
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA256 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA256_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 32,
					.increment = 1
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA256 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA256,
				.block_size = 64,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 32,
					.max = 32,
					.increment = 0
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA384 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 1,
					.max = 128,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 48,
					.increment = 1
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA384 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384,
				.block_size = 128,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 48,
					.max = 48,
					.increment = 0
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA512 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA512_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 1,
					.max = 128,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* SHA512 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA512,
				.block_size = 128,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 64,
					.max = 64,
					.increment = 0
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
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
		       struct rte_cryptodev_config *config)
{
	struct uadk_crypto_priv *priv = dev->data->dev_private;

	if (config->nb_queue_pairs != 0)
		priv->nb_qpairs = config->nb_queue_pairs;

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

	if (priv->cipher_init) {
		wd_cipher_uninit2();
		priv->cipher_init = false;
	}

	if (priv->auth_init) {
		wd_digest_uninit2();
		priv->auth_init = false;
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
		dev_info->max_nb_queue_pairs = priv->max_nb_qpairs;
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
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
			if (xform->next == NULL)
				res = UADK_CHAIN_ONLY_AUTH;
			else if (xform->next->type ==
					RTE_CRYPTO_SYM_XFORM_CIPHER)
				res = UADK_CHAIN_AUTH_CIPHER;
		}

		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
			if (xform->next == NULL)
				res = UADK_CHAIN_ONLY_CIPHER;
			else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
				res = UADK_CHAIN_CIPHER_AUTH;
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
	struct wd_ctx_params cparams = {0};
	struct wd_ctx_nums *ctx_set_num;
	char alg_name[MAX_ALG_NAME];
	int ret;

	sess->cipher.direction = cipher->op;
	sess->iv.offset = cipher->iv.offset;
	sess->iv.length = cipher->iv.length;

	switch (cipher->algo) {
	/* Cover supported cipher algorithms */
	case RTE_CRYPTO_CIPHER_AES_CTR:
		setup.alg = WD_CIPHER_AES;
		setup.mode = WD_CIPHER_CTR;
		sess->cipher.req.out_bytes = 64;
		rte_strscpy(alg_name, "ctr(aes)", sizeof(alg_name));
		break;
	case RTE_CRYPTO_CIPHER_AES_ECB:
		setup.alg = WD_CIPHER_AES;
		setup.mode = WD_CIPHER_ECB;
		sess->cipher.req.out_bytes = 16;
		rte_strscpy(alg_name, "ecb(aes)", sizeof(alg_name));
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		setup.alg = WD_CIPHER_AES;
		setup.mode = WD_CIPHER_CBC;
		rte_strscpy(alg_name, "cbc(aes)", sizeof(alg_name));
		if (cipher->key.length == 16)
			sess->cipher.req.out_bytes = 16;
		else
			sess->cipher.req.out_bytes = 64;
		break;
	case RTE_CRYPTO_CIPHER_AES_XTS:
		setup.alg = WD_CIPHER_AES;
		setup.mode = WD_CIPHER_XTS;
		rte_strscpy(alg_name, "xts(aes)", sizeof(alg_name));
		if (cipher->key.length == 16)
			sess->cipher.req.out_bytes = 32;
		else
			sess->cipher.req.out_bytes = 512;
		break;
	default:
		return -ENOTSUP;
	}

	if (!priv->cipher_init) {
		ctx_set_num = calloc(1, sizeof(*ctx_set_num));
		if (!ctx_set_num) {
			UADK_LOG(ERR, "failed to alloc ctx_set_size!");
			return -WD_ENOMEM;
		}

		cparams.op_type_num = 1;
		cparams.ctx_set_num = ctx_set_num;
		ctx_set_num->sync_ctx_num = priv->nb_qpairs;
		ctx_set_num->async_ctx_num = priv->nb_qpairs;

		ret = wd_cipher_init2_(alg_name, SCHED_POLICY_RR, TASK_MIX, &cparams);
		free(ctx_set_num);

		if (ret) {
			UADK_LOG(ERR, "failed to do cipher init2!");
			return ret;
		}
		priv->cipher_init = true;
	}

	params.numa_id = -1;	/* choose nearby numa node */
	setup.sched_param = &params;
	sess->handle_cipher = wd_cipher_alloc_sess(&setup);
	if (!sess->handle_cipher) {
		UADK_LOG(ERR, "uadk failed to alloc session!");
		ret = -EINVAL;
		goto uninit;
	}

	ret = wd_cipher_set_key(sess->handle_cipher, cipher->key.data, cipher->key.length);
	if (ret) {
		wd_cipher_free_sess(sess->handle_cipher);
		UADK_LOG(ERR, "uadk failed to set key!");
		ret = -EINVAL;
		goto uninit;
	}

	return 0;

uninit:
	wd_cipher_uninit2();
	priv->cipher_init = false;
	return ret;
}

/* Set session auth parameters */
static int
uadk_set_session_auth_parameters(struct rte_cryptodev *dev,
				 struct uadk_crypto_session *sess,
				 struct rte_crypto_sym_xform *xform)
{
	struct uadk_crypto_priv *priv = dev->data->dev_private;
	struct wd_digest_sess_setup setup = {0};
	struct sched_params params = {0};
	struct wd_ctx_params cparams = {0};
	struct wd_ctx_nums *ctx_set_num;
	char alg_name[MAX_ALG_NAME];
	int ret;

	sess->auth.operation = xform->auth.op;
	sess->auth.digest_length = xform->auth.digest_length;

	switch (xform->auth.algo) {
	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		setup.mode = (xform->auth.algo == RTE_CRYPTO_AUTH_MD5) ?
			     WD_DIGEST_NORMAL : WD_DIGEST_HMAC;
		setup.alg = WD_DIGEST_MD5;
		sess->auth.req.out_buf_bytes = 16;
		sess->auth.req.out_bytes = 16;
		rte_strscpy(alg_name, "md5", sizeof(alg_name));
		break;
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		setup.mode = (xform->auth.algo == RTE_CRYPTO_AUTH_SHA1) ?
			     WD_DIGEST_NORMAL : WD_DIGEST_HMAC;
		setup.alg = WD_DIGEST_SHA1;
		sess->auth.req.out_buf_bytes = 20;
		sess->auth.req.out_bytes = 20;
		rte_strscpy(alg_name, "sha1", sizeof(alg_name));
		break;
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		setup.mode = (xform->auth.algo == RTE_CRYPTO_AUTH_SHA224) ?
			     WD_DIGEST_NORMAL : WD_DIGEST_HMAC;
		setup.alg = WD_DIGEST_SHA224;
		sess->auth.req.out_buf_bytes = 28;
		sess->auth.req.out_bytes = 28;
		rte_strscpy(alg_name, "sha224", sizeof(alg_name));
		break;
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		setup.mode = (xform->auth.algo == RTE_CRYPTO_AUTH_SHA256) ?
			     WD_DIGEST_NORMAL : WD_DIGEST_HMAC;
		setup.alg = WD_DIGEST_SHA256;
		sess->auth.req.out_buf_bytes = 32;
		sess->auth.req.out_bytes = 32;
		rte_strscpy(alg_name, "sha256", sizeof(alg_name));
		break;
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		setup.mode = (xform->auth.algo == RTE_CRYPTO_AUTH_SHA384) ?
			     WD_DIGEST_NORMAL : WD_DIGEST_HMAC;
		setup.alg = WD_DIGEST_SHA384;
		sess->auth.req.out_buf_bytes = 48;
		sess->auth.req.out_bytes = 48;
		rte_strscpy(alg_name, "sha384", sizeof(alg_name));
		break;
	case RTE_CRYPTO_AUTH_SHA512:
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		setup.mode = (xform->auth.algo == RTE_CRYPTO_AUTH_SHA512) ?
			     WD_DIGEST_NORMAL : WD_DIGEST_HMAC;
		setup.alg = WD_DIGEST_SHA512;
		sess->auth.req.out_buf_bytes = 64;
		sess->auth.req.out_bytes = 64;
		rte_strscpy(alg_name, "sha512", sizeof(alg_name));
		break;
	default:
		return -ENOTSUP;
	}

	if (!priv->auth_init) {
		ctx_set_num = calloc(1, sizeof(*ctx_set_num));
		if (!ctx_set_num) {
			UADK_LOG(ERR, "failed to alloc ctx_set_size!");
			return -WD_ENOMEM;
		}

		cparams.op_type_num = 1;
		cparams.ctx_set_num = ctx_set_num;
		ctx_set_num->sync_ctx_num = priv->nb_qpairs;
		ctx_set_num->async_ctx_num = priv->nb_qpairs;

		ret = wd_digest_init2_(alg_name, SCHED_POLICY_RR, TASK_HW, &cparams);
		free(ctx_set_num);

		if (ret) {
			UADK_LOG(ERR, "failed to do digest init2!");
			return ret;
		}

		priv->auth_init = true;
	}

	params.numa_id = -1;	/* choose nearby numa node */
	setup.sched_param = &params;
	sess->handle_digest = wd_digest_alloc_sess(&setup);
	if (!sess->handle_digest) {
		UADK_LOG(ERR, "uadk failed to alloc session!");
		ret = -EINVAL;
		goto uninit;
	}

	/* if mode is HMAC, should set key */
	if (setup.mode == WD_DIGEST_HMAC) {
		ret = wd_digest_set_key(sess->handle_digest,
					xform->auth.key.data,
					xform->auth.key.length);
		if (ret) {
			UADK_LOG(ERR, "uadk failed to alloc session!");
			wd_digest_free_sess(sess->handle_digest);
			sess->handle_digest = 0;
			ret = -EINVAL;
			goto uninit;
		}
	}

	return 0;

uninit:
	wd_digest_uninit2();
	priv->auth_init = false;
	return ret;
}

static int
uadk_crypto_sym_session_configure(struct rte_cryptodev *dev,
				  struct rte_crypto_sym_xform *xform,
				  struct rte_cryptodev_sym_session *session)
{
	struct rte_crypto_sym_xform *cipher_xform = NULL;
	struct rte_crypto_sym_xform *auth_xform = NULL;
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
	case UADK_CHAIN_ONLY_AUTH:
		auth_xform = xform;
		break;
	case UADK_CHAIN_CIPHER_AUTH:
		cipher_xform = xform;
		auth_xform = xform->next;
		break;
	case UADK_CHAIN_AUTH_CIPHER:
		auth_xform = xform;
		cipher_xform = xform->next;
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

	if (auth_xform) {
		ret = uadk_set_session_auth_parameters(dev, sess, auth_xform);
		if (ret != 0) {
			UADK_LOG(ERR,
				"Invalid/unsupported auth parameters");
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

	if (sess->handle_digest) {
		wd_digest_free_sess(sess->handle_digest);
		sess->handle_digest = 0;
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

static void *uadk_cipher_async_cb(struct wd_cipher_req *req __rte_unused,
					 void *data __rte_unused)
{
	struct rte_crypto_op *op = req->cb_param;

	if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	return NULL;
}

static void
uadk_process_cipher_op(struct rte_crypto_op *op,
		       struct uadk_crypto_session *sess,
		       struct rte_mbuf *msrc, struct rte_mbuf *mdst,
		       bool async)
{
	uint32_t off = op->sym->cipher.data.offset;
	struct wd_cipher_req *req = &sess->cipher.req;
	int ret;

	req->src = rte_pktmbuf_mtod_offset(msrc, uint8_t *, off);
	req->in_bytes = op->sym->cipher.data.length;
	req->dst = rte_pktmbuf_mtod_offset(mdst, uint8_t *, off);
	req->out_buf_bytes = sess->cipher.req.in_bytes;
	req->iv_bytes = sess->iv.length;
	req->iv = rte_crypto_op_ctod_offset(op, uint8_t *, sess->iv.offset);
	req->cb = uadk_cipher_async_cb;
	req->cb_param = op;

	if (sess->cipher.direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		req->op_type = WD_CIPHER_ENCRYPTION;
	else
		req->op_type = WD_CIPHER_DECRYPTION;

	do {
		if (async)
			ret = wd_do_cipher_async(sess->handle_cipher, req);
		else
			ret = wd_do_cipher_sync(sess->handle_cipher, req);
	} while (ret == -WD_EBUSY);

	if (ret)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

static void *uadk_digest_async_cb(void *param)
{
	struct wd_digest_req *req = param;
	struct rte_crypto_op *op = req->cb_param;

	if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	return NULL;
}

static void
uadk_process_auth_op(struct uadk_qp *qp, struct rte_crypto_op *op,
		     struct uadk_crypto_session *sess,
		     struct rte_mbuf *msrc, struct rte_mbuf *mdst,
		     bool async, int idx)
{
	struct wd_digest_req *req = &sess->auth.req;
	uint32_t srclen = op->sym->auth.data.length;
	uint32_t off = op->sym->auth.data.offset;
	uint8_t *dst = NULL;
	int ret;

	if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		dst = qp->temp_digest[idx % BURST_MAX];
	} else {
		dst = op->sym->auth.digest.data;
		if (dst == NULL)
			dst = rte_pktmbuf_mtod_offset(mdst, uint8_t *,
					op->sym->auth.data.offset +
					op->sym->auth.data.length);
	}

	req->in = rte_pktmbuf_mtod_offset(msrc, uint8_t *, off);
	req->in_bytes = srclen;
	req->out = dst;
	req->cb = uadk_digest_async_cb;
	req->cb_param = op;

	do {
		if (async)
			ret = wd_do_digest_async(sess->handle_digest, req);
		else
			ret = wd_do_digest_sync(sess->handle_digest, req);
	} while (ret == -WD_EBUSY);

	if (ret)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
}

static uint16_t
uadk_crypto_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
			  uint16_t nb_ops)
{
	struct uadk_qp *qp = queue_pair;
	struct rte_mbuf *msrc, *mdst;
	struct rte_crypto_op *op;
	uint16_t enqd = 0;
	int i, ret;

	for (i = 0; i < nb_ops; i++) {
		struct uadk_crypto_session *sess = NULL;

		op = ops[i];
		op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
		msrc = op->sym->m_src;
		mdst = op->sym->m_dst ? op->sym->m_dst : op->sym->m_src;

		if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			if (likely(op->sym->session != NULL))
				sess = CRYPTODEV_GET_SYM_SESS_PRIV(
					op->sym->session);
		}

		if (!sess) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			continue;
		}

		switch (sess->chain_order) {
		case UADK_CHAIN_ONLY_CIPHER:
			uadk_process_cipher_op(op, sess, msrc, mdst, true);
			break;
		case UADK_CHAIN_ONLY_AUTH:
			uadk_process_auth_op(qp, op, sess, msrc, mdst, true, i);
			break;
		case UADK_CHAIN_CIPHER_AUTH:
			uadk_process_cipher_op(op, sess, msrc, mdst, false);
			uadk_process_auth_op(qp, op, sess, mdst, mdst, true, i);
			break;
		case UADK_CHAIN_AUTH_CIPHER:
			uadk_process_auth_op(qp, op, sess, msrc, mdst, false, i);
			uadk_process_cipher_op(op, sess, msrc, mdst, true);
			break;
		default:
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			break;
		}

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
	struct uadk_crypto_session *sess = NULL;
	struct rte_crypto_op *op;
	unsigned int nb_dequeued;
	unsigned int recv = 0, count = 0, i;
	int ret;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_pkts,
			(void **)ops, nb_ops, NULL);

	for (i = 0; i < nb_dequeued; i++) {
		op = ops[i];
		if (op->sess_type != RTE_CRYPTO_OP_WITH_SESSION)
			continue;

		sess = CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);

		if (!sess) {
			op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
			continue;
		}

		switch (sess->chain_order) {
		case UADK_CHAIN_ONLY_CIPHER:
		case UADK_CHAIN_AUTH_CIPHER:
			do {
				ret = wd_cipher_poll(1, &recv);
			} while (ret == -WD_EAGAIN);
			break;
		case UADK_CHAIN_ONLY_AUTH:
		case UADK_CHAIN_CIPHER_AUTH:
			do {
				ret = wd_digest_poll(1, &recv);
			} while (ret == -WD_EAGAIN);
			break;
		default:
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			break;
		}

		if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
			uint8_t *dst = qp->temp_digest[i % BURST_MAX];

			if (memcmp(dst, op->sym->auth.digest.data,
				   sess->auth.digest_length) != 0)
				op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		}

		count += recv;
		recv = 0;
	}

	qp->qp_stats.dequeued_count += nb_dequeued;

	return count;
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
	const char *input_args;
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

	input_args = rte_vdev_device_args(vdev);
	rte_cryptodev_pmd_parse_input_args(&init_params, input_args);

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
	priv->max_nb_qpairs = init_params.max_nb_queue_pairs;
	priv->nb_qpairs = UADK_CIPHER_DEF_CTXS;

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
RTE_PMD_REGISTER_PARAM_STRING(UADK_CRYPTO_DRIVER_NAME,
			      "max_nb_queue_pairs=<int>");
RTE_LOG_REGISTER_DEFAULT(uadk_crypto_logtype, INFO);
