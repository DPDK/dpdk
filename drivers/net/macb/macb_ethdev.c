/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022~2023 Phytium Technology Co., Ltd.
 */

#include <rte_bus_vdev.h>
#include <ethdev_driver.h>
#include <ethdev_vdev.h>
#include <rte_kvargs.h>
#include <rte_string_fns.h>

#include "macb_rxtx.h"

#define MACB_DRIVER_VERSION "5.8"
#define MACB_DEVICE_NAME_ARG "device"
#define MACB_USE_PHYDRV_ARG "usephydrv"
#define MACB_MAC_ADDRS_MAX 256
#define MAX_BUF_STR_LEN 256
#define MACB_PDEV_PATH "/sys/bus/platform/devices"
#define MACB_LINK_UPDATE_CHECK_TIMEOUT 90	/* 9s */
#define MACB_LINK_UPDATE_CHECK_INTERVAL 100 /* ms */

#define MACB_DEFAULT_TX_FREE_THRESH 32
#define MACB_DEFAULT_TX_RSBIT_THRESH 16

#define MACB_DEFAULT_RX_FREE_THRESH 16

int macb_logtype;
static int macb_log_initialized;

static const char *const valid_args[] = {
	MACB_DEVICE_NAME_ARG,
	MACB_USE_PHYDRV_ARG,
	NULL};

struct macb_devices {
	const char *names[MACB_MAX_PORT_NUM];
	uint32_t idx;
};

static int macb_dev_num;

static int macb_phy_auto_detect(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	uint16_t phyad;
	uint32_t phyid, phyid1, phyid2;
	struct phy_device *phydev = bp->phydev;
	struct phy_driver **phydrv;

	/*
	 * Custom external phy driver need to be added to phydrv_list.
	 */
	struct phy_driver *phydrv_list[] = {
		&genphy_driver,
		NULL
	};

	/*internal phy */
	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_USXGMII) {
		phydev->drv = &macb_usxgmii_pcs_driver;
		return 0;
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_2500BASEX ||
		bp->phy_interface == MACB_PHY_INTERFACE_MODE_1000BASEX ||
		bp->phy_interface == MACB_PHY_INTERFACE_MODE_100BASEX ||
		(bp->phy_interface == MACB_PHY_INTERFACE_MODE_SGMII && bp->fixed_link)) {
		phydev->drv = &macb_gbe_pcs_driver;
		return 0;
	}

	/*external phy use no driver*/
	if (!bp->phydrv_used) {
		phydev->drv = NULL;
		return 0;
	}

	for (phyad = 0; phyad < MAX_PHY_AD_NUM; phyad++) {
		phyid2 = macb_mdio_read(bp, phyad, GENERIC_PHY_PHYSID2);
		phyid1 = macb_mdio_read(bp, phyad, GENERIC_PHY_PHYSID1);
		phyid = phyid2 | (phyid1 << PHY_ID_OFFSET);
		/* If the phy_id is mostly Fs, there is no device there */
		if (phyid && ((phyid & 0x1fffffff) != 0x1fffffff)) {
			phydev->phy_id = phyid;
			phydev->phyad = phyad;
			break;
		}
	}

	/* check if already registered */
	for (phydrv = phydrv_list; *phydrv; phydrv++) {
		if ((phydev->phy_id & (*phydrv)->phy_id_mask) == (*phydrv)->phy_id)
			break;
	}

	if (*phydrv != NULL) {
		phydev->drv = *phydrv;
		MACB_INFO("Phy driver %s used", phydev->drv->name);
	} else {
		phydev->drv = &genphy_driver;
		MACB_INFO("Unknown phyid: 0x%x, general phy driver used", phyid);
	}

	/* phy probe */
	if (phydev->drv && phydev->drv->probe)
		phydev->drv->probe(phydev);

	return 0;
}

/**
 * DPDK callback to enable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return 0
 *
 *
 */
static int eth_macb_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	uint32_t cfg;

	if (!bp) {
		MACB_LOG(DEBUG, "Failed to get private data!");
		return -EPERM;
	}

	cfg = macb_readl(bp, NCFGR);
	cfg |= MACB_BIT(CAF);

	macb_writel(bp, NCFGR, cfg);

	return 0;
}

/**
 * DPDK callback to disable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return 0
 *
 *
 */
static int eth_macb_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	uint32_t cfg;

	if (!bp) {
		MACB_LOG(DEBUG, "Failed to get private data!");
		return -EPERM;
	}

	cfg = macb_readl(bp, NCFGR);
	cfg &= ~MACB_BIT(CAF);

	macb_writel(bp, NCFGR, cfg);

	return 0;
}

static int eth_macb_allmulticast_enable(struct rte_eth_dev *dev)
{
	unsigned long cfg;
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;

	cfg = macb_readl(bp, NCFGR);
	/* Enable all multicast mode */
	macb_or_gem_writel(bp, HRB, -1);
	macb_or_gem_writel(bp, HRT, -1);
	cfg |= MACB_BIT(NCFGR_MTI);

	macb_writel(bp, NCFGR, cfg);
	return 0;
}

static int eth_macb_allmulticast_disable(struct rte_eth_dev *dev)
{
	unsigned long cfg;
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;

	if (dev->data->promiscuous == 1)
		return 0; /* must remain in all_multicast mode */

	cfg = macb_readl(bp, NCFGR);
	/* Disable all multicast mode */
	macb_or_gem_writel(bp, HRB, 0);
	macb_or_gem_writel(bp, HRT, 0);
	cfg &= ~MACB_BIT(NCFGR_MTI);

	macb_writel(bp, NCFGR, cfg);
	return 0;
}

static int eth_macb_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	struct phy_device *phydev = bp->phydev;
	struct rte_eth_link link;
	int count, link_check;

	if (!priv->bp) {
		MACB_LOG(ERR, "Failed to get private data!");
		return -EPERM;
	}

	for (count = 0; count < MACB_LINK_UPDATE_CHECK_TIMEOUT; count++) {
		macb_check_for_link(bp);
		link_check = bp->link;
		if (link_check || wait_to_complete == 0)
			break;
		rte_delay_ms(MACB_LINK_UPDATE_CHECK_INTERVAL);
	}
	memset(&link, 0, sizeof(link));

	if (link_check) {
		if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_USXGMII ||
			bp->phy_interface == MACB_PHY_INTERFACE_MODE_2500BASEX ||
			bp->phy_interface == MACB_PHY_INTERFACE_MODE_1000BASEX ||
			bp->phy_interface == MACB_PHY_INTERFACE_MODE_100BASEX ||
			(bp->phy_interface == MACB_PHY_INTERFACE_MODE_SGMII && bp->fixed_link) ||
			!bp->phydrv_used) {
			link.link_speed = bp->speed;
			link.link_duplex =
				bp->duplex ? RTE_ETH_LINK_FULL_DUPLEX : RTE_ETH_LINK_HALF_DUPLEX;
		} else {
			/* get phy link info */
			if (phydev->drv && phydev->drv->read_status)
				phydev->drv->read_status(phydev);

			link.link_speed = phydev->speed;
			link.link_duplex = phydev->duplex ? RTE_ETH_LINK_FULL_DUPLEX :
					   RTE_ETH_LINK_HALF_DUPLEX;
		}
		link.link_status = RTE_ETH_LINK_UP;
		link.link_autoneg =
			!(dev->data->dev_conf.link_speeds & RTE_ETH_LINK_SPEED_FIXED);
	} else if (!link_check) {
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
		link.link_status = RTE_ETH_LINK_DOWN;
		link.link_autoneg = RTE_ETH_LINK_FIXED;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static void macb_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct macb_priv *priv = dev->data->dev_private;
	struct rte_eth_link link;
	char status[128];

	if (priv->stopped)
		return;

	if (eth_macb_link_update(dev, 0) < 0)
		return;

	rte_eth_linkstatus_get(dev, &link);
	rte_eth_link_to_str(status, sizeof(status), &link);
	MACB_INFO("Port %u: %s", dev->data->port_id, status);

	macb_link_change(priv->bp);
	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
}

static int eth_macb_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	struct phy_device *phydev = bp->phydev;

	if (!bp) {
		MACB_LOG(ERR, "Failed to get private data!");
		return -EPERM;
	}

	/* phy link up */
	if (phydev->drv && phydev->drv->resume)
		phydev->drv->resume(phydev);

	return 0;
}

static int eth_macb_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	struct phy_device *phydev = bp->phydev;

	if (!bp) {
		MACB_LOG(ERR, "Failed to get private data!");
		return -EPERM;
	}

	/* phy link down */
	if (phydev->drv && phydev->drv->suspend)
		phydev->drv->suspend(phydev);

	return 0;
}

/**
 * DPDK callback to get device statistics.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param stats
 *   Stats structure output buffer.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int eth_macb_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats,
			      struct eth_queue_stats *qstats __rte_unused)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct gem_stats *hwstat = &priv->bp->hw_stats.gem;

	RTE_ASSERT(priv->bp != NULL);

	macb_get_stats(priv->bp);

	stats->ipackets = hwstat->rx_frames - priv->prev_stats.ipackets;
	stats->opackets = hwstat->tx_frames - priv->prev_stats.opackets;
	stats->ibytes = hwstat->rx_octets_31_0 + (hwstat->rx_octets_47_32 << 32) -
					priv->prev_stats.ibytes;
	stats->obytes = hwstat->tx_octets_31_0 + (hwstat->tx_octets_47_32 << 32) -
					priv->prev_stats.obytes;
	stats->imissed = hwstat->rx_resource_drops + hwstat->rx_overruns -
					priv->prev_stats.imissed;
	stats->ierrors =
		(hwstat->rx_frame_check_sequence_errors + hwstat->rx_alignment_errors +
		 hwstat->rx_oversize_frames + hwstat->rx_jabbers +
		 hwstat->rx_undersized_frames + hwstat->rx_length_field_frame_errors +
		 hwstat->rx_ip_header_checksum_errors + hwstat->rx_tcp_checksum_errors +
		 hwstat->rx_udp_checksum_errors) -
		priv->prev_stats.ierrors;
	stats->oerrors =
		(hwstat->tx_late_collisions + hwstat->tx_excessive_collisions +
		 hwstat->tx_underrun + hwstat->tx_carrier_sense_errors) -
		priv->prev_stats.oerrors;

	return 0;
}

static int eth_macb_stats_reset(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	int ret;

	if (!priv->bp) {
		MACB_LOG(ERR, "Failed to get private data!");
		return -EPERM;
	}

	memset(&priv->prev_stats, 0, sizeof(struct rte_eth_stats));
	ret = eth_macb_stats_get(dev, &priv->prev_stats, NULL);
	if (unlikely(ret)) {
		MACB_LOG(ERR, "Failed to reset port statistics.");
		return ret;
	}

	return 0;
}

static int eth_macb_dev_infos_get(struct rte_eth_dev *dev __rte_unused,
								  struct rte_eth_dev_info *dev_info)
{
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_queues = MACB_MAX_QUEUES;
	dev_info->max_tx_queues = MACB_MAX_QUEUES;
	dev_info->max_rx_pktlen = MAX_JUMBO_FRAME_SIZE;

	/* MAX JUMBO FRAME */
	dev_info->max_rx_pktlen = MACB_MAX_JUMBO_FRAME;

	dev_info->max_mtu = dev_info->max_rx_pktlen - MACB_ETH_OVERHEAD;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_10M | RTE_ETH_LINK_SPEED_100M |
						   RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_10G;

	dev_info->rx_queue_offload_capa = macb_get_rx_queue_offloads_capa(dev);
	dev_info->rx_offload_capa =
		macb_get_rx_port_offloads_capa(dev) | dev_info->rx_queue_offload_capa;
	dev_info->tx_queue_offload_capa = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
					RTE_ETH_TX_OFFLOAD_UDP_CKSUM | RTE_ETH_TX_OFFLOAD_TCP_CKSUM;
	dev_info->tx_offload_capa = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
				RTE_ETH_TX_OFFLOAD_UDP_CKSUM | RTE_ETH_TX_OFFLOAD_TCP_CKSUM;
	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = MACB_DEFAULT_RX_FREE_THRESH,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = MACB_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = MACB_DEFAULT_TX_RSBIT_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = MACB_MAX_RING_DESC,
		.nb_min = MACB_MIN_RING_DESC,
		.nb_align = MACB_RXD_ALIGN,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = MACB_MAX_RING_DESC,
		.nb_min = MACB_MIN_RING_DESC,
		.nb_align = MACB_TXD_ALIGN,
	};

	dev_info->max_rx_queues = MACB_MAX_QUEUES;
	dev_info->max_tx_queues = MACB_MAX_QUEUES;


	return 0;
}

static const uint32_t *
eth_macb_dev_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused,
				  size_t *num_ptypes __rte_unused)
{
	static const uint32_t ptypes[] = {RTE_PTYPE_L3_IPV4, RTE_PTYPE_L3_IPV6,
					  RTE_PTYPE_L4_TCP, RTE_PTYPE_L4_UDP};

	return ptypes;
}

/**
 * DPDK callback to set mtu.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mtu
 *   The value of Maximum Transmission Unit (MTU) to set
 */
static int eth_macb_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	u32 frame_size = mtu + MACB_ETH_OVERHEAD;
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	u32 config;

	config = macb_readl(bp, NCFGR);

	/* refuse mtu that requires the support of scattered packets when this
	 * feature has not been enabled before.
	 */
	if (!dev->data->scattered_rx &&
		frame_size > dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM) {
		MACB_LOG(ERR, "mtu setting rejected.");
		return -EINVAL;
	}

	/* switch to jumbo mode if needed */
	if (mtu > RTE_ETHER_MAX_LEN)
		config |= MACB_BIT(JFRAME);
	else
		config &= ~MACB_BIT(JFRAME);
	macb_writel(bp, NCFGR, config);
	gem_writel(bp, JML, frame_size);

	return 0;
}

/* macb_set_hwaddr
 * set mac address.
 *
 * This function complete mac addr set.
 *
 * @param dev
 *   A pointer to the macb.
 *
 * @modify author
 *   Mengxiangbo
 * @modify time
 *   2021-02-07
 * @modify reason
 *   build
 **/
static void eth_macb_set_hwaddr(struct macb *bp)
{
	u32 bottom;
	u16 top;

	bottom = cpu_to_le32(*((u32 *)bp->dev->data->mac_addrs->addr_bytes));
	macb_or_gem_writel(bp, SA1B, bottom);
	top = cpu_to_le16(*((u16 *)(bp->dev->data->mac_addrs->addr_bytes + 4)));
	macb_or_gem_writel(bp, SA1T, top);

	/* Clear unused address register sets */
	macb_or_gem_writel(bp, SA2B, 0);
	macb_or_gem_writel(bp, SA2T, 0);
	macb_or_gem_writel(bp, SA3B, 0);
	macb_or_gem_writel(bp, SA3T, 0);
	macb_or_gem_writel(bp, SA4B, 0);
	macb_or_gem_writel(bp, SA4T, 0);
}

static void macb_get_hwaddr(struct macb *bp)
{
	struct rte_ether_addr mac_addr;
	u32 bottom;
	u16 top;
	u8 addr[6];

	bottom = macb_or_gem_readl(bp, SA1B);
	top = macb_or_gem_readl(bp, SA1T);

	addr[0] = bottom & 0xff;
	addr[1] = (bottom >> 8) & 0xff;
	addr[2] = (bottom >> 16) & 0xff;
	addr[3] = (bottom >> 24) & 0xff;
	addr[4] = top & 0xff;
	addr[5] = (top >> 8) & 0xff;

	memcpy(mac_addr.addr_bytes, addr, RTE_ETHER_ADDR_LEN);
	if (!rte_is_valid_assigned_ether_addr(&mac_addr)) {
		MACB_LOG(INFO, "Invalid MAC address, using random.");
		rte_eth_random_addr(addr);
	}
	memcpy(bp->dev->data->mac_addrs->addr_bytes, addr, sizeof(addr));
}

const struct macb_config macb_gem1p0_mac_config = {
	.caps = MACB_CAPS_MACB_IS_GEM | MACB_CAPS_GIGABIT_MODE_AVAILABLE |
			MACB_CAPS_JUMBO | MACB_CAPS_BD_RD_PREFETCH |
			MACB_CAPS_ISR_CLEAR_ON_WRITE | MACB_CAPS_PERFORMANCE_OPTIMIZING |
			MACB_CAPS_SEL_CLK_HW,
	.dma_burst_length = 16,
	.jumbo_max_len = 10240,
	.sel_clk_hw = macb_gem1p0_sel_clk,
};

static const struct macb_config macb_gem2p0_mac_config = {
	.caps = MACB_CAPS_MACB_IS_GEM | MACB_CAPS_GIGABIT_MODE_AVAILABLE |
				MACB_CAPS_JUMBO | MACB_CAPS_BD_RD_PREFETCH |
				MACB_CAPS_ISR_CLEAR_ON_WRITE | MACB_CAPS_PERFORMANCE_OPTIMIZING |
				MACB_CAPS_SEL_CLK_HW,
	.dma_burst_length = 16,
	.jumbo_max_len = 10240,
	.sel_clk_hw = macb_gem2p0_sel_clk,
};

static int eth_macb_set_default_mac_addr(struct rte_eth_dev *dev,
				     struct rte_ether_addr *mac_addr)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;

	memcpy(bp->dev->data->mac_addrs, mac_addr, RTE_ETHER_ADDR_LEN);

	eth_macb_set_hwaddr(bp);

	return 0;
}

/* macb_dev_configure
 * Macb dev init and hw init   include some register init and some
 * Nic operation func appoint .
 *
 * This function complete hw initialization.
 *
 * @param dev
 *   A pointer to the dev.
 *
 **/
static int eth_macb_dev_configure(struct rte_eth_dev *dev)
{
	u32 reg, val = 0;
	bool native_io;
	unsigned int queue_mask, num_queues;
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	const struct macb_config *macb_config = NULL;

	bp->dev_type = priv->dev_type;
	if (bp->dev_type == DEV_TYPE_PHYTIUM_GEM1P0_MAC) {
		macb_config = &macb_gem1p0_mac_config;
	} else if (bp->dev_type == DEV_TYPE_PHYTIUM_GEM2P0_MAC) {
		macb_config = &macb_gem2p0_mac_config;
	} else {
		MACB_LOG(ERR, "unsupportted device.");
		return -ENODEV;
	}

	native_io = macb_hw_is_native_io(bp);
	macb_probe_queues(bp->base, native_io, &queue_mask, &num_queues);

	bp->native_io = native_io;
	bp->num_queues = num_queues;
	bp->tx_ring_size = MACB_TX_RING_SIZE;
	bp->rx_ring_size = MACB_RX_RING_SIZE;
	bp->queue_mask = queue_mask;

	if (macb_config) {
		bp->dma_burst_length = macb_config->dma_burst_length;
		bp->jumbo_max_len = macb_config->jumbo_max_len;
		bp->sel_clk_hw = macb_config->sel_clk_hw;
	}

	/* setup capabilities */
	macb_configure_caps(bp, macb_config);
	bp->hw_dma_cap = HW_DMA_CAP_64B;

	/* set MTU */
	dev->data->mtu = RTE_ETHER_MTU;

	/* enable lsc interrupt */
	dev->data->dev_conf.intr_conf.lsc = true;

	/* prefetch init */
	if (bp->caps & MACB_CAPS_BD_RD_PREFETCH) {
		val = GEM_BFEXT(RXBD_RDBUFF, gem_readl(bp, DCFG10));
		if (val)
			bp->rx_bd_rd_prefetch =
				(4 << (val - 1)) * macb_dma_desc_get_size(bp);

		val = GEM_BFEXT(TXBD_RDBUFF, gem_readl(bp, DCFG10));
		if (val)
			bp->tx_bd_rd_prefetch =
				(4 << (val - 1)) * macb_dma_desc_get_size(bp);
	}

	/* Enable management port */
	macb_writel(bp, NCR, MACB_BIT(MPE));

	/* get mac address */
	macb_get_hwaddr(bp);

	/* Checksum offload is only available on gem with packet buffer */
	if (macb_is_gem(bp) && !(bp->caps & MACB_CAPS_FIFO_MODE))
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_CHECKSUM;
	/* Scatter gather disable */
	if (bp->caps & MACB_CAPS_SG_DISABLED)
		dev->data->dev_conf.rxmode.offloads &= ~RTE_ETH_RX_OFFLOAD_SCATTER;

	/* Check RX Flow Filters support.
	 * Max Rx flows set by availability of screeners & compare regs:
	 * each 4-tuple define requires 1 T2 screener reg + 3 compare regs
	 */
	reg = gem_readl(bp, DCFG8);
	bp->max_tuples = min((GEM_BFEXT(SCR2CMP, reg) / 3), GEM_BFEXT(T2SCR, reg));
	if (bp->max_tuples > 0) {
		if (GEM_BFEXT(SCR2ETH, reg) > 0) {
			reg = 0;
			reg = GEM_BFINS(ETHTCMP, (uint16_t)ETH_P_IP, reg);
			gem_writel_n(bp, ETHT, SCRT2_ETHT, reg);
			priv->hw_features |= RTE_5TUPLE_FLAGS;
		} else {
			bp->max_tuples = 0;
		}
	}
	/*
	 * Initialize to TRUE. If any of Rx queues doesn't meet the bulk
	 * allocation or vector Rx preconditions we will reset it.
	 */
	bp->rx_bulk_alloc_allowed = true;
	bp->rx_vec_allowed = true;

	return 0;
}

static u32 macb_mdc_clk_div(struct macb *bp)
{
	u32 config;
	unsigned long pclk_hz;
	struct macb_priv *priv = bp->dev->data->dev_private;

	pclk_hz = priv->pclk_hz;
	if (pclk_hz <= 20000000)
		config = GEM_BF(CLK, GEM_CLK_DIV8);
	else if (pclk_hz <= 40000000)
		config = GEM_BF(CLK, GEM_CLK_DIV16);
	else if (pclk_hz <= 80000000)
		config = GEM_BF(CLK, GEM_CLK_DIV32);
	else if (pclk_hz <= 120000000)
		config = GEM_BF(CLK, GEM_CLK_DIV48);
	else if (pclk_hz <= 160000000)
		config = GEM_BF(CLK, GEM_CLK_DIV64);
	else
		config = GEM_BF(CLK, GEM_CLK_DIV96);

	return config;
}

static void macb_configure_dma(struct macb *bp)
{
	struct macb_rx_queue *rxq;
	u32 buffer_size;
	unsigned int i;
	u32 dmacfg;

	/* Dma rx  buffer size set */
	buffer_size = bp->rx_buffer_size / RX_BUFFER_MULTIPLE;
	if (macb_is_gem(bp)) {
		dmacfg = gem_readl(bp, DMACFG) & ~GEM_BF(RXBS, -1L);
		for (i = 0; i < bp->dev->data->nb_rx_queues; i++) {
			rxq = bp->dev->data->rx_queues[i];
			if (i != 0)
				queue_writel(rxq, RBQS, buffer_size);
			else
				dmacfg |= GEM_BF(RXBS, buffer_size);
		}

		/* Disable PTP */
		dmacfg &= ~GEM_BIT(RXEXT);
		dmacfg &= ~GEM_BIT(TXEXT);

		/* Fixed burst length for DMA set */
		if (bp->dma_burst_length)
			dmacfg = GEM_BFINS(FBLDO, bp->dma_burst_length, dmacfg);

		/* TX RX packet buffer memory size select */
		dmacfg |= GEM_BIT(TXPBMS) | GEM_BF(RXBMS, -1L);
		dmacfg &= ~GEM_BIT(ENDIA_PKT);

		/* Big little endian set */
		if (bp->native_io)
			dmacfg &= ~GEM_BIT(ENDIA_DESC);
		else
			dmacfg |= GEM_BIT(ENDIA_DESC); /* CPU in big endian */

		/* Dma addr bit width set */
		dmacfg &= ~GEM_BIT(ADDR64);
		if (bp->hw_dma_cap & HW_DMA_CAP_64B)
			dmacfg |= GEM_BIT(ADDR64);

		/* TX IP/TCP/UDP checksum gen offload set */
		if (bp->dev->data->dev_conf.txmode.offloads & (RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		    RTE_ETH_TX_OFFLOAD_UDP_CKSUM | RTE_ETH_TX_OFFLOAD_TCP_CKSUM))
			dmacfg |= GEM_BIT(TXCOEN);
		else
			dmacfg &= ~GEM_BIT(TXCOEN);

		gem_writel(bp, DMACFG, dmacfg);
	}
}

static void macb_init_hw(struct macb *bp)
{
	u32 config;
	u32 max_len;

	/* Config NCFGR register*/
	config = macb_mdc_clk_div(bp);

	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_SGMII)
		config |= GEM_BIT(SGMIIEN) | GEM_BIT(PCSSEL);
	config |= MACB_BF(RBOF, MACB_RX_DATA_OFFSET);
	config |= MACB_BIT(PAE);
	if (bp->dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		config &= ~MACB_BIT(DRFCS);
	else
		config |= MACB_BIT(DRFCS);

	/* Enable jumbo frames */
	if (bp->dev->data->mtu > RTE_ETHER_MTU)
		config |= MACB_BIT(JFRAME);
	else
		/* Receive oversized frames */
		config |= MACB_BIT(BIG);

	/* Copy All Frames */
	if (bp->dev->data->promiscuous == 1)
		config |= MACB_BIT(CAF);
	else if (macb_is_gem(bp) && (bp->dev->data->dev_conf.rxmode.offloads &
								 RTE_ETH_RX_OFFLOAD_CHECKSUM))
		config |= GEM_BIT(RXCOEN);

	config |= macb_dbw(bp);

	/* RX IP/TCP/UDP checksum gen offload set */
	if (macb_is_gem(bp) &&
		(bp->dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM))
		config |= GEM_BIT(RXCOEN);
	macb_writel(bp, NCFGR, config);

	if ((bp->caps & MACB_CAPS_SEL_CLK_HW) && bp->sel_clk_hw)
		bp->sel_clk_hw(bp);

	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_USXGMII ||
		bp->phy_interface == MACB_PHY_INTERFACE_MODE_2500BASEX ||
		bp->phy_interface == MACB_PHY_INTERFACE_MODE_1000BASEX ||
		bp->phy_interface == MACB_PHY_INTERFACE_MODE_100BASEX ||
		(bp->phy_interface == MACB_PHY_INTERFACE_MODE_SGMII && bp->fixed_link)) {
		macb_mac_with_pcs_config(bp);
	}

	/* JUMBO value set */
	if (bp->dev->data->mtu > RTE_ETHER_MTU) {
		max_len = bp->dev->data->mtu + MACB_ETH_OVERHEAD;
		gem_writel(bp, JML, max_len);
	}
	bp->rx_frm_len_mask = MACB_RX_JFRMLEN_MASK;

	/* Set axi_pipe */
	if (bp->caps & MACB_CAPS_PERFORMANCE_OPTIMIZING)
		gem_writel(bp, AXI_PIPE, 0x1010);
}


static inline void macb_enable_rxtx(struct macb *bp)
{
	u32 ctrl = macb_readl(bp, NCR);
	ctrl |= MACB_BIT(RE) | MACB_BIT(TE);
	macb_writel(bp, NCR, ctrl);
}

/* macb_dev_start
 * start dev Complete hardware initialization .
 *
 * This function complete hw initialization.
 *
 * @param dev
 *   A pointer to the dev.
 *
 **/
static int eth_macb_dev_start(struct rte_eth_dev *dev)
{
	int err;
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	struct phy_device *phydev = bp->phydev;
	uint32_t *speeds;
	int num_speeds;
	bool setup_link = true;

	/* Make sure the phy device is disabled */
	eth_macb_dev_set_link_down(dev);

	/* phydev soft reset */
	if (phydev->drv && phydev->drv->soft_reset)
		phydev->drv->soft_reset(phydev);

	if (phydev->drv && phydev->drv->config_init)
		phydev->drv->config_init(phydev);

	/* hw reset */
	macb_reset_hw(bp);

	/* set mac addr */
	eth_macb_set_hwaddr(bp);

	/* hw init */
	macb_init_hw(bp);

	/* tx queue phyaddr check */
	err = macb_tx_phyaddr_check(dev);
	if (err) {
		MACB_LOG(ERR, "Tx phyaddr check failed.");
		goto out;
	}

	/* Init tx queue include mbuf mem alloc */
	macb_eth_tx_init(dev);

	/* rx queue phyaddr check */
	err = macb_rx_phyaddr_check(dev);
	if (err) {
		MACB_LOG(ERR, "Rx phyaddr check failed.");
		goto out;
	}

	/* Init rx queue include mbuf mem alloc */
	err = macb_eth_rx_init(dev);
	if (err) {
		MACB_LOG(ERR, "Rx init failed.");
		goto out;
	}

	macb_configure_dma(bp);

	/* Enable receive and transmit. */
	macb_enable_rxtx(bp);

	/* Make interface link up */
	err = eth_macb_dev_set_link_up(dev);
	if (err) {
		MACB_LOG(ERR, "Failed to set link up");
		goto out;
	}

	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_USXGMII ||
	    bp->phy_interface == MACB_PHY_INTERFACE_MODE_2500BASEX ||
	    bp->phy_interface == MACB_PHY_INTERFACE_MODE_1000BASEX ||
	    bp->phy_interface == MACB_PHY_INTERFACE_MODE_100BASEX ||
	    (bp->phy_interface == MACB_PHY_INTERFACE_MODE_SGMII && bp->fixed_link))
		setup_link = false;

	/* Setup link speed and duplex */
	if (setup_link) {
		speeds = &dev->data->dev_conf.link_speeds;
		if (*speeds == RTE_ETH_LINK_SPEED_AUTONEG) {
			bp->autoneg = RTE_ETH_LINK_AUTONEG;
		} else {
			num_speeds = 0;
			bp->autoneg = RTE_ETH_LINK_FIXED;

			if (*speeds &
			    ~(RTE_ETH_LINK_SPEED_10M_HD | RTE_ETH_LINK_SPEED_10M |
			    RTE_ETH_LINK_SPEED_100M_HD | RTE_ETH_LINK_SPEED_100M |
			    RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_FIXED |
			    RTE_ETH_LINK_SPEED_2_5G)) {
				num_speeds = -1;
				goto error_invalid_config;
			}
			if (*speeds & RTE_ETH_LINK_SPEED_10M_HD) {
				bp->speed = RTE_ETH_SPEED_NUM_10M;
				bp->duplex = RTE_ETH_LINK_HALF_DUPLEX;
				num_speeds++;
			} else if (*speeds & RTE_ETH_LINK_SPEED_10M) {
				bp->speed = RTE_ETH_SPEED_NUM_10M;
				bp->duplex = RTE_ETH_LINK_FULL_DUPLEX;
				num_speeds++;
			} else if (*speeds & RTE_ETH_LINK_SPEED_100M_HD) {
				bp->speed = RTE_ETH_SPEED_NUM_100M;
				bp->duplex = RTE_ETH_LINK_HALF_DUPLEX;
				num_speeds++;
			} else if (*speeds & RTE_ETH_LINK_SPEED_100M) {
				bp->speed = RTE_ETH_SPEED_NUM_100M;
				bp->duplex = RTE_ETH_LINK_FULL_DUPLEX;
				num_speeds++;
			} else if (*speeds & RTE_ETH_LINK_SPEED_1G) {
				bp->speed = RTE_ETH_SPEED_NUM_1G;
				bp->duplex = RTE_ETH_LINK_FULL_DUPLEX;
				num_speeds++;
			} else if (*speeds & RTE_ETH_LINK_SPEED_2_5G) {
				bp->speed = RTE_ETH_SPEED_NUM_2_5G;
				bp->duplex = RTE_ETH_LINK_FULL_DUPLEX;
				num_speeds++;
			}
			if (num_speeds == 0) {
				err = -EINVAL;
				goto error_invalid_config;
			}
		}
		macb_setup_link(bp);
	}

	eth_macb_stats_reset(dev);
	if (!bp->phydrv_used)
		bp->link = true;

	priv->stopped = false;
	return 0;
error_invalid_config:
	MACB_LOG(ERR, "Invalid advertised speeds (%u) for port %u",
			 dev->data->dev_conf.link_speeds, dev->data->port_id);
out:
	MACB_LOG(ERR, "Failed to start device");
	return err;
}

static int eth_macb_dev_stop(struct rte_eth_dev *dev)
{
	u32 i;
	struct rte_eth_link link;
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;

	if (priv->stopped)
		return 0;

	/* link down the interface */
	eth_macb_dev_set_link_down(dev);

	/* reset hw reg */
	macb_reset_hw(bp);

	/* release rx queue mbuf free mem */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct macb_rx_queue *rx_queue;
		if (!dev->data->rx_queues[i])
			continue;
		rx_queue = dev->data->rx_queues[i];
		macb_rx_queue_release_mbufs(rx_queue);
		macb_reset_rx_queue(rx_queue);
	}

	/* release tx queue mbuf free mem */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct macb_tx_queue *tx_queue;
		if (!dev->data->tx_queues[i])
			continue;
		tx_queue = dev->data->tx_queues[i];
		macb_tx_queue_release_mbufs(tx_queue);
		macb_reset_tx_queue(tx_queue, dev);
	}

	/* clear the recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	if (!bp->phydrv_used)
		bp->link = false;
	dev->data->dev_started = 0;
	priv->stopped = true;
	return 0;
}

/**
 * DPDK callback to close the device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static int eth_macb_dev_close(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	int ret = 0, loop = 10;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret = eth_macb_dev_stop(dev);

	do {
		loop--;
		int err;
		err = rte_intr_callback_unregister(priv->intr_handle,
						   macb_interrupt_handler, dev);
		if (err > 0)
			break;
		if (err != -EAGAIN || !loop) {
			MACB_LOG(WARNING, "Failed to unregister lsc callback.");
			break;
		}
		rte_delay_ms(10);
	} while (true);

	macb_dev_free_queues(dev);

	/* Ensure that register operations are completed before unmap. */
	rte_delay_ms(100);
	macb_iomem_deinit(priv->bp);
	rte_free(priv->bp->phydev);
	rte_free(priv->bp);

	macb_dev_num--;

	return ret;
}

static const struct eth_dev_ops macb_ops = {
	.dev_set_link_up = eth_macb_dev_set_link_up,
	.dev_set_link_down = eth_macb_dev_set_link_down,
	.link_update = eth_macb_link_update,
	.dev_configure = eth_macb_dev_configure,
	.rx_queue_setup = macb_eth_rx_queue_setup,
	.tx_queue_setup = macb_eth_tx_queue_setup,
	.rx_queue_release = macb_eth_rx_queue_release,
	.tx_queue_release = macb_eth_tx_queue_release,
	.dev_start = eth_macb_dev_start,
	.dev_stop = eth_macb_dev_stop,
	.dev_close = eth_macb_dev_close,
	.stats_get = eth_macb_stats_get,
	.stats_reset = eth_macb_stats_reset,
	.rxq_info_get = macb_rxq_info_get,
	.txq_info_get = macb_txq_info_get,
	.dev_infos_get = eth_macb_dev_infos_get,
	.mtu_set = eth_macb_mtu_set,
	.dev_supported_ptypes_get = eth_macb_dev_supported_ptypes_get,
	.promiscuous_enable = eth_macb_promiscuous_enable,
	.promiscuous_disable = eth_macb_promiscuous_disable,
	.allmulticast_enable = eth_macb_allmulticast_enable,
	.allmulticast_disable = eth_macb_allmulticast_disable,
	.mac_addr_set = eth_macb_set_default_mac_addr,
};

/**
 * Callback used by rte_kvargs_process() during argument parsing.
 *
 * @param key
 *   Pointer to the parsed key (unused).
 * @param value
 *   Pointer to the parsed value.
 * @param extra_args
 *   Pointer to the extra arguments which contains address of the
 *   table of pointers to parsed interface names.
 *
 * @return
 *   Always 0.
 */
static int macb_devices_get(const char *key __rte_unused, const char *value,
							void *extra_args)
{
	struct macb_devices *devices = extra_args;

	devices->names[devices->idx++] = value;

	return 0;
}

static int macb_phydrv_used_get(const char *key __rte_unused, const char *value,
							void *extra_args)
{
	bool *phydrv_used = extra_args;

	*phydrv_used = (bool)atoi(value);

	return 0;
}

/**
 * Init device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
static int macb_dev_init(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	struct macb *bp = priv->bp;
	int ret;

	dev->data->mac_addrs =
		rte_zmalloc("mac_addrs", RTE_ETHER_ADDR_LEN * MACB_MAC_ADDRS_MAX, 0);
	if (!dev->data->mac_addrs) {
		MACB_LOG(ERR, "Failed to allocate space for eth addrs");
		ret = -ENOMEM;
		goto out_free;
	}

	/* Initialize local interrupt handle for current port. */
	priv->intr_handle =
		rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_SHARED);
	if (priv->intr_handle == NULL) {
		MACB_LOG(ERR, "Fail to allocate intr_handle");
		ret = -EFAULT;
		goto out_free;
	}

	dev->rx_pkt_burst = macb_eth_recv_pkts;
	dev->tx_pkt_burst = macb_eth_xmit_pkts;
	dev->dev_ops = &macb_ops;
	dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS | RTE_ETH_DEV_INTR_LSC;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		if (dev->data->scattered_rx)
			dev->rx_pkt_burst = &macb_eth_recv_scattered_pkts;
		return 0;
	}

	bp->dev = dev;

	if (!bp->iomem) {
		ret = macb_iomem_init(priv->name, bp, priv->physical_addr);
		if (ret) {
			MACB_LOG(ERR, "Failed to init device's iomem.");
			ret = -EFAULT;
			goto out_free;
		}
	}

	if (rte_intr_fd_set(priv->intr_handle, bp->iomem->fd))
		return -rte_errno;

	if (rte_intr_type_set(priv->intr_handle, RTE_INTR_HANDLE_UIO))
		return -rte_errno;

	return 0;
out_free:
	return ret;
}

static int macb_get_dev_pclk(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	char pclk_hz[CLK_STR_LEN];
	char *s;
	char filename[PATH_MAX];

	snprintf(filename, PATH_MAX, "%s/%s/pclk_hz", MACB_PDEV_PATH, priv->name);

	FILE *file = fopen(filename, "r");
	if (!file) {
		MACB_LOG(ERR, "There is no macb_uio_pclk file!");
		return -ENFILE;
	}

	memset(pclk_hz, 0, CLK_STR_LEN);

	s = fgets(pclk_hz, CLK_STR_LEN, file);
	if (!s) {
		fclose(file);
		MACB_LOG(ERR, "get phy pclk error!");
		return -EINVAL;
	}

	priv->pclk_hz = atol(pclk_hz);
	fclose(file);
	return 0;
}

static const char *macb_phy_modes(phy_interface_t interface)
{
	switch (interface) {
	case MACB_PHY_INTERFACE_MODE_NA:
		return "";
	case MACB_PHY_INTERFACE_MODE_INTERNAL:
		return "internal";
	case MACB_PHY_INTERFACE_MODE_MII:
		return "mii";
	case MACB_PHY_INTERFACE_MODE_GMII:
		return "gmii";
	case MACB_PHY_INTERFACE_MODE_SGMII:
		return "sgmii";
	case MACB_PHY_INTERFACE_MODE_TBI:
		return "tbi";
	case MACB_PHY_INTERFACE_MODE_REVMII:
		return "rev-mii";
	case MACB_PHY_INTERFACE_MODE_RMII:
		return "rmii";
	case MACB_PHY_INTERFACE_MODE_RGMII:
		return "rgmii";
	case MACB_PHY_INTERFACE_MODE_RGMII_ID:
		return "rgmii-id";
	case MACB_PHY_INTERFACE_MODE_RGMII_RXID:
		return "rgmii-rxid";
	case MACB_PHY_INTERFACE_MODE_RGMII_TXID:
		return "rgmii-txid";
	case MACB_PHY_INTERFACE_MODE_RTBI:
		return "rtbi";
	case MACB_PHY_INTERFACE_MODE_SMII:
		return "smii";
	case MACB_PHY_INTERFACE_MODE_XGMII:
		return "xgmii";
	case MACB_PHY_INTERFACE_MODE_MOCA:
		return "moca";
	case MACB_PHY_INTERFACE_MODE_QSGMII:
		return "qsgmii";
	case MACB_PHY_INTERFACE_MODE_TRGMII:
		return "trgmii";
	case MACB_PHY_INTERFACE_MODE_100BASEX:
		return "100base-x";
	case MACB_PHY_INTERFACE_MODE_1000BASEX:
		return "1000base-x";
	case MACB_PHY_INTERFACE_MODE_2500BASEX:
		return "2500base-x";
	case MACB_PHY_INTERFACE_MODE_5GBASER:
		return "5gbase-r";
	case MACB_PHY_INTERFACE_MODE_RXAUI:
		return "rxaui";
	case MACB_PHY_INTERFACE_MODE_XAUI:
		return "xaui";
	case MACB_PHY_INTERFACE_MODE_10GBASER:
		return "10gbase-r";
	case MACB_PHY_INTERFACE_MODE_USXGMII:
		return "usxgmii";
	case MACB_PHY_INTERFACE_MODE_10GKR:
		return "10gbase-kr";
	default:
		return "unknown";
	}
}

static int macb_get_phy_mode(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	char phy_mode[PHY_MODE_LEN];
	char *s;
	int i;
	char filename[PATH_MAX];

	snprintf(filename, PATH_MAX, "%s/%s/phy_mode", MACB_PDEV_PATH, priv->name);

	FILE *file = fopen(filename, "r");
	if (!file) {
		MACB_LOG(ERR, "There is no phy_mode file!");
		return -ENFILE;
	}

	memset(phy_mode, 0, PHY_MODE_LEN);

	s = fgets(phy_mode, PHY_MODE_LEN, file);
	if (!s) {
		fclose(file);
		MACB_LOG(ERR, "get phy mode error!");
		return -EINVAL;
	}

	priv->phy_interface = MACB_PHY_INTERFACE_MODE_MAX + 1;
	for (i = 0; i < MACB_PHY_INTERFACE_MODE_MAX; i++) {
		if (!strcasecmp(phy_mode, macb_phy_modes(i))) {
			priv->phy_interface = i;
			break;
		}
	}

	if (priv->phy_interface > MACB_PHY_INTERFACE_MODE_MAX) {
		MACB_LOG(ERR, "Invalid phy_mode value: %s!", phy_mode);
		return -EINVAL;
	}

	fclose(file);
	return 0;
}

static int macb_get_physical_addr(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	char physical_addr[PHY_ADDR_LEN];
	char *s;
	char *stopstr;
	char filename[PATH_MAX];

	snprintf(filename, PATH_MAX, "%s/%s/physical_addr", MACB_PDEV_PATH, priv->name);

	FILE *file = fopen(filename, "r");
	if (!file) {
		MACB_LOG(ERR, "There is no physical_addr file!");
		return -ENFILE;
	}

	memset(physical_addr, 0, PHY_ADDR_LEN);

	s = fgets(physical_addr, PHY_ADDR_LEN, file);
	if (!s) {
		fclose(file);
		MACB_LOG(ERR, "get physical address error!");
		return -EINVAL;
	}

	priv->physical_addr = strtoul(physical_addr, &stopstr, 16);
	fclose(file);
	return 0;
}

static int macb_get_dev_type(struct rte_eth_dev *dev)
{
	struct macb_priv *priv = dev->data->dev_private;
	char dev_type[DEV_TYPE_LEN];
	char *s;
	char filename[PATH_MAX];
	priv->dev_type = DEV_TYPE_DEFAULT;

	snprintf(filename, PATH_MAX, "%s/%s/dev_type", MACB_PDEV_PATH, priv->name);

	FILE *file = fopen(filename, "r");
	if (!file) {
		MACB_LOG(ERR, "There is no macb_dev_type file!");
		return -ENFILE;
	}

	memset(dev_type, 0, DEV_TYPE_LEN);

	s = fgets(dev_type, DEV_TYPE_LEN, file);
	if (!s) {
		fclose(file);
		MACB_LOG(ERR, "get dev type error!");
		return -EINVAL;
	}
	if (!strcmp(dev_type, OF_PHYTIUM_GEM1P0_MAC) ||
	    !strcmp(dev_type, ACPI_PHYTIUM_GEM1P0_MAC)) {
		priv->dev_type = DEV_TYPE_PHYTIUM_GEM1P0_MAC;
	} else if (!strcmp(dev_type, OF_PHYTIUM_GEM2P0_MAC)) {
		priv->dev_type = DEV_TYPE_PHYTIUM_GEM2P0_MAC;
	} else {
		MACB_LOG(ERR, "Unsupported device type: %s.", dev_type);
		return -EINVAL;
	}

	fclose(file);
	return 0;
}

static int macb_get_speed_info(struct rte_eth_dev *dev, char *speed_info)
{
	char filename[PATH_MAX];
	char *s;
	struct macb_priv *priv = dev->data->dev_private;

	if (!speed_info) {
		MACB_LOG(ERR, "speed info is NULL.");
		return -ENOMEM;
	}

	snprintf(filename, PATH_MAX, "%s/%s/speed_info", MACB_PDEV_PATH, priv->name);
	FILE *file = fopen(filename, "r");
	if (!file) {
		MACB_LOG(ERR, "There is no speed_info file!");
		return -ENFILE;
	}

	s = fgets(speed_info, SPEED_INFO_LEN, file);
	if (!s) {
		fclose(file);
		MACB_LOG(ERR, "get speed info error!");
		return -EINVAL;
	}

	fclose(file);
	return 0;
}

static int macb_get_fixed_link_speed_info(struct rte_eth_dev *dev, struct macb *bp)
{
	char speed_info[SPEED_INFO_LEN];
	char *duplex = NULL;
	int ret = 0;

	memset(speed_info, 0, SPEED_INFO_LEN);

	ret = macb_get_speed_info(dev, speed_info);
	if (ret)
		return ret;

	if (!strcmp(speed_info, "unknown")) {
		MACB_LOG(ERR, "speed info is unknown.");
		return -EINVAL;
	} else if (!strncmp(speed_info, "fixed-link", 10)) {
		bp->speed = atoi(speed_info + 11);
		duplex = strstr(speed_info, "full-duplex");
		if (duplex) {
			bp->duplex = DUPLEX_FULL;
			return 0;
		}
		duplex = strstr(speed_info, "half-duplex");
		if (duplex) {
			bp->duplex = DUPLEX_HALF;
			return 0;
		}
	} else {
		MACB_LOG(ERR, "Unsupported speed_info : %s.", speed_info);
		return -EINVAL;
	}

	return -EINVAL;
}

static int macb_update_fixed_link(struct rte_eth_dev *dev, struct macb *bp)
{
	int ret = 0;
	char speed_info[SPEED_INFO_LEN] = {0};

	ret = macb_get_speed_info(dev, speed_info);
	if (ret)
		return ret;

	if (!strncmp(speed_info, "fixed-link", 10))
		bp->fixed_link = true;
	return ret;
}

/**
 * Create device representing Ethernet port.
 *
 * @param ethdev_name
 *   Pointer to the ethdev's name. example: net_macb0
 *
 * @param dev_name
 *   Pointer to the port's name. example: 3200c000.ethernet
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int macb_dev_create(struct rte_vdev_device *vdev, const char *ethdev_name,
			   const char *dev_name, bool phydrv_used)
{
	int ret;
	struct rte_eth_dev *eth_dev;
	struct macb_priv *priv;
	struct macb *bp;
	struct phy_device *phydev;

	eth_dev = rte_eth_dev_allocate(ethdev_name);
	if (!eth_dev) {
		MACB_LOG(ERR, "failed to allocate eth_dev.");
		return -ENOMEM;
	}

	if (eth_dev->data->dev_private)
		goto create_done;

	priv = rte_zmalloc_socket(ethdev_name, sizeof(*priv), 0, rte_socket_id());
	if (!priv) {
		ret = -ENOMEM;
		goto out_free;
	}

	bp = rte_zmalloc_socket(ethdev_name, sizeof(*bp), 0, rte_socket_id());
	if (!bp) {
		ret = -EPERM;
		goto out_free;
	}

	phydev = rte_zmalloc_socket(ethdev_name, sizeof(*phydev), 0, rte_socket_id());
	if (!phydev) {
		ret = -EPERM;
		goto out_free_bp;
	}

	eth_dev->device = &vdev->device;
	eth_dev->data->dev_private = priv;
	priv->bp = bp;
	strlcpy(priv->name, dev_name, sizeof(priv->name));
	bp->link = false;
	bp->fixed_link = false;
	bp->phydrv_used = phydrv_used;
	bp->phydev = phydev;
	phydev->bp = bp;
	priv->stopped = true;

	ret = macb_get_dev_pclk(eth_dev);
	if (ret)
		goto out_free_phydev;

	ret = macb_get_phy_mode(eth_dev);
	if (ret)
		goto out_free_phydev;
	bp->phy_interface = priv->phy_interface;

	ret = macb_get_physical_addr(eth_dev);
	if (ret)
		goto out_free_phydev;

	ret = macb_dev_init(eth_dev);
	if (ret)
		goto out_free_phydev;

	ret = macb_get_dev_type(eth_dev);
	if (ret)
		goto out_free_phydev;

	ret = macb_update_fixed_link(eth_dev, bp);
	if (ret)
		goto out_free_phydev;

	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_USXGMII) {
		ret = macb_get_fixed_link_speed_info(eth_dev, bp);
		if (ret < 0) {
			bp->speed = SPEED_10000;
			bp->duplex = DUPLEX_FULL;
		}
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_2500BASEX) {
		bp->speed = SPEED_2500;
		bp->duplex = DUPLEX_FULL;
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_1000BASEX) {
		bp->speed = SPEED_1000;
		bp->duplex = DUPLEX_FULL;
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_100BASEX) {
		bp->speed = SPEED_100;
		bp->duplex = DUPLEX_FULL;
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_SGMII && bp->fixed_link) {
		ret = macb_get_fixed_link_speed_info(eth_dev, bp);
		if (ret < 0) {
			bp->speed = SPEED_1000;
			bp->duplex = DUPLEX_FULL;
		}
	} else {
		bp->speed = SPEED_UNKNOWN;
		bp->duplex = DUPLEX_UNKNOWN;
	}

	macb_phy_auto_detect(eth_dev);

	ret = rte_intr_callback_register(priv->intr_handle, macb_interrupt_handler,
									 (void *)eth_dev);
	if (ret) {
		MACB_LOG(ERR, "register callback failed.");
		goto out_free_phydev;
	}

	rte_eth_dev_probing_finish(eth_dev);
create_done:
	return 0;

out_free_phydev:
	rte_free(phydev);
out_free_bp:
	rte_free(bp);

out_free:
	rte_eth_dev_release_port(eth_dev);

	return ret;
}

/**
 * DPDK callback to remove virtual device.
 *
 * @param vdev
 *   Pointer to the removed virtual device.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int rte_pmd_macb_remove(struct rte_vdev_device *vdev)
{
	uint16_t dev_id;
	int ret = 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	RTE_ETH_FOREACH_DEV(dev_id) {
		if (rte_eth_devices[dev_id].device != &vdev->device)
			continue;
		ret |= rte_eth_dev_close(dev_id);
	}

	return ret == 0 ? 0 : -EIO;
}

/**
 * DPDK callback to register the virtual device.
 *
 * @param vdev
 *   Pointer to the virtual device.
 *
 * @return
 *   0 on success, negative error value otherwise.
 */
static int rte_pmd_macb_probe(struct rte_vdev_device *vdev)
{
	struct rte_kvargs *kvlist;
	struct macb_devices devices;
	bool phydrv_used = true;
	int ret = -EINVAL;
	uint32_t i, dev_num;
	const char *params;
	enum rte_iova_mode iova_mode;
	char ethdev_name[RTE_DEV_NAME_MAX_LEN] = "";
	const char *vdev_name;
	struct rte_eth_dev *eth_dev;

	vdev_name = rte_vdev_device_name(vdev);

	/* secondary process probe */
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(vdev_name);
		if (!eth_dev) {
			MACB_LOG(ERR, "Secondary failed to probe eth_dev.");
			return -1;
		}

		if (vdev->device.numa_node == SOCKET_ID_ANY)
			vdev->device.numa_node = rte_socket_id();
		eth_dev->device = &vdev->device;
		rte_eth_dev_probing_finish(eth_dev);

		return 0;
	}

	iova_mode = rte_eal_iova_mode();
	if (iova_mode != RTE_IOVA_PA) {
		MACB_LOG(ERR, "Expecting 'PA' IOVA mode but current mode is 'VA', not "
					  "initializing");
		return -EINVAL;
	}

	params = rte_vdev_device_args(vdev);
	if (!params) {
		MACB_LOG(ERR, "failed to get the args.");
		return -EINVAL;
	}

	kvlist = rte_kvargs_parse(params, valid_args);
	if (!kvlist) {
		MACB_LOG(ERR, "failed to parse the kvargs.");
		return -EINVAL;
	}

	rte_kvargs_process(kvlist, MACB_USE_PHYDRV_ARG, macb_phydrv_used_get, &phydrv_used);

	dev_num = rte_kvargs_count(kvlist, MACB_DEVICE_NAME_ARG);

	/* compatibility support */
	if (!strcmp(vdev_name, "net_macb")) {
		if (dev_num > MACB_MAX_PORT_NUM) {
			ret = -EINVAL;
			MACB_LOG(ERR, "number of devices exceeded. Maximum value: %d.",
				 MACB_MAX_PORT_NUM);
			goto out_free_kvlist;
		}
	} else {
		if (dev_num != 1) {
			ret = -EINVAL;
			MACB_LOG(ERR, "Error args: one vdev to one device.");
			goto out_free_kvlist;
		}
	}

	devices.idx = 0;
	rte_kvargs_process(kvlist, MACB_DEVICE_NAME_ARG, macb_devices_get, &devices);

	MACB_INFO("Phytium mac driver v%s", MACB_DRIVER_VERSION);

	for (i = 0; i < dev_num; i++) {
		if (dev_num > 1)
			snprintf(ethdev_name, RTE_DEV_NAME_MAX_LEN, "%s%d", vdev_name, i);
		else
			snprintf(ethdev_name, RTE_DEV_NAME_MAX_LEN, "%s", vdev_name);

		ret = macb_dev_create(vdev, ethdev_name, devices.names[i], phydrv_used);
		if (ret) {
			MACB_LOG(ERR, "failed to create device.");
			goto out_cleanup;
		}

		macb_dev_num++;
	}

	rte_kvargs_free(kvlist);
	return 0;

out_cleanup:
	rte_pmd_macb_remove(vdev);

out_free_kvlist:
	rte_kvargs_free(kvlist);

	return ret;
}

static struct rte_vdev_driver pmd_macb_drv = {
	.probe = rte_pmd_macb_probe,
	.remove = rte_pmd_macb_remove,
};

RTE_PMD_REGISTER_KMOD_DEP(net_macb, "macb_uio");
RTE_PMD_REGISTER_VDEV(net_macb, pmd_macb_drv);
RTE_PMD_REGISTER_PARAM_STRING(net_macb,
					MACB_DEVICE_NAME_ARG "=<string> "
					MACB_USE_PHYDRV_ARG "=<int>");

RTE_INIT(macb_init_log)
{
	if (macb_log_initialized)
		return;

	macb_logtype = rte_log_register("pmd.net.macb");

	macb_log_initialized = 1;
}
