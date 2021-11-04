/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2021 Intel Corporation
 */

#include <rte_cryptodev.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security_driver.h>
#endif

#include "adf_transport_access_macros.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_la.h"

#include "qat_sym.h"
#include "qat_sym_session.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen1[] = {
	QAT_SYM_PLAIN_AUTH_CAP(SHA1,
		CAP_SET(block_size, 64),
		CAP_RNG(digest_size, 1, 20, 1)),
	QAT_SYM_AEAD_CAP(AES_GCM,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(digest_size, 8, 16, 4),
		CAP_RNG(aad_size, 0, 240, 1), CAP_RNG(iv_size, 0, 12, 12)),
	QAT_SYM_AEAD_CAP(AES_CCM,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 16, 2),
		CAP_RNG(aad_size, 0, 224, 1), CAP_RNG(iv_size, 7, 13, 1)),
	QAT_SYM_AUTH_CAP(AES_GMAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(digest_size, 8, 16, 4),
		CAP_RNG_ZERO(aad_size), CAP_RNG(iv_size, 0, 12, 12)),
	QAT_SYM_AUTH_CAP(AES_CMAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 16, 4),
			CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA224,
		CAP_SET(block_size, 64),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA256,
		CAP_SET(block_size, 64),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 32, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA384,
		CAP_SET(block_size, 128),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 48, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA512,
		CAP_SET(block_size, 128),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 64, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA1_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 20, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA224_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA256_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 32, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA384_HMAC,
		CAP_SET(block_size, 128),
		CAP_RNG(key_size, 1, 128, 1), CAP_RNG(digest_size, 1, 48, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA512_HMAC,
		CAP_SET(block_size, 128),
		CAP_RNG(key_size, 1, 128, 1), CAP_RNG(digest_size, 1, 64, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(MD5_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 16, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(AES_XCBC_MAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 12, 12, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SNOW3G_UIA2,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 4, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AUTH_CAP(KASUMI_F9,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 4, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(digest_size),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(AES_CBC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(AES_CTR,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(AES_XTS,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 32, 64, 32), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(AES_DOCSISBPI,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 16), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(SNOW3G_UEA2,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(KASUMI_F8,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(3DES_CBC,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(3DES_CTR,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 16, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(DES_CBC,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 24, 8), CAP_RNG(iv_size, 8, 8, 0)),
	QAT_SYM_CIPHER_CAP(DES_DOCSISBPI,
		CAP_SET(block_size, 8),
		CAP_RNG(key_size, 8, 8, 0), CAP_RNG(iv_size, 8, 8, 0)),
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

struct rte_cryptodev_ops qat_sym_crypto_ops_gen1 = {

	/* Device related operations */
	.dev_configure		= qat_cryptodev_config,
	.dev_start		= qat_cryptodev_start,
	.dev_stop		= qat_cryptodev_stop,
	.dev_close		= qat_cryptodev_close,
	.dev_infos_get		= qat_cryptodev_info_get,

	.stats_get		= qat_cryptodev_stats_get,
	.stats_reset		= qat_cryptodev_stats_reset,
	.queue_pair_setup	= qat_cryptodev_qp_setup,
	.queue_pair_release	= qat_cryptodev_qp_release,

	/* Crypto related operations */
	.sym_session_get_size	= qat_sym_session_get_private_size,
	.sym_session_configure	= qat_sym_session_configure,
	.sym_session_clear	= qat_sym_session_clear,

	/* Raw data-path API related operations */
	.sym_get_raw_dp_ctx_size = qat_sym_get_dp_ctx_size,
	.sym_configure_raw_dp_ctx = qat_sym_configure_dp_ctx,
};

static struct qat_capabilities_info
qat_sym_crypto_cap_get_gen1(struct qat_pci_device *qat_dev __rte_unused)
{
	struct qat_capabilities_info capa_info;
	capa_info.data = qat_sym_crypto_caps_gen1;
	capa_info.size = sizeof(qat_sym_crypto_caps_gen1);
	return capa_info;
}

uint64_t
qat_sym_crypto_feature_flags_get_gen1(
	struct qat_pci_device *qat_dev __rte_unused)
{
	uint64_t feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
			RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED |
			RTE_CRYPTODEV_FF_SYM_RAW_DP;

	return feature_flags;
}

#ifdef RTE_LIB_SECURITY

#define QAT_SECURITY_SYM_CAPABILITIES					\
	{	/* AES DOCSIS BPI */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_AES_DOCSISBPI,\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 32,			\
					.increment = 16			\
				},					\
				.iv_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	}

#define QAT_SECURITY_CAPABILITIES(sym)					\
	[0] = {	/* DOCSIS Uplink */					\
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,	\
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,		\
		.docsis = {						\
			.direction = RTE_SECURITY_DOCSIS_UPLINK		\
		},							\
		.crypto_capabilities = (sym)				\
	},								\
	[1] = {	/* DOCSIS Downlink */					\
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,	\
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,		\
		.docsis = {						\
			.direction = RTE_SECURITY_DOCSIS_DOWNLINK	\
		},							\
		.crypto_capabilities = (sym)				\
	}

static const struct rte_cryptodev_capabilities
					qat_security_sym_capabilities[] = {
	QAT_SECURITY_SYM_CAPABILITIES,
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_security_capability qat_security_capabilities_gen1[] = {
	QAT_SECURITY_CAPABILITIES(qat_security_sym_capabilities),
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

static const struct rte_security_capability *
qat_security_cap_get_gen1(void *dev __rte_unused)
{
	return qat_security_capabilities_gen1;
}

struct rte_security_ops security_qat_ops_gen1 = {
		.session_create = qat_security_session_create,
		.session_update = NULL,
		.session_stats_get = NULL,
		.session_destroy = qat_security_session_destroy,
		.set_pkt_metadata = NULL,
		.capabilities_get = qat_security_cap_get_gen1
};

void *
qat_sym_create_security_gen1(void *cryptodev)
{
	struct rte_security_ctx *security_instance;

	security_instance = rte_malloc(NULL, sizeof(struct rte_security_ctx),
			RTE_CACHE_LINE_SIZE);
	if (security_instance == NULL)
		return NULL;

	security_instance->device = cryptodev;
	security_instance->ops = &security_qat_ops_gen1;
	security_instance->sess_cnt = 0;

	return (void *)security_instance;
}

#endif

RTE_INIT(qat_sym_crypto_gen1_init)
{
	qat_sym_gen_dev_ops[QAT_GEN1].cryptodev_ops = &qat_sym_crypto_ops_gen1;
	qat_sym_gen_dev_ops[QAT_GEN1].get_capabilities =
			qat_sym_crypto_cap_get_gen1;
	qat_sym_gen_dev_ops[QAT_GEN1].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen1;
#ifdef RTE_LIB_SECURITY
	qat_sym_gen_dev_ops[QAT_GEN1].create_security_ctx =
			qat_sym_create_security_gen1;
#endif
}
