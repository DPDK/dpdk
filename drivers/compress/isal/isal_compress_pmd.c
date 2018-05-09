/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_bus_vdev.h>
#include <rte_compressdev_pmd.h>

/** Remove compression device */
static int
compdev_isal_remove_dev(struct rte_vdev_device *vdev __rte_unused)
{
	return 0;
}

/** Initialise ISA-L compression device */
static int
compdev_isal_probe(struct rte_vdev_device *dev __rte_unused)
{
	return 0;
}

static struct rte_vdev_driver compdev_isal_pmd_drv = {
	.probe = compdev_isal_probe,
	.remove = compdev_isal_remove_dev,
};

RTE_PMD_REGISTER_VDEV(COMPDEV_NAME_ISAL_PMD, compdev_isal_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(COMPDEV_NAME_ISAL_PMD,
	"socket_id=<int>");
