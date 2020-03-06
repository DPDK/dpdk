/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_cryptodev_pmd.h>

#include "zuc_pmd_private.h"

static const struct rte_cryptodev_capabilities zuc_pmd_capabilities[] = {
	{	/* ZUC (EIA3) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_ZUC_EIA3,
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
	{	/* ZUC (EEA3) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_ZUC_EEA3,
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
				},
			}, }
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

/** Configure device */
static int
zuc_pmd_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

/** Start device */
static int
zuc_pmd_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Stop device */
static void
zuc_pmd_stop(__rte_unused struct rte_cryptodev *dev)
{
}

/** Close device */
static int
zuc_pmd_close(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}


/** Get device statistics */
static void
zuc_pmd_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct zuc_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
zuc_pmd_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct zuc_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}


/** Get device info */
static void
zuc_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct zuc_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		/* No limit of number of sessions */
		dev_info->sym.max_nb_sessions = 0;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = zuc_pmd_capabilities;
	}
}

/** Release queue pair */
static int
zuc_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	if (dev->data->queue_pairs[qp_id] != NULL) {
		struct zuc_qp *qp = dev->data->queue_pairs[qp_id];

		if (qp->processed_ops)
			rte_ring_free(qp->processed_ops);

		rte_free(dev->data->queue_pairs[qp_id]);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** set a unique name for the queue pair based on its name, dev_id and qp_id */
static int
zuc_pmd_qp_set_unique_name(struct rte_cryptodev *dev,
		struct zuc_qp *qp)
{
	unsigned n = snprintf(qp->name, sizeof(qp->name),
			"zuc_pmd_%u_qp_%u",
			dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place processed ops on */
static struct rte_ring *
zuc_pmd_qp_create_processed_ops_ring(struct zuc_qp *qp,
		unsigned ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			ZUC_LOG(INFO, "Reusing existing ring %s"
					" for processed packets",
					 qp->name);
			return r;
		}

		ZUC_LOG(ERR, "Unable to reuse existing ring %s"
				" for processed packets",
				 qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
}

/** Setup a queue pair */
static int
zuc_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id)
{
	struct zuc_qp *qp = NULL;
	struct zuc_private *internals = dev->data->dev_private;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		zuc_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("ZUC PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return (-ENOMEM);

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	if (zuc_pmd_qp_set_unique_name(dev, qp))
		goto qp_setup_cleanup;

	qp->processed_ops = zuc_pmd_qp_create_processed_ops_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->processed_ops == NULL)
		goto qp_setup_cleanup;

	qp->mb_mgr = internals->mb_mgr;
	qp->sess_mp = qp_conf->mp_session;
	qp->sess_mp_priv = qp_conf->mp_session_private;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));

	return 0;

qp_setup_cleanup:
	if (qp)
		rte_free(qp);

	return -1;
}

/** Returns the size of the ZUC session structure */
static unsigned
zuc_pmd_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct zuc_session);
}

/** Configure a ZUC session from a crypto xform chain */
static int
zuc_pmd_sym_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool)
{
	void *sess_private_data;
	int ret;

	if (unlikely(sess == NULL)) {
		ZUC_LOG(ERR, "invalid session struct");
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		ZUC_LOG(ERR,
			"Couldn't get object from session mempool");

		return -ENOMEM;
	}

	ret = zuc_set_session_parameters(sess_private_data, xform);
	if (ret != 0) {
		ZUC_LOG(ERR, "failed configure session parameters");

		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_sym_session_private_data(sess, dev->driver_id,
		sess_private_data);

	return 0;
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
zuc_pmd_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_sym_session_private_data(sess, index);

	/* Zero out the whole structure */
	if (sess_priv) {
		memset(sess_priv, 0, sizeof(struct zuc_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_sym_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

struct rte_cryptodev_ops zuc_pmd_ops = {
		.dev_configure      = zuc_pmd_config,
		.dev_start          = zuc_pmd_start,
		.dev_stop           = zuc_pmd_stop,
		.dev_close          = zuc_pmd_close,

		.stats_get          = zuc_pmd_stats_get,
		.stats_reset        = zuc_pmd_stats_reset,

		.dev_infos_get      = zuc_pmd_info_get,

		.queue_pair_setup   = zuc_pmd_qp_setup,
		.queue_pair_release = zuc_pmd_qp_release,

		.sym_session_get_size   = zuc_pmd_sym_session_get_size,
		.sym_session_configure  = zuc_pmd_sym_session_configure,
		.sym_session_clear      = zuc_pmd_sym_session_clear
};

struct rte_cryptodev_ops *rte_zuc_pmd_ops = &zuc_pmd_ops;
