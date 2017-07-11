/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016 NXP.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Freescale Semiconductor, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <rte_ethdev.h>

#include <fslmc_logs.h>
#include <fslmc_vfio.h>
#include <mc/fsl_dpbp.h>
#include "portal/dpaa2_hw_pvt.h"
#include "portal/dpaa2_hw_dpio.h"

TAILQ_HEAD(dpbp_dev_list, dpaa2_dpbp_dev);
static struct dpbp_dev_list dpbp_dev_list
	= TAILQ_HEAD_INITIALIZER(dpbp_dev_list); /*!< DPBP device list */

static int
dpaa2_create_dpbp_device(struct fslmc_vfio_device *vdev __rte_unused,
			 struct vfio_device_info *obj_info __rte_unused,
			 int dpbp_id)
{
	struct dpaa2_dpbp_dev *dpbp_node;
	int ret;

	/* Allocate DPAA2 dpbp handle */
	dpbp_node = rte_malloc(NULL, sizeof(struct dpaa2_dpbp_dev), 0);
	if (!dpbp_node) {
		PMD_INIT_LOG(ERR, "Memory allocation failed for DPBP Device");
		return -1;
	}

	/* Open the dpbp object */
	dpbp_node->dpbp.regs = rte_mcp_ptr_list[MC_PORTAL_INDEX];
	ret = dpbp_open(&dpbp_node->dpbp,
			CMD_PRI_LOW, dpbp_id, &dpbp_node->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Resource alloc failure with err code: %d",
			     ret);
		rte_free(dpbp_node);
		return -1;
	}

	/* Clean the device first */
	ret = dpbp_reset(&dpbp_node->dpbp, CMD_PRI_LOW, dpbp_node->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure cleaning dpbp device with"
					" error code %d\n", ret);
		dpbp_close(&dpbp_node->dpbp, CMD_PRI_LOW, dpbp_node->token);
		rte_free(dpbp_node);
		return -1;
	}

	dpbp_node->dpbp_id = dpbp_id;
	rte_atomic16_init(&dpbp_node->in_use);

	TAILQ_INSERT_TAIL(&dpbp_dev_list, dpbp_node, next);

	PMD_INIT_LOG(DEBUG, "DPAA2: Added [dpbp.%d]", dpbp_id);

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

static struct rte_dpaa2_object rte_dpaa2_dpbp_obj = {
	.object_id = DPAA2_MC_DPBP_DEVID,
	.create = dpaa2_create_dpbp_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dpbp, rte_dpaa2_dpbp_obj);
