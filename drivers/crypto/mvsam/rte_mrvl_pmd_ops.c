/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Marvell International Ltd.
 * Copyright(c) 2017 Semihalf.
 * All rights reserved.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <cryptodev_pmd.h>
#include <rte_security_driver.h>

#include "mrvl_pmd_private.h"

/**
 * Capabilities list to be used in reporting to DPDK.
 */
static const struct rte_cryptodev_capabilities
	mrvl_crypto_pmd_capabilities[] = {
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
					.min = 12,
					.max = 16,
					.increment = 4
				},
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
						.min = 12,
						.max = 16,
						.increment = 4
					},
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
						.min = 12,
						.max = 20,
						.increment = 4
					},
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
					.min = 12,
					.max = 20,
					.increment = 4
				},
			}, }
		}, }
	},
	{
		/* SHA224 HMAC */
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
					.min = 12,
					.max = 28,
					.increment = 0
				},
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
					.min = 12,
					.max = 28,
					.increment = 4
				},
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
						.min = 12,
						.max = 32,
						.increment = 4
					},
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
						.min = 12,
						.max = 32,
						.increment = 4
					},
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
					.min = 12,
					.max = 48,
					.increment = 4
				},
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
					.min = 12,
					.max = 48,
					.increment = 4
				},
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
					.min = 12,
					.max = 64,
					.increment = 4
				},
			}, }
		}, }
	},
	{	/* SHA512  */
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
					.min = 12,
					.max = 64,
					.increment = 0
				},
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
	{	/* AES CTR */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_CTR,
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
	{	/* AES GCM */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.aead = {
				.algo = RTE_CRYPTO_AEAD_AES_GCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.aad_size = {
					.min = 0,
					.max = 64,
					.increment = 1
				},
				.iv_size = {
					.min = 12,
					.max = 16,
					.increment = 4
				}
			}, }
		}, }
	},
	{	/* AES GMAC (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_GMAC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.iv_size = {
					.min = 8,
					.max = 65532,
					.increment = 4
				}
			}, }
		}, }
	},
	{	/* 3DES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_CBC,
				.block_size = 8,
				.key_size = {
					.min = 24,
					.max = 24,
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
	{	/* 3DES CTR */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_CTR,
				.block_size = 8,
				.key_size = {
					.min = 24,
					.max = 24,
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
	{	/* 3DES ECB */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_ECB,
				.block_size = 8,
				.key_size = {
					.min = 24,
					.max = 24,
					.increment = 0
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* NULL (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
			}, },
		}, },
	},
	{	/* NULL (CIPHER) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_NULL,
				.block_size = 1,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
			}, },
		}, }
	},

	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};


/**
 * Configure device (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @param config Pointer to configuration structure.
 * @returns 0. Always.
 */
static int
mrvl_crypto_pmd_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

/**
 * Start device (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @returns 0. Always.
 */
static int
mrvl_crypto_pmd_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/**
 * Stop device (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @returns 0. Always.
 */
static void
mrvl_crypto_pmd_stop(__rte_unused struct rte_cryptodev *dev)
{
}

/**
 * Get device statistics (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @param stats Pointer to statistics structure [out].
 */
static void
mrvl_crypto_pmd_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct mrvl_crypto_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;

		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

/**
 * Reset device statistics (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 */
static void
mrvl_crypto_pmd_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct mrvl_crypto_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}

/**
 * Get device info (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @param dev_info Pointer to the device info structure [out].
 */
static void
mrvl_crypto_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct mrvl_crypto_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = mrvl_crypto_pmd_capabilities;
		dev_info->max_nb_queue_pairs = internals->max_nb_qpairs;
		dev_info->sym.max_nb_sessions = internals->max_nb_sessions;
	}
}

/**
 * Release queue pair (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @param qp_id ID of Queue Pair to release.
 * @returns 0. Always.
 */
static int
mrvl_crypto_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct mrvl_crypto_qp *qp =
			(struct mrvl_crypto_qp *)dev->data->queue_pairs[qp_id];

	if (dev->data->queue_pairs[qp_id] != NULL) {
		sam_cio_flush(qp->cio);
		sam_cio_deinit(qp->cio);
		rte_free(dev->data->queue_pairs[qp_id]);
		dev->data->queue_pairs[qp_id] = NULL;
	}

	return 0;
}

/**
 * Close device (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @returns 0. Always.
 */
static int
mrvl_crypto_pmd_close(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++)
		mrvl_crypto_pmd_qp_release(dev, qp_id);

	return 0;
}

/**
 * Setup a queue pair (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @param qp_id ID of the Queue Pair.
 * @param qp_conf Queue pair configuration (nb of descriptors).
 * @param socket_id NUMA socket to allocate memory on.
 * @returns 0 upon success, negative value otherwise.
 */
static int
mrvl_crypto_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id)
{
	struct mrvl_crypto_qp *qp = NULL;
	char match[RTE_CRYPTODEV_NAME_MAX_LEN];
	unsigned int n;

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("MRVL Crypto PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return -ENOMEM;

	/* Free old qp prior setup if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		mrvl_crypto_pmd_qp_release(dev, qp_id);

	do { /* Error handling block */

		/*
		 * This extra check is necessary due to a bug in
		 * crypto library.
		 */
		int num = sam_get_num_inst();
		if (num == 0) {
			MRVL_LOG(ERR, "No crypto engines detected!");
			return -1;
		}

		/*
		 * In case just one engine is enabled mapping will look as
		 * follows:
		 * qp:      0        1        2        3
		 * cio-x:y: cio-0:0, cio-0:1, cio-0:2, cio-0:3
		 *
		 * In case two crypto engines are enabled qps will
		 * be evenly spread among them. Even and odd qps will
		 * be handled by cio-0 and cio-1 respectively. qp-cio mapping
		 * will look as follows:
		 *
		 * qp:      0        1        2        3
		 * cio-x:y: cio-0:0, cio-1:0, cio-0:1, cio-1:1
		 *
		 * qp:      4        5        6        7
		 * cio-x:y: cio-0:2, cio-1:2, cio-0:3, cio-1:3
		 *
		 * In case of three crypto engines are enabled qps will
		 * be mapped as following:
		 *
		 * qp:      0        1        2        3
		 * cio-x:y: cio-0:0, cio-1:0, cio-2:0, cio-0:1
		 *
		 * qp:      4        5        6        7
		 * cio-x:y: cio-1:1, cio-2:1, cio-0:2, cio-1:2
		 *
		 * qp:      8        9        10       11
		 * cio-x:y: cio-2:2, cio-0:3, cio-1:3, cio-2:3
		 */
		n = snprintf(match, sizeof(match), "cio-%u:%u",
				qp_id % num, qp_id / num);

		if (n >= sizeof(match))
			break;

		qp->cio_params.match = match;
		qp->cio_params.size = qp_conf->nb_descriptors;

		if (sam_cio_init(&qp->cio_params, &qp->cio) < 0)
			break;

		qp->sess_mp = qp_conf->mp_session;
		qp->sess_mp_priv = qp_conf->mp_session_private;

		memset(&qp->stats, 0, sizeof(qp->stats));
		dev->data->queue_pairs[qp_id] = qp;
		return 0;
	} while (0);

	rte_free(qp);
	return -1;
}

/** Returns the size of the session structure (PMD ops callback).
 *
 * @param dev Pointer to the device structure [Unused].
 * @returns Size of Marvell crypto session.
 */
static unsigned
mrvl_crypto_pmd_sym_session_get_size(__rte_unused struct rte_cryptodev *dev)
{
	return sizeof(struct mrvl_crypto_session);
}

/** Configure the session from a crypto xform chain (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @param xform Pointer to the crypto configuration structure.
 * @param sess Pointer to the empty session structure.
 * @returns 0 upon success, negative value otherwise.
 */
static int
mrvl_crypto_pmd_sym_session_configure(__rte_unused struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mp)
{
	struct mrvl_crypto_session *mrvl_sess;
	void *sess_private_data;
	int ret;

	if (sess == NULL) {
		MRVL_LOG(ERR, "Invalid session struct!");
		return -EINVAL;
	}

	if (rte_mempool_get(mp, &sess_private_data)) {
		CDEV_LOG_ERR("Couldn't get object from session mempool.");
		return -ENOMEM;
	}

	memset(sess_private_data, 0, sizeof(struct mrvl_crypto_session));

	ret = mrvl_crypto_set_session_parameters(sess_private_data, xform);
	if (ret != 0) {
		MRVL_LOG(ERR, "Failed to configure session parameters!");

		/* Return session to mempool */
		rte_mempool_put(mp, sess_private_data);
		return ret;
	}

	set_sym_session_private_data(sess, dev->driver_id, sess_private_data);

	mrvl_sess = (struct mrvl_crypto_session *)sess_private_data;
	if (sam_session_create(&mrvl_sess->sam_sess_params,
				&mrvl_sess->sam_sess) < 0) {
		MRVL_LOG(DEBUG, "Failed to create session!");
		return -EIO;
	}

	/* free the keys memory allocated for session creation */
	if (mrvl_sess->sam_sess_params.cipher_key != NULL)
		free(mrvl_sess->sam_sess_params.cipher_key);
	if (mrvl_sess->sam_sess_params.auth_key != NULL)
		free(mrvl_sess->sam_sess_params.auth_key);

	return 0;
}

/**
 * Clear the memory of session so it doesn't leave key material behind.
 *
 * @param dev Pointer to the device structure.
 * @returns 0. Always.
 */
static void
mrvl_crypto_pmd_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{

	uint8_t index = dev->driver_id;
	void *sess_priv = get_sym_session_private_data(sess, index);

	/* Zero out the whole structure */
	if (sess_priv) {
		struct mrvl_crypto_session *mrvl_sess =
			(struct mrvl_crypto_session *)sess_priv;

		if (mrvl_sess->sam_sess &&
		    sam_session_destroy(mrvl_sess->sam_sess) < 0) {
			MRVL_LOG(ERR, "Error while destroying session!");
		}

		memset(mrvl_sess, 0, sizeof(struct mrvl_crypto_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_sym_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

/**
 * PMD handlers for crypto ops.
 */
static struct rte_cryptodev_ops mrvl_crypto_pmd_ops = {
		.dev_configure		= mrvl_crypto_pmd_config,
		.dev_start		= mrvl_crypto_pmd_start,
		.dev_stop		= mrvl_crypto_pmd_stop,
		.dev_close		= mrvl_crypto_pmd_close,

		.dev_infos_get		= mrvl_crypto_pmd_info_get,

		.stats_get		= mrvl_crypto_pmd_stats_get,
		.stats_reset		= mrvl_crypto_pmd_stats_reset,

		.queue_pair_setup	= mrvl_crypto_pmd_qp_setup,
		.queue_pair_release	= mrvl_crypto_pmd_qp_release,

		.sym_session_get_size	= mrvl_crypto_pmd_sym_session_get_size,
		.sym_session_configure	= mrvl_crypto_pmd_sym_session_configure,
		.sym_session_clear	= mrvl_crypto_pmd_sym_session_clear
};

struct rte_cryptodev_ops *rte_mrvl_crypto_pmd_ops = &mrvl_crypto_pmd_ops;

/* IPSEC full offloading */

/** Configure the session from a crypto xform chain (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @param conf Pointer to the security session configuration structure.
 * @param sess Pointer to the empty session structure.
 * @param mempool Pointer to memory pool.
 * @returns 0 upon success, negative value otherwise.
 */
static int
mrvl_crypto_pmd_security_session_create(__rte_unused void *dev,
				 struct rte_security_session_conf *conf,
				 struct rte_security_session *sess,
				 struct rte_mempool *mempool)
{
	struct mrvl_crypto_session *mrvl_sess;
	void *sess_private_data;
	int ret;

	if (sess == NULL) {
		MRVL_LOG(ERR, "Invalid session struct.");
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		MRVL_LOG(ERR, "Couldn't get object from session mempool.");
		return -ENOMEM;
	}

	switch (conf->protocol) {
	case RTE_SECURITY_PROTOCOL_IPSEC:
		mrvl_sess = (struct mrvl_crypto_session *)sess_private_data;

		struct rte_security_ipsec_xform *ipsec_xform = &conf->ipsec;
		struct rte_crypto_sym_xform *crypto_xform = conf->crypto_xform;

		ret = mrvl_ipsec_set_session_parameters(mrvl_sess,
							ipsec_xform,
							crypto_xform);
		if (ret != 0) {
			MRVL_LOG(ERR, "Failed to configure session parameters.");

			/* Return session to mempool */
			rte_mempool_put(mempool, sess_private_data);
			return ret;
		}

		if (mrvl_sess->sam_sess_params.cipher_mode == SAM_CIPHER_GCM) {
			/* Nonce is must for all counter modes */
			mrvl_sess->sam_sess_params.cipher_iv =
				(uint8_t *)&(conf->ipsec.salt);
		}

		ret = sam_session_create(&mrvl_sess->sam_sess_params,
				&mrvl_sess->sam_sess);
		if (ret < 0) {
			MRVL_LOG(ERR, "PMD: failed to create IPSEC session.");
			/* Return session to mempool */
			rte_mempool_put(mempool, sess_private_data);
			return ret;
		}
		break;
	case RTE_SECURITY_PROTOCOL_MACSEC:
		return -ENOTSUP;
	default:
		return -EINVAL;
	}

	set_sec_session_private_data(sess, sess_private_data);

	return ret;
}

/** Clear the memory of session so it doesn't leave key material behind */
static int
mrvl_crypto_pmd_security_session_destroy(void *dev __rte_unused,
		struct rte_security_session *sess)
{
	void *sess_priv = get_sec_session_private_data(sess);

	/* Zero out the whole structure */
	if (sess_priv) {
		struct mrvl_crypto_session *mrvl_sess =
			(struct mrvl_crypto_session *)sess_priv;
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);

		if (mrvl_sess->sam_sess &&
		    sam_session_destroy(mrvl_sess->sam_sess) < 0) {
			MRVL_LOG(ERR, "Error while destroying session!");
		}

		rte_free(mrvl_sess->sam_sess_params.cipher_key);
		rte_free(mrvl_sess->sam_sess_params.auth_key);
		rte_free(mrvl_sess->sam_sess_params.cipher_iv);
		memset(sess, 0, sizeof(struct rte_security_session));
		set_sec_session_private_data(sess, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
	return 0;
}

static const
struct rte_security_capability mrvl_crypto_pmd_sec_security_cap[] = {
	{ /* IPsec Lookaside Protocol offload ESP Tunnel Egress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 },
			.replay_win_sz_max = 128
		},
		.crypto_capabilities = mrvl_crypto_pmd_capabilities
	},
	{ /* IPsec Lookaside Protocol offload ESP Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 },
			.replay_win_sz_max = 128
		},
		.crypto_capabilities = mrvl_crypto_pmd_capabilities
	},
	{ /* IPsec Lookaside Protocol offload ESP Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 },
			.replay_win_sz_max = 128
		},
		.crypto_capabilities = mrvl_crypto_pmd_capabilities
	},
	{ /* IPsec Lookaside Protocol offload ESP Transport Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 },
			.replay_win_sz_max = 128
		},
		.crypto_capabilities = mrvl_crypto_pmd_capabilities
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

static const struct rte_security_capability *
mrvl_crypto_pmd_security_capabilities_get(void *device __rte_unused)
{
	return mrvl_crypto_pmd_sec_security_cap;
}

struct rte_security_ops mrvl_sec_security_pmd_ops = {
	.session_create = mrvl_crypto_pmd_security_session_create,
	.session_update = NULL,
	.session_stats_get = NULL,
	.session_destroy = mrvl_crypto_pmd_security_session_destroy,
	.set_pkt_metadata = NULL,
	.capabilities_get = mrvl_crypto_pmd_security_capabilities_get
};

struct rte_security_ops *rte_mrvl_security_pmd_ops = &mrvl_sec_security_pmd_ops;
