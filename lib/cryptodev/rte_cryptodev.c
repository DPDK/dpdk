/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Intel Corporation
 */

#include <sys/queue.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <eal_export.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <dev_driver.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_telemetry.h>

#include "rte_crypto.h"
#include "rte_cryptodev.h"
#include "cryptodev_pmd.h"
#include "cryptodev_trace.h"

static uint8_t nb_drivers;

static struct rte_cryptodev rte_crypto_devices[RTE_CRYPTO_MAX_DEVS];

RTE_EXPORT_INTERNAL_SYMBOL(rte_cryptodevs)
struct rte_cryptodev *rte_cryptodevs = rte_crypto_devices;

static struct rte_cryptodev_global cryptodev_globals = {
		.devs			= rte_crypto_devices,
		.data			= { NULL },
		.nb_devs		= 0
};

/* Public fastpath APIs. */
RTE_EXPORT_SYMBOL(rte_crypto_fp_ops)
struct rte_crypto_fp_ops rte_crypto_fp_ops[RTE_CRYPTO_MAX_DEVS];

/* spinlock for crypto device callbacks */
static rte_spinlock_t rte_cryptodev_cb_lock = RTE_SPINLOCK_INITIALIZER;

RTE_EXPORT_SYMBOL(rte_cryptodev_logtype)
RTE_LOG_REGISTER_DEFAULT(rte_cryptodev_logtype, INFO);

/**
 * The user application callback description.
 *
 * It contains callback address to be registered by user application,
 * the pointer to the parameters for callback, and the event type.
 */
struct rte_cryptodev_callback {
	TAILQ_ENTRY(rte_cryptodev_callback) next; /**< Callbacks list */
	rte_cryptodev_cb_fn cb_fn;		/**< Callback address */
	void *cb_arg;				/**< Parameter for callback */
	enum rte_cryptodev_event_type event;	/**< Interrupt event type */
	uint32_t active;			/**< Callback is executing */
};

/**
 * The crypto cipher algorithm strings identifiers.
 * Not to be used in application directly.
 * Application can use rte_cryptodev_get_cipher_algo_string().
 */
static const char *
crypto_cipher_algorithm_strings[] = {
	[RTE_CRYPTO_CIPHER_3DES_CBC]	= "3des-cbc",
	[RTE_CRYPTO_CIPHER_3DES_ECB]	= "3des-ecb",
	[RTE_CRYPTO_CIPHER_3DES_CTR]	= "3des-ctr",

	[RTE_CRYPTO_CIPHER_AES_CBC]	= "aes-cbc",
	[RTE_CRYPTO_CIPHER_AES_CTR]	= "aes-ctr",
	[RTE_CRYPTO_CIPHER_AES_DOCSISBPI]	= "aes-docsisbpi",
	[RTE_CRYPTO_CIPHER_AES_ECB]	= "aes-ecb",
	[RTE_CRYPTO_CIPHER_AES_F8]	= "aes-f8",
	[RTE_CRYPTO_CIPHER_AES_XTS]	= "aes-xts",

	[RTE_CRYPTO_CIPHER_ARC4]	= "arc4",

	[RTE_CRYPTO_CIPHER_DES_CBC]     = "des-cbc",
	[RTE_CRYPTO_CIPHER_DES_DOCSISBPI]	= "des-docsisbpi",

	[RTE_CRYPTO_CIPHER_NULL]	= "null",

	[RTE_CRYPTO_CIPHER_KASUMI_F8]	= "kasumi-f8",
	[RTE_CRYPTO_CIPHER_SNOW3G_UEA2]	= "snow3g-uea2",
	[RTE_CRYPTO_CIPHER_ZUC_EEA3]	= "zuc-eea3",
	[RTE_CRYPTO_CIPHER_SM4_ECB]	= "sm4-ecb",
	[RTE_CRYPTO_CIPHER_SM4_CBC]	= "sm4-cbc",
	[RTE_CRYPTO_CIPHER_SM4_CTR]	= "sm4-ctr",
	[RTE_CRYPTO_CIPHER_SM4_CFB]	= "sm4-cfb",
	[RTE_CRYPTO_CIPHER_SM4_OFB]	= "sm4-ofb",
	[RTE_CRYPTO_CIPHER_SM4_XTS]	= "sm4-xts"
};

/**
 * The crypto cipher operation strings identifiers.
 * It could be used in application command line.
 */
RTE_EXPORT_SYMBOL(rte_crypto_cipher_operation_strings)
const char *
rte_crypto_cipher_operation_strings[] = {
		[RTE_CRYPTO_CIPHER_OP_ENCRYPT]	= "encrypt",
		[RTE_CRYPTO_CIPHER_OP_DECRYPT]	= "decrypt"
};

/**
 * The crypto auth algorithm strings identifiers.
 * Not to be used in application directly.
 * Application can use rte_cryptodev_get_auth_algo_string().
 */
static const char *
crypto_auth_algorithm_strings[] = {
	[RTE_CRYPTO_AUTH_AES_CBC_MAC]	= "aes-cbc-mac",
	[RTE_CRYPTO_AUTH_AES_CMAC]	= "aes-cmac",
	[RTE_CRYPTO_AUTH_AES_GMAC]	= "aes-gmac",
	[RTE_CRYPTO_AUTH_AES_XCBC_MAC]	= "aes-xcbc-mac",

	[RTE_CRYPTO_AUTH_MD5]		= "md5",
	[RTE_CRYPTO_AUTH_MD5_HMAC]	= "md5-hmac",

	[RTE_CRYPTO_AUTH_NULL]		= "null",

	[RTE_CRYPTO_AUTH_SHA1]		= "sha1",
	[RTE_CRYPTO_AUTH_SHA1_HMAC]	= "sha1-hmac",

	[RTE_CRYPTO_AUTH_SHA224]	= "sha2-224",
	[RTE_CRYPTO_AUTH_SHA224_HMAC]	= "sha2-224-hmac",
	[RTE_CRYPTO_AUTH_SHA256]	= "sha2-256",
	[RTE_CRYPTO_AUTH_SHA256_HMAC]	= "sha2-256-hmac",
	[RTE_CRYPTO_AUTH_SHA384]	= "sha2-384",
	[RTE_CRYPTO_AUTH_SHA384_HMAC]	= "sha2-384-hmac",
	[RTE_CRYPTO_AUTH_SHA512]	= "sha2-512",
	[RTE_CRYPTO_AUTH_SHA512_HMAC]	= "sha2-512-hmac",

	[RTE_CRYPTO_AUTH_SHA3_224]	= "sha3-224",
	[RTE_CRYPTO_AUTH_SHA3_224_HMAC] = "sha3-224-hmac",
	[RTE_CRYPTO_AUTH_SHA3_256]	= "sha3-256",
	[RTE_CRYPTO_AUTH_SHA3_256_HMAC] = "sha3-256-hmac",
	[RTE_CRYPTO_AUTH_SHA3_384]	= "sha3-384",
	[RTE_CRYPTO_AUTH_SHA3_384_HMAC] = "sha3-384-hmac",
	[RTE_CRYPTO_AUTH_SHA3_512]	= "sha3-512",
	[RTE_CRYPTO_AUTH_SHA3_512_HMAC]	= "sha3-512-hmac",

	[RTE_CRYPTO_AUTH_KASUMI_F9]	= "kasumi-f9",
	[RTE_CRYPTO_AUTH_SNOW3G_UIA2]	= "snow3g-uia2",
	[RTE_CRYPTO_AUTH_ZUC_EIA3]	= "zuc-eia3",
	[RTE_CRYPTO_AUTH_SM3]		= "sm3",
	[RTE_CRYPTO_AUTH_SM3_HMAC]	= "sm3-hmac",

	[RTE_CRYPTO_AUTH_SHAKE_128]	 = "shake-128",
	[RTE_CRYPTO_AUTH_SHAKE_256]	 = "shake-256",
};

/**
 * The crypto AEAD algorithm strings identifiers.
 * Not to be used in application directly.
 * Application can use rte_cryptodev_get_aead_algo_string().
 */
static const char *
crypto_aead_algorithm_strings[] = {
	[RTE_CRYPTO_AEAD_AES_CCM]	= "aes-ccm",
	[RTE_CRYPTO_AEAD_AES_GCM]	= "aes-gcm",
	[RTE_CRYPTO_AEAD_CHACHA20_POLY1305] = "chacha20-poly1305",
	[RTE_CRYPTO_AEAD_SM4_GCM]   = "sm4-gcm",
};


/**
 * The crypto AEAD operation strings identifiers.
 * It could be used in application command line.
 */
RTE_EXPORT_SYMBOL(rte_crypto_aead_operation_strings)
const char *
rte_crypto_aead_operation_strings[] = {
	[RTE_CRYPTO_AEAD_OP_ENCRYPT]	= "encrypt",
	[RTE_CRYPTO_AEAD_OP_DECRYPT]	= "decrypt"
};

/**
 * Asymmetric crypto transform operation strings identifiers.
 * Not to be used in application directly.
 * Application can use rte_cryptodev_asym_get_xform_string().
 */
static const char *
crypto_asym_xform_strings[] = {
	[RTE_CRYPTO_ASYM_XFORM_NONE]	= "none",
	[RTE_CRYPTO_ASYM_XFORM_RSA]	= "rsa",
	[RTE_CRYPTO_ASYM_XFORM_MODEX]	= "modexp",
	[RTE_CRYPTO_ASYM_XFORM_MODINV]	= "modinv",
	[RTE_CRYPTO_ASYM_XFORM_DH]	= "dh",
	[RTE_CRYPTO_ASYM_XFORM_DSA]	= "dsa",
	[RTE_CRYPTO_ASYM_XFORM_ECDSA]	= "ecdsa",
	[RTE_CRYPTO_ASYM_XFORM_ECPM]	= "ecpm",
	[RTE_CRYPTO_ASYM_XFORM_SM2]	= "sm2",
};

/**
 * Asymmetric crypto operation strings identifiers.
 */
RTE_EXPORT_SYMBOL(rte_crypto_asym_op_strings)
const char *rte_crypto_asym_op_strings[] = {
	[RTE_CRYPTO_ASYM_OP_ENCRYPT]	= "encrypt",
	[RTE_CRYPTO_ASYM_OP_DECRYPT]	= "decrypt",
	[RTE_CRYPTO_ASYM_OP_SIGN]	= "sign",
	[RTE_CRYPTO_ASYM_OP_VERIFY]	= "verify"
};

/**
 * Asymmetric crypto key exchange operation strings identifiers.
 */
RTE_EXPORT_SYMBOL(rte_crypto_asym_ke_strings)
const char *rte_crypto_asym_ke_strings[] = {
	[RTE_CRYPTO_ASYM_KE_PRIV_KEY_GENERATE] = "priv_key_generate",
	[RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE] = "pub_key_generate",
	[RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE] = "sharedsecret_compute",
	[RTE_CRYPTO_ASYM_KE_PUB_KEY_VERIFY] = "pub_ec_key_verify"
};

/**
 * Public key size used in PQC ML-KEM based crypto ops.
 */
RTE_EXPORT_SYMBOL(rte_crypto_ml_kem_pubkey_size)
const uint16_t rte_crypto_ml_kem_pubkey_size[] = {
	[RTE_CRYPTO_ML_KEM_512] = 800,
	[RTE_CRYPTO_ML_KEM_768] = 1184,
	[RTE_CRYPTO_ML_KEM_1024] = 1568,
};

/**
 * Private key size used in PQC ML-KEM based crypto ops.
 */
RTE_EXPORT_SYMBOL(rte_crypto_ml_kem_privkey_size)
const uint16_t rte_crypto_ml_kem_privkey_size[] = {
	[RTE_CRYPTO_ML_KEM_512] = 1632,
	[RTE_CRYPTO_ML_KEM_768] = 2400,
	[RTE_CRYPTO_ML_KEM_1024] = 3168,
};

/**
 * Cipher size used in PQC ML-KEM based crypto ops.
 */
RTE_EXPORT_SYMBOL(rte_crypto_ml_kem_cipher_size)
const uint16_t rte_crypto_ml_kem_cipher_size[] = {
	[RTE_CRYPTO_ML_KEM_512] = 768,
	[RTE_CRYPTO_ML_KEM_768] = 1088,
	[RTE_CRYPTO_ML_KEM_1024] = 1568,
};

/**
 * Public key size used in PQC ML-DSA based crypto ops.
 */
RTE_EXPORT_SYMBOL(rte_crypto_ml_dsa_pubkey_size)
const uint16_t rte_crypto_ml_dsa_pubkey_size[] = {
	[RTE_CRYPTO_ML_DSA_44] = 1312,
	[RTE_CRYPTO_ML_DSA_65] = 1952,
	[RTE_CRYPTO_ML_DSA_87] = 2592,
};

/**
 * Private key size used in PQC ML-DSA based crypto ops.
 */
RTE_EXPORT_SYMBOL(rte_crypto_ml_dsa_privkey_size)
const uint16_t rte_crypto_ml_dsa_privkey_size[] = {
	[RTE_CRYPTO_ML_DSA_44] = 2560,
	[RTE_CRYPTO_ML_DSA_65] = 4032,
	[RTE_CRYPTO_ML_DSA_87] = 4896,
};

/**
 * Sign size used in PQC ML-KEM based crypto ops.
 */
RTE_EXPORT_SYMBOL(rte_crypto_ml_dsa_sign_size)
const uint16_t rte_crypto_ml_dsa_sign_size[] = {
	[RTE_CRYPTO_ML_DSA_44] = 2420,
	[RTE_CRYPTO_ML_DSA_65] = 3309,
	[RTE_CRYPTO_ML_DSA_87] = 4627,
};

struct rte_cryptodev_sym_session_pool_private_data {
	uint16_t sess_data_sz;
	/**< driver session data size */
	uint16_t user_data_sz;
	/**< session user data will be placed after sess_data */
};

/**
 * The private data structure stored in the asym session mempool private data.
 */
struct rte_cryptodev_asym_session_pool_private_data {
	uint16_t max_priv_session_sz;
	/**< Size of private session data used when creating mempool */
	uint16_t user_data_sz;
	/**< Session user data will be placed after sess_private_data */
};

RTE_EXPORT_SYMBOL(rte_cryptodev_get_cipher_algo_enum)
int
rte_cryptodev_get_cipher_algo_enum(enum rte_crypto_cipher_algorithm *algo_enum,
		const char *algo_string)
{
	unsigned int i;
	int ret = -1;	/* Invalid string */

	for (i = 1; i < RTE_DIM(crypto_cipher_algorithm_strings); i++) {
		if (strcmp(algo_string, crypto_cipher_algorithm_strings[i]) == 0) {
			*algo_enum = (enum rte_crypto_cipher_algorithm) i;
			ret = 0;
			break;
		}
	}

	rte_cryptodev_trace_get_cipher_algo_enum(algo_string, *algo_enum, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_get_auth_algo_enum)
int
rte_cryptodev_get_auth_algo_enum(enum rte_crypto_auth_algorithm *algo_enum,
		const char *algo_string)
{
	unsigned int i;
	int ret = -1;	/* Invalid string */

	for (i = 1; i < RTE_DIM(crypto_auth_algorithm_strings); i++) {
		if (strcmp(algo_string, crypto_auth_algorithm_strings[i]) == 0) {
			*algo_enum = (enum rte_crypto_auth_algorithm) i;
			ret = 0;
			break;
		}
	}

	rte_cryptodev_trace_get_auth_algo_enum(algo_string, *algo_enum, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_get_aead_algo_enum)
int
rte_cryptodev_get_aead_algo_enum(enum rte_crypto_aead_algorithm *algo_enum,
		const char *algo_string)
{
	unsigned int i;
	int ret = -1;	/* Invalid string */

	for (i = 1; i < RTE_DIM(crypto_aead_algorithm_strings); i++) {
		if (strcmp(algo_string, crypto_aead_algorithm_strings[i]) == 0) {
			*algo_enum = (enum rte_crypto_aead_algorithm) i;
			ret = 0;
			break;
		}
	}

	rte_cryptodev_trace_get_aead_algo_enum(algo_string, *algo_enum, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_get_xform_enum)
int
rte_cryptodev_asym_get_xform_enum(enum rte_crypto_asym_xform_type *xform_enum,
		const char *xform_string)
{
	unsigned int i;
	int ret = -1;	/* Invalid string */

	for (i = 1; i < RTE_DIM(crypto_asym_xform_strings); i++) {
		if (strcmp(xform_string,
			crypto_asym_xform_strings[i]) == 0) {
			*xform_enum = (enum rte_crypto_asym_xform_type) i;
			ret = 0;
			break;
		}
	}

	rte_cryptodev_trace_asym_get_xform_enum(xform_string, *xform_enum, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_cryptodev_get_cipher_algo_string, 23.03)
const char *
rte_cryptodev_get_cipher_algo_string(enum rte_crypto_cipher_algorithm algo_enum)
{
	const char *alg_str = NULL;

	if ((unsigned int)algo_enum < RTE_DIM(crypto_cipher_algorithm_strings))
		alg_str = crypto_cipher_algorithm_strings[algo_enum];

	rte_cryptodev_trace_get_cipher_algo_string(algo_enum, alg_str);

	return alg_str;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_cryptodev_get_auth_algo_string, 23.03)
const char *
rte_cryptodev_get_auth_algo_string(enum rte_crypto_auth_algorithm algo_enum)
{
	const char *alg_str = NULL;

	if ((unsigned int)algo_enum < RTE_DIM(crypto_auth_algorithm_strings))
		alg_str = crypto_auth_algorithm_strings[algo_enum];

	rte_cryptodev_trace_get_auth_algo_string(algo_enum, alg_str);

	return alg_str;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_cryptodev_get_aead_algo_string, 23.03)
const char *
rte_cryptodev_get_aead_algo_string(enum rte_crypto_aead_algorithm algo_enum)
{
	const char *alg_str = NULL;

	if ((unsigned int)algo_enum < RTE_DIM(crypto_aead_algorithm_strings))
		alg_str = crypto_aead_algorithm_strings[algo_enum];

	rte_cryptodev_trace_get_aead_algo_string(algo_enum, alg_str);

	return alg_str;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_cryptodev_asym_get_xform_string, 23.03)
const char *
rte_cryptodev_asym_get_xform_string(enum rte_crypto_asym_xform_type xform_enum)
{
	const char *xform_str = NULL;

	if ((unsigned int)xform_enum < RTE_DIM(crypto_asym_xform_strings))
		xform_str = crypto_asym_xform_strings[xform_enum];

	rte_cryptodev_trace_asym_get_xform_string(xform_enum, xform_str);

	return xform_str;
}

/**
 * The crypto auth operation strings identifiers.
 * It could be used in application command line.
 */
RTE_EXPORT_SYMBOL(rte_crypto_auth_operation_strings)
const char *
rte_crypto_auth_operation_strings[] = {
		[RTE_CRYPTO_AUTH_OP_VERIFY]	= "verify",
		[RTE_CRYPTO_AUTH_OP_GENERATE]	= "generate"
};

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_capability_get)
const struct rte_cryptodev_symmetric_capability *
rte_cryptodev_sym_capability_get(uint8_t dev_id,
		const struct rte_cryptodev_sym_capability_idx *idx)
{
	const struct rte_cryptodev_capabilities *capability;
	const struct rte_cryptodev_symmetric_capability *sym_capability = NULL;
	struct rte_cryptodev_info dev_info;
	int i = 0;

	rte_cryptodev_info_get(dev_id, &dev_info);

	while ((capability = &dev_info.capabilities[i++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (capability->op != RTE_CRYPTO_OP_TYPE_SYMMETRIC)
			continue;

		if (capability->sym.xform_type != idx->type)
			continue;

		if (idx->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
			capability->sym.auth.algo == idx->algo.auth) {
			sym_capability = &capability->sym;
			break;
		}

		if (idx->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
			capability->sym.cipher.algo == idx->algo.cipher) {
			sym_capability = &capability->sym;
			break;
		}

		if (idx->type == RTE_CRYPTO_SYM_XFORM_AEAD &&
				capability->sym.aead.algo == idx->algo.aead) {
			sym_capability = &capability->sym;
			break;
		}
	}

	rte_cryptodev_trace_sym_capability_get(dev_id, dev_info.driver_name,
		dev_info.driver_id, idx->type, sym_capability);

	return sym_capability;
}

static int
param_range_check(uint16_t size, const struct rte_crypto_param_range *range)
{
	unsigned int next_size;

	/* Check lower/upper bounds */
	if (size < range->min)
		return -1;

	if (size > range->max)
		return -1;

	/* If range is actually only one value, size is correct */
	if (range->increment == 0)
		return 0;

	/* Check if value is one of the supported sizes */
	for (next_size = range->min; next_size <= range->max;
			next_size += range->increment)
		if (size == next_size)
			return 0;

	return -1;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_capability_get)
const struct rte_cryptodev_asymmetric_xform_capability *
rte_cryptodev_asym_capability_get(uint8_t dev_id,
		const struct rte_cryptodev_asym_capability_idx *idx)
{
	const struct rte_cryptodev_capabilities *capability;
	const struct rte_cryptodev_asymmetric_xform_capability *asym_cap = NULL;
	struct rte_cryptodev_info dev_info;
	unsigned int i = 0;

	memset(&dev_info, 0, sizeof(struct rte_cryptodev_info));
	rte_cryptodev_info_get(dev_id, &dev_info);

	while ((capability = &dev_info.capabilities[i++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (capability->op != RTE_CRYPTO_OP_TYPE_ASYMMETRIC)
			continue;

		if (capability->asym.xform_capa.xform_type == idx->type) {
			asym_cap = &capability->asym.xform_capa;
			break;
		}
	}

	rte_cryptodev_trace_asym_capability_get(dev_info.driver_name,
		dev_info.driver_id, idx->type, asym_cap);

	return asym_cap;
};

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_capability_check_cipher)
int
rte_cryptodev_sym_capability_check_cipher(
		const struct rte_cryptodev_symmetric_capability *capability,
		uint16_t key_size, uint16_t iv_size)
{
	int ret = 0; /* success */

	if (param_range_check(key_size, &capability->cipher.key_size) != 0) {
		ret = -1;
		goto done;
	}

	if (param_range_check(iv_size, &capability->cipher.iv_size) != 0)
		ret = -1;

done:
	rte_cryptodev_trace_sym_capability_check_cipher(capability, key_size,
		iv_size, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_capability_check_auth)
int
rte_cryptodev_sym_capability_check_auth(
		const struct rte_cryptodev_symmetric_capability *capability,
		uint16_t key_size, uint16_t digest_size, uint16_t iv_size)
{
	int ret = 0; /* success */

	if (param_range_check(key_size, &capability->auth.key_size) != 0) {
		ret = -1;
		goto done;
	}

	if (param_range_check(digest_size,
		&capability->auth.digest_size) != 0) {
		ret = -1;
		goto done;
	}

	if (param_range_check(iv_size, &capability->auth.iv_size) != 0)
		ret = -1;

done:
	rte_cryptodev_trace_sym_capability_check_auth(capability, key_size,
		digest_size, iv_size, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_capability_check_aead)
int
rte_cryptodev_sym_capability_check_aead(
		const struct rte_cryptodev_symmetric_capability *capability,
		uint16_t key_size, uint16_t digest_size, uint16_t aad_size,
		uint16_t iv_size)
{
	int ret = 0; /* success */

	if (param_range_check(key_size, &capability->aead.key_size) != 0) {
		ret = -1;
		goto done;
	}

	if (param_range_check(digest_size,
		&capability->aead.digest_size) != 0) {
		ret = -1;
		goto done;
	}

	if (param_range_check(aad_size, &capability->aead.aad_size) != 0) {
		ret = -1;
		goto done;
	}

	if (param_range_check(iv_size, &capability->aead.iv_size) != 0)
		ret = -1;

done:
	rte_cryptodev_trace_sym_capability_check_aead(capability, key_size,
		digest_size, aad_size, iv_size, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_xform_capability_check_optype)
int
rte_cryptodev_asym_xform_capability_check_optype(
	const struct rte_cryptodev_asymmetric_xform_capability *capability,
	enum rte_crypto_asym_op_type op_type)
{
	int ret = 0;

	if (capability->op_types & (1 << op_type))
		ret = 1;

	rte_cryptodev_trace_asym_xform_capability_check_optype(
		capability->op_types, op_type, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_xform_capability_check_modlen)
int
rte_cryptodev_asym_xform_capability_check_modlen(
	const struct rte_cryptodev_asymmetric_xform_capability *capability,
	uint16_t modlen)
{
	int ret = 0; /* success */

	/* no need to check for limits, if min or max = 0 */
	if (capability->modlen.min != 0) {
		if (modlen < capability->modlen.min) {
			ret = -1;
			goto done;
		}
	}

	if (capability->modlen.max != 0) {
		if (modlen > capability->modlen.max) {
			ret = -1;
			goto done;
		}
	}

	/* in any case, check if given modlen is module increment */
	if (capability->modlen.increment != 0) {
		if (modlen % (capability->modlen.increment))
			ret = -1;
	}

done:
	rte_cryptodev_trace_asym_xform_capability_check_modlen(capability,
		modlen, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_xform_capability_check_hash)
bool
rte_cryptodev_asym_xform_capability_check_hash(
	const struct rte_cryptodev_asymmetric_xform_capability *capability,
	enum rte_crypto_auth_algorithm hash)
{
	bool ret = false;

	if (capability->hash_algos & RTE_BIT64(hash))
		ret = true;

	rte_cryptodev_trace_asym_xform_capability_check_hash(
		capability->hash_algos, hash, ret);

	return ret;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_cryptodev_asym_xform_capability_check_opcap, 24.11)
int
rte_cryptodev_asym_xform_capability_check_opcap(
	const struct rte_cryptodev_asymmetric_xform_capability *capability,
	enum rte_crypto_asym_op_type op_type, uint8_t cap)
{
	int ret = 0;

	if (!(capability->op_types & (1 << op_type)))
		return ret;

	if (capability->op_capa[op_type] & (1 << cap))
		ret = 1;

	return ret;
}

/* spinlock for crypto device enq callbacks */
static rte_spinlock_t rte_cryptodev_callback_lock = RTE_SPINLOCK_INITIALIZER;

static void
cryptodev_cb_cleanup(struct rte_cryptodev *dev)
{
	struct rte_cryptodev_cb_rcu *list;
	struct rte_cryptodev_cb *cb, *next;
	uint16_t qp_id;

	if (dev->enq_cbs == NULL && dev->deq_cbs == NULL)
		return;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		list = &dev->enq_cbs[qp_id];
		cb = list->next;
		while (cb != NULL) {
			next = cb->next;
			rte_free(cb);
			cb = next;
		}

		rte_free(list->qsbr);
	}

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		list = &dev->deq_cbs[qp_id];
		cb = list->next;
		while (cb != NULL) {
			next = cb->next;
			rte_free(cb);
			cb = next;
		}

		rte_free(list->qsbr);
	}

	rte_free(dev->enq_cbs);
	dev->enq_cbs = NULL;
	rte_free(dev->deq_cbs);
	dev->deq_cbs = NULL;
}

static int
cryptodev_cb_init(struct rte_cryptodev *dev)
{
	struct rte_cryptodev_cb_rcu *list;
	struct rte_rcu_qsbr *qsbr;
	uint16_t qp_id;
	size_t size;

	/* Max thread set to 1, as one DP thread accessing a queue-pair */
	const uint32_t max_threads = 1;

	dev->enq_cbs = rte_zmalloc(NULL,
				   sizeof(struct rte_cryptodev_cb_rcu) *
				   dev->data->nb_queue_pairs, 0);
	if (dev->enq_cbs == NULL) {
		CDEV_LOG_ERR("Failed to allocate memory for enq callbacks");
		return -ENOMEM;
	}

	dev->deq_cbs = rte_zmalloc(NULL,
				   sizeof(struct rte_cryptodev_cb_rcu) *
				   dev->data->nb_queue_pairs, 0);
	if (dev->deq_cbs == NULL) {
		CDEV_LOG_ERR("Failed to allocate memory for deq callbacks");
		rte_free(dev->enq_cbs);
		return -ENOMEM;
	}

	/* Create RCU QSBR variable */
	size = rte_rcu_qsbr_get_memsize(max_threads);

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		list = &dev->enq_cbs[qp_id];
		qsbr = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
		if (qsbr == NULL) {
			CDEV_LOG_ERR("Failed to allocate memory for RCU on "
				"queue_pair_id=%d", qp_id);
			goto cb_init_err;
		}

		if (rte_rcu_qsbr_init(qsbr, max_threads)) {
			CDEV_LOG_ERR("Failed to initialize for RCU on "
				"queue_pair_id=%d", qp_id);
			goto cb_init_err;
		}

		list->qsbr = qsbr;
	}

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		list = &dev->deq_cbs[qp_id];
		qsbr = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
		if (qsbr == NULL) {
			CDEV_LOG_ERR("Failed to allocate memory for RCU on "
				"queue_pair_id=%d", qp_id);
			goto cb_init_err;
		}

		if (rte_rcu_qsbr_init(qsbr, max_threads)) {
			CDEV_LOG_ERR("Failed to initialize for RCU on "
				"queue_pair_id=%d", qp_id);
			goto cb_init_err;
		}

		list->qsbr = qsbr;
	}

	return 0;

cb_init_err:
	cryptodev_cb_cleanup(dev);
	return -ENOMEM;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_get_feature_name)
const char *
rte_cryptodev_get_feature_name(uint64_t flag)
{
	rte_cryptodev_trace_get_feature_name(flag);

	switch (flag) {
	case RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO:
		return "SYMMETRIC_CRYPTO";
	case RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO:
		return "ASYMMETRIC_CRYPTO";
	case RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING:
		return "SYM_OPERATION_CHAINING";
	case RTE_CRYPTODEV_FF_CPU_SSE:
		return "CPU_SSE";
	case RTE_CRYPTODEV_FF_CPU_AVX:
		return "CPU_AVX";
	case RTE_CRYPTODEV_FF_CPU_AVX2:
		return "CPU_AVX2";
	case RTE_CRYPTODEV_FF_CPU_AVX512:
		return "CPU_AVX512";
	case RTE_CRYPTODEV_FF_CPU_AESNI:
		return "CPU_AESNI";
	case RTE_CRYPTODEV_FF_HW_ACCELERATED:
		return "HW_ACCELERATED";
	case RTE_CRYPTODEV_FF_IN_PLACE_SGL:
		return "IN_PLACE_SGL";
	case RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT:
		return "OOP_SGL_IN_SGL_OUT";
	case RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT:
		return "OOP_SGL_IN_LB_OUT";
	case RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT:
		return "OOP_LB_IN_SGL_OUT";
	case RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT:
		return "OOP_LB_IN_LB_OUT";
	case RTE_CRYPTODEV_FF_CPU_NEON:
		return "CPU_NEON";
	case RTE_CRYPTODEV_FF_CPU_ARM_CE:
		return "CPU_ARM_CE";
	case RTE_CRYPTODEV_FF_SECURITY:
		return "SECURITY_PROTOCOL";
	case RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_EXP:
		return "RSA_PRIV_OP_KEY_EXP";
	case RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT:
		return "RSA_PRIV_OP_KEY_QT";
	case RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED:
		return "DIGEST_ENCRYPTED";
	case RTE_CRYPTODEV_FF_SYM_CPU_CRYPTO:
		return "SYM_CPU_CRYPTO";
	case RTE_CRYPTODEV_FF_ASYM_SESSIONLESS:
		return "ASYM_SESSIONLESS";
	case RTE_CRYPTODEV_FF_SYM_SESSIONLESS:
		return "SYM_SESSIONLESS";
	case RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA:
		return "NON_BYTE_ALIGNED_DATA";
	case RTE_CRYPTODEV_FF_CIPHER_MULTIPLE_DATA_UNITS:
		return "CIPHER_MULTIPLE_DATA_UNITS";
	case RTE_CRYPTODEV_FF_CIPHER_WRAPPED_KEY:
		return "CIPHER_WRAPPED_KEY";
	default:
		return NULL;
	}
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_cryptodev_pmd_get_dev)
struct rte_cryptodev *
rte_cryptodev_pmd_get_dev(uint8_t dev_id)
{
	return &cryptodev_globals.devs[dev_id];
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_cryptodev_pmd_get_named_dev)
struct rte_cryptodev *
rte_cryptodev_pmd_get_named_dev(const char *name)
{
	struct rte_cryptodev *dev;
	unsigned int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < RTE_CRYPTO_MAX_DEVS; i++) {
		dev = &cryptodev_globals.devs[i];

		if ((dev->attached == RTE_CRYPTODEV_ATTACHED) &&
				(strcmp(dev->data->name, name) == 0))
			return dev;
	}

	return NULL;
}

static inline uint8_t
rte_cryptodev_is_valid_device_data(uint8_t dev_id)
{
	if (dev_id >= RTE_CRYPTO_MAX_DEVS ||
			rte_crypto_devices[dev_id].data == NULL)
		return 0;

	return 1;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_is_valid_dev)
unsigned int
rte_cryptodev_is_valid_dev(uint8_t dev_id)
{
	struct rte_cryptodev *dev = NULL;
	unsigned int ret = 1;

	if (!rte_cryptodev_is_valid_device_data(dev_id)) {
		ret = 0;
		goto done;
	}

	dev = rte_cryptodev_pmd_get_dev(dev_id);
	if (dev->attached != RTE_CRYPTODEV_ATTACHED)
		ret = 0;

done:
	rte_cryptodev_trace_is_valid_dev(dev_id, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_get_dev_id)
int
rte_cryptodev_get_dev_id(const char *name)
{
	unsigned i;
	int ret = -1;

	if (name == NULL)
		return -1;

	for (i = 0; i < RTE_CRYPTO_MAX_DEVS; i++) {
		if (!rte_cryptodev_is_valid_device_data(i))
			continue;
		if ((strcmp(cryptodev_globals.devs[i].data->name, name)
				== 0) &&
				(cryptodev_globals.devs[i].attached ==
						RTE_CRYPTODEV_ATTACHED)) {
			ret = (int)i;
			break;
		}
	}

	rte_cryptodev_trace_get_dev_id(name, ret);

	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_count)
uint8_t
rte_cryptodev_count(void)
{
	rte_cryptodev_trace_count(cryptodev_globals.nb_devs);

	return cryptodev_globals.nb_devs;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_device_count_by_driver)
uint8_t
rte_cryptodev_device_count_by_driver(uint8_t driver_id)
{
	uint8_t i, dev_count = 0;

	for (i = 0; i < RTE_CRYPTO_MAX_DEVS; i++)
		if (cryptodev_globals.devs[i].driver_id == driver_id &&
			cryptodev_globals.devs[i].attached ==
					RTE_CRYPTODEV_ATTACHED)
			dev_count++;

	rte_cryptodev_trace_device_count_by_driver(driver_id, dev_count);

	return dev_count;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_devices_get)
uint8_t
rte_cryptodev_devices_get(const char *driver_name, uint8_t *devices,
	uint8_t nb_devices)
{
	uint8_t i, count = 0;
	struct rte_cryptodev *devs = cryptodev_globals.devs;

	for (i = 0; i < RTE_CRYPTO_MAX_DEVS && count < nb_devices; i++) {
		if (!rte_cryptodev_is_valid_device_data(i))
			continue;

		if (devs[i].attached == RTE_CRYPTODEV_ATTACHED) {
			int cmp;

			cmp = strncmp(devs[i].device->driver->name,
					driver_name,
					strlen(driver_name) + 1);

			if (cmp == 0)
				devices[count++] = devs[i].data->dev_id;
		}
	}

	rte_cryptodev_trace_devices_get(driver_name, count);

	return count;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_get_sec_ctx)
void *
rte_cryptodev_get_sec_ctx(uint8_t dev_id)
{
	void *sec_ctx = NULL;

	if (dev_id < RTE_CRYPTO_MAX_DEVS &&
			(rte_crypto_devices[dev_id].feature_flags &
			RTE_CRYPTODEV_FF_SECURITY))
		sec_ctx = rte_crypto_devices[dev_id].security_ctx;

	rte_cryptodev_trace_get_sec_ctx(dev_id, sec_ctx);

	return sec_ctx;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_socket_id)
int
rte_cryptodev_socket_id(uint8_t dev_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_dev(dev_id))
		return -1;

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	rte_cryptodev_trace_socket_id(dev_id, dev->data->name,
		dev->data->socket_id);
	return dev->data->socket_id;
}

static inline int
rte_cryptodev_data_alloc(uint8_t dev_id, struct rte_cryptodev_data **data,
		int socket_id)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int n;

	/* generate memzone name */
	n = snprintf(mz_name, sizeof(mz_name), "rte_cryptodev_data_%u", dev_id);
	if (n >= (int)sizeof(mz_name))
		return -EINVAL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve(mz_name,
				sizeof(struct rte_cryptodev_data),
				socket_id, 0);
		CDEV_LOG_DEBUG("PRIMARY:reserved memzone for %s (%p)",
				mz_name, mz);
	} else {
		mz = rte_memzone_lookup(mz_name);
		CDEV_LOG_DEBUG("SECONDARY:looked up memzone for %s (%p)",
				mz_name, mz);
	}

	if (mz == NULL)
		return -ENOMEM;

	*data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		memset(*data, 0, sizeof(struct rte_cryptodev_data));

	return 0;
}

static inline int
rte_cryptodev_data_free(uint8_t dev_id, struct rte_cryptodev_data **data)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int n;

	/* generate memzone name */
	n = snprintf(mz_name, sizeof(mz_name), "rte_cryptodev_data_%u", dev_id);
	if (n >= (int)sizeof(mz_name))
		return -EINVAL;

	mz = rte_memzone_lookup(mz_name);
	if (mz == NULL)
		return -ENOMEM;

	RTE_ASSERT(*data == mz->addr);
	*data = NULL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		CDEV_LOG_DEBUG("PRIMARY:free memzone of %s (%p)",
				mz_name, mz);
		return rte_memzone_free(mz);
	} else {
		CDEV_LOG_DEBUG("SECONDARY:don't free memzone of %s (%p)",
				mz_name, mz);
	}

	return 0;
}

static uint8_t
rte_cryptodev_find_free_device_index(void)
{
	uint8_t dev_id;

	for (dev_id = 0; dev_id < RTE_CRYPTO_MAX_DEVS; dev_id++) {
		if (rte_crypto_devices[dev_id].attached ==
				RTE_CRYPTODEV_DETACHED)
			return dev_id;
	}
	return RTE_CRYPTO_MAX_DEVS;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_cryptodev_pmd_allocate)
struct rte_cryptodev *
rte_cryptodev_pmd_allocate(const char *name, int socket_id)
{
	struct rte_cryptodev *cryptodev;
	uint8_t dev_id;

	if (rte_cryptodev_pmd_get_named_dev(name) != NULL) {
		CDEV_LOG_ERR("Crypto device with name %s already "
				"allocated!", name);
		return NULL;
	}

	dev_id = rte_cryptodev_find_free_device_index();
	if (dev_id == RTE_CRYPTO_MAX_DEVS) {
		CDEV_LOG_ERR("Reached maximum number of crypto devices");
		return NULL;
	}

	cryptodev = rte_cryptodev_pmd_get_dev(dev_id);

	if (cryptodev->data == NULL) {
		struct rte_cryptodev_data **cryptodev_data =
				&cryptodev_globals.data[dev_id];

		int retval = rte_cryptodev_data_alloc(dev_id, cryptodev_data,
				socket_id);

		if (retval < 0 || *cryptodev_data == NULL)
			return NULL;

		cryptodev->data = *cryptodev_data;

		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			strlcpy(cryptodev->data->name, name,
				RTE_CRYPTODEV_NAME_MAX_LEN);

			cryptodev->data->dev_id = dev_id;
			cryptodev->data->socket_id = socket_id;
			cryptodev->data->dev_started = 0;
			CDEV_LOG_DEBUG("PRIMARY:init data");
		}

		CDEV_LOG_DEBUG("Data for %s: dev_id %d, socket %d, started %d",
				cryptodev->data->name,
				cryptodev->data->dev_id,
				cryptodev->data->socket_id,
				cryptodev->data->dev_started);

		/* init user callbacks */
		TAILQ_INIT(&(cryptodev->link_intr_cbs));

		cryptodev->attached = RTE_CRYPTODEV_ATTACHED;

		cryptodev_globals.nb_devs++;
	}

	return cryptodev;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_cryptodev_pmd_release_device)
int
rte_cryptodev_pmd_release_device(struct rte_cryptodev *cryptodev)
{
	int ret;
	uint8_t dev_id;

	if (cryptodev == NULL)
		return -EINVAL;

	dev_id = cryptodev->data->dev_id;

	cryptodev_fp_ops_reset(rte_crypto_fp_ops + dev_id);

	/* Close device only if device operations have been set */
	if (cryptodev->dev_ops && (rte_eal_process_type() == RTE_PROC_PRIMARY)) {
		ret = rte_cryptodev_close(dev_id);
		if (ret < 0)
			return ret;
	}

	ret = rte_cryptodev_data_free(dev_id, &cryptodev_globals.data[dev_id]);
	if (ret < 0)
		return ret;

	cryptodev->attached = RTE_CRYPTODEV_DETACHED;
	cryptodev_globals.nb_devs--;
	return 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_queue_pair_count)
uint16_t
rte_cryptodev_queue_pair_count(uint8_t dev_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_device_data(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return 0;
	}

	dev = &rte_crypto_devices[dev_id];
	rte_cryptodev_trace_queue_pair_count(dev, dev->data->name,
		dev->data->socket_id, dev->data->dev_id,
		dev->data->nb_queue_pairs);

	return dev->data->nb_queue_pairs;
}

static int
rte_cryptodev_queue_pairs_config(struct rte_cryptodev *dev, uint16_t nb_qpairs,
		int socket_id)
{
	struct rte_cryptodev_info dev_info;
	void **qp;
	unsigned i;

	if ((dev == NULL) || (nb_qpairs < 1)) {
		CDEV_LOG_ERR("invalid param: dev %p, nb_queues %u",
							dev, nb_qpairs);
		return -EINVAL;
	}

	CDEV_LOG_DEBUG("Setup %d queues pairs on device %u",
			nb_qpairs, dev->data->dev_id);

	memset(&dev_info, 0, sizeof(struct rte_cryptodev_info));

	if (dev->dev_ops->dev_infos_get == NULL)
		return -ENOTSUP;
	dev->dev_ops->dev_infos_get(dev, &dev_info);

	if (nb_qpairs > (dev_info.max_nb_queue_pairs)) {
		CDEV_LOG_ERR("Invalid num queue_pairs (%u) for dev %u",
				nb_qpairs, dev->data->dev_id);
	    return -EINVAL;
	}

	if (dev->data->queue_pairs == NULL) { /* first time configuration */
		dev->data->queue_pairs = rte_zmalloc_socket(
				"cryptodev->queue_pairs",
				sizeof(dev->data->queue_pairs[0]) *
				dev_info.max_nb_queue_pairs,
				RTE_CACHE_LINE_SIZE, socket_id);

		if (dev->data->queue_pairs == NULL) {
			dev->data->nb_queue_pairs = 0;
			CDEV_LOG_ERR("failed to get memory for qp meta data, "
							"nb_queues %u",
							nb_qpairs);
			return -(ENOMEM);
		}
	} else { /* re-configure */
		int ret;
		uint16_t old_nb_queues = dev->data->nb_queue_pairs;

		qp = dev->data->queue_pairs;

		if (dev->dev_ops->queue_pair_release == NULL)
			return -ENOTSUP;

		for (i = nb_qpairs; i < old_nb_queues; i++) {
			ret = dev->dev_ops->queue_pair_release(dev, i);
			if (ret < 0)
				return ret;
			qp[i] = NULL;
		}

	}
	dev->data->nb_queue_pairs = nb_qpairs;
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_cryptodev_queue_pair_reset, 24.11)
int
rte_cryptodev_queue_pair_reset(uint8_t dev_id, uint16_t queue_pair_id,
		const struct rte_cryptodev_qp_conf *qp_conf, int socket_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", queue_pair_id);
		return -EINVAL;
	}

	if (dev->dev_ops->queue_pair_reset == NULL)
		return -ENOTSUP;

	rte_cryptodev_trace_queue_pair_reset(dev_id, queue_pair_id, qp_conf, socket_id);
	return dev->dev_ops->queue_pair_reset(dev, queue_pair_id, qp_conf, socket_id);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_configure)
int
rte_cryptodev_configure(uint8_t dev_id, struct rte_cryptodev_config *config)
{
	struct rte_cryptodev *dev;
	int diag;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];

	if (dev->data->dev_started) {
		CDEV_LOG_ERR(
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	if (dev->dev_ops->dev_configure == NULL)
		return -ENOTSUP;

	rte_spinlock_lock(&rte_cryptodev_callback_lock);
	cryptodev_cb_cleanup(dev);
	rte_spinlock_unlock(&rte_cryptodev_callback_lock);

	/* Setup new number of queue pairs and reconfigure device. */
	diag = rte_cryptodev_queue_pairs_config(dev, config->nb_queue_pairs,
			config->socket_id);
	if (diag != 0) {
		CDEV_LOG_ERR("dev%d rte_crypto_dev_queue_pairs_config = %d",
				dev_id, diag);
		return diag;
	}

	rte_spinlock_lock(&rte_cryptodev_callback_lock);
	diag = cryptodev_cb_init(dev);
	rte_spinlock_unlock(&rte_cryptodev_callback_lock);
	if (diag) {
		CDEV_LOG_ERR("Callback init failed for dev_id=%d", dev_id);
		return diag;
	}

	rte_cryptodev_trace_configure(dev_id, config);
	return dev->dev_ops->dev_configure(dev, config);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_start)
int
rte_cryptodev_start(uint8_t dev_id)
{
	struct rte_cryptodev *dev;
	int diag;

	CDEV_LOG_DEBUG("Start dev_id=%" PRIu8, dev_id);

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];

	if (dev->dev_ops->dev_start == NULL)
		return -ENOTSUP;

	if (dev->data->dev_started != 0) {
		CDEV_LOG_ERR("Device with dev_id=%" PRIu8 " already started",
			dev_id);
		return 0;
	}

	diag = dev->dev_ops->dev_start(dev);
	/* expose selection of PMD fast-path functions */
	cryptodev_fp_ops_set(rte_crypto_fp_ops + dev_id, dev);

	rte_cryptodev_trace_start(dev_id, diag);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	return 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_stop)
void
rte_cryptodev_stop(uint8_t dev_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return;
	}

	dev = &rte_crypto_devices[dev_id];

	if (dev->dev_ops->dev_stop == NULL)
		return;

	if (dev->data->dev_started == 0) {
		CDEV_LOG_ERR("Device with dev_id=%" PRIu8 " already stopped",
			dev_id);
		return;
	}

	/* point fast-path functions to dummy ones */
	cryptodev_fp_ops_reset(rte_crypto_fp_ops + dev_id);

	dev->dev_ops->dev_stop(dev);
	rte_cryptodev_trace_stop(dev_id);
	dev->data->dev_started = 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_close)
int
rte_cryptodev_close(uint8_t dev_id)
{
	struct rte_cryptodev *dev;
	int retval;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -1;
	}

	dev = &rte_crypto_devices[dev_id];

	/* Device must be stopped before it can be closed */
	if (dev->data->dev_started == 1) {
		CDEV_LOG_ERR("Device %u must be stopped before closing",
				dev_id);
		return -EBUSY;
	}

	/* We can't close the device if there are outstanding sessions in use */
	if (dev->data->session_pool != NULL) {
		if (!rte_mempool_full(dev->data->session_pool)) {
			CDEV_LOG_ERR("dev_id=%u close failed, session mempool "
					"has sessions still in use, free "
					"all sessions before calling close",
					(unsigned)dev_id);
			return -EBUSY;
		}
	}

	if (dev->dev_ops->dev_close == NULL)
		return -ENOTSUP;
	retval = dev->dev_ops->dev_close(dev);
	rte_cryptodev_trace_close(dev_id, retval);

	if (retval < 0)
		return retval;

	return 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_get_qp_status)
int
rte_cryptodev_get_qp_status(uint8_t dev_id, uint16_t queue_pair_id)
{
	struct rte_cryptodev *dev;
	int ret = 0;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		ret = -EINVAL;
		goto done;
	}

	dev = &rte_crypto_devices[dev_id];
	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", queue_pair_id);
		ret = -EINVAL;
		goto done;
	}
	void **qps = dev->data->queue_pairs;

	if (qps[queue_pair_id])	{
		CDEV_LOG_DEBUG("qp %d on dev %d is initialised",
			queue_pair_id, dev_id);
		ret = 1;
		goto done;
	}

	CDEV_LOG_DEBUG("qp %d on dev %d is not initialised",
		queue_pair_id, dev_id);

done:
	rte_cryptodev_trace_get_qp_status(dev_id, queue_pair_id, ret);

	return ret;
}

static uint8_t
rte_cryptodev_sym_is_valid_session_pool(struct rte_mempool *mp,
	uint32_t sess_priv_size)
{
	struct rte_cryptodev_sym_session_pool_private_data *pool_priv;

	if (!mp)
		return 0;

	pool_priv = rte_mempool_get_priv(mp);

	if (!pool_priv || mp->private_data_size < sizeof(*pool_priv) ||
			pool_priv->sess_data_sz < sess_priv_size)
		return 0;

	return 1;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_queue_pair_setup)
int
rte_cryptodev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
		const struct rte_cryptodev_qp_conf *qp_conf, int socket_id)

{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", queue_pair_id);
		return -EINVAL;
	}

	if (!qp_conf) {
		CDEV_LOG_ERR("qp_conf cannot be NULL");
		return -EINVAL;
	}

	if (qp_conf->mp_session) {
		struct rte_cryptodev_sym_session_pool_private_data *pool_priv;

		pool_priv = rte_mempool_get_priv(qp_conf->mp_session);
		if (!pool_priv || qp_conf->mp_session->private_data_size <
				sizeof(*pool_priv)) {
			CDEV_LOG_ERR("Invalid mempool");
			return -EINVAL;
		}

		if (!rte_cryptodev_sym_is_valid_session_pool(qp_conf->mp_session,
					rte_cryptodev_sym_get_private_session_size(dev_id))) {
			CDEV_LOG_ERR("Invalid mempool");
			return -EINVAL;
		}
	}

	if (dev->data->dev_started) {
		CDEV_LOG_ERR(
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	if (dev->dev_ops->queue_pair_setup == NULL)
		return -ENOTSUP;

	rte_cryptodev_trace_queue_pair_setup(dev_id, queue_pair_id, qp_conf);
	return dev->dev_ops->queue_pair_setup(dev, queue_pair_id, qp_conf, socket_id);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_add_enq_callback)
struct rte_cryptodev_cb *
rte_cryptodev_add_enq_callback(uint8_t dev_id,
			       uint16_t qp_id,
			       rte_cryptodev_callback_fn cb_fn,
			       void *cb_arg)
{
#ifndef RTE_CRYPTO_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	struct rte_cryptodev *dev;
	struct rte_cryptodev_cb_rcu *list;
	struct rte_cryptodev_cb *cb, *tail;

	if (!cb_fn) {
		CDEV_LOG_ERR("Callback is NULL on dev_id=%d", dev_id);
		rte_errno = EINVAL;
		return NULL;
	}

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		rte_errno = ENODEV;
		return NULL;
	}

	dev = &rte_crypto_devices[dev_id];
	if (qp_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", qp_id);
		rte_errno = ENODEV;
		return NULL;
	}

	cb = rte_zmalloc(NULL, sizeof(*cb), 0);
	if (cb == NULL) {
		CDEV_LOG_ERR("Failed to allocate memory for callback on "
			     "dev=%d, queue_pair_id=%d", dev_id, qp_id);
		rte_errno = ENOMEM;
		return NULL;
	}

	rte_spinlock_lock(&rte_cryptodev_callback_lock);

	cb->fn = cb_fn;
	cb->arg = cb_arg;

	/* Add the callbacks in fifo order. */
	list = &dev->enq_cbs[qp_id];
	tail = list->next;

	if (tail) {
		while (tail->next)
			tail = tail->next;
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		rte_atomic_store_explicit(&tail->next, cb, rte_memory_order_release);
	} else {
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		rte_atomic_store_explicit(&list->next, cb, rte_memory_order_release);
	}

	rte_spinlock_unlock(&rte_cryptodev_callback_lock);

	rte_cryptodev_trace_add_enq_callback(dev_id, qp_id, cb_fn);
	return cb;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_remove_enq_callback)
int
rte_cryptodev_remove_enq_callback(uint8_t dev_id,
				  uint16_t qp_id,
				  struct rte_cryptodev_cb *cb)
{
#ifndef RTE_CRYPTO_CALLBACKS
	return -ENOTSUP;
#endif
	struct rte_cryptodev *dev;
	RTE_ATOMIC(struct rte_cryptodev_cb *) *prev_cb;
	struct rte_cryptodev_cb *curr_cb;
	struct rte_cryptodev_cb_rcu *list;
	int ret;

	ret = -EINVAL;

	if (!cb) {
		CDEV_LOG_ERR("Callback is NULL");
		return -EINVAL;
	}

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		return -ENODEV;
	}

	rte_cryptodev_trace_remove_enq_callback(dev_id, qp_id, cb->fn);

	dev = &rte_crypto_devices[dev_id];
	if (qp_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", qp_id);
		return -ENODEV;
	}

	rte_spinlock_lock(&rte_cryptodev_callback_lock);
	if (dev->enq_cbs == NULL) {
		CDEV_LOG_ERR("Callback not initialized");
		goto cb_err;
	}

	list = &dev->enq_cbs[qp_id];
	if (list == NULL) {
		CDEV_LOG_ERR("Callback list is NULL");
		goto cb_err;
	}

	if (list->qsbr == NULL) {
		CDEV_LOG_ERR("Rcu qsbr is NULL");
		goto cb_err;
	}

	prev_cb = &list->next;
	for (; *prev_cb != NULL; prev_cb = &curr_cb->next) {
		curr_cb = *prev_cb;
		if (curr_cb == cb) {
			/* Remove the user cb from the callback list. */
			rte_atomic_store_explicit(prev_cb, curr_cb->next,
				rte_memory_order_relaxed);
			ret = 0;
			break;
		}
	}

	if (!ret) {
		/* Call sync with invalid thread id as this is part of
		 * control plane API
		 */
		rte_rcu_qsbr_synchronize(list->qsbr, RTE_QSBR_THRID_INVALID);
		rte_free(cb);
	}

cb_err:
	rte_spinlock_unlock(&rte_cryptodev_callback_lock);
	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_add_deq_callback)
struct rte_cryptodev_cb *
rte_cryptodev_add_deq_callback(uint8_t dev_id,
			       uint16_t qp_id,
			       rte_cryptodev_callback_fn cb_fn,
			       void *cb_arg)
{
#ifndef RTE_CRYPTO_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	struct rte_cryptodev *dev;
	struct rte_cryptodev_cb_rcu *list;
	struct rte_cryptodev_cb *cb, *tail;

	if (!cb_fn) {
		CDEV_LOG_ERR("Callback is NULL on dev_id=%d", dev_id);
		rte_errno = EINVAL;
		return NULL;
	}

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		rte_errno = ENODEV;
		return NULL;
	}

	dev = &rte_crypto_devices[dev_id];
	if (qp_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", qp_id);
		rte_errno = ENODEV;
		return NULL;
	}

	cb = rte_zmalloc(NULL, sizeof(*cb), 0);
	if (cb == NULL) {
		CDEV_LOG_ERR("Failed to allocate memory for callback on "
			     "dev=%d, queue_pair_id=%d", dev_id, qp_id);
		rte_errno = ENOMEM;
		return NULL;
	}

	rte_spinlock_lock(&rte_cryptodev_callback_lock);

	cb->fn = cb_fn;
	cb->arg = cb_arg;

	/* Add the callbacks in fifo order. */
	list = &dev->deq_cbs[qp_id];
	tail = list->next;

	if (tail) {
		while (tail->next)
			tail = tail->next;
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		rte_atomic_store_explicit(&tail->next, cb, rte_memory_order_release);
	} else {
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		rte_atomic_store_explicit(&list->next, cb, rte_memory_order_release);
	}

	rte_spinlock_unlock(&rte_cryptodev_callback_lock);

	rte_cryptodev_trace_add_deq_callback(dev_id, qp_id, cb_fn);

	return cb;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_remove_deq_callback)
int
rte_cryptodev_remove_deq_callback(uint8_t dev_id,
				  uint16_t qp_id,
				  struct rte_cryptodev_cb *cb)
{
#ifndef RTE_CRYPTO_CALLBACKS
	return -ENOTSUP;
#endif
	struct rte_cryptodev *dev;
	RTE_ATOMIC(struct rte_cryptodev_cb *) *prev_cb;
	struct rte_cryptodev_cb *curr_cb;
	struct rte_cryptodev_cb_rcu *list;
	int ret;

	ret = -EINVAL;

	if (!cb) {
		CDEV_LOG_ERR("Callback is NULL");
		return -EINVAL;
	}

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		return -ENODEV;
	}

	rte_cryptodev_trace_remove_deq_callback(dev_id, qp_id, cb->fn);

	dev = &rte_crypto_devices[dev_id];
	if (qp_id >= dev->data->nb_queue_pairs) {
		CDEV_LOG_ERR("Invalid queue_pair_id=%d", qp_id);
		return -ENODEV;
	}

	rte_spinlock_lock(&rte_cryptodev_callback_lock);
	if (dev->enq_cbs == NULL) {
		CDEV_LOG_ERR("Callback not initialized");
		goto cb_err;
	}

	list = &dev->deq_cbs[qp_id];
	if (list == NULL) {
		CDEV_LOG_ERR("Callback list is NULL");
		goto cb_err;
	}

	if (list->qsbr == NULL) {
		CDEV_LOG_ERR("Rcu qsbr is NULL");
		goto cb_err;
	}

	prev_cb = &list->next;
	for (; *prev_cb != NULL; prev_cb = &curr_cb->next) {
		curr_cb = *prev_cb;
		if (curr_cb == cb) {
			/* Remove the user cb from the callback list. */
			rte_atomic_store_explicit(prev_cb, curr_cb->next,
				rte_memory_order_relaxed);
			ret = 0;
			break;
		}
	}

	if (!ret) {
		/* Call sync with invalid thread id as this is part of
		 * control plane API
		 */
		rte_rcu_qsbr_synchronize(list->qsbr, RTE_QSBR_THRID_INVALID);
		rte_free(cb);
	}

cb_err:
	rte_spinlock_unlock(&rte_cryptodev_callback_lock);
	return ret;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_stats_get)
int
rte_cryptodev_stats_get(uint8_t dev_id, struct rte_cryptodev_stats *stats)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		return -ENODEV;
	}

	if (stats == NULL) {
		CDEV_LOG_ERR("Invalid stats ptr");
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	memset(stats, 0, sizeof(*stats));

	if (dev->dev_ops->stats_get == NULL)
		return -ENOTSUP;
	dev->dev_ops->stats_get(dev, stats);

	rte_cryptodev_trace_stats_get(dev_id, stats);
	return 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_stats_reset)
void
rte_cryptodev_stats_reset(uint8_t dev_id)
{
	struct rte_cryptodev *dev;

	rte_cryptodev_trace_stats_reset(dev_id);

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return;
	}

	dev = &rte_crypto_devices[dev_id];

	if (dev->dev_ops->stats_reset == NULL)
		return;
	dev->dev_ops->stats_reset(dev);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_info_get)
void
rte_cryptodev_info_get(uint8_t dev_id, struct rte_cryptodev_info *dev_info)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%d", dev_id);
		return;
	}

	dev = &rte_crypto_devices[dev_id];

	memset(dev_info, 0, sizeof(struct rte_cryptodev_info));

	if (dev->dev_ops->dev_infos_get == NULL)
		return;
	dev->dev_ops->dev_infos_get(dev, dev_info);

	dev_info->driver_name = dev->device->driver->name;
	dev_info->device = dev->device;

	rte_cryptodev_trace_info_get(dev_id, dev_info->driver_name);

}

RTE_EXPORT_SYMBOL(rte_cryptodev_callback_register)
int
rte_cryptodev_callback_register(uint8_t dev_id,
			enum rte_cryptodev_event_type event,
			rte_cryptodev_cb_fn cb_fn, void *cb_arg)
{
	struct rte_cryptodev *dev;
	struct rte_cryptodev_callback *user_cb;

	if (!cb_fn)
		return -EINVAL;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	rte_spinlock_lock(&rte_cryptodev_cb_lock);

	TAILQ_FOREACH(user_cb, &(dev->link_intr_cbs), next) {
		if (user_cb->cb_fn == cb_fn &&
			user_cb->cb_arg == cb_arg &&
			user_cb->event == event) {
			break;
		}
	}

	/* create a new callback. */
	if (user_cb == NULL) {
		user_cb = rte_zmalloc("INTR_USER_CALLBACK",
				sizeof(struct rte_cryptodev_callback), 0);
		if (user_cb != NULL) {
			user_cb->cb_fn = cb_fn;
			user_cb->cb_arg = cb_arg;
			user_cb->event = event;
			TAILQ_INSERT_TAIL(&(dev->link_intr_cbs), user_cb, next);
		}
	}

	rte_spinlock_unlock(&rte_cryptodev_cb_lock);

	rte_cryptodev_trace_callback_register(dev_id, event, cb_fn);
	return (user_cb == NULL) ? -ENOMEM : 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_callback_unregister)
int
rte_cryptodev_callback_unregister(uint8_t dev_id,
			enum rte_cryptodev_event_type event,
			rte_cryptodev_cb_fn cb_fn, void *cb_arg)
{
	int ret;
	struct rte_cryptodev *dev;
	struct rte_cryptodev_callback *cb, *next;

	if (!cb_fn)
		return -EINVAL;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_crypto_devices[dev_id];
	rte_spinlock_lock(&rte_cryptodev_cb_lock);

	ret = 0;
	for (cb = TAILQ_FIRST(&dev->link_intr_cbs); cb != NULL; cb = next) {

		next = TAILQ_NEXT(cb, next);

		if (cb->cb_fn != cb_fn || cb->event != event ||
				(cb->cb_arg != (void *)-1 &&
				cb->cb_arg != cb_arg))
			continue;

		/*
		 * if this callback is not executing right now,
		 * then remove it.
		 */
		if (cb->active == 0) {
			TAILQ_REMOVE(&(dev->link_intr_cbs), cb, next);
			rte_free(cb);
		} else {
			ret = -EAGAIN;
		}
	}

	rte_spinlock_unlock(&rte_cryptodev_cb_lock);

	rte_cryptodev_trace_callback_unregister(dev_id, event, cb_fn);
	return ret;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_cryptodev_pmd_callback_process)
void
rte_cryptodev_pmd_callback_process(struct rte_cryptodev *dev,
	enum rte_cryptodev_event_type event)
{
	struct rte_cryptodev_callback *cb_lst;
	struct rte_cryptodev_callback dev_cb;

	rte_spinlock_lock(&rte_cryptodev_cb_lock);
	TAILQ_FOREACH(cb_lst, &(dev->link_intr_cbs), next) {
		if (cb_lst->cb_fn == NULL || cb_lst->event != event)
			continue;
		dev_cb = *cb_lst;
		cb_lst->active = 1;
		rte_spinlock_unlock(&rte_cryptodev_cb_lock);
		dev_cb.cb_fn(dev->data->dev_id, dev_cb.event,
						dev_cb.cb_arg);
		rte_spinlock_lock(&rte_cryptodev_cb_lock);
		cb_lst->active = 0;
	}
	rte_spinlock_unlock(&rte_cryptodev_cb_lock);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_cryptodev_queue_pair_event_error_query, 23.03)
int
rte_cryptodev_queue_pair_event_error_query(uint8_t dev_id, uint16_t qp_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}
	dev = &rte_crypto_devices[dev_id];

	if (qp_id >= dev->data->nb_queue_pairs)
		return -EINVAL;
	if (dev->dev_ops->queue_pair_event_error_query == NULL)
		return -ENOTSUP;

	return dev->dev_ops->queue_pair_event_error_query(dev, qp_id);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_session_pool_create)
struct rte_mempool *
rte_cryptodev_sym_session_pool_create(const char *name, uint32_t nb_elts,
	uint32_t elt_size, uint32_t cache_size, uint16_t user_data_size,
	int socket_id)
{
	struct rte_mempool *mp;
	struct rte_cryptodev_sym_session_pool_private_data *pool_priv;
	uint32_t obj_sz;

	obj_sz = sizeof(struct rte_cryptodev_sym_session) + elt_size + user_data_size;

	obj_sz = RTE_ALIGN_CEIL(obj_sz, RTE_CACHE_LINE_SIZE);
	mp = rte_mempool_create(name, nb_elts, obj_sz, cache_size,
			(uint32_t)(sizeof(*pool_priv)), NULL, NULL,
			NULL, NULL,
			socket_id, 0);
	if (mp == NULL) {
		CDEV_LOG_ERR("%s(name=%s) failed, rte_errno=%d",
			__func__, name, rte_errno);
		return NULL;
	}

	pool_priv = rte_mempool_get_priv(mp);
	if (!pool_priv) {
		CDEV_LOG_ERR("%s(name=%s) failed to get private data",
			__func__, name);
		rte_mempool_free(mp);
		return NULL;
	}

	pool_priv->sess_data_sz = elt_size;
	pool_priv->user_data_sz = user_data_size;

	rte_cryptodev_trace_sym_session_pool_create(name, nb_elts,
		elt_size, cache_size, user_data_size, mp);
	return mp;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_session_pool_create)
struct rte_mempool *
rte_cryptodev_asym_session_pool_create(const char *name, uint32_t nb_elts,
	uint32_t cache_size, uint16_t user_data_size, int socket_id)
{
	struct rte_mempool *mp;
	struct rte_cryptodev_asym_session_pool_private_data *pool_priv;
	uint32_t obj_sz, obj_sz_aligned;
	uint8_t dev_id;
	unsigned int priv_sz, max_priv_sz = 0;

	for (dev_id = 0; dev_id < RTE_CRYPTO_MAX_DEVS; dev_id++)
		if (rte_cryptodev_is_valid_dev(dev_id)) {
			priv_sz = rte_cryptodev_asym_get_private_session_size(dev_id);
			if (priv_sz > max_priv_sz)
				max_priv_sz = priv_sz;
		}
	if (max_priv_sz == 0) {
		CDEV_LOG_INFO("Could not set max private session size");
		return NULL;
	}

	obj_sz = rte_cryptodev_asym_get_header_session_size() + max_priv_sz +
			user_data_size;
	obj_sz_aligned =  RTE_ALIGN_CEIL(obj_sz, RTE_CACHE_LINE_SIZE);

	mp = rte_mempool_create(name, nb_elts, obj_sz_aligned, cache_size,
			(uint32_t)(sizeof(*pool_priv)),
			NULL, NULL, NULL, NULL,
			socket_id, 0);
	if (mp == NULL) {
		CDEV_LOG_ERR("%s(name=%s) failed, rte_errno=%d",
			__func__, name, rte_errno);
		return NULL;
	}

	pool_priv = rte_mempool_get_priv(mp);
	if (!pool_priv) {
		CDEV_LOG_ERR("%s(name=%s) failed to get private data",
			__func__, name);
		rte_mempool_free(mp);
		return NULL;
	}
	pool_priv->max_priv_session_sz = max_priv_sz;
	pool_priv->user_data_sz = user_data_size;

	rte_cryptodev_trace_asym_session_pool_create(name, nb_elts,
		user_data_size, cache_size, mp);
	return mp;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_session_create)
void *
rte_cryptodev_sym_session_create(uint8_t dev_id,
		struct rte_crypto_sym_xform *xforms,
		struct rte_mempool *mp)
{
	struct rte_cryptodev *dev;
	struct rte_cryptodev_sym_session *sess;
	struct rte_cryptodev_sym_session_pool_private_data *pool_priv;
	uint32_t sess_priv_sz;
	int ret;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		rte_errno = EINVAL;
		return NULL;
	}

	if (xforms == NULL) {
		CDEV_LOG_ERR("Invalid xform");
		rte_errno = EINVAL;
		return NULL;
	}

	sess_priv_sz = rte_cryptodev_sym_get_private_session_size(dev_id);
	if (!rte_cryptodev_sym_is_valid_session_pool(mp, sess_priv_sz)) {
		CDEV_LOG_ERR("Invalid mempool");
		rte_errno = EINVAL;
		return NULL;
	}

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	/* Allocate a session structure from the session pool */
	if (rte_mempool_get(mp, (void **)&sess)) {
		CDEV_LOG_ERR("couldn't get object from session mempool");
		rte_errno = ENOMEM;
		return NULL;
	}

	pool_priv = rte_mempool_get_priv(mp);
	sess->driver_id = dev->driver_id;
	sess->sess_data_sz = pool_priv->sess_data_sz;
	sess->user_data_sz = pool_priv->user_data_sz;
	sess->driver_priv_data_iova = rte_mempool_virt2iova(sess) +
		offsetof(struct rte_cryptodev_sym_session, driver_priv_data);

	if (dev->dev_ops->sym_session_configure == NULL) {
		rte_errno = ENOTSUP;
		goto error_exit;
	}
	memset(sess->driver_priv_data, 0, pool_priv->sess_data_sz + pool_priv->user_data_sz);

	ret = dev->dev_ops->sym_session_configure(dev, xforms, sess);
	if (ret < 0) {
		rte_errno = -ret;
		goto error_exit;
	}
	sess->driver_id = dev->driver_id;

	rte_cryptodev_trace_sym_session_create(dev_id, sess, xforms, mp);

	return (void *)sess;
error_exit:
	rte_mempool_put(mp, (void *)sess);
	return NULL;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_session_create)
int
rte_cryptodev_asym_session_create(uint8_t dev_id,
		struct rte_crypto_asym_xform *xforms, struct rte_mempool *mp,
		void **session)
{
	struct rte_cryptodev_asym_session *sess;
	uint32_t session_priv_data_sz;
	struct rte_cryptodev_asym_session_pool_private_data *pool_priv;
	unsigned int session_header_size =
			rte_cryptodev_asym_get_header_session_size();
	struct rte_cryptodev *dev;
	int ret;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	if (dev == NULL)
		return -EINVAL;

	if (!mp) {
		CDEV_LOG_ERR("invalid mempool");
		return -EINVAL;
	}

	session_priv_data_sz = rte_cryptodev_asym_get_private_session_size(
			dev_id);
	pool_priv = rte_mempool_get_priv(mp);

	if (pool_priv->max_priv_session_sz < session_priv_data_sz) {
		CDEV_LOG_DEBUG(
			"The private session data size used when creating the mempool is smaller than this device's private session data.");
		return -EINVAL;
	}

	/* Verify if provided mempool can hold elements big enough. */
	if (mp->elt_size < session_header_size + session_priv_data_sz) {
		CDEV_LOG_ERR(
			"mempool elements too small to hold session objects");
		return -EINVAL;
	}

	/* Allocate a session structure from the session pool */
	if (rte_mempool_get(mp, session)) {
		CDEV_LOG_ERR("couldn't get object from session mempool");
		return -ENOMEM;
	}

	sess = *session;
	sess->driver_id = dev->driver_id;
	sess->user_data_sz = pool_priv->user_data_sz;
	sess->max_priv_data_sz = pool_priv->max_priv_session_sz;

	/* Clear device session pointer.*/
	memset(sess->sess_private_data, 0, session_priv_data_sz + sess->user_data_sz);

	if (dev->dev_ops->asym_session_configure == NULL)
		return -ENOTSUP;

	if (sess->sess_private_data[0] == 0) {
		ret = dev->dev_ops->asym_session_configure(dev, xforms, sess);
		if (ret < 0) {
			CDEV_LOG_ERR(
				"dev_id %d failed to configure session details",
				dev_id);
			return ret;
		}
	}

	rte_cryptodev_trace_asym_session_create(dev_id, xforms, mp, sess);
	return 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_session_free)
int
rte_cryptodev_sym_session_free(uint8_t dev_id, void *_sess)
{
	struct rte_cryptodev *dev;
	struct rte_mempool *sess_mp;
	struct rte_cryptodev_sym_session *sess = _sess;
	struct rte_cryptodev_sym_session_pool_private_data *pool_priv;

	if (sess == NULL)
		return -EINVAL;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	if (dev == NULL || sess == NULL)
		return -EINVAL;

	sess_mp = rte_mempool_from_obj(sess);
	if (!sess_mp)
		return -EINVAL;
	pool_priv = rte_mempool_get_priv(sess_mp);

	if (sess->driver_id != dev->driver_id) {
		CDEV_LOG_ERR("Session created by driver %u but freed by %u",
			sess->driver_id, dev->driver_id);
		return -EINVAL;
	}

	if (dev->dev_ops->sym_session_clear == NULL)
		return -ENOTSUP;

	dev->dev_ops->sym_session_clear(dev, sess);

	memset(sess->driver_priv_data, 0, pool_priv->sess_data_sz + pool_priv->user_data_sz);

	/* Return session to mempool */
	rte_mempool_put(sess_mp, sess);

	rte_cryptodev_trace_sym_session_free(dev_id, sess);
	return 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_session_free)
int
rte_cryptodev_asym_session_free(uint8_t dev_id, void *sess)
{
	struct rte_mempool *sess_mp;
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	if (dev == NULL || sess == NULL)
		return -EINVAL;

	if (dev->dev_ops->asym_session_clear == NULL)
		return -ENOTSUP;

	dev->dev_ops->asym_session_clear(dev, sess);

	rte_free(((struct rte_cryptodev_asym_session *)sess)->event_mdata);

	/* Return session to mempool */
	sess_mp = rte_mempool_from_obj(sess);
	rte_mempool_put(sess_mp, sess);

	rte_cryptodev_trace_asym_session_free(dev_id, sess);
	return 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_get_header_session_size)
unsigned int
rte_cryptodev_asym_get_header_session_size(void)
{
	return sizeof(struct rte_cryptodev_asym_session);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_get_private_session_size)
unsigned int
rte_cryptodev_sym_get_private_session_size(uint8_t dev_id)
{
	struct rte_cryptodev *dev;
	unsigned int priv_sess_size;

	if (!rte_cryptodev_is_valid_dev(dev_id))
		return 0;

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	if (dev->dev_ops->sym_session_get_size == NULL)
		return 0;

	priv_sess_size = dev->dev_ops->sym_session_get_size(dev);

	rte_cryptodev_trace_sym_get_private_session_size(dev_id,
		priv_sess_size);

	return priv_sess_size;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_get_private_session_size)
unsigned int
rte_cryptodev_asym_get_private_session_size(uint8_t dev_id)
{
	struct rte_cryptodev *dev;
	unsigned int priv_sess_size;

	if (!rte_cryptodev_is_valid_dev(dev_id))
		return 0;

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	if (dev->dev_ops->asym_session_get_size == NULL)
		return 0;

	priv_sess_size = dev->dev_ops->asym_session_get_size(dev);

	rte_cryptodev_trace_asym_get_private_session_size(dev_id,
		priv_sess_size);

	return priv_sess_size;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_session_set_user_data)
int
rte_cryptodev_sym_session_set_user_data(void *_sess, void *data,
		uint16_t size)
{
	struct rte_cryptodev_sym_session *sess = _sess;

	if (sess == NULL)
		return -EINVAL;

	if (sess->user_data_sz < size)
		return -ENOMEM;

	rte_memcpy(sess->driver_priv_data + sess->sess_data_sz, data, size);

	rte_cryptodev_trace_sym_session_set_user_data(sess, data, size);

	return 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_session_get_user_data)
void *
rte_cryptodev_sym_session_get_user_data(void *_sess)
{
	struct rte_cryptodev_sym_session *sess = _sess;
	void *data = NULL;

	if (sess == NULL || sess->user_data_sz == 0)
		return NULL;

	data = (void *)(sess->driver_priv_data + sess->sess_data_sz);

	rte_cryptodev_trace_sym_session_get_user_data(sess, data);

	return data;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_session_set_user_data)
int
rte_cryptodev_asym_session_set_user_data(void *session, void *data, uint16_t size)
{
	struct rte_cryptodev_asym_session *sess = session;
	if (sess == NULL)
		return -EINVAL;

	if (sess->user_data_sz < size)
		return -ENOMEM;

	rte_memcpy(sess->sess_private_data +
			sess->max_priv_data_sz,
			data, size);

	rte_cryptodev_trace_asym_session_set_user_data(sess, data, size);

	return 0;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_asym_session_get_user_data)
void *
rte_cryptodev_asym_session_get_user_data(void *session)
{
	struct rte_cryptodev_asym_session *sess = session;
	void *data = NULL;

	if (sess == NULL || sess->user_data_sz == 0)
		return NULL;

	data = (void *)(sess->sess_private_data + sess->max_priv_data_sz);

	rte_cryptodev_trace_asym_session_get_user_data(sess, data);

	return data;
}

static inline void
sym_crypto_fill_status(struct rte_crypto_sym_vec *vec, int32_t errnum)
{
	uint32_t i;
	for (i = 0; i < vec->num; i++)
		vec->status[i] = errnum;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_sym_cpu_crypto_process)
uint32_t
rte_cryptodev_sym_cpu_crypto_process(uint8_t dev_id,
	void *_sess, union rte_crypto_sym_ofs ofs,
	struct rte_crypto_sym_vec *vec)
{
	struct rte_cryptodev *dev;
	struct rte_cryptodev_sym_session *sess = _sess;

	if (!rte_cryptodev_is_valid_dev(dev_id)) {
		sym_crypto_fill_status(vec, EINVAL);
		return 0;
	}

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	if (dev->dev_ops->sym_cpu_process == NULL ||
		!(dev->feature_flags & RTE_CRYPTODEV_FF_SYM_CPU_CRYPTO)) {
		sym_crypto_fill_status(vec, ENOTSUP);
		return 0;
	}

	rte_cryptodev_trace_sym_cpu_crypto_process(dev_id, sess);

	return dev->dev_ops->sym_cpu_process(dev, sess, ofs, vec);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_get_raw_dp_ctx_size)
int
rte_cryptodev_get_raw_dp_ctx_size(uint8_t dev_id)
{
	struct rte_cryptodev *dev;
	int32_t size = sizeof(struct rte_crypto_raw_dp_ctx);
	int32_t priv_size;

	if (!rte_cryptodev_is_valid_dev(dev_id))
		return -EINVAL;

	dev = rte_cryptodev_pmd_get_dev(dev_id);

	if (dev->dev_ops->sym_get_raw_dp_ctx_size == NULL ||
		!(dev->feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP)) {
		return -ENOTSUP;
	}

	priv_size = dev->dev_ops->sym_get_raw_dp_ctx_size(dev);
	if (priv_size < 0)
		return -ENOTSUP;

	rte_cryptodev_trace_get_raw_dp_ctx_size(dev_id);

	return RTE_ALIGN_CEIL((size + priv_size), 8);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_configure_raw_dp_ctx)
int
rte_cryptodev_configure_raw_dp_ctx(uint8_t dev_id, uint16_t qp_id,
	struct rte_crypto_raw_dp_ctx *ctx,
	enum rte_crypto_op_sess_type sess_type,
	union rte_cryptodev_session_ctx session_ctx,
	uint8_t is_update)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_get_qp_status(dev_id, qp_id))
		return -EINVAL;

	dev = rte_cryptodev_pmd_get_dev(dev_id);
	if (!(dev->feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP)
			|| dev->dev_ops->sym_configure_raw_dp_ctx == NULL)
		return -ENOTSUP;

	rte_cryptodev_trace_configure_raw_dp_ctx(dev_id, qp_id, sess_type);

	return dev->dev_ops->sym_configure_raw_dp_ctx(dev, qp_id, ctx,
			sess_type, session_ctx, is_update);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_session_event_mdata_set)
int
rte_cryptodev_session_event_mdata_set(uint8_t dev_id, void *sess,
	enum rte_crypto_op_type op_type,
	enum rte_crypto_op_sess_type sess_type,
	void *ev_mdata,
	uint16_t size)
{
	struct rte_cryptodev *dev;

	if (sess == NULL || ev_mdata == NULL)
		return -EINVAL;

	if (!rte_cryptodev_is_valid_dev(dev_id))
		goto skip_pmd_op;

	dev = rte_cryptodev_pmd_get_dev(dev_id);
	if (dev->dev_ops->session_ev_mdata_set == NULL)
		goto skip_pmd_op;

	rte_cryptodev_trace_session_event_mdata_set(dev_id, sess, op_type,
		sess_type, ev_mdata, size);

	return dev->dev_ops->session_ev_mdata_set(dev, sess, op_type, sess_type, ev_mdata);

skip_pmd_op:
	if (op_type == RTE_CRYPTO_OP_TYPE_SYMMETRIC)
		return rte_cryptodev_sym_session_set_user_data(sess, ev_mdata,
				size);
	else if (op_type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
		struct rte_cryptodev_asym_session *s = sess;

		if (s->event_mdata == NULL) {
			s->event_mdata = rte_malloc(NULL, size, 0);
			if (s->event_mdata == NULL)
				return -ENOMEM;
		}
		rte_memcpy(s->event_mdata, ev_mdata, size);

		return 0;
	} else
		return -ENOTSUP;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_raw_enqueue_burst)
uint32_t
rte_cryptodev_raw_enqueue_burst(struct rte_crypto_raw_dp_ctx *ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void **user_data, int *enqueue_status)
{
	return ctx->enqueue_burst(ctx->qp_data, ctx->drv_ctx_data, vec,
			ofs, user_data, enqueue_status);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_raw_enqueue_done)
int
rte_cryptodev_raw_enqueue_done(struct rte_crypto_raw_dp_ctx *ctx,
		uint32_t n)
{
	return ctx->enqueue_done(ctx->qp_data, ctx->drv_ctx_data, n);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_raw_dequeue_burst)
uint32_t
rte_cryptodev_raw_dequeue_burst(struct rte_crypto_raw_dp_ctx *ctx,
	rte_cryptodev_raw_get_dequeue_count_t get_dequeue_count,
	uint32_t max_nb_to_dequeue,
	rte_cryptodev_raw_post_dequeue_t post_dequeue,
	void **out_user_data, uint8_t is_user_data_array,
	uint32_t *n_success_jobs, int *status)
{
	return ctx->dequeue_burst(ctx->qp_data, ctx->drv_ctx_data,
		get_dequeue_count, max_nb_to_dequeue, post_dequeue,
		out_user_data, is_user_data_array, n_success_jobs, status);
}

RTE_EXPORT_SYMBOL(rte_cryptodev_raw_dequeue_done)
int
rte_cryptodev_raw_dequeue_done(struct rte_crypto_raw_dp_ctx *ctx,
		uint32_t n)
{
	return ctx->dequeue_done(ctx->qp_data, ctx->drv_ctx_data, n);
}

/** Initialise rte_crypto_op mempool element */
static void
rte_crypto_op_init(struct rte_mempool *mempool,
		void *opaque_arg,
		void *_op_data,
		__rte_unused unsigned i)
{
	struct rte_crypto_op *op = _op_data;
	enum rte_crypto_op_type type = *(enum rte_crypto_op_type *)opaque_arg;

	memset(_op_data, 0, mempool->elt_size);

	__rte_crypto_op_reset(op, type);

	op->phys_addr = rte_mempool_virt2iova(_op_data);
	op->mempool = mempool;
}


RTE_EXPORT_SYMBOL(rte_crypto_op_pool_create)
struct rte_mempool *
rte_crypto_op_pool_create(const char *name, enum rte_crypto_op_type type,
		unsigned nb_elts, unsigned cache_size, uint16_t priv_size,
		int socket_id)
{
	struct rte_crypto_op_pool_private *priv;

	unsigned elt_size = sizeof(struct rte_crypto_op) +
			priv_size;

	if (type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		elt_size += sizeof(struct rte_crypto_sym_op);
	} else if (type == RTE_CRYPTO_OP_TYPE_ASYMMETRIC) {
		elt_size += sizeof(struct rte_crypto_asym_op);
	} else if (type == RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		elt_size += RTE_MAX(sizeof(struct rte_crypto_sym_op),
		                    sizeof(struct rte_crypto_asym_op));
	} else {
		CDEV_LOG_ERR("Invalid op_type");
		return NULL;
	}

	/* lookup mempool in case already allocated */
	struct rte_mempool *mp = rte_mempool_lookup(name);

	if (mp != NULL) {
		priv = (struct rte_crypto_op_pool_private *)
				rte_mempool_get_priv(mp);

		if (mp->elt_size != elt_size ||
				mp->cache_size < cache_size ||
				mp->size < nb_elts ||
				priv->priv_size <  priv_size) {
			mp = NULL;
			CDEV_LOG_ERR("Mempool %s already exists but with "
					"incompatible parameters", name);
			return NULL;
		}
		return mp;
	}

	mp = rte_mempool_create(
			name,
			nb_elts,
			elt_size,
			cache_size,
			sizeof(struct rte_crypto_op_pool_private),
			NULL,
			NULL,
			rte_crypto_op_init,
			&type,
			socket_id,
			0);

	if (mp == NULL) {
		CDEV_LOG_ERR("Failed to create mempool %s", name);
		return NULL;
	}

	priv = (struct rte_crypto_op_pool_private *)
			rte_mempool_get_priv(mp);

	priv->priv_size = priv_size;
	priv->type = type;

	rte_cryptodev_trace_op_pool_create(name, socket_id, type, nb_elts, mp);
	return mp;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_cryptodev_pmd_create_dev_name)
int
rte_cryptodev_pmd_create_dev_name(char *name, const char *dev_name_prefix)
{
	struct rte_cryptodev *dev = NULL;
	uint32_t i = 0;

	if (name == NULL)
		return -EINVAL;

	for (i = 0; i < RTE_CRYPTO_MAX_DEVS; i++) {
		int ret = snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN,
				"%s_%u", dev_name_prefix, i);

		if (ret < 0)
			return ret;

		dev = rte_cryptodev_pmd_get_named_dev(name);
		if (!dev)
			return 0;
	}

	return -1;
}

TAILQ_HEAD(cryptodev_driver_list, cryptodev_driver);

static struct cryptodev_driver_list cryptodev_driver_list =
	TAILQ_HEAD_INITIALIZER(cryptodev_driver_list);

RTE_EXPORT_SYMBOL(rte_cryptodev_driver_id_get)
int
rte_cryptodev_driver_id_get(const char *name)
{
	struct cryptodev_driver *driver;
	const char *driver_name;
	int driver_id = -1;

	if (name == NULL) {
		CDEV_LOG_DEBUG("name pointer NULL");
		return -1;
	}

	TAILQ_FOREACH(driver, &cryptodev_driver_list, next) {
		driver_name = driver->driver->name;
		if (strncmp(driver_name, name, strlen(driver_name) + 1) == 0) {
			driver_id = driver->id;
			break;
		}
	}

	rte_cryptodev_trace_driver_id_get(name, driver_id);

	return driver_id;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_name_get)
const char *
rte_cryptodev_name_get(uint8_t dev_id)
{
	struct rte_cryptodev *dev;

	if (!rte_cryptodev_is_valid_device_data(dev_id)) {
		CDEV_LOG_ERR("Invalid dev_id=%" PRIu8, dev_id);
		return NULL;
	}

	dev = rte_cryptodev_pmd_get_dev(dev_id);
	if (dev == NULL)
		return NULL;

	rte_cryptodev_trace_name_get(dev_id, dev->data->name);

	return dev->data->name;
}

RTE_EXPORT_SYMBOL(rte_cryptodev_driver_name_get)
const char *
rte_cryptodev_driver_name_get(uint8_t driver_id)
{
	struct cryptodev_driver *driver;

	TAILQ_FOREACH(driver, &cryptodev_driver_list, next) {
		if (driver->id == driver_id) {
			rte_cryptodev_trace_driver_name_get(driver_id,
				driver->driver->name);
			return driver->driver->name;
		}
	}
	return NULL;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_cryptodev_allocate_driver)
uint8_t
rte_cryptodev_allocate_driver(struct cryptodev_driver *crypto_drv,
		const struct rte_driver *drv)
{
	crypto_drv->driver = drv;
	crypto_drv->id = nb_drivers;

	TAILQ_INSERT_TAIL(&cryptodev_driver_list, crypto_drv, next);

	rte_cryptodev_trace_allocate_driver(drv->name);

	return nb_drivers++;
}

RTE_INIT(cryptodev_init_fp_ops)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(rte_crypto_fp_ops); i++)
		cryptodev_fp_ops_reset(rte_crypto_fp_ops + i);
}

static int
cryptodev_handle_dev_list(const char *cmd __rte_unused,
		const char *params __rte_unused,
		struct rte_tel_data *d)
{
	int dev_id;

	if (rte_cryptodev_count() < 1)
		return -EINVAL;

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	for (dev_id = 0; dev_id < RTE_CRYPTO_MAX_DEVS; dev_id++)
		if (rte_cryptodev_is_valid_dev(dev_id))
			rte_tel_data_add_array_int(d, dev_id);

	return 0;
}

static int
cryptodev_handle_dev_info(const char *cmd __rte_unused,
		const char *params, struct rte_tel_data *d)
{
	struct rte_cryptodev_info cryptodev_info;
	int dev_id;
	char *end_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	dev_id = strtoul(params, &end_param, 0);
	if (*end_param != '\0')
		CDEV_LOG_ERR("Extra parameters passed to command, ignoring");
	if (!rte_cryptodev_is_valid_dev(dev_id))
		return -EINVAL;

	rte_cryptodev_info_get(dev_id, &cryptodev_info);

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_string(d, "device_name",
		cryptodev_info.device->name);
	rte_tel_data_add_dict_uint(d, "max_nb_queue_pairs",
		cryptodev_info.max_nb_queue_pairs);

	return 0;
}

#define ADD_DICT_STAT(s) rte_tel_data_add_dict_uint(d, #s, cryptodev_stats.s)

static int
cryptodev_handle_dev_stats(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_cryptodev_stats cryptodev_stats;
	int dev_id, ret;
	char *end_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	dev_id = strtoul(params, &end_param, 0);
	if (*end_param != '\0')
		CDEV_LOG_ERR("Extra parameters passed to command, ignoring");
	if (!rte_cryptodev_is_valid_dev(dev_id))
		return -EINVAL;

	ret = rte_cryptodev_stats_get(dev_id, &cryptodev_stats);
	if (ret < 0)
		return ret;

	rte_tel_data_start_dict(d);
	ADD_DICT_STAT(enqueued_count);
	ADD_DICT_STAT(dequeued_count);
	ADD_DICT_STAT(enqueue_err_count);
	ADD_DICT_STAT(dequeue_err_count);

	return 0;
}

#define CRYPTO_CAPS_SZ                                             \
	(RTE_ALIGN_CEIL(sizeof(struct rte_cryptodev_capabilities), \
					sizeof(uint64_t)) /        \
	 sizeof(uint64_t))

static int
crypto_caps_array(struct rte_tel_data *d,
		  const struct rte_cryptodev_capabilities *capabilities)
{
	const struct rte_cryptodev_capabilities *dev_caps;
	uint64_t caps_val[CRYPTO_CAPS_SZ];
	unsigned int i = 0, j;

	rte_tel_data_start_array(d, RTE_TEL_UINT_VAL);

	while ((dev_caps = &capabilities[i++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		memset(&caps_val, 0, CRYPTO_CAPS_SZ * sizeof(caps_val[0]));
		rte_memcpy(caps_val, dev_caps, sizeof(capabilities[0]));
		for (j = 0; j < CRYPTO_CAPS_SZ; j++)
			rte_tel_data_add_array_uint(d, caps_val[j]);
	}

	return i;
}

static int
cryptodev_handle_dev_caps(const char *cmd __rte_unused, const char *params,
			  struct rte_tel_data *d)
{
	struct rte_cryptodev_info dev_info;
	struct rte_tel_data *crypto_caps;
	int crypto_caps_n;
	char *end_param;
	int dev_id;

	if (!params || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	dev_id = strtoul(params, &end_param, 0);
	if (*end_param != '\0')
		CDEV_LOG_ERR("Extra parameters passed to command, ignoring");
	if (!rte_cryptodev_is_valid_dev(dev_id))
		return -EINVAL;

	rte_tel_data_start_dict(d);
	crypto_caps = rte_tel_data_alloc();
	if (!crypto_caps)
		return -ENOMEM;

	rte_cryptodev_info_get(dev_id, &dev_info);
	crypto_caps_n = crypto_caps_array(crypto_caps, dev_info.capabilities);
	rte_tel_data_add_dict_container(d, "crypto_caps", crypto_caps, 0);
	rte_tel_data_add_dict_int(d, "crypto_caps_n", crypto_caps_n);

	return 0;
}

RTE_INIT(cryptodev_init_telemetry)
{
	rte_telemetry_register_cmd("/cryptodev/info", cryptodev_handle_dev_info,
			"Returns information for a cryptodev. Parameters: int dev_id");
	rte_telemetry_register_cmd("/cryptodev/list",
			cryptodev_handle_dev_list,
			"Returns list of available crypto devices by IDs. No parameters.");
	rte_telemetry_register_cmd("/cryptodev/stats",
			cryptodev_handle_dev_stats,
			"Returns the stats for a cryptodev. Parameters: int dev_id");
	rte_telemetry_register_cmd("/cryptodev/caps",
			cryptodev_handle_dev_caps,
			"Returns the capabilities for a cryptodev. Parameters: int dev_id");
}
