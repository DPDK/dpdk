/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include <rte_cryptodev.h>

#include "otx_cryptodev_capabilities.h"

static const struct rte_cryptodev_capabilities otx_sym_capabilities[] = {
	/* Symmetric capabilities */
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
			}, },
		}, },
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
					.min = 8,
					.max = 16,
					.increment = 4
				},
				.iv_size = {
					.min = 12,
					.max = 12,
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
					.min = 1,
					.max = 16,
					.increment = 1
				},
			}, }
		}, }
	},
	{	/* MD5 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_MD5_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 8,
					.max = 64,
					.increment = 8
				},
				.digest_size = {
					.min = 1,
					.max = 16,
					.increment = 1
				},
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
					.max = 1024,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 20,
					.increment = 1
				},
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
					.max = 1024,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 28,
					.increment = 1
				},
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
					.max = 1024,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 32,
					.increment = 1
				},
			}, }
		}, }
	},
	{	/* SHA384 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384,
				.block_size = 64,
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
			}, }
		}, }
	},
	{	/* SHA384 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 1024,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 48,
					.increment = 1
					},
			}, }
		}, }
	},
	{	/* SHA512 */
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
					.max = 1024,
					.increment = 1
				},
				.digest_size = {
					.min = 1,
					.max = 64,
					.increment = 1
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
	{	/* 3DES CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_CBC,
				.block_size = 8,
				.key_size = {
					.min = 24,
					.max = 24,
					.increment = 0
				},
				.iv_size = {
					.min = 8,
					.max = 16,
					.increment = 8
				}
			}, }
		}, }
	},
	{	/* 3DES ECB */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_3DES_ECB,
				.block_size = 8,
				.key_size = {
					.min = 24,
					.max = 24,
					.increment = 0
				},
				.iv_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				}
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
	{	/* AES XTS */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_XTS,
				.block_size = 16,
				.key_size = {
					.min = 32,
					.max = 64,
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
					.min = 8,
					.max = 16,
					.increment = 4
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
				}
			}, }
		}, }
	},
	/* End of symmetric capabilities */
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_cryptodev_capabilities otx_asym_capabilities[] = {
	/* Asymmetric capabilities */
	{	/* RSA */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_RSA,
				.op_types = ((1 << RTE_CRYPTO_ASYM_OP_SIGN) |
					(1 << RTE_CRYPTO_ASYM_OP_VERIFY) |
					(1 << RTE_CRYPTO_ASYM_OP_ENCRYPT) |
					(1 << RTE_CRYPTO_ASYM_OP_DECRYPT)),
				{.modlen = {
					.min = 17,
					.max = 1024,
					.increment = 1
				}, }
			}
		}, }
	},
	{	/* MOD_EXP */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX,
				.op_types = 0,
				{.modlen = {
					.min = 17,
					.max = 1024,
					.increment = 1
				}, }
			}
		}, }
	},
	{	/* ECDSA */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_ECDSA,
				.op_types = ((1 << RTE_CRYPTO_ASYM_OP_SIGN) |
					(1 << RTE_CRYPTO_ASYM_OP_VERIFY)),
				}
			},
		}
	},
	{	/* ECPM */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_ECPM,
				.op_types = 0
				}
			},
		}
	},
	/* End of asymmetric capabilities */
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

const struct rte_cryptodev_capabilities *
otx_get_capabilities(uint64_t flags)
{
	if (flags & RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO)
		return otx_asym_capabilities;
	else
		return otx_sym_capabilities;

}
