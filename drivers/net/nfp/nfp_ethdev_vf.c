/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

#include <rte_alarm.h>
#include <nfp_common_pci.h>

#include "nfd3/nfp_nfd3.h"
#include "nfdk/nfp_nfdk.h"
#include "nfpcore/nfp_cpp.h"

#include "nfp_logs.h"
#include "nfp_net_common.h"
#include "nfp_rxtx_vec.h"

#define NFP_VF_DRIVER_NAME net_nfp_vf

static int
nfp_netvf_start(struct rte_eth_dev *dev)
{
	int ret;
	uint16_t i;
	struct nfp_hw *hw;
	uint32_t new_ctrl;
	uint32_t update = 0;
	uint32_t intr_vector;
	struct nfp_net_hw *net_hw;
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	/* Check and configure queue intr-vector mapping */
	if (dev->data->dev_conf.intr_conf.rxq != 0) {
		if (rte_intr_type_get(intr_handle) == RTE_INTR_HANDLE_UIO) {
			/*
			 * Better not to share LSC with RX interrupts.
			 * Unregistering LSC interrupt handler.
			 */
			rte_intr_callback_unregister(intr_handle,
					nfp_net_dev_interrupt_handler, (void *)dev);

			if (dev->data->nb_rx_queues > 1) {
				PMD_INIT_LOG(ERR, "PMD rx interrupt only "
						"supports 1 queue with UIO.");
				return -EIO;
			}
		}

		intr_vector = dev->data->nb_rx_queues;
		if (rte_intr_efd_enable(intr_handle, intr_vector) != 0)
			return -1;

		nfp_configure_rx_interrupt(dev, intr_handle);
		update = NFP_NET_CFG_UPDATE_MSIX;
	}

	rte_intr_enable(intr_handle);

	new_ctrl = nfp_check_offloads(dev);

	/* Writing configuration parameters in the device */
	net_hw = dev->data->dev_private;
	hw = &net_hw->super;
	nfp_net_params_setup(net_hw);

	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;

	if ((rxmode->offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH) != 0) {
		nfp_net_rss_config_default(dev);
		update |= NFP_NET_CFG_UPDATE_RSS;
		new_ctrl |= nfp_net_cfg_ctrl_rss(hw->cap);
	}

	/* Enable device */
	new_ctrl |= NFP_NET_CFG_CTRL_ENABLE;

	update |= NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING;

	if ((hw->cap & NFP_NET_CFG_CTRL_RINGCFG) != 0)
		new_ctrl |= NFP_NET_CFG_CTRL_RINGCFG;

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, new_ctrl);
	if (nfp_reconfig(hw, new_ctrl, update) != 0)
		return -EIO;

	hw->ctrl = new_ctrl;

	/*
	 * Allocating rte mbufs for configured rx queues.
	 * This requires queues being enabled before.
	 */
	if (nfp_net_rx_freelist_setup(dev) != 0) {
		ret = -ENOMEM;
		goto error;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;

error:
	/*
	 * An error returned by this function should mean the app
	 * exiting and then the system releasing all the memory
	 * allocated even memory coming from hugepages.
	 *
	 * The device could be enabled at this point with some queues
	 * ready for getting packets. This is true if the call to
	 * nfp_net_rx_freelist_setup() succeeds for some queues but
	 * fails for subsequent queues.
	 *
	 * This should make the app exiting but better if we tell the
	 * device first.
	 */
	nfp_net_disable_queues(dev);

	return ret;
}

static int
nfp_netvf_stop(struct rte_eth_dev *dev)
{
	nfp_net_disable_queues(dev);

	/* Clear queues */
	nfp_net_stop_tx_queue(dev);

	nfp_net_stop_rx_queue(dev);

	return 0;
}

static int
nfp_netvf_set_link_up(struct rte_eth_dev *dev __rte_unused)
{
	return -ENOTSUP;
}

/* Set the link down. */
static int
nfp_netvf_set_link_down(struct rte_eth_dev *dev __rte_unused)
{
	return -ENOTSUP;
}

/* Reset and stop device. The device can not be restarted. */
static int
nfp_netvf_close(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *net_hw;
	struct rte_pci_device *pci_dev;
	struct nfp_net_hw_priv *hw_priv;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	net_hw = dev->data->dev_private;
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	hw_priv = dev->process_private;

	rte_free(net_hw->eth_xstats_base);
	rte_free(hw_priv);

	/*
	 * We assume that the DPDK application is stopping all the
	 * threads/queues before calling the device close function.
	 */
	nfp_net_disable_queues(dev);

	/* Clear queues */
	nfp_net_close_tx_queue(dev);
	nfp_net_close_rx_queue(dev);

	rte_intr_disable(pci_dev->intr_handle);

	/* Unregister callback func from eal lib */
	rte_intr_callback_unregister(pci_dev->intr_handle,
			nfp_net_dev_interrupt_handler, (void *)dev);

	/* Cancel possible impending LSC work here before releasing the port */
	rte_eal_alarm_cancel(nfp_net_dev_interrupt_delayed_handler, (void *)dev);

	return 0;
}

/* Initialise and register VF driver with DPDK Application */
static const struct eth_dev_ops nfp_netvf_eth_dev_ops = {
	.dev_configure          = nfp_net_configure,
	.dev_start              = nfp_netvf_start,
	.dev_stop               = nfp_netvf_stop,
	.dev_set_link_up        = nfp_netvf_set_link_up,
	.dev_set_link_down      = nfp_netvf_set_link_down,
	.dev_close              = nfp_netvf_close,
	.promiscuous_enable     = nfp_net_promisc_enable,
	.promiscuous_disable    = nfp_net_promisc_disable,
	.allmulticast_enable    = nfp_net_allmulticast_enable,
	.allmulticast_disable   = nfp_net_allmulticast_disable,
	.link_update            = nfp_net_link_update,
	.stats_get              = nfp_net_stats_get,
	.stats_reset            = nfp_net_stats_reset,
	.xstats_get             = nfp_net_xstats_get,
	.xstats_reset           = nfp_net_xstats_reset,
	.xstats_get_names       = nfp_net_xstats_get_names,
	.xstats_get_by_id       = nfp_net_xstats_get_by_id,
	.xstats_get_names_by_id = nfp_net_xstats_get_names_by_id,
	.dev_infos_get          = nfp_net_infos_get,
	.dev_supported_ptypes_get = nfp_net_supported_ptypes_get,
	.mtu_set                = nfp_net_dev_mtu_set,
	.mac_addr_set           = nfp_net_set_mac_addr,
	.vlan_offload_set       = nfp_net_vlan_offload_set,
	.reta_update            = nfp_net_reta_update,
	.reta_query             = nfp_net_reta_query,
	.rss_hash_update        = nfp_net_rss_hash_update,
	.rss_hash_conf_get      = nfp_net_rss_hash_conf_get,
	.rx_queue_setup         = nfp_net_rx_queue_setup,
	.rx_queue_release       = nfp_net_rx_queue_release,
	.tx_queue_setup         = nfp_net_tx_queue_setup,
	.tx_queue_release       = nfp_net_tx_queue_release,
	.rx_queue_intr_enable   = nfp_rx_queue_intr_enable,
	.rx_queue_intr_disable  = nfp_rx_queue_intr_disable,
	.rx_burst_mode_get      = nfp_net_rx_burst_mode_get,
	.tx_burst_mode_get      = nfp_net_tx_burst_mode_get,
};

static inline void
nfp_netvf_ethdev_ops_mount(struct nfp_pf_dev *pf_dev,
		struct rte_eth_dev *eth_dev)
{
	if (pf_dev->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3)
		eth_dev->tx_pkt_burst = nfp_net_nfd3_xmit_pkts;
	else
		nfp_net_nfdk_xmit_pkts_set(eth_dev);

	eth_dev->dev_ops = &nfp_netvf_eth_dev_ops;
	eth_dev->rx_queue_count = nfp_net_rx_queue_count;
	nfp_net_recv_pkts_set(eth_dev);
}

static int
nfp_netvf_init(struct rte_eth_dev *eth_dev)
{
	int err;
	uint16_t port;
	uint32_t start_q;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;
	struct nfp_pf_dev *pf_dev;
	uint64_t tx_bar_off = 0;
	uint64_t rx_bar_off = 0;
	struct rte_pci_device *pci_dev;
	struct nfp_net_hw_priv *hw_priv;
	const struct nfp_dev_info *dev_info;

	port = eth_dev->data->port_id;
	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	dev_info = nfp_dev_info_get(pci_dev->id.device_id);
	if (dev_info == NULL) {
		PMD_INIT_LOG(ERR, "Not supported device ID.");
		return -ENODEV;
	}

	net_hw = eth_dev->data->dev_private;
	hw = &net_hw->super;

	hw->ctrl_bar = pci_dev->mem_resource[0].addr;
	if (hw->ctrl_bar == NULL) {
		PMD_DRV_LOG(ERR, "The hw->super.ctrl_bar is NULL. BAR0 not configured.");
		return -ENODEV;
	}

	pf_dev = rte_zmalloc(NULL, sizeof(*pf_dev), 0);
	if (pf_dev == NULL) {
		PMD_INIT_LOG(ERR, "Can not allocate memory for the PF device.");
		return -ENOMEM;
	}

	pf_dev->pci_dev = pci_dev;

	/* Check the version from firmware */
	if (!nfp_net_version_check(hw, pf_dev)) {
		err = -EINVAL;
		goto pf_dev_free;
	}

	/* Set the ctrl bar size */
	nfp_net_ctrl_bar_size_set(pf_dev);

	PMD_INIT_LOG(DEBUG, "Ctrl bar: %p.", hw->ctrl_bar);

	err = nfp_net_common_init(pf_dev, net_hw);
	if (err != 0)
		goto pf_dev_free;

	nfp_netvf_ethdev_ops_mount(pf_dev, eth_dev);

	hw_priv = rte_zmalloc(NULL, sizeof(*hw_priv), 0);
	if (hw_priv == NULL) {
		PMD_INIT_LOG(ERR, "Can not alloc memory for hw priv data.");
		err = -ENOMEM;
		goto hw_priv_free;
	}

	hw_priv->dev_info = dev_info;
	hw_priv->pf_dev = pf_dev;

	if (!nfp_net_recv_pkt_meta_check_register(hw_priv)) {
		PMD_INIT_LOG(ERR, "VF register meta check function failed.");
		err = -EINVAL;
		goto hw_priv_free;
	}

	eth_dev->process_private = hw_priv;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	net_hw->eth_xstats_base = rte_calloc("rte_eth_xstat",
			nfp_net_xstats_size(eth_dev), sizeof(struct rte_eth_xstat), 0);
	if (net_hw->eth_xstats_base == NULL) {
		PMD_INIT_LOG(ERR, "No memory for xstats base values on device %s!",
				pci_dev->device.name);
		err = -ENOMEM;
		goto hw_priv_free;
	}

	/* Work out where in the BAR the queues start. */
	start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_TXQ);
	tx_bar_off = nfp_qcp_queue_offset(dev_info, start_q);
	start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_RXQ);
	rx_bar_off = nfp_qcp_queue_offset(dev_info, start_q);

	net_hw->tx_bar = (uint8_t *)pci_dev->mem_resource[2].addr + tx_bar_off;
	net_hw->rx_bar = (uint8_t *)pci_dev->mem_resource[2].addr + rx_bar_off;

	PMD_INIT_LOG(DEBUG, "The ctrl_bar: %p, tx_bar: %p, rx_bar: %p.",
			hw->ctrl_bar, net_hw->tx_bar, net_hw->rx_bar);

	nfp_net_cfg_queue_setup(net_hw);
	net_hw->mtu = RTE_ETHER_MTU;

	/* VLAN insertion is incompatible with LSOv2 */
	if ((hw->cap & NFP_NET_CFG_CTRL_LSO2) != 0)
		hw->cap &= ~NFP_NET_CFG_CTRL_TXVLAN;

	nfp_net_log_device_information(net_hw, pf_dev);

	/* Initializing spinlock for reconfigs */
	rte_spinlock_init(&hw->reconfig_lock);

	/* Allocating memory for mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to space for MAC address.");
		err = -ENOMEM;
		goto free_xstats;
	}

	nfp_read_mac(hw);
	if (rte_is_valid_assigned_ether_addr(&hw->mac_addr) == 0) {
		PMD_INIT_LOG(INFO, "Using random mac address for port %hu.", port);
		/* Using random mac addresses for VFs */
		rte_eth_random_addr(&hw->mac_addr.addr_bytes[0]);
		nfp_write_mac(hw, &hw->mac_addr.addr_bytes[0]);
	}

	/* Copying mac address to DPDK eth_dev struct */
	rte_ether_addr_copy(&hw->mac_addr, eth_dev->data->mac_addrs);

	if ((hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR) == 0)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_NOLIVE_MAC_ADDR;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	PMD_INIT_LOG(INFO, "Port %hu VendorID=%#x DeviceID=%#x "
			"mac=" RTE_ETHER_ADDR_PRT_FMT,
			port, pci_dev->id.vendor_id,
			pci_dev->id.device_id,
			RTE_ETHER_ADDR_BYTES(&hw->mac_addr));

	/* Registering LSC interrupt handler */
	rte_intr_callback_register(pci_dev->intr_handle,
			nfp_net_dev_interrupt_handler, (void *)eth_dev);
	/* Telling the firmware about the LSC interrupt entry */
	nn_cfg_writeb(hw, NFP_NET_CFG_LSC, NFP_NET_IRQ_LSC_IDX);
	/* Unmask the LSC interrupt */
	nfp_net_irq_unmask(eth_dev);
	/* Recording current stats counters values */
	nfp_net_stats_reset(eth_dev);

	return 0;

free_xstats:
	rte_free(net_hw->eth_xstats_base);
hw_priv_free:
	rte_free(hw_priv);
pf_dev_free:
	rte_free(pf_dev);

	return err;
}

static const struct rte_pci_id pci_id_nfp_vf_net_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETRONOME,
				PCI_DEVICE_ID_NFP3800_VF_NIC)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETRONOME,
				PCI_DEVICE_ID_NFP6000_VF_NIC)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CORIGINE,
				PCI_DEVICE_ID_NFP3800_VF_NIC)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CORIGINE,
				PCI_DEVICE_ID_NFP6000_VF_NIC)
	},
	{
		.vendor_id = 0,
	},
};

static int
nfp_vf_pci_uninit(struct rte_eth_dev *eth_dev)
{
	/* VF cleanup, just free private port data */
	return nfp_netvf_close(eth_dev);
}

static int
nfp_vf_pci_probe(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
			sizeof(struct nfp_net_hw), nfp_netvf_init);
}

static int
nfp_vf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, nfp_vf_pci_uninit);
}

static struct nfp_class_driver rte_nfp_net_vf_pmd = {
	.drv_class = NFP_CLASS_ETH,
	.name = RTE_STR(net_nfp_vf),
	.id_table = pci_id_nfp_vf_net_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = nfp_vf_pci_probe,
	.remove = nfp_vf_pci_remove,
};

RTE_INIT(rte_nfp_vf_pmd_init)
{
	nfp_class_driver_register(&rte_nfp_net_vf_pmd);
}

RTE_PMD_REGISTER_PCI_TABLE(NFP_VF_DRIVER_NAME, pci_id_nfp_vf_net_map);
RTE_PMD_REGISTER_KMOD_DEP(NFP_VF_DRIVER_NAME, "* igb_uio | uio_pci_generic | vfio");
