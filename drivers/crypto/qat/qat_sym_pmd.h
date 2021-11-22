/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_SYM_PMD_H_
#define _QAT_SYM_PMD_H_

#ifdef BUILD_QAT_SYM

#include <rte_ether.h>
#include <rte_cryptodev.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security.h>
#endif

#include "qat_crypto.h"
#include "qat_device.h"

/** Intel(R) QAT Symmetric Crypto PMD name */
#define CRYPTODEV_NAME_QAT_SYM_PMD	crypto_qat

/* Internal capabilities */
#define QAT_SYM_CAP_MIXED_CRYPTO	(1 << 0)
#define QAT_SYM_CAP_VALID		(1 << 31)

/**
 * Macro to add a sym capability
 * helper function to add an sym capability
 * <n: name> <b: block size> <k: key size> <d: digest size>
 * <a: aad_size> <i: iv_size>
 **/
#define QAT_SYM_PLAIN_AUTH_CAP(n, b, d)					\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_##n,		\
				b, d					\
			}, }						\
		}, }							\
	}

#define QAT_SYM_AUTH_CAP(n, b, k, d, a, i)				\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,	\
			{.auth = {					\
				.algo = RTE_CRYPTO_AUTH_##n,		\
				b, k, d, a, i				\
			}, }						\
		}, }							\
	}

#define QAT_SYM_AEAD_CAP(n, b, k, d, a, i)				\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,	\
			{.aead = {					\
				.algo = RTE_CRYPTO_AEAD_##n,		\
				b, k, d, a, i				\
			}, }						\
		}, }							\
	}

#define QAT_SYM_CIPHER_CAP(n, b, k, i)					\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,			\
		{.sym = {						\
			.xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,	\
			{.cipher = {					\
				.algo = RTE_CRYPTO_CIPHER_##n,		\
				b, k, i					\
			}, }						\
		}, }							\
	}

extern uint8_t qat_sym_driver_id;

extern struct qat_crypto_gen_dev_ops qat_sym_gen_dev_ops[];

int
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param);

int
qat_sym_dev_destroy(struct qat_pci_device *qat_pci_dev);

void
qat_sym_init_op_cookie(void *op_cookie);

#endif
#endif /* _QAT_SYM_PMD_H_ */
