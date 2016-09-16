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
				.aad_size = { 0 }
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

/** Configure device */
static int
null_crypto_pmd_config(__rte_unused struct rte_cryptodev *dev)
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
		dev_info->dev_type = dev->dev_type;
		dev_info->max_nb_queue_pairs = internals->max_nb_qpairs;
		dev_info->sym.max_nb_sessions = internals->max_nb_sessions;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = null_crypto_pmd_capabilities;
	}
}

/** Release queue pair */
static int
null_crypto_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	if (dev->data->queue_pairs[qp_id] != NULL) {
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

	if (n > sizeof(qp->name))
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
		if (r->prod.size >= ring_size) {
			NULL_CRYPTO_LOG_INFO(
				"Reusing existing ring %s for processed packets",
				qp->name);
			return r;
		}

		NULL_CRYPTO_LOG_INFO(
			"Unable to reuse existing ring %s for processed packets",
			 qp->name);
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
		NULL_CRYPTO_LOG_ERR("Invalid qp_id %u, greater than maximum "
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
		NULL_CRYPTO_LOG_ERR("Failed to allocate queue pair memory");
		return (-ENOMEM);
	}

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	retval = null_crypto_pmd_qp_set_unique_name(dev, qp);
	if (retval) {
		NULL_CRYPTO_LOG_ERR("Failed to create unique name for null "
				"crypto device");
		goto qp_setup_cleanup;
	}

	qp->processed_pkts = null_crypto_pmd_qp_create_processed_pkts_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->processed_pkts == NULL) {
		NULL_CRYPTO_LOG_ERR("Failed to create unique name for null "
				"crypto device");
		goto qp_setup_cleanup;
	}

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
null_crypto_pmd_qp_start(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Stop queue pair */
static int
null_crypto_pmd_qp_stop(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Return the number of allocated queue pairs */
static uint32_t
null_crypto_pmd_qp_count(struct rte_cryptodev *dev)
{
	return dev->data->nb_queue_pairs;
}

/** Returns the size of the NULL crypto session structure */
static unsigned
null_crypto_pmd_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct null_crypto_session);
}

/** Configure a null crypto session from a crypto xform chain */
static void *
null_crypto_pmd_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_sym_xform *xform, void *sess)
{
	int retval;

	if (unlikely(sess == NULL)) {
		NULL_CRYPTO_LOG_ERR("invalid session struct");
		return NULL;
	}
	retval = null_crypto_set_session_parameters(
			(struct null_crypto_session *)sess, xform);
	if (retval != 0) {
		NULL_CRYPTO_LOG_ERR("failed configure session parameters");
		return NULL;
	}

	return sess;
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
null_crypto_pmd_session_clear(struct rte_cryptodev *dev __rte_unused,
		void *sess)
{
	if (sess)
		memset(sess, 0, sizeof(struct null_crypto_session));
}

struct rte_cryptodev_ops pmd_ops = {
		.dev_configure		= null_crypto_pmd_config,
		.dev_start		= null_crypto_pmd_start,
		.dev_stop		= null_crypto_pmd_stop,
		.dev_close		= null_crypto_pmd_close,

		.stats_get		= null_crypto_pmd_stats_get,
		.stats_reset		= null_crypto_pmd_stats_reset,

		.dev_infos_get		= null_crypto_pmd_info_get,

		.queue_pair_setup	= null_crypto_pmd_qp_setup,
		.queue_pair_release	= null_crypto_pmd_qp_release,
		.queue_pair_start	= null_crypto_pmd_qp_start,
		.queue_pair_stop	= null_crypto_pmd_qp_stop,
		.queue_pair_count	= null_crypto_pmd_qp_count,

		.session_get_size	= null_crypto_pmd_session_get_size,
		.session_configure	= null_crypto_pmd_session_configure,
		.session_clear		= null_crypto_pmd_session_clear
};

struct rte_cryptodev_ops *null_crypto_pmd_ops = &pmd_ops;
