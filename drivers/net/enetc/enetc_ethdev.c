/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2020 NXP
 */

#include <stdbool.h>
#include <ethdev_pci.h>
#include <rte_random.h>
#include <dpaax_iova_table.h>

#include "enetc_logs.h"
#include "enetc.h"

static int
enetc_dev_start(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t val;

	PMD_INIT_FUNC_TRACE();
	if (hw->device_id == ENETC_DEV_ID_VF)
		return 0;

	val = enetc_port_rd(enetc_hw, ENETC_PM0_CMD_CFG);
	enetc_port_wr(enetc_hw, ENETC_PM0_CMD_CFG,
		      val | ENETC_PM0_TX_EN | ENETC_PM0_RX_EN);

	/* Enable port */
	val = enetc_port_rd(enetc_hw, ENETC_PMR);
	enetc_port_wr(enetc_hw, ENETC_PMR, val | ENETC_PMR_EN);

	/* set auto-speed for RGMII */
	if (enetc_port_rd(enetc_hw, ENETC_PM0_IF_MODE) & ENETC_PMO_IFM_RG) {
		enetc_port_wr(enetc_hw, ENETC_PM0_IF_MODE,
			      ENETC_PM0_IFM_RGAUTO);
		enetc_port_wr(enetc_hw, ENETC_PM1_IF_MODE,
			      ENETC_PM0_IFM_RGAUTO);
	}
	if (enetc_global_rd(enetc_hw,
			    ENETC_G_EPFBLPR(1)) == ENETC_G_EPFBLPR1_XGMII) {
		enetc_port_wr(enetc_hw, ENETC_PM0_IF_MODE,
			      ENETC_PM0_IFM_XGMII);
		enetc_port_wr(enetc_hw, ENETC_PM1_IF_MODE,
			      ENETC_PM0_IFM_XGMII);
	}

	return 0;
}

static int
enetc_dev_stop(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t val;

	PMD_INIT_FUNC_TRACE();
	dev->data->dev_started = 0;
	if (hw->device_id == ENETC_DEV_ID_VF)
		return 0;

	/* Disable port */
	val = enetc_port_rd(enetc_hw, ENETC_PMR);
	enetc_port_wr(enetc_hw, ENETC_PMR, val & (~ENETC_PMR_EN));

	val = enetc_port_rd(enetc_hw, ENETC_PM0_CMD_CFG);
	enetc_port_wr(enetc_hw, ENETC_PM0_CMD_CFG,
		      val & (~(ENETC_PM0_TX_EN | ENETC_PM0_RX_EN)));

	return 0;
}

static const uint32_t *
enetc_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_UNKNOWN
	};

	return ptypes;
}

/* return 0 means link status changed, -1 means not changed */
static int
enetc_link_update(struct rte_eth_dev *dev, int wait_to_complete __rte_unused)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct rte_eth_link link;
	uint32_t status;

	PMD_INIT_FUNC_TRACE();

	memset(&link, 0, sizeof(link));

	status = enetc_port_rd(enetc_hw, ENETC_PM0_STATUS);

	if (status & ENETC_LINK_MODE)
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	else
		link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;

	if (status & ENETC_LINK_STATUS)
		link.link_status = RTE_ETH_LINK_UP;
	else
		link.link_status = RTE_ETH_LINK_DOWN;

	switch (status & ENETC_LINK_SPEED_MASK) {
	case ENETC_LINK_SPEED_1G:
		link.link_speed = RTE_ETH_SPEED_NUM_1G;
		break;

	case ENETC_LINK_SPEED_100M:
		link.link_speed = RTE_ETH_SPEED_NUM_100M;
		break;

	default:
	case ENETC_LINK_SPEED_10M:
		link.link_speed = RTE_ETH_SPEED_NUM_10M;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

static void
print_ethaddr(const char *name, const struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	ENETC_PMD_NOTICE("%s%s\n", name, buf);
}

static int
enetc_hardware_init(struct enetc_eth_hw *hw)
{
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t *mac = (uint32_t *)hw->mac.addr;
	uint32_t high_mac = 0;
	uint16_t low_mac = 0;

	PMD_INIT_FUNC_TRACE();
	/* Calculating and storing the base HW addresses */
	hw->hw.port = (void *)((size_t)hw->hw.reg + ENETC_PORT_BASE);
	hw->hw.global = (void *)((size_t)hw->hw.reg + ENETC_GLOBAL_BASE);

	/* WA for Rx lock-up HW erratum */
	enetc_port_wr(enetc_hw, ENETC_PM0_RX_FIFO, 1);

	/* set ENETC transaction flags to coherent, don't allocate.
	 * BD writes merge with surrounding cache line data, frame data writes
	 * overwrite cache line.
	 */
	enetc_wr(enetc_hw, ENETC_SICAR0, ENETC_SICAR0_COHERENT);

	/* Enabling Station Interface */
	enetc_wr(enetc_hw, ENETC_SIMR, ENETC_SIMR_EN);


	if (hw->device_id == ENETC_DEV_ID_VF) {
		*mac = (uint32_t)enetc_rd(enetc_hw, ENETC_SIPMAR0);
		high_mac = (uint32_t)*mac;
		mac++;
		*mac = (uint32_t)enetc_rd(enetc_hw, ENETC_SIPMAR1);
		low_mac = (uint16_t)*mac;
	} else {
		*mac = (uint32_t)enetc_port_rd(enetc_hw, ENETC_PSIPMAR0(0));
		high_mac = (uint32_t)*mac;
		mac++;
		*mac = (uint16_t)enetc_port_rd(enetc_hw, ENETC_PSIPMAR1(0));
		low_mac = (uint16_t)*mac;
	}

	if ((high_mac | low_mac) == 0) {
		char *first_byte;

		ENETC_PMD_NOTICE("MAC is not available for this SI, "
				"set random MAC\n");
		mac = (uint32_t *)hw->mac.addr;
		*mac = (uint32_t)rte_rand();
		first_byte = (char *)mac;
		*first_byte &= 0xfe;	/* clear multicast bit */
		*first_byte |= 0x02;	/* set local assignment bit (IEEE802) */

		enetc_port_wr(enetc_hw, ENETC_PSIPMAR0(0), *mac);
		mac++;
		*mac = (uint16_t)rte_rand();
		enetc_port_wr(enetc_hw, ENETC_PSIPMAR1(0), *mac);
		print_ethaddr("New address: ",
			      (const struct rte_ether_addr *)hw->mac.addr);
	}

	return 0;
}

static int
enetc_dev_infos_get(struct rte_eth_dev *dev __rte_unused,
		    struct rte_eth_dev_info *dev_info)
{
	PMD_INIT_FUNC_TRACE();
	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = MAX_BD_COUNT,
		.nb_min = MIN_BD_COUNT,
		.nb_align = BD_ALIGN,
	};
	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = MAX_BD_COUNT,
		.nb_min = MIN_BD_COUNT,
		.nb_align = BD_ALIGN,
	};
	dev_info->max_rx_queues = MAX_RX_RINGS;
	dev_info->max_tx_queues = MAX_TX_RINGS;
	dev_info->max_rx_pktlen = ENETC_MAC_MAXFRM_SIZE;
	dev_info->rx_offload_capa =
		(RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		 RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		 RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		 RTE_ETH_RX_OFFLOAD_KEEP_CRC);

	return 0;
}

static int
enetc_alloc_txbdr(struct enetc_bdr *txr, uint16_t nb_desc)
{
	int size;

	size = nb_desc * sizeof(struct enetc_swbd);
	txr->q_swbd = rte_malloc(NULL, size, ENETC_BD_RING_ALIGN);
	if (txr->q_swbd == NULL)
		return -ENOMEM;

	size = nb_desc * sizeof(struct enetc_tx_bd);
	txr->bd_base = rte_malloc(NULL, size, ENETC_BD_RING_ALIGN);
	if (txr->bd_base == NULL) {
		rte_free(txr->q_swbd);
		txr->q_swbd = NULL;
		return -ENOMEM;
	}

	txr->bd_count = nb_desc;
	txr->next_to_clean = 0;
	txr->next_to_use = 0;

	return 0;
}

static void
enetc_free_bdr(struct enetc_bdr *rxr)
{
	rte_free(rxr->q_swbd);
	rte_free(rxr->bd_base);
	rxr->q_swbd = NULL;
	rxr->bd_base = NULL;
}

static void
enetc_setup_txbdr(struct enetc_hw *hw, struct enetc_bdr *tx_ring)
{
	int idx = tx_ring->index;
	phys_addr_t bd_address;

	bd_address = (phys_addr_t)
		     rte_mem_virt2iova((const void *)tx_ring->bd_base);
	enetc_txbdr_wr(hw, idx, ENETC_TBBAR0,
		       lower_32_bits((uint64_t)bd_address));
	enetc_txbdr_wr(hw, idx, ENETC_TBBAR1,
		       upper_32_bits((uint64_t)bd_address));
	enetc_txbdr_wr(hw, idx, ENETC_TBLENR,
		       ENETC_RTBLENR_LEN(tx_ring->bd_count));

	enetc_txbdr_wr(hw, idx, ENETC_TBCIR, 0);
	enetc_txbdr_wr(hw, idx, ENETC_TBCISR, 0);
	tx_ring->tcir = (void *)((size_t)hw->reg +
			ENETC_BDR(TX, idx, ENETC_TBCIR));
	tx_ring->tcisr = (void *)((size_t)hw->reg +
			 ENETC_BDR(TX, idx, ENETC_TBCISR));
}

static int
enetc_tx_queue_setup(struct rte_eth_dev *dev,
		     uint16_t queue_idx,
		     uint16_t nb_desc,
		     unsigned int socket_id __rte_unused,
		     const struct rte_eth_txconf *tx_conf)
{
	int err = 0;
	struct enetc_bdr *tx_ring;
	struct rte_eth_dev_data *data = dev->data;
	struct enetc_eth_adapter *priv =
			ENETC_DEV_PRIVATE(data->dev_private);

	PMD_INIT_FUNC_TRACE();
	if (nb_desc > MAX_BD_COUNT)
		return -1;

	tx_ring = rte_zmalloc(NULL, sizeof(struct enetc_bdr), 0);
	if (tx_ring == NULL) {
		ENETC_PMD_ERR("Failed to allocate TX ring memory");
		err = -ENOMEM;
		return -1;
	}

	err = enetc_alloc_txbdr(tx_ring, nb_desc);
	if (err)
		goto fail;

	tx_ring->index = queue_idx;
	tx_ring->ndev = dev;
	enetc_setup_txbdr(&priv->hw.hw, tx_ring);
	data->tx_queues[queue_idx] = tx_ring;

	if (!tx_conf->tx_deferred_start) {
		/* enable ring */
		enetc_txbdr_wr(&priv->hw.hw, tx_ring->index,
			       ENETC_TBMR, ENETC_TBMR_EN);
		dev->data->tx_queue_state[tx_ring->index] =
			       RTE_ETH_QUEUE_STATE_STARTED;
	} else {
		dev->data->tx_queue_state[tx_ring->index] =
			       RTE_ETH_QUEUE_STATE_STOPPED;
	}

	return 0;
fail:
	rte_free(tx_ring);

	return err;
}

static void
enetc_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	void *txq = dev->data->tx_queues[qid];

	if (txq == NULL)
		return;

	struct enetc_bdr *tx_ring = (struct enetc_bdr *)txq;
	struct enetc_eth_hw *eth_hw =
		ENETC_DEV_PRIVATE_TO_HW(tx_ring->ndev->data->dev_private);
	struct enetc_hw *hw;
	struct enetc_swbd *tx_swbd;
	int i;
	uint32_t val;

	/* Disable the ring */
	hw = &eth_hw->hw;
	val = enetc_txbdr_rd(hw, tx_ring->index, ENETC_TBMR);
	val &= (~ENETC_TBMR_EN);
	enetc_txbdr_wr(hw, tx_ring->index, ENETC_TBMR, val);

	/* clean the ring*/
	i = tx_ring->next_to_clean;
	tx_swbd = &tx_ring->q_swbd[i];
	while (tx_swbd->buffer_addr != NULL) {
		rte_pktmbuf_free(tx_swbd->buffer_addr);
		tx_swbd->buffer_addr = NULL;
		tx_swbd++;
		i++;
		if (unlikely(i == tx_ring->bd_count)) {
			i = 0;
			tx_swbd = &tx_ring->q_swbd[i];
		}
	}

	enetc_free_bdr(tx_ring);
	rte_free(tx_ring);
}

static int
enetc_alloc_rxbdr(struct enetc_bdr *rxr,
		  uint16_t nb_rx_desc)
{
	int size;

	size = nb_rx_desc * sizeof(struct enetc_swbd);
	rxr->q_swbd = rte_malloc(NULL, size, ENETC_BD_RING_ALIGN);
	if (rxr->q_swbd == NULL)
		return -ENOMEM;

	size = nb_rx_desc * sizeof(union enetc_rx_bd);
	rxr->bd_base = rte_malloc(NULL, size, ENETC_BD_RING_ALIGN);
	if (rxr->bd_base == NULL) {
		rte_free(rxr->q_swbd);
		rxr->q_swbd = NULL;
		return -ENOMEM;
	}

	rxr->bd_count = nb_rx_desc;
	rxr->next_to_clean = 0;
	rxr->next_to_use = 0;
	rxr->next_to_alloc = 0;

	return 0;
}

static void
enetc_setup_rxbdr(struct enetc_hw *hw, struct enetc_bdr *rx_ring,
		  struct rte_mempool *mb_pool)
{
	int idx = rx_ring->index;
	uint16_t buf_size;
	phys_addr_t bd_address;

	bd_address = (phys_addr_t)
		     rte_mem_virt2iova((const void *)rx_ring->bd_base);
	enetc_rxbdr_wr(hw, idx, ENETC_RBBAR0,
		       lower_32_bits((uint64_t)bd_address));
	enetc_rxbdr_wr(hw, idx, ENETC_RBBAR1,
		       upper_32_bits((uint64_t)bd_address));
	enetc_rxbdr_wr(hw, idx, ENETC_RBLENR,
		       ENETC_RTBLENR_LEN(rx_ring->bd_count));

	rx_ring->mb_pool = mb_pool;
	rx_ring->rcir = (void *)((size_t)hw->reg +
			ENETC_BDR(RX, idx, ENETC_RBCIR));
	enetc_refill_rx_ring(rx_ring, (enetc_bd_unused(rx_ring)));
	buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rx_ring->mb_pool) -
		   RTE_PKTMBUF_HEADROOM);
	enetc_rxbdr_wr(hw, idx, ENETC_RBBSR, buf_size);
	enetc_rxbdr_wr(hw, idx, ENETC_RBPIR, 0);
}

static int
enetc_rx_queue_setup(struct rte_eth_dev *dev,
		     uint16_t rx_queue_id,
		     uint16_t nb_rx_desc,
		     unsigned int socket_id __rte_unused,
		     const struct rte_eth_rxconf *rx_conf,
		     struct rte_mempool *mb_pool)
{
	int err = 0;
	struct enetc_bdr *rx_ring;
	struct rte_eth_dev_data *data =  dev->data;
	struct enetc_eth_adapter *adapter =
			ENETC_DEV_PRIVATE(data->dev_private);
	uint64_t rx_offloads = data->dev_conf.rxmode.offloads;

	PMD_INIT_FUNC_TRACE();
	if (nb_rx_desc > MAX_BD_COUNT)
		return -1;

	rx_ring = rte_zmalloc(NULL, sizeof(struct enetc_bdr), 0);
	if (rx_ring == NULL) {
		ENETC_PMD_ERR("Failed to allocate RX ring memory");
		err = -ENOMEM;
		return err;
	}

	err = enetc_alloc_rxbdr(rx_ring, nb_rx_desc);
	if (err)
		goto fail;

	rx_ring->index = rx_queue_id;
	rx_ring->ndev = dev;
	enetc_setup_rxbdr(&adapter->hw.hw, rx_ring, mb_pool);
	data->rx_queues[rx_queue_id] = rx_ring;

	if (!rx_conf->rx_deferred_start) {
		/* enable ring */
		enetc_rxbdr_wr(&adapter->hw.hw, rx_ring->index, ENETC_RBMR,
			       ENETC_RBMR_EN);
		dev->data->rx_queue_state[rx_ring->index] =
			       RTE_ETH_QUEUE_STATE_STARTED;
	} else {
		dev->data->rx_queue_state[rx_ring->index] =
			       RTE_ETH_QUEUE_STATE_STOPPED;
	}

	rx_ring->crc_len = (uint8_t)((rx_offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC) ?
				     RTE_ETHER_CRC_LEN : 0);

	return 0;
fail:
	rte_free(rx_ring);

	return err;
}

static void
enetc_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	void *rxq = dev->data->rx_queues[qid];

	if (rxq == NULL)
		return;

	struct enetc_bdr *rx_ring = (struct enetc_bdr *)rxq;
	struct enetc_eth_hw *eth_hw =
		ENETC_DEV_PRIVATE_TO_HW(rx_ring->ndev->data->dev_private);
	struct enetc_swbd *q_swbd;
	struct enetc_hw *hw;
	uint32_t val;
	int i;

	/* Disable the ring */
	hw = &eth_hw->hw;
	val = enetc_rxbdr_rd(hw, rx_ring->index, ENETC_RBMR);
	val &= (~ENETC_RBMR_EN);
	enetc_rxbdr_wr(hw, rx_ring->index, ENETC_RBMR, val);

	/* Clean the ring */
	i = rx_ring->next_to_clean;
	q_swbd = &rx_ring->q_swbd[i];
	while (i != rx_ring->next_to_use) {
		rte_pktmbuf_free(q_swbd->buffer_addr);
		q_swbd->buffer_addr = NULL;
		q_swbd++;
		i++;
		if (unlikely(i == rx_ring->bd_count)) {
			i = 0;
			q_swbd = &rx_ring->q_swbd[i];
		}
	}

	enetc_free_bdr(rx_ring);
	rte_free(rx_ring);
}

static
int enetc_stats_get(struct rte_eth_dev *dev,
		    struct rte_eth_stats *stats)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;

	/* Total received packets, bad + good, if we want to get counters of
	 * only good received packets then use ENETC_PM0_RFRM,
	 * ENETC_PM0_TFRM registers.
	 */
	stats->ipackets = enetc_port_rd(enetc_hw, ENETC_PM0_RPKT);
	stats->opackets = enetc_port_rd(enetc_hw, ENETC_PM0_TPKT);
	stats->ibytes =  enetc_port_rd(enetc_hw, ENETC_PM0_REOCT);
	stats->obytes = enetc_port_rd(enetc_hw, ENETC_PM0_TEOCT);
	/* Dropped + Truncated packets, use ENETC_PM0_RDRNTP for without
	 * truncated packets
	 */
	stats->imissed = enetc_port_rd(enetc_hw, ENETC_PM0_RDRP);
	stats->ierrors = enetc_port_rd(enetc_hw, ENETC_PM0_RERR);
	stats->oerrors = enetc_port_rd(enetc_hw, ENETC_PM0_TERR);

	return 0;
}

static int
enetc_stats_reset(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;

	enetc_port_wr(enetc_hw, ENETC_PM0_STAT_CONFIG, ENETC_CLEAR_STATS);

	return 0;
}

static int
enetc_dev_close(struct rte_eth_dev *dev)
{
	uint16_t i;
	int ret;

	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	ret = enetc_dev_stop(dev);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		enetc_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		enetc_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;

	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_depopulate();

	return ret;
}

static int
enetc_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t psipmr = 0;

	psipmr = enetc_port_rd(enetc_hw, ENETC_PSIPMR);

	/* Setting to enable promiscuous mode*/
	psipmr |= ENETC_PSIPMR_SET_UP(0) | ENETC_PSIPMR_SET_MP(0);

	enetc_port_wr(enetc_hw, ENETC_PSIPMR, psipmr);

	return 0;
}

static int
enetc_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t psipmr = 0;

	/* Setting to disable promiscuous mode for SI0*/
	psipmr = enetc_port_rd(enetc_hw, ENETC_PSIPMR);
	psipmr &= (~ENETC_PSIPMR_SET_UP(0));

	if (dev->data->all_multicast == 0)
		psipmr &= (~ENETC_PSIPMR_SET_MP(0));

	enetc_port_wr(enetc_hw, ENETC_PSIPMR, psipmr);

	return 0;
}

static int
enetc_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t psipmr = 0;

	psipmr = enetc_port_rd(enetc_hw, ENETC_PSIPMR);

	/* Setting to enable allmulticast mode for SI0*/
	psipmr |= ENETC_PSIPMR_SET_MP(0);

	enetc_port_wr(enetc_hw, ENETC_PSIPMR, psipmr);

	return 0;
}

static int
enetc_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t psipmr = 0;

	if (dev->data->promiscuous == 1)
		return 0; /* must remain in all_multicast mode */

	/* Setting to disable all multicast mode for SI0*/
	psipmr = enetc_port_rd(enetc_hw, ENETC_PSIPMR) &
			       ~(ENETC_PSIPMR_SET_MP(0));

	enetc_port_wr(enetc_hw, ENETC_PSIPMR, psipmr);

	return 0;
}

static int
enetc_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t frame_size = mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;

	/*
	 * Refuse mtu that requires the support of scattered packets
	 * when this feature has not been enabled before.
	 */
	if (dev->data->min_rx_buf_size &&
		!dev->data->scattered_rx && frame_size >
		dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM) {
		ENETC_PMD_ERR("SG not enabled, will not fit in one buffer");
		return -EINVAL;
	}

	enetc_port_wr(enetc_hw, ENETC_PTCMSDUR(0), ENETC_MAC_MAXFRM_SIZE);
	enetc_port_wr(enetc_hw, ENETC_PTXMBAR, 2 * ENETC_MAC_MAXFRM_SIZE);

	/*setting the MTU*/
	enetc_port_wr(enetc_hw, ENETC_PM0_MAXFRM, ENETC_SET_MAXFRM(frame_size) |
		      ENETC_SET_TX_MTU(ENETC_MAC_MAXFRM_SIZE));

	return 0;
}

static int
enetc_dev_configure(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct rte_eth_conf *eth_conf = &dev->data->dev_conf;
	uint64_t rx_offloads = eth_conf->rxmode.offloads;
	uint32_t checksum = L3_CKSUM | L4_CKSUM;
	uint32_t max_len;

	PMD_INIT_FUNC_TRACE();

	max_len = dev->data->dev_conf.rxmode.mtu + RTE_ETHER_HDR_LEN +
		RTE_ETHER_CRC_LEN;
	enetc_port_wr(enetc_hw, ENETC_PM0_MAXFRM, ENETC_SET_MAXFRM(max_len));
	enetc_port_wr(enetc_hw, ENETC_PTCMSDUR(0), ENETC_MAC_MAXFRM_SIZE);
	enetc_port_wr(enetc_hw, ENETC_PTXMBAR, 2 * ENETC_MAC_MAXFRM_SIZE);

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC) {
		int config;

		config = enetc_port_rd(enetc_hw, ENETC_PM0_CMD_CFG);
		config |= ENETC_PM0_CRC;
		enetc_port_wr(enetc_hw, ENETC_PM0_CMD_CFG, config);
	}

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM)
		checksum &= ~L3_CKSUM;

	if (rx_offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_TCP_CKSUM))
		checksum &= ~L4_CKSUM;

	enetc_port_wr(enetc_hw, ENETC_PAR_PORT_CFG, checksum);


	return 0;
}

static int
enetc_rx_queue_start(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct enetc_eth_adapter *priv =
			ENETC_DEV_PRIVATE(dev->data->dev_private);
	struct enetc_bdr *rx_ring;
	uint32_t rx_data;

	rx_ring = dev->data->rx_queues[qidx];
	if (dev->data->rx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STOPPED) {
		rx_data = enetc_rxbdr_rd(&priv->hw.hw, rx_ring->index,
					 ENETC_RBMR);
		rx_data = rx_data | ENETC_RBMR_EN;
		enetc_rxbdr_wr(&priv->hw.hw, rx_ring->index, ENETC_RBMR,
			       rx_data);
		dev->data->rx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	return 0;
}

static int
enetc_rx_queue_stop(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct enetc_eth_adapter *priv =
			ENETC_DEV_PRIVATE(dev->data->dev_private);
	struct enetc_bdr *rx_ring;
	uint32_t rx_data;

	rx_ring = dev->data->rx_queues[qidx];
	if (dev->data->rx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STARTED) {
		rx_data = enetc_rxbdr_rd(&priv->hw.hw, rx_ring->index,
					 ENETC_RBMR);
		rx_data = rx_data & (~ENETC_RBMR_EN);
		enetc_rxbdr_wr(&priv->hw.hw, rx_ring->index, ENETC_RBMR,
			       rx_data);
		dev->data->rx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;
	}

	return 0;
}

static int
enetc_tx_queue_start(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct enetc_eth_adapter *priv =
			ENETC_DEV_PRIVATE(dev->data->dev_private);
	struct enetc_bdr *tx_ring;
	uint32_t tx_data;

	tx_ring = dev->data->tx_queues[qidx];
	if (dev->data->tx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STOPPED) {
		tx_data = enetc_txbdr_rd(&priv->hw.hw, tx_ring->index,
					 ENETC_TBMR);
		tx_data = tx_data | ENETC_TBMR_EN;
		enetc_txbdr_wr(&priv->hw.hw, tx_ring->index, ENETC_TBMR,
			       tx_data);
		dev->data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	return 0;
}

static int
enetc_tx_queue_stop(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct enetc_eth_adapter *priv =
			ENETC_DEV_PRIVATE(dev->data->dev_private);
	struct enetc_bdr *tx_ring;
	uint32_t tx_data;

	tx_ring = dev->data->tx_queues[qidx];
	if (dev->data->tx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STARTED) {
		tx_data = enetc_txbdr_rd(&priv->hw.hw, tx_ring->index,
					 ENETC_TBMR);
		tx_data = tx_data & (~ENETC_TBMR_EN);
		enetc_txbdr_wr(&priv->hw.hw, tx_ring->index, ENETC_TBMR,
			       tx_data);
		dev->data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;
	}

	return 0;
}

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_enetc_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_FREESCALE, ENETC_DEV_ID) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_FREESCALE, ENETC_DEV_ID_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

/* Features supported by this driver */
static const struct eth_dev_ops enetc_ops = {
	.dev_configure        = enetc_dev_configure,
	.dev_start            = enetc_dev_start,
	.dev_stop             = enetc_dev_stop,
	.dev_close            = enetc_dev_close,
	.link_update          = enetc_link_update,
	.stats_get            = enetc_stats_get,
	.stats_reset          = enetc_stats_reset,
	.promiscuous_enable   = enetc_promiscuous_enable,
	.promiscuous_disable  = enetc_promiscuous_disable,
	.allmulticast_enable  = enetc_allmulticast_enable,
	.allmulticast_disable = enetc_allmulticast_disable,
	.dev_infos_get        = enetc_dev_infos_get,
	.mtu_set              = enetc_mtu_set,
	.rx_queue_setup       = enetc_rx_queue_setup,
	.rx_queue_start       = enetc_rx_queue_start,
	.rx_queue_stop        = enetc_rx_queue_stop,
	.rx_queue_release     = enetc_rx_queue_release,
	.tx_queue_setup       = enetc_tx_queue_setup,
	.tx_queue_start       = enetc_tx_queue_start,
	.tx_queue_stop        = enetc_tx_queue_stop,
	.tx_queue_release     = enetc_tx_queue_release,
	.dev_supported_ptypes_get = enetc_supported_ptypes_get,
};

/**
 * Initialisation of the enetc device
 *
 * @param eth_dev
 *   - Pointer to the structure rte_eth_dev
 *
 * @return
 *   - On success, zero.
 *   - On failure, negative value.
 */
static int
enetc_dev_init(struct rte_eth_dev *eth_dev)
{
	int error = 0;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();
	eth_dev->dev_ops = &enetc_ops;
	eth_dev->rx_pkt_burst = &enetc_recv_pkts;
	eth_dev->tx_pkt_burst = &enetc_xmit_pkts;

	/* Retrieving and storing the HW base address of device */
	hw->hw.reg = (void *)pci_dev->mem_resource[0].addr;
	hw->device_id = pci_dev->id.device_id;

	error = enetc_hardware_init(hw);
	if (error != 0) {
		ENETC_PMD_ERR("Hardware initialization failed");
		return -1;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("enetc_eth",
					RTE_ETHER_ADDR_LEN, 0);
	if (!eth_dev->data->mac_addrs) {
		ENETC_PMD_ERR("Failed to allocate %d bytes needed to "
			      "store MAC addresses",
			      RTE_ETHER_ADDR_LEN * 1);
		error = -ENOMEM;
		return -1;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.addr,
			&eth_dev->data->mac_addrs[0]);

	/* Set MTU */
	enetc_port_wr(&hw->hw, ENETC_PM0_MAXFRM,
		      ENETC_SET_MAXFRM(RTE_ETHER_MAX_LEN));
	eth_dev->data->mtu = RTE_ETHER_MAX_LEN - RTE_ETHER_HDR_LEN -
		RTE_ETHER_CRC_LEN;

	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_populate();

	ENETC_PMD_DEBUG("port_id %d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);
	return 0;
}

static int
enetc_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	return enetc_dev_close(eth_dev);
}

static int
enetc_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct enetc_eth_adapter),
					     enetc_dev_init);
}

static int
enetc_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, enetc_dev_uninit);
}

static struct rte_pci_driver rte_enetc_pmd = {
	.id_table = pci_id_enetc_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = enetc_pci_probe,
	.remove = enetc_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_enetc, rte_enetc_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_enetc, pci_id_enetc_map);
RTE_PMD_REGISTER_KMOD_DEP(net_enetc, "* vfio-pci");
RTE_LOG_REGISTER_DEFAULT(enetc_logtype_pmd, NOTICE);
