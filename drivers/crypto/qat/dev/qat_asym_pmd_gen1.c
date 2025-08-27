/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2022 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include "qat_asym.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"

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
	QAT_ASYM_CAP(ECDH,
			((1 << RTE_CRYPTO_ASYM_KE_PUB_KEY_GENERATE) |
			(1 << RTE_CRYPTO_ASYM_KE_SHARED_SECRET_COMPUTE) |
			(1 << RTE_CRYPTO_ASYM_KE_PUB_KEY_VERIFY)),
			64, 512, 64),
	QAT_ASYM_CAP(ECDSA,
			((1 << RTE_CRYPTO_ASYM_OP_SIGN) |
			(1 << RTE_CRYPTO_ASYM_OP_VERIFY)),
			64, 512, 64),
	QAT_ASYM_CAP(ECPM,
			((1 << RTE_CRYPTO_ASYM_OP_SIGN) |
			(1 << RTE_CRYPTO_ASYM_OP_VERIFY)),
			64, 512, 64),
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

int
qat_asym_crypto_cap_get_gen1(struct qat_cryptodev_private *internals,
			const char *capa_memz_name,
			const uint16_t __rte_unused slice_map)
{
	const uint32_t size = sizeof(qat_asym_crypto_caps_gen1);
	uint32_t i;

	internals->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (internals->capa_mz == NULL) {
		internals->capa_mz = rte_memzone_reserve(capa_memz_name,
				size, rte_socket_id(), 0);
		if (internals->capa_mz == NULL) {
			QAT_LOG(DEBUG,
				"Error allocating memzone for capabilities");
			return -1;
		}
	}

	struct rte_cryptodev_capabilities *addr =
			(struct rte_cryptodev_capabilities *)
				internals->capa_mz->addr;
	const struct rte_cryptodev_capabilities *capabilities =
		qat_asym_crypto_caps_gen1;
	const uint32_t capa_num =
		size / sizeof(struct rte_cryptodev_capabilities);
	uint32_t curr_capa = 0;

	for (i = 0; i < capa_num; i++) {
		memcpy(addr + curr_capa, capabilities + i,
			sizeof(struct rte_cryptodev_capabilities));
		curr_capa++;
	}
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	return 0;
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

int
qat_asym_crypto_set_session_gen1(void *cdev __rte_unused,
		void *session __rte_unused)
{
	return 0;
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
