/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#include "base/hinic_compat.h"
#include "base/hinic_pmd_hwdev.h"
#include "base/hinic_pmd_niccfg.h"
#include "hinic_pmd_ethdev.h"

/* Vendor ID used by Huawei devices */
#define HINIC_HUAWEI_VENDOR_ID		0x19E5

/* Hinic devices */
#define HINIC_DEV_ID_PRD		0x1822
#define HINIC_DEV_ID_MEZZ_25GE		0x0210
#define HINIC_DEV_ID_MEZZ_40GE		0x020D
#define HINIC_DEV_ID_MEZZ_100GE		0x0205

#define HINIC_MIN_RX_BUF_SIZE		1024
#define HINIC_MAX_MAC_ADDRS		1

/** Driver-specific log messages type. */
int hinic_logtype;

static const struct rte_eth_desc_lim hinic_rx_desc_lim = {
	.nb_max = HINIC_MAX_QUEUE_DEPTH,
	.nb_min = HINIC_MIN_QUEUE_DEPTH,
	.nb_align = HINIC_RXD_ALIGN,
};

static const struct rte_eth_desc_lim hinic_tx_desc_lim = {
	.nb_max = HINIC_MAX_QUEUE_DEPTH,
	.nb_min = HINIC_MIN_QUEUE_DEPTH,
	.nb_align = HINIC_TXD_ALIGN,
};

/**
 * Get link speed from NIC.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param speed_capa
 *   Pointer to link speed structure.
 */
static void hinic_get_speed_capa(struct rte_eth_dev *dev, uint32_t *speed_capa)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	u32 supported_link, advertised_link;
	int err;

#define HINIC_LINK_MODE_SUPPORT_1G	(1U << HINIC_GE_BASE_KX)

#define HINIC_LINK_MODE_SUPPORT_10G	(1U << HINIC_10GE_BASE_KR)

#define HINIC_LINK_MODE_SUPPORT_25G	((1U << HINIC_25GE_BASE_KR_S) | \
					(1U << HINIC_25GE_BASE_CR_S) | \
					(1U << HINIC_25GE_BASE_KR) | \
					(1U << HINIC_25GE_BASE_CR))

#define HINIC_LINK_MODE_SUPPORT_40G	((1U << HINIC_40GE_BASE_KR4) | \
					(1U << HINIC_40GE_BASE_CR4))

#define HINIC_LINK_MODE_SUPPORT_100G	((1U << HINIC_100GE_BASE_KR4) | \
					(1U << HINIC_100GE_BASE_CR4))

	err = hinic_get_link_mode(nic_dev->hwdev,
				  &supported_link, &advertised_link);
	if (err || supported_link == HINIC_SUPPORTED_UNKNOWN ||
	    advertised_link == HINIC_SUPPORTED_UNKNOWN) {
		PMD_DRV_LOG(WARNING, "Get speed capability info failed, device: %s, port_id: %u",
			  nic_dev->proc_dev_name, dev->data->port_id);
	} else {
		*speed_capa = 0;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_1G))
			*speed_capa |= ETH_LINK_SPEED_1G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_10G))
			*speed_capa |= ETH_LINK_SPEED_10G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_25G))
			*speed_capa |= ETH_LINK_SPEED_25G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_40G))
			*speed_capa |= ETH_LINK_SPEED_40G;
		if (!!(supported_link & HINIC_LINK_MODE_SUPPORT_100G))
			*speed_capa |= ETH_LINK_SPEED_100G;
	}
}

/**
 * DPDK callback to get information about the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param info
 *   Pointer to Info structure output buffer.
 */
static void
hinic_dev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct hinic_nic_dev *nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	info->max_rx_queues  = nic_dev->nic_cap.max_rqs;
	info->max_tx_queues  = nic_dev->nic_cap.max_sqs;
	info->min_rx_bufsize = HINIC_MIN_RX_BUF_SIZE;
	info->max_rx_pktlen  = HINIC_MAX_JUMBO_FRAME_SIZE;
	info->max_mac_addrs  = HINIC_MAX_MAC_ADDRS;

	hinic_get_speed_capa(dev, &info->speed_capa);
	info->rx_queue_offload_capa = 0;
	info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP |
				DEV_RX_OFFLOAD_IPV4_CKSUM |
				DEV_RX_OFFLOAD_UDP_CKSUM |
				DEV_RX_OFFLOAD_TCP_CKSUM;

	info->tx_queue_offload_capa = 0;
	info->tx_offload_capa = DEV_TX_OFFLOAD_VLAN_INSERT |
				DEV_TX_OFFLOAD_IPV4_CKSUM |
				DEV_TX_OFFLOAD_UDP_CKSUM |
				DEV_TX_OFFLOAD_TCP_CKSUM |
				DEV_TX_OFFLOAD_SCTP_CKSUM |
				DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
				DEV_TX_OFFLOAD_TCP_TSO |
				DEV_TX_OFFLOAD_MULTI_SEGS;

	info->hash_key_size = HINIC_RSS_KEY_SIZE;
	info->reta_size = HINIC_RSS_INDIR_SIZE;
	info->rx_desc_lim = hinic_rx_desc_lim;
	info->tx_desc_lim = hinic_tx_desc_lim;
}

static int hinic_func_init(__rte_unused struct rte_eth_dev *eth_dev)
{
	return 0;
}

/**
 * DPDK callback to close the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void hinic_dev_close(__rte_unused struct rte_eth_dev *dev)
{
}

static const struct eth_dev_ops hinic_pmd_ops = {
	.dev_infos_get                 = hinic_dev_infos_get,
};

static int hinic_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	PMD_DRV_LOG(INFO, "Initializing pf hinic-%.4x:%.2x:%.2x.%x in %s process",
		    pci_dev->addr.domain, pci_dev->addr.bus,
		    pci_dev->addr.devid, pci_dev->addr.function,
		    (rte_eal_process_type() == RTE_PROC_PRIMARY) ?
		    "primary" : "secondary");

	/* rte_eth_dev ops, rx_burst and tx_burst */
	eth_dev->dev_ops = &hinic_pmd_ops;

	return hinic_func_init(eth_dev);
}

static int hinic_dev_uninit(struct rte_eth_dev *dev)
{
	struct hinic_nic_dev *nic_dev;

	nic_dev = HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	hinic_clear_bit(HINIC_DEV_INIT, &nic_dev->dev_status);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hinic_dev_close(dev);

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	return HINIC_OK;
}

static struct rte_pci_id pci_id_hinic_map[] = {
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_PRD) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_MEZZ_25GE) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_MEZZ_40GE) },
	{ RTE_PCI_DEVICE(HINIC_HUAWEI_VENDOR_ID, HINIC_DEV_ID_MEZZ_100GE) },
	{.vendor_id = 0},
};

static int hinic_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct hinic_nic_dev), hinic_dev_init);
}

static int hinic_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, hinic_dev_uninit);
}

static struct rte_pci_driver rte_hinic_pmd = {
	.id_table = pci_id_hinic_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = hinic_pci_probe,
	.remove = hinic_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_hinic, rte_hinic_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_hinic, pci_id_hinic_map);

RTE_INIT(hinic_init_log)
{
	hinic_logtype = rte_log_register("pmd.net.hinic");
	if (hinic_logtype >= 0)
		rte_log_set_level(hinic_logtype, RTE_LOG_INFO);
}
