/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_cryptodev_pmd.h>
#include <rte_malloc.h>

#include "ccp_pmd_private.h"
#include "ccp_dev.h"
#include "ccp_crypto.h"

#define CCP_BASE_SYM_CRYPTO_CAPABILITIES				\
	{	/* SHA1 */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				 .algo = RTE_CRYPTO_AUTH_SHA1,		\
				 .block_size = 64,			\
				 .key_size = {				\
					 .min = 0,			\
					 .max = 0,			\
					 .increment = 0			\
				 },					\
				 .digest_size = {			\
					 .min = 20,			\
					 .max = 20,			\
					 .increment = 0			\
				 },					\
				 .aad_size = { 0 }			\
			 }, }						\
		}, }							\
	},								\
	{	/* SHA1 HMAC */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				 .algo = RTE_CRYPTO_AUTH_SHA1_HMAC,     \
				 .block_size = 64,                      \
				 .key_size = {                          \
					 .min = 1,                      \
					 .max = 64,                     \
					 .increment = 1                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 20,                     \
					 .max = 20,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA224 */                                            \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA224,        \
				 .block_size = 64,                      \
				 .key_size = {                          \
					 .min = 0,                      \
					 .max = 0,			\
					 .increment = 0                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 28,                     \
					 .max = 28,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA224 HMAC */                                       \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA224_HMAC,   \
				 .block_size = 64,                      \
				 .key_size = {                          \
					 .min = 1,                      \
					 .max = 64,                     \
					 .increment = 1                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 28,                     \
					 .max = 28,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA3-224 */                                          \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA3_224,      \
				 .block_size = 144,                     \
				 .key_size = {                          \
					 .min = 0,                      \
					 .max = 0,                      \
					 .increment = 0                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 28,                     \
					 .max = 28,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }							\
	},                                                              \
	{	/* SHA3-224  HMAC*/                                     \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA3_224_HMAC, \
				 .block_size = 144,                     \
				 .key_size = {                          \
					 .min = 1,                      \
					 .max = 144,                    \
					 .increment = 1                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 28,                     \
					 .max = 28,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA256 */                                            \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA256,        \
				 .block_size = 64,                      \
				 .key_size = {                          \
					 .min = 0,                      \
					 .max = 0,                      \
					 .increment = 0                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 32,                     \
					 .max = 32,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA256 HMAC */                                       \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA256_HMAC,   \
				 .block_size = 64,                      \
				 .key_size = {                          \
					 .min = 1,                      \
					 .max = 64,                     \
					 .increment = 1                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 32,                     \
					 .max = 32,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA3-256 */                                          \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA3_256,      \
				 .block_size = 136,                     \
				 .key_size = {                          \
					 .min = 0,                      \
					 .max = 0,                      \
					 .increment = 0                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 32,                     \
					 .max = 32,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA3-256-HMAC */                                     \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA3_256_HMAC, \
				 .block_size = 136,                     \
				 .key_size = {                          \
					 .min = 1,                      \
					 .max = 136,                    \
					 .increment = 1                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 32,                     \
					 .max = 32,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }						\
		}, }                                                    \
	},                                                              \
	{	/* SHA384 */                                            \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA384,        \
				 .block_size = 128,                     \
				 .key_size = {                          \
					 .min = 0,                      \
					 .max = 0,                      \
					 .increment = 0                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 48,                     \
					 .max = 48,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA384 HMAC */                                       \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA384_HMAC,   \
				 .block_size = 128,                     \
				 .key_size = {                          \
					 .min = 1,                      \
					 .max = 128,                    \
					 .increment = 1                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 48,                     \
					 .max = 48,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA3-384 */                                          \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA3_384,      \
				 .block_size = 104,                     \
				 .key_size = {                          \
					 .min = 0,                      \
					 .max = 0,                      \
					 .increment = 0                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 48,                     \
					 .max = 48,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA3-384-HMAC */                                     \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA3_384_HMAC, \
				 .block_size = 104,                     \
				 .key_size = {                          \
					 .min = 1,                      \
					 .max = 104,                    \
					 .increment = 1                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 48,                     \
					 .max = 48,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA512  */                                           \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA512,        \
				 .block_size = 128,                     \
				 .key_size = {                          \
					 .min = 0,                      \
					 .max = 0,                      \
					 .increment = 0                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 64,                     \
					 .max = 64,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }			\
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA512 HMAC */                                       \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA512_HMAC,   \
				 .block_size = 128,                     \
				 .key_size = {                          \
					 .min = 1,                      \
					 .max = 128,                    \
					 .increment = 1                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 64,                     \
					 .max = 64,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA3-512  */                                         \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA3_512,      \
				 .block_size = 72,                      \
				 .key_size = {                          \
					 .min = 0,                      \
					 .max = 0,                      \
					 .increment = 0                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 64,                     \
					 .max = 64,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			 }, }                                           \
		}, }                                                    \
	},                                                              \
	{	/* SHA3-512-HMAC  */                                    \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_SHA3_512_HMAC, \
				 .block_size = 72,                      \
				 .key_size = {                          \
					 .min = 1,                      \
					 .max = 72,                     \
					 .increment = 1                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 64,                     \
					 .max = 64,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = { 0 }                      \
			}, }                                            \
		}, }                                                    \
	},                                                              \
	{	/*AES-CMAC */                                           \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,        \
			{.auth = {                                      \
				 .algo = RTE_CRYPTO_AUTH_AES_CMAC,      \
				 .block_size = 16,                      \
				 .key_size = {                          \
					 .min = 16,                     \
					 .max = 32,                     \
					 .increment = 8                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 16,                     \
					 .max = 16,                     \
					 .increment = 0                 \
				 },                                     \
			}, }                                            \
		}, }                                                    \
	},                                                              \
	{       /* AES ECB */                                           \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,      \
			{.cipher = {                                    \
				.algo = RTE_CRYPTO_CIPHER_AES_ECB,      \
				.block_size = 16,                       \
				.key_size = {                           \
				   .min = 16,                           \
				   .max = 32,                           \
				   .increment = 8                       \
				},                                      \
				.iv_size = {                            \
				   .min = 0,                            \
				   .max = 0,                            \
				   .increment = 0                       \
				}                                       \
			}, }						\
		}, }                                                    \
	},                                                              \
	{       /* AES CBC */                                           \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,      \
			{.cipher = {                                    \
				.algo = RTE_CRYPTO_CIPHER_AES_CBC,      \
				.block_size = 16,                       \
				.key_size = {                           \
					.min = 16,                      \
					.max = 32,                      \
					.increment = 8                  \
				},                                      \
				.iv_size = {                            \
					.min = 16,                      \
					.max = 16,                      \
					.increment = 0                  \
				}                                       \
			}, }                                            \
		}, }                                                    \
	},                                                              \
	{	/* AES CTR */                                           \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,      \
			{.cipher = {                                    \
				.algo = RTE_CRYPTO_CIPHER_AES_CTR,      \
				.block_size = 16,                       \
				.key_size = {                           \
					.min = 16,                      \
					.max = 32,                      \
					.increment = 8                  \
				},                                      \
				.iv_size = {                            \
					.min = 16,                      \
					.max = 16,                      \
					.increment = 0                  \
				}                                       \
			}, }                                            \
		}, }                                                    \
	},                                                              \
	{	/* 3DES CBC */                                          \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,      \
			{.cipher = {                                    \
				.algo = RTE_CRYPTO_CIPHER_3DES_CBC,     \
				.block_size = 8,                        \
				.key_size = {                           \
					.min = 16,                      \
					.max = 24,                      \
					.increment = 8                  \
				},                                      \
				.iv_size = {                            \
					.min = 8,                       \
					.max = 8,                       \
					.increment = 0                  \
				}                                       \
			}, }                                            \
		}, }                                                    \
	},                                                              \
	{       /* AES GCM */                                           \
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,                     \
		{.sym = {                                               \
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,        \
			{.aead = {                                      \
				 .algo = RTE_CRYPTO_AEAD_AES_GCM,       \
				 .block_size = 16,                      \
				 .key_size = {                          \
					 .min = 16,                     \
					 .max = 32,                     \
					 .increment = 8                 \
				 },                                     \
				 .digest_size = {                       \
					 .min = 16,                     \
					 .max = 16,                     \
					 .increment = 0                 \
				 },                                     \
				 .aad_size = {                          \
					 .min = 0,                      \
					 .max = 65535,                  \
					 .increment = 1                 \
				 },                                     \
				 .iv_size = {                           \
					 .min = 12,                     \
					 .max = 16,                     \
					 .increment = 4                 \
				 },                                     \
			}, }                                            \
		}, }                                                    \
	}

#define CCP_EXTRA_SYM_CRYPTO_CAPABILITIES				\
	{	/* MD5 HMAC */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				 .algo = RTE_CRYPTO_AUTH_MD5_HMAC,	\
				 .block_size = 64,			\
				 .key_size = {				\
					 .min = 1,			\
					 .max = 64,			\
					 .increment = 1			\
				 },					\
				 .digest_size = {			\
					 .min = 16,			\
					 .max = 16,			\
					 .increment = 0			\
				 },					\
				 .aad_size = { 0 }			\
			}, }						\
		}, }							\
	}

static const struct rte_cryptodev_capabilities ccp_crypto_cap[] = {
	CCP_BASE_SYM_CRYPTO_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_cryptodev_capabilities ccp_crypto_cap_complete[] = {
	CCP_EXTRA_SYM_CRYPTO_CAPABILITIES,
	CCP_BASE_SYM_CRYPTO_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static int
ccp_pmd_config(struct rte_cryptodev *dev __rte_unused,
	       struct rte_cryptodev_config *config __rte_unused)
{
	return 0;
}

static int
ccp_pmd_start(struct rte_cryptodev *dev)
{
	return ccp_dev_start(dev);
}

static void
ccp_pmd_stop(struct rte_cryptodev *dev __rte_unused)
{

}

static int
ccp_pmd_close(struct rte_cryptodev *dev __rte_unused)
{
	return 0;
}

static void
ccp_pmd_stats_get(struct rte_cryptodev *dev,
		  struct rte_cryptodev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct ccp_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}

}

static void
ccp_pmd_stats_reset(struct rte_cryptodev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct ccp_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}

static void
ccp_pmd_info_get(struct rte_cryptodev *dev,
		 struct rte_cryptodev_info *dev_info)
{
	struct ccp_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = ccp_crypto_cap;
		if (internals->auth_opt == 1)
			dev_info->capabilities = ccp_crypto_cap_complete;
		dev_info->max_nb_queue_pairs = internals->max_nb_qpairs;
		dev_info->sym.max_nb_sessions = internals->max_nb_sessions;
	}
}

static int
ccp_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct ccp_qp *qp;

	if (dev->data->queue_pairs[qp_id] != NULL) {
		qp = (struct ccp_qp *)dev->data->queue_pairs[qp_id];
		rte_ring_free(qp->processed_pkts);
		rte_mempool_free(qp->batch_mp);
		rte_free(qp);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

static int
ccp_pmd_qp_set_unique_name(struct rte_cryptodev *dev,
		struct ccp_qp *qp)
{
	unsigned int n = snprintf(qp->name, sizeof(qp->name),
			"ccp_pmd_%u_qp_%u",
			dev->data->dev_id, qp->id);

	if (n > sizeof(qp->name))
		return -1;

	return 0;
}

static struct rte_ring *
ccp_pmd_qp_create_batch_info_ring(struct ccp_qp *qp,
				  unsigned int ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (r->size >= ring_size) {
			CCP_LOG_INFO(
				"Reusing ring %s for processed packets",
				qp->name);
			return r;
		}
		CCP_LOG_INFO(
			"Unable to reuse ring %s for processed packets",
			 qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);
}

static int
ccp_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		 const struct rte_cryptodev_qp_conf *qp_conf,
		 int socket_id, struct rte_mempool *session_pool)
{
	struct ccp_private *internals = dev->data->dev_private;
	struct ccp_qp *qp;
	int retval = 0;

	if (qp_id >= internals->max_nb_qpairs) {
		CCP_LOG_ERR("Invalid qp_id %u, should be less than %u",
			    qp_id, internals->max_nb_qpairs);
		return (-EINVAL);
	}

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		ccp_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("CCP Crypto PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL) {
		CCP_LOG_ERR("Failed to allocate queue pair memory");
		return (-ENOMEM);
	}

	qp->dev = dev;
	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	retval = ccp_pmd_qp_set_unique_name(dev, qp);
	if (retval) {
		CCP_LOG_ERR("Failed to create unique name for ccp qp");
		goto qp_setup_cleanup;
	}

	qp->processed_pkts = ccp_pmd_qp_create_batch_info_ring(qp,
			qp_conf->nb_descriptors, socket_id);
	if (qp->processed_pkts == NULL) {
		CCP_LOG_ERR("Failed to create batch info ring");
		goto qp_setup_cleanup;
	}

	qp->sess_mp = session_pool;

	/* mempool for batch info */
	qp->batch_mp = rte_mempool_create(
				qp->name,
				qp_conf->nb_descriptors,
				sizeof(struct ccp_batch_info),
				RTE_CACHE_LINE_SIZE,
				0, NULL, NULL, NULL, NULL,
				SOCKET_ID_ANY, 0);
	if (qp->batch_mp == NULL)
		goto qp_setup_cleanup;
	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	return 0;

qp_setup_cleanup:
	dev->data->queue_pairs[qp_id] = NULL;
	if (qp)
		rte_free(qp);
	return -1;
}

static int
ccp_pmd_qp_start(struct rte_cryptodev *dev __rte_unused,
		 uint16_t queue_pair_id __rte_unused)
{
	return -ENOTSUP;
}

static int
ccp_pmd_qp_stop(struct rte_cryptodev *dev __rte_unused,
		uint16_t queue_pair_id __rte_unused)
{
	return -ENOTSUP;
}

static uint32_t
ccp_pmd_qp_count(struct rte_cryptodev *dev)
{
	return dev->data->nb_queue_pairs;
}

static unsigned
ccp_pmd_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct ccp_session);
}

static int
ccp_pmd_session_configure(struct rte_cryptodev *dev,
			  struct rte_crypto_sym_xform *xform,
			  struct rte_cryptodev_sym_session *sess,
			  struct rte_mempool *mempool)
{
	int ret;
	void *sess_private_data;
	struct ccp_private *internals;

	if (unlikely(sess == NULL || xform == NULL)) {
		CCP_LOG_ERR("Invalid session struct or xform");
		return -ENOMEM;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		CCP_LOG_ERR("Couldn't get object from session mempool");
		return -ENOMEM;
	}
	internals = (struct ccp_private *)dev->data->dev_private;
	ret = ccp_set_session_parameters(sess_private_data, xform, internals);
	if (ret != 0) {
		CCP_LOG_ERR("failed configure session parameters");

		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}
	set_session_private_data(sess, dev->driver_id,
				 sess_private_data);

	return 0;
}

static void
ccp_pmd_session_clear(struct rte_cryptodev *dev,
		      struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_session_private_data(sess, index);

	if (sess_priv) {
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);

		rte_mempool_put(sess_mp, sess_priv);
		memset(sess_priv, 0, sizeof(struct ccp_session));
		set_session_private_data(sess, index, NULL);
	}
}

struct rte_cryptodev_ops ccp_ops = {
		.dev_configure		= ccp_pmd_config,
		.dev_start		= ccp_pmd_start,
		.dev_stop		= ccp_pmd_stop,
		.dev_close		= ccp_pmd_close,

		.stats_get		= ccp_pmd_stats_get,
		.stats_reset		= ccp_pmd_stats_reset,

		.dev_infos_get		= ccp_pmd_info_get,

		.queue_pair_setup	= ccp_pmd_qp_setup,
		.queue_pair_release	= ccp_pmd_qp_release,
		.queue_pair_start	= ccp_pmd_qp_start,
		.queue_pair_stop	= ccp_pmd_qp_stop,
		.queue_pair_count	= ccp_pmd_qp_count,

		.session_get_size	= ccp_pmd_session_get_size,
		.session_configure	= ccp_pmd_session_configure,
		.session_clear		= ccp_pmd_session_clear,
};

struct rte_cryptodev_ops *ccp_pmd_ops = &ccp_ops;
