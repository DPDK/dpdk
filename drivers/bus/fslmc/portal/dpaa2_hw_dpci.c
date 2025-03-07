/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017, 2020, 2023 NXP
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <eal_export.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <dev_driver.h>
#include <ethdev_driver.h>

#include <fslmc_logs.h>
#include <bus_fslmc_driver.h>
#include <mc/fsl_dpci.h>
#include "portal/dpaa2_hw_pvt.h"
#include "portal/dpaa2_hw_dpio.h"

TAILQ_HEAD(dpci_dev_list, dpaa2_dpci_dev);
static struct dpci_dev_list dpci_dev_list
	= TAILQ_HEAD_INITIALIZER(dpci_dev_list); /*!< DPCI device list */

static struct dpaa2_dpci_dev *get_dpci_from_id(uint32_t dpci_id)
{
	struct dpaa2_dpci_dev *dpci_dev = NULL;

	/* Get DPCI dev handle from list using index */
	TAILQ_FOREACH(dpci_dev, &dpci_dev_list, next) {
		if (dpci_dev->dpci_id == dpci_id)
			break;
	}

	return dpci_dev;
}

static int
rte_dpaa2_create_dpci_device(int vdev_fd __rte_unused,
	struct vfio_device_info *obj_info __rte_unused,
	struct rte_dpaa2_device *obj)
{
	struct dpaa2_dpci_dev *dpci_node;
	struct dpci_attr attr;
	struct dpci_rx_queue_cfg rx_queue_cfg;
	struct dpci_rx_queue_attr rx_attr;
	struct dpci_tx_queue_attr tx_attr;
	int ret, i, dpci_id = obj->object_id;

	/* Allocate DPAA2 dpci handle */
	dpci_node = rte_malloc(NULL, sizeof(struct dpaa2_dpci_dev), 0);
	if (!dpci_node) {
		DPAA2_BUS_ERR("Memory allocation failed for DPCI Device");
		return -ENOMEM;
	}

	/* Open the dpci object */
	dpci_node->dpci.regs = dpaa2_get_mcp_ptr(MC_PORTAL_INDEX);
	ret = dpci_open(&dpci_node->dpci,
			CMD_PRI_LOW, dpci_id, &dpci_node->token);
	if (ret) {
		DPAA2_BUS_ERR("Resource alloc failure with err code: %d", ret);
		goto err;
	}

	/* Get the device attributes */
	ret = dpci_get_attributes(&dpci_node->dpci,
				  CMD_PRI_LOW, dpci_node->token, &attr);
	if (ret != 0) {
		DPAA2_BUS_ERR("Reading device failed with err code: %d", ret);
		goto err;
	}

	for (i = 0; i < DPAA2_DPCI_MAX_QUEUES; i++) {
		struct dpaa2_queue *rxq;

		memset(&rx_queue_cfg, 0, sizeof(struct dpci_rx_queue_cfg));
		ret = dpci_set_rx_queue(&dpci_node->dpci,
					CMD_PRI_LOW,
					dpci_node->token,
					i, &rx_queue_cfg);
		if (ret) {
			DPAA2_BUS_ERR("Setting Rx queue failed with err code: %d",
				      ret);
			goto err;
		}

		/* Allocate DQ storage for the DPCI Rx queues */
		rxq = &dpci_node->rx_queue[i];
		ret = dpaa2_queue_storage_alloc(rxq, 1);
		if (ret)
			goto err;
	}

	/* Enable the device */
	ret = dpci_enable(&dpci_node->dpci,
			  CMD_PRI_LOW, dpci_node->token);
	if (ret != 0) {
		DPAA2_BUS_ERR("Enabling device failed with err code: %d", ret);
		goto err;
	}

	for (i = 0; i < DPAA2_DPCI_MAX_QUEUES; i++) {
		/* Get the Rx FQID's */
		ret = dpci_get_rx_queue(&dpci_node->dpci,
					CMD_PRI_LOW,
					dpci_node->token, i,
					&rx_attr);
		if (ret != 0) {
			DPAA2_BUS_ERR("Rx queue fetch failed with err code: %d",
				      ret);
			goto err;
		}
		dpci_node->rx_queue[i].fqid = rx_attr.fqid;

		ret = dpci_get_tx_queue(&dpci_node->dpci,
					CMD_PRI_LOW,
					dpci_node->token, i,
					&tx_attr);
		if (ret != 0) {
			DPAA2_BUS_ERR("Reading device failed with err code: %d",
				      ret);
			goto err;
		}
		dpci_node->tx_queue[i].fqid = tx_attr.fqid;
	}

	dpci_node->dpci_id = dpci_id;
	rte_atomic16_init(&dpci_node->in_use);

	TAILQ_INSERT_TAIL(&dpci_dev_list, dpci_node, next);

	return 0;

err:
	for (i = 0; i < DPAA2_DPCI_MAX_QUEUES; i++) {
		struct dpaa2_queue *rxq = &dpci_node->rx_queue[i];

		dpaa2_queue_storage_free(rxq, 1);
	}
	rte_free(dpci_node);

	return ret;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa2_alloc_dpci_dev)
struct dpaa2_dpci_dev *rte_dpaa2_alloc_dpci_dev(void)
{
	struct dpaa2_dpci_dev *dpci_dev = NULL;

	/* Get DPCI dev handle from list using index */
	TAILQ_FOREACH(dpci_dev, &dpci_dev_list, next) {
		if (dpci_dev && rte_atomic16_test_and_set(&dpci_dev->in_use))
			break;
	}

	return dpci_dev;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_dpaa2_free_dpci_dev)
void rte_dpaa2_free_dpci_dev(struct dpaa2_dpci_dev *dpci)
{
	struct dpaa2_dpci_dev *dpci_dev = NULL;

	/* Match DPCI handle and mark it free */
	TAILQ_FOREACH(dpci_dev, &dpci_dev_list, next) {
		if (dpci_dev == dpci) {
			rte_atomic16_dec(&dpci_dev->in_use);
			return;
		}
	}
}


static void
rte_dpaa2_close_dpci_device(int object_id)
{
	struct dpaa2_dpci_dev *dpci_dev = NULL;

	dpci_dev = get_dpci_from_id((uint32_t)object_id);

	if (dpci_dev) {
		rte_dpaa2_free_dpci_dev(dpci_dev);
		dpci_close(&dpci_dev->dpci, CMD_PRI_LOW, dpci_dev->token);
		TAILQ_REMOVE(&dpci_dev_list, dpci_dev, next);
		rte_free(dpci_dev);
	}
}

static struct rte_dpaa2_object rte_dpaa2_dpci_obj = {
	.dev_type = DPAA2_CI,
	.create = rte_dpaa2_create_dpci_device,
	.close = rte_dpaa2_close_dpci_device,
};

RTE_PMD_REGISTER_DPAA2_OBJECT(dpci, rte_dpaa2_dpci_obj);
