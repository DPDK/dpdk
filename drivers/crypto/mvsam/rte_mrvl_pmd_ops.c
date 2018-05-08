/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Marvell International Ltd.
 * Copyright(c) 2017 Semihalf.
 * All rights reserved.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_cryptodev_pmd.h>

#include "rte_mrvl_pmd_private.h"

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
					.min = 16,
					.max = 16,
					.increment = 0
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
						.min = 16,
						.max = 16,
						.increment = 0
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
						.min = 20,
						.max = 20,
						.increment = 0
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
					.min = 20,
					.max = 20,
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
					.min = 28,
					.max = 28,
					.increment = 0
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
						.min = 32,
						.max = 32,
						.increment = 0
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
						.min = 32,
						.max = 32,
						.increment = 0
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
					.min = 48,
					.max = 48,
					.increment = 0
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
					.min = 48,
					.max = 48,
					.increment = 0
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
					.min = 64,
					.max = 64,
					.increment = 0
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
					.min = 64,
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
					.min = 8,
					.max = 12,
					.increment = 4
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
		int socket_id, struct rte_mempool *session_pool)
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
			MRVL_CRYPTO_LOG_ERR("No crypto engines detected.\n");
			return -1;
		}

		/*
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
		 * In case just one engine is enabled mapping will look as
		 * follows:
		 * qp:      0        1        2        3
		 * cio-x:y: cio-0:0, cio-0:1, cio-0:2, cio-0:3
		 */
		n = snprintf(match, sizeof(match), "cio-%u:%u",
				qp_id % num, qp_id / num);

		if (n >= sizeof(match))
			break;

		qp->cio_params.match = match;
		qp->cio_params.size = qp_conf->nb_descriptors;

		if (sam_cio_init(&qp->cio_params, &qp->cio) < 0)
			break;

		qp->sess_mp = session_pool;

		memset(&qp->stats, 0, sizeof(qp->stats));
		dev->data->queue_pairs[qp_id] = qp;
		return 0;
	} while (0);

	rte_free(qp);
	return -1;
}

/** Start queue pair (PMD ops callback) - not supported.
 *
 * @param dev Pointer to the device structure.
 * @param qp_id ID of the Queue Pair.
 * @returns -ENOTSUP. Always.
 */
static int
mrvl_crypto_pmd_qp_start(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Stop queue pair (PMD ops callback) - not supported.
 *
 * @param dev Pointer to the device structure.
 * @param qp_id ID of the Queue Pair.
 * @returns -ENOTSUP. Always.
 */
static int
mrvl_crypto_pmd_qp_stop(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Return the number of allocated queue pairs (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @returns Number of allocated queue pairs.
 */
static uint32_t
mrvl_crypto_pmd_qp_count(struct rte_cryptodev *dev)
{
	return dev->data->nb_queue_pairs;
}

/** Returns the size of the session structure (PMD ops callback).
 *
 * @param dev Pointer to the device structure [Unused].
 * @returns Size of Marvell crypto session.
 */
static unsigned
mrvl_crypto_pmd_session_get_size(__rte_unused struct rte_cryptodev *dev)
{
	return sizeof(struct mrvl_crypto_session);
}

/** Configure the session from a crypto xform chain (PMD ops callback).
 *
 * @param dev Pointer to the device structure.
 * @param xform Pointer to the crytpo configuration structure.
 * @param sess Pointer to the empty session structure.
 * @returns 0 upon success, negative value otherwise.
 */
static int
mrvl_crypto_pmd_session_configure(__rte_unused struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mp)
{
	struct mrvl_crypto_session *mrvl_sess;
	void *sess_private_data;
	int ret;

	if (sess == NULL) {
		MRVL_CRYPTO_LOG_ERR("Invalid session struct.");
		return -EINVAL;
	}

	if (rte_mempool_get(mp, &sess_private_data)) {
		CDEV_LOG_ERR("Couldn't get object from session mempool.");
		return -ENOMEM;
	}

	ret = mrvl_crypto_set_session_parameters(sess_private_data, xform);
	if (ret != 0) {
		MRVL_CRYPTO_LOG_ERR("Failed to configure session parameters.");

		/* Return session to mempool */
		rte_mempool_put(mp, sess_private_data);
		return ret;
	}

	set_session_private_data(sess, dev->driver_id, sess_private_data);

	mrvl_sess = (struct mrvl_crypto_session *)sess_private_data;
	if (sam_session_create(&mrvl_sess->sam_sess_params,
				&mrvl_sess->sam_sess) < 0) {
		MRVL_CRYPTO_LOG_DBG("Failed to create session!");
		return -EIO;
	}

	return 0;
}

/**
 * Clear the memory of session so it doesn't leave key material behind.
 *
 * @param dev Pointer to the device structure.
 * @returns 0. Always.
 */
static void
mrvl_crypto_pmd_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{

	uint8_t index = dev->driver_id;
	void *sess_priv = get_session_private_data(sess, index);

	/* Zero out the whole structure */
	if (sess_priv) {
		struct mrvl_crypto_session *mrvl_sess =
			(struct mrvl_crypto_session *)sess_priv;

		if (mrvl_sess->sam_sess &&
		    sam_session_destroy(mrvl_sess->sam_sess) < 0) {
			MRVL_CRYPTO_LOG_INFO("Error while destroying session!");
		}

		memset(sess, 0, sizeof(struct mrvl_crypto_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_session_private_data(sess, index, NULL);
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
		.queue_pair_start	= mrvl_crypto_pmd_qp_start,
		.queue_pair_stop	= mrvl_crypto_pmd_qp_stop,
		.queue_pair_count	= mrvl_crypto_pmd_qp_count,

		.session_get_size	= mrvl_crypto_pmd_session_get_size,
		.session_configure	= mrvl_crypto_pmd_session_configure,
		.session_clear		= mrvl_crypto_pmd_session_clear
};

struct rte_cryptodev_ops *rte_mrvl_crypto_pmd_ops = &mrvl_crypto_pmd_ops;
