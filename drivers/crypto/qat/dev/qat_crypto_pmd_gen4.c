/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2021 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include "qat_sym_session.h"
#include "qat_sym.h"
#include "qat_asym.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen4[] = {
	QAT_SYM_CIPHER_CAP(AES_CBC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
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
	QAT_SYM_AUTH_CAP(AES_XCBC_MAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 12, 12, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(AES_CMAC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 16, 4),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(AES_DOCSISBPI,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 16), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AUTH_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(digest_size),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(NULL,
		CAP_SET(block_size, 1),
		CAP_RNG_ZERO(key_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_PLAIN_AUTH_CAP(SHA1,
		CAP_SET(block_size, 64),
		CAP_RNG(digest_size, 1, 20, 1)),
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
	QAT_SYM_CIPHER_CAP(AES_CTR,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
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
	QAT_SYM_AEAD_CAP(CHACHA20_POLY1305,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 32, 32, 0),
		CAP_RNG(digest_size, 16, 16, 0),
		CAP_RNG(aad_size, 0, 240, 1), CAP_RNG(iv_size, 12, 12, 0)),
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static struct qat_capabilities_info
qat_sym_crypto_cap_get_gen4(struct qat_pci_device *qat_dev __rte_unused)
{
	struct qat_capabilities_info capa_info;
	capa_info.data = qat_sym_crypto_caps_gen4;
	capa_info.size = sizeof(qat_sym_crypto_caps_gen4);
	return capa_info;
}

RTE_INIT(qat_sym_crypto_gen4_init)
{
	qat_sym_gen_dev_ops[QAT_GEN4].cryptodev_ops = &qat_sym_crypto_ops_gen1;
	qat_sym_gen_dev_ops[QAT_GEN4].get_capabilities =
			qat_sym_crypto_cap_get_gen4;
	qat_sym_gen_dev_ops[QAT_GEN4].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen1;
#ifdef RTE_LIB_SECURITY
	qat_sym_gen_dev_ops[QAT_GEN4].create_security_ctx =
			qat_sym_create_security_gen1;
#endif
}

RTE_INIT(qat_asym_crypto_gen4_init)
{
	qat_asym_gen_dev_ops[QAT_GEN4].cryptodev_ops = NULL;
	qat_asym_gen_dev_ops[QAT_GEN4].get_capabilities = NULL;
	qat_asym_gen_dev_ops[QAT_GEN4].get_feature_flags = NULL;
}
