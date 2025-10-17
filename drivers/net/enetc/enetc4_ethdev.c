/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 NXP
 */

#include <stdbool.h>
#include <rte_random.h>
#include <dpaax_iova_table.h>

#include "base/enetc4_hw.h"
#include "enetc_logs.h"
#include "enetc.h"

/* Supported Rx offloads */
static uint64_t dev_rx_offloads_sup =
	RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
	RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
	RTE_ETH_RX_OFFLOAD_TCP_CKSUM;

/* Supported Tx offloads */
static uint64_t dev_tx_offloads_sup =
	RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
	RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
	RTE_ETH_TX_OFFLOAD_TCP_CKSUM;

static int
enetc4_dev_start(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t val;

	PMD_INIT_FUNC_TRACE();

	val = enetc4_port_rd(enetc_hw, ENETC4_PM_CMD_CFG(0));
	enetc4_port_wr(enetc_hw, ENETC4_PM_CMD_CFG(0),
		       val | PM_CMD_CFG_TX_EN | PM_CMD_CFG_RX_EN);

	val = enetc4_port_rd(enetc_hw, ENETC4_PM_CMD_CFG(1));
	enetc4_port_wr(enetc_hw, ENETC4_PM_CMD_CFG(1),
		       val | PM_CMD_CFG_TX_EN | PM_CMD_CFG_RX_EN);

	/* Enable port */
	val = enetc4_port_rd(enetc_hw, ENETC4_PMR);
	enetc4_port_wr(enetc_hw, ENETC4_PMR, val | ENETC4_PMR_EN);

	/* Enable port transmit/receive */
	enetc4_port_wr(enetc_hw, ENETC4_POR, 0);

	return 0;
}

static int
enetc4_dev_stop(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t val;

	PMD_INIT_FUNC_TRACE();

	/* Disable port */
	val = enetc4_port_rd(enetc_hw, ENETC4_PMR);
	enetc4_port_wr(enetc_hw, ENETC4_PMR, val & (~ENETC4_PMR_EN));

	val = enetc4_port_rd(enetc_hw, ENETC4_PM_CMD_CFG(0));
	enetc4_port_wr(enetc_hw, ENETC4_PM_CMD_CFG(0),
		      val & (~(PM_CMD_CFG_TX_EN | PM_CMD_CFG_RX_EN)));

	val = enetc4_port_rd(enetc_hw, ENETC4_PM_CMD_CFG(1));
	enetc4_port_wr(enetc_hw, ENETC4_PM_CMD_CFG(1),
		      val & (~(PM_CMD_CFG_TX_EN | PM_CMD_CFG_RX_EN)));

	return 0;
}

static int
enetc4_mac_init(struct enetc_eth_hw *hw, struct rte_eth_dev *eth_dev)
{
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t high_mac = 0;
	uint16_t low_mac = 0;
	char eth_name[ENETC_ETH_NAMESIZE];

	PMD_INIT_FUNC_TRACE();

	/* Enabling Station Interface */
	enetc4_wr(enetc_hw, ENETC_SIMR, ENETC_SIMR_EN);

	high_mac = (uint32_t)enetc4_port_rd(enetc_hw, ENETC4_PSIPMAR0(0));
	low_mac = (uint16_t)enetc4_port_rd(enetc_hw, ENETC4_PSIPMAR1(0));

	if ((high_mac | low_mac) == 0) {
		ENETC_PMD_NOTICE("MAC is not available for this SI, "
				"set random MAC");
		rte_eth_random_addr(hw->mac.addr);
		high_mac = *(uint32_t *)hw->mac.addr;
		enetc4_port_wr(enetc_hw, ENETC4_PMAR0, high_mac);
		low_mac = *(uint16_t *)(hw->mac.addr + 4);
		enetc4_port_wr(enetc_hw, ENETC4_PMAR1, low_mac);
		enetc_print_ethaddr("New address: ",
			      (const struct rte_ether_addr *)hw->mac.addr);
	}

	/* Allocate memory for storing MAC addresses */
	snprintf(eth_name, sizeof(eth_name), "enetc4_eth_%d", eth_dev->data->port_id);
	eth_dev->data->mac_addrs = rte_zmalloc(eth_name,
					RTE_ETHER_ADDR_LEN, 0);
	if (!eth_dev->data->mac_addrs) {
		ENETC_PMD_ERR("Failed to allocate %d bytes needed to "
			      "store MAC addresses",
			      RTE_ETHER_ADDR_LEN * 1);
		return -ENOMEM;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.addr,
			&eth_dev->data->mac_addrs[0]);

	return 0;
}

int
enetc4_dev_infos_get(struct rte_eth_dev *dev,
		    struct rte_eth_dev_info *dev_info)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);

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
	dev_info->max_rx_queues = hw->max_rx_queues;
	dev_info->max_tx_queues = hw->max_tx_queues;
	dev_info->max_rx_pktlen = ENETC4_MAC_MAXFRM_SIZE;
	dev_info->rx_offload_capa = dev_rx_offloads_sup;
	dev_info->tx_offload_capa = dev_tx_offloads_sup;
	dev_info->flow_type_rss_offloads = ENETC_RSS_OFFLOAD_ALL;

	return 0;
}

static int
enetc4_alloc_txbdr(struct enetc_bdr *txr, uint16_t nb_desc)
{
	int size;

	size = nb_desc * sizeof(struct enetc_swbd);
	txr->q_swbd = rte_malloc(NULL, size, ENETC_BD_RING_ALIGN);
	if (txr->q_swbd == NULL)
		return -ENOMEM;

	size = nb_desc * sizeof(struct enetc_bdr);
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
enetc4_free_bdr(struct enetc_bdr *rxr)
{
	rte_free(rxr->bd_base);
	rte_free(rxr->q_swbd);
	rxr->q_swbd = NULL;
	rxr->bd_base = NULL;
}

static void
enetc4_setup_txbdr(struct enetc_hw *hw, struct enetc_bdr *tx_ring)
{
	int idx = tx_ring->index;
	phys_addr_t bd_address;

	bd_address = (phys_addr_t)
		     rte_mem_virt2iova((const void *)tx_ring->bd_base);
	enetc4_txbdr_wr(hw, idx, ENETC_TBBAR0,
		       lower_32_bits((uint64_t)bd_address));
	enetc4_txbdr_wr(hw, idx, ENETC_TBBAR1,
		       upper_32_bits((uint64_t)bd_address));
	enetc4_txbdr_wr(hw, idx, ENETC_TBLENR,
		       ENETC_RTBLENR_LEN(tx_ring->bd_count));

	enetc4_txbdr_wr(hw, idx, ENETC_TBCIR, 0);
	enetc4_txbdr_wr(hw, idx, ENETC_TBCISR, 0);
	tx_ring->tcir = (void *)((size_t)hw->reg +
			ENETC_BDR(TX, idx, ENETC_TBCIR));
	tx_ring->tcisr = (void *)((size_t)hw->reg +
			 ENETC_BDR(TX, idx, ENETC_TBCISR));
}

int
enetc4_tx_queue_setup(struct rte_eth_dev *dev,
		     uint16_t queue_idx,
		     uint16_t nb_desc,
		     unsigned int socket_id __rte_unused,
		     const struct rte_eth_txconf *tx_conf)
{
	int err;
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
		return err;
	}

	tx_ring->index = queue_idx;
	err = enetc4_alloc_txbdr(tx_ring, nb_desc);
	if (err)
		goto fail;

	tx_ring->ndev = dev;
	enetc4_setup_txbdr(&priv->hw.hw, tx_ring);
	data->tx_queues[queue_idx] = tx_ring;
	if (!tx_conf->tx_deferred_start) {
		/* enable ring */
		enetc4_txbdr_wr(&priv->hw.hw, tx_ring->index,
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

void
enetc4_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	void *txq = dev->data->tx_queues[qid];
	struct enetc_hw *hw;
	struct enetc_swbd *tx_swbd;
	int i;
	uint32_t val;
	struct enetc_bdr *tx_ring;
	struct enetc_eth_hw *eth_hw;

	PMD_INIT_FUNC_TRACE();
	if (txq == NULL)
		return;

	tx_ring = (struct enetc_bdr *)txq;
	eth_hw =
		ENETC_DEV_PRIVATE_TO_HW(tx_ring->ndev->data->dev_private);

	/* Disable the ring */
	hw = &eth_hw->hw;
	val = enetc4_txbdr_rd(hw, tx_ring->index, ENETC_TBMR);
	val &= (~ENETC_TBMR_EN);
	enetc4_txbdr_wr(hw, tx_ring->index, ENETC_TBMR, val);

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

	enetc4_free_bdr(tx_ring);
	rte_free(tx_ring);
}

static int
enetc4_alloc_rxbdr(struct enetc_bdr *rxr, uint16_t nb_desc)
{
	int size;

	size = nb_desc * sizeof(struct enetc_swbd);
	rxr->q_swbd = rte_malloc(NULL, size, ENETC_BD_RING_ALIGN);
	if (rxr->q_swbd == NULL)
		return -ENOMEM;

	size = nb_desc * sizeof(struct enetc_bdr);
	rxr->bd_base = rte_malloc(NULL, size, ENETC_BD_RING_ALIGN);
	if (rxr->bd_base == NULL) {
		rte_free(rxr->q_swbd);
		rxr->q_swbd = NULL;
		return -ENOMEM;
	}
	rxr->bd_count = nb_desc;
	rxr->next_to_clean = 0;
	rxr->next_to_use = 0;
	rxr->next_to_alloc = 0;

	return 0;
}

static void
enetc4_setup_rxbdr(struct enetc_hw *hw, struct enetc_bdr *rx_ring,
		  struct rte_mempool *mb_pool)
{
	int idx = rx_ring->index;
	uint16_t buf_size;
	phys_addr_t bd_address;

	bd_address = (phys_addr_t)
		     rte_mem_virt2iova((const void *)rx_ring->bd_base);

	enetc4_rxbdr_wr(hw, idx, ENETC_RBBAR0,
		       lower_32_bits((uint64_t)bd_address));
	enetc4_rxbdr_wr(hw, idx, ENETC_RBBAR1,
		       upper_32_bits((uint64_t)bd_address));
	enetc4_rxbdr_wr(hw, idx, ENETC_RBLENR,
		       ENETC_RTBLENR_LEN(rx_ring->bd_count));

	rx_ring->mb_pool = mb_pool;
	rx_ring->rcir = (void *)((size_t)hw->reg +
			ENETC_BDR(RX, idx, ENETC_RBCIR));
	enetc_refill_rx_ring(rx_ring, (enetc_bd_unused(rx_ring)));
	buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rx_ring->mb_pool) -
		   RTE_PKTMBUF_HEADROOM);
	enetc4_rxbdr_wr(hw, idx, ENETC_RBBSR, buf_size);
	enetc4_rxbdr_wr(hw, idx, ENETC_RBPIR, 0);
}

int
enetc4_rx_queue_setup(struct rte_eth_dev *dev,
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

	rx_ring->index = rx_queue_id;
	err = enetc4_alloc_rxbdr(rx_ring, nb_rx_desc);
	if (err)
		goto fail;

	rx_ring->ndev = dev;
	enetc4_setup_rxbdr(&adapter->hw.hw, rx_ring, mb_pool);
	data->rx_queues[rx_queue_id] = rx_ring;

	if (!rx_conf->rx_deferred_start) {
		/* enable ring */
		enetc4_rxbdr_wr(&adapter->hw.hw, rx_ring->index, ENETC_RBMR,
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

void
enetc4_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	void *rxq = dev->data->rx_queues[qid];
	struct enetc_swbd *q_swbd;
	struct enetc_hw *hw;
	uint32_t val;
	int i;
	struct enetc_bdr *rx_ring;
	struct enetc_eth_hw *eth_hw;

	PMD_INIT_FUNC_TRACE();
	if (rxq == NULL)
		return;

	rx_ring = (struct enetc_bdr *)rxq;
	eth_hw =
		ENETC_DEV_PRIVATE_TO_HW(rx_ring->ndev->data->dev_private);

	/* Disable the ring */
	hw = &eth_hw->hw;
	val = enetc4_rxbdr_rd(hw, rx_ring->index, ENETC_RBMR);
	val &= (~ENETC_RBMR_EN);
	enetc4_rxbdr_wr(hw, rx_ring->index, ENETC_RBMR, val);

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

	enetc4_free_bdr(rx_ring);
	rte_free(rx_ring);
}

static
int enetc4_stats_get(struct rte_eth_dev *dev,
		    struct rte_eth_stats *stats)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;

	/*
	 * Total received packets, bad + good, if we want to get counters
	 * of only good received packets then use ENETC4_PM_RFRM,
	 * ENETC4_PM_TFRM registers.
	 */
	stats->ipackets = enetc4_port_rd(enetc_hw, ENETC4_PM_RPKT(0));
	stats->opackets = enetc4_port_rd(enetc_hw, ENETC4_PM_TPKT(0));
	stats->ibytes =  enetc4_port_rd(enetc_hw, ENETC4_PM_REOCT(0));
	stats->obytes = enetc4_port_rd(enetc_hw, ENETC4_PM_TEOCT(0));
	/*
	 * Dropped + Truncated packets, use ENETC4_PM_RDRNTP(0) for without
	 * truncated packets
	 */
	stats->imissed = enetc4_port_rd(enetc_hw, ENETC4_PM_RDRP(0));
	stats->ierrors = enetc4_port_rd(enetc_hw, ENETC4_PM_RERR(0));
	stats->oerrors = enetc4_port_rd(enetc_hw, ENETC4_PM_TERR(0));

	return 0;
}

static int
enetc4_stats_reset(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;

	enetc4_port_wr(enetc_hw, ENETC4_PM0_STAT_CONFIG, ENETC4_CLEAR_STATS);

	return 0;
}

static void
enetc4_rss_configure(struct enetc_hw *hw, int enable)
{
	uint32_t reg;

	reg = enetc4_rd(hw, ENETC_SIMR);
	reg &= ~ENETC_SIMR_RSSE;
	reg |= (enable) ? ENETC_SIMR_RSSE : 0;
	enetc4_wr(hw, ENETC_SIMR, reg);
}

int
enetc4_dev_close(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint16_t i;
	int ret;

	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (hw->device_id == ENETC4_DEV_ID_VF)
		ret = enetc4_vf_dev_stop(dev);
	else
		ret = enetc4_dev_stop(dev);

	if (dev->data->nb_rx_queues > 1) {
		/* Disable RSS */
		enetc4_rss_configure(enetc_hw, false);
		/* Free CBDR */
		enetc_free_cbdr(&hw->cbdr);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		enetc4_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		enetc4_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;

	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_depopulate();

	return ret;
}

int
enetc4_dev_configure(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_conf *eth_conf = &dev->data->dev_conf;
	uint64_t rx_offloads = eth_conf->rxmode.offloads;
	uint64_t tx_offloads = eth_conf->txmode.offloads;
	uint32_t checksum = L3_CKSUM | L4_CKSUM;
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t max_len;
	uint32_t val, num_rss;
	uint32_t ret = 0, i;
	uint32_t *rss_table;

	PMD_INIT_FUNC_TRACE();

	max_len = dev->data->dev_conf.rxmode.mtu + RTE_ETHER_HDR_LEN +
		  RTE_ETHER_CRC_LEN;
	enetc4_port_wr(enetc_hw, ENETC4_PM_MAXFRM(0), ENETC_SET_MAXFRM(max_len));

	val = ENETC4_MAC_MAXFRM_SIZE | SDU_TYPE_MPDU;
	enetc4_port_wr(enetc_hw, ENETC4_PTCTMSDUR(0), val | SDU_TYPE_MPDU);

	/* Rx offloads which are enabled by default */
	if (dev_rx_offloads_sup & ~rx_offloads) {
		ENETC_PMD_INFO("Some of rx offloads enabled by default"
				" - requested 0x%" PRIx64 " fixed are 0x%" PRIx64,
				rx_offloads, dev_rx_offloads_sup);
	}

	/* Tx offloads which are enabled by default */
	if (dev_tx_offloads_sup & ~tx_offloads) {
		ENETC_PMD_INFO("Some of tx offloads enabled by default"
			       " - requested 0x%" PRIx64 " fixed are 0x%" PRIx64,
			       tx_offloads, dev_tx_offloads_sup);
	}

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM)
		checksum &= ~L3_CKSUM;

	if (rx_offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_TCP_CKSUM))
		checksum &= ~L4_CKSUM;

	enetc4_port_wr(enetc_hw, ENETC4_PARCSCR, checksum);

	/* Disable and reset RX and TX rings */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		enetc4_rxbdr_wr(enetc_hw, i, ENETC_RBMR, ENETC_BMR_RESET);

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		enetc4_rxbdr_wr(enetc_hw, i, ENETC_TBMR, ENETC_BMR_RESET);

	if (dev->data->nb_rx_queues <= 1)
		return 0;

	/* Setup RSS */
	/* Setup control BDR */
	ret = enetc4_setup_cbdr(dev, enetc_hw, ENETC_CBDR_SIZE, &hw->cbdr);
	if (ret) {
		/* Disable RSS */
		enetc4_rss_configure(enetc_hw, false);
		return ret;
	}

	/* Reset CIR again after enable CBDR*/
	rte_delay_us(ENETC_CBDR_DELAY);
	ENETC_PMD_DEBUG("CIR %x after CBDR enable", rte_read32(hw->cbdr.regs.cir));
	rte_write32(0, hw->cbdr.regs.cir);
	ENETC_PMD_DEBUG("CIR %x after reset", rte_read32(hw->cbdr.regs.cir));

	val = enetc_rd(enetc_hw, ENETC_SIPCAPR0);
	if (val & ENETC_SIPCAPR0_RSS) {
		num_rss = enetc_rd(enetc_hw, ENETC_SIRSSCAPR);
		hw->num_rss = ENETC_SIRSSCAPR_GET_NUM_RSS(num_rss);
		ENETC_PMD_DEBUG("num_rss = %d", hw->num_rss);

		/* Add number of BDR groups */
		enetc4_wr(enetc_hw, ENETC_SIRBGCR, dev->data->nb_rx_queues);


		/* Configuring indirecton table with default values
		 * Hash algorithm and RSS secret key to be filled by PF
		 */
		rss_table = rte_malloc(NULL, hw->num_rss * sizeof(*rss_table), ENETC_CBDR_ALIGN);
		if (!rss_table) {
			enetc4_rss_configure(enetc_hw, false);
			enetc_free_cbdr(&hw->cbdr);
			return -ENOMEM;
		}

		ENETC_PMD_DEBUG("Enabling RSS for port %s with queues = %d", dev->device->name,
							dev->data->nb_rx_queues);
		for (i = 0; i < hw->num_rss; i++)
			rss_table[i] = i % dev->data->nb_rx_queues;

		ret = enetc_ntmp_rsst_query_or_update_entry(&hw->cbdr,
					rss_table, hw->num_rss, false);
		if (ret) {
			ENETC_PMD_WARN("RSS indirection table update fails,"
					"Scaling behaviour is undefined");
			enetc4_rss_configure(enetc_hw, false);
			enetc_free_cbdr(&hw->cbdr);
		}
		rte_free(rss_table);

		/* Enable RSS */
		enetc4_rss_configure(enetc_hw, true);
	}

	return 0;
}

int
enetc4_rx_queue_start(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct enetc_eth_adapter *priv =
			ENETC_DEV_PRIVATE(dev->data->dev_private);
	struct enetc_bdr *rx_ring;
	uint32_t rx_data;

	PMD_INIT_FUNC_TRACE();
	rx_ring = dev->data->rx_queues[qidx];
	if (dev->data->rx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STOPPED) {
		rx_data = enetc4_rxbdr_rd(&priv->hw.hw, rx_ring->index,
					 ENETC_RBMR);
		rx_data = rx_data | ENETC_RBMR_EN;
		enetc4_rxbdr_wr(&priv->hw.hw, rx_ring->index, ENETC_RBMR,
			       rx_data);
		dev->data->rx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	return 0;
}

int
enetc4_rx_queue_stop(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct enetc_eth_adapter *priv =
			ENETC_DEV_PRIVATE(dev->data->dev_private);
	struct enetc_bdr *rx_ring;
	uint32_t rx_data;

	PMD_INIT_FUNC_TRACE();
	rx_ring = dev->data->rx_queues[qidx];
	if (dev->data->rx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STARTED) {
		rx_data = enetc4_rxbdr_rd(&priv->hw.hw, rx_ring->index,
					 ENETC_RBMR);
		rx_data = rx_data & (~ENETC_RBMR_EN);
		enetc4_rxbdr_wr(&priv->hw.hw, rx_ring->index, ENETC_RBMR,
			       rx_data);
		dev->data->rx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;
	}

	return 0;
}

int
enetc4_tx_queue_start(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct enetc_eth_adapter *priv =
			ENETC_DEV_PRIVATE(dev->data->dev_private);
	struct enetc_bdr *tx_ring;
	uint32_t tx_data;

	PMD_INIT_FUNC_TRACE();
	tx_ring = dev->data->tx_queues[qidx];
	if (dev->data->tx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STOPPED) {
		tx_data = enetc4_txbdr_rd(&priv->hw.hw, tx_ring->index,
					 ENETC_TBMR);
		tx_data = tx_data | ENETC_TBMR_EN;
		enetc4_txbdr_wr(&priv->hw.hw, tx_ring->index, ENETC_TBMR,
			       tx_data);
		dev->data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	return 0;
}

int
enetc4_tx_queue_stop(struct rte_eth_dev *dev, uint16_t qidx)
{
	struct enetc_eth_adapter *priv =
			ENETC_DEV_PRIVATE(dev->data->dev_private);
	struct enetc_bdr *tx_ring;
	uint32_t tx_data;

	PMD_INIT_FUNC_TRACE();
	tx_ring = dev->data->tx_queues[qidx];
	if (dev->data->tx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STARTED) {
		tx_data = enetc4_txbdr_rd(&priv->hw.hw, tx_ring->index,
					 ENETC_TBMR);
		tx_data = tx_data & (~ENETC_TBMR_EN);
		enetc4_txbdr_wr(&priv->hw.hw, tx_ring->index, ENETC_TBMR,
			       tx_data);
		dev->data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;
	}

	return 0;
}

const uint32_t *
enetc4_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused,
			    size_t *no_of_elements)
{
	PMD_INIT_FUNC_TRACE();
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_TUNNEL_ESP,
		RTE_PTYPE_UNKNOWN
	};

	*no_of_elements = RTE_DIM(ptypes);
	return ptypes;
}

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_enetc4_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_NXP, ENETC4_DEV_ID) },
	{ .vendor_id = 0, /* sentinel */ },
};

/* Features supported by this driver */
static const struct eth_dev_ops enetc4_ops = {
	.dev_configure        = enetc4_dev_configure,
	.dev_start            = enetc4_dev_start,
	.dev_stop             = enetc4_dev_stop,
	.dev_close            = enetc4_dev_close,
	.dev_infos_get        = enetc4_dev_infos_get,
	.stats_get            = enetc4_stats_get,
	.stats_reset          = enetc4_stats_reset,
	.rx_queue_setup       = enetc4_rx_queue_setup,
	.rx_queue_start       = enetc4_rx_queue_start,
	.rx_queue_stop        = enetc4_rx_queue_stop,
	.rx_queue_release     = enetc4_rx_queue_release,
	.tx_queue_setup       = enetc4_tx_queue_setup,
	.tx_queue_start       = enetc4_tx_queue_start,
	.tx_queue_stop        = enetc4_tx_queue_stop,
	.tx_queue_release     = enetc4_tx_queue_release,
	.dev_supported_ptypes_get = enetc4_supported_ptypes_get,
};

/*
 * Storing the HW base addresses
 *
 * @param eth_dev
 *   - Pointer to the structure rte_eth_dev
 */
void
enetc4_dev_hw_init(struct rte_eth_dev *eth_dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	eth_dev->rx_pkt_burst = &enetc_recv_pkts_nc;
	eth_dev->tx_pkt_burst = &enetc_xmit_pkts_nc;

	/* Retrieving and storing the HW base address of device */
	hw->hw.reg = (void *)pci_dev->mem_resource[0].addr;
	hw->device_id = pci_dev->id.device_id;

	/* Calculating and storing the base HW addresses */
	hw->hw.port = (void *)((size_t)hw->hw.reg + ENETC_PORT_BASE);
	hw->hw.global = (void *)((size_t)hw->hw.reg + ENETC_GLOBAL_BASE);
}

/**
 * Initialisation of the enetc4 device
 *
 * @param eth_dev
 *   - Pointer to the structure rte_eth_dev
 *
 * @return
 *   - On success, zero.
 *   - On failure, negative value.
 */

static int
enetc4_dev_init(struct rte_eth_dev *eth_dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int error = 0;
	uint32_t si_cap;
	struct enetc_hw *enetc_hw = &hw->hw;

	PMD_INIT_FUNC_TRACE();
	eth_dev->dev_ops = &enetc4_ops;
	enetc4_dev_hw_init(eth_dev);

	si_cap = enetc_rd(enetc_hw, ENETC_SICAPR0);
	hw->max_tx_queues = si_cap & ENETC_SICAPR0_BDR_MASK;
	hw->max_rx_queues = (si_cap >> 16) & ENETC_SICAPR0_BDR_MASK;

	ENETC_PMD_DEBUG("Max RX queues = %d Max TX queues = %d",
			hw->max_rx_queues, hw->max_tx_queues);
	error = enetc4_mac_init(hw, eth_dev);
	if (error != 0) {
		ENETC_PMD_ERR("MAC initialization failed");
		return -1;
	}

	/* Set MTU */
	enetc_port_wr(&hw->hw, ENETC4_PM_MAXFRM(0),
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
enetc4_dev_uninit(struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	return enetc4_dev_close(eth_dev);
}

static int
enetc4_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			   struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct enetc_eth_adapter),
					     enetc4_dev_init);
}

int
enetc4_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, enetc4_dev_uninit);
}

static struct rte_pci_driver rte_enetc4_pmd = {
	.id_table = pci_id_enetc4_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = enetc4_pci_probe,
	.remove = enetc4_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_enetc4, rte_enetc4_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_enetc4, pci_id_enetc4_map);
RTE_PMD_REGISTER_KMOD_DEP(net_enetc4, "* vfio-pci");
RTE_LOG_REGISTER_DEFAULT(enetc4_logtype_pmd, NOTICE);
