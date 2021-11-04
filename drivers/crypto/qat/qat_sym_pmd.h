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

#include "qat_sym_capabilities.h"
#include "qat_crypto.h"
#include "qat_device.h"

/** Intel(R) QAT Symmetric Crypto PMD driver name */
#define CRYPTODEV_NAME_QAT_SYM_PMD	crypto_qat

/* Internal capabilities */
#define QAT_SYM_CAP_MIXED_CRYPTO	(1 << 0)
#define QAT_SYM_CAP_VALID		(1 << 31)

extern uint8_t qat_sym_driver_id;

int
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev,
		struct qat_dev_cmd_param *qat_dev_cmd_param);

int
qat_sym_dev_destroy(struct qat_pci_device *qat_pci_dev);

void
qat_sym_init_op_cookie(void *op_cookie);

#endif
#endif /* _QAT_SYM_PMD_H_ */
