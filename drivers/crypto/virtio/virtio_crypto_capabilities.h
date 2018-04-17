/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#ifndef _VIRTIO_CRYPTO_CAPABILITIES_H_
#define _VIRTIO_CRYPTO_CAPABILITIES_H_

#define VIRTIO_SYM_CAPABILITIES					\
	{	/* SHA1 HMAC */						\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,	\
				.block_size = 64,			\
				.key_size = {				\
					.min = 1,			\
					.max = 64,			\
					.increment = 1			\
				},					\
				.digest_size = {			\
					.min = 1,			\
					.max = 20,			\
					.increment = 1			\
				},					\
				.iv_size = { 0 }			\
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
	}

#endif /* _VIRTIO_CRYPTO_CAPABILITIES_H_ */
