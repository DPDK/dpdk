/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_cryptodev_pmd.h>

#include "aesni_gcm_pmd_private.h"

static const struct rte_cryptodev_capabilities aesni_gcm_pmd_capabilities[] = {
	{	/* AES GCM (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_GCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 8,
					.max = 16,
					.increment = 4
				},
				.aad_size = {
					.min = 8,
					.max = 12,
					.increment = 4
				}
			}, }
		}, }
	},
	{	/* AES GCM (CIPHER) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_GCM,
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
aesni_gcm_pmd_config(__rte_unused struct rte_cryptodev *dev)
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
		dev_info->dev_type = dev->dev_type;
                dev_info->feature_flags = dev->feature_flags;
                dev_info->capabilities = aesni_gcm_pmd_capabilities;

		dev_info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		dev_info->sym.max_nb_sessions = internals->max_nb_sessions;
	}
}

/** Release queue pair */
static int
aesni_gcm_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	if (dev->data->queue_pairs[qp_id] != NULL) {
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

	if (n > sizeof(qp->name))
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
		if (r->prod.size >= ring_size) {
			GCM_LOG_INFO("Reusing existing ring %s for processed"
					" packets", qp->name);
			return r;
		}

		GCM_LOG_ERR("Unable to reuse existing ring %s for processed"
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

	qp->ops = &gcm_ops[internals->vector_mode];

	qp->processed_pkts = aesni_gcm_pmd_qp_create_processed_pkts_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->processed_pkts == NULL)
		goto qp_setup_cleanup;

	qp->sess_mp = dev->data->session_pool;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));

	return 0;

qp_setup_cleanup:
	if (qp)
		rte_free(qp);

	return -1;
}

/** Start queue pair */
static int
aesni_gcm_pmd_qp_start(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Stop queue pair */
static int
aesni_gcm_pmd_qp_stop(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Return the number of allocated queue pairs */
static uint32_t
aesni_gcm_pmd_qp_count(struct rte_cryptodev *dev)
{
	return dev->data->nb_queue_pairs;
}

/** Returns the size of the aesni gcm session structure */
static unsigned
aesni_gcm_pmd_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct aesni_gcm_session);
}

/** Configure a aesni gcm session from a crypto xform chain */
static void *
aesni_gcm_pmd_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,	void *sess)
{
	struct aesni_gcm_private *internals = dev->data->dev_private;

	if (unlikely(sess == NULL)) {
		GCM_LOG_ERR("invalid session struct");
		return NULL;
	}

	if (aesni_gcm_set_session_parameters(&gcm_ops[internals->vector_mode],
			sess, xform) != 0) {
		GCM_LOG_ERR("failed configure session parameters");
		return NULL;
	}

	return sess;
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
aesni_gcm_pmd_session_clear(struct rte_cryptodev *dev __rte_unused, void *sess)
{
	if (sess)
		memset(sess, 0, sizeof(struct aesni_gcm_session));
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
		.queue_pair_start	= aesni_gcm_pmd_qp_start,
		.queue_pair_stop	= aesni_gcm_pmd_qp_stop,
		.queue_pair_count	= aesni_gcm_pmd_qp_count,

		.session_get_size	= aesni_gcm_pmd_session_get_size,
		.session_configure	= aesni_gcm_pmd_session_configure,
		.session_clear		= aesni_gcm_pmd_session_clear
};

struct rte_cryptodev_ops *rte_aesni_gcm_pmd_ops = &aesni_gcm_pmd_ops;
