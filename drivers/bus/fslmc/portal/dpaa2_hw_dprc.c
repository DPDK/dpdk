/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2021 NXP
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#include <rte_malloc.h>
#include <rte_dev.h>

#include <fslmc_logs.h>
#include <rte_fslmc.h>
#include <mc/fsl_dprc.h>
#include "portal/dpaa2_hw_pvt.h"

TAILQ_HEAD(dprc_dev_list, dpaa2_dprc_dev);
static struct dprc_dev_list dprc_dev_list
	= TAILQ_HEAD_INITIALIZER(dprc_dev_list); /*!< DPRC device list */

static int
rte_dpaa2_create_dprc_device(int vdev_fd __rte_unused,
			     struct vfio_device_info *obj_info __rte_unused,
			     int dprc_id)
{
	struct dpaa2_dprc_dev *dprc_node;
	struct dprc_endpoint endpoint1, endpoint2;
	struct rte_dpaa2_device *dev, *dev_tmp;
	int ret;

	/* Allocate DPAA2 dprc handle */
	dprc_node = rte_malloc(NULL, sizeof(struct dpaa2_dprc_dev), 0);
	if (!dprc_node) {
		DPAA2_BUS_ERR("Memory allocation failed for DPRC Device");
		return -ENOMEM;
	}

	/* Open the dprc object */
	dprc_node->dprc.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
	dprc_node->dprc_id = dprc_id;
	ret = dprc_open(&dprc_node->dprc,
			CMD_PRI_LOW, dprc_id, &dprc_node->token);
	if (ret) {
		DPAA2_BUS_ERR("Resource alloc failure with err code: %d", ret);
		rte_free(dprc_node);
		return ret;
	}

	RTE_TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next, dev_tmp) {
		if (dev->dev_type == DPAA2_ETH) {
			int link_state;

			memset(&endpoint1, 0, sizeof(struct dprc_endpoint));
			memset(&endpoint2, 0, sizeof(struct dprc_endpoint));
			strcpy(endpoint1.type, "dpni");
			endpoint1.id = dev->object_id;
			ret = dprc_get_connection(&dprc_node->dprc,
						CMD_PRI_LOW,
						dprc_node->token,
						&endpoint1, &endpoint2,
						&link_state);
			if (ret) {
				DPAA2_BUS_ERR("dpni.%d connection failed!",
					dev->object_id);
				dprc_close(&dprc_node->dprc, CMD_PRI_LOW,
					   dprc_node->token);
				rte_free(dprc_node);
				return ret;
			}

			if (!strcmp(endpoint2.type, "dpmac"))
				dev->ep_dev_type = DPAA2_MAC;
			else if (!strcmp(endpoint2.type, "dpni"))
				dev->ep_dev_type = DPAA2_ETH;
			else if (!strcmp(endpoint2.type, "dpdmux"))
				dev->ep_dev_type = DPAA2_MUX;
			else
				dev->ep_dev_type = DPAA2_UNKNOWN;

			dev->ep_object_id = endpoint2.id;
		} else {
			dev->ep_dev_type = DPAA2_UNKNOWN;
		}
		sprintf(dev->ep_name, "%s.%d", endpoint2.type, endpoint2.id);
	}

	TAILQ_INSERT_TAIL(&dprc_dev_list, dprc_node, next);

	return 0;
}

static struct rte_dpaa2_object rte_dpaa2_dprc_obj = {
	.dev_type = DPAA2_DPRC,
	.create = rte_dpaa2_create_dprc_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dprc, rte_dpaa2_dprc_obj);
