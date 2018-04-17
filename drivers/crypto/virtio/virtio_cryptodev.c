/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_cryptodev_pmd.h>

#include "virtio_cryptodev.h"

uint8_t cryptodev_virtio_driver_id;

static int
crypto_virtio_pci_probe(
	struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev __rte_unused)
{
	return 0;
}

static int
crypto_virtio_pci_remove(
	struct rte_pci_device *pci_dev __rte_unused)
{
	return 0;
}

static struct rte_pci_driver rte_virtio_crypto_driver = {
	.probe = crypto_virtio_pci_probe,
	.remove = crypto_virtio_pci_remove
};

static struct cryptodev_driver virtio_crypto_drv;

RTE_PMD_REGISTER_PCI(CRYPTODEV_NAME_VIRTIO_PMD, rte_virtio_crypto_driver);
RTE_PMD_REGISTER_CRYPTO_DRIVER(virtio_crypto_drv,
	rte_virtio_crypto_driver.driver,
	cryptodev_virtio_driver_id);
