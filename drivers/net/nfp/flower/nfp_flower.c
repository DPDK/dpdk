/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_flower.h"

#include <rte_alarm.h>
#include <rte_malloc.h>
#include <rte_service_component.h>

#include "../nfd3/nfp_nfd3.h"
#include "../nfdk/nfp_nfdk.h"
#include "../nfpcore/nfp_nsp.h"
#include "../nfpcore/nfp_rtsym.h"
#include "../nfp_cpp_bridge.h"
#include "../nfp_logs.h"
#include "../nfp_mtr.h"
#include "nfp_flower_ctrl.h"
#include "nfp_flower_representor.h"

#define CTRL_VNIC_NB_DESC 512

int
nfp_flower_pf_start(struct rte_eth_dev *dev)
{
	int ret;
	uint16_t i;
	struct nfp_hw *hw;
	uint32_t new_ctrl;
	uint32_t update = 0;
	struct nfp_net_hw *net_hw;
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	net_hw = repr->app_fw_flower->pf_hw;
	hw = &net_hw->super;

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	new_ctrl = nfp_check_offloads(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(net_hw);

	update |= NFP_NET_CFG_UPDATE_RSS;
	new_ctrl |= nfp_net_cfg_ctrl_rss(hw->cap);

	/* Enable device */
	new_ctrl |= NFP_NET_CFG_CTRL_ENABLE;

	update |= NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING;

	if ((hw->cap & NFP_NET_CFG_CTRL_RINGCFG) != 0)
		new_ctrl |= NFP_NET_CFG_CTRL_RINGCFG;

	/* If an error when reconfig we avoid to change hw state */
	ret = nfp_reconfig(hw, new_ctrl, update);
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

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

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

	repr = dev->data->dev_private;
	hw = repr->app_fw_flower->pf_hw;
	pf_dev = hw->pf_dev;
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(pf_dev->app_fw_priv);

	nfp_mtr_priv_uninit(pf_dev);

	/*
	 * We assume that the DPDK application is stopping all the
	 * threads/queues before calling the device close function.
	 */
	nfp_net_disable_queues(dev);

	/* Clear queues */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		this_tx_q = dev->data->tx_queues[i];
		nfp_net_reset_tx_queue(this_tx_q);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		this_rx_q = dev->data->rx_queues[i];
		nfp_net_reset_rx_queue(this_rx_q);
	}

	/* Cancel possible impending LSC work here before releasing the port */
	rte_eal_alarm_cancel(nfp_net_dev_interrupt_delayed_handler, (void *)dev);

	nn_cfg_writeb(&hw->super, NFP_NET_CFG_LSC, 0xff);

	/* Now it is safe to free all PF resources */
	PMD_DRV_LOG(INFO, "Freeing PF resources");
	nfp_cpp_area_free(pf_dev->ctrl_area);
	nfp_cpp_area_free(pf_dev->qc_area);
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
	.dev_stop               = nfp_net_stop,
	.dev_close              = nfp_flower_pf_close,
};

static inline struct nfp_flower_representor *
nfp_flower_get_repr(struct nfp_net_hw *hw,
		uint32_t port_id)
{
	uint8_t port;
	struct nfp_app_fw_flower *app_fw_flower;

	/* Obtain handle to app_fw_flower here */
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(hw->pf_dev->app_fw_priv);

	switch (NFP_FLOWER_CMSG_PORT_TYPE(port_id)) {
	case NFP_FLOWER_CMSG_PORT_TYPE_PHYS_PORT:
		port =  NFP_FLOWER_CMSG_PORT_PHYS_PORT_NUM(port_id);
		return app_fw_flower->phy_reprs[port];
	case NFP_FLOWER_CMSG_PORT_TYPE_PCIE_PORT:
		port = NFP_FLOWER_CMSG_PORT_VNIC(port_id);
		return app_fw_flower->vf_reprs[port];
	default:
		break;
	}

	return NULL;
}

bool
nfp_flower_pf_dispatch_pkts(struct nfp_net_hw *hw,
		struct rte_mbuf *mbuf,
		uint32_t port_id)
{
	struct nfp_flower_representor *repr;

	repr = nfp_flower_get_repr(hw, port_id);
	if (repr == NULL) {
		PMD_RX_LOG(ERR, "Can not get repr for port %u", hw->idx);
		return false;
	}

	if (repr->ring == NULL) {
		PMD_RX_LOG(ERR, "No ring available for repr_port %s", repr->name);
		return false;
	}

	rte_ring_enqueue(repr->ring, (void *)mbuf);
	return true;
}

static uint16_t
nfp_flower_pf_nfd3_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	return nfp_net_nfd3_xmit_pkts_common(tx_queue, tx_pkts, nb_pkts, true);
}

static uint16_t
nfp_flower_pf_nfdk_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	return nfp_net_nfdk_xmit_pkts_common(tx_queue, tx_pkts, nb_pkts, true);
}

static void
nfp_flower_pf_xmit_pkts_register(struct nfp_app_fw_flower *app_fw_flower)
{
	struct nfp_net_hw *hw;
	struct nfp_flower_nfd_func *nfd_func;

	hw = app_fw_flower->pf_hw;
	nfd_func = &app_fw_flower->nfd_func;

	if (hw->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3)
		nfd_func->pf_xmit_t = nfp_flower_pf_nfd3_xmit_pkts;
	else
		nfd_func->pf_xmit_t = nfp_flower_pf_nfdk_xmit_pkts;
}

uint16_t
nfp_flower_pf_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	struct nfp_net_txq *txq;
	struct nfp_app_fw_flower *app_fw_flower;

	txq = tx_queue;
	app_fw_flower = txq->hw->pf_dev->app_fw_priv;

	return app_fw_flower->nfd_func.pf_xmit_t(tx_queue, tx_pkts, nb_pkts);
}

static int
nfp_flower_init_vnic_common(struct nfp_net_hw *hw,
		const char *vnic_type)
{
	int err;
	uint32_t start_q;
	uint64_t rx_bar_off;
	uint64_t tx_bar_off;
	struct nfp_pf_dev *pf_dev;
	struct rte_pci_device *pci_dev;

	pf_dev = hw->pf_dev;
	pci_dev = hw->pf_dev->pci_dev;

	PMD_INIT_LOG(DEBUG, "%s vNIC ctrl bar: %p", vnic_type, hw->super.ctrl_bar);

	err = nfp_net_common_init(pci_dev, hw);
	if (err != 0)
		return err;

	/* Work out where in the BAR the queues start */
	start_q = nn_cfg_readl(&hw->super, NFP_NET_CFG_START_TXQ);
	tx_bar_off = (uint64_t)start_q * NFP_QCP_QUEUE_ADDR_SZ;
	start_q = nn_cfg_readl(&hw->super, NFP_NET_CFG_START_RXQ);
	rx_bar_off = (uint64_t)start_q * NFP_QCP_QUEUE_ADDR_SZ;

	hw->tx_bar = pf_dev->qc_bar + tx_bar_off;
	hw->rx_bar = pf_dev->qc_bar + rx_bar_off;

	/* Set the current MTU to the maximum supported */
	hw->mtu = hw->max_mtu;

	/* Reuse cfg queue setup function */
	nfp_net_cfg_queue_setup(hw);

	PMD_INIT_LOG(INFO, "%s vNIC max_rx_queues: %u, max_tx_queues: %u",
			vnic_type, hw->max_rx_queues, hw->max_tx_queues);

	/* Initializing spinlock for reconfigs */
	rte_spinlock_init(&hw->super.reconfig_lock);

	return 0;
}

static int
nfp_flower_init_ctrl_vnic(struct nfp_net_hw *hw)
{
	uint16_t i;
	int ret = 0;
	uint16_t n_txq;
	uint16_t n_rxq;
	const char *pci_name;
	unsigned int numa_node;
	struct rte_mempool *mp;
	struct nfp_net_rxq *rxq;
	struct nfp_net_txq *txq;
	struct nfp_pf_dev *pf_dev;
	struct rte_eth_dev *eth_dev;
	const struct rte_memzone *tz;
	struct nfp_app_fw_flower *app_fw_flower;
	char ctrl_rxring_name[RTE_MEMZONE_NAMESIZE];
	char ctrl_txring_name[RTE_MEMZONE_NAMESIZE];
	char ctrl_pktmbuf_pool_name[RTE_MEMZONE_NAMESIZE];

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

	pci_name = strchr(pf_dev->pci_dev->name, ':') + 1;

	/* Create a mbuf pool for the ctrl vNIC */
	numa_node = rte_socket_id();
	snprintf(ctrl_pktmbuf_pool_name, sizeof(ctrl_pktmbuf_pool_name),
			"%s_ctrlmp", pci_name);
	app_fw_flower->ctrl_pktmbuf_pool =
			rte_pktmbuf_pool_create(ctrl_pktmbuf_pool_name,
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

	snprintf(ctrl_rxring_name, sizeof(ctrl_rxring_name), "%s_ctrx_ring", pci_name);
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
		rxq->qcp_fl = hw->rx_bar + NFP_QCP_QUEUE_OFF(rxq->fl_qcidx);

		/*
		 * Tracking mbuf size for detecting a potential mbuf overflow due to
		 * RX offset.
		 */
		rxq->mem_pool = mp;
		rxq->mbuf_size = rxq->mem_pool->elt_size;
		rxq->mbuf_size -= (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM);
		hw->flbufsz = rxq->mbuf_size;

		rxq->rx_count = CTRL_VNIC_NB_DESC;
		rxq->rx_free_thresh = DEFAULT_RX_FREE_THRESH;

		/*
		 * Allocate RX ring hardware descriptors. A memzone large enough to
		 * handle the maximum ring size is allocated in order to allow for
		 * resizing in later calls to the queue setup function.
		 */
		tz = rte_eth_dma_zone_reserve(eth_dev, ctrl_rxring_name, i,
				sizeof(struct nfp_net_rx_desc) *
				hw->dev_info->max_qc_size,
				NFP_MEMZONE_ALIGN, numa_node);
		if (tz == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating rx dma");
			rte_free(rxq);
			ret = -ENOMEM;
			goto rx_queue_setup_cleanup;
		}

		/* Saving physical and virtual addresses for the RX ring */
		rxq->dma = (uint64_t)tz->iova;
		rxq->rxds = tz->addr;

		/* Mbuf pointers array for referencing mbufs linked to RX descriptors */
		rxq->rxbufs = rte_zmalloc_socket("rxq->rxbufs",
				sizeof(*rxq->rxbufs) * CTRL_VNIC_NB_DESC,
				RTE_CACHE_LINE_SIZE, numa_node);
		if (rxq->rxbufs == NULL) {
			rte_eth_dma_zone_free(eth_dev, ctrl_rxring_name, i);
			rte_free(rxq);
			ret = -ENOMEM;
			goto rx_queue_setup_cleanup;
		}

		nfp_net_reset_rx_queue(rxq);

		rxq->hw = hw;

		/*
		 * Telling the HW about the physical address of the RX ring and number
		 * of descriptors in log2 format.
		 */
		nn_cfg_writeq(&hw->super, NFP_NET_CFG_RXR_ADDR(i), rxq->dma);
		nn_cfg_writeb(&hw->super, NFP_NET_CFG_RXR_SZ(i), rte_log2_u32(CTRL_VNIC_NB_DESC));
	}

	snprintf(ctrl_txring_name, sizeof(ctrl_txring_name), "%s_cttx_ring", pci_name);
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
		tz = rte_eth_dma_zone_reserve(eth_dev, ctrl_txring_name, i,
				sizeof(struct nfp_net_nfd3_tx_desc) *
				hw->dev_info->max_qc_size,
				NFP_MEMZONE_ALIGN, numa_node);
		if (tz == NULL) {
			PMD_DRV_LOG(ERR, "Error allocating tx dma");
			rte_free(txq);
			ret = -ENOMEM;
			goto tx_queue_setup_cleanup;
		}

		txq->tx_count = CTRL_VNIC_NB_DESC;
		txq->tx_free_thresh = DEFAULT_RX_FREE_THRESH;

		/* Queue mapping based on firmware configuration */
		txq->qidx = i;
		txq->tx_qcidx = i * hw->stride_tx;
		txq->qcp_q = hw->tx_bar + NFP_QCP_QUEUE_OFF(txq->tx_qcidx);

		/* Saving physical and virtual addresses for the TX ring */
		txq->dma = (uint64_t)tz->iova;
		txq->txds = tz->addr;

		/* Mbuf pointers array for referencing mbufs linked to TX descriptors */
		txq->txbufs = rte_zmalloc_socket("txq->txbufs",
				sizeof(*txq->txbufs) * CTRL_VNIC_NB_DESC,
				RTE_CACHE_LINE_SIZE, numa_node);
		if (txq->txbufs == NULL) {
			rte_eth_dma_zone_free(eth_dev, ctrl_txring_name, i);
			rte_free(txq);
			ret = -ENOMEM;
			goto tx_queue_setup_cleanup;
		}

		nfp_net_reset_tx_queue(txq);

		txq->hw = hw;

		/*
		 * Telling the HW about the physical address of the TX ring and number
		 * of descriptors in log2 format.
		 */
		nn_cfg_writeq(&hw->super, NFP_NET_CFG_TXR_ADDR(i), txq->dma);
		nn_cfg_writeb(&hw->super, NFP_NET_CFG_TXR_SZ(i), rte_log2_u32(CTRL_VNIC_NB_DESC));
	}

	return 0;

tx_queue_setup_cleanup:
	for (i = 0; i < hw->max_tx_queues; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq != NULL) {
			rte_free(txq->txbufs);
			rte_eth_dma_zone_free(eth_dev, ctrl_txring_name, i);
			rte_free(txq);
		}
	}
rx_queue_setup_cleanup:
	for (i = 0; i < hw->max_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq != NULL) {
			rte_free(rxq->rxbufs);
			rte_eth_dma_zone_free(eth_dev, ctrl_rxring_name, i);
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
	const char *pci_name;
	struct nfp_net_rxq *rxq;
	struct nfp_net_txq *txq;
	struct rte_eth_dev *eth_dev;
	struct nfp_app_fw_flower *app_fw_flower;
	char ctrl_txring_name[RTE_MEMZONE_NAMESIZE];
	char ctrl_rxring_name[RTE_MEMZONE_NAMESIZE];

	eth_dev = hw->eth_dev;
	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(hw->pf_dev->app_fw_priv);

	pci_name = strchr(app_fw_flower->pf_hw->pf_dev->pci_dev->name, ':') + 1;

	snprintf(ctrl_txring_name, sizeof(ctrl_txring_name), "%s_cttx_ring", pci_name);
	for (i = 0; i < hw->max_tx_queues; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq != NULL) {
			rte_free(txq->txbufs);
			rte_eth_dma_zone_free(eth_dev, ctrl_txring_name, i);
			rte_free(txq);
		}
	}

	snprintf(ctrl_rxring_name, sizeof(ctrl_rxring_name), "%s_ctrx_ring", pci_name);
	for (i = 0; i < hw->max_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq != NULL) {
			rte_free(rxq->rxbufs);
			rte_eth_dma_zone_free(eth_dev, ctrl_rxring_name, i);
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
nfp_flower_start_ctrl_vnic(struct nfp_net_hw *net_hw)
{
	int ret;
	uint32_t update;
	uint32_t new_ctrl;
	struct nfp_hw *hw;
	struct rte_eth_dev *dev;

	dev = net_hw->eth_dev;
	hw = &net_hw->super;

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(net_hw);

	new_ctrl = NFP_NET_CFG_CTRL_ENABLE;
	update = NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING |
			NFP_NET_CFG_UPDATE_MSIX;

	rte_wmb();

	/* If an error when reconfig we avoid to change hw state */
	ret = nfp_reconfig(hw, new_ctrl, update);
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

	app_fw_flower->ctrl_vnic_id = service_id;
	PMD_INIT_LOG(INFO, "%s registered", flower_service.name);

	/* Map them to available service cores */
	ret = nfp_map_service(service_id);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not map %s", flower_service.name);
		return -EINVAL;
	}

	return 0;
}

static void
nfp_flower_pkt_add_metadata_register(struct nfp_app_fw_flower *app_fw_flower)
{
	struct nfp_net_hw *hw;
	struct nfp_flower_nfd_func *nfd_func;

	hw = app_fw_flower->pf_hw;
	nfd_func = &app_fw_flower->nfd_func;

	if (hw->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3)
		nfd_func->pkt_add_metadata_t = nfp_flower_nfd3_pkt_add_metadata;
	else
		nfd_func->pkt_add_metadata_t = nfp_flower_nfdk_pkt_add_metadata;
}

uint32_t
nfp_flower_pkt_add_metadata(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mbuf *mbuf,
		uint32_t port_id)
{
	return app_fw_flower->nfd_func.pkt_add_metadata_t(mbuf, port_id);
}

static void
nfp_flower_nfd_func_register(struct nfp_app_fw_flower *app_fw_flower)
{
	nfp_flower_pkt_add_metadata_register(app_fw_flower);
	nfp_flower_ctrl_vnic_xmit_register(app_fw_flower);
	nfp_flower_pf_xmit_pkts_register(app_fw_flower);
}

int
nfp_init_app_fw_flower(struct nfp_pf_dev *pf_dev,
		const struct nfp_dev_info *dev_info)
{
	int ret;
	int err;
	uint64_t ext_features;
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

	ret = nfp_flow_priv_init(pf_dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "init flow priv failed");
		goto app_cleanup;
	}

	ret = nfp_mtr_priv_init(pf_dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Error initializing metering private data");
		goto flow_priv_cleanup;
	}

	/* Allocate memory for the PF AND ctrl vNIC here (hence the * 2) */
	pf_hw = rte_zmalloc_socket("nfp_pf_vnic", 2 * sizeof(struct nfp_net_hw),
			RTE_CACHE_LINE_SIZE, numa_node);
	if (pf_hw == NULL) {
		PMD_INIT_LOG(ERR, "Could not malloc nfp pf vnic");
		ret = -ENOMEM;
		goto mtr_priv_cleanup;
	}

	/* Map the PF ctrl bar */
	pf_dev->ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, "_pf0_net_bar0",
			NFP_NET_CFG_BAR_SZ, &pf_dev->ctrl_area);
	if (pf_dev->ctrl_bar == NULL) {
		PMD_INIT_LOG(ERR, "Cloud not map the PF vNIC ctrl bar");
		ret = -ENODEV;
		goto vnic_cleanup;
	}

	/* Read the extra features */
	ext_features = nfp_rtsym_read_le(pf_dev->sym_tbl, "_abi_flower_extra_features",
			&err);
	if (err != 0) {
		PMD_INIT_LOG(ERR, "Couldn't read extra features from fw");
		ret = -EIO;
		goto pf_cpp_area_cleanup;
	}

	/* Store the extra features */
	app_fw_flower->ext_features = ext_features;

	/* Fill in the PF vNIC and populate app struct */
	app_fw_flower->pf_hw = pf_hw;
	pf_hw->super.ctrl_bar = pf_dev->ctrl_bar;
	pf_hw->pf_dev = pf_dev;
	pf_hw->cpp = pf_dev->cpp;
	pf_hw->dev_info = dev_info;

	ret = nfp_flower_init_vnic_common(app_fw_flower->pf_hw, "pf_vnic");
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not initialize flower PF vNIC");
		goto pf_cpp_area_cleanup;
	}

	nfp_flower_nfd_func_register(app_fw_flower);

	/* The ctrl vNIC struct comes directly after the PF one */
	app_fw_flower->ctrl_hw = pf_hw + 1;
	ctrl_hw = app_fw_flower->ctrl_hw;

	/* Map the ctrl vNIC ctrl bar */
	ctrl_hw->super.ctrl_bar = nfp_rtsym_map(pf_dev->sym_tbl, "_pf0_net_ctrl_bar",
			NFP_NET_CFG_BAR_SZ, &ctrl_hw->ctrl_area);
	if (ctrl_hw->super.ctrl_bar == NULL) {
		PMD_INIT_LOG(ERR, "Cloud not map the ctrl vNIC ctrl bar");
		ret = -ENODEV;
		goto pf_cpp_area_cleanup;
	}

	/* Now populate the ctrl vNIC */
	ctrl_hw->pf_dev = pf_dev;
	ctrl_hw->cpp = pf_dev->cpp;
	ctrl_hw->dev_info = dev_info;

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
mtr_priv_cleanup:
	nfp_mtr_priv_uninit(pf_dev);
flow_priv_cleanup:
	nfp_flow_priv_uninit(pf_dev);
app_cleanup:
	rte_free(app_fw_flower);

	return ret;
}

int
nfp_secondary_init_app_fw_flower(struct nfp_pf_dev *pf_dev)
{
	struct rte_eth_dev *eth_dev;
	const char *port_name = "pf_vnic_eth_dev";

	PMD_INIT_LOG(DEBUG, "Secondary attaching to port %s", port_name);

	eth_dev = rte_eth_dev_attach_secondary(port_name);
	if (eth_dev == NULL) {
		PMD_INIT_LOG(ERR, "Secondary process attach to port %s failed", port_name);
		return -ENODEV;
	}

	eth_dev->process_private = pf_dev->cpp;
	eth_dev->dev_ops = &nfp_flower_pf_vnic_ops;
	eth_dev->rx_pkt_burst = nfp_net_recv_pkts;
	eth_dev->tx_pkt_burst = nfp_flower_pf_xmit_pkts;
	rte_eth_dev_probing_finish(eth_dev);

	return 0;
}
