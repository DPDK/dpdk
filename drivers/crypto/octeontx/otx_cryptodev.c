/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <cryptodev_pmd.h>
#include <rte_log.h>
#include <rte_pci.h>

#include "otx_cryptodev.h"
#include "otx_cryptodev_ops.h"

#include "cpt_pmd_logs.h"

/* Device ID */
#define PCI_VENDOR_ID_CAVIUM		0x177d
#define CPT_81XX_PCI_VF_DEVICE_ID	0xa041

uint8_t otx_cryptodev_driver_id;

static struct rte_pci_id pci_id_cpt_table[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, CPT_81XX_PCI_VF_DEVICE_ID),
	},
	/* sentinel */
	{
		.device_id = 0
	},
};

static int
otx_cpt_pci_probe(struct rte_pci_driver *pci_drv,
			struct rte_pci_device *pci_dev)
{
	struct rte_cryptodev *cryptodev;
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	int retval;

	if (pci_drv == NULL)
		return -ENODEV;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	cryptodev = rte_cryptodev_pmd_allocate(name, rte_socket_id());
	if (cryptodev == NULL)
		return -ENOMEM;

	cryptodev->device = &pci_dev->device;
	cryptodev->device->driver = &pci_drv->driver;
	cryptodev->driver_id = otx_cryptodev_driver_id;

	/* init user callbacks */
	TAILQ_INIT(&(cryptodev->link_intr_cbs));

	/* Invoke PMD device initialization function */
	retval = otx_cpt_dev_create(cryptodev);
	if (retval == 0) {
		rte_cryptodev_pmd_probing_finish(cryptodev);
		return 0;
	}

	CPT_LOG_ERR("[DRV %s]: Failed to create device "
			"(vendor_id: 0x%x device_id: 0x%x",
			pci_drv->driver.name,
			(unsigned int) pci_dev->id.vendor_id,
			(unsigned int) pci_dev->id.device_id);

	cryptodev->attached = RTE_CRYPTODEV_DETACHED;

	return -ENXIO;
}

static int
otx_cpt_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_cryptodev *cryptodev;
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	void *dev_priv;

	if (pci_dev == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	if (pci_dev->driver == NULL)
		return -ENODEV;

	dev_priv = cryptodev->data->dev_private;

	/* free crypto device */
	rte_cryptodev_pmd_release_device(cryptodev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(dev_priv);

	cryptodev->device->driver = NULL;
	cryptodev->device = NULL;
	cryptodev->data = NULL;

	return 0;
}

static struct rte_pci_driver otx_cryptodev_pmd = {
	.id_table = pci_id_cpt_table,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = otx_cpt_pci_probe,
	.remove = otx_cpt_pci_remove,
};

static struct cryptodev_driver otx_cryptodev_drv;

RTE_PMD_REGISTER_PCI(CRYPTODEV_NAME_OCTEONTX_PMD, otx_cryptodev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(CRYPTODEV_NAME_OCTEONTX_PMD, pci_id_cpt_table);
RTE_PMD_REGISTER_KMOD_DEP(CRYPTODEV_NAME_OCTEONTX_PMD, "vfio-pci");
RTE_PMD_REGISTER_CRYPTO_DRIVER(otx_cryptodev_drv, otx_cryptodev_pmd.driver,
		otx_cryptodev_driver_id);
RTE_LOG_REGISTER_DEFAULT(otx_cpt_logtype, NOTICE);
