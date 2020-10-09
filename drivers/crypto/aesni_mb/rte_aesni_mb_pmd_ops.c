/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2017 Intel Corporation
 */

#include <string.h>

#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_cryptodev_pmd.h>

#include "aesni_mb_pmd_private.h"


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
					.min = 1,
					.max = 16,
					.increment = 1
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
					.max = 65535,
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
					.min = 1,
					.max = 20,
					.increment = 1
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
					.max = 65535,
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
					.max = 65535,
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
					.min = 1,
					.max = 32,
					.increment = 1
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
					.max = 65535,
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
					.min = 1,
					.max = 48,
					.increment = 1
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
					.max = 65535,
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
					.min = 1,
					.max = 64,
					.increment = 1
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
#if IMB_VERSION_NUM >= IMB_VERSION(0, 53, 3)
					.max = 32,
					.increment = 16
#else
					.max = 16,
					.increment = 0
#endif
				},
				.iv_size = {
					.min = 16,
					.max = 16,
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
	{	/*  3DES CBC */
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
	{	/* AES CCM */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.aead = {
				.algo = RTE_CRYPTO_AEAD_AES_CCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
#if IMB_VERSION(0, 54, 2) <= IMB_VERSION_NUM
					.max = 32,
					.increment = 16
#else
					.max = 16,
					.increment = 0
#endif
				},
				.digest_size = {
					.min = 4,
					.max = 16,
					.increment = 2
				},
				.aad_size = {
					.min = 0,
					.max = 46,
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
	{	/* AES CMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_CMAC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
					.increment = 0
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
#if IMB_VERSION(0, 53, 0) <= IMB_VERSION_NUM
	{	/* AES ECB */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_ECB,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.iv_size = { 0 }
			}, }
		}, }
	},
#endif
#if IMB_VERSION(0, 53, 3) <= IMB_VERSION_NUM
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
#endif
#if IMB_VERSION(0, 54, 3) <= IMB_VERSION_NUM
	{	/* CHACHA20-POLY1305 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.aead = {
				.algo = RTE_CRYPTO_AEAD_CHACHA20_POLY1305,
				.block_size = 64,
				.key_size = {
					.min = 32,
					.max = 32,
					.increment = 0
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.aad_size = {
					.min = 0,
					.max = 240,
					.increment = 1
				},
				.iv_size = {
					.min = 12,
					.max = 12,
					.increment = 0
				},
			}, }
		}, }
	},
#endif
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
static const struct rte_cryptodev_capabilities
					aesni_mb_pmd_security_crypto_cap[] = {
	{	/* AES DOCSIS BPI */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_DOCSISBPI,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 16
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

static const struct rte_security_capability aesni_mb_pmd_security_cap[] = {
	{	/* DOCSIS Uplink */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,
		.docsis = {
			.direction = RTE_SECURITY_DOCSIS_UPLINK
		},
		.crypto_capabilities = aesni_mb_pmd_security_crypto_cap
	},
	{	/* DOCSIS Downlink */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,
		.docsis = {
			.direction = RTE_SECURITY_DOCSIS_DOWNLINK
		},
		.crypto_capabilities = aesni_mb_pmd_security_crypto_cap
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};
#endif

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
		/* No limit of number of sessions */
		dev_info->sym.max_nb_sessions = 0;
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
		if (qp->mb_mgr)
			free_mb_mgr(qp->mb_mgr);
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

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place processed operations on */
static struct rte_ring *
aesni_mb_pmd_qp_create_processed_ops_ring(struct aesni_mb_qp *qp,
		unsigned int ring_size, int socket_id)
{
	struct rte_ring *r;
	char ring_name[RTE_CRYPTODEV_NAME_MAX_LEN];

	unsigned int n = strlcpy(ring_name, qp->name, sizeof(ring_name));

	if (n >= sizeof(ring_name))
		return NULL;

	r = rte_ring_lookup(ring_name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			AESNI_MB_LOG(INFO, "Reusing existing ring %s for processed ops",
			ring_name);
			return r;
		}

		AESNI_MB_LOG(ERR, "Unable to reuse existing ring %s for processed ops",
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
		int socket_id)
{
	struct aesni_mb_qp *qp = NULL;
	struct aesni_mb_private *internals = dev->data->dev_private;
	int ret = -1;

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


	qp->mb_mgr = alloc_mb_mgr(0);
	if (qp->mb_mgr == NULL) {
		ret = -ENOMEM;
		goto qp_setup_cleanup;
	}

	switch (internals->vector_mode) {
	case RTE_AESNI_MB_SSE:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_SSE;
		init_mb_mgr_sse(qp->mb_mgr);
		break;
	case RTE_AESNI_MB_AVX:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX;
		init_mb_mgr_avx(qp->mb_mgr);
		break;
	case RTE_AESNI_MB_AVX2:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX2;
		init_mb_mgr_avx2(qp->mb_mgr);
		break;
	case RTE_AESNI_MB_AVX512:
		dev->feature_flags |= RTE_CRYPTODEV_FF_CPU_AVX512;
		init_mb_mgr_avx512(qp->mb_mgr);
		break;
	default:
		AESNI_MB_LOG(ERR, "Unsupported vector mode %u\n",
				internals->vector_mode);
		goto qp_setup_cleanup;
	}

	qp->ingress_queue = aesni_mb_pmd_qp_create_processed_ops_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->ingress_queue == NULL) {
		ret = -1;
		goto qp_setup_cleanup;
	}

	qp->sess_mp = qp_conf->mp_session;
	qp->sess_mp_priv = qp_conf->mp_session_private;

	memset(&qp->stats, 0, sizeof(qp->stats));

	char mp_name[RTE_MEMPOOL_NAMESIZE];

	snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
				"digest_mp_%u_%u", dev->data->dev_id, qp_id);
	return 0;

qp_setup_cleanup:
	if (qp) {
		if (qp->mb_mgr)
			free_mb_mgr(qp->mb_mgr);
		rte_free(qp);
	}

	return ret;
}

/** Returns the size of the aesni multi-buffer session structure */
static unsigned
aesni_mb_pmd_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct aesni_mb_session);
}

/** Configure a aesni multi-buffer session from a crypto xform chain */
static int
aesni_mb_pmd_sym_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool)
{
	void *sess_private_data;
	struct aesni_mb_private *internals = dev->data->dev_private;
	int ret;

	if (unlikely(sess == NULL)) {
		AESNI_MB_LOG(ERR, "invalid session struct");
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		AESNI_MB_LOG(ERR,
				"Couldn't get object from session mempool");
		return -ENOMEM;
	}

	ret = aesni_mb_set_session_parameters(internals->mb_mgr,
			sess_private_data, xform);
	if (ret != 0) {
		AESNI_MB_LOG(ERR, "failed configure session parameters");

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
aesni_mb_pmd_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_sym_session_private_data(sess, index);

	/* Zero out the whole structure */
	if (sess_priv) {
		memset(sess_priv, 0, sizeof(struct aesni_mb_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_sym_session_private_data(sess, index, NULL);
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

		.sym_cpu_process	= aesni_mb_cpu_crypto_process_bulk,

		.sym_session_get_size	= aesni_mb_pmd_sym_session_get_size,
		.sym_session_configure	= aesni_mb_pmd_sym_session_configure,
		.sym_session_clear	= aesni_mb_pmd_sym_session_clear
};

struct rte_cryptodev_ops *rte_aesni_mb_pmd_ops = &aesni_mb_pmd_ops;

#ifdef AESNI_MB_DOCSIS_SEC_ENABLED
/**
 * Configure a aesni multi-buffer session from a security session
 * configuration
 */
static int
aesni_mb_pmd_sec_sess_create(void *dev, struct rte_security_session_conf *conf,
		struct rte_security_session *sess,
		struct rte_mempool *mempool)
{
	void *sess_private_data;
	struct rte_cryptodev *cdev = (struct rte_cryptodev *)dev;
	int ret;

	if (conf->action_type != RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL ||
			conf->protocol != RTE_SECURITY_PROTOCOL_DOCSIS) {
		AESNI_MB_LOG(ERR, "Invalid security protocol");
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		AESNI_MB_LOG(ERR, "Couldn't get object from session mempool");
		return -ENOMEM;
	}

	ret = aesni_mb_set_docsis_sec_session_parameters(cdev, conf,
			sess_private_data);

	if (ret != 0) {
		AESNI_MB_LOG(ERR, "Failed to configure session parameters");

		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_sec_session_private_data(sess, sess_private_data);

	return ret;
}

/** Clear the memory of session so it doesn't leave key material behind */
static int
aesni_mb_pmd_sec_sess_destroy(void *dev __rte_unused,
		struct rte_security_session *sess)
{
	void *sess_priv = get_sec_session_private_data(sess);

	if (sess_priv) {
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		memset(sess_priv, 0, sizeof(struct aesni_mb_session));
		set_sec_session_private_data(sess, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
	return 0;
}

/** Get security capabilities for aesni multi-buffer */
static const struct rte_security_capability *
aesni_mb_pmd_sec_capa_get(void *device __rte_unused)
{
	return aesni_mb_pmd_security_cap;
}

static struct rte_security_ops aesni_mb_pmd_sec_ops = {
		.session_create = aesni_mb_pmd_sec_sess_create,
		.session_update = NULL,
		.session_stats_get = NULL,
		.session_destroy = aesni_mb_pmd_sec_sess_destroy,
		.set_pkt_metadata = NULL,
		.capabilities_get = aesni_mb_pmd_sec_capa_get
};

struct rte_security_ops *rte_aesni_mb_pmd_sec_ops = &aesni_mb_pmd_sec_ops;
#endif
