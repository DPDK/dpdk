/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2018 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_net.c
 *
 * Netronome vNIC DPDK Poll-Mode Driver: Main entry point
 */

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_dev.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_mempool.h>
#include <rte_version.h>
#include <rte_string_fns.h>
#include <rte_alarm.h>
#include <rte_spinlock.h>
#include <rte_service_component.h>

#include "eal_firmware.h"

#include "nfpcore/nfp_cpp.h"
#include "nfpcore/nfp_nffw.h"
#include "nfpcore/nfp_hwinfo.h"
#include "nfpcore/nfp_mip.h"
#include "nfpcore/nfp_rtsym.h"
#include "nfpcore/nfp_nsp.h"

#include "nfp_net_pmd.h"
#include "nfp_rxtx.h"
#include "nfp_net_logs.h"
#include "nfp_net_ctrl.h"
#include "nfp_cpp_bridge.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>

/* Prototypes */
static int nfp_net_close(struct rte_eth_dev *dev);
static int nfp_net_init(struct rte_eth_dev *eth_dev);
static int nfp_pf_init(struct rte_pci_device *pci_dev);
static int nfp_pf_secondary_init(struct rte_pci_device *pci_dev);
static int nfp_net_pf_read_mac(struct nfp_pf_dev *pf_dev, int port);
static int nfp_pci_uninit(struct rte_eth_dev *eth_dev);
static int nfp_init_phyports(struct nfp_pf_dev *pf_dev);
static int nfp_net_stop(struct rte_eth_dev *dev);
static int nfp_fw_setup(struct rte_pci_device *dev,
			struct nfp_cpp *cpp,
			struct nfp_eth_table *nfp_eth_table,
			struct nfp_hwinfo *hwinfo);

static int
__nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t update)
{
	int cnt;
	uint32_t new;
	struct timespec wait;

	PMD_DRV_LOG(DEBUG, "Writing to the configuration queue (%p)...",
		    hw->qcp_cfg);

	if (hw->qcp_cfg == NULL)
		rte_panic("Bad configuration queue pointer\n");

	nfp_qcp_ptr_add(hw->qcp_cfg, NFP_QCP_WRITE_PTR, 1);

	wait.tv_sec = 0;
	wait.tv_nsec = 1000000;

	PMD_DRV_LOG(DEBUG, "Polling for update ack...");

	/* Poll update field, waiting for NFP to ack the config */
	for (cnt = 0; ; cnt++) {
		new = nn_cfg_readl(hw, NFP_NET_CFG_UPDATE);
		if (new == 0)
			break;
		if (new & NFP_NET_CFG_UPDATE_ERR) {
			PMD_INIT_LOG(ERR, "Reconfig error: 0x%08x", new);
			return -1;
		}
		if (cnt >= NFP_NET_POLL_TIMEOUT) {
			PMD_INIT_LOG(ERR, "Reconfig timeout for 0x%08x after"
					  " %dms", update, cnt);
			rte_panic("Exiting\n");
		}
		nanosleep(&wait, 0); /* waiting for a 1ms */
	}
	PMD_DRV_LOG(DEBUG, "Ack DONE");
	return 0;
}

/*
 * Reconfigure the NIC
 * @nn:    device to reconfigure
 * @ctrl:    The value for the ctrl field in the BAR config
 * @update:  The value for the update field in the BAR config
 *
 * Write the update word to the BAR and ping the reconfig queue. Then poll
 * until the firmware has acknowledged the update by zeroing the update word.
 */
int
nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t ctrl, uint32_t update)
{
	uint32_t err;

	PMD_DRV_LOG(DEBUG, "nfp_net_reconfig: ctrl=%08x update=%08x",
		    ctrl, update);

	rte_spinlock_lock(&hw->reconfig_lock);

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, ctrl);
	nn_cfg_writel(hw, NFP_NET_CFG_UPDATE, update);

	rte_wmb();

	err = __nfp_net_reconfig(hw, update);

	rte_spinlock_unlock(&hw->reconfig_lock);

	if (!err)
		return 0;

	/*
	 * Reconfig errors imply situations where they can be handled.
	 * Otherwise, rte_panic is called inside __nfp_net_reconfig
	 */
	PMD_INIT_LOG(ERR, "Error nfp_net reconfig for ctrl: %x update: %x",
		     ctrl, update);
	return -EIO;
}

/*
 * Configure an Ethernet device. This function must be invoked first
 * before any other function in the Ethernet API. This function can
 * also be re-invoked when a device is in the stopped state.
 */
int
nfp_net_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	struct rte_eth_txmode *txmode;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * A DPDK app sends info about how many queues to use and how
	 * those queues need to be configured. This is used by the
	 * DPDK core and it makes sure no more queues than those
	 * advertised by the driver are requested. This function is
	 * called after that internal process
	 */

	PMD_INIT_LOG(DEBUG, "Configure");

	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;
	txmode = &dev_conf->txmode;

	if (rxmode->mq_mode & ETH_MQ_RX_RSS_FLAG)
		rxmode->offloads |= DEV_RX_OFFLOAD_RSS_HASH;

	/* Checking TX mode */
	if (txmode->mq_mode) {
		PMD_INIT_LOG(INFO, "TX mq_mode DCB and VMDq not supported");
		return -EINVAL;
	}

	/* Checking RX mode */
	if (rxmode->mq_mode & ETH_MQ_RX_RSS &&
	    !(hw->cap & NFP_NET_CFG_CTRL_RSS)) {
		PMD_INIT_LOG(INFO, "RSS not supported");
		return -EINVAL;
	}

	return 0;
}

void
nfp_net_enable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint64_t enabled_queues = 0;
	int i;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Enabling the required TX queues in the device */
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, enabled_queues);

	enabled_queues = 0;

	/* Enabling the required RX queues in the device */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, enabled_queues);
}

void
nfp_net_disable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint32_t new_ctrl, update = 0;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, 0);
	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, 0);

	new_ctrl = hw->ctrl & ~NFP_NET_CFG_CTRL_ENABLE;
	update = NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING |
		 NFP_NET_CFG_UPDATE_MSIX;

	if (hw->cap & NFP_NET_CFG_CTRL_RINGCFG)
		new_ctrl &= ~NFP_NET_CFG_CTRL_RINGCFG;

	/* If an error when reconfig we avoid to change hw state */
	if (nfp_net_reconfig(hw, new_ctrl, update) < 0)
		return;

	hw->ctrl = new_ctrl;
}

void
nfp_net_params_setup(struct nfp_net_hw *hw)
{
	nn_cfg_writel(hw, NFP_NET_CFG_MTU, hw->mtu);
	nn_cfg_writel(hw, NFP_NET_CFG_FLBUFSZ, hw->flbufsz);
}

void
nfp_net_cfg_queue_setup(struct nfp_net_hw *hw)
{
	hw->qcp_cfg = hw->tx_bar + NFP_QCP_QUEUE_ADDR_SZ;
}

#define ETH_ADDR_LEN	6

void
nfp_eth_copy_mac(uint8_t *dst, const uint8_t *src)
{
	int i;

	for (i = 0; i < ETH_ADDR_LEN; i++)
		dst[i] = src[i];
}

static int
nfp_net_pf_read_mac(struct nfp_pf_dev *pf_dev, int port)
{
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_net_hw *hw = NULL;

	/* Grab a pointer to the correct physical port */
	hw = pf_dev->ports[port];

	nfp_eth_table = nfp_eth_read_ports(pf_dev->cpp);

	nfp_eth_copy_mac((uint8_t *)&hw->mac_addr,
			 (uint8_t *)&nfp_eth_table->ports[port].mac_addr);

	free(nfp_eth_table);
	return 0;
}

void
nfp_net_write_mac(struct nfp_net_hw *hw, uint8_t *mac)
{
	uint32_t mac0 = *(uint32_t *)mac;
	uint16_t mac1;

	nn_writel(rte_cpu_to_be_32(mac0), hw->ctrl_bar + NFP_NET_CFG_MACADDR);

	mac += 4;
	mac1 = *(uint16_t *)mac;
	nn_writew(rte_cpu_to_be_16(mac1),
		  hw->ctrl_bar + NFP_NET_CFG_MACADDR + 6);
}

int
nfp_set_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct nfp_net_hw *hw;
	uint32_t update, ctrl;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if ((hw->ctrl & NFP_NET_CFG_CTRL_ENABLE) &&
	    !(hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR)) {
		PMD_INIT_LOG(INFO, "MAC address unable to change when"
				  " port enabled");
		return -EBUSY;
	}

	if ((hw->ctrl & NFP_NET_CFG_CTRL_ENABLE) &&
	    !(hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR))
		return -EBUSY;

	/* Writing new MAC to the specific port BAR address */
	nfp_net_write_mac(hw, (uint8_t *)mac_addr);

	/* Signal the NIC about the change */
	update = NFP_NET_CFG_UPDATE_MACADDR;
	ctrl = hw->ctrl;
	if ((hw->ctrl & NFP_NET_CFG_CTRL_ENABLE) &&
	    (hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR))
		ctrl |= NFP_NET_CFG_CTRL_LIVE_ADDR;
	if (nfp_net_reconfig(hw, ctrl, update) < 0) {
		PMD_INIT_LOG(INFO, "MAC address update failed");
		return -EIO;
	}
	return 0;
}

int
nfp_configure_rx_interrupt(struct rte_eth_dev *dev,
			   struct rte_intr_handle *intr_handle)
{
	struct nfp_net_hw *hw;
	int i;

	if (!intr_handle->intr_vec) {
		intr_handle->intr_vec =
			rte_zmalloc("intr_vec",
				    dev->data->nb_rx_queues * sizeof(int), 0);
		if (!intr_handle->intr_vec) {
			PMD_INIT_LOG(ERR, "Failed to allocate %d rx_queues"
				     " intr_vec", dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (intr_handle->type == RTE_INTR_HANDLE_UIO) {
		PMD_INIT_LOG(INFO, "VF: enabling RX interrupt with UIO");
		/* UIO just supports one queue and no LSC*/
		nn_cfg_writeb(hw, NFP_NET_CFG_RXR_VEC(0), 0);
		intr_handle->intr_vec[0] = 0;
	} else {
		PMD_INIT_LOG(INFO, "VF: enabling RX interrupt with VFIO");
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			/*
			 * The first msix vector is reserved for non
			 * efd interrupts
			*/
			nn_cfg_writeb(hw, NFP_NET_CFG_RXR_VEC(i), i + 1);
			intr_handle->intr_vec[i] = i + 1;
			PMD_INIT_LOG(DEBUG, "intr_vec[%d]= %d", i,
					    intr_handle->intr_vec[i]);
		}
	}

	/* Avoiding TX interrupts */
	hw->ctrl |= NFP_NET_CFG_CTRL_MSIX_TX_OFF;
	return 0;
}

uint32_t
nfp_check_offloads(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	struct rte_eth_txmode *txmode;
	uint32_t ctrl = 0;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;
	txmode = &dev_conf->txmode;

	if (rxmode->offloads & DEV_RX_OFFLOAD_IPV4_CKSUM) {
		if (hw->cap & NFP_NET_CFG_CTRL_RXCSUM)
			ctrl |= NFP_NET_CFG_CTRL_RXCSUM;
	}

	if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_STRIP) {
		if (hw->cap & NFP_NET_CFG_CTRL_RXVLAN)
			ctrl |= NFP_NET_CFG_CTRL_RXVLAN;
	}

	if (rxmode->offloads & DEV_RX_OFFLOAD_JUMBO_FRAME)
		hw->mtu = rxmode->max_rx_pkt_len;

	if (txmode->offloads & DEV_TX_OFFLOAD_VLAN_INSERT)
		ctrl |= NFP_NET_CFG_CTRL_TXVLAN;

	/* L2 broadcast */
	if (hw->cap & NFP_NET_CFG_CTRL_L2BC)
		ctrl |= NFP_NET_CFG_CTRL_L2BC;

	/* L2 multicast */
	if (hw->cap & NFP_NET_CFG_CTRL_L2MC)
		ctrl |= NFP_NET_CFG_CTRL_L2MC;

	/* TX checksum offload */
	if (txmode->offloads & DEV_TX_OFFLOAD_IPV4_CKSUM ||
	    txmode->offloads & DEV_TX_OFFLOAD_UDP_CKSUM ||
	    txmode->offloads & DEV_TX_OFFLOAD_TCP_CKSUM)
		ctrl |= NFP_NET_CFG_CTRL_TXCSUM;

	/* LSO offload */
	if (txmode->offloads & DEV_TX_OFFLOAD_TCP_TSO) {
		if (hw->cap & NFP_NET_CFG_CTRL_LSO)
			ctrl |= NFP_NET_CFG_CTRL_LSO;
		else
			ctrl |= NFP_NET_CFG_CTRL_LSO2;
	}

	/* RX gather */
	if (txmode->offloads & DEV_TX_OFFLOAD_MULTI_SEGS)
		ctrl |= NFP_NET_CFG_CTRL_GATHER;

	return ctrl;
}

static int
nfp_net_start(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint32_t new_ctrl, update = 0;
	struct nfp_net_hw *hw;
	struct nfp_pf_dev *pf_dev;
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	uint32_t intr_vector;
	int ret;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pf_dev = NFP_NET_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	PMD_INIT_LOG(DEBUG, "Start");

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	/* check and configure queue intr-vector mapping */
	if (dev->data->dev_conf.intr_conf.rxq != 0) {
		if (pf_dev->multiport) {
			PMD_INIT_LOG(ERR, "PMD rx interrupt is not supported "
					  "with NFP multiport PF");
				return -EINVAL;
		}
		if (intr_handle->type == RTE_INTR_HANDLE_UIO) {
			/*
			 * Better not to share LSC with RX interrupts.
			 * Unregistering LSC interrupt handler
			 */
			rte_intr_callback_unregister(&pci_dev->intr_handle,
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

	rte_intr_enable(intr_handle);

	new_ctrl = nfp_check_offloads(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(hw);

	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;

	if (rxmode->mq_mode & ETH_MQ_RX_RSS) {
		nfp_net_rss_config_default(dev);
		update |= NFP_NET_CFG_UPDATE_RSS;
		new_ctrl |= NFP_NET_CFG_CTRL_RSS;
	}

	/* Enable device */
	new_ctrl |= NFP_NET_CFG_CTRL_ENABLE;

	update |= NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING;

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

	if (hw->is_phyport) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY)
			/* Configure the physical port up */
			nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 1);
		else
			nfp_eth_set_configured(dev->process_private,
					       hw->nfp_idx, 1);
	}

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
	int i;
	struct nfp_net_hw *hw;

	PMD_INIT_LOG(DEBUG, "Stop");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	nfp_net_disable_queues(dev);

	/* Clear queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		nfp_net_reset_tx_queue(
			(struct nfp_net_txq *)dev->data->tx_queues[i]);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		nfp_net_reset_rx_queue(
			(struct nfp_net_rxq *)dev->data->rx_queues[i]);
	}

	if (hw->is_phyport) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY)
			/* Configure the physical port down */
			nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 0);
		else
			nfp_eth_set_configured(dev->process_private,
					       hw->nfp_idx, 0);
	}

	return 0;
}

/* Set the link up. */
static int
nfp_net_set_link_up(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	PMD_DRV_LOG(DEBUG, "Set link up");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!hw->is_phyport)
		return -ENOTSUP;

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

	if (!hw->is_phyport)
		return -ENOTSUP;

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
	int i;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	PMD_INIT_LOG(DEBUG, "Close");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	/*
	 * We assume that the DPDK application is stopping all the
	 * threads/queues before calling the device close function.
	 */

	nfp_net_disable_queues(dev);

	/* Clear queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		nfp_net_reset_tx_queue(
			(struct nfp_net_txq *)dev->data->tx_queues[i]);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		nfp_net_reset_rx_queue(
			(struct nfp_net_rxq *)dev->data->rx_queues[i]);
	}

	/* Only free PF resources after all physical ports have been closed */
	if (pci_dev->id.device_id == PCI_DEVICE_ID_NFP4000_PF_NIC ||
	    pci_dev->id.device_id == PCI_DEVICE_ID_NFP6000_PF_NIC) {
		struct nfp_pf_dev *pf_dev;
		pf_dev = NFP_NET_DEV_PRIVATE_TO_PF(dev->data->dev_private);

		/* Mark this port as unused and free device priv resources*/
		nn_cfg_writeb(hw, NFP_NET_CFG_LSC, 0xff);
		pf_dev->ports[hw->idx] = NULL;
		rte_eth_dev_release_port(dev);

		for (i = 0; i < pf_dev->total_phyports; i++) {
			/* Check to see if ports are still in use */
			if (pf_dev->ports[i])
				return 0;
		}

		/* Now it is safe to free all PF resources */
		PMD_INIT_LOG(INFO, "Freeing PF resources");
		nfp_cpp_area_free(pf_dev->ctrl_area);
		nfp_cpp_area_free(pf_dev->hwqueues_area);
		free(pf_dev->hwinfo);
		free(pf_dev->sym_tbl);
		nfp_cpp_free(pf_dev->cpp);
		rte_free(pf_dev);
	}

	rte_intr_disable(&pci_dev->intr_handle);

	/* unregister callback func from eal lib */
	rte_intr_callback_unregister(&pci_dev->intr_handle,
				     nfp_net_dev_interrupt_handler,
				     (void *)dev);

	/*
	 * The ixgbe PMD driver disables the pcie master on the
	 * device. The i40e does not...
	 */

	return 0;
}

int
nfp_net_promisc_enable(struct rte_eth_dev *dev)
{
	uint32_t new_ctrl, update = 0;
	struct nfp_net_hw *hw;
	int ret;

	PMD_DRV_LOG(DEBUG, "Promiscuous mode enable");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!(hw->cap & NFP_NET_CFG_CTRL_PROMISC)) {
		PMD_INIT_LOG(INFO, "Promiscuous mode not supported");
		return -ENOTSUP;
	}

	if (hw->ctrl & NFP_NET_CFG_CTRL_PROMISC) {
		PMD_DRV_LOG(INFO, "Promiscuous mode already enabled");
		return 0;
	}

	new_ctrl = hw->ctrl | NFP_NET_CFG_CTRL_PROMISC;
	update = NFP_NET_CFG_UPDATE_GEN;

	/*
	 * DPDK sets promiscuous mode on just after this call assuming
	 * it can not fail ...
	 */
	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (ret < 0)
		return ret;

	hw->ctrl = new_ctrl;

	return 0;
}

int
nfp_net_promisc_disable(struct rte_eth_dev *dev)
{
	uint32_t new_ctrl, update = 0;
	struct nfp_net_hw *hw;
	int ret;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if ((hw->ctrl & NFP_NET_CFG_CTRL_PROMISC) == 0) {
		PMD_DRV_LOG(INFO, "Promiscuous mode already disabled");
		return 0;
	}

	new_ctrl = hw->ctrl & ~NFP_NET_CFG_CTRL_PROMISC;
	update = NFP_NET_CFG_UPDATE_GEN;

	/*
	 * DPDK sets promiscuous mode off just before this call
	 * assuming it can not fail ...
	 */
	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (ret < 0)
		return ret;

	hw->ctrl = new_ctrl;

	return 0;
}

/*
 * return 0 means link status changed, -1 means not changed
 *
 * Wait to complete is needed as it can take up to 9 seconds to get the Link
 * status.
 */
int
nfp_net_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	struct nfp_net_hw *hw;
	struct rte_eth_link link;
	uint32_t nn_link_status;
	int ret;

	static const uint32_t ls_to_ethtool[] = {
		[NFP_NET_CFG_STS_LINK_RATE_UNSUPPORTED] = ETH_SPEED_NUM_NONE,
		[NFP_NET_CFG_STS_LINK_RATE_UNKNOWN]     = ETH_SPEED_NUM_NONE,
		[NFP_NET_CFG_STS_LINK_RATE_1G]          = ETH_SPEED_NUM_1G,
		[NFP_NET_CFG_STS_LINK_RATE_10G]         = ETH_SPEED_NUM_10G,
		[NFP_NET_CFG_STS_LINK_RATE_25G]         = ETH_SPEED_NUM_25G,
		[NFP_NET_CFG_STS_LINK_RATE_40G]         = ETH_SPEED_NUM_40G,
		[NFP_NET_CFG_STS_LINK_RATE_50G]         = ETH_SPEED_NUM_50G,
		[NFP_NET_CFG_STS_LINK_RATE_100G]        = ETH_SPEED_NUM_100G,
	};

	PMD_DRV_LOG(DEBUG, "Link update");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	nn_link_status = nn_cfg_readl(hw, NFP_NET_CFG_STS);

	memset(&link, 0, sizeof(struct rte_eth_link));

	if (nn_link_status & NFP_NET_CFG_STS_LINK)
		link.link_status = ETH_LINK_UP;

	link.link_duplex = ETH_LINK_FULL_DUPLEX;

	nn_link_status = (nn_link_status >> NFP_NET_CFG_STS_LINK_RATE_SHIFT) &
			 NFP_NET_CFG_STS_LINK_RATE_MASK;

	if (nn_link_status >= RTE_DIM(ls_to_ethtool))
		link.link_speed = ETH_SPEED_NUM_NONE;
	else
		link.link_speed = ls_to_ethtool[nn_link_status];

	ret = rte_eth_linkstatus_set(dev, &link);
	if (ret == 0) {
		if (link.link_status)
			PMD_DRV_LOG(INFO, "NIC Link is Up");
		else
			PMD_DRV_LOG(INFO, "NIC Link is Down");
	}
	return ret;
}

int
nfp_net_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	int i;
	struct nfp_net_hw *hw;
	struct rte_eth_stats nfp_dev_stats;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* RTE_ETHDEV_QUEUE_STAT_CNTRS default value is 16 */

	memset(&nfp_dev_stats, 0, sizeof(nfp_dev_stats));

	/* reading per RX ring stats */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nfp_dev_stats.q_ipackets[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_RXR_STATS(i));

		nfp_dev_stats.q_ipackets[i] -=
			hw->eth_stats_base.q_ipackets[i];

		nfp_dev_stats.q_ibytes[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_RXR_STATS(i) + 0x8);

		nfp_dev_stats.q_ibytes[i] -=
			hw->eth_stats_base.q_ibytes[i];
	}

	/* reading per TX ring stats */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nfp_dev_stats.q_opackets[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_TXR_STATS(i));

		nfp_dev_stats.q_opackets[i] -=
			hw->eth_stats_base.q_opackets[i];

		nfp_dev_stats.q_obytes[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_TXR_STATS(i) + 0x8);

		nfp_dev_stats.q_obytes[i] -=
			hw->eth_stats_base.q_obytes[i];
	}

	nfp_dev_stats.ipackets =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_FRAMES);

	nfp_dev_stats.ipackets -= hw->eth_stats_base.ipackets;

	nfp_dev_stats.ibytes =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_OCTETS);

	nfp_dev_stats.ibytes -= hw->eth_stats_base.ibytes;

	nfp_dev_stats.opackets =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_FRAMES);

	nfp_dev_stats.opackets -= hw->eth_stats_base.opackets;

	nfp_dev_stats.obytes =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_OCTETS);

	nfp_dev_stats.obytes -= hw->eth_stats_base.obytes;

	/* reading general device stats */
	nfp_dev_stats.ierrors =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_ERRORS);

	nfp_dev_stats.ierrors -= hw->eth_stats_base.ierrors;

	nfp_dev_stats.oerrors =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_ERRORS);

	nfp_dev_stats.oerrors -= hw->eth_stats_base.oerrors;

	/* RX ring mbuf allocation failures */
	nfp_dev_stats.rx_nombuf = dev->data->rx_mbuf_alloc_failed;

	nfp_dev_stats.imissed =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_DISCARDS);

	nfp_dev_stats.imissed -= hw->eth_stats_base.imissed;

	if (stats) {
		memcpy(stats, &nfp_dev_stats, sizeof(*stats));
		return 0;
	}
	return -EINVAL;
}

int
nfp_net_stats_reset(struct rte_eth_dev *dev)
{
	int i;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * hw->eth_stats_base records the per counter starting point.
	 * Lets update it now
	 */

	/* reading per RX ring stats */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		hw->eth_stats_base.q_ipackets[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_RXR_STATS(i));

		hw->eth_stats_base.q_ibytes[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_RXR_STATS(i) + 0x8);
	}

	/* reading per TX ring stats */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		hw->eth_stats_base.q_opackets[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_TXR_STATS(i));

		hw->eth_stats_base.q_obytes[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_TXR_STATS(i) + 0x8);
	}

	hw->eth_stats_base.ipackets =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_FRAMES);

	hw->eth_stats_base.ibytes =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_OCTETS);

	hw->eth_stats_base.opackets =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_FRAMES);

	hw->eth_stats_base.obytes =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_OCTETS);

	/* reading general device stats */
	hw->eth_stats_base.ierrors =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_ERRORS);

	hw->eth_stats_base.oerrors =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_ERRORS);

	/* RX ring mbuf allocation failures */
	dev->data->rx_mbuf_alloc_failed = 0;

	hw->eth_stats_base.imissed =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_DISCARDS);

	return 0;
}

int
nfp_net_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	dev_info->max_rx_queues = (uint16_t)hw->max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->max_tx_queues;
	dev_info->min_rx_bufsize = RTE_ETHER_MIN_MTU;
	dev_info->max_rx_pktlen = hw->max_mtu;
	/* Next should change when PF support is implemented */
	dev_info->max_mac_addrs = 1;

	if (hw->cap & NFP_NET_CFG_CTRL_RXVLAN)
		dev_info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP;

	if (hw->cap & NFP_NET_CFG_CTRL_RXCSUM)
		dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_IPV4_CKSUM |
					     DEV_RX_OFFLOAD_UDP_CKSUM |
					     DEV_RX_OFFLOAD_TCP_CKSUM;

	if (hw->cap & NFP_NET_CFG_CTRL_TXVLAN)
		dev_info->tx_offload_capa = DEV_TX_OFFLOAD_VLAN_INSERT;

	if (hw->cap & NFP_NET_CFG_CTRL_TXCSUM)
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_IPV4_CKSUM |
					     DEV_TX_OFFLOAD_UDP_CKSUM |
					     DEV_TX_OFFLOAD_TCP_CKSUM;

	if (hw->cap & NFP_NET_CFG_CTRL_LSO_ANY)
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_TCP_TSO;

	if (hw->cap & NFP_NET_CFG_CTRL_GATHER)
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_MULTI_SEGS;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = DEFAULT_RX_PTHRESH,
			.hthresh = DEFAULT_RX_HTHRESH,
			.wthresh = DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = DEFAULT_TX_PTHRESH,
			.hthresh = DEFAULT_TX_HTHRESH,
			.wthresh = DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = DEFAULT_TX_RSBIT_THRESH,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = NFP_NET_MAX_RX_DESC,
		.nb_min = NFP_NET_MIN_RX_DESC,
		.nb_align = NFP_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = NFP_NET_MAX_TX_DESC,
		.nb_min = NFP_NET_MIN_TX_DESC,
		.nb_align = NFP_ALIGN_RING_DESC,
		.nb_seg_max = NFP_TX_MAX_SEG,
		.nb_mtu_seg_max = NFP_TX_MAX_MTU_SEG,
	};

	/* All NFP devices support jumbo frames */
	dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_JUMBO_FRAME;

	if (hw->cap & NFP_NET_CFG_CTRL_RSS) {
		dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_RSS_HASH;

		dev_info->flow_type_rss_offloads = ETH_RSS_IPV4 |
						   ETH_RSS_NONFRAG_IPV4_TCP |
						   ETH_RSS_NONFRAG_IPV4_UDP |
						   ETH_RSS_IPV6 |
						   ETH_RSS_NONFRAG_IPV6_TCP |
						   ETH_RSS_NONFRAG_IPV6_UDP;

		dev_info->reta_size = NFP_NET_CFG_RSS_ITBL_SZ;
		dev_info->hash_key_size = NFP_NET_CFG_RSS_KEY_SZ;
	}

	dev_info->speed_capa = ETH_LINK_SPEED_1G | ETH_LINK_SPEED_10G |
			       ETH_LINK_SPEED_25G | ETH_LINK_SPEED_40G |
			       ETH_LINK_SPEED_50G | ETH_LINK_SPEED_100G;

	return 0;
}

const uint32_t *
nfp_net_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to nfp_net_set_hash() */
		RTE_PTYPE_INNER_L3_IPV4,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_INNER_L3_IPV6_EXT,
		RTE_PTYPE_INNER_L4_MASK,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == nfp_net_recv_pkts)
		return ptypes;
	return NULL;
}

int
nfp_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev;
	struct nfp_net_hw *hw;
	int base = 0;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (pci_dev->intr_handle.type != RTE_INTR_HANDLE_UIO)
		base = 1;

	/* Make sure all updates are written before un-masking */
	rte_wmb();
	nn_cfg_writeb(hw, NFP_NET_CFG_ICR(base + queue_id),
		      NFP_NET_CFG_ICR_UNMASKED);
	return 0;
}

int
nfp_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev;
	struct nfp_net_hw *hw;
	int base = 0;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (pci_dev->intr_handle.type != RTE_INTR_HANDLE_UIO)
		base = 1;

	/* Make sure all updates are written before un-masking */
	rte_wmb();
	nn_cfg_writeb(hw, NFP_NET_CFG_ICR(base + queue_id), 0x1);
	return 0;
}

static void
nfp_net_dev_link_status_print(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_eth_link link;

	rte_eth_linkstatus_get(dev, &link);
	if (link.link_status)
		PMD_DRV_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s",
			    dev->data->port_id, link.link_speed,
			    link.link_duplex == ETH_LINK_FULL_DUPLEX
			    ? "full-duplex" : "half-duplex");
	else
		PMD_DRV_LOG(INFO, " Port %d: Link Down",
			    dev->data->port_id);

	PMD_DRV_LOG(INFO, "PCI Address: " PCI_PRI_FMT,
		    pci_dev->addr.domain, pci_dev->addr.bus,
		    pci_dev->addr.devid, pci_dev->addr.function);
}

/* Interrupt configuration and handling */

/*
 * nfp_net_irq_unmask - Unmask an interrupt
 *
 * If MSI-X auto-masking is enabled clear the mask bit, otherwise
 * clear the ICR for the entry.
 */
static void
nfp_net_irq_unmask(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	struct rte_pci_device *pci_dev;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (hw->ctrl & NFP_NET_CFG_CTRL_MSIXAUTO) {
		/* If MSI-X auto-masking is used, clear the entry */
		rte_wmb();
		rte_intr_ack(&pci_dev->intr_handle);
	} else {
		/* Make sure all updates are written before un-masking */
		rte_wmb();
		nn_cfg_writeb(hw, NFP_NET_CFG_ICR(NFP_NET_IRQ_LSC_IDX),
			      NFP_NET_CFG_ICR_UNMASKED);
	}
}

/*
 * Interrupt handler which shall be registered for alarm callback for delayed
 * handling specific interrupt to wait for the stable nic state. As the NIC
 * interrupt state is not stable for nfp after link is just down, it needs
 * to wait 4 seconds to get the stable status.
 *
 * @param handle   Pointer to interrupt handle.
 * @param param    The address of parameter (struct rte_eth_dev *)
 *
 * @return  void
 */
static void
nfp_net_dev_interrupt_delayed_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	nfp_net_link_update(dev, 0);
	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);

	nfp_net_dev_link_status_print(dev);

	/* Unmasking */
	nfp_net_irq_unmask(dev);
}

void
nfp_net_dev_interrupt_handler(void *param)
{
	int64_t timeout;
	struct rte_eth_link link;
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	PMD_DRV_LOG(DEBUG, "We got a LSC interrupt!!!");

	rte_eth_linkstatus_get(dev, &link);

	nfp_net_link_update(dev, 0);

	/* likely to up */
	if (!link.link_status) {
		/* handle it 1 sec later, wait it being stable */
		timeout = NFP_NET_LINK_UP_CHECK_TIMEOUT;
		/* likely to down */
	} else {
		/* handle it 4 sec later, wait it being stable */
		timeout = NFP_NET_LINK_DOWN_CHECK_TIMEOUT;
	}

	if (rte_eal_alarm_set(timeout * 1000,
			      nfp_net_dev_interrupt_delayed_handler,
			      (void *)dev) < 0) {
		PMD_INIT_LOG(ERR, "Error setting alarm");
		/* Unmasking */
		nfp_net_irq_unmask(dev);
	}
}

int
nfp_net_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* check that mtu is within the allowed range */
	if (mtu < RTE_ETHER_MIN_MTU || (uint32_t)mtu > hw->max_mtu)
		return -EINVAL;

	/* mtu setting is forbidden if port is started */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "port %d must be stopped before configuration",
			    dev->data->port_id);
		return -EBUSY;
	}

	/* switch to jumbo mode if needed */
	if ((uint32_t)mtu > RTE_ETHER_MTU)
		dev->data->dev_conf.rxmode.offloads |= DEV_RX_OFFLOAD_JUMBO_FRAME;
	else
		dev->data->dev_conf.rxmode.offloads &= ~DEV_RX_OFFLOAD_JUMBO_FRAME;

	/* update max frame size */
	dev->data->dev_conf.rxmode.max_rx_pkt_len = (uint32_t)mtu;

	/* writing to configuration space */
	nn_cfg_writel(hw, NFP_NET_CFG_MTU, (uint32_t)mtu);

	hw->mtu = mtu;

	return 0;
}

int
nfp_net_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	uint32_t new_ctrl, update;
	struct nfp_net_hw *hw;
	int ret;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	new_ctrl = 0;

	/* Enable vlan strip if it is not configured yet */
	if ((mask & ETH_VLAN_STRIP_OFFLOAD) &&
	    !(hw->ctrl & NFP_NET_CFG_CTRL_RXVLAN))
		new_ctrl = hw->ctrl | NFP_NET_CFG_CTRL_RXVLAN;

	/* Disable vlan strip just if it is configured */
	if (!(mask & ETH_VLAN_STRIP_OFFLOAD) &&
	    (hw->ctrl & NFP_NET_CFG_CTRL_RXVLAN))
		new_ctrl = hw->ctrl & ~NFP_NET_CFG_CTRL_RXVLAN;

	if (new_ctrl == 0)
		return 0;

	update = NFP_NET_CFG_UPDATE_GEN;

	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (!ret)
		hw->ctrl = new_ctrl;

	return ret;
}

static int
nfp_net_rss_reta_write(struct rte_eth_dev *dev,
		    struct rte_eth_rss_reta_entry64 *reta_conf,
		    uint16_t reta_size)
{
	uint32_t reta, mask;
	int i, j;
	int idx, shift;
	struct nfp_net_hw *hw =
		NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (reta_size != NFP_NET_CFG_RSS_ITBL_SZ) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, NFP_NET_CFG_RSS_ITBL_SZ);
		return -EINVAL;
	}

	/*
	 * Update Redirection Table. There are 128 8bit-entries which can be
	 * manage as 32 32bit-entries
	 */
	for (i = 0; i < reta_size; i += 4) {
		/* Handling 4 RSS entries per loop */
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) & 0xF);

		if (!mask)
			continue;

		reta = 0;
		/* If all 4 entries were set, don't need read RETA register */
		if (mask != 0xF)
			reta = nn_cfg_readl(hw, NFP_NET_CFG_RSS_ITBL + i);

		for (j = 0; j < 4; j++) {
			if (!(mask & (0x1 << j)))
				continue;
			if (mask != 0xF)
				/* Clearing the entry bits */
				reta &= ~(0xFF << (8 * j));
			reta |= reta_conf[idx].reta[shift + j] << (8 * j);
		}
		nn_cfg_writel(hw, NFP_NET_CFG_RSS_ITBL + (idx * 64) + shift,
			      reta);
	}
	return 0;
}

/* Update Redirection Table(RETA) of Receive Side Scaling of Ethernet device */
int
nfp_net_reta_update(struct rte_eth_dev *dev,
		    struct rte_eth_rss_reta_entry64 *reta_conf,
		    uint16_t reta_size)
{
	struct nfp_net_hw *hw =
		NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t update;
	int ret;

	if (!(hw->ctrl & NFP_NET_CFG_CTRL_RSS))
		return -EINVAL;

	ret = nfp_net_rss_reta_write(dev, reta_conf, reta_size);
	if (ret != 0)
		return ret;

	update = NFP_NET_CFG_UPDATE_RSS;

	if (nfp_net_reconfig(hw, hw->ctrl, update) < 0)
		return -EIO;

	return 0;
}

 /* Query Redirection Table(RETA) of Receive Side Scaling of Ethernet device. */
int
nfp_net_reta_query(struct rte_eth_dev *dev,
		   struct rte_eth_rss_reta_entry64 *reta_conf,
		   uint16_t reta_size)
{
	uint8_t i, j, mask;
	int idx, shift;
	uint32_t reta;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!(hw->ctrl & NFP_NET_CFG_CTRL_RSS))
		return -EINVAL;

	if (reta_size != NFP_NET_CFG_RSS_ITBL_SZ) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, NFP_NET_CFG_RSS_ITBL_SZ);
		return -EINVAL;
	}

	/*
	 * Reading Redirection Table. There are 128 8bit-entries which can be
	 * manage as 32 32bit-entries
	 */
	for (i = 0; i < reta_size; i += 4) {
		/* Handling 4 RSS entries per loop */
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) & 0xF);

		if (!mask)
			continue;

		reta = nn_cfg_readl(hw, NFP_NET_CFG_RSS_ITBL + (idx * 64) +
				    shift);
		for (j = 0; j < 4; j++) {
			if (!(mask & (0x1 << j)))
				continue;
			reta_conf[idx].reta[shift + j] =
				(uint8_t)((reta >> (8 * j)) & 0xF);
		}
	}
	return 0;
}

static int
nfp_net_rss_hash_write(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct nfp_net_hw *hw;
	uint64_t rss_hf;
	uint32_t cfg_rss_ctrl = 0;
	uint8_t key;
	int i;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Writing the key byte a byte */
	for (i = 0; i < rss_conf->rss_key_len; i++) {
		memcpy(&key, &rss_conf->rss_key[i], 1);
		nn_cfg_writeb(hw, NFP_NET_CFG_RSS_KEY + i, key);
	}

	rss_hf = rss_conf->rss_hf;

	if (rss_hf & ETH_RSS_IPV4)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4;

	if (rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_TCP;

	if (rss_hf & ETH_RSS_NONFRAG_IPV4_UDP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_UDP;

	if (rss_hf & ETH_RSS_IPV6)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6;

	if (rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_TCP;

	if (rss_hf & ETH_RSS_NONFRAG_IPV6_UDP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_UDP;

	cfg_rss_ctrl |= NFP_NET_CFG_RSS_MASK;
	cfg_rss_ctrl |= NFP_NET_CFG_RSS_TOEPLITZ;

	/* configuring where to apply the RSS hash */
	nn_cfg_writel(hw, NFP_NET_CFG_RSS_CTRL, cfg_rss_ctrl);

	/* Writing the key size */
	nn_cfg_writeb(hw, NFP_NET_CFG_RSS_KEY_SZ, rss_conf->rss_key_len);

	return 0;
}

int
nfp_net_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	uint32_t update;
	uint64_t rss_hf;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	rss_hf = rss_conf->rss_hf;

	/* Checking if RSS is enabled */
	if (!(hw->ctrl & NFP_NET_CFG_CTRL_RSS)) {
		if (rss_hf != 0) { /* Enable RSS? */
			PMD_DRV_LOG(ERR, "RSS unsupported");
			return -EINVAL;
		}
		return 0; /* Nothing to do */
	}

	if (rss_conf->rss_key_len > NFP_NET_CFG_RSS_KEY_SZ) {
		PMD_DRV_LOG(ERR, "hash key too long");
		return -EINVAL;
	}

	nfp_net_rss_hash_write(dev, rss_conf);

	update = NFP_NET_CFG_UPDATE_RSS;

	if (nfp_net_reconfig(hw, hw->ctrl, update) < 0)
		return -EIO;

	return 0;
}

int
nfp_net_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	uint64_t rss_hf;
	uint32_t cfg_rss_ctrl;
	uint8_t key;
	int i;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!(hw->ctrl & NFP_NET_CFG_CTRL_RSS))
		return -EINVAL;

	rss_hf = rss_conf->rss_hf;
	cfg_rss_ctrl = nn_cfg_readl(hw, NFP_NET_CFG_RSS_CTRL);

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4)
		rss_hf |= ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_NONFRAG_IPV4_UDP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4_TCP)
		rss_hf |= ETH_RSS_NONFRAG_IPV4_TCP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6_TCP)
		rss_hf |= ETH_RSS_NONFRAG_IPV6_TCP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4_UDP)
		rss_hf |= ETH_RSS_NONFRAG_IPV4_UDP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6_UDP)
		rss_hf |= ETH_RSS_NONFRAG_IPV6_UDP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6)
		rss_hf |= ETH_RSS_NONFRAG_IPV4_UDP | ETH_RSS_NONFRAG_IPV6_UDP;

	/* Propagate current RSS hash functions to caller */
	rss_conf->rss_hf = rss_hf;

	/* Reading the key size */
	rss_conf->rss_key_len = nn_cfg_readl(hw, NFP_NET_CFG_RSS_KEY_SZ);

	/* Reading the key byte a byte */
	for (i = 0; i < rss_conf->rss_key_len; i++) {
		key = nn_cfg_readb(hw, NFP_NET_CFG_RSS_KEY + i);
		memcpy(&rss_conf->rss_key[i], &key, 1);
	}

	return 0;
}

int
nfp_net_rss_config_default(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rss_conf rss_conf;
	struct rte_eth_rss_reta_entry64 nfp_reta_conf[2];
	uint16_t rx_queues = dev->data->nb_rx_queues;
	uint16_t queue;
	int i, j, ret;

	PMD_DRV_LOG(INFO, "setting default RSS conf for %u queues",
		rx_queues);

	nfp_reta_conf[0].mask = ~0x0;
	nfp_reta_conf[1].mask = ~0x0;

	queue = 0;
	for (i = 0; i < 0x40; i += 8) {
		for (j = i; j < (i + 8); j++) {
			nfp_reta_conf[0].reta[j] = queue;
			nfp_reta_conf[1].reta[j] = queue++;
			queue %= rx_queues;
		}
	}
	ret = nfp_net_rss_reta_write(dev, nfp_reta_conf, 0x80);
	if (ret != 0)
		return ret;

	dev_conf = &dev->data->dev_conf;
	if (!dev_conf) {
		PMD_DRV_LOG(INFO, "wrong rss conf");
		return -EINVAL;
	}
	rss_conf = dev_conf->rx_adv_conf.rss_conf;

	ret = nfp_net_rss_hash_write(dev, &rss_conf);

	return ret;
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
	.mac_addr_set           = nfp_set_mac_addr,
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
};


static int
nfp_net_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct nfp_pf_dev *pf_dev;
	struct nfp_net_hw *hw;

	uint64_t tx_bar_off = 0, rx_bar_off = 0;
	uint32_t start_q;
	int stride = 4;
	int port = 0;
	int err;

	PMD_INIT_FUNC_TRACE();

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* Use backpointer here to the PF of this eth_dev */
	pf_dev = NFP_NET_DEV_PRIVATE_TO_PF(eth_dev->data->dev_private);

	/* NFP can not handle DMA addresses requiring more than 40 bits */
	if (rte_mem_check_dma_mask(40)) {
		RTE_LOG(ERR, PMD, "device %s can not be used:",
				   pci_dev->device.name);
		RTE_LOG(ERR, PMD, "\trestricted dma mask to 40 bits!\n");
		return -ENODEV;
	};

	if ((pci_dev->id.device_id == PCI_DEVICE_ID_NFP4000_PF_NIC) ||
	    (pci_dev->id.device_id == PCI_DEVICE_ID_NFP6000_PF_NIC)) {
		port = ((struct nfp_net_hw *)eth_dev->data->dev_private)->idx;
		if (port < 0 || port > 7) {
			PMD_DRV_LOG(ERR, "Port value is wrong");
			return -ENODEV;
		}

		/* Use PF array of physical ports to get pointer to
		 * this specific port
		 */
		hw = pf_dev->ports[port];

		PMD_INIT_LOG(DEBUG, "Working with physical port number: %d, "
				    "NFP internal port number: %d",
				    port, hw->nfp_idx);

	} else {
		hw = NFP_NET_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	}

	eth_dev->dev_ops = &nfp_net_eth_dev_ops;
	eth_dev->rx_queue_count = nfp_net_rx_queue_count;
	eth_dev->rx_pkt_burst = &nfp_net_recv_pkts;
	eth_dev->tx_pkt_burst = &nfp_net_xmit_pkts;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

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

	if (hw->is_phyport) {
		if (port == 0) {
			hw->ctrl_bar = pf_dev->ctrl_bar;
		} else {
			if (!pf_dev->ctrl_bar)
				return -ENODEV;
			/* Use port offset in pf ctrl_bar for this
			 * ports control bar
			 */
			hw->ctrl_bar = pf_dev->ctrl_bar +
				       (port * NFP_PF_CSR_SLICE_SIZE);
		}
	}

	PMD_INIT_LOG(DEBUG, "ctrl bar: %p", hw->ctrl_bar);

	hw->max_rx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_RXRINGS);
	hw->max_tx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_TXRINGS);

	/* Work out where in the BAR the queues start. */
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_NFP4000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_VF_NIC:
		start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_TXQ);
		tx_bar_off = (uint64_t)start_q * NFP_QCP_QUEUE_ADDR_SZ;
		start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_RXQ);
		rx_bar_off = (uint64_t)start_q * NFP_QCP_QUEUE_ADDR_SZ;
		break;
	default:
		PMD_DRV_LOG(ERR, "nfp_net: no device ID matching");
		err = -ENODEV;
		goto dev_err_ctrl_map;
	}

	PMD_INIT_LOG(DEBUG, "tx_bar_off: 0x%" PRIx64 "", tx_bar_off);
	PMD_INIT_LOG(DEBUG, "rx_bar_off: 0x%" PRIx64 "", rx_bar_off);

	if (hw->is_phyport) {
		hw->tx_bar = pf_dev->hw_queues + tx_bar_off;
		hw->rx_bar = pf_dev->hw_queues + rx_bar_off;
		eth_dev->data->dev_private = hw;
	} else {
		hw->tx_bar = (uint8_t *)pci_dev->mem_resource[2].addr +
			     tx_bar_off;
		hw->rx_bar = (uint8_t *)pci_dev->mem_resource[2].addr +
			     rx_bar_off;
	}

	PMD_INIT_LOG(DEBUG, "ctrl_bar: %p, tx_bar: %p, rx_bar: %p",
		     hw->ctrl_bar, hw->tx_bar, hw->rx_bar);

	nfp_net_cfg_queue_setup(hw);

	/* Get some of the read-only fields from the config BAR */
	hw->ver = nn_cfg_readl(hw, NFP_NET_CFG_VERSION);
	hw->cap = nn_cfg_readl(hw, NFP_NET_CFG_CAP);
	hw->max_mtu = nn_cfg_readl(hw, NFP_NET_CFG_MAX_MTU);
	hw->mtu = RTE_ETHER_MTU;

	/* VLAN insertion is incompatible with LSOv2 */
	if (hw->cap & NFP_NET_CFG_CTRL_LSO2)
		hw->cap &= ~NFP_NET_CFG_CTRL_TXVLAN;

	if (NFD_CFG_MAJOR_VERSION_of(hw->ver) < 2)
		hw->rx_offset = NFP_NET_RX_OFFSET;
	else
		hw->rx_offset = nn_cfg_readl(hw, NFP_NET_CFG_RX_OFFSET_ADDR);

	PMD_INIT_LOG(INFO, "VER: %u.%u, Maximum supported MTU: %d",
			   NFD_CFG_MAJOR_VERSION_of(hw->ver),
			   NFD_CFG_MINOR_VERSION_of(hw->ver), hw->max_mtu);

	PMD_INIT_LOG(INFO, "CAP: %#x, %s%s%s%s%s%s%s%s%s%s%s%s%s%s", hw->cap,
		     hw->cap & NFP_NET_CFG_CTRL_PROMISC ? "PROMISC " : "",
		     hw->cap & NFP_NET_CFG_CTRL_L2BC    ? "L2BCFILT " : "",
		     hw->cap & NFP_NET_CFG_CTRL_L2MC    ? "L2MCFILT " : "",
		     hw->cap & NFP_NET_CFG_CTRL_RXCSUM  ? "RXCSUM "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_TXCSUM  ? "TXCSUM "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_RXVLAN  ? "RXVLAN "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_TXVLAN  ? "TXVLAN "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_SCATTER ? "SCATTER " : "",
		     hw->cap & NFP_NET_CFG_CTRL_GATHER  ? "GATHER "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR ? "LIVE_ADDR "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_LSO     ? "TSO "     : "",
		     hw->cap & NFP_NET_CFG_CTRL_LSO2     ? "TSOv2 "     : "",
		     hw->cap & NFP_NET_CFG_CTRL_RSS     ? "RSS "     : "",
		     hw->cap & NFP_NET_CFG_CTRL_RSS2     ? "RSSv2 "     : "");

	hw->ctrl = 0;

	hw->stride_rx = stride;
	hw->stride_tx = stride;

	PMD_INIT_LOG(INFO, "max_rx_queues: %u, max_tx_queues: %u",
		     hw->max_rx_queues, hw->max_tx_queues);

	/* Initializing spinlock for reconfigs */
	rte_spinlock_init(&hw->reconfig_lock);

	/* Allocating memory for mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr",
					       RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to space for MAC address");
		err = -ENOMEM;
		goto dev_err_queues_map;
	}

	if (hw->is_phyport) {
		nfp_net_pf_read_mac(pf_dev, port);
		nfp_net_write_mac(hw, (uint8_t *)&hw->mac_addr);
	}

	if (!rte_is_valid_assigned_ether_addr(
		    (struct rte_ether_addr *)&hw->mac_addr)) {
		PMD_INIT_LOG(INFO, "Using random mac address for port %d",
				   port);
		/* Using random mac addresses for VFs */
		rte_eth_random_addr(&hw->mac_addr[0]);
		nfp_net_write_mac(hw, (uint8_t *)&hw->mac_addr);
	}

	/* Copying mac address to DPDK eth_dev struct */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac_addr,
			&eth_dev->data->mac_addrs[0]);

	if (!(hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR))
		eth_dev->data->dev_flags |= RTE_ETH_DEV_NOLIVE_MAC_ADDR;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	PMD_INIT_LOG(INFO, "port %d VendorID=0x%x DeviceID=0x%x "
		     "mac=%02x:%02x:%02x:%02x:%02x:%02x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id,
		     hw->mac_addr[0], hw->mac_addr[1], hw->mac_addr[2],
		     hw->mac_addr[3], hw->mac_addr[4], hw->mac_addr[5]);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* Registering LSC interrupt handler */
		rte_intr_callback_register(&pci_dev->intr_handle,
					   nfp_net_dev_interrupt_handler,
					   (void *)eth_dev);
		/* Telling the firmware about the LSC interrupt entry */
		nn_cfg_writeb(hw, NFP_NET_CFG_LSC, NFP_NET_IRQ_LSC_IDX);
		/* Recording current stats counters values */
		nfp_net_stats_reset(eth_dev);
	}

	return 0;

dev_err_queues_map:
		nfp_cpp_area_free(hw->hwqueues_area);
dev_err_ctrl_map:
		nfp_cpp_area_free(hw->ctrl_area);

	return err;
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
			dev->device.name);
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
nfp_fw_setup(struct rte_pci_device *dev, struct nfp_cpp *cpp,
	     struct nfp_eth_table *nfp_eth_table, struct nfp_hwinfo *hwinfo)
{
	struct nfp_nsp *nsp;
	const char *nfp_fw_model;
	char card_desc[100];
	int err = 0;

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
	if (!nsp) {
		PMD_DRV_LOG(ERR, "NFP error when obtaining NSP handle");
		return -EIO;
	}

	nfp_nsp_device_soft_reset(nsp);
	err = nfp_fw_upload(dev, nsp, card_desc);

	nfp_nsp_close(nsp);
	return err;
}

static int nfp_init_phyports(struct nfp_pf_dev *pf_dev)
{
	struct nfp_net_hw *hw;
	struct rte_eth_dev *eth_dev;
	struct nfp_eth_table *nfp_eth_table = NULL;
	int ret = 0;
	int i;

	nfp_eth_table = nfp_eth_read_ports(pf_dev->cpp);
	if (!nfp_eth_table) {
		PMD_INIT_LOG(ERR, "Error reading NFP ethernet table");
		ret = -EIO;
		goto error;
	}

	/* Loop through all physical ports on PF */
	for (i = 0; i < pf_dev->total_phyports; i++) {
		const unsigned int numa_node = rte_socket_id();
		char port_name[RTE_ETH_NAME_MAX_LEN];

		snprintf(port_name, sizeof(port_name), "%s_port%d",
			 pf_dev->pci_dev->device.name, i);

		/* Allocate a eth_dev for this phyport */
		eth_dev = rte_eth_dev_allocate(port_name);
		if (!eth_dev) {
			ret = -ENODEV;
			goto port_cleanup;
		}

		/* Allocate memory for this phyport */
		eth_dev->data->dev_private =
			rte_zmalloc_socket(port_name, sizeof(struct nfp_net_hw),
					   RTE_CACHE_LINE_SIZE, numa_node);
		if (!eth_dev->data->dev_private) {
			ret = -ENOMEM;
			rte_eth_dev_release_port(eth_dev);
			goto port_cleanup;
		}

		hw = NFP_NET_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

		/* Add this device to the PF's array of physical ports */
		pf_dev->ports[i] = hw;

		hw->pf_dev = pf_dev;
		hw->cpp = pf_dev->cpp;
		hw->eth_dev = eth_dev;
		hw->idx = i;
		hw->nfp_idx = nfp_eth_table->ports[i].index;
		hw->is_phyport = true;

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
	ret = 0;
	goto eth_table_cleanup;

port_cleanup:
	for (i = 0; i < pf_dev->total_phyports; i++) {
		if (pf_dev->ports[i] && pf_dev->ports[i]->eth_dev) {
			struct rte_eth_dev *tmp_dev;
			tmp_dev = pf_dev->ports[i]->eth_dev;
			rte_eth_dev_release_port(tmp_dev);
			pf_dev->ports[i] = NULL;
		}
	}
eth_table_cleanup:
	free(nfp_eth_table);
error:
	return ret;
}

static int nfp_pf_init(struct rte_pci_device *pci_dev)
{
	struct nfp_pf_dev *pf_dev = NULL;
	struct nfp_cpp *cpp;
	struct nfp_hwinfo *hwinfo;
	struct nfp_rtsym_table *sym_tbl;
	struct nfp_eth_table *nfp_eth_table = NULL;
	char name[RTE_ETH_NAME_MAX_LEN];
	int total_ports;
	int ret = -ENODEV;
	int err;

	if (!pci_dev)
		return ret;

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

	if (!cpp) {
		PMD_INIT_LOG(ERR, "A CPP handle can not be obtained");
		ret = -EIO;
		goto error;
	}

	hwinfo = nfp_hwinfo_read(cpp);
	if (!hwinfo) {
		PMD_INIT_LOG(ERR, "Error reading hwinfo table");
		ret = -EIO;
		goto error;
	}

	nfp_eth_table = nfp_eth_read_ports(cpp);
	if (!nfp_eth_table) {
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
	if (!sym_tbl) {
		PMD_INIT_LOG(ERR, "Something is wrong with the firmware"
				" symbol table");
		ret = -EIO;
		goto eth_table_cleanup;
	}

	total_ports = nfp_rtsym_read_le(sym_tbl, "nfd_cfg_pf0_num_ports", &err);
	if (total_ports != (int)nfp_eth_table->count) {
		PMD_DRV_LOG(ERR, "Inconsistent number of ports");
		ret = -EIO;
		goto sym_tbl_cleanup;
	}

	PMD_INIT_LOG(INFO, "Total physical ports: %d", total_ports);

	if (total_ports <= 0 || total_ports > 8) {
		PMD_INIT_LOG(ERR, "nfd_cfg_pf0_num_ports symbol with wrong value");
		ret = -ENODEV;
		goto sym_tbl_cleanup;
	}
	/* Allocate memory for the PF "device" */
	snprintf(name, sizeof(name), "nfp_pf%d", 0);
	pf_dev = rte_zmalloc(name, sizeof(*pf_dev), 0);
	if (!pf_dev) {
		ret = -ENOMEM;
		goto sym_tbl_cleanup;
	}

	/* Populate the newly created PF device */
	pf_dev->cpp = cpp;
	pf_dev->hwinfo = hwinfo;
	pf_dev->sym_tbl = sym_tbl;
	pf_dev->total_phyports = total_ports;

	if (total_ports > 1)
		pf_dev->multiport = true;

	pf_dev->pci_dev = pci_dev;

	/* Map the symbol table */
	pf_dev->ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, "_pf0_net_bar0",
				     pf_dev->total_phyports * 32768,
				     &pf_dev->ctrl_area);
	if (!pf_dev->ctrl_bar) {
		PMD_INIT_LOG(ERR, "nfp_rtsym_map fails for _pf0_net_ctrl_bar");
		ret = -EIO;
		goto pf_cleanup;
	}

	PMD_INIT_LOG(DEBUG, "ctrl bar: %p", pf_dev->ctrl_bar);

	/* configure access to tx/rx vNIC BARs */
	pf_dev->hw_queues = nfp_cpp_map_area(pf_dev->cpp, 0, 0,
					      NFP_PCIE_QUEUE(0),
					      NFP_QCP_QUEUE_AREA_SZ,
					      &pf_dev->hwqueues_area);
	if (!pf_dev->hw_queues) {
		PMD_INIT_LOG(ERR, "nfp_rtsym_map fails for net.qc");
		ret = -EIO;
		goto ctrl_area_cleanup;
	}

	PMD_INIT_LOG(DEBUG, "tx/rx bar address: 0x%p", pf_dev->hw_queues);

	/* Initialize and prep physical ports now
	 * This will loop through all physical ports
	 */
	ret = nfp_init_phyports(pf_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Could not create physical ports");
		goto hwqueues_cleanup;
	}

	/* register the CPP bridge service here for primary use */
	nfp_register_cpp_service(pf_dev->cpp);

	return 0;

hwqueues_cleanup:
	nfp_cpp_area_free(pf_dev->hwqueues_area);
ctrl_area_cleanup:
	nfp_cpp_area_free(pf_dev->ctrl_area);
pf_cleanup:
	rte_free(pf_dev);
sym_tbl_cleanup:
	free(sym_tbl);
eth_table_cleanup:
	free(nfp_eth_table);
hwinfo_cleanup:
	free(hwinfo);
error:
	return ret;
}

/*
 * When attaching to the NFP4000/6000 PF on a secondary process there
 * is no need to initialize the PF again. Only minimal work is required
 * here
 */
static int nfp_pf_secondary_init(struct rte_pci_device *pci_dev)
{
	struct nfp_cpp *cpp;
	struct nfp_rtsym_table *sym_tbl;
	int total_ports;
	int i;
	int err;

	if (!pci_dev)
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

	if (!cpp) {
		PMD_INIT_LOG(ERR, "A CPP handle can not be obtained");
		return -EIO;
	}

	/*
	 * We don't have access to the PF created in the primary process
	 * here so we have to read the number of ports from firmware
	 */
	sym_tbl = nfp_rtsym_table_read(cpp);
	if (!sym_tbl) {
		PMD_INIT_LOG(ERR, "Something is wrong with the firmware"
				" symbol table");
		return -EIO;
	}

	total_ports = nfp_rtsym_read_le(sym_tbl, "nfd_cfg_pf0_num_ports", &err);

	for (i = 0; i < total_ports; i++) {
		struct rte_eth_dev *eth_dev;
		char port_name[RTE_ETH_NAME_MAX_LEN];

		snprintf(port_name, sizeof(port_name), "%s_port%d",
			 pci_dev->device.name, i);

		PMD_DRV_LOG(DEBUG, "Secondary attaching to port %s",
		    port_name);
		eth_dev = rte_eth_dev_attach_secondary(port_name);
		if (!eth_dev) {
			RTE_LOG(ERR, EAL,
			"secondary process attach failed, "
			"ethdev doesn't exist");
			return -ENODEV;
		}
		eth_dev->process_private = cpp;
		eth_dev->dev_ops = &nfp_net_eth_dev_ops;
		eth_dev->rx_queue_count = nfp_net_rx_queue_count;
		eth_dev->rx_pkt_burst = &nfp_net_recv_pkts;
		eth_dev->tx_pkt_burst = &nfp_net_xmit_pkts;
		rte_eth_dev_probing_finish(eth_dev);
	}

	/* Register the CPP bridge service for the secondary too */
	nfp_register_cpp_service(cpp);

	return 0;
}

static int nfp_pf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
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
			       PCI_DEVICE_ID_NFP4000_PF_NIC)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_NETRONOME,
			       PCI_DEVICE_ID_NFP6000_PF_NIC)
	},
	{
		.vendor_id = 0,
	},
};

static int nfp_pci_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	uint16_t port_id;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	if (pci_dev->id.device_id == PCI_DEVICE_ID_NFP4000_PF_NIC ||
	    pci_dev->id.device_id == PCI_DEVICE_ID_NFP6000_PF_NIC) {
		/* Free up all physical ports under PF */
		RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device)
			rte_eth_dev_close(port_id);
		/*
		 * Ports can be closed and freed but hotplugging is not
		 * currently supported
		 */
		return -ENOTSUP;
	}

	/* VF cleanup, just free private port data */
	return nfp_net_close(eth_dev);
}

static int eth_nfp_pci_remove(struct rte_pci_device *pci_dev)
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
RTE_LOG_REGISTER_SUFFIX(nfp_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(nfp_logtype_driver, driver, NOTICE);
/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
