/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2021 Intel Corporation
 */

#ifndef _QAT_CRYPTO_PMD_GENS_H_
#define _QAT_CRYPTO_PMD_GENS_H_

#include <rte_cryptodev.h>
#include "qat_crypto.h"
#include "qat_sym_session.h"

extern struct rte_cryptodev_ops qat_sym_crypto_ops_gen1;
extern struct rte_cryptodev_ops qat_asym_crypto_ops_gen1;

/* -----------------GENx control path APIs ---------------- */
uint64_t
qat_sym_crypto_feature_flags_get_gen1(struct qat_pci_device *qat_dev);

void
qat_sym_session_set_ext_hash_flags_gen2(struct qat_sym_session *session,
		uint8_t hash_flag);

struct qat_capabilities_info
qat_asym_crypto_cap_get_gen1(struct qat_pci_device *qat_dev);

uint64_t
qat_asym_crypto_feature_flags_get_gen1(struct qat_pci_device *qat_dev);

#ifdef RTE_LIB_SECURITY
extern struct rte_security_ops security_qat_ops_gen1;

void *
qat_sym_create_security_gen1(void *cryptodev);
#endif

#endif
