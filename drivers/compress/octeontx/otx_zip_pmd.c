/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include <string.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cpuflags.h>
#include <rte_malloc.h>

#include "otx_zip.h"

struct rte_compressdev_ops octtx_zip_pmd_ops = {

};

static int
zip_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	int ret = 0;
	char compressdev_name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	struct rte_compressdev *compressdev;
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id(),
	};

	ZIP_PMD_INFO("vendor_id=0x%x device_id=0x%x",
			(unsigned int)pci_dev->id.vendor_id,
			(unsigned int)pci_dev->id.device_id);

	rte_pci_device_name(&pci_dev->addr, compressdev_name,
			    sizeof(compressdev_name));

	compressdev = rte_compressdev_pmd_create(compressdev_name,
		&pci_dev->device, sizeof(struct zip_vf), &init_params);
	if (compressdev == NULL) {
		ZIP_PMD_ERR("driver %s: create failed", init_params.name);
		return -ENODEV;
	}

	/*
	 * create only if proc_type is primary.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/*  create vf dev with given pmd dev id */
		ret = zipvf_create(compressdev);
		if (ret < 0) {
			ZIP_PMD_ERR("Device creation failed");
			rte_compressdev_pmd_destroy(compressdev);
			return ret;
		}
	}

	compressdev->dev_ops = &octtx_zip_pmd_ops;
	/* register rx/tx burst functions for data path */
	compressdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;
	return ret;
}

static int
zip_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_compressdev *compressdev;
	char compressdev_name[RTE_COMPRESSDEV_NAME_MAX_LEN];

	if (pci_dev == NULL) {
		ZIP_PMD_ERR(" Invalid PCI Device\n");
		return -EINVAL;
	}
	rte_pci_device_name(&pci_dev->addr, compressdev_name,
			sizeof(compressdev_name));

	compressdev = rte_compressdev_pmd_get_named_dev(compressdev_name);
	if (compressdev == NULL)
		return -ENODEV;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		if (zipvf_destroy(compressdev) < 0)
			return -ENODEV;
	}
	return rte_compressdev_pmd_destroy(compressdev);
}

static struct rte_pci_id pci_id_octtx_zipvf_table[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			PCI_DEVICE_ID_OCTEONTX_ZIPVF),
	},
	{
		.device_id = 0
	},
};

/**
 * Structure that represents a PCI driver
 */
static struct rte_pci_driver octtx_zip_pmd = {
	.id_table    = pci_id_octtx_zipvf_table,
	.drv_flags   = RTE_PCI_DRV_NEED_MAPPING,
	.probe       = zip_pci_probe,
	.remove      = zip_pci_remove,
};

RTE_PMD_REGISTER_PCI(COMPRESSDEV_NAME_ZIP_PMD, octtx_zip_pmd);
RTE_PMD_REGISTER_PCI_TABLE(COMPRESSDEV_NAME_ZIP_PMD, pci_id_octtx_zipvf_table);

RTE_INIT(octtx_zip_init_log);

static void
octtx_zip_init_log(void)
{
	octtx_zip_logtype_driver = rte_log_register("pmd.compress.octeontx");
	if (octtx_zip_logtype_driver >= 0)
		rte_log_set_level(octtx_zip_logtype_driver, RTE_LOG_INFO);
}
