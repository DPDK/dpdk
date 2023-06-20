/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2021 Intel Corporation
 */

#ifndef _PMD_AESNI_MB_PRIV_H_
#define _PMD_AESNI_MB_PRIV_H_

#define AESNI_MB_DOCSIS_SEC_ENABLED 1
#include <rte_security.h>
#include <rte_security_driver.h>
#include <rte_ether.h>

#include "ipsec_mb_private.h"

#define AES_CCM_DIGEST_MIN_LEN 4
#define AES_CCM_DIGEST_MAX_LEN 16
#define HMAC_MAX_BLOCK_SIZE 128
#define HMAC_IPAD_VALUE			(0x36)
#define HMAC_OPAD_VALUE			(0x5C)

#if IMB_VERSION(1, 2, 0) < IMB_VERSION_NUM
#define MAX_NUM_SEGS 16
#endif

static const struct rte_cryptodev_capabilities aesni_mb_capabilities[] = {
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
				.iv_size = { 0 }
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
				.iv_size = { 0 }
			}, },
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
					.max = 32,
					.increment = 16
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
	{	/* ZUC (EIA3) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_ZUC_EIA3,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 16
				},
				.digest_size = {
					.min = 4,
#if IMB_VERSION(1, 2, 0) < IMB_VERSION_NUM
					.max = 16,
					.increment = 4
#else
					.max = 4,
					.increment = 0
#endif
				},
				.iv_size = {
					.min = 16,
					.max = 25,
					.increment = 9
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
					.max = 32,
					.increment = 16
				},
				.iv_size = {
					.min = 16,
					.max = 25,
					.increment = 9
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
					.max = 1024,
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
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

uint8_t pmd_driver_id_aesni_mb;

struct aesni_mb_qp_data {
	uint8_t temp_digests[IMB_MAX_JOBS][DIGEST_LENGTH_MAX];
	/* *< Buffers used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
#if IMB_VERSION(1, 2, 0) < IMB_VERSION_NUM
	struct IMB_SGL_IOV sgl_segs[MAX_NUM_SEGS];
#endif
	union {
		struct gcm_context_data gcm_sgl_ctx;
		struct chacha20_poly1305_context_data chacha_sgl_ctx;
	};
};

/* Maximum length for digest */
#define DIGEST_LENGTH_MAX 64
static const unsigned int auth_blocksize[] = {
		[IMB_AUTH_NULL]			= 0,
		[IMB_AUTH_MD5]			= 64,
		[IMB_AUTH_HMAC_SHA_1]		= 64,
		[IMB_AUTH_HMAC_SHA_224]		= 64,
		[IMB_AUTH_HMAC_SHA_256]		= 64,
		[IMB_AUTH_HMAC_SHA_384]		= 128,
		[IMB_AUTH_HMAC_SHA_512]		= 128,
		[IMB_AUTH_AES_XCBC]		= 16,
		[IMB_AUTH_AES_CCM]		= 16,
		[IMB_AUTH_AES_CMAC]		= 16,
		[IMB_AUTH_AES_GMAC]		= 16,
		[IMB_AUTH_SHA_1]		= 64,
		[IMB_AUTH_SHA_224]		= 64,
		[IMB_AUTH_SHA_256]		= 64,
		[IMB_AUTH_SHA_384]		= 128,
		[IMB_AUTH_SHA_512]		= 128,
		[IMB_AUTH_ZUC_EIA3_BITLEN]	= 16,
		[IMB_AUTH_SNOW3G_UIA2_BITLEN]	= 16,
		[IMB_AUTH_KASUMI_UIA1]		= 16
};

/**
 * Get the blocksize in bytes for a specified authentication algorithm
 *
 * @Note: this function will not return a valid value for a non-valid
 * authentication algorithm
 */
static inline unsigned int
get_auth_algo_blocksize(IMB_HASH_ALG algo)
{
	return auth_blocksize[algo];
}

static const unsigned int auth_truncated_digest_byte_lengths[] = {
		[IMB_AUTH_MD5]			= 12,
		[IMB_AUTH_HMAC_SHA_1]		= 12,
		[IMB_AUTH_HMAC_SHA_224]		= 14,
		[IMB_AUTH_HMAC_SHA_256]		= 16,
		[IMB_AUTH_HMAC_SHA_384]		= 24,
		[IMB_AUTH_HMAC_SHA_512]		= 32,
		[IMB_AUTH_AES_XCBC]		= 12,
		[IMB_AUTH_AES_CMAC]		= 12,
		[IMB_AUTH_AES_CCM]		= 8,
		[IMB_AUTH_NULL]			= 0,
		[IMB_AUTH_AES_GMAC]		= 12,
		[IMB_AUTH_SHA_1]		= 20,
		[IMB_AUTH_SHA_224]		= 28,
		[IMB_AUTH_SHA_256]		= 32,
		[IMB_AUTH_SHA_384]		= 48,
		[IMB_AUTH_SHA_512]		= 64,
		[IMB_AUTH_ZUC_EIA3_BITLEN]	= 4,
		[IMB_AUTH_SNOW3G_UIA2_BITLEN]	= 4,
		[IMB_AUTH_KASUMI_UIA1]		= 4
};

/**
 * Get the IPsec specified truncated length in bytes of the HMAC digest for a
 * specified authentication algorithm
 *
 * @Note: this function will not return a valid value for a non-valid
 * authentication algorithm
 */
static inline unsigned int
get_truncated_digest_byte_length(IMB_HASH_ALG algo)
{
	return auth_truncated_digest_byte_lengths[algo];
}

static const unsigned int auth_digest_byte_lengths[] = {
		[IMB_AUTH_MD5]			= 16,
		[IMB_AUTH_HMAC_SHA_1]		= 20,
		[IMB_AUTH_HMAC_SHA_224]		= 28,
		[IMB_AUTH_HMAC_SHA_256]		= 32,
		[IMB_AUTH_HMAC_SHA_384]		= 48,
		[IMB_AUTH_HMAC_SHA_512]		= 64,
		[IMB_AUTH_AES_XCBC]		= 16,
		[IMB_AUTH_AES_CMAC]		= 16,
		[IMB_AUTH_AES_CCM]		= 16,
		[IMB_AUTH_AES_GMAC]		= 16,
		[IMB_AUTH_NULL]			= 0,
		[IMB_AUTH_SHA_1]		= 20,
		[IMB_AUTH_SHA_224]		= 28,
		[IMB_AUTH_SHA_256]		= 32,
		[IMB_AUTH_SHA_384]		= 48,
		[IMB_AUTH_SHA_512]		= 64,
		[IMB_AUTH_ZUC_EIA3_BITLEN]	= 4,
		[IMB_AUTH_SNOW3G_UIA2_BITLEN]	= 4,
		[IMB_AUTH_KASUMI_UIA1]		= 4
	/**< Vector mode dependent pointer table of the multi-buffer APIs */

};

/**
 * Get the full digest size in bytes for a specified authentication algorithm
 * (if available in the Multi-buffer library)
 *
 * @Note: this function will not return a valid value for a non-valid
 * authentication algorithm
 */
static inline unsigned int
get_digest_byte_length(IMB_HASH_ALG algo)
{
	return auth_digest_byte_lengths[algo];
}

/** AES-NI multi-buffer private session structure */
struct aesni_mb_session {
	IMB_JOB template_job;
	/*< Template job structure */
	uint32_t session_id;
	/*< IPSec MB session ID */
	struct {
		uint16_t offset;
	} iv;
	struct {
		uint16_t offset;
	} auth_iv;
	/* *< IV parameters
	 */

	/* * Cipher Parameters
	 */
	struct {
		union {
			struct {
				uint32_t encode[60] __rte_aligned(16);
				/* *< encode key */
				uint32_t decode[60] __rte_aligned(16);
				/* *< decode key */
			} expanded_aes_keys;
			/* *< Expanded AES keys - Allocating space to
			 * contain the maximum expanded key size which
			 * is 240 bytes for 256 bit AES, calculate by:
			 * ((key size (bytes)) *
			 * ((number of rounds) + 1))
			 */
			struct {
				const void *ks_ptr[3];
				uint64_t key[3][16];
			} exp_3des_keys;
			/* *< Expanded 3DES keys */

			struct gcm_key_data gcm_key;
			/* *< Expanded GCM key */
			uint8_t zuc_cipher_key[32];
			/* *< ZUC cipher key */
			snow3g_key_schedule_t pKeySched_snow3g_cipher;
			/* *< SNOW3G scheduled cipher key */
			kasumi_key_sched_t pKeySched_kasumi_cipher;
			/* *< KASUMI scheduled cipher key */
		};
	} cipher;

	/* *< Authentication Parameters */
	struct {
		enum rte_crypto_auth_operation operation;
		/* *< auth operation generate or verify */
		union {
			struct {
				uint8_t inner[128] __rte_aligned(16);
				/* *< inner pad */
				uint8_t outer[128] __rte_aligned(16);
				/* *< outer pad */
			} pads;
			/* *< HMAC Authentication pads -
			 * allocating space for the maximum pad
			 * size supported which is 128 bytes for
			 * SHA512
			 */

			struct {
				uint32_t k1_expanded[44] __rte_aligned(16);
				/* *< k1 (expanded key). */
				uint8_t k2[16] __rte_aligned(16);
				/* *< k2. */
				uint8_t k3[16] __rte_aligned(16);
				/* *< k3. */
			} xcbc;

			struct {
				uint32_t expkey[60] __rte_aligned(16);
				/* *< k1 (expanded key). */
				uint32_t skey1[4] __rte_aligned(16);
				/* *< k2. */
				uint32_t skey2[4] __rte_aligned(16);
				/* *< k3. */
			} cmac;
			/* *< Expanded XCBC authentication keys */
			uint8_t zuc_auth_key[32];
			/* *< ZUC authentication key */
			snow3g_key_schedule_t pKeySched_snow3g_auth;
			/* *< SNOW3G scheduled authentication key */
			kasumi_key_sched_t pKeySched_kasumi_auth;
			/* *< KASUMI scheduled authentication key */
		};
		/* * Requested digest size from Cryptodev */
		uint16_t req_digest_len;

	} auth;
} __rte_cache_aligned;

typedef void (*hash_one_block_t)(const void *data, void *digest);
typedef void (*aes_keyexp_t)(const void *key, void *enc_exp_keys,
			void *dec_exp_keys);

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

#endif /* _PMD_AESNI_MB_PRIV_H_ */
