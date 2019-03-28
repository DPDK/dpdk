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
	}									\

#endif /* _QAT_ASYM_CAPABILITIES_H_ */
