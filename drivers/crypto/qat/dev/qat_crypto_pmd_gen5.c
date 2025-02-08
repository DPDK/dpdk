/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include "qat_sym_session.h"
#include "qat_sym.h"
#include "qat_asym.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"


static struct rte_cryptodev_capabilities qat_sym_crypto_legacy_caps_gen5[] = {
	QAT_SYM_PLAIN_AUTH_CAP(SHA1,
		CAP_SET(block_size, 64),
		CAP_RNG(digest_size, 1, 20, 1)),
	QAT_SYM_AUTH_CAP(SHA224,
		CAP_SET(block_size, 64),
		CAP_RNG_ZERO(key_size), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA224_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 28, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_AUTH_CAP(SHA1_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 1, 64, 1), CAP_RNG(digest_size, 1, 20, 1),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(SM4_ECB,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 0, 0, 0)),
};

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen5[] = {
	QAT_SYM_CIPHER_CAP(AES_CBC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 8), CAP_RNG(iv_size, 16, 16, 0)),
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
	QAT_SYM_CIPHER_CAP(SM4_CBC,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_CIPHER_CAP(SM4_CTR,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_PLAIN_AUTH_CAP(SM3,
		CAP_SET(block_size, 64),
		CAP_RNG(digest_size, 32, 32, 0)),
	QAT_SYM_AUTH_CAP(SM3_HMAC,
		CAP_SET(block_size, 64),
		CAP_RNG(key_size, 16, 64, 4), CAP_RNG(digest_size, 32, 32, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG_ZERO(iv_size)),
	QAT_SYM_CIPHER_CAP(ZUC_EEA3,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 16), CAP_RNG(iv_size, 16, 25, 1)),
	QAT_SYM_AUTH_CAP(ZUC_EIA3,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 32, 16), CAP_RNG(digest_size, 4, 16, 4),
		CAP_RNG_ZERO(aad_size), CAP_RNG(iv_size, 16, 25, 1)),
	QAT_SYM_CIPHER_CAP(SNOW3G_UEA2,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(iv_size, 16, 16, 0)),
	QAT_SYM_AUTH_CAP(SNOW3G_UIA2,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 16, 16, 0), CAP_RNG(digest_size, 4, 4, 0),
		CAP_RNG_ZERO(aad_size), CAP_RNG(iv_size, 16, 16, 0)),
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static int
check_cipher_capa(const struct rte_cryptodev_capabilities *cap,
		enum rte_crypto_cipher_algorithm algo)
{
	if (cap->op != RTE_CRYPTO_OP_TYPE_SYMMETRIC)
		return 0;
	if (cap->sym.xform_type != RTE_CRYPTO_SYM_XFORM_CIPHER)
		return 0;
	if (cap->sym.cipher.algo != algo)
		return 0;
	return 1;
}

static int
check_auth_capa(const struct rte_cryptodev_capabilities *cap,
		enum rte_crypto_auth_algorithm algo)
{
	if (cap->op != RTE_CRYPTO_OP_TYPE_SYMMETRIC)
		return 0;
	if (cap->sym.xform_type != RTE_CRYPTO_SYM_XFORM_AUTH)
		return 0;
	if (cap->sym.auth.algo != algo)
		return 0;
	return 1;
}

static int
qat_sym_crypto_cap_get_gen5(struct qat_cryptodev_private *internals,
			const char *capa_memz_name,
			const uint16_t __rte_unused slice_map)
{
	uint32_t legacy_capa_num, capa_num;
	uint32_t size = sizeof(qat_sym_crypto_caps_gen5);
	uint32_t legacy_size = sizeof(qat_sym_crypto_legacy_caps_gen5);
	uint32_t i, iter = 0;
	uint32_t curr_capa = 0;
	legacy_capa_num = legacy_size/sizeof(struct rte_cryptodev_capabilities);
	capa_num = RTE_DIM(qat_sym_crypto_caps_gen5);

	if (unlikely(internals->qat_dev->options.legacy_alg))
		size = size + legacy_size;

	internals->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (internals->capa_mz == NULL) {
		internals->capa_mz = rte_memzone_reserve(capa_memz_name,
				size, rte_socket_id(), 0);
		if (internals->capa_mz == NULL) {
			QAT_LOG(DEBUG,
				"Error allocating memzone for capabilities");
			return -1;
		}
	}

	struct rte_cryptodev_capabilities *addr =
			(struct rte_cryptodev_capabilities *)
				internals->capa_mz->addr;

	struct rte_cryptodev_capabilities *capabilities;

	if (unlikely(internals->qat_dev->options.legacy_alg)) {
		capabilities = qat_sym_crypto_legacy_caps_gen5;
		memcpy(addr, capabilities, legacy_size);
		addr += legacy_capa_num;
	}
	capabilities = qat_sym_crypto_caps_gen5;

	for (i = 0; i < capa_num; i++, iter++) {
		if (slice_map & ICP_ACCEL_MASK_ZUC_256_SLICE && (
			check_auth_capa(&capabilities[iter],
				RTE_CRYPTO_AUTH_ZUC_EIA3) ||
			check_cipher_capa(&capabilities[iter],
				RTE_CRYPTO_CIPHER_ZUC_EEA3))) {
			continue;
		}

		*(addr + curr_capa) = *(capabilities + iter);
		curr_capa++;
	}
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	return 0;
}

static int
qat_sym_crypto_set_session_gen5(void *cdev, void *session)
{
	struct qat_sym_session *ctx = session;
	enum rte_proc_type_t proc_type = rte_eal_process_type();
	int ret;

	if (proc_type == RTE_PROC_AUTO || proc_type == RTE_PROC_INVALID)
		return -EINVAL;

	ret = qat_sym_crypto_set_session_gen4(cdev, session);

	if (ret == -ENOTSUP) {
		/* GEN4 returning -ENOTSUP as it cannot handle some mixed algo,
		 * this is addressed by GEN5
		 */
		if ((ctx->aes_cmac ||
				ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_NULL) &&
				(ctx->qat_cipher_alg ==
				ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2 ||
				ctx->qat_cipher_alg ==
				ICP_QAT_HW_CIPHER_ALGO_ZUC_3G_128_EEA3 ||
				ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_ZUC_256)) {
			qat_sym_session_set_ext_hash_flags_gen2(ctx, 0);
		} else if ((ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_ZUC_256_MAC_32 ||
				ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_ZUC_256_MAC_64 ||
				ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_ZUC_256_MAC_128) &&
				ctx->qat_cipher_alg != ICP_QAT_HW_CIPHER_ALGO_ZUC_256) {
			qat_sym_session_set_ext_hash_flags_gen2(ctx,
					1 << ICP_QAT_FW_AUTH_HDR_FLAG_ZUC_EIA3_BITPOS);
		}

		ret = 0;
	}

	return ret;
}

RTE_INIT(qat_sym_crypto_gen5_init)
{
	qat_sym_gen_dev_ops[QAT_GEN5].cryptodev_ops = &qat_sym_crypto_ops_gen1;
	qat_sym_gen_dev_ops[QAT_GEN5].get_capabilities =
			qat_sym_crypto_cap_get_gen5;
	qat_sym_gen_dev_ops[QAT_GEN5].set_session =
			qat_sym_crypto_set_session_gen5;
	qat_sym_gen_dev_ops[QAT_GEN5].set_raw_dp_ctx =
			qat_sym_configure_raw_dp_ctx_gen4;
	qat_sym_gen_dev_ops[QAT_GEN5].get_feature_flags =
			qat_sym_crypto_feature_flags_get_gen1;
	qat_sym_gen_dev_ops[QAT_GEN5].create_security_ctx =
			qat_sym_create_security_gen1;
}

RTE_INIT(qat_asym_crypto_gen5_init)
{
	qat_asym_gen_dev_ops[QAT_GEN5].cryptodev_ops =
			&qat_asym_crypto_ops_gen1;
	qat_asym_gen_dev_ops[QAT_GEN5].get_capabilities =
			qat_asym_crypto_cap_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN5].get_feature_flags =
			qat_asym_crypto_feature_flags_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN5].set_session =
			qat_asym_crypto_set_session_gen1;
}
