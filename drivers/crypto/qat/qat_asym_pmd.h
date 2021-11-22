/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */


#ifndef _QAT_ASYM_PMD_H_
#define _QAT_ASYM_PMD_H_

#include <rte_cryptodev.h>
#include "qat_crypto.h"
#include "qat_device.h"

/** Intel(R) QAT Asymmetric Crypto PMD name */
#define CRYPTODEV_NAME_QAT_ASYM_PMD	crypto_qat_asym


/**
 * Helper function to add an asym capability
 * <name> <op type> <modlen (min, max, increment)>
 **/
#define QAT_ASYM_CAP(n, o, l, r, i)					\
	{								\
		.op = RTE_CRYPTO_OP_TYPE_ASYMMETRIC,			\
		{.asym = {						\
			.xform_capa = {					\
				.xform_type = RTE_CRYPTO_ASYM_XFORM_##n,\
				.op_types = o,				\
				{					\
				.modlen = {				\
				.min = l,				\
				.max = r,				\
				.increment = i				\
				}, }					\
			}						\
		},							\
		}							\
	}

extern uint8_t qat_asym_driver_id;

extern struct qat_crypto_gen_dev_ops qat_asym_gen_dev_ops[];

void
qat_asym_init_op_cookie(void *op_cookie);

uint16_t
qat_asym_pmd_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
			      uint16_t nb_ops);

uint16_t
qat_asym_pmd_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
			      uint16_t nb_ops);

#endif /* _QAT_ASYM_PMD_H_ */
