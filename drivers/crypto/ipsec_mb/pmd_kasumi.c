/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2021 Intel Corporation
 */

#include <bus_vdev_driver.h>
#include <rte_common.h>
#include <rte_cpuflags.h>
#include <rte_cryptodev.h>
#include <rte_hexdump.h>
#include <rte_malloc.h>

#include "pmd_kasumi_priv.h"
#include "pmd_aesni_mb_priv.h"

struct rte_cryptodev_ops kasumi_pmd_ops = {
	.dev_configure = ipsec_mb_config,
	.dev_start = ipsec_mb_start,
	.dev_stop = ipsec_mb_stop,
	.dev_close = ipsec_mb_close,

	.stats_get = ipsec_mb_stats_get,
	.stats_reset = ipsec_mb_stats_reset,

	.dev_infos_get = ipsec_mb_info_get,

	.queue_pair_setup = ipsec_mb_qp_setup,
	.queue_pair_release = ipsec_mb_qp_release,

	.sym_session_get_size = ipsec_mb_sym_session_get_size,
	.sym_session_configure = ipsec_mb_sym_session_configure,
	.sym_session_clear = ipsec_mb_sym_session_clear
};

struct rte_cryptodev_ops *rte_kasumi_pmd_ops = &kasumi_pmd_ops;

static int
kasumi_probe(struct rte_vdev_device *vdev)
{
	return ipsec_mb_create(vdev, IPSEC_MB_PMD_TYPE_KASUMI);
}

static struct rte_vdev_driver cryptodev_kasumi_pmd_drv = {
	.probe = kasumi_probe,
	.remove = ipsec_mb_remove
};

static struct cryptodev_driver kasumi_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_KASUMI_PMD, cryptodev_kasumi_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_KASUMI_PMD, cryptodev_kasumi_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_KASUMI_PMD,
			       "max_nb_queue_pairs=<int> socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(kasumi_crypto_drv,
				cryptodev_kasumi_pmd_drv.driver,
				pmd_driver_id_kasumi);

/* Constructor function to register kasumi PMD */
RTE_INIT(ipsec_mb_register_kasumi)
{
	struct ipsec_mb_internals *kasumi_data
	    = &ipsec_mb_pmds[IPSEC_MB_PMD_TYPE_KASUMI];

	kasumi_data->caps = kasumi_capabilities;
	kasumi_data->dequeue_burst = aesni_mb_dequeue_burst;
	kasumi_data->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO
				| RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING
				| RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA
				| RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT
				| RTE_CRYPTODEV_FF_SYM_SESSIONLESS
				| RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;
	kasumi_data->internals_priv_size = 0;
	kasumi_data->ops = &kasumi_pmd_ops;
	kasumi_data->qp_priv_size = sizeof(struct aesni_mb_qp_data);
	kasumi_data->session_configure = aesni_mb_session_configure;
	kasumi_data->session_priv_size = sizeof(struct aesni_mb_session);
}
