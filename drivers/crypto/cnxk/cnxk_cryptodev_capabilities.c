/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <cryptodev_pmd.h>
#include <rte_security.h>

#include "roc_api.h"

#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_capabilities.h"
#include "cnxk_security_ar.h"

#define CPT_CAPS_ADD(cnxk_caps, cur_pos, hw_caps, name)                        \
	do {                                                                   \
		if ((hw_caps[CPT_ENG_TYPE_SE].name) ||                         \
		    (hw_caps[CPT_ENG_TYPE_IE].name) ||                         \
		    (hw_caps[CPT_ENG_TYPE_AE].name))                           \
			cpt_caps_add(cnxk_caps, cur_pos, caps_##name,          \
				     RTE_DIM(caps_##name));                    \
	} while (0)

#define SEC_CAPS_ADD(cnxk_caps, cur_pos, hw_caps, name)                        \
	do {                                                                   \
		if ((hw_caps[CPT_ENG_TYPE_SE].name) ||                         \
		    (hw_caps[CPT_ENG_TYPE_IE].name) ||                         \
		    (hw_caps[CPT_ENG_TYPE_AE].name))                           \
			sec_caps_add(cnxk_caps, cur_pos, sec_caps_##name,      \
				     RTE_DIM(sec_caps_##name));                \
	} while (0)

static const struct rte_cryptodev_capabilities caps_mul[] = {
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
	{	/* ECFPM */
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,
		{.asym = {
			.xform_capa = {
				.xform_type = RTE_CRYPTO_ASYM_XFORM_ECFPM,
				.op_types = 0
				}
			},
		}
	},
};

static const struct rte_cryptodev_capabilities caps_sha1_sha2[] = {
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
};

static const struct rte_cryptodev_capabilities caps_sm3[] = {
	{	/* SM3 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SM3,
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
			}, }
		}, }
	}
};

static const struct rte_cryptodev_capabilities caps_sha3[] = {
	{	/* SHA3_224 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA3_224,
				.block_size = 144,
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
	{	/* SHA3_224 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA3_224_HMAC,
				.block_size = 144,
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
	{	/* SHA3_256 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA3_256,
				.block_size = 136,
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
	{	/* SHA3_256 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA3_256_HMAC,
				.block_size = 136,
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
	{	/* SHA3_384 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA3_384,
				.block_size = 104,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 1,
					.max = 48,
					.increment = 0
					},
			}, }
		}, }
	},
	{	/* SHA3_384 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA3_384_HMAC,
				.block_size = 104,
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
	{	/* SHA3_512 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA3_512,
				.block_size = 72,
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
	{	/* SHA3_512 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA3_512_HMAC,
				.block_size = 72,
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
	{	/* SHAKE_128 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHAKE_128,
				.block_size = 168,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 1,
					.max = 255,
					.increment = 1
				},
			}, }
		}, }
	},
	{	/* SHAKE_256 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHAKE_256,
				.block_size = 136,
				.key_size = {
					.min = 0,
					.max = 0,
					.increment = 0
				},
				.digest_size = {
					.min = 1,
					.max = 255,
					.increment = 1
				},
			}, }
		}, }
	},
};

static const struct rte_cryptodev_capabilities caps_chacha20[] = {
	{	/* Chacha20-Poly1305 */
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
	}
};

static const struct rte_cryptodev_capabilities caps_zuc_snow3g[] = {
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
};

static const struct rte_cryptodev_capabilities caps_aes[] = {
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
					.increment = 32
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
					.min = 4,
					.max = 16,
					.increment = 1
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
					.increment = 1
				},
				.aad_size = {
					.min = 0,
					.max = 1024,
					.increment = 1
				},
				.iv_size = {
					.min = 11,
					.max = 13,
					.increment = 1
				}
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
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 4,
					.max = 4,
					.increment = 0
				},
			}, }
		}, }
	},
};

static const struct rte_cryptodev_capabilities caps_kasumi[] = {
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
};

static const struct rte_cryptodev_capabilities caps_des[] = {
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
};

static const struct rte_cryptodev_capabilities caps_docsis[] = {
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
};

static const struct rte_cryptodev_capabilities caps_null[] = {
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
};

static const struct rte_cryptodev_capabilities caps_sm4[] = {
	{	/* SM4 CBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_SM4_CBC,
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
	{	/* SM4 ECB */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_SM4_ECB,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 16,
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
	{	/* SM4 CTR */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_SM4_CTR,
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
	{	/* SM4 OFB */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_SM4_OFB,
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
	{	/* SM4 CFB */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{.cipher = {
				.algo = RTE_CRYPTO_CIPHER_SM4_CFB,
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
};

static const struct rte_cryptodev_capabilities caps_end[] = {
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_cryptodev_capabilities sec_caps_aes[] = {
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
					.min = 8,
					.max = 12,
					.increment = 4
				},
				.iv_size = {
					.min = 11,
					.max = 11,
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
					.increment = 8
				},
				.digest_size = {
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.aad_size = {
					.min = 8,
					.max = 12,
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
	{	/* AES-XCBC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {
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
					.increment = 0,
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
};

static const struct rte_cryptodev_capabilities sec_caps_des[] = {
	{	/* DES  */
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
};

static const struct rte_cryptodev_capabilities sec_caps_sha1_sha2[] = {
	{	/* SHA1 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 20,
					.max = 64,
					.increment = 1
				},
				.digest_size = {
					.min = 12,
					.max = 12,
					.increment = 0
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
					.min = 16,
					.max = 16,
					.increment = 0
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
					.min = 48,
					.max = 48,
					.increment = 0
				},
				.digest_size = {
					.min = 24,
					.max = 24,
					.increment = 0
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
					.min = 64,
					.max = 64,
					.increment = 0
				},
				.digest_size = {
					.min = 32,
					.max = 32,
					.increment = 0
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
					.min = 16,
					.max = 16,
					.increment = 0
				},
				.digest_size = {
					.min = 12,
					.max = 12,
					.increment = 0
				},
			}, }
		}, }
	},
};

static const struct rte_cryptodev_capabilities sec_caps_null[] = {
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
};

static const struct rte_security_capability sec_caps_templ[] = {
	{	/* IPsec Lookaside Protocol ESP Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 },
		},
		.crypto_capabilities = NULL,
	},
	{	/* IPsec Lookaside Protocol ESP Tunnel Egress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 },
		},
		.crypto_capabilities = NULL,
	},
	{	/* IPsec Lookaside Protocol ESP Transport Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 },
		},
		.crypto_capabilities = NULL,
	},
	{	/* IPsec Lookaside Protocol ESP Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 },
		},
		.crypto_capabilities = NULL,
	},
	{	/* IPsec Lookaside Protocol AH Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_AH,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 },
		},
		.crypto_capabilities = NULL,
	},
	{	/* IPsec Lookaside Protocol AH Tunnel Egress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_AH,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 },
		},
		.crypto_capabilities = NULL,
	},
	{	/* IPsec Lookaside Protocol AH Transport Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_AH,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 },
		},
		.crypto_capabilities = NULL,
	},
	{	/* IPsec Lookaside Protocol AH Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_AH,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 },
		},
		.crypto_capabilities = NULL,
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

static void
cpt_caps_add(struct rte_cryptodev_capabilities cnxk_caps[], int *cur_pos,
	     const struct rte_cryptodev_capabilities *caps, int nb_caps)
{
	PLT_VERIFY(*cur_pos + nb_caps <= CNXK_CPT_MAX_CAPS);

	memcpy(&cnxk_caps[*cur_pos], caps, nb_caps * sizeof(caps[0]));
	*cur_pos += nb_caps;
}

static void
cn10k_crypto_caps_update(struct rte_cryptodev_capabilities cnxk_caps[])
{

	struct rte_cryptodev_capabilities *caps;
	int i = 0;

	while ((caps = &cnxk_caps[i++])->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if ((caps->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC) &&
		    (caps->sym.xform_type == RTE_CRYPTO_SYM_XFORM_CIPHER) &&
		    (caps->sym.cipher.algo == RTE_CRYPTO_CIPHER_ZUC_EEA3)) {

			caps->sym.cipher.key_size.max = 32;
			caps->sym.cipher.key_size.increment = 16;
			caps->sym.cipher.iv_size.max = 25;
			caps->sym.cipher.iv_size.increment = 1;
		}

		if ((caps->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC) &&
		    (caps->sym.xform_type == RTE_CRYPTO_SYM_XFORM_AUTH) &&
		    (caps->sym.auth.algo == RTE_CRYPTO_AUTH_ZUC_EIA3)) {

			caps->sym.auth.key_size.max = 32;
			caps->sym.auth.key_size.increment = 16;
			caps->sym.auth.digest_size.max = 16;
			caps->sym.auth.digest_size.increment = 4;
			caps->sym.auth.iv_size.max = 25;
			caps->sym.auth.iv_size.increment = 1;
		}
	}
}

static void
cn9k_crypto_caps_add(struct rte_cryptodev_capabilities cnxk_caps[], int *cur_pos)
{
	    cpt_caps_add(cnxk_caps, cur_pos, caps_docsis, RTE_DIM(caps_docsis));
}

static void
cn10k_crypto_caps_add(struct rte_cryptodev_capabilities cnxk_caps[],
		     union cpt_eng_caps *hw_caps, int *cur_pos)
{
	if (hw_caps->sg_ver2) {
		CPT_CAPS_ADD(cnxk_caps, cur_pos, hw_caps, sm3);
		CPT_CAPS_ADD(cnxk_caps, cur_pos, hw_caps, sm4);
	}
}

static void
crypto_caps_populate(struct rte_cryptodev_capabilities cnxk_caps[],
		     union cpt_eng_caps *hw_caps)
{
	int cur_pos = 0;

	CPT_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, mul);
	CPT_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, sha1_sha2);
	CPT_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, sha3);
	CPT_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, chacha20);
	CPT_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, zuc_snow3g);
	CPT_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, aes);
	CPT_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, kasumi);
	CPT_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, des);

	if (!roc_model_is_cn10k())
		cn9k_crypto_caps_add(cnxk_caps, &cur_pos);

	if (roc_model_is_cn10k())
		cn10k_crypto_caps_add(cnxk_caps, hw_caps, &cur_pos);

	cpt_caps_add(cnxk_caps, &cur_pos, caps_null, RTE_DIM(caps_null));
	cpt_caps_add(cnxk_caps, &cur_pos, caps_end, RTE_DIM(caps_end));

	if (roc_model_is_cn10k())
		cn10k_crypto_caps_update(cnxk_caps);
}

const struct rte_cryptodev_capabilities *
cnxk_crypto_capabilities_get(struct cnxk_cpt_vf *vf)
{
	return vf->crypto_caps;
}

static void
sec_caps_limit_check(int *cur_pos, int nb_caps)
{
	PLT_VERIFY(*cur_pos + nb_caps <= CNXK_SEC_CRYPTO_MAX_CAPS);
}

static void
sec_caps_add(struct rte_cryptodev_capabilities cnxk_caps[], int *cur_pos,
	     const struct rte_cryptodev_capabilities *caps, int nb_caps)
{
	sec_caps_limit_check(cur_pos, nb_caps);

	memcpy(&cnxk_caps[*cur_pos], caps, nb_caps * sizeof(caps[0]));
	*cur_pos += nb_caps;
}

static void
cn10k_sec_crypto_caps_update(struct rte_cryptodev_capabilities cnxk_caps[],
			     int *cur_pos)
{
	const struct rte_cryptodev_capabilities *cap;
	unsigned int i;

	sec_caps_limit_check(cur_pos, 1);

	/* NULL auth */
	for (i = 0; i < RTE_DIM(caps_null); i++) {
		cap = &caps_null[i];
		if (cap->sym.xform_type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		    cap->sym.auth.algo == RTE_CRYPTO_AUTH_NULL) {
			cnxk_caps[*cur_pos] = caps_null[i];
			*cur_pos += 1;
		}
	}
}

static void
cn9k_sec_crypto_caps_update(struct rte_cryptodev_capabilities cnxk_caps[])
{

	struct rte_cryptodev_capabilities *caps;
	int i = 0;

	while ((caps = &cnxk_caps[i++])->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if ((caps->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC) &&
		    (caps->sym.xform_type == RTE_CRYPTO_SYM_XFORM_AUTH) &&
		    (caps->sym.auth.algo == RTE_CRYPTO_AUTH_SHA256_HMAC)) {
			caps->sym.auth.key_size.min = 32;
			caps->sym.auth.key_size.max = 64;
			caps->sym.auth.key_size.increment = 1;

			break;
		}
	}
}

static void
sec_crypto_caps_populate(struct rte_cryptodev_capabilities cnxk_caps[],
			 union cpt_eng_caps *hw_caps)
{
	int cur_pos = 0;

	SEC_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, aes);
	SEC_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, des);
	SEC_CAPS_ADD(cnxk_caps, &cur_pos, hw_caps, sha1_sha2);

	if (roc_model_is_cn10k())
		cn10k_sec_crypto_caps_update(cnxk_caps, &cur_pos);
	else
		cn9k_sec_crypto_caps_update(cnxk_caps);

	sec_caps_add(cnxk_caps, &cur_pos, sec_caps_null,
		     RTE_DIM(sec_caps_null));
	sec_caps_add(cnxk_caps, &cur_pos, caps_end, RTE_DIM(caps_end));
}

static void
cnxk_sec_caps_update(struct rte_security_capability *sec_cap)
{
	sec_cap->ipsec.options.udp_encap = 1;
	sec_cap->ipsec.options.copy_df = 1;
	sec_cap->ipsec.options.copy_dscp = 1;
}

static void
cn10k_sec_caps_update(struct rte_security_capability *sec_cap)
{
	if (sec_cap->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
#ifdef LA_IPSEC_DEBUG
		sec_cap->ipsec.options.iv_gen_disable = 1;
#endif
	} else {
		sec_cap->ipsec.options.udp_ports_verify = 1;
		if (sec_cap->ipsec.mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL)
			sec_cap->ipsec.options.tunnel_hdr_verify =
				RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR;
	}
	sec_cap->ipsec.options.dec_ttl = 1;
	sec_cap->ipsec.options.ip_csum_enable = 1;
	sec_cap->ipsec.options.l4_csum_enable = 1;
	sec_cap->ipsec.options.stats = 1;
	sec_cap->ipsec.options.esn = 1;
	sec_cap->ipsec.options.copy_flabel = 1;
	sec_cap->ipsec.replay_win_sz_max = ROC_AR_WIN_SIZE_MAX;
}

static void
cn9k_sec_caps_update(struct rte_security_capability *sec_cap)
{
	if (sec_cap->ipsec.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
#ifdef LA_IPSEC_DEBUG
		sec_cap->ipsec.options.iv_gen_disable = 1;
#endif
	}
	sec_cap->ipsec.replay_win_sz_max = CNXK_ON_AR_WIN_SIZE_MAX;
	sec_cap->ipsec.options.esn = 1;
}

void
cnxk_cpt_caps_populate(struct cnxk_cpt_vf *vf)
{
	unsigned long i;

	crypto_caps_populate(vf->crypto_caps, vf->cpt.hw_caps);
	sec_crypto_caps_populate(vf->sec_crypto_caps, vf->cpt.hw_caps);

	PLT_STATIC_ASSERT(RTE_DIM(sec_caps_templ) <= RTE_DIM(vf->sec_caps));
	memcpy(vf->sec_caps, sec_caps_templ, sizeof(sec_caps_templ));

	for (i = 0; i < RTE_DIM(sec_caps_templ) - 1; i++) {
		vf->sec_caps[i].crypto_capabilities = vf->sec_crypto_caps;

		cnxk_sec_caps_update(&vf->sec_caps[i]);

		if (roc_model_is_cn10k())
			cn10k_sec_caps_update(&vf->sec_caps[i]);

		if (roc_model_is_cn9k())
			cn9k_sec_caps_update(&vf->sec_caps[i]);

	}
}

const struct rte_security_capability *
cnxk_crypto_sec_capabilities_get(void *device)
{
	struct rte_cryptodev *dev = device;
	struct cnxk_cpt_vf *vf;

	vf = dev->data->dev_private;
	return vf->sec_caps;
}
