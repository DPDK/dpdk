/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
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
 *     * Neither the name of NXP nor the names of its
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

#include <fslmc_vfio.h>
#include <mc/fsl_dpcon.h>
#include <portal/dpaa2_hw_pvt.h>
#include "dpaa2_eventdev.h"

TAILQ_HEAD(dpcon_dev_list, dpaa2_dpcon_dev);
static struct dpcon_dev_list dpcon_dev_list
	= TAILQ_HEAD_INITIALIZER(dpcon_dev_list); /*!< DPCON device list */

static int
rte_dpaa2_create_dpcon_device(struct fslmc_vfio_device *vdev __rte_unused,
			      struct vfio_device_info *obj_info __rte_unused,
			 int dpcon_id)
{
	struct dpaa2_dpcon_dev *dpcon_node;
	struct dpcon_attr attr;
	int ret;

	/* Allocate DPAA2 dpcon handle */
	dpcon_node = rte_malloc(NULL, sizeof(struct dpaa2_dpcon_dev), 0);
	if (!dpcon_node) {
		PMD_DRV_LOG(ERR, "Memory allocation failed for DPCON Device");
		return -1;
	}

	/* Open the dpcon object */
	dpcon_node->dpcon.regs = rte_mcp_ptr_list[MC_PORTAL_INDEX];
	ret = dpcon_open(&dpcon_node->dpcon,
			 CMD_PRI_LOW, dpcon_id, &dpcon_node->token);
	if (ret) {
		PMD_DRV_LOG(ERR, "Resource alloc failure with err code: %d",
			    ret);
		rte_free(dpcon_node);
		return -1;
	}

	/* Get the device attributes */
	ret = dpcon_get_attributes(&dpcon_node->dpcon,
				   CMD_PRI_LOW, dpcon_node->token, &attr);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Reading device failed with err code: %d",
			    ret);
		rte_free(dpcon_node);
		return -1;
	}

	/* Updating device specific private information*/
	dpcon_node->qbman_ch_id = attr.qbman_ch_id;
	dpcon_node->num_priorities = attr.num_priorities;
	dpcon_node->dpcon_id = dpcon_id;
	rte_atomic16_init(&dpcon_node->in_use);

	TAILQ_INSERT_TAIL(&dpcon_dev_list, dpcon_node, next);

	PMD_DRV_LOG(DEBUG, "DPAA2: Added [dpcon.%d]", dpcon_id);

	return 0;
}

struct dpaa2_dpcon_dev *rte_dpaa2_alloc_dpcon_dev(void)
{
	struct dpaa2_dpcon_dev *dpcon_dev = NULL;

	/* Get DPCON dev handle from list using index */
	TAILQ_FOREACH(dpcon_dev, &dpcon_dev_list, next) {
		if (dpcon_dev && rte_atomic16_test_and_set(&dpcon_dev->in_use))
			break;
	}

	return dpcon_dev;
}

void rte_dpaa2_free_dpcon_dev(struct dpaa2_dpcon_dev *dpcon)
{
	struct dpaa2_dpcon_dev *dpcon_dev = NULL;

	/* Match DPCON handle and mark it free */
	TAILQ_FOREACH(dpcon_dev, &dpcon_dev_list, next) {
		if (dpcon_dev == dpcon) {
			rte_atomic16_dec(&dpcon_dev->in_use);
			return;
		}
	}
}

static struct rte_dpaa2_object rte_dpaa2_dpcon_obj = {
	.object_id = DPAA2_MC_DPCON_DEVID,
	.create = rte_dpaa2_create_dpcon_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dpcon, rte_dpaa2_dpcon_obj);
