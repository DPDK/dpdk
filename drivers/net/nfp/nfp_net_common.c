/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2018 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

#include "nfp_net_common.h"

#include <rte_alarm.h>

#include "flower/nfp_flower_representor.h"
#include "nfd3/nfp_nfd3.h"
#include "nfdk/nfp_nfdk.h"
#include "nfpcore/nfp_mip.h"
#include "nfpcore/nfp_nsp.h"
#include "nfp_logs.h"

#define NFP_TX_MAX_SEG       UINT8_MAX
#define NFP_TX_MAX_MTU_SEG   8

#define NFP_NET_LINK_DOWN_CHECK_TIMEOUT 4000 /* ms */
#define NFP_NET_LINK_UP_CHECK_TIMEOUT   1000 /* ms */

#define DEFAULT_FLBUF_SIZE        9216
#define NFP_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + RTE_VLAN_HLEN * 2)

enum nfp_xstat_group {
	NFP_XSTAT_GROUP_NET,
	NFP_XSTAT_GROUP_MAC
};

struct nfp_xstat {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	int offset;
	enum nfp_xstat_group group;
};

#define NFP_XSTAT_NET(_name, _offset) {                 \
	.name = _name,                                  \
	.offset = NFP_NET_CFG_STATS_##_offset,          \
	.group = NFP_XSTAT_GROUP_NET,                   \
}

#define NFP_XSTAT_MAC(_name, _offset) {                 \
	.name = _name,                                  \
	.offset = NFP_MAC_STATS_##_offset,              \
	.group = NFP_XSTAT_GROUP_MAC,                   \
}

static const struct nfp_xstat nfp_net_xstats[] = {
	/*
	 * Basic xstats available on both VF and PF.
	 * Note that in case new statistics of group NFP_XSTAT_GROUP_NET
	 * are added to this array, they must appear before any statistics
	 * of group NFP_XSTAT_GROUP_MAC.
	 */
	NFP_XSTAT_NET("rx_good_packets_mc", RX_MC_FRAMES),
	NFP_XSTAT_NET("tx_good_packets_mc", TX_MC_FRAMES),
	NFP_XSTAT_NET("rx_good_packets_bc", RX_BC_FRAMES),
	NFP_XSTAT_NET("tx_good_packets_bc", TX_BC_FRAMES),
	NFP_XSTAT_NET("rx_good_bytes_uc", RX_UC_OCTETS),
	NFP_XSTAT_NET("tx_good_bytes_uc", TX_UC_OCTETS),
	NFP_XSTAT_NET("rx_good_bytes_mc", RX_MC_OCTETS),
	NFP_XSTAT_NET("tx_good_bytes_mc", TX_MC_OCTETS),
	NFP_XSTAT_NET("rx_good_bytes_bc", RX_BC_OCTETS),
	NFP_XSTAT_NET("tx_good_bytes_bc", TX_BC_OCTETS),
	NFP_XSTAT_NET("tx_missed_erros", TX_DISCARDS),
	NFP_XSTAT_NET("bpf_pass_pkts", APP0_FRAMES),
	NFP_XSTAT_NET("bpf_pass_bytes", APP0_BYTES),
	NFP_XSTAT_NET("bpf_app1_pkts", APP1_FRAMES),
	NFP_XSTAT_NET("bpf_app1_bytes", APP1_BYTES),
	NFP_XSTAT_NET("bpf_app2_pkts", APP2_FRAMES),
	NFP_XSTAT_NET("bpf_app2_bytes", APP2_BYTES),
	NFP_XSTAT_NET("bpf_app3_pkts", APP3_FRAMES),
	NFP_XSTAT_NET("bpf_app3_bytes", APP3_BYTES),
	/*
	 * MAC xstats available only on PF. These statistics are not available for VFs as the
	 * PF is not initialized when the VF is initialized as it is still bound to the kernel
	 * driver. As such, the PMD cannot obtain a CPP handle and access the rtsym_table in order
	 * to get the pointer to the start of the MAC statistics counters.
	 */
	NFP_XSTAT_MAC("mac.rx_octets", RX_IN_OCTS),
	NFP_XSTAT_MAC("mac.rx_frame_too_long_errors", RX_FRAME_TOO_LONG_ERRORS),
	NFP_XSTAT_MAC("mac.rx_range_length_errors", RX_RANGE_LENGTH_ERRORS),
	NFP_XSTAT_MAC("mac.rx_vlan_received_ok", RX_VLAN_RECEIVED_OK),
	NFP_XSTAT_MAC("mac.rx_errors", RX_IN_ERRORS),
	NFP_XSTAT_MAC("mac.rx_broadcast_pkts", RX_IN_BROADCAST_PKTS),
	NFP_XSTAT_MAC("mac.rx_drop_events", RX_DROP_EVENTS),
	NFP_XSTAT_MAC("mac.rx_alignment_errors", RX_ALIGNMENT_ERRORS),
	NFP_XSTAT_MAC("mac.rx_pause_mac_ctrl_frames", RX_PAUSE_MAC_CTRL_FRAMES),
	NFP_XSTAT_MAC("mac.rx_frames_received_ok", RX_FRAMES_RECEIVED_OK),
	NFP_XSTAT_MAC("mac.rx_frame_check_sequence_errors", RX_FRAME_CHECK_SEQ_ERRORS),
	NFP_XSTAT_MAC("mac.rx_unicast_pkts", RX_UNICAST_PKTS),
	NFP_XSTAT_MAC("mac.rx_multicast_pkts", RX_MULTICAST_PKTS),
	NFP_XSTAT_MAC("mac.rx_pkts", RX_PKTS),
	NFP_XSTAT_MAC("mac.rx_undersize_pkts", RX_UNDERSIZE_PKTS),
	NFP_XSTAT_MAC("mac.rx_pkts_64_octets", RX_PKTS_64_OCTS),
	NFP_XSTAT_MAC("mac.rx_pkts_65_to_127_octets", RX_PKTS_65_TO_127_OCTS),
	NFP_XSTAT_MAC("mac.rx_pkts_128_to_255_octets", RX_PKTS_128_TO_255_OCTS),
	NFP_XSTAT_MAC("mac.rx_pkts_256_to_511_octets", RX_PKTS_256_TO_511_OCTS),
	NFP_XSTAT_MAC("mac.rx_pkts_512_to_1023_octets", RX_PKTS_512_TO_1023_OCTS),
	NFP_XSTAT_MAC("mac.rx_pkts_1024_to_1518_octets", RX_PKTS_1024_TO_1518_OCTS),
	NFP_XSTAT_MAC("mac.rx_pkts_1519_to_max_octets", RX_PKTS_1519_TO_MAX_OCTS),
	NFP_XSTAT_MAC("mac.rx_jabbers", RX_JABBERS),
	NFP_XSTAT_MAC("mac.rx_fragments", RX_FRAGMENTS),
	NFP_XSTAT_MAC("mac.rx_oversize_pkts", RX_OVERSIZE_PKTS),
	NFP_XSTAT_MAC("mac.rx_pause_frames_class0", RX_PAUSE_FRAMES_CLASS0),
	NFP_XSTAT_MAC("mac.rx_pause_frames_class1", RX_PAUSE_FRAMES_CLASS1),
	NFP_XSTAT_MAC("mac.rx_pause_frames_class2", RX_PAUSE_FRAMES_CLASS2),
	NFP_XSTAT_MAC("mac.rx_pause_frames_class3", RX_PAUSE_FRAMES_CLASS3),
	NFP_XSTAT_MAC("mac.rx_pause_frames_class4", RX_PAUSE_FRAMES_CLASS4),
	NFP_XSTAT_MAC("mac.rx_pause_frames_class5", RX_PAUSE_FRAMES_CLASS5),
	NFP_XSTAT_MAC("mac.rx_pause_frames_class6", RX_PAUSE_FRAMES_CLASS6),
	NFP_XSTAT_MAC("mac.rx_pause_frames_class7", RX_PAUSE_FRAMES_CLASS7),
	NFP_XSTAT_MAC("mac.rx_mac_ctrl_frames_received", RX_MAC_CTRL_FRAMES_REC),
	NFP_XSTAT_MAC("mac.rx_mac_head_drop", RX_MAC_HEAD_DROP),
	NFP_XSTAT_MAC("mac.tx_queue_drop", TX_QUEUE_DROP),
	NFP_XSTAT_MAC("mac.tx_octets", TX_OUT_OCTS),
	NFP_XSTAT_MAC("mac.tx_vlan_transmitted_ok", TX_VLAN_TRANSMITTED_OK),
	NFP_XSTAT_MAC("mac.tx_errors", TX_OUT_ERRORS),
	NFP_XSTAT_MAC("mac.tx_broadcast_pkts", TX_BROADCAST_PKTS),
	NFP_XSTAT_MAC("mac.tx_pause_mac_ctrl_frames", TX_PAUSE_MAC_CTRL_FRAMES),
	NFP_XSTAT_MAC("mac.tx_frames_transmitted_ok", TX_FRAMES_TRANSMITTED_OK),
	NFP_XSTAT_MAC("mac.tx_unicast_pkts", TX_UNICAST_PKTS),
	NFP_XSTAT_MAC("mac.tx_multicast_pkts", TX_MULTICAST_PKTS),
	NFP_XSTAT_MAC("mac.tx_pkts_64_octets", TX_PKTS_64_OCTS),
	NFP_XSTAT_MAC("mac.tx_pkts_65_to_127_octets", TX_PKTS_65_TO_127_OCTS),
	NFP_XSTAT_MAC("mac.tx_pkts_128_to_255_octets", TX_PKTS_128_TO_255_OCTS),
	NFP_XSTAT_MAC("mac.tx_pkts_256_to_511_octets", TX_PKTS_256_TO_511_OCTS),
	NFP_XSTAT_MAC("mac.tx_pkts_512_to_1023_octets", TX_PKTS_512_TO_1023_OCTS),
	NFP_XSTAT_MAC("mac.tx_pkts_1024_to_1518_octets", TX_PKTS_1024_TO_1518_OCTS),
	NFP_XSTAT_MAC("mac.tx_pkts_1519_to_max_octets", TX_PKTS_1519_TO_MAX_OCTS),
	NFP_XSTAT_MAC("mac.tx_pause_frames_class0", TX_PAUSE_FRAMES_CLASS0),
	NFP_XSTAT_MAC("mac.tx_pause_frames_class1", TX_PAUSE_FRAMES_CLASS1),
	NFP_XSTAT_MAC("mac.tx_pause_frames_class2", TX_PAUSE_FRAMES_CLASS2),
	NFP_XSTAT_MAC("mac.tx_pause_frames_class3", TX_PAUSE_FRAMES_CLASS3),
	NFP_XSTAT_MAC("mac.tx_pause_frames_class4", TX_PAUSE_FRAMES_CLASS4),
	NFP_XSTAT_MAC("mac.tx_pause_frames_class5", TX_PAUSE_FRAMES_CLASS5),
	NFP_XSTAT_MAC("mac.tx_pause_frames_class6", TX_PAUSE_FRAMES_CLASS6),
	NFP_XSTAT_MAC("mac.tx_pause_frames_class7", TX_PAUSE_FRAMES_CLASS7),
};

static const uint32_t nfp_net_link_speed_nfp2rte[] = {
	[NFP_NET_CFG_STS_LINK_RATE_UNSUPPORTED] = RTE_ETH_SPEED_NUM_NONE,
	[NFP_NET_CFG_STS_LINK_RATE_UNKNOWN]     = RTE_ETH_SPEED_NUM_NONE,
	[NFP_NET_CFG_STS_LINK_RATE_1G]          = RTE_ETH_SPEED_NUM_1G,
	[NFP_NET_CFG_STS_LINK_RATE_10G]         = RTE_ETH_SPEED_NUM_10G,
	[NFP_NET_CFG_STS_LINK_RATE_25G]         = RTE_ETH_SPEED_NUM_25G,
	[NFP_NET_CFG_STS_LINK_RATE_40G]         = RTE_ETH_SPEED_NUM_40G,
	[NFP_NET_CFG_STS_LINK_RATE_50G]         = RTE_ETH_SPEED_NUM_50G,
	[NFP_NET_CFG_STS_LINK_RATE_100G]        = RTE_ETH_SPEED_NUM_100G,
};

static uint16_t
nfp_net_link_speed_rte2nfp(uint16_t speed)
{
	uint16_t i;

	for (i = 0; i < RTE_DIM(nfp_net_link_speed_nfp2rte); i++) {
		if (speed == nfp_net_link_speed_nfp2rte[i])
			return i;
	}

	return NFP_NET_CFG_STS_LINK_RATE_UNKNOWN;
}

static void
nfp_net_notify_port_speed(struct nfp_net_hw *hw,
		struct rte_eth_link *link)
{
	/*
	 * Read the link status from NFP_NET_CFG_STS. If the link is down
	 * then write the link speed NFP_NET_CFG_STS_LINK_RATE_UNKNOWN to
	 * NFP_NET_CFG_STS_NSP_LINK_RATE.
	 */
	if (link->link_status == RTE_ETH_LINK_DOWN) {
		nn_cfg_writew(&hw->super, NFP_NET_CFG_STS_NSP_LINK_RATE,
				NFP_NET_CFG_STS_LINK_RATE_UNKNOWN);
		return;
	}

	/*
	 * Link is up so write the link speed from the eth_table to
	 * NFP_NET_CFG_STS_NSP_LINK_RATE.
	 */
	nn_cfg_writew(&hw->super, NFP_NET_CFG_STS_NSP_LINK_RATE,
			nfp_net_link_speed_rte2nfp(link->link_speed));
}

/* The length of firmware version string */
#define FW_VER_LEN        32

/**
 * Reconfigure the firmware via the mailbox
 *
 * @param net_hw
 *   Device to reconfigure
 * @param mbox_cmd
 *   The value for the mailbox command
 *
 * @return
 *   - (0) if OK to reconfigure by the mailbox.
 *   - (-EIO) if I/O err and fail to reconfigure by the mailbox
 */
int
nfp_net_mbox_reconfig(struct nfp_net_hw *net_hw,
		uint32_t mbox_cmd)
{
	int ret;
	uint32_t mbox;

	mbox = net_hw->tlv_caps.mbox_off;

	rte_spinlock_lock(&net_hw->super.reconfig_lock);

	nn_cfg_writeq(&net_hw->super, mbox + NFP_NET_CFG_MBOX_SIMPLE_CMD, mbox_cmd);
	nn_cfg_writel(&net_hw->super, NFP_NET_CFG_UPDATE, NFP_NET_CFG_UPDATE_MBOX);

	rte_wmb();

	ret = nfp_reconfig_real(&net_hw->super, NFP_NET_CFG_UPDATE_MBOX);

	rte_spinlock_unlock(&net_hw->super.reconfig_lock);

	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Error nft net mailbox reconfig: mbox=%#08x update=%#08x",
				mbox_cmd, NFP_NET_CFG_UPDATE_MBOX);
		return -EIO;
	}

	return nn_cfg_readl(&net_hw->super, mbox + NFP_NET_CFG_MBOX_SIMPLE_RET);
}

struct nfp_net_hw *
nfp_net_get_hw(const struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	if ((dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR) != 0) {
		struct nfp_flower_representor *repr;
		repr = dev->data->dev_private;
		hw = repr->app_fw_flower->pf_hw;
	} else {
		hw = dev->data->dev_private;
	}

	return hw;
}

/*
 * Configure an Ethernet device.
 *
 * This function must be invoked first before any other function in the Ethernet API.
 * This function can also be re-invoked when a device is in the stopped state.
 *
 * A DPDK app sends info about how many queues to use and how  those queues
 * need to be configured. This is used by the DPDK core and it makes sure no
 * more queues than those advertised by the driver are requested.
 * This function is called after that internal process.
 */
int
nfp_net_configure(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	struct rte_eth_txmode *txmode;

	hw = nfp_net_get_hw(dev);
	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;
	txmode = &dev_conf->txmode;

	if ((rxmode->mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) != 0)
		rxmode->offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	/* Checking TX mode */
	if (txmode->mq_mode != RTE_ETH_MQ_TX_NONE) {
		PMD_DRV_LOG(ERR, "TX mq_mode DCB and VMDq not supported");
		return -EINVAL;
	}

	/* Checking RX mode */
	if ((rxmode->mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) != 0 &&
			(hw->super.cap & NFP_NET_CFG_CTRL_RSS_ANY) == 0) {
		PMD_DRV_LOG(ERR, "RSS not supported");
		return -EINVAL;
	}

	/* Checking MTU set */
	if (rxmode->mtu > hw->max_mtu + NFP_ETH_OVERHEAD) {
		PMD_DRV_LOG(ERR, "MTU (%u) larger than the maximum possible frame size (%u)",
				rxmode->mtu, hw->max_mtu + NFP_ETH_OVERHEAD);
		return -ERANGE;
	}

	return 0;
}

void
nfp_net_log_device_information(const struct nfp_net_hw *hw)
{
	uint32_t cap = hw->super.cap;

	PMD_INIT_LOG(INFO, "VER: %u.%u, Maximum supported MTU: %d",
			hw->ver.major, hw->ver.minor, hw->max_mtu);

	PMD_INIT_LOG(INFO, "CAP: %#x, %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", cap,
			cap & NFP_NET_CFG_CTRL_PROMISC   ? "PROMISC "   : "",
			cap & NFP_NET_CFG_CTRL_L2BC      ? "L2BCFILT "  : "",
			cap & NFP_NET_CFG_CTRL_L2MC      ? "L2MCFILT "  : "",
			cap & NFP_NET_CFG_CTRL_RXCSUM    ? "RXCSUM "    : "",
			cap & NFP_NET_CFG_CTRL_TXCSUM    ? "TXCSUM "    : "",
			cap & NFP_NET_CFG_CTRL_RXVLAN    ? "RXVLAN "    : "",
			cap & NFP_NET_CFG_CTRL_TXVLAN    ? "TXVLAN "    : "",
			cap & NFP_NET_CFG_CTRL_RXVLAN_V2 ? "RXVLANv2 "  : "",
			cap & NFP_NET_CFG_CTRL_TXVLAN_V2 ? "TXVLANv2 "  : "",
			cap & NFP_NET_CFG_CTRL_RXQINQ    ? "RXQINQ "    : "",
			cap & NFP_NET_CFG_CTRL_SCATTER   ? "SCATTER "   : "",
			cap & NFP_NET_CFG_CTRL_GATHER    ? "GATHER "    : "",
			cap & NFP_NET_CFG_CTRL_LIVE_ADDR ? "LIVE_ADDR " : "",
			cap & NFP_NET_CFG_CTRL_LSO       ? "TSO "       : "",
			cap & NFP_NET_CFG_CTRL_LSO2      ? "TSOv2 "     : "",
			cap & NFP_NET_CFG_CTRL_RSS       ? "RSS "       : "",
			cap & NFP_NET_CFG_CTRL_RSS2      ? "RSSv2 "     : "");

	PMD_INIT_LOG(INFO, "max_rx_queues: %u, max_tx_queues: %u",
			hw->max_rx_queues, hw->max_tx_queues);
}

static inline void
nfp_net_enable_rxvlan_cap(struct nfp_net_hw *hw,
		uint32_t *ctrl)
{
	if ((hw->super.cap & NFP_NET_CFG_CTRL_RXVLAN_V2) != 0)
		*ctrl |= NFP_NET_CFG_CTRL_RXVLAN_V2;
	else if ((hw->super.cap & NFP_NET_CFG_CTRL_RXVLAN) != 0)
		*ctrl |= NFP_NET_CFG_CTRL_RXVLAN;
}

void
nfp_net_enable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	hw = nfp_net_get_hw(dev);

	nfp_enable_queues(&hw->super, dev->data->nb_rx_queues,
			dev->data->nb_tx_queues);
}

void
nfp_net_disable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);

	nfp_disable_queues(&net_hw->super);
}

void
nfp_net_params_setup(struct nfp_net_hw *hw)
{
	nn_cfg_writel(&hw->super, NFP_NET_CFG_MTU, hw->mtu);
	nn_cfg_writel(&hw->super, NFP_NET_CFG_FLBUFSZ, hw->flbufsz);
}

void
nfp_net_cfg_queue_setup(struct nfp_net_hw *hw)
{
	hw->super.qcp_cfg = hw->tx_bar + NFP_QCP_QUEUE_ADDR_SZ;
}

int
nfp_net_set_mac_addr(struct rte_eth_dev *dev,
		struct rte_ether_addr *mac_addr)
{
	uint32_t update;
	uint32_t new_ctrl;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;
	if ((hw->ctrl & NFP_NET_CFG_CTRL_ENABLE) != 0 &&
			(hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR) == 0) {
		PMD_DRV_LOG(ERR, "MAC address unable to change when port enabled");
		return -EBUSY;
	}

	/* Writing new MAC to the specific port BAR address */
	nfp_write_mac(hw, (uint8_t *)mac_addr);

	update = NFP_NET_CFG_UPDATE_MACADDR;
	new_ctrl = hw->ctrl;
	if ((hw->ctrl & NFP_NET_CFG_CTRL_ENABLE) != 0 &&
			(hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR) != 0)
		new_ctrl |= NFP_NET_CFG_CTRL_LIVE_ADDR;

	/* Signal the NIC about the change */
	if (nfp_reconfig(hw, new_ctrl, update) != 0) {
		PMD_DRV_LOG(ERR, "MAC address update failed");
		return -EIO;
	}

	hw->ctrl = new_ctrl;

	return 0;
}

int
nfp_configure_rx_interrupt(struct rte_eth_dev *dev,
		struct rte_intr_handle *intr_handle)
{
	uint16_t i;
	struct nfp_net_hw *hw;

	if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
				dev->data->nb_rx_queues) != 0) {
		PMD_DRV_LOG(ERR, "Failed to allocate %d rx_queues intr_vec",
				dev->data->nb_rx_queues);
		return -ENOMEM;
	}

	hw = nfp_net_get_hw(dev);

	if (rte_intr_type_get(intr_handle) == RTE_INTR_HANDLE_UIO) {
		PMD_DRV_LOG(INFO, "VF: enabling RX interrupt with UIO");
		/* UIO just supports one queue and no LSC */
		nn_cfg_writeb(&hw->super, NFP_NET_CFG_RXR_VEC(0), 0);
		if (rte_intr_vec_list_index_set(intr_handle, 0, 0) != 0)
			return -1;
	} else {
		PMD_DRV_LOG(INFO, "VF: enabling RX interrupt with VFIO");
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			/*
			 * The first msix vector is reserved for non
			 * efd interrupts.
			 */
			nn_cfg_writeb(&hw->super, NFP_NET_CFG_RXR_VEC(i), i + 1);
			if (rte_intr_vec_list_index_set(intr_handle, i, i + 1) != 0)
				return -1;
		}
	}

	/* Avoiding TX interrupts */
	hw->super.ctrl |= NFP_NET_CFG_CTRL_MSIX_TX_OFF;
	return 0;
}

uint32_t
nfp_check_offloads(struct rte_eth_dev *dev)
{
	uint32_t cap;
	uint32_t ctrl = 0;
	uint64_t rx_offload;
	uint64_t tx_offload;
	struct nfp_net_hw *hw;
	struct rte_eth_conf *dev_conf;

	hw = nfp_net_get_hw(dev);
	cap = hw->super.cap;

	dev_conf = &dev->data->dev_conf;
	rx_offload = dev_conf->rxmode.offloads;
	tx_offload = dev_conf->txmode.offloads;

	if ((rx_offload & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM) != 0) {
		if ((cap & NFP_NET_CFG_CTRL_RXCSUM) != 0)
			ctrl |= NFP_NET_CFG_CTRL_RXCSUM;
	}

	if ((rx_offload & RTE_ETH_RX_OFFLOAD_VLAN_STRIP) != 0)
		nfp_net_enable_rxvlan_cap(hw, &ctrl);

	if ((rx_offload & RTE_ETH_RX_OFFLOAD_QINQ_STRIP) != 0) {
		if ((cap & NFP_NET_CFG_CTRL_RXQINQ) != 0)
			ctrl |= NFP_NET_CFG_CTRL_RXQINQ;
	}

	hw->mtu = dev->data->mtu;

	if ((tx_offload & RTE_ETH_TX_OFFLOAD_VLAN_INSERT) != 0) {
		if ((cap & NFP_NET_CFG_CTRL_TXVLAN_V2) != 0)
			ctrl |= NFP_NET_CFG_CTRL_TXVLAN_V2;
		else if ((cap & NFP_NET_CFG_CTRL_TXVLAN) != 0)
			ctrl |= NFP_NET_CFG_CTRL_TXVLAN;
	}

	/* L2 broadcast */
	if ((cap & NFP_NET_CFG_CTRL_L2BC) != 0)
		ctrl |= NFP_NET_CFG_CTRL_L2BC;

	/* L2 multicast */
	if ((cap & NFP_NET_CFG_CTRL_L2MC) != 0)
		ctrl |= NFP_NET_CFG_CTRL_L2MC;

	/* TX checksum offload */
	if ((tx_offload & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM) != 0 ||
			(tx_offload & RTE_ETH_TX_OFFLOAD_UDP_CKSUM) != 0 ||
			(tx_offload & RTE_ETH_TX_OFFLOAD_TCP_CKSUM) != 0)
		ctrl |= NFP_NET_CFG_CTRL_TXCSUM;

	/* LSO offload */
	if ((tx_offload & RTE_ETH_TX_OFFLOAD_TCP_TSO) != 0 ||
			(tx_offload & RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO) != 0) {
		if ((cap & NFP_NET_CFG_CTRL_LSO) != 0)
			ctrl |= NFP_NET_CFG_CTRL_LSO;
		else
			ctrl |= NFP_NET_CFG_CTRL_LSO2;
	}

	/* RX gather */
	if ((tx_offload & RTE_ETH_TX_OFFLOAD_MULTI_SEGS) != 0)
		ctrl |= NFP_NET_CFG_CTRL_GATHER;

	return ctrl;
}

int
nfp_net_promisc_enable(struct rte_eth_dev *dev)
{
	int ret;
	uint32_t update;
	uint32_t new_ctrl;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);

	hw = &net_hw->super;
	if ((hw->cap & NFP_NET_CFG_CTRL_PROMISC) == 0) {
		PMD_DRV_LOG(ERR, "Promiscuous mode not supported");
		return -ENOTSUP;
	}

	if ((hw->ctrl & NFP_NET_CFG_CTRL_PROMISC) != 0) {
		PMD_DRV_LOG(INFO, "Promiscuous mode already enabled");
		return 0;
	}

	new_ctrl = hw->ctrl | NFP_NET_CFG_CTRL_PROMISC;
	update = NFP_NET_CFG_UPDATE_GEN;

	ret = nfp_reconfig(hw, new_ctrl, update);
	if (ret != 0)
		return ret;

	hw->ctrl = new_ctrl;

	return 0;
}

int
nfp_net_promisc_disable(struct rte_eth_dev *dev)
{
	int ret;
	uint32_t update;
	uint32_t new_ctrl;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	if ((hw->ctrl & NFP_NET_CFG_CTRL_PROMISC) == 0) {
		PMD_DRV_LOG(INFO, "Promiscuous mode already disabled");
		return 0;
	}

	new_ctrl = hw->ctrl & ~NFP_NET_CFG_CTRL_PROMISC;
	update = NFP_NET_CFG_UPDATE_GEN;

	ret = nfp_reconfig(hw, new_ctrl, update);
	if (ret != 0)
		return ret;

	hw->ctrl = new_ctrl;

	return 0;
}

static int
nfp_net_set_allmulticast_mode(struct rte_eth_dev *dev,
		bool enable)
{
	int ret;
	uint32_t update;
	struct nfp_hw *hw;
	uint32_t cap_extend;
	uint32_t ctrl_extend;
	uint32_t new_ctrl_extend;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	cap_extend = hw->cap_ext;
	if ((cap_extend & NFP_NET_CFG_CTRL_MCAST_FILTER) == 0) {
		PMD_DRV_LOG(ERR, "Allmulticast mode not supported");
		return -ENOTSUP;
	}

	/*
	 * Allmulticast mode enabled when NFP_NET_CFG_CTRL_MCAST_FILTER bit is 0.
	 * Allmulticast mode disabled when NFP_NET_CFG_CTRL_MCAST_FILTER bit is 1.
	 */
	ctrl_extend = hw->ctrl_ext;
	if (enable) {
		if ((ctrl_extend & NFP_NET_CFG_CTRL_MCAST_FILTER) == 0)
			return 0;

		new_ctrl_extend = ctrl_extend & ~NFP_NET_CFG_CTRL_MCAST_FILTER;
	} else {
		if ((ctrl_extend & NFP_NET_CFG_CTRL_MCAST_FILTER) != 0)
			return 0;

		new_ctrl_extend = ctrl_extend | NFP_NET_CFG_CTRL_MCAST_FILTER;
	}

	update = NFP_NET_CFG_UPDATE_GEN;

	ret = nfp_ext_reconfig(hw, new_ctrl_extend, update);
	if (ret != 0)
		return ret;

	hw->ctrl_ext = new_ctrl_extend;
	return 0;
}

int
nfp_net_allmulticast_enable(struct rte_eth_dev *dev)
{
	return nfp_net_set_allmulticast_mode(dev, true);
}

int
nfp_net_allmulticast_disable(struct rte_eth_dev *dev)
{
	return nfp_net_set_allmulticast_mode(dev, false);
}

int
nfp_net_link_update_common(struct rte_eth_dev *dev,
		struct nfp_net_hw *hw,
		struct rte_eth_link *link,
		uint32_t link_status)
{
	int ret;
	uint32_t i;
	uint32_t nn_link_status;
	struct nfp_eth_table *nfp_eth_table;

	link->link_speed = RTE_ETH_SPEED_NUM_NONE;

	if (link->link_status == RTE_ETH_LINK_UP) {
		if (hw->pf_dev != NULL) {
			nfp_eth_table = hw->pf_dev->nfp_eth_table;
			if (nfp_eth_table != NULL) {
				uint32_t speed = nfp_eth_table->ports[hw->idx].speed;
				for (i = 0; i < RTE_DIM(nfp_net_link_speed_nfp2rte); i++) {
					if (nfp_net_link_speed_nfp2rte[i] == speed) {
						link->link_speed = speed;
						break;
					}
				}
			}
		} else {
			/*
			 * Shift and mask nn_link_status so that it is effectively the value
			 * at offset NFP_NET_CFG_STS_NSP_LINK_RATE.
			 */
			nn_link_status = (link_status >> NFP_NET_CFG_STS_LINK_RATE_SHIFT) &
					NFP_NET_CFG_STS_LINK_RATE_MASK;
			if (nn_link_status < RTE_DIM(nfp_net_link_speed_nfp2rte))
				link->link_speed = nfp_net_link_speed_nfp2rte[nn_link_status];
		}
	}

	ret = rte_eth_linkstatus_set(dev, link);
	if (ret == 0) {
		if (link->link_status != 0)
			PMD_DRV_LOG(INFO, "NIC Link is Up");
		else
			PMD_DRV_LOG(INFO, "NIC Link is Down");
	}

	return ret;
}

/*
 * Return 0 means link status changed, -1 means not changed
 *
 * Wait to complete is needed as it can take up to 9 seconds to get the Link
 * status.
 */
int
nfp_net_link_update(struct rte_eth_dev *dev,
		__rte_unused int wait_to_complete)
{
	int ret;
	struct nfp_net_hw *hw;
	uint32_t nn_link_status;
	struct rte_eth_link link;

	hw = nfp_net_get_hw(dev);

	memset(&link, 0, sizeof(struct rte_eth_link));

	/* Read link status */
	nn_link_status = nn_cfg_readw(&hw->super, NFP_NET_CFG_STS);
	if ((nn_link_status & NFP_NET_CFG_STS_LINK) != 0)
		link.link_status = RTE_ETH_LINK_UP;

	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

	ret = nfp_net_link_update_common(dev, hw, &link, nn_link_status);

	/*
	 * Notify the port to update the speed value in the CTRL BAR from NSP.
	 * Not applicable for VFs as the associated PF is still attached to the
	 * kernel driver.
	 */
	if (hw->pf_dev != NULL)
		nfp_net_notify_port_speed(hw, &link);

	return ret;
}

int
nfp_net_stats_get(struct rte_eth_dev *dev,
		struct rte_eth_stats *stats)
{
	uint16_t i;
	struct nfp_net_hw *hw;
	struct rte_eth_stats nfp_dev_stats;

	if (stats == NULL)
		return -EINVAL;

	hw = nfp_net_get_hw(dev);

	memset(&nfp_dev_stats, 0, sizeof(nfp_dev_stats));

	/* Reading per RX ring stats */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nfp_dev_stats.q_ipackets[i] =
				nn_cfg_readq(&hw->super, NFP_NET_CFG_RXR_STATS(i));
		nfp_dev_stats.q_ipackets[i] -=
				hw->eth_stats_base.q_ipackets[i];

		nfp_dev_stats.q_ibytes[i] =
				nn_cfg_readq(&hw->super, NFP_NET_CFG_RXR_STATS(i) + 0x8);
		nfp_dev_stats.q_ibytes[i] -=
				hw->eth_stats_base.q_ibytes[i];
	}

	/* Reading per TX ring stats */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nfp_dev_stats.q_opackets[i] =
				nn_cfg_readq(&hw->super, NFP_NET_CFG_TXR_STATS(i));
		nfp_dev_stats.q_opackets[i] -= hw->eth_stats_base.q_opackets[i];

		nfp_dev_stats.q_obytes[i] =
				nn_cfg_readq(&hw->super, NFP_NET_CFG_TXR_STATS(i) + 0x8);
		nfp_dev_stats.q_obytes[i] -= hw->eth_stats_base.q_obytes[i];
	}

	nfp_dev_stats.ipackets = nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_RX_FRAMES);
	nfp_dev_stats.ipackets -= hw->eth_stats_base.ipackets;

	nfp_dev_stats.ibytes = nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_RX_OCTETS);
	nfp_dev_stats.ibytes -= hw->eth_stats_base.ibytes;

	nfp_dev_stats.opackets =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_TX_FRAMES);
	nfp_dev_stats.opackets -= hw->eth_stats_base.opackets;

	nfp_dev_stats.obytes =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_TX_OCTETS);
	nfp_dev_stats.obytes -= hw->eth_stats_base.obytes;

	/* Reading general device stats */
	nfp_dev_stats.ierrors =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_RX_ERRORS);
	nfp_dev_stats.ierrors -= hw->eth_stats_base.ierrors;

	nfp_dev_stats.oerrors =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_TX_ERRORS);
	nfp_dev_stats.oerrors -= hw->eth_stats_base.oerrors;

	/* RX ring mbuf allocation failures */
	nfp_dev_stats.rx_nombuf = dev->data->rx_mbuf_alloc_failed;

	nfp_dev_stats.imissed =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_RX_DISCARDS);
	nfp_dev_stats.imissed -= hw->eth_stats_base.imissed;

	memcpy(stats, &nfp_dev_stats, sizeof(*stats));
	return 0;
}

/*
 * hw->eth_stats_base records the per counter starting point.
 * Lets update it now.
 */
int
nfp_net_stats_reset(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_hw *hw;

	hw = nfp_net_get_hw(dev);

	/* Reading per RX ring stats */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		hw->eth_stats_base.q_ipackets[i] =
				nn_cfg_readq(&hw->super, NFP_NET_CFG_RXR_STATS(i));

		hw->eth_stats_base.q_ibytes[i] =
				nn_cfg_readq(&hw->super, NFP_NET_CFG_RXR_STATS(i) + 0x8);
	}

	/* Reading per TX ring stats */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		hw->eth_stats_base.q_opackets[i] =
				nn_cfg_readq(&hw->super, NFP_NET_CFG_TXR_STATS(i));

		hw->eth_stats_base.q_obytes[i] =
				nn_cfg_readq(&hw->super, NFP_NET_CFG_TXR_STATS(i) + 0x8);
	}

	hw->eth_stats_base.ipackets =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_RX_FRAMES);

	hw->eth_stats_base.ibytes =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_RX_OCTETS);

	hw->eth_stats_base.opackets =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_TX_FRAMES);

	hw->eth_stats_base.obytes =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_TX_OCTETS);

	/* Reading general device stats */
	hw->eth_stats_base.ierrors =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_RX_ERRORS);

	hw->eth_stats_base.oerrors =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_TX_ERRORS);

	/* RX ring mbuf allocation failures */
	dev->data->rx_mbuf_alloc_failed = 0;

	hw->eth_stats_base.imissed =
			nn_cfg_readq(&hw->super, NFP_NET_CFG_STATS_RX_DISCARDS);

	return 0;
}

uint32_t
nfp_net_xstats_size(const struct rte_eth_dev *dev)
{
	uint32_t count;
	struct nfp_net_hw *hw;
	const uint32_t size = RTE_DIM(nfp_net_xstats);

	/* If the device is a VF, then there will be no MAC stats */
	hw = nfp_net_get_hw(dev);
	if (hw->mac_stats == NULL) {
		for (count = 0; count < size; count++) {
			if (nfp_net_xstats[count].group == NFP_XSTAT_GROUP_MAC)
				break;
		}

		return count;
	}

	return size;
}

static const struct nfp_xstat *
nfp_net_xstats_info(const struct rte_eth_dev *dev,
		uint32_t index)
{
	if (index >= nfp_net_xstats_size(dev)) {
		PMD_DRV_LOG(ERR, "xstat index out of bounds");
		return NULL;
	}

	return &nfp_net_xstats[index];
}

static uint64_t
nfp_net_xstats_value(const struct rte_eth_dev *dev,
		uint32_t index,
		bool raw)
{
	uint64_t value;
	struct nfp_net_hw *hw;
	struct nfp_xstat xstat;

	hw = nfp_net_get_hw(dev);
	xstat = nfp_net_xstats[index];

	if (xstat.group == NFP_XSTAT_GROUP_MAC)
		value = nn_readq(hw->mac_stats + xstat.offset);
	else
		value = nn_cfg_readq(&hw->super, xstat.offset);

	if (raw)
		return value;

	/*
	 * A baseline value of each statistic counter is recorded when stats are "reset".
	 * Thus, the value returned by this function need to be decremented by this
	 * baseline value. The result is the count of this statistic since the last time
	 * it was "reset".
	 */
	return value - hw->eth_xstats_base[index].value;
}

/* NOTE: All callers ensure dev is always set. */
int
nfp_net_xstats_get_names(struct rte_eth_dev *dev,
		struct rte_eth_xstat_name *xstats_names,
		unsigned int size)
{
	uint32_t id;
	uint32_t nfp_size;
	uint32_t read_size;

	nfp_size = nfp_net_xstats_size(dev);

	if (xstats_names == NULL)
		return nfp_size;

	/* Read at most NFP xstats number of names. */
	read_size = RTE_MIN(size, nfp_size);

	for (id = 0; id < read_size; id++)
		rte_strlcpy(xstats_names[id].name, nfp_net_xstats[id].name,
				RTE_ETH_XSTATS_NAME_SIZE);

	return read_size;
}

/* NOTE: All callers ensure dev is always set. */
int
nfp_net_xstats_get(struct rte_eth_dev *dev,
		struct rte_eth_xstat *xstats,
		unsigned int n)
{
	uint32_t id;
	uint32_t nfp_size;
	uint32_t read_size;

	nfp_size = nfp_net_xstats_size(dev);

	if (xstats == NULL)
		return nfp_size;

	/* Read at most NFP xstats number of values. */
	read_size = RTE_MIN(n, nfp_size);

	for (id = 0; id < read_size; id++) {
		xstats[id].id = id;
		xstats[id].value = nfp_net_xstats_value(dev, id, false);
	}

	return read_size;
}

/*
 * NOTE: The only caller rte_eth_xstats_get_names_by_id() ensures dev,
 * ids, xstats_names and size are valid, and non-NULL.
 */
int
nfp_net_xstats_get_names_by_id(struct rte_eth_dev *dev,
		const uint64_t *ids,
		struct rte_eth_xstat_name *xstats_names,
		unsigned int size)
{
	uint32_t i;
	uint32_t read_size;

	/* Read at most NFP xstats number of names. */
	read_size = RTE_MIN(size, nfp_net_xstats_size(dev));

	for (i = 0; i < read_size; i++) {
		const struct nfp_xstat *xstat;

		/* Make sure ID is valid for device. */
		xstat = nfp_net_xstats_info(dev, ids[i]);
		if (xstat == NULL)
			return -EINVAL;

		rte_strlcpy(xstats_names[i].name, xstat->name,
				RTE_ETH_XSTATS_NAME_SIZE);
	}

	return read_size;
}

/*
 * NOTE: The only caller rte_eth_xstats_get_by_id() ensures dev,
 * ids, values and n are valid, and non-NULL.
 */
int
nfp_net_xstats_get_by_id(struct rte_eth_dev *dev,
		const uint64_t *ids,
		uint64_t *values,
		unsigned int n)
{
	uint32_t i;
	uint32_t read_size;

	/* Read at most NFP xstats number of values. */
	read_size = RTE_MIN(n, nfp_net_xstats_size(dev));

	for (i = 0; i < read_size; i++) {
		const struct nfp_xstat *xstat;

		/* Make sure index is valid for device. */
		xstat = nfp_net_xstats_info(dev, ids[i]);
		if (xstat == NULL)
			return -EINVAL;

		values[i] = nfp_net_xstats_value(dev, ids[i], false);
	}

	return read_size;
}

int
nfp_net_xstats_reset(struct rte_eth_dev *dev)
{
	uint32_t id;
	uint32_t read_size;
	struct nfp_net_hw *hw;

	hw = nfp_net_get_hw(dev);
	read_size = nfp_net_xstats_size(dev);

	for (id = 0; id < read_size; id++) {
		hw->eth_xstats_base[id].id = id;
		hw->eth_xstats_base[id].value = nfp_net_xstats_value(dev, id, true);
	}

	/* Successfully reset xstats, now call function to reset basic stats. */
	return nfp_net_stats_reset(dev);
}

void
nfp_net_rx_desc_limits(struct nfp_net_hw *hw,
		uint16_t *min_rx_desc,
		uint16_t *max_rx_desc)
{
	*max_rx_desc = hw->dev_info->max_qc_size;
	*min_rx_desc = hw->dev_info->min_qc_size;
}

void
nfp_net_tx_desc_limits(struct nfp_net_hw *hw,
		uint16_t *min_tx_desc,
		uint16_t *max_tx_desc)
{
	uint16_t tx_dpp;

	if (hw->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3)
		tx_dpp = NFD3_TX_DESC_PER_PKT;
	else
		tx_dpp = NFDK_TX_DESC_PER_SIMPLE_PKT;

	*max_tx_desc = hw->dev_info->max_qc_size / tx_dpp;
	*min_tx_desc = hw->dev_info->min_qc_size / tx_dpp;
}

int
nfp_net_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	uint32_t cap;
	uint32_t cap_extend;
	uint16_t min_rx_desc;
	uint16_t max_rx_desc;
	uint16_t min_tx_desc;
	uint16_t max_tx_desc;
	struct nfp_net_hw *hw;

	hw = nfp_net_get_hw(dev);

	nfp_net_rx_desc_limits(hw, &min_rx_desc, &max_rx_desc);
	nfp_net_tx_desc_limits(hw, &min_tx_desc, &max_tx_desc);

	dev_info->max_rx_queues = (uint16_t)hw->max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->max_tx_queues;
	dev_info->min_rx_bufsize = RTE_ETHER_MIN_MTU;
	/*
	 * The maximum rx packet length is set to the maximum layer 3 MTU,
	 * plus layer 2, CRC and VLAN headers.
	 * The maximum layer 3 MTU (max_mtu) is read from hardware,
	 * which was set by the firmware loaded onto the card.
	 */
	dev_info->max_rx_pktlen = hw->max_mtu + NFP_ETH_OVERHEAD;
	dev_info->max_mtu = hw->max_mtu;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	/* Next should change when PF support is implemented */
	dev_info->max_mac_addrs = 1;

	cap = hw->super.cap;

	if ((cap & (NFP_NET_CFG_CTRL_RXVLAN | NFP_NET_CFG_CTRL_RXVLAN_V2)) != 0)
		dev_info->rx_offload_capa = RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	if ((cap & NFP_NET_CFG_CTRL_RXQINQ) != 0)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_QINQ_STRIP;

	if ((cap & NFP_NET_CFG_CTRL_RXCSUM) != 0)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
				RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
				RTE_ETH_RX_OFFLOAD_TCP_CKSUM;

	if ((cap & (NFP_NET_CFG_CTRL_TXVLAN | NFP_NET_CFG_CTRL_TXVLAN_V2)) != 0)
		dev_info->tx_offload_capa = RTE_ETH_TX_OFFLOAD_VLAN_INSERT;

	if ((cap & NFP_NET_CFG_CTRL_TXCSUM) != 0)
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
				RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
				RTE_ETH_TX_OFFLOAD_TCP_CKSUM;

	if ((cap & NFP_NET_CFG_CTRL_LSO_ANY) != 0) {
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_TCP_TSO;
		if ((cap & NFP_NET_CFG_CTRL_VXLAN) != 0)
			dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO;
	}

	if ((cap & NFP_NET_CFG_CTRL_GATHER) != 0)
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	cap_extend = hw->super.cap_ext;
	if ((cap_extend & NFP_NET_CFG_CTRL_IPSEC) != 0) {
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_SECURITY;
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_SECURITY;
	}

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = DEFAULT_RX_PTHRESH,
			.hthresh = DEFAULT_RX_HTHRESH,
			.wthresh = DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = DEFAULT_TX_PTHRESH,
			.hthresh = DEFAULT_TX_HTHRESH,
			.wthresh = DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = DEFAULT_TX_RSBIT_THRESH,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = max_rx_desc,
		.nb_min = min_rx_desc,
		.nb_align = NFP_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = max_tx_desc,
		.nb_min = min_tx_desc,
		.nb_align = NFP_ALIGN_RING_DESC,
		.nb_seg_max = NFP_TX_MAX_SEG,
		.nb_mtu_seg_max = NFP_TX_MAX_MTU_SEG,
	};

	if ((cap & NFP_NET_CFG_CTRL_RSS_ANY) != 0) {
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

		dev_info->flow_type_rss_offloads = RTE_ETH_RSS_IPV4 |
				RTE_ETH_RSS_NONFRAG_IPV4_TCP |
				RTE_ETH_RSS_NONFRAG_IPV4_UDP |
				RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
				RTE_ETH_RSS_IPV6 |
				RTE_ETH_RSS_NONFRAG_IPV6_TCP |
				RTE_ETH_RSS_NONFRAG_IPV6_UDP |
				RTE_ETH_RSS_NONFRAG_IPV6_SCTP;

		dev_info->reta_size = NFP_NET_CFG_RSS_ITBL_SZ;
		dev_info->hash_key_size = NFP_NET_CFG_RSS_KEY_SZ;
	}

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_1G |
			RTE_ETH_LINK_SPEED_10G |
			RTE_ETH_LINK_SPEED_25G |
			RTE_ETH_LINK_SPEED_40G |
			RTE_ETH_LINK_SPEED_50G |
			RTE_ETH_LINK_SPEED_100G;

	return 0;
}

int
nfp_net_common_init(struct rte_pci_device *pci_dev,
		struct nfp_net_hw *hw)
{
	const int stride = 4;

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	hw->max_rx_queues = nn_cfg_readl(&hw->super, NFP_NET_CFG_MAX_RXRINGS);
	hw->max_tx_queues = nn_cfg_readl(&hw->super, NFP_NET_CFG_MAX_TXRINGS);
	if (hw->max_rx_queues == 0 || hw->max_tx_queues == 0) {
		PMD_INIT_LOG(ERR, "Device %s can not be used, there are no valid queue "
				"pairs for use", pci_dev->name);
		return -ENODEV;
	}

	nfp_net_cfg_read_version(hw);
	if (!nfp_net_is_valid_nfd_version(hw->ver))
		return -EINVAL;

	if (nfp_net_check_dma_mask(hw, pci_dev->name) != 0)
		return -ENODEV;

	/* Get some of the read-only fields from the config BAR */
	hw->super.cap = nn_cfg_readl(&hw->super, NFP_NET_CFG_CAP);
	hw->super.cap_ext = nn_cfg_readl(&hw->super, NFP_NET_CFG_CAP_WORD1);
	hw->max_mtu = nn_cfg_readl(&hw->super, NFP_NET_CFG_MAX_MTU);
	hw->flbufsz = DEFAULT_FLBUF_SIZE;

	nfp_net_init_metadata_format(hw);

	/* Read the Rx offset configured from firmware */
	if (hw->ver.major < 2)
		hw->rx_offset = NFP_NET_RX_OFFSET;
	else
		hw->rx_offset = nn_cfg_readl(&hw->super, NFP_NET_CFG_RX_OFFSET);

	hw->super.ctrl = 0;
	hw->stride_rx = stride;
	hw->stride_tx = stride;

	return 0;
}

const uint32_t *
nfp_net_supported_ptypes_get(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *net_hw;
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_TUNNEL_VXLAN,
		RTE_PTYPE_TUNNEL_NVGRE,
		RTE_PTYPE_TUNNEL_GENEVE,
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L3_IPV4,
		RTE_PTYPE_INNER_L3_IPV4_EXT,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_INNER_L3_IPV6_EXT,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_ICMP,
		RTE_PTYPE_INNER_L4_SCTP,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst != nfp_net_recv_pkts)
		return NULL;

	net_hw = dev->data->dev_private;
	if ((net_hw->super.cap_ext & NFP_NET_CFG_CTRL_PKT_TYPE) == 0)
		return NULL;

	return ptypes;
}

int
nfp_rx_queue_intr_enable(struct rte_eth_dev *dev,
		uint16_t queue_id)
{
	uint16_t base = 0;
	struct nfp_net_hw *hw;
	struct rte_pci_device *pci_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	if (rte_intr_type_get(pci_dev->intr_handle) != RTE_INTR_HANDLE_UIO)
		base = 1;

	/* Make sure all updates are written before un-masking */
	rte_wmb();

	hw = nfp_net_get_hw(dev);
	nn_cfg_writeb(&hw->super, NFP_NET_CFG_ICR(base + queue_id),
			NFP_NET_CFG_ICR_UNMASKED);
	return 0;
}

int
nfp_rx_queue_intr_disable(struct rte_eth_dev *dev,
		uint16_t queue_id)
{
	uint16_t base = 0;
	struct nfp_net_hw *hw;
	struct rte_pci_device *pci_dev;

	pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	if (rte_intr_type_get(pci_dev->intr_handle) != RTE_INTR_HANDLE_UIO)
		base = 1;

	/* Make sure all updates are written before un-masking */
	rte_wmb();

	hw = nfp_net_get_hw(dev);
	nn_cfg_writeb(&hw->super, NFP_NET_CFG_ICR(base + queue_id), NFP_NET_CFG_ICR_RXTX);

	return 0;
}

static void
nfp_net_dev_link_status_print(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	rte_eth_linkstatus_get(dev, &link);
	if (link.link_status != 0)
		PMD_DRV_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s",
				dev->data->port_id, link.link_speed,
				link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX ?
				"full-duplex" : "half-duplex");
	else
		PMD_DRV_LOG(INFO, " Port %d: Link Down", dev->data->port_id);

	PMD_DRV_LOG(INFO, "PCI Address: " PCI_PRI_FMT,
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
}

/*
 * Unmask an interrupt
 *
 * If MSI-X auto-masking is enabled clear the mask bit, otherwise
 * clear the ICR for the entry.
 */
void
nfp_net_irq_unmask(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	struct rte_pci_device *pci_dev;

	hw = nfp_net_get_hw(dev);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	/* Make sure all updates are written before un-masking */
	rte_wmb();

	if ((hw->super.ctrl & NFP_NET_CFG_CTRL_MSIXAUTO) != 0) {
		/* If MSI-X auto-masking is used, clear the entry */
		rte_intr_ack(pci_dev->intr_handle);
	} else {
		nn_cfg_writeb(&hw->super, NFP_NET_CFG_ICR(NFP_NET_IRQ_LSC_IDX),
				NFP_NET_CFG_ICR_UNMASKED);
	}
}

/**
 * Interrupt handler which shall be registered for alarm callback for delayed
 * handling specific interrupt to wait for the stable nic state. As the NIC
 * interrupt state is not stable for nfp after link is just down, it needs
 * to wait 4 seconds to get the stable status.
 *
 * @param param
 *   The address of parameter (struct rte_eth_dev *)
 */
void
nfp_net_dev_interrupt_delayed_handler(void *param)
{
	struct rte_eth_dev *dev = param;

	nfp_net_link_update(dev, 0);
	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);

	nfp_net_dev_link_status_print(dev);

	/* Unmasking */
	nfp_net_irq_unmask(dev);
}

void
nfp_net_dev_interrupt_handler(void *param)
{
	int64_t timeout;
	struct rte_eth_link link;
	struct rte_eth_dev *dev = param;

	PMD_DRV_LOG(DEBUG, "We got a LSC interrupt!!!");

	rte_eth_linkstatus_get(dev, &link);

	nfp_net_link_update(dev, 0);

	/* Likely to up */
	if (link.link_status == 0) {
		/* Handle it 1 sec later, wait it being stable */
		timeout = NFP_NET_LINK_UP_CHECK_TIMEOUT;
	} else {  /* Likely to down */
		/* Handle it 4 sec later, wait it being stable */
		timeout = NFP_NET_LINK_DOWN_CHECK_TIMEOUT;
	}

	if (rte_eal_alarm_set(timeout * 1000,
			nfp_net_dev_interrupt_delayed_handler,
			(void *)dev) != 0) {
		PMD_INIT_LOG(ERR, "Error setting alarm");
		/* Unmasking */
		nfp_net_irq_unmask(dev);
	}
}

int
nfp_net_dev_mtu_set(struct rte_eth_dev *dev,
		uint16_t mtu)
{
	struct nfp_net_hw *hw;

	hw = nfp_net_get_hw(dev);

	/* MTU setting is forbidden if port is started */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "port %d must be stopped before configuration",
				dev->data->port_id);
		return -EBUSY;
	}

	/* MTU larger than current mbufsize not supported */
	if (mtu > hw->flbufsz) {
		PMD_DRV_LOG(ERR, "MTU (%u) larger than current mbufsize (%u) not supported",
				mtu, hw->flbufsz);
		return -ERANGE;
	}

	/* Writing to configuration space */
	nn_cfg_writel(&hw->super, NFP_NET_CFG_MTU, mtu);

	hw->mtu = mtu;

	return 0;
}

int
nfp_net_vlan_offload_set(struct rte_eth_dev *dev,
		int mask)
{
	int ret;
	uint32_t update;
	uint32_t new_ctrl;
	struct nfp_hw *hw;
	uint64_t rx_offload;
	struct nfp_net_hw *net_hw;
	uint32_t rxvlan_ctrl = 0;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;
	rx_offload = dev->data->dev_conf.rxmode.offloads;
	new_ctrl = hw->ctrl;

	/* VLAN stripping setting */
	if ((mask & RTE_ETH_VLAN_STRIP_MASK) != 0) {
		nfp_net_enable_rxvlan_cap(net_hw, &rxvlan_ctrl);
		if ((rx_offload & RTE_ETH_RX_OFFLOAD_VLAN_STRIP) != 0)
			new_ctrl |= rxvlan_ctrl;
		else
			new_ctrl &= ~rxvlan_ctrl;
	}

	/* QinQ stripping setting */
	if ((mask & RTE_ETH_QINQ_STRIP_MASK) != 0) {
		if ((rx_offload & RTE_ETH_RX_OFFLOAD_QINQ_STRIP) != 0)
			new_ctrl |= NFP_NET_CFG_CTRL_RXQINQ;
		else
			new_ctrl &= ~NFP_NET_CFG_CTRL_RXQINQ;
	}

	if (new_ctrl == hw->ctrl)
		return 0;

	update = NFP_NET_CFG_UPDATE_GEN;

	ret = nfp_reconfig(hw, new_ctrl, update);
	if (ret != 0)
		return ret;

	hw->ctrl = new_ctrl;

	return 0;
}

static int
nfp_net_rss_reta_write(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size)
{
	uint16_t i;
	uint16_t j;
	uint16_t idx;
	uint8_t mask;
	uint32_t reta;
	uint16_t shift;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	if (reta_size != NFP_NET_CFG_RSS_ITBL_SZ) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured (%hu)"
				" doesn't match hardware can supported (%d)",
				reta_size, NFP_NET_CFG_RSS_ITBL_SZ);
		return -EINVAL;
	}

	/*
	 * Update Redirection Table. There are 128 8bit-entries which can be
	 * manage as 32 32bit-entries.
	 */
	for (i = 0; i < reta_size; i += 4) {
		/* Handling 4 RSS entries per loop */
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) & 0xF);
		if (mask == 0)
			continue;

		reta = 0;

		/* If all 4 entries were set, don't need read RETA register */
		if (mask != 0xF)
			reta = nn_cfg_readl(hw, NFP_NET_CFG_RSS_ITBL + i);

		for (j = 0; j < 4; j++) {
			if ((mask & (0x1 << j)) == 0)
				continue;

			/* Clearing the entry bits */
			if (mask != 0xF)
				reta &= ~(0xFF << (8 * j));

			reta |= reta_conf[idx].reta[shift + j] << (8 * j);
		}

		nn_cfg_writel(hw, NFP_NET_CFG_RSS_ITBL + (idx * 64) + shift, reta);
	}

	return 0;
}

/* Update Redirection Table(RETA) of Receive Side Scaling of Ethernet device */
int
nfp_net_reta_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size)
{
	int ret;
	uint32_t update;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0)
		return -EINVAL;

	ret = nfp_net_rss_reta_write(dev, reta_conf, reta_size);
	if (ret != 0)
		return ret;

	update = NFP_NET_CFG_UPDATE_RSS;

	if (nfp_reconfig(hw, hw->ctrl, update) != 0)
		return -EIO;

	return 0;
}

/* Query Redirection Table(RETA) of Receive Side Scaling of Ethernet device. */
int
nfp_net_reta_query(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size)
{
	uint16_t i;
	uint16_t j;
	uint16_t idx;
	uint8_t mask;
	uint32_t reta;
	uint16_t shift;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0)
		return -EINVAL;

	if (reta_size != NFP_NET_CFG_RSS_ITBL_SZ) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured (%d)"
				" doesn't match hardware can supported (%d)",
				reta_size, NFP_NET_CFG_RSS_ITBL_SZ);
		return -EINVAL;
	}

	/*
	 * Reading Redirection Table. There are 128 8bit-entries which can be
	 * manage as 32 32bit-entries.
	 */
	for (i = 0; i < reta_size; i += 4) {
		/* Handling 4 RSS entries per loop */
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (reta_conf[idx].mask >> shift) & 0xF;

		if (mask == 0)
			continue;

		reta = nn_cfg_readl(hw, NFP_NET_CFG_RSS_ITBL + (idx * 64) + shift);
		for (j = 0; j < 4; j++) {
			if ((mask & (0x1 << j)) == 0)
				continue;

			reta_conf[idx].reta[shift + j] =
					(uint8_t)((reta >> (8 * j)) & 0xF);
		}
	}

	return 0;
}

static int
nfp_net_rss_hash_write(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	uint8_t i;
	uint8_t key;
	uint64_t rss_hf;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;
	uint32_t cfg_rss_ctrl = 0;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	/* Writing the key byte by byte */
	for (i = 0; i < rss_conf->rss_key_len; i++) {
		memcpy(&key, &rss_conf->rss_key[i], 1);
		nn_cfg_writeb(hw, NFP_NET_CFG_RSS_KEY + i, key);
	}

	rss_hf = rss_conf->rss_hf;

	if ((rss_hf & RTE_ETH_RSS_IPV4) != 0)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP) != 0)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_TCP;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP) != 0)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_UDP;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_SCTP) != 0)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_SCTP;

	if ((rss_hf & RTE_ETH_RSS_IPV6) != 0)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP) != 0)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_TCP;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP) != 0)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_UDP;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_SCTP) != 0)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_SCTP;

	cfg_rss_ctrl |= NFP_NET_CFG_RSS_MASK;
	cfg_rss_ctrl |= NFP_NET_CFG_RSS_TOEPLITZ;

	/* Configuring where to apply the RSS hash */
	nn_cfg_writel(hw, NFP_NET_CFG_RSS_CTRL, cfg_rss_ctrl);

	/* Writing the key size */
	nn_cfg_writeb(hw, NFP_NET_CFG_RSS_KEY_SZ, rss_conf->rss_key_len);

	return 0;
}

int
nfp_net_rss_hash_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	uint32_t update;
	uint64_t rss_hf;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	rss_hf = rss_conf->rss_hf;

	/* Checking if RSS is enabled */
	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0) {
		if (rss_hf != 0) {
			PMD_DRV_LOG(ERR, "RSS unsupported");
			return -EINVAL;
		}

		return 0; /* Nothing to do */
	}

	if (rss_conf->rss_key_len > NFP_NET_CFG_RSS_KEY_SZ) {
		PMD_DRV_LOG(ERR, "RSS hash key too long");
		return -EINVAL;
	}

	nfp_net_rss_hash_write(dev, rss_conf);

	update = NFP_NET_CFG_UPDATE_RSS;

	if (nfp_reconfig(hw, hw->ctrl, update) != 0)
		return -EIO;

	return 0;
}

int
nfp_net_rss_hash_conf_get(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	uint8_t i;
	uint8_t key;
	uint64_t rss_hf;
	struct nfp_hw *hw;
	uint32_t cfg_rss_ctrl;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0)
		return -EINVAL;

	rss_hf = rss_conf->rss_hf;
	cfg_rss_ctrl = nn_cfg_readl(hw, NFP_NET_CFG_RSS_CTRL);

	if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4) != 0)
		rss_hf |= RTE_ETH_RSS_IPV4;

	if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4_TCP) != 0)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;

	if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6_TCP) != 0)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_TCP;

	if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4_UDP) != 0)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;

	if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6_UDP) != 0)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_UDP;

	if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6) != 0)
		rss_hf |= RTE_ETH_RSS_IPV6;

	if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4_SCTP) != 0)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_SCTP;

	if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6_SCTP) != 0)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_SCTP;

	/* Propagate current RSS hash functions to caller */
	rss_conf->rss_hf = rss_hf;

	/* Reading the key size */
	rss_conf->rss_key_len = nn_cfg_readl(hw, NFP_NET_CFG_RSS_KEY_SZ);

	/* Reading the key byte a byte */
	for (i = 0; i < rss_conf->rss_key_len; i++) {
		key = nn_cfg_readb(hw, NFP_NET_CFG_RSS_KEY + i);
		memcpy(&rss_conf->rss_key[i], &key, 1);
	}

	return 0;
}

int
nfp_net_rss_config_default(struct rte_eth_dev *dev)
{
	int ret;
	uint8_t i;
	uint8_t j;
	uint16_t queue = 0;
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rss_conf rss_conf;
	uint16_t rx_queues = dev->data->nb_rx_queues;
	struct rte_eth_rss_reta_entry64 nfp_reta_conf[2];

	nfp_reta_conf[0].mask = ~0x0;
	nfp_reta_conf[1].mask = ~0x0;

	for (i = 0; i < 0x40; i += 8) {
		for (j = i; j < (i + 8); j++) {
			nfp_reta_conf[0].reta[j] = queue;
			nfp_reta_conf[1].reta[j] = queue++;
			queue %= rx_queues;
		}
	}

	ret = nfp_net_rss_reta_write(dev, nfp_reta_conf, 0x80);
	if (ret != 0)
		return ret;

	dev_conf = &dev->data->dev_conf;
	if (dev_conf == NULL) {
		PMD_DRV_LOG(ERR, "Wrong rss conf");
		return -EINVAL;
	}

	rss_conf = dev_conf->rx_adv_conf.rss_conf;
	ret = nfp_net_rss_hash_write(dev, &rss_conf);

	return ret;
}

void
nfp_net_stop_rx_queue(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_rxq *this_rx_q;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		this_rx_q = dev->data->rx_queues[i];
		nfp_net_reset_rx_queue(this_rx_q);
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
}

void
nfp_net_close_rx_queue(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_rxq *this_rx_q;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		this_rx_q = dev->data->rx_queues[i];
		nfp_net_reset_rx_queue(this_rx_q);
		nfp_net_rx_queue_release(dev, i);
	}
}

void
nfp_net_stop_tx_queue(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_txq *this_tx_q;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		this_tx_q = dev->data->tx_queues[i];
		nfp_net_reset_tx_queue(this_tx_q);
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
}

void
nfp_net_close_tx_queue(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_txq *this_tx_q;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		this_tx_q = dev->data->tx_queues[i];
		nfp_net_reset_tx_queue(this_tx_q);
		nfp_net_tx_queue_release(dev, i);
	}
}

int
nfp_net_set_vxlan_port(struct nfp_net_hw *net_hw,
		size_t idx,
		uint16_t port)
{
	int ret;
	uint32_t i;
	struct nfp_hw *hw = &net_hw->super;

	if (idx >= NFP_NET_N_VXLAN_PORTS) {
		PMD_DRV_LOG(ERR, "The idx value is out of range.");
		return -ERANGE;
	}

	net_hw->vxlan_ports[idx] = port;

	for (i = 0; i < NFP_NET_N_VXLAN_PORTS; i += 2) {
		nn_cfg_writel(hw, NFP_NET_CFG_VXLAN_PORT + i * sizeof(port),
				(net_hw->vxlan_ports[i + 1] << 16) | net_hw->vxlan_ports[i]);
	}

	rte_spinlock_lock(&hw->reconfig_lock);

	nn_cfg_writel(hw, NFP_NET_CFG_UPDATE, NFP_NET_CFG_UPDATE_VXLAN);
	rte_wmb();

	ret = nfp_reconfig_real(hw, NFP_NET_CFG_UPDATE_VXLAN);

	rte_spinlock_unlock(&hw->reconfig_lock);

	return ret;
}

/*
 * The firmware with NFD3 can not handle DMA address requiring more
 * than 40 bits.
 */
int
nfp_net_check_dma_mask(struct nfp_net_hw *hw,
		char *name)
{
	if (hw->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3 &&
			rte_mem_check_dma_mask(40) != 0) {
		PMD_DRV_LOG(ERR, "Device %s can't be used: restricted dma mask to 40 bits!",
				name);
		return -ENODEV;
	}

	return 0;
}

void
nfp_net_init_metadata_format(struct nfp_net_hw *hw)
{
	/*
	 * ABI 4.x and ctrl vNIC always use chained metadata, in other cases we allow use of
	 * single metadata if only RSS(v1) is supported by hw capability, and RSS(v2)
	 * also indicate that we are using chained metadata.
	 */
	if (hw->ver.major == 4) {
		hw->meta_format = NFP_NET_METAFORMAT_CHAINED;
	} else if ((hw->super.cap & NFP_NET_CFG_CTRL_CHAIN_META) != 0) {
		hw->meta_format = NFP_NET_METAFORMAT_CHAINED;
		/*
		 * RSS is incompatible with chained metadata. hw->super.cap just represents
		 * firmware's ability rather than the firmware's configuration. We decide
		 * to reduce the confusion to allow us can use hw->super.cap to identify RSS later.
		 */
		hw->super.cap &= ~NFP_NET_CFG_CTRL_RSS;
	} else {
		hw->meta_format = NFP_NET_METAFORMAT_SINGLE;
	}
}

void
nfp_net_cfg_read_version(struct nfp_net_hw *hw)
{
	union {
		uint32_t whole;
		struct nfp_net_fw_ver split;
	} version;

	version.whole = nn_cfg_readl(&hw->super, NFP_NET_CFG_VERSION);
	hw->ver = version.split;
}

static void
nfp_net_get_nsp_info(struct nfp_net_hw *hw,
		char *nsp_version)
{
	struct nfp_nsp *nsp;

	nsp = nfp_nsp_open(hw->cpp);
	if (nsp == NULL)
		return;

	snprintf(nsp_version, FW_VER_LEN, "%hu.%hu",
			nfp_nsp_get_abi_ver_major(nsp),
			nfp_nsp_get_abi_ver_minor(nsp));

	nfp_nsp_close(nsp);
}

static void
nfp_net_get_mip_name(struct nfp_net_hw *hw,
		char *mip_name)
{
	struct nfp_mip *mip;

	mip = nfp_mip_open(hw->cpp);
	if (mip == NULL)
		return;

	snprintf(mip_name, FW_VER_LEN, "%s", nfp_mip_name(mip));

	nfp_mip_close(mip);
}

static void
nfp_net_get_app_name(struct nfp_net_hw *hw,
		char *app_name)
{
	switch (hw->pf_dev->app_fw_id) {
	case NFP_APP_FW_CORE_NIC:
		snprintf(app_name, FW_VER_LEN, "%s", "nic");
		break;
	case NFP_APP_FW_FLOWER_NIC:
		snprintf(app_name, FW_VER_LEN, "%s", "flower");
		break;
	default:
		snprintf(app_name, FW_VER_LEN, "%s", "unknown");
		break;
	}
}

int
nfp_net_firmware_version_get(struct rte_eth_dev *dev,
		char *fw_version,
		size_t fw_size)
{
	struct nfp_net_hw *hw;
	char mip_name[FW_VER_LEN];
	char app_name[FW_VER_LEN];
	char nsp_version[FW_VER_LEN];
	char vnic_version[FW_VER_LEN];

	if (fw_size < FW_VER_LEN)
		return FW_VER_LEN;

	hw = nfp_net_get_hw(dev);

	if ((dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR) == 0) {
		snprintf(vnic_version, FW_VER_LEN, "%d.%d.%d.%d",
			hw->ver.extend, hw->ver.class,
			hw->ver.major, hw->ver.minor);
	} else {
		snprintf(vnic_version, FW_VER_LEN, "*");
	}

	nfp_net_get_nsp_info(hw, nsp_version);
	nfp_net_get_mip_name(hw, mip_name);
	nfp_net_get_app_name(hw, app_name);

	snprintf(fw_version, FW_VER_LEN, "%s %s %s %s",
			vnic_version, nsp_version, mip_name, app_name);

	return 0;
}

bool
nfp_net_is_valid_nfd_version(struct nfp_net_fw_ver version)
{
	uint8_t nfd_version = version.extend;

	if (nfd_version == NFP_NET_CFG_VERSION_DP_NFD3)
		return true;

	if (nfd_version == NFP_NET_CFG_VERSION_DP_NFDK) {
		if (version.major < 5) {
			PMD_INIT_LOG(ERR, "NFDK must use ABI 5 or newer, found: %d",
					version.major);
			return false;
		}

		return true;
	}

	return false;
}

/* Disable rx and tx functions to allow for reconfiguring. */
int
nfp_net_stop(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	hw = nfp_net_get_hw(dev);

	nfp_net_disable_queues(dev);

	/* Clear queues */
	nfp_net_stop_tx_queue(dev);
	nfp_net_stop_rx_queue(dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		/* Configure the physical port down */
		nfp_eth_set_configured(hw->cpp, hw->nfp_idx, 0);
	else
		nfp_eth_set_configured(dev->process_private, hw->nfp_idx, 0);

	return 0;
}

static enum rte_eth_fc_mode
nfp_net_get_pause_mode(struct nfp_eth_table_port *eth_port)
{
	enum rte_eth_fc_mode mode;

	if (eth_port->rx_pause_enabled) {
		if (eth_port->tx_pause_enabled)
			mode = RTE_ETH_FC_FULL;
		else
			mode = RTE_ETH_FC_RX_PAUSE;
	} else {
		if (eth_port->tx_pause_enabled)
			mode = RTE_ETH_FC_TX_PAUSE;
		else
			mode = RTE_ETH_FC_NONE;
	}

	return mode;
}

int
nfp_net_flow_ctrl_get(struct rte_eth_dev *dev,
		struct rte_eth_fc_conf *fc_conf)
{
	struct nfp_net_hw *net_hw;
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_eth_table_port *eth_port;

	net_hw = nfp_net_get_hw(dev);
	if (net_hw->pf_dev == NULL)
		return -EINVAL;

	nfp_eth_table = net_hw->pf_dev->nfp_eth_table;
	eth_port = &nfp_eth_table->ports[dev->data->port_id];

	/* Currently only RX/TX switch are supported */
	fc_conf->mode = nfp_net_get_pause_mode(eth_port);

	return 0;
}

static int
nfp_net_pause_frame_set(struct nfp_net_hw *net_hw,
		struct nfp_eth_table_port *eth_port,
		enum rte_eth_fc_mode mode)
{
	int err;
	bool flag;
	struct nfp_nsp *nsp;

	nsp = nfp_eth_config_start(net_hw->cpp, eth_port->index);
	if (nsp == NULL) {
		PMD_DRV_LOG(ERR, "NFP error when obtaining NSP handle.");
		return -EIO;
	}

	flag = (mode & RTE_ETH_FC_TX_PAUSE) == 0 ? false : true;
	err = nfp_eth_set_tx_pause(nsp, flag);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to configure TX pause frame.");
		nfp_eth_config_cleanup_end(nsp);
		return err;
	}

	flag = (mode & RTE_ETH_FC_RX_PAUSE) == 0 ? false : true;
	err = nfp_eth_set_rx_pause(nsp, flag);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to configure RX pause frame.");
		nfp_eth_config_cleanup_end(nsp);
		return err;
	}

	err = nfp_eth_config_commit_end(nsp);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to configure pause frame.");
		return err;
	}

	return 0;
}

int
nfp_net_flow_ctrl_set(struct rte_eth_dev *dev,
		struct rte_eth_fc_conf *fc_conf)
{
	int ret;
	struct nfp_net_hw *net_hw;
	enum rte_eth_fc_mode set_mode;
	enum rte_eth_fc_mode original_mode;
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_eth_table_port *eth_port;

	net_hw = nfp_net_get_hw(dev);
	if (net_hw->pf_dev == NULL)
		return -EINVAL;

	nfp_eth_table = net_hw->pf_dev->nfp_eth_table;
	eth_port = &nfp_eth_table->ports[net_hw->idx];

	original_mode = nfp_net_get_pause_mode(eth_port);
	set_mode = fc_conf->mode;

	if (set_mode == original_mode)
		return 0;

	ret = nfp_net_pause_frame_set(net_hw, eth_port, set_mode);
	if (ret != 0)
		return ret;

	/* Update eth_table after modifying RX/TX pause frame mode. */
	eth_port->tx_pause_enabled = (set_mode & RTE_ETH_FC_TX_PAUSE) == 0 ? false : true;
	eth_port->rx_pause_enabled = (set_mode & RTE_ETH_FC_RX_PAUSE) == 0 ? false : true;

	return 0;
}
