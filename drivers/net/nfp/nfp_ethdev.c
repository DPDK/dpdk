/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_ethdev.c
 *
 * Netronome vNIC DPDK Poll-Mode Driver: Main entry point
 */

#include <rte_common.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <dev_driver.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_mempool.h>
#include <rte_service_component.h>
#include <rte_alarm.h>
#include "eal_firmware.h"

#include "nfpcore/nfp_cpp.h"
#include "nfpcore/nfp_nffw.h"
#include "nfpcore/nfp_hwinfo.h"
#include "nfpcore/nfp_mip.h"
#include "nfpcore/nfp_rtsym.h"
#include "nfpcore/nfp_nsp.h"

#include "nfp_common.h"
#include "nfp_ctrl.h"
#include "nfp_rxtx.h"
#include "nfp_logs.h"
#include "nfp_cpp_bridge.h"

#include "flower/nfp_flower.h"

static int
nfp_net_pf_read_mac(struct nfp_app_fw_nic *app_fw_nic, int port)
{
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_net_hw *hw = NULL;

	/* Grab a pointer to the correct physical port */
	hw = app_fw_nic->ports[port];

	nfp_eth_table = nfp_eth_read_ports(app_fw_nic->pf_dev->cpp);

	nfp_eth_copy_mac((uint8_t *)&hw->mac_addr,
			 (uint8_t *)&nfp_eth_table->ports[port].mac_addr);

	free(nfp_eth_table);
	return 0;
}

static int
nfp_net_start(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint32_t new_ctrl, update = 0;
	struct nfp_net_hw *hw;
	struct nfp_pf_dev *pf_dev;
	struct nfp_app_fw_nic *app_fw_nic;
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	uint32_t intr_vector;
	int ret;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pf_dev = NFP_NET_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(pf_dev->app_fw_priv);

	PMD_INIT_LOG(DEBUG, "Start");

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	/* check and configure queue intr-vector mapping */
	if (dev->data->dev_conf.intr_conf.rxq != 0) {
		if (app_fw_nic->multiport) {
			PMD_INIT_LOG(ERR, "PMD rx interrupt is not supported "
					  "with NFP multiport PF");
				return -EINVAL;
		}
		if (rte_intr_type_get(intr_handle) ==
						RTE_INTR_HANDLE_UIO) {
			/*
			 * Better not to share LSC with RX interrupts.
			 * Unregistering LSC interrupt handler
			 */
			rte_intr_callback_unregister(pci_dev->intr_handle,
				nfp_net_dev_interrupt_handler, (void *)dev);

			if (dev->data->nb_rx_queues > 1) {
				PMD_INIT_LOG(ERR, "PMD rx interrupt only "
					     "supports 1 queue with UIO");
				return -EIO;
			}
		}
		intr_vector = dev->data->nb_rx_queues;
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;

		nfp_configure_rx_interrupt(dev, intr_handle);
		update = NFP_NET_CFG_UPDATE_MSIX;
	}

	/* Checking MTU set */
	if (dev->data->mtu > hw->flbufsz) {
		PMD_INIT_LOG(ERR, "MTU (%u) can't be larger than the current NFP_FRAME_SIZE (%u)",
				dev->data->mtu, hw->flbufsz);
		return -ERANGE;
	}

	rte_intr_enable(intr_handle);

	new_ctrl = nfp_check_offloads(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(hw);

	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;

	if (rxmode->mq_mode & RTE_ETH_MQ_RX_RSS) {
		nfp_net_rss_config_default(dev);
		update |= NFP_NET_CFG_UPDATE_RSS;
		new_ctrl |= nfp_net_cfg_ctrl_rss(hw->cap);
	}

	/* Enable device */
	new_ctrl |= NFP_NET_CFG_CTRL_ENABLE;

	update |= NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING;

	/* Enable vxlan */
	if (hw->cap & NFP_NET_CFG_CTRL_VXLAN) {
		new_ctrl |= NFP_NET_CFG_CTRL_VXLAN;
		update |= NFP_NET_CFG_UPDATE_VXLAN;
	}

	if (hw->cap & NFP_NET_CFG_CTRL_RINGCFG)
		new_ctrl |= NFP_NET_CFG_CTRL_RINGCFG;

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, new_ctrl);
	if (nfp_net_reconfig(hw, new_ctrl, update) < 0)
		return -EIO;

	/*
	 * Allocating rte mbufs for configured rx queues.
	 * This requires queues being enabled before
	 */
	if (nfp_net_rx_freelist_setup(dev) < 0) {
		ret = -ENOMEM;
		goto error;
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port up */
		nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 1);
	else
		nfp_eth_set_configured(dev->process_private,
				       hw->nfp_idx, 1);

	hw->ctrl = new_ctrl;

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

/* Stop device: disable rx and tx functions to allow for reconfiguring. */
static int
nfp_net_stop(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	PMD_INIT_LOG(DEBUG, "Stop");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	nfp_net_disable_queues(dev);

	/* Clear queues */
	nfp_net_stop_tx_queue(dev);

	nfp_net_stop_rx_queue(dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port down */
		nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 0);
	else
		nfp_eth_set_configured(dev->process_private,
				       hw->nfp_idx, 0);

	return 0;
}

/* Set the link up. */
static int
nfp_net_set_link_up(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	PMD_DRV_LOG(DEBUG, "Set link up");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port down */
		return nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 1);
	else
		return nfp_eth_set_configured(dev->process_private,
					      hw->nfp_idx, 1);
}

/* Set the link down. */
static int
nfp_net_set_link_down(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	PMD_DRV_LOG(DEBUG, "Set link down");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port down */
		return nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 0);
	else
		return nfp_eth_set_configured(dev->process_private,
					      hw->nfp_idx, 0);
}

/* Reset and stop device. The device can not be restarted. */
static int
nfp_net_close(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	struct rte_pci_device *pci_dev;
	struct nfp_pf_dev *pf_dev;
	struct nfp_app_fw_nic *app_fw_nic;
	int i;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	PMD_INIT_LOG(DEBUG, "Close");

	pf_dev = NFP_NET_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(pf_dev->app_fw_priv);

	/*
	 * We assume that the DPDK application is stopping all the
	 * threads/queues before calling the device close function.
	 */

	nfp_net_disable_queues(dev);

	/* Clear queues */
	nfp_net_close_tx_queue(dev);

	nfp_net_close_rx_queue(dev);

	/* Cancel possible impending LSC work here before releasing the port*/
	rte_eal_alarm_cancel(nfp_net_dev_interrupt_delayed_handler,
			     (void *)dev);

	/* Only free PF resources after all physical ports have been closed */
	/* Mark this port as unused and free device priv resources*/
	nn_cfg_writeb(hw, NFP_NET_CFG_LSC, 0xff);
	app_fw_nic->ports[hw->idx] = NULL;
	rte_eth_dev_release_port(dev);

	for (i = 0; i < app_fw_nic->total_phyports; i++) {
		/* Check to see if ports are still in use */
		if (app_fw_nic->ports[i])
			return 0;
	}

	/* Now it is safe to free all PF resources */
	PMD_INIT_LOG(INFO, "Freeing PF resources");
	nfp_cpp_area_free(pf_dev->ctrl_area);
	nfp_cpp_area_free(pf_dev->hwqueues_area);
	free(pf_dev->hwinfo);
	free(pf_dev->sym_tbl);
	nfp_cpp_free(pf_dev->cpp);
	rte_free(app_fw_nic);
	rte_free(pf_dev);

	rte_intr_disable(pci_dev->intr_handle);

	/* unregister callback func from eal lib */
	rte_intr_callback_unregister(pci_dev->intr_handle,
			nfp_net_dev_interrupt_handler, (void *)dev);

	/*
	 * The ixgbe PMD disables the pcie master on the
	 * device. The i40e does not...
	 */

	return 0;
}

static int
nfp_net_find_vxlan_idx(struct nfp_net_hw *hw,
		uint16_t port,
		uint32_t *idx)
{
	uint32_t i;
	int free_idx = -1;

	for (i = 0; i < NFP_NET_N_VXLAN_PORTS; i++) {
		if (hw->vxlan_ports[i] == port) {
			free_idx = i;
			break;
		}

		if (hw->vxlan_usecnt[i] == 0) {
			free_idx = i;
			break;
		}
	}

	if (free_idx == -1)
		return -EINVAL;

	*idx = free_idx;

	return 0;
}

static int
nfp_udp_tunnel_port_add(struct rte_eth_dev *dev,
		struct rte_eth_udp_tunnel *tunnel_udp)
{
	int ret;
	uint32_t idx;
	uint16_t vxlan_port;
	struct nfp_net_hw *hw;
	enum rte_eth_tunnel_type tnl_type;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	vxlan_port = tunnel_udp->udp_port;
	tnl_type   = tunnel_udp->prot_type;

	if (tnl_type != RTE_ETH_TUNNEL_TYPE_VXLAN) {
		PMD_DRV_LOG(ERR, "Not VXLAN tunnel");
		return -ENOTSUP;
	}

	ret = nfp_net_find_vxlan_idx(hw, vxlan_port, &idx);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed find valid vxlan idx");
		return -EINVAL;
	}

	if (hw->vxlan_usecnt[idx] == 0) {
		ret = nfp_net_set_vxlan_port(hw, idx, vxlan_port);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Failed set vxlan port");
			return -EINVAL;
		}
	}

	hw->vxlan_usecnt[idx]++;

	return 0;
}

static int
nfp_udp_tunnel_port_del(struct rte_eth_dev *dev,
		struct rte_eth_udp_tunnel *tunnel_udp)
{
	int ret;
	uint32_t idx;
	uint16_t vxlan_port;
	struct nfp_net_hw *hw;
	enum rte_eth_tunnel_type tnl_type;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	vxlan_port = tunnel_udp->udp_port;
	tnl_type   = tunnel_udp->prot_type;

	if (tnl_type != RTE_ETH_TUNNEL_TYPE_VXLAN) {
		PMD_DRV_LOG(ERR, "Not VXLAN tunnel");
		return -ENOTSUP;
	}

	ret = nfp_net_find_vxlan_idx(hw, vxlan_port, &idx);
	if (ret != 0 || hw->vxlan_usecnt[idx] == 0) {
		PMD_DRV_LOG(ERR, "Failed find valid vxlan idx");
		return -EINVAL;
	}

	hw->vxlan_usecnt[idx]--;

	if (hw->vxlan_usecnt[idx] == 0) {
		ret = nfp_net_set_vxlan_port(hw, idx, 0);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Failed set vxlan port");
			return -EINVAL;
		}
	}

	return 0;
}

/* Initialise and register driver with DPDK Application */
static const struct eth_dev_ops nfp_net_eth_dev_ops = {
	.dev_configure		= nfp_net_configure,
	.dev_start		= nfp_net_start,
	.dev_stop		= nfp_net_stop,
	.dev_set_link_up	= nfp_net_set_link_up,
	.dev_set_link_down	= nfp_net_set_link_down,
	.dev_close		= nfp_net_close,
	.promiscuous_enable	= nfp_net_promisc_enable,
	.promiscuous_disable	= nfp_net_promisc_disable,
	.link_update		= nfp_net_link_update,
	.stats_get		= nfp_net_stats_get,
	.stats_reset		= nfp_net_stats_reset,
	.dev_infos_get		= nfp_net_infos_get,
	.dev_supported_ptypes_get = nfp_net_supported_ptypes_get,
	.mtu_set		= nfp_net_dev_mtu_set,
	.mac_addr_set		= nfp_net_set_mac_addr,
	.vlan_offload_set	= nfp_net_vlan_offload_set,
	.reta_update		= nfp_net_reta_update,
	.reta_query		= nfp_net_reta_query,
	.rss_hash_update	= nfp_net_rss_hash_update,
	.rss_hash_conf_get	= nfp_net_rss_hash_conf_get,
	.rx_queue_setup		= nfp_net_rx_queue_setup,
	.rx_queue_release	= nfp_net_rx_queue_release,
	.tx_queue_setup		= nfp_net_tx_queue_setup,
	.tx_queue_release	= nfp_net_tx_queue_release,
	.rx_queue_intr_enable   = nfp_rx_queue_intr_enable,
	.rx_queue_intr_disable  = nfp_rx_queue_intr_disable,
	.udp_tunnel_port_add    = nfp_udp_tunnel_port_add,
	.udp_tunnel_port_del    = nfp_udp_tunnel_port_del,
};

static inline int
nfp_net_ethdev_ops_mount(struct nfp_net_hw *hw, struct rte_eth_dev *eth_dev)
{
	switch (NFD_CFG_CLASS_VER_of(hw->ver)) {
	case NFP_NET_CFG_VERSION_DP_NFD3:
		eth_dev->tx_pkt_burst = &nfp_net_nfd3_xmit_pkts;
		break;
	case NFP_NET_CFG_VERSION_DP_NFDK:
		if (NFD_CFG_MAJOR_VERSION_of(hw->ver) < 5) {
			PMD_DRV_LOG(ERR, "NFDK must use ABI 5 or newer, found: %d",
				NFD_CFG_MAJOR_VERSION_of(hw->ver));
			return -EINVAL;
		}
		eth_dev->tx_pkt_burst = &nfp_net_nfdk_xmit_pkts;
		break;
	default:
		PMD_DRV_LOG(ERR, "The version of firmware is not correct.");
		return -EINVAL;
	}

	eth_dev->dev_ops = &nfp_net_eth_dev_ops;
	eth_dev->rx_queue_count = nfp_net_rx_queue_count;
	eth_dev->rx_pkt_burst = &nfp_net_recv_pkts;

	return 0;
}

static int
nfp_net_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct nfp_pf_dev *pf_dev;
	struct nfp_app_fw_nic *app_fw_nic;
	struct nfp_net_hw *hw;
	struct rte_ether_addr *tmp_ether_addr;
	uint64_t rx_bar_off = 0;
	uint64_t tx_bar_off = 0;
	uint32_t start_q;
	int stride = 4;
	int port = 0;

	PMD_INIT_FUNC_TRACE();

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* Use backpointer here to the PF of this eth_dev */
	pf_dev = NFP_NET_DEV_PRIVATE_TO_PF(eth_dev->data->dev_private);

	/* Use backpointer to the CoreNIC app struct */
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(pf_dev->app_fw_priv);

	port = ((struct nfp_net_hw *)eth_dev->data->dev_private)->idx;
	if (port < 0 || port > 7) {
		PMD_DRV_LOG(ERR, "Port value is wrong");
		return -ENODEV;
	}

	/*
	 * Use PF array of physical ports to get pointer to
	 * this specific port
	 */
	hw = app_fw_nic->ports[port];

	PMD_INIT_LOG(DEBUG, "Working with physical port number: %d, "
			"NFP internal port number: %d", port, hw->nfp_idx);

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	PMD_INIT_LOG(DEBUG, "nfp_net: device (%u:%u) %u:%u:%u:%u",
		     pci_dev->id.vendor_id, pci_dev->id.device_id,
		     pci_dev->addr.domain, pci_dev->addr.bus,
		     pci_dev->addr.devid, pci_dev->addr.function);

	hw->ctrl_bar = (uint8_t *)pci_dev->mem_resource[0].addr;
	if (hw->ctrl_bar == NULL) {
		PMD_DRV_LOG(ERR,
			"hw->ctrl_bar is NULL. BAR0 not configured");
		return -ENODEV;
	}

	if (port == 0) {
		hw->ctrl_bar = pf_dev->ctrl_bar;
	} else {
		if (pf_dev->ctrl_bar == NULL)
			return -ENODEV;
		/* Use port offset in pf ctrl_bar for this ports control bar */
		hw->ctrl_bar = pf_dev->ctrl_bar + (port * NFP_PF_CSR_SLICE_SIZE);
	}

	PMD_INIT_LOG(DEBUG, "ctrl bar: %p", hw->ctrl_bar);

	hw->ver = nn_cfg_readl(hw, NFP_NET_CFG_VERSION);

	if (nfp_net_check_dma_mask(hw, pci_dev->name) != 0)
		return -ENODEV;

	if (nfp_net_ethdev_ops_mount(hw, eth_dev))
		return -EINVAL;

	hw->max_rx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_RXRINGS);
	hw->max_tx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_TXRINGS);

	/* Work out where in the BAR the queues start. */
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_NFP3800_PF_NIC:
	case PCI_DEVICE_ID_NFP4000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_PF_NIC:
		start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_TXQ);
		tx_bar_off = nfp_pci_queue(pci_dev, start_q);
		start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_RXQ);
		rx_bar_off = nfp_pci_queue(pci_dev, start_q);
		break;
	default:
		PMD_DRV_LOG(ERR, "nfp_net: no device ID matching");
		return -ENODEV;
	}

	PMD_INIT_LOG(DEBUG, "tx_bar_off: 0x%" PRIx64 "", tx_bar_off);
	PMD_INIT_LOG(DEBUG, "rx_bar_off: 0x%" PRIx64 "", rx_bar_off);

	hw->tx_bar = pf_dev->hw_queues + tx_bar_off;
	hw->rx_bar = pf_dev->hw_queues + rx_bar_off;
	eth_dev->data->dev_private = hw;

	PMD_INIT_LOG(DEBUG, "ctrl_bar: %p, tx_bar: %p, rx_bar: %p",
		     hw->ctrl_bar, hw->tx_bar, hw->rx_bar);

	nfp_net_cfg_queue_setup(hw);

	/* Get some of the read-only fields from the config BAR */
	hw->cap = nn_cfg_readl(hw, NFP_NET_CFG_CAP);
	hw->max_mtu = nn_cfg_readl(hw, NFP_NET_CFG_MAX_MTU);
	hw->mtu = RTE_ETHER_MTU;
	hw->flbufsz = DEFAULT_FLBUF_SIZE;

	/* VLAN insertion is incompatible with LSOv2 */
	if (hw->cap & NFP_NET_CFG_CTRL_LSO2)
		hw->cap &= ~NFP_NET_CFG_CTRL_TXVLAN;

	nfp_net_init_metadata_format(hw);

	if (NFD_CFG_MAJOR_VERSION_of(hw->ver) < 2)
		hw->rx_offset = NFP_NET_RX_OFFSET;
	else
		hw->rx_offset = nn_cfg_readl(hw, NFP_NET_CFG_RX_OFFSET_ADDR);

	hw->ctrl = 0;

	hw->stride_rx = stride;
	hw->stride_tx = stride;

	nfp_net_log_device_information(hw);

	/* Initializing spinlock for reconfigs */
	rte_spinlock_init(&hw->reconfig_lock);

	/* Allocating memory for mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr",
					       RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to space for MAC address");
		return -ENOMEM;
	}

	nfp_net_pf_read_mac(app_fw_nic, port);
	nfp_net_write_mac(hw, (uint8_t *)&hw->mac_addr);

	tmp_ether_addr = (struct rte_ether_addr *)&hw->mac_addr;
	if (!rte_is_valid_assigned_ether_addr(tmp_ether_addr)) {
		PMD_INIT_LOG(INFO, "Using random mac address for port %d", port);
		/* Using random mac addresses for VFs */
		rte_eth_random_addr(&hw->mac_addr[0]);
		nfp_net_write_mac(hw, (uint8_t *)&hw->mac_addr);
	}

	/* Copying mac address to DPDK eth_dev struct */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac_addr,
			&eth_dev->data->mac_addrs[0]);

	if ((hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR) == 0)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_NOLIVE_MAC_ADDR;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	PMD_INIT_LOG(INFO, "port %d VendorID=0x%x DeviceID=0x%x "
		     "mac=" RTE_ETHER_ADDR_PRT_FMT,
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id,
		     hw->mac_addr[0], hw->mac_addr[1], hw->mac_addr[2],
		     hw->mac_addr[3], hw->mac_addr[4], hw->mac_addr[5]);

	/* Registering LSC interrupt handler */
	rte_intr_callback_register(pci_dev->intr_handle,
			nfp_net_dev_interrupt_handler, (void *)eth_dev);
	/* Telling the firmware about the LSC interrupt entry */
	nn_cfg_writeb(hw, NFP_NET_CFG_LSC, NFP_NET_IRQ_LSC_IDX);
	/* Recording current stats counters values */
	nfp_net_stats_reset(eth_dev);

	return 0;
}

#define DEFAULT_FW_PATH       "/lib/firmware/netronome"

static int
nfp_fw_upload(struct rte_pci_device *dev, struct nfp_nsp *nsp, char *card)
{
	struct nfp_cpp *cpp = nsp->cpp;
	void *fw_buf;
	char fw_name[125];
	char serial[40];
	size_t fsize;

	/* Looking for firmware file in order of priority */

	/* First try to find a firmware image specific for this device */
	snprintf(serial, sizeof(serial),
			"serial-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
		cpp->serial[0], cpp->serial[1], cpp->serial[2], cpp->serial[3],
		cpp->serial[4], cpp->serial[5], cpp->interface >> 8,
		cpp->interface & 0xff);

	snprintf(fw_name, sizeof(fw_name), "%s/%s.nffw", DEFAULT_FW_PATH,
			serial);

	PMD_DRV_LOG(DEBUG, "Trying with fw file: %s", fw_name);
	if (rte_firmware_read(fw_name, &fw_buf, &fsize) == 0)
		goto load_fw;
	/* Then try the PCI name */
	snprintf(fw_name, sizeof(fw_name), "%s/pci-%s.nffw", DEFAULT_FW_PATH,
			dev->name);

	PMD_DRV_LOG(DEBUG, "Trying with fw file: %s", fw_name);
	if (rte_firmware_read(fw_name, &fw_buf, &fsize) == 0)
		goto load_fw;

	/* Finally try the card type and media */
	snprintf(fw_name, sizeof(fw_name), "%s/%s", DEFAULT_FW_PATH, card);
	PMD_DRV_LOG(DEBUG, "Trying with fw file: %s", fw_name);
	if (rte_firmware_read(fw_name, &fw_buf, &fsize) < 0) {
		PMD_DRV_LOG(INFO, "Firmware file %s not found.", fw_name);
		return -ENOENT;
	}

load_fw:
	PMD_DRV_LOG(INFO, "Firmware file found at %s with size: %zu",
		fw_name, fsize);
	PMD_DRV_LOG(INFO, "Uploading the firmware ...");
	nfp_nsp_load_fw(nsp, fw_buf, fsize);
	PMD_DRV_LOG(INFO, "Done");

	free(fw_buf);

	return 0;
}

static int
nfp_fw_setup(struct rte_pci_device *dev,
		struct nfp_cpp *cpp,
		struct nfp_eth_table *nfp_eth_table,
		struct nfp_hwinfo *hwinfo)
{
	struct nfp_nsp *nsp;
	const char *nfp_fw_model;
	char card_desc[100];
	int err = 0;

	nfp_fw_model = nfp_hwinfo_lookup(hwinfo, "nffw.partno");
	if (nfp_fw_model == NULL)
		nfp_fw_model = nfp_hwinfo_lookup(hwinfo, "assembly.partno");

	if (nfp_fw_model) {
		PMD_DRV_LOG(INFO, "firmware model found: %s", nfp_fw_model);
	} else {
		PMD_DRV_LOG(ERR, "firmware model NOT found");
		return -EIO;
	}

	if (nfp_eth_table->count == 0 || nfp_eth_table->count > 8) {
		PMD_DRV_LOG(ERR, "NFP ethernet table reports wrong ports: %u",
			nfp_eth_table->count);
		return -EIO;
	}

	PMD_DRV_LOG(INFO, "NFP ethernet port table reports %u ports",
			nfp_eth_table->count);

	PMD_DRV_LOG(INFO, "Port speed: %u", nfp_eth_table->ports[0].speed);

	snprintf(card_desc, sizeof(card_desc), "nic_%s_%dx%d.nffw",
			nfp_fw_model, nfp_eth_table->count,
			nfp_eth_table->ports[0].speed / 1000);

	nsp = nfp_nsp_open(cpp);
	if (nsp == NULL) {
		PMD_DRV_LOG(ERR, "NFP error when obtaining NSP handle");
		return -EIO;
	}

	nfp_nsp_device_soft_reset(nsp);
	err = nfp_fw_upload(dev, nsp, card_desc);

	nfp_nsp_close(nsp);
	return err;
}

static int
nfp_init_app_fw_nic(struct nfp_pf_dev *pf_dev)
{
	int i;
	int ret;
	int err = 0;
	int total_vnics;
	struct nfp_net_hw *hw;
	unsigned int numa_node;
	struct rte_eth_dev *eth_dev;
	struct nfp_app_fw_nic *app_fw_nic;
	struct nfp_eth_table *nfp_eth_table;
	char port_name[RTE_ETH_NAME_MAX_LEN];

	nfp_eth_table = pf_dev->nfp_eth_table;
	PMD_INIT_LOG(INFO, "Total physical ports: %d", nfp_eth_table->count);

	/* Allocate memory for the CoreNIC app */
	app_fw_nic = rte_zmalloc("nfp_app_fw_nic", sizeof(*app_fw_nic), 0);
	if (app_fw_nic == NULL)
		return -ENOMEM;

	/* Point the app_fw_priv pointer in the PF to the coreNIC app */
	pf_dev->app_fw_priv = app_fw_nic;

	/* Read the number of vNIC's created for the PF */
	total_vnics = nfp_rtsym_read_le(pf_dev->sym_tbl, "nfd_cfg_pf0_num_ports", &err);
	if (err != 0 || total_vnics <= 0 || total_vnics > 8) {
		PMD_INIT_LOG(ERR, "nfd_cfg_pf0_num_ports symbol with wrong value");
		ret = -ENODEV;
		goto app_cleanup;
	}

	/*
	 * For coreNIC the number of vNICs exposed should be the same as the
	 * number of physical ports
	 */
	if (total_vnics != (int)nfp_eth_table->count) {
		PMD_INIT_LOG(ERR, "Total physical ports do not match number of vNICs");
		ret = -ENODEV;
		goto app_cleanup;
	}

	/* Populate coreNIC app properties*/
	app_fw_nic->total_phyports = total_vnics;
	app_fw_nic->pf_dev = pf_dev;
	if (total_vnics > 1)
		app_fw_nic->multiport = true;

	/* Map the symbol table */
	pf_dev->ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, "_pf0_net_bar0",
			app_fw_nic->total_phyports * 32768, &pf_dev->ctrl_area);
	if (pf_dev->ctrl_bar == NULL) {
		PMD_INIT_LOG(ERR, "nfp_rtsym_map fails for _pf0_net_ctrl_bar");
		ret = -EIO;
		goto app_cleanup;
	}

	PMD_INIT_LOG(DEBUG, "ctrl bar: %p", pf_dev->ctrl_bar);

	/* Loop through all physical ports on PF */
	numa_node = rte_socket_id();
	for (i = 0; i < app_fw_nic->total_phyports; i++) {
		snprintf(port_name, sizeof(port_name), "%s_port%d",
			 pf_dev->pci_dev->device.name, i);

		/* Allocate a eth_dev for this phyport */
		eth_dev = rte_eth_dev_allocate(port_name);
		if (eth_dev == NULL) {
			ret = -ENODEV;
			goto port_cleanup;
		}

		/* Allocate memory for this phyport */
		eth_dev->data->dev_private =
			rte_zmalloc_socket(port_name, sizeof(struct nfp_net_hw),
				RTE_CACHE_LINE_SIZE, numa_node);
		if (eth_dev->data->dev_private == NULL) {
			ret = -ENOMEM;
			rte_eth_dev_release_port(eth_dev);
			goto port_cleanup;
		}

		hw = NFP_NET_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

		/* Add this device to the PF's array of physical ports */
		app_fw_nic->ports[i] = hw;

		hw->pf_dev = pf_dev;
		hw->cpp = pf_dev->cpp;
		hw->eth_dev = eth_dev;
		hw->idx = i;
		hw->nfp_idx = nfp_eth_table->ports[i].index;

		eth_dev->device = &pf_dev->pci_dev->device;

		/* ctrl/tx/rx BAR mappings and remaining init happens in
		 * nfp_net_init
		 */
		ret = nfp_net_init(eth_dev);
		if (ret) {
			ret = -ENODEV;
			goto port_cleanup;
		}

		rte_eth_dev_probing_finish(eth_dev);

	} /* End loop, all ports on this PF */

	return 0;

port_cleanup:
	for (i = 0; i < app_fw_nic->total_phyports; i++) {
		if (app_fw_nic->ports[i] && app_fw_nic->ports[i]->eth_dev) {
			struct rte_eth_dev *tmp_dev;
			tmp_dev = app_fw_nic->ports[i]->eth_dev;
			rte_eth_dev_release_port(tmp_dev);
			app_fw_nic->ports[i] = NULL;
		}
	}
	nfp_cpp_area_free(pf_dev->ctrl_area);
app_cleanup:
	rte_free(app_fw_nic);

	return ret;
}

static int
nfp_pf_init(struct rte_pci_device *pci_dev)
{
	int ret;
	int err = 0;
	uint64_t addr;
	struct nfp_cpp *cpp;
	enum nfp_app_fw_id app_fw_id;
	struct nfp_pf_dev *pf_dev;
	struct nfp_hwinfo *hwinfo;
	char name[RTE_ETH_NAME_MAX_LEN];
	struct nfp_rtsym_table *sym_tbl;
	struct nfp_eth_table *nfp_eth_table;

	if (pci_dev == NULL)
		return -ENODEV;

	/*
	 * When device bound to UIO, the device could be used, by mistake,
	 * by two DPDK apps, and the UIO driver does not avoid it. This
	 * could lead to a serious problem when configuring the NFP CPP
	 * interface. Here we avoid this telling to the CPP init code to
	 * use a lock file if UIO is being used.
	 */
	if (pci_dev->kdrv == RTE_PCI_KDRV_VFIO)
		cpp = nfp_cpp_from_device_name(pci_dev, 0);
	else
		cpp = nfp_cpp_from_device_name(pci_dev, 1);

	if (cpp == NULL) {
		PMD_INIT_LOG(ERR, "A CPP handle can not be obtained");
		return -EIO;
	}

	hwinfo = nfp_hwinfo_read(cpp);
	if (hwinfo == NULL) {
		PMD_INIT_LOG(ERR, "Error reading hwinfo table");
		ret = -EIO;
		goto cpp_cleanup;
	}

	/* Read the number of physical ports from hardware */
	nfp_eth_table = nfp_eth_read_ports(cpp);
	if (nfp_eth_table == NULL) {
		PMD_INIT_LOG(ERR, "Error reading NFP ethernet table");
		ret = -EIO;
		goto hwinfo_cleanup;
	}

	if (nfp_fw_setup(pci_dev, cpp, nfp_eth_table, hwinfo)) {
		PMD_INIT_LOG(ERR, "Error when uploading firmware");
		ret = -EIO;
		goto eth_table_cleanup;
	}

	/* Now the symbol table should be there */
	sym_tbl = nfp_rtsym_table_read(cpp);
	if (sym_tbl == NULL) {
		PMD_INIT_LOG(ERR, "Something is wrong with the firmware"
				" symbol table");
		ret = -EIO;
		goto eth_table_cleanup;
	}

	/* Read the app ID of the firmware loaded */
	app_fw_id = nfp_rtsym_read_le(sym_tbl, "_pf0_net_app_id", &err);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Couldn't read app_fw_id from fw");
		ret = -EIO;
		goto sym_tbl_cleanup;
	}

	/* Allocate memory for the PF "device" */
	snprintf(name, sizeof(name), "nfp_pf%d", 0);
	pf_dev = rte_zmalloc(name, sizeof(*pf_dev), 0);
	if (pf_dev == NULL) {
		ret = -ENOMEM;
		goto sym_tbl_cleanup;
	}

	/* Populate the newly created PF device */
	pf_dev->app_fw_id = app_fw_id;
	pf_dev->cpp = cpp;
	pf_dev->hwinfo = hwinfo;
	pf_dev->sym_tbl = sym_tbl;
	pf_dev->pci_dev = pci_dev;
	pf_dev->nfp_eth_table = nfp_eth_table;

	/* configure access to tx/rx vNIC BARs */
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_NFP3800_PF_NIC:
		addr = NFP_PCIE_QUEUE(NFP_PCIE_QCP_NFP3800_OFFSET,
					0, NFP_PCIE_QUEUE_NFP3800_MASK);
		break;
	case PCI_DEVICE_ID_NFP4000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_PF_NIC:
		addr = NFP_PCIE_QUEUE(NFP_PCIE_QCP_NFP6000_OFFSET,
					0, NFP_PCIE_QUEUE_NFP6000_MASK);
		break;
	default:
		PMD_INIT_LOG(ERR, "nfp_net: no device ID matching");
		ret = -ENODEV;
		goto pf_cleanup;
	}

	pf_dev->hw_queues = nfp_cpp_map_area(pf_dev->cpp, 0, 0,
			addr, NFP_QCP_QUEUE_AREA_SZ,
			&pf_dev->hwqueues_area);
	if (pf_dev->hw_queues == NULL) {
		PMD_INIT_LOG(ERR, "nfp_rtsym_map fails for net.qc");
		ret = -EIO;
		goto pf_cleanup;
	}

	PMD_INIT_LOG(DEBUG, "tx/rx bar address: 0x%p", pf_dev->hw_queues);

	/*
	 * PF initialization has been done at this point. Call app specific
	 * init code now
	 */
	switch (pf_dev->app_fw_id) {
	case NFP_APP_FW_CORE_NIC:
		PMD_INIT_LOG(INFO, "Initializing coreNIC");
		ret = nfp_init_app_fw_nic(pf_dev);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Could not initialize coreNIC!");
			goto hwqueues_cleanup;
		}
		break;
	case NFP_APP_FW_FLOWER_NIC:
		PMD_INIT_LOG(INFO, "Initializing Flower");
		ret = nfp_init_app_fw_flower(pf_dev);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Could not initialize Flower!");
			goto hwqueues_cleanup;
		}
		break;
	default:
		PMD_INIT_LOG(ERR, "Unsupported Firmware loaded");
		ret = -EINVAL;
		goto hwqueues_cleanup;
	}

	/* register the CPP bridge service here for primary use */
	ret = nfp_enable_cpp_service(pf_dev);
	if (ret != 0)
		PMD_INIT_LOG(INFO, "Enable cpp service failed.");

	return 0;

hwqueues_cleanup:
	nfp_cpp_area_free(pf_dev->hwqueues_area);
pf_cleanup:
	rte_free(pf_dev);
sym_tbl_cleanup:
	free(sym_tbl);
eth_table_cleanup:
	free(nfp_eth_table);
hwinfo_cleanup:
	free(hwinfo);
cpp_cleanup:
	nfp_cpp_free(cpp);

	return ret;
}

static int
nfp_secondary_init_app_fw_nic(struct rte_pci_device *pci_dev,
		struct nfp_rtsym_table *sym_tbl,
		struct nfp_cpp *cpp)
{
	int i;
	int err = 0;
	int ret = 0;
	int total_vnics;
	struct nfp_net_hw *hw;

	/* Read the number of vNIC's created for the PF */
	total_vnics = nfp_rtsym_read_le(sym_tbl, "nfd_cfg_pf0_num_ports", &err);
	if (err != 0 || total_vnics <= 0 || total_vnics > 8) {
		PMD_INIT_LOG(ERR, "nfd_cfg_pf0_num_ports symbol with wrong value");
		return -ENODEV;
	}

	for (i = 0; i < total_vnics; i++) {
		struct rte_eth_dev *eth_dev;
		char port_name[RTE_ETH_NAME_MAX_LEN];
		snprintf(port_name, sizeof(port_name), "%s_port%d",
				pci_dev->device.name, i);

		PMD_INIT_LOG(DEBUG, "Secondary attaching to port %s", port_name);
		eth_dev = rte_eth_dev_attach_secondary(port_name);
		if (eth_dev == NULL) {
			PMD_INIT_LOG(ERR, "Secondary process attach to port %s failed", port_name);
			ret = -ENODEV;
			break;
		}

		eth_dev->process_private = cpp;
		hw = NFP_NET_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
		if (nfp_net_ethdev_ops_mount(hw, eth_dev))
			return -EINVAL;

		rte_eth_dev_probing_finish(eth_dev);
	}

	return ret;
}

/*
 * When attaching to the NFP4000/6000 PF on a secondary process there
 * is no need to initialise the PF again. Only minimal work is required
 * here
 */
static int
nfp_pf_secondary_init(struct rte_pci_device *pci_dev)
{
	int err = 0;
	int ret = 0;
	struct nfp_cpp *cpp;
	enum nfp_app_fw_id app_fw_id;
	struct nfp_rtsym_table *sym_tbl;

	if (pci_dev == NULL)
		return -ENODEV;

	/*
	 * When device bound to UIO, the device could be used, by mistake,
	 * by two DPDK apps, and the UIO driver does not avoid it. This
	 * could lead to a serious problem when configuring the NFP CPP
	 * interface. Here we avoid this telling to the CPP init code to
	 * use a lock file if UIO is being used.
	 */
	if (pci_dev->kdrv == RTE_PCI_KDRV_VFIO)
		cpp = nfp_cpp_from_device_name(pci_dev, 0);
	else
		cpp = nfp_cpp_from_device_name(pci_dev, 1);

	if (cpp == NULL) {
		PMD_INIT_LOG(ERR, "A CPP handle can not be obtained");
		return -EIO;
	}

	/*
	 * We don't have access to the PF created in the primary process
	 * here so we have to read the number of ports from firmware
	 */
	sym_tbl = nfp_rtsym_table_read(cpp);
	if (sym_tbl == NULL) {
		PMD_INIT_LOG(ERR, "Something is wrong with the firmware"
				" symbol table");
		return -EIO;
	}

	/* Read the app ID of the firmware loaded */
	app_fw_id = nfp_rtsym_read_le(sym_tbl, "_pf0_net_app_id", &err);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Couldn't read app_fw_id from fw");
		goto sym_tbl_cleanup;
	}

	switch (app_fw_id) {
	case NFP_APP_FW_CORE_NIC:
		PMD_INIT_LOG(INFO, "Initializing coreNIC");
		ret = nfp_secondary_init_app_fw_nic(pci_dev, sym_tbl, cpp);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Could not initialize coreNIC!");
			goto sym_tbl_cleanup;
		}
		break;
	case NFP_APP_FW_FLOWER_NIC:
		PMD_INIT_LOG(INFO, "Initializing Flower");
		ret = nfp_secondary_init_app_fw_flower(cpp);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Could not initialize Flower!");
			goto sym_tbl_cleanup;
		}
		break;
	default:
		PMD_INIT_LOG(ERR, "Unsupported Firmware loaded");
		ret = -EINVAL;
		goto sym_tbl_cleanup;
	}

sym_tbl_cleanup:
	free(sym_tbl);

	return ret;
}

static int
nfp_pf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *dev)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		return nfp_pf_init(dev);
	else
		return nfp_pf_secondary_init(dev);
}

static const struct rte_pci_id pci_id_nfp_pf_net_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETRONOME,
			       PCI_DEVICE_ID_NFP3800_PF_NIC)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETRONOME,
			       PCI_DEVICE_ID_NFP4000_PF_NIC)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETRONOME,
			       PCI_DEVICE_ID_NFP6000_PF_NIC)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CORIGINE,
			       PCI_DEVICE_ID_NFP3800_PF_NIC)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CORIGINE,
			       PCI_DEVICE_ID_NFP4000_PF_NIC)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CORIGINE,
			       PCI_DEVICE_ID_NFP6000_PF_NIC)
	},
	{
		.vendor_id = 0,
	},
};

static int
nfp_pci_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	uint16_t port_id;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* Free up all physical ports under PF */
	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device)
		rte_eth_dev_close(port_id);
	/*
	 * Ports can be closed and freed but hotplugging is not
	 * currently supported
	 */
	return -ENOTSUP;
}

static int
eth_nfp_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, nfp_pci_uninit);
}

static struct rte_pci_driver rte_nfp_net_pf_pmd = {
	.id_table = pci_id_nfp_pf_net_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = nfp_pf_pci_probe,
	.remove = eth_nfp_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_nfp_pf, rte_nfp_net_pf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_nfp_pf, pci_id_nfp_pf_net_map);
RTE_PMD_REGISTER_KMOD_DEP(net_nfp_pf, "* igb_uio | uio_pci_generic | vfio");
/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
