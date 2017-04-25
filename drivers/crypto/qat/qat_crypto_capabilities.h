/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _QAT_CRYPTO_CAPABILITIES_H_
#define _QAT_CRYPTO_CAPABILITIES_H_

#define QAT_BASE_CPM16_SYM_CAPABILITIES					\
	{	/* SHA1 HMAC */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,	\
				.block_size = 64,			\
				.key_size = {				\
					.min = 64,			\
					.max = 64,			\
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
	{	/* SHA224 HMAC */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_SHA224_HMAC,	\
				.block_size = 64,			\
				.key_size = {				\
					.min = 64,			\
					.max = 64,			\
					.increment = 0			\
				},					\
				.digest_size = {			\
					.min = 28,			\
					.max = 28,			\
					.increment = 0			\
				},					\
				.aad_size = { 0 }			\
			}, }						\
		}, }							\
	},								\
	{	/* SHA256 HMAC */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_SHA256_HMAC,	\
				.block_size = 64,			\
				.key_size = {				\
					.min = 64,			\
					.max = 64,			\
					.increment = 0			\
				},					\
				.digest_size = {			\
					.min = 32,			\
					.max = 32,			\
					.increment = 0			\
				},					\
				.aad_size = { 0 }			\
			}, }						\
		}, }							\
	},								\
	{	/* SHA384 HMAC */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_SHA384_HMAC,	\
				.block_size = 64,			\
				.key_size = {				\
					.min = 128,			\
					.max = 128,			\
					.increment = 0			\
				},					\
				.digest_size = {			\
					.min = 48,			\
					.max = 48,			\
					.increment = 0			\
					},				\
				.aad_size = { 0 }			\
			}, }						\
		}, }							\
	},								\
	{	/* SHA512 HMAC */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_SHA512_HMAC,	\
				.block_size = 128,			\
				.key_size = {				\
					.min = 128,			\
					.max = 128,			\
					.increment = 0			\
				},					\
				.digest_size = {			\
					.min = 64,			\
					.max = 64,			\
					.increment = 0			\
				},					\
				.aad_size = { 0 }			\
			}, }						\
		}, }							\
	},								\
	{	/* MD5 HMAC */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_MD5_HMAC,	\
				.block_size = 64,			\
				.key_size = {				\
					.min = 8,			\
					.max = 64,			\
					.increment = 8			\
				},					\
				.digest_size = {			\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.aad_size = { 0 }			\
			}, }						\
		}, }							\
	},								\
	{	/* AES XCBC MAC */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_AES_XCBC_MAC,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.digest_size = {			\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.aad_size = { 0 }			\
			}, }						\
		}, }							\
	},								\
	{	/* AES GCM (AUTH) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_AES_GCM,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 32,			\
					.increment = 8			\
				},					\
				.digest_size = {			\
					.min = 8,			\
					.max = 16,			\
					.increment = 4			\
				},					\
				.aad_size = {				\
					.min = 0,			\
					.max = 240,			\
					.increment = 1			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* AES GMAC (AUTH) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_AES_GMAC,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 32,			\
					.increment = 8			\
				},					\
				.digest_size = {			\
					.min = 8,			\
					.max = 16,			\
					.increment = 4			\
				},					\
				.aad_size = {				\
					.min = 1,			\
					.max = 65535,			\
					.increment = 1			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* SNOW 3G (UIA2) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_SNOW3G_UIA2,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.digest_size = {			\
					.min = 4,			\
					.max = 4,			\
					.increment = 0			\
				},					\
				.aad_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* AES GCM (CIPHER) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_AES_GCM,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 32,			\
					.increment = 8			\
				},					\
				.iv_size = {				\
					.min = 12,			\
					.max = 12,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* AES CBC */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_AES_CBC,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 32,			\
					.increment = 8			\
				},					\
				.iv_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* AES DOCSIS BPI */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_AES_DOCSISBPI,\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.iv_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* SNOW 3G (UEA2) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_SNOW3G_UEA2,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.iv_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* AES CTR */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_AES_CTR,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 32,			\
					.increment = 8			\
				},					\
				.iv_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* NULL (AUTH) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_NULL,		\
				.block_size = 1,			\
				.key_size = {				\
					.min = 0,			\
					.max = 0,			\
					.increment = 0			\
				},					\
				.digest_size = {			\
					.min = 0,			\
					.max = 0,			\
					.increment = 0			\
				},					\
				.aad_size = { 0 }			\
			}, },						\
		}, },							\
	},								\
	{	/* NULL (CIPHER) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_NULL,		\
				.block_size = 1,			\
				.key_size = {				\
					.min = 0,			\
					.max = 0,			\
					.increment = 0			\
				},					\
				.iv_size = {				\
					.min = 0,			\
					.max = 0,			\
					.increment = 0			\
				}					\
			}, },						\
		}, }							\
	},								\
	{       /* KASUMI (F8) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_KASUMI_F8,	\
				.block_size = 8,			\
				.key_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.iv_size = {				\
					.min = 8,			\
					.max = 8,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{       /* KASUMI (F9) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_KASUMI_F9,	\
				.block_size = 8,			\
				.key_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.digest_size = {			\
					.min = 4,			\
					.max = 4,			\
					.increment = 0			\
				},					\
				.aad_size = {				\
					.min = 8,			\
					.max = 8,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* 3DES CBC */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_3DES_CBC,	\
				.block_size = 8,			\
				.key_size = {				\
					.min = 16,			\
					.max = 24,			\
					.increment = 8			\
				},					\
				.iv_size = {				\
					.min = 8,			\
					.max = 8,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* 3DES CTR */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_3DES_CTR,	\
				.block_size = 8,			\
				.key_size = {				\
					.min = 16,			\
					.max = 24,			\
					.increment = 8			\
				},					\
				.iv_size = {				\
					.min = 8,			\
					.max = 8,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* DES CBC */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_DES_CBC,	\
				.block_size = 8,			\
				.key_size = {				\
					.min = 8,			\
					.max = 8,			\
					.increment = 0			\
				},					\
				.iv_size = {				\
					.min = 8,			\
					.max = 8,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* DES DOCSISBPI */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_DES_DOCSISBPI,\
				.block_size = 8,			\
				.key_size = {				\
					.min = 8,			\
					.max = 8,			\
					.increment = 0			\
				},					\
				.iv_size = {				\
					.min = 8,			\
					.max = 8,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	}

#define QAT_EXTRA_CPM17_SYM_CAPABILITIES				\
	{	/* ZUC (EEA3) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_ZUC_EEA3,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.iv_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	},								\
	{	/* ZUC (EIA3) */					\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_ZUC_EIA3,	\
				.block_size = 16,			\
				.key_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				},					\
				.digest_size = {			\
					.min = 4,			\
					.max = 4,			\
					.increment = 0			\
				},					\
				.aad_size = {				\
					.min = 16,			\
					.max = 16,			\
					.increment = 0			\
				}					\
			}, }						\
		}, }							\
	}

#endif /* _QAT_CRYPTO_CAPABILITIES_H_ */
