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
#include "otx2_cryptodev_capabilities.h"
#include "otx2_cryptodev_mbox.h"
#include "otx2_cryptodev_ops.h"
#include "otx2_cryptodev_sec.h"
#include "otx2_dev.h"

/* CPT common headers */
#include "cpt_common.h"
#include "cpt_pmd_logs.h"

uint8_t otx2_cryptodev_driver_id;

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
	struct otx2_dev *otx2_dev;
	struct otx2_cpt_vf *vf;
	uint16_t nb_queues;
	int ret;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dev = rte_cryptodev_pmd_create(name, &pci_dev->device, &init_params);
	if (dev == NULL) {
		ret = -ENODEV;
		goto exit;
	}

	dev->dev_ops = &otx2_cpt_ops;

	dev->driver_id = otx2_cryptodev_driver_id;

	/* Get private data space allocated */
	vf = dev->data->dev_private;

	otx2_dev = &vf->otx2_dev;

	/* Initialize the base otx2_dev object */
	ret = otx2_dev_init(pci_dev, otx2_dev);
	if (ret) {
		CPT_LOG_ERR("Could not initialize otx2_dev");
		goto pmd_destroy;
	}

	/* Get number of queues available on the device */
	ret = otx2_cpt_available_queues_get(dev, &nb_queues);
	if (ret) {
		CPT_LOG_ERR("Could not determine the number of queues available");
		goto otx2_dev_fini;
	}

	/* Don't exceed the limits set per VF */
	nb_queues = RTE_MIN(nb_queues, OTX2_CPT_MAX_QUEUES_PER_VF);

	if (nb_queues == 0) {
		CPT_LOG_ERR("No free queues available on the device");
		goto otx2_dev_fini;
	}

	vf->max_queues = nb_queues;

	CPT_LOG_INFO("Max queues supported by device: %d", vf->max_queues);

	ret = otx2_cpt_hardware_caps_get(dev, vf->hw_caps);
	if (ret) {
		CPT_LOG_ERR("Could not determine hardware capabilities");
		goto otx2_dev_fini;
	}

	otx2_crypto_capabilities_init(vf->hw_caps);
	otx2_crypto_sec_capabilities_init(vf->hw_caps);

	/* Create security ctx */
	ret = otx2_crypto_sec_ctx_create(dev);
	if (ret)
		goto otx2_dev_fini;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			     RTE_CRYPTODEV_FF_HW_ACCELERATED |
			     RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			     RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			     RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			     RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			     RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO |
			     RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT |
			     RTE_CRYPTODEV_FF_SYM_SESSIONLESS |
			     RTE_CRYPTODEV_FF_SECURITY;

	return 0;

otx2_dev_fini:
	otx2_dev_fini(pci_dev, otx2_dev);
pmd_destroy:
	rte_cryptodev_pmd_destroy(dev);
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

	/* Destroy security ctx */
	otx2_crypto_sec_ctx_destroy(dev);

	return rte_cryptodev_pmd_destroy(dev);
}

static struct rte_pci_driver otx2_cryptodev_pmd = {
	.id_table = pci_id_cpt_table,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = otx2_cpt_pci_probe,
	.remove = otx2_cpt_pci_remove,
};

static struct cryptodev_driver otx2_cryptodev_drv;

RTE_PMD_REGISTER_PCI(CRYPTODEV_NAME_OCTEONTX2_PMD, otx2_cryptodev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(CRYPTODEV_NAME_OCTEONTX2_PMD, pci_id_cpt_table);
RTE_PMD_REGISTER_KMOD_DEP(CRYPTODEV_NAME_OCTEONTX2_PMD, "vfio-pci");
RTE_PMD_REGISTER_CRYPTO_DRIVER(otx2_cryptodev_drv, otx2_cryptodev_pmd.driver,
		otx2_cryptodev_driver_id);
RTE_LOG_REGISTER(otx2_cpt_logtype, pmd.crypto.octeontx2, NOTICE);
