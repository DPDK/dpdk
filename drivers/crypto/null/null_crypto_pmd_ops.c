/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <cryptodev_pmd.h>

#include "null_crypto_pmd_private.h"

static const struct rte_cryptodev_capabilities null_crypto_pmd_capabilities[] = {
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
				.iv_size = { 0 }
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
				.iv_size = { 0 }
			}, },
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

/** Configure device */
static int
null_crypto_pmd_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

/** Start device */
static int
null_crypto_pmd_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Stop device */
static void
null_crypto_pmd_stop(__rte_unused struct rte_cryptodev *dev)
{
}

/** Close device */
static int
null_crypto_pmd_close(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Get device statistics */
static void
null_crypto_pmd_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct null_crypto_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
null_crypto_pmd_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct null_crypto_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}


/** Get device info */
static void
null_crypto_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct null_crypto_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->max_nb_queue_pairs = internals->max_nb_qpairs;
		/* No limit of number of sessions */
		dev_info->sym.max_nb_sessions = 0;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = null_crypto_pmd_capabilities;
	}
}

/** Release queue pair */
static int
null_crypto_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	if (dev->data->queue_pairs[qp_id] != NULL) {
		struct null_crypto_qp *qp = dev->data->queue_pairs[qp_id];

		rte_ring_free(qp->processed_pkts);

		rte_free(dev->data->queue_pairs[qp_id]);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** set a unique name for the queue pair based on it's name, dev_id and qp_id */
static int
null_crypto_pmd_qp_set_unique_name(struct rte_cryptodev *dev,
		struct null_crypto_qp *qp)
{
	unsigned n = snprintf(qp->name, sizeof(qp->name),
			"null_crypto_pmd_%u_qp_%u",
			dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place process packets on */
static struct rte_ring *
null_crypto_pmd_qp_create_processed_pkts_ring(struct null_crypto_qp *qp,
		unsigned ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			NULL_LOG(INFO,
					"Reusing existing ring %s for "
					" processed packets", qp->name);
			return r;
		}

		NULL_LOG(INFO,
				"Unable to reuse existing ring %s for "
				" processed packets", qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
}

/** Setup a queue pair */
static int
null_crypto_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id)
{
	struct null_crypto_private *internals = dev->data->dev_private;
	struct null_crypto_qp *qp;
	int retval;

	if (qp_id >= internals->max_nb_qpairs) {
		NULL_LOG(ERR, "Invalid qp_id %u, greater than maximum "
				"number of queue pairs supported (%u).",
				qp_id, internals->max_nb_qpairs);
		return (-EINVAL);
	}

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		null_crypto_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("Null Crypto PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL) {
		NULL_LOG(ERR, "Failed to allocate queue pair memory");
		return (-ENOMEM);
	}

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	retval = null_crypto_pmd_qp_set_unique_name(dev, qp);
	if (retval) {
		NULL_LOG(ERR, "Failed to create unique name for null "
				"crypto device");

		goto qp_setup_cleanup;
	}

	qp->processed_pkts = null_crypto_pmd_qp_create_processed_pkts_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->processed_pkts == NULL) {
		NULL_LOG(ERR, "Failed to create unique name for null "
				"crypto device");
		goto qp_setup_cleanup;
	}

	qp->sess_mp = qp_conf->mp_session;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));

	return 0;

qp_setup_cleanup:
	rte_free(qp);

	return -1;
}

/** Returns the size of the NULL crypto session structure */
static unsigned
null_crypto_pmd_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct null_crypto_session);
}

/** Configure a null crypto session from a crypto xform chain */
static int
null_crypto_pmd_sym_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess)
{
	void *sess_private_data;
	int ret;

	if (unlikely(sess == NULL)) {
		NULL_LOG(ERR, "invalid session struct");
		return -EINVAL;
	}

	sess_private_data = CRYPTODEV_GET_SYM_SESS_PRIV(sess);

	ret = null_crypto_set_session_parameters(sess_private_data, xform);
	if (ret != 0) {
		NULL_LOG(ERR, "failed configure session parameters");
		return ret;
	}

	return 0;
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
null_crypto_pmd_sym_session_clear(struct rte_cryptodev *dev __rte_unused,
		struct rte_cryptodev_sym_session *sess __rte_unused)
{}

static struct rte_cryptodev_ops pmd_ops = {
		.dev_configure		= null_crypto_pmd_config,
		.dev_start		= null_crypto_pmd_start,
		.dev_stop		= null_crypto_pmd_stop,
		.dev_close		= null_crypto_pmd_close,

		.stats_get		= null_crypto_pmd_stats_get,
		.stats_reset		= null_crypto_pmd_stats_reset,

		.dev_infos_get		= null_crypto_pmd_info_get,

		.queue_pair_setup	= null_crypto_pmd_qp_setup,
		.queue_pair_release	= null_crypto_pmd_qp_release,

		.sym_session_get_size	= null_crypto_pmd_sym_session_get_size,
		.sym_session_configure	= null_crypto_pmd_sym_session_configure,
		.sym_session_clear	= null_crypto_pmd_sym_session_clear
};

struct rte_cryptodev_ops *null_crypto_pmd_ops = &pmd_ops;
