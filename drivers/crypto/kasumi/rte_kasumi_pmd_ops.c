/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_cryptodev_pmd.h>

#include "rte_kasumi_pmd_private.h"

static const struct rte_cryptodev_capabilities kasumi_pmd_capabilities[] = {
	{	/* KASUMI (F9) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_KASUMI_F9,
				.block_size = 8,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 4,
					.max = 4,
					.increment = 0
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* KASUMI (F8) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_KASUMI_F8,
				.block_size = 8,
				.key_size = {
					.min = 16,
					.max = 16,
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

/** Configure device */
static int
kasumi_pmd_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

/** Start device */
static int
kasumi_pmd_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Stop device */
static void
kasumi_pmd_stop(__rte_unused struct rte_cryptodev *dev)
{
}

/** Close device */
static int
kasumi_pmd_close(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}


/** Get device statistics */
static void
kasumi_pmd_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct kasumi_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
kasumi_pmd_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct kasumi_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}


/** Get device info */
static void
kasumi_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct kasumi_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		dev_info->sym.max_nb_sessions = internals->max_nb_sessions;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = kasumi_pmd_capabilities;
	}
}

/** Release queue pair */
static int
kasumi_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct kasumi_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp != NULL) {
		rte_ring_free(qp->processed_ops);
		rte_free(qp);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** set a unique name for the queue pair based on its name, dev_id and qp_id */
static int
kasumi_pmd_qp_set_unique_name(struct rte_cryptodev *dev,
		struct kasumi_qp *qp)
{
	unsigned n = snprintf(qp->name, sizeof(qp->name),
			"kasumi_pmd_%u_qp_%u",
			dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place processed ops on */
static struct rte_ring *
kasumi_pmd_qp_create_processed_ops_ring(struct kasumi_qp *qp,
		unsigned ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (rte_ring_get_size(r) == ring_size) {
			KASUMI_LOG_INFO("Reusing existing ring %s"
					" for processed packets",
					 qp->name);
			return r;
		}

		KASUMI_LOG_ERR("Unable to reuse existing ring %s"
				" for processed packets",
				 qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
}

/** Setup a queue pair */
static int
kasumi_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id, struct rte_mempool *session_pool)
{
	struct kasumi_qp *qp = NULL;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		kasumi_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("KASUMI PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return (-ENOMEM);

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	if (kasumi_pmd_qp_set_unique_name(dev, qp))
		goto qp_setup_cleanup;

	qp->processed_ops = kasumi_pmd_qp_create_processed_ops_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->processed_ops == NULL)
		goto qp_setup_cleanup;

	qp->sess_mp = session_pool;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));

	return 0;

qp_setup_cleanup:
	rte_free(qp);

	return -1;
}

/** Start queue pair */
static int
kasumi_pmd_qp_start(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Stop queue pair */
static int
kasumi_pmd_qp_stop(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Return the number of allocated queue pairs */
static uint32_t
kasumi_pmd_qp_count(struct rte_cryptodev *dev)
{
	return dev->data->nb_queue_pairs;
}

/** Returns the size of the KASUMI session structure */
static unsigned
kasumi_pmd_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct kasumi_session);
}

/** Configure a KASUMI session from a crypto xform chain */
static int
kasumi_pmd_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool)
{
	void *sess_private_data;
	int ret;

	if (unlikely(sess == NULL)) {
		KASUMI_LOG_ERR("invalid session struct");
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		CDEV_LOG_ERR(
			"Couldn't get object from session mempool");
		return -ENOMEM;
	}

	ret = kasumi_set_session_parameters(sess_private_data, xform);
	if (ret != 0) {
		KASUMI_LOG_ERR("failed configure session parameters");

		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_session_private_data(sess, dev->driver_id,
		sess_private_data);

	return 0;
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
kasumi_pmd_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_session_private_data(sess, index);

	/* Zero out the whole structure */
	if (sess_priv) {
		memset(sess_priv, 0, sizeof(struct kasumi_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

struct rte_cryptodev_ops kasumi_pmd_ops = {
		.dev_configure      = kasumi_pmd_config,
		.dev_start          = kasumi_pmd_start,
		.dev_stop           = kasumi_pmd_stop,
		.dev_close          = kasumi_pmd_close,

		.stats_get          = kasumi_pmd_stats_get,
		.stats_reset        = kasumi_pmd_stats_reset,

		.dev_infos_get      = kasumi_pmd_info_get,

		.queue_pair_setup   = kasumi_pmd_qp_setup,
		.queue_pair_release = kasumi_pmd_qp_release,
		.queue_pair_start   = kasumi_pmd_qp_start,
		.queue_pair_stop    = kasumi_pmd_qp_stop,
		.queue_pair_count   = kasumi_pmd_qp_count,

		.session_get_size   = kasumi_pmd_session_get_size,
		.session_configure  = kasumi_pmd_session_configure,
		.session_clear      = kasumi_pmd_session_clear
};

struct rte_cryptodev_ops *rte_kasumi_pmd_ops = &kasumi_pmd_ops;
