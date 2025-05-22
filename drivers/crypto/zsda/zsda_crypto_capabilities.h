/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 ZTE Corporation
 */

#ifndef _ZSDA_SYM_CAPABILITIES_H_
#define _ZSDA_SYM_CAPABILITIES_H_

static const struct rte_cryptodev_capabilities zsda_crypto_dev_capabilities[] = {
	{/* SHA1 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{ .auth = {
				.algo = RTE_CRYPTO_AUTH_SHA1,
				.block_size = 64,
				.key_size = {.min = 0, .max = 0, .increment = 0},
				.digest_size = {.min = 20, .max = 20, .increment = 2},
				.iv_size = {0} },
			}	},
		}
	},
	{/* SHA224 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{ .auth = {
				.algo = RTE_CRYPTO_AUTH_SHA224,
				.block_size = 64,
				.key_size = {.min = 0, .max = 0, .increment = 0},
				.digest_size = {.min = 28, .max = 28, .increment = 0},
				.iv_size = {0} },
			}	},
		}
	},
	{/* SHA256 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{ .auth = {
				.algo = RTE_CRYPTO_AUTH_SHA256,
				.block_size = 64,
				.key_size = {.min = 0, .max = 0, .increment = 0},
				.digest_size = {.min = 32, .max = 32, .increment = 0},
				.iv_size = {0} },
			} },
		}
	},
	{/* SHA384 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{ .auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384,
				.block_size = 128,
				.key_size = {.min = 0, .max = 0, .increment = 0},
				.digest_size = {.min = 48, .max = 48, .increment = 0},
				.iv_size = {0} },
			} },
		}
	},
	{/* SHA512 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{ .auth = {
				.algo = RTE_CRYPTO_AUTH_SHA512,
				.block_size = 128,
				.key_size = {.min = 0, .max = 0, .increment = 0},
				.digest_size = {.min = 64, .max = 64, .increment = 0},
				.iv_size = {0} },
			} },
		}
	},
	{/* SM3 */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{ .auth = {
				.algo = RTE_CRYPTO_AUTH_SM3,
				.block_size = 64,
				.key_size = {.min = 0, .max = 0, .increment = 0},
				.digest_size = {.min = 32, .max = 32, .increment = 0},
				.iv_size = {0} },
			} },
		}
	},
	{/* AES XTS */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{ .cipher = {
				.algo = RTE_CRYPTO_CIPHER_AES_XTS,
				.block_size = 16,
				.key_size = {.min = 32, .max = 64, .increment = 32},
				.iv_size = {.min = 16, .max = 16, .increment = 0} },
			} },
		}
	},
	{/* SM4 XTS */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{ .sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
			{ .cipher = {
				.algo = RTE_CRYPTO_CIPHER_SM4_XTS,
				.block_size = 16,
				.key_size = {.min = 32, .max = 32, .increment = 0},
				.iv_size = {.min = 16, .max = 16, .increment = 0} },
			} },
		}
	}
};
#endif /* _ZSDA_SYM_CAPABILITIES_H_ */
