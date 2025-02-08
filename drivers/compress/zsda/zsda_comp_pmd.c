/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <rte_malloc.h>

#include "zsda_logs.h"
#include "zsda_qp_common.h"
#include "zsda_comp_pmd.h"

static struct rte_compressdev_ops compress_zsda_ops = {

	.dev_configure = NULL,
	.dev_start = NULL,
	.dev_stop = NULL,
	.dev_close = NULL,
	.dev_infos_get = NULL,

	.stats_get = NULL,
	.stats_reset = NULL,
	.queue_pair_setup = NULL,
	.queue_pair_release = NULL,

	.private_xform_create = NULL,
	.private_xform_free = NULL
};

/* An rte_driver is needed in the registration of the device with compressdev.
 * The actual zsda pci's rte_driver can't be used as its name represents
 * the whole pci device with all services. Think of this as a holder for a name
 * for the compression part of the pci device.
 */
static const char zsda_comp_drv_name[] = RTE_STR(COMPRESSDEV_NAME_ZSDA_PMD);
static const struct rte_driver compdev_zsda_driver = {
	.name = zsda_comp_drv_name, .alias = zsda_comp_drv_name};

int
zsda_comp_dev_create(struct zsda_pci_device *zsda_pci_dev)
{
	struct zsda_device_info *dev_info =
		&zsda_devs[zsda_pci_dev->zsda_dev_id];

	struct rte_compressdev_pmd_init_params init_params = {
		.name = "",
		.socket_id = (int)rte_socket_id(),
	};

	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	struct rte_compressdev *compressdev;
	struct zsda_comp_dev_private *comp_dev;

	snprintf(name, RTE_COMPRESSDEV_NAME_MAX_LEN, "%s_%s",
		 zsda_pci_dev->name, "comp");

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dev_info->comp_rte_dev.driver = &compdev_zsda_driver;
	dev_info->comp_rte_dev.numa_node = dev_info->pci_dev->device.numa_node;
	dev_info->comp_rte_dev.devargs = NULL;

	compressdev = rte_compressdev_pmd_create(
		name, &(dev_info->comp_rte_dev),
		sizeof(struct zsda_comp_dev_private), &init_params);

	if (compressdev == NULL)
		return -ENODEV;

	compressdev->dev_ops = &compress_zsda_ops;

	compressdev->enqueue_burst = NULL;
	compressdev->dequeue_burst = NULL;

	compressdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;

	comp_dev = compressdev->data->dev_private;
	comp_dev->zsda_pci_dev = zsda_pci_dev;
	comp_dev->compressdev = compressdev;

	zsda_pci_dev->comp_dev = comp_dev;

	return ZSDA_SUCCESS;
}

int
zsda_comp_dev_destroy(struct zsda_pci_device *zsda_pci_dev)
{
	struct zsda_comp_dev_private *comp_dev;

	if (zsda_pci_dev == NULL)
		return -ENODEV;

	comp_dev = zsda_pci_dev->comp_dev;
	if (comp_dev == NULL)
		return ZSDA_SUCCESS;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(zsda_pci_dev->comp_dev->capa_mz);

	rte_compressdev_pmd_destroy(comp_dev->compressdev);
	zsda_pci_dev->comp_dev = NULL;

	return ZSDA_SUCCESS;
}
