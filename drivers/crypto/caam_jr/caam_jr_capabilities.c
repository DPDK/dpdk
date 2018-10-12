/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
 */

#include <caam_jr_capabilities.h>

static const struct rte_cryptodev_capabilities caam_jr_capabilities[] = {
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
					.max = 64,
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
	{	/* SHA224 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA224_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
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
	{	/* SHA256 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA256_HMAC,
				.block_size = 64,
				.key_size = {
					.min = 1,
					.max = 64,
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
	{	/* SHA384 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA384_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 1,
					.max = 128,
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
	{	/* SHA512 HMAC */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_SHA512_HMAC,
				.block_size = 128,
				.key_size = {
					.min = 1,
					.max = 128,
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
					.min = 16,
					.max = 16,
					.increment = 0
				}
			}, }
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
					.min = 16,
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

	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static const struct rte_security_capability caam_jr_security_cap[] = {
	{ /* IPsec Lookaside Protocol offload ESP Transport Egress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = caam_jr_capabilities
	},
	{ /* IPsec Lookaside Protocol offload ESP Tunnel Ingress */
		.action = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
		.ipsec = {
			.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP,
			.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
			.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
			.options = { 0 }
		},
		.crypto_capabilities = caam_jr_capabilities
	},
	{
		.action = RTE_SECURITY_ACTION_TYPE_NONE
	}
};

const struct rte_cryptodev_capabilities *
caam_jr_get_cryptodev_capabilities(void)
{
	return caam_jr_capabilities;
}

const struct rte_security_capability *
caam_jr_get_security_capabilities(void *device __rte_unused)
{
	return caam_jr_security_cap;
}
