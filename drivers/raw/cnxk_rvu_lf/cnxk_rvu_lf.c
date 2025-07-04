/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <bus_pci_driver.h>
#include <eal_export.h>
#include <rte_common.h>
#include <dev_driver.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <roc_api.h>

#include "cnxk_rvu_lf.h"
#include "cnxk_rvu_lf_driver.h"

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_msg_id_range_set)
int
rte_pmd_rvu_lf_msg_id_range_set(uint8_t dev_id, uint16_t from, uint16_t to)
{
	struct rte_rawdev *rawdev = rte_rawdev_pmd_get_dev(dev_id);
	struct roc_rvu_lf *roc_rvu_lf;

	if (rawdev == NULL)
		return -EINVAL;

	roc_rvu_lf = (struct roc_rvu_lf *)rawdev->dev_private;

	return roc_rvu_lf_msg_id_range_set(roc_rvu_lf, from, to);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_msg_process)
int
rte_pmd_rvu_lf_msg_process(uint8_t dev_id, uint16_t vf, uint16_t msg_id,
			void *req, uint16_t req_len, void *rsp, uint16_t rsp_len)
{
	struct rte_rawdev *rawdev = rte_rawdev_pmd_get_dev(dev_id);
	struct roc_rvu_lf *roc_rvu_lf;

	if (rawdev == NULL)
		return -EINVAL;

	roc_rvu_lf = (struct roc_rvu_lf *)rawdev->dev_private;

	return roc_rvu_lf_msg_process(roc_rvu_lf, vf, msg_id, req, req_len, rsp, rsp_len);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_msg_handler_register)
int
rte_pmd_rvu_lf_msg_handler_register(uint8_t dev_id, rte_pmd_rvu_lf_msg_handler_cb_fn cb)
{
	struct rte_rawdev *rawdev = rte_rawdev_pmd_get_dev(dev_id);
	struct roc_rvu_lf *roc_rvu_lf;

	if (rawdev == NULL)
		return -EINVAL;

	roc_rvu_lf = (struct roc_rvu_lf *)rawdev->dev_private;

	return roc_rvu_lf_msg_handler_register(roc_rvu_lf, (roc_rvu_lf_msg_handler_cb_fn)cb);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_msg_handler_unregister)
int
rte_pmd_rvu_lf_msg_handler_unregister(uint8_t dev_id)
{
	struct rte_rawdev *rawdev = rte_rawdev_pmd_get_dev(dev_id);
	struct roc_rvu_lf *roc_rvu_lf;

	if (rawdev == NULL)
		return -EINVAL;

	roc_rvu_lf = (struct roc_rvu_lf *)rawdev->dev_private;

	return roc_rvu_lf_msg_handler_unregister(roc_rvu_lf);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_irq_register)
int
rte_pmd_rvu_lf_irq_register(uint8_t dev_id, unsigned int irq,
			    rte_pmd_rvu_lf_intr_callback_fn cb, void *data)
{
	struct rte_rawdev *rawdev = rte_rawdev_pmd_get_dev(dev_id);
	struct roc_rvu_lf *roc_rvu_lf;

	if (rawdev == NULL)
		return -EINVAL;

	roc_rvu_lf = (struct roc_rvu_lf *)rawdev->dev_private;

	return roc_rvu_lf_irq_register(roc_rvu_lf, irq, (roc_rvu_lf_intr_cb_fn)cb, data);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_irq_unregister)
int
rte_pmd_rvu_lf_irq_unregister(uint8_t dev_id, unsigned int irq,
			      rte_pmd_rvu_lf_intr_callback_fn cb, void *data)
{
	struct rte_rawdev *rawdev = rte_rawdev_pmd_get_dev(dev_id);
	struct roc_rvu_lf *roc_rvu_lf;

	if (rawdev == NULL)
		return -EINVAL;

	roc_rvu_lf = (struct roc_rvu_lf *)rawdev->dev_private;

	return roc_rvu_lf_irq_unregister(roc_rvu_lf, irq, (roc_rvu_lf_intr_cb_fn)cb, data);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_bar_get)
int
rte_pmd_rvu_lf_bar_get(uint8_t dev_id, uint8_t bar_num, size_t *va, size_t *mask)
{
	struct roc_rvu_lf *roc_rvu_lf;
	struct rte_rawdev *rawdev;

	rawdev = rte_rawdev_pmd_get_dev(dev_id);
	if (rawdev == NULL)
		return -EINVAL;

	roc_rvu_lf = (struct roc_rvu_lf *)rawdev->dev_private;

	if (bar_num > PCI_MAX_RESOURCE ||
			(roc_rvu_lf->pci_dev->mem_resource[bar_num].addr == NULL)) {
		*va = 0;
		*mask = 0;
		return -ENOTSUP;
	}
	*va = (size_t)(roc_rvu_lf->pci_dev->mem_resource[bar_num].addr);
	*mask = (size_t)(roc_rvu_lf->pci_dev->mem_resource[bar_num].len - 1);

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_npa_pf_func_get)
uint16_t
rte_pmd_rvu_lf_npa_pf_func_get(void)
{
	return roc_npa_pf_func_get();
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_sso_pf_func_get)
uint16_t
rte_pmd_rvu_lf_sso_pf_func_get(void)
{
	return roc_sso_pf_func_get();
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pmd_rvu_lf_pf_func_get)
uint16_t
rte_pmd_rvu_lf_pf_func_get(uint8_t dev_id)
{
	struct roc_rvu_lf *roc_rvu_lf;
	struct rte_rawdev *rawdev;

	rawdev = rte_rawdev_pmd_get_dev(dev_id);
	if (rawdev == NULL)
		return 0;

	roc_rvu_lf = (struct roc_rvu_lf *)rawdev->dev_private;

	return roc_rvu_lf_pf_func_get(roc_rvu_lf);
}

static const struct rte_rawdev_ops rvu_lf_rawdev_ops = {
	.dev_selftest = rvu_lf_rawdev_selftest,
};

static void
rvu_lf_rawdev_get_name(char *name, struct rte_pci_device *pci_dev)
{
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "RVU LF:%02x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);
}

static int
rvu_lf_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rvu_lf_rawdev;
	struct roc_rvu_lf *roc_rvu_lf;
	int ret;

	RTE_SET_USED(pci_drv);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!pci_dev->mem_resource[2].addr) {
		CNXK_RVU_LF_LOG(ERR, "BARs have invalid values: BAR0 %p, BAR2 %p",
			      pci_dev->mem_resource[2].addr, pci_dev->mem_resource[4].addr);
		return -ENODEV;
	}

	ret = roc_plt_init();
	if (ret)
		return ret;

	rvu_lf_rawdev_get_name(name, pci_dev);
	rvu_lf_rawdev = rte_rawdev_pmd_allocate(name, sizeof(*roc_rvu_lf),
					      rte_socket_id());
	if (rvu_lf_rawdev == NULL) {
		CNXK_RVU_LF_LOG(ERR, "Failed to allocate rawdev");
		return -ENOMEM;
	}

	rvu_lf_rawdev->dev_ops = &rvu_lf_rawdev_ops;
	rvu_lf_rawdev->device = &pci_dev->device;
	rvu_lf_rawdev->driver_name = pci_dev->driver->driver.name;

	roc_rvu_lf = (struct roc_rvu_lf *)rvu_lf_rawdev->dev_private;
	roc_rvu_lf->pci_dev = pci_dev;

	ret = roc_rvu_lf_dev_init(roc_rvu_lf);
	if (ret) {
		rte_rawdev_pmd_release(rvu_lf_rawdev);
		return ret;
	}

	return 0;
}

static int
rvu_lf_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct roc_rvu_lf *roc_rvu_lf;
	struct rte_rawdev *rawdev;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev == NULL) {
		CNXK_RVU_LF_LOG(ERR, "invalid pci_dev");
		return -EINVAL;
	}

	rvu_lf_rawdev_get_name(name, pci_dev);
	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (rawdev == NULL) {
		CNXK_RVU_LF_LOG(ERR, "invalid device name (%s)", name);
		return -EINVAL;
	}

	roc_rvu_lf = (struct roc_rvu_lf *)rawdev->dev_private;
	roc_rvu_lf_dev_fini(roc_rvu_lf);

	return rte_rawdev_pmd_release(rawdev);
}

static const struct rte_pci_id pci_rvu_lf_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF20KA, PCI_DEVID_CNXK_RVU_BPHY_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF20KA, PCI_DEVID_CNXK_RVU_BPHY_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cnxk_rvu_lf_rawdev_pmd = {
	.id_table = pci_rvu_lf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = rvu_lf_probe,
	.remove = rvu_lf_remove,
};

RTE_PMD_REGISTER_PCI(rvu_lf_rawdev_pci_driver, cnxk_rvu_lf_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(rvu_lf_rawdev_pci_driver, pci_rvu_lf_map);
RTE_PMD_REGISTER_KMOD_DEP(rvu_lf_rawdev_pci_driver, "vfio-pci");
RTE_LOG_REGISTER_SUFFIX(cnxk_logtype_rvu_lf, rvu_lf, INFO);
