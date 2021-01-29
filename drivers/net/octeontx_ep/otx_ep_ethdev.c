/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <ethdev_pci.h>

#include "otx2_common.h"
#include "otx_ep_common.h"
#include "otx_ep_vf.h"
#include "otx2_ep_vf.h"
#include "otx_ep_rxtx.h"

#define OTX_EP_DEV(_eth_dev) \
	((struct otx_ep_device *)(_eth_dev)->data->dev_private)

static const struct rte_eth_desc_lim otx_ep_rx_desc_lim = {
	.nb_max		= OTX_EP_MAX_OQ_DESCRIPTORS,
	.nb_min		= OTX_EP_MIN_OQ_DESCRIPTORS,
	.nb_align	= OTX_EP_RXD_ALIGN,
};

static const struct rte_eth_desc_lim otx_ep_tx_desc_lim = {
	.nb_max		= OTX_EP_MAX_IQ_DESCRIPTORS,
	.nb_min		= OTX_EP_MIN_IQ_DESCRIPTORS,
	.nb_align	= OTX_EP_TXD_ALIGN,
};

static int
otx_ep_dev_info_get(struct rte_eth_dev *eth_dev,
		    struct rte_eth_dev_info *devinfo)
{
	struct otx_ep_device *otx_epvf;

	otx_epvf = OTX_EP_DEV(eth_dev);

	devinfo->speed_capa = ETH_LINK_SPEED_10G;
	devinfo->max_rx_queues = otx_epvf->max_rx_queues;
	devinfo->max_tx_queues = otx_epvf->max_tx_queues;

	devinfo->min_rx_bufsize = OTX_EP_MIN_RX_BUF_SIZE;
	devinfo->max_rx_pktlen = OTX_EP_MAX_PKT_SZ;
	devinfo->rx_offload_capa = DEV_RX_OFFLOAD_JUMBO_FRAME;
	devinfo->rx_offload_capa |= DEV_RX_OFFLOAD_SCATTER;
	devinfo->tx_offload_capa = DEV_TX_OFFLOAD_MULTI_SEGS;

	devinfo->max_mac_addrs = OTX_EP_MAX_MAC_ADDRS;

	devinfo->rx_desc_lim = otx_ep_rx_desc_lim;
	devinfo->tx_desc_lim = otx_ep_tx_desc_lim;

	return 0;
}

static int
otx_ep_chip_specific_setup(struct otx_ep_device *otx_epvf)
{
	struct rte_pci_device *pdev = otx_epvf->pdev;
	uint32_t dev_id = pdev->id.device_id;
	int ret = 0;

	switch (dev_id) {
	case PCI_DEVID_OCTEONTX_EP_VF:
		otx_epvf->chip_id = dev_id;
		ret = otx_ep_vf_setup_device(otx_epvf);
		break;
	case PCI_DEVID_OCTEONTX2_EP_NET_VF:
	case PCI_DEVID_CN98XX_EP_NET_VF:
		otx_epvf->chip_id = dev_id;
		ret = otx2_ep_vf_setup_device(otx_epvf);
		break;
	default:
		otx_ep_err("Unsupported device\n");
		ret = -EINVAL;
	}

	if (!ret)
		otx_ep_info("OTX_EP dev_id[%d]\n", dev_id);

	return ret;
}

/* OTX_EP VF device initialization */
static int
otx_epdev_init(struct otx_ep_device *otx_epvf)
{
	uint32_t ethdev_queues;
	int ret = 0;

	ret = otx_ep_chip_specific_setup(otx_epvf);
	if (ret) {
		otx_ep_err("Chip specific setup failed\n");
		goto setup_fail;
	}

	otx_epvf->fn_list.setup_device_regs(otx_epvf);

	ethdev_queues = (uint32_t)(otx_epvf->sriov_info.rings_per_vf);
	otx_epvf->max_rx_queues = ethdev_queues;
	otx_epvf->max_tx_queues = ethdev_queues;

	otx_ep_info("OTX_EP Device is Ready\n");

setup_fail:
	return ret;
}

static int
otx_ep_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_eth_rxmode *rxmode;
	struct rte_eth_txmode *txmode;
	struct rte_eth_conf *conf;

	conf = &data->dev_conf;
	rxmode = &conf->rxmode;
	txmode = &conf->txmode;
	if (eth_dev->data->nb_rx_queues > otx_epvf->max_rx_queues ||
	    eth_dev->data->nb_tx_queues > otx_epvf->max_tx_queues) {
		otx_ep_err("invalid num queues\n");
		return -EINVAL;
	}
	otx_ep_info("OTX_EP Device is configured with num_txq %d num_rxq %d\n",
		    eth_dev->data->nb_rx_queues, eth_dev->data->nb_tx_queues);

	otx_epvf->rx_offloads = rxmode->offloads;
	otx_epvf->tx_offloads = txmode->offloads;

	return 0;
}

/* Define our ethernet definitions */
static const struct eth_dev_ops otx_ep_eth_dev_ops = {
	.dev_configure		= otx_ep_dev_configure,
	.dev_infos_get		= otx_ep_dev_info_get,
};

static int
otx_ep_eth_dev_uninit(__rte_unused struct rte_eth_dev *eth_dev)
{
	return 0;
}

static int
otx_ep_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pdev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct otx_ep_device *otx_epvf = OTX_EP_DEV(eth_dev);
	struct rte_ether_addr vf_mac_addr;

	/* Single process support */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	otx_epvf->eth_dev = eth_dev;
	otx_epvf->port_id = eth_dev->data->port_id;
	eth_dev->dev_ops = &otx_ep_eth_dev_ops;
	eth_dev->data->mac_addrs = rte_zmalloc("otx_ep", RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		otx_ep_err("MAC addresses memory allocation failed\n");
		return -ENOMEM;
	}
	rte_eth_random_addr(vf_mac_addr.addr_bytes);
	rte_ether_addr_copy(&vf_mac_addr, eth_dev->data->mac_addrs);
	otx_epvf->hw_addr = pdev->mem_resource[0].addr;
	otx_epvf->pdev = pdev;

	otx_epdev_init(otx_epvf);

	return 0;
}

static int
otx_ep_eth_dev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		      struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct otx_ep_device),
					     otx_ep_eth_dev_init);
}

static int
otx_ep_eth_dev_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev,
					      otx_ep_eth_dev_uninit);
}

/* Set of PCI devices this driver supports */
static const struct rte_pci_id pci_id_otx_ep_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_OCTEONTX_EP_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_OCTEONTX2_EP_NET_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN98XX_EP_NET_VF) },
	{ .vendor_id = 0, /* sentinel */ }
};

static struct rte_pci_driver rte_otx_ep_pmd = {
	.id_table	= pci_id_otx_ep_map,
	.drv_flags      = RTE_PCI_DRV_NEED_MAPPING,
	.probe		= otx_ep_eth_dev_pci_probe,
	.remove		= otx_ep_eth_dev_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_otx_ep, rte_otx_ep_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_otx_ep, pci_id_otx_ep_map);
RTE_PMD_REGISTER_KMOD_DEP(net_otx_ep, "* igb_uio | vfio-pci");
RTE_LOG_REGISTER(otx_net_ep_logtype, pmd.net.octeontx_ep, NOTICE);
