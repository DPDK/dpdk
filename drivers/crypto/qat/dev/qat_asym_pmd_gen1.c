/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2021 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include "qat_asym.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"
#include "qat_pke_functionality_arrays.h"

struct rte_cryptodev_ops qat_asym_crypto_ops_gen1 = {
	/* Device related operations */
	.dev_configure		= qat_cryptodev_config,
	.dev_start		= qat_cryptodev_start,
	.dev_stop		= qat_cryptodev_stop,
	.dev_close		= qat_cryptodev_close,
	.dev_infos_get		= qat_cryptodev_info_get,

	.stats_get		= qat_cryptodev_stats_get,
	.stats_reset		= qat_cryptodev_stats_reset,
	.queue_pair_setup	= qat_cryptodev_qp_setup,
	.queue_pair_release	= qat_cryptodev_qp_release,

	/* Crypto related operations */
	.asym_session_get_size	= qat_asym_session_get_private_size,
	.asym_session_configure	= qat_asym_session_configure,
	.asym_session_clear	= qat_asym_session_clear
};

static struct rte_cryptodev_capabilities qat_asym_crypto_caps_gen1[] = {
	QAT_ASYM_CAP(MODEX,
		0, 1, 512, 1),
	QAT_ASYM_CAP(MODINV,
		0, 1, 512, 1),
	QAT_ASYM_CAP(RSA,
			((1 << RTE_CRYPTO_ASYM_OP_SIGN) |
			(1 << RTE_CRYPTO_ASYM_OP_VERIFY) |
			(1 << RTE_CRYPTO_ASYM_OP_ENCRYPT) |
			(1 << RTE_CRYPTO_ASYM_OP_DECRYPT)),
			64, 512, 64),
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};


struct qat_capabilities_info
qat_asym_crypto_cap_get_gen1(struct qat_pci_device *qat_dev __rte_unused)
{
	struct qat_capabilities_info capa_info;
	capa_info.data = qat_asym_crypto_caps_gen1;
	capa_info.size = sizeof(qat_asym_crypto_caps_gen1);
	return capa_info;
}

uint64_t
qat_asym_crypto_feature_flags_get_gen1(
	struct qat_pci_device *qat_dev __rte_unused)
{
	uint64_t feature_flags = RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_ASYM_SESSIONLESS |
			RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_EXP |
			RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT;

	return feature_flags;
}

RTE_INIT(qat_asym_crypto_gen1_init)
{
	qat_asym_gen_dev_ops[QAT_GEN1].cryptodev_ops =
			&qat_asym_crypto_ops_gen1;
	qat_asym_gen_dev_ops[QAT_GEN1].get_capabilities =
			qat_asym_crypto_cap_get_gen1;
	qat_asym_gen_dev_ops[QAT_GEN1].get_feature_flags =
			qat_asym_crypto_feature_flags_get_gen1;
}
