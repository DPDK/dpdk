/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <rte_bus_vdev.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

uint8_t ccp_cryptodev_driver_id;

/** Remove ccp pmd */
static int
cryptodev_ccp_remove(struct rte_vdev_device *dev __rte_unused)
{
	return 0;
}

/** Probe ccp pmd */
static int
cryptodev_ccp_probe(struct rte_vdev_device *vdev __rte_unused)
{
	return 0;
}

static struct rte_vdev_driver cryptodev_ccp_pmd_drv = {
	.probe = cryptodev_ccp_probe,
	.remove = cryptodev_ccp_remove
};

static struct cryptodev_driver ccp_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_CCP_PMD, cryptodev_ccp_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_CCP_PMD,
	"max_nb_queue_pairs=<int> max_nb_sessions=<int> socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(ccp_crypto_drv, cryptodev_ccp_pmd_drv.driver,
			       ccp_cryptodev_driver_id);
