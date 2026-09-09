/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024-2026 NXP
 */

#include <stdbool.h>
#include <rte_bus_pci.h>
#include <rte_kvargs.h>
#include <rte_random.h>
#include <dpaax_iova_table.h>
#include "enetc_logs.h"
#include "enetc.h"

#define ENETC4_VSI_DISABLE		"enetc4_vsi_disable"
#define ENETC4_VSI_TIMEOUT		"enetc4_vsi_timeout"
#define ENETC4_VSI_DELAY		"enetc4_vsi_delay"
#define ENETC4_NC_MEMORY		"nc"

static void
enetc4_vf_get_devarg_nc(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_devargs *devargs = dev->device->devargs;
	struct rte_kvargs *kvlist;
	const char *val;

	if (!devargs)
		return;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (!kvlist)
		return;

	val = rte_kvargs_get(kvlist, ENETC4_NC_MEMORY);
	if (val && atoi(val) == 1)
		hw->nc_mode = 1;

	rte_kvargs_free(kvlist);
}

#define ENETC_CRC_TABLE_SIZE		256
#define ENETC_POLY			0x1021
#define ENETC_CRC_INIT			0xffff
#define ENETC_BYTE_SIZE			8
#define ENETC_MSB_BIT			0x8000

/* Backward-compat devarg for a PF running a kernel before 6.18.37, which uses
 * the legacy PF-to-VF link message layout (4-bit speed code + 4-bit cookie).
 * Usage: -a <pci_addr>,vf_link_legacy=1
 */
#define ENETC_VF_LINK_LEGACY		"vf_link_legacy"

uint16_t enetc_crc_table[ENETC_CRC_TABLE_SIZE];
bool enetc_crc_gen;

/* Supported Rx offloads */
static uint64_t dev_rx_offloads_sup =
	RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
	RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
	RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
	RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
	RTE_ETH_RX_OFFLOAD_KEEP_CRC |
	RTE_ETH_RX_OFFLOAD_TCP_LRO |
	RTE_ETH_RX_OFFLOAD_SCATTER;

/* Supported Tx offloads */
static uint64_t dev_tx_offloads_sup =
	RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
	RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
	RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
	RTE_ETH_TX_OFFLOAD_TCP_TSO |
	RTE_ETH_TX_OFFLOAD_UDP_TSO |
	RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

static void
enetc_gen_crc_table(void)
{
	uint16_t crc = 0;
	uint16_t c;

	for (int i = 0; i < ENETC_CRC_TABLE_SIZE; i++) {
		crc = 0;
		c = i << ENETC_BYTE_SIZE;
		for (int j = 0; j < ENETC_BYTE_SIZE; j++) {
			if ((crc ^ c) & ENETC_MSB_BIT)
				crc = (crc << 1) ^ ENETC_POLY;
			else
				crc = crc << 1;
			c = c << 1;
		}

		enetc_crc_table[i] = crc;
	}

	enetc_crc_gen = true;
}

static uint16_t
enetc_crc_calc(uint16_t crc, const uint8_t *buffer, size_t len)
{
	uint8_t data;

	while (len--) {
		data = *buffer;
		crc = (crc << 8) ^ enetc_crc_table[((crc >> 8) ^ data) & 0xff];
		buffer++;
	}
	return crc;
}

static int
parse_vf_link_legacy(const char *key __rte_unused, const char *value,
		     void *opaque)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)opaque;
	struct enetc_eth_hw *hw =
			ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	char *endptr;
	unsigned long val;

	if (!value || *value == '\0') {
		ENETC_PMD_WARN("Empty value for devarg %s, ignoring",
			       ENETC_VF_LINK_LEGACY);
		return -EINVAL;
	}

	errno = 0;
	val = strtoul(value, &endptr, 0);
	if (errno != 0 || *endptr != '\0' || val > 1) {
		ENETC_PMD_WARN("Invalid value '%s' for devarg %s, expected 0 or 1",
			       value, ENETC_VF_LINK_LEGACY);
		return -EINVAL;
	}

	hw->vf_link_legacy = (uint8_t)val;

	return 0;
}

static void
enetc4_vf_get_devargs(struct rte_eth_dev *dev)
{
	struct rte_devargs *devargs;
	struct rte_kvargs *kvlist;

	devargs = dev->device->devargs;
	if (!devargs)
		return;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (!kvlist)
		return;

	if (rte_kvargs_count(kvlist, ENETC_VF_LINK_LEGACY)) {
		if (rte_kvargs_process(kvlist, ENETC_VF_LINK_LEGACY,
				       parse_vf_link_legacy, (void *)dev) < 0)
			ENETC_PMD_WARN("Failed to parse devarg %s",
				       ENETC_VF_LINK_LEGACY);
	}

	rte_kvargs_free(kvlist);
}

static int
enetc4_vf_dev_infos_get(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *dev_info)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = MAX_BD_COUNT,
		.nb_min = MIN_BD_COUNT,
		.nb_align = BD_ALIGN,
		.nb_seg_max = ENETC4_MAX_SEGS,
		.nb_mtu_seg_max = ENETC4_MAX_SEGS,
	};
	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = MAX_BD_COUNT,
		.nb_min = MIN_BD_COUNT,
		.nb_align = BD_ALIGN,
		.nb_seg_max = ENETC4_MAX_SEGS,
		.nb_mtu_seg_max = ENETC4_MAX_SEGS,
	};
	dev_info->max_rx_queues = hw->max_rx_queues;
	dev_info->max_tx_queues = hw->max_tx_queues;
	dev_info->max_rx_pktlen = ENETC4_MAC_MAXFRM_SIZE;
	dev_info->max_mtu = dev_info->max_rx_pktlen - (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN);
	dev_info->max_mac_addrs = ENETC4_MAC_ENTRIES;
	dev_info->rx_offload_capa = dev_rx_offloads_sup;
	dev_info->tx_offload_capa = dev_tx_offloads_sup;
	dev_info->flow_type_rss_offloads = ENETC_RSS_OFFLOAD_ALL;

	return 0;
}


int
enetc4_vf_dev_stop(struct rte_eth_dev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int
enetc4_vf_dev_start(struct rte_eth_dev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int
enetc4_vf_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats,
		    struct eth_queue_stats *qstats __rte_unused)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_bdr *rx_ring;
	uint8_t i;

	PMD_INIT_FUNC_TRACE();

	/*
	 * The SI-level counters are read-only for a VF; they cannot be
	 * zeroed directly. Instead, stats_reset captures a baseline
	 * snapshot, and stats_get reports the delta so that the reported
	 * values appear to start from zero after each reset call.
	 */
	stats->ipackets = enetc4_rd64(enetc_hw, ENETC4_SIRFRM0) -
			  hw->vf_stats_saved.ipackets;
	stats->opackets = enetc4_rd64(enetc_hw, ENETC4_SITFRM0) -
			  hw->vf_stats_saved.opackets;
	stats->ibytes   = enetc4_rd64(enetc_hw, ENETC4_SIROCT0) -
			  hw->vf_stats_saved.ibytes;
	stats->obytes   = enetc4_rd64(enetc_hw, ENETC4_SITOCT0) -
			  hw->vf_stats_saved.obytes;
	stats->oerrors  = (uint32_t)enetc4_rd(enetc_hw, ENETC4_SITDFCR) -
			  (uint32_t)hw->vf_stats_saved.oerrors;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rx_ring = dev->data->rx_queues[i];
		stats->ierrors += rx_ring->ierrors;
	}

	return 0;
}

/*
 * Reset VF statistics by capturing a new baseline snapshot of the
 * SI-level hardware counters. Because those counters are read-only
 * for a VF (hardware erratum prevents reliable clear via FLR/soft
 * reset too), the driver uses a software delta approach: every
 * stats_get call reports current_hw_value - saved_baseline.
 */
static int
enetc4_vf_stats_reset(struct rte_eth_dev *dev)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_bdr *rx_ring;
	uint8_t i;

	PMD_INIT_FUNC_TRACE();

	/* Snapshot current HW SI counter values as the new zero baseline. */
	hw->vf_stats_saved.ipackets = enetc4_rd64(enetc_hw, ENETC4_SIRFRM0);
	hw->vf_stats_saved.opackets = enetc4_rd64(enetc_hw, ENETC4_SITFRM0);
	hw->vf_stats_saved.ibytes   = enetc4_rd64(enetc_hw, ENETC4_SIROCT0);
	hw->vf_stats_saved.obytes   = enetc4_rd64(enetc_hw, ENETC4_SITOCT0);
	hw->vf_stats_saved.oerrors  = enetc4_rd(enetc_hw, ENETC4_SITDFCR);

	/* Reset the per-ring software Rx error accumulators. */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rx_ring = dev->data->rx_queues[i];
		if (rx_ring)
			rx_ring->ierrors = 0;
	}

	return 0;
}


static void
enetc_msg_vf_fill_common_hdr(struct enetc_msg_swbd *msg,
					uint8_t class_id, uint8_t cmd_id, uint8_t proto_ver,
					uint8_t len, uint8_t cookie)
{
	struct enetc_msg_cmd_header *hdr = msg->vaddr;

	hdr->class_id = class_id;
	hdr->cmd_id = cmd_id;
	hdr->proto_ver = proto_ver;
	hdr->len = len;
	hdr->cookie = cookie;
	/* Incrementing msg 2 bytes ahead as the first two bytes are for CRC */
	hdr->csum = rte_cpu_to_be_16(enetc_crc_calc(ENETC_CRC_INIT,
				(uint8_t *)msg->vaddr + sizeof(uint16_t),
				msg->size - sizeof(uint16_t)));

	dcbf(hdr);
}

/* Messaging */
static void
enetc4_msg_vsi_write_msg(struct enetc_hw *hw,
		struct enetc_msg_swbd *msg)
{
	uint32_t val;

	val = enetc_vsi_set_msize(msg->size) | lower_32_bits(msg->dma);
	enetc_wr(hw, ENETC4_VSIMSGSNDAR1, upper_32_bits(msg->dma));
	enetc_wr(hw, ENETC4_VSIMSGSNDAR0, val);
}

static void
enetc4_msg_vsi_reply_msg(struct enetc_eth_hw *hw, int vsimsgsr,
			 struct enetc_psi_reply_msg *reply_msg)
{
	int8_t class_id = 0;
	uint8_t status = 0;

	/* Extracting 8 bits of message result in class_id */
	class_id |= ((ENETC_SIMSGSR_GET_MC(vsimsgsr) >> 8) & 0xff);

	/* Extracting message result in status. With an older kernel PF
	 * (vf_link_legacy set) the lower byte holds a 4-bit cookie in the
	 * low nibble and a 4-bit result in the high nibble, so extract the
	 * upper 4 bits. Otherwise the full lower byte carries the result.
	 */
	if (hw->vf_link_legacy)
		status |= ((ENETC_SIMSGSR_GET_MC(vsimsgsr) >> 4) & 0xf);
	else
		status |= (ENETC_SIMSGSR_GET_MC(vsimsgsr) & 0xff);

	reply_msg->class_id = class_id;
	reply_msg->status = status;
}

static void
enetc4_msg_get_psi_msg(struct enetc_hw *enetc_hw, struct enetc_psi_reply_msg *reply_msg)
{
	struct enetc_eth_hw *hw = container_of(enetc_hw, struct enetc_eth_hw, hw);
	int vsimsgrr;
	int8_t class_id = 0;
	uint8_t status = 0;

	vsimsgrr = enetc_rd(enetc_hw, ENETC4_VSIMSGRR);

	/* Extracting 8 bits of message result in class_id */
	class_id |= ((ENETC_SIMSGSR_GET_MC(vsimsgrr) >> 8) & 0xff);

	/* Extracting message result in status. With an older kernel PF
	 * (vf_link_legacy set) the lower byte holds a 4-bit cookie in the
	 * low nibble and a 4-bit result in the high nibble, so extract the
	 * upper 4 bits. Otherwise the full lower byte carries the result.
	 */
	if (hw->vf_link_legacy)
		status |= ((ENETC_SIMSGSR_GET_MC(vsimsgrr) >> 4) & 0xf);
	else
		status |= (ENETC_SIMSGSR_GET_MC(vsimsgrr) & 0xff);

	reply_msg->class_id = class_id;
	reply_msg->status = status;
}

/* Forward declaration: defined later in this file */
static int enetc4_vf_get_link_speed(struct rte_eth_dev *dev,
				     struct enetc_psi_reply_msg *reply_msg);

/*
 * Decode a PF-to-VF link-speed status code into the link_speed and
 * link_duplex fields of *link.  vf_link_legacy selects the older
 * 4-bit code layout used by kernel PFs before v6.18.37.
 */
static void
enetc4_decode_link_speed(uint8_t status, bool vf_link_legacy,
			 struct rte_eth_link *link)
{
	switch (status) {
	case ENETC_SPEED_UNKNOWN:
		ENETC_PMD_DEBUG("Speed unknown");
		link->link_speed = RTE_ETH_SPEED_NUM_NONE;
		break;
	case ENETC_SPEED_10_HALF_DUPLEX:
		link->link_speed = RTE_ETH_SPEED_NUM_10M;
		link->link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
		break;
	case ENETC_SPEED_10_FULL_DUPLEX:
		link->link_speed = RTE_ETH_SPEED_NUM_10M;
		link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		break;
	case ENETC_SPEED_100_HALF_DUPLEX:
		link->link_speed = RTE_ETH_SPEED_NUM_100M;
		link->link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
		break;
	case ENETC_SPEED_100_FULL_DUPLEX:
		link->link_speed = RTE_ETH_SPEED_NUM_100M;
		link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		break;
	case ENETC_SPEED_1000:
		link->link_speed = RTE_ETH_SPEED_NUM_1G;
		link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		break;
	case ENETC_SPEED_2500:
		link->link_speed = RTE_ETH_SPEED_NUM_2_5G;
		link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		break;
	case ENETC_SPEED_5000:
		link->link_speed = RTE_ETH_SPEED_NUM_5G;
		link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		break;
	default:
		if (vf_link_legacy) {
			/* Legacy PF-to-VF message layout (older kernel PF):
			 * speeds above 5Gbps use fixed 4-bit class codes.
			 */
			switch (status) {
			case ENETC_SPEED_LEGACY_10G:
				link->link_speed = RTE_ETH_SPEED_NUM_10G;
				link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
				break;
			case ENETC_SPEED_LEGACY_25G:
				link->link_speed = RTE_ETH_SPEED_NUM_25G;
				link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
				break;
			case ENETC_SPEED_LEGACY_50G:
				link->link_speed = RTE_ETH_SPEED_NUM_50G;
				link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
				break;
			case ENETC_SPEED_LEGACY_100G:
				link->link_speed = RTE_ETH_SPEED_NUM_100G;
				link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
				break;
			case ENETC_SPEED_LEGACY_NOT_SUPPORTED:
				ENETC_PMD_DEBUG("Speed not supported");
				link->link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
				break;
			default:
				ENETC_PMD_ERR("Unknown speed status");
				link->link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
				break;
			}
			break;
		}
		/* Any status here is > ENETC_SPEED_5000. Validate against
		 * the set of speeds that the NETC IP is known to support.
		 * An unrecognised code yields UNKNOWN rather than a
		 * fabricated speed.
		 */
		switch ((status - ENETC_SPEED_5000) * 1000 + 5000) {
		case RTE_ETH_SPEED_NUM_10G:
			link->link_speed = RTE_ETH_SPEED_NUM_10G;
			link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case RTE_ETH_SPEED_NUM_25G:
			link->link_speed = RTE_ETH_SPEED_NUM_25G;
			link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case RTE_ETH_SPEED_NUM_40G:
			link->link_speed = RTE_ETH_SPEED_NUM_40G;
			link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case RTE_ETH_SPEED_NUM_50G:
			link->link_speed = RTE_ETH_SPEED_NUM_50G;
			link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case RTE_ETH_SPEED_NUM_100G:
			link->link_speed = RTE_ETH_SPEED_NUM_100G;
			link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		case RTE_ETH_SPEED_NUM_200G:
			link->link_speed = RTE_ETH_SPEED_NUM_200G;
			link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
			break;
		default:
			ENETC_PMD_WARN("Unrecognized speed code 0x%x, "
				       "reporting unknown", status);
			link->link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
			break;
		}
		break;
	}
}

static void
enetc4_process_psi_msg(struct rte_eth_dev *eth_dev, struct enetc_hw *enetc_hw)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct enetc_psi_reply_msg *msg;
	struct rte_eth_link link;
	int ret = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		return;
	}

	rte_eth_linkstatus_get(eth_dev, &link);
	enetc4_msg_get_psi_msg(enetc_hw, msg);

	if (msg->class_id == ENETC_CLASS_ID_LINK_STATUS) {
		if (msg->status & ENETC_LINK_DOWN) {
			ENETC_PMD_DEBUG("Link is down");
			link.link_status = RTE_ETH_LINK_DOWN;
		} else {
			ENETC_PMD_DEBUG("Link is up");
			link.link_status = RTE_ETH_LINK_UP;
			/* Re-query speed from PF so the cached value reflects
			 * the current negotiated speed after link-up. This is a
			 * mailbox round trip issued from the link-status
			 * interrupt handler, which runs on the shared EAL
			 * interrupt thread with the mailbox interrupt masked, so
			 * it busy-waits up to (vsi_timeout * vsi_delay) us
			 * (by default 100 * 2000 us = 200 ms). If this stall on
			 * the EAL interrupt thread ever becomes a problem, reduce
			 * the wait budget with the "enetc4_vsi_timeout" and
			 * "enetc4_vsi_delay" devargs.
			 */
			memset(msg, 0, sizeof(*msg));
			if (!enetc4_vf_get_link_speed(eth_dev, msg) &&
			    msg->class_id == ENETC_CLASS_ID_LINK_SPEED)
				enetc4_decode_link_speed(msg->status,
							hw->vf_link_legacy,
							&link);
		}
		ret = rte_eth_linkstatus_set(eth_dev, &link);
		if (!ret)
			ENETC_PMD_DEBUG("Link status has been changed");

		/* Process user registered callback */
		rte_eth_dev_callback_process(eth_dev,
			RTE_ETH_EVENT_INTR_LSC, NULL);
	} else {
		ENETC_PMD_ERR("Wrong message 0x%x", msg->class_id);
	}

	rte_free(msg);
}

static int
enetc4_msg_vsi_send(struct enetc_eth_hw *hw, struct enetc_msg_swbd *msg,
		    int *vsimsgsr_out)
{
	struct enetc_hw *enetc_hw = &hw->hw;
	int timeout = hw->vsi_timeout ? (int)hw->vsi_timeout :
					ENETC4_DEF_VSI_WAIT_TIMEOUT_UPDATE;
	int delay_us = hw->vsi_delay ? (int)hw->vsi_delay :
				       ENETC4_DEF_VSI_WAIT_DELAY_UPDATE;
	uint8_t class_id = 0;
	int err = 0;
	int vsimsgsr;

	pthread_mutex_lock(&hw->vsi_lock);
	enetc4_msg_vsi_write_msg(enetc_hw, msg);

	do {
		vsimsgsr = enetc_rd(enetc_hw, ENETC4_VSIMSGSR);
		if (!(vsimsgsr & ENETC4_VSIMSGSR_MB))
			break;
		rte_delay_us(delay_us);
	} while (--timeout);

	if (!timeout) {
		ENETC_PMD_ERR("Message not processed by PSI");
		pthread_mutex_unlock(&hw->vsi_lock);
		return -ETIMEDOUT;
	}
	/* check for message delivery error */
	if (vsimsgsr & ENETC4_VSIMSGSR_MS) {
		ENETC_PMD_ERR("Transfer error when copying the data");
		pthread_mutex_unlock(&hw->vsi_lock);
		return -EIO;
	}

	class_id |= ((ENETC_SIMSGSR_GET_MC(vsimsgsr) >> 8) & 0xff);

	/* Check the user-defined completion status. */
	if (class_id != ENETC_MSG_CLASS_ID_CMD_SUCCESS) {
		switch (class_id) {
		case ENETC_MSG_CLASS_ID_PERMISSION_DENY:
			ENETC_PMD_ERR("Permission denied");
			err = -EACCES;
			break;
		case ENETC_MSG_CLASS_ID_CMD_NOT_SUPPORT:
			ENETC_PMD_ERR("Command not supported");
			err = -EOPNOTSUPP;
			break;
		case ENETC_MSG_CLASS_ID_PSI_BUSY:
			ENETC_PMD_ERR("PSI Busy");
			err = -EBUSY;
			break;
		case ENETC_MSG_CLASS_ID_CMD_TIMEOUT:
			ENETC_PMD_ERR("Command timeout");
			err = -ETIME;
			break;
		case ENETC_MSG_CLASS_ID_CRC_ERROR:
			ENETC_PMD_ERR("CRC error");
			err = -EIO;
			break;
		case ENETC_MSG_CLASS_ID_PROTO_NOT_SUPPORT:
			ENETC_PMD_ERR("Protocol Version not supported");
			err = -EOPNOTSUPP;
			break;
		case ENETC_MSG_CLASS_ID_INVALID_MSG_LEN:
			ENETC_PMD_ERR("Invalid message length");
			err = -EINVAL;
			break;
		case ENETC_CLASS_ID_MAC_FILTER:
		case ENETC_CLASS_ID_LINK_STATUS:
		case ENETC_CLASS_ID_LINK_SPEED:
		case ENETC_CLASS_ID_GET_IP_VER:
			break;
		default:
			err = -EIO;
		}
	}

	if (vsimsgsr_out != NULL)
		*vsimsgsr_out = vsimsgsr;
	pthread_mutex_unlock(&hw->vsi_lock);
	return err;
}

static int
enetc4_vf_set_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *addr)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_cmd_set_primary_mac *cmd;
	struct enetc_msg_swbd *msg;
	struct enetc_psi_reply_msg *reply_msg;
	uint32_t msg_size;
	int err = 0;
	int vsimsgsr_mac_set = 0;

	PMD_INIT_FUNC_TRACE();
	reply_msg = rte_zmalloc(NULL, sizeof(*reply_msg), RTE_CACHE_LINE_SIZE);
	if (!reply_msg) {
		ENETC_PMD_ERR("Failed to alloc memory for reply_msg");
		return -ENOMEM;
	}

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		rte_free(reply_msg);
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_primary_mac),
				ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		rte_free(reply_msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	cmd = (struct enetc_msg_cmd_set_primary_mac *)msg->vaddr;

	cmd->count = 0;
	memcpy(&cmd->addr.addr_bytes, addr, sizeof(struct rte_ether_addr));

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_MAC_FILTER,
					ENETC_CMD_ID_SET_PRIMARY_MAC, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, &vsimsgsr_mac_set);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(hw, vsimsgsr_mac_set, reply_msg);

	if (reply_msg->class_id == ENETC_CLASS_ID_MAC_FILTER) {
		switch (reply_msg->status) {
		case ENETC_INVALID_MAC_ADDR:
			ENETC_PMD_ERR("Invalid MAC address");
			err = -EINVAL;
			break;
		case ENETC_DUPLICATE_MAC_ADDR:
			ENETC_PMD_ERR("Duplicate MAC address");
			err = -EINVAL;
			break;
		default:
			err = -EINVAL;
			break;
		}
	}

	if (err) {
		ENETC_PMD_ERR("VSI command execute error!");
		goto end;
	}

	rte_ether_addr_copy((struct rte_ether_addr *)&cmd->addr,
			&dev->data->mac_addrs[0]);

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(reply_msg);
	rte_free(msg);
	return err;
}

static int
enetc4_vf_promisc_send_message(struct rte_eth_dev *dev, bool promisc_en)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_cmd_set_promisc *cmd;
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_promisc), ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	cmd = (struct enetc_msg_cmd_set_promisc *)msg->vaddr;

	/* op_type is based on the result of message format
	 *    7  6      1       0
	      type   promisc  flush
	 */

	if (promisc_en)
		cmd->op_type = ENETC_PROMISC_ENABLE;
	else
		cmd->op_type = ENETC_PROMISC_DISABLE;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_MAC_FILTER,
				ENETC_CMD_ID_SET_MAC_PROMISCUOUS, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, NULL);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}

static int
enetc4_vf_allmulti_send_message(struct rte_eth_dev *dev, bool mc_promisc)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_cmd_set_promisc *cmd;
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_promisc),
				ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	cmd = (struct enetc_msg_cmd_set_promisc *)msg->vaddr;

	/* op_type is based on the result of message format
	 *    7  6      1       0
	      type   promisc  flush
	 */

	if (mc_promisc)
		cmd->op_type = ENETC_ALLMULTI_PROMISC_EN;
	else
		cmd->op_type = ENETC_ALLMULTI_PROMISC_DIS;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_MAC_FILTER,
				ENETC_CMD_ID_SET_MAC_PROMISCUOUS, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, NULL);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}


static int
enetc4_vf_multicast_enable(struct rte_eth_dev *dev)
{
	int err;

	PMD_INIT_FUNC_TRACE();
	err = enetc4_vf_allmulti_send_message(dev, true);
	if (err) {
		ENETC_PMD_ERR("Failed to enable multicast promiscuous mode");
		return err;
	}

	return 0;
}

static int
enetc4_vf_multicast_disable(struct rte_eth_dev *dev)
{
	int err;

	PMD_INIT_FUNC_TRACE();
	err = enetc4_vf_allmulti_send_message(dev, false);
	if (err) {
		ENETC_PMD_ERR("Failed to disable multicast promiscuous mode");
		return err;
	}

	return 0;
}

static int
enetc4_vf_promisc_enable(struct rte_eth_dev *dev)
{
	int err;

	PMD_INIT_FUNC_TRACE();
	err = enetc4_vf_promisc_send_message(dev, true);
	if (err) {
		ENETC_PMD_ERR("Failed to enable promiscuous mode");
		return err;
	}

	return 0;
}

static int
enetc4_vf_promisc_disable(struct rte_eth_dev *dev)
{
	int err;

	PMD_INIT_FUNC_TRACE();
	err = enetc4_vf_promisc_send_message(dev, false);
	if (err) {
		ENETC_PMD_ERR("Failed to disable promiscuous mode");
		return err;
	}

	return 0;
}

static int
enetc4_vf_get_link_status(struct rte_eth_dev *dev, struct enetc_psi_reply_msg *reply_msg)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;
	int vsimsgsr_link_st = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_get_link_status),
			ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_LINK_STATUS,
			ENETC_CMD_ID_GET_LINK_STATUS, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, &vsimsgsr_link_st);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(hw, vsimsgsr_link_st, reply_msg);
end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}

static int
enetc4_vf_get_link_speed(struct rte_eth_dev *dev, struct enetc_psi_reply_msg *reply_msg)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;
	int vsimsgsr_link_sp = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_get_link_speed),
				ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_LINK_SPEED,
			ENETC_CMD_ID_GET_LINK_SPEED, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, &vsimsgsr_link_sp);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(hw, vsimsgsr_link_sp, reply_msg);
end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}

/*
 * Get the NETC IP minor revision (IP_MN) from the PSI using the 'Get IP
 * version' command class (class ID 0xF0, cmd_id 0x1). In the reply, the
 * low 8 bits of the return code carry the minor revision and the upper 8
 * bits carry the class code (0xF0). The PSI only implements the IP_MN
 * command of this class; the major revision comes from the PCI revision.
 */
static int
enetc4_vf_get_ip_minor_revision(struct rte_eth_dev *dev, uint8_t *ip_mn)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	uint16_t mc;
	uint8_t class_id;
	int vsimsgsr = 0;
	int err = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		return -ENOMEM;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_get_ip_ver),
				ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	/* COOKIE is 0 so that the command is executed as blocking on PSI */
	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_GET_IP_VER,
			ENETC_CMD_ID_GET_IP_MN, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, &vsimsgsr);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	/*
	 * For the IP version command class the class-specific field is
	 * reused to carry the 8-bit version value in the lower byte of the
	 * return code, so parse the full low byte here instead of using the
	 * generic reply parser.
	 */
	mc = ENETC_SIMSGSR_GET_MC(vsimsgsr);
	class_id = (mc >> 8) & 0xff;

	if (class_id != ENETC_CLASS_ID_GET_IP_VER) {
		ENETC_PMD_ERR("Wrong reply message 0x%x", class_id);
		err = -EIO;
		goto end;
	}

	*ip_mn = mc & 0xff;
	if (*ip_mn == ENETC_IP_VER_NOT_AVAILABLE) {
		ENETC_PMD_DEBUG("IP minor revision not available");
		err = -ENOTSUP;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}

/*
 * Retrieve the NETC firmware/IP version and format it as
 * "<major>.<minor>". The IP major revision is taken from the PCI
 * revision ID register, and the IP minor revision is fetched from the
 * PSI via VSI-PSI messaging ('Get IP version' command class). This
 * mirrors the behaviour of the kernel PF/VF drivers, where the PSI only
 * implements the IP_MN command of the 0xF0 class.
 */
static int
enetc4_vf_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(dev, *pci_dev);
	uint8_t ip_mj = 0, ip_mn = 0;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (fw_version == NULL)
		return -EINVAL;

	/* IP major revision is exposed through the PCI revision ID */
	ret = rte_pci_read_config(pci_dev, &ip_mj, sizeof(ip_mj),
			RTE_PCI_REVISION_ID);
	if (ret != sizeof(ip_mj)) {
		ENETC_PMD_ERR("Failed to read PCI revision ID");
		return -EIO;
	}

	/* IP minor revision is fetched from the PSI via VSI-PSI messaging.
	 * If the PSI reports the version as unavailable, fall back to a
	 * partial version string so the caller still gets useful output.
	 */
	ret = enetc4_vf_get_ip_minor_revision(dev, &ip_mn);
	if (ret && ret != -ENOTSUP) {
		ENETC_PMD_ERR("Failed to get NETC minor revision");
		return ret;
	}

	if (fw_size == 0) {
		if (ret == -ENOTSUP)
			return snprintf(NULL, 0, "%u.unknown", ip_mj) + 1;
		else
			return snprintf(NULL, 0, "%u.%u", ip_mj, ip_mn) + 1;
	}

	if (ret == -ENOTSUP)
		ret = snprintf(fw_version, fw_size, "%u.unknown", ip_mj);
	else
		ret = snprintf(fw_version, fw_size, "%u.%u", ip_mj, ip_mn);
	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add trailing '\0' */
	if ((size_t)ret > fw_size)
		return ret;

	return 0;
}

/* VF station interface registers dumped by .get_reg */
static const uint32_t enetc4_vf_si_regs[] = {
	ENETC_SIMR, ENETC_SICAPR0, ENETC_SIPMAR0, ENETC_SIPMAR1,
	ENETC4_SIROCT0, ENETC4_SIRFRM0, ENETC4_SITOCT0, ENETC4_SITFRM0,
	ENETC4_SITDFCR, ENETC4_SIMSIVR, ENETC4_VSIIER, ENETC4_VSIIDR,
	ENETC4_VSIMSGSR, ENETC4_VSIMSGRR,
};

/*
 * Dump the VF-accessible registers. Only station interface and per-ring
 * BD ring registers are reachable by a VF; port registers are not.
 * When info->data is NULL, only the register count and width are
 * reported so the caller can size its buffer.
 */
static int
enetc4_vf_get_regs(struct rte_eth_dev *dev, struct rte_dev_reg_info *regs)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t count, addr;
	uint32_t *buf;
	uint16_t i, j;

	count = RTE_DIM(enetc4_vf_si_regs);
	count += RTE_DIM(enetc4_txbdr_regs) * dev->data->nb_tx_queues;
	count += RTE_DIM(enetc4_rxbdr_regs) * dev->data->nb_rx_queues;

	if (regs->data == NULL) {
		regs->length = count;
		regs->width = sizeof(uint32_t);
		return 0;
	}

	if (regs->length && regs->length < count)
		return -ENOTSUP;

	buf = regs->data;

	for (i = 0; i < RTE_DIM(enetc4_vf_si_regs); i++)
		*buf++ = enetc_rd(enetc_hw, enetc4_vf_si_regs[i]);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		for (j = 0; j < RTE_DIM(enetc4_txbdr_regs); j++) {
			addr = ENETC_BDR(TX, i, enetc4_txbdr_regs[j]);
			*buf++ = enetc_rd(enetc_hw, addr);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		for (j = 0; j < RTE_DIM(enetc4_rxbdr_regs); j++) {
			addr = ENETC_BDR(RX, i, enetc4_rxbdr_regs[j]);
			*buf++ = enetc_rd(enetc_hw, addr);
		}
	}

	regs->version = (uint32_t)hw->device_id << 16 | hw->revision_id;

	return 0;
}

static int
enetc4_vf_link_update_dummy(struct rte_eth_dev *dev __rte_unused,
			    int wait_to_complete __rte_unused)
{
	return 0;
}

static int
enetc4_vf_link_update(struct rte_eth_dev *dev, int wait_to_complete __rte_unused)
{
	struct enetc_eth_hw *hw =
			ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_psi_reply_msg *reply_msg;
	struct rte_eth_link link;
	int err;

	PMD_INIT_FUNC_TRACE();
	reply_msg = rte_zmalloc(NULL, sizeof(*reply_msg), RTE_CACHE_LINE_SIZE);
	if (!reply_msg) {
		ENETC_PMD_ERR("Failed to alloc memory for reply_msg");
		return -ENOMEM;
	}

	memset(&link, 0, sizeof(struct rte_eth_link));

	err = enetc4_vf_get_link_status(dev, reply_msg);
	if (err) {
		ENETC_PMD_ERR("Failed to get link status");
		rte_free(reply_msg);
		return err;
	}

	if (reply_msg->class_id == ENETC_CLASS_ID_LINK_STATUS) {
		if (reply_msg->status & ENETC_LINK_DOWN)
			link.link_status = RTE_ETH_LINK_DOWN;
		else
			link.link_status = RTE_ETH_LINK_UP;
	} else {
		ENETC_PMD_ERR("Wrong reply message");
		rte_free(reply_msg);
		return -1;
	}

	err = enetc4_vf_get_link_speed(dev, reply_msg);
	if (err) {
		ENETC_PMD_ERR("Failed to get link speed");
		rte_free(reply_msg);
		return err;
	}

	if (reply_msg->class_id == ENETC_CLASS_ID_LINK_SPEED) {
		enetc4_decode_link_speed(reply_msg->status,
					 hw->vf_link_legacy, &link);
	} else {
		ENETC_PMD_ERR("Wrong reply message");
		rte_free(reply_msg);
		return -1;
	}

	link.link_autoneg = 1;
	rte_eth_linkstatus_set(dev, &link);
	rte_free(reply_msg);

	return 0;
}

static int
enetc4_vf_vlan_promisc(struct rte_eth_dev *dev, bool promisc_en)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_cmd_set_vlan_promisc *cmd;
	struct enetc_msg_swbd *msg;
	uint32_t msg_size;
	int err = 0;

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_vlan_promisc),
				ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}
	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;

	cmd = (struct enetc_msg_cmd_set_vlan_promisc *)msg->vaddr;
	/* op is based on the result of message format
	 *	   1	  0
	 *	promisc	flush
	 */

	if (promisc_en)
		cmd->op = ENETC_PROMISC_VLAN_ENABLE;
	else
		cmd->op = ENETC_PROMISC_VLAN_DISABLE;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_VLAN_FILTER,
				ENETC_CMD_ID_SET_VLAN_PROMISCUOUS, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, NULL);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);
	return err;
}

static int
enetc4_vf_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *addr,
			uint32_t index __rte_unused, uint32_t pool __rte_unused)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_cmd_set_primary_mac *cmd;
	struct enetc_msg_swbd *msg;
	struct enetc_psi_reply_msg *reply_msg;
	uint32_t msg_size;
	int err = 0;
	int vsimsgsr_mac_add = 0;

	PMD_INIT_FUNC_TRACE();

	if (!rte_is_valid_assigned_ether_addr(addr))
		return -EINVAL;

	reply_msg = rte_zmalloc(NULL, sizeof(*reply_msg), RTE_CACHE_LINE_SIZE);
	if (!reply_msg) {
		ENETC_PMD_ERR("Failed to alloc memory for reply_msg");
		return -ENOMEM;
	}

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		rte_free(reply_msg);
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_set_primary_mac),
			ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		rte_free(reply_msg);
		return -ENOMEM;
	}
	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;
	cmd = (struct enetc_msg_cmd_set_primary_mac *)msg->vaddr;
	memcpy(&cmd->addr.addr_bytes, addr, sizeof(struct rte_ether_addr));
	cmd->count = 1;

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_MAC_FILTER,
			ENETC_MSG_ADD_EXACT_MAC_ENTRIES, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, &vsimsgsr_mac_add);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(hw, vsimsgsr_mac_add, reply_msg);

	if (reply_msg->class_id == ENETC_CLASS_ID_MAC_FILTER) {
		switch (reply_msg->status) {
		case ENETC_INVALID_MAC_ADDR:
			ENETC_PMD_ERR("Invalid MAC address");
			err = -EINVAL;
			break;
		case ENETC_DUPLICATE_MAC_ADDR:
			ENETC_PMD_ERR("Duplicate MAC address");
			err = -EINVAL;
			break;
		case ENETC_MAC_FILTER_NO_RESOURCE:
			ENETC_PMD_ERR("Not enough exact-match entries available");
			err = -EINVAL;
			break;
		default:
			err = -EINVAL;
			break;
		}
	}

	if (err) {
		ENETC_PMD_ERR("VSI command execute error!");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(reply_msg);
	rte_free(msg);
	return err;
}

static int enetc4_vf_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_vlan_exact_filter *cmd;
	struct enetc_msg_swbd *msg;
	struct enetc_psi_reply_msg *reply_msg;
	uint32_t msg_size;
	int err = 0;
	int vsimsgsr_vlan = 0;

	PMD_INIT_FUNC_TRACE();

	reply_msg = rte_zmalloc(NULL, sizeof(*reply_msg), RTE_CACHE_LINE_SIZE);
	if (!reply_msg) {
		ENETC_PMD_ERR("Failed to alloc memory for reply_msg");
		return -ENOMEM;
	}

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		rte_free(reply_msg);
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_vlan_exact_filter),
			ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		rte_free(reply_msg);
		return -ENOMEM;
	}
	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;
	cmd = (struct enetc_msg_vlan_exact_filter *)msg->vaddr;
	cmd->vlan_count = 1;
	cmd->vlan_id = vlan_id;

	/* TPID 2-bit encoding value is taken from the H/W block guide:
	 *	00b Standard C-VLAN 0x8100
	 *	01b Standard S-VLAN 0x88A8
	 *	10b Custom VLAN as defined by CVLANR1[ETYPE]
	 *	11b Custom VLAN as defined by CVLANR2[ETYPE]
	 * Currently Standard C-VLAN is supported. To support others in future.
	 */
	cmd->tpid = 0;

	if (on) {
		enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_VLAN_FILTER,
				ENETC_MSG_ADD_EXACT_VLAN_ENTRIES, 0, 0, 0);
	} else {
		enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_VLAN_FILTER,
				ENETC_MSG_REMOVE_EXACT_VLAN_ENTRIES, 0, 0, 0);
	}

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, &vsimsgsr_vlan);
	if (err) {
		ENETC_PMD_ERR("VSI message send error");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(hw, vsimsgsr_vlan, reply_msg);

	if (reply_msg->class_id == ENETC_CLASS_ID_VLAN_FILTER) {
		switch (reply_msg->status) {
		case ENETC_INVALID_VLAN_ENTRY:
			ENETC_PMD_ERR("VLAN entry not valid");
			err = -EINVAL;
			break;
		case ENETC_DUPLICATE_VLAN_ENTRY:
			ENETC_PMD_ERR("Duplicated VLAN entry");
			err = -EINVAL;
			break;
		case ENETC_VLAN_ENTRY_NOT_FOUND:
			ENETC_PMD_ERR("VLAN entry not found");
			err = -EINVAL;
			break;
		case ENETC_VLAN_NO_RESOURCE:
			ENETC_PMD_ERR("Not enough exact-match entries available");
			err = -EINVAL;
			break;
		default:
			err = -EINVAL;
			break;
		}
	}

	if (err) {
		ENETC_PMD_ERR("VSI command execute error!");
		goto end;
	}

end:
	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(reply_msg);
	rte_free(msg);
	return err;
}

static int enetc4_vf_vlan_offload_set(struct rte_eth_dev *dev, int mask __rte_unused)
{
	int err = 0;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.offloads) {
		ENETC_PMD_DEBUG("VLAN filter table entry inserted:"
					"Disabling VLAN promisc mode");
		err = enetc4_vf_vlan_promisc(dev, false);
		if (err) {
			ENETC_PMD_ERR("Added VLAN filter table entry:"
					"Failed to disable promiscuous mode");
			return err;
		}
	} else {
		ENETC_PMD_DEBUG("Enabling VLAN promisc mode");
		err = enetc4_vf_vlan_promisc(dev, true);
		if (err) {
			ENETC_PMD_ERR("Vlan filter table empty:"
					"Failed to enable promiscuous mode");
			return err;
		}
	}

	return 0;
}

/* Configure SI-based VLAN insertion/removal via VSI-PSI mailbox (class 0x24). */
static int
enetc4_vf_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_si_vlan_iso *cmd;
	struct enetc_msg_swbd *msg;
	struct enetc_psi_reply_msg *reply_msg;
	uint32_t msg_size;
	int err = 0;
	int vsimsgsr_pvid = 0;

	PMD_INIT_FUNC_TRACE();

	if (vlan_id > RTE_ETHER_MAX_VLAN_ID) {
		ENETC_PMD_ERR("Invalid vlan_id = %u > %d", vlan_id,
			      RTE_ETHER_MAX_VLAN_ID);
		return -EINVAL;
	}

	reply_msg = rte_zmalloc(NULL, sizeof(*reply_msg), RTE_CACHE_LINE_SIZE);
	if (!reply_msg) {
		ENETC_PMD_ERR("Failed to alloc memory for reply_msg");
		return -ENOMEM;
	}

	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		rte_free(reply_msg);
		return -ENOMEM;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_si_vlan_iso),
			     ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg body");
		rte_free(msg);
		rte_free(reply_msg);
		return -ENOMEM;
	}
	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	if (msg->dma == RTE_BAD_IOVA) {
		ENETC_PMD_ERR("Failed to get IOVA for SI VLAN isolation msg body");
		rte_free(msg->vaddr);
		rte_free(msg);
		rte_free(reply_msg);
		return -ENOMEM;
	}
	msg->size = msg_size;

	cmd = (struct enetc_msg_si_vlan_iso *)msg->vaddr;

	if (on) {
		cmd->ctrl = (uint8_t)ENETC_SI_VLAN_ISO_E;
		cmd->pcp_dei_vid_hi = (uint8_t)((vlan_id >> 8) & 0x0f);
		cmd->vid_lo = (uint8_t)(vlan_id & 0xff);
		cmd->flags = (uint8_t)(ENETC_SI_VLAN_ISO_SVIE | ENETC_SI_VLAN_ISO_VTE);
	}
	/* on=0: all fields zero (rte_zmalloc cleared the buffer) */

	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_SI_VLAN_ISO,
				     ENETC_CMD_ID_SET_SI_VLAN_ISO, 0, 0, 0);

	/* send the command and wait for PSI reply */
	err = enetc4_msg_vsi_send(hw, msg, &vsimsgsr_pvid);
	if (err) {
		ENETC_PMD_ERR("VSI message send error for SI VLAN isolation");
		goto end;
	}

	enetc4_msg_vsi_reply_msg(hw, vsimsgsr_pvid, reply_msg);

	/*
	 * The PSI reports command completion in the reply class_id: a
	 * successful command returns ENETC_MSG_CLASS_ID_CMD_SUCCESS (0x1),
	 * while any other class_id (e.g. CMD_NOT_SUPPORT 0x3) indicates a
	 * failure. The command class value (0x24) is only used in the
	 * outgoing VF-to-PSI header and is never echoed back in the reply,
	 * so treat class_id != CMD_SUCCESS as failure.
	 */
	if (reply_msg->class_id != ENETC_MSG_CLASS_ID_CMD_SUCCESS) {
		ENETC_PMD_ERR("SI VLAN isolation command failed: class_id=0x%x status=0x%x",
			      reply_msg->class_id, reply_msg->status);
		err = -EINVAL;
	}

end:
	rte_free(msg->vaddr);
	rte_free(msg);
	rte_free(reply_msg);
	return err;
}

static int
enetc4_vf_mtu_set(struct rte_eth_dev *dev __rte_unused, uint16_t mtu __rte_unused)
{
	return 0;
}

static int
enetc4_vf_link_register_notif(struct rte_eth_dev *dev, bool enable)
{
	struct enetc_eth_hw *hw = ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_msg_swbd *msg;
	struct rte_eth_link link;
	uint32_t msg_size;
	int err = 0;
	uint8_t cmd;

	PMD_INIT_FUNC_TRACE();
	memset(&link, 0, sizeof(struct rte_eth_link));
	msg = rte_zmalloc(NULL, sizeof(*msg), RTE_CACHE_LINE_SIZE);
	if (!msg) {
		ENETC_PMD_ERR("Failed to alloc msg");
		err = -ENOMEM;
		return err;
	}

	msg_size = RTE_ALIGN(sizeof(struct enetc_msg_cmd_get_link_status), ENETC_VSI_PSI_MSG_SIZE);
	msg->vaddr = rte_zmalloc(NULL, msg_size, 0);
	if (!msg->vaddr) {
		ENETC_PMD_ERR("Failed to alloc memory for msg");
		rte_free(msg);
		return -ENOMEM;
	}

	msg->dma = rte_mem_virt2iova((const void *)msg->vaddr);
	msg->size = msg_size;
	if (enable)
		cmd = ENETC_CMD_ID_REGISTER_LINK_NOTIF;
	else
		cmd = ENETC_CMD_ID_UNREGISTER_LINK_NOTIF;
	enetc_msg_vf_fill_common_hdr(msg, ENETC_CLASS_ID_LINK_STATUS,
			cmd, 0, 0, 0);

	/* send the command and wait */
	err = enetc4_msg_vsi_send(hw, msg, NULL);
	if (err)
		ENETC_PMD_ERR("VSI msg error for link status notification");

	/* free memory no longer required */
	rte_free(msg->vaddr);
	rte_free(msg);

	return err;
}

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_vf_id_enetc4_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_NXP, ENETC4_DEV_ID_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static int
enetc4_vf_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct enetc_bdr *rx_ring;
	uint16_t vec;

	if (hw->nc_mode)
		return -ENOTSUP;

	if (!hw->rxq_intr_en)
		return -ENOTSUP;

	vec = queue_id + ENETC4_VF_RX_VEC_BASE;
	rx_ring = (struct enetc_bdr *)dev->data->rx_queues[queue_id];

	enetc_wr(enetc_hw, ENETC_SIMSIRRV(queue_id), vec);
	/*
	 * Do not overwrite RBICR1 when RSC (LRO) is active: the timer
	 * programmed there is the coalesce-hold window and zeroing it
	 * would disable coalescing, flushing every segment individually.
	 */
	if (!rx_ring->rsc_enable)
		enetc4_rxbdr_wr(enetc_hw, queue_id, ENETC4_RBICR1, 0);
	enetc4_rxbdr_wr(enetc_hw, queue_id, ENETC4_RBICR0,
			ENETC4_RBICR0_ICEN | ENETC4_RBICR0_ICPT(1));
	enetc_wr(enetc_hw, ENETC_SIRXIDR, BIT(queue_id));

	enetc4_rxbdr_wr(enetc_hw, queue_id, ENETC_RBIER, ENETC_RBIER_RXTIE);

	return 0;
}

static int
enetc4_vf_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;

	if (hw->nc_mode)
		return -ENOTSUP;

	if (!hw->rxq_intr_en)
		return 0;

	enetc4_rxbdr_wr(enetc_hw, queue_id, ENETC_RBIER, 0);
	enetc_wr(enetc_hw, ENETC_SIMSIRRV(queue_id), 0);

	return 0;
}

/* Features supported by this driver */
/* ops table used when VSI messaging is disabled */
static const struct eth_dev_ops enetc4_vf_ops_no_vsi_m = {
	.dev_configure        = enetc4_dev_configure,
	.dev_start            = enetc4_vf_dev_start,
	.dev_stop             = enetc4_vf_dev_stop,
	.dev_close            = enetc4_dev_close,
	.stats_get            = enetc4_vf_stats_get,
	.stats_reset          = enetc4_vf_stats_reset,
	.dev_infos_get        = enetc4_vf_dev_infos_get,
	.get_reg              = enetc4_vf_get_regs,
	.mtu_set              = enetc4_vf_mtu_set,
	.link_update	      = enetc4_vf_link_update_dummy,
	.rx_queue_setup       = enetc4_rx_queue_setup,
	.rx_queue_start       = enetc4_rx_queue_start,
	.rx_queue_stop        = enetc4_rx_queue_stop,
	.rx_queue_release     = enetc4_rx_queue_release,
	.rxq_info_get         = enetc4_rxq_info_get,
	.rx_queue_intr_enable  = enetc4_vf_rx_queue_intr_enable,
	.rx_queue_intr_disable = enetc4_vf_rx_queue_intr_disable,
	.tx_queue_setup       = enetc4_tx_queue_setup,
	.tx_queue_start       = enetc4_tx_queue_start,
	.tx_queue_stop        = enetc4_tx_queue_stop,
	.tx_queue_release     = enetc4_tx_queue_release,
	.txq_info_get         = enetc4_txq_info_get,
	.dev_supported_ptypes_get = enetc4_supported_ptypes_get,
};

static const struct eth_dev_ops enetc4_vf_ops = {
	.dev_configure        = enetc4_dev_configure,
	.dev_start            = enetc4_vf_dev_start,
	.dev_stop             = enetc4_vf_dev_stop,
	.dev_close            = enetc4_dev_close,
	.stats_get            = enetc4_vf_stats_get,
	.stats_reset          = enetc4_vf_stats_reset,
	.dev_infos_get        = enetc4_vf_dev_infos_get,
	.fw_version_get       = enetc4_vf_fw_version_get,
	.get_reg              = enetc4_vf_get_regs,
	.mtu_set              = enetc4_vf_mtu_set,
	.mac_addr_set         = enetc4_vf_set_mac_addr,
	.mac_addr_add	      = enetc4_vf_mac_addr_add,
	.promiscuous_enable   = enetc4_vf_promisc_enable,
	.promiscuous_disable  = enetc4_vf_promisc_disable,
	.allmulticast_enable  = enetc4_vf_multicast_enable,
	.allmulticast_disable = enetc4_vf_multicast_disable,
	.link_update	      = enetc4_vf_link_update,
	.vlan_filter_set      = enetc4_vf_vlan_filter_set,
	.vlan_offload_set     = enetc4_vf_vlan_offload_set,
	.vlan_pvid_set        = enetc4_vf_vlan_pvid_set,
	.rx_queue_setup       = enetc4_rx_queue_setup,
	.rx_queue_start       = enetc4_rx_queue_start,
	.rx_queue_stop        = enetc4_rx_queue_stop,
	.rx_queue_release     = enetc4_rx_queue_release,
	.rxq_info_get         = enetc4_rxq_info_get,
	.rx_queue_intr_enable  = enetc4_vf_rx_queue_intr_enable,
	.rx_queue_intr_disable = enetc4_vf_rx_queue_intr_disable,
	.tx_queue_setup       = enetc4_tx_queue_setup,
	.tx_queue_start       = enetc4_tx_queue_start,
	.tx_queue_stop        = enetc4_tx_queue_stop,
	.tx_queue_release     = enetc4_tx_queue_release,
	.txq_info_get         = enetc4_txq_info_get,
	.dev_supported_ptypes_get = enetc4_supported_ptypes_get,
};

static int
enetc4_vf_mac_init(struct enetc_eth_hw *hw, struct rte_eth_dev *eth_dev)
{
	uint32_t *mac = (uint32_t *)hw->mac.addr;
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t high_mac = 0;
	uint16_t low_mac = 0;
	char vf_eth_name[ENETC_ETH_NAMESIZE];

	PMD_INIT_FUNC_TRACE();

	/* Enabling Station Interface */
	enetc4_wr(enetc_hw, ENETC_SIMR, ENETC_SIMR_EN);
	*mac = (uint32_t)enetc_rd(enetc_hw, ENETC_SIPMAR0);
	high_mac = (uint32_t)*mac;
	mac++;
	*mac = (uint16_t)enetc_rd(enetc_hw, ENETC_SIPMAR1);
	low_mac = (uint16_t)*mac;

	if ((high_mac | low_mac) == 0) {
		char *first_byte;
		ENETC_PMD_NOTICE("MAC is not available for this SI, "
				 "set random MAC");
		mac = (uint32_t *)hw->mac.addr;
		*mac = (uint32_t)rte_rand();
		first_byte = (char *)mac;
		*first_byte &= 0xfe;    /* clear multicast bit */
		*first_byte |= 0x02;    /* set local assignment bit (IEEE802) */
		enetc4_port_wr(enetc_hw, ENETC4_PMAR0, *mac);
		mac++;
		*mac = (uint16_t)rte_rand();
		enetc4_port_wr(enetc_hw, ENETC4_PMAR1, *mac);
		enetc_print_ethaddr("New address: ",
			(const struct rte_ether_addr *)hw->mac.addr);
	}

	/* Allocate memory for storing MAC addresses */
	snprintf(vf_eth_name, sizeof(vf_eth_name), "enetc4_vf_eth_%d", eth_dev->data->port_id);
	eth_dev->data->mac_addrs = rte_zmalloc(vf_eth_name,
					RTE_ETHER_ADDR_LEN, 0);
	if (!eth_dev->data->mac_addrs) {
		ENETC_PMD_ERR("Failed to allocate %d bytes needed to "
			      "store MAC addresses",
			      RTE_ETHER_ADDR_LEN * 1);
		return -ENOMEM;
	}

	if (!enetc_crc_gen)
		enetc_gen_crc_table();

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.addr,
			     &eth_dev->data->mac_addrs[0]);

	return 0;
}

static void
enetc_vf_enable_mr_int(struct enetc_hw *hw, bool en)
{
	uint32_t val;

	val = enetc_rd(hw, ENETC4_VSIIER);
	val &= ~ENETC4_VSIIER_MRIE;
	val |= (en) ? ENETC4_VSIIER_MRIE : 0;
	enetc_wr(hw, ENETC4_VSIIER, val);
	ENETC_PMD_DEBUG("Interrupt enable status (VSIIER) = 0x%x", val);
}

static void
enetc4_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	uint32_t status;

	/* Disable interrupts before process */
	enetc_vf_enable_mr_int(enetc_hw, false);

	status = enetc_rd(enetc_hw, ENETC4_VSIIDR);
	ENETC_PMD_DEBUG("Got INTR VSIIDR status = 0x%0x", status);
	/* Check for PSI to VSI message interrupt */
	if (!(status & ENETC4_VSIIER_MRIE)) {
		ENETC_PMD_ERR("Interrupt is not PSI to VSI");
		goto intr_clear;
	}

	enetc4_process_psi_msg(eth_dev, enetc_hw);
intr_clear:
	/* Clear Interrupts */
	enetc_wr(enetc_hw, ENETC4_VSIIDR, 0xffffffff);
	enetc_vf_enable_mr_int(enetc_hw, true);
}

static int
enetc4_vf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct enetc_eth_hw *hw =
			    ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(eth_dev, *pci_dev);
	int error = 0;
	uint32_t si_cap;
	struct enetc_hw *enetc_hw = &hw->hw;
	pthread_mutexattr_t attr;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		eth_dev->dev_ops = &enetc4_vf_ops;
		/*
		 * Secondary process: set the fast-path burst pointers here or
		 * rte_eth_rx_burst()/rte_eth_tx_burst() would dereference NULL.
		 * hw->nc_mode lives in the shared dev_private and has already
		 * been set by the primary, so honour it to match the base BD
		 * ring layout. The RSC (LRO) and LSO (TSO) bursts use doubled
		 * 32B descriptor rings that the primary selects only at queue
		 * setup; the secondary cannot observe that choice, so it always
		 * uses the base (nc or cacheable) burst. Running the Rx/Tx
		 * datapath from a secondary is therefore not supported when LRO
		 * or TSO is enabled (see the ENETC4 NIC guide).
		 */
		if (hw->nc_mode) {
			eth_dev->rx_pkt_burst = &enetc_recv_pkts_nc;
			eth_dev->tx_pkt_burst = &enetc_xmit_pkts_nc;
		} else {
			eth_dev->rx_pkt_burst = &enetc_recv_pkts_cacheable;
			eth_dev->tx_pkt_burst = &enetc_xmit_pkts_cacheable;
		}
		return 0;
	}

	/* check if VSI messaging should be disabled via devarg */
	if (eth_dev->device->devargs) {
		struct rte_kvargs *kvlist;

		kvlist = rte_kvargs_parse(eth_dev->device->devargs->args,
					  NULL);
		if (kvlist) {
			const char *val;

			if (rte_kvargs_count(kvlist, ENETC4_VSI_DISABLE) != 0) {
				ENETC_PMD_NOTICE("VSI messaging disabled by devarg");
				eth_dev->dev_ops = &enetc4_vf_ops_no_vsi_m;
			} else {
				eth_dev->dev_ops = &enetc4_vf_ops;
			}

			/* parse optional VSI-PSI timeout devarg */
			val = rte_kvargs_get(kvlist, ENETC4_VSI_TIMEOUT);
			if (val) {
				errno = 0;
				hw->vsi_timeout = (uint32_t)strtoul(val, NULL, 0);
				if (errno != 0 || hw->vsi_timeout == 0) {
					ENETC_PMD_ERR("Invalid VSI Timeout value = %u",
							hw->vsi_timeout);
					rte_kvargs_free(kvlist);
					return -1;
				}
				ENETC_PMD_NOTICE("VSI timeout set to %u", hw->vsi_timeout);
			}

			/* parse optional VSI-PSI delay devarg */
			val = rte_kvargs_get(kvlist, ENETC4_VSI_DELAY);
			if (val) {
				errno = 0;
				hw->vsi_delay = (uint32_t)strtoul(val, NULL, 0);
				if (errno != 0 || hw->vsi_delay == 0) {
					ENETC_PMD_ERR("Invalid VSI Delay value = %u",
							hw->vsi_delay);
					rte_kvargs_free(kvlist);
					return -1;
				}
				ENETC_PMD_NOTICE("VSI delay set to %u us", hw->vsi_delay);
			}

			rte_kvargs_free(kvlist);
		} else {
			eth_dev->dev_ops = &enetc4_vf_ops;
		}
	} else {
		eth_dev->dev_ops = &enetc4_vf_ops;
	}

	enetc4_dev_hw_init(eth_dev);

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&hw->vsi_lock, &attr);
	pthread_mutexattr_destroy(&attr);

	hw->nc_mode = 0;
	enetc4_vf_get_devarg_nc(eth_dev);
	if (hw->nc_mode) {
		eth_dev->rx_pkt_burst = &enetc_recv_pkts_nc;
		eth_dev->tx_pkt_burst = &enetc_xmit_pkts_nc;
		ENETC_PMD_LOG(INFO, "nc=1: using non-cacheable BD ops (_nc)");
	}

	si_cap = enetc_rd(enetc_hw, ENETC_SICAPR0);
	hw->max_tx_queues = si_cap & ENETC_SICAPR0_BDR_MASK;
	hw->max_rx_queues = (si_cap >> 16) & ENETC_SICAPR0_BDR_MASK;

	ENETC_PMD_DEBUG("Max RX queues = %d Max TX queues = %d",
			hw->max_rx_queues, hw->max_tx_queues);
	error = enetc4_vf_mac_init(hw, eth_dev);
	if (error != 0) {
		ENETC_PMD_ERR("MAC initialization failed!!");
		return -1;
	}

	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_populate();

	/* Parse VF specific devargs (e.g. vf_link_legacy) before the first
	 * link update so that PF-to-VF link messages are interpreted using
	 * the correct (legacy or current) layout.
	 */
	enetc4_vf_get_devargs(eth_dev);

	ENETC_PMD_DEBUG("port_id %d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);
	/* update link if VSI messaging is enabled */
	if (eth_dev->dev_ops == &enetc4_vf_ops)
		enetc4_vf_link_update(eth_dev, 0);

	return 0;
}

static int
enetc4_vf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		    struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct enetc_eth_adapter),
					     enetc4_vf_dev_init);
}

int
enetc4_vf_dev_intr(struct rte_eth_dev *eth_dev, bool enable)
{
	struct enetc_eth_hw *hw =
		ENETC_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct enetc_hw *enetc_hw = &hw->hw;
	struct rte_pci_device *pci_dev = RTE_CLASS_TO_BUS_DEVICE(eth_dev, *pci_dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	if (!(intr_handle && rte_intr_fd_get(intr_handle))) {
		ENETC_PMD_ERR("No INTR handle");
		return -1;
	}
	if (enable) {
		/* if the interrupts were configured on this devices*/
		ret = rte_intr_callback_register(intr_handle,
				enetc4_dev_interrupt_handler, eth_dev);
		if (ret) {
			ENETC_PMD_ERR("Failed to register INTR callback %d", ret);
			return ret;
		}
		/* set one IRQ entry for PSI-to-VSI messaging */
		/* Vector index 0 */
		enetc_wr(enetc_hw, ENETC4_SIMSIVR, ENETC4_SI_INT_IDX);

		if (rte_intr_cap_multiple(intr_handle) &&
		    eth_dev->data->nb_rx_queues > 0) {
			uint16_t nb_rx = eth_dev->data->nb_rx_queues;
			uint16_t i;

			ret = rte_intr_efd_enable(intr_handle,
					nb_rx + ENETC4_VF_RX_VEC_BASE);
			if (ret) {
				ENETC_PMD_WARN("Failed to enable per-queue Rx eventfds: %d",
					       ret);
				hw->rxq_intr_en = 0;
			} else {
				ret = rte_intr_vec_list_alloc(intr_handle,
						"enetc4_vf_rx_intr", nb_rx);
				if (ret) {
					ENETC_PMD_WARN("Failed to alloc intr vec list: %d",
						       ret);
					rte_intr_efd_disable(intr_handle);
					hw->rxq_intr_en = 0;
				} else {
					for (i = 0; i < nb_rx; i++)
						rte_intr_vec_list_index_set(intr_handle, i,
							i + ENETC4_VF_RX_VEC_BASE);
					hw->rxq_intr_en = 1;
				}
			}
		}

		/* enable uio/vfio intr/eventfd mapping */
		ret = rte_intr_enable(intr_handle);
		if (ret) {
			ENETC_PMD_ERR("Failed to enable INTR %d", ret);
			goto intr_enable_fail;
		}

		/* Enable message received interrupt */
		enetc_vf_enable_mr_int(enetc_hw, true);
		ret = enetc4_vf_link_register_notif(eth_dev, true);
		if (ret) {
			ENETC_PMD_ERR("Failed to register link notifications %d", ret);
			goto disable;
		}

		return ret;
	}

	ret = enetc4_vf_link_register_notif(eth_dev, false);
	if (ret)
		ENETC_PMD_WARN("Failed to un-register link notification %d", ret);
disable:
	enetc_vf_enable_mr_int(enetc_hw, false);
	hw->rxq_intr_en = 0;
	ret = rte_intr_disable(intr_handle);
	if (ret)
		ENETC_PMD_WARN("Failed to disable INTR %d", ret);
intr_enable_fail:
	rte_intr_vec_list_free(intr_handle);
	rte_intr_efd_disable(intr_handle);
	ret = rte_intr_callback_unregister(intr_handle,
			enetc4_dev_interrupt_handler, eth_dev);
	if (ret < 0)
		ENETC_PMD_WARN("Failed to unregister intr callback: %d", ret);

	return ret;
}

static struct rte_pci_driver rte_enetc4_vf_pmd = {
	.id_table = pci_vf_id_enetc4_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = enetc4_vf_pci_probe,
	.remove = enetc4_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_enetc4_vf, rte_enetc4_vf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_enetc4_vf, pci_vf_id_enetc4_map);
RTE_PMD_REGISTER_KMOD_DEP(net_enetc4_vf, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_enetc4_vf,
			      ENETC4_VSI_DISABLE "=<any> "
			      ENETC4_VSI_TIMEOUT "=<uint> "
			      ENETC4_VSI_DELAY "=<uint> "
			      ENETC4_NC_MEMORY "=<int> "
			      ENETC_VF_LINK_LEGACY "=<0|1>");
RTE_LOG_REGISTER_DEFAULT(enetc4_vf_logtype_pmd, NOTICE);
