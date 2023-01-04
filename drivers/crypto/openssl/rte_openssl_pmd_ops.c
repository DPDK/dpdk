/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <cryptodev_pmd.h>

#include "openssl_pmd_private.h"
#include "compat.h"
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
#include <openssl/provider.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#endif

static const struct rte_cryptodev_capabilities openssl_pmd_capabilities[] = {
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
					.min = 1,
					.max = 16,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 20,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 28,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 28,
					.increment = 1
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
					.min = 1,
					.max = 32,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 48,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 1,
					.max = 64,
					.increment = 1
				},
				.iv_size = { 0 }
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
					.min = 0,
					.max = 65535,
					.increment = 1
				},
				.iv_size = {
					.min = 12,
					.max = 16,
					.increment = 4
				},
			}, }
		}, }
	},
	{	/* AES CCM */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.aead = {
				.algo = RTE_CRYPTO_AEAD_AES_CCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 4,
					.max = 16,
					.increment = 2
				},
				.aad_size = {
					.min = 0,
					.max = 65535,
					.increment = 1
				},
				.iv_size = {
					.min = 7,
					.max = 13,
					.increment = 1
				},
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
					.min = 12,
					.max = 16,
					.increment = 4
				}
			}, }
		}, }
	},
	{	/* AES CMAC (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_CMAC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 4,
					.max = 16,
					.increment = 4
				},
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
					.min = 8,
					.max = 24,
					.increment = 8
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
					.min = 16,
					.max = 24,
					.increment = 8
				},
				.iv_size = {
					.min = 8,
					.max = 8,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* DES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_DES_CBC,
				.block_size = 8,
				.key_size = {
					.min = 8,
					.max = 8,
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
	{	/* DES DOCSIS BPI */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_DES_DOCSISBPI,
				.block_size = 8,
				.key_size = {
					.min = 8,
					.max = 8,
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
	{	/* RSA */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_RSA,
				.op_types = ((1 << RTE_CRYPTO_ASYM_OP_SIGN) |
					(1 << RTE_CRYPTO_ASYM_OP_VERIFY) |
					(1 << RTE_CRYPTO_ASYM_OP_ENCRYPT) |
					(1 << RTE_CRYPTO_ASYM_OP_DECRYPT)),
				{
				.modlen = {
				/* min length is based on openssl rsa keygen */
				.min = 30,
				/* value 0 symbolizes no limit on max length */
				.max = 0,
				.increment = 1
				}, }
			}
		},
		}
	},
	{	/* modexp */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX,
				.op_types = 0,
				{
				.modlen = {
				/* value 0 symbolizes no limit on min length */
				.min = 0,
				/* value 0 symbolizes no limit on max length */
				.max = 0,
				.increment = 1
				}, }
			}
		},
		}
	},
	{	/* modinv */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_MODINV,
				.op_types = 0,
				{
				.modlen = {
				/* value 0 symbolizes no limit on min length */
				.min = 0,
				/* value 0 symbolizes no limit on max length */
				.max = 0,
				.increment = 1
				}, }
			}
		},
		}
	},
	{	/* dh */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_DH,
				.op_types =
				((1<<RTE_CRYPTO_ASYM_KE_PRIV_KEY_GENERATE) |
				(1 << RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE |
				(1 <<
				RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE))),
				{
				.modlen = {
				/* value 0 symbolizes no limit on min length */
				.min = 0,
				/* value 0 symbolizes no limit on max length */
				.max = 0,
				.increment = 1
				}, }
			}
		},
		}
	},
	{	/* dsa */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_DSA,
				.op_types =
				((1<<RTE_CRYPTO_ASYM_OP_SIGN) |
				(1 << RTE_CRYPTO_ASYM_OP_VERIFY)),
				{
				.modlen = {
				/* value 0 symbolizes no limit on min length */
				.min = 0,
				/* value 0 symbolizes no limit on max length */
				.max = 0,
				.increment = 1
				}, }
			}
		},
		}
	},

	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};


/** Configure device */
static int
openssl_pmd_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	return 0;
}

/** Start device */
static int
openssl_pmd_start(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

/** Stop device */
static void
openssl_pmd_stop(__rte_unused struct rte_cryptodev *dev)
{
}

/** Close device */
static int
openssl_pmd_close(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}


/** Get device statistics */
static void
openssl_pmd_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct openssl_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->stats.enqueued_count;
		stats->dequeued_count += qp->stats.dequeued_count;

		stats->enqueue_err_count += qp->stats.enqueue_err_count;
		stats->dequeue_err_count += qp->stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
openssl_pmd_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct openssl_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->stats, 0, sizeof(qp->stats));
	}
}


/** Get device info */
static void
openssl_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct openssl_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = openssl_pmd_capabilities;
		dev_info->max_nb_queue_pairs = internals->max_nb_qpairs;
		/* No limit of number of sessions */
		dev_info->sym.max_nb_sessions = 0;
	}
}

/** Release queue pair */
static int
openssl_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	if (dev->data->queue_pairs[qp_id] != NULL) {
		struct openssl_qp *qp = dev->data->queue_pairs[qp_id];

		rte_ring_free(qp->processed_ops);

		rte_free(dev->data->queue_pairs[qp_id]);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** set a unique name for the queue pair based on it's name, dev_id and qp_id */
static int
openssl_pmd_qp_set_unique_name(struct rte_cryptodev *dev,
		struct openssl_qp *qp)
{
	unsigned int n = snprintf(qp->name, sizeof(qp->name),
			"openssl_pmd_%u_qp_%u",
			dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}


/** Create a ring to place processed operations on */
static struct rte_ring *
openssl_pmd_qp_create_processed_ops_ring(struct openssl_qp *qp,
		unsigned int ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			OPENSSL_LOG(INFO,
					"Reusing existing ring %s for processed ops",
				 qp->name);
			return r;
		}

		OPENSSL_LOG(ERR,
				"Unable to reuse existing ring %s for processed ops",
			 qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
}


/** Setup a queue pair */
static int
openssl_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		const struct rte_cryptodev_qp_conf *qp_conf,
		int socket_id)
{
	struct openssl_qp *qp = NULL;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		openssl_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("OPENSSL PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return -ENOMEM;

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	if (openssl_pmd_qp_set_unique_name(dev, qp))
		goto qp_setup_cleanup;

	qp->processed_ops = openssl_pmd_qp_create_processed_ops_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->processed_ops == NULL)
		goto qp_setup_cleanup;

	qp->sess_mp = qp_conf->mp_session;

	memset(&qp->stats, 0, sizeof(qp->stats));

	return 0;

qp_setup_cleanup:
	rte_free(qp);

	return -1;
}

/** Returns the size of the symmetric session structure */
static unsigned
openssl_pmd_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct openssl_session);
}

/** Returns the size of the asymmetric session structure */
static unsigned
openssl_pmd_asym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct openssl_asym_session);
}

/** Configure the session from a crypto xform chain */
static int
openssl_pmd_sym_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess)
{
	void *sess_private_data = CRYPTODEV_GET_SYM_SESS_PRIV(sess);
	int ret;

	if (unlikely(sess == NULL)) {
		OPENSSL_LOG(ERR, "invalid session struct");
		return -EINVAL;
	}

	ret = openssl_set_session_parameters(sess_private_data, xform);
	if (ret != 0) {
		OPENSSL_LOG(ERR, "failed configure session parameters");

		/* Return session to mempool */
		return ret;
	}

	return 0;
}

static int openssl_set_asym_session_parameters(
		struct openssl_asym_session *asym_session,
		struct rte_crypto_asym_xform *xform)
{
	int ret = -1;

	if ((xform->xform_type != RTE_CRYPTO_ASYM_XFORM_DH) &&
		(xform->next != NULL)) {
		OPENSSL_LOG(ERR, "chained xfrms are not supported on %s",
			rte_cryptodev_asym_get_xform_string(xform->xform_type));
		return ret;
	}

	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
	{
		BIGNUM *n = NULL;
		BIGNUM *e = NULL;
		BIGNUM *d = NULL;
		BIGNUM *p = NULL, *q = NULL, *dmp1 = NULL;
		BIGNUM *iqmp = NULL, *dmq1 = NULL;

		/* copy xfrm data into rsa struct */
		n = BN_bin2bn((const unsigned char *)xform->rsa.n.data,
				xform->rsa.n.length, n);
		e = BN_bin2bn((const unsigned char *)xform->rsa.e.data,
				xform->rsa.e.length, e);

		if (!n || !e)
			goto err_rsa;

#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		OSSL_PARAM_BLD * param_bld = OSSL_PARAM_BLD_new();
		if (!param_bld) {
			OPENSSL_LOG(ERR, "failed to allocate resources\n");
			goto err_rsa;
		}

		if (!OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_RSA_N, n)
			|| !OSSL_PARAM_BLD_push_BN(param_bld,
					OSSL_PKEY_PARAM_RSA_E, e)) {
			OSSL_PARAM_BLD_free(param_bld);
			OPENSSL_LOG(ERR, "failed to allocate resources\n");
			goto err_rsa;
		}

		if (xform->rsa.key_type == RTE_RSA_KEY_TYPE_EXP) {
			d = BN_bin2bn(
			(const unsigned char *)xform->rsa.d.data,
			xform->rsa.d.length,
			d);
			if (!d) {
				OSSL_PARAM_BLD_free(param_bld);
				goto err_rsa;
			}
		} else {
			p = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.p.data,
					xform->rsa.qt.p.length,
					p);
			q = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.q.data,
					xform->rsa.qt.q.length,
					q);
			dmp1 = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.dP.data,
					xform->rsa.qt.dP.length,
					dmp1);
			dmq1 = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.dQ.data,
					xform->rsa.qt.dQ.length,
					dmq1);
			iqmp = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.qInv.data,
					xform->rsa.qt.qInv.length,
					iqmp);

			if (!p || !q || !dmp1 || !dmq1 || !iqmp) {
				OSSL_PARAM_BLD_free(param_bld);
				goto err_rsa;
			}

			if (!OSSL_PARAM_BLD_push_BN(param_bld,
							OSSL_PKEY_PARAM_RSA_FACTOR1, p)
				|| !OSSL_PARAM_BLD_push_BN(param_bld,
							OSSL_PKEY_PARAM_RSA_FACTOR2, q)
				|| !OSSL_PARAM_BLD_push_BN(param_bld,
							OSSL_PKEY_PARAM_RSA_EXPONENT1, dmp1)
				|| !OSSL_PARAM_BLD_push_BN(param_bld,
							OSSL_PKEY_PARAM_RSA_EXPONENT2, dmq1)
				|| !OSSL_PARAM_BLD_push_BN(param_bld,
							OSSL_PKEY_PARAM_RSA_COEFFICIENT1, iqmp)) {
				OSSL_PARAM_BLD_free(param_bld);
				goto err_rsa;
			}
		}

		if (!OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_RSA_N, n)
			|| !OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_RSA_E, e)
			|| !OSSL_PARAM_BLD_push_BN(param_bld,
						OSSL_PKEY_PARAM_RSA_D, d)) {
			OSSL_PARAM_BLD_free(param_bld);
			goto err_rsa;
		}

		EVP_PKEY_CTX *key_ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
		EVP_PKEY *pkey = NULL;
		EVP_PKEY_CTX *rsa_ctx = NULL;
		OSSL_PARAM *params = NULL;

		params = OSSL_PARAM_BLD_to_param(param_bld);
		if (!params) {
			OSSL_PARAM_BLD_free(param_bld);
			goto err_rsa;
		}

		if (key_ctx == NULL
			|| EVP_PKEY_fromdata_init(key_ctx) <= 0
			|| EVP_PKEY_fromdata(key_ctx, &pkey,
				EVP_PKEY_KEYPAIR, params) <= 0) {
			OSSL_PARAM_free(params);
			goto err_rsa;
		}

		rsa_ctx = EVP_PKEY_CTX_new(pkey, NULL);
		asym_session->xfrm_type = RTE_CRYPTO_ASYM_XFORM_RSA;
		asym_session->u.r.ctx = rsa_ctx;
		EVP_PKEY_CTX_free(key_ctx);
		OSSL_PARAM_free(params);
		break;
#else
		RSA *rsa = RSA_new();
		if (rsa == NULL)
			goto err_rsa;

		if (xform->rsa.key_type == RTE_RSA_KEY_TYPE_EXP) {
			d = BN_bin2bn(
			(const unsigned char *)xform->rsa.d.data,
			xform->rsa.d.length,
			d);
			if (!d) {
				RSA_free(rsa);
				goto err_rsa;
			}
		} else {
			p = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.p.data,
					xform->rsa.qt.p.length,
					p);
			q = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.q.data,
					xform->rsa.qt.q.length,
					q);
			dmp1 = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.dP.data,
					xform->rsa.qt.dP.length,
					dmp1);
			dmq1 = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.dQ.data,
					xform->rsa.qt.dQ.length,
					dmq1);
			iqmp = BN_bin2bn((const unsigned char *)
					xform->rsa.qt.qInv.data,
					xform->rsa.qt.qInv.length,
					iqmp);

			if (!p || !q || !dmp1 || !dmq1 || !iqmp) {
				RSA_free(rsa);
				goto err_rsa;
			}
			ret = set_rsa_params(rsa, p, q);
			if (ret) {
				OPENSSL_LOG(ERR,
					"failed to set rsa params\n");
				RSA_free(rsa);
				goto err_rsa;
			}
			ret = set_rsa_crt_params(rsa, dmp1, dmq1, iqmp);
			if (ret) {
				OPENSSL_LOG(ERR,
					"failed to set crt params\n");
				RSA_free(rsa);
				/*
				 * set already populated params to NULL
				 * as its freed by call to RSA_free
				 */
				p = q = NULL;
				goto err_rsa;
			}
		}

		ret = set_rsa_keys(rsa, n, e, d);
		if (ret) {
			OPENSSL_LOG(ERR, "Failed to load rsa keys\n");
			RSA_free(rsa);
			return ret;
		}
		asym_session->u.r.rsa = rsa;
		asym_session->xfrm_type = RTE_CRYPTO_ASYM_XFORM_RSA;
		break;
#endif
err_rsa:
		BN_clear_free(n);
		BN_clear_free(e);
		BN_clear_free(d);
		BN_clear_free(p);
		BN_clear_free(q);
		BN_clear_free(dmp1);
		BN_clear_free(dmq1);
		BN_clear_free(iqmp);

		return -1;
	}
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
	{
		struct rte_crypto_modex_xform *xfrm = &(xform->modex);

		BN_CTX *ctx = BN_CTX_new();
		if (ctx == NULL) {
			OPENSSL_LOG(ERR,
				" failed to allocate resources\n");
			return ret;
		}
		BN_CTX_start(ctx);
		BIGNUM *mod = BN_CTX_get(ctx);
		BIGNUM *exp = BN_CTX_get(ctx);
		if (mod == NULL || exp == NULL) {
			BN_CTX_end(ctx);
			BN_CTX_free(ctx);
			return ret;
		}

		mod = BN_bin2bn((const unsigned char *)
				xfrm->modulus.data,
				xfrm->modulus.length, mod);
		exp = BN_bin2bn((const unsigned char *)
				xfrm->exponent.data,
				xfrm->exponent.length, exp);
		asym_session->u.e.ctx = ctx;
		asym_session->u.e.mod = mod;
		asym_session->u.e.exp = exp;
		asym_session->xfrm_type = RTE_CRYPTO_ASYM_XFORM_MODEX;
		break;
	}
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
	{
		struct rte_crypto_modinv_xform *xfrm = &(xform->modinv);

		BN_CTX *ctx = BN_CTX_new();
		if (ctx == NULL) {
			OPENSSL_LOG(ERR,
				" failed to allocate resources\n");
			return ret;
		}
		BN_CTX_start(ctx);
		BIGNUM *mod = BN_CTX_get(ctx);
		if (mod == NULL) {
			BN_CTX_end(ctx);
			BN_CTX_free(ctx);
			return ret;
		}

		mod = BN_bin2bn((const unsigned char *)
				xfrm->modulus.data,
				xfrm->modulus.length,
				mod);
		asym_session->u.m.ctx = ctx;
		asym_session->u.m.modulus = mod;
		asym_session->xfrm_type = RTE_CRYPTO_ASYM_XFORM_MODINV;
		break;
	}
	case RTE_CRYPTO_ASYM_XFORM_DH:
	{
		BIGNUM *p = NULL;
		BIGNUM *g = NULL;

		p = BN_bin2bn((const unsigned char *)
				xform->dh.p.data,
				xform->dh.p.length,
				p);
		g = BN_bin2bn((const unsigned char *)
				xform->dh.g.data,
				xform->dh.g.length,
				g);
		if (!p || !g)
			goto err_dh;

		DH *dh = NULL;
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		OSSL_PARAM_BLD *param_bld = NULL;
		param_bld = OSSL_PARAM_BLD_new();
		if (!param_bld) {
			OPENSSL_LOG(ERR, "failed to allocate resources\n");
			goto err_dh;
		}
		if ((!OSSL_PARAM_BLD_push_utf8_string(param_bld,
					"group", "ffdhe2048", 0))
			|| (!OSSL_PARAM_BLD_push_BN(param_bld,
					OSSL_PKEY_PARAM_FFC_P, p))
			|| (!OSSL_PARAM_BLD_push_BN(param_bld,
					OSSL_PKEY_PARAM_FFC_G, g))) {
			OSSL_PARAM_BLD_free(param_bld);
			goto err_dh;
		}

		OSSL_PARAM_BLD *param_bld_peer = NULL;
		param_bld_peer = OSSL_PARAM_BLD_new();
		if (!param_bld_peer) {
			OPENSSL_LOG(ERR, "failed to allocate resources\n");
			OSSL_PARAM_BLD_free(param_bld);
			goto err_dh;
		}
		if ((!OSSL_PARAM_BLD_push_utf8_string(param_bld_peer,
					"group", "ffdhe2048", 0))
			|| (!OSSL_PARAM_BLD_push_BN(param_bld_peer,
					OSSL_PKEY_PARAM_FFC_P, p))
			|| (!OSSL_PARAM_BLD_push_BN(param_bld_peer,
					OSSL_PKEY_PARAM_FFC_G, g))) {
			OSSL_PARAM_BLD_free(param_bld);
			OSSL_PARAM_BLD_free(param_bld_peer);
			goto err_dh;
		}

		asym_session->u.dh.param_bld = param_bld;
		asym_session->u.dh.param_bld_peer = param_bld_peer;
#else
		dh = DH_new();
		if (dh == NULL) {
			OPENSSL_LOG(ERR,
				"failed to allocate resources\n");
			goto err_dh;
		}
		ret = set_dh_params(dh, p, g);
		if (ret) {
			DH_free(dh);
			goto err_dh;
		}
#endif
		asym_session->u.dh.dh_key = dh;
		asym_session->xfrm_type = RTE_CRYPTO_ASYM_XFORM_DH;
		break;

err_dh:
		OPENSSL_LOG(ERR, " failed to set dh params\n");
		BN_free(p);
		BN_free(g);
		return -1;
	}
	case RTE_CRYPTO_ASYM_XFORM_DSA:
	{
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		BIGNUM *p = NULL, *g = NULL;
		BIGNUM *q = NULL, *priv_key = NULL;
		BIGNUM *pub_key = BN_new();
		BN_zero(pub_key);
		OSSL_PARAM_BLD *param_bld = NULL;

		p = BN_bin2bn((const unsigned char *)
				xform->dsa.p.data,
				xform->dsa.p.length,
				p);

		g = BN_bin2bn((const unsigned char *)
				xform->dsa.g.data,
				xform->dsa.g.length,
				g);

		q = BN_bin2bn((const unsigned char *)
				xform->dsa.q.data,
				xform->dsa.q.length,
				q);
		if (!p || !q || !g)
			goto err_dsa;

		priv_key = BN_bin2bn((const unsigned char *)
				xform->dsa.x.data,
				xform->dsa.x.length,
				priv_key);
		if (priv_key == NULL)
			goto err_dsa;

		param_bld = OSSL_PARAM_BLD_new();
		if (!param_bld) {
			OPENSSL_LOG(ERR, "failed to allocate resources\n");
			goto err_dsa;
		}

		if (!OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_FFC_P, p)
			|| !OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_FFC_G, g)
			|| !OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_FFC_Q, q)
			|| !OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_PRIV_KEY, priv_key)) {
			OSSL_PARAM_BLD_free(param_bld);
			OPENSSL_LOG(ERR, "failed to allocate resources\n");
			goto err_dsa;
		}
		asym_session->xfrm_type = RTE_CRYPTO_ASYM_XFORM_DSA;
		asym_session->u.s.param_bld = param_bld;

		break;
#else
		BIGNUM *p = NULL, *g = NULL;
		BIGNUM *q = NULL, *priv_key = NULL;
		BIGNUM *pub_key = BN_new();
		BN_zero(pub_key);

		p = BN_bin2bn((const unsigned char *)
				xform->dsa.p.data,
				xform->dsa.p.length,
				p);

		g = BN_bin2bn((const unsigned char *)
				xform->dsa.g.data,
				xform->dsa.g.length,
				g);

		q = BN_bin2bn((const unsigned char *)
				xform->dsa.q.data,
				xform->dsa.q.length,
				q);
		if (!p || !q || !g)
			goto err_dsa;

		priv_key = BN_bin2bn((const unsigned char *)
				xform->dsa.x.data,
				xform->dsa.x.length,
				priv_key);
		if (priv_key == NULL)
			goto err_dsa;

		DSA *dsa = DSA_new();
		if (dsa == NULL) {
			OPENSSL_LOG(ERR,
				" failed to allocate resources\n");
			goto err_dsa;
		}

		ret = set_dsa_params(dsa, p, q, g);
		if (ret) {
			DSA_free(dsa);
			OPENSSL_LOG(ERR, "Failed to dsa params\n");
			goto err_dsa;
		}

		/*
		 * openssl 1.1.0 mandate that public key can't be
		 * NULL in very first call. so set a dummy pub key.
		 * to keep consistency, lets follow same approach for
		 * both versions
		 */
		/* just set dummy public for very 1st call */
		ret = set_dsa_keys(dsa, pub_key, priv_key);
		if (ret) {
			DSA_free(dsa);
			OPENSSL_LOG(ERR, "Failed to set keys\n");
			return -1;
		}
		asym_session->u.s.dsa = dsa;
		asym_session->xfrm_type = RTE_CRYPTO_ASYM_XFORM_DSA;
		break;
#endif
err_dsa:
		BN_free(p);
		BN_free(q);
		BN_free(g);
		BN_free(priv_key);
		BN_free(pub_key);
		return -1;
	}
	default:
		return ret;
	}

	return 0;
}

/** Configure the session from a crypto xform chain */
static int
openssl_pmd_asym_session_configure(struct rte_cryptodev *dev __rte_unused,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *sess)
{
	void *asym_sess_private_data;
	int ret;

	if (unlikely(sess == NULL)) {
		OPENSSL_LOG(ERR, "invalid asymmetric session struct");
		return -EINVAL;
	}

	asym_sess_private_data = sess->sess_private_data;
	ret = openssl_set_asym_session_parameters(asym_sess_private_data,
			xform);
	if (ret != 0) {
		OPENSSL_LOG(ERR, "failed configure session parameters");
		return ret;
	}

	return 0;
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
openssl_pmd_sym_session_clear(struct rte_cryptodev *dev __rte_unused,
		struct rte_cryptodev_sym_session *sess)
{
	void *sess_priv = CRYPTODEV_GET_SYM_SESS_PRIV(sess);

	/* Zero out the whole structure */
	openssl_reset_session(sess_priv);
}

static void openssl_reset_asym_session(struct openssl_asym_session *sess)
{
	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		if (sess->u.r.ctx)
			EVP_PKEY_CTX_free(sess->u.r.ctx);
#else
		if (sess->u.r.rsa)
			RSA_free(sess->u.r.rsa);
#endif
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		if (sess->u.e.ctx) {
			BN_CTX_end(sess->u.e.ctx);
			BN_CTX_free(sess->u.e.ctx);
		}
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODINV:
		if (sess->u.m.ctx) {
			BN_CTX_end(sess->u.m.ctx);
			BN_CTX_free(sess->u.m.ctx);
		}
		break;
	case RTE_CRYPTO_ASYM_XFORM_DH:
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		sess->u.dh.param_bld = NULL;
		sess->u.dh.param_bld_peer = NULL;
#else
		if (sess->u.dh.dh_key)
			DH_free(sess->u.dh.dh_key);
#endif
		break;
	case RTE_CRYPTO_ASYM_XFORM_DSA:
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		sess->u.s.param_bld = NULL;
#else
		if (sess->u.s.dsa)
			DSA_free(sess->u.s.dsa);
#endif
		break;
	default:
		break;
	}
}

/** Clear the memory of asymmetric session
 * so it doesn't leave key material behind
 */
static void
openssl_pmd_asym_session_clear(struct rte_cryptodev *dev __rte_unused,
		struct rte_cryptodev_asym_session *sess)
{
	void *sess_priv = sess->sess_private_data;

	/* Zero out the whole structure */
	if (sess_priv) {
		openssl_reset_asym_session(sess_priv);
		memset(sess_priv, 0, sizeof(struct openssl_asym_session));
	}
}

struct rte_cryptodev_ops openssl_pmd_ops = {
		.dev_configure		= openssl_pmd_config,
		.dev_start		= openssl_pmd_start,
		.dev_stop		= openssl_pmd_stop,
		.dev_close		= openssl_pmd_close,

		.stats_get		= openssl_pmd_stats_get,
		.stats_reset		= openssl_pmd_stats_reset,

		.dev_infos_get		= openssl_pmd_info_get,

		.queue_pair_setup	= openssl_pmd_qp_setup,
		.queue_pair_release	= openssl_pmd_qp_release,

		.sym_session_get_size	= openssl_pmd_sym_session_get_size,
		.asym_session_get_size	= openssl_pmd_asym_session_get_size,
		.sym_session_configure	= openssl_pmd_sym_session_configure,
		.asym_session_configure	= openssl_pmd_asym_session_configure,
		.sym_session_clear	= openssl_pmd_sym_session_clear,
		.asym_session_clear	= openssl_pmd_asym_session_clear
};

struct rte_cryptodev_ops *rte_openssl_pmd_ops = &openssl_pmd_ops;
