/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include <rte_alarm.h>

#include "base/rnp_mac_regs.h"
#include "base/rnp_dma_regs.h"
#include "base/rnp_mac.h"
#include "base/rnp_fw_cmd.h"
#include "base/rnp_mbx_fw.h"

#include "rnp.h"
#include "rnp_rxtx.h"
#include "rnp_link.h"

static void
rnp_link_flow_setup(struct rnp_eth_port *port)
{
	struct rnp_hw *hw = port->hw;
	u32 ctrl = 0;
	u16 lane = 0;

	lane = port->attr.nr_lane;
	rte_spinlock_lock(&port->rx_mac_lock);
	ctrl = RNP_MAC_REG_RD(hw, lane, RNP_MAC_RX_CFG);
	if (port->attr.link_ready) {
		ctrl &= ~RNP_MAC_LM;
		RNP_RX_ETH_ENABLE(hw, lane);
	} else {
		RNP_RX_ETH_DISABLE(hw, lane);
		ctrl |= RNP_MAC_LM;
	}
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_RX_CFG, ctrl);
	rte_spinlock_unlock(&port->rx_mac_lock);
}

static uint64_t
rnp_parse_speed_code(uint32_t speed_code)
{
	uint32_t speed = 0;

	switch (speed_code) {
	case RNP_LANE_SPEED_10M:
		speed = RTE_ETH_SPEED_NUM_10M;
		break;
	case RNP_LANE_SPEED_100M:
		speed = RTE_ETH_SPEED_NUM_100M;
		break;
	case RNP_LANE_SPEED_1G:
		speed = RTE_ETH_SPEED_NUM_1G;
		break;
	case RNP_LANE_SPEED_10G:
		speed = RTE_ETH_SPEED_NUM_10G;
		break;
	case RNP_LANE_SPEED_25G:
		speed = RTE_ETH_SPEED_NUM_25G;
		break;
	case RNP_LANE_SPEED_40G:
		speed = RTE_ETH_SPEED_NUM_40G;
		break;
	default:
		speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	}

	return speed;
}

static bool
rnp_update_speed_changed(struct rnp_eth_port *port)
{
	struct rnp_hw *hw = port->hw;
	uint32_t speed_code = 0;
	bool speed_changed = 0;
	bool duplex = false;
	uint32_t magic = 0;
	uint32_t linkstate;
	uint64_t speed = 0;
	uint16_t lane = 0;

	lane = port->attr.nr_lane;
	linkstate = RNP_E_REG_RD(hw, RNP_DEVICE_LINK_EX);
	magic = linkstate & 0xF0000000;
	/* check if speed is changed. even if link is not changed */
	if (RNP_SPEED_META_VALID(magic) &&
			(linkstate & RNP_LINK_STATE(lane))) {
		speed_code = rnpce_link_speed_code(linkstate, lane);
		speed = rnp_parse_speed_code(speed_code);
		if (speed != port->attr.speed) {
			duplex = RNP_LINK_DUPLEX_STATE(linkstate, lane);
			port->attr.phy_meta.link_duplex = duplex;
			port->attr.speed = speed;
			speed_changed = 1;
		}
	}

	return speed_changed;
}

static bool
rnp_update_link_changed(struct rnp_eth_port *port,
			struct rnp_link_stat_req *link)
{
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t link_up_bit = 0;
	bool link_changed = 0;
	uint32_t sync_bit = 0;
	bool duplex = 0;

	link_up_bit = link->lane_status & RTE_BIT32(lane);
	sync_bit = RNP_E_REG_RD(hw, RNP_FW_LINK_SYNC);
	if (link_up_bit) {
		/* port link down to up */
		if (!port->attr.link_ready)
			link_changed = true;
		port->attr.link_ready = true;
		if (link->port_st_magic == RNP_SPEED_VALID_MAGIC) {
			port->attr.speed = link->states[lane].speed;
			duplex = link->states[lane].duplex;
			port->attr.duplex = duplex;
			RNP_PMD_INFO("phy_id %d speed %d duplex "
					"%d issgmii %d PortID %d",
					link->states[lane].phy_addr,
					link->states[lane].speed,
					link->states[lane].duplex,
					link->states[lane].is_sgmii,
					port->attr.port_id);
		}
	} else {
		/* port link to down */
		if (port->attr.link_ready)
			link_changed = true;
		port->attr.link_ready = false;
	}
	if (!link_changed && sync_bit != link_up_bit)
		link_changed = 1;

	return link_changed;
}

static void rnp_link_stat_sync_mark(struct rnp_hw *hw, int lane, int up)
{
	uint32_t sync;

	rte_spinlock_lock(&hw->link_sync);
	sync = RNP_E_REG_RD(hw, RNP_FW_LINK_SYNC);
	sync &= ~RNP_LINK_MAGIC_MASK;
	sync |= RNP_LINK_MAGIC_CODE;
	if (up)
		sync |= RTE_BIT32(lane);
	else
		sync &= ~RTE_BIT32(lane);
	RNP_E_REG_WR(hw, RNP_FW_LINK_SYNC, sync);
	rte_spinlock_unlock(&hw->link_sync);
}

static void rnp_link_report(struct rnp_eth_port *port, bool link_en)
{
	struct rte_eth_dev_data *data = port->eth_dev->data;
	struct rnp_hw *hw = port->hw;
	struct rnp_rx_queue *rxq;
	struct rnp_tx_queue *txq;
	struct rte_eth_link link;
	uint16_t idx;

	if (data == NULL)
		return;
	for (idx = 0; idx < data->nb_rx_queues; idx++) {
		rxq = data->rx_queues[idx];
		if (!rxq)
			continue;
		rxq->rx_link = link_en;
	}
	for (idx = 0; idx < data->nb_tx_queues; idx++) {
		txq = data->tx_queues[idx];
		if (!txq)
			continue;
		txq->tx_link = link_en;
	}
	/* set default link info */
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	link.link_status = RTE_ETH_LINK_DOWN;
	link.link_autoneg = RTE_ETH_LINK_FIXED;
	if (link_en) {
		link.link_autoneg = port->attr.phy_meta.link_autoneg;
		link.link_duplex = port->attr.phy_meta.link_duplex;
		link.link_speed = port->attr.speed;
		link.link_status = link_en;
	}
	RNP_PMD_LOG(INFO, "PF[%d]link changed: changed_lane:0x%x, "
			"status:0x%x",
			hw->mbx.pf_num,
			port->attr.nr_lane,
			link_en);
	/* report link info to upper firmwork */
	rte_eth_linkstatus_set(port->eth_dev, &link);
	/* notice event process link status change */
	rte_eth_dev_callback_process(port->eth_dev, RTE_ETH_EVENT_INTR_LSC, NULL);
	/* notice firmware LSC event sw received */
	rnp_link_stat_sync_mark(hw, port->attr.nr_lane, link_en);
}

static void rnp_dev_alarm_link_handler(void *param)
{
	struct rnp_eth_port *port = param;
	uint32_t status;

	if (port == NULL || port->eth_dev == NULL)
		return;
	status = port->attr.link_ready;
	rnp_link_report(port, status);
}

void rnp_link_event(struct rnp_eth_adapter *adapter,
		    struct rnp_mbx_fw_cmd_req *req)
{
	struct rnp_link_stat_req *link = (struct rnp_link_stat_req *)req->data;
	struct rnp_hw *hw = &adapter->hw;
	struct rnp_eth_port *port = NULL;
	bool speed_changed;
	bool link_changed;
	uint32_t lane;
	uint8_t i = 0;

	/* get real-time link && speed info */
	for (i = 0; i < hw->max_port_num; i++) {
		port = adapter->ports[i];
		if (port == NULL)
			continue;
		speed_changed = false;
		link_changed = false;
		lane = port->attr.nr_lane;
		if (RNP_LINK_NOCHANGED(lane, link->changed_lanes)) {
			speed_changed = rnp_update_speed_changed(port);
			if (!speed_changed)
				continue;
		}
		link_changed = rnp_update_link_changed(port, link);
		if (link_changed || speed_changed) {
			rnp_link_flow_setup(port);
			rte_eal_alarm_set(RNP_ALARM_INTERVAL,
					rnp_dev_alarm_link_handler,
					(void *)port);
		}
	}
}

int rnp_dev_link_update(struct rte_eth_dev *eth_dev,
			int wait_to_complete)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	struct rnp_phy_meta *phy_meta = &port->attr.phy_meta;
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	struct rte_eth_link link;
	uint32_t status;

	PMD_INIT_FUNC_TRACE();
	memset(&link, 0, sizeof(link));
	if (wait_to_complete && rte_eal_process_type() == RTE_PROC_PRIMARY)
		rnp_mbx_fw_get_lane_stat(port);
	status = port->attr.link_ready;
	link.link_duplex = phy_meta->link_duplex;
	link.link_status = status ? RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;
	if (link.link_status)
		link.link_speed = port->attr.speed;
	link.link_autoneg = phy_meta->link_autoneg ?
		RTE_ETH_LINK_AUTONEG : RTE_ETH_LINK_FIXED;
	rnp_link_stat_sync_mark(hw, lane, link.link_status);
	rte_eth_linkstatus_set(eth_dev, &link);

	return 0;
}

static void rnp_dev_link_task(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	bool speed_changed = false;
	bool link_changed = false;
	bool duplex_attr = false;
	uint32_t speed_code = 0;
	uint32_t link_state;
	bool duplex = false;
	uint32_t speed = 0;

	link_state = RNP_E_REG_RD(hw, RNP_DEVICE_LINK_EX);
	if (link_state & RNP_LINK_DUPLEX_ATTR_EN)
		duplex_attr = true;
	else
		link_state = RNP_E_REG_RD(hw, RNP_DEVICE_LINK);
	if (link_state & RNP_LINK_STATE(lane)) {
		/* Port link change to up */
		speed_code = rnpce_link_speed_code(link_state, lane);
		speed = rnp_parse_speed_code(speed_code);
		if (duplex_attr) {
			duplex = RNP_LINK_DUPLEX_STATE(link_state, lane);
			port->attr.phy_meta.link_duplex = duplex;
		}
		port->attr.speed = speed;
		port->attr.pre_link = port->attr.link_ready;
		port->attr.link_ready = true;
	} else {
		/* Port link to down */
		port->attr.speed = RTE_ETH_SPEED_NUM_UNKNOWN;
		port->attr.pre_link = port->attr.link_ready;
		port->attr.link_ready = false;
	}
	if (port->attr.pre_link != port->attr.link_ready)
		link_changed = true;
	if (!link_changed)
		speed_changed = rnp_update_speed_changed(port);
	if (link_changed || speed_changed) {
		if (!duplex_attr)
			rnp_mbx_fw_get_lane_stat(port);
		rnp_link_flow_setup(port);
		rnp_link_report(port, port->attr.link_ready);
	}
	rte_eal_alarm_set(RNP_ALARM_INTERVAL,
			rnp_dev_link_task,
			(void *)dev);
}

void
rnp_run_link_poll_task(struct rnp_eth_port *port)
{
	rte_eal_alarm_set(RNP_ALARM_INTERVAL, rnp_dev_link_task,
			(void *)port->eth_dev);
}

void
rnp_cancel_link_poll_task(struct rnp_eth_port *port)
{
	rte_eal_alarm_cancel(rnp_dev_link_task, port->eth_dev);
}
