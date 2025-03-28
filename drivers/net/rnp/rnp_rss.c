/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include <stdint.h>

#include "base/rnp_bdq_if.h"
#include "base/rnp_eth_regs.h"

#include "rnp.h"
#include "rnp_rxtx.h"
#include "rnp_rss.h"

static const struct rnp_rss_hash_cfg rnp_rss_cfg[] = {
	{RNP_RSS_IPV4, RNP_RSS_HASH_IPV4, RTE_ETH_RSS_IPV4},
	{RNP_RSS_IPV4, RNP_RSS_HASH_IPV4, RTE_ETH_RSS_FRAG_IPV4},
	{RNP_RSS_IPV4, RNP_RSS_HASH_IPV4, RTE_ETH_RSS_NONFRAG_IPV4_OTHER},
	{RNP_RSS_IPV6, RNP_RSS_HASH_IPV6, RTE_ETH_RSS_IPV6},
	{RNP_RSS_IPV6, RNP_RSS_HASH_IPV6, RTE_ETH_RSS_FRAG_IPV6},
	{RNP_RSS_IPV6, RNP_RSS_HASH_IPV6, RTE_ETH_RSS_NONFRAG_IPV6_OTHER},
	{RNP_RSS_IPV4_TCP, RNP_RSS_HASH_IPV4_TCP, RTE_ETH_RSS_NONFRAG_IPV4_TCP},
	{RNP_RSS_IPV4_UDP, RNP_RSS_HASH_IPV4_UDP, RTE_ETH_RSS_NONFRAG_IPV4_UDP},
	{RNP_RSS_IPV4_SCTP, RNP_RSS_HASH_IPV4_SCTP, RTE_ETH_RSS_NONFRAG_IPV4_SCTP},
	{RNP_RSS_IPV6_TCP, RNP_RSS_HASH_IPV6_TCP, RTE_ETH_RSS_NONFRAG_IPV6_TCP},
	{RNP_RSS_IPV6_UDP, RNP_RSS_HASH_IPV6_UDP, RTE_ETH_RSS_NONFRAG_IPV6_UDP},
	{RNP_RSS_IPV6_SCTP, RNP_RSS_HASH_IPV6_SCTP, RTE_ETH_RSS_NONFRAG_IPV6_SCTP}
};

static uint8_t rnp_rss_default_key[40] = {
	0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
	0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
	0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
	0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
	0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA,
};

int
rnp_dev_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t port_offset = port->attr.port_offset;
	uint32_t *indirtbl = &port->indirtbl[0];
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	struct rnp_rx_queue *rxq;
	uint16_t i, idx, shift;
	uint16_t hwrid;
	uint16_t qid = 0;

	if (reta_size > RNP_RSS_INDIR_SIZE) {
		RNP_PMD_ERR("Invalid reta size, reta_size:%d", reta_size);
		return -EINVAL;
	}
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			indirtbl[i] = reta_conf[idx].reta[shift];
	}
	for (i = 0; i < RNP_RSS_INDIR_SIZE; i++) {
		qid = indirtbl[i];
		if (qid < dev->data->nb_rx_queues) {
			rxq = dev->data->rx_queues[qid];
			hwrid = rxq->attr.index - port_offset;
			RNP_E_REG_WR(hw, RNP_RSS_REDIR_TB(lane, i), hwrid);
			rxq->rx_offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
		} else {
			RNP_PMD_WARN("port[%d] reta[%d]-queue=%d "
					"rx queue is out range of cur Settings",
					dev->data->port_id, i, qid);
		}
	}
	port->reta_has_cfg = true;

	return 0;
}

static uint16_t
rnp_hwrid_to_queue_id(struct rte_eth_dev *dev, uint16_t hwrid)
{
	struct rnp_rx_queue *rxq;
	bool find = false;
	uint16_t idx;

	for (idx = 0; idx < dev->data->nb_rx_queues; idx++) {
		rxq = dev->data->rx_queues[idx];
		if (!rxq)
			continue;
		if (rxq->attr.index == hwrid) {
			find = true;
			break;
		}
	}
	if (find)
		return rxq->attr.queue_id;

	return UINT16_MAX;
}

int
rnp_dev_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t port_offset = port->attr.port_offset;
	struct rnp_hw *hw = port->hw;
	uint32_t *indirtbl = &port->indirtbl[0];
	uint16_t lane = port->attr.nr_lane;
	uint16_t i, idx, shift;
	uint16_t hwrid;
	uint16_t queue_id;

	if (reta_size > RNP_RSS_INDIR_SIZE) {
		RNP_PMD_ERR("Invalid reta size, reta_size:%d", reta_size);
		return -EINVAL;
	}
	for (i = 0; i < reta_size; i++) {
		hwrid = RNP_E_REG_RD(hw, RNP_RSS_REDIR_TB(lane, i));
		hwrid = hwrid + port_offset;
		queue_id = rnp_hwrid_to_queue_id(dev, hwrid);
		if (queue_id == UINT16_MAX) {
			RNP_PMD_ERR("Invalid rss-table value is the"
					" Sw-queue not Match Hardware?");
			return -EINVAL;
		}
		indirtbl[i] = queue_id;
	}
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = (uint16_t)indirtbl[i];
	}

	return 0;
}

static void rnp_disable_rss(struct rte_eth_dev *dev)
{
	struct rnp_eth_adapter *adapter = RNP_DEV_TO_ADAPTER(dev);
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rte_eth_rss_conf *conf = &port->rss_conf;
	struct rnp_rx_queue *rxq = NULL;
	struct rnp_hw *hw = port->hw;
	uint8_t rss_disable = 0;
	uint32_t mrqc_reg = 0;
	uint16_t lane, index;
	uint16_t idx;

	memset(conf, 0, sizeof(*conf));
	lane = port->attr.nr_lane;
	for (idx = 0; idx < hw->max_port_num; idx++) {
		if (adapter->ports[idx] == NULL) {
			rss_disable++;
			continue;
		}
		if (!adapter->ports[idx]->rss_conf.rss_hf)
			rss_disable++;
	}

	for (idx = 0; idx < dev->data->nb_rx_queues; idx++) {
		rxq = dev->data->rx_queues[idx];
		if (!rxq)
			continue;
		rxq->rx_offloads &= ~RTE_ETH_RX_OFFLOAD_RSS_HASH;
	}
	/* we use software way to achieve multiple port mode
	 * rss feature disable by set RSS table to default ring.
	 * So when re enable RSS,the rss reta table need to set
	 * last user set State
	 */
	rxq = dev->data->rx_queues[0];
	index = rxq->attr.index - port->attr.port_offset;
	for (idx = 0; idx < RNP_RSS_INDIR_SIZE; idx++)
		RNP_E_REG_WR(hw, RNP_RSS_REDIR_TB(lane, idx), index);
	if (rss_disable == hw->max_port_num) {
		mrqc_reg = RNP_E_REG_RD(hw, RNP_RSS_MRQC_ADDR);
		mrqc_reg &= ~RNP_RSS_HASH_CFG_MASK;
		RNP_E_REG_WR(hw, RNP_RSS_MRQC_ADDR, mrqc_reg);
	}
}

static void
rnp_rss_hash_set(struct rte_eth_dev *dev, struct rte_eth_rss_conf *rss_conf)
{
	uint64_t rss_hash_level = RTE_ETH_RSS_LEVEL(rss_conf->rss_hf);
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_rx_queue *rxq = NULL;
	struct rnp_hw *hw = port->hw;
	uint8_t *hash_key;
	uint32_t mrqc_reg = 0;
	uint32_t rss_key;
	uint64_t rss_hf;
	uint16_t i;

	rss_hf = rss_conf->rss_hf;
	hash_key = rss_conf->rss_key;
	if (hash_key != NULL) {
		for (i = 0; i < RNP_MAX_HASH_KEY_SIZE; i++) {
			rss_key  = hash_key[(i * 4)];
			rss_key |= hash_key[(i * 4) + 1] << 8;
			rss_key |= hash_key[(i * 4) + 2] << 16;
			rss_key |= hash_key[(i * 4) + 3] << 24;
			rss_key = rte_cpu_to_be_32(rss_key);
			RNP_E_REG_WR(hw, RNP_RSS_KEY_TABLE(9 - i), rss_key);
		}
	}
	if (rss_hf) {
		for (i = 0; i < RTE_DIM(rnp_rss_cfg); i++)
			if (rnp_rss_cfg[i].rss_flag & rss_hf)
				mrqc_reg |= rnp_rss_cfg[i].reg_val;
		/* Enable inner rss mode
		 * If enable, outer(vxlan/nvgre) rss won't cals
		 */
		if (rss_hash_level == RNP_RSS_LEVEL_INNER)
			RNP_E_REG_WR(hw, RNP_RSS_INNER_CTRL, RNP_INNER_RSS_EN);
		else
			RNP_E_REG_WR(hw, RNP_RSS_INNER_CTRL, RNP_INNER_RSS_DIS);
		RNP_E_REG_WR(hw, RNP_RSS_MRQC_ADDR, mrqc_reg);
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			rxq = dev->data->rx_queues[i];
			if (!rxq)
				continue;
			rxq->rx_offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
		}
	}
}

static void
rnp_reta_table_update(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t port_offset = port->attr.port_offset;
	uint32_t *indirtbl = &port->indirtbl[0];
	struct rnp_hw *hw = port->hw;
	struct rnp_rx_queue *rxq;
	int i = 0, qid = 0, p_id;
	uint16_t hwrid;

	p_id = port->attr.nr_lane;
	for (i = 0; i < RNP_RSS_INDIR_SIZE; i++) {
		qid = indirtbl[i];
		if (qid < dev->data->nb_rx_queues) {
			rxq = dev->data->rx_queues[qid];
			hwrid = rxq->attr.index - port_offset;
			RNP_E_REG_WR(hw, RNP_RSS_REDIR_TB(p_id, i), hwrid);
			rxq->rx_offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
		} else {
			RNP_PMD_LOG(WARNING, "port[%d] reta[%d]-queue=%d "
					"rx queues is out range of cur set",
					dev->data->port_id, i, qid);
		}
	}
}

int
rnp_dev_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);

	if (rss_conf->rss_key &&
			rss_conf->rss_key_len > RNP_MAX_HASH_KEY_SIZE) {
		RNP_PMD_ERR("Invalid rss key, rss_key_len:%d",
				rss_conf->rss_key_len);
		return -EINVAL;
	}
	if (rss_conf->rss_hf &&
			(!(rss_conf->rss_hf & RNP_SUPPORT_RSS_OFFLOAD_ALL))) {
		RNP_PMD_ERR("RSS type don't support");
		return -EINVAL;
	}
	if (!rss_conf->rss_hf) {
		rnp_disable_rss(dev);
	} else {
		rnp_rss_hash_set(dev, rss_conf);
		rnp_reta_table_update(dev);
	}
	port->rss_conf = *rss_conf;

	return 0;
}

int
rnp_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_hw *hw = port->hw;
	uint8_t *hash_key;
	uint32_t rss_key;
	uint64_t rss_hf;
	uint32_t mrqc;
	uint16_t i;

	hash_key = rss_conf->rss_key;
	if (hash_key != NULL) {
		for (i = 0; i < 10; i++) {
			rss_key = RNP_E_REG_RD(hw, RNP_RSS_KEY_TABLE(9 - i));
			rss_key = rte_be_to_cpu_32(rss_key);
			hash_key[(i * 4)] = rss_key & 0x000000FF;
			hash_key[(i * 4) + 1] = (rss_key >> 8) & 0x000000FF;
			hash_key[(i * 4) + 2] = (rss_key >> 16) & 0x000000FF;
			hash_key[(i * 4) + 3] = (rss_key >> 24) & 0x000000FF;
		}
	}
	rss_hf = 0;
	mrqc = RNP_E_REG_RD(hw, RNP_RSS_MRQC_ADDR) & RNP_RSS_HASH_CFG_MASK;
	if (mrqc == 0) {
		rss_conf->rss_hf = 0;
		return 0;
	}
	for (i = 0; i < RTE_DIM(rnp_rss_cfg); i++)
		if (rnp_rss_cfg[i].reg_val & mrqc)
			rss_hf |= rnp_rss_cfg[i].rss_flag;

	rss_conf->rss_hf = rss_hf;

	return 0;
}

int rnp_dev_rss_configure(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint32_t *indirtbl = port->indirtbl;
	enum rte_eth_rx_mq_mode mq_mode = 0;
	struct rte_eth_rss_conf rss_conf;
	struct rnp_rx_queue *rxq;
	int i, j;

	mq_mode = dev->data->dev_conf.rxmode.mq_mode;
	if (dev->data->rx_queues == NULL) {
		RNP_PMD_ERR("rx_queue is not setup skip rss set");
		return -EINVAL;
	}
	rss_conf = dev->data->dev_conf.rx_adv_conf.rss_conf;
	if (!(rss_conf.rss_hf & RNP_SUPPORT_RSS_OFFLOAD_ALL) ||
			!(mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)) {
		rnp_disable_rss(dev);

		return 0;
	}
	if (rss_conf.rss_key == NULL)
		rss_conf.rss_key = rnp_rss_default_key;

	if (port->rxq_num_changed || !port->reta_has_cfg) {
		/* set default reta policy */
		for (i = 0; i < RNP_RSS_INDIR_SIZE; i++) {
			j = i % dev->data->nb_rx_queues;
			rxq = dev->data->rx_queues[j];
			if (!rxq) {
				RNP_PMD_ERR("rss Set reta-cfg rxq %d Is NULL", i);
				return -EINVAL;
			}
			indirtbl[i] = rxq->attr.queue_id;
		}
	}
	rnp_reta_table_update(dev);
	port->rss_conf = rss_conf;
	/* setup rss key and hash func */
	rnp_rss_hash_set(dev, &rss_conf);

	return 0;
}
