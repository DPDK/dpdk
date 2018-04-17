/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#ifndef _VIRTIO_CRYPTO_CAPABILITIES_H_
#define _VIRTIO_CRYPTO_CAPABILITIES_H_

#define VIRTIO_SYM_CAPABILITIES					\
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
