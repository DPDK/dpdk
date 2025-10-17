/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 NXP
 */

#include <stdbool.h>
#include <rte_random.h>
#include <dpaax_iova_table.h>

#include "base/enetc4_hw.h"
#include "enetc_logs.h"
#include "enetc.h"

static int
enetc4_dev_start(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t val;

	PMD_INIT_FUNC_TRACE();

	val = enetc4_port_rd(enetc_hw, ENETC4_PM_CMD_CFG(0));
	enetc4_port_wr(enetc_hw, ENETC4_PM_CMD_CFG(0),
		       val | PM_CMD_CFG_TX_EN | PM_CMD_CFG_RX_EN);

	val = enetc4_port_rd(enetc_hw, ENETC4_PM_CMD_CFG(1));
	enetc4_port_wr(enetc_hw, ENETC4_PM_CMD_CFG(1),
		       val | PM_CMD_CFG_TX_EN | PM_CMD_CFG_RX_EN);

	/* Enable port */
	val = enetc4_port_rd(enetc_hw, ENETC4_PMR);
	enetc4_port_wr(enetc_hw, ENETC4_PMR, val | ENETC4_PMR_EN);

	/* Enable port transmit/receive */
	enetc4_port_wr(enetc_hw, ENETC4_POR, 0);

	return 0;
}

static int
enetc4_dev_stop(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t val;

	PMD_INIT_FUNC_TRACE();

	/* Disable port */
	val = enetc4_port_rd(enetc_hw, ENETC4_PMR);
	enetc4_port_wr(enetc_hw, ENETC4_PMR, val & (~ENETC4_PMR_EN));

	val = enetc4_port_rd(enetc_hw, ENETC4_PM_CMD_CFG(0));
	enetc4_port_wr(enetc_hw, ENETC4_PM_CMD_CFG(0),
		      val & (~(PM_CMD_CFG_TX_EN | PM_CMD_CFG_RX_EN)));

	val = enetc4_port_rd(enetc_hw, ENETC4_PM_CMD_CFG(1));
	enetc4_port_wr(enetc_hw, ENETC4_PM_CMD_CFG(1),
		      val & (~(PM_CMD_CFG_TX_EN | PM_CMD_CFG_RX_EN)));

	return 0;
}

static int
enetc4_mac_init(struct enetc_eth_hw *hw, struct rte_eth_dev *eth_dev)
{
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t high_mac = 0;
	uint16_t low_mac = 0;
	char eth_name[ENETC_ETH_NAMESIZE];

	PMD_INIT_FUNC_TRACE();

	/* Enabling Station Interface */
	enetc4_wr(enetc_hw, ENETC_SIMR, ENETC_SIMR_EN);

	high_mac = (uint32_t)enetc4_port_rd(enetc_hw, ENETC4_PSIPMAR0(0));
	low_mac = (uint16_t)enetc4_port_rd(enetc_hw, ENETC4_PSIPMAR1(0));

	if ((high_mac | low_mac) == 0) {
		ENETC_PMD_NOTICE("MAC is not available for this SI, "
				"set random MAC");
		rte_eth_random_addr(hw->mac.addr);
		high_mac = *(uint32_t *)hw->mac.addr;
		enetc4_port_wr(enetc_hw, ENETC4_PMAR0, high_mac);
		low_mac = *(uint16_t *)(hw->mac.addr + 4);
		enetc4_port_wr(enetc_hw, ENETC4_PMAR1, low_mac);
		enetc_print_ethaddr("New address: ",
			      (const struct rte_ether_addr *)hw->mac.addr);
	}

	/* Allocate memory for storing MAC addresses */
	snprintf(eth_name, sizeof(eth_name), "enetc4_eth_%d", eth_dev->data->port_id);
	eth_dev->data->mac_addrs = rte_zmalloc(eth_name,
					RTE_ETHER_ADDR_LEN, 0);
	if (!eth_dev->data->mac_addrs) {
		ENETC_PMD_ERR("Failed to allocate %d bytes needed to "
			      "store MAC addresses",
			      RTE_ETHER_ADDR_LEN * 1);
		return -ENOMEM;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.addr,
			&eth_dev->data->mac_addrs[0]);

	return 0;
}

int
enetc4_dev_infos_get(struct rte_eth_dev *dev __rte_unused,
		    struct rte_eth_dev_info *dev_info)
{
	PMD_INIT_FUNC_TRACE();
	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = MAX_BD_COUNT,
		.nb_min = MIN_BD_COUNT,
		.nb_align = BD_ALIGN,
	};
	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = MAX_BD_COUNT,
		.nb_min = MIN_BD_COUNT,
		.nb_align = BD_ALIGN,
	};
	dev_info->max_rx_queues = MAX_RX_RINGS;
	dev_info->max_tx_queues = MAX_TX_RINGS;
	dev_info->max_rx_pktlen = ENETC4_MAC_MAXFRM_SIZE;

	return 0;
}

int
enetc4_dev_close(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (hw->device_id == ENETC4_DEV_ID_VF)
		ret = enetc4_vf_dev_stop(dev);
	else
		ret = enetc4_dev_stop(dev);

	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_depopulate();

	return ret;
}

int
enetc4_dev_configure(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t max_len;
	uint32_t val;

	PMD_INIT_FUNC_TRACE();

	max_len = dev->data->dev_conf.rxmode.mtu + RTE_ETHER_HDR_LEN +
		  RTE_ETHER_CRC_LEN;
	enetc4_port_wr(enetc_hw, ENETC4_PM_MAXFRM(0), ENETC_SET_MAXFRM(max_len));

	val = ENETC4_MAC_MAXFRM_SIZE | SDU_TYPE_MPDU;
	enetc4_port_wr(enetc_hw, ENETC4_PTCTMSDUR(0), val | SDU_TYPE_MPDU);

	return 0;
}



/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_enetc4_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_NXP, ENETC4_DEV_ID) },
	{ .vendor_id = 0, /* sentinel */ },
};

/* Features supported by this driver */
static const struct eth_dev_ops enetc4_ops = {
	.dev_configure        = enetc4_dev_configure,
	.dev_start            = enetc4_dev_start,
	.dev_stop             = enetc4_dev_stop,
	.dev_close            = enetc4_dev_close,
	.dev_infos_get        = enetc4_dev_infos_get,
};

/*
 * Storing the HW base addresses
 *
 * @param eth_dev
 *   - Pointer to the structure rte_eth_dev
 */
void
enetc4_dev_hw_init(struct rte_eth_dev *eth_dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	eth_dev->rx_pkt_burst = &enetc_recv_pkts;
	eth_dev->tx_pkt_burst = &enetc_xmit_pkts;

	/* Retrieving and storing the HW base address of device */
	hw->hw.reg = (void *)pci_dev->mem_resource[0].addr;
	hw->device_id = pci_dev->id.device_id;

	/* Calculating and storing the base HW addresses */
	hw->hw.port = (void *)((size_t)hw->hw.reg + ENETC_PORT_BASE);
	hw->hw.global = (void *)((size_t)hw->hw.reg + ENETC_GLOBAL_BASE);
}

/**
 * Initialisation of the enetc4 device
 *
 * @param eth_dev
 *   - Pointer to the structure rte_eth_dev
 *
 * @return
 *   - On success, zero.
 *   - On failure, negative value.
 */

static int
enetc4_dev_init(struct rte_eth_dev *eth_dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int error = 0;

	PMD_INIT_FUNC_TRACE();
	eth_dev->dev_ops = &enetc4_ops;
	enetc4_dev_hw_init(eth_dev);

	error = enetc4_mac_init(hw, eth_dev);
	if (error != 0) {
		ENETC_PMD_ERR("MAC initialization failed");
		return -1;
	}

	/* Set MTU */
	enetc_port_wr(&hw->hw, ENETC4_PM_MAXFRM(0),
		      ENETC_SET_MAXFRM(RTE_ETHER_MAX_LEN));
	eth_dev->data->mtu = RTE_ETHER_MAX_LEN - RTE_ETHER_HDR_LEN -
		RTE_ETHER_CRC_LEN;

	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_populate();

	ENETC_PMD_DEBUG("port_id %d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);
	return 0;
}

static int
enetc4_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	return enetc4_dev_close(eth_dev);
}

static int
enetc4_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct enetc_eth_adapter),
					     enetc4_dev_init);
}

int
enetc4_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, enetc4_dev_uninit);
}

static struct rte_pci_driver rte_enetc4_pmd = {
	.id_table = pci_id_enetc4_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = enetc4_pci_probe,
	.remove = enetc4_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_enetc4, rte_enetc4_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_enetc4, pci_id_enetc4_map);
RTE_PMD_REGISTER_KMOD_DEP(net_enetc4, "* vfio-pci");
RTE_LOG_REGISTER_DEFAULT(enetc4_logtype_pmd, NOTICE);
