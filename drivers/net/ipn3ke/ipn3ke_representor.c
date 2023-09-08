/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdint.h>
#include <unistd.h>

#include <bus_pci_driver.h>
#include <rte_ethdev.h>
#include <rte_pci.h>
#include <rte_malloc.h>

#include <rte_mbuf.h>
#include <rte_sched.h>
#include <ethdev_driver.h>
#include <rte_spinlock.h>

#include <rte_io.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <bus_ifpga_driver.h>
#include <ifpga_logs.h>

#include "ipn3ke_rawdev_api.h"
#include "ipn3ke_flow.h"
#include "ipn3ke_logs.h"
#include "ipn3ke_ethdev.h"

static int ipn3ke_rpst_scan_num;
static rte_thread_t ipn3ke_rpst_scan_thread;

/** Double linked list of representor port. */
TAILQ_HEAD(ipn3ke_rpst_list, ipn3ke_rpst);

static struct ipn3ke_rpst_list ipn3ke_rpst_list =
	TAILQ_HEAD_INITIALIZER(ipn3ke_rpst_list);

static rte_spinlock_t ipn3ke_link_notify_list_lk = RTE_SPINLOCK_INITIALIZER;

static int
ipn3ke_rpst_link_check(struct ipn3ke_rpst *rpst);

static int
ipn3ke_rpst_dev_infos_get(struct rte_eth_dev *ethdev,
	struct rte_eth_dev_info *dev_info)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);

	dev_info->speed_capa =
		(hw->retimer.mac_type ==
			IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) ?
		RTE_ETH_LINK_SPEED_10G :
		((hw->retimer.mac_type ==
			IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI) ?
		RTE_ETH_LINK_SPEED_25G :
		RTE_ETH_LINK_SPEED_AUTONEG);

	dev_info->max_rx_queues  = 1;
	dev_info->max_tx_queues  = 1;
	dev_info->min_rx_bufsize = IPN3KE_AFU_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen  = IPN3KE_AFU_FRAME_SIZE_MAX;
	dev_info->max_mac_addrs  = hw->port_num;
	dev_info->max_vfs = 0;
	dev_info->default_txconf = (struct rte_eth_txconf) {
		.offloads = 0,
	};
	dev_info->rx_queue_offload_capa = 0;
	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
		RTE_ETH_RX_OFFLOAD_QINQ_STRIP |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_VLAN_EXTEND |
		RTE_ETH_RX_OFFLOAD_VLAN_FILTER;

	dev_info->tx_queue_offload_capa = RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
		RTE_ETH_TX_OFFLOAD_QINQ_INSERT |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_TX_OFFLOAD_TCP_TSO |
		RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
		dev_info->tx_queue_offload_capa;

	dev_info->dev_capa =
		RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP |
		RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP;
	dev_info->dev_capa &= ~RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;

	dev_info->switch_info.name = ethdev->device->name;
	dev_info->switch_info.domain_id = rpst->switch_domain_id;
	dev_info->switch_info.port_id = rpst->port_id;

	return 0;
}

static int
ipn3ke_rpst_dev_configure(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ipn3ke_rpst_dev_start(struct rte_eth_dev *dev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);
	struct rte_rawdev *rawdev;
	uint64_t base_mac;
	uint32_t val;
	char attr_name[IPN3KE_RAWDEV_ATTR_LEN_MAX];
	uint16_t i;

	rawdev = hw->rawdev;

	memset(attr_name, 0, sizeof(attr_name));
	snprintf(attr_name, IPN3KE_RAWDEV_ATTR_LEN_MAX, "%s",
			"LineSideBaseMAC");
	rawdev->dev_ops->attr_get(rawdev, attr_name, &base_mac);
	rte_ether_addr_copy((struct rte_ether_addr *)&base_mac,
			&rpst->mac_addr);

	rte_ether_addr_copy(&rpst->mac_addr, &dev->data->mac_addrs[0]);
	dev->data->mac_addrs->addr_bytes[RTE_ETHER_ADDR_LEN - 1] =
		(uint8_t)rpst->port_id + 1;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Set mac address */
		rte_memcpy(((char *)(&val)),
			(char *)&dev->data->mac_addrs->addr_bytes[0],
			sizeof(uint32_t));
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_PRIMARY_MAC_ADDR0,
				rpst->port_id,
				0);
		rte_memcpy(((char *)(&val)),
			(char *)&dev->data->mac_addrs->addr_bytes[4],
			sizeof(uint16_t));
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_PRIMARY_MAC_ADDR1,
				rpst->port_id,
				0);

		/* Enable the TX path */
		ipn3ke_xmac_tx_enable(hw, rpst->port_id, 0);

		/* Disables source address override */
		ipn3ke_xmac_smac_ovd_dis(hw, rpst->port_id, 0);

		/* Enable the RX path */
		ipn3ke_xmac_rx_enable(hw, rpst->port_id, 0);

		/* Clear line side TX statistics counters */
		ipn3ke_xmac_tx_clr_10G_stcs(hw, rpst->port_id, 0);

		/* Clear line side RX statistics counters */
		ipn3ke_xmac_rx_clr_10G_stcs(hw, rpst->port_id, 0);

		/* Clear NIC side TX statistics counters */
		ipn3ke_xmac_tx_clr_10G_stcs(hw, rpst->port_id, 1);

		/* Clear NIC side RX statistics counters */
		ipn3ke_xmac_rx_clr_10G_stcs(hw, rpst->port_id, 1);
	} else if (hw->retimer.mac_type ==
				IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI) {
		/* Clear line side TX statistics counters */
		ipn3ke_xmac_tx_clr_25G_stcs(hw, rpst->port_id, 0);

		/* Clear line side RX statistics counters */
		ipn3ke_xmac_rx_clr_25G_stcs(hw, rpst->port_id, 0);

		/* Clear NIC side TX statistics counters */
		ipn3ke_xmac_tx_clr_25G_stcs(hw, rpst->port_id, 1);

		/* Clear NIC side RX statistics counters */
		ipn3ke_xmac_rx_clr_25G_stcs(hw, rpst->port_id, 1);
	}

	ipn3ke_rpst_link_update(dev, 0);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static int
ipn3ke_rpst_dev_stop(struct rte_eth_dev *dev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);
	uint16_t i;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable the TX path */
		ipn3ke_xmac_tx_disable(hw, rpst->port_id, 0);

		/* Disable the RX path */
		ipn3ke_xmac_rx_disable(hw, rpst->port_id, 0);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
ipn3ke_rpst_dev_close(struct rte_eth_dev *dev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable the TX path */
		ipn3ke_xmac_tx_disable(hw, rpst->port_id, 0);

		/* Disable the RX path */
		ipn3ke_xmac_rx_disable(hw, rpst->port_id, 0);
	}

	return 0;
}

/*
 * Reset PF device only to re-initialize resources in PMD layer
 */
static int
ipn3ke_rpst_dev_reset(struct rte_eth_dev *dev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable the TX path */
		ipn3ke_xmac_tx_disable(hw, rpst->port_id, 0);

		/* Disable the RX path */
		ipn3ke_xmac_rx_disable(hw, rpst->port_id, 0);
	}

	return 0;
}

static int
ipn3ke_rpst_rx_queue_start(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t rx_queue_id)
{
	return 0;
}

static int
ipn3ke_rpst_rx_queue_stop(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t rx_queue_id)
{
	return 0;
}

static int
ipn3ke_rpst_tx_queue_start(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t tx_queue_id)
{
	return 0;
}

static int
ipn3ke_rpst_tx_queue_stop(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t tx_queue_id)
{
	return 0;
}

static int
ipn3ke_rpst_rx_queue_setup(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t queue_idx, __rte_unused uint16_t nb_desc,
	__rte_unused unsigned int socket_id,
	__rte_unused const struct rte_eth_rxconf *rx_conf,
	__rte_unused struct rte_mempool *mp)
{
	return 0;
}

static int
ipn3ke_rpst_tx_queue_setup(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t queue_idx, __rte_unused uint16_t nb_desc,
	__rte_unused unsigned int socket_id,
	__rte_unused const struct rte_eth_txconf *tx_conf)
{
	return 0;
}

/* Statistics collected by each port, VSI, VEB, and S-channel */
struct ipn3ke_rpst_eth_stats {
	uint64_t tx_bytes;               /* gotc */
	uint64_t tx_multicast;           /* mptc */
	uint64_t tx_broadcast;           /* bptc */
	uint64_t tx_unicast;             /* uptc */
	uint64_t tx_discards;            /* tdpc */
	uint64_t tx_errors;              /* tepc */
	uint64_t rx_bytes;               /* gorc */
	uint64_t rx_multicast;           /* mprc */
	uint64_t rx_broadcast;           /* bprc */
	uint64_t rx_unicast;             /* uprc */
	uint64_t rx_discards;            /* rdpc */
	uint64_t rx_unknown_protocol;    /* rupp */
};

/* store statistics names and its offset in stats structure */
struct ipn3ke_rpst_xstats_name_offset {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

static const struct ipn3ke_rpst_xstats_name_offset
ipn3ke_rpst_stats_strings[] = {
	{"tx_multicast_packets",          offsetof(struct ipn3ke_rpst_eth_stats,
							tx_multicast)},
	{"tx_broadcast_packets",          offsetof(struct ipn3ke_rpst_eth_stats,
							tx_broadcast)},
	{"tx_unicast_packets",            offsetof(struct ipn3ke_rpst_eth_stats,
							tx_unicast)},
	{"tx_dropped_packets",            offsetof(struct ipn3ke_rpst_eth_stats,
							tx_discards)},
	{"rx_multicast_packets",          offsetof(struct ipn3ke_rpst_eth_stats,
							rx_multicast)},
	{"rx_broadcast_packets",          offsetof(struct ipn3ke_rpst_eth_stats,
							rx_broadcast)},
	{"rx_unicast_packets",            offsetof(struct ipn3ke_rpst_eth_stats,
							rx_unicast)},
	{"rx_dropped_packets",            offsetof(struct ipn3ke_rpst_eth_stats,
							rx_discards)},
	{"rx_unknown_protocol_packets", offsetof(struct ipn3ke_rpst_eth_stats,
							rx_unknown_protocol)},
};

#define IPN3KE_RPST_ETH_XSTATS_CNT (sizeof(ipn3ke_rpst_stats_strings) / \
		sizeof(ipn3ke_rpst_stats_strings[0]))

#define IPN3KE_RPST_PRIO_XSTATS_CNT    8

/* Statistics collected by the MAC */
struct ipn3ke_rpst_hw_port_stats {
	/* eth stats collected by the port */
	struct ipn3ke_rpst_eth_stats eth;

	/* additional port specific stats */
	uint64_t tx_dropped_link_down;
	uint64_t crc_errors;
	uint64_t illegal_bytes;
	uint64_t error_bytes;
	uint64_t mac_local_faults;
	uint64_t mac_remote_faults;
	uint64_t rx_length_errors;
	uint64_t link_xon_rx;
	uint64_t link_xoff_rx;
	uint64_t priority_xon_rx[IPN3KE_RPST_PRIO_XSTATS_CNT];
	uint64_t priority_xoff_rx[IPN3KE_RPST_PRIO_XSTATS_CNT];
	uint64_t link_xon_tx;
	uint64_t link_xoff_tx;
	uint64_t priority_xon_tx[IPN3KE_RPST_PRIO_XSTATS_CNT];
	uint64_t priority_xoff_tx[IPN3KE_RPST_PRIO_XSTATS_CNT];
	uint64_t priority_xon_2_xoff[IPN3KE_RPST_PRIO_XSTATS_CNT];
	uint64_t rx_size_64;
	uint64_t rx_size_65_127;
	uint64_t rx_size_128_255;
	uint64_t rx_size_256_511;
	uint64_t rx_size_512_1023;
	uint64_t rx_size_1024_1518;
	uint64_t rx_size_big;
	uint64_t rx_undersize;
	uint64_t rx_fragments;
	uint64_t rx_oversize;
	uint64_t rx_jabber;
	uint64_t tx_size_64;
	uint64_t tx_size_65_127;
	uint64_t tx_size_128_255;
	uint64_t tx_size_256_511;
	uint64_t tx_size_512_1023;
	uint64_t tx_size_1024_1518;
	uint64_t tx_size_1519_to_max;
	uint64_t mac_short_packet_dropped;
	uint64_t checksum_error;
	/* flow director stats */
	uint64_t fd_atr_match;
	uint64_t fd_sb_match;
	uint64_t fd_atr_tunnel_match;
	uint32_t fd_atr_status;
	uint32_t fd_sb_status;
	/* EEE LPI */
	uint32_t tx_lpi_status;
	uint32_t rx_lpi_status;
	uint64_t tx_lpi_count;
	uint64_t rx_lpi_count;
};

static const struct ipn3ke_rpst_xstats_name_offset
ipn3ke_rpst_hw_port_strings[] = {
	{"tx_link_down_dropped",      offsetof(struct ipn3ke_rpst_hw_port_stats,
						tx_dropped_link_down)},
	{"rx_crc_errors",             offsetof(struct ipn3ke_rpst_hw_port_stats,
						crc_errors)},
	{"rx_illegal_byte_errors",    offsetof(struct ipn3ke_rpst_hw_port_stats,
						illegal_bytes)},
	{"rx_error_bytes",            offsetof(struct ipn3ke_rpst_hw_port_stats,
						error_bytes)},
	{"mac_local_errors",          offsetof(struct ipn3ke_rpst_hw_port_stats,
						mac_local_faults)},
	{"mac_remote_errors",         offsetof(struct ipn3ke_rpst_hw_port_stats,
						mac_remote_faults)},
	{"rx_length_errors",          offsetof(struct ipn3ke_rpst_hw_port_stats,
						rx_length_errors)},
	{"tx_xon_packets",            offsetof(struct ipn3ke_rpst_hw_port_stats,
						link_xon_tx)},
	{"rx_xon_packets",            offsetof(struct ipn3ke_rpst_hw_port_stats,
						link_xon_rx)},
	{"tx_xoff_packets",           offsetof(struct ipn3ke_rpst_hw_port_stats,
						link_xoff_tx)},
	{"rx_xoff_packets",           offsetof(struct ipn3ke_rpst_hw_port_stats,
						link_xoff_rx)},
	{"rx_size_64_packets",        offsetof(struct ipn3ke_rpst_hw_port_stats,
						rx_size_64)},
	{"rx_size_65_to_127_packets", offsetof(struct ipn3ke_rpst_hw_port_stats,
						rx_size_65_127)},
	{"rx_size_128_to_255_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 rx_size_128_255)},
	{"rx_size_256_to_511_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 rx_size_256_511)},
	{"rx_size_512_to_1023_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 rx_size_512_1023)},
	{"rx_size_1024_to_1518_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 rx_size_1024_1518)},
	{"rx_size_1519_to_max_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 rx_size_big)},
	{"rx_undersized_errors",      offsetof(struct ipn3ke_rpst_hw_port_stats,
					       rx_undersize)},
	{"rx_oversize_errors",        offsetof(struct ipn3ke_rpst_hw_port_stats,
					       rx_oversize)},
	{"rx_mac_short_dropped",      offsetof(struct ipn3ke_rpst_hw_port_stats,
					       mac_short_packet_dropped)},
	{"rx_fragmented_errors",      offsetof(struct ipn3ke_rpst_hw_port_stats,
					       rx_fragments)},
	{"rx_jabber_errors",          offsetof(struct ipn3ke_rpst_hw_port_stats,
					       rx_jabber)},
	{"tx_size_64_packets",        offsetof(struct ipn3ke_rpst_hw_port_stats,
					       tx_size_64)},
	{"tx_size_65_to_127_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 tx_size_65_127)},
	{"tx_size_128_to_255_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 tx_size_128_255)},
	{"tx_size_256_to_511_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 tx_size_256_511)},
	{"tx_size_512_to_1023_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 tx_size_512_1023)},
	{"tx_size_1024_to_1518_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 tx_size_1024_1518)},
	{"tx_size_1519_to_max_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 tx_size_1519_to_max)},
	{"rx_flow_director_atr_match_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 fd_atr_match)},
	{"rx_flow_director_sb_match_packets",
				offsetof(struct ipn3ke_rpst_hw_port_stats,
					 fd_sb_match)},
	{"tx_low_power_idle_status",  offsetof(struct ipn3ke_rpst_hw_port_stats,
					       tx_lpi_status)},
	{"rx_low_power_idle_status",  offsetof(struct ipn3ke_rpst_hw_port_stats,
					       rx_lpi_status)},
	{"tx_low_power_idle_count",   offsetof(struct ipn3ke_rpst_hw_port_stats,
					       tx_lpi_count)},
	{"rx_low_power_idle_count",   offsetof(struct ipn3ke_rpst_hw_port_stats,
					       rx_lpi_count)},
};

#define IPN3KE_RPST_HW_PORT_XSTATS_CNT (sizeof(ipn3ke_rpst_hw_port_strings) \
		/ sizeof(ipn3ke_rpst_hw_port_strings[0]))

static const struct ipn3ke_rpst_xstats_name_offset
ipn3ke_rpst_rxq_prio_strings[] = {
	{"xon_packets",               offsetof(struct ipn3ke_rpst_hw_port_stats,
					       priority_xon_rx)},
	{"xoff_packets",              offsetof(struct ipn3ke_rpst_hw_port_stats,
					       priority_xoff_rx)},
};

#define IPN3KE_RPST_RXQ_PRIO_XSTATS_CNT (sizeof(ipn3ke_rpst_rxq_prio_strings) \
		/ sizeof(ipn3ke_rpst_rxq_prio_strings[0]))

static const struct ipn3ke_rpst_xstats_name_offset
ipn3ke_rpst_txq_prio_strings[] = {
	{"xon_packets",               offsetof(struct ipn3ke_rpst_hw_port_stats,
					       priority_xon_tx)},
	{"xoff_packets",              offsetof(struct ipn3ke_rpst_hw_port_stats,
					       priority_xoff_tx)},
	{"xon_to_xoff_packets",       offsetof(struct ipn3ke_rpst_hw_port_stats,
					       priority_xon_2_xoff)},
};

#define IPN3KE_RPST_TXQ_PRIO_XSTATS_CNT (sizeof(ipn3ke_rpst_txq_prio_strings) \
		/ sizeof(ipn3ke_rpst_txq_prio_strings[0]))

static uint32_t
ipn3ke_rpst_xstats_calc_num(void)
{
	return IPN3KE_RPST_ETH_XSTATS_CNT
		+ IPN3KE_RPST_HW_PORT_XSTATS_CNT
		+ (IPN3KE_RPST_RXQ_PRIO_XSTATS_CNT
			* IPN3KE_RPST_PRIO_XSTATS_CNT)
		+ (IPN3KE_RPST_TXQ_PRIO_XSTATS_CNT
			* IPN3KE_RPST_PRIO_XSTATS_CNT);
}

static void
ipn3ke_rpst_25g_nic_side_tx_stats_reset(struct ipn3ke_hw *hw,
uint16_t port_id)
{
	uint32_t tmp = 0x00000001;
	/* Bit[0]: Software can set this bit to the value of 1
	 * to reset all of the TX statistics registers at the same time.
	 * This bit is selfclearing.
	 */
	(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_TX_STATISTICS_CONFIG,
			port_id,
			1);

	while (tmp & 0x00000001) {
		tmp = 0x00000000;
		(*hw->f_mac_read)(hw,
				&tmp,
				IPN3KE_25G_TX_STATISTICS_CONFIG,
				port_id,
				1);
		if (tmp & 0x00000001)
			usleep(5);
		else
			return;
	}
}

static void
ipn3ke_rpst_25g_nic_side_rx_stats_reset(struct ipn3ke_hw *hw,
uint16_t port_id)
{
	uint32_t tmp = 0x00000001;
	/* Bit[0]: Software can set this bit to the value of 1
	 * to reset all of the RX statistics registers at the same time.
	 * This bit is selfclearing.
	 */
	(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_RX_STATISTICS_CONFIG,
			port_id,
			1);

	while (tmp & 0x00000001) {
		tmp = 0x00000000;
		(*hw->f_mac_read)(hw,
				&tmp,
				IPN3KE_25G_RX_STATISTICS_CONFIG,
				port_id,
				1);
		if (tmp & 0x00000001)
			usleep(5);
		else
			return;
	}
}

static void
ipn3ke_rpst_10g_nic_side_tx_stats_reset(struct ipn3ke_hw *hw,
uint16_t port_id)
{
	uint32_t tmp;

	/*Bit [0]: Set this register to 1 to clear all TX statistics
	 *counters.
	 *The IP core clears this bit when all counters are cleared.
	 *Bits [31:1]: Reserved.
	 */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
		&tmp,
		IPN3KE_10G_TX_STATS_CLR,
		port_id,
		1);
	tmp |= 0x00000001;
	(*hw->f_mac_write)(hw,
		tmp,
		IPN3KE_10G_TX_STATS_CLR,
		port_id,
		1);
}

static void
ipn3ke_rpst_10g_nic_side_rx_stats_reset(struct ipn3ke_hw *hw,
uint16_t port_id)
{
	uint32_t tmp;

	/*Bit [0]: Set this register to 1 to clear all RX statistics
	 *counters.
	 *The IP core clears this bit when all counters are cleared.
	 *Bits [31:1]: Reserved
	 */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
		&tmp,
		IPN3KE_10G_RX_STATS_CLR,
		port_id,
		1);
	tmp |= 0x00000001;
	(*hw->f_mac_write)(hw,
		tmp,
		IPN3KE_10G_RX_STATS_CLR,
		port_id,
		1);
}

static uint64_t
ipn3ke_rpst_read_64bits_statistics_register(uint32_t addr_lo,
uint32_t addr_hi, struct ipn3ke_hw *hw, uint16_t port_id)
{
	uint32_t statistics_lo = 0x00000000;
	uint32_t statistics_hi = 0x00000000;
	uint64_t statistics = 0x0000000000000000;

	(*hw->f_mac_read)(hw,
			&statistics_lo,
			addr_lo,
			port_id,
			0);

	(*hw->f_mac_read)(hw,
			&statistics_hi,
			addr_hi,
			port_id,
			0);

	statistics += statistics_hi;
	statistics = statistics << IPN3KE_REGISTER_WIDTH;
	statistics += statistics_lo;
	return statistics;

}

static int
ipn3ke_rpst_read_25g_lineside_stats_registers
(struct ipn3ke_hw *hw,
uint16_t port_id,
struct ipn3ke_rpst_hw_port_stats *hw_stats)
{
	uint32_t tmp;
	uint64_t statistics;

	memset(hw_stats, 0, sizeof(*hw_stats));

	/*check Tx statistics is real time.
	 *if statistics has been paused, make it real time.
	 */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
			&tmp,
			IPN3KE_25G_TX_STATISTICS_CONFIG,
			port_id,
			0);

	if (tmp & IPN3KE_25G_TX_STATISTICS_CONFIG_SHADOW_REQUEST_MASK) {
		tmp &= 0xfffffffb;
		(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_TX_STATISTICS_CONFIG,
			port_id,
			0);
	}

	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
		&tmp,
		IPN3KE_25G_TX_STATISTICS_STATUS,
		port_id,
		0);
	if (tmp & IPN3KE_25G_TX_STATISTICS_STATUS_SHADOW_REQUEST_MASK) {
		tmp = 0x00000000;
		(*hw->f_mac_read)(hw,
			&tmp,
			IPN3KE_25G_TX_STATISTICS_CONFIG,
			port_id,
			0);
		tmp &= 0xfffffffb;
		(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_TX_STATISTICS_CONFIG,
			port_id,
			0);
	}

	/*check Rx statistics is real time.
	 *if statistics has been paused, make it real time.
	 */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
			&tmp,
			IPN3KE_25G_RX_STATISTICS_CONFIG,
			port_id,
			0);
	if (tmp & IPN3KE_25G_RX_STATISTICS_CONFIG_SHADOW_REQUEST_MASK) {
		tmp &= 0xfffffffb;
		(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_RX_STATISTICS_CONFIG,
			port_id,
			0);
	}

	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
		&tmp,
		IPN3KE_25G_RX_STATISTICS_STATUS,
		port_id,
		0);

	if (tmp & IPN3KE_25G_RX_STATISTICS_STATUS_SHADOW_REQUEST_MASK) {
		tmp = 0x00000000;
		(*hw->f_mac_read)(hw,
			&tmp,
			IPN3KE_25G_RX_STATISTICS_CONFIG,
			port_id,
			0);
		tmp &= 0xfffffffb;
		(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_RX_STATISTICS_CONFIG,
			port_id,
			0);
	}

	/* pause Tx counter to read the statistics */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
		&tmp,
		IPN3KE_25G_TX_STATISTICS_CONFIG,
		port_id,
		0);
	tmp |= 0x00000004;
	(*hw->f_mac_write)(hw,
		tmp,
		IPN3KE_25G_TX_STATISTICS_CONFIG,
		port_id,
		0);

	/* pause Rx counter to read the statistics */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
		&tmp,
		IPN3KE_25G_RX_STATISTICS_CONFIG,
		port_id,
		0);
	tmp |= 0x00000004;
	(*hw->f_mac_write)(hw,
		tmp,
		IPN3KE_25G_RX_STATISTICS_CONFIG,
		port_id,
		0);

	/*Number of transmitted frames less than 64 bytes
	 *and reporting a CRC error
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_FRAGMENTS_LO,
		IPN3KE_25G_CNTR_TX_FRAGMENTS_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;
	hw_stats->crc_errors += statistics;

	/*Number of transmitted oversized frames reporting a CRC error*/
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_JABBERS_LO,
		IPN3KE_25G_CNTR_TX_JABBERS_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;
	hw_stats->crc_errors += statistics;

	/* Number of transmitted packets with FCS errors */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_FCS_LO,
		IPN3KE_25G_CNTR_TX_FCS_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;
	hw_stats->checksum_error += statistics;

	/*Number of transmitted frames with a frame of length at
	 *least 64 reporting a CRC error
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_CRCERR_LO,
		IPN3KE_25G_CNTR_TX_CRCERR_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;
	hw_stats->crc_errors += statistics;

	/*Number of errored multicast frames transmitted,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_MCAST_DATA_ERR_LO,
		IPN3KE_25G_CNTR_TX_MCAST_DATA_ERR_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;

	/*Number of errored broadcast frames transmitted,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_BCAST_DATA_ERR_LO,
		IPN3KE_25G_CNTR_TX_BCAST_DATA_ERR_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;

	/*Number of errored unicast frames transmitted,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_UCAST_DATA_ERR_LO,
		IPN3KE_25G_CNTR_TX_UCAST_DATA_ERR_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;

	/* Number of errored multicast control frames transmitted */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_MCAST_CTRL_ERR_LO,
		IPN3KE_25G_CNTR_TX_MCAST_CTRL_ERR_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;

	/* Number of errored broadcast control frames transmitted */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_BCAST_CTRL_ERR_LO,
		IPN3KE_25G_CNTR_TX_BCAST_CTRL_ERR_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;

	/* Number of errored unicast control frames transmitted */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_UCAST_CTRL_ERR_LO,
		IPN3KE_25G_CNTR_TX_UCAST_CTRL_ERR_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;

	/* Number of errored pause frames transmitted */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_PAUSE_ERR_LO,
		IPN3KE_25G_CNTR_TX_PAUSE_ERR_HI,
		hw, port_id);
	hw_stats->eth.tx_errors += statistics;

	/*Number of 64-byte transmitted frames,
	 *including the CRC field but excluding the preamble
	 *and SFD bytes
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_64B_LO,
		IPN3KE_25G_CNTR_TX_64B_HI,
		hw, port_id);
	hw_stats->tx_size_64 += statistics;

	/* Number of transmitted frames between 65 and 127 bytes */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_65_127B_LO,
		IPN3KE_25G_CNTR_TX_65_127B_HI,
		hw, port_id);
	hw_stats->tx_size_65_127 += statistics;

	/* Number of transmitted frames between 128 and 255 bytes */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_128_255B_LO,
		IPN3KE_25G_CNTR_TX_128_255B_HI,
		hw, port_id);
	hw_stats->tx_size_128_255 += statistics;

	/* Number of transmitted frames between 256 and 511 bytes */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_256_511B_LO,
		IPN3KE_25G_CNTR_TX_256_511B_HI,
		hw, port_id);
	hw_stats->tx_size_256_511 += statistics;

	/* Number of transmitted frames between 512 and 1023 bytes */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_512_1023B_LO,
		IPN3KE_25G_CNTR_TX_512_1023B_HI,
		hw, port_id);
	hw_stats->tx_size_512_1023 += statistics;

	/* Number of transmitted frames between 1024 and 1518 bytes */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_1024_1518B_LO,
		IPN3KE_25G_CNTR_TX_1024_1518B_HI,
		hw, port_id);
	hw_stats->tx_size_1024_1518 += statistics;

	/*Number of transmitted frames of size between 1519 bytes
	 *and the number of bytes specified in the MAX_TX_SIZE_CONFIG
	 *register
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_1519_MAXB_LO,
		IPN3KE_25G_CNTR_TX_1519_MAXB_HI,
		hw, port_id);
	hw_stats->tx_size_1519_to_max += statistics;

	/*Number of oversized frames (frames with more bytes than the
	 *number specified in the MAX_TX_SIZE_CONFIG register)
	 *transmitted
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_OVERSIZE_LO,
		IPN3KE_25G_CNTR_TX_OVERSIZE_HI,
		hw, port_id);

	/*Number of valid multicast frames transmitted,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_MCAST_DATA_OK_LO,
		IPN3KE_25G_CNTR_TX_MCAST_DATA_OK_HI,
		hw, port_id);
	hw_stats->eth.tx_multicast += statistics;

	/*Number of valid broadcast frames transmitted,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_BCAST_DATA_OK_LO,
		IPN3KE_25G_CNTR_TX_BCAST_DATA_OK_HI,
		hw, port_id);
	hw_stats->eth.tx_broadcast += statistics;

	/*Number of valid unicast frames transmitted,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_UCAST_DATA_OK_LO,
		IPN3KE_25G_CNTR_TX_UCAST_DATA_OK_HI,
		hw, port_id);
	hw_stats->eth.tx_unicast += statistics;

	/*Number of valid multicast frames transmitted,
	 *excluding data frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_MCAST_CTRL_LO,
		IPN3KE_25G_CNTR_TX_MCAST_CTRL_HI,
		hw, port_id);
	hw_stats->eth.tx_multicast += statistics;

	/*Number of valid broadcast frames transmitted,
	 *excluding data frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_BCAST_CTRL_LO,
		IPN3KE_25G_CNTR_TX_BCAST_CTRL_HI,
		hw, port_id);
	hw_stats->eth.tx_broadcast += statistics;

	/*Number of valid unicast frames transmitted,
	 *excluding data frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_UCAST_CTRL_LO,
		IPN3KE_25G_CNTR_TX_UCAST_CTRL_HI,
		hw, port_id);
	hw_stats->eth.tx_unicast += statistics;

	/* Number of valid pause frames transmitted */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_PAUSE_LO,
		IPN3KE_25G_CNTR_TX_PAUSE_HI,
		hw, port_id);

	/*Number of transmitted runt packets. The IP core does not
	 *transmit frames of length less than nine bytes.
	 *The IP core pads frames of length nine bytes to 64 bytes to
	 *extend them to 64 bytes. Therefore, this counter does not
	 *increment in normal operating conditions.
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_TX_RUNT_LO,
		IPN3KE_25G_CNTR_TX_RUNT_HI,
		hw, port_id);

	/*Number of transmitted payload bytes in frames with no FCS,
	 *undersized, oversized, or payload length errors.
	 *If VLAN detection is turned off for the TX MAC (bit[1]
	 *of the TX_MAC_CONTROL register at offset 0x40A has
	 *the value of 1), the IP core counts the VLAN header bytes
	 *(4 bytes for VLAN and 8 bytes for stacked VLAN)
	 *as payload bytes. This register is compliant with
	 *the requirements for aOctetsTransmittedOK in section
	 *5.2.2.1.8 of the IEEE Standard 802.3-2008.
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_TX_PAYLOAD_OCTETS_OK_LO,
		IPN3KE_25G_TX_PAYLOAD_OCTETS_OK_HI,
		hw, port_id);
	hw_stats->eth.tx_bytes += statistics;

	/*Number of transmitted bytes in frames with no FCS, undersized,
	 *oversized, or payload length errors. This register is
	 *compliant with the requirements for ifOutOctets in RFC3635
	 *(Managed Objects for Ethernet-like Interface Types)
	 *and TX etherStatsOctets in RFC2819(Remote Network Monitoring
	 *Management Information Base (RMON)).
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_TX_FRAME_OCTETS_OK_LO,
		IPN3KE_25G_TX_FRAME_OCTETS_OK_HI,
		hw, port_id);

	/*Number of received frames less than 64 bytes
	 *and reporting a CRC error
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_FRAGMENTS_LO,
		IPN3KE_25G_CNTR_RX_FRAGMENTS_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;
	hw_stats->crc_errors += statistics;
	hw_stats->rx_length_errors += statistics;

	/* Number of received oversized frames reporting a CRC error */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_JABBERS_LO,
		IPN3KE_25G_CNTR_RX_JABBERS_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;
	hw_stats->crc_errors += statistics;
	hw_stats->rx_length_errors += statistics;

	/*Number of received packets with FCS errors.
	 *This register maintains a count of the number of pulses
	 *on the "l<n>_rx_fcs_error" or "rx_fcs_error" output signal
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_FCS_LO,
		IPN3KE_25G_CNTR_RX_FCS_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;
	hw_stats->checksum_error += statistics;

	/*Number of received frames with a frame of length at least 64
	 *with CRC error
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_CRCERR_LO,
		IPN3KE_25G_CNTR_RX_CRCERR_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;
	hw_stats->crc_errors += statistics;

	/*Number of errored multicast frames received,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_MCAST_DATA_ERR_LO,
		IPN3KE_25G_CNTR_RX_MCAST_DATA_ERR_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;

	/*Number of errored broadcast frames received,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_BCAST_DATA_ERR_LO,
		IPN3KE_25G_CNTR_RX_BCAST_DATA_ERR_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;

	/*Number of errored unicast frames received,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_UCAST_DATA_ERR_LO,
		IPN3KE_25G_CNTR_RX_UCAST_DATA_ERR_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;

	/* Number of errored multicast control frames received */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_MCAST_CTRL_ERR_LO,
		IPN3KE_25G_CNTR_RX_MCAST_CTRL_ERR_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;

	/* Number of errored broadcast control frames received */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_BCAST_CTRL_ERR_LO,
		IPN3KE_25G_CNTR_RX_BCAST_CTRL_ERR_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;

	/* Number of errored unicast control frames received */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_UCAST_CTRL_ERR_LO,
		IPN3KE_25G_CNTR_RX_UCAST_CTRL_ERR_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;

	/* Number of errored pause frames received */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_PAUSE_ERR_LO,
		IPN3KE_25G_CNTR_RX_PAUSE_ERR_HI,
		hw, port_id);
	hw_stats->eth.rx_discards += statistics;

	/*Number of 64-byte received frames,
	 *including the CRC field but excluding the preamble
	 *and SFD bytes
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_64B_LO,
		IPN3KE_25G_CNTR_RX_64B_HI,
		hw, port_id);
	hw_stats->rx_size_64 += statistics;

	/*Number of received frames between 65 and 127 bytes */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_65_127B_LO,
		IPN3KE_25G_CNTR_RX_65_127B_HI,
		hw, port_id);
	hw_stats->rx_size_65_127 += statistics;

	/*Number of received frames between 128 and 255 bytes
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_128_255B_LO,
		IPN3KE_25G_CNTR_RX_128_255B_HI,
		hw, port_id);
	hw_stats->rx_size_128_255 += statistics;

	/*Number of received frames between 256 and 511 bytes
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_256_511B_LO,
		IPN3KE_25G_CNTR_RX_256_511B_HI,
		hw, port_id);
	hw_stats->rx_size_256_511 += statistics;

	/*Number of received frames between 512 and 1023 bytes
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_512_1023B_LO,
		IPN3KE_25G_CNTR_RX_512_1023B_HI,
		hw, port_id);
	hw_stats->rx_size_512_1023 += statistics;

	/*Number of received frames between 1024 and 1518 bytes
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_1024_1518B_LO,
		IPN3KE_25G_CNTR_RX_1024_1518B_HI,
		hw, port_id);
	hw_stats->rx_size_1024_1518 += statistics;

	/*Number of received frames of size between 1519 bytes
	 *and the number of bytes specified in the MAX_TX_SIZE_CONFIG
	 *register
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_1519_MAXB_LO,
		IPN3KE_25G_CNTR_RX_1519_MAXB_HI,
		hw, port_id);
	hw_stats->rx_size_big += statistics;

	/*Number of oversized frames (frames with more bytes
	 *than the number specified in the MAX_TX_SIZE_CONFIG register)
	 *received
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_OVERSIZE_LO,
		IPN3KE_25G_CNTR_RX_OVERSIZE_HI,
		hw, port_id);
	hw_stats->rx_jabber += statistics;

	/*Number of valid multicast frames received,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_MCAST_DATA_OK_LO,
		IPN3KE_25G_CNTR_RX_MCAST_DATA_OK_HI,
		hw, port_id);
	hw_stats->eth.rx_multicast += statistics;

	/*Number of valid broadcast frames received,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_BCAST_DATA_OK_LO,
		IPN3KE_25G_CNTR_RX_BCAST_DATA_OK_HI,
		hw, port_id);
	hw_stats->eth.rx_broadcast += statistics;

	/*Number of valid unicast frames received,
	 *excluding control frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_UCAST_DATA_OK_LO,
		IPN3KE_25G_CNTR_RX_UCAST_DATA_OK_HI,
		hw, port_id);
	hw_stats->eth.rx_unicast += statistics;

	/*Number of valid multicast frames received,
	 *excluding data frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_MCAST_CTRL_LO,
		IPN3KE_25G_CNTR_RX_MCAST_CTRL_HI,
		hw, port_id);
	hw_stats->eth.rx_multicast += statistics;

	/*Number of valid broadcast frames received,
	 *excluding data frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_BCAST_CTRL_LO,
		IPN3KE_25G_CNTR_RX_BCAST_CTRL_HI,
		hw, port_id);
	hw_stats->eth.rx_broadcast += statistics;

	/*Number of valid unicast frames received,
	 *excluding data frames
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_UCAST_CTRL_LO,
		IPN3KE_25G_CNTR_RX_UCAST_CTRL_HI,
		hw, port_id);
	hw_stats->eth.rx_unicast += statistics;

	/*Number of received pause frames, with or without error
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_PAUSE_LO,
		IPN3KE_25G_CNTR_RX_PAUSE_HI,
		hw, port_id);

	/*Number of received runt packets. A runt is a packet of size
	 *less than 64 bytes but greater than eight bytes.
	 *If a packet is eight bytes or smaller, it is considered
	 *a decoding error and not a runt frame, and the IP core
	 *does not flag it nor count it as a runt.
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_CNTR_RX_RUNT_LO,
		IPN3KE_25G_CNTR_RX_RUNT_HI,
		hw, port_id);

	/*Number of received payload bytes in frames with no FCS,
	 *undersized, oversized, or payload length errors.
	 *If VLAN detection is turned off for the RX MAC (bit [1] of the
	 *"RXMAC_CONTROL" register at offset 0x50A has the value of 1),
	 *the IP core counts the VLAN header bytes (4 bytes for VLAN and
	 *8 bytes for stacked VLAN) as payload bytes.
	 *This register is compliant with the requirements for
	 *aOctetsReceivedOK in section 5.2.2.1.14 of the IEEE Standard
	 *802.3-2008
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_RX_PAYLOAD_OCTETS_OK_LO,
		IPN3KE_25G_RX_PAYLOAD_OCTETS_OK_HI,
		hw, port_id);
	hw_stats->eth.rx_bytes += statistics;

	/*Number of received bytes in frames with no FCS, undersized,
	 *oversized, or payload length errors.
	 *This register is compliant with the requirements for
	 *ifInOctets in RFC3635 (Managed Objects for Ethernet-like
	 *Interface Types) and RX etherStatsOctets in RFC2819
	 *(Remote Network Monitoring Management Information Base
	 *(RMON)).
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_25G_RX_FRAME_OCTETS_OK_LO,
		IPN3KE_25G_RX_FRAME_OCTETS_OK_HI,
		hw, port_id);

	/*resume Tx counter to real time
	 */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
			&tmp,
			IPN3KE_25G_TX_STATISTICS_CONFIG,
			port_id,
			0);
	tmp &= 0xfffffffb;
	(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_TX_STATISTICS_CONFIG,
			port_id,
			0);

	/*resume Rx counter to real time
	 */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
			&tmp,
			IPN3KE_25G_RX_STATISTICS_CONFIG,
			port_id,
			0);
	tmp &= 0xfffffffb;
	(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_RX_STATISTICS_CONFIG,
			port_id,
			0);

	return 0;
}

static void
ipn3ke_rpst_25g_lineside_tx_stats_reset(struct ipn3ke_hw *hw,
uint16_t port_id)
{
	uint32_t tmp = 0x00000001;
	/* Bit[0]: Software can set this bit to the value of 1
	 * to reset all of the TX statistics registers at the same time.
	 * This bit is selfclearing.
	 */
	(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_TX_STATISTICS_CONFIG,
			port_id,
			0);

	while (tmp & 0x00000001) {
		tmp = 0x00000000;
		(*hw->f_mac_read)(hw,
				&tmp,
				IPN3KE_25G_TX_STATISTICS_CONFIG,
				port_id,
				0);
		if (tmp & 0x00000001)
			usleep(5);
		else
			return;
	}
}

static void
ipn3ke_rpst_25g_lineside_rx_stats_reset(struct ipn3ke_hw *hw,
uint16_t port_id)
{
	uint32_t tmp = 0x00000001;
	/* Bit[0]: Software can set this bit to the value of 1
	 * to reset all of the RX statistics registers at the same time.
	 * This bit is selfclearing.
	 */
	(*hw->f_mac_write)(hw,
			tmp,
			IPN3KE_25G_RX_STATISTICS_CONFIG,
			port_id,
			0);

	while (tmp & 0x00000001) {
		tmp = 0x00000000;
		(*hw->f_mac_read)(hw,
				&tmp,
				IPN3KE_25G_RX_STATISTICS_CONFIG,
				port_id,
				0);
		if (tmp & 0x00000001)
			usleep(5);
		else
			return;
	}
}

static uint64_t
ipn3ke_rpst_read_36bits_statistics_register(uint32_t addr_lo,
uint32_t addr_hi, struct ipn3ke_hw *hw, uint16_t port_id)
{
	uint32_t statistics_lo = 0x00000000;
	uint32_t statistics_hi = 0x00000000;
	uint64_t statistics = 0x0000000000000000;

	(*hw->f_mac_read)(hw,
			&statistics_lo,
			addr_lo,
			port_id,
			0);
	(*hw->f_mac_read)(hw,
			&statistics_hi,
			addr_hi,
			port_id,
			0);
	statistics_hi &= IPN3KE_10G_STATS_HI_VALID_MASK;
	statistics += statistics_hi;
	statistics = statistics << IPN3KE_REGISTER_WIDTH;
	statistics += statistics_lo;
	return statistics;
}

static int
ipn3ke_rpst_read_10g_lineside_stats_registers
(struct ipn3ke_hw *hw,
uint16_t port_id,
struct ipn3ke_rpst_hw_port_stats *hw_stats,
struct rte_eth_stats *stats)
{
	uint64_t statistics = 0;

	memset(hw_stats, 0, sizeof(*hw_stats));
	memset(stats, 0, sizeof(*stats));

	/*36-bit statistics counter that collects the number of frames
	 *that are successfully transmitted, including control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_FRAME_OK_LO,
		IPN3KE_10G_TX_STATS_FRAME_OK_HI,
		hw, port_id);
	stats->opackets = statistics;

	/*36-bit statistics counter that collects the number of frames
	 *that are successfully received, including control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_FRAME_OK_LO,
		IPN3KE_10G_RX_STATS_FRAME_OK_HI,
		hw, port_id);
	stats->ipackets = statistics;

	/*36-bit statistics counter that collects the number of frames
	 *transmitted with error, including control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_FRAME_ERR_LO,
		IPN3KE_10G_TX_STATS_FRAME_ERR_HI,
		hw, port_id);
	stats->oerrors = statistics;
	hw_stats->eth.tx_errors = statistics;

	/*36-bit statistics counter that collects the number of frames
	 *received with error, including control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_FRAME_ERR_LO,
		IPN3KE_10G_RX_STATS_FRAME_ERR_HI,
		hw, port_id);
	stats->ierrors = statistics;
	hw_stats->eth.rx_discards = statistics;

	/*36-bit statistics counter that collects the number
	 *of RX frames with CRC error.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_FRAME_CRC_ERR_LO,
		IPN3KE_10G_RX_STATS_FRAME_CRC_ERR_HI,
		hw, port_id);
	hw_stats->crc_errors = statistics;

	/*64-bit statistics counter that collects the payload length,
	 *including the bytes in control frames.
	 *The payload length is the number of data and padding bytes
	 *transmitted.
	 *If the tx_vlan_detection[0] register bit is set to 1,
	 *the VLAN and stacked VLAN tags are counted as part of
	 *the TX payload.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_OCTETS_OK_LO,
		IPN3KE_10G_TX_STATS_OCTETS_OK_HI,
		hw, port_id);
	stats->obytes = statistics;
	hw_stats->eth.tx_bytes = statistics;

	/*64-bit statistics counter that collects the payload length,
	 *including the bytes in control frames.
	 *The payload length is the number of data and padding bytes
	 *received.
	 *If the rx_vlan_detection[0] register bit is set to 1,
	 *the VLAN and stacked VLAN tags are counted as part of
	 *the RX payload.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_OCTETS_OK_LO,
		IPN3KE_10G_RX_STATS_OCTETS_OK_HI,
		hw, port_id);
	stats->ibytes = statistics;
	hw_stats->eth.rx_bytes = statistics;

	/*36-bit statistics counter that collects the number of
	 *valid pause frames transmitted.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_PAUSE_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_TX_STATS_PAUSE_MAC_CTRL_FRAMES_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *valid pause frames received.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_PAUSE_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_RX_STATS_PAUSE_MAC_CTRL_FRAMES_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of frames
	 *transmitted that are invalid and with error.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_IF_ERRORS_LO,
		IPN3KE_10G_TX_STATS_IF_ERRORS_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of frames
	 *received that are invalid and with error.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_IF_ERRORS_LO,
		IPN3KE_10G_RX_STATS_IF_ERRORS_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *good unicast frames transmitted,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_UNICAST_FRAME_OK_LO,
		IPN3KE_10G_TX_STATS_UNICAST_FRAME_OK_HI,
		hw, port_id);
	hw_stats->eth.tx_unicast = statistics;

	/*36-bit statistics counter that collects the number of
	 *good unicast frames received,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_UNICAST_FRAME_OK_LO,
		IPN3KE_10G_RX_STATS_UNICAST_FRAME_OK_HI,
		hw, port_id);
	hw_stats->eth.rx_unicast = statistics;

	/*36-bit statistics counter that collects the number of
	 *unicast frames transmitted with error,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_UNICAST_FRAME_ERR_LO,
		IPN3KE_10G_TX_STATS_UNICAST_FRAME_ERR_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *unicast frames received with error,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_UNICAST_FRAME_ERR_LO,
		IPN3KE_10G_RX_STATS_UNICAST_FRAME_ERR_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *good multicast frames transmitted,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_MULTICAST_FRAME_OK_LO,
		IPN3KE_10G_TX_STATS_MULTICAST_FRAME_OK_HI,
		hw, port_id);
	hw_stats->eth.tx_multicast = statistics;

	/*36-bit statistics counter that collects the number of
	 *good multicast frames received,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_MULTICAST_FRAME_OK_LO,
		IPN3KE_10G_RX_STATS_MULTICAST_FRAME_OK_HI,
		hw, port_id);
	hw_stats->eth.rx_multicast = statistics;

	/*36-bit statistics counter that collects the number of
	 *multicast frames transmitted with error,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_MULTICAST_FRAME_ERR_LO,
		IPN3KE_10G_TX_STATS_MULTICAST_FRAME_ERR_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number
	 *of multicast frames received with error,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_MULTICAST_FRAME_ERR_LO,
		IPN3KE_10G_RX_STATS_MULTICAST_FRAME_ERR_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *good broadcast frames transmitted,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_BROADCAST_FRAME_OK_LO,
		IPN3KE_10G_TX_STATS_BROADCAST_FRAME_OK_HI,
		hw, port_id);
	hw_stats->eth.tx_broadcast = statistics;

	/*36-bit statistics counter that collects the number of
	 *good broadcast frames received,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_BROADCAST_FRAME_OK_LO,
		IPN3KE_10G_RX_STATS_BROADCAST_FRAME_OK_HI,
		hw, port_id);
	hw_stats->eth.rx_broadcast = statistics;

	/*36-bit statistics counter that collects the number
	 *of broadcast frames transmitted with error,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_BROADCAST_FRAME_ERR_LO,
		IPN3KE_10G_TX_STATS_BROADCAST_FRAME_ERR_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *broadcast frames received with error,
	 *excluding control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_BROADCAST_FRAME_ERR_LO,
		IPN3KE_10G_RX_STATS_BROADCAST_FRAME_ERR_HI,
		hw, port_id);

	/*64-bit statistics counter that collects the total number of
	 *octets transmitted.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_OCTETS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_OCTETS_HI,
		hw, port_id);

	/*64-bit statistics counter that collects the total number of
	 *octets received.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_64bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_OCTETS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_OCTETS_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the total number of
	 *good, errored, and invalid frames transmitted.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the total number of
	 *good, errored, and invalid frames received.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *undersized TX frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_UNDER_SIZE_PKTS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_UNDER_SIZE_PKTS_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *undersized RX frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_UNDER_SIZE_PKTS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_UNDER_SIZE_PKTS_HI,
		hw, port_id);
	hw_stats->rx_undersize = statistics;

	/*36-bit statistics counter that collects the number of
	 *TX frames whose length exceeds the maximum frame length
	 *specified.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_OVER_SIZE_PKTS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_OVER_SIZE_PKTS_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *RX frames whose length exceeds the maximum frame length
	 *specified.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_OVER_SIZE_PKTS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_OVER_SIZE_PKTS_HI,
		hw, port_id);
	hw_stats->rx_oversize = statistics;

	/*36-bit statistics counter that collects the number of
	 *64-byte TX frames,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_64_OCTETS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_64_OCTETS_HI,
		hw, port_id);
	hw_stats->tx_size_64 = statistics;

	/*36-bit statistics counter that collects the number of
	 *64-byte RX frames,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_64_OCTETS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_64_OCTETS_HI,
		hw, port_id);
	hw_stats->rx_size_64 = statistics;

	/*36-bit statistics counter that collects the number of
	 *TX frames between the length of 65 and 127 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_65_127_OCTETS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_65_127_OCTETS_HI,
		hw, port_id);
	hw_stats->tx_size_65_127 = statistics;

	/*36-bit statistics counter that collects the number of
	 *RX frames between the length of 65 and 127 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_65_127_OCTETS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_65_127_OCTETS_HI,
		hw, port_id);
	hw_stats->rx_size_65_127 = statistics;

	/*36-bit statistics counter that collects the number of
	 *TX frames between the length of 128 and 255 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_128_255_OCTETS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_128_255_OCTETS_HI,
		hw, port_id);
	hw_stats->tx_size_128_255 = statistics;

	/*36-bit statistics counter that collects the number of
	 *RX frames between the length of 128 and 255 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_128_255_OCTETS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_128_255_OCTETS_HI,
		hw, port_id);
	hw_stats->rx_size_128_255 = statistics;

	/*36-bit statistics counter that collects the number of
	 *TX frames between the length of 256 and 511 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_256_511_OCTETS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_256_511_OCTETS_HI,
		hw, port_id);
	hw_stats->tx_size_256_511 = statistics;

	/*36-bit statistics counter that collects the number of
	 *RX frames between the length of 256 and 511 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_256_511_OCTETS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_256_511_OCTETS_HI,
		hw, port_id);
	hw_stats->rx_size_256_511 = statistics;

	/*36-bit statistics counter that collects the number of
	 *TX frames between the length of 512 and 1023 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_512_1023_OCTETS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_512_1023_OCTETS_HI,
		hw, port_id);
	hw_stats->tx_size_512_1023 = statistics;

	/*36-bit statistics counter that collects the number of
	 *RX frames between the length of 512 and 1023 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_512_1023_OCTETS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_512_1023_OCTETS_HI,
		hw, port_id);
	hw_stats->rx_size_512_1023 = statistics;

	/*36-bit statistics counter that collects the number of
	 *TX frames between the length of 1024 and 1518 bytes,
	 *including the CRC field but
	 *excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_1024_1518_OCTETS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_1024_1518_OCTETS_HI,
		hw, port_id);
	hw_stats->tx_size_1024_1518 = statistics;

	/*36-bit statistics counter that collects the number of
	 *RX frames between the length of 1024 and 1518 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_1024_1518_OCTETS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_1024_1518_OCTETS_HI,
		hw, port_id);
	hw_stats->rx_size_1024_1518 = statistics;

	/*36-bit statistics counter that collects the number of
	 *TX frames equal or more than the length of 1,519 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good, errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_1519_X_OCTETS_LO,
		IPN3KE_10G_TX_STATS_ETHER_STATS_PKTS_1519_X_OCTETS_HI,
		hw, port_id);
	hw_stats->tx_size_1519_to_max = statistics;

	/*36-bit statistics counter that collects the number of
	 *RX frames equal or more than the length of 1,519 bytes,
	 *including the CRC field
	 *but excluding the preamble and SFD bytes.
	 *This count includes good,
	 *errored, and invalid frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_1519_X_OCTETS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_PKTS_1519_X_OCTETS_HI,
		hw, port_id);
	hw_stats->rx_size_big = statistics;

	/*36-bit statistics counter that collects the total number of
	 *RX frames with length less than 64 bytes and CRC error.
	 *The MAC does not drop these frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_FRAGMENTS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_FRAGMENTS_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *oversized RX frames with CRC error.
	 *The MAC does not drop these frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_JABBERS_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_JABBERS_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *RX frames with CRC error,
	 *whose length is between 64 and the maximum frame length
	 *specified in the register.
	 *The MAC does not drop these frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_ETHER_STATS_CRC_ERR_LO,
		IPN3KE_10G_RX_STATS_ETHER_STATS_CRC_ERR_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *valid TX unicast control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_UNICAST_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_TX_STATS_UNICAST_MAC_CTRL_FRAMES_HI,
		hw, port_id);
	hw_stats->eth.tx_unicast += statistics;

	/*36-bit statistics counter that collects the number of
	 *valid RX unicast control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_UNICAST_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_RX_STATS_UNICAST_MAC_CTRL_FRAMES_HI,
		hw, port_id);
	hw_stats->eth.rx_unicast += statistics;

	/*36-bit statistics counter that collects the number of
	 *valid TX multicast control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_MULTICAST_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_TX_STATS_MULTICAST_MAC_CTRL_FRAMES_HI,
		hw, port_id);
	hw_stats->eth.tx_multicast += statistics;

	/*36-bit statistics counter that collects the number of
	 *valid RX multicast control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_MULTICAST_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_RX_STATS_MULTICAST_MAC_CTRL_FRAMES_HI,
		hw, port_id);
	hw_stats->eth.rx_multicast += statistics;

	/*36-bit statistics counter that collects the number of
	 *valid TX broadcast control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_BROADCAST_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_TX_STATS_BROADCAST_MAC_CTRL_FRAMES_HI,
		hw, port_id);
	hw_stats->eth.tx_broadcast += statistics;

	/*36-bit statistics counter that collects the number of
	 *valid RX broadcast control frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_BROADCAST_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_RX_STATS_BROADCAST_MAC_CTRL_FRAMES_HI,
		hw, port_id);
	hw_stats->eth.rx_broadcast += statistics;

	/*36-bit statistics counter that collects the number of
	 *valid TX PFC frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_TX_STATS_PFC_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_TX_STATS_PFC_MAC_CTRL_FRAMES_HI,
		hw, port_id);

	/*36-bit statistics counter that collects the number of
	 *valid RX PFC frames.
	 */
	statistics = ipn3ke_rpst_read_36bits_statistics_register(
		IPN3KE_10G_RX_STATS_PFC_MAC_CTRL_FRAMES_LO,
		IPN3KE_10G_RX_STATS_PFC_MAC_CTRL_FRAMES_HI,
		hw, port_id);

	return 0;
}

static void
ipn3ke_rpst_10g_lineside_tx_stats_reset(struct ipn3ke_hw *hw,
uint16_t port_id)
{
	uint32_t tmp;

	/*Bit [0]: Set this register to 1 to clear all TX statistics
	 *counters.
	 *The IP core clears this bit when all counters are cleared.
	 *Bits [31:1]: Reserved.
	 */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
		&tmp,
		IPN3KE_10G_TX_STATS_CLR,
		port_id,
		0);
	tmp |= 0x00000001;
	(*hw->f_mac_write)(hw,
		tmp,
		IPN3KE_10G_TX_STATS_CLR,
		port_id,
		0);
}

static void
ipn3ke_rpst_10g_lineside_rx_stats_reset(struct ipn3ke_hw *hw,
uint16_t port_id)
{
	uint32_t tmp;

	/*Bit [0]: Set this register to 1 to clear all RX statistics
	 *counters.
	 *The IP core clears this bit when all counters are cleared.
	 *Bits [31:1]: Reserved
	 */
	tmp = 0x00000000;
	(*hw->f_mac_read)(hw,
		&tmp,
		IPN3KE_10G_RX_STATS_CLR,
		port_id,
		0);
	tmp |= 0x00000001;
	(*hw->f_mac_write)(hw,
		tmp,
		IPN3KE_10G_RX_STATS_CLR,
		port_id,
		0);
}

static int
ipn3ke_rpst_stats_reset(struct rte_eth_dev *ethdev)
{
	uint16_t port_id = 0;
	char *ch;
	int cnt = 0;
	struct rte_afu_device *afu_dev = NULL;
	struct ipn3ke_hw *hw = NULL;

	if (!ethdev) {
		IPN3KE_AFU_PMD_ERR("ethernet device to reset is NULL!");
		return -EINVAL;
	}

	afu_dev = RTE_ETH_DEV_TO_AFU(ethdev);
	if (!afu_dev) {
		IPN3KE_AFU_PMD_ERR("afu device to reset is NULL!");
		return -EINVAL;
	}

	if (!afu_dev->shared.data) {
		IPN3KE_AFU_PMD_ERR("hardware data to reset is NULL!");
		return -EINVAL;
	}

	hw = afu_dev->shared.data;

	ch = ethdev->data->name;
	if (!ch) {
		IPN3KE_AFU_PMD_ERR("ethdev name is NULL!");
		return -EINVAL;
	}
	while (ch) {
		if (*ch == '_')
			cnt++;
		ch++;
		if (cnt == 3)
			break;
	}
	if (!ch) {
		IPN3KE_AFU_PMD_ERR("Can not get port_id from ethdev name!");
		return -EINVAL;
	}
	port_id = atoi(ch);

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI) {
		ipn3ke_rpst_25g_nic_side_tx_stats_reset(hw, port_id);
		ipn3ke_rpst_25g_nic_side_rx_stats_reset(hw, port_id);
		ipn3ke_rpst_25g_lineside_tx_stats_reset(hw, port_id);
		ipn3ke_rpst_25g_lineside_rx_stats_reset(hw, port_id);
	} else if (hw->retimer.mac_type ==
			IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		ipn3ke_rpst_10g_nic_side_tx_stats_reset(hw, port_id);
		ipn3ke_rpst_10g_nic_side_rx_stats_reset(hw, port_id);
		ipn3ke_rpst_10g_lineside_tx_stats_reset(hw, port_id);
		ipn3ke_rpst_10g_lineside_rx_stats_reset(hw, port_id);
	}

	return 0;
}

static int
ipn3ke_rpst_stats_get
(struct rte_eth_dev *ethdev, struct rte_eth_stats *stats)
{
	uint16_t port_id = 0;
	char *ch;
	int cnt = 0;
	int i = 0;
	struct rte_afu_device *afu_dev = NULL;
	struct ipn3ke_hw *hw = NULL;
	struct ipn3ke_rpst_hw_port_stats hw_stats;

	if (!ethdev) {
		IPN3KE_AFU_PMD_ERR("ethernet device to get statistics is NULL");
		return -EINVAL;
	}
	if (!stats) {
		IPN3KE_AFU_PMD_ERR("Address to return statistics is NULL!");
		return -EINVAL;
	}

	afu_dev = RTE_ETH_DEV_TO_AFU(ethdev);
	if (!afu_dev) {
		IPN3KE_AFU_PMD_ERR("afu device to get statistics is NULL!");
		return -EINVAL;
	}

	if (!afu_dev->shared.data) {
		IPN3KE_AFU_PMD_ERR("hardware data to get statistics is NULL!");
		return -EINVAL;
	}

	hw = afu_dev->shared.data;

	ch = ethdev->data->name;
	if (!ch) {
		IPN3KE_AFU_PMD_ERR("ethdev name is NULL!");
		return -EINVAL;
	}
	while (ch) {
		if (*ch == '_')
			cnt++;
		ch++;
		if (cnt == 3)
			break;
	}
	if (!ch) {
		IPN3KE_AFU_PMD_ERR("Can not get port_id from ethdev name!");
		return -EINVAL;
	}
	port_id = atoi(ch);

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI) {
		ipn3ke_rpst_read_25g_lineside_stats_registers(hw,
							port_id,
							&hw_stats);

		stats->ipackets  = hw_stats.rx_size_64
					+ hw_stats.rx_size_65_127
					+ hw_stats.rx_size_128_255
					+ hw_stats.rx_size_256_511
					+ hw_stats.rx_size_512_1023
					+ hw_stats.rx_size_1024_1518
					+ hw_stats.rx_size_big
					+ hw_stats.rx_undersize
					+ hw_stats.rx_fragments
					+ hw_stats.rx_oversize
					+ hw_stats.rx_jabber;
		stats->opackets  = hw_stats.tx_size_64
					+ hw_stats.tx_size_65_127
					+ hw_stats.tx_size_128_255
					+ hw_stats.tx_size_256_511
					+ hw_stats.tx_size_512_1023
					+ hw_stats.tx_size_1024_1518
					+ hw_stats.tx_size_1519_to_max;
		stats->ibytes    = hw_stats.eth.rx_bytes;
		stats->obytes    = hw_stats.eth.tx_bytes;
		stats->imissed   = 0;
		stats->ierrors   = hw_stats.eth.rx_discards
					+ hw_stats.eth.rx_unknown_protocol;
		stats->oerrors   = hw_stats.eth.tx_discards
					+ hw_stats.eth.tx_errors;
		stats->rx_nombuf = 0;
		for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
			stats->q_ipackets[i] = 0;
			stats->q_opackets[i] = 0;
			stats->q_ibytes[i] = 0;
			stats->q_obytes[i] = 0;
			stats->q_errors[i] = 0;
		}
	} else {
		ipn3ke_rpst_read_10g_lineside_stats_registers(hw,
							port_id,
							&hw_stats,
							stats);
	}

	return 0;
}

static int
ipn3ke_rpst_xstats_get
(struct rte_eth_dev *ethdev, struct rte_eth_xstat *xstats, unsigned int n)
{
	uint16_t port_id = 0;
	char *ch = NULL;
	int cnt = 0;
	unsigned int i, count, prio;
	struct rte_afu_device *afu_dev = NULL;
	struct ipn3ke_hw *hw = NULL;
	struct ipn3ke_rpst_hw_port_stats hw_stats;
	struct rte_eth_stats stats;

	if (!ethdev) {
		IPN3KE_AFU_PMD_ERR("ethernet device to get statistics is NULL");
		return -EINVAL;
	}

	afu_dev = RTE_ETH_DEV_TO_AFU(ethdev);
	if (!afu_dev) {
		IPN3KE_AFU_PMD_ERR("afu device to get statistics is NULL!");
		return -EINVAL;
	}

	if (!afu_dev->shared.data) {
		IPN3KE_AFU_PMD_ERR("hardware data to get statistics is NULL!");
		return -EINVAL;
	}

	hw = afu_dev->shared.data;

	ch = ethdev->data->name;
	if (!ch) {
		IPN3KE_AFU_PMD_ERR("ethdev name is NULL!");
		return -EINVAL;
	}
	while (ch) {
		if (*ch == '_')
			cnt++;
		ch++;
		if (cnt == 3)
			break;
	}
	if (!ch) {
		IPN3KE_AFU_PMD_ERR("Can not get port_id from ethdev name!");
		return -EINVAL;
	}
	port_id = atoi(ch);

	count = ipn3ke_rpst_xstats_calc_num();
	if (n < count)
		return count;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI) {
		ipn3ke_rpst_read_25g_lineside_stats_registers(hw,
							port_id,
							&hw_stats);
	} else {
		ipn3ke_rpst_read_10g_lineside_stats_registers(hw,
							port_id,
							&hw_stats,
							&stats);
	}

	count = 0;

	/* Get stats from ipn3ke_rpst_stats */
	for (i = 0; i < IPN3KE_RPST_ETH_XSTATS_CNT; i++) {
		xstats[count].value = *(uint64_t *)(((char *)&hw_stats.eth)
			+ ipn3ke_rpst_stats_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	/* Get individual stats from ipn3ke_rpst_hw_port */
	for (i = 0; i < IPN3KE_RPST_HW_PORT_XSTATS_CNT; i++) {
		xstats[count].value = *(uint64_t *)(((char *)(&hw_stats)) +
			ipn3ke_rpst_hw_port_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	/* Get individual stats from ipn3ke_rpst_rxq_pri */
	for (i = 0; i < IPN3KE_RPST_RXQ_PRIO_XSTATS_CNT; i++) {
		for (prio = 0; prio < IPN3KE_RPST_PRIO_XSTATS_CNT; prio++) {
			xstats[count].value =
				*(uint64_t *)(((char *)(&hw_stats)) +
				ipn3ke_rpst_rxq_prio_strings[i].offset +
				(sizeof(uint64_t) * prio));
			xstats[count].id = count;
			count++;
		}
	}

	/* Get individual stats from ipn3ke_rpst_txq_prio */
	for (i = 0; i < IPN3KE_RPST_TXQ_PRIO_XSTATS_CNT; i++) {
		for (prio = 0; prio < IPN3KE_RPST_PRIO_XSTATS_CNT; prio++) {
			xstats[count].value =
				*(uint64_t *)(((char *)(&hw_stats)) +
				ipn3ke_rpst_txq_prio_strings[i].offset +
				(sizeof(uint64_t) * prio));
			xstats[count].id = count;
			count++;
		}
	}

	return count;
}

static int
ipn3ke_rpst_xstats_get_names
(__rte_unused struct rte_eth_dev *dev,
struct rte_eth_xstat_name *xstats_names,
__rte_unused unsigned int limit)
{
	unsigned int count = 0;
	unsigned int i, prio;

	if (!xstats_names)
		return ipn3ke_rpst_xstats_calc_num();

	/* Note: limit checked in rte_eth_xstats_names() */

	/* Get stats from ipn3ke_rpst_stats */
	for (i = 0; i < IPN3KE_RPST_ETH_XSTATS_CNT; i++) {
		snprintf(xstats_names[count].name,
			 sizeof(xstats_names[count].name),
			 "%s",
			 ipn3ke_rpst_stats_strings[i].name);
		count++;
	}

	/* Get individual stats from ipn3ke_rpst_hw_port */
	for (i = 0; i < IPN3KE_RPST_HW_PORT_XSTATS_CNT; i++) {
		snprintf(xstats_names[count].name,
			 sizeof(xstats_names[count].name),
			 "%s",
			 ipn3ke_rpst_hw_port_strings[i].name);
		count++;
	}

	/* Get individual stats from ipn3ke_rpst_rxq_pri */
	for (i = 0; i < IPN3KE_RPST_RXQ_PRIO_XSTATS_CNT; i++) {
		for (prio = 0; prio < 8; prio++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "rx_priority%u_%s",
				 prio,
				 ipn3ke_rpst_rxq_prio_strings[i].name);
			count++;
		}
	}

	/* Get individual stats from ipn3ke_rpst_txq_prio */
	for (i = 0; i < IPN3KE_RPST_TXQ_PRIO_XSTATS_CNT; i++) {
		for (prio = 0; prio < 8; prio++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "tx_priority%u_%s",
				 prio,
				 ipn3ke_rpst_txq_prio_strings[i].name);
			count++;
		}
	}
	return count;
}

static void
ipn3ke_update_link(struct rte_rawdev *rawdev,
	uint16_t port, struct rte_eth_link *link)
{
	uint64_t line_link_bitmap = 0;
	enum ifpga_rawdev_link_speed link_speed;

	rawdev->dev_ops->attr_get(rawdev,
				"LineSideLinkStatus",
				(uint64_t *)&line_link_bitmap);

	/* Parse the link status */
	if ((1 << port) & line_link_bitmap)
		link->link_status = 1;
	else
		link->link_status = 0;

	IPN3KE_AFU_PMD_DEBUG("port is %d\n", port);
	IPN3KE_AFU_PMD_DEBUG("link->link_status is %d\n", link->link_status);

	rawdev->dev_ops->attr_get(rawdev,
				"LineSideLinkSpeed",
				(uint64_t *)&link_speed);
	switch (link_speed) {
	case IFPGA_RAWDEV_LINK_SPEED_10GB:
		link->link_speed = RTE_ETH_SPEED_NUM_10G;
		break;
	case IFPGA_RAWDEV_LINK_SPEED_25GB:
		link->link_speed = RTE_ETH_SPEED_NUM_25G;
		break;
	default:
		IPN3KE_AFU_PMD_ERR("Unknown link speed info %u", link_speed);
		break;
	}
}

/*
 * Set device link up.
 */
int
ipn3ke_rpst_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);
	struct rte_eth_dev *pf;
	int ret = 0;

	if (rpst->i40e_pf_eth) {
		ret = rte_eth_dev_set_link_up(rpst->i40e_pf_eth_port_id);
		pf = rpst->i40e_pf_eth;
		(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
	}

	return ret;
}

/*
 * Set device link down.
 */
int
ipn3ke_rpst_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);
	struct rte_eth_dev *pf;
	int ret = 0;

	if (rpst->i40e_pf_eth) {
		ret = rte_eth_dev_set_link_down(rpst->i40e_pf_eth_port_id);
		pf = rpst->i40e_pf_eth;
		(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
	}

	return ret;
}

int
ipn3ke_rpst_link_update(struct rte_eth_dev *ethdev,
	__rte_unused int wait_to_complete)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	struct rte_rawdev *rawdev;
	struct rte_eth_link link;
	struct rte_eth_dev *pf;

	memset(&link, 0, sizeof(link));

	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = !(ethdev->data->dev_conf.link_speeds &
				RTE_ETH_LINK_SPEED_FIXED);

	rawdev = hw->rawdev;
	ipn3ke_update_link(rawdev, rpst->port_id, &link);

	if (!rpst->ori_linfo.link_status &&
		link.link_status) {
		IPN3KE_AFU_PMD_DEBUG("Update Rpst %d Up\n", rpst->port_id);
		rpst->ori_linfo.link_status = link.link_status;
		rpst->ori_linfo.link_speed = link.link_speed;

		rte_eth_linkstatus_set(ethdev, &link);

		if (rpst->i40e_pf_eth) {
			IPN3KE_AFU_PMD_DEBUG("Update FVL PF %d Up\n",
				rpst->i40e_pf_eth_port_id);
			rte_eth_dev_set_link_up(rpst->i40e_pf_eth_port_id);
			pf = rpst->i40e_pf_eth;
			(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
		}
	} else if (rpst->ori_linfo.link_status &&
				!link.link_status) {
		IPN3KE_AFU_PMD_DEBUG("Update Rpst %d Down\n",
			rpst->port_id);
		rpst->ori_linfo.link_status = link.link_status;
		rpst->ori_linfo.link_speed = link.link_speed;

		rte_eth_linkstatus_set(ethdev, &link);

		if (rpst->i40e_pf_eth) {
			IPN3KE_AFU_PMD_DEBUG("Update FVL PF %d Down\n",
				rpst->i40e_pf_eth_port_id);
			rte_eth_dev_set_link_down(rpst->i40e_pf_eth_port_id);
			pf = rpst->i40e_pf_eth;
			(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
		}
	}

	return 0;
}

static int
ipn3ke_rpst_link_check(struct ipn3ke_rpst *rpst)
{
	struct ipn3ke_hw *hw;
	struct rte_rawdev *rawdev;
	struct rte_eth_link link;
	struct rte_eth_dev *pf;

	if (rpst == NULL)
		return -1;

	hw = rpst->hw;

	memset(&link, 0, sizeof(link));

	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = !(rpst->ethdev->data->dev_conf.link_speeds &
				RTE_ETH_LINK_SPEED_FIXED);

	rawdev = hw->rawdev;
	ipn3ke_update_link(rawdev, rpst->port_id, &link);

	if (!rpst->ori_linfo.link_status &&
				link.link_status) {
		IPN3KE_AFU_PMD_DEBUG("Check Rpst %d Up\n", rpst->port_id);
		rpst->ori_linfo.link_status = link.link_status;
		rpst->ori_linfo.link_speed = link.link_speed;

		rte_eth_linkstatus_set(rpst->ethdev, &link);

		if (rpst->i40e_pf_eth) {
			IPN3KE_AFU_PMD_DEBUG("Check FVL PF %d Up\n",
				rpst->i40e_pf_eth_port_id);
			rte_eth_dev_set_link_up(rpst->i40e_pf_eth_port_id);
			pf = rpst->i40e_pf_eth;
			(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
		}
	} else if (rpst->ori_linfo.link_status &&
		!link.link_status) {
		IPN3KE_AFU_PMD_DEBUG("Check Rpst %d Down\n", rpst->port_id);
		rpst->ori_linfo.link_status = link.link_status;
		rpst->ori_linfo.link_speed = link.link_speed;

		rte_eth_linkstatus_set(rpst->ethdev, &link);

		if (rpst->i40e_pf_eth) {
			IPN3KE_AFU_PMD_DEBUG("Check FVL PF %d Down\n",
				rpst->i40e_pf_eth_port_id);
			rte_eth_dev_set_link_down(rpst->i40e_pf_eth_port_id);
			pf = rpst->i40e_pf_eth;
			(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
		}
	}

	return 0;
}

static uint32_t
ipn3ke_rpst_scan_handle_request(__rte_unused void *param)
{
	struct ipn3ke_rpst *rpst;
	int num = 0;
#define MS 1000
#define SCAN_NUM 32

	for (;;) {
		num = 0;
		TAILQ_FOREACH(rpst, &ipn3ke_rpst_list, next) {
			if (rpst->i40e_pf_eth &&
				rpst->ethdev->data->dev_started &&
				rpst->i40e_pf_eth->data->dev_started)
				ipn3ke_rpst_link_check(rpst);

			if (++num > SCAN_NUM)
				rte_delay_us(1 * MS);
		}
		rte_delay_us(50 * MS);

		if (num == 0 || num == 0xffffff)
			return 0;
	}

	return 0;
}

static int
ipn3ke_rpst_scan_check(void)
{
	int ret;

	if (ipn3ke_rpst_scan_num == 1) {
		ret = rte_thread_create_internal_control(&ipn3ke_rpst_scan_thread,
			"ipn3ke-scn",
			ipn3ke_rpst_scan_handle_request, NULL);
		if (ret) {
			IPN3KE_AFU_PMD_ERR("Fail to create ipn3ke rpst scan thread");
			return -1;
		}
	} else if (ipn3ke_rpst_scan_num == 0) {
		ret = pthread_cancel((pthread_t)ipn3ke_rpst_scan_thread.opaque_id);
		if (ret)
			IPN3KE_AFU_PMD_ERR("Can't cancel the thread");

		ret = rte_thread_join(ipn3ke_rpst_scan_thread, NULL);
		if (ret)
			IPN3KE_AFU_PMD_ERR("Can't join the thread");

		return ret;
	}

	return 0;
}

int
ipn3ke_rpst_promiscuous_enable(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t rddata, val;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Enable all unicast */
		(*hw->f_mac_read)(hw,
				&rddata,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
		val = 1;
		val &= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLUCAST_MASK;
		val |= rddata;
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
	}

	return 0;
}

int
ipn3ke_rpst_promiscuous_disable(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t rddata, val;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable all unicast */
		(*hw->f_mac_read)(hw,
				&rddata,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
		val = 0;
		val &= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLUCAST_MASK;
		val |= rddata;
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
	}

	return 0;
}

int
ipn3ke_rpst_allmulticast_enable(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t rddata, val;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Enable all unicast */
		(*hw->f_mac_read)(hw,
				&rddata,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
		val = 1;
		val <<= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_SHIFT;
		val &= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_MASK;
		val |= rddata;
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
	}

	return 0;
}

int
ipn3ke_rpst_allmulticast_disable(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t rddata, val;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable all unicast */
		(*hw->f_mac_read)(hw,
				&rddata,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
		val = 0;
		val <<= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_SHIFT;
		val &= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_MASK;
		val |= rddata;
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
	}

	return 0;
}

int
ipn3ke_rpst_mac_addr_set(struct rte_eth_dev *ethdev,
				struct rte_ether_addr *mac_addr)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t val;

	if (!rte_is_valid_assigned_ether_addr(mac_addr)) {
		IPN3KE_AFU_PMD_ERR("Tried to set invalid MAC address.");
		return -EINVAL;
	}

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		rte_ether_addr_copy(&mac_addr[0], &rpst->mac_addr);

		/* Set mac address */
		rte_memcpy(((char *)(&val)), &mac_addr[0], sizeof(uint32_t));
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_PRIMARY_MAC_ADDR0,
				rpst->port_id,
				0);
		rte_memcpy(((char *)(&val)), &mac_addr[4], sizeof(uint16_t));
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_PRIMARY_MAC_ADDR0,
				rpst->port_id,
				0);
	}

	return 0;
}

int
ipn3ke_rpst_mtu_set(struct rte_eth_dev *ethdev, uint16_t mtu)
{
	int ret = 0;
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	struct rte_eth_dev_data *dev_data = ethdev->data;

	/* mtu setting is forbidden if port is start */
	/* make sure NIC port is stopped */
	if (rpst->i40e_pf_eth && rpst->i40e_pf_eth->data->dev_started) {
		IPN3KE_AFU_PMD_ERR("NIC port %d must "
			"be stopped before configuration",
			rpst->i40e_pf_eth->data->port_id);
		return -EBUSY;
	}
	/* mtu setting is forbidden if port is start */
	if (dev_data->dev_started) {
		IPN3KE_AFU_PMD_ERR("FPGA port %d must "
			"be stopped before configuration",
			dev_data->port_id);
		return -EBUSY;
	}

	if (rpst->i40e_pf_eth) {
		ret = rpst->i40e_pf_eth->dev_ops->mtu_set(rpst->i40e_pf_eth,
							mtu);
		if (!ret)
			rpst->i40e_pf_eth->data->mtu = mtu;
	}

	return ret;
}

static int
ipn3ke_afu_flow_ops_get(struct rte_eth_dev *ethdev,
			const struct rte_flow_ops **ops)
{
	struct ipn3ke_hw *hw;
	struct ipn3ke_rpst *rpst;

	if (ethdev == NULL)
		return -EINVAL;

	hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);

	if (hw->acc_flow)
		*ops = &ipn3ke_flow_ops;
	else if (rpst->i40e_pf_eth)
		(*rpst->i40e_pf_eth->dev_ops->flow_ops_get)(ethdev, ops);
	else
		return -EINVAL;

	return 0;
}

static const struct eth_dev_ops ipn3ke_rpst_dev_ops = {
	.dev_infos_get        = ipn3ke_rpst_dev_infos_get,

	.dev_configure        = ipn3ke_rpst_dev_configure,
	.dev_start            = ipn3ke_rpst_dev_start,
	.dev_stop             = ipn3ke_rpst_dev_stop,
	.dev_close            = ipn3ke_rpst_dev_close,
	.dev_reset            = ipn3ke_rpst_dev_reset,

	.stats_get            = ipn3ke_rpst_stats_get,
	.xstats_get           = ipn3ke_rpst_xstats_get,
	.xstats_get_names     = ipn3ke_rpst_xstats_get_names,
	.stats_reset          = ipn3ke_rpst_stats_reset,
	.xstats_reset         = ipn3ke_rpst_stats_reset,

	.flow_ops_get         = ipn3ke_afu_flow_ops_get,

	.rx_queue_start       = ipn3ke_rpst_rx_queue_start,
	.rx_queue_stop        = ipn3ke_rpst_rx_queue_stop,
	.tx_queue_start       = ipn3ke_rpst_tx_queue_start,
	.tx_queue_stop        = ipn3ke_rpst_tx_queue_stop,
	.rx_queue_setup       = ipn3ke_rpst_rx_queue_setup,
	.tx_queue_setup       = ipn3ke_rpst_tx_queue_setup,

	.dev_set_link_up      = ipn3ke_rpst_dev_set_link_up,
	.dev_set_link_down    = ipn3ke_rpst_dev_set_link_down,
	.link_update          = ipn3ke_rpst_link_update,

	.promiscuous_enable   = ipn3ke_rpst_promiscuous_enable,
	.promiscuous_disable  = ipn3ke_rpst_promiscuous_disable,
	.allmulticast_enable  = ipn3ke_rpst_allmulticast_enable,
	.allmulticast_disable = ipn3ke_rpst_allmulticast_disable,
	.mac_addr_set         = ipn3ke_rpst_mac_addr_set,
	.mtu_set              = ipn3ke_rpst_mtu_set,

	.tm_ops_get           = ipn3ke_tm_ops_get,
};

static uint16_t ipn3ke_rpst_recv_pkts(__rte_unused void *rx_q,
	__rte_unused struct rte_mbuf **rx_pkts, __rte_unused uint16_t nb_pkts)
{
	return 0;
}

static uint16_t
ipn3ke_rpst_xmit_pkts(__rte_unused void *tx_queue,
	__rte_unused struct rte_mbuf **tx_pkts, __rte_unused uint16_t nb_pkts)
{
	return 0;
}

int
ipn3ke_rpst_init(struct rte_eth_dev *ethdev, void *init_params)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	struct ipn3ke_rpst *representor_param =
			(struct ipn3ke_rpst *)init_params;

	if (representor_param->port_id >= representor_param->hw->port_num)
		return -ENODEV;

	if (ipn3ke_bridge_func.set_i40e_sw_dev == NULL)
		return -ENOMEM;

	rpst->ethdev = ethdev;
	rpst->switch_domain_id = representor_param->switch_domain_id;
	rpst->port_id = representor_param->port_id;
	rpst->hw = representor_param->hw;
	rpst->i40e_pf_eth = representor_param->i40e_pf_eth;
	rpst->i40e_pf_eth_port_id = representor_param->i40e_pf_eth_port_id;
	if (rpst->i40e_pf_eth)
		ipn3ke_bridge_func.set_i40e_sw_dev(rpst->i40e_pf_eth_port_id,
					    rpst->ethdev);

	ethdev->data->mac_addrs = rte_zmalloc("ipn3ke", RTE_ETHER_ADDR_LEN, 0);
	if (!ethdev->data->mac_addrs) {
		IPN3KE_AFU_PMD_ERR("Failed to "
			"allocated memory for storing mac address");
		return -ENODEV;
	}

	if (rpst->hw->tm_hw_enable)
		ipn3ke_tm_init(rpst);

	/* Set representor device ops */
	ethdev->dev_ops = &ipn3ke_rpst_dev_ops;

	/* No data-path, but need stub Rx/Tx functions to avoid crash
	 * when testing with the likes of testpmd.
	 */
	ethdev->rx_pkt_burst = ipn3ke_rpst_recv_pkts;
	ethdev->tx_pkt_burst = ipn3ke_rpst_xmit_pkts;

	ethdev->data->nb_rx_queues = 1;
	ethdev->data->nb_tx_queues = 1;

	ethdev->data->mac_addrs = rte_zmalloc("ipn3ke_afu_representor",
						RTE_ETHER_ADDR_LEN,
						0);
	if (!ethdev->data->mac_addrs) {
		IPN3KE_AFU_PMD_ERR("Failed to "
			"allocated memory for storing mac address");
		return -ENODEV;
	}

	ethdev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR |
					RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	rte_spinlock_lock(&ipn3ke_link_notify_list_lk);
	TAILQ_INSERT_TAIL(&ipn3ke_rpst_list, rpst, next);
	ipn3ke_rpst_scan_num++;
	ipn3ke_rpst_scan_check();
	rte_spinlock_unlock(&ipn3ke_link_notify_list_lk);

	return 0;
}

int
ipn3ke_rpst_uninit(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);

	rte_spinlock_lock(&ipn3ke_link_notify_list_lk);
	TAILQ_REMOVE(&ipn3ke_rpst_list, rpst, next);
	ipn3ke_rpst_scan_num--;
	ipn3ke_rpst_scan_check();
	rte_spinlock_unlock(&ipn3ke_link_notify_list_lk);

	return 0;
}
