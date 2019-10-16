/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_mempool.h>
#include <rte_pci.h>

#include "otx2_common.h"
#include "otx2_cryptodev.h"
#include "otx2_cryptodev_ops.h"

/* CPT common headers */
#include "cpt_common.h"
#include "cpt_pmd_logs.h"

int otx2_cpt_logtype;

static struct rte_pci_id pci_id_cpt_table[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_CPT_VF)
	},
	/* sentinel */
	{
		.device_id = 0
	},
};

static int
otx2_cpt_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		   struct rte_pci_device *pci_dev)
{
	struct rte_cryptodev_pmd_init_params init_params = {
		.name = "",
		.socket_id = rte_socket_id(),
		.private_data_size = sizeof(struct otx2_cpt_vf)
	};
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *dev;
	int ret;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dev = rte_cryptodev_pmd_create(name, &pci_dev->device, &init_params);
	if (dev == NULL) {
		ret = -ENODEV;
		goto exit;
	}

	dev->dev_ops = &otx2_cpt_ops;

	dev->driver_id = otx2_cryptodev_driver_id;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			     RTE_CRYPTODEV_FF_HW_ACCELERATED;

	return 0;

exit:
	CPT_LOG_ERR("Could not create device (vendor_id: 0x%x device_id: 0x%x)",
		    pci_dev->id.vendor_id, pci_dev->id.device_id);
	return ret;
}

static int
otx2_cpt_pci_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	struct rte_cryptodev *dev;

	if (pci_dev == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dev = rte_cryptodev_pmd_get_named_dev(name);
	if (dev == NULL)
		return -ENODEV;

	return rte_cryptodev_pmd_destroy(dev);
}

static struct rte_pci_driver otx2_cryptodev_pmd = {
	.id_table = pci_id_cpt_table,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = otx2_cpt_pci_probe,
	.remove = otx2_cpt_pci_remove,
};

static struct cryptodev_driver otx2_cryptodev_drv;

RTE_INIT(otx2_cpt_init_log);
RTE_PMD_REGISTER_PCI(CRYPTODEV_NAME_OCTEONTX2_PMD, otx2_cryptodev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(CRYPTODEV_NAME_OCTEONTX2_PMD, pci_id_cpt_table);
RTE_PMD_REGISTER_CRYPTO_DRIVER(otx2_cryptodev_drv, otx2_cryptodev_pmd.driver,
		otx2_cryptodev_driver_id);

RTE_INIT(otx2_cpt_init_log)
{
	/* Bus level logs */
	otx2_cpt_logtype = rte_log_register("pmd.crypto.octeontx2");
	if (otx2_cpt_logtype >= 0)
		rte_log_set_level(otx2_cpt_logtype, RTE_LOG_NOTICE);
}
