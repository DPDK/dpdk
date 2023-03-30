/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <dev_driver.h>
#include <rte_eal.h>
#include <rte_malloc.h>

#include "rte_mldev_pmd.h"

struct rte_ml_dev *
rte_ml_dev_pmd_create(const char *name, struct rte_device *device,
		      struct rte_ml_dev_pmd_init_params *params)
{
	struct rte_ml_dev *dev;

	RTE_MLDEV_LOG(INFO, "ML device initialisation - name: %s, socket_id: %u", name,
		      params->socket_id);

	/* Allocate device structure */
	dev = rte_ml_dev_pmd_allocate(name, params->socket_id);
	if (dev == NULL) {
		RTE_MLDEV_LOG(ERR, "Failed to allocate ML device for %s", name);
		return NULL;
	}

	/* Allocate private device structure */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		dev->data->dev_private =
			rte_zmalloc_socket("ml_dev_private", params->private_data_size,
					   RTE_CACHE_LINE_SIZE, params->socket_id);

		if (dev->data->dev_private == NULL) {
			RTE_MLDEV_LOG(ERR, "Cannot allocate memory for mldev %s private data",
				      name);
			rte_ml_dev_pmd_release(dev);
			return NULL;
		}
	}
	dev->device = device;

	return dev;
}

int
rte_ml_dev_pmd_destroy(struct rte_ml_dev *dev)
{
	int ret;

	RTE_MLDEV_LOG(INFO, "Releasing ML device - name: %s", dev->device->name);
	ret = rte_ml_dev_pmd_release(dev);
	if (ret)
		return ret;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(dev->data->dev_private);

	dev->data = NULL;
	dev->device = NULL;

	return 0;
}
