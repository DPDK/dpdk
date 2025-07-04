/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "pmd_chacha_poly_priv.h"
#include "pmd_aesni_mb_priv.h"

struct rte_cryptodev_ops chacha20_poly1305_pmd_ops = {
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

struct rte_cryptodev_ops *rte_chacha20_poly1305_pmd_ops =
						&chacha20_poly1305_pmd_ops;

static int
chacha20_poly1305_probe(struct rte_vdev_device *vdev)
{
	return ipsec_mb_create(vdev, IPSEC_MB_PMD_TYPE_CHACHA20_POLY1305);
}

static struct rte_vdev_driver cryptodev_chacha20_poly1305_pmd_drv = {
	.probe = chacha20_poly1305_probe,
	.remove = ipsec_mb_remove
};

static struct cryptodev_driver chacha20_poly1305_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_CHACHA20_POLY1305_PMD,
					cryptodev_chacha20_poly1305_pmd_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_CHACHA20_POLY1305_PMD,
					cryptodev_chacha20_poly1305_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_CHACHA20_POLY1305_PMD,
			       "max_nb_queue_pairs=<int> socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(chacha20_poly1305_crypto_drv,
				cryptodev_chacha20_poly1305_pmd_drv.driver,
				pmd_driver_id_chacha20_poly1305);

/* Constructor function to register chacha20_poly1305 PMD */
RTE_INIT(ipsec_mb_register_chacha20_poly1305)
{
	struct ipsec_mb_internals *chacha_poly_data
		= &ipsec_mb_pmds[IPSEC_MB_PMD_TYPE_CHACHA20_POLY1305];

	chacha_poly_data->caps = chacha20_poly1305_capabilities;
	chacha_poly_data->dequeue_burst = aesni_mb_dequeue_burst;
	chacha_poly_data->feature_flags =
		RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
		RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
		RTE_CRYPTODEV_FF_IN_PLACE_SGL |
		RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
		RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
		RTE_CRYPTODEV_FF_SYM_CPU_CRYPTO |
		RTE_CRYPTODEV_FF_SYM_SESSIONLESS;
	chacha_poly_data->internals_priv_size = 0;
	chacha_poly_data->ops = &chacha20_poly1305_pmd_ops;
	chacha_poly_data->qp_priv_size = sizeof(struct aesni_mb_qp_data);
	chacha_poly_data->session_configure = aesni_mb_session_configure;
	chacha_poly_data->session_priv_size = sizeof(struct aesni_mb_session);
}
