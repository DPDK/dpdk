/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 NXP
 */

#include <rte_dpaa_bus.h>

static int
dpaa_qdma_probe(__rte_unused struct rte_dpaa_driver *dpaa_drv,
		__rte_unused struct rte_dpaa_device *dpaa_dev)
{
	return 0;
}

static int
dpaa_qdma_remove(__rte_unused struct rte_dpaa_device *dpaa_dev)
{
	return 0;
}

static struct rte_dpaa_driver rte_dpaa_qdma_pmd;

static struct rte_dpaa_driver rte_dpaa_qdma_pmd = {
	.drv_type = FSL_DPAA_QDMA,
	.probe = dpaa_qdma_probe,
	.remove = dpaa_qdma_remove,
};

RTE_PMD_REGISTER_DPAA(dpaa_qdma, rte_dpaa_qdma_pmd);
RTE_LOG_REGISTER_DEFAULT(dpaa_qdma_logtype, INFO);
