/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include <ethdev_pci.h>
#include <ethdev_driver.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "rnp.h"
#include "rnp_logs.h"
#include "base/rnp_mbx.h"
#include "base/rnp_fw_cmd.h"
#include "base/rnp_mbx_fw.h"
#include "base/rnp_mac.h"
#include "base/rnp_eth_regs.h"
#include "base/rnp_common.h"
#include "base/rnp_dma_regs.h"
#include "base/rnp_mac_regs.h"
#include "rnp_rxtx.h"
#include "rnp_rss.h"
#include "rnp_link.h"

static int rnp_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static struct rte_eth_dev *
rnp_alloc_eth_port(struct rte_pci_device *pci, char *name)
{
	struct rte_eth_dev *eth_dev = NULL;
	struct rnp_eth_port *port = NULL;

	eth_dev = rte_eth_dev_allocate(name);
	if (!eth_dev) {
		RNP_PMD_ERR("Could not allocate eth_dev for %s", name);
		return NULL;
	}
	port = rte_zmalloc_socket(name,
			sizeof(*port),
			RTE_CACHE_LINE_SIZE,
			pci->device.numa_node);
	if (!port) {
		RNP_PMD_ERR("Could not allocate rnp_eth_port for %s", name);
		goto fail_calloc;
	}
	rte_eth_copy_pci_info(eth_dev, pci);
	eth_dev->data->dev_private = port;
	eth_dev->device = &pci->device;

	return eth_dev;
fail_calloc:
	rte_free(port);
	rte_eth_dev_release_port(eth_dev);

	return NULL;
}

static int
rnp_mbx_fw_reply_handler(struct rnp_eth_adapter *adapter,
			 void *cmd)
{
	struct rnp_mbx_fw_cmd_reply *reply = cmd;
	struct rnp_mbx_req_cookie *cookie;

	RTE_SET_USED(adapter);
	/* dbg_here; */
	cookie = reply->cookie;
	if (!cookie || cookie->magic != RNP_COOKIE_MAGIC) {
		RNP_PMD_ERR("invalid cookie:%p opcode:0x%x v0:0x%x",
				cookie, reply->opcode, *((int *)reply));
		return -EIO;
	}
	if (cookie->priv_len > 0)
		memcpy(cookie->priv, reply->data, RNP_FW_REP_DATA_NUM);

	cookie->done = 1;
	if (reply->flags & RNP_FLAGS_ERR)
		cookie->errcode = reply->error_code;
	else
		cookie->errcode = 0;

	return 0;
}

static int rnp_mbx_fw_req_handler(struct rnp_eth_adapter *adapter,
				  void *cmd)
{
	struct rnp_mbx_fw_cmd_req *req = cmd;

	switch (req->opcode) {
	case RNP_LINK_STATUS_EVENT:
		rnp_link_event(adapter, req);
		break;
	default:
		break;
	}

	return 0;
}

static int rnp_process_fw_msg(struct rnp_eth_adapter *adapter)
{
	const struct rnp_mbx_ops *ops = RNP_DEV_PP_TO_MBX_OPS(adapter->eth_dev);
	struct rnp_hw *hw = &adapter->hw;
	void *msg_cmd = NULL;
	uint16_t msg_flag;

	msg_cmd = (void *)hw->msgbuf;
	memset(msg_cmd, 0, sizeof(hw->msgbuf));
	/* check fw req */
	if (!ops->check_for_msg(hw, RNP_MBX_FW)) {
		rnp_rcv_msg_from_fw(adapter, msg_cmd);
		msg_flag = hw->msgbuf[2] | hw->msgbuf[3];
		if (msg_flag & RNP_FLAGS_DD)
			rnp_mbx_fw_reply_handler(adapter, msg_cmd);
		else
			rnp_mbx_fw_req_handler(adapter, msg_cmd);
	}

	return 0;
}

static void rnp_dev_interrupt_handler(void *param)
{
	struct rnp_eth_adapter *adapter = param;
	uint16_t exp = RNP_PF_OP_DONE;

	if (!rte_atomic_compare_exchange_strong_explicit(&adapter->pf_op, &exp,
			RNP_PF_OP_PROCESS, rte_memory_order_acquire,
			rte_memory_order_acquire))
		return;
	rnp_process_fw_msg(adapter);
	rte_atomic_store_explicit(&adapter->pf_op, RNP_PF_OP_DONE,
			rte_memory_order_release);
}

static void rnp_mac_rx_enable(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t mac_cfg;

	rte_spinlock_lock(&port->rx_mac_lock);
	mac_cfg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_RX_CFG);
	mac_cfg |= RNP_MAC_RE;

	if (port->jumbo_en) {
		mac_cfg |= RNP_MAC_JE;
		mac_cfg |= RNP_MAC_GPSLCE | RNP_MAC_WD;
	} else {
		mac_cfg &= ~RNP_MAC_JE;
		mac_cfg &= ~RNP_MAC_WD;
	}
	mac_cfg &= ~RNP_MAC_GPSL_MASK;
	mac_cfg |= (RNP_MAC_MAX_GPSL << RNP_MAC_CPSL_SHIFT);
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_RX_CFG, mac_cfg);
	rte_spinlock_unlock(&port->rx_mac_lock);
}

static void rnp_mac_rx_disable(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t mac_cfg;

	/* to protect conflict hw resource */
	rte_spinlock_lock(&port->rx_mac_lock);
	mac_cfg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_RX_CFG);
	mac_cfg &= ~RNP_MAC_RE;

	RNP_MAC_REG_WR(hw, lane, RNP_MAC_RX_CFG, mac_cfg);
	rte_spinlock_unlock(&port->rx_mac_lock);
}

static void rnp_mac_tx_enable(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t mac_cfg;

	mac_cfg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_TX_CFG);
	mac_cfg |= RNP_MAC_TE;
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_TX_CFG, mac_cfg);
}

static void rnp_mac_tx_disable(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t ctrl;

	/* must wait for tx side has send finish
	 * before fisable tx side
	 */
	ctrl = RNP_MAC_REG_RD(hw, lane, RNP_MAC_TX_CFG);
	ctrl &= ~RNP_MAC_TE;
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_TX_CFG, ctrl);
}

static void rnp_mac_init(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t mac_cfg;

	rnp_mac_tx_enable(dev);
	rnp_mac_rx_enable(dev);

	mac_cfg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_LPI_CTRL);
	mac_cfg |= RNP_MAC_PLSDIS | RNP_MAC_PLS;
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_LPI_CTRL, mac_cfg);
}

static int
rnp_rx_scattered_setup(struct rte_eth_dev *dev)
{
	uint16_t max_pkt_size =
		dev->data->dev_conf.rxmode.mtu + RNP_ETH_OVERHEAD;
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_hw *hw = port->hw;
	struct rnp_rx_queue *rxq;
	uint16_t dma_buf_size;
	uint16_t queue_id;
	uint32_t dma_ctrl;

	if (dev->data->rx_queues == NULL)
		return -ENOMEM;
	for (queue_id = 0; queue_id < dev->data->nb_rx_queues; queue_id++) {
		rxq = dev->data->rx_queues[queue_id];
		if (!rxq)
			continue;
		if (hw->min_dma_size == 0)
			hw->min_dma_size = rxq->rx_buf_len;
		else
			hw->min_dma_size = RTE_MIN(hw->min_dma_size,
					rxq->rx_buf_len);
	}
	if (hw->min_dma_size < RNP_MIN_DMA_BUF_SIZE) {
		RNP_PMD_ERR("port[%d] scatter dma len is not support %d",
				dev->data->port_id, hw->min_dma_size);
		return -ENOTSUP;
	}
	dma_buf_size = hw->min_dma_size;
	if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_SCATTER ||
			max_pkt_size > dma_buf_size ||
			dev->data->mtu + RNP_ETH_OVERHEAD > dma_buf_size)
		dev->data->scattered_rx = 1;
	else
		dev->data->scattered_rx = 0;
	/* Setup max dma scatter engine split size */
	if (max_pkt_size == dma_buf_size)
		dma_buf_size += (dma_buf_size % 16);
	RNP_PMD_INFO("PF[%d] MaxPktLen %d MbSize %d MbHeadRoom %d",
			hw->mbx.pf_num, max_pkt_size,
			dma_buf_size, RTE_PKTMBUF_HEADROOM);
	dma_ctrl = RNP_E_REG_RD(hw,  RNP_DMA_CTRL);
	dma_ctrl &= ~RNP_DMA_SCATTER_MEM_MASK;
	dma_ctrl |= ((dma_buf_size / 16) << RNP_DMA_SCATTER_MEN_S);
	RNP_E_REG_WR(hw, RNP_DMA_CTRL, dma_ctrl);

	return 0;
}

static int rnp_enable_all_rx_queue(struct rte_eth_dev *dev)
{
	struct rnp_rx_queue *rxq;
	uint16_t idx;
	int ret = 0;

	for (idx = 0; idx < dev->data->nb_rx_queues; idx++) {
		rxq = dev->data->rx_queues[idx];
		if (!rxq || rxq->rx_deferred_start)
			continue;
		if (dev->data->rx_queue_state[idx] ==
				RTE_ETH_QUEUE_STATE_STOPPED) {
			ret = rnp_rx_queue_start(dev, idx);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int rnp_enable_all_tx_queue(struct rte_eth_dev *dev)
{
	struct rnp_tx_queue *txq;
	uint16_t idx;
	int ret = 0;

	for (idx = 0; idx < dev->data->nb_tx_queues; idx++) {
		txq = dev->data->tx_queues[idx];
		if (!txq || txq->tx_deferred_start)
			continue;
		if (dev->data->tx_queue_state[idx] ==
				RTE_ETH_QUEUE_STATE_STOPPED) {
			ret = rnp_tx_queue_start(dev, idx);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static void
rnp_vlan_strip_queue_set(struct rte_eth_dev *dev, uint16_t queue,
			 int on)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_rx_queue *rxq = NULL;
	struct rnp_hw *hw = port->hw;
	uint16_t index;
	uint32_t reg;

	rxq = dev->data->rx_queues[queue];
	if (rxq) {
		index = rxq->attr.index;
		reg = RNP_E_REG_RD(hw, RNP_VLAN_Q_STRIP_CTRL(index));
		if (on) {
			reg |= 1 << (index % 32);
			rxq->rx_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
		} else {
			reg &= ~(1 << (index % 32));
			rxq->rx_offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
		}
		RNP_E_REG_WR(hw, RNP_VLAN_Q_STRIP_CTRL(index), reg);
	}
}

static void
rnp_vlan_strip_enable(struct rnp_eth_port *port, bool en)
{
	int i = 0;

	for (i = 0; i < port->eth_dev->data->nb_rx_queues; i++) {
		if (port->eth_dev->data->rx_queues[i] == NULL) {
			RNP_PMD_ERR("Strip queue[%d] is NULL.", i);
			continue;
		}
		rnp_vlan_strip_queue_set(port->eth_dev, i, en);
	}
}

static void
rnp_double_vlan_enable(struct rnp_eth_port *port, bool on)
{
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t ctrl;

	/* En Double Vlan Engine */
	ctrl = RNP_MAC_REG_RD(hw, lane, RNP_MAC_VLAN_TAG);
	if (on)
		ctrl |= RNP_MAC_VLAN_EDVLP | RNP_MAC_VLAN_ESVL;
	else
		ctrl &= ~(RNP_MAC_VLAN_EDVLP | RNP_MAC_VLAN_ESVL);
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_VLAN_TAG, ctrl);
}

static int
rnp_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);

	if (mask & RTE_ETH_QINQ_STRIP_MASK) {
		RNP_PMD_ERR("QinQ Strip isn't supported.");
		return -ENOTSUP;
	}
	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER)
			rnp_rx_vlan_filter_en(port, true);
		else

			rnp_rx_vlan_filter_en(port, false);
	}
	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			rnp_vlan_strip_enable(port, true);
		else
			rnp_vlan_strip_enable(port, false);
	}
	if (mask & RTE_ETH_VLAN_EXTEND_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_EXTEND)
			rnp_double_vlan_enable(port, true);
		else
			rnp_double_vlan_enable(port, false);
	}

	return 0;
}

static int
rnp_vlan_filter_set(struct rte_eth_dev *dev,
		uint16_t vlan_id, int on)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);

	return rnp_update_vlan_filter(port, vlan_id, on);
}

static int rnp_dev_start(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	uint16_t max_rx_pkt_len = eth_dev->data->mtu;
	bool lsc = data->dev_conf.intr_conf.lsc;
	struct rnp_hw *hw = port->hw;
	uint16_t lane = 0;
	uint16_t idx = 0;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	lane = port->attr.nr_lane;
	ret = rnp_clock_valid_check(hw, lane);
	if (ret) {
		RNP_PMD_ERR("port[%d] function[%d] lane[%d] hw clock error",
				data->port_id, hw->mbx.pf_num, lane);
		return ret;
	}
	/* disable eth rx flow */
	RNP_RX_ETH_DISABLE(hw, lane);
	ret = rnp_dev_rss_configure(eth_dev);
	if (ret)
		return ret;
	ret = rnp_rx_scattered_setup(eth_dev);
	if (ret)
		return ret;
	ret = rnp_mtu_set(eth_dev, max_rx_pkt_len);
	if (ret)
		return ret;
	ret = rnp_enable_all_tx_queue(eth_dev);
	if (ret)
		goto txq_start_failed;
	ret = rnp_enable_all_rx_queue(eth_dev);
	if (ret)
		goto rxq_start_failed;
	rnp_mac_init(eth_dev);
	ret = rnp_mbx_fw_lane_link_event_en(port, lsc);
	if (ret < 0)
		goto rxq_start_failed;
	if (!lsc)
		rnp_run_link_poll_task(port);
	ret = rnp_dev_set_link_up(eth_dev);
	if (ret < 0)
		goto rxq_start_failed;
	/* enable eth rx flow */
	RNP_RX_ETH_ENABLE(hw, lane);
	rnp_rx_func_select(eth_dev);
	rnp_tx_func_select(eth_dev);
	port->port_stopped = 0;

	return 0;
rxq_start_failed:
	for (idx = 0; idx < data->nb_rx_queues; idx++)
		rnp_rx_queue_stop(eth_dev, idx);
txq_start_failed:
	for (idx = 0; idx < data->nb_tx_queues; idx++)
		rnp_tx_queue_stop(eth_dev, idx);

	return ret;
}

static int rnp_disable_all_rx_queue(struct rte_eth_dev *dev)
{
	struct rnp_rx_queue *rxq;
	uint16_t idx;
	int ret = 0;

	for (idx = 0; idx < dev->data->nb_rx_queues; idx++) {
		rxq = dev->data->rx_queues[idx];
		if (!rxq || rxq->rx_deferred_start)
			continue;
		if (dev->data->rx_queue_state[idx] ==
				RTE_ETH_QUEUE_STATE_STARTED) {
			ret = rnp_rx_queue_stop(dev, idx);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int rnp_disable_all_tx_queue(struct rte_eth_dev *dev)
{
	struct rnp_tx_queue *txq;
	uint16_t idx;
	int ret = 0;

	for (idx = 0; idx < dev->data->nb_tx_queues; idx++) {
		txq = dev->data->tx_queues[idx];
		if (!txq || txq->tx_deferred_start)
			continue;
		if (dev->data->tx_queue_state[idx] ==
				RTE_ETH_QUEUE_STATE_STARTED) {
			ret = rnp_tx_queue_stop(dev, idx);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static void rnp_set_rx_cksum_offload(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_hw *hw = port->hw;
	uint32_t cksum_ctrl;
	uint64_t offloads;

	offloads = dev->data->dev_conf.rxmode.offloads;
	cksum_ctrl = RNP_HW_CHECK_ERR_MASK;
	/* enable rx checksum feature */
	if (!rnp_pf_is_multiple_ports(hw->device_id)) {
		if (offloads & RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM) {
			/* Tunnel Option Cksum L4_Option */
			cksum_ctrl &= ~RNP_HW_L4_CKSUM_ERR;
			if (offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
						RTE_ETH_RX_OFFLOAD_TCP_CKSUM))
				cksum_ctrl &= ~RNP_HW_INNER_L4_CKSUM_ERR;
			else
				cksum_ctrl |= RNP_HW_INNER_L4_CKSUM_ERR;
		} else {
			/* no tunnel option cksum l4_option */
			cksum_ctrl |= RNP_HW_INNER_L4_CKSUM_ERR;
			if (offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
						RTE_ETH_RX_OFFLOAD_TCP_CKSUM))
				cksum_ctrl &= ~RNP_HW_L4_CKSUM_ERR;
			else
				cksum_ctrl |= RNP_HW_L4_CKSUM_ERR;
		}
		if (offloads & RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM) {
			/* tunnel option cksum l3_option */
			cksum_ctrl &= ~RNP_HW_L3_CKSUM_ERR;
			if (offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM)
				cksum_ctrl &= ~RNP_HW_INNER_L3_CKSUM_ERR;
			else
				cksum_ctrl |= RNP_HW_INNER_L3_CKSUM_ERR;
		} else {
			/* no tunnel option cksum l3_option */
			cksum_ctrl |= RNP_HW_INNER_L3_CKSUM_ERR;
			if (offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM)
				cksum_ctrl &= ~RNP_HW_L3_CKSUM_ERR;
			else
				cksum_ctrl |= RNP_HW_L3_CKSUM_ERR;
		}
		/* sctp option */
		if (offloads & RTE_ETH_RX_OFFLOAD_SCTP_CKSUM) {
			cksum_ctrl &= ~RNP_HW_SCTP_CKSUM_ERR;
			RNP_E_REG_WR(hw, RNP_HW_SCTP_CKSUM_CTRL, true);
		} else {
			RNP_E_REG_WR(hw, RNP_HW_SCTP_CKSUM_CTRL, false);
		}
		RNP_E_REG_WR(hw, RNP_HW_CHECK_ERR_CTRL, cksum_ctrl);
	} else {
		/* Enabled all support checksum features
		 * use software mode support per port rx checksum
		 * feature enabled/disabled for multiple port mode
		 */
		RNP_E_REG_WR(hw, RNP_HW_CHECK_ERR_CTRL, RNP_HW_ERR_RX_ALL_MASK);
		RNP_E_REG_WR(hw, RNP_HW_SCTP_CKSUM_CTRL, true);
	}
}

static void
rnp_qinq_insert_offload_en(struct rnp_eth_port *port, bool on)
{
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t cvlan_ctrl, svlan_ctrl;

	/* en double vlan engine */
	rnp_double_vlan_enable(port, on);
	/* setup inner vlan mode*/
	cvlan_ctrl = RNP_MAC_REG_RD(hw, lane, RNP_MAC_INNER_VLAN_INCL);
	if (on) {
		cvlan_ctrl |= RNP_MAC_VLAN_VLTI;
		cvlan_ctrl &= ~RNP_MAC_VLAN_CSVL;
		if (port->invlan_type == RNP_SVLAN_TYPE)
			cvlan_ctrl |= RNP_MAC_VLAN_INSERT_SVLAN;
		cvlan_ctrl &= ~RNP_MAC_VLAN_VLC;
		cvlan_ctrl |= RNP_MAC_VLAN_VLC_ADD;
	} else {
		cvlan_ctrl = 0;
	}
	/* setup outer vlan mode */
	svlan_ctrl = RNP_MAC_REG_RD(hw, lane, RNP_MAC_VLAN_INCL);
	if (on) {
		svlan_ctrl |= RNP_MAC_VLAN_VLTI;
		svlan_ctrl &= ~RNP_MAC_VLAN_CSVL;
		if (port->outvlan_type == RNP_SVLAN_TYPE)
			svlan_ctrl |= RNP_MAC_VLAN_INSERT_SVLAN;
		svlan_ctrl &= ~RNP_MAC_VLAN_VLC;
		svlan_ctrl |= RNP_MAC_VLAN_VLC_ADD;
	} else {
		svlan_ctrl = 0;
	}
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_INNER_VLAN_INCL, cvlan_ctrl);
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_VLAN_INCL, svlan_ctrl);
}

static void
rnp_vlan_insert_offload_en(struct rnp_eth_port *port, bool on)
{
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t ctrl;

	ctrl = RNP_MAC_REG_RD(hw, lane, RNP_MAC_VLAN_INCL);
	if (on) {
		ctrl |= RNP_MAC_VLAN_VLTI;
		ctrl &= ~RNP_MAC_VLAN_CSVL;
		if (port->invlan_type == RNP_SVLAN_TYPE)
			ctrl |= RNP_MAC_VLAN_INSERT_SVLAN;
		ctrl &= ~RNP_MAC_VLAN_VLC;
		ctrl |= RNP_MAC_VLAN_VLC_ADD;
	} else {
		ctrl = 0;
	}
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_VLAN_INCL, ctrl);
}

static int rnp_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct rte_eth_txmode *txmode = &eth_dev->data->dev_conf.txmode;
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_QINQ_INSERT &&
	    txmode->offloads & RTE_ETH_TX_OFFLOAD_VLAN_INSERT)
		rnp_qinq_insert_offload_en(port, true);
	else
		rnp_qinq_insert_offload_en(port, false);
	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_VLAN_INSERT &&
	    !(txmode->offloads & RTE_ETH_TX_OFFLOAD_QINQ_INSERT))
		rnp_vlan_insert_offload_en(port, true);
	if (!(txmode->offloads & RTE_ETH_TX_OFFLOAD_VLAN_INSERT))
		rnp_vlan_insert_offload_en(port, false);
	if (port->last_rx_num != eth_dev->data->nb_rx_queues)
		port->rxq_num_changed = true;
	else
		port->rxq_num_changed = false;
	port->last_rx_num = eth_dev->data->nb_rx_queues;
	rnp_set_rx_cksum_offload(eth_dev);

	return 0;
}

static int rnp_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	const struct rte_eth_dev_data *data = eth_dev->data;
	bool lsc = eth_dev->data->dev_conf.intr_conf.lsc;
	struct rte_eth_link link;
	int ret;

	if (port->port_stopped)
		return 0;
	eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_prepare = rte_eth_pkt_burst_dummy;

	/* clear the recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(eth_dev, &link);
	rnp_dev_set_link_down(eth_dev);
	ret = rnp_disable_all_tx_queue(eth_dev);
	if (ret < 0) {
		RNP_PMD_ERR("port[%u] disable tx queue failed", data->port_id);
		return ret;
	}
	ret = rnp_disable_all_rx_queue(eth_dev);
	if (ret < 0) {
		RNP_PMD_ERR("port[%u] disable rx queue failed", data->port_id);
		return ret;
	}
	rnp_mac_tx_disable(eth_dev);
	rnp_mac_rx_disable(eth_dev);
	if (!lsc)
		rnp_cancel_link_poll_task(port);
	port->attr.link_ready = false;
	port->attr.speed = 0;

	eth_dev->data->dev_started = 0;
	port->port_stopped = 1;

	return 0;
}

static void rnp_change_manage_port(struct rnp_eth_adapter *adapter)
{
	uint16_t idx = 0;

	adapter->eth_dev = NULL;
	for (idx = 0; idx < adapter->inited_ports; idx++) {
		if (adapter->ports[idx])
			adapter->eth_dev = adapter->ports[idx]->eth_dev;
	}
}

static int rnp_dev_close(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_adapter *adapter = RNP_DEV_TO_ADAPTER(eth_dev);
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	uint16_t exp = RNP_PF_OP_DONE;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;
	ret = rnp_dev_stop(eth_dev);
	if (ret < 0)
		return ret;
	do {
		ret = rte_atomic_compare_exchange_strong_explicit(&adapter->pf_op,
				&exp, RNP_PF_OP_CLOSING, rte_memory_order_acquire,
				rte_memory_order_acquire);
	} while (!ret);
	adapter->closed_ports++;
	adapter->ports[port->attr.sw_id] = NULL;
	if (adapter->intr_registered && adapter->eth_dev == eth_dev)
		rnp_change_manage_port(adapter);
	if (adapter->closed_ports == adapter->inited_ports) {
		struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI((void *)eth_dev->device);
		if (adapter->intr_registered) {
			/* disable uio irq before callback unregister */
			rte_intr_disable(pci_dev->intr_handle);
			rte_intr_callback_unregister(pci_dev->intr_handle,
					rnp_dev_interrupt_handler,
					(void *)eth_dev);
			adapter->intr_registered = false;
		}
		rnp_dma_mem_free(&adapter->hw, &adapter->hw.fw_info.mem);
		rte_free(adapter);
	}
	rte_atomic_store_explicit(&adapter->pf_op, RNP_PF_OP_DONE,
			rte_memory_order_release);

	return 0;
}

static uint32_t
rnp_get_speed_caps(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint32_t speed_cap = 0;
	uint32_t support_link;
	uint32_t speed = 0;
	int bit_pos = 0;

	support_link = port->attr.phy_meta.supported_link;
	if (support_link == 0)
		return 0;
	while (support_link) {
		bit_pos = rte_ffs32(support_link) - 1;
		speed = RTE_BIT32(bit_pos);
		switch (speed) {
		case RNP_SPEED_CAP_10M_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_10M;
			break;
		case RNP_SPEED_CAP_100M_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_100M;
			break;
		case RNP_SPEED_CAP_1GB_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_1G;
			break;
		case RNP_SPEED_CAP_10GB_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_10G;
			break;
		case RNP_SPEED_CAP_40GB_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_40G;
			break;
		case RNP_SPEED_CAP_25GB_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_25G;
			break;
		case RNP_SPEED_CAP_10M_HALF:
			speed_cap |= RTE_ETH_LINK_SPEED_10M_HD;
			break;
		case RNP_SPEED_CAP_100M_HALF:
			speed_cap |= RTE_ETH_LINK_SPEED_100M_HD;
			break;
		default:
			speed_cap |= 0;
		}
		support_link &= ~speed;
	}
	if (!port->attr.phy_meta.link_autoneg)
		speed_cap |= RTE_ETH_LINK_SPEED_FIXED;

	return speed_cap;
}

static int rnp_dev_infos_get(struct rte_eth_dev *eth_dev,
			     struct rte_eth_dev_info *dev_info)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim){
		.nb_max = RNP_MAX_BD_COUNT,
		.nb_min = RNP_MIN_BD_COUNT,
		.nb_align = RNP_BD_ALIGN,
		.nb_seg_max = RNP_RX_MAX_SEG,
		.nb_mtu_seg_max = RNP_RX_MAX_MTU_SEG,
	};
	dev_info->tx_desc_lim = (struct rte_eth_desc_lim){
		.nb_max = RNP_MAX_BD_COUNT,
		.nb_min = RNP_MIN_BD_COUNT,
		.nb_align = RNP_BD_ALIGN,
		.nb_seg_max = RNP_TX_MAX_SEG,
		.nb_mtu_seg_max = RNP_TX_MAX_MTU_SEG,
	};

	dev_info->max_rx_pktlen = RNP_MAC_MAXFRM_SIZE;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	dev_info->max_mtu = dev_info->max_rx_pktlen - RNP_ETH_OVERHEAD;
	dev_info->min_rx_bufsize = RNP_MIN_DMA_BUF_SIZE;
	dev_info->max_rx_queues = port->attr.max_rx_queues;
	dev_info->max_tx_queues = port->attr.max_tx_queues;
	/* mac filter info */
	dev_info->max_mac_addrs = port->attr.max_mac_addrs;
	/* for RSS offload just support four tuple */
	dev_info->flow_type_rss_offloads = RNP_SUPPORT_RSS_OFFLOAD_ALL;
	dev_info->hash_key_size = RNP_MAX_HASH_KEY_SIZE * sizeof(uint32_t);
	dev_info->reta_size = RNP_RSS_INDIR_SIZE;
	/* speed cap info */
	dev_info->speed_capa = rnp_get_speed_caps(eth_dev);
	/* per queue offload */
	dev_info->rx_queue_offload_capa = RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	/* rx support offload cap */
	dev_info->rx_offload_capa = RNP_RX_CHECKSUM_SUPPORT |
				    RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
				    RTE_ETH_RX_OFFLOAD_SCATTER;
	dev_info->rx_offload_capa |= dev_info->rx_queue_offload_capa;
	/* tx support offload cap */
	dev_info->tx_offload_capa = 0 |
				    RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
				    RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
				    RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
				    RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
				    RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
				    RTE_ETH_TX_OFFLOAD_TCP_TSO |
				    RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
				    RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
				    RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
				    RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
				    RTE_ETH_TX_OFFLOAD_QINQ_INSERT;
	/* default ring configure */
	dev_info->default_rxportconf.burst_size = 32;
	dev_info->default_txportconf.burst_size = 32;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = 256;
	dev_info->default_txportconf.ring_size = 256;
	/* default port configure */
	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_drop_en = 0,
		.rx_thresh = {
			.pthresh = RNP_RX_DESC_FETCH_TH,
			.hthresh = RNP_RX_DESC_FETCH_BURST,
		},
		.rx_free_thresh = RNP_DEFAULT_RX_FREE_THRESH,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = RNP_TX_DESC_FETCH_TH,
			.hthresh = RNP_TX_DESC_FETCH_BURST,
		},
		.tx_free_thresh = RNP_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = RNP_DEFAULT_TX_RS_THRESH,
		.offloads = 0,
	};

	return 0;
}

static int rnp_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();

	return rnp_update_mpfm(port, RNP_MPF_MODE_PROMISC, 1);
}

static int rnp_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();

	return rnp_update_mpfm(port, RNP_MPF_MODE_PROMISC, 0);
}

static int rnp_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();

	return rnp_update_mpfm(port, RNP_MPF_MODE_ALLMULTI, 1);
}

static int rnp_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();
	if (eth_dev->data->promiscuous == 1)
		return 0;
	return rnp_update_mpfm(port, RNP_MPF_MODE_ALLMULTI, 0);
}

static bool
rnp_verify_pf_scatter(struct rnp_eth_adapter *adapter)
{
	struct rnp_hw *hw = &adapter->hw;
	struct rte_eth_dev *eth_dev;
	uint8_t i = 0;

	for (i = 0; i < hw->max_port_num; i++) {
		if (adapter->ports[i] == NULL)
			continue;
		eth_dev = adapter->ports[i]->eth_dev;
		if (eth_dev->data == NULL)
			continue;
		/* sub port of pf eth_dev state is not
		 * started so the scatter_rx attr isn't
		 * setup don't check this sub port.
		 */
		if (!eth_dev->data->dev_started)
			continue;
		if (!eth_dev->data->scattered_rx)
			return false;
	}

	return true;
}

static int
rnp_update_valid_mtu(struct rnp_eth_port *port, uint16_t *set_mtu)
{
	struct rnp_eth_adapter *adapter = port->hw->back;
	struct rnp_eth_port *sub_port = NULL;
	struct rnp_hw *hw = port->hw;
	uint16_t origin_mtu = 0;
	uint16_t mtu = 0;
	uint8_t i = 0;

	if (hw->max_port_num == 1) {
		port->cur_mtu = *set_mtu;

		return 0;
	}
	origin_mtu = port->cur_mtu;
	port->cur_mtu = *set_mtu;
	mtu = *set_mtu;
	for (i = 0; i < hw->max_port_num; i++) {
		sub_port = adapter->ports[i];
		if (sub_port == NULL)
			continue;
		mtu = RTE_MAX(mtu, sub_port->cur_mtu);
	}
	if (hw->max_port_num > 1 &&
			mtu + RNP_ETH_OVERHEAD > hw->min_dma_size) {
		if (!rnp_verify_pf_scatter(adapter)) {
			RNP_PMD_ERR("single pf multiple port max_frame_sz "
					"is bigger than min_dma_size please "
					"stop all pf port before set mtu.");
			port->cur_mtu = origin_mtu;
			return -EINVAL;
		}
	}
	*set_mtu = mtu;

	return 0;
}

static int
rnp_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint32_t frame_size = mtu + RNP_ETH_OVERHEAD;
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	bool jumbo_en = false;
	uint32_t reg;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	/* check that mtu is within the allowed range */
	if (frame_size < RTE_ETHER_MIN_LEN ||
			frame_size > RNP_MAC_MAXFRM_SIZE) {
		RNP_PMD_ERR("valid packet length must be "
				"range from %u to  %u, "
				"when Jumbo Frame Feature disabled",
				(uint32_t)RTE_ETHER_MIN_LEN,
				(uint32_t)RTE_ETHER_MAX_LEN);
		return -EINVAL;
	}
	/*
	 * Refuse mtu that requires the support of scattered packets
	 * when this feature has not been enabled before.
	 */
	if (dev->data->dev_started && !dev->data->scattered_rx &&
			frame_size > dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM) {
		RNP_PMD_ERR("port %d mtu update must be stopped "
				"before configuration when scatter rx off.",
				dev->data->port_id);

		return -EBUSY;
	}
	/* For one pf multiple port the mtu we must set
	 * the biggest mtu the ports selong to pf
	 * because of the control button is only one
	 */
	ret = rnp_update_valid_mtu(port, &mtu);
	if (ret < 0)
		return ret;
	frame_size = mtu + RNP_ETH_OVERHEAD;
	if (frame_size > RTE_ETHER_MAX_LEN)
		jumbo_en = true;
	/* setting the MTU */
	RNP_E_REG_WR(hw, RNP_MAX_FRAME_CTRL, frame_size);
	RNP_E_REG_WR(hw, RNP_MIN_FRAME_CTRL, 60);
	if (jumbo_en) {
		/* To protect conflict hw resource */
		rte_spinlock_lock(&port->rx_mac_lock);
		reg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_RX_CFG);
		reg |= RNP_MAC_JE;
		RNP_MAC_REG_WR(hw, lane, RNP_MAC_RX_CFG, reg);
		rte_spinlock_unlock(&port->rx_mac_lock);
	}
	port->jumbo_en = jumbo_en;

	return 0;
}

struct rte_rnp_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint32_t offset;
	uint32_t reg_base;
	bool hi_addr_en;
};

static const struct rte_rnp_xstats_name_off rte_rnp_rx_eth_stats_str[] = {
	{"rx_trans_drop", offsetof(struct rnp_hw_eth_stats,
			rx_trans_drop), RNP_ETH_RXTRANS_DROP, false},
	{"rx_trunc_drop", offsetof(struct rnp_hw_eth_stats,
			rx_trunc_drop), RNP_ETH_RXTRUNC_DROP, false},
	{"rx_undersize_err_packets", offsetof(struct rnp_hw_eth_stats,
			rx_slen_drop), RNP_ETH_RXSLAN_DROP, false},
	{"rx_oversize_err_packets", offsetof(struct rnp_hw_eth_stats,
			rx_glen_drop), RNP_ETH_RXGLAN_DROP, false},
	{"rx_iph_err_packet", offsetof(struct rnp_hw_eth_stats,
			rx_cksum_e_drop), RNP_ETH_RXCKSUM_E_DROP, false},
	{"rx_cksum_err_packet", offsetof(struct rnp_hw_eth_stats,
			rx_iph_e_drop), RNP_ETH_RXIPH_E_DROP, false},
};

static const struct rte_rnp_xstats_name_off rte_rnp_mac_stats_str[] = {
	{"rx_packets", offsetof(struct rnp_hw_mac_stats,
			rx_all_pkts), RNP_MMC_RX_GBFRMB, true},
	{"rx_bytes", offsetof(struct rnp_hw_mac_stats,
			rx_all_bytes), RNP_MMC_RX_GBOCTGB, true},
	{"rx_unicast_packets", offsetof(struct rnp_hw_mac_stats,
			rx_unicast), RNP_MMC_RX_UCASTGB, true},
	{"rx_broadcast_packets", offsetof(struct rnp_hw_mac_stats,
			rx_broadcast), RNP_MMC_RX_BCASTGB, true},
	{"rx_multicast_packets", offsetof(struct rnp_hw_mac_stats,
			rx_multicast), RNP_MMC_RX_MCASTGB, true},
	{"rx_pause_packets", offsetof(struct rnp_hw_mac_stats,
			rx_pause), RNP_MMC_RX_PAUSEB, true},
	{"rx_vlan_packets", offsetof(struct rnp_hw_mac_stats,
			rx_vlan), RNP_MMC_RX_VLANGB, true},
	{"rx_64_byte_packets", offsetof(struct rnp_hw_mac_stats,
			rx_64octes_pkts), RNP_MMC_RX_64_BYTESB, true},
	{"rx_65_to_127_byte_packets", offsetof(struct rnp_hw_mac_stats,
			rx_65to127_octes_pkts), RNP_MMC_RX_65TO127_BYTESB, true},
	{"rx_128_to_255_byte_packets", offsetof(struct rnp_hw_mac_stats,
			rx_128to255_octes_pkts), RNP_MMC_RX_128TO255_BYTESB, true},
	{"rx_256_to_511_byte_packets", offsetof(struct rnp_hw_mac_stats,
			rx_256to511_octes_pkts), RNP_MMC_RX_256TO511_BYTESB, true},
	{"rx_512_to_1023_byte_packets", offsetof(struct rnp_hw_mac_stats,
			rx_512to1023_octes_pkts), RNP_MMC_RX_512TO1203_BYTESB, true},
	{"rx_1024_to_max_byte_packets", offsetof(struct rnp_hw_mac_stats,
			rx_1024tomax_octes_pkts), RNP_MMC_RX_1024TOMAX_BYTESB, true},
	{"rx_len_over_9k", offsetof(struct rnp_hw_mac_stats,
			rx_oversize_9k), RNP_MMC_RX_OSIZEGB, false},
	{"rx_crc_errors", offsetof(struct rnp_hw_mac_stats,
			rx_crc_err), RNP_MMC_RX_CRCERB, true},
	{"rx_crc_errors_small_packets", offsetof(struct rnp_hw_mac_stats,
			rx_runt_err), RNP_MMC_RX_RUNTERB, false},
	{"rx_jabber_errors", offsetof(struct rnp_hw_mac_stats,
			rx_jabber_err), RNP_MMC_RX_JABBER_ERR, false},
	{"rx_length_errors", offsetof(struct rnp_hw_mac_stats,
			rx_len_err), RNP_MMC_RX_LENERRB, true},
	{"rx_out_of_range_errors", offsetof(struct rnp_hw_mac_stats,
			rx_len_invalid), RNP_MMC_RX_OUTOF_RANGE, true},
	{"rx_watchdog_errors", offsetof(struct rnp_hw_mac_stats,
			rx_watchdog_err), RNP_MMC_RX_WDOGERRB, true},

	{"tx_packets", offsetof(struct rnp_hw_mac_stats,
			tx_all_pkts), RNP_MMC_TX_GBFRMB, true},
	{"tx_bytes", offsetof(struct rnp_hw_mac_stats,
			tx_all_bytes), RNP_MMC_TX_GBOCTGB, true},
	{"tx_unicast_packets", offsetof(struct rnp_hw_mac_stats,
			tx_all_unicast), RNP_MMC_TX_GBUCASTB, true},
	{"tx_broadcast_packets", offsetof(struct rnp_hw_mac_stats,
			tx_all_broadcast), RNP_MMC_TX_BCASTB, true},
	{"tx_multicast_packets", offsetof(struct rnp_hw_mac_stats,
			tx_all_multicast), RNP_MMC_TX_MCASTB, true},
	{"tx_vlan_packets", offsetof(struct rnp_hw_mac_stats,
			tx_vlan_pkts), RNP_MMC_TX_VLANB, true},
	{"tx_pause_packets", offsetof(struct rnp_hw_mac_stats,
			tx_pause_pkts), RNP_MMC_TX_PAUSEB, true},
	{"tx_64_byte_packets", offsetof(struct rnp_hw_mac_stats,
			tx_64octes_pkts), RNP_MMC_TX_64_BYTESB, true},
	{"tx_65_to_127_byte_packets", offsetof(struct rnp_hw_mac_stats,
			tx_65to127_octes_pkts), RNP_MMC_TX_65TO127_BYTESB, true},
	{"tx_128_to_255_byte_packets", offsetof(struct rnp_hw_mac_stats,
			tx_128to255_octes_pkts), RNP_MMC_TX_128TO255_BYTEB, true},
	{"tx_256_to_511_byte_packets", offsetof(struct rnp_hw_mac_stats,
			tx_256to511_octes_pkts), RNP_MMC_TX_256TO511_BYTEB, true},
	{"tx_512_to_1023_byte_packets", offsetof(struct rnp_hw_mac_stats,
			tx_512to1023_octes_pkts), RNP_MMC_TX_512TO1023_BYTEB, true},
	{"tx_1024_to_max_byte_packets", offsetof(struct rnp_hw_mac_stats,
			tx_1024tomax_octes_pkts), RNP_MMC_TX_1024TOMAX_BYTEB, true},
	{"tx_underflow_errors", offsetof(struct rnp_hw_mac_stats,
			tx_underflow_err), RNP_MMC_TX_UNDRFLWB, true},
};

#define RNP_NB_HW_MAC_STATS (RTE_DIM(rte_rnp_mac_stats_str))
#define RNP_NB_RX_HW_ETH_STATS (RTE_DIM(rte_rnp_rx_eth_stats_str))
#define RNP_GET_E_HW_COUNT(stats, offset)            \
	((uint64_t *)(((char *)(stats)) + (offset)))
#define RNP_ADD_INCL_COUNT(stats, offset, val)       \
	((*(RNP_GET_E_HW_COUNT((stats), (offset)))) += (val))
static inline void
rnp_store_hw_stats(struct rnp_hw_mac_stats *stats,
		   uint32_t offset, uint64_t val)
{
	*(uint64_t *)(((char *)stats) + offset) = val;
}

static int rnp_dev_cal_xstats_num(void)
{
	int cnt = RNP_NB_HW_MAC_STATS + RNP_NB_RX_HW_ETH_STATS;

	return cnt;
}

static inline void
rnp_update_eth_stats_32bit(struct rnp_hw_eth_stats *new,
			   struct rnp_hw_eth_stats *old,
			   uint32_t offset, uint32_t val)
{
	uint64_t *last_count = NULL;

	last_count = RNP_GET_E_HW_COUNT(old, offset);
	if (val >= *last_count)
		RNP_ADD_INCL_COUNT(new, offset, val - (*last_count));
	else
		RNP_ADD_INCL_COUNT(new, offset, val + UINT32_MAX);
	*last_count = val;
}

static void rnp_get_eth_count(struct rnp_hw *hw,
			      uint16_t lane,
			      struct rnp_hw_eth_stats *new,
			      struct rnp_hw_eth_stats *old,
			      const struct rte_rnp_xstats_name_off *ptr)
{
	uint64_t val = 0;

	if (ptr->reg_base) {
		val = RNP_E_REG_RD(hw, ptr->reg_base + 0x40 * lane);
		rnp_update_eth_stats_32bit(new, old, ptr->offset, val);
	}
}

static void
rnp_get_mmc_info(struct rnp_hw *hw,
		 uint16_t lane,
		 struct rnp_hw_mac_stats *stats,
		 const struct rte_rnp_xstats_name_off *ptr)
{
	uint64_t count = 0;
	uint32_t offset;
	uint64_t hi_reg;

	if (ptr->reg_base) {
		count = RNP_MAC_REG_RD(hw, lane, ptr->reg_base);
		if (ptr->hi_addr_en) {
			offset = ptr->reg_base + 4;
			hi_reg = RNP_MAC_REG_RD(hw, lane, offset);
			count += (hi_reg << 32);
		}
		rnp_store_hw_stats(stats, ptr->offset, count);
	}
}

static void rnp_get_hw_stats(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_hw_eth_stats *old = &port->eth_stats_old;
	struct rnp_hw_eth_stats *new = &port->eth_stats;
	struct rnp_hw_mac_stats *stats = &port->mac_stats;
	const struct rte_rnp_xstats_name_off *ptr;
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t i;

	for (i = 0; i < RNP_NB_RX_HW_ETH_STATS; i++) {
		ptr = &rte_rnp_rx_eth_stats_str[i];
		rnp_get_eth_count(hw, lane, new, old, ptr);
	}
	for (i = 0; i < RNP_NB_HW_MAC_STATS; i++) {
		ptr = &rte_rnp_mac_stats_str[i];
		rnp_get_mmc_info(hw, lane, stats, ptr);
	}
}

static int
rnp_dev_stats_get(struct rte_eth_dev *dev,
		  struct rte_eth_stats *stats)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_hw_eth_stats *eth_stats = &port->eth_stats;
	struct rnp_hw_mac_stats *mac_stats = &port->mac_stats;
	struct rte_eth_dev_data *data = dev->data;
	uint16_t i = 0;

	PMD_INIT_FUNC_TRACE();
	rnp_get_hw_stats(dev);

	for (i = 0; i < data->nb_rx_queues; i++) {
		const struct rnp_rx_queue *rxq = dev->data->rx_queues[i];

		if (!rxq)
			continue;
		stats->ipackets += rxq->stats.ipackets;
		stats->ibytes += rxq->stats.ibytes;
		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[i] = rxq->stats.ipackets;
			stats->q_ibytes[i] = rxq->stats.ibytes;
		}
	}

	for (i = 0; i < data->nb_tx_queues; i++) {
		const struct rnp_tx_queue *txq = dev->data->tx_queues[i];

		if (!txq)
			continue;
		stats->opackets += txq->stats.opackets;
		stats->obytes += txq->stats.obytes;
		stats->oerrors += txq->stats.errors;
		if (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[i] = txq->stats.opackets;
			stats->q_obytes[i] = txq->stats.obytes;
		}
	}
	stats->imissed = eth_stats->rx_trans_drop + eth_stats->rx_trunc_drop;
	stats->ierrors = mac_stats->rx_crc_err + mac_stats->rx_len_err;
	stats->ierrors += mac_stats->rx_watchdog_err;
	stats->oerrors += mac_stats->tx_underflow_err;

	return 0;
}

static int
rnp_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_hw_eth_stats *eth_stats = &port->eth_stats;
	uint16_t idx;

	PMD_INIT_FUNC_TRACE();
	memset(eth_stats, 0, sizeof(*eth_stats));
	for (idx = 0; idx < dev->data->nb_rx_queues; idx++) {
		struct rnp_rx_queue *rxq = dev->data->rx_queues[idx];

		if (!rxq)
			continue;
		memset(&rxq->stats, 0, sizeof(struct rnp_queue_stats));
	}
	for (idx = 0; idx < dev->data->nb_tx_queues; idx++) {
		struct rnp_tx_queue *txq = dev->data->tx_queues[idx];

		if (!txq)
			continue;
		memset(&txq->stats, 0, sizeof(struct rnp_queue_stats));
	}

	return 0;
}

static inline uint64_t
rnp_get_statistic_value(void *stats_ptr, uint32_t offset)
{
	stats_ptr = (char *)stats_ptr + offset;

	return *((uint64_t *)stats_ptr);
}

static int
rnp_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		   unsigned int n __rte_unused)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_hw_eth_stats *eth_stats = &port->eth_stats;
	struct rnp_hw_mac_stats *mac_stats = &port->mac_stats;
	int count = 0;
	uint32_t i;

	if (xstats != NULL) {
		rnp_get_hw_stats(dev);
		for (i = 0; i < RNP_NB_RX_HW_ETH_STATS; i++) {
			xstats[count].value = rnp_get_statistic_value(eth_stats,
					rte_rnp_rx_eth_stats_str[i].offset);
			xstats[count].id = count;
			count++;
		}
		for (i = 0; i < RNP_NB_HW_MAC_STATS; i++) {
			xstats[count].value = rnp_get_statistic_value(mac_stats,
					rte_rnp_mac_stats_str[i].offset);
			xstats[count].id = count;
			count++;
		}
	} else {
		return rnp_dev_cal_xstats_num();
	}

	return count;
}

static int
rnp_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t reg;

	/* set MMC reset hw counter when read event */
	reg = RNP_MAC_REG_RD(hw, lane, RNP_MMC_CTRL);
	reg |= RNP_MMC_RSTONRD;
	RNP_MAC_REG_WR(hw, lane, RNP_MMC_CTRL, reg);

	rnp_dev_stats_reset(dev);
	rnp_get_hw_stats(dev);
	reg = RNP_MAC_REG_RD(hw, lane, RNP_MMC_CTRL);
	reg &= ~RNP_MMC_RSTONRD;
	RNP_MAC_REG_WR(hw, lane, RNP_MMC_CTRL, reg);

	return 0;
}

static int
rnp_dev_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
			 struct rte_eth_xstat_name *xstats_names,
			 __rte_unused unsigned int size)
{
	int count = 0;
	uint32_t i;

	if (xstats_names == NULL)
		return rnp_dev_cal_xstats_num();

	for (i = 0; i < RNP_NB_RX_HW_ETH_STATS; i++) {
		strlcpy(xstats_names[count].name,
				rte_rnp_rx_eth_stats_str[i].name,
				sizeof(xstats_names[count].name));
		count++;
	}
	for (i = 0; i < RNP_NB_HW_MAC_STATS; i++) {
		strlcpy(xstats_names[count].name,
				rte_rnp_mac_stats_str[i].name,
				sizeof(xstats_names[count].name));
		count++;
	}

	return count;
}

static int
rnp_dev_mac_addr_set(struct rte_eth_dev *dev,
		     struct rte_ether_addr *mac_addr)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);

	return rnp_set_macaddr(port, (u8 *)mac_addr, 0);
}

static int
rnp_dev_mac_addr_add(struct rte_eth_dev *dev,
		     struct rte_ether_addr *mac_addr,
		     uint32_t index,
		     uint32_t vmdq __rte_unused)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);

	if (index >= port->attr.max_mac_addrs) {
		RNP_PMD_ERR("mac add index %u is of range", index);
		return -EINVAL;
	}

	return rnp_set_macaddr(port, (u8 *)mac_addr, index);
}

static void
rnp_dev_mac_addr_remove(struct rte_eth_dev *dev,
			uint32_t index)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);

	if (index >= port->attr.max_mac_addrs) {
		RNP_PMD_ERR("mac add index %u is of range", index);
		return;
	}
	rnp_clear_macaddr(port, index);
}

static int
rnp_dev_set_mc_addr_list(struct rte_eth_dev *dev,
			 struct rte_ether_addr *mc_addr_list,
			 uint32_t nb_mc_addr)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint32_t idx = 0;

	if (nb_mc_addr > port->attr.max_mc_mac_hash)
		return -EINVAL;
	rnp_clear_mc_hash(port);
	for (idx = 0; idx < nb_mc_addr; idx++) {
		if (!rte_is_multicast_ether_addr(&mc_addr_list[idx])) {
			RNP_PMD_ERR("mc_list[%d] isn't a valid multicast", idx);
			return -EINVAL;
		}
	}
	for (idx = 0; idx < nb_mc_addr; idx++)
		rnp_update_mc_hash(port, (uint8_t *)&mc_addr_list[idx]);

	return 0;
}

static uint32_t *rnp_support_ptypes_get(void)
{
	static uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_TIMESYNC,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_TUNNEL_VXLAN,
		RTE_PTYPE_TUNNEL_GRE,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_UNKNOWN,
	};

	return ptypes;
}

static const uint32_t *
rnp_dev_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused,
			     size_t *no_of_elements __rte_unused)
{
	return rnp_support_ptypes_get();
}

/* Features supported by this driver */
static const struct eth_dev_ops rnp_eth_dev_ops = {
	.dev_configure                = rnp_dev_configure,
	.dev_close                    = rnp_dev_close,
	.dev_start                    = rnp_dev_start,
	.dev_stop                     = rnp_dev_stop,
	.dev_infos_get                = rnp_dev_infos_get,

	/* PROMISC */
	.promiscuous_enable           = rnp_promiscuous_enable,
	.promiscuous_disable          = rnp_promiscuous_disable,
	.allmulticast_enable          = rnp_allmulticast_enable,
	.allmulticast_disable         = rnp_allmulticast_disable,

	.mtu_set                      = rnp_mtu_set,
	.rx_queue_setup               = rnp_rx_queue_setup,
	.rx_queue_release             = rnp_dev_rx_queue_release,
	.rx_queue_stop                = rnp_rx_queue_stop,
	.rx_queue_start               = rnp_rx_queue_start,
	.rxq_info_get                 = rnp_rx_queue_info_get,
	.rx_burst_mode_get            = rnp_rx_burst_mode_get,
	.tx_queue_setup               = rnp_tx_queue_setup,
	.tx_queue_release             = rnp_dev_tx_queue_release,
	.tx_queue_stop                = rnp_tx_queue_stop,
	.tx_queue_start               = rnp_tx_queue_start,
	.txq_info_get                 = rnp_tx_queue_info_get,
	.tx_burst_mode_get            = rnp_tx_burst_mode_get,
	/* rss impl */
	.reta_update                  = rnp_dev_rss_reta_update,
	.reta_query                   = rnp_dev_rss_reta_query,
	.rss_hash_update              = rnp_dev_rss_hash_update,
	.rss_hash_conf_get            = rnp_dev_rss_hash_conf_get,
	/* stats */
	.stats_get                    = rnp_dev_stats_get,
	.stats_reset                  = rnp_dev_stats_reset,
	.xstats_get                   = rnp_dev_xstats_get,
	.xstats_reset                 = rnp_dev_xstats_reset,
	.xstats_get_names             = rnp_dev_xstats_get_names,
	/* link impl */
	.link_update                  = rnp_dev_link_update,
	.dev_set_link_up              = rnp_dev_set_link_up,
	.dev_set_link_down            = rnp_dev_set_link_down,
	/* mac address filter */
	.mac_addr_set                 = rnp_dev_mac_addr_set,
	.mac_addr_add                 = rnp_dev_mac_addr_add,
	.mac_addr_remove              = rnp_dev_mac_addr_remove,
	.set_mc_addr_list             = rnp_dev_set_mc_addr_list,
	/* vlan offload */
	.vlan_offload_set             = rnp_vlan_offload_set,
	.vlan_strip_queue_set         = rnp_vlan_strip_queue_set,
	.vlan_filter_set              = rnp_vlan_filter_set,
	.dev_supported_ptypes_get     = rnp_dev_supported_ptypes_get,
};

static void
rnp_setup_port_attr(struct rnp_eth_port *port,
		    struct rte_eth_dev *eth_dev,
		    uint8_t sw_id)
{
	struct rnp_port_attr *attr = &port->attr;
	struct rnp_hw *hw = port->hw;
	uint32_t lane;

	PMD_INIT_FUNC_TRACE();

	lane = hw->phy_port_ids[sw_id] & (hw->max_port_num - 1);
	attr->port_id = eth_dev->data->port_id;
	attr->port_offset = RNP_E_REG_RD(hw, RNP_TC_PORT_OFFSET(lane));
	attr->nr_lane = lane;
	attr->sw_id = sw_id;

	attr->max_rx_queues = RNP_MAX_RX_QUEUE_NUM / hw->max_port_num;
	attr->max_tx_queues = RNP_MAX_TX_QUEUE_NUM / hw->max_port_num;

	if (hw->nic_mode > RNP_SINGLE_10G) {
		attr->max_mac_addrs = RNP_PORT_MAX_MACADDR;
		attr->max_uc_mac_hash = 0;
		attr->max_mc_mac_hash = RNP_PORT_MAX_MC_MAC_SIZE;
		attr->uc_hash_tb_size = 0;
		attr->mc_hash_tb_size = RNP_PORT_MAX_MC_HASH_TB;
		attr->hash_table_shift = RNP_PORT_HASH_SHIFT;
	} else {
		attr->max_mac_addrs = RNP_MAX_MAC_ADDRS;
		attr->max_uc_mac_hash = RNP_MAX_HASH_UC_MAC_SIZE;
		attr->max_mc_mac_hash = RNP_MAX_HASH_MC_MAC_SIZE;
		attr->uc_hash_tb_size = RNP_MAX_UC_HASH_TABLE;
		attr->mc_hash_tb_size = RNP_MAC_MC_HASH_TABLE;
		attr->hash_table_shift = 0;
	}
	port->outvlan_type = RNP_SVLAN_TYPE;
	port->invlan_type = RNP_CVLAN_TYPE;

	rnp_mbx_fw_get_lane_stat(port);

	RNP_PMD_INFO("PF[%d] SW-ETH-PORT[%d]<->PHY_LANE[%d]",
			hw->mbx.pf_num, sw_id, lane);
}

static int
rnp_init_port_resource(struct rnp_eth_adapter *adapter,
		       struct rte_eth_dev *eth_dev,
		       char *name,
		       uint8_t p_id)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	struct rte_pci_device *pci_dev = adapter->pdev;
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE] = " ";

	PMD_INIT_FUNC_TRACE();

	port->eth_dev = eth_dev;
	port->hw = &adapter->hw;

	eth_dev->dev_ops = &rnp_eth_dev_ops;
	eth_dev->device = &pci_dev->device;
	eth_dev->data->mtu = RTE_ETHER_MTU;

	rnp_setup_port_attr(port, eth_dev, p_id);
	eth_dev->data->mac_addrs = rte_zmalloc(name,
			sizeof(struct rte_ether_addr) *
			port->attr.max_mac_addrs, 0);
	if (!eth_dev->data->mac_addrs) {
		RNP_PMD_ERR("zmalloc for mac failed! Exiting.");
		return -ENOMEM;
	}
	rnp_get_mac_addr(port, port->mac_addr.addr_bytes);
	rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					&port->mac_addr);
	RNP_PMD_INFO("get mac addr from firmware %s", mac_str);
	if (!rte_is_valid_assigned_ether_addr(&port->mac_addr)) {
		RNP_PMD_WARN("get mac_addr is invalid, just use random");
		rte_eth_random_addr(port->mac_addr.addr_bytes);
	}
	rte_ether_addr_copy(&port->mac_addr, &eth_dev->data->mac_addrs[0]);
	rnp_set_macaddr(port, (u8 *)&port->mac_addr, 0);

	rte_spinlock_init(&port->rx_mac_lock);
	adapter->ports[p_id] = port;
	adapter->inited_ports++;

	return 0;
}

static int
rnp_proc_priv_init(struct rte_eth_dev *dev)
{
	struct rnp_proc_priv *priv;

	priv = rte_zmalloc_socket("rnp_proc_priv",
			sizeof(struct rnp_proc_priv),
			RTE_CACHE_LINE_SIZE,
			dev->device->numa_node);
	if (!priv)
		return -ENOMEM;
	dev->process_private = priv;

	return 0;
}

static int
rnp_rx_reset_pool_setup(struct rnp_eth_adapter *adapter)
{
	struct rte_eth_dev *eth_dev = adapter->eth_dev;
	char name[RTE_MEMPOOL_NAMESIZE];

	snprintf(name, sizeof(name), "rx_reset_pool_%d:%d",
			eth_dev->data->port_id, eth_dev->device->numa_node);

	adapter->reset_pool = rte_pktmbuf_pool_create(name, 2,
			0, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
			eth_dev->device->numa_node);
	if (adapter->reset_pool == NULL) {
		RNP_PMD_ERR("mempool %s create failed", name);
		return -ENOMEM;
	}

	return 0;
}

static int
rnp_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI((void *)eth_dev->device);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	char name[RTE_ETH_NAME_MAX_LEN] = " ";
	struct rnp_eth_adapter *adapter;
	struct rte_eth_dev *sub_eth_dev;
	struct rnp_hw *hw;
	uint16_t p_id;
	int ret = -1;

	PMD_INIT_FUNC_TRACE();

	snprintf(name, sizeof(name), "rnp_adapter_%d", eth_dev->data->port_id);
	adapter = rte_zmalloc(name, sizeof(struct rnp_eth_adapter), 0);
	if (!adapter) {
		RNP_PMD_ERR("rnp_adapter zmalloc mem failed");
		return -ENOMEM;
	}
	hw = &adapter->hw;
	adapter->pdev = pci_dev;
	adapter->eth_dev = eth_dev;
	adapter->ports[0] = port;
	hw->back = (void *)adapter;
	port->eth_dev = eth_dev;
	port->hw = hw;

	hw->e_ctrl = (u8 *)pci_dev->mem_resource[4].addr;
	hw->c_ctrl = (u8 *)pci_dev->mem_resource[0].addr;
	hw->c_blen = pci_dev->mem_resource[0].len;
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->mbx.en_vfs = pci_dev->max_vfs;
	if (hw->mbx.en_vfs > hw->max_vfs) {
		ret = -EINVAL;
		RNP_PMD_ERR("sriov vfs max support 64");
		goto free_ad;
	}

	strlcpy(hw->device_name, pci_dev->device.name,
			strlen(pci_dev->device.name) + 1);
	ret = rnp_proc_priv_init(eth_dev);
	if (ret < 0) {
		RNP_PMD_ERR("proc_priv_alloc failed");
		goto free_ad;
	}
	ret = rnp_init_mbx_pf(hw);
	if (ret < 0) {
		RNP_PMD_ERR("mailbox hardware init failed");
		goto free_ad;
	}
	ret = rnp_init_hw(hw);
	if (ret < 0) {
		RNP_PMD_ERR("Hardware initialization failed");
		goto free_ad;
	}
	ret = rnp_setup_common_ops(hw);
	if (ret < 0) {
		RNP_PMD_ERR("hardware common ops setup failed");
		goto free_ad;
	}
	rnp_mbx_fw_pf_link_event_en(port, false);
	for (p_id = 0; p_id < hw->max_port_num; p_id++) {
		/* port 0 resource has been allocated when probe */
		if (!p_id) {
			sub_eth_dev = eth_dev;
		} else {
			if (strlen(hw->device_name) + 4 > sizeof(name))
				return -EINVAL;
			snprintf(name, sizeof(name),
					"%s_%d", hw->device_name, p_id);
			sub_eth_dev = rnp_alloc_eth_port(pci_dev, name);
			if (!sub_eth_dev) {
				RNP_PMD_ERR("%s sub_eth alloc failed",
						hw->device_name);
				ret = -ENOMEM;
				goto eth_alloc_error;
			}
			ret = rnp_proc_priv_init(sub_eth_dev);
			if (ret < 0) {
				RNP_PMD_ERR("proc_priv_alloc failed");
				goto eth_alloc_error;
			}
			memcpy(sub_eth_dev->process_private,
					eth_dev->process_private,
					sizeof(struct rnp_proc_priv));
		}
		ret = rnp_init_port_resource(adapter, sub_eth_dev, name, p_id);
		if (ret)
			goto eth_alloc_error;
		rnp_mac_rx_disable(sub_eth_dev);
		rnp_mac_tx_disable(sub_eth_dev);
		if (p_id) {
			/* port 0 will be probe by platform */
			rte_eth_dev_probing_finish(sub_eth_dev);
		}
	}
	ret = rnp_rx_reset_pool_setup(adapter);
	if (ret)
		goto eth_alloc_error;
	/* enable link update event interrupt */
	rte_intr_callback_register(intr_handle,
			rnp_dev_interrupt_handler, adapter);
	rte_intr_enable(intr_handle);
	rnp_mbx_fw_pf_link_event_en(port, true);

	return 0;

eth_alloc_error:
	for (p_id = 0; p_id < adapter->inited_ports; p_id++) {
		port = adapter->ports[p_id];
		if (!port)
			continue;
		if (port->eth_dev) {
			rnp_dev_close(port->eth_dev);
			/* just release eth_dev allocated by myself */
			if (port->eth_dev != adapter->eth_dev)
				rte_eth_dev_release_port(port->eth_dev);
		}
	}
free_ad:
	if (hw->fw_info.cookie_pool)
		rnp_dma_mem_free(hw, &hw->fw_info.mem);
	rte_free(adapter);

	return ret;

}

static int
rnp_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI((void *)eth_dev->device);
	uint16_t port_id;
	int err = 0;

	/* Free up other ports and all resources */
	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device)
		err |= rte_eth_dev_close(port_id);

	return err == 0 ? 0 : -EIO;
}

static int
rnp_pci_remove(struct rte_pci_device *pci_dev)
{
	char device_name[RTE_ETH_NAME_MAX_LEN] = "";
	struct rte_eth_dev *eth_dev = NULL;
	uint16_t idx = 0;

	/* Find a port belong to pf that not be called dev_close */
	for (idx = 0; idx < RNP_MAX_PORT_OF_PF; idx++) {
		if (idx)
			snprintf(device_name, sizeof(device_name), "%s_%d",
					pci_dev->device.name,
					idx);
		else
			snprintf(device_name, sizeof(device_name), "%s",
					pci_dev->device.name);
		eth_dev = rte_eth_dev_allocated(device_name);
		if (eth_dev)
			break;
	}
	if (eth_dev)
		/* Cleanup eth dev */
		return rnp_eth_dev_uninit(eth_dev);
	return -ENODEV;
}

static int
rnp_pci_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	int rc;

	RTE_SET_USED(pci_drv);

	rc = rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct rnp_eth_port),
			rnp_eth_dev_init);

	return rc;
}

static const struct rte_pci_id pci_id_rnp_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_MUCSE, RNP_DEV_ID_N10G_X2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_MUCSE, RNP_DEV_ID_N10G_X4) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_MUCSE, RNP_DEV_ID_N10G_X8) },
	{ .vendor_id = 0, },
};

static struct rte_pci_driver rte_rnp_pmd = {
	.id_table = pci_id_rnp_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = rnp_pci_probe,
	.remove = rnp_pci_remove,
};

RTE_LOG_REGISTER_SUFFIX(rnp_init_logtype, init, NOTICE);

RTE_PMD_REGISTER_PCI(net_rnp, rte_rnp_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_rnp, pci_id_rnp_map);
RTE_PMD_REGISTER_KMOD_DEP(net_rnp, "igb_uio | uio_pci_generic | vfio-pci");
