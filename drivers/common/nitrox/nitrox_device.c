/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <eal_export.h>
#include <rte_malloc.h>

#include "nitrox_device.h"
#include "nitrox_hal.h"

#define PCI_VENDOR_ID_CAVIUM	0x177d
#define NITROX_V_PCI_VF_DEV_ID	0x13

TAILQ_HEAD(ndev_list, nitrox_device);
static struct ndev_list ndev_list = TAILQ_HEAD_INITIALIZER(ndev_list);

static struct nitrox_device *
ndev_allocate(struct rte_pci_device *pdev)
{
	struct nitrox_device *ndev;

	ndev = rte_zmalloc_socket("nitrox device", sizeof(*ndev),
				   RTE_CACHE_LINE_SIZE,
				   pdev->device.numa_node);
	if (!ndev)
		return NULL;

	TAILQ_INSERT_TAIL(&ndev_list, ndev, next);
	return ndev;
}

static void
ndev_init(struct nitrox_device *ndev, struct rte_pci_device *pdev)
{
	enum nitrox_vf_mode vf_mode;

	ndev->pdev = pdev;
	ndev->bar_addr = pdev->mem_resource[0].addr;
	vf_mode = vf_get_vf_config_mode(ndev->bar_addr);
	ndev->nr_queues = vf_config_mode_to_nr_queues(vf_mode);
}

static struct nitrox_device *
find_ndev(struct rte_pci_device *pdev)
{
	struct nitrox_device *ndev;

	TAILQ_FOREACH(ndev, &ndev_list, next)
		if (ndev->pdev == pdev)
			return ndev;

	return NULL;
}

static void
ndev_release(struct nitrox_device *ndev)
{
	if (!ndev)
		return;

	TAILQ_REMOVE(&ndev_list, ndev, next);
	rte_free(ndev);
}

TAILQ_HEAD(ndrv_list, nitrox_driver);
static struct ndrv_list ndrv_list = TAILQ_HEAD_INITIALIZER(ndrv_list);

RTE_EXPORT_INTERNAL_SYMBOL(nitrox_register_driver)
void
nitrox_register_driver(struct nitrox_driver *ndrv)
{
	TAILQ_INSERT_TAIL(&ndrv_list, ndrv, next);
}

static int
nitrox_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pdev)
{
	struct nitrox_device *ndev;
	struct nitrox_driver *ndrv;
	int err = -1;

	/* Nitrox CSR space */
	if (!pdev->mem_resource[0].addr)
		return -EINVAL;

	ndev = ndev_allocate(pdev);
	if (!ndev)
		return -ENOMEM;

	ndev_init(ndev, pdev);
	TAILQ_FOREACH(ndrv, &ndrv_list, next) {
		err = ndrv->create(ndev);
		if (err)
			goto drv_err;
	}

	return 0;

drv_err:
	ndrv = TAILQ_PREV(ndrv, ndrv_list, next);
	while (ndrv != NULL) {
		ndrv->destroy(ndev);
		ndrv = TAILQ_PREV(ndrv, ndrv_list, next);
	}

	ndev_release(ndev);
	return err;
}

static int
nitrox_pci_remove(struct rte_pci_device *pdev)
{
	struct nitrox_device *ndev;
	struct nitrox_driver *ndrv;
	int err;

	ndev = find_ndev(pdev);
	if (!ndev)
		return -ENODEV;

	TAILQ_FOREACH(ndrv, &ndrv_list, next) {
		err = ndrv->destroy(ndev);
		if (err)
			return err;
	}

	ndev_release(ndev);
	return 0;
}

static struct rte_pci_id pci_id_nitrox_map[] = {
	{
		/* Nitrox 5 VF */
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, NITROX_V_PCI_VF_DEV_ID)
	},
	{.device_id = 0},
};

static struct rte_pci_driver nitrox_pmd = {
	.id_table       = pci_id_nitrox_map,
	.drv_flags      = RTE_PCI_DRV_NEED_MAPPING,
	.probe          = nitrox_pci_probe,
	.remove         = nitrox_pci_remove,
};

RTE_PMD_REGISTER_PCI(nitrox, nitrox_pmd);
RTE_PMD_REGISTER_PCI_TABLE(nitrox, pci_id_nitrox_map);
