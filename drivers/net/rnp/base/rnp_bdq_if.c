/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include "rnp_osdep.h"

#include "../rnp.h"
#include "rnp_dma_regs.h"
#include "rnp_eth_regs.h"
#include "rnp_bdq_if.h"
#include "rnp_common.h"
#include "../rnp_rxtx.h"

static void
rnp_read_mac_veb(struct rnp_hw *hw,
		  u16 nr_lane,
		  u16 vf_id,
		  struct rnp_veb_cfg *cfg)
{
	cfg->mac_lo = RNP_E_REG_RD(hw, RNP_VEB_MAC_LO(nr_lane, vf_id));
	cfg->mac_hi = RNP_E_REG_RD(hw, RNP_VEB_MAC_HI(nr_lane, vf_id));
	cfg->ring = RNP_E_REG_RD(hw, RNP_VEB_VF_RING(nr_lane, vf_id));
}

static void
rnp_update_mac_veb(struct rnp_hw *hw,
		   u16 nr_lane,
		   u16 vf_id,
		   struct rnp_veb_cfg *cfg)
{
	u32 reg = cfg->ring;
	u16 idx = 0;

	idx = nr_lane;
	wmb();
	RNP_E_REG_WR(hw, RNP_VEB_MAC_LO(idx, vf_id), cfg->mac_lo);
	RNP_E_REG_WR(hw, RNP_VEB_MAC_HI(idx, vf_id), cfg->mac_hi);
	reg |= ((RNP_VEB_SWITCH_VF_EN | vf_id) << 8);
	RNP_E_REG_WR(hw, RNP_VEB_VF_RING(idx, vf_id), reg);
}

void
rnp_rxq_flow_disable(struct rnp_hw *hw,
		     u16 hw_idx)
{
	u32 fc_ctrl;

	spin_lock(&hw->rxq_reset_lock);
	fc_ctrl = RNP_E_REG_RD(hw, RNP_RING_FC_EN(hw_idx));
	wmb();
	RNP_E_REG_WR(hw, RNP_RING_FC_THRESH(hw_idx), 0);
	fc_ctrl |= 1 << (hw_idx % 32);
	wmb();
	RNP_E_REG_WR(hw, RNP_RING_FC_EN(hw_idx), fc_ctrl);
}

void
rnp_rxq_flow_enable(struct rnp_hw *hw,
		    u16 hw_idx)
{
	u32 fc_ctrl;


	fc_ctrl = RNP_E_REG_RD(hw, RNP_RING_FC_EN(hw_idx));
	fc_ctrl &= ~(1 << (hw_idx % 32));
	wmb();
	RNP_E_REG_WR(hw, RNP_RING_FC_EN(hw_idx), fc_ctrl);

	spin_unlock(&hw->rxq_reset_lock);
}

#define RNP_RXQ_RESET_PKT_LEN	(64)

static void
rnp_reset_xmit(struct rnp_tx_queue *txq, u64 pkt_addr)
{
	volatile struct rnp_tx_desc *txbd;
	struct rnp_txsw_entry *tx_entry;
	u16 timeout = 0;
	u16 tx_id;

	tx_id = txq->tx_tail;
	txbd = &txq->tx_bdr[tx_id];
	tx_entry = &txq->sw_ring[tx_id];
	memset(tx_entry, 0, sizeof(*tx_entry));

	txbd->d.addr = pkt_addr;
	txbd->d.blen = RNP_RXQ_RESET_PKT_LEN;
	wmb();
	txbd->d.cmd = cpu_to_le16(RNP_CMD_EOP | RNP_CMD_RS);
	tx_id = (tx_id + 1) & txq->attr.nb_desc_mask;
	wmb();
	RNP_REG_WR(txq->tx_tailreg, 0, tx_id);
	do {
		if (txbd->d.cmd & RNP_CMD_DD)
			break;
		if (timeout == 1000)
			RNP_PMD_ERR("rx queue %u reset send pkt is hang",
					txq->attr.index);
		timeout++;
		udelay(10);
	} while (1);
}

void
rnp_reset_hw_rxq_op(struct rnp_hw *hw,
		    struct rnp_rx_queue *rxq,
		    struct rnp_tx_queue *txq,
		    struct rnp_rxq_reset_res *res)
{
	u8 reset_pcap[RNP_RXQ_RESET_PKT_LEN] = {
		0x01, 0x02, 0x27, 0xe2, 0x9f, 0xa6, 0x08, 0x00,
		0x27, 0xfc, 0x6a, 0xc9, 0x08, 0x00, 0x45, 0x00,
		0x01, 0xc4, 0xb5, 0xd0, 0x00, 0x7a, 0x40, 0x01,
		0xbc, 0xea, 0x02, 0x01, 0x01, 0x02, 0x02, 0x01,
		0x01, 0x01, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd,
		0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5,
		0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd,
		0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5};
	struct rnp_veb_cfg veb_bak_cfg[RNP_MAX_PORT_OF_PF];
	struct rnp_veb_cfg reset_cfg = {0};
	volatile struct rnp_rx_desc *rxbd;
	u16 index = rxq->attr.index;
	u16 vf_num = hw->mbx.vf_num;
	u8 *macaddr = res->eth_hdr;
	u16 timeout = 0;
	u16 vf_id = 0;
	u16 head = 0;
	u16 idx = 0;

	memcpy(macaddr, reset_pcap, RNP_RXQ_RESET_PKT_LEN);
	macaddr[5] = index;
	reset_cfg.mac_hi = RNP_GET_MAC_HI(macaddr);
	reset_cfg.mac_lo = RNP_GET_MAC_LO(macaddr);
	reset_cfg.ring = index;
	vf_id = (vf_num != UINT16_MAX) ? vf_num : index / 2;
	if (hw->mbx.vf_num == UINT16_MAX) {
		for (idx = 0; idx < RNP_MAX_PORT_OF_PF; idx++) {
			rnp_read_mac_veb(hw, idx, vf_id, &veb_bak_cfg[idx]);
			rnp_update_mac_veb(hw, idx, vf_id, &reset_cfg);
		}
	} else {
		idx = rxq->attr.lane_id;
		rnp_read_mac_veb(hw, idx, vf_id, &veb_bak_cfg[idx]);
		rnp_update_mac_veb(hw, idx, vf_id, &reset_cfg);
	}
	wmb();
	timeout = 0;
	do {
		if (!RNP_E_REG_RD(hw, RNP_RXQ_READY(index)))
			break;
		udelay(5);
		timeout++;
	} while (timeout < 100);
	timeout = 0;
	do {
		if (RNP_E_REG_RD(hw, RNP_TXQ_READY(index)))
			break;
		udelay(10);
		timeout++;
	} while (timeout < 100);
	rxq->rx_tail = RNP_E_REG_RD(hw, RNP_RXQ_HEAD(index));
	rxbd = &rxq->rx_bdr[rxq->rx_tail];
	rxbd->d.pkt_addr = res->rx_pkt_addr;
	if (rxq->rx_tail != rxq->attr.nb_desc_mask)
		RNP_E_REG_WR(hw, RNP_RXQ_LEN(index), rxq->rx_tail + 1);
	wmb();
	RNP_REG_WR(rxq->rx_tailreg, 0, 0);
	RNP_E_REG_WR(hw, RNP_RXQ_START(index), TRUE);
	rnp_reset_xmit(txq, res->tx_pkt_addr);
	timeout = 0;
	do {
		if (rxbd->wb.qword1.cmd & cpu_to_le32(RNP_CMD_DD))
			break;
		if (timeout == 1000)
			RNP_PMD_LOG(ERR, "rx_queue[%d] reset queue hang",
					index);
		udelay(10);
		timeout++;
	} while (1);
	timeout = 0;
	do {
		head = RNP_E_REG_RD(hw, RNP_RXQ_HEAD(index));
		if (head == 0)
			break;
		timeout++;
		if (timeout == 1000)
			RNP_PMD_LOG(ERR, "rx_queue[%d] reset head to 0 failed",
					index);
		udelay(10);
	} while (1);
	RNP_E_REG_WR(hw, RNP_RXQ_START(index), FALSE);
	rxbd->d.pkt_addr = 0;
	rxbd->d.cmd = 0;
	if (hw->mbx.vf_num == UINT16_MAX) {
		for (idx = 0; idx < 4; idx++)
			rnp_update_mac_veb(hw, idx, vf_id, &veb_bak_cfg[idx]);
	} else {
		idx = rxq->attr.lane_id;
		rnp_update_mac_veb(hw, idx, vf_id, &veb_bak_cfg[idx]);
	}
	rxq->rx_tail = head;
}

void rnp_setup_rxbdr(struct rnp_hw *hw,
		     struct rnp_rx_queue *rxq)
{
	u16 max_desc = rxq->attr.nb_desc;
	u16 idx = rxq->attr.index;
	phys_addr_t bd_address;
	u32 dmah, dmal;
	u32 desc_ctrl;

	RNP_E_REG_WR(hw, RNP_RXQ_START(idx), FALSE);
	bd_address = (phys_addr_t)rxq->ring_phys_addr;
	dmah = upper_32_bits((uint64_t)bd_address);
	dmal = lower_32_bits((uint64_t)bd_address);
	desc_ctrl = rxq->pburst << RNQ_DESC_FETCH_BURST_S | rxq->pthresh;
	if (hw->mbx.sriov_st)
		dmah |= (hw->mbx.sriov_st << 24);
	/* we must set sriov_state to hi dma_address high 8bit for vf isolation
	 * |---8bit-----|----------24bit--------|
	 * |sriov_state-|-------high dma address|
	 * |---------------8bit-----------------|
	 * |7bit | 6bit |5-0bit-----------------|
	 * |vf_en|pf_num|-------vf_num----------|
	 */
	RNP_E_REG_WR(hw, RNP_RXQ_BASE_ADDR_LO(idx), dmal);
	RNP_E_REG_WR(hw, RNP_RXQ_BASE_ADDR_HI(idx), dmah);
	RNP_E_REG_WR(hw, RNP_RXQ_LEN(idx), max_desc);
	rxq->rx_tailreg = (u32 *)((u8 *)hw->e_ctrl + RNP_RXQ_TAIL(idx));
	rxq->rx_headreg = (u32 *)((u8 *)hw->e_ctrl + RNP_RXQ_HEAD(idx));
	rxq->rx_tail = RNP_E_REG_RD(hw, RNP_RXQ_HEAD(idx));
	RNP_E_REG_WR(hw, RNP_RXQ_DESC_FETCH_CTRL(idx), desc_ctrl);
	RNP_E_REG_WR(hw, RNP_RXQ_DROP_TIMEOUT_TH(idx),
			rxq->nodesc_tm_thresh);
}

int rnp_get_dma_ring_index(struct rnp_eth_port *port, u16 queue_idx)
{
	struct rnp_hw *hw = port->hw;
	u16 lane = port->attr.nr_lane;
	u16 hwrid = 0;

	switch (hw->nic_mode) {
	case RNP_DUAL_10G:
		hwrid = 2 * (queue_idx + lane) - queue_idx % 2;
		break;
	case RNP_QUAD_10G:
		hwrid = 4 * (queue_idx) + lane;
		break;
	default:
		hwrid = queue_idx;
	}

	return hwrid;
}

void rnp_setup_txbdr(struct rnp_hw *hw, struct rnp_tx_queue *txq)
{
	u16 max_desc = txq->attr.nb_desc;
	u16 idx = txq->attr.index;
	phys_addr_t bd_address;
	u32 desc_ctrl = 0;
	u32 dmah, dmal;

	bd_address = (phys_addr_t)txq->ring_phys_addr;
	desc_ctrl = txq->pburst << RNQ_DESC_FETCH_BURST_S | txq->pthresh;
	dmah = upper_32_bits((u64)bd_address);
	dmal = lower_32_bits((u64)bd_address);
	if (hw->mbx.sriov_st)
		dmah |= (hw->mbx.sriov_st << 24);
	/* We must set sriov_state to hi dma_address high 8bit for vf isolation
	 * |---8bit-----|----------24bit--------|
	 * |sriov_state-|-------high dma address|
	 * |---------------8bit-----------------|
	 * |7bit | 6bit |5-0bit-----------------|
	 * |vf_en|pf_num|-------vf_num----------|
	 */
	RNP_E_REG_WR(hw, RNP_TXQ_BASE_ADDR_LO(idx), dmal);
	RNP_E_REG_WR(hw, RNP_TXQ_BASE_ADDR_HI(idx), dmah);
	RNP_E_REG_WR(hw, RNP_TXQ_LEN(idx), max_desc);
	RNP_E_REG_WR(hw, RNP_TXQ_DESC_FETCH_CTRL(idx), desc_ctrl);
	RNP_E_REG_WR(hw, RNP_RXTX_IRQ_MASK(idx), RNP_RXTX_IRQ_MASK_ALL);
	txq->tx_headreg = (void *)((u8 *)hw->e_ctrl + RNP_TXQ_HEAD(idx));
	txq->tx_tailreg = (void *)((u8 *)hw->e_ctrl + RNP_TXQ_TAIL(idx));

	txq->tx_tail = RNP_E_REG_RD(hw, RNP_TXQ_HEAD(idx));
	RNP_E_REG_WR(hw, RNP_TXQ_TAIL(idx), 0);
}

static void
rnp_txq_reset_pre(struct rnp_hw *hw)
{
	u16 i = 0;

	spin_lock(&hw->txq_reset_lock);
	for (i = 0; i < RNP_MAX_RX_QUEUE_NUM; i++) {
		wmb();
		RNP_E_REG_WR(hw, RNP_RXQ_START(i), 0);
	}
}

static void
rnp_txq_reset_fin(struct rnp_hw *hw)
{
	u16 i = 0;

	for (i = 0; i < RNP_MAX_RX_QUEUE_NUM; i++) {
		wmb();
		RNP_E_REG_WR(hw, RNP_RXQ_START(i), 1);
	}
	spin_unlock(&hw->txq_reset_lock);
}

static void
rnp_xmit_nop_frame_ring(struct rnp_hw *hw,
			struct rnp_tx_queue *txq,
			u16 head)
{
	volatile struct rnp_tx_desc *tx_desc;
	u16 check_head = 0;
	u16 timeout = 0;
	u16 index = 0;
	u16 tx_id;

	tx_id = head;
	index = txq->attr.index;
	tx_desc = &txq->tx_bdr[tx_id];

	/* set length to 0 */
	tx_desc->d.blen = 0;
	tx_desc->d.addr = 0;
	wmb();
	tx_desc->d.cmd = cpu_to_le16(RNP_CMD_EOP);
	wmb();
	/* update tail */
	RNP_REG_WR(txq->tx_tailreg, 0, 0);
	do {
		check_head = RNP_E_REG_RD(hw, RNP_TXQ_HEAD(index));
		if (check_head == 0)
			break;
		if (timeout == 1000)
			RNP_PMD_ERR("tx_queue[%d] reset may be hang "
					"check_head %d base head %d",
					index, check_head, head);
		timeout++;
		udelay(10);
	} while (1);
	/* restore the origin right state */
	wmb();
	RNP_E_REG_WR(hw, RNP_TXQ_LEN(index), txq->attr.nb_desc);
}

void rnp_reset_hw_txq_op(struct rnp_hw *hw,
			 struct rnp_tx_queue *txq)
{
	u16 timeout = 0;
	u16 index = 0;
	u16 head;
	u16 tail;

	timeout = 0;
	/* Disable Tx Queue */
	index = txq->attr.index;
	rnp_txq_reset_pre(hw);
	rmb();
	tail = RNP_E_REG_RD(hw, RNP_TXQ_TAIL(index));
	txq->tx_tail = tail;
	do {
		/* wait for hw head is stopped */
		head = RNP_E_REG_RD(hw, RNP_TXQ_HEAD(index));
		if (head == txq->tx_tail)
			break;
		if (timeout > 1000) {
			RNP_PMD_ERR("txq[%u] 1000*10us can't "
				    "wait for hw head == tail", index);
			break;
		}
		timeout++;
		udelay(10);
	} while (1);
	rmb();
	head = RNP_E_REG_RD(hw, RNP_TXQ_HEAD(index));
	/* head is zero no need to reset */
	if (head == 0)
		goto tx_reset_fin;
	wmb();
	if (head != txq->attr.nb_desc_mask)
		RNP_E_REG_WR(hw, RNP_TXQ_LEN(index), head + 1);
	wmb();
	/* reset hw head */
	rnp_xmit_nop_frame_ring(hw, txq, head);
	rmb();
	txq->tx_tail = RNP_E_REG_RD(hw, RNP_TXQ_HEAD(index));
tx_reset_fin:
	rnp_txq_reset_fin(hw);
}
