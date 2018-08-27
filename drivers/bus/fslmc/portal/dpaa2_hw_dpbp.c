/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016 NXP
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <rte_ethdev_driver.h>
#include <rte_mbuf_pool_ops.h>

#include <fslmc_logs.h>
#include <rte_fslmc.h>
#include <mc/fsl_dpbp.h>
#include "portal/dpaa2_hw_pvt.h"
#include "portal/dpaa2_hw_dpio.h"

/* List of all the memseg information locally maintained in dpaa2 driver. This
 * is to optimize the PA_to_VA searches until a better mechanism (algo) is
 * available.
 */
struct dpaa2_memseg_list rte_dpaa2_memsegs
	= TAILQ_HEAD_INITIALIZER(rte_dpaa2_memsegs);

TAILQ_HEAD(dpbp_dev_list, dpaa2_dpbp_dev);
static struct dpbp_dev_list dpbp_dev_list
	= TAILQ_HEAD_INITIALIZER(dpbp_dev_list); /*!< DPBP device list */

static int
dpaa2_create_dpbp_device(int vdev_fd __rte_unused,
			 struct vfio_device_info *obj_info __rte_unused,
			 int dpbp_id)
{
	struct dpaa2_dpbp_dev *dpbp_node;
	int ret;
	static int register_once;

	/* Allocate DPAA2 dpbp handle */
	dpbp_node = rte_malloc(NULL, sizeof(struct dpaa2_dpbp_dev), 0);
	if (!dpbp_node) {
		DPAA2_BUS_ERR("Memory allocation failed for DPBP Device");
		return -1;
	}

	/* Open the dpbp object */
	dpbp_node->dpbp.regs = rte_mcp_ptr_list[MC_PORTAL_INDEX];
	ret = dpbp_open(&dpbp_node->dpbp,
			CMD_PRI_LOW, dpbp_id, &dpbp_node->token);
	if (ret) {
		DPAA2_BUS_ERR("Unable to open buffer pool object: err(%d)",
			      ret);
		rte_free(dpbp_node);
		return -1;
	}

	/* Clean the device first */
	ret = dpbp_reset(&dpbp_node->dpbp, CMD_PRI_LOW, dpbp_node->token);
	if (ret) {
		DPAA2_BUS_ERR("Unable to reset buffer pool device. err(%d)",
			      ret);
		dpbp_close(&dpbp_node->dpbp, CMD_PRI_LOW, dpbp_node->token);
		rte_free(dpbp_node);
		return -1;
	}

	dpbp_node->dpbp_id = dpbp_id;
	rte_atomic16_init(&dpbp_node->in_use);

	TAILQ_INSERT_TAIL(&dpbp_dev_list, dpbp_node, next);

	if (!register_once) {
		rte_mbuf_set_platform_mempool_ops(DPAA2_MEMPOOL_OPS_NAME);
		register_once = 1;
	}

	return 0;
}

struct dpaa2_dpbp_dev *dpaa2_alloc_dpbp_dev(void)
{
	struct dpaa2_dpbp_dev *dpbp_dev = NULL;

	/* Get DPBP dev handle from list using index */
	TAILQ_FOREACH(dpbp_dev, &dpbp_dev_list, next) {
		if (dpbp_dev && rte_atomic16_test_and_set(&dpbp_dev->in_use))
			break;
	}

	return dpbp_dev;
}

void dpaa2_free_dpbp_dev(struct dpaa2_dpbp_dev *dpbp)
{
	struct dpaa2_dpbp_dev *dpbp_dev = NULL;

	/* Match DPBP handle and mark it free */
	TAILQ_FOREACH(dpbp_dev, &dpbp_dev_list, next) {
		if (dpbp_dev == dpbp) {
			rte_atomic16_dec(&dpbp_dev->in_use);
			return;
		}
	}
}

int dpaa2_dpbp_supported(void)
{
	if (TAILQ_EMPTY(&dpbp_dev_list))
		return -1;
	return 0;
}

static struct rte_dpaa2_object rte_dpaa2_dpbp_obj = {
	.dev_type = DPAA2_BPOOL,
	.create = dpaa2_create_dpbp_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dpbp, rte_dpaa2_dpbp_obj);
