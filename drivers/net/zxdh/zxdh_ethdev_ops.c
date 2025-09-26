/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <rte_malloc.h>
#include <rte_ether.h>

#include "zxdh_ethdev.h"
#include "zxdh_pci.h"
#include "zxdh_msg.h"
#include "zxdh_ethdev_ops.h"
#include "zxdh_tables.h"
#include "zxdh_logs.h"
#include "zxdh_rxtx.h"
#include "zxdh_np.h"
#include "zxdh_queue.h"
#include "zxdh_mtr.h"
#include "zxdh_common.h"

#define ZXDH_VLAN_FILTER_GROUPS       64
#define ZXDH_INVALID_LOGIC_QID        0xFFFFU

/* Supported RSS */
#define ZXDH_RSS_HF_MASK     (~(ZXDH_RSS_HF))
#define ZXDH_HF_F5           1
#define ZXDH_HF_F3           2
#define ZXDH_HF_MAC_VLAN     4
#define ZXDH_HF_ALL          0

struct zxdh_hw_mac_stats {
	uint64_t rx_total;
	uint64_t rx_pause;
	uint64_t rx_unicast;
	uint64_t rx_multicast;
	uint64_t rx_broadcast;
	uint64_t rx_vlan;
	uint64_t rx_size_64;
	uint64_t rx_size_65_127;
	uint64_t rx_size_128_255;
	uint64_t rx_size_256_511;
	uint64_t rx_size_512_1023;
	uint64_t rx_size_1024_1518;
	uint64_t rx_size_1519_mru;
	uint64_t rx_undersize;
	uint64_t rx_oversize;
	uint64_t rx_fragment;
	uint64_t rx_jabber;
	uint64_t rx_control;
	uint64_t rx_eee;

	uint64_t tx_total;
	uint64_t tx_pause;
	uint64_t tx_unicast;
	uint64_t tx_multicast;
	uint64_t tx_broadcast;
	uint64_t tx_vlan;
	uint64_t tx_size_64;
	uint64_t tx_size_65_127;
	uint64_t tx_size_128_255;
	uint64_t tx_size_256_511;
	uint64_t tx_size_512_1023;
	uint64_t tx_size_1024_1518;
	uint64_t tx_size_1519_mtu;
	uint64_t tx_undersize;
	uint64_t tx_oversize;
	uint64_t tx_fragment;
	uint64_t tx_jabber;
	uint64_t tx_control;
	uint64_t tx_eee;

	uint64_t rx_error;
	uint64_t rx_fcs_error;
	uint64_t rx_drop;

	uint64_t tx_error;
	uint64_t tx_fcs_error;
	uint64_t tx_drop;

};

struct zxdh_hw_mac_bytes {
	uint64_t rx_total_bytes;
	uint64_t rx_good_bytes;
	uint64_t tx_total_bytes;
	uint64_t tx_good_bytes;
};

struct rte_zxdh_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

static const struct rte_zxdh_xstats_name_off zxdh_rxq_stat_strings[] = {
	{"good_packets",           offsetof(struct zxdh_virtnet_rx, stats.packets)},
	{"good_bytes",             offsetof(struct zxdh_virtnet_rx, stats.bytes)},
	{"errors",                 offsetof(struct zxdh_virtnet_rx, stats.errors)},
	{"idle",                   offsetof(struct zxdh_virtnet_rx, stats.idle)},
	{"full",                   offsetof(struct zxdh_virtnet_rx, stats.full)},
	{"norefill",               offsetof(struct zxdh_virtnet_rx, stats.norefill)},
	{"multicast_packets",      offsetof(struct zxdh_virtnet_rx, stats.multicast)},
	{"broadcast_packets",      offsetof(struct zxdh_virtnet_rx, stats.broadcast)},
	{"truncated_err",          offsetof(struct zxdh_virtnet_rx, stats.truncated_err)},
	{"offload_cfg_err",        offsetof(struct zxdh_virtnet_rx, stats.offload_cfg_err)},
	{"invalid_hdr_len_err",    offsetof(struct zxdh_virtnet_rx, stats.invalid_hdr_len_err)},
	{"no_segs_err",            offsetof(struct zxdh_virtnet_rx, stats.no_segs_err)},
	{"undersize_packets",      offsetof(struct zxdh_virtnet_rx, stats.size_bins[0])},
	{"size_64_packets",        offsetof(struct zxdh_virtnet_rx, stats.size_bins[1])},
	{"size_65_127_packets",    offsetof(struct zxdh_virtnet_rx, stats.size_bins[2])},
	{"size_128_255_packets",   offsetof(struct zxdh_virtnet_rx, stats.size_bins[3])},
	{"size_256_511_packets",   offsetof(struct zxdh_virtnet_rx, stats.size_bins[4])},
	{"size_512_1023_packets",  offsetof(struct zxdh_virtnet_rx, stats.size_bins[5])},
	{"size_1024_1518_packets", offsetof(struct zxdh_virtnet_rx, stats.size_bins[6])},
	{"size_1519_max_packets",  offsetof(struct zxdh_virtnet_rx, stats.size_bins[7])},
};

static const struct rte_zxdh_xstats_name_off zxdh_txq_stat_strings[] = {
	{"good_packets",           offsetof(struct zxdh_virtnet_tx, stats.packets)},
	{"good_bytes",             offsetof(struct zxdh_virtnet_tx, stats.bytes)},
	{"errors",                 offsetof(struct zxdh_virtnet_tx, stats.errors)},
	{"idle",                   offsetof(struct zxdh_virtnet_tx, stats.idle)},
	{"norefill",               offsetof(struct zxdh_virtnet_tx, stats.norefill)},
	{"multicast_packets",      offsetof(struct zxdh_virtnet_tx, stats.multicast)},
	{"broadcast_packets",      offsetof(struct zxdh_virtnet_tx, stats.broadcast)},
	{"truncated_err",          offsetof(struct zxdh_virtnet_tx, stats.truncated_err)},
	{"offload_cfg_err",        offsetof(struct zxdh_virtnet_tx, stats.offload_cfg_err)},
	{"invalid_hdr_len_err",    offsetof(struct zxdh_virtnet_tx, stats.invalid_hdr_len_err)},
	{"no_segs_err",            offsetof(struct zxdh_virtnet_tx, stats.no_segs_err)},
	{"undersize_packets",      offsetof(struct zxdh_virtnet_tx, stats.size_bins[0])},
	{"size_64_packets",        offsetof(struct zxdh_virtnet_tx, stats.size_bins[1])},
	{"size_65_127_packets",    offsetof(struct zxdh_virtnet_tx, stats.size_bins[2])},
	{"size_128_255_packets",   offsetof(struct zxdh_virtnet_tx, stats.size_bins[3])},
	{"size_256_511_packets",   offsetof(struct zxdh_virtnet_tx, stats.size_bins[4])},
	{"size_512_1023_packets",  offsetof(struct zxdh_virtnet_tx, stats.size_bins[5])},
	{"size_1024_1518_packets", offsetof(struct zxdh_virtnet_tx, stats.size_bins[6])},
	{"size_1519_max_packets",  offsetof(struct zxdh_virtnet_tx, stats.size_bins[7])},
};

static const struct rte_zxdh_xstats_name_off zxdh_np_stat_strings[] = {
	{"np_rx_unicast_pkts",     offsetof(struct zxdh_hw_np_stats, rx_unicast_pkts)},
	{"np_tx_unicast_pkts",     offsetof(struct zxdh_hw_np_stats, tx_unicast_pkts)},
	{"np_rx_unicast_bytes",    offsetof(struct zxdh_hw_np_stats, rx_unicast_bytes)},
	{"np_tx_unicast_bytes",    offsetof(struct zxdh_hw_np_stats, tx_unicast_bytes)},
	{"np_rx_multicast_pkts",   offsetof(struct zxdh_hw_np_stats, rx_multicast_pkts)},
	{"np_tx_multicast_pkts",   offsetof(struct zxdh_hw_np_stats, tx_multicast_pkts)},
	{"np_rx_multicast_bytes",  offsetof(struct zxdh_hw_np_stats, rx_multicast_bytes)},
	{"np_tx_multicast_bytes",  offsetof(struct zxdh_hw_np_stats, tx_multicast_bytes)},
	{"np_rx_broadcast_pkts",   offsetof(struct zxdh_hw_np_stats, rx_broadcast_pkts)},
	{"np_tx_broadcast_pkts",   offsetof(struct zxdh_hw_np_stats, tx_broadcast_pkts)},
	{"np_rx_broadcast_bytes",  offsetof(struct zxdh_hw_np_stats, rx_broadcast_bytes)},
	{"np_tx_broadcast_bytes",  offsetof(struct zxdh_hw_np_stats, tx_broadcast_bytes)},
	{"np_rx_mtu_drop_pkts",    offsetof(struct zxdh_hw_np_stats, rx_mtu_drop_pkts)},
	{"np_tx_mtu_drop_pkts",    offsetof(struct zxdh_hw_np_stats, tx_mtu_drop_pkts)},
	{"np_tx_mtu_drop_bytes",   offsetof(struct zxdh_hw_np_stats, tx_mtu_drop_bytes)},
	{"np_rx_mtu_drop_bytes",   offsetof(struct zxdh_hw_np_stats, rx_mtu_drop_bytes)},
	{"np_rx_plcr_drop_pkts",   offsetof(struct zxdh_hw_np_stats, rx_mtr_drop_pkts)},
	{"np_rx_plcr_drop_bytes",  offsetof(struct zxdh_hw_np_stats, rx_mtr_drop_bytes)},
	{"np_tx_plcr_drop_pkts",   offsetof(struct zxdh_hw_np_stats, tx_mtr_drop_pkts)},
	{"np_tx_plcr_drop_bytes",  offsetof(struct zxdh_hw_np_stats, tx_mtr_drop_bytes)},
};

static const struct rte_zxdh_xstats_name_off zxdh_mac_stat_strings[] = {
	{"mac_rx_total",           offsetof(struct zxdh_hw_mac_stats, rx_total)},
	{"mac_rx_pause",           offsetof(struct zxdh_hw_mac_stats, rx_pause)},
	{"mac_rx_unicast",         offsetof(struct zxdh_hw_mac_stats, rx_unicast)},
	{"mac_rx_multicast",       offsetof(struct zxdh_hw_mac_stats, rx_multicast)},
	{"mac_rx_broadcast",       offsetof(struct zxdh_hw_mac_stats, rx_broadcast)},
	{"mac_rx_vlan",            offsetof(struct zxdh_hw_mac_stats, rx_vlan)},
	{"mac_rx_size_64",         offsetof(struct zxdh_hw_mac_stats, rx_size_64)},
	{"mac_rx_size_65_127",     offsetof(struct zxdh_hw_mac_stats, rx_size_65_127)},
	{"mac_rx_size_128_255",    offsetof(struct zxdh_hw_mac_stats, rx_size_128_255)},
	{"mac_rx_size_256_511",    offsetof(struct zxdh_hw_mac_stats, rx_size_256_511)},
	{"mac_rx_size_512_1023",   offsetof(struct zxdh_hw_mac_stats, rx_size_512_1023)},
	{"mac_rx_size_1024_1518",  offsetof(struct zxdh_hw_mac_stats, rx_size_1024_1518)},
	{"mac_rx_size_1519_mru",   offsetof(struct zxdh_hw_mac_stats, rx_size_1519_mru)},
	{"mac_rx_undersize",       offsetof(struct zxdh_hw_mac_stats, rx_undersize)},
	{"mac_rx_oversize",        offsetof(struct zxdh_hw_mac_stats, rx_oversize)},
	{"mac_rx_fragment",        offsetof(struct zxdh_hw_mac_stats, rx_fragment)},
	{"mac_rx_jabber",          offsetof(struct zxdh_hw_mac_stats, rx_jabber)},
	{"mac_rx_control",         offsetof(struct zxdh_hw_mac_stats, rx_control)},
	{"mac_rx_eee",             offsetof(struct zxdh_hw_mac_stats, rx_eee)},
	{"mac_rx_error",           offsetof(struct zxdh_hw_mac_stats, rx_error)},
	{"mac_rx_fcs_error",       offsetof(struct zxdh_hw_mac_stats, rx_fcs_error)},
	{"mac_rx_drop",            offsetof(struct zxdh_hw_mac_stats, rx_drop)},

	{"mac_tx_total",           offsetof(struct zxdh_hw_mac_stats, tx_total)},
	{"mac_tx_pause",           offsetof(struct zxdh_hw_mac_stats, tx_pause)},
	{"mac_tx_unicast",         offsetof(struct zxdh_hw_mac_stats, tx_unicast)},
	{"mac_tx_multicast",       offsetof(struct zxdh_hw_mac_stats, tx_multicast)},
	{"mac_tx_broadcast",       offsetof(struct zxdh_hw_mac_stats, tx_broadcast)},
	{"mac_tx_vlan",            offsetof(struct zxdh_hw_mac_stats, tx_vlan)},
	{"mac_tx_size_64",         offsetof(struct zxdh_hw_mac_stats, tx_size_64)},
	{"mac_tx_size_65_127",     offsetof(struct zxdh_hw_mac_stats, tx_size_65_127)},
	{"mac_tx_size_128_255",    offsetof(struct zxdh_hw_mac_stats, tx_size_128_255)},
	{"mac_tx_size_256_511",    offsetof(struct zxdh_hw_mac_stats, tx_size_256_511)},
	{"mac_tx_size_512_1023",   offsetof(struct zxdh_hw_mac_stats, tx_size_512_1023)},
	{"mac_tx_size_1024_1518",  offsetof(struct zxdh_hw_mac_stats, tx_size_1024_1518)},
	{"mac_tx_size_1519_mtu",   offsetof(struct zxdh_hw_mac_stats, tx_size_1519_mtu)},
	{"mac_tx_undersize",       offsetof(struct zxdh_hw_mac_stats, tx_undersize)},
	{"mac_tx_oversize",        offsetof(struct zxdh_hw_mac_stats, tx_oversize)},
	{"mac_tx_fragment",        offsetof(struct zxdh_hw_mac_stats, tx_fragment)},
	{"mac_tx_jabber",          offsetof(struct zxdh_hw_mac_stats, tx_jabber)},
	{"mac_tx_control",         offsetof(struct zxdh_hw_mac_stats, tx_control)},
	{"mac_tx_eee",             offsetof(struct zxdh_hw_mac_stats, tx_eee)},
	{"mac_tx_error",           offsetof(struct zxdh_hw_mac_stats, tx_error)},
	{"mac_tx_fcs_error",       offsetof(struct zxdh_hw_mac_stats, tx_fcs_error)},
	{"mac_tx_drop",            offsetof(struct zxdh_hw_mac_stats, tx_drop)},
};

static const struct rte_zxdh_xstats_name_off zxdh_mac_bytes_strings[] = {
	{"mac_rx_total_bytes",      offsetof(struct zxdh_hw_mac_bytes, rx_total_bytes)},
	{"mac_rx_good_bytes",       offsetof(struct zxdh_hw_mac_bytes, rx_good_bytes)},
	{"mac_tx_total_bytes",      offsetof(struct zxdh_hw_mac_bytes, tx_total_bytes)},
	{"mac_tx_good_bytes",       offsetof(struct zxdh_hw_mac_bytes, tx_good_bytes)},
};

static const struct rte_zxdh_xstats_name_off zxdh_vqm_stat_strings[] = {
	{"vqm_rx_vport_packets",    offsetof(struct zxdh_hw_vqm_stats, rx_total)},
	{"vqm_tx_vport_packets",    offsetof(struct zxdh_hw_vqm_stats, tx_total)},
	{"vqm_rx_vport_bytes",      offsetof(struct zxdh_hw_vqm_stats, rx_bytes)},
	{"vqm_tx_vport_bytes",      offsetof(struct zxdh_hw_vqm_stats, tx_bytes)},
	{"vqm_rx_vport_dropped",    offsetof(struct zxdh_hw_vqm_stats, rx_drop)},
};

#define ZXDH_NB_RXQ_XSTATS (sizeof(zxdh_rxq_stat_strings) / \
			sizeof(zxdh_rxq_stat_strings[0]))
#define ZXDH_NB_TXQ_XSTATS (sizeof(zxdh_txq_stat_strings) / \
			sizeof(zxdh_txq_stat_strings[0]))

#define ZXDH_NP_XSTATS (sizeof(zxdh_np_stat_strings) / \
			sizeof(zxdh_np_stat_strings[0]))

#define ZXDH_MAC_XSTATS (sizeof(zxdh_mac_stat_strings) / \
			sizeof(zxdh_mac_stat_strings[0]))

#define ZXDH_MAC_BYTES (sizeof(zxdh_mac_bytes_strings) / \
			sizeof(zxdh_mac_bytes_strings[0]))

#define ZXDH_VQM_XSTATS (sizeof(zxdh_vqm_stat_strings) / \
			sizeof(zxdh_vqm_stat_strings[0]))

static int32_t zxdh_config_port_status(struct rte_eth_dev *dev, uint16_t link_status)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_port_attr_table port_attr = {0};
	struct zxdh_msg_info msg_info = {0};
	int32_t ret = 0;

	if (hw->is_pf) {
		ret = zxdh_get_port_attr(hw, hw->vport.vport, &port_attr);
		if (ret) {
			PMD_DRV_LOG(ERR, "get port_attr failed");
			return -1;
		}
		port_attr.is_up = link_status;

		ret = zxdh_set_port_attr(hw, hw->vport.vport, &port_attr);
		if (ret) {
			PMD_DRV_LOG(ERR, "write port_attr failed");
			return -1;
		}
	} else {
		struct zxdh_port_attr_set_msg *port_attr_msg = &msg_info.data.port_attr_msg;

		zxdh_msg_head_build(hw, ZXDH_PORT_ATTRS_SET, &msg_info);
		port_attr_msg->mode = ZXDH_PORT_VPORT_IS_UP_FLAG;
		port_attr_msg->value = link_status;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
				hw->vport.vport, ZXDH_PORT_VPORT_IS_UP_FLAG);
			return ret;
		}
	}
	return ret;
}

static int32_t
zxdh_link_info_get(struct rte_eth_dev *dev, struct rte_eth_link *link)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	uint16_t status = 0;
	int32_t ret = 0;
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	void *link_msg_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, link_msg);

	if (zxdh_pci_with_feature(hw, ZXDH_NET_F_STATUS))
		zxdh_pci_read_dev_config(hw, offsetof(struct zxdh_net_config, status),
					&status, sizeof(status));

	link->link_status = status;

	if (status == RTE_ETH_LINK_DOWN) {
		link->link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
		link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	} else {
		zxdh_agent_msg_build(hw, ZXDH_MAC_LINK_GET, &msg_info);

		ret = zxdh_send_msg_to_riscv(dev, &msg_info, sizeof(struct zxdh_msg_info),
				zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info),
				ZXDH_BAR_MODULE_MAC);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
					hw->vport.vport, ZXDH_MAC_LINK_GET);
			return -1;
		}

		link->link_speed = ZXDH_GET(link_info_msg, link_msg_addr, speed);
		link->link_autoneg = ZXDH_GET(link_info_msg, link_msg_addr, autoneg);
		hw->speed_mode = ZXDH_GET(link_info_msg, link_msg_addr, speed_modes);
		if ((ZXDH_GET(link_info_msg, link_msg_addr, duplex) & RTE_ETH_LINK_FULL_DUPLEX) ==
				RTE_ETH_LINK_FULL_DUPLEX)
			link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		else
			link->link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
	}

	if (hw->switchoffload) {
		link->link_speed = RTE_ETH_SPEED_NUM_25G;
		link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		link->link_autoneg = RTE_ETH_LINK_AUTONEG;
		link->link_status = RTE_ETH_LINK_UP;
	}
	hw->speed = link->link_speed;

	return 0;
}

static int zxdh_set_link_status(struct rte_eth_dev *dev, uint8_t link_status)
{
	uint16_t curr_link_status = dev->data->dev_link.link_status;

	struct rte_eth_link link;
	struct zxdh_hw *hw = dev->data->dev_private;
	int32_t ret = 0;

	if (link_status == curr_link_status) {
		PMD_DRV_LOG(DEBUG, "curr_link_status %u", curr_link_status);
		return 0;
	}

	hw->admin_status = link_status;
	ret = zxdh_link_info_get(dev, &link);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to get link status from hw");
		return ret;
	}
	dev->data->dev_link.link_status = hw->admin_status & link.link_status;

	if (dev->data->dev_link.link_status == RTE_ETH_LINK_UP) {
		dev->data->dev_link.link_speed = link.link_speed;
		dev->data->dev_link.link_duplex = link.link_duplex;
	} else {
		dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
		dev->data->dev_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	}
	return zxdh_config_port_status(dev, dev->data->dev_link.link_status);
}

int zxdh_dev_set_link_up(struct rte_eth_dev *dev)
{
	int ret = zxdh_set_link_status(dev, RTE_ETH_LINK_UP);

	if (ret)
		PMD_DRV_LOG(ERR, "Set link up failed, code:%d", ret);

	return ret;
}

int32_t zxdh_dev_link_update(struct rte_eth_dev *dev, int32_t wait_to_complete __rte_unused)
{
	struct rte_eth_link link;
	struct zxdh_hw *hw = dev->data->dev_private;
	int32_t ret = 0;

	memset(&link, 0, sizeof(link));
	link.link_duplex = hw->duplex;
	link.link_speed  = hw->speed;
	link.link_autoneg = RTE_ETH_LINK_AUTONEG;

	ret = zxdh_link_info_get(dev, &link);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to get link status from hw");
		return ret;
	}
	link.link_status &= hw->admin_status;
	if (link.link_status == RTE_ETH_LINK_DOWN) {
		PMD_DRV_LOG(DEBUG, "dev link status is down.");
		goto link_down;
	}
	goto out;

link_down:
	link.link_status = RTE_ETH_LINK_DOWN;
	link.link_speed  = RTE_ETH_SPEED_NUM_UNKNOWN;
out:
	if (link.link_status != dev->data->dev_link.link_status) {
		ret = zxdh_config_port_status(dev, link.link_status);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "set port attr %d failed", link.link_status);
			return ret;
		}
	}
	return rte_eth_linkstatus_set(dev, &link);
}

int zxdh_dev_set_link_down(struct rte_eth_dev *dev)
{
	int ret = zxdh_set_link_status(dev, RTE_ETH_LINK_DOWN);

	if (ret)
		PMD_DRV_LOG(ERR, "Set link down failed");
	return ret;
}

int
zxdh_dev_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *addr)
{
	struct zxdh_hw *hw = (struct zxdh_hw *)dev->data->dev_private;
	struct rte_ether_addr *old_addr = &dev->data->mac_addrs[0];
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	void *mac_reply_msg_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, mac_reply_msg);
	int ret = 0;

	if (!rte_is_valid_assigned_ether_addr(addr)) {
		PMD_DRV_LOG(ERR, "mac address is invalid!");
		return -EINVAL;
	}
	if (rte_is_same_ether_addr(old_addr, addr))
		return 0;

	if (hw->is_pf) {
		ret = zxdh_add_mac_table(hw, hw->vport.vport, addr, hw->hash_search_index, 0, 0);
		if (ret) {
			if (ret == -EADDRINUSE) {
				PMD_DRV_LOG(ERR, "pf mac add failed! mac is in used, code:%d", ret);
				return -EADDRINUSE;
			}
			PMD_DRV_LOG(ERR, "mac_addr_add failed, code:%d", ret);
			return ret;
		}
		hw->uc_num++;

		ret = zxdh_del_mac_table(hw, hw->vport.vport, old_addr,
			hw->hash_search_index, 0, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "mac_addr_del failed, code:%d", ret);
			return ret;
		}
		hw->uc_num--;
	} else {
		struct zxdh_mac_filter *mac_filter = &msg_info.data.mac_filter_msg;
		mac_filter->filter_flag = ZXDH_MAC_UNFILTER;
		mac_filter->mac = *addr;
		zxdh_msg_head_build(hw, ZXDH_MAC_ADD, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info),
				zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info));
		if (ret) {
			uint8_t flag = ZXDH_GET(mac_reply_msg, mac_reply_msg_addr, mac_flag);
			if (flag == ZXDH_EEXIST_MAC_FLAG) {
				PMD_DRV_LOG(ERR, "pf mac add failed! mac is in used, code:%d", ret);
				return -EADDRINUSE;
			}
			PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
				hw->vport.vport, ZXDH_MAC_ADD);
			return ret;
		}
		hw->uc_num++;

		mac_filter->filter_flag = ZXDH_MAC_UNFILTER;
		mac_filter->mac_flag = true;
		mac_filter->mac = *old_addr;
		zxdh_msg_head_build(hw, ZXDH_MAC_DEL, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
				hw->vport.vport, ZXDH_MAC_DEL);
			return ret;
		}
		hw->uc_num--;
	}
	rte_ether_addr_copy(addr, (struct rte_ether_addr *)hw->mac_addr);
	zxdh_pci_write_dev_config(hw, offsetof(struct zxdh_net_config, mac),
								&hw->mac_addr, RTE_ETHER_ADDR_LEN);
	return ret;
}

int
zxdh_dev_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
	uint32_t index, uint32_t vmdq __rte_unused)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	void *mac_reply_msg_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, mac_reply_msg);
	uint16_t i;
	int ret;

	if (index >= ZXDH_MAX_MAC_ADDRS) {
		PMD_DRV_LOG(ERR, "Add mac index (%u) is out of range", index);
		return -EINVAL;
	}

	for (i = 0; (i != ZXDH_MAX_MAC_ADDRS); ++i) {
		if (memcmp(&dev->data->mac_addrs[i], mac_addr, sizeof(*mac_addr)))
			continue;

		PMD_DRV_LOG(INFO, "MAC address already configured");
		return -EADDRINUSE;
	}

	if (hw->is_pf) {
		if (rte_is_unicast_ether_addr(mac_addr)) {
			if (hw->uc_num < ZXDH_MAX_UC_MAC_ADDRS) {
				ret = zxdh_add_mac_table(hw, hw->vport.vport,
							mac_addr, hw->hash_search_index, 0, 0);
				if (ret) {
					if (ret == -EADDRINUSE) {
						PMD_DRV_LOG(ERR, "pf mac add failed mac is in used");
						return -EADDRINUSE;
					}
					PMD_DRV_LOG(ERR, "mac_addr_add failed, code:%d", ret);
					return ret;
				}
				memcpy(&hw->mac_addr, mac_addr, 6);
				hw->uc_num++;
			} else {
				PMD_DRV_LOG(ERR, "MC_MAC is out of range, MAX_MC_MAC:%d",
						ZXDH_MAX_MC_MAC_ADDRS);
				return -EINVAL;
			}
		} else {
			if (hw->mc_num < ZXDH_MAX_MC_MAC_ADDRS) {
				ret = zxdh_add_mac_table(hw, hw->vport.vport,
							mac_addr, hw->hash_search_index, 0, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "mac_addr_add  failed, code:%d", ret);
					return ret;
				}
				hw->mc_num++;
			} else {
				PMD_DRV_LOG(ERR, "MC_MAC is out of range, MAX_MC_MAC:%d",
						ZXDH_MAX_MC_MAC_ADDRS);
				return -EINVAL;
			}
		}
	} else {
		struct zxdh_mac_filter *mac_filter = &msg_info.data.mac_filter_msg;

		mac_filter->filter_flag = ZXDH_MAC_FILTER;
		mac_filter->mac = *mac_addr;
		zxdh_msg_head_build(hw, ZXDH_MAC_ADD, &msg_info);
		if (rte_is_unicast_ether_addr(mac_addr)) {
			if (hw->uc_num < ZXDH_MAX_UC_MAC_ADDRS) {
				ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info),
						zxdh_msg_reply_info,
						ZXDH_ST_SZ_BYTES(msg_reply_info));
				if (ret) {
					uint8_t flag = ZXDH_GET(mac_reply_msg,
						mac_reply_msg_addr, mac_flag);
					if (flag == ZXDH_EEXIST_MAC_FLAG) {
						PMD_DRV_LOG(ERR, "pf mac add failed mac is in used");
						return -EADDRINUSE;
					}
					PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
							hw->vport.vport, ZXDH_MAC_ADD);
					return ret;
				}
				hw->uc_num++;
			} else {
				PMD_DRV_LOG(ERR, "MC_MAC is out of range, MAX_MC_MAC:%d",
						ZXDH_MAX_MC_MAC_ADDRS);
				return -EINVAL;
			}
		} else {
			if (hw->mc_num < ZXDH_MAX_MC_MAC_ADDRS) {
				ret = zxdh_vf_send_msg_to_pf(dev, &msg_info,
							sizeof(msg_info), NULL, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
							hw->vport.vport, ZXDH_MAC_ADD);
					return ret;
				}
				hw->mc_num++;
			} else {
				PMD_DRV_LOG(ERR, "MC_MAC is out of range, MAX_MC_MAC:%d",
						ZXDH_MAX_MC_MAC_ADDRS);
				return -EINVAL;
			}
		}
	}
	dev->data->mac_addrs[index] = *mac_addr;
	return 0;
}

void zxdh_dev_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	struct rte_ether_addr *mac_addr = &dev->data->mac_addrs[index];
	uint16_t ret = 0;

	if (index >= ZXDH_MAX_MAC_ADDRS)
		return;

	if (hw->is_pf) {
		if (rte_is_unicast_ether_addr(mac_addr)) {
			if (hw->uc_num <= ZXDH_MAX_UC_MAC_ADDRS) {
				ret = zxdh_del_mac_table(hw, hw->vport.vport,
						mac_addr, hw->hash_search_index, 0, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "mac_addr_del  failed, code:%d", ret);
					return;
				}
				hw->uc_num--;
			} else {
				PMD_DRV_LOG(ERR, "MC_MAC is out of range, MAX_MC_MAC:%d",
						ZXDH_MAX_MC_MAC_ADDRS);
				return;
			}
		} else {
			if (hw->mc_num <= ZXDH_MAX_MC_MAC_ADDRS) {
				ret = zxdh_del_mac_table(hw, hw->vport.vport,
							mac_addr, hw->hash_search_index, 0, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "mac_addr_del  failed, code:%d", ret);
					return;
				}
				hw->mc_num--;
			} else {
				PMD_DRV_LOG(ERR, "MC_MAC is out of range, MAX_MC_MAC:%d",
						ZXDH_MAX_MC_MAC_ADDRS);
				return;
			}
		}
	} else {
		struct zxdh_mac_filter *mac_filter = &msg_info.data.mac_filter_msg;

		mac_filter->filter_flag = ZXDH_MAC_FILTER;
		mac_filter->mac = *mac_addr;
		zxdh_msg_head_build(hw, ZXDH_MAC_DEL, &msg_info);
		if (rte_is_unicast_ether_addr(mac_addr)) {
			if (hw->uc_num <= ZXDH_MAX_UC_MAC_ADDRS) {
				ret = zxdh_vf_send_msg_to_pf(dev, &msg_info,
							sizeof(msg_info), NULL, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
							hw->vport.vport, ZXDH_MAC_DEL);
					return;
				}
				hw->uc_num--;
			} else {
				PMD_DRV_LOG(ERR, "MC_MAC is out of range, MAX_MC_MAC:%d",
						ZXDH_MAX_MC_MAC_ADDRS);
				return;
			}
		} else {
			if (hw->mc_num <= ZXDH_MAX_MC_MAC_ADDRS) {
				ret = zxdh_vf_send_msg_to_pf(dev, &msg_info,
							sizeof(msg_info), NULL, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
							hw->vport.vport, ZXDH_MAC_DEL);
					return;
				}
				hw->mc_num--;
			} else {
				PMD_DRV_LOG(ERR, "MC_MAC is out of range, MAX_MC_MAC:%d",
						ZXDH_MAX_MC_MAC_ADDRS);
				return;
			}
		}
	}
	memset(&dev->data->mac_addrs[index], 0, sizeof(struct rte_ether_addr));
}

int zxdh_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw	= dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	int16_t ret = 0;

	if (hw->promisc_status == 0) {
		if (hw->is_pf) {
			ret = zxdh_dev_unicast_table_set(hw, hw->vport.vport, true);
			if (hw->allmulti_status == 0)
				ret = zxdh_dev_multicast_table_set(hw, hw->vport.vport, true);

		} else {
			struct zxdh_port_promisc_msg *promisc_msg = &msg_info.data.port_promisc_msg;

			zxdh_msg_head_build(hw, ZXDH_PORT_PROMISC_SET, &msg_info);
			promisc_msg->mode = ZXDH_PROMISC_MODE;
			promisc_msg->value = true;
			if (hw->allmulti_status == 0)
				promisc_msg->mc_follow = true;

			ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
			if (ret) {
				PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
						hw->vport.vport, ZXDH_PROMISC_MODE);
				return ret;
			}
		}
		hw->promisc_status = 1;
	}
	return ret;
}

int zxdh_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int16_t ret = 0;
	struct zxdh_msg_info msg_info = {0};

	if (hw->promisc_status == 1) {
		if (hw->is_pf) {
			ret = zxdh_dev_unicast_table_set(hw, hw->vport.vport, false);
			if (hw->allmulti_status == 0)
				ret = zxdh_dev_multicast_table_set(hw, hw->vport.vport, false);

		} else {
			struct zxdh_port_promisc_msg *promisc_msg = &msg_info.data.port_promisc_msg;

			zxdh_msg_head_build(hw, ZXDH_PORT_PROMISC_SET, &msg_info);
			promisc_msg->mode = ZXDH_PROMISC_MODE;
			promisc_msg->value = false;
			if (hw->allmulti_status == 0)
				promisc_msg->mc_follow = true;

			ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
			if (ret) {
				PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
						hw->vport.vport, ZXDH_PROMISC_MODE);
				return ret;
			}
		}
		hw->promisc_status = 0;
	}
	return ret;
}

int zxdh_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int16_t ret = 0;
	struct zxdh_msg_info msg_info = {0};

	if (hw->allmulti_status == 0) {
		if (hw->is_pf) {
			ret = zxdh_dev_multicast_table_set(hw, hw->vport.vport, true);
		} else {
			struct zxdh_port_promisc_msg *promisc_msg = &msg_info.data.port_promisc_msg;

			zxdh_msg_head_build(hw, ZXDH_PORT_PROMISC_SET, &msg_info);

			promisc_msg->mode = ZXDH_ALLMULTI_MODE;
			promisc_msg->value = true;
			ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
			if (ret) {
				PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
						hw->vport.vport, ZXDH_ALLMULTI_MODE);
				return ret;
			}
		}
		hw->allmulti_status = 1;
	}
	return ret;
}

int zxdh_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int16_t ret = 0;
	struct zxdh_msg_info msg_info = {0};

	if (hw->allmulti_status == 1) {
		if (hw->is_pf) {
			if (hw->promisc_status == 1)
				goto end;
			ret = zxdh_dev_multicast_table_set(hw, hw->vport.vport, false);
		} else {
			struct zxdh_port_promisc_msg *promisc_msg = &msg_info.data.port_promisc_msg;

			zxdh_msg_head_build(hw, ZXDH_PORT_PROMISC_SET, &msg_info);
			if (hw->promisc_status == 1)
				goto end;
			promisc_msg->mode = ZXDH_ALLMULTI_MODE;
			promisc_msg->value = false;
			ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
			if (ret) {
				PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
						hw->vport.vport, ZXDH_ALLMULTI_MODE);
				return ret;
			}
		}
		hw->allmulti_status = 0;
	}
	return ret;
end:
	hw->allmulti_status = 0;
	return ret;
}

int
zxdh_dev_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct zxdh_hw *hw = (struct zxdh_hw *)dev->data->dev_private;
	uint16_t idx = 0;
	uint16_t bit_idx = 0;
	uint8_t msg_type = 0;
	int ret = 0;

	vlan_id &= RTE_VLAN_ID_MASK;
	if (vlan_id == 0 || vlan_id == RTE_ETHER_MAX_VLAN_ID) {
		PMD_DRV_LOG(ERR, "vlan id (%d) is reserved", vlan_id);
		return -EINVAL;
	}

	idx = vlan_id / ZXDH_VLAN_FILTER_GROUPS;
	bit_idx = vlan_id % ZXDH_VLAN_FILTER_GROUPS;

	if (on) {
		if (dev->data->vlan_filter_conf.ids[idx] & (1ULL << bit_idx)) {
			PMD_DRV_LOG(ERR, "vlan:%d has already added", vlan_id);
			return 0;
		}
		msg_type = ZXDH_VLAN_FILTER_ADD;
	} else {
		if (!(dev->data->vlan_filter_conf.ids[idx] & (1ULL << bit_idx))) {
			PMD_DRV_LOG(ERR, "vlan:%d has already deleted", vlan_id);
			return 0;
		}
		msg_type = ZXDH_VLAN_FILTER_DEL;
	}

	if (hw->is_pf) {
		ret = zxdh_vlan_filter_table_set(hw, hw->vport.vport, vlan_id, on);
		if (ret) {
			PMD_DRV_LOG(ERR, "vlan_id:%d table set failed", vlan_id);
			return -1;
		}
	} else {
		struct zxdh_msg_info msg = {0};
		zxdh_msg_head_build(hw, msg_type, &msg);
		msg.data.vlan_filter_msg.vlan_id = vlan_id;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg, sizeof(struct zxdh_msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
					hw->vport.vport, msg_type);
			return ret;
		}
	}

	if (on)
		dev->data->vlan_filter_conf.ids[idx] |= (1ULL << bit_idx);
	else
		dev->data->vlan_filter_conf.ids[idx] &= ~(1ULL << bit_idx);

	return 0;
}

int
zxdh_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct rte_eth_rxmode *rxmode;
	struct zxdh_msg_info msg = {0};
	int ret = 0;

	rxmode = &dev->data->dev_conf.rxmode;
	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER) {
			if (hw->is_pf) {
				ret = zxdh_set_vlan_filter(hw, hw->vport.vport, true);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d vlan filter set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			} else {
				msg.data.vlan_filter_set_msg.enable = true;
				zxdh_msg_head_build(hw, ZXDH_VLAN_FILTER_SET, &msg);
				ret = zxdh_vf_send_msg_to_pf(hw->eth_dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d vlan filter set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			}
		} else {
			if (hw->is_pf) {
				ret = zxdh_set_vlan_filter(hw, hw->vport.vport, false);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d vlan filter set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			} else {
				msg.data.vlan_filter_set_msg.enable = false;
				zxdh_msg_head_build(hw, ZXDH_VLAN_FILTER_SET, &msg);
				ret = zxdh_vf_send_msg_to_pf(hw->eth_dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d vlan filter set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			}
		}
	}

	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP) {
			if (hw->is_pf) {
				ret = zxdh_set_vlan_offload(hw, hw->vport.vport,
						ZXDH_VLAN_STRIP_TYPE, true);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d vlan strip set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			} else {
				msg.data.vlan_offload_msg.enable = true;
				msg.data.vlan_offload_msg.type = ZXDH_VLAN_STRIP_TYPE;
				zxdh_msg_head_build(hw, ZXDH_VLAN_OFFLOAD, &msg);
				ret = zxdh_vf_send_msg_to_pf(hw->eth_dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d vlan strip set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			}
		} else {
			if (hw->is_pf) {
				ret = zxdh_set_vlan_offload(hw, hw->vport.vport,
						ZXDH_VLAN_STRIP_TYPE, false);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d vlan strip set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			} else {
				msg.data.vlan_offload_msg.enable = false;
				msg.data.vlan_offload_msg.type = ZXDH_VLAN_STRIP_TYPE;
				zxdh_msg_head_build(hw, ZXDH_VLAN_OFFLOAD, &msg);
				ret = zxdh_vf_send_msg_to_pf(hw->eth_dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d vlan strip set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			}
		}
	}

	if (mask & RTE_ETH_QINQ_STRIP_MASK) {
		memset(&msg, 0, sizeof(struct zxdh_msg_info));
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_QINQ_STRIP) {
			if (hw->is_pf) {
				ret = zxdh_set_vlan_offload(hw, hw->vport.vport,
					ZXDH_QINQ_STRIP_TYPE, true);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d qinq offload set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			} else {
				msg.data.vlan_offload_msg.enable = true;
				msg.data.vlan_offload_msg.type = ZXDH_QINQ_STRIP_TYPE;
				zxdh_msg_head_build(hw, ZXDH_VLAN_OFFLOAD, &msg);
				ret = zxdh_vf_send_msg_to_pf(hw->eth_dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d qinq offload set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			}
		} else {
			if (hw->is_pf) {
				ret = zxdh_set_vlan_offload(hw, hw->vport.vport,
						ZXDH_QINQ_STRIP_TYPE, false);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d qinq offload set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			} else {
				msg.data.vlan_offload_msg.enable = false;
				msg.data.vlan_offload_msg.type = ZXDH_QINQ_STRIP_TYPE;
				zxdh_msg_head_build(hw, ZXDH_VLAN_OFFLOAD, &msg);
				ret = zxdh_vf_send_msg_to_pf(hw->eth_dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
				if (ret) {
					PMD_DRV_LOG(ERR, "port %d qinq offload set failed",
						hw->vport.vfid);
					return -EAGAIN;
				}
			}
		}
	}

	return ret;
}

int
zxdh_vlan_tpid_set(struct rte_eth_dev *dev, enum rte_vlan_type vlan_type, uint16_t tpid)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_port_vlan_table port_vlan_table = {0};
	struct zxdh_msg_info msg = {0};
	int ret = 0;

	if (vlan_type != RTE_ETH_VLAN_TYPE_OUTER) {
		PMD_DRV_LOG(ERR, "unsupported rte vlan type!");
		return -1;
	}

	if (hw->is_pf) {
		ret = zxdh_get_port_vlan_attr(hw, hw->vport.vport, &port_vlan_table);
		if (ret != 0)
			PMD_DRV_LOG(ERR, "get port vlan attr table failed");
		port_vlan_table.hit_flag = 1;
		port_vlan_table.business_vlan_tpid = tpid;
		ret = zxdh_set_port_vlan_attr(hw, hw->vport.vport, &port_vlan_table);
		if (ret != 0)
			PMD_DRV_LOG(ERR, "set port vlan tpid %d attr table failed", tpid);
	} else {
		zxdh_msg_head_build(hw, ZXDH_VLAN_SET_TPID, &msg);
		msg.data.zxdh_vlan_tpid.tpid = tpid;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg,
				sizeof(struct zxdh_msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "port %d vlan tpid %d set failed",
				hw->vfid, tpid);
			return -1;
		}
	}

	return 0;
}

int
zxdh_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg = {0};
	uint16_t old_reta[RTE_ETH_RSS_RETA_SIZE_256];
	uint16_t idx;
	uint16_t i;
	uint16_t pos;
	int ret;

	if (reta_size != RTE_ETH_RSS_RETA_SIZE_256) {
		PMD_DRV_LOG(ERR, "reta_size is illegal(%u).reta_size should be 256", reta_size);
		return -EINVAL;
	}
	if (!hw->rss_reta) {
		hw->rss_reta = rte_calloc(NULL, RTE_ETH_RSS_RETA_SIZE_256, sizeof(uint16_t), 0);
		if (hw->rss_reta == NULL) {
			PMD_DRV_LOG(ERR, "Failed to allocate RSS reta");
			return -ENOMEM;
		}
	}
	for (idx = 0, i = 0; (i < reta_size); ++i) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		pos = i % RTE_ETH_RETA_GROUP_SIZE;
		if (((reta_conf[idx].mask >> pos) & 0x1) == 0)
			continue;
		if (reta_conf[idx].reta[pos] > dev->data->nb_rx_queues) {
			PMD_DRV_LOG(ERR, "reta table value err(%u >= %u)",
				reta_conf[idx].reta[pos], dev->data->nb_rx_queues);
			return -EINVAL;
		}
		if (hw->rss_reta[i] != reta_conf[idx].reta[pos])
			break;
	}
	if (i == reta_size) {
		PMD_DRV_LOG(INFO, "reta table same with buffered table");
		return 0;
	}
	memcpy(old_reta, hw->rss_reta, sizeof(old_reta));

	for (idx = 0, i = 0; i < reta_size; ++i) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		pos = i % RTE_ETH_RETA_GROUP_SIZE;
		if (((reta_conf[idx].mask >> pos) & 0x1) == 0)
			continue;
		hw->rss_reta[i] = reta_conf[idx].reta[pos];
	}

	zxdh_msg_head_build(hw, ZXDH_RSS_RETA_SET, &msg);
	for (i = 0; i < reta_size; i++)
		msg.data.rss_reta.reta[i] =
			(hw->channel_context[hw->rss_reta[i] * 2].ph_chno);

	if (hw->is_pf) {
		ret = zxdh_rss_table_set(hw, hw->vport.vport, &msg.data.rss_reta);
		if (ret) {
			PMD_DRV_LOG(ERR, "rss reta table set failed");
			return -EINVAL;
		}
	} else {
		ret = zxdh_vf_send_msg_to_pf(dev, &msg, sizeof(struct zxdh_msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "vf rss reta table set failed");
			return -EINVAL;
		}
	}
	return ret;
}

uint16_t
zxdh_hw_qid_to_logic_qid(struct rte_eth_dev *dev, uint16_t qid)
{
	struct zxdh_hw *priv = (struct zxdh_hw *)dev->data->dev_private;
	uint16_t i;

	for (i = 0; i < priv->max_queue_pairs * 2 ; i++) {
		if (priv->channel_context[i].valid)
			if (qid == priv->channel_context[i].ph_chno)
				return i;
	}

	return ZXDH_INVALID_LOGIC_QID;
}

int
zxdh_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct zxdh_hw *hw = (struct zxdh_hw *)dev->data->dev_private;
	struct zxdh_msg_info msg = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	void *rss_reta_msg_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, rss_reta_msg);
	uint16_t idx;
	uint16_t i;
	int ret = 0;
	uint16_t qid_logic;

	ret = (!reta_size || reta_size > RTE_ETH_RSS_RETA_SIZE_256);
	if (ret) {
		PMD_DRV_LOG(ERR, "request reta size(%u) not same with buffered(%u)",
			reta_size, RTE_ETH_RSS_RETA_SIZE_256);
		return -EINVAL;
	}

	/* Fill each entry of the table even if its bit is not set. */
	for (idx = 0, i = 0; (i != reta_size); ++i) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		reta_conf[idx].reta[i % RTE_ETH_RETA_GROUP_SIZE] = hw->rss_reta[i];
	}

	zxdh_msg_head_build(hw, ZXDH_RSS_RETA_GET, &msg);

	if (hw->is_pf) {
		ret = zxdh_rss_table_get(hw, hw->vport.vport, rss_reta_msg_addr);
		if (ret) {
			PMD_DRV_LOG(ERR, "rss reta table set failed");
			return -EINVAL;
		}
	} else {
		ret = zxdh_vf_send_msg_to_pf(dev, &msg, sizeof(struct zxdh_msg_info),
					zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info));
		if (ret) {
			PMD_DRV_LOG(ERR, "vf rss reta table get failed");
			return -EINVAL;
		}
	}

	struct zxdh_rss_reta *reta_table = rss_reta_msg_addr;

	for (idx = 0, i = 0; i < reta_size; ++i) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;

		qid_logic = zxdh_hw_qid_to_logic_qid(dev, reta_table->reta[i]);
		if (qid_logic == ZXDH_INVALID_LOGIC_QID) {
			PMD_DRV_LOG(ERR, "rsp phy reta qid (%u) is illegal(%u)",
				reta_table->reta[i], qid_logic);
			return -EINVAL;
		}
		reta_conf[idx].reta[i % RTE_ETH_RETA_GROUP_SIZE] = qid_logic >> 1;
	}
	return 0;
}

static uint32_t
zxdh_rss_hf_to_hw(uint64_t hf)
{
	uint32_t hw_hf = 0;

	if (hf & ZXDH_HF_MAC_VLAN_ETH)
		hw_hf |= ZXDH_HF_MAC_VLAN;
	if (hf & ZXDH_HF_F3_ETH)
		hw_hf |= ZXDH_HF_F3;
	if (hf & ZXDH_HF_F5_ETH)
		hw_hf |= ZXDH_HF_F5;

	if (hw_hf == (ZXDH_HF_MAC_VLAN | ZXDH_HF_F3 | ZXDH_HF_F5))
		hw_hf = ZXDH_HF_ALL;
	return hw_hf;
}

static uint64_t
zxdh_rss_hf_to_eth(uint64_t hw_hf)
{
	uint64_t hf = 0;

	if (hw_hf == ZXDH_HF_ALL)
		return (ZXDH_HF_MAC_VLAN_ETH | ZXDH_HF_F3_ETH | ZXDH_HF_F5_ETH);

	if (hw_hf & ZXDH_HF_MAC_VLAN)
		hf |= ZXDH_HF_MAC_VLAN_ETH;
	if (hw_hf & ZXDH_HF_F3)
		hf |= ZXDH_HF_F3_ETH;
	if (hw_hf & ZXDH_HF_F5)
		hf |= ZXDH_HF_F5_ETH;

	return hf;
}

int
zxdh_rss_hash_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_conf *rss_conf)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct rte_eth_rss_conf *old_rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;
	struct zxdh_msg_info msg = {0};
	struct zxdh_port_attr_table port_attr = {0};
	uint32_t hw_hf_new, hw_hf_old;
	int need_update_hf = 0;
	int ret = 0;

	ret = rss_conf->rss_hf & ZXDH_RSS_HF_MASK;
	if (ret) {
		PMD_DRV_LOG(ERR, "Not support some hash function (%08lx)", rss_conf->rss_hf);
		return -EINVAL;
	}

	hw_hf_new = zxdh_rss_hf_to_hw(rss_conf->rss_hf);
	hw_hf_old = zxdh_rss_hf_to_hw(old_rss_conf->rss_hf);

	if (hw_hf_new != hw_hf_old || hw->rss_enable != !!rss_conf->rss_hf)
		need_update_hf = 1;

	if (need_update_hf) {
		if (hw->is_pf) {
			ret = zxdh_get_port_attr(hw, hw->vport.vport, &port_attr);
			port_attr.rss_enable = !!rss_conf->rss_hf;
			ret = zxdh_set_port_attr(hw, hw->vport.vport, &port_attr);
			if (ret) {
				PMD_DRV_LOG(ERR, "rss enable set failed");
				return -EINVAL;
			}
		} else {
			msg.data.rss_enable.enable = !!rss_conf->rss_hf;
			zxdh_msg_head_build(hw, ZXDH_RSS_ENABLE, &msg);
			ret = zxdh_vf_send_msg_to_pf(dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
			if (ret) {
				PMD_DRV_LOG(ERR, "rss enable set failed");
				return -EINVAL;
			}
		}
		if (hw->is_pf) {
			hw->rss_enable = !!rss_conf->rss_hf;
			if (rss_conf->rss_hf == 0)
				return 0;
			ret = zxdh_get_port_attr(hw, hw->vport.vport, &port_attr);
			port_attr.rss_hash_factor = hw_hf_new;
			ret = zxdh_set_port_attr(hw, hw->vport.vport, &port_attr);
			if (ret) {
				PMD_DRV_LOG(ERR, "rss hash factor set failed");
				return -EINVAL;
			}
		} else {
			msg.data.rss_hf.rss_hf = hw_hf_new;
			zxdh_msg_head_build(hw, ZXDH_RSS_HF_SET, &msg);
			ret = zxdh_vf_send_msg_to_pf(dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
			if (ret) {
				PMD_DRV_LOG(ERR, "rss hash factor set failed");
				return -EINVAL;
			}
		}
		old_rss_conf->rss_hf = rss_conf->rss_hf;
	}

	return 0;
}

int
zxdh_rss_hash_conf_get(struct rte_eth_dev *dev, struct rte_eth_rss_conf *rss_conf)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct rte_eth_rss_conf *old_rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;
	struct zxdh_msg_info msg = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	void *rss_hf_msg_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, rss_hf_msg);
	struct zxdh_port_attr_table port_attr = {0};
	uint64_t rss_hf = 0;
	uint64_t hw_hf = 0;
	uint8_t need_update_hf = 0;
	int ret;

	if (rss_conf == NULL) {
		PMD_DRV_LOG(ERR, "rss conf is NULL");
		return -ENOMEM;
	}

	if (hw->rss_enable == 0) {
		rss_conf->rss_hf = 0;
		return 0;
	}

	if (old_rss_conf->rss_hf == 0)
		need_update_hf = 1;

	if (!need_update_hf) {
		hw_hf = zxdh_rss_hf_to_hw(old_rss_conf->rss_hf);
		rss_conf->rss_hf = zxdh_rss_hf_to_eth(hw_hf);
	}

	if (need_update_hf) {
		zxdh_msg_head_build(hw, ZXDH_RSS_HF_GET, &msg);
		if (hw->is_pf) {
			ret = zxdh_get_port_attr(hw, hw->vport.vport, &port_attr);
			if (ret) {
				PMD_DRV_LOG(ERR, "rss hash factor set failed");
				return -EINVAL;
			}
			ZXDH_SET(rss_hf, rss_hf_msg_addr, rss_hf, port_attr.rss_hash_factor);
		} else {
			ret = zxdh_vf_send_msg_to_pf(dev, &msg, sizeof(struct zxdh_msg_info),
					zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info));
			if (ret) {
				PMD_DRV_LOG(ERR, "rss hash factor set failed");
				return -EINVAL;
			}
		}
		rss_hf = ZXDH_GET(rss_hf, rss_hf_msg_addr, rss_hf);
		rss_conf->rss_hf = zxdh_rss_hf_to_eth(rss_hf);
		old_rss_conf->rss_hf = zxdh_rss_hf_to_eth(hw_hf);
	}

	return 0;
}

static int
zxdh_get_rss_enable_conf(struct rte_eth_dev *dev)
{
	if (dev->data->dev_conf.rxmode.mq_mode == RTE_ETH_MQ_RX_RSS)
		return dev->data->nb_rx_queues == 1 ? 0 : 1;
	else if (dev->data->dev_conf.rxmode.mq_mode == RTE_ETH_MQ_RX_NONE)
		return 0;

	return 0;
}

int
zxdh_rss_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *dev_data = dev->data;
	struct zxdh_hw *hw = (struct zxdh_hw *)dev->data->dev_private;
	struct zxdh_port_attr_table port_attr = {0};
	struct zxdh_msg_info msg = {0};
	int ret = 0;
	uint32_t hw_hf;
	uint32_t i;

	if (dev->data->nb_rx_queues == 0) {
		PMD_DRV_LOG(ERR, "port %u nb_rx_queues is 0", dev->data->port_id);
		return -1;
	}

	/* config rss enable */
	uint8_t curr_rss_enable = zxdh_get_rss_enable_conf(dev);

	if (hw->rss_enable != curr_rss_enable) {
		if (hw->is_pf) {
			ret = zxdh_get_port_attr(hw, hw->vport.vport, &port_attr);
			port_attr.rss_enable = curr_rss_enable;
			ret = zxdh_set_port_attr(hw, hw->vport.vport, &port_attr);
			if (ret) {
				PMD_DRV_LOG(ERR, "rss enable set failed");
				return -EINVAL;
			}
		} else {
			msg.data.rss_enable.enable = curr_rss_enable;
			zxdh_msg_head_build(hw, ZXDH_RSS_ENABLE, &msg);
			ret = zxdh_vf_send_msg_to_pf(dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
			if (ret) {
				PMD_DRV_LOG(ERR, "rss enable set failed");
				return -EINVAL;
			}
		}
		hw->rss_enable = curr_rss_enable;
	}

	if (curr_rss_enable && hw->rss_init == 0) {
		/* config hash factor */
		hw_hf = zxdh_rss_hf_to_hw(dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf);
		memset(&msg, 0, sizeof(msg));
		if (hw->is_pf) {
			ret = zxdh_get_port_attr(hw, hw->vport.vport, &port_attr);
			port_attr.rss_hash_factor = hw_hf;
			ret = zxdh_set_port_attr(hw, hw->vport.vport, &port_attr);
			if (ret) {
				PMD_DRV_LOG(ERR, "rss hash factor set failed");
				return -EINVAL;
			}
		} else {
			msg.data.rss_hf.rss_hf = hw_hf;
			zxdh_msg_head_build(hw, ZXDH_RSS_HF_SET, &msg);
			ret = zxdh_vf_send_msg_to_pf(dev, &msg,
						sizeof(struct zxdh_msg_info), NULL, 0);
			if (ret) {
				PMD_DRV_LOG(ERR, "rss hash factor set failed");
				return -EINVAL;
			}
		}
		hw->rss_init = 1;
	}

	if (!hw->rss_reta) {
		hw->rss_reta = rte_calloc(NULL, RTE_ETH_RSS_RETA_SIZE_256, sizeof(uint16_t), 0);
		if (hw->rss_reta == NULL) {
			PMD_DRV_LOG(ERR, "alloc memory fail");
			return -1;
		}
	}
	for (i = 0; i < RTE_ETH_RSS_RETA_SIZE_256; i++)
		hw->rss_reta[i] = i % dev_data->nb_rx_queues;

	/* hw config reta */
	zxdh_msg_head_build(hw, ZXDH_RSS_RETA_SET, &msg);
	for (i = 0; i < RTE_ETH_RSS_RETA_SIZE_256; i++)
		msg.data.rss_reta.reta[i] =
			hw->channel_context[hw->rss_reta[i] * 2].ph_chno;

	if (hw->is_pf) {
		ret = zxdh_rss_table_set(hw, hw->vport.vport, &msg.data.rss_reta);
		if (ret) {
			PMD_DRV_LOG(ERR, "rss reta table set failed");
			return -EINVAL;
		}
	} else {
		ret = zxdh_vf_send_msg_to_pf(dev, &msg, sizeof(struct zxdh_msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "vf rss reta table set failed");
			return -EINVAL;
		}
	}
	return 0;
}

static int32_t
zxdh_hw_vqm_stats_get(struct rte_eth_dev *dev, enum zxdh_agent_msg_type opcode,
			struct zxdh_hw_vqm_stats *hw_stats)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	enum ZXDH_BAR_MODULE_ID module_id;
	int ret = 0;

	switch (opcode) {
	case ZXDH_VQM_DEV_STATS_GET:
	case ZXDH_VQM_QUEUE_STATS_GET:
	case ZXDH_VQM_QUEUE_STATS_RESET:
		module_id = ZXDH_BAR_MODULE_VQM;
		break;
	case ZXDH_MAC_STATS_GET:
	case ZXDH_MAC_STATS_RESET:
		module_id = ZXDH_BAR_MODULE_MAC;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid opcode %u", opcode);
		return -1;
	}

	zxdh_agent_msg_build(hw, opcode, &msg_info);

	ret = zxdh_send_msg_to_riscv(dev, &msg_info, sizeof(struct zxdh_msg_info),
				zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info), module_id);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to get hw stats");
		return -1;
	}

	void *hw_vqm_stats = ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, vqm_stats);
	memcpy(hw_stats, hw_vqm_stats, sizeof(struct zxdh_hw_vqm_stats));

	return 0;
}

static int
zxdh_hw_mac_stats_get(struct rte_eth_dev *dev,
		struct zxdh_hw_mac_stats *mac_stats,
		struct zxdh_hw_mac_bytes *mac_bytes)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint64_t virt_addr = (uint64_t)(hw->bar_addr[ZXDH_BAR0_INDEX] + ZXDH_MAC_OFFSET);
	uint64_t stats_addr =  0;
	uint64_t bytes_addr =  0;

	if (hw->speed <= RTE_ETH_SPEED_NUM_25G) {
		stats_addr = virt_addr + ZXDH_MAC_STATS_OFFSET + 352 * (hw->phyport % 4);
		bytes_addr = virt_addr + ZXDH_MAC_BYTES_OFFSET + 32 * (hw->phyport % 4);
	} else {
		stats_addr = virt_addr + ZXDH_MAC_STATS_OFFSET + 352 * 4;
		bytes_addr = virt_addr + ZXDH_MAC_BYTES_OFFSET + 32 * 4;
	}

	memcpy(mac_stats, (void *)stats_addr, sizeof(struct zxdh_hw_mac_stats));
	memcpy(mac_bytes, (void *)bytes_addr, sizeof(struct zxdh_hw_mac_bytes));
	return 0;
}

void
zxdh_data_hi_to_lo(uint64_t *data)
{
	uint32_t n_data_hi;
	uint32_t n_data_lo;

	n_data_lo = *data >> 32;
	n_data_hi = *data;
	*data =  (uint64_t)(rte_le_to_cpu_32(n_data_hi)) << 32 |
				rte_le_to_cpu_32(n_data_lo);
}

static int
zxdh_np_stats_get(struct rte_eth_dev *dev, struct zxdh_hw_np_stats *np_stats)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_dtb_shared_data *dtb_data = &hw->dev_sd->dtb_sd;
	struct zxdh_np_stats_data stats_data;
	uint32_t stats_id = zxdh_vport_to_vfid(hw->vport);
	uint32_t idx = 0;
	int ret = 0;

	idx = stats_id + ZXDH_BROAD_STATS_EGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				0, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->tx_broadcast_pkts = stats_data.n_pkts_dropped;
	np_stats->tx_broadcast_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->tx_broadcast_pkts);
	zxdh_data_hi_to_lo(&np_stats->tx_broadcast_bytes);

	idx = stats_id + ZXDH_BROAD_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				0, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->rx_broadcast_pkts = stats_data.n_pkts_dropped;
	np_stats->rx_broadcast_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->rx_broadcast_pkts);
	zxdh_data_hi_to_lo(&np_stats->rx_broadcast_bytes);

	idx = stats_id + ZXDH_MULTICAST_STATS_EGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				0, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->tx_multicast_pkts = stats_data.n_pkts_dropped;
	np_stats->tx_multicast_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->tx_multicast_pkts);
	zxdh_data_hi_to_lo(&np_stats->tx_multicast_bytes);

	idx = stats_id + ZXDH_MULTICAST_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				0, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->rx_multicast_pkts = stats_data.n_pkts_dropped;
	np_stats->rx_multicast_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->rx_multicast_pkts);
	zxdh_data_hi_to_lo(&np_stats->rx_multicast_bytes);

	idx = stats_id + ZXDH_UNICAST_STATS_EGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				0, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->tx_unicast_pkts = stats_data.n_pkts_dropped;
	np_stats->tx_unicast_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->tx_unicast_pkts);
	zxdh_data_hi_to_lo(&np_stats->tx_unicast_bytes);

	idx = stats_id + ZXDH_UNICAST_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				0, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->rx_unicast_pkts = stats_data.n_pkts_dropped;
	np_stats->rx_unicast_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->rx_unicast_pkts);
	zxdh_data_hi_to_lo(&np_stats->rx_unicast_bytes);

	idx = stats_id + ZXDH_MTU_STATS_EGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				1, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->tx_mtu_drop_pkts = stats_data.n_pkts_dropped;
	np_stats->tx_mtu_drop_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->tx_mtu_drop_pkts);
	zxdh_data_hi_to_lo(&np_stats->tx_mtu_drop_bytes);

	idx = stats_id + ZXDH_MTU_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				1, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->rx_mtu_drop_pkts = stats_data.n_pkts_dropped;
	np_stats->rx_mtu_drop_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->rx_mtu_drop_pkts);
	zxdh_data_hi_to_lo(&np_stats->rx_mtu_drop_bytes);

	idx = stats_id + ZXDH_MTR_STATS_EGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				1, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->tx_mtr_drop_pkts = stats_data.n_pkts_dropped;
	np_stats->tx_mtr_drop_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->tx_mtr_drop_pkts);
	zxdh_data_hi_to_lo(&np_stats->tx_mtr_drop_bytes);

	idx = stats_id + ZXDH_MTR_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(hw->dev_id, dtb_data->queueid,
				1, idx, (uint32_t *)&stats_data);
	if (ret)
		return ret;
	np_stats->rx_mtr_drop_pkts = stats_data.n_pkts_dropped;
	np_stats->rx_mtr_drop_bytes = stats_data.n_bytes_dropped;
	zxdh_data_hi_to_lo(&np_stats->rx_mtr_drop_pkts);
	zxdh_data_hi_to_lo(&np_stats->rx_mtr_drop_bytes);

	return 0;
}

static int
zxdh_hw_np_stats_get(struct rte_eth_dev *dev, struct zxdh_hw_np_stats *np_stats)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	void *hw_stas_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, hw_stats);
	int ret = 0;

	if (hw->is_pf) {
		ret = zxdh_np_stats_get(dev, np_stats);
		if (ret) {
			PMD_DRV_LOG(ERR, "get np stats failed");
			return -1;
		}
	} else {
		zxdh_msg_head_build(hw, ZXDH_GET_NP_STATS, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(struct zxdh_msg_info),
					zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info));
		if (ret) {
			PMD_DRV_LOG(ERR,
				"Failed to send msg: port 0x%x msg type", hw->vport.vport);
			return -1;
		}
		memcpy(np_stats, hw_stas_addr, sizeof(struct zxdh_hw_np_stats));
	}
	return ret;
}

int
zxdh_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_hw_vqm_stats vqm_stats = {0};
	struct zxdh_hw_np_stats np_stats = {0};
	struct zxdh_hw_mac_stats mac_stats = {0};
	struct zxdh_hw_mac_bytes mac_bytes = {0};
	uint32_t i = 0;

	zxdh_hw_vqm_stats_get(dev, ZXDH_VQM_DEV_STATS_GET,  &vqm_stats);
	if (hw->is_pf)
		zxdh_hw_mac_stats_get(dev, &mac_stats, &mac_bytes);

	zxdh_hw_np_stats_get(dev, &np_stats);

	stats->ipackets = vqm_stats.rx_total;
	stats->opackets = vqm_stats.tx_total;
	stats->ibytes = vqm_stats.rx_bytes;
	stats->obytes = vqm_stats.tx_bytes;
	stats->imissed = vqm_stats.rx_drop + mac_stats.rx_drop;
	stats->ierrors = vqm_stats.rx_error + mac_stats.rx_error + np_stats.rx_mtu_drop_pkts;
	stats->oerrors = vqm_stats.tx_error + mac_stats.tx_error + np_stats.tx_mtu_drop_pkts;

	if (hw->i_mtr_en || hw->e_mtr_en)
		stats->imissed  += np_stats.rx_mtr_drop_pkts;

	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
	for (i = 0; (i < dev->data->nb_rx_queues) && (i < RTE_ETHDEV_QUEUE_STAT_CNTRS); i++) {
		struct zxdh_virtnet_rx *rxvq = dev->data->rx_queues[i];

		if (rxvq == NULL)
			continue;
		stats->q_ipackets[i] = *(uint64_t *)(((char *)rxvq) +
				zxdh_rxq_stat_strings[0].offset);
		stats->q_ibytes[i] = *(uint64_t *)(((char *)rxvq) +
				zxdh_rxq_stat_strings[1].offset);
		stats->q_errors[i] = *(uint64_t *)(((char *)rxvq) +
				zxdh_rxq_stat_strings[2].offset);
		stats->q_errors[i] += *(uint64_t *)(((char *)rxvq) +
				zxdh_rxq_stat_strings[5].offset);
	}

	for (i = 0; (i < dev->data->nb_tx_queues) && (i < RTE_ETHDEV_QUEUE_STAT_CNTRS); i++) {
		struct zxdh_virtnet_tx *txvq = dev->data->tx_queues[i];

		if (txvq == NULL)
			continue;
		stats->q_opackets[i] = *(uint64_t *)(((char *)txvq) +
				zxdh_txq_stat_strings[0].offset);
		stats->q_obytes[i] = *(uint64_t *)(((char *)txvq) +
				zxdh_txq_stat_strings[1].offset);
		stats->q_errors[i] += *(uint64_t *)(((char *)txvq) +
				zxdh_txq_stat_strings[2].offset);
		stats->q_errors[i] += *(uint64_t *)(((char *)txvq) +
				zxdh_txq_stat_strings[5].offset);
	}
	return 0;
}

static int
zxdh_hw_stats_reset(struct rte_eth_dev *dev, enum zxdh_agent_msg_type opcode)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	enum ZXDH_BAR_MODULE_ID module_id;
	int ret = 0;

	switch (opcode) {
	case ZXDH_VQM_DEV_STATS_RESET:
		module_id = ZXDH_BAR_MODULE_VQM;
		break;
	case ZXDH_MAC_STATS_RESET:
		module_id = ZXDH_BAR_MODULE_MAC;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid opcode %u", opcode);
		return -1;
	}

	zxdh_agent_msg_build(hw, opcode, &msg_info);

	ret = zxdh_send_msg_to_riscv(dev, &msg_info, sizeof(struct zxdh_msg_info),
				zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info), module_id);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to reset hw stats");
		return -1;
	}
	return 0;
}

int
zxdh_hw_np_stats_pf_reset(struct rte_eth_dev *dev, uint32_t stats_id)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_hw_stats_data stats_data;
	uint32_t idx = 0;
	int ret = 0;

	idx = stats_id + ZXDH_UNICAST_STATS_EGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);
	if (ret)
		return ret;

	idx = stats_id + ZXDH_UNICAST_STATS_INGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);
	if (ret)
		return ret;

	idx = stats_id + ZXDH_MULTICAST_STATS_EGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);
	if (ret)
		return ret;

	idx = stats_id + ZXDH_MULTICAST_STATS_INGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);
	if (ret)
		return ret;

	idx = stats_id + ZXDH_BROAD_STATS_EGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);
	if (ret)
		return ret;

	idx = stats_id + ZXDH_BROAD_STATS_INGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);
	if (ret)
		return ret;

	idx = stats_id + ZXDH_MTU_STATS_EGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);
	if (ret)
		return ret;

	idx = stats_id + ZXDH_MTU_STATS_INGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);
	if (ret)
		return ret;

	idx = stats_id + ZXDH_MTR_STATS_EGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);
	if (ret)
		return ret;

	idx = stats_id + ZXDH_MTR_STATS_INGRESS_BASE;
	ret = zxdh_np_stat_ppu_cnt_get_ex(hw->dev_id, 1, idx,
			1, (uint32_t *)&stats_data);

	return ret;
}

static int
zxdh_hw_np_stats_vf_reset(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	int ret = 0;

	msg_info.data.np_stats_query.clear_mode = 1;
	zxdh_msg_head_build(hw, ZXDH_GET_NP_STATS, &msg_info);
	ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(struct zxdh_msg_info),
			zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info));
	if (ret)
		PMD_DRV_LOG(ERR, "Failed to send ZXDH_PORT_METER_STAT_GET msg. code:%d", ret);

	return ret;
}

static int
zxdh_np_stats_reset(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint32_t stats_id = zxdh_vport_to_vfid(hw->vport);
	int ret;

	if (hw->is_pf)
		ret = zxdh_hw_np_stats_pf_reset(dev, stats_id);
	else
		ret = zxdh_hw_np_stats_vf_reset(dev);
	return ret;
}

int zxdh_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int i = 0;

	zxdh_hw_stats_reset(dev, ZXDH_VQM_DEV_STATS_RESET);
	if (hw->is_pf)
		zxdh_hw_stats_reset(dev, ZXDH_MAC_STATS_RESET);
	zxdh_np_stats_reset(dev);
	for (i = 0; ((i < dev->data->nb_rx_queues) && (i < RTE_ETHDEV_QUEUE_STAT_CNTRS)); i++) {
		struct zxdh_virtnet_rx *rxvq = dev->data->rx_queues[i];
		if (rxvq == NULL)
			continue;
		memset(&rxvq->stats, 0, sizeof(struct zxdh_virtnet_stats));
	}
	for (i = 0; ((i < dev->data->nb_tx_queues) && (i < RTE_ETHDEV_QUEUE_STAT_CNTRS)); i++) {
		struct zxdh_virtnet_tx *txvq = dev->data->tx_queues[i];
		if (txvq == NULL)
			continue;
		memset(&txvq->stats, 0, sizeof(struct zxdh_virtnet_stats));
	}

	return 0;
}

int zxdh_dev_mtu_set(struct rte_eth_dev *dev, uint16_t new_mtu)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_panel_table panel = {0};
	struct zxdh_port_attr_table vport_att = {0};
	uint16_t vfid = zxdh_vport_to_vfid(hw->vport);
	int ret;

	if (hw->is_pf) {
		ret = zxdh_get_panel_attr(dev, &panel);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "get_panel_attr failed ret:%d", ret);
			return ret;
		}

		ret = zxdh_get_port_attr(hw, hw->vport.vport, &vport_att);
		if (ret != 0) {
			PMD_DRV_LOG(ERR,
				"[vfid:%d] zxdh_dev_mtu, get vport failed ret:%d", vfid, ret);
			return ret;
		}

		panel.mtu = new_mtu;
		panel.mtu_enable = 1;
		ret = zxdh_set_panel_attr(dev, &panel);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "set zxdh_dev_mtu failed, ret:%u", ret);
			return ret;
		}

		vport_att.mtu_enable = 1;
		vport_att.mtu = new_mtu;
		ret = zxdh_set_port_attr(hw, hw->vport.vport, &vport_att);
		if (ret != 0) {
			PMD_DRV_LOG(ERR,
				"[vfid:%d] zxdh_dev_mtu, set vport failed ret:%d", vfid, ret);
			return ret;
		}
	} else {
		struct zxdh_msg_info msg_info = {0};
		struct zxdh_port_attr_set_msg *attr_msg = &msg_info.data.port_attr_msg;

		zxdh_msg_head_build(hw, ZXDH_PORT_ATTRS_SET, &msg_info);
		attr_msg->mode = ZXDH_PORT_MTU_OFFLOAD_EN_OFF_FLAG;
		attr_msg->value = 1;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
				hw->vport.vport, ZXDH_PORT_MTU_OFFLOAD_EN_OFF_FLAG);
			return ret;
		}
		attr_msg->mode = ZXDH_PORT_MTU_FLAG;
		attr_msg->value = new_mtu;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
				hw->vport.vport, ZXDH_PORT_MTU_FLAG);
			return ret;
		}
	}
	dev->data->mtu = new_mtu;
	return 0;
}

int32_t
zxdh_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats, uint32_t n)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_hw_np_stats np_stats = {0};
	struct zxdh_hw_mac_stats mac_stats = {0};
	struct zxdh_hw_mac_bytes mac_bytes = {0};
	struct zxdh_hw_vqm_stats  vqm_stats = {0};
	uint32_t nstats = dev->data->nb_tx_queues * ZXDH_NB_TXQ_XSTATS +
			dev->data->nb_rx_queues * ZXDH_NB_RXQ_XSTATS +
			ZXDH_NP_XSTATS + ZXDH_VQM_XSTATS;
	uint32_t i = 0;
	uint32_t count = 0;
	uint32_t t = 0;

	if (hw->is_pf) {
		nstats += ZXDH_MAC_XSTATS + ZXDH_MAC_BYTES;
		zxdh_hw_mac_stats_get(dev, &mac_stats, &mac_bytes);
	}
	if (n < nstats)
		return nstats;
	zxdh_hw_vqm_stats_get(dev, ZXDH_VQM_DEV_STATS_GET,  &vqm_stats);
	zxdh_hw_np_stats_get(dev, &np_stats);
	for (i = 0; i < ZXDH_NP_XSTATS; i++) {
		xstats[count].value = *(uint64_t *)(((char *)&np_stats)
						 + zxdh_np_stat_strings[i].offset);
		xstats[count].id = count;
		count++;
	}
	if (hw->is_pf) {
		for (i = 0; i < ZXDH_MAC_XSTATS; i++) {
			xstats[count].value = *(uint64_t *)(((char *)&mac_stats)
						 + zxdh_mac_stat_strings[i].offset);
			xstats[count].id = count;
			count++;
		}
		for (i = 0; i < ZXDH_MAC_BYTES; i++) {
			xstats[count].value = *(uint64_t *)(((char *)&mac_bytes)
						 + zxdh_mac_bytes_strings[i].offset);
			xstats[count].id = count;
			count++;
		}
	}
	for (i = 0; i < ZXDH_VQM_XSTATS; i++) {
		xstats[count].value = *(uint64_t *)(((char *)&vqm_stats)
						 + zxdh_vqm_stat_strings[i].offset);
		xstats[count].id = count;
		count++;
	}
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct zxdh_virtnet_rx *rxvq = dev->data->rx_queues[i];

		if (rxvq == NULL)
			continue;
		for (t = 0; t < ZXDH_NB_RXQ_XSTATS; t++) {
			xstats[count].value = *(uint64_t *)(((char *)rxvq)
						 + zxdh_rxq_stat_strings[t].offset);
			xstats[count].id = count;
			count++;
		}
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct zxdh_virtnet_tx *txvq = dev->data->tx_queues[i];

		if (txvq == NULL)
			continue;

		for (t = 0; t < ZXDH_NB_TXQ_XSTATS; t++) {
			xstats[count].value = *(uint64_t *)(((char *)txvq)
						 + zxdh_txq_stat_strings[t].offset);
			xstats[count].id = count;
			count++;
		}
	}
	return count;
}


int32_t
zxdh_dev_xstats_get_names(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				__rte_unused unsigned int limit)
{
	uint32_t i	   = 0;
	uint32_t count = 0;
	uint32_t t	   = 0;
	struct zxdh_hw *hw = dev->data->dev_private;
	unsigned int nstats = dev->data->nb_tx_queues * ZXDH_NB_TXQ_XSTATS +
					dev->data->nb_rx_queues * ZXDH_NB_RXQ_XSTATS +
					ZXDH_NP_XSTATS + ZXDH_VQM_XSTATS;

	if (hw->is_pf)
		nstats += ZXDH_MAC_XSTATS + ZXDH_MAC_BYTES;

	if (xstats_names != NULL) {
		for (i = 0; i < ZXDH_NP_XSTATS; i++) {
			snprintf(xstats_names[count].name, sizeof(xstats_names[count].name),
			"%s", zxdh_np_stat_strings[i].name);
			count++;
		}
		if (hw->is_pf) {
			for (i = 0; i < ZXDH_MAC_XSTATS; i++) {
				snprintf(xstats_names[count].name, sizeof(xstats_names[count].name),
				"%s", zxdh_mac_stat_strings[i].name);
				count++;
			}
			for (i = 0; i < ZXDH_MAC_BYTES; i++) {
				snprintf(xstats_names[count].name, sizeof(xstats_names[count].name),
				"%s", zxdh_mac_bytes_strings[i].name);
				count++;
			}
		}
		for (i = 0; i < ZXDH_VQM_XSTATS; i++) {
			snprintf(xstats_names[count].name, sizeof(xstats_names[count].name),
			"%s", zxdh_vqm_stat_strings[i].name);
			count++;
		}
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			struct virtnet_rx *rxvq = dev->data->rx_queues[i];

			if (rxvq == NULL)
				continue;
			for (t = 0; t < ZXDH_NB_RXQ_XSTATS; t++) {
				snprintf(xstats_names[count].name, sizeof(xstats_names[count].name),
				"rx_q%u_%s", i, zxdh_rxq_stat_strings[t].name);
				count++;
			}
		}

		for (i = 0; i < dev->data->nb_tx_queues; i++) {
			struct virtnet_tx *txvq = dev->data->tx_queues[i];

			if (txvq == NULL)
				continue;
			for (t = 0; t < ZXDH_NB_TXQ_XSTATS; t++) {
				snprintf(xstats_names[count].name, sizeof(xstats_names[count].name),
				"tx_q%u_%s", i, zxdh_txq_stat_strings[t].name);
				count++;
			}
		}
		return count;
	}
	return nstats;
}

int
zxdh_dev_fw_version_get(struct rte_eth_dev *dev,
		 char *fw_version, size_t fw_size __rte_unused)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	void *flash_msg_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, flash_msg);
	char fw_ver[ZXDH_FWVERS_LEN] = {0};
	uint32_t ret = 0;

	zxdh_agent_msg_build(hw, ZXDH_FLASH_FIR_VERSION_GET, &msg_info);

	ret = zxdh_send_msg_to_riscv(dev, &msg_info, sizeof(struct zxdh_msg_info),
				zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info),
				ZXDH_MODULE_FLASH);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
				hw->vport.vport, ZXDH_FLASH_FIR_VERSION_GET);
		return -1;
	}

	memcpy(fw_ver, flash_msg_addr, ZXDH_FWVERS_LEN);
	snprintf(fw_version, ZXDH_FWVERS_LEN - 1, "%s", fw_ver);

	return 0;
}

static uint8_t
zxdh_en_module_eeprom_read(struct rte_eth_dev *dev,
		 struct zxdh_mac_module_eeprom_msg *query, uint8_t *data)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	uint8_t ret = 0;

	zxdh_agent_msg_build(hw, ZXDH_MAC_MODULE_EEPROM_READ, &msg_info);

	msg_info.data.module_eeprom_msg.i2c_addr = query->i2c_addr;
	msg_info.data.module_eeprom_msg.bank = query->bank;
	msg_info.data.module_eeprom_msg.page = query->page;
	msg_info.data.module_eeprom_msg.offset = query->offset;
	msg_info.data.module_eeprom_msg.length = query->length;

	ret = zxdh_send_msg_to_riscv(dev, &msg_info, sizeof(struct zxdh_msg_info),
				zxdh_msg_reply_info, ZXDH_ST_SZ_BYTES(msg_reply_info),
				ZXDH_BAR_MODULE_MAC);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
				hw->vport.vport, ZXDH_MAC_MODULE_EEPROM_READ);
		return -1;
	}
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, zxdh_msg_reply_info, reply_body);
	void *module_eeprom_msg_addr =
			ZXDH_ADDR_OF(msg_reply_body, reply_body_addr, module_eeprom_msg);
	void *agent_mac_module_eeprom_msg_data_addr =
			ZXDH_ADDR_OF(agent_mac_module_eeprom_msg, module_eeprom_msg_addr, data);
	uint8_t length = ZXDH_GET(agent_mac_module_eeprom_msg, module_eeprom_msg_addr, length);
	if (data)
		memcpy(data, agent_mac_module_eeprom_msg_data_addr, length);

	return length;
}

int
zxdh_dev_get_module_info(struct rte_eth_dev *dev,
			 struct rte_eth_dev_module_info *modinfo)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_mac_module_eeprom_msg query = {0};
	uint8_t read_bytes;
	uint8_t data[2] = {0};

	if (!hw->is_pf)
		return -EOPNOTSUPP;

	query.i2c_addr = ZXDH_SFF_I2C_ADDRESS_LOW;
	query.page = 0;
	query.offset = 0;
	query.length = 2;

	read_bytes = zxdh_en_module_eeprom_read(dev, &query, data);
	if (read_bytes != query.length) {
		PMD_DRV_LOG(ERR, "zxdh_en_module_eeprom_read failed");
		return -EIO;
	}

	switch (data[0]) {
	case ZXDH_MODULE_ID_SFP:
		modinfo->type       = RTE_ETH_MODULE_SFF_8472;
		modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8472_LEN;
		break;
	case ZXDH_MODULE_ID_QSFP:
		modinfo->type       = RTE_ETH_MODULE_SFF_8436;
		modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8436_MAX_LEN;
		break;
	case ZXDH_MODULE_ID_QSFP_PLUS:
	case ZXDH_MODULE_ID_QSFP28:
		if (data[1] < 3) {
			modinfo->type       = RTE_ETH_MODULE_SFF_8436;
			modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8436_MAX_LEN;
		} else {
			modinfo->type       = RTE_ETH_MODULE_SFF_8636;
			modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8636_MAX_LEN;
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "can not recognize module identifier 0x%x!", data[0]);
		return -EINVAL;
	}

	return 0;
}


int
zxdh_dev_get_module_eeprom(struct rte_eth_dev *dev, struct rte_dev_eeprom_info *info)
{
	struct zxdh_mac_module_eeprom_msg query = {0};
	uint32_t offset = info->offset;
	uint32_t length = info->length;
	uint32_t offset_boundary = 0;
	uint32_t total_read_bytes = 0;
	uint8_t read_bytes = 0;
	uint8_t identifier;
	uint8_t *data = NULL;

	if (!info->length)
		return -EINVAL;

	data = info->data;
	memset(data, 0, info->length);

	query.i2c_addr = ZXDH_SFF_I2C_ADDRESS_LOW;
	query.bank = 0;
	query.page = 0;
	query.offset = 0;
	query.length = 1;
	read_bytes = zxdh_en_module_eeprom_read(dev, &query, &identifier);
	if (read_bytes != query.length) {
		PMD_DRV_LOG(ERR, "read eeprom failed(read_bytes %d != query.length %d)!",
			 read_bytes, query.length);
		return -EIO;
	}

	while (total_read_bytes < info->length) {
		if (identifier == ZXDH_MODULE_ID_SFP) {
			if (offset < 256) {
				query.i2c_addr = ZXDH_SFF_I2C_ADDRESS_LOW;
				query.page = 0;
				query.offset = offset;
			} else {
				query.i2c_addr = ZXDH_SFF_I2C_ADDRESS_HIGH;
				query.page = 0;
				query.offset = offset - 256;
			}
			offset_boundary = (query.offset < 128) ? 128 : 256;
			query.length = ((query.offset + length) > offset_boundary) ?
						 (offset_boundary - query.offset) : length;
		} else if (identifier == ZXDH_MODULE_ID_QSFP ||
				identifier == ZXDH_MODULE_ID_QSFP_PLUS ||
				identifier == ZXDH_MODULE_ID_QSFP28) {
			query.i2c_addr = ZXDH_SFF_I2C_ADDRESS_LOW;
			if (offset < 256) {
				query.page = 0;
				query.offset = offset;
			} else {
				query.page = (offset - 256) / 128 + 1;
				query.offset = offset - 128 * query.page;
			}
			offset_boundary = (query.offset < 128) ? 128 : 256;
			query.length = ((query.offset + length) > offset_boundary) ?
						 (offset_boundary - query.offset) : length;
		} else {
			PMD_DRV_LOG(ERR, "can not recognize module identifier 0x%x!", identifier);
			return -EINVAL;
		}

		read_bytes = zxdh_en_module_eeprom_read(dev, &query, data + total_read_bytes);
		if (read_bytes != query.length) {
			PMD_DRV_LOG(ERR, "read eeprom failed(read_bytes %d != query.length %d)",
				 read_bytes, query.length);
			return -EIO;
		}
		total_read_bytes = (total_read_bytes + read_bytes) % UINT32_MAX;
		offset += read_bytes;
		length -= read_bytes;
	}
	return 0;
}
