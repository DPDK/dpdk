/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_rxtx.h"
#include "axgbe_ethdev.h"
#include "axgbe_common.h"
#include "axgbe_phy.h"

static int eth_axgbe_dev_init(struct rte_eth_dev *eth_dev);
static int eth_axgbe_dev_uninit(struct rte_eth_dev *eth_dev);
static int  axgbe_dev_configure(struct rte_eth_dev *dev);
static int  axgbe_dev_start(struct rte_eth_dev *dev);
static void axgbe_dev_stop(struct rte_eth_dev *dev);
static void axgbe_dev_interrupt_handler(void *param);
static void axgbe_dev_close(struct rte_eth_dev *dev);
static void axgbe_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void axgbe_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void axgbe_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void axgbe_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int axgbe_dev_link_update(struct rte_eth_dev *dev,
				 int wait_to_complete);
static int axgbe_dev_stats_get(struct rte_eth_dev *dev,
				struct rte_eth_stats *stats);
static void axgbe_dev_stats_reset(struct rte_eth_dev *dev);
static void axgbe_dev_info_get(struct rte_eth_dev *dev,
			       struct rte_eth_dev_info *dev_info);

/* The set of PCI devices this driver supports */
#define AMD_PCI_VENDOR_ID       0x1022
#define AMD_PCI_AXGBE_DEVICE_V2A 0x1458
#define AMD_PCI_AXGBE_DEVICE_V2B 0x1459

int axgbe_logtype_init;
int axgbe_logtype_driver;

static const struct rte_pci_id pci_id_axgbe_map[] = {
	{RTE_PCI_DEVICE(AMD_PCI_VENDOR_ID, AMD_PCI_AXGBE_DEVICE_V2A)},
	{RTE_PCI_DEVICE(AMD_PCI_VENDOR_ID, AMD_PCI_AXGBE_DEVICE_V2B)},
	{ .vendor_id = 0, },
};

static struct axgbe_version_data axgbe_v2a = {
	.init_function_ptrs_phy_impl    = axgbe_init_function_ptrs_phy_v2,
	.xpcs_access			= AXGBE_XPCS_ACCESS_V2,
	.mmc_64bit			= 1,
	.tx_max_fifo_size		= 229376,
	.rx_max_fifo_size		= 229376,
	.tx_tstamp_workaround		= 1,
	.ecc_support			= 1,
	.i2c_support			= 1,
	.an_cdr_workaround		= 1,
};

static struct axgbe_version_data axgbe_v2b = {
	.init_function_ptrs_phy_impl    = axgbe_init_function_ptrs_phy_v2,
	.xpcs_access			= AXGBE_XPCS_ACCESS_V2,
	.mmc_64bit			= 1,
	.tx_max_fifo_size		= 65536,
	.rx_max_fifo_size		= 65536,
	.tx_tstamp_workaround		= 1,
	.ecc_support			= 1,
	.i2c_support			= 1,
	.an_cdr_workaround		= 1,
};

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = AXGBE_MAX_RING_DESC,
	.nb_min = AXGBE_MIN_RING_DESC,
	.nb_align = 8,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = AXGBE_MAX_RING_DESC,
	.nb_min = AXGBE_MIN_RING_DESC,
	.nb_align = 8,
};

static const struct eth_dev_ops axgbe_eth_dev_ops = {
	.dev_configure        = axgbe_dev_configure,
	.dev_start            = axgbe_dev_start,
	.dev_stop             = axgbe_dev_stop,
	.dev_close            = axgbe_dev_close,
	.promiscuous_enable   = axgbe_dev_promiscuous_enable,
	.promiscuous_disable  = axgbe_dev_promiscuous_disable,
	.allmulticast_enable  = axgbe_dev_allmulticast_enable,
	.allmulticast_disable = axgbe_dev_allmulticast_disable,
	.link_update          = axgbe_dev_link_update,
	.stats_get            = axgbe_dev_stats_get,
	.stats_reset          = axgbe_dev_stats_reset,
	.dev_infos_get        = axgbe_dev_info_get,
	.rx_queue_setup       = axgbe_dev_rx_queue_setup,
	.rx_queue_release     = axgbe_dev_rx_queue_release,
	.tx_queue_setup       = axgbe_dev_tx_queue_setup,
	.tx_queue_release     = axgbe_dev_tx_queue_release,
};

static int axgbe_phy_reset(struct axgbe_port *pdata)
{
	pdata->phy_link = -1;
	pdata->phy_speed = SPEED_UNKNOWN;
	return pdata->phy_if.phy_reset(pdata);
}

/*
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) regsitered before.
 *
 * @return
 *  void
 */
static void
axgbe_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct axgbe_port *pdata = dev->data->dev_private;
	unsigned int dma_isr, dma_ch_isr;

	pdata->phy_if.an_isr(pdata);
	/*DMA related interrupts*/
	dma_isr = AXGMAC_IOREAD(pdata, DMA_ISR);
	if (dma_isr) {
		if (dma_isr & 1) {
			dma_ch_isr =
				AXGMAC_DMA_IOREAD((struct axgbe_rx_queue *)
						  pdata->rx_queues[0],
						  DMA_CH_SR);
			AXGMAC_DMA_IOWRITE((struct axgbe_rx_queue *)
					   pdata->rx_queues[0],
					   DMA_CH_SR, dma_ch_isr);
		}
	}
	/* Enable interrupts since disabled after generation*/
	rte_intr_enable(&pdata->pci_dev->intr_handle);
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
axgbe_dev_configure(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata =  dev->data->dev_private;
	/* Checksum offload to hardware */
	pdata->rx_csum_enable = dev->data->dev_conf.rxmode.offloads &
				DEV_RX_OFFLOAD_CHECKSUM;
	return 0;
}

static int
axgbe_dev_rx_mq_config(struct rte_eth_dev *dev)
{
	struct axgbe_port *pdata = (struct axgbe_port *)dev->data->dev_private;

	if (dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS)
		pdata->rss_enable = 1;
	else if (dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_NONE)
		pdata->rss_enable = 0;
	else
		return  -1;
	return 0;
}

static int
axgbe_dev_start(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	struct axgbe_port *pdata = (struct axgbe_port *)dev->data->dev_private;
	int ret;

	/* Multiqueue RSS */
	ret = axgbe_dev_rx_mq_config(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Unable to config RX MQ\n");
		return ret;
	}
	ret = axgbe_phy_reset(pdata);
	if (ret) {
		PMD_DRV_LOG(ERR, "phy reset failed\n");
		return ret;
	}
	ret = pdata->hw_if.init(pdata);
	if (ret) {
		PMD_DRV_LOG(ERR, "dev_init failed\n");
		return ret;
	}

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(&pdata->pci_dev->intr_handle);

	/* phy start*/
	pdata->phy_if.phy_start(pdata);
	axgbe_dev_enable_tx(dev);
	axgbe_dev_enable_rx(dev);

	axgbe_clear_bit(AXGBE_STOPPED, &pdata->dev_state);
	axgbe_clear_bit(AXGBE_DOWN, &pdata->dev_state);
	return 0;
}

/* Stop device: disable rx and tx functions to allow for reconfiguring. */
static void
axgbe_dev_stop(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	struct axgbe_port *pdata = dev->data->dev_private;

	rte_intr_disable(&pdata->pci_dev->intr_handle);

	if (axgbe_test_bit(AXGBE_STOPPED, &pdata->dev_state))
		return;

	axgbe_set_bit(AXGBE_STOPPED, &pdata->dev_state);
	axgbe_dev_disable_tx(dev);
	axgbe_dev_disable_rx(dev);

	pdata->phy_if.phy_stop(pdata);
	pdata->hw_if.exit(pdata);
	memset(&dev->data->dev_link, 0, sizeof(struct rte_eth_link));
	axgbe_set_bit(AXGBE_DOWN, &pdata->dev_state);
}

/* Clear all resources like TX/RX queues. */
static void
axgbe_dev_close(struct rte_eth_dev *dev)
{
	axgbe_dev_clear_queues(dev);
}

static void
axgbe_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	struct axgbe_port *pdata = dev->data->dev_private;

	AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, PR, 1);
}

static void
axgbe_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	struct axgbe_port *pdata = dev->data->dev_private;

	AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, PR, 0);
}

static void
axgbe_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	struct axgbe_port *pdata = dev->data->dev_private;

	if (AXGMAC_IOREAD_BITS(pdata, MAC_PFR, PM))
		return;
	AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, PM, 1);
}

static void
axgbe_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	struct axgbe_port *pdata = dev->data->dev_private;

	if (!AXGMAC_IOREAD_BITS(pdata, MAC_PFR, PM))
		return;
	AXGMAC_IOWRITE_BITS(pdata, MAC_PFR, PM, 0);
}

/* return 0 means link status changed, -1 means not changed */
static int
axgbe_dev_link_update(struct rte_eth_dev *dev,
		      int wait_to_complete __rte_unused)
{
	struct axgbe_port *pdata = dev->data->dev_private;
	struct rte_eth_link link;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	rte_delay_ms(800);

	pdata->phy_if.phy_status(pdata);

	memset(&link, 0, sizeof(struct rte_eth_link));
	link.link_duplex = pdata->phy.duplex;
	link.link_status = pdata->phy_link;
	link.link_speed = pdata->phy_speed;
	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			      ETH_LINK_SPEED_FIXED);
	ret = rte_eth_linkstatus_set(dev, &link);
	if (ret == -1)
		PMD_DRV_LOG(ERR, "No change in link status\n");

	return ret;
}

static int
axgbe_dev_stats_get(struct rte_eth_dev *dev,
		    struct rte_eth_stats *stats)
{
	struct axgbe_rx_queue *rxq;
	struct axgbe_tx_queue *txq;
	unsigned int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		stats->q_ipackets[i] = rxq->pkts;
		stats->ipackets += rxq->pkts;
		stats->q_ibytes[i] = rxq->bytes;
		stats->ibytes += rxq->bytes;
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		stats->q_opackets[i] = txq->pkts;
		stats->opackets += txq->pkts;
		stats->q_obytes[i] = txq->bytes;
		stats->obytes += txq->bytes;
	}

	return 0;
}

static void
axgbe_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct axgbe_rx_queue *rxq;
	struct axgbe_tx_queue *txq;
	unsigned int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		rxq->pkts = 0;
		rxq->bytes = 0;
		rxq->errors = 0;
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		txq->pkts = 0;
		txq->bytes = 0;
		txq->errors = 0;
	}
}

static void
axgbe_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct axgbe_port *pdata = dev->data->dev_private;

	dev_info->max_rx_queues = pdata->rx_ring_count;
	dev_info->max_tx_queues = pdata->tx_ring_count;
	dev_info->min_rx_bufsize = AXGBE_RX_MIN_BUF_SIZE;
	dev_info->max_rx_pktlen = AXGBE_RX_MAX_BUF_SIZE;
	dev_info->max_mac_addrs = AXGBE_MAX_MAC_ADDRS;
	dev_info->speed_capa =  ETH_LINK_SPEED_10G;

	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM  |
		DEV_RX_OFFLOAD_TCP_CKSUM  |
		DEV_RX_OFFLOAD_KEEP_CRC;

	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_IPV4_CKSUM  |
		DEV_TX_OFFLOAD_UDP_CKSUM   |
		DEV_TX_OFFLOAD_TCP_CKSUM;

	if (pdata->hw_feat.rss) {
		dev_info->flow_type_rss_offloads = AXGBE_RSS_OFFLOAD;
		dev_info->reta_size = pdata->hw_feat.hash_table_size;
		dev_info->hash_key_size =  AXGBE_RSS_HASH_KEY_SIZE;
	}

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = AXGBE_RX_FREE_THRESH,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = AXGBE_TX_FREE_THRESH,
	};
}

static void axgbe_get_all_hw_features(struct axgbe_port *pdata)
{
	unsigned int mac_hfr0, mac_hfr1, mac_hfr2;
	struct axgbe_hw_features *hw_feat = &pdata->hw_feat;

	mac_hfr0 = AXGMAC_IOREAD(pdata, MAC_HWF0R);
	mac_hfr1 = AXGMAC_IOREAD(pdata, MAC_HWF1R);
	mac_hfr2 = AXGMAC_IOREAD(pdata, MAC_HWF2R);

	memset(hw_feat, 0, sizeof(*hw_feat));

	hw_feat->version = AXGMAC_IOREAD(pdata, MAC_VR);

	/* Hardware feature register 0 */
	hw_feat->gmii        = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, GMIISEL);
	hw_feat->vlhash      = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, VLHASH);
	hw_feat->sma         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, SMASEL);
	hw_feat->rwk         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, RWKSEL);
	hw_feat->mgk         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, MGKSEL);
	hw_feat->mmc         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, MMCSEL);
	hw_feat->aoe         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, ARPOFFSEL);
	hw_feat->ts          = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, TSSEL);
	hw_feat->eee         = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, EEESEL);
	hw_feat->tx_coe      = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, TXCOESEL);
	hw_feat->rx_coe      = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, RXCOESEL);
	hw_feat->addn_mac    = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R,
					      ADDMACADRSEL);
	hw_feat->ts_src      = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, TSSTSSEL);
	hw_feat->sa_vlan_ins = AXGMAC_GET_BITS(mac_hfr0, MAC_HWF0R, SAVLANINS);

	/* Hardware feature register 1 */
	hw_feat->rx_fifo_size  = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R,
						RXFIFOSIZE);
	hw_feat->tx_fifo_size  = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R,
						TXFIFOSIZE);
	hw_feat->adv_ts_hi     = AXGMAC_GET_BITS(mac_hfr1,
						 MAC_HWF1R, ADVTHWORD);
	hw_feat->dma_width     = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, ADDR64);
	hw_feat->dcb           = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, DCBEN);
	hw_feat->sph           = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, SPHEN);
	hw_feat->tso           = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, TSOEN);
	hw_feat->dma_debug     = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, DBGMEMA);
	hw_feat->rss           = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, RSSEN);
	hw_feat->tc_cnt	       = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R, NUMTC);
	hw_feat->hash_table_size = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R,
						  HASHTBLSZ);
	hw_feat->l3l4_filter_num = AXGMAC_GET_BITS(mac_hfr1, MAC_HWF1R,
						  L3L4FNUM);

	/* Hardware feature register 2 */
	hw_feat->rx_q_cnt     = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, RXQCNT);
	hw_feat->tx_q_cnt     = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, TXQCNT);
	hw_feat->rx_ch_cnt    = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, RXCHCNT);
	hw_feat->tx_ch_cnt    = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, TXCHCNT);
	hw_feat->pps_out_num  = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R, PPSOUTNUM);
	hw_feat->aux_snap_num = AXGMAC_GET_BITS(mac_hfr2, MAC_HWF2R,
						AUXSNAPNUM);

	/* Translate the Hash Table size into actual number */
	switch (hw_feat->hash_table_size) {
	case 0:
		break;
	case 1:
		hw_feat->hash_table_size = 64;
		break;
	case 2:
		hw_feat->hash_table_size = 128;
		break;
	case 3:
		hw_feat->hash_table_size = 256;
		break;
	}

	/* Translate the address width setting into actual number */
	switch (hw_feat->dma_width) {
	case 0:
		hw_feat->dma_width = 32;
		break;
	case 1:
		hw_feat->dma_width = 40;
		break;
	case 2:
		hw_feat->dma_width = 48;
		break;
	default:
		hw_feat->dma_width = 32;
	}

	/* The Queue, Channel and TC counts are zero based so increment them
	 * to get the actual number
	 */
	hw_feat->rx_q_cnt++;
	hw_feat->tx_q_cnt++;
	hw_feat->rx_ch_cnt++;
	hw_feat->tx_ch_cnt++;
	hw_feat->tc_cnt++;

	/* Translate the fifo sizes into actual numbers */
	hw_feat->rx_fifo_size = 1 << (hw_feat->rx_fifo_size + 7);
	hw_feat->tx_fifo_size = 1 << (hw_feat->tx_fifo_size + 7);
}

static void axgbe_init_all_fptrs(struct axgbe_port *pdata)
{
	axgbe_init_function_ptrs_dev(&pdata->hw_if);
	axgbe_init_function_ptrs_phy(&pdata->phy_if);
	axgbe_init_function_ptrs_i2c(&pdata->i2c_if);
	pdata->vdata->init_function_ptrs_phy_impl(&pdata->phy_if);
}

static void axgbe_set_counts(struct axgbe_port *pdata)
{
	/* Set all the function pointers */
	axgbe_init_all_fptrs(pdata);

	/* Populate the hardware features */
	axgbe_get_all_hw_features(pdata);

	/* Set default max values if not provided */
	if (!pdata->tx_max_channel_count)
		pdata->tx_max_channel_count = pdata->hw_feat.tx_ch_cnt;
	if (!pdata->rx_max_channel_count)
		pdata->rx_max_channel_count = pdata->hw_feat.rx_ch_cnt;

	if (!pdata->tx_max_q_count)
		pdata->tx_max_q_count = pdata->hw_feat.tx_q_cnt;
	if (!pdata->rx_max_q_count)
		pdata->rx_max_q_count = pdata->hw_feat.rx_q_cnt;

	/* Calculate the number of Tx and Rx rings to be created
	 *  -Tx (DMA) Channels map 1-to-1 to Tx Queues so set
	 *   the number of Tx queues to the number of Tx channels
	 *   enabled
	 *  -Rx (DMA) Channels do not map 1-to-1 so use the actual
	 *   number of Rx queues or maximum allowed
	 */
	pdata->tx_ring_count = RTE_MIN(pdata->hw_feat.tx_ch_cnt,
				     pdata->tx_max_channel_count);
	pdata->tx_ring_count = RTE_MIN(pdata->tx_ring_count,
				     pdata->tx_max_q_count);

	pdata->tx_q_count = pdata->tx_ring_count;

	pdata->rx_ring_count = RTE_MIN(pdata->hw_feat.rx_ch_cnt,
				     pdata->rx_max_channel_count);

	pdata->rx_q_count = RTE_MIN(pdata->hw_feat.rx_q_cnt,
				  pdata->rx_max_q_count);
}

static void axgbe_default_config(struct axgbe_port *pdata)
{
	pdata->pblx8 = DMA_PBL_X8_ENABLE;
	pdata->tx_sf_mode = MTL_TSF_ENABLE;
	pdata->tx_threshold = MTL_TX_THRESHOLD_64;
	pdata->tx_pbl = DMA_PBL_32;
	pdata->tx_osp_mode = DMA_OSP_ENABLE;
	pdata->rx_sf_mode = MTL_RSF_ENABLE;
	pdata->rx_threshold = MTL_RX_THRESHOLD_64;
	pdata->rx_pbl = DMA_PBL_32;
	pdata->pause_autoneg = 1;
	pdata->tx_pause = 0;
	pdata->rx_pause = 0;
	pdata->phy_speed = SPEED_UNKNOWN;
	pdata->power_down = 0;
}

/*
 * It returns 0 on success.
 */
static int
eth_axgbe_dev_init(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();
	struct axgbe_port *pdata;
	struct rte_pci_device *pci_dev;
	uint32_t reg, mac_lo, mac_hi;
	int ret;

	eth_dev->dev_ops = &axgbe_eth_dev_ops;
	eth_dev->rx_pkt_burst = &axgbe_recv_pkts;

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pdata = (struct axgbe_port *)eth_dev->data->dev_private;
	/* initial state */
	axgbe_set_bit(AXGBE_DOWN, &pdata->dev_state);
	axgbe_set_bit(AXGBE_STOPPED, &pdata->dev_state);
	pdata->eth_dev = eth_dev;

	pci_dev = RTE_DEV_TO_PCI(eth_dev->device);
	pdata->pci_dev = pci_dev;

	pdata->xgmac_regs =
		(void *)pci_dev->mem_resource[AXGBE_AXGMAC_BAR].addr;
	pdata->xprop_regs = (void *)((uint8_t *)pdata->xgmac_regs
				     + AXGBE_MAC_PROP_OFFSET);
	pdata->xi2c_regs = (void *)((uint8_t *)pdata->xgmac_regs
				    + AXGBE_I2C_CTRL_OFFSET);
	pdata->xpcs_regs = (void *)pci_dev->mem_resource[AXGBE_XPCS_BAR].addr;

	/* version specific driver data*/
	if (pci_dev->id.device_id == AMD_PCI_AXGBE_DEVICE_V2A)
		pdata->vdata = &axgbe_v2a;
	else
		pdata->vdata = &axgbe_v2b;

	/* Configure the PCS indirect addressing support */
	reg = XPCS32_IOREAD(pdata, PCS_V2_WINDOW_DEF);
	pdata->xpcs_window = XPCS_GET_BITS(reg, PCS_V2_WINDOW_DEF, OFFSET);
	pdata->xpcs_window <<= 6;
	pdata->xpcs_window_size = XPCS_GET_BITS(reg, PCS_V2_WINDOW_DEF, SIZE);
	pdata->xpcs_window_size = 1 << (pdata->xpcs_window_size + 7);
	pdata->xpcs_window_mask = pdata->xpcs_window_size - 1;
	pdata->xpcs_window_def_reg = PCS_V2_WINDOW_DEF;
	pdata->xpcs_window_sel_reg = PCS_V2_WINDOW_SELECT;
	PMD_INIT_LOG(DEBUG,
		     "xpcs window :%x, size :%x, mask :%x ", pdata->xpcs_window,
		     pdata->xpcs_window_size, pdata->xpcs_window_mask);
	XP_IOWRITE(pdata, XP_INT_EN, 0x1fffff);

	/* Retrieve the MAC address */
	mac_lo = XP_IOREAD(pdata, XP_MAC_ADDR_LO);
	mac_hi = XP_IOREAD(pdata, XP_MAC_ADDR_HI);
	pdata->mac_addr.addr_bytes[0] = mac_lo & 0xff;
	pdata->mac_addr.addr_bytes[1] = (mac_lo >> 8) & 0xff;
	pdata->mac_addr.addr_bytes[2] = (mac_lo >> 16) & 0xff;
	pdata->mac_addr.addr_bytes[3] = (mac_lo >> 24) & 0xff;
	pdata->mac_addr.addr_bytes[4] = mac_hi & 0xff;
	pdata->mac_addr.addr_bytes[5] = (mac_hi >> 8)  &  0xff;

	eth_dev->data->mac_addrs = rte_zmalloc("axgbe_mac_addr",
					       ETHER_ADDR_LEN, 0);
	if (!eth_dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR,
			     "Failed to alloc %u bytes needed to store MAC addr tbl",
			     ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	if (!is_valid_assigned_ether_addr(&pdata->mac_addr))
		eth_random_addr(pdata->mac_addr.addr_bytes);

	/* Copy the permanent MAC address */
	ether_addr_copy(&pdata->mac_addr, &eth_dev->data->mac_addrs[0]);

	/* Clock settings */
	pdata->sysclk_rate = AXGBE_V2_DMA_CLOCK_FREQ;
	pdata->ptpclk_rate = AXGBE_V2_PTP_CLOCK_FREQ;

	/* Set the DMA coherency values */
	pdata->coherent = 1;
	pdata->axdomain = AXGBE_DMA_OS_AXDOMAIN;
	pdata->arcache = AXGBE_DMA_OS_ARCACHE;
	pdata->awcache = AXGBE_DMA_OS_AWCACHE;

	/* Set the maximum channels and queues */
	reg = XP_IOREAD(pdata, XP_PROP_1);
	pdata->tx_max_channel_count = XP_GET_BITS(reg, XP_PROP_1, MAX_TX_DMA);
	pdata->rx_max_channel_count = XP_GET_BITS(reg, XP_PROP_1, MAX_RX_DMA);
	pdata->tx_max_q_count = XP_GET_BITS(reg, XP_PROP_1, MAX_TX_QUEUES);
	pdata->rx_max_q_count = XP_GET_BITS(reg, XP_PROP_1, MAX_RX_QUEUES);

	/* Set the hardware channel and queue counts */
	axgbe_set_counts(pdata);

	/* Set the maximum fifo amounts */
	reg = XP_IOREAD(pdata, XP_PROP_2);
	pdata->tx_max_fifo_size = XP_GET_BITS(reg, XP_PROP_2, TX_FIFO_SIZE);
	pdata->tx_max_fifo_size *= 16384;
	pdata->tx_max_fifo_size = RTE_MIN(pdata->tx_max_fifo_size,
					  pdata->vdata->tx_max_fifo_size);
	pdata->rx_max_fifo_size = XP_GET_BITS(reg, XP_PROP_2, RX_FIFO_SIZE);
	pdata->rx_max_fifo_size *= 16384;
	pdata->rx_max_fifo_size = RTE_MIN(pdata->rx_max_fifo_size,
					  pdata->vdata->rx_max_fifo_size);
	/* Issue software reset to DMA */
	ret = pdata->hw_if.exit(pdata);
	if (ret)
		PMD_DRV_LOG(ERR, "hw_if->exit EBUSY error\n");

	/* Set default configuration data */
	axgbe_default_config(pdata);

	/* Set default max values if not provided */
	if (!pdata->tx_max_fifo_size)
		pdata->tx_max_fifo_size = pdata->hw_feat.tx_fifo_size;
	if (!pdata->rx_max_fifo_size)
		pdata->rx_max_fifo_size = pdata->hw_feat.rx_fifo_size;

	pdata->tx_desc_count = AXGBE_MAX_RING_DESC;
	pdata->rx_desc_count = AXGBE_MAX_RING_DESC;
	pthread_mutex_init(&pdata->xpcs_mutex, NULL);
	pthread_mutex_init(&pdata->i2c_mutex, NULL);
	pthread_mutex_init(&pdata->an_mutex, NULL);
	pthread_mutex_init(&pdata->phy_mutex, NULL);

	ret = pdata->phy_if.phy_init(pdata);
	if (ret) {
		rte_free(eth_dev->data->mac_addrs);
		return ret;
	}

	rte_intr_callback_register(&pci_dev->intr_handle,
				   axgbe_dev_interrupt_handler,
				   (void *)eth_dev);
	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

	return 0;
}

static int
eth_axgbe_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = RTE_DEV_TO_PCI(eth_dev->device);
	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;
	axgbe_dev_clear_queues(eth_dev);

	/* disable uio intr before callback unregister */
	rte_intr_disable(&pci_dev->intr_handle);
	rte_intr_callback_unregister(&pci_dev->intr_handle,
				     axgbe_dev_interrupt_handler,
				     (void *)eth_dev);

	return 0;
}

static int eth_axgbe_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct axgbe_port), eth_axgbe_dev_init);
}

static int eth_axgbe_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_axgbe_dev_uninit);
}

static struct rte_pci_driver rte_axgbe_pmd = {
	.id_table = pci_id_axgbe_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_axgbe_pci_probe,
	.remove = eth_axgbe_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_axgbe, rte_axgbe_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_axgbe, pci_id_axgbe_map);
RTE_PMD_REGISTER_KMOD_DEP(net_axgbe, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_INIT(axgbe_init_log)
{
	axgbe_logtype_init = rte_log_register("pmd.net.axgbe.init");
	if (axgbe_logtype_init >= 0)
		rte_log_set_level(axgbe_logtype_init, RTE_LOG_NOTICE);
	axgbe_logtype_driver = rte_log_register("pmd.net.axgbe.driver");
	if (axgbe_logtype_driver >= 0)
		rte_log_set_level(axgbe_logtype_driver, RTE_LOG_NOTICE);
}
