/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2025 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <ethdev_pci.h>

#include "ngbe_logs.h"
#include "base/ngbe.h"
#include "ngbe_ethdev.h"
#include "ngbe_rxtx.h"
#include "ngbe_regs_group.h"

#define NGBEVF_PMD_NAME "rte_ngbevf_pmd" /* PMD name */
static int ngbevf_dev_close(struct rte_eth_dev *dev);
static int ngbevf_dev_promiscuous_enable(struct rte_eth_dev *dev);
static int ngbevf_dev_promiscuous_disable(struct rte_eth_dev *dev);

/*
 * The set of PCI devices this driver supports (for VF)
 */
static const struct rte_pci_id pci_id_ngbevf_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL_W_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A2_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A2S_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A4_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A4S_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL2_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL2S_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL4_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860AL4S_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860NCSI_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A1_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_WANGXUN, NGBE_DEV_ID_EM_WX1860A1L_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = NGBE_RING_DESC_MAX,
	.nb_min = NGBE_RING_DESC_MIN,
	.nb_align = NGBE_RXD_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = NGBE_RING_DESC_MAX,
	.nb_min = NGBE_RING_DESC_MIN,
	.nb_align = NGBE_TXD_ALIGN,
	.nb_seg_max = NGBE_TX_MAX_SEG,
	.nb_mtu_seg_max = NGBE_TX_MAX_SEG,
};

static const struct eth_dev_ops ngbevf_eth_dev_ops;

/*
 * Negotiate mailbox API version with the PF.
 * After reset API version is always set to the basic one (ngbe_mbox_api_10).
 * Then we try to negotiate starting with the most recent one.
 * If all negotiation attempts fail, then we will proceed with
 * the default one (ngbe_mbox_api_10).
 */
static void
ngbevf_negotiate_api(struct ngbe_hw *hw)
{
	int32_t i;

	/* start with highest supported, proceed down */
	static const int sup_ver[] = {
		ngbe_mbox_api_13,
		ngbe_mbox_api_12,
		ngbe_mbox_api_11,
		ngbe_mbox_api_10,
	};

	for (i = 0; i < ARRAY_SIZE(sup_ver); i++) {
		if (ngbevf_negotiate_api_version(hw, sup_ver[i]) == 0)
			break;
	}
}

/*
 * Virtual Function device init
 */
static int
eth_ngbevf_dev_init(struct rte_eth_dev *eth_dev)
{
	int err;
	uint32_t tc, tcs;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct ngbe_hw *hw = ngbe_dev_hw(eth_dev);

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &ngbevf_eth_dev_ops;

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->sub_system_id = pci_dev->id.subsystem_device_id;
	ngbe_map_device_id(hw);
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;

	/* Initialize the shared code (base driver) */
	err = ngbe_init_shared_code(hw);
	if (err != 0) {
		PMD_INIT_LOG(ERR,
			"Shared code init failed for ngbevf: %d", err);
		return -EIO;
	}

	/* init_mailbox_params */
	hw->mbx.init_params(hw);

	hw->mac.num_rar_entries = 32; /* The MAX of the underlying PF */
	err = hw->mac.reset_hw(hw);

	/*
	 * The VF reset operation returns the NGBE_ERR_INVALID_MAC_ADDR when
	 * the underlying PF driver has not assigned a MAC address to the VF.
	 * In this case, assign a random MAC address.
	 */
	if (err != 0 && err != NGBE_ERR_INVALID_MAC_ADDR) {
		PMD_INIT_LOG(ERR, "VF Initialization Failure: %d", err);
		/*
		 * This error code will be propagated to the app by
		 * rte_eth_dev_reset, so use a public error code rather than
		 * the internal-only NGBE_ERR_RESET_FAILED
		 */
		return -EAGAIN;
	}

	/* negotiate mailbox API version to use with the PF. */
	ngbevf_negotiate_api(hw);

	/* Get Rx/Tx queue count via mailbox, which is ready after reset_hw */
	ngbevf_get_queues(hw, &tcs, &tc);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("ngbevf", RTE_ETHER_ADDR_LEN *
					       hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate %u bytes needed to store MAC addresses",
			     RTE_ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}

	/* reset the hardware with the new settings */
	err = hw->mac.start_hw(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "VF Initialization Failure: %d", err);
		return -EIO;
	}

	/* enter promiscuous mode */
	ngbevf_dev_promiscuous_enable(eth_dev);

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x mac.type=%s",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id, "ngbe_mac_sp_vf");

	return 0;
}

/* Virtual Function device uninit */
static int
eth_ngbevf_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ngbevf_dev_close(eth_dev);

	return 0;
}

static int eth_ngbevf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct ngbe_adapter), eth_ngbevf_dev_init);
}

static int eth_ngbevf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_ngbevf_dev_uninit);
}

/*
 * virtual function driver struct
 */
static struct rte_pci_driver rte_ngbevf_pmd = {
	.id_table = pci_id_ngbevf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_ngbevf_pci_probe,
	.remove = eth_ngbevf_pci_remove,
};

static int
ngbevf_dev_info_get(struct rte_eth_dev *dev,
		     struct rte_eth_dev_info *dev_info)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	dev_info->max_rx_queues = (uint16_t)hw->mac.max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->mac.max_tx_queues;
	dev_info->min_rx_bufsize = 1024;
	dev_info->max_rx_pktlen = NGBE_FRAME_SIZE_MAX;
	dev_info->max_mac_addrs = hw->mac.num_rar_entries;
	dev_info->max_hash_mac_addrs = NGBE_VMDQ_NUM_UC_MAC;
	dev_info->max_vfs = pci_dev->max_vfs;
	dev_info->max_vmdq_pools = RTE_ETH_64_POOLS;
	dev_info->rx_queue_offload_capa = ngbe_get_rx_queue_offloads(dev);
	dev_info->rx_offload_capa = (ngbe_get_rx_port_offloads(dev) |
				     dev_info->rx_queue_offload_capa);
	dev_info->tx_queue_offload_capa = 0;
	dev_info->tx_offload_capa = ngbe_get_tx_port_offloads(dev);
	dev_info->hash_key_size = NGBE_HKEY_MAX_INDEX * sizeof(uint32_t);
	dev_info->reta_size = RTE_ETH_RSS_RETA_SIZE_128;
	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = NGBE_DEFAULT_RX_PTHRESH,
			.hthresh = NGBE_DEFAULT_RX_HTHRESH,
			.wthresh = NGBE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = NGBE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = NGBE_DEFAULT_TX_PTHRESH,
			.hthresh = NGBE_DEFAULT_TX_HTHRESH,
			.wthresh = NGBE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = NGBE_DEFAULT_TX_FREE_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	return 0;
}

static int
ngbevf_dev_close(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hw->mac.reset_hw(hw);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	return 0;
}

static int
ngbevf_dev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct ngbe_hw *hw;
	uint32_t max_frame = mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;
	struct rte_eth_dev_data *dev_data = dev->data;

	hw = ngbe_dev_hw(dev);

	if (mtu < RTE_ETHER_MIN_MTU ||
			max_frame > RTE_ETHER_MAX_JUMBO_FRAME_LEN)
		return -EINVAL;

	/* If device is started, refuse mtu that requires the support of
	 * scattered packets when this feature has not been enabled before.
	 */
	if (dev_data->dev_started && !dev_data->scattered_rx &&
	    (max_frame + 2 * RTE_VLAN_HLEN >
	     dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM)) {
		PMD_INIT_LOG(ERR, "Stop port first.");
		return -EINVAL;
	}

	/*
	 * When supported by the underlying PF driver, use the NGBE_VF_SET_MTU
	 * request of the version 2.0 of the mailbox API.
	 * For now, use the NGBE_VF_SET_LPE request of the version 1.0
	 * of the mailbox API.
	 */
	if (ngbevf_rlpml_set_vf(hw, max_frame))
		return -EINVAL;

	return 0;
}

static int
ngbevf_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	int ret;

	switch (hw->mac.update_xcast_mode(hw, NGBEVF_XCAST_MODE_PROMISC)) {
	case 0:
		ret = 0;
		break;
	case NGBE_ERR_FEATURE_NOT_SUPPORTED:
		ret = -ENOTSUP;
		break;
	default:
		ret = -EAGAIN;
		break;
	}

	return ret;
}

static int
ngbevf_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	int ret;

	switch (hw->mac.update_xcast_mode(hw, NGBEVF_XCAST_MODE_NONE)) {
	case 0:
		ret = 0;
		break;
	case NGBE_ERR_FEATURE_NOT_SUPPORTED:
		ret = -ENOTSUP;
		break;
	default:
		ret = -EAGAIN;
		break;
	}

	return ret;
}

static int
ngbevf_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	int ret;

	if (dev->data->promiscuous == 1)
		return 0;

	switch (hw->mac.update_xcast_mode(hw, NGBEVF_XCAST_MODE_ALLMULTI)) {
	case 0:
		ret = 0;
		break;
	case NGBE_ERR_FEATURE_NOT_SUPPORTED:
		ret = -ENOTSUP;
		break;
	default:
		ret = -EAGAIN;
		break;
	}

	return ret;
}

static int
ngbevf_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	int ret;

	switch (hw->mac.update_xcast_mode(hw, NGBEVF_XCAST_MODE_MULTI)) {
	case 0:
		ret = 0;
		break;
	case NGBE_ERR_FEATURE_NOT_SUPPORTED:
		ret = -ENOTSUP;
		break;
	default:
		ret = -EAGAIN;
		break;
	}

	return ret;
}

/*
 * dev_ops for virtual function, bare necessities for basic vf
 * operation have been implemented
 */
static const struct eth_dev_ops ngbevf_eth_dev_ops = {
	.promiscuous_enable   = ngbevf_dev_promiscuous_enable,
	.promiscuous_disable  = ngbevf_dev_promiscuous_disable,
	.allmulticast_enable  = ngbevf_dev_allmulticast_enable,
	.allmulticast_disable = ngbevf_dev_allmulticast_disable,
	.dev_infos_get        = ngbevf_dev_info_get,
	.mtu_set              = ngbevf_dev_set_mtu,
};

RTE_PMD_REGISTER_PCI(net_ngbe_vf, rte_ngbevf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ngbe_vf, pci_id_ngbevf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ngbe_vf, "* igb_uio | vfio-pci");
