/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <rte_bus_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <roc_api.h>

struct cnxk_bphy_cgx_queue {
	unsigned int lmac;
	/* queue holds up to one response */
	void *rsp;
};

struct cnxk_bphy_cgx {
	struct roc_bphy_cgx *rcgx;
	struct cnxk_bphy_cgx_queue queues[MAX_LMACS_PER_CGX];
	unsigned int num_queues;
};

static void
cnxk_bphy_cgx_format_name(char *name, unsigned int len,
			  struct rte_pci_device *pci_dev)
{
	snprintf(name, len, "BPHY_CGX:%x:%02x.%x", pci_dev->addr.bus,
		 pci_dev->addr.devid, pci_dev->addr.function);
}

static int
cnxk_bphy_cgx_queue_def_conf(struct rte_rawdev *dev, uint16_t queue_id,
			     rte_rawdev_obj_t queue_conf,
			     size_t queue_conf_size)
{
	unsigned int *conf;

	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	if (queue_conf_size != sizeof(*conf))
		return -EINVAL;

	conf = (unsigned int *)queue_conf;
	*conf = 1;

	return 0;
}

static uint16_t
cnxk_bphy_cgx_queue_count(struct rte_rawdev *dev)
{
	struct cnxk_bphy_cgx *cgx = dev->dev_private;

	return cgx->num_queues;
}

static const struct rte_rawdev_ops cnxk_bphy_cgx_rawdev_ops = {
	.queue_def_conf = cnxk_bphy_cgx_queue_def_conf,
	.queue_count = cnxk_bphy_cgx_queue_count,
};

static void
cnxk_bphy_cgx_init_queues(struct cnxk_bphy_cgx *cgx)
{
	struct roc_bphy_cgx *rcgx = cgx->rcgx;
	unsigned int i;

	for (i = 0; i < RTE_DIM(cgx->queues); i++) {
		if (!(rcgx->lmac_bmap & BIT_ULL(i)))
			continue;

		cgx->queues[cgx->num_queues++].lmac = i;
	}
}

static void
cnxk_bphy_cgx_fini_queues(struct cnxk_bphy_cgx *cgx)
{
	unsigned int i;

	for (i = 0; i < cgx->num_queues; i++) {
		if (cgx->queues[i].rsp)
			rte_free(cgx->queues[i].rsp);
	}

	cgx->num_queues = 0;
}

static int
cnxk_bphy_cgx_rawdev_probe(struct rte_pci_driver *pci_drv,
			   struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;
	struct cnxk_bphy_cgx *cgx;
	struct roc_bphy_cgx *rcgx;
	int ret;

	RTE_SET_USED(pci_drv);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!pci_dev->mem_resource[0].addr)
		return -ENODEV;

	ret = roc_plt_init();
	if (ret)
		return ret;

	cnxk_bphy_cgx_format_name(name, sizeof(name), pci_dev);
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(*cgx), rte_socket_id());
	if (!rawdev)
		return -ENOMEM;

	rawdev->dev_ops = &cnxk_bphy_cgx_rawdev_ops;
	rawdev->device = &pci_dev->device;
	rawdev->driver_name = pci_dev->driver->driver.name;

	cgx = rawdev->dev_private;
	cgx->rcgx = rte_zmalloc(NULL, sizeof(*rcgx), 0);
	if (!cgx->rcgx) {
		ret = -ENOMEM;
		goto out_pmd_release;
	}

	rcgx = cgx->rcgx;
	rcgx->bar0_pa = pci_dev->mem_resource[0].phys_addr;
	rcgx->bar0_va = pci_dev->mem_resource[0].addr;
	ret = roc_bphy_cgx_dev_init(rcgx);
	if (ret)
		goto out_free;

	cnxk_bphy_cgx_init_queues(cgx);

	return 0;
out_free:
	rte_free(rcgx);
out_pmd_release:
	rte_rawdev_pmd_release(rawdev);

	return ret;
}

static int
cnxk_bphy_cgx_rawdev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;
	struct cnxk_bphy_cgx *cgx;

	cnxk_bphy_cgx_format_name(name, sizeof(name), pci_dev);
	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rawdev)
		return -ENODEV;

	cgx = rawdev->dev_private;
	cnxk_bphy_cgx_fini_queues(cgx);
	roc_bphy_cgx_dev_fini(cgx->rcgx);
	rte_free(cgx->rcgx);

	return rte_rawdev_pmd_release(rawdev);
}

static const struct rte_pci_id cnxk_bphy_cgx_map[] = {
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN9K_CGX)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN10K_RPM)},
	{} /* sentinel */
};

static struct rte_pci_driver bphy_cgx_rawdev_pmd = {
	.id_table = cnxk_bphy_cgx_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = cnxk_bphy_cgx_rawdev_probe,
	.remove = cnxk_bphy_cgx_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(cnxk_bphy_cgx_rawdev_pci_driver, bphy_cgx_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(cnxk_bphy_cgx_rawdev_pci_driver, cnxk_bphy_cgx_map);
RTE_PMD_REGISTER_KMOD_DEP(cnxk_bphy_cgx_rawdev_pci_driver, "vfio-pci");
