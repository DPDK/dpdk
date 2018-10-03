/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#include <stdbool.h>
#include <rte_ethdev_pci.h>

#include "enetc_logs.h"
#include "enetc.h"

int enetc_logtype_pmd;

/* Functions Prototypes */
static int enetc_dev_configure(struct rte_eth_dev *dev);
static int enetc_dev_start(struct rte_eth_dev *dev);
static void enetc_dev_stop(struct rte_eth_dev *dev);
static void enetc_dev_close(struct rte_eth_dev *dev);
static void enetc_dev_infos_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static int enetc_link_update(struct rte_eth_dev *dev, int wait_to_complete);
static int enetc_hardware_init(struct enetc_eth_hw *hw);

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_enetc_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_FREESCALE, ENETC_DEV_ID) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_FREESCALE, ENETC_DEV_ID_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

/* Features supported by this driver */
static const struct eth_dev_ops enetc_ops = {
	.dev_configure        = enetc_dev_configure,
	.dev_start            = enetc_dev_start,
	.dev_stop             = enetc_dev_stop,
	.dev_close            = enetc_dev_close,
	.link_update          = enetc_link_update,
	.dev_infos_get        = enetc_dev_infos_get,
};

/**
 * Initialisation of the enetc device
 *
 * @param eth_dev
 *   - Pointer to the structure rte_eth_dev
 *
 * @return
 *   - On success, zero.
 *   - On failure, negative value.
 */
static int
enetc_dev_init(struct rte_eth_dev *eth_dev)
{
	int error = 0;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();
	eth_dev->dev_ops = &enetc_ops;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;

	/* Retrieving and storing the HW base address of device */
	hw->hw.reg = (void *)pci_dev->mem_resource[0].addr;
	hw->device_id = pci_dev->id.device_id;

	error = enetc_hardware_init(hw);
	if (error != 0) {
		ENETC_PMD_ERR("Hardware initialization failed");
		return -1;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("enetc_eth", ETHER_ADDR_LEN, 0);
	if (!eth_dev->data->mac_addrs) {
		ENETC_PMD_ERR("Failed to allocate %d bytes needed to "
			      "store MAC addresses",
			      ETHER_ADDR_LEN * 1);
		error = -ENOMEM;
		return -1;
	}

	/* Copy the permanent MAC address */
	ether_addr_copy((struct ether_addr *)hw->mac.addr,
			&eth_dev->data->mac_addrs[0]);

	ENETC_PMD_DEBUG("port_id %d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);
	return 0;
}

static int
enetc_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();
	rte_free(eth_dev->data->mac_addrs);

	return 0;
}

static int
enetc_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

static int
enetc_dev_start(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t val;

	PMD_INIT_FUNC_TRACE();
	val = ENETC_REG_READ(ENETC_GET_HW_ADDR(hw->hw.port,
			     ENETC_PM0_CMD_CFG));
	ENETC_REG_WRITE(ENETC_GET_HW_ADDR(hw->hw.port, ENETC_PM0_CMD_CFG),
			val | ENETC_PM0_TX_EN | ENETC_PM0_RX_EN);

	/* Enable port */
	val = ENETC_REG_READ(ENETC_GET_HW_ADDR(hw->hw.port, ENETC_PMR));
	ENETC_REG_WRITE(ENETC_GET_HW_ADDR(hw->hw.port, ENETC_PMR),
			val | ENETC_PMR_EN);

	return 0;
}

static void
enetc_dev_stop(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t val;

	PMD_INIT_FUNC_TRACE();
	/* Disable port */
	val = ENETC_REG_READ(ENETC_GET_HW_ADDR(hw->hw.port, ENETC_PMR));
	ENETC_REG_WRITE(ENETC_GET_HW_ADDR(hw->hw.port, ENETC_PMR),
			val & (~ENETC_PMR_EN));

	val = ENETC_REG_READ(ENETC_GET_HW_ADDR(hw->hw.port,
			     ENETC_PM0_CMD_CFG));
	ENETC_REG_WRITE(ENETC_GET_HW_ADDR(hw->hw.port, ENETC_PM0_CMD_CFG),
			val & (~(ENETC_PM0_TX_EN | ENETC_PM0_RX_EN)));
}

static void
enetc_dev_close(struct rte_eth_dev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
}

/* return 0 means link status changed, -1 means not changed */
static int
enetc_link_update(struct rte_eth_dev *dev, int wait_to_complete __rte_unused)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_link link;
	uint32_t status;

	PMD_INIT_FUNC_TRACE();

	memset(&link, 0, sizeof(link));

	status = ENETC_REG_READ(ENETC_GET_HW_ADDR(hw->hw.port,
				ENETC_PM0_STATUS));

	if (status & ENETC_LINK_MODE)
		link.link_duplex = ETH_LINK_FULL_DUPLEX;
	else
		link.link_duplex = ETH_LINK_HALF_DUPLEX;

	if (status & ENETC_LINK_STATUS)
		link.link_status = ETH_LINK_UP;
	else
		link.link_status = ETH_LINK_DOWN;

	switch (status & ENETC_LINK_SPEED_MASK) {
	case ENETC_LINK_SPEED_1G:
		link.link_speed = ETH_SPEED_NUM_1G;
		break;

	case ENETC_LINK_SPEED_100M:
		link.link_speed = ETH_SPEED_NUM_100M;
		break;

	default:
	case ENETC_LINK_SPEED_10M:
		link.link_speed = ETH_SPEED_NUM_10M;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static int
enetc_hardware_init(struct enetc_eth_hw *hw)
{
	uint32_t psipmr = 0;

	PMD_INIT_FUNC_TRACE();
	/* Calculating and storing the base HW addresses */
	hw->hw.port = (void *)((size_t)hw->hw.reg + ENETC_PORT_BASE);
	hw->hw.global = (void *)((size_t)hw->hw.reg + ENETC_GLOBAL_BASE);

	/* Enabling Station Interface */
	ENETC_REG_WRITE(ENETC_GET_HW_ADDR(hw->hw.reg, ENETC_SIMR),
					  ENETC_SIMR_EN);

	/* Setting to accept broadcast packets for each inetrface */
	psipmr |= ENETC_PSIPMR_SET_UP(0) | ENETC_PSIPMR_SET_MP(0) |
		  ENETC_PSIPMR_SET_VLAN_MP(0);
	psipmr |= ENETC_PSIPMR_SET_UP(1) | ENETC_PSIPMR_SET_MP(1) |
		  ENETC_PSIPMR_SET_VLAN_MP(1);
	psipmr |= ENETC_PSIPMR_SET_UP(2) | ENETC_PSIPMR_SET_MP(2) |
		  ENETC_PSIPMR_SET_VLAN_MP(2);

	ENETC_REG_WRITE(ENETC_GET_HW_ADDR(hw->hw.port, ENETC_PSIPMR),
			psipmr);

	/* Enabling broadcast address */
	ENETC_REG_WRITE(ENETC_GET_HW_ADDR(hw->hw.port, ENETC_PSIPMAR0(0)),
			0xFFFFFFFF);
	ENETC_REG_WRITE(ENETC_GET_HW_ADDR(hw->hw.port, ENETC_PSIPMAR1(0)),
			0xFFFF << 16);

	return 0;
}

static void
enetc_dev_infos_get(struct rte_eth_dev *dev __rte_unused,
		    struct rte_eth_dev_info *dev_info)
{
	PMD_INIT_FUNC_TRACE();
	dev_info->max_rx_queues = MAX_RX_RINGS;
	dev_info->max_tx_queues = MAX_TX_RINGS;
	dev_info->max_rx_pktlen = 1500;
}

static int
enetc_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct enetc_eth_adapter),
					     enetc_dev_init);
}

static int
enetc_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, enetc_dev_uninit);
}

static struct rte_pci_driver rte_enetc_pmd = {
	.id_table = pci_id_enetc_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA,
	.probe = enetc_pci_probe,
	.remove = enetc_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_enetc, rte_enetc_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_enetc, pci_id_enetc_map);
RTE_PMD_REGISTER_KMOD_DEP(net_enetc, "* vfio-pci");

RTE_INIT(enetc_pmd_init_log)
{
	enetc_logtype_pmd = rte_log_register("pmd.net.enetc");
	if (enetc_logtype_pmd >= 0)
		rte_log_set_level(enetc_logtype_pmd, RTE_LOG_NOTICE);
}
