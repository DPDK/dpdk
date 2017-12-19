/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_cryptodev_pmd.h>

#include "rte_snow3g_pmd_private.h"

static const struct rte_cryptodev_capabilities snow3g_pmd_capabilities[] = {
	{	/* SNOW 3G (UIA2) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SNOW3G_UIA2,
				.block_size = 16,
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
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* SNOW 3G (UEA2) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.iv_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

/** Configure device */
static int
snow3g_pmd_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

/** Start device */
static int
snow3g_pmd_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Stop device */
static void
snow3g_pmd_stop(__rte_unused struct rte_cryptodev *dev)
{
}

/** Close device */
static int
snow3g_pmd_close(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}


/** Get device statistics */
static void
snow3g_pmd_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct snow3g_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
snow3g_pmd_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct snow3g_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}


/** Get device info */
static void
snow3g_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct snow3g_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		dev_info->sym.max_nb_sessions = internals->max_nb_sessions;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = snow3g_pmd_capabilities;
	}
}

/** Release queue pair */
static int
snow3g_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	if (dev->data->queue_pairs[qp_id] != NULL) {
		rte_free(dev->data->queue_pairs[qp_id]);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** set a unique name for the queue pair based on its name, dev_id and qp_id */
static int
snow3g_pmd_qp_set_unique_name(struct rte_cryptodev *dev,
		struct snow3g_qp *qp)
{
	unsigned n = snprintf(qp->name, sizeof(qp->name),
			"snow3g_pmd_%u_qp_%u",
			dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place processed ops on */
static struct rte_ring *
snow3g_pmd_qp_create_processed_ops_ring(struct snow3g_qp *qp,
		unsigned ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			SNOW3G_LOG_INFO("Reusing existing ring %s"
					" for processed packets",
					 qp->name);
			return r;
		}

		SNOW3G_LOG_ERR("Unable to reuse existing ring %s"
				" for processed packets",
				 qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
}

/** Setup a queue pair */
static int
snow3g_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id, struct rte_mempool *session_pool)
{
	struct snow3g_qp *qp = NULL;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		snow3g_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("SNOW 3G PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return (-ENOMEM);

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	if (snow3g_pmd_qp_set_unique_name(dev, qp))
		goto qp_setup_cleanup;

	qp->processed_ops = snow3g_pmd_qp_create_processed_ops_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->processed_ops == NULL)
		goto qp_setup_cleanup;

	qp->sess_mp = session_pool;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));

	return 0;

qp_setup_cleanup:
	if (qp)
		rte_free(qp);

	return -1;
}

/** Start queue pair */
static int
snow3g_pmd_qp_start(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Stop queue pair */
static int
snow3g_pmd_qp_stop(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Return the number of allocated queue pairs */
static uint32_t
snow3g_pmd_qp_count(struct rte_cryptodev *dev)
{
	return dev->data->nb_queue_pairs;
}

/** Returns the size of the SNOW 3G session structure */
static unsigned
snow3g_pmd_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct snow3g_session);
}

/** Configure a SNOW 3G session from a crypto xform chain */
static int
snow3g_pmd_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool)
{
	void *sess_private_data;
	int ret;

	if (unlikely(sess == NULL)) {
		SNOW3G_LOG_ERR("invalid session struct");
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		CDEV_LOG_ERR(
			"Couldn't get object from session mempool");
		return -ENOMEM;
	}

	ret = snow3g_set_session_parameters(sess_private_data, xform);
	if (ret != 0) {
		SNOW3G_LOG_ERR("failed configure session parameters");

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
snow3g_pmd_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_session_private_data(sess, index);

	/* Zero out the whole structure */
	if (sess_priv) {
		memset(sess_priv, 0, sizeof(struct snow3g_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

struct rte_cryptodev_ops snow3g_pmd_ops = {
		.dev_configure      = snow3g_pmd_config,
		.dev_start          = snow3g_pmd_start,
		.dev_stop           = snow3g_pmd_stop,
		.dev_close          = snow3g_pmd_close,

		.stats_get          = snow3g_pmd_stats_get,
		.stats_reset        = snow3g_pmd_stats_reset,

		.dev_infos_get      = snow3g_pmd_info_get,

		.queue_pair_setup   = snow3g_pmd_qp_setup,
		.queue_pair_release = snow3g_pmd_qp_release,
		.queue_pair_start   = snow3g_pmd_qp_start,
		.queue_pair_stop    = snow3g_pmd_qp_stop,
		.queue_pair_count   = snow3g_pmd_qp_count,

		.session_get_size   = snow3g_pmd_session_get_size,
		.session_configure  = snow3g_pmd_session_configure,
		.session_clear      = snow3g_pmd_session_clear
};

struct rte_cryptodev_ops *rte_snow3g_pmd_ops = &snow3g_pmd_ops;
