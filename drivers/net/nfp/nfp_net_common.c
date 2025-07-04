/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2018 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

#include "nfp_net_common.h"

#include <rte_alarm.h>

#include "flower/nfp_flower_cmsg.h"
#include "flower/nfp_flower_representor.h"
#include "nfd3/nfp_nfd3.h"
#include "nfdk/nfp_nfdk.h"
#include "nfpcore/nfp_mip.h"
#include "nfpcore/nfp_nsp.h"
#include "nfpcore/nfp_rtsym.h"
#include "nfp_logs.h"
#include "nfp_net_meta.h"

#define NFP_TX_MAX_SEG       UINT8_MAX
#define NFP_TX_MAX_MTU_SEG   8

#define NFP_NET_LINK_DOWN_CHECK_TIMEOUT 4000 /* ms */
#define NFP_NET_LINK_UP_CHECK_TIMEOUT   1000 /* ms */

#define DEFAULT_FLBUF_SIZE        9216
#define NFP_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + RTE_VLAN_HLEN * 2)

/* Only show FEC capability supported by the current speed. */
#define NFP_FEC_CAPA_ENTRY_NUM  1

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

static bool
nfp_net_is_pf(struct rte_eth_dev *dev)
{
	if (rte_eth_dev_is_repr(dev))
		return nfp_flower_repr_is_pf(dev);

	return ((struct nfp_net_hw_priv *)dev->process_private)->is_pf;
}

static size_t
nfp_net_link_speed_rte2nfp(uint32_t speed)
{
	size_t i;

	for (i = 0; i < RTE_DIM(nfp_net_link_speed_nfp2rte); i++) {
		if (speed == nfp_net_link_speed_nfp2rte[i])
			return i;
	}

	return NFP_NET_CFG_STS_LINK_RATE_UNKNOWN;
}

static uint32_t
nfp_net_link_speed_nfp2rte_check(uint32_t speed)
{
	size_t i;

	for (i = 0; i < RTE_DIM(nfp_net_link_speed_nfp2rte); i++) {
		if (speed == nfp_net_link_speed_nfp2rte[i])
			return nfp_net_link_speed_nfp2rte[i];
	}

	return RTE_ETH_SPEED_NUM_NONE;
}

void
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

/**
 * Reconfigure the firmware of VF configure
 *
 * @param net_hw
 *   Device to reconfigure
 * @param pf_dev
 *   Get the Device info
 * @param update
 *   The value for the mailbox VF command
 * @param value
 *   The value of update
 * @param offset
 *   The offset in the VF configure table
 *
 * @return
 *   - (0) if OK to reconfigure vf configure.
 *   - (-EIO) if I/O err and fail to configure the vf configure
 */
static int
nfp_net_vf_reconfig(struct nfp_net_hw *net_hw,
		struct nfp_pf_dev *pf_dev,
		uint16_t update,
		uint8_t value,
		uint32_t offset)
{
	int ret;
	struct nfp_hw *hw;

	hw = &net_hw->super;
	rte_spinlock_lock(&hw->reconfig_lock);

	/* Write update info to mailbox in VF config symbol */
	nn_writeb(value, pf_dev->vf_cfg_tbl_bar + offset);
	nn_writew(update, pf_dev->vf_cfg_tbl_bar + NFP_NET_VF_CFG_MB_UPD);
	nn_cfg_writel(hw, NFP_NET_CFG_UPDATE, NFP_NET_CFG_UPDATE_VF);

	rte_wmb();

	ret = nfp_reconfig_real(hw, NFP_NET_CFG_UPDATE_VF);

	rte_spinlock_unlock(&hw->reconfig_lock);

	if (ret != 0)
		return -EIO;

	return nn_readw(pf_dev->vf_cfg_tbl_bar + NFP_NET_VF_CFG_MB_RET);
}

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
		PMD_DRV_LOG(ERR, "Error nft net mailbox reconfig: mbox=%#08x update=%#08x.",
				mbox_cmd, NFP_NET_CFG_UPDATE_MBOX);
		return -EIO;
	}

	return nn_cfg_readl(&net_hw->super, mbox + NFP_NET_CFG_MBOX_SIMPLE_RET);
}

struct nfp_net_hw *
nfp_net_get_hw(const struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	if (rte_eth_dev_is_repr(dev)) {
		struct nfp_flower_representor *repr;
		repr = dev->data->dev_private;
		hw = repr->app_fw_flower->pf_hw;
	} else {
		hw = dev->data->dev_private;
	}

	return hw;
}

uint8_t
nfp_net_get_idx(const struct rte_eth_dev *dev)
{
	uint8_t idx;

	if (rte_eth_dev_is_repr(dev)) {
		struct nfp_flower_representor *repr;
		repr = dev->data->dev_private;
		idx = repr->idx;
	} else {
		struct nfp_net_hw *hw;
		hw = dev->data->dev_private;
		idx = hw->idx;
	}

	return idx;
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
		PMD_DRV_LOG(ERR, "TX mq_mode DCB and VMDq not supported.");
		return -EINVAL;
	}

	/* Checking RX mode */
	if ((rxmode->mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) != 0 &&
			(hw->super.cap & NFP_NET_CFG_CTRL_RSS_ANY) == 0) {
		PMD_DRV_LOG(ERR, "RSS not supported.");
		return -EINVAL;
	}

	/* Checking MTU set */
	if (rxmode->mtu > hw->max_mtu + NFP_ETH_OVERHEAD) {
		PMD_DRV_LOG(ERR, "MTU (%u) larger than the maximum possible frame size (%u).",
				rxmode->mtu, hw->max_mtu + NFP_ETH_OVERHEAD);
		return -ERANGE;
	}

	return 0;
}

void
nfp_net_log_device_information(const struct nfp_net_hw *hw,
		struct nfp_pf_dev *pf_dev)
{
	uint32_t cap = hw->super.cap;
	uint32_t cap_ext = hw->super.cap_ext;
	uint32_t cap_rss = hw->super.cap_rss;

	PMD_INIT_LOG(INFO, "VER: %u.%u, Maximum supported MTU: %d.",
			pf_dev->ver.major, pf_dev->ver.minor, hw->max_mtu);

	PMD_INIT_LOG(INFO, "CAP: %#x.", cap);
	PMD_INIT_LOG(INFO, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
			cap & NFP_NET_CFG_CTRL_ENABLE        ? "ENABLE "      : "",
			cap & NFP_NET_CFG_CTRL_PROMISC       ? "PROMISC "     : "",
			cap & NFP_NET_CFG_CTRL_L2BC          ? "L2BCFILT "    : "",
			cap & NFP_NET_CFG_CTRL_L2MC          ? "L2MCFILT "    : "",
			cap & NFP_NET_CFG_CTRL_RXCSUM        ? "RXCSUM "      : "",
			cap & NFP_NET_CFG_CTRL_TXCSUM        ? "TXCSUM "      : "",
			cap & NFP_NET_CFG_CTRL_RXVLAN        ? "RXVLAN "      : "",
			cap & NFP_NET_CFG_CTRL_TXVLAN        ? "TXVLAN "      : "",
			cap & NFP_NET_CFG_CTRL_SCATTER       ? "SCATTER "     : "",
			cap & NFP_NET_CFG_CTRL_GATHER        ? "GATHER "      : "",
			cap & NFP_NET_CFG_CTRL_LSO           ? "TSO "         : "",
			cap & NFP_NET_CFG_CTRL_RXQINQ        ? "RXQINQ "      : "",
			cap & NFP_NET_CFG_CTRL_RXVLAN_V2     ? "RXVLANv2 "    : "",
			cap & NFP_NET_CFG_CTRL_RINGCFG       ? "RINGCFG "     : "",
			cap & NFP_NET_CFG_CTRL_RSS           ? "RSS "         : "",
			cap & NFP_NET_CFG_CTRL_IRQMOD        ? "IRQMOD "      : "",
			cap & NFP_NET_CFG_CTRL_RINGPRIO      ? "RINGPRIO "    : "",
			cap & NFP_NET_CFG_CTRL_MSIXAUTO      ? "MSIXAUTO "    : "",
			cap & NFP_NET_CFG_CTRL_TXRWB         ? "TXRWB "       : "",
			cap & NFP_NET_CFG_CTRL_L2SWITCH      ? "L2SWITCH "    : "",
			cap & NFP_NET_CFG_CTRL_TXVLAN_V2     ? "TXVLANv2 "    : "",
			cap & NFP_NET_CFG_CTRL_VXLAN         ? "VXLAN "       : "",
			cap & NFP_NET_CFG_CTRL_NVGRE         ? "NVGRE "       : "",
			cap & NFP_NET_CFG_CTRL_MSIX_TX_OFF   ? "MSIX_TX_OFF " : "",
			cap & NFP_NET_CFG_CTRL_LSO2          ? "TSOv2 "       : "",
			cap & NFP_NET_CFG_CTRL_RSS2          ? "RSSv2 "       : "",
			cap & NFP_NET_CFG_CTRL_CSUM_COMPLETE ? "CSUM "        : "",
			cap & NFP_NET_CFG_CTRL_LIVE_ADDR     ? "LIVE_ADDR "   : "",
			cap & NFP_NET_CFG_CTRL_USO           ? "USO"          : "");

	PMD_INIT_LOG(INFO, "CAP_WORD1: %#x.", cap_ext);
	PMD_INIT_LOG(INFO, "%s%s%s%s%s%s%s",
			cap_ext & NFP_NET_CFG_CTRL_PKT_TYPE        ? "PKT_TYPE "        : "",
			cap_ext & NFP_NET_CFG_CTRL_IPSEC           ? "IPSEC "           : "",
			cap_ext & NFP_NET_CFG_CTRL_IPSEC_SM_LOOKUP ? "IPSEC_SM "        : "",
			cap_ext & NFP_NET_CFG_CTRL_IPSEC_LM_LOOKUP ? "IPSEC_LM "        : "",
			cap_ext & NFP_NET_CFG_CTRL_MULTI_PF        ? "MULTI_PF "        : "",
			cap_ext & NFP_NET_CFG_CTRL_FLOW_STEER      ? "FLOW_STEER "      : "",
			cap_ext & NFP_NET_CFG_CTRL_IN_ORDER        ? "VIRTIO_IN_ORDER " : "");

	PMD_INIT_LOG(INFO, "CAP_RSS: %#x.", cap_rss);
	PMD_INIT_LOG(INFO, "%s%s%s",
			cap_rss & NFP_NET_CFG_RSS_TOEPLITZ        ? "RSS_TOEPLITZ "   : "",
			cap_rss & NFP_NET_CFG_RSS_XOR             ? "RSS_XOR "        : "",
			cap_rss & NFP_NET_CFG_RSS_CRC32           ? "RSS_CRC32 "      : "");

	PMD_INIT_LOG(INFO, "The max_rx_queues: %u, max_tx_queues: %u.",
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
		PMD_DRV_LOG(ERR, "MAC address unable to change when port enabled.");
		return -EBUSY;
	}

	if (rte_is_valid_assigned_ether_addr(mac_addr) == 0) {
		PMD_DRV_LOG(ERR, "Invalid MAC address.");
		return -EINVAL;
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
		PMD_DRV_LOG(ERR, "MAC address update failed.");
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
		PMD_DRV_LOG(ERR, "Failed to allocate %d rx_queues intr_vec.",
				dev->data->nb_rx_queues);
		return -ENOMEM;
	}

	hw = nfp_net_get_hw(dev);

	if (rte_intr_type_get(intr_handle) == RTE_INTR_HANDLE_UIO) {
		PMD_DRV_LOG(INFO, "VF: enabling RX interrupt with UIO.");
		/* UIO just supports one queue and no LSC */
		nn_cfg_writeb(&hw->super, NFP_NET_CFG_RXR_VEC(0), 0);
		if (rte_intr_vec_list_index_set(intr_handle, 0, 0) != 0)
			return -1;
	} else {
		PMD_DRV_LOG(INFO, "VF: enabling RX interrupt with VFIO.");
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
			(tx_offload & RTE_ETH_TX_OFFLOAD_UDP_TSO) != 0 ||
			(tx_offload & RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO) != 0) {
		if ((cap & NFP_NET_CFG_CTRL_LSO) != 0)
			ctrl |= NFP_NET_CFG_CTRL_LSO;
		else if ((cap & NFP_NET_CFG_CTRL_LSO2) != 0)
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
		PMD_DRV_LOG(ERR, "Promiscuous mode not supported.");
		return -ENOTSUP;
	}

	if ((hw->ctrl & NFP_NET_CFG_CTRL_PROMISC) != 0) {
		PMD_DRV_LOG(INFO, "Promiscuous mode already enabled.");
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

	if ((hw->cap & NFP_NET_CFG_CTRL_PROMISC) == 0) {
		PMD_DRV_LOG(ERR, "Promiscuous mode not supported.");
		return -ENOTSUP;
	}

	if ((hw->ctrl & NFP_NET_CFG_CTRL_PROMISC) == 0) {
		PMD_DRV_LOG(INFO, "Promiscuous mode already disabled.");
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
		PMD_DRV_LOG(DEBUG, "Allmulticast mode not supported.");
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

static void
nfp_net_pf_speed_update(struct rte_eth_dev *dev,
		struct nfp_net_hw_priv *hw_priv,
		struct rte_eth_link *link)
{
	uint8_t idx;
	enum nfp_eth_aneg aneg;
	struct nfp_pf_dev *pf_dev;
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_eth_table_port *eth_port;

	pf_dev = hw_priv->pf_dev;
	idx = nfp_net_get_idx(dev);
	aneg = pf_dev->nfp_eth_table->ports[idx].aneg;

	/* Compare whether the current status has changed. */
	if (pf_dev->speed_updated || aneg == NFP_ANEG_AUTO) {
		nfp_eth_table = nfp_eth_read_ports(pf_dev->cpp);
		if (nfp_eth_table == NULL) {
			PMD_DRV_LOG(DEBUG, "Failed to get nfp_eth_table.");
		} else {
			pf_dev->nfp_eth_table->ports[idx] = nfp_eth_table->ports[idx];
			free(nfp_eth_table);
			pf_dev->speed_updated = false;
		}
	}

	nfp_eth_table = pf_dev->nfp_eth_table;
	eth_port = &nfp_eth_table->ports[idx];

	link->link_speed = nfp_net_link_speed_nfp2rte_check(eth_port->speed);

	if (dev->data->dev_conf.link_speeds == RTE_ETH_LINK_SPEED_AUTONEG &&
			eth_port->supp_aneg)
		link->link_autoneg = RTE_ETH_LINK_AUTONEG;
}

static void
nfp_net_vf_speed_update(struct rte_eth_link *link,
		uint32_t link_status)
{
	size_t link_rate_index;

	/*
	 * Shift and mask link_status so that it is effectively the value
	 * at offset NFP_NET_CFG_STS_NSP_LINK_RATE.
	 */
	link_rate_index = (link_status >> NFP_NET_CFG_STS_LINK_RATE_SHIFT) &
			NFP_NET_CFG_STS_LINK_RATE_MASK;
	if (link_rate_index < RTE_DIM(nfp_net_link_speed_nfp2rte))
		link->link_speed = nfp_net_link_speed_nfp2rte[link_rate_index];
	else
		link->link_speed = RTE_ETH_SPEED_NUM_NONE;
}

int
nfp_net_link_update_common(struct rte_eth_dev *dev,
		struct rte_eth_link *link,
		uint32_t link_status)
{
	int ret;
	struct nfp_net_hw_priv *hw_priv;

	hw_priv = dev->process_private;
	if (link->link_status == RTE_ETH_LINK_UP) {
		if (nfp_net_is_pf(dev))
			nfp_net_pf_speed_update(dev, hw_priv, link);
		else
			nfp_net_vf_speed_update(link, link_status);
	}

	ret = rte_eth_linkstatus_set(dev, link);
	if (ret == 0) {
		if (link->link_status == RTE_ETH_LINK_UP)
			PMD_DRV_LOG(INFO, "NIC Link is Up.");
		else
			PMD_DRV_LOG(INFO, "NIC Link is Down.");
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
	struct nfp_net_hw_priv *hw_priv;

	hw = nfp_net_get_hw(dev);
	hw_priv = dev->process_private;

	memset(&link, 0, sizeof(struct rte_eth_link));

	/* Read link status */
	nn_link_status = nn_cfg_readw(&hw->super, NFP_NET_CFG_STS);
	if ((nn_link_status & NFP_NET_CFG_STS_LINK) != 0)
		link.link_status = RTE_ETH_LINK_UP;

	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

	ret = nfp_net_link_update_common(dev, &link, nn_link_status);
	if (ret == -EIO)
		return ret;

	/*
	 * Notify the port to update the speed value in the CTRL BAR from NSP.
	 * Not applicable for VFs as the associated PF is still attached to the
	 * kernel driver.
	 */
	if (hw_priv != NULL && hw_priv->is_pf)
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
	bool vf_flag = false;
	struct nfp_net_hw *hw;
	struct nfp_flower_representor *repr;
	const uint32_t size = RTE_DIM(nfp_net_xstats);

	if (rte_eth_dev_is_repr(dev)) {
		repr = dev->data->dev_private;
		if (nfp_flower_repr_is_vf(repr))
			vf_flag = true;
	} else {
		hw = dev->data->dev_private;
		if (hw->mac_stats == NULL)
			vf_flag = true;
	}

	/* If the device is a VF or VF-repr, then there will be no MAC stats */
	if (vf_flag) {
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
		PMD_DRV_LOG(ERR, "The xstat index out of bounds.");
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
	uint8_t *mac_stats;
	struct nfp_net_hw *hw;
	struct nfp_xstat xstat;
	struct rte_eth_xstat *xstats_base;
	struct nfp_flower_representor *repr;

	if (rte_eth_dev_is_repr(dev)) {
		repr = dev->data->dev_private;
		hw = repr->app_fw_flower->pf_hw;

		mac_stats = repr->mac_stats;
		xstats_base = repr->repr_xstats_base;
	} else {
		hw = dev->data->dev_private;

		mac_stats = hw->mac_stats;
		xstats_base = hw->eth_xstats_base;
	}

	xstat = nfp_net_xstats[index];

	if (xstat.group == NFP_XSTAT_GROUP_MAC)
		value = nn_readq(mac_stats + xstat.offset);
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
	return value - xstats_base[index].value;
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
	struct rte_eth_xstat *xstats_base;
	struct nfp_flower_representor *repr;

	read_size = nfp_net_xstats_size(dev);

	if (rte_eth_dev_is_repr(dev)) {
		repr = dev->data->dev_private;
		xstats_base = repr->repr_xstats_base;
	} else {
		hw = dev->data->dev_private;
		xstats_base = hw->eth_xstats_base;
	}

	for (id = 0; id < read_size; id++) {
		xstats_base[id].id = id;
		xstats_base[id].value = nfp_net_xstats_value(dev, id, true);
	}

	/* Successfully reset xstats, now call function to reset basic stats. */
	if (rte_eth_dev_is_repr(dev))
		return nfp_flower_repr_stats_reset(dev);
	else
		return nfp_net_stats_reset(dev);
}

void
nfp_net_rx_desc_limits(struct nfp_net_hw_priv *hw_priv,
		uint16_t *min_rx_desc,
		uint16_t *max_rx_desc)
{
	*max_rx_desc = hw_priv->dev_info->max_qc_size;
	*min_rx_desc = hw_priv->dev_info->min_qc_size;
}

void
nfp_net_tx_desc_limits(struct nfp_net_hw_priv *hw_priv,
		uint16_t *min_tx_desc,
		uint16_t *max_tx_desc)
{
	uint16_t tx_dpp;

	if (hw_priv->pf_dev->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3)
		tx_dpp = NFD3_TX_DESC_PER_PKT;
	else
		tx_dpp = NFDK_TX_DESC_PER_SIMPLE_PKT;

	*max_tx_desc = hw_priv->dev_info->max_qc_size / tx_dpp;
	*min_tx_desc = hw_priv->dev_info->min_qc_size / tx_dpp;
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
	struct nfp_net_hw_priv *hw_priv;

	hw = nfp_net_get_hw(dev);
	hw_priv = dev->process_private;
	if (hw_priv == NULL)
		return -EINVAL;

	nfp_net_rx_desc_limits(hw_priv, &min_rx_desc, &max_rx_desc);
	nfp_net_tx_desc_limits(hw_priv, &min_tx_desc, &max_tx_desc);

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
		if ((cap & NFP_NET_CFG_CTRL_USO) != 0)
			dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_UDP_TSO;
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
		dev_info->flow_type_rss_offloads = NFP_NET_RSS_CAP;
		dev_info->reta_size = NFP_NET_CFG_RSS_ITBL_SZ;
		dev_info->hash_key_size = NFP_NET_CFG_RSS_KEY_SZ;
		nfp_net_rss_algo_capa_get(hw, dev_info);
	}

	/* Only PF supports getting speed capability. */
	if (hw_priv->is_pf)
		dev_info->speed_capa = hw_priv->pf_dev->speed_capa;

	return 0;
}

int
nfp_net_common_init(struct nfp_pf_dev *pf_dev,
		struct nfp_net_hw *hw)
{
	const int stride = 4;
	struct rte_pci_device *pci_dev;

	pci_dev = pf_dev->pci_dev;
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	hw->max_rx_queues = nn_cfg_readl(&hw->super, NFP_NET_CFG_MAX_RXRINGS);
	hw->max_tx_queues = nn_cfg_readl(&hw->super, NFP_NET_CFG_MAX_TXRINGS);
	hw->super.cap_rss = nn_cfg_readl(&hw->super, NFP_NET_CFG_RSS_CAP);
	if (hw->max_rx_queues == 0 || hw->max_tx_queues == 0) {
		PMD_INIT_LOG(ERR, "Device %s can not be used, there are no valid queue "
				"pairs for use.", pci_dev->name);
		return -ENODEV;
	}

	if (nfp_net_check_dma_mask(pf_dev, pci_dev->name) != 0)
		return -ENODEV;

	/* Get some of the read-only fields from the config BAR */
	hw->super.cap = nn_cfg_readl(&hw->super, NFP_NET_CFG_CAP);
	hw->super.cap_ext = nn_cfg_readl(&hw->super, NFP_NET_CFG_CAP_WORD1);
	hw->max_mtu = nn_cfg_readl(&hw->super, NFP_NET_CFG_MAX_MTU);
	hw->flbufsz = DEFAULT_FLBUF_SIZE;

	nfp_net_meta_init_format(hw, pf_dev);

	/* Read the Rx offset configured from firmware */
	if (pf_dev->ver.major < 2)
		hw->rx_offset = NFP_NET_RX_OFFSET;
	else
		hw->rx_offset = nn_cfg_readl(&hw->super, NFP_NET_CFG_RX_OFFSET);

	hw->super.ctrl = 0;
	hw->stride_rx = stride;
	hw->stride_tx = stride;

	return 0;
}

const uint32_t *
nfp_net_supported_ptypes_get(struct rte_eth_dev *dev, size_t *no_of_elements)
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
	};

	if (dev->rx_pkt_burst == NULL)
		return NULL;

	net_hw = dev->data->dev_private;
	if ((net_hw->super.cap_ext & NFP_NET_CFG_CTRL_PKT_TYPE) == 0)
		return NULL;

	*no_of_elements = RTE_DIM(ptypes);
	return ptypes;
}

int
nfp_net_ptypes_set(struct rte_eth_dev *dev,
		uint32_t ptype_mask)
{
	int ret;
	uint32_t update;
	uint32_t ctrl_ext;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;

	net_hw = dev->data->dev_private;
	hw = &net_hw->super;

	if ((hw->cap_ext & NFP_NET_CFG_CTRL_PKT_TYPE) == 0)
		return -ENOTSUP;

	ctrl_ext = hw->ctrl_ext;
	if (ptype_mask == 0) {
		if ((ctrl_ext & NFP_NET_CFG_CTRL_PKT_TYPE) == 0)
			return 0;

		ctrl_ext &= ~NFP_NET_CFG_CTRL_PKT_TYPE;
	} else {
		if ((ctrl_ext & NFP_NET_CFG_CTRL_PKT_TYPE) != 0)
			return 0;

		ctrl_ext |= NFP_NET_CFG_CTRL_PKT_TYPE;
	}

	update = NFP_NET_CFG_UPDATE_GEN;

	ret = nfp_ext_reconfig(hw, ctrl_ext, update);
	if (ret != 0)
		return ret;

	hw->ctrl_ext = ctrl_ext;

	return 0;
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
		PMD_DRV_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s.",
				dev->data->port_id, link.link_speed,
				link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX ?
				"full-duplex" : "half-duplex");
	else
		PMD_DRV_LOG(INFO, " Port %d: Link Down.", dev->data->port_id);

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
		PMD_INIT_LOG(ERR, "Error setting alarm.");
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
		PMD_DRV_LOG(ERR, "Port %d must be stopped before configuration.",
				dev->data->port_id);
		return -EBUSY;
	}

	/* MTU larger than current mbufsize not supported */
	if (mtu > hw->flbufsz) {
		PMD_DRV_LOG(ERR, "MTU (%u) larger than current mbufsize (%u) not supported.",
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
				" does not match hardware can supported (%d).",
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
				" does not match hardware can supported (%d).",
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

static void
nfp_net_rss_hf_get(uint32_t cfg_rss_ctrl,
		struct rte_eth_rss_conf *rss_conf)
{
	uint64_t rss_hf = 0;

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

	rss_conf->rss_hf = rss_hf;
}

static void
nfp_net_rss_hf_set(uint32_t *cfg_rss_ctrl,
		struct rte_eth_rss_conf *rss_conf)
{
	uint64_t rss_hf = rss_conf->rss_hf;

	if ((rss_hf & RTE_ETH_RSS_IPV4) != 0)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP) != 0)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_TCP;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP) != 0)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_UDP;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_SCTP) != 0)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_SCTP;

	if ((rss_hf & RTE_ETH_RSS_IPV6) != 0)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP) != 0)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_TCP;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP) != 0)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_UDP;

	if ((rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_SCTP) != 0)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_SCTP;
}

static int
nfp_net_rss_algo_conf_get(uint32_t cfg_rss_ctrl,
		struct rte_eth_rss_conf *rss_conf)
{
	if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_TOEPLITZ) != 0)
		rss_conf->algorithm = RTE_ETH_HASH_FUNCTION_TOEPLITZ;
	else if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_XOR) != 0)
		rss_conf->algorithm = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
	else if ((cfg_rss_ctrl & NFP_NET_CFG_RSS_CRC32) != 0)
		rss_conf->algorithm = RTE_ETH_HASH_FUNCTION_DEFAULT;
	else
		return -EIO;

	return 0;
}

static int
nfp_net_rss_algo_conf_set(uint32_t *cfg_rss_ctrl,
		struct rte_eth_rss_conf *rss_conf)
{
	enum rte_eth_hash_function algorithm = rss_conf->algorithm;

	if (algorithm == RTE_ETH_HASH_FUNCTION_TOEPLITZ)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_TOEPLITZ;
	else if (algorithm == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_XOR;
	else if (algorithm == RTE_ETH_HASH_FUNCTION_DEFAULT)
		*cfg_rss_ctrl |= NFP_NET_CFG_RSS_CRC32;
	else
		return -ENOTSUP;

	return 0;
}

static int
nfp_net_rss_hash_write(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	int ret;
	uint8_t i;
	uint8_t key;
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

	nfp_net_rss_hf_set(&cfg_rss_ctrl, rss_conf);

	cfg_rss_ctrl |= NFP_NET_CFG_RSS_MASK;

	ret = nfp_net_rss_algo_conf_set(&cfg_rss_ctrl, rss_conf);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Fail to set rss algorithm configuration.");
		return ret;
	}

	/* Configuring where to apply the RSS hash */
	nn_cfg_writel(hw, NFP_NET_CFG_RSS_CTRL, cfg_rss_ctrl);

	return 0;
}

int
nfp_net_rss_hash_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	int ret;
	uint32_t update;
	struct nfp_hw *hw;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	/* Checking if RSS is enabled */
	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0) {
		PMD_DRV_LOG(ERR, "RSS unsupported.");
		return -EINVAL;
	}

	if (rss_conf->rss_key_len > NFP_NET_CFG_RSS_KEY_SZ) {
		PMD_DRV_LOG(ERR, "RSS hash key too long.");
		return -EINVAL;
	}

	ret = nfp_net_rss_hash_write(dev, rss_conf);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "RSS write failed.");
		return ret;
	}

	update = NFP_NET_CFG_UPDATE_RSS;

	if (nfp_reconfig(hw, hw->ctrl, update) != 0)
		return -EIO;

	return 0;
}

int
nfp_net_rss_hash_conf_get(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf)
{
	int ret;
	uint8_t i;
	uint8_t key;
	struct nfp_hw *hw;
	uint32_t cfg_rss_ctrl;
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(dev);
	hw = &net_hw->super;

	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0)
		return -EINVAL;

	cfg_rss_ctrl = nn_cfg_readl(hw, NFP_NET_CFG_RSS_CTRL);

	/* Propagate current RSS hash functions to caller */
	nfp_net_rss_hf_get(cfg_rss_ctrl, rss_conf);

	/* Reading the key size */
	rss_conf->rss_key_len = NFP_NET_CFG_RSS_KEY_SZ;

	/* Reading the key byte a byte */
	if (rss_conf->rss_key != NULL) {
		for (i = 0; i < rss_conf->rss_key_len; i++) {
			key = nn_cfg_readb(hw, NFP_NET_CFG_RSS_KEY + i);
			memcpy(&rss_conf->rss_key[i], &key, 1);
		}
	}

	ret = nfp_net_rss_algo_conf_get(cfg_rss_ctrl, rss_conf);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Fail to get rss algorithm configuration.");
		return ret;
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
	uint8_t default_key[] = {
		0x6d, 0x5a, 0x56, 0xda,
		0x25, 0x5b, 0x0e, 0xc2,
		0x41, 0x67, 0x25, 0x3d,
		0x43, 0xa3, 0x8f, 0xb0,
		0xd0, 0xca, 0x2b, 0xcb,
		0xae, 0x7b, 0x30, 0xb4,
		0x77, 0xcb, 0x2d, 0xa3,
		0x80, 0x30, 0xf2, 0x0c,
		0x6a, 0x42, 0xb7, 0x3b,
		0xbe, 0xac, 0x01, 0xfa,
	};
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
		PMD_DRV_LOG(ERR, "Wrong rss conf.");
		return -EINVAL;
	}

	rss_conf = dev_conf->rx_adv_conf.rss_conf;
	if (rss_conf.rss_key_len == 0) {
		rss_conf.rss_key = default_key;
		rss_conf.rss_key_len = NFP_NET_CFG_RSS_KEY_SZ;
	}

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
		uint16_t port,
		uint32_t ctrl)
{
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

	return nfp_reconfig(hw, ctrl, NFP_NET_CFG_UPDATE_VXLAN);
}

/*
 * The firmware with NFD3 can not handle DMA address requiring more
 * than 40 bits.
 */
int
nfp_net_check_dma_mask(struct nfp_pf_dev *pf_dev,
		char *name)
{
	if (pf_dev->ver.extend == NFP_NET_CFG_VERSION_DP_NFD3 &&
			rte_mem_check_dma_mask(40) != 0) {
		PMD_DRV_LOG(ERR, "Device %s can not be used: restricted dma mask to 40 bits!",
				name);
		return -ENODEV;
	}

	return 0;
}

int
nfp_net_txrwb_alloc(struct rte_eth_dev *eth_dev)
{
	struct nfp_net_hw *net_hw;
	char mz_name[RTE_MEMZONE_NAMESIZE];

	net_hw = nfp_net_get_hw(eth_dev);
	snprintf(mz_name, sizeof(mz_name), "%s_TXRWB", eth_dev->data->name);
	net_hw->txrwb_mz = rte_memzone_reserve_aligned(mz_name,
			net_hw->max_tx_queues * sizeof(uint64_t),
			rte_socket_id(),
			RTE_MEMZONE_IOVA_CONTIG, RTE_CACHE_LINE_SIZE);
	if (net_hw->txrwb_mz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to alloc %s for TX ring write back.",
				mz_name);
		return -ENOMEM;
	}

	return 0;
}

void
nfp_net_txrwb_free(struct rte_eth_dev *eth_dev)
{
	struct nfp_net_hw *net_hw;

	net_hw = nfp_net_get_hw(eth_dev);
	if (net_hw->txrwb_mz == NULL)
		return;

	rte_memzone_free(net_hw->txrwb_mz);
	net_hw->txrwb_mz = NULL;
}

static void
nfp_net_cfg_read_version(struct nfp_hw *hw,
		struct nfp_pf_dev *pf_dev)
{
	union {
		uint32_t whole;
		struct nfp_net_fw_ver split;
	} version;

	version.whole = nn_cfg_readl(hw, NFP_NET_CFG_VERSION);
	pf_dev->ver = version.split;
}

bool
nfp_net_version_check(struct nfp_hw *hw,
		struct nfp_pf_dev *pf_dev)
{
	nfp_net_cfg_read_version(hw, pf_dev);
	if (!nfp_net_is_valid_nfd_version(pf_dev->ver))
		return false;

	if (!nfp_net_is_valid_version_class(pf_dev->ver))
		return false;

	return true;
}

static void
nfp_net_get_nsp_info(struct nfp_net_hw_priv *hw_priv,
		char *nsp_version)
{
	struct nfp_nsp *nsp;

	nsp = nfp_nsp_open(hw_priv->pf_dev->cpp);
	if (nsp == NULL)
		return;

	snprintf(nsp_version, FW_VER_LEN, "%hu.%hu",
			nfp_nsp_get_abi_ver_major(nsp),
			nfp_nsp_get_abi_ver_minor(nsp));

	nfp_nsp_close(nsp);
}

void
nfp_net_get_fw_version(struct nfp_cpp *cpp,
		uint32_t *mip_version)
{
	struct nfp_mip *mip;

	mip = nfp_mip_open(cpp);
	if (mip == NULL) {
		*mip_version = 0;
		return;
	}

	*mip_version = nfp_mip_fw_version(mip);

	nfp_mip_close(mip);
}

static void
nfp_net_get_mip_name(struct nfp_net_hw_priv *hw_priv,
		char *mip_name)
{
	struct nfp_mip *mip;

	mip = nfp_mip_open(hw_priv->pf_dev->cpp);
	if (mip == NULL)
		return;

	strlcpy(mip_name, nfp_mip_name(mip), FW_VER_LEN);

	nfp_mip_close(mip);
}

static void
nfp_net_get_app_name(struct nfp_net_hw_priv *hw_priv,
		char *app_name)
{
	switch (hw_priv->pf_dev->app_fw_id) {
	case NFP_APP_FW_CORE_NIC:
		strlcpy(app_name, "nic", FW_VER_LEN);
		break;
	case NFP_APP_FW_FLOWER_NIC:
		strlcpy(app_name, "flower", FW_VER_LEN);
		break;
	default:
		strlcpy(app_name, "unknown", FW_VER_LEN);
		break;
	}
}

int
nfp_net_firmware_version_get(struct rte_eth_dev *dev,
		char *fw_version,
		size_t fw_size)
{
	struct nfp_net_hw *hw;
	struct nfp_pf_dev *pf_dev;
	struct nfp_net_hw_priv *hw_priv;
	char app_name[FW_VER_LEN] = {0};
	char mip_name[FW_VER_LEN] = {0};
	char nsp_version[FW_VER_LEN] = {0};
	char vnic_version[FW_VER_LEN] = {0};

	if (fw_size < FW_VER_LEN)
		return FW_VER_LEN;

	hw = nfp_net_get_hw(dev);
	hw_priv = dev->process_private;
	pf_dev = hw_priv->pf_dev;

	if (hw->fw_version[0] != 0) {
		snprintf(fw_version, FW_VER_LEN, "%s", hw->fw_version);
		return 0;
	}

	if (!rte_eth_dev_is_repr(dev)) {
		snprintf(vnic_version, FW_VER_LEN, "%d.%d.%d.%d",
			pf_dev->ver.extend, pf_dev->ver.class,
			pf_dev->ver.major, pf_dev->ver.minor);
	} else {
		snprintf(vnic_version, FW_VER_LEN, "*");
	}

	nfp_net_get_nsp_info(hw_priv, nsp_version);
	nfp_net_get_mip_name(hw_priv, mip_name);
	nfp_net_get_app_name(hw_priv, app_name);

	if (nsp_version[0] == 0 || mip_name[0] == 0) {
		snprintf(fw_version, FW_VER_LEN, "%s %s %s %s",
			vnic_version, nsp_version, mip_name, app_name);
		return 0;
	}

	snprintf(hw->fw_version, FW_VER_LEN, "%s %s %s %s",
			vnic_version, nsp_version, mip_name, app_name);

	snprintf(fw_version, FW_VER_LEN, "%s", hw->fw_version);

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
			PMD_INIT_LOG(ERR, "NFDK must use ABI 5 or newer, found: %d.",
					version.major);
			return false;
		}

		return true;
	}

	return false;
}

bool
nfp_net_is_valid_version_class(struct nfp_net_fw_ver version)
{
	switch (version.class) {
	case NFP_NET_CFG_VERSION_CLASS_GENERIC:
		return true;
	case NFP_NET_CFG_VERSION_CLASS_NO_EMEM:
		return true;
	default:
		return false;
	}
}

void
nfp_net_ctrl_bar_size_set(struct nfp_pf_dev *pf_dev)
{
	if (pf_dev->ver.class == NFP_NET_CFG_VERSION_CLASS_GENERIC)
		pf_dev->ctrl_bar_size = NFP_NET_CFG_BAR_SZ_32K;
	else
		pf_dev->ctrl_bar_size = NFP_NET_CFG_BAR_SZ_8K;
}

/* Disable rx and tx functions to allow for reconfiguring. */
int
nfp_net_stop(struct rte_eth_dev *dev)
{
	int ret;
	struct nfp_net_hw *hw;
	struct nfp_net_hw_priv *hw_priv;

	hw = nfp_net_get_hw(dev);
	hw_priv = dev->process_private;

	nfp_net_disable_queues(dev);

	/* Clear queues */
	nfp_net_stop_tx_queue(dev);
	nfp_net_stop_rx_queue(dev);

	ret = nfp_eth_set_configured(hw_priv->pf_dev->cpp, hw->nfp_idx, 0);
	if (ret < 0)
		return ret;

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
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_eth_table_port *eth_port;

	hw_priv = dev->process_private;
	if (hw_priv == NULL || hw_priv->pf_dev == NULL)
		return -EINVAL;

	nfp_eth_table = hw_priv->pf_dev->nfp_eth_table;
	eth_port = &nfp_eth_table->ports[dev->data->port_id];

	/* Currently only RX/TX switch are supported */
	fc_conf->mode = nfp_net_get_pause_mode(eth_port);

	return 0;
}

static int
nfp_net_pause_frame_set(struct nfp_net_hw_priv *hw_priv,
		struct nfp_eth_table_port *eth_port,
		enum rte_eth_fc_mode mode)
{
	int err;
	bool flag;
	struct nfp_nsp *nsp;

	nsp = nfp_eth_config_start(hw_priv->pf_dev->cpp, eth_port->index);
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
	if (err < 0) {
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
	uint8_t idx;
	enum rte_eth_fc_mode set_mode;
	struct nfp_net_hw_priv *hw_priv;
	enum rte_eth_fc_mode original_mode;
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_eth_table_port *eth_port;

	idx = nfp_net_get_idx(dev);
	hw_priv = dev->process_private;
	if (hw_priv == NULL || hw_priv->pf_dev == NULL)
		return -EINVAL;

	nfp_eth_table = hw_priv->pf_dev->nfp_eth_table;
	eth_port = &nfp_eth_table->ports[idx];

	original_mode = nfp_net_get_pause_mode(eth_port);
	set_mode = fc_conf->mode;

	if (set_mode == original_mode)
		return 0;

	ret = nfp_net_pause_frame_set(hw_priv, eth_port, set_mode);
	if (ret != 0)
		return ret;

	/* Update eth_table after modifying RX/TX pause frame mode. */
	eth_port->tx_pause_enabled = (set_mode & RTE_ETH_FC_TX_PAUSE) == 0 ? false : true;
	eth_port->rx_pause_enabled = (set_mode & RTE_ETH_FC_RX_PAUSE) == 0 ? false : true;

	return 0;
}

int
nfp_net_fec_get_capability(struct rte_eth_dev *dev,
		struct rte_eth_fec_capa *speed_fec_capa,
		__rte_unused unsigned int num)
{
	uint8_t idx;
	uint16_t speed;
	uint32_t supported_fec;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_eth_table_port *eth_port;

	idx = nfp_net_get_idx(dev);
	hw_priv = dev->process_private;
	if (hw_priv == NULL || hw_priv->pf_dev == NULL)
		return -EINVAL;

	nfp_eth_table = hw_priv->pf_dev->nfp_eth_table;
	eth_port = &nfp_eth_table->ports[idx];

	speed = eth_port->speed;
	supported_fec = nfp_eth_supported_fec_modes(eth_port);
	if (speed == 0 || supported_fec == 0) {
		PMD_DRV_LOG(ERR, "FEC modes supported or Speed is invalid.");
		return -EINVAL;
	}

	if (speed_fec_capa == NULL)
		return NFP_FEC_CAPA_ENTRY_NUM;

	speed_fec_capa->speed = speed;

	if ((supported_fec & NFP_FEC_AUTO) != 0)
		speed_fec_capa->capa |= RTE_ETH_FEC_MODE_CAPA_MASK(AUTO);
	if ((supported_fec & NFP_FEC_BASER) != 0)
		speed_fec_capa->capa |= RTE_ETH_FEC_MODE_CAPA_MASK(BASER);
	if ((supported_fec & NFP_FEC_REED_SOLOMON) != 0)
		speed_fec_capa->capa |= RTE_ETH_FEC_MODE_CAPA_MASK(RS);
	if ((supported_fec & NFP_FEC_DISABLED) != 0)
		speed_fec_capa->capa |= RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC);

	return NFP_FEC_CAPA_ENTRY_NUM;
}

static uint32_t
nfp_net_fec_nfp_to_rte(enum nfp_eth_fec fec)
{
	switch (fec) {
	case NFP_FEC_AUTO_BIT:
		return RTE_ETH_FEC_MODE_CAPA_MASK(AUTO);
	case NFP_FEC_BASER_BIT:
		return RTE_ETH_FEC_MODE_CAPA_MASK(BASER);
	case NFP_FEC_REED_SOLOMON_BIT:
		return RTE_ETH_FEC_MODE_CAPA_MASK(RS);
	case NFP_FEC_DISABLED_BIT:
		return RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC);
	default:
		PMD_DRV_LOG(ERR, "FEC mode is invalid.");
		return 0;
	}
}

int
nfp_net_fec_get(struct rte_eth_dev *dev,
		uint32_t *fec_capa)
{
	uint8_t idx;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_eth_table_port *eth_port;

	idx = nfp_net_get_idx(dev);
	hw_priv = dev->process_private;
	if (hw_priv == NULL || hw_priv->pf_dev == NULL)
		return -EINVAL;

	if (dev->data->dev_link.link_status == RTE_ETH_LINK_DOWN) {
		nfp_eth_table = nfp_eth_read_ports(hw_priv->pf_dev->cpp);
		hw_priv->pf_dev->nfp_eth_table->ports[idx] = nfp_eth_table->ports[idx];
		free(nfp_eth_table);
	}

	nfp_eth_table = hw_priv->pf_dev->nfp_eth_table;
	eth_port = &nfp_eth_table->ports[idx];

	if (!nfp_eth_can_support_fec(eth_port)) {
		PMD_DRV_LOG(ERR, "NFP can not support FEC.");
		return -ENOTSUP;
	}

	/*
	 * If link is down and AUTO is enabled, AUTO is returned, otherwise,
	 * configured FEC mode is returned.
	 * If link is up, current FEC mode is returned.
	 */
	if (dev->data->dev_link.link_status == RTE_ETH_LINK_DOWN)
		*fec_capa = nfp_net_fec_nfp_to_rte(eth_port->fec);
	else
		*fec_capa = nfp_net_fec_nfp_to_rte(eth_port->act_fec);

	if (*fec_capa == 0)
		return -EINVAL;

	return 0;
}

static enum nfp_eth_fec
nfp_net_fec_rte_to_nfp(uint32_t fec)
{
	switch (fec) {
	case RTE_BIT32(RTE_ETH_FEC_AUTO):
		return NFP_FEC_AUTO_BIT;
	case RTE_BIT32(RTE_ETH_FEC_NOFEC):
		return NFP_FEC_DISABLED_BIT;
	case RTE_BIT32(RTE_ETH_FEC_RS):
		return NFP_FEC_REED_SOLOMON_BIT;
	case RTE_BIT32(RTE_ETH_FEC_BASER):
		return NFP_FEC_BASER_BIT;
	default:
		return NFP_FEC_INVALID_BIT;
	}
}

int
nfp_net_fec_set(struct rte_eth_dev *dev,
		uint32_t fec_capa)
{
	int ret;
	uint8_t idx;
	enum nfp_eth_fec fec;
	uint32_t supported_fec;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_eth_table *nfp_eth_table;
	struct nfp_eth_table_port *eth_port;

	idx = nfp_net_get_idx(dev);
	hw_priv = dev->process_private;
	if (hw_priv == NULL || hw_priv->pf_dev == NULL)
		return -EINVAL;

	nfp_eth_table = hw_priv->pf_dev->nfp_eth_table;
	eth_port = &nfp_eth_table->ports[idx];

	supported_fec = nfp_eth_supported_fec_modes(eth_port);
	if (supported_fec == 0) {
		PMD_DRV_LOG(ERR, "NFP can not support FEC.");
		return -ENOTSUP;
	}

	fec = nfp_net_fec_rte_to_nfp(fec_capa);
	if (fec == NFP_FEC_INVALID_BIT) {
		PMD_DRV_LOG(ERR, "FEC modes is invalid.");
		return -EINVAL;
	}

	if ((RTE_BIT32(fec) & supported_fec) == 0) {
		PMD_DRV_LOG(ERR, "Unsupported FEC mode is set.");
		return -EIO;
	}

	ret = nfp_eth_set_fec(hw_priv->pf_dev->cpp, eth_port->index, fec);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "NFP set FEC mode failed.");
		return ret;
	}

	return 0;
}

uint32_t
nfp_net_get_phyports_from_nsp(struct nfp_pf_dev *pf_dev)
{
	if (pf_dev->multi_pf.enabled)
		return 1;
	else
		return pf_dev->nfp_eth_table->count;
}

uint32_t
nfp_net_get_phyports_from_fw(struct nfp_pf_dev *pf_dev)
{
	int ret = 0;
	uint8_t total_phyports;
	char pf_name[RTE_ETH_NAME_MAX_LEN];

	/* Read the number of vNIC's created for the PF */
	snprintf(pf_name, sizeof(pf_name), "nfd_cfg_pf%u_num_ports",
			pf_dev->multi_pf.function_id);
	total_phyports = nfp_rtsym_read_le(pf_dev->sym_tbl, pf_name, &ret);
	if (ret != 0 || total_phyports == 0 || total_phyports > 8) {
		PMD_INIT_LOG(ERR, "%s symbol with wrong value", pf_name);
		return 0;
	}

	return total_phyports;
}

uint8_t
nfp_function_id_get(const struct nfp_pf_dev *pf_dev,
		uint8_t port_id)
{
	if (pf_dev->multi_pf.enabled)
		return pf_dev->multi_pf.function_id;

	return port_id;
}

static int
nfp_net_sriov_check(struct nfp_pf_dev *pf_dev,
		uint16_t cap)
{
	uint16_t cap_vf;

	cap_vf = nn_readw(pf_dev->vf_cfg_tbl_bar + NFP_NET_VF_CFG_MB_CAP);
	if ((cap_vf & cap) != cap)
		return -ENOTSUP;

	return 0;
}

static int
nfp_net_sriov_update(struct nfp_net_hw *net_hw,
		struct nfp_pf_dev *pf_dev,
		uint16_t update)
{
	int ret;

	/* Reuse NFP_NET_VF_CFG_MB_VF_NUM to pass vf_base_id to FW. */
	ret = nfp_net_vf_reconfig(net_hw, pf_dev, update, pf_dev->vf_base_id,
			NFP_NET_VF_CFG_MB_VF_NUM);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Error nfp VF reconfig.");
		return ret;
	}

	return 0;
}

static int
nfp_net_vf_queues_config(struct nfp_net_hw *net_hw,
		struct nfp_pf_dev *pf_dev)
{
	int ret;
	uint32_t i;
	uint32_t offset;

	ret = nfp_net_sriov_check(pf_dev, NFP_NET_VF_CFG_MB_CAP_QUEUE_CONFIG);
	if (ret != 0) {
		if (ret == -ENOTSUP) {
			PMD_INIT_LOG(DEBUG, "Set VF max queue not supported.");
			return 0;
		}

		PMD_INIT_LOG(ERR, "Set VF max queue failed.");
		return ret;
	}

	offset = NFP_NET_VF_CFG_MB_SZ + pf_dev->max_vfs * NFP_NET_VF_CFG_SZ;
	for (i = 0; i < pf_dev->sriov_vf; i++) {
		ret = nfp_net_vf_reconfig(net_hw, pf_dev, NFP_NET_VF_CFG_MB_UPD_QUEUE_CONFIG,
				pf_dev->queue_per_vf, pf_dev->vf_base_id + offset + i);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Set VF max_queue failed.");
			return ret;
		}
	}

	return 0;
}

static int
nfp_net_sriov_init(struct nfp_net_hw *net_hw,
		struct nfp_pf_dev *pf_dev)
{
	int ret;

	ret = nfp_net_sriov_check(pf_dev, NFP_NET_VF_CFG_MB_CAP_SPLIT);
	if (ret != 0) {
		if (ret == -ENOTSUP) {
			PMD_INIT_LOG(DEBUG, "Set VF split not supported.");
			return 0;
		}

		PMD_INIT_LOG(ERR, "Set VF split failed.");
		return ret;
	}

	nn_writeb(pf_dev->sriov_vf, pf_dev->vf_cfg_tbl_bar + NFP_NET_VF_CFG_MB_VF_CNT);

	ret = nfp_net_sriov_update(net_hw, pf_dev, NFP_NET_VF_CFG_MB_UPD_SPLIT);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "The nfp sriov update spilt failed.");
		return ret;
	}

	return 0;
}

int
nfp_net_vf_config_app_init(struct nfp_net_hw *net_hw,
		struct nfp_pf_dev *pf_dev)
{
	int ret;

	if (pf_dev->sriov_vf == 0)
		return 0;

	ret = nfp_net_sriov_init(net_hw, pf_dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init sriov module.");
		return ret;
	}

	ret = nfp_net_vf_queues_config(net_hw, pf_dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to config vf queue.");
		return ret;
	}

	return 0;
}

static inline bool
nfp_net_meta_has_no_port_type(__rte_unused struct nfp_net_meta_parsed *meta)
{
	return true;
}

static inline bool
nfp_net_meta_is_not_pf_port(__rte_unused struct nfp_net_meta_parsed *meta)
{
	return false;
}

static inline bool
nfp_net_meta_is_pf_port(struct nfp_net_meta_parsed *meta)
{
	return nfp_flower_port_is_phy_port(meta->port_id);
}

bool
nfp_net_recv_pkt_meta_check_register(struct nfp_net_hw_priv *hw_priv)
{
	struct nfp_pf_dev *pf_dev;

	pf_dev = hw_priv->pf_dev;
	if (!hw_priv->is_pf) {
		pf_dev->recv_pkt_meta_check_t = nfp_net_meta_has_no_port_type;
		return true;
	}

	switch (pf_dev->app_fw_id) {
	case NFP_APP_FW_CORE_NIC:
		pf_dev->recv_pkt_meta_check_t = nfp_net_meta_has_no_port_type;
		break;
	case NFP_APP_FW_FLOWER_NIC:
		if (pf_dev->multi_pf.enabled)
			pf_dev->recv_pkt_meta_check_t = nfp_net_meta_is_pf_port;
		else
			pf_dev->recv_pkt_meta_check_t = nfp_net_meta_is_not_pf_port;
		break;
	default:
		PMD_INIT_LOG(ERR, "Unsupported Firmware loaded.");
		return false;
	}

	return true;
}

static int
nfp_net_get_nfp_index(struct rte_eth_dev *dev)
{
	int nfp_idx;

	if (rte_eth_dev_is_repr(dev)) {
		struct nfp_flower_representor *repr;
		repr = dev->data->dev_private;
		nfp_idx = repr->nfp_idx;
	} else {
		struct nfp_net_hw *net_hw;
		net_hw = dev->data->dev_private;
		nfp_idx = net_hw->nfp_idx;
	}

	return nfp_idx;
}

int
nfp_net_get_eeprom_len(__rte_unused struct rte_eth_dev *dev)
{
	return RTE_ETHER_ADDR_LEN;
}

static int
nfp_net_get_port_mac_hwinfo(struct nfp_net_hw_priv *hw_priv,
		uint32_t index,
		struct rte_ether_addr *mac_addr)
{
	int ret;
	char hwinfo[32];
	struct nfp_nsp *nsp;

	snprintf(hwinfo, sizeof(hwinfo), "eth%u.mac", index);

	nsp = nfp_nsp_open(hw_priv->pf_dev->cpp);
	if (nsp == NULL)
		return -EOPNOTSUPP;

	ret = nfp_nsp_hwinfo_lookup(nsp, hwinfo, sizeof(hwinfo));
	nfp_nsp_close(nsp);

	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Read persistent MAC address failed for eth_index %u.", index);
		return ret;
	}

	ret = rte_ether_unformat_addr(hwinfo, mac_addr);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Can not parse persistent MAC address.");
		return -EOPNOTSUPP;
	}

	return 0;
}

static int
nfp_net_set_port_mac_hwinfo(struct nfp_net_hw_priv *hw_priv,
		uint32_t index,
		struct rte_ether_addr *mac_addr)
{
	int ret;
	char hwinfo_mac[32];
	struct nfp_nsp *nsp;
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, mac_addr);
	snprintf(hwinfo_mac, sizeof(hwinfo_mac), "eth%u.mac=%s", index, buf);

	nsp = nfp_nsp_open(hw_priv->pf_dev->cpp);
	if (nsp == NULL)
		return -EOPNOTSUPP;

	ret = nfp_nsp_hwinfo_set(nsp, hwinfo_mac, sizeof(hwinfo_mac));
	nfp_nsp_close(nsp);

	if (ret != 0) {
		PMD_DRV_LOG(ERR, "HWinfo set failed: %d.", ret);
		return ret;
	}

	return 0;
}

int
nfp_net_get_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *eeprom)
{
	int ret;
	uint32_t nfp_idx;
	struct nfp_net_hw *net_hw;
	struct rte_ether_addr mac_addr;
	struct nfp_net_hw_priv *hw_priv;

	if (eeprom->length == 0)
		return -EINVAL;

	hw_priv = dev->process_private;
	nfp_idx = nfp_net_get_nfp_index(dev);

	ret = nfp_net_get_port_mac_hwinfo(hw_priv, nfp_idx, &mac_addr);
	if (ret != 0)
		return -EOPNOTSUPP;

	net_hw = nfp_net_get_hw(dev);
	eeprom->magic = net_hw->vendor_id | (net_hw->device_id << 16);
	memcpy(eeprom->data, mac_addr.addr_bytes + eeprom->offset, eeprom->length);

	return 0;
}

int
nfp_net_set_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *eeprom)
{
	int ret;
	uint32_t nfp_idx;
	struct nfp_net_hw *net_hw;
	struct rte_ether_addr mac_addr;
	struct nfp_net_hw_priv *hw_priv;

	if (eeprom->length == 0)
		return -EINVAL;

	net_hw = nfp_net_get_hw(dev);
	if (eeprom->magic != (uint32_t)(net_hw->vendor_id | (net_hw->device_id << 16)))
		return -EINVAL;

	hw_priv = dev->process_private;
	nfp_idx = nfp_net_get_nfp_index(dev);
	ret = nfp_net_get_port_mac_hwinfo(hw_priv, nfp_idx, &mac_addr);
	if (ret != 0)
		return -EOPNOTSUPP;

	memcpy(mac_addr.addr_bytes + eeprom->offset, eeprom->data, eeprom->length);
	ret = nfp_net_set_port_mac_hwinfo(hw_priv, nfp_idx, &mac_addr);
	if (ret != 0)
		return -EOPNOTSUPP;

	return 0;
}

int
nfp_net_get_module_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_module_info *info)
{
	int ret = 0;
	uint8_t data;
	uint32_t idx;
	uint32_t read_len;
	struct nfp_nsp *nsp;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_eth_table_port *eth_port;

	hw_priv = dev->process_private;
	nsp = nfp_nsp_open(hw_priv->pf_dev->cpp);
	if (nsp == NULL) {
		PMD_DRV_LOG(ERR, "Unable to open NSP.");
		return -EIO;
	}

	if (!nfp_nsp_has_read_module_eeprom(nsp)) {
		PMD_DRV_LOG(ERR, "Read module eeprom not supported. Please update flash.");
		ret = -EOPNOTSUPP;
		goto exit_close_nsp;
	}

	idx = nfp_net_get_idx(dev);
	eth_port = &hw_priv->pf_dev->nfp_eth_table->ports[idx];
	switch (eth_port->interface) {
	case NFP_INTERFACE_SFP:
		/* FALLTHROUGH */
	case NFP_INTERFACE_SFP28:
		/* Read which revision the transceiver compiles with */
		ret = nfp_nsp_read_module_eeprom(nsp, eth_port->eth_index,
				SFP_SFF8472_COMPLIANCE, &data, 1, &read_len);
		if (ret != 0)
			goto exit_close_nsp;

		if (data == 0) {
			info->type = RTE_ETH_MODULE_SFF_8079;
			info->eeprom_len = RTE_ETH_MODULE_SFF_8079_LEN;
		} else {
			info->type = RTE_ETH_MODULE_SFF_8472;
			info->eeprom_len = RTE_ETH_MODULE_SFF_8472_LEN;
		}
		break;
	case NFP_INTERFACE_QSFP:
		/* Read which revision the transceiver compiles with */
		ret = nfp_nsp_read_module_eeprom(nsp, eth_port->eth_index,
				SFP_SFF_REV_COMPLIANCE, &data, 1, &read_len);
		if (ret != 0)
			goto exit_close_nsp;

		if (data == 0) {
			info->type = RTE_ETH_MODULE_SFF_8436;
			info->eeprom_len = RTE_ETH_MODULE_SFF_8436_MAX_LEN;
		} else {
			info->type = RTE_ETH_MODULE_SFF_8636;
			info->eeprom_len = RTE_ETH_MODULE_SFF_8636_MAX_LEN;
		}
		break;
	case NFP_INTERFACE_QSFP28:
		info->type = RTE_ETH_MODULE_SFF_8636;
		info->eeprom_len = RTE_ETH_MODULE_SFF_8636_MAX_LEN;
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported module %#x detected.",
				eth_port->interface);
		ret = -EINVAL;
	}

exit_close_nsp:
	nfp_nsp_close(nsp);
	return ret;
}

int
nfp_net_get_module_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *info)
{
	int ret = 0;
	uint32_t idx;
	struct nfp_nsp *nsp;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_eth_table_port *eth_port;

	hw_priv = dev->process_private;
	nsp = nfp_nsp_open(hw_priv->pf_dev->cpp);
	if (nsp == NULL) {
		PMD_DRV_LOG(ERR, "Unable to open NSP.");
		return -EIO;
	}

	if (!nfp_nsp_has_read_module_eeprom(nsp)) {
		PMD_DRV_LOG(ERR, "Read module eeprom not supported. Please update flash.");
		ret = -EOPNOTSUPP;
		goto exit_close_nsp;
	}

	idx = nfp_net_get_idx(dev);
	eth_port = &hw_priv->pf_dev->nfp_eth_table->ports[idx];
	ret = nfp_nsp_read_module_eeprom(nsp, eth_port->eth_index, info->offset,
			info->data, info->length, &info->length);
	if (ret != 0) {
		if (info->length)
			PMD_DRV_LOG(ERR, "Incomplete read from module EEPROM: %d.", ret);
		else
			PMD_DRV_LOG(ERR, "Read from module EEPROM failed: %d.", ret);
	}

exit_close_nsp:
	nfp_nsp_close(nsp);
	return ret;
}

static int
nfp_net_led_control(struct rte_eth_dev *dev,
		bool is_on)
{
	int ret;
	uint32_t nfp_idx;
	struct nfp_net_hw_priv *hw_priv;

	hw_priv = dev->process_private;
	nfp_idx = nfp_net_get_nfp_index(dev);

	ret = nfp_eth_set_idmode(hw_priv->pf_dev->cpp, nfp_idx, is_on);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Set nfp idmode failed.");
		return ret;
	}

	return 0;
}

int
nfp_net_led_on(struct rte_eth_dev *dev)
{
	return nfp_net_led_control(dev, true);
}

int
nfp_net_led_off(struct rte_eth_dev *dev)
{
	return nfp_net_led_control(dev, false);
}

void
nfp_net_rss_algo_capa_get(struct nfp_net_hw *hw,
		struct rte_eth_dev_info *dev_info)
{
	uint32_t cap_rss;

	cap_rss = hw->super.cap_rss;
	if ((cap_rss & NFP_NET_CFG_RSS_TOEPLITZ) != 0)
		dev_info->rss_algo_capa |= RTE_ETH_HASH_ALGO_CAPA_MASK(TOEPLITZ);

	if ((cap_rss & NFP_NET_CFG_RSS_XOR) != 0)
		dev_info->rss_algo_capa |= RTE_ETH_HASH_ALGO_CAPA_MASK(SIMPLE_XOR);

	if ((cap_rss & NFP_NET_CFG_RSS_CRC32) != 0)
		dev_info->rss_algo_capa |= RTE_ETH_HASH_ALGO_CAPA_MASK(DEFAULT);
}
