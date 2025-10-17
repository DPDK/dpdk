/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 NXP
 */

#include <stdbool.h>
#include <rte_random.h>
#include <dpaax_iova_table.h>
#include "base/enetc4_hw.h"
#include "base/enetc_hw.h"
#include "enetc_logs.h"
#include "enetc.h"

int
enetc4_vf_dev_stop(struct rte_eth_dev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int
enetc4_vf_dev_start(struct rte_eth_dev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int
enetc4_vf_stats_get(struct rte_eth_dev *dev,
		    struct rte_eth_stats *stats)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_bdr *rx_ring;
	uint8_t i;

	PMD_INIT_FUNC_TRACE();
	stats->ipackets = enetc4_rd(enetc_hw, ENETC4_SIRFRM0);
	stats->opackets = enetc4_rd(enetc_hw, ENETC4_SITFRM0);
	stats->ibytes = enetc4_rd(enetc_hw, ENETC4_SIROCT0);
	stats->obytes = enetc4_rd(enetc_hw, ENETC4_SITOCT0);
	stats->oerrors = enetc4_rd(enetc_hw, ENETC4_SITDFCR);
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rx_ring = dev->data->rx_queues[i];
		stats->ierrors += rx_ring->ierrors;
	}
	return 0;
}

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_vf_id_enetc4_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_NXP, ENETC4_DEV_ID_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

/* Features supported by this driver */
static const struct eth_dev_ops enetc4_vf_ops = {
	.dev_configure        = enetc4_dev_configure,
	.dev_start            = enetc4_vf_dev_start,
	.dev_stop             = enetc4_vf_dev_stop,
	.dev_close            = enetc4_dev_close,
	.dev_infos_get        = enetc4_dev_infos_get,
	.stats_get            = enetc4_vf_stats_get,
	.rx_queue_setup       = enetc4_rx_queue_setup,
	.rx_queue_start       = enetc4_rx_queue_start,
	.rx_queue_stop        = enetc4_rx_queue_stop,
	.rx_queue_release     = enetc4_rx_queue_release,
	.tx_queue_setup       = enetc4_tx_queue_setup,
	.tx_queue_start       = enetc4_tx_queue_start,
	.tx_queue_stop        = enetc4_tx_queue_stop,
	.tx_queue_release     = enetc4_tx_queue_release,
};

static int
enetc4_vf_mac_init(struct enetc_eth_hw *hw, struct rte_eth_dev *eth_dev)
{
	uint32_t *mac = (uint32_t *)hw->mac.addr;
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t high_mac = 0;
	uint16_t low_mac = 0;
	char vf_eth_name[ENETC_ETH_NAMESIZE];

	PMD_INIT_FUNC_TRACE();

	/* Enabling Station Interface */
	enetc4_wr(enetc_hw, ENETC_SIMR, ENETC_SIMR_EN);
	*mac = (uint32_t)enetc_rd(enetc_hw, ENETC_SIPMAR0);
	high_mac = (uint32_t)*mac;
	mac++;
	*mac = (uint16_t)enetc_rd(enetc_hw, ENETC_SIPMAR1);
	low_mac = (uint16_t)*mac;

	if ((high_mac | low_mac) == 0) {
		char *first_byte;
		ENETC_PMD_NOTICE("MAC is not available for this SI, "
				 "set random MAC");
		mac = (uint32_t *)hw->mac.addr;
		*mac = (uint32_t)rte_rand();
		first_byte = (char *)mac;
		*first_byte &= 0xfe;    /* clear multicast bit */
		*first_byte |= 0x02;    /* set local assignment bit (IEEE802) */
		enetc4_port_wr(enetc_hw, ENETC4_PMAR0, *mac);
		mac++;
		*mac = (uint16_t)rte_rand();
		enetc4_port_wr(enetc_hw, ENETC4_PMAR1, *mac);
		enetc_print_ethaddr("New address: ",
			(const struct rte_ether_addr *)hw->mac.addr);
	}

	/* Allocate memory for storing MAC addresses */
	snprintf(vf_eth_name, sizeof(vf_eth_name), "enetc4_vf_eth_%d", eth_dev->data->port_id);
	eth_dev->data->mac_addrs = rte_zmalloc(vf_eth_name,
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

static int
enetc4_vf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct enetc_eth_hw *hw =
			    ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int error = 0;

	PMD_INIT_FUNC_TRACE();
	eth_dev->dev_ops = &enetc4_vf_ops;
	enetc4_dev_hw_init(eth_dev);

	error = enetc4_vf_mac_init(hw, eth_dev);
	if (error != 0) {
		ENETC_PMD_ERR("MAC initialization failed!!");
		return -1;
	}

	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_populate();

	ENETC_PMD_DEBUG("port_id %d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);
	return 0;
}

static int
enetc4_vf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		    struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct enetc_eth_adapter),
					     enetc4_vf_dev_init);
}

static struct rte_pci_driver rte_enetc4_vf_pmd = {
	.id_table = pci_vf_id_enetc4_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = enetc4_vf_pci_probe,
	.remove = enetc4_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_enetc4_vf, rte_enetc4_vf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_enetc4_vf, pci_vf_id_enetc4_map);
RTE_PMD_REGISTER_KMOD_DEP(net_enetc4_vf, "* uio_pci_generic");
RTE_LOG_REGISTER_DEFAULT(enetc4_vf_logtype_pmd, NOTICE);
