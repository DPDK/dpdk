/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_ethdev_driver.h>
#include <rte_mbuf.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "txgbe_rxtx.h"

static int
txgbe_is_vf(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	switch (hw->mac.type) {
	case txgbe_mac_raptor_vf:
		return 1;
	default:
		return 0;
	}
}

uint64_t
txgbe_get_rx_queue_offloads(struct rte_eth_dev *dev __rte_unused)
{
	return DEV_RX_OFFLOAD_VLAN_STRIP;
}

uint64_t
txgbe_get_rx_port_offloads(struct rte_eth_dev *dev)
{
	uint64_t offloads;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_eth_dev_sriov *sriov = &RTE_ETH_DEV_SRIOV(dev);

	offloads = DEV_RX_OFFLOAD_IPV4_CKSUM  |
		   DEV_RX_OFFLOAD_UDP_CKSUM   |
		   DEV_RX_OFFLOAD_TCP_CKSUM   |
		   DEV_RX_OFFLOAD_KEEP_CRC    |
		   DEV_RX_OFFLOAD_JUMBO_FRAME |
		   DEV_RX_OFFLOAD_VLAN_FILTER |
		   DEV_RX_OFFLOAD_RSS_HASH |
		   DEV_RX_OFFLOAD_SCATTER;

	if (!txgbe_is_vf(dev))
		offloads |= (DEV_RX_OFFLOAD_VLAN_FILTER |
			     DEV_RX_OFFLOAD_QINQ_STRIP |
			     DEV_RX_OFFLOAD_VLAN_EXTEND);

	/*
	 * RSC is only supported by PF devices in a non-SR-IOV
	 * mode.
	 */
	if (hw->mac.type == txgbe_mac_raptor && !sriov->active)
		offloads |= DEV_RX_OFFLOAD_TCP_LRO;

	if (hw->mac.type == txgbe_mac_raptor)
		offloads |= DEV_RX_OFFLOAD_MACSEC_STRIP;

	offloads |= DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM;

	return offloads;
}

uint64_t
txgbe_get_tx_queue_offloads(struct rte_eth_dev *dev)
{
	RTE_SET_USED(dev);

	return 0;
}

uint64_t
txgbe_get_tx_port_offloads(struct rte_eth_dev *dev)
{
	uint64_t tx_offload_capa;

	tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM  |
		DEV_TX_OFFLOAD_UDP_CKSUM   |
		DEV_TX_OFFLOAD_TCP_CKSUM   |
		DEV_TX_OFFLOAD_SCTP_CKSUM  |
		DEV_TX_OFFLOAD_TCP_TSO     |
		DEV_TX_OFFLOAD_UDP_TSO	   |
		DEV_TX_OFFLOAD_UDP_TNL_TSO	|
		DEV_TX_OFFLOAD_IP_TNL_TSO	|
		DEV_TX_OFFLOAD_VXLAN_TNL_TSO	|
		DEV_TX_OFFLOAD_GRE_TNL_TSO	|
		DEV_TX_OFFLOAD_IPIP_TNL_TSO	|
		DEV_TX_OFFLOAD_GENEVE_TNL_TSO	|
		DEV_TX_OFFLOAD_MULTI_SEGS;

	if (!txgbe_is_vf(dev))
		tx_offload_capa |= DEV_TX_OFFLOAD_QINQ_INSERT;

	tx_offload_capa |= DEV_TX_OFFLOAD_MACSEC_INSERT;

	tx_offload_capa |= DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM;

	return tx_offload_capa;
}

void __rte_cold
txgbe_set_rx_function(struct rte_eth_dev *dev)
{
	RTE_SET_USED(dev);
}

/**
 * txgbe_get_rscctl_maxdesc
 *
 * @pool Memory pool of the Rx queue
 */
static inline uint32_t
txgbe_get_rscctl_maxdesc(struct rte_mempool *pool)
{
	struct rte_pktmbuf_pool_private *mp_priv = rte_mempool_get_priv(pool);

	uint16_t maxdesc =
		RTE_IPV4_MAX_PKT_LEN /
			(mp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM);

	if (maxdesc >= 16)
		return TXGBE_RXCFG_RSCMAX_16;
	else if (maxdesc >= 8)
		return TXGBE_RXCFG_RSCMAX_8;
	else if (maxdesc >= 4)
		return TXGBE_RXCFG_RSCMAX_4;
	else
		return TXGBE_RXCFG_RSCMAX_1;
}

/**
 * txgbe_set_rsc - configure RSC related port HW registers
 *
 * Configures the port's RSC related registers.
 *
 * @dev port handle
 *
 * Returns 0 in case of success or a non-zero error code
 */
static int
txgbe_set_rsc(struct rte_eth_dev *dev)
{
	struct rte_eth_rxmode *rx_conf = &dev->data->dev_conf.rxmode;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_eth_dev_info dev_info = { 0 };
	bool rsc_capable = false;
	uint16_t i;
	uint32_t rdrxctl;
	uint32_t rfctl;

	/* Sanity check */
	dev->dev_ops->dev_infos_get(dev, &dev_info);
	if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_TCP_LRO)
		rsc_capable = true;

	if (!rsc_capable && (rx_conf->offloads & DEV_RX_OFFLOAD_TCP_LRO)) {
		PMD_INIT_LOG(CRIT, "LRO is requested on HW that doesn't "
				   "support it");
		return -EINVAL;
	}

	/* RSC global configuration */

	if ((rx_conf->offloads & DEV_RX_OFFLOAD_KEEP_CRC) &&
	     (rx_conf->offloads & DEV_RX_OFFLOAD_TCP_LRO)) {
		PMD_INIT_LOG(CRIT, "LRO can't be enabled when HW CRC "
				    "is disabled");
		return -EINVAL;
	}

	rfctl = rd32(hw, TXGBE_PSRCTL);
	if (rsc_capable && (rx_conf->offloads & DEV_RX_OFFLOAD_TCP_LRO))
		rfctl &= ~TXGBE_PSRCTL_RSCDIA;
	else
		rfctl |= TXGBE_PSRCTL_RSCDIA;
	wr32(hw, TXGBE_PSRCTL, rfctl);

	/* If LRO hasn't been requested - we are done here. */
	if (!(rx_conf->offloads & DEV_RX_OFFLOAD_TCP_LRO))
		return 0;

	/* Set PSRCTL.RSCACK bit */
	rdrxctl = rd32(hw, TXGBE_PSRCTL);
	rdrxctl |= TXGBE_PSRCTL_RSCACK;
	wr32(hw, TXGBE_PSRCTL, rdrxctl);

	/* Per-queue RSC configuration */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct txgbe_rx_queue *rxq = dev->data->rx_queues[i];
		uint32_t srrctl =
			rd32(hw, TXGBE_RXCFG(rxq->reg_idx));
		uint32_t psrtype =
			rd32(hw, TXGBE_POOLRSS(rxq->reg_idx));
		uint32_t eitr =
			rd32(hw, TXGBE_ITR(rxq->reg_idx));

		/*
		 * txgbe PMD doesn't support header-split at the moment.
		 */
		srrctl &= ~TXGBE_RXCFG_HDRLEN_MASK;
		srrctl |= TXGBE_RXCFG_HDRLEN(128);

		/*
		 * TODO: Consider setting the Receive Descriptor Minimum
		 * Threshold Size for an RSC case. This is not an obviously
		 * beneficiary option but the one worth considering...
		 */

		srrctl |= TXGBE_RXCFG_RSCENA;
		srrctl &= ~TXGBE_RXCFG_RSCMAX_MASK;
		srrctl |= txgbe_get_rscctl_maxdesc(rxq->mb_pool);
		psrtype |= TXGBE_POOLRSS_L4HDR;

		/*
		 * RSC: Set ITR interval corresponding to 2K ints/s.
		 *
		 * Full-sized RSC aggregations for a 10Gb/s link will
		 * arrive at about 20K aggregation/s rate.
		 *
		 * 2K inst/s rate will make only 10% of the
		 * aggregations to be closed due to the interrupt timer
		 * expiration for a streaming at wire-speed case.
		 *
		 * For a sparse streaming case this setting will yield
		 * at most 500us latency for a single RSC aggregation.
		 */
		eitr &= ~TXGBE_ITR_IVAL_MASK;
		eitr |= TXGBE_ITR_IVAL_10G(TXGBE_QUEUE_ITR_INTERVAL_DEFAULT);
		eitr |= TXGBE_ITR_WRDSA;

		wr32(hw, TXGBE_RXCFG(rxq->reg_idx), srrctl);
		wr32(hw, TXGBE_POOLRSS(rxq->reg_idx), psrtype);
		wr32(hw, TXGBE_ITR(rxq->reg_idx), eitr);

		/*
		 * RSC requires the mapping of the queue to the
		 * interrupt vector.
		 */
		txgbe_set_ivar_map(hw, 0, rxq->reg_idx, i);
	}

	dev->data->lro = 1;

	PMD_INIT_LOG(DEBUG, "enabling LRO mode");

	return 0;
}

/*
 * Initializes Receive Unit.
 */
int __rte_cold
txgbe_dev_rx_init(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw;
	struct txgbe_rx_queue *rxq;
	uint64_t bus_addr;
	uint32_t fctrl;
	uint32_t hlreg0;
	uint32_t srrctl;
	uint32_t rdrxctl;
	uint32_t rxcsum;
	uint16_t buf_size;
	uint16_t i;
	struct rte_eth_rxmode *rx_conf = &dev->data->dev_conf.rxmode;
	int rc;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	/*
	 * Make sure receives are disabled while setting
	 * up the RX context (registers, descriptor rings, etc.).
	 */
	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA, 0);
	wr32m(hw, TXGBE_PBRXCTL, TXGBE_PBRXCTL_ENA, 0);

	/* Enable receipt of broadcasted frames */
	fctrl = rd32(hw, TXGBE_PSRCTL);
	fctrl |= TXGBE_PSRCTL_BCA;
	wr32(hw, TXGBE_PSRCTL, fctrl);

	/*
	 * Configure CRC stripping, if any.
	 */
	hlreg0 = rd32(hw, TXGBE_SECRXCTL);
	if (rx_conf->offloads & DEV_RX_OFFLOAD_KEEP_CRC)
		hlreg0 &= ~TXGBE_SECRXCTL_CRCSTRIP;
	else
		hlreg0 |= TXGBE_SECRXCTL_CRCSTRIP;
	wr32(hw, TXGBE_SECRXCTL, hlreg0);

	/*
	 * Configure jumbo frame support, if any.
	 */
	if (rx_conf->offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		wr32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK,
			TXGBE_FRMSZ_MAX(rx_conf->max_rx_pkt_len));
	} else {
		wr32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK,
			TXGBE_FRMSZ_MAX(TXGBE_FRAME_SIZE_DFT));
	}

	/*
	 * If loopback mode is configured, set LPBK bit.
	 */
	hlreg0 = rd32(hw, TXGBE_PSRCTL);
	if (hw->mac.type == txgbe_mac_raptor &&
	    dev->data->dev_conf.lpbk_mode)
		hlreg0 |= TXGBE_PSRCTL_LBENA;
	else
		hlreg0 &= ~TXGBE_PSRCTL_LBENA;

	wr32(hw, TXGBE_PSRCTL, hlreg0);

	/*
	 * Assume no header split and no VLAN strip support
	 * on any Rx queue first .
	 */
	rx_conf->offloads &= ~DEV_RX_OFFLOAD_VLAN_STRIP;

	/* Setup RX queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		/*
		 * Reset crc_len in case it was changed after queue setup by a
		 * call to configure.
		 */
		if (rx_conf->offloads & DEV_RX_OFFLOAD_KEEP_CRC)
			rxq->crc_len = RTE_ETHER_CRC_LEN;
		else
			rxq->crc_len = 0;

		/* Setup the Base and Length of the Rx Descriptor Rings */
		bus_addr = rxq->rx_ring_phys_addr;
		wr32(hw, TXGBE_RXBAL(rxq->reg_idx),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, TXGBE_RXBAH(rxq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		wr32(hw, TXGBE_RXRP(rxq->reg_idx), 0);
		wr32(hw, TXGBE_RXWP(rxq->reg_idx), 0);

		srrctl = TXGBE_RXCFG_RNGLEN(rxq->nb_rx_desc);

		/* Set if packets are dropped when no descriptors available */
		if (rxq->drop_en)
			srrctl |= TXGBE_RXCFG_DROP;

		/*
		 * Configure the RX buffer size in the PKTLEN field of
		 * the RXCFG register of the queue.
		 * The value is in 1 KB resolution. Valid values can be from
		 * 1 KB to 16 KB.
		 */
		buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
			RTE_PKTMBUF_HEADROOM);
		buf_size = ROUND_UP(buf_size, 0x1 << 10);
		srrctl |= TXGBE_RXCFG_PKTLEN(buf_size);

		wr32(hw, TXGBE_RXCFG(rxq->reg_idx), srrctl);

		/* It adds dual VLAN length for supporting dual VLAN */
		if (dev->data->dev_conf.rxmode.max_rx_pkt_len +
					    2 * TXGBE_VLAN_TAG_SIZE > buf_size)
			dev->data->scattered_rx = 1;
		if (rxq->offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			rx_conf->offloads |= DEV_RX_OFFLOAD_VLAN_STRIP;
	}

	if (rx_conf->offloads & DEV_RX_OFFLOAD_SCATTER)
		dev->data->scattered_rx = 1;

	/*
	 * Setup the Checksum Register.
	 * Disable Full-Packet Checksum which is mutually exclusive with RSS.
	 * Enable IP/L4 checksum computation by hardware if requested to do so.
	 */
	rxcsum = rd32(hw, TXGBE_PSRCTL);
	rxcsum |= TXGBE_PSRCTL_PCSD;
	if (rx_conf->offloads & DEV_RX_OFFLOAD_CHECKSUM)
		rxcsum |= TXGBE_PSRCTL_L4CSUM;
	else
		rxcsum &= ~TXGBE_PSRCTL_L4CSUM;

	wr32(hw, TXGBE_PSRCTL, rxcsum);

	if (hw->mac.type == txgbe_mac_raptor) {
		rdrxctl = rd32(hw, TXGBE_SECRXCTL);
		if (rx_conf->offloads & DEV_RX_OFFLOAD_KEEP_CRC)
			rdrxctl &= ~TXGBE_SECRXCTL_CRCSTRIP;
		else
			rdrxctl |= TXGBE_SECRXCTL_CRCSTRIP;
		wr32(hw, TXGBE_SECRXCTL, rdrxctl);
	}

	rc = txgbe_set_rsc(dev);
	if (rc)
		return rc;

	txgbe_set_rx_function(dev);

	return 0;
}

/*
 * Initializes Transmit Unit.
 */
void __rte_cold
txgbe_dev_tx_init(struct rte_eth_dev *dev)
{
	struct txgbe_hw     *hw;
	struct txgbe_tx_queue *txq;
	uint64_t bus_addr;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	/* Setup the Base and Length of the Tx Descriptor Rings */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];

		bus_addr = txq->tx_ring_phys_addr;
		wr32(hw, TXGBE_TXBAL(txq->reg_idx),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, TXGBE_TXBAH(txq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		wr32m(hw, TXGBE_TXCFG(txq->reg_idx), TXGBE_TXCFG_BUFLEN_MASK,
			TXGBE_TXCFG_BUFLEN(txq->nb_tx_desc));
		/* Setup the HW Tx Head and TX Tail descriptor pointers */
		wr32(hw, TXGBE_TXRP(txq->reg_idx), 0);
		wr32(hw, TXGBE_TXWP(txq->reg_idx), 0);
	}
}

