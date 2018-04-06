/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_ethdev.h"

static int eth_axgbe_dev_init(struct rte_eth_dev *eth_dev);
static int eth_axgbe_dev_uninit(struct rte_eth_dev *eth_dev);

/* The set of PCI devices this driver supports */
#define AMD_PCI_VENDOR_ID       0x1022
#define AMD_PCI_AXGBE_DEVICE_V2A 0x1458
#define AMD_PCI_AXGBE_DEVICE_V2B 0x1459

int axgbe_logtype_init;
int axgbe_logtype_driver;

static const struct rte_pci_id pci_id_axgbe_map[] = {
	{RTE_PCI_DEVICE(AMD_PCI_VENDOR_ID, AMD_PCI_AXGBE_DEVICE_V2A)},
	{RTE_PCI_DEVICE(AMD_PCI_VENDOR_ID, AMD_PCI_AXGBE_DEVICE_V2B)},
	{ .vendor_id = 0, },
};

/*
 * It returns 0 on success.
 */
static int
eth_axgbe_dev_init(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();
	struct axgbe_port *pdata;
	struct rte_pci_device *pci_dev;

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pdata = (struct axgbe_port *)eth_dev->data->dev_private;
	pdata->eth_dev = eth_dev;

	pci_dev = RTE_DEV_TO_PCI(eth_dev->device);
	pdata->pci_dev = pci_dev;

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

	return 0;
}

static int
eth_axgbe_dev_uninit(struct rte_eth_dev *eth_dev __rte_unused)
{
	/* stub function */
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int eth_axgbe_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct axgbe_port), eth_axgbe_dev_init);
}

static int eth_axgbe_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_axgbe_dev_uninit);
}

static struct rte_pci_driver rte_axgbe_pmd = {
	.id_table = pci_id_axgbe_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_axgbe_pci_probe,
	.remove = eth_axgbe_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_axgbe, rte_axgbe_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_axgbe, pci_id_axgbe_map);
RTE_PMD_REGISTER_KMOD_DEP(net_axgbe, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_INIT(axgbe_init_log);
static void
axgbe_init_log(void)
{
	axgbe_logtype_init = rte_log_register("pmd.net.axgbe.init");
	if (axgbe_logtype_init >= 0)
		rte_log_set_level(axgbe_logtype_init, RTE_LOG_NOTICE);
	axgbe_logtype_driver = rte_log_register("pmd.net.axgbe.driver");
	if (axgbe_logtype_driver >= 0)
		rte_log_set_level(axgbe_logtype_driver, RTE_LOG_NOTICE);
}
