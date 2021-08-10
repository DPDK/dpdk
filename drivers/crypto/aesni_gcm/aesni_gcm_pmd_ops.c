/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <cryptodev_pmd.h>

#include "aesni_gcm_pmd_private.h"

static const struct rte_cryptodev_capabilities aesni_gcm_pmd_capabilities[] = {
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
					.min = 1,
					.max = 16,
					.increment = 1
				},
				.iv_size = {
					.min = 12,
					.max = 12,
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
					.min = 1,
					.max = 16,
					.increment = 1
				},
				.aad_size = {
					.min = 0,
					.max = 65535,
					.increment = 1
				},
				.iv_size = {
					.min = 12,
					.max = 12,
					.increment = 0
				}
			}, }
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

/** Configure device */
static int
aesni_gcm_pmd_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

/** Start device */
static int
aesni_gcm_pmd_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Stop device */
static void
aesni_gcm_pmd_stop(__rte_unused struct rte_cryptodev *dev)
{
}

/** Close device */
static int
aesni_gcm_pmd_close(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}


/** Get device statistics */
static void
aesni_gcm_pmd_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct aesni_gcm_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
aesni_gcm_pmd_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct aesni_gcm_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}


/** Get device info */
static void
aesni_gcm_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct aesni_gcm_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = aesni_gcm_pmd_capabilities;

		dev_info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		/* No limit of number of sessions */
		dev_info->sym.max_nb_sessions = 0;
	}
}

/** Release queue pair */
static int
aesni_gcm_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	if (dev->data->queue_pairs[qp_id] != NULL) {
		struct aesni_gcm_qp *qp = dev->data->queue_pairs[qp_id];

		if (qp->processed_pkts)
			rte_ring_free(qp->processed_pkts);

		rte_free(dev->data->queue_pairs[qp_id]);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** set a unique name for the queue pair based on it's name, dev_id and qp_id */
static int
aesni_gcm_pmd_qp_set_unique_name(struct rte_cryptodev *dev,
		struct aesni_gcm_qp *qp)
{
	unsigned n = snprintf(qp->name, sizeof(qp->name),
			"aesni_gcm_pmd_%u_qp_%u",
			dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place process packets on */
static struct rte_ring *
aesni_gcm_pmd_qp_create_processed_pkts_ring(struct aesni_gcm_qp *qp,
		unsigned ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			AESNI_GCM_LOG(INFO, "Reusing existing ring %s for processed"
				" packets", qp->name);
			return r;
		}
		AESNI_GCM_LOG(ERR, "Unable to reuse existing ring %s for processed"
				" packets", qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
}

/** Setup a queue pair */
static int
aesni_gcm_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id)
{
	struct aesni_gcm_qp *qp = NULL;
	struct aesni_gcm_private *internals = dev->data->dev_private;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		aesni_gcm_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("AES-NI PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return (-ENOMEM);

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	if (aesni_gcm_pmd_qp_set_unique_name(dev, qp))
		goto qp_setup_cleanup;

	qp->ops = (const struct aesni_gcm_ops *)internals->ops;

	qp->processed_pkts = aesni_gcm_pmd_qp_create_processed_pkts_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->processed_pkts == NULL)
		goto qp_setup_cleanup;

	qp->sess_mp = qp_conf->mp_session;
	qp->sess_mp_priv = qp_conf->mp_session_private;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));

	return 0;

qp_setup_cleanup:
	if (qp)
		rte_free(qp);

	return -1;
}

/** Returns the size of the aesni gcm session structure */
static unsigned
aesni_gcm_pmd_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct aesni_gcm_session);
}

/** Configure a aesni gcm session from a crypto xform chain */
static int
aesni_gcm_pmd_sym_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool)
{
	void *sess_private_data;
	int ret;
	struct aesni_gcm_private *internals = dev->data->dev_private;

	if (unlikely(sess == NULL)) {
		AESNI_GCM_LOG(ERR, "invalid session struct");
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		AESNI_GCM_LOG(ERR,
				"Couldn't get object from session mempool");
		return -ENOMEM;
	}
	ret = aesni_gcm_set_session_parameters(internals->ops,
				sess_private_data, xform);
	if (ret != 0) {
		AESNI_GCM_LOG(ERR, "failed configure session parameters");

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
aesni_gcm_pmd_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_sym_session_private_data(sess, index);

	/* Zero out the whole structure */
	if (sess_priv) {
		memset(sess_priv, 0, sizeof(struct aesni_gcm_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_sym_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

struct rte_cryptodev_ops aesni_gcm_pmd_ops = {
		.dev_configure		= aesni_gcm_pmd_config,
		.dev_start		= aesni_gcm_pmd_start,
		.dev_stop		= aesni_gcm_pmd_stop,
		.dev_close		= aesni_gcm_pmd_close,

		.stats_get		= aesni_gcm_pmd_stats_get,
		.stats_reset		= aesni_gcm_pmd_stats_reset,

		.dev_infos_get		= aesni_gcm_pmd_info_get,

		.queue_pair_setup	= aesni_gcm_pmd_qp_setup,
		.queue_pair_release	= aesni_gcm_pmd_qp_release,

		.sym_cpu_process        = aesni_gcm_pmd_cpu_crypto_process,

		.sym_session_get_size	= aesni_gcm_pmd_sym_session_get_size,
		.sym_session_configure	= aesni_gcm_pmd_sym_session_configure,
		.sym_session_clear	= aesni_gcm_pmd_sym_session_clear
};

struct rte_cryptodev_ops *rte_aesni_gcm_pmd_ops = &aesni_gcm_pmd_ops;
