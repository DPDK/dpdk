/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
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

#include "rte_aesni_mb_pmd_private.h"


static const struct rte_cryptodev_capabilities aesni_mb_pmd_capabilities[] = {
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
					.max = 12,
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
					.min = 12,
					.max = 12,
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
					.min = 14,
					.max = 14,
					.increment = 0
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
					.min = 16,
					.max = 16,
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
					.min = 24,
					.max = 24,
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
					.min = 32,
					.max = 32,
					.increment = 0
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
	{	/* AES XCBC HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_XCBC_MAC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 12,
					.max = 12,
					.increment = 0
				},
				.iv_size = { 0 }
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
					.min = 12,
					.max = 16,
					.increment = 4
				}
			}, }
		}, }
	},
	{	/* AES DOCSIS BPI */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_DOCSISBPI,
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
aesni_mb_pmd_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

/** Start device */
static int
aesni_mb_pmd_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Stop device */
static void
aesni_mb_pmd_stop(__rte_unused struct rte_cryptodev *dev)
{
}

/** Close device */
static int
aesni_mb_pmd_close(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}


/** Get device statistics */
static void
aesni_mb_pmd_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct aesni_mb_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;

		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
aesni_mb_pmd_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct aesni_mb_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}


/** Get device info */
static void
aesni_mb_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct aesni_mb_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = aesni_mb_pmd_capabilities;
		dev_info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		dev_info->sym.max_nb_sessions = internals->max_nb_sessions;
	}
}

/** Release queue pair */
static int
aesni_mb_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct aesni_mb_qp *qp = dev->data->queue_pairs[qp_id];
	struct rte_ring *r = NULL;

	if (qp != NULL) {
		r = rte_ring_lookup(qp->name);
		if (r)
			rte_ring_free(r);
		rte_free(qp);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** set a unique name for the queue pair based on it's name, dev_id and qp_id */
static int
aesni_mb_pmd_qp_set_unique_name(struct rte_cryptodev *dev,
		struct aesni_mb_qp *qp)
{
	unsigned n = snprintf(qp->name, sizeof(qp->name),
			"aesni_mb_pmd_%u_qp_%u",
			dev->data->dev_id, qp->id);

	if (n > sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place processed operations on */
static struct rte_ring *
aesni_mb_pmd_qp_create_processed_ops_ring(struct aesni_mb_qp *qp,
		const char *str, unsigned int ring_size, int socket_id)
{
	struct rte_ring *r;
	char ring_name[RTE_CRYPTODEV_NAME_LEN];

	unsigned int n = snprintf(ring_name, sizeof(ring_name),
				"%s_%s",
				qp->name, str);

	if (n > sizeof(ring_name))
		return NULL;

	r = rte_ring_lookup(ring_name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			MB_LOG_INFO("Reusing existing ring %s for processed ops",
			ring_name);
			return r;
		}

		MB_LOG_ERR("Unable to reuse existing ring %s for processed ops",
			ring_name);
		return NULL;
	}

	return rte_ring_create(ring_name, ring_size, socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
}

/** Setup a queue pair */
static int
aesni_mb_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id, struct rte_mempool *session_pool)
{
	struct aesni_mb_qp *qp = NULL;
	struct aesni_mb_private *internals = dev->data->dev_private;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		aesni_mb_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("AES-NI PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return -ENOMEM;

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	if (aesni_mb_pmd_qp_set_unique_name(dev, qp))
		goto qp_setup_cleanup;


	qp->op_fns = &job_ops[internals->vector_mode];

	qp->ingress_queue = aesni_mb_pmd_qp_create_processed_ops_ring(qp,
			"ingress", qp_conf->nb_descriptors, socket_id);
	if (qp->ingress_queue == NULL)
		goto qp_setup_cleanup;

	qp->sess_mp = session_pool;

	memset(&qp->stats, 0, sizeof(qp->stats));

	/* Initialise multi-buffer manager */
	(*qp->op_fns->job.init_mgr)(&qp->mb_mgr);
	return 0;

qp_setup_cleanup:
	if (qp)
		rte_free(qp);

	return -1;
}

/** Start queue pair */
static int
aesni_mb_pmd_qp_start(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Stop queue pair */
static int
aesni_mb_pmd_qp_stop(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Return the number of allocated queue pairs */
static uint32_t
aesni_mb_pmd_qp_count(struct rte_cryptodev *dev)
{
	return dev->data->nb_queue_pairs;
}

/** Returns the size of the aesni multi-buffer session structure */
static unsigned
aesni_mb_pmd_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct aesni_mb_session);
}

/** Configure a aesni multi-buffer session from a crypto xform chain */
static int
aesni_mb_pmd_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool)
{
	void *sess_private_data;
	struct aesni_mb_private *internals = dev->data->dev_private;
	int ret;

	if (unlikely(sess == NULL)) {
		MB_LOG_ERR("invalid session struct");
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		CDEV_LOG_ERR(
			"Couldn't get object from session mempool");
		return -ENOMEM;
	}

	ret = aesni_mb_set_session_parameters(&job_ops[internals->vector_mode],
			sess_private_data, xform);
	if (ret != 0) {
		MB_LOG_ERR("failed configure session parameters");

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
aesni_mb_pmd_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_session_private_data(sess, index);

	/* Zero out the whole structure */
	if (sess_priv) {
		memset(sess_priv, 0, sizeof(struct aesni_mb_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

struct rte_cryptodev_ops aesni_mb_pmd_ops = {
		.dev_configure		= aesni_mb_pmd_config,
		.dev_start		= aesni_mb_pmd_start,
		.dev_stop		= aesni_mb_pmd_stop,
		.dev_close		= aesni_mb_pmd_close,

		.stats_get		= aesni_mb_pmd_stats_get,
		.stats_reset		= aesni_mb_pmd_stats_reset,

		.dev_infos_get		= aesni_mb_pmd_info_get,

		.queue_pair_setup	= aesni_mb_pmd_qp_setup,
		.queue_pair_release	= aesni_mb_pmd_qp_release,
		.queue_pair_start	= aesni_mb_pmd_qp_start,
		.queue_pair_stop	= aesni_mb_pmd_qp_stop,
		.queue_pair_count	= aesni_mb_pmd_qp_count,

		.session_get_size	= aesni_mb_pmd_session_get_size,
		.session_configure	= aesni_mb_pmd_session_configure,
		.session_clear		= aesni_mb_pmd_session_clear
};

struct rte_cryptodev_ops *rte_aesni_mb_pmd_ops = &aesni_mb_pmd_ops;
