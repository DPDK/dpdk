/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2019 Advanced Micro Devices, Inc. All rights reserved.
 */
#ifndef RTE_ETH_AXGBE_REGS_H_
#define RTE_ETH_AXGBE_REGS_H_

#include "axgbe_common.h"

static const uint32_t dma_reg_tbl[] = {
	DMA_MR,		/* DMA Mode */
	DMA_SBMR,	/* DMA Sys Bus Mode */
	DMA_ISR,	/* DMA Interrupt Status */
	DMA_AXIARCR,	/* DMA AXI Tx AR ACE Ctrl */
	DMA_AXIAWCR,	/* DMA AXI Rx AW ACE Ctrl */
	DMA_AXIAWRCR,	/* DMA AXI TxRx AWR ACE Ctrl */
	DMA_DSR0,	/* DMA Debug Status0 */
	DMA_DSR1,	/* DMA Debug Status1 */
	EDMA_TX_CONTROL,/* DMA Tx EDMA Ctrl */
	EDMA_RX_CONTROL,/* DMA Rx EDMA Ctrl */
};

static const uint32_t dma_txch_reg_tbl[] = {
	DMA_CH_CR,      /* DMA Channel Ctrl */
	DMA_CH_TCR,     /* DMA Tx Ctrl */
	DMA_CH_TDLR_HI, /* DMA TxDescList HAddr */
	DMA_CH_TDLR_LO, /* DMA TxDescList LAddr */
	DMA_CH_TDTR_LO, /* DMA TxDescTail LAddr */
	DMA_CH_TDRLR,   /* DMA TxDescRing Length */
	DMA_CH_IER,     /* DMA Interrupt Enable */
	DMA_CH_CATDR_LO,/* DMA CurrApp TxDesc LAddr */
	DMA_CH_CATBR_HI,/* DMA CurrApp TxBuf HAddr */
	DMA_CH_CATBR_LO,/* DMA CurrApp TxBuf LAddr */
	DMA_CH_SR,      /* DMA Channel Status */
};

static const uint32_t dma_rxch_reg_tbl[] = {
	DMA_CH_RCR,	/* DMA Rx Ctrl */
	DMA_CH_RDLR_HI,	/* DMA RxDescList HAddr */
	DMA_CH_RDLR_LO,	/* DMA RxDescList LAddr */
	DMA_CH_RDTR_LO,	/* DMA RxDescTail LAddr */
	DMA_CH_RDRLR,	/* DMA RxDescRing Length */
	DMA_CH_RIWT,	/* DMA Rx Interrupt WatchDog Timer */
	DMA_CH_CARDR_LO,/* DMA CurrApp RxDesc LAddr */
	DMA_CH_CARBR_HI,/* DMA CurrApp RxBuf HAddr */
	DMA_CH_CARBR_LO,/* DMA CurrApp RxBuf LAddr */

};

static const uint32_t mtl_reg_tbl[] = {
	MTL_OMR,	/* MTL Operation Mode */
	MTL_FDCR,	/* MTL FIFO Debug Ctrl */
	MTL_FDSR,	/* MTL FIFO Debug Status */
	MTL_FDDR,	/* MTL FIFO Debug Data */
	MTL_ISR,	/* MTL Interrupt Status */
	MTL_RQDCM0R,	/* MTL RxQ DMA Map0 */
	MTL_TCPM0R,	/* MTL TC Prty Map0 */
	MTL_TCPM1R,	/* MTL TC Prty Map1 */
};

static const uint32_t mtl_txq_reg_tbl[] = {
	MTL_Q_TQOMR,	/* MTL TxQ Operation Mode */
	MTL_Q_TQUR,	/* MTL TxQ Underflow */
	MTL_Q_TQDR,	/* MTL TxQ Debug */
	MTL_Q_IER,	/* MTL Q Interrupt Enable */
	MTL_Q_ISR,	/* MTL Q Interrupt Status */
};

static const uint32_t mtl_rxq_reg_tbl[] = {
	MTL_Q_RQOMR,	/* MTL RxQ Operation Mode */
	MTL_Q_RQMPOCR,	/* MTL RxQ Missed Pkt OverFlow Cnt */
	MTL_Q_RQDR,	/* MTL RxQ Debug */
	MTL_Q_RQFCR,	/* MTL RxQ Flow Control */
};

static const uint32_t mac_reg_tbl[] = {
	MAC_TCR,	/* MAC Tx Config */
	MAC_RCR,	/* MAC Rx Config */
	MAC_PFR,	/* MAC Packet Filter */
	MAC_WTR,	/* MAC WatchDog Timeout */
	MAC_HTR0,	/* MAC Hash Table0 */
	MAC_VLANTR,	/* MAC VLAN Tag Ctrl */
	MAC_VLANHTR,	/* MAC VLAN Hash Table */
	MAC_VLANIR,	/* MAC VLAN Incl */
	MAC_IVLANIR,	/* MAC Inner VLAN Incl */
	MAC_RETMR,	/* MAC Rx Eth Type Match */
	MAC_Q0TFCR,	/* MAC Q0 Tx Flow Ctrl */
	MAC_RFCR,	/* MAC Rx Flow Ctrl */
	MAC_RQC0R,	/* MAC RxQ Ctrl0 */
	MAC_RQC1R,	/* MAC RxQ Ctrl1 */
	MAC_RQC2R,	/* MAC RxQ Ctrl2 */
	MAC_RQC3R,	/* MAC RxQ Ctrl3 */
	MAC_ISR,	/* MAC Interrupt Status */
	MAC_IER,	/* MAC Interrupt Enable */
	MAC_RTSR,	/* MAC Rx Tx Status */
	MAC_PMTCSR,	/* MAC PMT Ctrl Status */
	MAC_RWKPFR,	/* MAC RWK Packet Filter */
	MAC_LPICSR,	/* MAC LPI Ctrl Status */
	MAC_LPITCR,	/* MAC LPI Timers Ctrl */
	MAC_VR,		/* MAC Version */
	MAC_DR,		/* MAC Debug Status */
	MAC_HWF0R,	/* MAC HW Feature0 */
	MAC_HWF1R,	/* MAC HW Feature1 */
	MAC_HWF2R,	/* MAC HW Feature2 */
	MAC_MDIOSCAR,	/* MDIO Single Cmd Addr */
	MAC_MDIOSCCDR,	/* MDIO Single Cmd/Data */
	MAC_MDIOISR,	/* MDIO Interrupt Status */
	MAC_MDIOIER,	/* MDIO Interrupt Enable */
	MAC_MDIOCL22R,	/* MDIO Clause22 Port */
	MAC_GPIOCR,	/* MAC GPIO Ctrl */
	MAC_GPIOSR,	/* MAC GPIO Status */
	MAC_RSSCR,	/* MAC RSS Ctrl */
	MAC_RSSAR,	/* MAC RSS Addr */
};

/* MAC Address Register Table */
static const uint32_t mac_addr_reg_tbl[] = {
	MAC_MACAHR(0),	MAC_MACALR(0),	MAC_MACAHR(1),	MAC_MACALR(1),
	MAC_MACAHR(2),	MAC_MACALR(2),	MAC_MACAHR(3),	MAC_MACALR(3),
	MAC_MACAHR(4),	MAC_MACALR(4),	MAC_MACAHR(5),	MAC_MACALR(5),
	MAC_MACAHR(6),	MAC_MACALR(6),	MAC_MACAHR(7),	MAC_MACALR(7),
	MAC_MACAHR(8),	MAC_MACALR(8),	MAC_MACAHR(9),	MAC_MACALR(9),
	MAC_MACAHR(10),	MAC_MACALR(10),	MAC_MACAHR(11),	MAC_MACALR(11),
	MAC_MACAHR(12),	MAC_MACALR(12),	MAC_MACAHR(13),	MAC_MACALR(13),
	MAC_MACAHR(14),	MAC_MACALR(14),	MAC_MACAHR(15),	MAC_MACALR(15),
	MAC_MACAHR(16),	MAC_MACALR(16),	MAC_MACAHR(17),	MAC_MACALR(17),
	MAC_MACAHR(18),	MAC_MACALR(18),	MAC_MACAHR(19),	MAC_MACALR(19),
	MAC_MACAHR(20),	MAC_MACALR(20),	MAC_MACAHR(21),	MAC_MACALR(21),
	MAC_MACAHR(22),	MAC_MACALR(22),	MAC_MACAHR(23),	MAC_MACALR(23),
	MAC_MACAHR(24),	MAC_MACALR(24),	MAC_MACAHR(25),	MAC_MACALR(25),
	MAC_MACAHR(26),	MAC_MACALR(26),	MAC_MACAHR(27),	MAC_MACALR(27),
	MAC_MACAHR(28),	MAC_MACALR(28),	MAC_MACAHR(29),	MAC_MACALR(29),
	MAC_MACAHR(30),	MAC_MACALR(30),	MAC_MACAHR(31),	MAC_MACALR(31),

};

static const uint32_t mac_ieee1558_reg_tbl[] = {
	MAC_RSSDR,	/* MAC RSS Data */
	MAC_TSCR,	/* MAC TimeStamp Ctrl */
	MAC_SSIR,	/* MAC Sub Second Incr */
	MAC_STSR,	/* MAC Sys Time Secs */
	MAC_STNR,	/* MAC Sys Time NSecs */
	MAC_STSUR,	/* MAC Sys Time Secs Update */
	MAC_STNUR,	/* MAC Sys Time NSecs Update */
	MAC_TSAR,	/* MAC TimeStamp Addend */
	MAC_TSSR,	/* MAC TimeStamp Status */
	MAC_TXSNR,	/* MAC TxTS Status NSecs */
	MAC_TXSSR,	/* MAC TxTS Status Secs */
};

static inline int
axgbe_regs_get_count(struct axgbe_port *pdata)
{
	int count = 0;
	unsigned int i = 0;

	count = ARRAY_SIZE(dma_reg_tbl);
	for (i = 0; i < pdata->tx_ring_count; i++)
		count += ARRAY_SIZE(dma_txch_reg_tbl);
	for (i = 0; i < pdata->rx_ring_count; i++)
		count += ARRAY_SIZE(dma_rxch_reg_tbl);
	count += ARRAY_SIZE(mtl_reg_tbl);
	for (i = 0; i < pdata->tx_q_count; i++)
		count += ARRAY_SIZE(mtl_txq_reg_tbl);
	for (i = 0; i < pdata->rx_q_count; i++)
		count += ARRAY_SIZE(mtl_rxq_reg_tbl);
	count += ARRAY_SIZE(mac_reg_tbl);
	count += ARRAY_SIZE(mac_addr_reg_tbl);
	count += ARRAY_SIZE(mac_ieee1558_reg_tbl);

	return count;
};

static inline int
axgbe_regs_dump(struct axgbe_port *pdata, uint32_t *data)
{
	unsigned int i = 0, j = 0;
	unsigned int base_reg, reg;

	for (i = 0; i < ARRAY_SIZE(dma_reg_tbl); i++)
		*data++ = AXGMAC_IOREAD(pdata, dma_reg_tbl[i]);

	for (j = 0; j < pdata->tx_ring_count; j++) {
		base_reg = DMA_CH_BASE + (j * DMA_CH_INC);
		for (i = 0; i < ARRAY_SIZE(dma_txch_reg_tbl); i++) {
			reg = base_reg + dma_txch_reg_tbl[i];
			*data++ = AXGMAC_IOREAD(pdata, reg);
		}
	}

	for (j = 0; j < pdata->rx_ring_count; j++) {
		base_reg = DMA_CH_BASE + (j * DMA_CH_INC);
		for (i = 0; i < ARRAY_SIZE(dma_rxch_reg_tbl); i++) {
			reg = base_reg + dma_rxch_reg_tbl[i];
			*data++ = AXGMAC_IOREAD(pdata, reg);
		}
	}

	for (i = 0; i < ARRAY_SIZE(mtl_reg_tbl); i++)
		*data++ = AXGMAC_IOREAD(pdata, mtl_reg_tbl[i]);

	for (j = 0; j < pdata->tx_q_count; j++) {
		base_reg = MTL_Q_BASE + (j * MTL_Q_INC);
		for (i = 0; i < ARRAY_SIZE(mtl_txq_reg_tbl); i++) {
			reg = base_reg + mtl_txq_reg_tbl[i];
			*data++ = AXGMAC_IOREAD(pdata, reg);
		}
	}

	for (j = 0; j < pdata->rx_q_count; j++) {
		base_reg = MTL_Q_BASE + (j * MTL_Q_INC);
		for (i = 0; i < ARRAY_SIZE(mtl_rxq_reg_tbl); i++) {
			reg = base_reg + mtl_rxq_reg_tbl[i];
			*data++ = AXGMAC_IOREAD(pdata, reg);
		}
	}

	for (i = 0; i < ARRAY_SIZE(mac_reg_tbl); i++)
		*data++ = AXGMAC_IOREAD(pdata, mac_reg_tbl[i]);

	for (i = 0; i < ARRAY_SIZE(mac_addr_reg_tbl); i++)
		*data++ = AXGMAC_IOREAD(pdata, mac_addr_reg_tbl[i]);

	for (i = 0; i < ARRAY_SIZE(mac_ieee1558_reg_tbl); i++)
		*data++ = AXGMAC_IOREAD(pdata, mac_ieee1558_reg_tbl[i]);

	return 0;
};

#endif /* RTE_ETH_AXGBE_REGS_H_ */
