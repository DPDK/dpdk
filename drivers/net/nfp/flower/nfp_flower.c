/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_service_component.h>
#include <rte_malloc.h>
#include <rte_alarm.h>
#include <ethdev_pci.h>
#include <ethdev_driver.h>

#include "../nfp_common.h"
#include "../nfp_logs.h"
#include "../nfp_ctrl.h"
#include "../nfp_cpp_bridge.h"
#include "../nfp_rxtx.h"
#include "../nfpcore/nfp_mip.h"
#include "../nfpcore/nfp_rtsym.h"
#include "../nfpcore/nfp_nsp.h"
#include "nfp_flower.h"
#include "nfp_flower_ctrl.h"
#include "nfp_flower_representor.h"

#define CTRL_VNIC_NB_DESC 512
#define DEFAULT_FLBUF_SIZE 9216

static void
nfp_pf_repr_enable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint64_t enabled_queues = 0;
	int i;
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;

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

static void
nfp_pf_repr_disable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint32_t new_ctrl;
	uint32_t update = 0;
	struct nfp_flower_representor *repr;

	repr = (struct nfp_flower_representor *)dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;

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

int
nfp_flower_pf_start(struct rte_eth_dev *dev)
{
	int ret;
	uint32_t new_ctrl;
	uint32_t update = 0;
	struct nfp_net_hw *hw;
	struct nfp_flower_representor *repr;

	repr = (struct nfp_flower_representor *)dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;

	/* Disabling queues just in case... */
	nfp_pf_repr_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_pf_repr_enable_queues(dev);

	new_ctrl = nfp_check_offloads(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(hw);

	update |= NFP_NET_CFG_UPDATE_RSS;

	if (hw->cap & NFP_NET_CFG_CTRL_RSS2)
		new_ctrl |= NFP_NET_CFG_CTRL_RSS2;
	else
		new_ctrl |= NFP_NET_CFG_CTRL_RSS;

	/* Enable device */
	new_ctrl |= NFP_NET_CFG_CTRL_ENABLE;

	update |= NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING;

	if (hw->cap & NFP_NET_CFG_CTRL_RINGCFG)
		new_ctrl |= NFP_NET_CFG_CTRL_RINGCFG;

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, new_ctrl);

	/* If an error when reconfig we avoid to change hw state */
	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to reconfig PF vnic");
		return -EIO;
	}

	hw->ctrl = new_ctrl;

	/* Setup the freelist ring */
	ret = nfp_net_rx_freelist_setup(dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Error with flower PF vNIC freelist setup");
		return -EIO;
	}

	return 0;
}

/* Stop device: disable rx and tx functions to allow for reconfiguring. */
int
nfp_flower_pf_stop(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_hw *hw;
	struct nfp_net_txq *this_tx_q;
	struct nfp_net_rxq *this_rx_q;
	struct nfp_flower_representor *repr;

	repr = (struct nfp_flower_representor *)dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;

	nfp_pf_repr_disable_queues(dev);

	/* Clear queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		this_tx_q = (struct nfp_net_txq *)dev->data->tx_queues[i];
		nfp_net_reset_tx_queue(this_tx_q);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		this_rx_q = (struct nfp_net_rxq *)dev->data->rx_queues[i];
		nfp_net_reset_rx_queue(this_rx_q);
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port down */
		nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 0);
	else
		nfp_eth_set_configured(dev->process_private, hw->nfp_idx, 0);

	return 0;
}

/* Reset and stop device. The device can not be restarted. */
static int
nfp_flower_pf_close(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_hw *hw;
	struct nfp_pf_dev *pf_dev;
	struct nfp_net_txq *this_tx_q;
	struct nfp_net_rxq *this_rx_q;
	struct nfp_flower_representor *repr;
	struct nfp_app_fw_flower *app_fw_flower;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	repr = (struct nfp_flower_representor *)dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;
	pf_dev = hw->pf_dev;
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(pf_dev->app_fw_priv);

	/*
	 * We assume that the DPDK application is stopping all the
	 * threads/queues before calling the device close function.
	 */
	nfp_pf_repr_disable_queues(dev);

	/* Clear queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		this_tx_q = (struct nfp_net_txq *)dev->data->tx_queues[i];
		nfp_net_reset_tx_queue(this_tx_q);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		this_rx_q = (struct nfp_net_rxq *)dev->data->rx_queues[i];
		nfp_net_reset_rx_queue(this_rx_q);
	}

	/* Cancel possible impending LSC work here before releasing the port*/
	rte_eal_alarm_cancel(nfp_net_dev_interrupt_delayed_handler, (void *)dev);

	nn_cfg_writeb(hw, NFP_NET_CFG_LSC, 0xff);

	rte_eth_dev_release_port(dev);

	/* Now it is safe to free all PF resources */
	PMD_DRV_LOG(INFO, "Freeing PF resources");
	nfp_cpp_area_free(pf_dev->ctrl_area);
	nfp_cpp_area_free(pf_dev->hwqueues_area);
	free(pf_dev->hwinfo);
	free(pf_dev->sym_tbl);
	nfp_cpp_free(pf_dev->cpp);
	rte_free(app_fw_flower);
	rte_free(pf_dev);

	return 0;
}

static const struct eth_dev_ops nfp_flower_pf_vnic_ops = {
	.dev_infos_get          = nfp_net_infos_get,
	.link_update            = nfp_net_link_update,
	.dev_configure          = nfp_net_configure,

	.dev_start              = nfp_flower_pf_start,
	.dev_stop               = nfp_flower_pf_stop,
	.dev_close              = nfp_flower_pf_close,
};

static int
nfp_flower_init_vnic_common(struct nfp_net_hw *hw, const char *vnic_type)
{
	uint32_t start_q;
	uint64_t rx_bar_off;
	uint64_t tx_bar_off;
	const int stride = 4;
	struct nfp_pf_dev *pf_dev;
	struct rte_pci_device *pci_dev;

	pf_dev = hw->pf_dev;
	pci_dev = hw->pf_dev->pci_dev;

	/* NFP can not handle DMA addresses requiring more than 40 bits */
	if (rte_mem_check_dma_mask(40)) {
		PMD_INIT_LOG(ERR, "Device %s can not be used: restricted dma mask to 40 bits!\n",
				pci_dev->device.name);
		return -ENODEV;
	};

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	PMD_INIT_LOG(DEBUG, "%s vNIC ctrl bar: %p", vnic_type, hw->ctrl_bar);

	/* Read the number of available rx/tx queues from hardware */
	hw->max_rx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_RXRINGS);
	hw->max_tx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_TXRINGS);

	/* Work out where in the BAR the queues start */
	start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_TXQ);
	tx_bar_off = (uint64_t)start_q * NFP_QCP_QUEUE_ADDR_SZ;
	start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_RXQ);
	rx_bar_off = (uint64_t)start_q * NFP_QCP_QUEUE_ADDR_SZ;

	hw->tx_bar = pf_dev->hw_queues + tx_bar_off;
	hw->rx_bar = pf_dev->hw_queues + rx_bar_off;

	/* Get some of the read-only fields from the config BAR */
	hw->ver = nn_cfg_readl(hw, NFP_NET_CFG_VERSION);
	hw->cap = nn_cfg_readl(hw, NFP_NET_CFG_CAP);
	hw->max_mtu = nn_cfg_readl(hw, NFP_NET_CFG_MAX_MTU);
	/* Set the current MTU to the maximum supported */
	hw->mtu = hw->max_mtu;
	hw->flbufsz = DEFAULT_FLBUF_SIZE;

	/* read the Rx offset configured from firmware */
	if (NFD_CFG_MAJOR_VERSION_of(hw->ver) < 2)
		hw->rx_offset = NFP_NET_RX_OFFSET;
	else
		hw->rx_offset = nn_cfg_readl(hw, NFP_NET_CFG_RX_OFFSET_ADDR);

	hw->ctrl = 0;
	hw->stride_rx = stride;
	hw->stride_tx = stride;

	/* Reuse cfg queue setup function */
	nfp_net_cfg_queue_setup(hw);

	PMD_INIT_LOG(INFO, "%s vNIC max_rx_queues: %u, max_tx_queues: %u",
			vnic_type, hw->max_rx_queues, hw->max_tx_queues);

	/* Initializing spinlock for reconfigs */
	rte_spinlock_init(&hw->reconfig_lock);

	return 0;
}

static int
nfp_flower_init_ctrl_vnic(struct nfp_net_hw *hw)
{
	uint32_t i;
	int ret = 0;
	uint16_t n_txq;
	uint16_t n_rxq;
	unsigned int numa_node;
	struct rte_mempool *mp;
	struct nfp_net_rxq *rxq;
	struct nfp_net_txq *txq;
	struct nfp_pf_dev *pf_dev;
	struct rte_eth_dev *eth_dev;
	const struct rte_memzone *tz;
	struct nfp_app_fw_flower *app_fw_flower;

	/* Set up some pointers here for ease of use */
	pf_dev = hw->pf_dev;
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(pf_dev->app_fw_priv);

	ret = nfp_flower_init_vnic_common(hw, "ctrl_vnic");
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not init pf vnic");
		return -EINVAL;
	}

	/* Allocate memory for the eth_dev of the vNIC */
	hw->eth_dev = rte_zmalloc("nfp_ctrl_vnic",
			sizeof(struct rte_eth_dev), RTE_CACHE_LINE_SIZE);
	if (hw->eth_dev == NULL) {
		PMD_INIT_LOG(ERR, "Could not allocate ctrl vnic");
		return -ENOMEM;
	}

	/* Grab the pointer to the newly created rte_eth_dev here */
	eth_dev = hw->eth_dev;

	/* Also allocate memory for the data part of the eth_dev */
	eth_dev->data = rte_zmalloc("nfp_ctrl_vnic_data",
			sizeof(struct rte_eth_dev_data), RTE_CACHE_LINE_SIZE);
	if (eth_dev->data == NULL) {
		PMD_INIT_LOG(ERR, "Could not allocate ctrl vnic data");
		ret = -ENOMEM;
		goto eth_dev_cleanup;
	}

	/* Create a mbuf pool for the ctrl vNIC */
	numa_node = rte_socket_id();
	app_fw_flower->ctrl_pktmbuf_pool = rte_pktmbuf_pool_create("ctrl_mbuf_pool",
			4 * CTRL_VNIC_NB_DESC, 64, 0, 9216, numa_node);
	if (app_fw_flower->ctrl_pktmbuf_pool == NULL) {
		PMD_INIT_LOG(ERR, "Create mbuf pool for ctrl vnic failed");
		ret = -ENOMEM;
		goto dev_data_cleanup;
	}

	mp = app_fw_flower->ctrl_pktmbuf_pool;

	/* Configure the ctrl vNIC device */
	n_rxq = hw->max_rx_queues;
	n_txq = hw->max_tx_queues;
	eth_dev->data->rx_queues = rte_zmalloc("ethdev->rx_queues",
			sizeof(eth_dev->data->rx_queues[0]) * n_rxq,
			RTE_CACHE_LINE_SIZE);
	if (eth_dev->data->rx_queues == NULL) {
		PMD_INIT_LOG(ERR, "rte_zmalloc failed for ctrl vNIC rx queues");
		ret = -ENOMEM;
		goto mempool_cleanup;
	}

	eth_dev->data->tx_queues = rte_zmalloc("ethdev->tx_queues",
			sizeof(eth_dev->data->tx_queues[0]) * n_txq,
			RTE_CACHE_LINE_SIZE);
	if (eth_dev->data->tx_queues == NULL) {
		PMD_INIT_LOG(ERR, "rte_zmalloc failed for ctrl vNIC tx queues");
		ret = -ENOMEM;
		goto rx_queue_free;
	}

	/* Fill in some of the eth_dev fields */
	eth_dev->device = &pf_dev->pci_dev->device;
	eth_dev->data->nb_tx_queues = n_rxq;
	eth_dev->data->nb_rx_queues = n_txq;
	eth_dev->data->dev_private = hw;

	/* Set up the Rx queues */
	for (i = 0; i < n_rxq; i++) {
		rxq = rte_zmalloc_socket("ethdev RX queue",
				sizeof(struct nfp_net_rxq), RTE_CACHE_LINE_SIZE,
				numa_node);
		if (rxq == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating rxq");
			ret = -ENOMEM;
			goto rx_queue_setup_cleanup;
		}

		eth_dev->data->rx_queues[i] = rxq;

		/* Hw queues mapping based on firmware configuration */
		rxq->qidx = i;
		rxq->fl_qcidx = i * hw->stride_rx;
		rxq->rx_qcidx = rxq->fl_qcidx + (hw->stride_rx - 1);
		rxq->qcp_fl = hw->rx_bar + NFP_QCP_QUEUE_OFF(rxq->fl_qcidx);
		rxq->qcp_rx = hw->rx_bar + NFP_QCP_QUEUE_OFF(rxq->rx_qcidx);

		/*
		 * Tracking mbuf size for detecting a potential mbuf overflow due to
		 * RX offset
		 */
		rxq->mem_pool = mp;
		rxq->mbuf_size = rxq->mem_pool->elt_size;
		rxq->mbuf_size -= (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM);
		hw->flbufsz = rxq->mbuf_size;

		rxq->rx_count = CTRL_VNIC_NB_DESC;
		rxq->rx_free_thresh = DEFAULT_RX_FREE_THRESH;
		rxq->drop_en = 1;

		/*
		 * Allocate RX ring hardware descriptors. A memzone large enough to
		 * handle the maximum ring size is allocated in order to allow for
		 * resizing in later calls to the queue setup function.
		 */
		tz = rte_eth_dma_zone_reserve(eth_dev, "ctrl_rx_ring", i,
				sizeof(struct nfp_net_rx_desc) * NFP_NET_MAX_RX_DESC,
				NFP_MEMZONE_ALIGN, numa_node);
		if (tz == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating rx dma");
			rte_free(rxq);
			ret = -ENOMEM;
			goto rx_queue_setup_cleanup;
		}

		/* Saving physical and virtual addresses for the RX ring */
		rxq->dma = (uint64_t)tz->iova;
		rxq->rxds = (struct nfp_net_rx_desc *)tz->addr;

		/* Mbuf pointers array for referencing mbufs linked to RX descriptors */
		rxq->rxbufs = rte_zmalloc_socket("rxq->rxbufs",
				sizeof(*rxq->rxbufs) * CTRL_VNIC_NB_DESC,
				RTE_CACHE_LINE_SIZE, numa_node);
		if (rxq->rxbufs == NULL) {
			rte_eth_dma_zone_free(eth_dev, "ctrl_rx_ring", i);
			rte_free(rxq);
			ret = -ENOMEM;
			goto rx_queue_setup_cleanup;
		}

		nfp_net_reset_rx_queue(rxq);

		rxq->hw = hw;

		/*
		 * Telling the HW about the physical address of the RX ring and number
		 * of descriptors in log2 format
		 */
		nn_cfg_writeq(hw, NFP_NET_CFG_RXR_ADDR(i), rxq->dma);
		nn_cfg_writeb(hw, NFP_NET_CFG_RXR_SZ(i), rte_log2_u32(CTRL_VNIC_NB_DESC));
	}

	/* Set up the Tx queues */
	for (i = 0; i < n_txq; i++) {
		txq = rte_zmalloc_socket("ethdev TX queue",
				sizeof(struct nfp_net_txq), RTE_CACHE_LINE_SIZE,
				numa_node);
		if (txq == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating txq");
			ret = -ENOMEM;
			goto tx_queue_setup_cleanup;
		}

		eth_dev->data->tx_queues[i] = txq;

		/*
		 * Allocate TX ring hardware descriptors. A memzone large enough to
		 * handle the maximum ring size is allocated in order to allow for
		 * resizing in later calls to the queue setup function.
		 */
		tz = rte_eth_dma_zone_reserve(eth_dev, "ctrl_tx_ring", i,
				sizeof(struct nfp_net_nfd3_tx_desc) * NFP_NET_MAX_TX_DESC,
				NFP_MEMZONE_ALIGN, numa_node);
		if (tz == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating tx dma");
			rte_free(txq);
			ret = -ENOMEM;
			goto tx_queue_setup_cleanup;
		}

		txq->tx_count = CTRL_VNIC_NB_DESC;
		txq->tx_free_thresh = DEFAULT_RX_FREE_THRESH;
		txq->tx_pthresh = DEFAULT_TX_PTHRESH;
		txq->tx_hthresh = DEFAULT_TX_HTHRESH;
		txq->tx_wthresh = DEFAULT_TX_WTHRESH;

		/* Queue mapping based on firmware configuration */
		txq->qidx = i;
		txq->tx_qcidx = i * hw->stride_tx;
		txq->qcp_q = hw->tx_bar + NFP_QCP_QUEUE_OFF(txq->tx_qcidx);

		/* Saving physical and virtual addresses for the TX ring */
		txq->dma = (uint64_t)tz->iova;
		txq->txds = (struct nfp_net_nfd3_tx_desc *)tz->addr;

		/* Mbuf pointers array for referencing mbufs linked to TX descriptors */
		txq->txbufs = rte_zmalloc_socket("txq->txbufs",
				sizeof(*txq->txbufs) * CTRL_VNIC_NB_DESC,
				RTE_CACHE_LINE_SIZE, numa_node);
		if (txq->txbufs == NULL) {
			rte_eth_dma_zone_free(eth_dev, "ctrl_tx_ring", i);
			rte_free(txq);
			ret = -ENOMEM;
			goto tx_queue_setup_cleanup;
		}

		nfp_net_reset_tx_queue(txq);

		txq->hw = hw;

		/*
		 * Telling the HW about the physical address of the TX ring and number
		 * of descriptors in log2 format
		 */
		nn_cfg_writeq(hw, NFP_NET_CFG_TXR_ADDR(i), txq->dma);
		nn_cfg_writeb(hw, NFP_NET_CFG_TXR_SZ(i), rte_log2_u32(CTRL_VNIC_NB_DESC));
	}

	return 0;

tx_queue_setup_cleanup:
	for (i = 0; i < hw->max_tx_queues; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq != NULL) {
			rte_free(txq->txbufs);
			rte_eth_dma_zone_free(eth_dev, "ctrl_tx_ring", i);
			rte_free(txq);
		}
	}
rx_queue_setup_cleanup:
	for (i = 0; i < hw->max_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq != NULL) {
			rte_free(rxq->rxbufs);
			rte_eth_dma_zone_free(eth_dev, "ctrl_rx_ring", i);
			rte_free(rxq);
		}
	}
	rte_free(eth_dev->data->tx_queues);
rx_queue_free:
	rte_free(eth_dev->data->rx_queues);
mempool_cleanup:
	rte_mempool_free(mp);
dev_data_cleanup:
	rte_free(eth_dev->data);
eth_dev_cleanup:
	rte_free(eth_dev);

	return ret;
}

static void
nfp_flower_cleanup_ctrl_vnic(struct nfp_net_hw *hw)
{
	uint32_t i;
	struct nfp_net_rxq *rxq;
	struct nfp_net_txq *txq;
	struct rte_eth_dev *eth_dev;
	struct nfp_app_fw_flower *app_fw_flower;

	eth_dev = hw->eth_dev;
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(hw->pf_dev->app_fw_priv);

	for (i = 0; i < hw->max_tx_queues; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq != NULL) {
			rte_free(txq->txbufs);
			rte_eth_dma_zone_free(eth_dev, "ctrl_tx_ring", i);
			rte_free(txq);
		}
	}

	for (i = 0; i < hw->max_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq != NULL) {
			rte_free(rxq->rxbufs);
			rte_eth_dma_zone_free(eth_dev, "ctrl_rx_ring", i);
			rte_free(rxq);
		}
	}

	rte_free(eth_dev->data->tx_queues);
	rte_free(eth_dev->data->rx_queues);
	rte_mempool_free(app_fw_flower->ctrl_pktmbuf_pool);
	rte_free(eth_dev->data);
	rte_free(eth_dev);
}

static int
nfp_flower_start_ctrl_vnic(struct nfp_net_hw *hw)
{
	int ret;
	uint32_t update;
	uint32_t new_ctrl;
	struct rte_eth_dev *dev;

	dev = hw->eth_dev;

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(hw);

	new_ctrl = NFP_NET_CFG_CTRL_ENABLE;
	update = NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING |
			NFP_NET_CFG_UPDATE_MSIX;

	rte_wmb();

	/* If an error when reconfig we avoid to change hw state */
	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to reconfig ctrl vnic");
		return -EIO;
	}

	hw->ctrl = new_ctrl;

	/* Setup the freelist ring */
	ret = nfp_net_rx_freelist_setup(dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Error with flower ctrl vNIC freelist setup");
		return -EIO;
	}

	return 0;
}

static int
nfp_flower_ctrl_vnic_service(void *arg)
{
	struct nfp_app_fw_flower *app_fw_flower = arg;

	nfp_flower_ctrl_vnic_poll(app_fw_flower);

	return 0;
}

static int
nfp_flower_enable_services(struct nfp_app_fw_flower *app_fw_flower)
{
	int ret;
	uint32_t service_id;
	const struct rte_service_spec flower_service = {
		.name              = "flower_ctrl_vnic_service",
		.callback          = nfp_flower_ctrl_vnic_service,
		.callback_userdata = (void *)app_fw_flower,
	};

	/* Register the flower services */
	ret = rte_service_component_register(&flower_service, &service_id);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not register %s", flower_service.name);
		return -EINVAL;
	}

	PMD_INIT_LOG(INFO, "%s registered", flower_service.name);

	/* Map them to available service cores*/
	ret = nfp_map_service(service_id);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not map %s", flower_service.name);
		return -EINVAL;
	}

	return 0;
}

int
nfp_init_app_fw_flower(struct nfp_pf_dev *pf_dev)
{
	int ret;
	unsigned int numa_node;
	struct nfp_net_hw *pf_hw;
	struct nfp_net_hw *ctrl_hw;
	struct nfp_app_fw_flower *app_fw_flower;

	numa_node = rte_socket_id();

	/* Allocate memory for the Flower app */
	app_fw_flower = rte_zmalloc_socket("nfp_app_fw_flower", sizeof(*app_fw_flower),
			RTE_CACHE_LINE_SIZE, numa_node);
	if (app_fw_flower == NULL) {
		PMD_INIT_LOG(ERR, "Could not malloc app fw flower");
		return -ENOMEM;
	}

	pf_dev->app_fw_priv = app_fw_flower;

	/* Allocate memory for the PF AND ctrl vNIC here (hence the * 2) */
	pf_hw = rte_zmalloc_socket("nfp_pf_vnic", 2 * sizeof(struct nfp_net_adapter),
			RTE_CACHE_LINE_SIZE, numa_node);
	if (pf_hw == NULL) {
		PMD_INIT_LOG(ERR, "Could not malloc nfp pf vnic");
		ret = -ENOMEM;
		goto app_cleanup;
	}

	/* Map the PF ctrl bar */
	pf_dev->ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, "_pf0_net_bar0",
			32768, &pf_dev->ctrl_area);
	if (pf_dev->ctrl_bar == NULL) {
		PMD_INIT_LOG(ERR, "Cloud not map the PF vNIC ctrl bar");
		ret = -ENODEV;
		goto vnic_cleanup;
	}

	/* Fill in the PF vNIC and populate app struct */
	app_fw_flower->pf_hw = pf_hw;
	pf_hw->ctrl_bar = pf_dev->ctrl_bar;
	pf_hw->pf_dev = pf_dev;
	pf_hw->cpp = pf_dev->cpp;

	ret = nfp_flower_init_vnic_common(app_fw_flower->pf_hw, "pf_vnic");
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not initialize flower PF vNIC");
		goto pf_cpp_area_cleanup;
	}

	/* The ctrl vNIC struct comes directly after the PF one */
	app_fw_flower->ctrl_hw = pf_hw + 1;
	ctrl_hw = app_fw_flower->ctrl_hw;

	/* Map the ctrl vNIC ctrl bar */
	ctrl_hw->ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, "_pf0_net_ctrl_bar",
			32768, &ctrl_hw->ctrl_area);
	if (ctrl_hw->ctrl_bar == NULL) {
		PMD_INIT_LOG(ERR, "Cloud not map the ctrl vNIC ctrl bar");
		ret = -ENODEV;
		goto pf_cpp_area_cleanup;
	}

	/* Now populate the ctrl vNIC */
	ctrl_hw->pf_dev = pf_dev;
	ctrl_hw->cpp = pf_dev->cpp;

	ret = nfp_flower_init_ctrl_vnic(app_fw_flower->ctrl_hw);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not initialize flower ctrl vNIC");
		goto ctrl_cpp_area_cleanup;
	}

	/* Start the ctrl vNIC */
	ret = nfp_flower_start_ctrl_vnic(app_fw_flower->ctrl_hw);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not start flower ctrl vNIC");
		goto ctrl_vnic_cleanup;
	}

	/* Start up flower services */
	ret = nfp_flower_enable_services(app_fw_flower);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not enable flower services");
		ret = -ESRCH;
		goto ctrl_vnic_cleanup;
	}

	ret = nfp_flower_repr_create(app_fw_flower);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not create representor ports");
		goto ctrl_vnic_cleanup;
	}

	return 0;

ctrl_vnic_cleanup:
	nfp_flower_cleanup_ctrl_vnic(app_fw_flower->ctrl_hw);
ctrl_cpp_area_cleanup:
	nfp_cpp_area_free(ctrl_hw->ctrl_area);
pf_cpp_area_cleanup:
	nfp_cpp_area_free(pf_dev->ctrl_area);
vnic_cleanup:
	rte_free(pf_hw);
app_cleanup:
	rte_free(app_fw_flower);

	return ret;
}

int
nfp_secondary_init_app_fw_flower(struct nfp_cpp *cpp)
{
	struct rte_eth_dev *eth_dev;
	const char *port_name = "pf_vnic_eth_dev";

	PMD_INIT_LOG(DEBUG, "Secondary attaching to port %s", port_name);

	eth_dev = rte_eth_dev_attach_secondary(port_name);
	if (eth_dev == NULL) {
		PMD_INIT_LOG(ERR, "Secondary process attach to port %s failed", port_name);
		return -ENODEV;
	}

	eth_dev->process_private = cpp;
	eth_dev->dev_ops = &nfp_flower_pf_vnic_ops;
	rte_eth_dev_probing_finish(eth_dev);

	return 0;
}
