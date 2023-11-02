/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

#include <unistd.h>

#include <eal_firmware.h>
#include <rte_alarm.h>

#include "flower/nfp_flower.h"
#include "nfd3/nfp_nfd3.h"
#include "nfdk/nfp_nfdk.h"
#include "nfpcore/nfp_cpp.h"
#include "nfpcore/nfp_hwinfo.h"
#include "nfpcore/nfp_rtsym.h"
#include "nfpcore/nfp_nsp.h"
#include "nfpcore/nfp6000_pcie.h"
#include "nfpcore/nfp_resource.h"

#include "nfp_cpp_bridge.h"
#include "nfp_ipsec.h"
#include "nfp_logs.h"

#define NFP_PF_DRIVER_NAME net_nfp_pf

static void
nfp_net_pf_read_mac(struct nfp_app_fw_nic *app_fw_nic,
		uint16_t port)
{
	struct nfp_net_hw *hw;
	struct nfp_eth_table *nfp_eth_table;

	/* Grab a pointer to the correct physical port */
	hw = app_fw_nic->ports[port];

	nfp_eth_table = app_fw_nic->pf_dev->nfp_eth_table;

	rte_ether_addr_copy(&nfp_eth_table->ports[port].mac_addr, &hw->super.mac_addr);
}

static int
nfp_net_start(struct rte_eth_dev *dev)
{
	int ret;
	uint16_t i;
	struct nfp_hw *hw;
	uint32_t new_ctrl;
	uint32_t update = 0;
	uint32_t cap_extend;
	uint32_t intr_vector;
	uint32_t ctrl_extend = 0;
	struct nfp_net_hw *net_hw;
	struct nfp_pf_dev *pf_dev;
	struct rte_eth_rxmode *rxmode;
	struct nfp_app_fw_nic *app_fw_nic;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	net_hw = dev->data->dev_private;
	pf_dev = net_hw->pf_dev;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(pf_dev->app_fw_priv);
	hw = &net_hw->super;

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	/* Check and configure queue intr-vector mapping */
	if (dev->data->dev_conf.intr_conf.rxq != 0) {
		if (app_fw_nic->multiport) {
			PMD_INIT_LOG(ERR, "PMD rx interrupt is not supported "
					"with NFP multiport PF");
				return -EINVAL;
		}

		if (rte_intr_type_get(intr_handle) == RTE_INTR_HANDLE_UIO) {
			/*
			 * Better not to share LSC with RX interrupts.
			 * Unregistering LSC interrupt handler.
			 */
			rte_intr_callback_unregister(intr_handle,
					nfp_net_dev_interrupt_handler, (void *)dev);

			if (dev->data->nb_rx_queues > 1) {
				PMD_INIT_LOG(ERR, "PMD rx interrupt only "
						"supports 1 queue with UIO");
				return -EIO;
			}
		}

		intr_vector = dev->data->nb_rx_queues;
		if (rte_intr_efd_enable(intr_handle, intr_vector) != 0)
			return -1;

		nfp_configure_rx_interrupt(dev, intr_handle);
		update = NFP_NET_CFG_UPDATE_MSIX;
	}

	/* Checking MTU set */
	if (dev->data->mtu > net_hw->flbufsz) {
		PMD_INIT_LOG(ERR, "MTU (%u) can't be larger than the current NFP_FRAME_SIZE (%u)",
				dev->data->mtu, net_hw->flbufsz);
		return -ERANGE;
	}

	rte_intr_enable(intr_handle);

	new_ctrl = nfp_check_offloads(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(net_hw);

	rxmode = &dev->data->dev_conf.rxmode;
	if ((rxmode->mq_mode & RTE_ETH_MQ_RX_RSS) != 0) {
		nfp_net_rss_config_default(dev);
		update |= NFP_NET_CFG_UPDATE_RSS;
		new_ctrl |= nfp_net_cfg_ctrl_rss(hw->cap);
	}

	/* Enable device */
	new_ctrl |= NFP_NET_CFG_CTRL_ENABLE;

	update |= NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING;

	/* Enable vxlan */
	if ((hw->cap & NFP_NET_CFG_CTRL_VXLAN) != 0) {
		new_ctrl |= NFP_NET_CFG_CTRL_VXLAN;
		update |= NFP_NET_CFG_UPDATE_VXLAN;
	}

	if ((hw->cap & NFP_NET_CFG_CTRL_RINGCFG) != 0)
		new_ctrl |= NFP_NET_CFG_CTRL_RINGCFG;

	if (nfp_reconfig(hw, new_ctrl, update) != 0)
		return -EIO;

	hw->ctrl = new_ctrl;

	/* Enable packet type offload by extend ctrl word1. */
	cap_extend = hw->cap_ext;
	if ((cap_extend & NFP_NET_CFG_CTRL_PKT_TYPE) != 0)
		ctrl_extend = NFP_NET_CFG_CTRL_PKT_TYPE;

	if ((cap_extend & NFP_NET_CFG_CTRL_IPSEC) != 0)
		ctrl_extend |= NFP_NET_CFG_CTRL_IPSEC_SM_LOOKUP
				| NFP_NET_CFG_CTRL_IPSEC_LM_LOOKUP;

	update = NFP_NET_CFG_UPDATE_GEN;
	if (nfp_ext_reconfig(hw, ctrl_extend, update) != 0)
		return -EIO;

	hw->ctrl_ext = ctrl_extend;

	/*
	 * Allocating rte mbufs for configured rx queues.
	 * This requires queues being enabled before.
	 */
	if (nfp_net_rx_freelist_setup(dev) != 0) {
		ret = -ENOMEM;
		goto error;
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port up */
		nfp_eth_set_configured(net_hw->cpp, net_hw->nfp_idx, 1);
	else
		nfp_eth_set_configured(dev->process_private, net_hw->nfp_idx, 1);

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

/* Set the link up. */
static int
nfp_net_set_link_up(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	hw = dev->data->dev_private;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port down */
		return nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 1);
	else
		return nfp_eth_set_configured(dev->process_private, hw->nfp_idx, 1);
}

/* Set the link down. */
static int
nfp_net_set_link_down(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	hw = dev->data->dev_private;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port down */
		return nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 0);
	else
		return nfp_eth_set_configured(dev->process_private, hw->nfp_idx, 0);
}

static uint8_t
nfp_function_id_get(const struct nfp_pf_dev *pf_dev,
		uint8_t phy_port)
{
	if (pf_dev->multi_pf.enabled)
		return pf_dev->multi_pf.function_id;

	return phy_port;
}

static void
nfp_net_beat_timer(void *arg)
{
	uint64_t cur_sec;
	struct nfp_multi_pf *multi_pf = arg;

	cur_sec = rte_rdtsc();
	nn_writeq(cur_sec, multi_pf->beat_addr + NFP_BEAT_OFFSET(multi_pf->function_id));

	/* Beat once per second. */
	if (rte_eal_alarm_set(1000 * 1000, nfp_net_beat_timer,
			(void *)multi_pf) < 0) {
		PMD_DRV_LOG(ERR, "Error setting alarm");
	}
}

static int
nfp_net_keepalive_init(struct nfp_cpp *cpp,
		struct nfp_multi_pf *multi_pf)
{
	uint8_t *base;
	uint64_t addr;
	uint32_t size;
	uint32_t cpp_id;
	struct nfp_resource *res;

	res = nfp_resource_acquire(cpp, NFP_RESOURCE_KEEPALIVE);
	if (res == NULL)
		return -EIO;

	cpp_id = nfp_resource_cpp_id(res);
	addr = nfp_resource_address(res);
	size = nfp_resource_size(res);

	nfp_resource_release(res);

	/* Allocate a fixed area for keepalive. */
	base = nfp_cpp_map_area(cpp, cpp_id, addr, size, &multi_pf->beat_area);
	if (base == NULL) {
		PMD_DRV_LOG(ERR, "Failed to map area for keepalive.");
		return -EIO;
	}

	multi_pf->beat_addr = base;

	return 0;
}

static void
nfp_net_keepalive_uninit(struct nfp_multi_pf *multi_pf)
{
	nfp_cpp_area_release_free(multi_pf->beat_area);
}

static int
nfp_net_keepalive_start(struct nfp_multi_pf *multi_pf)
{
	if (rte_eal_alarm_set(1000 * 1000, nfp_net_beat_timer,
			(void *)multi_pf) < 0) {
		PMD_DRV_LOG(ERR, "Error setting alarm");
		return -EIO;
	}

	return 0;
}

static void
nfp_net_keepalive_stop(struct nfp_multi_pf *multi_pf)
{
	/* Cancel keepalive for multiple PF setup */
	rte_eal_alarm_cancel(nfp_net_beat_timer, (void *)multi_pf);
}

/* Reset and stop device. The device can not be restarted. */
static int
nfp_net_close(struct rte_eth_dev *dev)
{
	uint8_t i;
	uint8_t id;
	struct nfp_net_hw *hw;
	struct nfp_pf_dev *pf_dev;
	struct rte_pci_device *pci_dev;
	struct nfp_app_fw_nic *app_fw_nic;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	hw = dev->data->dev_private;
	pf_dev = hw->pf_dev;
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

	/* Clear ipsec */
	nfp_ipsec_uninit(dev);

	/* Cancel possible impending LSC work here before releasing the port */
	rte_eal_alarm_cancel(nfp_net_dev_interrupt_delayed_handler, (void *)dev);

	/* Only free PF resources after all physical ports have been closed */
	/* Mark this port as unused and free device priv resources */
	nn_cfg_writeb(&hw->super, NFP_NET_CFG_LSC, 0xff);
	app_fw_nic->ports[hw->idx] = NULL;

	for (i = 0; i < app_fw_nic->total_phyports; i++) {
		id = nfp_function_id_get(pf_dev, i);

		/* Check to see if ports are still in use */
		if (app_fw_nic->ports[id] != NULL)
			return 0;
	}

	/* Now it is safe to free all PF resources */
	PMD_INIT_LOG(INFO, "Freeing PF resources");
	if (pf_dev->multi_pf.enabled) {
		nfp_net_keepalive_stop(&pf_dev->multi_pf);
		nfp_net_keepalive_uninit(&pf_dev->multi_pf);
	}
	nfp_cpp_area_free(pf_dev->ctrl_area);
	nfp_cpp_area_free(pf_dev->qc_area);
	free(pf_dev->hwinfo);
	free(pf_dev->sym_tbl);
	nfp_cpp_free(pf_dev->cpp);
	rte_free(app_fw_nic);
	rte_free(pf_dev);

	rte_intr_disable(pci_dev->intr_handle);

	/* Unregister callback func from eal lib */
	rte_intr_callback_unregister(pci_dev->intr_handle,
			nfp_net_dev_interrupt_handler, (void *)dev);

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

	hw = dev->data->dev_private;
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

	hw = dev->data->dev_private;
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
	.dev_configure          = nfp_net_configure,
	.dev_start              = nfp_net_start,
	.dev_stop               = nfp_net_stop,
	.dev_set_link_up        = nfp_net_set_link_up,
	.dev_set_link_down      = nfp_net_set_link_down,
	.dev_close              = nfp_net_close,
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
	.udp_tunnel_port_add    = nfp_udp_tunnel_port_add,
	.udp_tunnel_port_del    = nfp_udp_tunnel_port_del,
	.fw_version_get         = nfp_net_firmware_version_get,
	.flow_ctrl_get          = nfp_net_flow_ctrl_get,
	.flow_ctrl_set          = nfp_net_flow_ctrl_set,
};

static inline void
nfp_net_ethdev_ops_mount(struct nfp_net_hw *hw,
		struct rte_eth_dev *eth_dev)
{
	if (hw->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3)
		eth_dev->tx_pkt_burst = nfp_net_nfd3_xmit_pkts;
	else
		eth_dev->tx_pkt_burst = nfp_net_nfdk_xmit_pkts;

	eth_dev->dev_ops = &nfp_net_eth_dev_ops;
	eth_dev->rx_queue_count = nfp_net_rx_queue_count;
	eth_dev->rx_pkt_burst = &nfp_net_recv_pkts;
}

static int
nfp_net_init(struct rte_eth_dev *eth_dev)
{
	int err;
	uint16_t port;
	uint64_t rx_base;
	uint64_t tx_base;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;
	struct nfp_pf_dev *pf_dev;
	struct rte_pci_device *pci_dev;
	struct nfp_app_fw_nic *app_fw_nic;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	net_hw = eth_dev->data->dev_private;

	/* Use backpointer here to the PF of this eth_dev */
	pf_dev = net_hw->pf_dev;

	/* Use backpointer to the CoreNIC app struct */
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(pf_dev->app_fw_priv);

	port = ((struct nfp_net_hw *)eth_dev->data->dev_private)->idx;
	if (port > 7) {
		PMD_DRV_LOG(ERR, "Port value is wrong");
		return -ENODEV;
	}

	hw = &net_hw->super;

	PMD_INIT_LOG(DEBUG, "Working with physical port number: %hu, "
			"NFP internal port number: %d", port, net_hw->nfp_idx);

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	if (port == 0 || pf_dev->multi_pf.enabled) {
		uint32_t min_size;

		hw->ctrl_bar = pf_dev->ctrl_bar;
		min_size = NFP_MAC_STATS_SIZE * net_hw->pf_dev->nfp_eth_table->max_index;
		net_hw->mac_stats_bar = nfp_rtsym_map(net_hw->pf_dev->sym_tbl, "_mac_stats",
				min_size, &net_hw->mac_stats_area);
		if (net_hw->mac_stats_bar == NULL) {
			PMD_INIT_LOG(ERR, "nfp_rtsym_map fails for _mac_stats_bar");
			return -EIO;
		}

		net_hw->mac_stats = net_hw->mac_stats_bar;
	} else {
		if (pf_dev->ctrl_bar == NULL)
			return -ENODEV;

		/* Use port offset in pf ctrl_bar for this ports control bar */
		hw->ctrl_bar = pf_dev->ctrl_bar + (port * NFP_NET_CFG_BAR_SZ);
		net_hw->mac_stats = app_fw_nic->ports[0]->mac_stats_bar +
				(net_hw->nfp_idx * NFP_MAC_STATS_SIZE);
	}

	PMD_INIT_LOG(DEBUG, "ctrl bar: %p", hw->ctrl_bar);
	PMD_INIT_LOG(DEBUG, "MAC stats: %p", net_hw->mac_stats);

	err = nfp_net_common_init(pci_dev, net_hw);
	if (err != 0)
		return err;

	err = nfp_net_tlv_caps_parse(eth_dev);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Failed to parser TLV caps");
		return err;
	}

	err = nfp_ipsec_init(eth_dev);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Failed to init IPsec module");
		return err;
	}

	nfp_net_ethdev_ops_mount(net_hw, eth_dev);

	net_hw->eth_xstats_base = rte_malloc("rte_eth_xstat", sizeof(struct rte_eth_xstat) *
			nfp_net_xstats_size(eth_dev), 0);
	if (net_hw->eth_xstats_base == NULL) {
		PMD_INIT_LOG(ERR, "no memory for xstats base values on device %s!",
				pci_dev->device.name);
		return -ENOMEM;
	}

	/* Work out where in the BAR the queues start. */
	tx_base = nn_cfg_readl(hw, NFP_NET_CFG_START_TXQ);
	rx_base = nn_cfg_readl(hw, NFP_NET_CFG_START_RXQ);

	net_hw->tx_bar = pf_dev->qc_bar + tx_base * NFP_QCP_QUEUE_ADDR_SZ;
	net_hw->rx_bar = pf_dev->qc_bar + rx_base * NFP_QCP_QUEUE_ADDR_SZ;
	eth_dev->data->dev_private = net_hw;

	PMD_INIT_LOG(DEBUG, "ctrl_bar: %p, tx_bar: %p, rx_bar: %p",
			hw->ctrl_bar, net_hw->tx_bar, net_hw->rx_bar);

	nfp_net_cfg_queue_setup(net_hw);
	net_hw->mtu = RTE_ETHER_MTU;

	/* VLAN insertion is incompatible with LSOv2 */
	if ((hw->cap & NFP_NET_CFG_CTRL_LSO2) != 0)
		hw->cap &= ~NFP_NET_CFG_CTRL_TXVLAN;

	nfp_net_log_device_information(net_hw);

	/* Initializing spinlock for reconfigs */
	rte_spinlock_init(&hw->reconfig_lock);

	/* Allocating memory for mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to space for MAC address");
		return -ENOMEM;
	}

	nfp_net_pf_read_mac(app_fw_nic, port);
	nfp_write_mac(hw, &hw->mac_addr.addr_bytes[0]);

	if (rte_is_valid_assigned_ether_addr(&hw->mac_addr) == 0) {
		PMD_INIT_LOG(INFO, "Using random mac address for port %d", port);
		/* Using random mac addresses for VFs */
		rte_eth_random_addr(&hw->mac_addr.addr_bytes[0]);
		nfp_write_mac(hw, &hw->mac_addr.addr_bytes[0]);
	}

	/* Copying mac address to DPDK eth_dev struct */
	rte_ether_addr_copy(&hw->mac_addr, eth_dev->data->mac_addrs);

	if ((hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR) == 0)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_NOLIVE_MAC_ADDR;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	PMD_INIT_LOG(INFO, "port %d VendorID=%#x DeviceID=%#x "
			"mac=" RTE_ETHER_ADDR_PRT_FMT,
			eth_dev->data->port_id, pci_dev->id.vendor_id,
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
}

#define DEFAULT_FW_PATH       "/lib/firmware/netronome"

static int
nfp_fw_upload(struct rte_pci_device *dev,
		struct nfp_nsp *nsp,
		char *card)
{
	void *fw_buf;
	size_t fsize;
	char serial[40];
	char fw_name[125];
	uint16_t interface;
	uint32_t cpp_serial_len;
	const uint8_t *cpp_serial;
	struct nfp_cpp *cpp = nfp_nsp_cpp(nsp);

	cpp_serial_len = nfp_cpp_serial(cpp, &cpp_serial);
	if (cpp_serial_len != NFP_SERIAL_LEN)
		return -ERANGE;

	interface = nfp_cpp_interface(cpp);

	/* Looking for firmware file in order of priority */

	/* First try to find a firmware image specific for this device */
	snprintf(serial, sizeof(serial),
			"serial-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
			cpp_serial[0], cpp_serial[1], cpp_serial[2], cpp_serial[3],
			cpp_serial[4], cpp_serial[5], interface >> 8, interface & 0xff);
	snprintf(fw_name, sizeof(fw_name), "%s/%s.nffw", DEFAULT_FW_PATH, serial);

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
	if (rte_firmware_read(fw_name, &fw_buf, &fsize) == 0)
		goto load_fw;

	PMD_DRV_LOG(ERR, "Can't find suitable firmware.");
	return -ENOENT;

load_fw:
	PMD_DRV_LOG(INFO, "Firmware file found at %s with size: %zu",
			fw_name, fsize);
	PMD_DRV_LOG(INFO, "Uploading the firmware ...");
	if (nfp_nsp_load_fw(nsp, fw_buf, fsize) < 0) {
		free(fw_buf);
		PMD_DRV_LOG(ERR, "Firmware load failed.");
		return -EIO;
	}

	PMD_DRV_LOG(INFO, "Done");

	free(fw_buf);

	return 0;
}

static void
nfp_fw_unload(struct nfp_cpp *cpp)
{
	struct nfp_nsp *nsp;

	nsp = nfp_nsp_open(cpp);
	if (nsp == NULL)
		return;

	nfp_nsp_device_soft_reset(nsp);
	nfp_nsp_close(nsp);
}

static int
nfp_fw_reload(struct rte_pci_device *dev,
		struct nfp_nsp *nsp,
		char *card_desc)
{
	int err;

	nfp_nsp_device_soft_reset(nsp);
	err = nfp_fw_upload(dev, nsp, card_desc);
	if (err != 0)
		PMD_DRV_LOG(ERR, "NFP firmware load failed");

	return err;
}

static int
nfp_fw_loaded_check_alive(struct rte_pci_device *dev,
		struct nfp_nsp *nsp,
		char *card_desc,
		const struct nfp_dev_info *dev_info,
		struct nfp_multi_pf *multi_pf)
{
	int offset;
	uint32_t i;
	uint64_t beat;
	uint32_t port_num;

	/*
	 * If the beats of any other port changed in 3s,
	 * we should not reload the firmware.
	 */
	for (port_num = 0; port_num < dev_info->pf_num_per_unit; port_num++) {
		if (port_num == multi_pf->function_id)
			continue;

		offset = NFP_BEAT_OFFSET(port_num);
		beat = nn_readq(multi_pf->beat_addr + offset);
		for (i = 0; i < 3; i++) {
			sleep(1);
			if (nn_readq(multi_pf->beat_addr + offset) != beat)
				return 0;
		}
	}

	return nfp_fw_reload(dev, nsp, card_desc);
}

static int
nfp_fw_reload_for_multipf(struct rte_pci_device *dev,
		struct nfp_nsp *nsp,
		char *card_desc,
		struct nfp_cpp *cpp,
		const struct nfp_dev_info *dev_info,
		struct nfp_multi_pf *multi_pf)
{
	int err;

	err = nfp_net_keepalive_init(cpp, multi_pf);
	if (err != 0)
		PMD_DRV_LOG(ERR, "NFP write beat failed");

	if (nfp_nsp_fw_loaded(nsp))
		err = nfp_fw_loaded_check_alive(dev, nsp, card_desc, dev_info, multi_pf);
	else
		err = nfp_fw_reload(dev, nsp, card_desc);
	if (err != 0) {
		nfp_net_keepalive_uninit(multi_pf);
		return err;
	}

	err = nfp_net_keepalive_start(multi_pf);
	if (err != 0) {
		nfp_net_keepalive_uninit(multi_pf);
		PMD_DRV_LOG(ERR, "NFP write beat failed");
	}

	return err;
}

static int
nfp_fw_setup(struct rte_pci_device *dev,
		struct nfp_cpp *cpp,
		struct nfp_eth_table *nfp_eth_table,
		struct nfp_hwinfo *hwinfo,
		const struct nfp_dev_info *dev_info,
		struct nfp_multi_pf *multi_pf)
{
	int err;
	char card_desc[100];
	struct nfp_nsp *nsp;
	const char *nfp_fw_model;

	nfp_fw_model = nfp_hwinfo_lookup(hwinfo, "nffw.partno");
	if (nfp_fw_model == NULL)
		nfp_fw_model = nfp_hwinfo_lookup(hwinfo, "assembly.partno");

	if (nfp_fw_model != NULL) {
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

	if (multi_pf->enabled)
		err = nfp_fw_reload_for_multipf(dev, nsp, card_desc, cpp, dev_info, multi_pf);
	else
		err = nfp_fw_reload(dev, nsp, card_desc);

	nfp_nsp_close(nsp);
	return err;
}

static inline bool
nfp_check_multi_pf_from_fw(uint32_t total_vnics)
{
	if (total_vnics == 1)
		return true;

	return false;
}

static inline bool
nfp_check_multi_pf_from_nsp(struct rte_pci_device *pci_dev,
		struct nfp_cpp *cpp)
{
	bool flag;
	struct nfp_nsp *nsp;

	nsp = nfp_nsp_open(cpp);
	if (nsp == NULL) {
		PMD_DRV_LOG(ERR, "NFP error when obtaining NSP handle");
		return false;
	}

	flag = (nfp_nsp_get_abi_ver_major(nsp) > 0) &&
			(pci_dev->id.device_id == PCI_DEVICE_ID_NFP3800_PF_NIC);

	nfp_nsp_close(nsp);
	return flag;
}

static int
nfp_enable_multi_pf(struct nfp_pf_dev *pf_dev)
{
	int err = 0;
	uint64_t tx_base;
	uint8_t *ctrl_bar;
	struct nfp_hw *hw;
	uint32_t cap_extend;
	struct nfp_net_hw net_hw;
	struct nfp_cpp_area *area;
	char name[RTE_ETH_NAME_MAX_LEN];

	memset(&net_hw, 0, sizeof(struct nfp_net_hw));

	/* Map the symbol table */
	snprintf(name, sizeof(name), "_pf%u_net_bar0",
			pf_dev->multi_pf.function_id);
	ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, name, NFP_NET_CFG_BAR_SZ,
			&area);
	if (ctrl_bar == NULL) {
		PMD_INIT_LOG(ERR, "Failed to find data vNIC memory symbol");
		return -ENODEV;
	}

	hw = &net_hw.super;
	hw->ctrl_bar = ctrl_bar;

	cap_extend = nn_cfg_readl(hw, NFP_NET_CFG_CAP_WORD1);
	if ((cap_extend & NFP_NET_CFG_CTRL_MULTI_PF) == 0) {
		PMD_INIT_LOG(ERR, "Loaded firmware doesn't support multiple PF");
		err = -EINVAL;
		goto end;
	}

	tx_base = nn_cfg_readl(hw, NFP_NET_CFG_START_TXQ);
	net_hw.tx_bar = pf_dev->qc_bar + tx_base * NFP_QCP_QUEUE_ADDR_SZ;
	nfp_net_cfg_queue_setup(&net_hw);
	rte_spinlock_init(&hw->reconfig_lock);
	nfp_ext_reconfig(&net_hw.super, NFP_NET_CFG_CTRL_MULTI_PF, NFP_NET_CFG_UPDATE_GEN);
end:
	nfp_cpp_area_release_free(area);
	return err;
}

static int
nfp_init_app_fw_nic(struct nfp_pf_dev *pf_dev,
		const struct nfp_dev_info *dev_info)
{
	uint8_t i;
	uint8_t id;
	int ret = 0;
	uint32_t total_vnics;
	struct nfp_net_hw *hw;
	unsigned int numa_node;
	struct rte_eth_dev *eth_dev;
	struct nfp_app_fw_nic *app_fw_nic;
	struct nfp_eth_table *nfp_eth_table;
	char bar_name[RTE_ETH_NAME_MAX_LEN];
	char port_name[RTE_ETH_NAME_MAX_LEN];
	char vnic_name[RTE_ETH_NAME_MAX_LEN];

	nfp_eth_table = pf_dev->nfp_eth_table;
	PMD_INIT_LOG(INFO, "Total physical ports: %d", nfp_eth_table->count);
	id = nfp_function_id_get(pf_dev, 0);

	/* Allocate memory for the CoreNIC app */
	app_fw_nic = rte_zmalloc("nfp_app_fw_nic", sizeof(*app_fw_nic), 0);
	if (app_fw_nic == NULL)
		return -ENOMEM;

	/* Point the app_fw_priv pointer in the PF to the coreNIC app */
	pf_dev->app_fw_priv = app_fw_nic;

	/* Read the number of vNIC's created for the PF */
	snprintf(vnic_name, sizeof(vnic_name), "nfd_cfg_pf%u_num_ports", id);
	total_vnics = nfp_rtsym_read_le(pf_dev->sym_tbl, vnic_name, &ret);
	if (ret != 0 || total_vnics == 0 || total_vnics > 8) {
		PMD_INIT_LOG(ERR, "%s symbol with wrong value", vnic_name);
		ret = -ENODEV;
		goto app_cleanup;
	}

	if (pf_dev->multi_pf.enabled) {
		if (!nfp_check_multi_pf_from_fw(total_vnics)) {
			PMD_INIT_LOG(ERR, "NSP report multipf, but FW report not multipf");
			ret = -ENODEV;
			goto app_cleanup;
		}
	} else {
		/*
		 * For coreNIC the number of vNICs exposed should be the same as the
		 * number of physical ports.
		 */
		if (total_vnics != nfp_eth_table->count) {
			PMD_INIT_LOG(ERR, "Total physical ports do not match number of vNICs");
			ret = -ENODEV;
			goto app_cleanup;
		}
	}

	/* Populate coreNIC app properties */
	app_fw_nic->total_phyports = total_vnics;
	app_fw_nic->pf_dev = pf_dev;
	if (total_vnics > 1)
		app_fw_nic->multiport = true;

	/* Map the symbol table */
	snprintf(bar_name, sizeof(bar_name), "_pf%u_net_bar0", id);
	pf_dev->ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, bar_name,
			app_fw_nic->total_phyports * NFP_NET_CFG_BAR_SZ,
			&pf_dev->ctrl_area);
	if (pf_dev->ctrl_bar == NULL) {
		PMD_INIT_LOG(ERR, "nfp_rtsym_map fails for %s", bar_name);
		ret = -EIO;
		goto app_cleanup;
	}

	PMD_INIT_LOG(DEBUG, "ctrl bar: %p", pf_dev->ctrl_bar);

	/* Loop through all physical ports on PF */
	numa_node = rte_socket_id();
	for (i = 0; i < app_fw_nic->total_phyports; i++) {
		id = nfp_function_id_get(pf_dev, i);
		snprintf(port_name, sizeof(port_name), "%s_port%u",
				pf_dev->pci_dev->device.name, id);

		/* Allocate a eth_dev for this phyport */
		eth_dev = rte_eth_dev_allocate(port_name);
		if (eth_dev == NULL) {
			ret = -ENODEV;
			goto port_cleanup;
		}

		/* Allocate memory for this phyport */
		eth_dev->data->dev_private = rte_zmalloc_socket(port_name,
				sizeof(struct nfp_net_hw),
				RTE_CACHE_LINE_SIZE, numa_node);
		if (eth_dev->data->dev_private == NULL) {
			ret = -ENOMEM;
			rte_eth_dev_release_port(eth_dev);
			goto port_cleanup;
		}

		hw = eth_dev->data->dev_private;

		/* Add this device to the PF's array of physical ports */
		app_fw_nic->ports[id] = hw;

		hw->dev_info = dev_info;
		hw->pf_dev = pf_dev;
		hw->cpp = pf_dev->cpp;
		hw->eth_dev = eth_dev;
		hw->idx = id;
		hw->nfp_idx = nfp_eth_table->ports[id].index;

		eth_dev->device = &pf_dev->pci_dev->device;

		/*
		 * Ctrl/tx/rx BAR mappings and remaining init happens in
		 * @nfp_net_init()
		 */
		ret = nfp_net_init(eth_dev);
		if (ret != 0) {
			ret = -ENODEV;
			goto port_cleanup;
		}

		rte_eth_dev_probing_finish(eth_dev);

	} /* End loop, all ports on this PF */

	return 0;

port_cleanup:
	for (i = 0; i < app_fw_nic->total_phyports; i++) {
		id = nfp_function_id_get(pf_dev, i);

		if (app_fw_nic->ports[id] != NULL &&
				app_fw_nic->ports[id]->eth_dev != NULL) {
			struct rte_eth_dev *tmp_dev;
			tmp_dev = app_fw_nic->ports[id]->eth_dev;
			nfp_ipsec_uninit(tmp_dev);
			rte_eth_dev_release_port(tmp_dev);
			app_fw_nic->ports[id] = NULL;
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
	uint32_t i;
	uint32_t id;
	int ret = 0;
	uint64_t addr;
	uint32_t index;
	uint32_t cpp_id;
	uint8_t function_id;
	struct nfp_cpp *cpp;
	struct nfp_pf_dev *pf_dev;
	struct nfp_hwinfo *hwinfo;
	enum nfp_app_fw_id app_fw_id;
	char name[RTE_ETH_NAME_MAX_LEN];
	struct nfp_rtsym_table *sym_tbl;
	char app_name[RTE_ETH_NAME_MAX_LEN];
	struct nfp_eth_table *nfp_eth_table;
	const struct nfp_dev_info *dev_info;

	if (pci_dev == NULL)
		return -ENODEV;

	if (pci_dev->mem_resource[0].addr == NULL) {
		PMD_INIT_LOG(ERR, "The address of BAR0 is NULL.");
		return -ENODEV;
	}

	dev_info = nfp_dev_info_get(pci_dev->id.device_id);
	if (dev_info == NULL) {
		PMD_INIT_LOG(ERR, "Not supported device ID");
		return -ENODEV;
	}

	/* Allocate memory for the PF "device" */
	function_id = (pci_dev->addr.function) & 0x07;
	snprintf(name, sizeof(name), "nfp_pf%u", function_id);
	pf_dev = rte_zmalloc(name, sizeof(*pf_dev), 0);
	if (pf_dev == NULL) {
		PMD_INIT_LOG(ERR, "Can't allocate memory for the PF device");
		return -ENOMEM;
	}

	/*
	 * When device bound to UIO, the device could be used, by mistake,
	 * by two DPDK apps, and the UIO driver does not avoid it. This
	 * could lead to a serious problem when configuring the NFP CPP
	 * interface. Here we avoid this telling to the CPP init code to
	 * use a lock file if UIO is being used.
	 */
	if (pci_dev->kdrv == RTE_PCI_KDRV_VFIO)
		cpp = nfp_cpp_from_nfp6000_pcie(pci_dev, dev_info, false);
	else
		cpp = nfp_cpp_from_nfp6000_pcie(pci_dev, dev_info, true);

	if (cpp == NULL) {
		PMD_INIT_LOG(ERR, "A CPP handle can not be obtained");
		ret = -EIO;
		goto pf_cleanup;
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

	pf_dev->multi_pf.enabled = nfp_check_multi_pf_from_nsp(pci_dev, cpp);
	pf_dev->multi_pf.function_id = function_id;

	/* Force the physical port down to clear the possible DMA error */
	for (i = 0; i < nfp_eth_table->count; i++) {
		id = nfp_function_id_get(pf_dev, i);
		index = nfp_eth_table->ports[id].index;
		nfp_eth_set_configured(cpp, index, 0);
	}

	if (nfp_fw_setup(pci_dev, cpp, nfp_eth_table, hwinfo,
			dev_info, &pf_dev->multi_pf) != 0) {
		PMD_INIT_LOG(ERR, "Error when uploading firmware");
		ret = -EIO;
		goto eth_table_cleanup;
	}

	/* Now the symbol table should be there */
	sym_tbl = nfp_rtsym_table_read(cpp);
	if (sym_tbl == NULL) {
		PMD_INIT_LOG(ERR, "Something is wrong with the firmware symbol table");
		ret = -EIO;
		goto fw_cleanup;
	}

	/* Read the app ID of the firmware loaded */
	snprintf(app_name, sizeof(app_name), "_pf%u_net_app_id", function_id);
	app_fw_id = nfp_rtsym_read_le(sym_tbl, app_name, &ret);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Couldn't read %s from firmware", app_name);
		ret = -EIO;
		goto sym_tbl_cleanup;
	}

	/* Populate the newly created PF device */
	pf_dev->app_fw_id = app_fw_id;
	pf_dev->cpp = cpp;
	pf_dev->hwinfo = hwinfo;
	pf_dev->sym_tbl = sym_tbl;
	pf_dev->pci_dev = pci_dev;
	pf_dev->nfp_eth_table = nfp_eth_table;

	/* Configure access to tx/rx vNIC BARs */
	addr = nfp_qcp_queue_offset(dev_info, 0);
	cpp_id = NFP_CPP_ISLAND_ID(0, NFP_CPP_ACTION_RW, 0, 0);

	pf_dev->qc_bar = nfp_cpp_map_area(pf_dev->cpp, cpp_id,
			addr, dev_info->qc_area_sz, &pf_dev->qc_area);
	if (pf_dev->qc_bar == NULL) {
		PMD_INIT_LOG(ERR, "nfp_rtsym_map fails for net.qc");
		ret = -EIO;
		goto sym_tbl_cleanup;
	}

	PMD_INIT_LOG(DEBUG, "qc_bar address: %p", pf_dev->qc_bar);

	/*
	 * PF initialization has been done at this point. Call app specific
	 * init code now.
	 */
	switch (pf_dev->app_fw_id) {
	case NFP_APP_FW_CORE_NIC:
		if (pf_dev->multi_pf.enabled) {
			ret = nfp_enable_multi_pf(pf_dev);
			if (ret != 0)
				goto hwqueues_cleanup;
		}

		PMD_INIT_LOG(INFO, "Initializing coreNIC");
		ret = nfp_init_app_fw_nic(pf_dev, dev_info);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Could not initialize coreNIC!");
			goto hwqueues_cleanup;
		}
		break;
	case NFP_APP_FW_FLOWER_NIC:
		PMD_INIT_LOG(INFO, "Initializing Flower");
		ret = nfp_init_app_fw_flower(pf_dev, dev_info);
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

	/* Register the CPP bridge service here for primary use */
	ret = nfp_enable_cpp_service(pf_dev);
	if (ret != 0)
		PMD_INIT_LOG(INFO, "Enable cpp service failed.");

	return 0;

hwqueues_cleanup:
	nfp_cpp_area_free(pf_dev->qc_area);
sym_tbl_cleanup:
	free(sym_tbl);
fw_cleanup:
	nfp_fw_unload(cpp);
	nfp_net_keepalive_stop(&pf_dev->multi_pf);
eth_table_cleanup:
	free(nfp_eth_table);
hwinfo_cleanup:
	free(hwinfo);
cpp_cleanup:
	nfp_cpp_free(cpp);
pf_cleanup:
	rte_free(pf_dev);

	return ret;
}

static int
nfp_secondary_init_app_fw_nic(struct nfp_pf_dev *pf_dev)
{
	uint32_t i;
	int err = 0;
	int ret = 0;
	uint8_t function_id;
	uint32_t total_vnics;
	struct nfp_net_hw *hw;
	char pf_name[RTE_ETH_NAME_MAX_LEN];

	/* Read the number of vNIC's created for the PF */
	function_id = (pf_dev->pci_dev->addr.function) & 0x07;
	snprintf(pf_name, sizeof(pf_name), "nfd_cfg_pf%u_num_ports", function_id);
	total_vnics = nfp_rtsym_read_le(pf_dev->sym_tbl, pf_name, &err);
	if (err != 0 || total_vnics == 0 || total_vnics > 8) {
		PMD_INIT_LOG(ERR, "%s symbol with wrong value", pf_name);
		return -ENODEV;
	}

	for (i = 0; i < total_vnics; i++) {
		uint32_t id = i;
		struct rte_eth_dev *eth_dev;
		char port_name[RTE_ETH_NAME_MAX_LEN];

		if (nfp_check_multi_pf_from_fw(total_vnics))
			id = function_id;
		snprintf(port_name, sizeof(port_name), "%s_port%u",
				pf_dev->pci_dev->device.name, id);

		PMD_INIT_LOG(DEBUG, "Secondary attaching to port %s", port_name);
		eth_dev = rte_eth_dev_attach_secondary(port_name);
		if (eth_dev == NULL) {
			PMD_INIT_LOG(ERR, "Secondary process attach to port %s failed", port_name);
			ret = -ENODEV;
			break;
		}

		eth_dev->process_private = pf_dev->cpp;
		hw = eth_dev->data->dev_private;
		nfp_net_ethdev_ops_mount(hw, eth_dev);

		rte_eth_dev_probing_finish(eth_dev);
	}

	return ret;
}

/*
 * When attaching to the NFP4000/6000 PF on a secondary process there
 * is no need to initialise the PF again. Only minimal work is required
 * here.
 */
static int
nfp_pf_secondary_init(struct rte_pci_device *pci_dev)
{
	int ret = 0;
	struct nfp_cpp *cpp;
	uint8_t function_id;
	struct nfp_pf_dev *pf_dev;
	enum nfp_app_fw_id app_fw_id;
	char name[RTE_ETH_NAME_MAX_LEN];
	struct nfp_rtsym_table *sym_tbl;
	const struct nfp_dev_info *dev_info;
	char app_name[RTE_ETH_NAME_MAX_LEN];

	if (pci_dev == NULL)
		return -ENODEV;

	if (pci_dev->mem_resource[0].addr == NULL) {
		PMD_INIT_LOG(ERR, "The address of BAR0 is NULL.");
		return -ENODEV;
	}

	dev_info = nfp_dev_info_get(pci_dev->id.device_id);
	if (dev_info == NULL) {
		PMD_INIT_LOG(ERR, "Not supported device ID");
		return -ENODEV;
	}

	/* Allocate memory for the PF "device" */
	snprintf(name, sizeof(name), "nfp_pf%d", 0);
	pf_dev = rte_zmalloc(name, sizeof(*pf_dev), 0);
	if (pf_dev == NULL) {
		PMD_INIT_LOG(ERR, "Can't allocate memory for the PF device");
		return -ENOMEM;
	}

	/*
	 * When device bound to UIO, the device could be used, by mistake,
	 * by two DPDK apps, and the UIO driver does not avoid it. This
	 * could lead to a serious problem when configuring the NFP CPP
	 * interface. Here we avoid this telling to the CPP init code to
	 * use a lock file if UIO is being used.
	 */
	if (pci_dev->kdrv == RTE_PCI_KDRV_VFIO)
		cpp = nfp_cpp_from_nfp6000_pcie(pci_dev, dev_info, false);
	else
		cpp = nfp_cpp_from_nfp6000_pcie(pci_dev, dev_info, true);

	if (cpp == NULL) {
		PMD_INIT_LOG(ERR, "A CPP handle can not be obtained");
		ret = -EIO;
		goto pf_cleanup;
	}

	/*
	 * We don't have access to the PF created in the primary process
	 * here so we have to read the number of ports from firmware.
	 */
	sym_tbl = nfp_rtsym_table_read(cpp);
	if (sym_tbl == NULL) {
		PMD_INIT_LOG(ERR, "Something is wrong with the firmware symbol table");
		ret = -EIO;
		goto pf_cleanup;
	}

	/* Read the app ID of the firmware loaded */
	function_id = pci_dev->addr.function & 0x7;
	snprintf(app_name, sizeof(app_name), "_pf%u_net_app_id", function_id);
	app_fw_id = nfp_rtsym_read_le(sym_tbl, app_name, &ret);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Couldn't read %s from fw", app_name);
		ret = -EIO;
		goto sym_tbl_cleanup;
	}

	/* Populate the newly created PF device */
	pf_dev->app_fw_id = app_fw_id;
	pf_dev->cpp = cpp;
	pf_dev->sym_tbl = sym_tbl;
	pf_dev->pci_dev = pci_dev;

	/* Call app specific init code now */
	switch (app_fw_id) {
	case NFP_APP_FW_CORE_NIC:
		PMD_INIT_LOG(INFO, "Initializing coreNIC");
		ret = nfp_secondary_init_app_fw_nic(pf_dev);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Could not initialize coreNIC!");
			goto sym_tbl_cleanup;
		}
		break;
	case NFP_APP_FW_FLOWER_NIC:
		PMD_INIT_LOG(INFO, "Initializing Flower");
		ret = nfp_secondary_init_app_fw_flower(pf_dev);
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

	return 0;

sym_tbl_cleanup:
	free(sym_tbl);
pf_cleanup:
	rte_free(pf_dev);

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
	uint16_t port_id;
	struct rte_pci_device *pci_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* Free up all physical ports under PF */
	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device)
		rte_eth_dev_close(port_id);
	/*
	 * Ports can be closed and freed but hotplugging is not
	 * currently supported.
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

RTE_PMD_REGISTER_PCI(NFP_PF_DRIVER_NAME, rte_nfp_net_pf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(NFP_PF_DRIVER_NAME, pci_id_nfp_pf_net_map);
RTE_PMD_REGISTER_KMOD_DEP(NFP_PF_DRIVER_NAME, "* igb_uio | uio_pci_generic | vfio");
