/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _QAT_ASYM_CAPABILITIES_H_
#define _QAT_ASYM_CAPABILITIES_H_

#define QAT_BASE_GEN1_ASYM_CAPABILITIES						\
	{	/* modexp */							\
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,				\
		{.asym = {							\
			.xform_capa = {						\
				.xform_type = RTE_CRYPTO_ASYM_XFORM_MODEX,	\
				.op_types = 0,					\
				{						\
				.modlen = {					\
				.min = 1,					\
				.max = 512,					\
				.increment = 1					\
				}, }						\
			}							\
		},								\
		}								\
	},									\
	{	/* modinv */							\
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,				\
		{.asym = {							\
			.xform_capa = {						\
				.xform_type = RTE_CRYPTO_ASYM_XFORM_MODINV,	\
				.op_types = 0,					\
				{						\
				.modlen = {					\
				.min = 1,					\
				.max = 512,					\
				.increment = 1					\
				}, }						\
			}							\
		},								\
		}								\
	},									\
	{	/* RSA */							\
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,				\
		{.asym = {							\
			.xform_capa = {						\
				.xform_type = RTE_CRYPTO_ASYM_XFORM_RSA,	\
				.op_types = ((1 << RTE_CRYPTO_ASYM_OP_SIGN) |	\
					(1 << RTE_CRYPTO_ASYM_OP_VERIFY) |	\
					(1 << RTE_CRYPTO_ASYM_OP_ENCRYPT) |	\
					(1 << RTE_CRYPTO_ASYM_OP_DECRYPT)),	\
				{						\
				.modlen = {					\
				/* min length is based on openssl rsa keygen */	\
				.min = 64,					\
				/* value 0 symbolizes no limit on max length */	\
				.max = 512,					\
				.increment = 64					\
				}, }						\
			}							\
		},								\
		}								\
	}									\

#endif /* _QAT_ASYM_CAPABILITIES_H_ */
