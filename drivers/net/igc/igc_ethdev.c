/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#include <stdint.h>
#include <string.h>

#include <rte_string_fns.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_alarm.h>

#include "igc_logs.h"
#include "igc_txrx.h"
#include "igc_filter.h"
#include "igc_flow.h"

#define IGC_INTEL_VENDOR_ID		0x8086

#define IGC_FC_PAUSE_TIME		0x0680
#define IGC_LINK_UPDATE_CHECK_TIMEOUT	90  /* 9s */
#define IGC_LINK_UPDATE_CHECK_INTERVAL	100 /* ms */

#define IGC_MISC_VEC_ID			RTE_INTR_VEC_ZERO_OFFSET
#define IGC_RX_VEC_START		RTE_INTR_VEC_RXTX_OFFSET
#define IGC_MSIX_OTHER_INTR_VEC		0   /* MSI-X other interrupt vector */
#define IGC_FLAG_NEED_LINK_UPDATE	(1u << 0)	/* need update link */

#define IGC_DEFAULT_RX_FREE_THRESH	32

#define IGC_DEFAULT_RX_PTHRESH		8
#define IGC_DEFAULT_RX_HTHRESH		8
#define IGC_DEFAULT_RX_WTHRESH		4

#define IGC_DEFAULT_TX_PTHRESH		8
#define IGC_DEFAULT_TX_HTHRESH		1
#define IGC_DEFAULT_TX_WTHRESH		16

/* MSI-X other interrupt vector */
#define IGC_MSIX_OTHER_INTR_VEC		0

/* External VLAN Enable bit mask */
#define IGC_CTRL_EXT_EXT_VLAN		(1u << 26)

/* Speed select */
#define IGC_CTRL_SPEED_MASK		(7u << 8)
#define IGC_CTRL_SPEED_2500		(6u << 8)

/* External VLAN Ether Type bit mask and shift */
#define IGC_VET_EXT			0xFFFF0000
#define IGC_VET_EXT_SHIFT		16

/* Force EEE Auto-negotiation */
#define IGC_EEER_EEE_FRC_AN		(1u << 28)

/* Per Queue Good Packets Received Count */
#define IGC_PQGPRC(idx)		(0x10010 + 0x100 * (idx))
/* Per Queue Good Octets Received Count */
#define IGC_PQGORC(idx)		(0x10018 + 0x100 * (idx))
/* Per Queue Good Octets Transmitted Count */
#define IGC_PQGOTC(idx)		(0x10034 + 0x100 * (idx))
/* Per Queue Multicast Packets Received Count */
#define IGC_PQMPRC(idx)		(0x10038 + 0x100 * (idx))
/* Transmit Queue Drop Packet Count */
#define IGC_TQDPC(idx)		(0xe030 + 0x40 * (idx))

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#define U32_0_IN_U64		0	/* lower bytes of u64 */
#define U32_1_IN_U64		1	/* higher bytes of u64 */
#else
#define U32_0_IN_U64		1
#define U32_1_IN_U64		0
#endif

#define IGC_ALARM_INTERVAL	8000000u
/* us, about 13.6s some per-queue registers will wrap around back to 0. */

/* Transmit and receive latency (for PTP timestamps) */
#define IGC_I225_TX_LATENCY_10		240
#define IGC_I225_TX_LATENCY_100		58
#define IGC_I225_TX_LATENCY_1000	80
#define IGC_I225_TX_LATENCY_2500	1325
#define IGC_I225_RX_LATENCY_10		6450
#define IGC_I225_RX_LATENCY_100		185
#define IGC_I225_RX_LATENCY_1000	300
#define IGC_I225_RX_LATENCY_2500	1485

uint64_t igc_tx_timestamp_dynflag;
int igc_tx_timestamp_dynfield_offset = -1;

static const struct rte_eth_desc_lim rx_desc_lim = {
	.nb_max = IGC_MAX_RXD,
	.nb_min = IGC_MIN_RXD,
	.nb_align = IGC_RXD_ALIGN,
};

static const struct rte_eth_desc_lim tx_desc_lim = {
	.nb_max = IGC_MAX_TXD,
	.nb_min = IGC_MIN_TXD,
	.nb_align = IGC_TXD_ALIGN,
	.nb_seg_max = IGC_TX_MAX_SEG,
	.nb_mtu_seg_max = IGC_TX_MAX_MTU_SEG,
};

static const struct rte_pci_id pci_id_igc_map[] = {
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_LM) },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_V)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_I)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_IT)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_K)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I226_K)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I226_LMVP)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I226_LM)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I226_V)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I226_IT)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I226_BLANK_NVM)  },
	{ .vendor_id = 0, /* sentinel */ },
};

/* store statistics names and its offset in stats structure */
struct rte_igc_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

static const struct rte_igc_xstats_name_off rte_igc_stats_strings[] = {
	{"rx_crc_errors", offsetof(struct igc_hw_stats, crcerrs)},
	{"rx_align_errors", offsetof(struct igc_hw_stats, algnerrc)},
	{"rx_errors", offsetof(struct igc_hw_stats, rxerrc)},
	{"rx_missed_packets", offsetof(struct igc_hw_stats, mpc)},
	{"tx_single_collision_packets", offsetof(struct igc_hw_stats, scc)},
	{"tx_multiple_collision_packets", offsetof(struct igc_hw_stats, mcc)},
	{"tx_excessive_collision_packets", offsetof(struct igc_hw_stats,
		ecol)},
	{"tx_late_collisions", offsetof(struct igc_hw_stats, latecol)},
	{"tx_total_collisions", offsetof(struct igc_hw_stats, colc)},
	{"tx_deferred_packets", offsetof(struct igc_hw_stats, dc)},
	{"tx_no_carrier_sense_packets", offsetof(struct igc_hw_stats, tncrs)},
	{"tx_discarded_packets", offsetof(struct igc_hw_stats, htdpmc)},
	{"rx_length_errors", offsetof(struct igc_hw_stats, rlec)},
	{"rx_xon_packets", offsetof(struct igc_hw_stats, xonrxc)},
	{"tx_xon_packets", offsetof(struct igc_hw_stats, xontxc)},
	{"rx_xoff_packets", offsetof(struct igc_hw_stats, xoffrxc)},
	{"tx_xoff_packets", offsetof(struct igc_hw_stats, xofftxc)},
	{"rx_flow_control_unsupported_packets", offsetof(struct igc_hw_stats,
		fcruc)},
	{"rx_size_64_packets", offsetof(struct igc_hw_stats, prc64)},
	{"rx_size_65_to_127_packets", offsetof(struct igc_hw_stats, prc127)},
	{"rx_size_128_to_255_packets", offsetof(struct igc_hw_stats, prc255)},
	{"rx_size_256_to_511_packets", offsetof(struct igc_hw_stats, prc511)},
	{"rx_size_512_to_1023_packets", offsetof(struct igc_hw_stats,
		prc1023)},
	{"rx_size_1024_to_max_packets", offsetof(struct igc_hw_stats,
		prc1522)},
	{"rx_broadcast_packets", offsetof(struct igc_hw_stats, bprc)},
	{"rx_multicast_packets", offsetof(struct igc_hw_stats, mprc)},
	{"rx_undersize_errors", offsetof(struct igc_hw_stats, ruc)},
	{"rx_fragment_errors", offsetof(struct igc_hw_stats, rfc)},
	{"rx_oversize_errors", offsetof(struct igc_hw_stats, roc)},
	{"rx_jabber_errors", offsetof(struct igc_hw_stats, rjc)},
	{"rx_no_buffers", offsetof(struct igc_hw_stats, rnbc)},
	{"rx_management_packets", offsetof(struct igc_hw_stats, mgprc)},
	{"rx_management_dropped", offsetof(struct igc_hw_stats, mgpdc)},
	{"tx_management_packets", offsetof(struct igc_hw_stats, mgptc)},
	{"rx_total_packets", offsetof(struct igc_hw_stats, tpr)},
	{"tx_total_packets", offsetof(struct igc_hw_stats, tpt)},
	{"rx_total_bytes", offsetof(struct igc_hw_stats, tor)},
	{"tx_total_bytes", offsetof(struct igc_hw_stats, tot)},
	{"tx_size_64_packets", offsetof(struct igc_hw_stats, ptc64)},
	{"tx_size_65_to_127_packets", offsetof(struct igc_hw_stats, ptc127)},
	{"tx_size_128_to_255_packets", offsetof(struct igc_hw_stats, ptc255)},
	{"tx_size_256_to_511_packets", offsetof(struct igc_hw_stats, ptc511)},
	{"tx_size_512_to_1023_packets", offsetof(struct igc_hw_stats,
		ptc1023)},
	{"tx_size_1023_to_max_packets", offsetof(struct igc_hw_stats,
		ptc1522)},
	{"tx_multicast_packets", offsetof(struct igc_hw_stats, mptc)},
	{"tx_broadcast_packets", offsetof(struct igc_hw_stats, bptc)},
	{"tx_tso_packets", offsetof(struct igc_hw_stats, tsctc)},
	{"rx_sent_to_host_packets", offsetof(struct igc_hw_stats, rpthc)},
	{"tx_sent_by_host_packets", offsetof(struct igc_hw_stats, hgptc)},
	{"interrupt_assert_count", offsetof(struct igc_hw_stats, iac)},
	{"rx_descriptor_lower_threshold",
		offsetof(struct igc_hw_stats, icrxdmtc)},
};

#define IGC_NB_XSTATS (sizeof(rte_igc_stats_strings) / \
		sizeof(rte_igc_stats_strings[0]))

static int eth_igc_configure(struct rte_eth_dev *dev);
static int eth_igc_link_update(struct rte_eth_dev *dev, int wait_to_complete);
static int eth_igc_stop(struct rte_eth_dev *dev);
static int eth_igc_start(struct rte_eth_dev *dev);
static int eth_igc_set_link_up(struct rte_eth_dev *dev);
static int eth_igc_set_link_down(struct rte_eth_dev *dev);
static int eth_igc_close(struct rte_eth_dev *dev);
static int eth_igc_reset(struct rte_eth_dev *dev);
static int eth_igc_promiscuous_enable(struct rte_eth_dev *dev);
static int eth_igc_promiscuous_disable(struct rte_eth_dev *dev);
static int eth_igc_fw_version_get(struct rte_eth_dev *dev,
				char *fw_version, size_t fw_size);
static int eth_igc_infos_get(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *dev_info);
static int eth_igc_led_on(struct rte_eth_dev *dev);
static int eth_igc_led_off(struct rte_eth_dev *dev);
static const uint32_t *eth_igc_supported_ptypes_get(struct rte_eth_dev *dev);
static int eth_igc_rar_set(struct rte_eth_dev *dev,
		struct rte_ether_addr *mac_addr, uint32_t index, uint32_t pool);
static void eth_igc_rar_clear(struct rte_eth_dev *dev, uint32_t index);
static int eth_igc_default_mac_addr_set(struct rte_eth_dev *dev,
			struct rte_ether_addr *addr);
static int eth_igc_set_mc_addr_list(struct rte_eth_dev *dev,
			 struct rte_ether_addr *mc_addr_set,
			 uint32_t nb_mc_addr);
static int eth_igc_allmulticast_enable(struct rte_eth_dev *dev);
static int eth_igc_allmulticast_disable(struct rte_eth_dev *dev);
static int eth_igc_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int eth_igc_stats_get(struct rte_eth_dev *dev,
			struct rte_eth_stats *rte_stats);
static int eth_igc_xstats_get(struct rte_eth_dev *dev,
			struct rte_eth_xstat *xstats, unsigned int n);
static int eth_igc_xstats_get_by_id(struct rte_eth_dev *dev,
				const uint64_t *ids,
				uint64_t *values, unsigned int n);
static int eth_igc_xstats_get_names(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				unsigned int size);
static int eth_igc_xstats_get_names_by_id(struct rte_eth_dev *dev,
		const uint64_t *ids, struct rte_eth_xstat_name *xstats_names,
		unsigned int limit);
static int eth_igc_xstats_reset(struct rte_eth_dev *dev);
static int
eth_igc_queue_stats_mapping_set(struct rte_eth_dev *dev,
	uint16_t queue_id, uint8_t stat_idx, uint8_t is_rx);
static int
eth_igc_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id);
static int
eth_igc_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id);
static int
eth_igc_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf);
static int
eth_igc_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf);
static int eth_igc_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
static int eth_igc_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size);
static int eth_igc_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf);
static int eth_igc_rss_hash_conf_get(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf);
static int
eth_igc_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on);
static int eth_igc_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int eth_igc_vlan_tpid_set(struct rte_eth_dev *dev,
		      enum rte_vlan_type vlan_type, uint16_t tpid);
static int eth_igc_timesync_enable(struct rte_eth_dev *dev);
static int eth_igc_timesync_disable(struct rte_eth_dev *dev);
static int eth_igc_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
					  struct timespec *timestamp,
					  uint32_t flags);
static int eth_igc_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
					  struct timespec *timestamp);
static int eth_igc_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta);
static int eth_igc_timesync_read_time(struct rte_eth_dev *dev,
				  struct timespec *timestamp);
static int eth_igc_timesync_write_time(struct rte_eth_dev *dev,
				   const struct timespec *timestamp);
static int eth_igc_read_clock(struct rte_eth_dev *dev, uint64_t *clock);

static const struct eth_dev_ops eth_igc_ops = {
	.dev_configure		= eth_igc_configure,
	.link_update		= eth_igc_link_update,
	.dev_stop		= eth_igc_stop,
	.dev_start		= eth_igc_start,
	.dev_close		= eth_igc_close,
	.dev_reset		= eth_igc_reset,
	.dev_set_link_up	= eth_igc_set_link_up,
	.dev_set_link_down	= eth_igc_set_link_down,
	.promiscuous_enable	= eth_igc_promiscuous_enable,
	.promiscuous_disable	= eth_igc_promiscuous_disable,
	.allmulticast_enable	= eth_igc_allmulticast_enable,
	.allmulticast_disable	= eth_igc_allmulticast_disable,
	.fw_version_get		= eth_igc_fw_version_get,
	.dev_infos_get		= eth_igc_infos_get,
	.dev_led_on		= eth_igc_led_on,
	.dev_led_off		= eth_igc_led_off,
	.dev_supported_ptypes_get = eth_igc_supported_ptypes_get,
	.mtu_set		= eth_igc_mtu_set,
	.mac_addr_add		= eth_igc_rar_set,
	.mac_addr_remove	= eth_igc_rar_clear,
	.mac_addr_set		= eth_igc_default_mac_addr_set,
	.set_mc_addr_list	= eth_igc_set_mc_addr_list,

	.rx_queue_setup		= eth_igc_rx_queue_setup,
	.rx_queue_release	= eth_igc_rx_queue_release,
	.tx_queue_setup		= eth_igc_tx_queue_setup,
	.tx_queue_release	= eth_igc_tx_queue_release,
	.tx_done_cleanup	= eth_igc_tx_done_cleanup,
	.rxq_info_get		= eth_igc_rxq_info_get,
	.txq_info_get		= eth_igc_txq_info_get,
	.stats_get		= eth_igc_stats_get,
	.xstats_get		= eth_igc_xstats_get,
	.xstats_get_by_id	= eth_igc_xstats_get_by_id,
	.xstats_get_names_by_id	= eth_igc_xstats_get_names_by_id,
	.xstats_get_names	= eth_igc_xstats_get_names,
	.stats_reset		= eth_igc_xstats_reset,
	.xstats_reset		= eth_igc_xstats_reset,
	.queue_stats_mapping_set = eth_igc_queue_stats_mapping_set,
	.rx_queue_intr_enable	= eth_igc_rx_queue_intr_enable,
	.rx_queue_intr_disable	= eth_igc_rx_queue_intr_disable,
	.flow_ctrl_get		= eth_igc_flow_ctrl_get,
	.flow_ctrl_set		= eth_igc_flow_ctrl_set,
	.reta_update		= eth_igc_rss_reta_update,
	.reta_query		= eth_igc_rss_reta_query,
	.rss_hash_update	= eth_igc_rss_hash_update,
	.rss_hash_conf_get	= eth_igc_rss_hash_conf_get,
	.vlan_filter_set	= eth_igc_vlan_filter_set,
	.vlan_offload_set	= eth_igc_vlan_offload_set,
	.vlan_tpid_set		= eth_igc_vlan_tpid_set,
	.vlan_strip_queue_set	= eth_igc_vlan_strip_queue_set,
	.flow_ops_get		= eth_igc_flow_ops_get,
	.timesync_enable	= eth_igc_timesync_enable,
	.timesync_disable	= eth_igc_timesync_disable,
	.timesync_read_rx_timestamp = eth_igc_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp = eth_igc_timesync_read_tx_timestamp,
	.timesync_adjust_time	= eth_igc_timesync_adjust_time,
	.timesync_read_time	= eth_igc_timesync_read_time,
	.timesync_write_time	= eth_igc_timesync_write_time,
	.read_clock             = eth_igc_read_clock,
};

/*
 * multiple queue mode checking
 */
static int
igc_check_mq_mode(struct rte_eth_dev *dev)
{
	enum rte_eth_rx_mq_mode rx_mq_mode = dev->data->dev_conf.rxmode.mq_mode;
	enum rte_eth_tx_mq_mode tx_mq_mode = dev->data->dev_conf.txmode.mq_mode;

	if (RTE_ETH_DEV_SRIOV(dev).active != 0) {
		PMD_INIT_LOG(ERR, "SRIOV is not supported.");
		return -EINVAL;
	}

	if (rx_mq_mode != RTE_ETH_MQ_RX_NONE &&
		rx_mq_mode != RTE_ETH_MQ_RX_RSS) {
		/* RSS together with VMDq not supported*/
		PMD_INIT_LOG(ERR, "RX mode %d is not supported.",
				rx_mq_mode);
		return -EINVAL;
	}

	/* To no break software that set invalid mode, only display
	 * warning if invalid mode is used.
	 */
	if (tx_mq_mode != RTE_ETH_MQ_TX_NONE)
		PMD_INIT_LOG(WARNING,
			"TX mode %d is not supported. Due to meaningless in this driver, just ignore",
			tx_mq_mode);

	return 0;
}

static int
eth_igc_configure(struct rte_eth_dev *dev)
{
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	ret  = igc_check_mq_mode(dev);
	if (ret != 0)
		return ret;

	intr->flags |= IGC_FLAG_NEED_LINK_UPDATE;
	return 0;
}

static int
eth_igc_set_link_up(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	if (hw->phy.media_type == igc_media_type_copper)
		igc_power_up_phy(hw);
	else
		igc_power_up_fiber_serdes_link(hw);
	return 0;
}

static int
eth_igc_set_link_down(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	if (hw->phy.media_type == igc_media_type_copper)
		igc_power_down_phy(hw);
	else
		igc_shutdown_fiber_serdes_link(hw);
	return 0;
}

/*
 * disable other interrupt
 */
static void
igc_intr_other_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	if (rte_intr_allow_others(intr_handle) &&
		dev->data->dev_conf.intr_conf.lsc) {
		IGC_WRITE_REG(hw, IGC_EIMC, 1u << IGC_MSIX_OTHER_INTR_VEC);
	}

	IGC_WRITE_REG(hw, IGC_IMC, ~0);
	IGC_WRITE_FLUSH(hw);
}

/*
 * enable other interrupt
 */
static inline void
igc_intr_other_enable(struct rte_eth_dev *dev)
{
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	if (rte_intr_allow_others(intr_handle) &&
		dev->data->dev_conf.intr_conf.lsc) {
		IGC_WRITE_REG(hw, IGC_EIMS, 1u << IGC_MSIX_OTHER_INTR_VEC);
	}

	IGC_WRITE_REG(hw, IGC_IMS, intr->mask);
	IGC_WRITE_FLUSH(hw);
}

/*
 * It reads ICR and gets interrupt causes, check it and set a bit flag
 * to update link status.
 */
static void
eth_igc_interrupt_get_status(struct rte_eth_dev *dev)
{
	uint32_t icr;
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);

	/* read-on-clear nic registers here */
	icr = IGC_READ_REG(hw, IGC_ICR);

	intr->flags = 0;
	if (icr & IGC_ICR_LSC)
		intr->flags |= IGC_FLAG_NEED_LINK_UPDATE;
}

/* return 0 means link status changed, -1 means not changed */
static int
eth_igc_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_eth_link link;
	int link_check, count;

	link_check = 0;
	hw->mac.get_link_status = 1;

	/* possible wait-to-complete in up to 9 seconds */
	for (count = 0; count < IGC_LINK_UPDATE_CHECK_TIMEOUT; count++) {
		/* Read the real link status */
		switch (hw->phy.media_type) {
		case igc_media_type_copper:
			/* Do the work to read phy */
			igc_check_for_link(hw);
			link_check = !hw->mac.get_link_status;
			break;

		case igc_media_type_fiber:
			igc_check_for_link(hw);
			link_check = (IGC_READ_REG(hw, IGC_STATUS) &
				      IGC_STATUS_LU);
			break;

		case igc_media_type_internal_serdes:
			igc_check_for_link(hw);
			link_check = hw->mac.serdes_has_link;
			break;

		default:
			break;
		}
		if (link_check || wait_to_complete == 0)
			break;
		rte_delay_ms(IGC_LINK_UPDATE_CHECK_INTERVAL);
	}
	memset(&link, 0, sizeof(link));

	/* Now we check if a transition has happened */
	if (link_check) {
		uint16_t duplex, speed;
		hw->mac.ops.get_link_up_info(hw, &speed, &duplex);
		link.link_duplex = (duplex == FULL_DUPLEX) ?
				RTE_ETH_LINK_FULL_DUPLEX :
				RTE_ETH_LINK_HALF_DUPLEX;
		link.link_speed = speed;
		link.link_status = RTE_ETH_LINK_UP;
		link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				RTE_ETH_LINK_SPEED_FIXED);

		if (speed == SPEED_2500) {
			uint32_t tipg = IGC_READ_REG(hw, IGC_TIPG);
			if ((tipg & IGC_TIPG_IPGT_MASK) != 0x0b) {
				tipg &= ~IGC_TIPG_IPGT_MASK;
				tipg |= 0x0b;
				IGC_WRITE_REG(hw, IGC_TIPG, tipg);
			}
		}
	} else {
		link.link_speed = 0;
		link.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
		link.link_status = RTE_ETH_LINK_DOWN;
		link.link_autoneg = RTE_ETH_LINK_FIXED;
	}

	return rte_eth_linkstatus_set(dev, &link);
}

/*
 * It executes link_update after knowing an interrupt is present.
 */
static void
eth_igc_interrupt_action(struct rte_eth_dev *dev)
{
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_eth_link link;
	int ret;

	if (intr->flags & IGC_FLAG_NEED_LINK_UPDATE) {
		intr->flags &= ~IGC_FLAG_NEED_LINK_UPDATE;

		/* set get_link_status to check register later */
		ret = eth_igc_link_update(dev, 0);

		/* check if link has changed */
		if (ret < 0)
			return;

		rte_eth_linkstatus_get(dev, &link);
		if (link.link_status)
			PMD_DRV_LOG(INFO,
				" Port %d: Link Up - speed %u Mbps - %s",
				dev->data->port_id,
				(unsigned int)link.link_speed,
				link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX ?
				"full-duplex" : "half-duplex");
		else
			PMD_DRV_LOG(INFO, " Port %d: Link Down",
				dev->data->port_id);

		PMD_DRV_LOG(DEBUG, "PCI Address: " PCI_PRI_FMT,
				pci_dev->addr.domain,
				pci_dev->addr.bus,
				pci_dev->addr.devid,
				pci_dev->addr.function);
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
	}
}

/*
 * Interrupt handler which shall be registered at first.
 *
 * @handle
 *  Pointer to interrupt handle.
 * @param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 */
static void
eth_igc_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	eth_igc_interrupt_get_status(dev);
	eth_igc_interrupt_action(dev);
}

static void igc_read_queue_stats_register(struct rte_eth_dev *dev);

/*
 * Update the queue status every IGC_ALARM_INTERVAL time.
 * @param
 *  The address of parameter (struct rte_eth_dev *) registered before.
 */
static void
igc_update_queue_stats_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	igc_read_queue_stats_register(dev);
	rte_eal_alarm_set(IGC_ALARM_INTERVAL,
			igc_update_queue_stats_handler, dev);
}

/*
 * rx,tx enable/disable
 */
static void
eth_igc_rxtx_control(struct rte_eth_dev *dev, bool enable)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t tctl, rctl;

	tctl = IGC_READ_REG(hw, IGC_TCTL);
	rctl = IGC_READ_REG(hw, IGC_RCTL);

	if (enable) {
		/* enable Tx/Rx */
		tctl |= IGC_TCTL_EN;
		rctl |= IGC_RCTL_EN;
	} else {
		/* disable Tx/Rx */
		tctl &= ~IGC_TCTL_EN;
		rctl &= ~IGC_RCTL_EN;
	}
	IGC_WRITE_REG(hw, IGC_TCTL, tctl);
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	IGC_WRITE_FLUSH(hw);
}

/*
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC.
 */
static int
eth_igc_stop(struct rte_eth_dev *dev)
{
	struct igc_adapter *adapter = IGC_DEV_PRIVATE(dev);
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct rte_eth_link link;

	dev->data->dev_started = 0;
	adapter->stopped = 1;

	/* disable receive and transmit */
	eth_igc_rxtx_control(dev, false);

	/* disable all MSI-X interrupts */
	IGC_WRITE_REG(hw, IGC_EIMC, 0x1f);
	IGC_WRITE_FLUSH(hw);

	/* clear all MSI-X interrupts */
	IGC_WRITE_REG(hw, IGC_EICR, 0x1f);

	igc_intr_other_disable(dev);

	rte_eal_alarm_cancel(igc_update_queue_stats_handler, dev);

	/* disable intr eventfd mapping */
	rte_intr_disable(intr_handle);

	igc_reset_hw(hw);

	/* disable all wake up */
	IGC_WRITE_REG(hw, IGC_WUC, 0);

	/* disable checking EEE operation in MAC loopback mode */
	igc_read_reg_check_clear_bits(hw, IGC_EEER, IGC_EEER_EEE_FRC_AN);

	/* Set bit for Go Link disconnect */
	igc_read_reg_check_set_bits(hw, IGC_82580_PHY_POWER_MGMT,
			IGC_82580_PM_GO_LINKD);

	/* Power down the phy. Needed to make the link go Down */
	eth_igc_set_link_down(dev);

	igc_dev_clear_queues(dev);

	/* clear the recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(dev, &link);

	if (!rte_intr_allow_others(intr_handle))
		/* resume to the default handler */
		rte_intr_callback_register(intr_handle,
					   eth_igc_interrupt_handler,
					   (void *)dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	rte_intr_vec_list_free(intr_handle);

	return 0;
}

/*
 * write interrupt vector allocation register
 * @hw
 *  board private structure
 * @queue_index
 *  queue index, valid 0,1,2,3
 * @tx
 *  tx:1, rx:0
 * @msix_vector
 *  msix-vector, valid 0,1,2,3,4
 */
static void
igc_write_ivar(struct igc_hw *hw, uint8_t queue_index,
		bool tx, uint8_t msix_vector)
{
	uint8_t offset = 0;
	uint8_t reg_index = queue_index >> 1;
	uint32_t val;

	/*
	 * IVAR(0)
	 * bit31...24	bit23...16	bit15...8	bit7...0
	 * TX1		RX1		TX0		RX0
	 *
	 * IVAR(1)
	 * bit31...24	bit23...16	bit15...8	bit7...0
	 * TX3		RX3		TX2		RX2
	 */

	if (tx)
		offset = 8;

	if (queue_index & 1)
		offset += 16;

	val = IGC_READ_REG_ARRAY(hw, IGC_IVAR0, reg_index);

	/* clear bits */
	val &= ~((uint32_t)0xFF << offset);

	/* write vector and valid bit */
	val |= (uint32_t)(msix_vector | IGC_IVAR_VALID) << offset;

	IGC_WRITE_REG_ARRAY(hw, IGC_IVAR0, reg_index, val);
}

/* Sets up the hardware to generate MSI-X interrupts properly
 * @hw
 *  board private structure
 */
static void
igc_configure_msix_intr(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;

	uint32_t intr_mask;
	uint32_t vec = IGC_MISC_VEC_ID;
	uint32_t base = IGC_MISC_VEC_ID;
	uint32_t misc_shift = 0;
	int i, nb_efd;

	/* won't configure msix register if no mapping is done
	 * between intr vector and event fd
	 */
	if (!rte_intr_dp_is_en(intr_handle))
		return;

	if (rte_intr_allow_others(intr_handle)) {
		base = IGC_RX_VEC_START;
		vec = base;
		misc_shift = 1;
	}

	/* turn on MSI-X capability first */
	IGC_WRITE_REG(hw, IGC_GPIE, IGC_GPIE_MSIX_MODE |
				IGC_GPIE_PBA | IGC_GPIE_EIAME |
				IGC_GPIE_NSICR);

	nb_efd = rte_intr_nb_efd_get(intr_handle);
	if (nb_efd < 0)
		return;

	intr_mask = RTE_LEN2MASK(nb_efd, uint32_t) << misc_shift;

	if (dev->data->dev_conf.intr_conf.lsc)
		intr_mask |= (1u << IGC_MSIX_OTHER_INTR_VEC);

	/* enable msix auto-clear */
	igc_read_reg_check_set_bits(hw, IGC_EIAC, intr_mask);

	/* set other cause interrupt vector */
	igc_read_reg_check_set_bits(hw, IGC_IVAR_MISC,
		(uint32_t)(IGC_MSIX_OTHER_INTR_VEC | IGC_IVAR_VALID) << 8);

	/* enable auto-mask */
	igc_read_reg_check_set_bits(hw, IGC_EIAM, intr_mask);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		igc_write_ivar(hw, i, 0, vec);
		rte_intr_vec_list_index_set(intr_handle, i, vec);
		if (vec < base + rte_intr_nb_efd_get(intr_handle) - 1)
			vec++;
	}

	IGC_WRITE_FLUSH(hw);
}

/**
 * It enables the interrupt mask and then enable the interrupt.
 *
 * @dev
 *  Pointer to struct rte_eth_dev.
 * @on
 *  Enable or Disable
 */
static void
igc_lsc_interrupt_setup(struct rte_eth_dev *dev, uint8_t on)
{
	struct igc_interrupt *intr = IGC_DEV_PRIVATE_INTR(dev);

	if (on)
		intr->mask |= IGC_ICR_LSC;
	else
		intr->mask &= ~IGC_ICR_LSC;
}

/*
 * It enables the interrupt.
 * It will be called once only during nic initialized.
 */
static void
igc_rxq_interrupt_setup(struct rte_eth_dev *dev)
{
	uint32_t mask;
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	int misc_shift = rte_intr_allow_others(intr_handle) ? 1 : 0;
	int nb_efd;

	/* won't configure msix register if no mapping is done
	 * between intr vector and event fd
	 */
	if (!rte_intr_dp_is_en(intr_handle))
		return;

	nb_efd = rte_intr_nb_efd_get(intr_handle);
	if (nb_efd < 0)
		return;

	mask = RTE_LEN2MASK(nb_efd, uint32_t) << misc_shift;
	IGC_WRITE_REG(hw, IGC_EIMS, mask);
}

/*
 *  Get hardware rx-buffer size.
 */
static inline int
igc_get_rx_buffer_size(struct igc_hw *hw)
{
	return (IGC_READ_REG(hw, IGC_RXPBS) & 0x3f) << 10;
}

/*
 * igc_hw_control_acquire sets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means
 * that the driver is loaded.
 */
static void
igc_hw_control_acquire(struct igc_hw *hw)
{
	uint32_t ctrl_ext;

	/* Let firmware know the driver has taken over */
	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);
	IGC_WRITE_REG(hw, IGC_CTRL_EXT, ctrl_ext | IGC_CTRL_EXT_DRV_LOAD);
}

/*
 * igc_hw_control_release resets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that the
 * driver is no longer loaded.
 */
static void
igc_hw_control_release(struct igc_hw *hw)
{
	uint32_t ctrl_ext;

	/* Let firmware taken over control of h/w */
	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);
	IGC_WRITE_REG(hw, IGC_CTRL_EXT,
			ctrl_ext & ~IGC_CTRL_EXT_DRV_LOAD);
}

static int
igc_hardware_init(struct igc_hw *hw)
{
	uint32_t rx_buf_size;
	int diag;

	/* Let the firmware know the OS is in control */
	igc_hw_control_acquire(hw);

	/* Issue a global reset */
	igc_reset_hw(hw);

	/* disable all wake up */
	IGC_WRITE_REG(hw, IGC_WUC, 0);

	/*
	 * Hardware flow control
	 * - High water mark should allow for at least two standard size (1518)
	 *   frames to be received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit. Here we use an arbitrary value of 1500 which will
	 *   restart after one full frame is pulled from the buffer. There
	 *   could be several smaller frames in the buffer and if so they will
	 *   not trigger the XON until their total number reduces the buffer
	 *   by 1500.
	 */
	rx_buf_size = igc_get_rx_buffer_size(hw);
	hw->fc.high_water = rx_buf_size - (RTE_ETHER_MAX_LEN * 2);
	hw->fc.low_water = hw->fc.high_water - 1500;
	hw->fc.pause_time = IGC_FC_PAUSE_TIME;
	hw->fc.send_xon = 1;
	hw->fc.requested_mode = igc_fc_full;

	diag = igc_init_hw(hw);
	if (diag < 0)
		return diag;

	igc_get_phy_info(hw);
	igc_check_for_link(hw);

	return 0;
}

static int
eth_igc_start(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_adapter *adapter = IGC_DEV_PRIVATE(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint32_t nsec, sec, baset_l, baset_h, tqavctrl;
	struct timespec system_time;
	int64_t n, systime;
	uint32_t txqctl = 0;
	uint32_t *speeds;
	uint16_t i;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* disable all MSI-X interrupts */
	IGC_WRITE_REG(hw, IGC_EIMC, 0x1f);
	IGC_WRITE_FLUSH(hw);

	/* clear all MSI-X interrupts */
	IGC_WRITE_REG(hw, IGC_EICR, 0x1f);

	/* disable uio/vfio intr/eventfd mapping */
	if (!adapter->stopped)
		rte_intr_disable(intr_handle);

	/* Power up the phy. Needed to make the link go Up */
	eth_igc_set_link_up(dev);

	/* Put the address into the Receive Address Array */
	igc_rar_set(hw, hw->mac.addr, 0);

	/* Initialize the hardware */
	if (igc_hardware_init(hw)) {
		PMD_DRV_LOG(ERR, "Unable to initialize the hardware");
		return -EIO;
	}
	adapter->stopped = 0;

	/* check and configure queue intr-vector mapping */
	if (rte_intr_cap_multiple(intr_handle) &&
		dev->data->dev_conf.intr_conf.rxq) {
		uint32_t intr_vector = dev->data->nb_rx_queues;
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle)) {
		if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
						  dev->data->nb_rx_queues)) {
			PMD_DRV_LOG(ERR,
				"Failed to allocate %d rx_queues intr_vec",
				dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}

	/* configure msix for rx interrupt */
	igc_configure_msix_intr(dev);

	igc_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	ret = igc_rx_init(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Unable to initialize RX hardware");
		igc_dev_clear_queues(dev);
		return ret;
	}

	if (igc_tx_timestamp_dynflag > 0) {
		adapter->base_time = 0;
		adapter->cycle_time = NSEC_PER_SEC;

		IGC_WRITE_REG(hw, IGC_TSSDP, 0);
		IGC_WRITE_REG(hw, IGC_TSIM, TSINTR_TXTS);
		IGC_WRITE_REG(hw, IGC_IMS, IGC_ICR_TS);

		IGC_WRITE_REG(hw, IGC_TSAUXC, 0);
		IGC_WRITE_REG(hw, IGC_I350_DTXMXPKTSZ, IGC_DTXMXPKTSZ_TSN);
		IGC_WRITE_REG(hw, IGC_TXPBS, IGC_TXPBSIZE_TSN);

		tqavctrl = IGC_READ_REG(hw, IGC_I210_TQAVCTRL);
		tqavctrl |= IGC_TQAVCTRL_TRANSMIT_MODE_TSN |
			    IGC_TQAVCTRL_ENHANCED_QAV;
		IGC_WRITE_REG(hw, IGC_I210_TQAVCTRL, tqavctrl);

		IGC_WRITE_REG(hw, IGC_QBVCYCLET_S, adapter->cycle_time);
		IGC_WRITE_REG(hw, IGC_QBVCYCLET, adapter->cycle_time);

		for (i = 0; i < dev->data->nb_tx_queues; i++) {
			IGC_WRITE_REG(hw, IGC_STQT(i), 0);
			IGC_WRITE_REG(hw, IGC_ENDQT(i), NSEC_PER_SEC);

			txqctl |= IGC_TXQCTL_QUEUE_MODE_LAUNCHT;
			IGC_WRITE_REG(hw, IGC_TXQCTL(i), txqctl);
		}

		clock_gettime(CLOCK_REALTIME, &system_time);
		IGC_WRITE_REG(hw, IGC_SYSTIML, system_time.tv_nsec);
		IGC_WRITE_REG(hw, IGC_SYSTIMH, system_time.tv_sec);

		nsec = IGC_READ_REG(hw, IGC_SYSTIML);
		sec = IGC_READ_REG(hw, IGC_SYSTIMH);
		systime = (int64_t)sec * NSEC_PER_SEC + (int64_t)nsec;

		if (systime > adapter->base_time) {
			n = (systime - adapter->base_time) /
			     adapter->cycle_time;
			adapter->base_time = adapter->base_time +
				(n + 1) * adapter->cycle_time;
		}

		baset_h = adapter->base_time / NSEC_PER_SEC;
		baset_l = adapter->base_time % NSEC_PER_SEC;
		IGC_WRITE_REG(hw, IGC_BASET_H, baset_h);
		IGC_WRITE_REG(hw, IGC_BASET_L, baset_l);
	}

	igc_clear_hw_cntrs_base_generic(hw);

	/* VLAN Offload Settings */
	eth_igc_vlan_offload_set(dev,
		RTE_ETH_VLAN_STRIP_MASK | RTE_ETH_VLAN_FILTER_MASK |
		RTE_ETH_VLAN_EXTEND_MASK);

	/* Setup link speed and duplex */
	speeds = &dev->data->dev_conf.link_speeds;
	if (*speeds == RTE_ETH_LINK_SPEED_AUTONEG) {
		hw->phy.autoneg_advertised = IGC_ALL_SPEED_DUPLEX_2500;
		hw->mac.autoneg = 1;
	} else {
		int num_speeds = 0;

		if (*speeds & RTE_ETH_LINK_SPEED_FIXED) {
			PMD_DRV_LOG(ERR,
				    "Force speed mode currently not supported");
			igc_dev_clear_queues(dev);
			return -EINVAL;
		}

		hw->phy.autoneg_advertised = 0;
		hw->mac.autoneg = 1;

		if (*speeds & ~(RTE_ETH_LINK_SPEED_10M_HD | RTE_ETH_LINK_SPEED_10M |
				RTE_ETH_LINK_SPEED_100M_HD | RTE_ETH_LINK_SPEED_100M |
				RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_2_5G)) {
			num_speeds = -1;
			goto error_invalid_config;
		}
		if (*speeds & RTE_ETH_LINK_SPEED_10M_HD) {
			hw->phy.autoneg_advertised |= ADVERTISE_10_HALF;
			num_speeds++;
		}
		if (*speeds & RTE_ETH_LINK_SPEED_10M) {
			hw->phy.autoneg_advertised |= ADVERTISE_10_FULL;
			num_speeds++;
		}
		if (*speeds & RTE_ETH_LINK_SPEED_100M_HD) {
			hw->phy.autoneg_advertised |= ADVERTISE_100_HALF;
			num_speeds++;
		}
		if (*speeds & RTE_ETH_LINK_SPEED_100M) {
			hw->phy.autoneg_advertised |= ADVERTISE_100_FULL;
			num_speeds++;
		}
		if (*speeds & RTE_ETH_LINK_SPEED_1G) {
			hw->phy.autoneg_advertised |= ADVERTISE_1000_FULL;
			num_speeds++;
		}
		if (*speeds & RTE_ETH_LINK_SPEED_2_5G) {
			hw->phy.autoneg_advertised |= ADVERTISE_2500_FULL;
			num_speeds++;
		}
		if (num_speeds == 0)
			goto error_invalid_config;
	}

	igc_setup_link(hw);

	if (rte_intr_allow_others(intr_handle)) {
		/* check if lsc interrupt is enabled */
		if (dev->data->dev_conf.intr_conf.lsc)
			igc_lsc_interrupt_setup(dev, 1);
		else
			igc_lsc_interrupt_setup(dev, 0);
	} else {
		rte_intr_callback_unregister(intr_handle,
					     eth_igc_interrupt_handler,
					     (void *)dev);
		if (dev->data->dev_conf.intr_conf.lsc)
			PMD_DRV_LOG(INFO,
				"LSC won't enable because of no intr multiplex");
	}

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(intr_handle);

	rte_eal_alarm_set(IGC_ALARM_INTERVAL,
			igc_update_queue_stats_handler, dev);

	/* check if rxq interrupt is enabled */
	if (dev->data->dev_conf.intr_conf.rxq &&
			rte_intr_dp_is_en(intr_handle))
		igc_rxq_interrupt_setup(dev);

	/* resume enabled intr since hw reset */
	igc_intr_other_enable(dev);

	eth_igc_rxtx_control(dev, true);
	eth_igc_link_update(dev, 0);

	/* configure MAC-loopback mode */
	if (dev->data->dev_conf.lpbk_mode == 1) {
		uint32_t reg_val;

		reg_val = IGC_READ_REG(hw, IGC_CTRL);
		reg_val &= ~IGC_CTRL_SPEED_MASK;
		reg_val |= IGC_CTRL_SLU | IGC_CTRL_FRCSPD |
			IGC_CTRL_FRCDPX | IGC_CTRL_FD | IGC_CTRL_SPEED_2500;
		IGC_WRITE_REG(hw, IGC_CTRL, reg_val);

		igc_read_reg_check_set_bits(hw, IGC_EEER, IGC_EEER_EEE_FRC_AN);
	}

	return 0;

error_invalid_config:
	PMD_DRV_LOG(ERR, "Invalid advertised speeds (%u) for port %u",
		     dev->data->dev_conf.link_speeds, dev->data->port_id);
	igc_dev_clear_queues(dev);
	return -EINVAL;
}

static int
igc_reset_swfw_lock(struct igc_hw *hw)
{
	int ret_val;

	/*
	 * Do mac ops initialization manually here, since we will need
	 * some function pointers set by this call.
	 */
	ret_val = igc_init_mac_params(hw);
	if (ret_val)
		return ret_val;

	/*
	 * SMBI lock should not fail in this early stage. If this is the case,
	 * it is due to an improper exit of the application.
	 * So force the release of the faulty lock.
	 */
	if (igc_get_hw_semaphore_generic(hw) < 0)
		PMD_DRV_LOG(DEBUG, "SMBI lock released");

	igc_put_hw_semaphore_generic(hw);

	if (hw->mac.ops.acquire_swfw_sync != NULL) {
		uint16_t mask;

		/*
		 * Phy lock should not fail in this early stage.
		 * If this is the case, it is due to an improper exit of the
		 * application. So force the release of the faulty lock.
		 */
		mask = IGC_SWFW_PHY0_SM;
		if (hw->mac.ops.acquire_swfw_sync(hw, mask) < 0) {
			PMD_DRV_LOG(DEBUG, "SWFW phy%d lock released",
				    hw->bus.func);
		}
		hw->mac.ops.release_swfw_sync(hw, mask);

		/*
		 * This one is more tricky since it is common to all ports; but
		 * swfw_sync retries last long enough (1s) to be almost sure
		 * that if lock can not be taken it is due to an improper lock
		 * of the semaphore.
		 */
		mask = IGC_SWFW_EEP_SM;
		if (hw->mac.ops.acquire_swfw_sync(hw, mask) < 0)
			PMD_DRV_LOG(DEBUG, "SWFW common locks released");

		hw->mac.ops.release_swfw_sync(hw, mask);
	}

	return IGC_SUCCESS;
}

/*
 * free all rx/tx queues.
 */
static void
igc_dev_free_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		eth_igc_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		eth_igc_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

static int
eth_igc_close(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_adapter *adapter = IGC_DEV_PRIVATE(dev);
	int retry = 0;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!adapter->stopped)
		ret = eth_igc_stop(dev);

	igc_flow_flush(dev, NULL);
	igc_clear_all_filter(dev);

	igc_intr_other_disable(dev);
	do {
		int ret = rte_intr_callback_unregister(intr_handle,
				eth_igc_interrupt_handler, dev);
		if (ret >= 0 || ret == -ENOENT || ret == -EINVAL)
			break;

		PMD_DRV_LOG(ERR, "intr callback unregister failed: %d", ret);
		DELAY(200 * 1000); /* delay 200ms */
	} while (retry++ < 5);

	igc_phy_hw_reset(hw);
	igc_hw_control_release(hw);
	igc_dev_free_queues(dev);

	/* Reset any pending lock */
	igc_reset_swfw_lock(hw);

	return ret;
}

static void
igc_identify_hardware(struct rte_eth_dev *dev, struct rte_pci_device *pci_dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
}

static int
eth_igc_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	int i, error = 0;

	PMD_INIT_FUNC_TRACE();
	dev->dev_ops = &eth_igc_ops;
	dev->rx_queue_count = eth_igc_rx_queue_count;
	dev->rx_descriptor_status = eth_igc_rx_descriptor_status;
	dev->tx_descriptor_status = eth_igc_tx_descriptor_status;

	/*
	 * for secondary processes, we don't initialize any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		dev->rx_pkt_burst = igc_recv_pkts;
		if (dev->data->scattered_rx)
			dev->rx_pkt_burst = igc_recv_scattered_pkts;

		dev->tx_pkt_burst = igc_xmit_pkts;
		dev->tx_pkt_prepare = eth_igc_prep_pkts;
		return 0;
	}

	rte_eth_copy_pci_info(dev, pci_dev);
	dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	hw->back = pci_dev;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;

	igc_identify_hardware(dev, pci_dev);
	if (igc_setup_init_funcs(hw, false) != IGC_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	igc_get_bus_info(hw);

	/* Reset any pending lock */
	if (igc_reset_swfw_lock(hw) != IGC_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	/* Finish initialization */
	if (igc_setup_init_funcs(hw, true) != IGC_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	hw->mac.autoneg = 1;
	hw->phy.autoneg_wait_to_complete = 0;
	hw->phy.autoneg_advertised = IGC_ALL_SPEED_DUPLEX_2500;

	/* Copper options */
	if (hw->phy.media_type == igc_media_type_copper) {
		hw->phy.mdix = 0; /* AUTO_ALL_MODES */
		hw->phy.disable_polarity_correction = 0;
		hw->phy.ms_type = igc_ms_hw_default;
	}

	/*
	 * Start from a known state, this is important in reading the nvm
	 * and mac from that.
	 */
	igc_reset_hw(hw);

	/* Make sure we have a good EEPROM before we read from it */
	if (igc_validate_nvm_checksum(hw) < 0) {
		/*
		 * Some PCI-E parts fail the first check due to
		 * the link being in sleep state, call it again,
		 * if it fails a second time its a real issue.
		 */
		if (igc_validate_nvm_checksum(hw) < 0) {
			PMD_INIT_LOG(ERR, "EEPROM checksum invalid");
			error = -EIO;
			goto err_late;
		}
	}

	/* Read the permanent MAC address out of the EEPROM */
	if (igc_read_mac_addr(hw) != 0) {
		PMD_INIT_LOG(ERR, "EEPROM error while reading MAC address");
		error = -EIO;
		goto err_late;
	}

	/* Allocate memory for storing MAC addresses */
	dev->data->mac_addrs = rte_zmalloc("igc",
		RTE_ETHER_ADDR_LEN * hw->mac.rar_entry_count, 0);
	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes for storing MAC",
				RTE_ETHER_ADDR_LEN * hw->mac.rar_entry_count);
		error = -ENOMEM;
		goto err_late;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.addr,
			&dev->data->mac_addrs[0]);

	/* Now initialize the hardware */
	if (igc_hardware_init(hw) != 0) {
		PMD_INIT_LOG(ERR, "Hardware initialization failed");
		rte_free(dev->data->mac_addrs);
		dev->data->mac_addrs = NULL;
		error = -ENODEV;
		goto err_late;
	}

	hw->mac.get_link_status = 1;
	igc->stopped = 0;

	/* Indicate SOL/IDER usage */
	if (igc_check_reset_block(hw) < 0)
		PMD_INIT_LOG(ERR,
			"PHY reset is blocked due to SOL/IDER session.");

	PMD_INIT_LOG(DEBUG, "port_id %d vendorID=0x%x deviceID=0x%x",
			dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);

	rte_intr_callback_register(pci_dev->intr_handle,
			eth_igc_interrupt_handler, (void *)dev);

	/* enable uio/vfio intr/eventfd mapping */
	rte_intr_enable(pci_dev->intr_handle);

	/* enable support intr */
	igc_intr_other_enable(dev);

	/* initiate queue status */
	for (i = 0; i < IGC_QUEUE_PAIRS_NUM; i++) {
		igc->txq_stats_map[i] = -1;
		igc->rxq_stats_map[i] = -1;
	}

	igc_flow_init(dev);
	igc_clear_all_filter(dev);
	return 0;

err_late:
	igc_hw_control_release(hw);
	return error;
}

static int
eth_igc_dev_uninit(__rte_unused struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();
	eth_igc_close(eth_dev);
	return 0;
}

static int
eth_igc_reset(struct rte_eth_dev *dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = eth_igc_dev_uninit(dev);
	if (ret)
		return ret;

	return eth_igc_dev_init(dev);
}

static int
eth_igc_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t rctl;

	rctl = IGC_READ_REG(hw, IGC_RCTL);
	rctl |= (IGC_RCTL_UPE | IGC_RCTL_MPE);
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	return 0;
}

static int
eth_igc_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t rctl;

	rctl = IGC_READ_REG(hw, IGC_RCTL);
	rctl &= (~IGC_RCTL_UPE);
	if (dev->data->all_multicast == 1)
		rctl |= IGC_RCTL_MPE;
	else
		rctl &= (~IGC_RCTL_MPE);
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	return 0;
}

static int
eth_igc_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t rctl;

	rctl = IGC_READ_REG(hw, IGC_RCTL);
	rctl |= IGC_RCTL_MPE;
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	return 0;
}

static int
eth_igc_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t rctl;

	if (dev->data->promiscuous == 1)
		return 0;	/* must remain in all_multicast mode */

	rctl = IGC_READ_REG(hw, IGC_RCTL);
	rctl &= (~IGC_RCTL_MPE);
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);
	return 0;
}

static int
eth_igc_fw_version_get(struct rte_eth_dev *dev, char *fw_version,
		       size_t fw_size)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_fw_version fw;
	int ret;

	igc_get_fw_version(hw, &fw);

	/* if option rom is valid, display its version too */
	if (fw.or_valid) {
		ret = snprintf(fw_version, fw_size,
			 "%d.%d, 0x%08x, %d.%d.%d",
			 fw.eep_major, fw.eep_minor, fw.etrack_id,
			 fw.or_major, fw.or_build, fw.or_patch);
	/* no option rom */
	} else {
		if (fw.etrack_id != 0X0000) {
			ret = snprintf(fw_version, fw_size,
				 "%d.%d, 0x%08x",
				 fw.eep_major, fw.eep_minor,
				 fw.etrack_id);
		} else {
			ret = snprintf(fw_version, fw_size,
				 "%d.%d.%d",
				 fw.eep_major, fw.eep_minor,
				 fw.eep_build);
		}
	}
	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add the size of '\0' */
	if (fw_size < (size_t)ret)
		return ret;
	else
		return 0;
}

static int
eth_igc_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	dev_info->min_rx_bufsize = 256; /* See BSIZE field of RCTL register. */
	dev_info->max_rx_pktlen = MAX_RX_JUMBO_FRAME_SIZE;
	dev_info->max_mac_addrs = hw->mac.rar_entry_count;
	dev_info->dev_capa &= ~RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP;
	dev_info->rx_offload_capa = IGC_RX_OFFLOAD_ALL;
	dev_info->tx_offload_capa = IGC_TX_OFFLOAD_ALL;
	dev_info->rx_queue_offload_capa = RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	dev_info->max_rx_queues = IGC_QUEUE_PAIRS_NUM;
	dev_info->max_tx_queues = IGC_QUEUE_PAIRS_NUM;
	dev_info->max_vmdq_pools = 0;

	dev_info->hash_key_size = IGC_HKEY_MAX_INDEX * sizeof(uint32_t);
	dev_info->reta_size = RTE_ETH_RSS_RETA_SIZE_128;
	dev_info->flow_type_rss_offloads = IGC_RSS_OFFLOAD_ALL;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = IGC_DEFAULT_RX_PTHRESH,
			.hthresh = IGC_DEFAULT_RX_HTHRESH,
			.wthresh = IGC_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = IGC_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = IGC_DEFAULT_TX_PTHRESH,
			.hthresh = IGC_DEFAULT_TX_HTHRESH,
			.wthresh = IGC_DEFAULT_TX_WTHRESH,
		},
		.offloads = 0,
	};

	dev_info->rx_desc_lim = rx_desc_lim;
	dev_info->tx_desc_lim = tx_desc_lim;

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_10M_HD | RTE_ETH_LINK_SPEED_10M |
			RTE_ETH_LINK_SPEED_100M_HD | RTE_ETH_LINK_SPEED_100M |
			RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_2_5G;

	dev_info->max_mtu = dev_info->max_rx_pktlen - IGC_ETH_OVERHEAD;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	return 0;
}

static int
eth_igc_led_on(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	return igc_led_on(hw) == IGC_SUCCESS ? 0 : -ENOTSUP;
}

static int
eth_igc_led_off(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	return igc_led_off(hw) == IGC_SUCCESS ? 0 : -ENOTSUP;
}

static const uint32_t *
eth_igc_supported_ptypes_get(__rte_unused struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to rx_desc_pkt_info_to_pkt_type() */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_TUNNEL_IP,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_INNER_L3_IPV6_EXT,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	return ptypes;
}

static int
eth_igc_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t frame_size = mtu + IGC_ETH_OVERHEAD;
	uint32_t rctl;

	/* if extend vlan has been enabled */
	if (IGC_READ_REG(hw, IGC_CTRL_EXT) & IGC_CTRL_EXT_EXT_VLAN)
		frame_size += VLAN_TAG_SIZE;

	/*
	 * If device is started, refuse mtu that requires the support of
	 * scattered packets when this feature has not been enabled before.
	 */
	if (dev->data->dev_started && !dev->data->scattered_rx &&
	    frame_size > dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM) {
		PMD_INIT_LOG(ERR, "Stop port first.");
		return -EINVAL;
	}

	rctl = IGC_READ_REG(hw, IGC_RCTL);
	if (mtu > RTE_ETHER_MTU)
		rctl |= IGC_RCTL_LPE;
	else
		rctl &= ~IGC_RCTL_LPE;
	IGC_WRITE_REG(hw, IGC_RCTL, rctl);

	IGC_WRITE_REG(hw, IGC_RLPML, frame_size);

	return 0;
}

static int
eth_igc_rar_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		uint32_t index, uint32_t pool)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	igc_rar_set(hw, mac_addr->addr_bytes, index);
	RTE_SET_USED(pool);
	return 0;
}

static void
eth_igc_rar_clear(struct rte_eth_dev *dev, uint32_t index)
{
	uint8_t addr[RTE_ETHER_ADDR_LEN];
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	memset(addr, 0, sizeof(addr));
	igc_rar_set(hw, addr, index);
}

static int
eth_igc_default_mac_addr_set(struct rte_eth_dev *dev,
			struct rte_ether_addr *addr)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	igc_rar_set(hw, addr->addr_bytes, 0);
	return 0;
}

static int
eth_igc_set_mc_addr_list(struct rte_eth_dev *dev,
			 struct rte_ether_addr *mc_addr_set,
			 uint32_t nb_mc_addr)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	igc_update_mc_addr_list(hw, (u8 *)mc_addr_set, nb_mc_addr);
	return 0;
}

/*
 * Read hardware registers
 */
static void
igc_read_stats_registers(struct igc_hw *hw, struct igc_hw_stats *stats)
{
	int pause_frames;

	uint64_t old_gprc  = stats->gprc;
	uint64_t old_gptc  = stats->gptc;
	uint64_t old_tpr   = stats->tpr;
	uint64_t old_tpt   = stats->tpt;
	uint64_t old_rpthc = stats->rpthc;
	uint64_t old_hgptc = stats->hgptc;

	stats->crcerrs += IGC_READ_REG(hw, IGC_CRCERRS);
	stats->algnerrc += IGC_READ_REG(hw, IGC_ALGNERRC);
	stats->rxerrc += IGC_READ_REG(hw, IGC_RXERRC);
	stats->mpc += IGC_READ_REG(hw, IGC_MPC);
	stats->scc += IGC_READ_REG(hw, IGC_SCC);
	stats->ecol += IGC_READ_REG(hw, IGC_ECOL);

	stats->mcc += IGC_READ_REG(hw, IGC_MCC);
	stats->latecol += IGC_READ_REG(hw, IGC_LATECOL);
	stats->colc += IGC_READ_REG(hw, IGC_COLC);

	stats->dc += IGC_READ_REG(hw, IGC_DC);
	stats->tncrs += IGC_READ_REG(hw, IGC_TNCRS);
	stats->htdpmc += IGC_READ_REG(hw, IGC_HTDPMC);
	stats->rlec += IGC_READ_REG(hw, IGC_RLEC);
	stats->xonrxc += IGC_READ_REG(hw, IGC_XONRXC);
	stats->xontxc += IGC_READ_REG(hw, IGC_XONTXC);

	/*
	 * For watchdog management we need to know if we have been
	 * paused during the last interval, so capture that here.
	 */
	pause_frames = IGC_READ_REG(hw, IGC_XOFFRXC);
	stats->xoffrxc += pause_frames;
	stats->xofftxc += IGC_READ_REG(hw, IGC_XOFFTXC);
	stats->fcruc += IGC_READ_REG(hw, IGC_FCRUC);
	stats->prc64 += IGC_READ_REG(hw, IGC_PRC64);
	stats->prc127 += IGC_READ_REG(hw, IGC_PRC127);
	stats->prc255 += IGC_READ_REG(hw, IGC_PRC255);
	stats->prc511 += IGC_READ_REG(hw, IGC_PRC511);
	stats->prc1023 += IGC_READ_REG(hw, IGC_PRC1023);
	stats->prc1522 += IGC_READ_REG(hw, IGC_PRC1522);
	stats->gprc += IGC_READ_REG(hw, IGC_GPRC);
	stats->bprc += IGC_READ_REG(hw, IGC_BPRC);
	stats->mprc += IGC_READ_REG(hw, IGC_MPRC);
	stats->gptc += IGC_READ_REG(hw, IGC_GPTC);

	/* For the 64-bit byte counters the low dword must be read first. */
	/* Both registers clear on the read of the high dword */

	/* Workaround CRC bytes included in size, take away 4 bytes/packet */
	stats->gorc += IGC_READ_REG(hw, IGC_GORCL);
	stats->gorc += ((uint64_t)IGC_READ_REG(hw, IGC_GORCH) << 32);
	stats->gorc -= (stats->gprc - old_gprc) * RTE_ETHER_CRC_LEN;
	stats->gotc += IGC_READ_REG(hw, IGC_GOTCL);
	stats->gotc += ((uint64_t)IGC_READ_REG(hw, IGC_GOTCH) << 32);
	stats->gotc -= (stats->gptc - old_gptc) * RTE_ETHER_CRC_LEN;

	stats->rnbc += IGC_READ_REG(hw, IGC_RNBC);
	stats->ruc += IGC_READ_REG(hw, IGC_RUC);
	stats->rfc += IGC_READ_REG(hw, IGC_RFC);
	stats->roc += IGC_READ_REG(hw, IGC_ROC);
	stats->rjc += IGC_READ_REG(hw, IGC_RJC);

	stats->mgprc += IGC_READ_REG(hw, IGC_MGTPRC);
	stats->mgpdc += IGC_READ_REG(hw, IGC_MGTPDC);
	stats->mgptc += IGC_READ_REG(hw, IGC_MGTPTC);
	stats->b2ospc += IGC_READ_REG(hw, IGC_B2OSPC);
	stats->b2ogprc += IGC_READ_REG(hw, IGC_B2OGPRC);
	stats->o2bgptc += IGC_READ_REG(hw, IGC_O2BGPTC);
	stats->o2bspc += IGC_READ_REG(hw, IGC_O2BSPC);

	stats->tpr += IGC_READ_REG(hw, IGC_TPR);
	stats->tpt += IGC_READ_REG(hw, IGC_TPT);

	stats->tor += IGC_READ_REG(hw, IGC_TORL);
	stats->tor += ((uint64_t)IGC_READ_REG(hw, IGC_TORH) << 32);
	stats->tor -= (stats->tpr - old_tpr) * RTE_ETHER_CRC_LEN;
	stats->tot += IGC_READ_REG(hw, IGC_TOTL);
	stats->tot += ((uint64_t)IGC_READ_REG(hw, IGC_TOTH) << 32);
	stats->tot -= (stats->tpt - old_tpt) * RTE_ETHER_CRC_LEN;

	stats->ptc64 += IGC_READ_REG(hw, IGC_PTC64);
	stats->ptc127 += IGC_READ_REG(hw, IGC_PTC127);
	stats->ptc255 += IGC_READ_REG(hw, IGC_PTC255);
	stats->ptc511 += IGC_READ_REG(hw, IGC_PTC511);
	stats->ptc1023 += IGC_READ_REG(hw, IGC_PTC1023);
	stats->ptc1522 += IGC_READ_REG(hw, IGC_PTC1522);
	stats->mptc += IGC_READ_REG(hw, IGC_MPTC);
	stats->bptc += IGC_READ_REG(hw, IGC_BPTC);
	stats->tsctc += IGC_READ_REG(hw, IGC_TSCTC);

	stats->iac += IGC_READ_REG(hw, IGC_IAC);
	stats->rpthc += IGC_READ_REG(hw, IGC_RPTHC);
	stats->hgptc += IGC_READ_REG(hw, IGC_HGPTC);
	stats->icrxdmtc += IGC_READ_REG(hw, IGC_ICRXDMTC);

	/* Host to Card Statistics */
	stats->hgorc += IGC_READ_REG(hw, IGC_HGORCL);
	stats->hgorc += ((uint64_t)IGC_READ_REG(hw, IGC_HGORCH) << 32);
	stats->hgorc -= (stats->rpthc - old_rpthc) * RTE_ETHER_CRC_LEN;
	stats->hgotc += IGC_READ_REG(hw, IGC_HGOTCL);
	stats->hgotc += ((uint64_t)IGC_READ_REG(hw, IGC_HGOTCH) << 32);
	stats->hgotc -= (stats->hgptc - old_hgptc) * RTE_ETHER_CRC_LEN;
	stats->lenerrs += IGC_READ_REG(hw, IGC_LENERRS);
}

/*
 * Write 0 to all queue status registers
 */
static void
igc_reset_queue_stats_register(struct igc_hw *hw)
{
	int i;

	for (i = 0; i < IGC_QUEUE_PAIRS_NUM; i++) {
		IGC_WRITE_REG(hw, IGC_PQGPRC(i), 0);
		IGC_WRITE_REG(hw, IGC_PQGPTC(i), 0);
		IGC_WRITE_REG(hw, IGC_PQGORC(i), 0);
		IGC_WRITE_REG(hw, IGC_PQGOTC(i), 0);
		IGC_WRITE_REG(hw, IGC_PQMPRC(i), 0);
		IGC_WRITE_REG(hw, IGC_RQDPC(i), 0);
		IGC_WRITE_REG(hw, IGC_TQDPC(i), 0);
	}
}

/*
 * Read all hardware queue status registers
 */
static void
igc_read_queue_stats_register(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_hw_queue_stats *queue_stats =
				IGC_DEV_PRIVATE_QUEUE_STATS(dev);
	int i;

	/*
	 * This register is not cleared on read. Furthermore, the register wraps
	 * around back to 0x00000000 on the next increment when reaching a value
	 * of 0xFFFFFFFF and then continues normal count operation.
	 */
	for (i = 0; i < IGC_QUEUE_PAIRS_NUM; i++) {
		union {
			u64 ddword;
			u32 dword[2];
		} value;
		u32 tmp;

		/*
		 * Read the register first, if the value is smaller than that
		 * previous read, that mean the register has been overflowed,
		 * then we add the high 4 bytes by 1 and replace the low 4
		 * bytes by the new value.
		 */
		tmp = IGC_READ_REG(hw, IGC_PQGPRC(i));
		value.ddword = queue_stats->pqgprc[i];
		if (value.dword[U32_0_IN_U64] > tmp)
			value.dword[U32_1_IN_U64]++;
		value.dword[U32_0_IN_U64] = tmp;
		queue_stats->pqgprc[i] = value.ddword;

		tmp = IGC_READ_REG(hw, IGC_PQGPTC(i));
		value.ddword = queue_stats->pqgptc[i];
		if (value.dword[U32_0_IN_U64] > tmp)
			value.dword[U32_1_IN_U64]++;
		value.dword[U32_0_IN_U64] = tmp;
		queue_stats->pqgptc[i] = value.ddword;

		tmp = IGC_READ_REG(hw, IGC_PQGORC(i));
		value.ddword = queue_stats->pqgorc[i];
		if (value.dword[U32_0_IN_U64] > tmp)
			value.dword[U32_1_IN_U64]++;
		value.dword[U32_0_IN_U64] = tmp;
		queue_stats->pqgorc[i] = value.ddword;

		tmp = IGC_READ_REG(hw, IGC_PQGOTC(i));
		value.ddword = queue_stats->pqgotc[i];
		if (value.dword[U32_0_IN_U64] > tmp)
			value.dword[U32_1_IN_U64]++;
		value.dword[U32_0_IN_U64] = tmp;
		queue_stats->pqgotc[i] = value.ddword;

		tmp = IGC_READ_REG(hw, IGC_PQMPRC(i));
		value.ddword = queue_stats->pqmprc[i];
		if (value.dword[U32_0_IN_U64] > tmp)
			value.dword[U32_1_IN_U64]++;
		value.dword[U32_0_IN_U64] = tmp;
		queue_stats->pqmprc[i] = value.ddword;

		tmp = IGC_READ_REG(hw, IGC_RQDPC(i));
		value.ddword = queue_stats->rqdpc[i];
		if (value.dword[U32_0_IN_U64] > tmp)
			value.dword[U32_1_IN_U64]++;
		value.dword[U32_0_IN_U64] = tmp;
		queue_stats->rqdpc[i] = value.ddword;

		tmp = IGC_READ_REG(hw, IGC_TQDPC(i));
		value.ddword = queue_stats->tqdpc[i];
		if (value.dword[U32_0_IN_U64] > tmp)
			value.dword[U32_1_IN_U64]++;
		value.dword[U32_0_IN_U64] = tmp;
		queue_stats->tqdpc[i] = value.ddword;
	}
}

static int
eth_igc_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *rte_stats)
{
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_hw_stats *stats = IGC_DEV_PRIVATE_STATS(dev);
	struct igc_hw_queue_stats *queue_stats =
			IGC_DEV_PRIVATE_QUEUE_STATS(dev);
	int i;

	/*
	 * Cancel status handler since it will read the queue status registers
	 */
	rte_eal_alarm_cancel(igc_update_queue_stats_handler, dev);

	/* Read status register */
	igc_read_queue_stats_register(dev);
	igc_read_stats_registers(hw, stats);

	if (rte_stats == NULL) {
		/* Restart queue status handler */
		rte_eal_alarm_set(IGC_ALARM_INTERVAL,
				igc_update_queue_stats_handler, dev);
		return -EINVAL;
	}

	/* Rx Errors */
	rte_stats->imissed = stats->mpc;
	rte_stats->ierrors = stats->crcerrs + stats->rlec +
			stats->rxerrc + stats->algnerrc;

	/* Tx Errors */
	rte_stats->oerrors = stats->ecol + stats->latecol;

	rte_stats->ipackets = stats->gprc;
	rte_stats->opackets = stats->gptc;
	rte_stats->ibytes   = stats->gorc;
	rte_stats->obytes   = stats->gotc;

	/* Get per-queue statuses */
	for (i = 0; i < IGC_QUEUE_PAIRS_NUM; i++) {
		/* GET TX queue statuses */
		int map_id = igc->txq_stats_map[i];
		if (map_id >= 0) {
			rte_stats->q_opackets[map_id] += queue_stats->pqgptc[i];
			rte_stats->q_obytes[map_id] += queue_stats->pqgotc[i];
		}
		/* Get RX queue statuses */
		map_id = igc->rxq_stats_map[i];
		if (map_id >= 0) {
			rte_stats->q_ipackets[map_id] += queue_stats->pqgprc[i];
			rte_stats->q_ibytes[map_id] += queue_stats->pqgorc[i];
			rte_stats->q_errors[map_id] += queue_stats->rqdpc[i];
		}
	}

	/* Restart queue status handler */
	rte_eal_alarm_set(IGC_ALARM_INTERVAL,
			igc_update_queue_stats_handler, dev);
	return 0;
}

static int
eth_igc_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		   unsigned int n)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_hw_stats *hw_stats =
			IGC_DEV_PRIVATE_STATS(dev);
	unsigned int i;

	igc_read_stats_registers(hw, hw_stats);

	if (n < IGC_NB_XSTATS)
		return IGC_NB_XSTATS;

	/* If this is a reset xstats is NULL, and we have cleared the
	 * registers by reading them.
	 */
	if (!xstats)
		return 0;

	/* Extended stats */
	for (i = 0; i < IGC_NB_XSTATS; i++) {
		xstats[i].id = i;
		xstats[i].value = *(uint64_t *)(((char *)hw_stats) +
			rte_igc_stats_strings[i].offset);
	}

	return IGC_NB_XSTATS;
}

static int
eth_igc_xstats_reset(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_hw_stats *hw_stats = IGC_DEV_PRIVATE_STATS(dev);
	struct igc_hw_queue_stats *queue_stats =
			IGC_DEV_PRIVATE_QUEUE_STATS(dev);

	/* Cancel queue status handler for avoid conflict */
	rte_eal_alarm_cancel(igc_update_queue_stats_handler, dev);

	/* HW registers are cleared on read */
	igc_reset_queue_stats_register(hw);
	igc_read_stats_registers(hw, hw_stats);

	/* Reset software totals */
	memset(hw_stats, 0, sizeof(*hw_stats));
	memset(queue_stats, 0, sizeof(*queue_stats));

	/* Restart the queue status handler */
	rte_eal_alarm_set(IGC_ALARM_INTERVAL, igc_update_queue_stats_handler,
			dev);

	return 0;
}

static int
eth_igc_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names, unsigned int size)
{
	unsigned int i;

	if (xstats_names == NULL)
		return IGC_NB_XSTATS;

	if (size < IGC_NB_XSTATS) {
		PMD_DRV_LOG(ERR, "not enough buffers!");
		return IGC_NB_XSTATS;
	}

	for (i = 0; i < IGC_NB_XSTATS; i++)
		strlcpy(xstats_names[i].name, rte_igc_stats_strings[i].name,
			sizeof(xstats_names[i].name));

	return IGC_NB_XSTATS;
}

static int
eth_igc_xstats_get_names_by_id(struct rte_eth_dev *dev,
		const uint64_t *ids, struct rte_eth_xstat_name *xstats_names,
		unsigned int limit)
{
	unsigned int i;

	if (!ids)
		return eth_igc_xstats_get_names(dev, xstats_names, limit);

	for (i = 0; i < limit; i++) {
		if (ids[i] >= IGC_NB_XSTATS) {
			PMD_DRV_LOG(ERR, "id value isn't valid");
			return -EINVAL;
		}
		strlcpy(xstats_names[i].name,
			rte_igc_stats_strings[ids[i]].name,
			sizeof(xstats_names[i].name));
	}
	return limit;
}

static int
eth_igc_xstats_get_by_id(struct rte_eth_dev *dev, const uint64_t *ids,
		uint64_t *values, unsigned int n)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_hw_stats *hw_stats = IGC_DEV_PRIVATE_STATS(dev);
	unsigned int i;

	igc_read_stats_registers(hw, hw_stats);

	if (!ids) {
		if (n < IGC_NB_XSTATS)
			return IGC_NB_XSTATS;

		/* If this is a reset xstats is NULL, and we have cleared the
		 * registers by reading them.
		 */
		if (!values)
			return 0;

		/* Extended stats */
		for (i = 0; i < IGC_NB_XSTATS; i++)
			values[i] = *(uint64_t *)(((char *)hw_stats) +
					rte_igc_stats_strings[i].offset);

		return IGC_NB_XSTATS;

	} else {
		for (i = 0; i < n; i++) {
			if (ids[i] >= IGC_NB_XSTATS) {
				PMD_DRV_LOG(ERR, "id value isn't valid");
				return -EINVAL;
			}
			values[i] = *(uint64_t *)(((char *)hw_stats) +
					rte_igc_stats_strings[ids[i]].offset);
		}
		return n;
	}
}

static int
eth_igc_queue_stats_mapping_set(struct rte_eth_dev *dev,
		uint16_t queue_id, uint8_t stat_idx, uint8_t is_rx)
{
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);

	/* check queue id is valid */
	if (queue_id >= IGC_QUEUE_PAIRS_NUM) {
		PMD_DRV_LOG(ERR, "queue id(%u) error, max is %u",
			queue_id, IGC_QUEUE_PAIRS_NUM - 1);
		return -EINVAL;
	}

	/* store the mapping status id */
	if (is_rx)
		igc->rxq_stats_map[queue_id] = stat_idx;
	else
		igc->txq_stats_map[queue_id] = stat_idx;

	return 0;
}

static int
eth_igc_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint32_t vec = IGC_MISC_VEC_ID;

	if (rte_intr_allow_others(intr_handle))
		vec = IGC_RX_VEC_START;

	uint32_t mask = 1u << (queue_id + vec);

	IGC_WRITE_REG(hw, IGC_EIMC, mask);
	IGC_WRITE_FLUSH(hw);

	return 0;
}

static int
eth_igc_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint32_t vec = IGC_MISC_VEC_ID;

	if (rte_intr_allow_others(intr_handle))
		vec = IGC_RX_VEC_START;

	uint32_t mask = 1u << (queue_id + vec);

	IGC_WRITE_REG(hw, IGC_EIMS, mask);
	IGC_WRITE_FLUSH(hw);

	rte_intr_enable(intr_handle);

	return 0;
}

static int
eth_igc_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t ctrl;
	int tx_pause;
	int rx_pause;

	fc_conf->pause_time = hw->fc.pause_time;
	fc_conf->high_water = hw->fc.high_water;
	fc_conf->low_water = hw->fc.low_water;
	fc_conf->send_xon = hw->fc.send_xon;
	fc_conf->autoneg = hw->mac.autoneg;

	/*
	 * Return rx_pause and tx_pause status according to actual setting of
	 * the TFCE and RFCE bits in the CTRL register.
	 */
	ctrl = IGC_READ_REG(hw, IGC_CTRL);
	if (ctrl & IGC_CTRL_TFCE)
		tx_pause = 1;
	else
		tx_pause = 0;

	if (ctrl & IGC_CTRL_RFCE)
		rx_pause = 1;
	else
		rx_pause = 0;

	if (rx_pause && tx_pause)
		fc_conf->mode = RTE_ETH_FC_FULL;
	else if (rx_pause)
		fc_conf->mode = RTE_ETH_FC_RX_PAUSE;
	else if (tx_pause)
		fc_conf->mode = RTE_ETH_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_ETH_FC_NONE;

	return 0;
}

static int
eth_igc_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t rx_buf_size;
	uint32_t max_high_water;
	uint32_t rctl;
	int err;

	if (fc_conf->autoneg != hw->mac.autoneg)
		return -ENOTSUP;

	rx_buf_size = igc_get_rx_buffer_size(hw);
	PMD_DRV_LOG(DEBUG, "Rx packet buffer size = 0x%x", rx_buf_size);

	/* At least reserve one Ethernet frame for watermark */
	max_high_water = rx_buf_size - RTE_ETHER_MAX_LEN;
	if (fc_conf->high_water > max_high_water ||
		fc_conf->high_water < fc_conf->low_water) {
		PMD_DRV_LOG(ERR,
			"Incorrect high(%u)/low(%u) water value, max is %u",
			fc_conf->high_water, fc_conf->low_water,
			max_high_water);
		return -EINVAL;
	}

	switch (fc_conf->mode) {
	case RTE_ETH_FC_NONE:
		hw->fc.requested_mode = igc_fc_none;
		break;
	case RTE_ETH_FC_RX_PAUSE:
		hw->fc.requested_mode = igc_fc_rx_pause;
		break;
	case RTE_ETH_FC_TX_PAUSE:
		hw->fc.requested_mode = igc_fc_tx_pause;
		break;
	case RTE_ETH_FC_FULL:
		hw->fc.requested_mode = igc_fc_full;
		break;
	default:
		PMD_DRV_LOG(ERR, "unsupported fc mode: %u", fc_conf->mode);
		return -EINVAL;
	}

	hw->fc.pause_time     = fc_conf->pause_time;
	hw->fc.high_water     = fc_conf->high_water;
	hw->fc.low_water      = fc_conf->low_water;
	hw->fc.send_xon	      = fc_conf->send_xon;

	err = igc_setup_link_generic(hw);
	if (err == IGC_SUCCESS) {
		/**
		 * check if we want to forward MAC frames - driver doesn't have
		 * native capability to do that, so we'll write the registers
		 * ourselves
		 **/
		rctl = IGC_READ_REG(hw, IGC_RCTL);

		/* set or clear MFLCN.PMCF bit depending on configuration */
		if (fc_conf->mac_ctrl_frame_fwd != 0)
			rctl |= IGC_RCTL_PMCF;
		else
			rctl &= ~IGC_RCTL_PMCF;

		IGC_WRITE_REG(hw, IGC_RCTL, rctl);
		IGC_WRITE_FLUSH(hw);

		return 0;
	}

	PMD_DRV_LOG(ERR, "igc_setup_link_generic = 0x%x", err);
	return -EIO;
}

static int
eth_igc_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint16_t i;

	if (reta_size != RTE_ETH_RSS_RETA_SIZE_128) {
		PMD_DRV_LOG(ERR,
			"The size of RSS redirection table configured(%d) doesn't match the number hardware can supported(%d)",
			reta_size, RTE_ETH_RSS_RETA_SIZE_128);
		return -EINVAL;
	}

	RTE_BUILD_BUG_ON(RTE_ETH_RSS_RETA_SIZE_128 % IGC_RSS_RDT_REG_SIZE);

	/* set redirection table */
	for (i = 0; i < RTE_ETH_RSS_RETA_SIZE_128; i += IGC_RSS_RDT_REG_SIZE) {
		union igc_rss_reta_reg reta, reg;
		uint16_t idx, shift;
		uint8_t j, mask;

		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) &
				IGC_RSS_RDT_REG_SIZE_MASK);

		/* if no need to update the register */
		if (!mask ||
		    shift > (RTE_ETH_RETA_GROUP_SIZE - IGC_RSS_RDT_REG_SIZE))
			continue;

		/* check mask whether need to read the register value first */
		if (mask == IGC_RSS_RDT_REG_SIZE_MASK)
			reg.dword = 0;
		else
			reg.dword = IGC_READ_REG_LE_VALUE(hw,
					IGC_RETA(i / IGC_RSS_RDT_REG_SIZE));

		/* update the register */
		RTE_BUILD_BUG_ON(sizeof(reta.bytes) != IGC_RSS_RDT_REG_SIZE);
		for (j = 0; j < IGC_RSS_RDT_REG_SIZE; j++) {
			if (mask & (1u << j))
				reta.bytes[j] =
					(uint8_t)reta_conf[idx].reta[shift + j];
			else
				reta.bytes[j] = reg.bytes[j];
		}
		IGC_WRITE_REG_LE_VALUE(hw,
			IGC_RETA(i / IGC_RSS_RDT_REG_SIZE), reta.dword);
	}

	return 0;
}

static int
eth_igc_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint16_t i;

	if (reta_size != RTE_ETH_RSS_RETA_SIZE_128) {
		PMD_DRV_LOG(ERR,
			"The size of RSS redirection table configured(%d) doesn't match the number hardware can supported(%d)",
			reta_size, RTE_ETH_RSS_RETA_SIZE_128);
		return -EINVAL;
	}

	RTE_BUILD_BUG_ON(RTE_ETH_RSS_RETA_SIZE_128 % IGC_RSS_RDT_REG_SIZE);

	/* read redirection table */
	for (i = 0; i < RTE_ETH_RSS_RETA_SIZE_128; i += IGC_RSS_RDT_REG_SIZE) {
		union igc_rss_reta_reg reta;
		uint16_t idx, shift;
		uint8_t j, mask;

		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) &
				IGC_RSS_RDT_REG_SIZE_MASK);

		/* if no need to read register */
		if (!mask ||
		    shift > (RTE_ETH_RETA_GROUP_SIZE - IGC_RSS_RDT_REG_SIZE))
			continue;

		/* read register and get the queue index */
		RTE_BUILD_BUG_ON(sizeof(reta.bytes) != IGC_RSS_RDT_REG_SIZE);
		reta.dword = IGC_READ_REG_LE_VALUE(hw,
				IGC_RETA(i / IGC_RSS_RDT_REG_SIZE));
		for (j = 0; j < IGC_RSS_RDT_REG_SIZE; j++) {
			if (mask & (1u << j))
				reta_conf[idx].reta[shift + j] = reta.bytes[j];
		}
	}

	return 0;
}

static int
eth_igc_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	igc_hw_rss_hash_set(hw, rss_conf);
	return 0;
}

static int
eth_igc_rss_hash_conf_get(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t *hash_key = (uint32_t *)rss_conf->rss_key;
	uint32_t mrqc;
	uint64_t rss_hf;

	if (hash_key != NULL) {
		int i;

		/* if not enough space for store hash key */
		if (rss_conf->rss_key_len != IGC_HKEY_SIZE) {
			PMD_DRV_LOG(ERR,
				"RSS hash key size %u in parameter doesn't match the hardware hash key size %u",
				rss_conf->rss_key_len, IGC_HKEY_SIZE);
			return -EINVAL;
		}

		/* read RSS key from register */
		for (i = 0; i < IGC_HKEY_MAX_INDEX; i++)
			hash_key[i] = IGC_READ_REG_LE_VALUE(hw, IGC_RSSRK(i));
	}

	/* get RSS functions configured in MRQC register */
	mrqc = IGC_READ_REG(hw, IGC_MRQC);
	if ((mrqc & IGC_MRQC_ENABLE_RSS_4Q) == 0)
		return 0;

	rss_hf = 0;
	if (mrqc & IGC_MRQC_RSS_FIELD_IPV4)
		rss_hf |= RTE_ETH_RSS_IPV4;
	if (mrqc & IGC_MRQC_RSS_FIELD_IPV4_TCP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;
	if (mrqc & IGC_MRQC_RSS_FIELD_IPV6)
		rss_hf |= RTE_ETH_RSS_IPV6;
	if (mrqc & IGC_MRQC_RSS_FIELD_IPV6_EX)
		rss_hf |= RTE_ETH_RSS_IPV6_EX;
	if (mrqc & IGC_MRQC_RSS_FIELD_IPV6_TCP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_TCP;
	if (mrqc & IGC_MRQC_RSS_FIELD_IPV6_TCP_EX)
		rss_hf |= RTE_ETH_RSS_IPV6_TCP_EX;
	if (mrqc & IGC_MRQC_RSS_FIELD_IPV4_UDP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;
	if (mrqc & IGC_MRQC_RSS_FIELD_IPV6_UDP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_UDP;
	if (mrqc & IGC_MRQC_RSS_FIELD_IPV6_UDP_EX)
		rss_hf |= RTE_ETH_RSS_IPV6_UDP_EX;

	rss_conf->rss_hf |= rss_hf;
	return 0;
}

static int
eth_igc_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_vfta *shadow_vfta = IGC_DEV_PRIVATE_VFTA(dev);
	uint32_t vfta;
	uint32_t vid_idx;
	uint32_t vid_bit;

	vid_idx = (vlan_id >> IGC_VFTA_ENTRY_SHIFT) & IGC_VFTA_ENTRY_MASK;
	vid_bit = 1u << (vlan_id & IGC_VFTA_ENTRY_BIT_SHIFT_MASK);
	vfta = shadow_vfta->vfta[vid_idx];
	if (on)
		vfta |= vid_bit;
	else
		vfta &= ~vid_bit;
	IGC_WRITE_REG_ARRAY(hw, IGC_VFTA, vid_idx, vfta);

	/* update local VFTA copy */
	shadow_vfta->vfta[vid_idx] = vfta;

	return 0;
}

static void
igc_vlan_hw_filter_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	igc_read_reg_check_clear_bits(hw, IGC_RCTL,
			IGC_RCTL_CFIEN | IGC_RCTL_VFE);
}

static void
igc_vlan_hw_filter_enable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_vfta *shadow_vfta = IGC_DEV_PRIVATE_VFTA(dev);
	uint32_t reg_val;
	int i;

	/* Filter Table Enable, CFI not used for packet acceptance */
	reg_val = IGC_READ_REG(hw, IGC_RCTL);
	reg_val &= ~IGC_RCTL_CFIEN;
	reg_val |= IGC_RCTL_VFE;
	IGC_WRITE_REG(hw, IGC_RCTL, reg_val);

	/* restore VFTA table */
	for (i = 0; i < IGC_VFTA_SIZE; i++)
		IGC_WRITE_REG_ARRAY(hw, IGC_VFTA, i, shadow_vfta->vfta[i]);
}

static void
igc_vlan_hw_strip_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	igc_read_reg_check_clear_bits(hw, IGC_CTRL, IGC_CTRL_VME);
}

static void
igc_vlan_hw_strip_enable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	igc_read_reg_check_set_bits(hw, IGC_CTRL, IGC_CTRL_VME);
}

static int
igc_vlan_hw_extend_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t frame_size = dev->data->mtu + IGC_ETH_OVERHEAD;
	uint32_t ctrl_ext;

	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);

	/* if extend vlan hasn't been enabled */
	if ((ctrl_ext & IGC_CTRL_EXT_EXT_VLAN) == 0)
		return 0;

	/* Update maximum packet length */
	if (frame_size < RTE_ETHER_MIN_MTU + VLAN_TAG_SIZE) {
		PMD_DRV_LOG(ERR, "Maximum packet length %u error, min is %u",
			frame_size, VLAN_TAG_SIZE + RTE_ETHER_MIN_MTU);
		return -EINVAL;
	}
	IGC_WRITE_REG(hw, IGC_RLPML, frame_size - VLAN_TAG_SIZE);

	IGC_WRITE_REG(hw, IGC_CTRL_EXT, ctrl_ext & ~IGC_CTRL_EXT_EXT_VLAN);
	return 0;
}

static int
igc_vlan_hw_extend_enable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t frame_size = dev->data->mtu + IGC_ETH_OVERHEAD;
	uint32_t ctrl_ext;

	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);

	/* if extend vlan has been enabled */
	if (ctrl_ext & IGC_CTRL_EXT_EXT_VLAN)
		return 0;

	/* Update maximum packet length */
	if (frame_size > MAX_RX_JUMBO_FRAME_SIZE) {
		PMD_DRV_LOG(ERR, "Maximum packet length %u error, max is %u",
			frame_size, MAX_RX_JUMBO_FRAME_SIZE);
		return -EINVAL;
	}
	IGC_WRITE_REG(hw, IGC_RLPML, frame_size);

	IGC_WRITE_REG(hw, IGC_CTRL_EXT, ctrl_ext | IGC_CTRL_EXT_EXT_VLAN);
	return 0;
}

static int
eth_igc_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct rte_eth_rxmode *rxmode;

	rxmode = &dev->data->dev_conf.rxmode;
	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			igc_vlan_hw_strip_enable(dev);
		else
			igc_vlan_hw_strip_disable(dev);
	}

	if (mask & RTE_ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER)
			igc_vlan_hw_filter_enable(dev);
		else
			igc_vlan_hw_filter_disable(dev);
	}

	if (mask & RTE_ETH_VLAN_EXTEND_MASK) {
		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_EXTEND)
			return igc_vlan_hw_extend_enable(dev);
		else
			return igc_vlan_hw_extend_disable(dev);
	}

	return 0;
}

static int
eth_igc_vlan_tpid_set(struct rte_eth_dev *dev,
		      enum rte_vlan_type vlan_type,
		      uint16_t tpid)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t reg_val;

	/* only outer TPID of double VLAN can be configured*/
	if (vlan_type == RTE_ETH_VLAN_TYPE_OUTER) {
		reg_val = IGC_READ_REG(hw, IGC_VET);
		reg_val = (reg_val & (~IGC_VET_EXT)) |
			((uint32_t)tpid << IGC_VET_EXT_SHIFT);
		IGC_WRITE_REG(hw, IGC_VET, reg_val);

		return 0;
	}

	/* all other TPID values are read-only*/
	PMD_DRV_LOG(ERR, "Not supported");
	return -ENOTSUP;
}

static int
eth_igc_timesync_enable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct timespec system_time;
	struct igc_rx_queue *rxq;
	uint32_t val;
	uint16_t i;

	IGC_WRITE_REG(hw, IGC_TSAUXC, 0x0);

	clock_gettime(CLOCK_REALTIME, &system_time);
	IGC_WRITE_REG(hw, IGC_SYSTIML, system_time.tv_nsec);
	IGC_WRITE_REG(hw, IGC_SYSTIMH, system_time.tv_sec);

	/* Enable timestamping of received PTP packets. */
	val = IGC_READ_REG(hw, IGC_RXPBS);
	val |= IGC_RXPBS_CFG_TS_EN;
	IGC_WRITE_REG(hw, IGC_RXPBS, val);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		val = IGC_READ_REG(hw, IGC_SRRCTL(i));
		/* For now, only support retrieving Rx timestamp from timer0. */
		val |= IGC_SRRCTL_TIMER1SEL(0) | IGC_SRRCTL_TIMER0SEL(0) |
		       IGC_SRRCTL_TIMESTAMP;
		IGC_WRITE_REG(hw, IGC_SRRCTL(i), val);
	}

	val = IGC_TSYNCRXCTL_ENABLED | IGC_TSYNCRXCTL_TYPE_ALL |
	      IGC_TSYNCRXCTL_RXSYNSIG;
	IGC_WRITE_REG(hw, IGC_TSYNCRXCTL, val);

	/* Enable Timestamping of transmitted PTP packets. */
	IGC_WRITE_REG(hw, IGC_TSYNCTXCTL, IGC_TSYNCTXCTL_ENABLED |
		      IGC_TSYNCTXCTL_TXSYNSIG);

	/* Read TXSTMP registers to discard any timestamp previously stored. */
	IGC_READ_REG(hw, IGC_TXSTMPL);
	IGC_READ_REG(hw, IGC_TXSTMPH);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		rxq->offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;
	}

	return 0;
}

static int
eth_igc_timesync_read_time(struct rte_eth_dev *dev, struct timespec *ts)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	ts->tv_nsec = IGC_READ_REG(hw, IGC_SYSTIML);
	ts->tv_sec = IGC_READ_REG(hw, IGC_SYSTIMH);

	return 0;
}

static int
eth_igc_timesync_write_time(struct rte_eth_dev *dev, const struct timespec *ts)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	IGC_WRITE_REG(hw, IGC_SYSTIML, ts->tv_nsec);
	IGC_WRITE_REG(hw, IGC_SYSTIMH, ts->tv_sec);

	return 0;
}

static int
eth_igc_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t nsec, sec;
	uint64_t systime, ns;
	struct timespec ts;

	nsec = (uint64_t)IGC_READ_REG(hw, IGC_SYSTIML);
	sec = (uint64_t)IGC_READ_REG(hw, IGC_SYSTIMH);
	systime = sec * NSEC_PER_SEC + nsec;

	ns = systime + delta;
	ts = rte_ns_to_timespec(ns);

	IGC_WRITE_REG(hw, IGC_SYSTIML, ts.tv_nsec);
	IGC_WRITE_REG(hw, IGC_SYSTIMH, ts.tv_sec);

	return 0;
}

static int
eth_igc_timesync_read_rx_timestamp(__rte_unused struct rte_eth_dev *dev,
			       struct timespec *timestamp,
			       uint32_t flags)
{
	struct rte_eth_link link;
	int adjust = 0;
	struct igc_rx_queue *rxq;
	uint64_t rx_timestamp;

	/* Get current link speed. */
	eth_igc_link_update(dev, 1);
	rte_eth_linkstatus_get(dev, &link);

	switch (link.link_speed) {
	case SPEED_10:
		adjust = IGC_I225_RX_LATENCY_10;
		break;
	case SPEED_100:
		adjust = IGC_I225_RX_LATENCY_100;
		break;
	case SPEED_1000:
		adjust = IGC_I225_RX_LATENCY_1000;
		break;
	case SPEED_2500:
		adjust = IGC_I225_RX_LATENCY_2500;
		break;
	}

	rxq = dev->data->rx_queues[flags];
	rx_timestamp = rxq->rx_timestamp - adjust;
	*timestamp = rte_ns_to_timespec(rx_timestamp);

	return 0;
}

static int
eth_igc_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
			       struct timespec *timestamp)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct rte_eth_link link;
	uint32_t val, nsec, sec;
	uint64_t tx_timestamp;
	int adjust = 0;

	val = IGC_READ_REG(hw, IGC_TSYNCTXCTL);
	if (!(val & IGC_TSYNCTXCTL_VALID))
		return -EINVAL;

	nsec = (uint64_t)IGC_READ_REG(hw, IGC_TXSTMPL);
	sec = (uint64_t)IGC_READ_REG(hw, IGC_TXSTMPH);
	tx_timestamp = sec * NSEC_PER_SEC + nsec;

	/* Get current link speed. */
	eth_igc_link_update(dev, 1);
	rte_eth_linkstatus_get(dev, &link);

	switch (link.link_speed) {
	case SPEED_10:
		adjust = IGC_I225_TX_LATENCY_10;
		break;
	case SPEED_100:
		adjust = IGC_I225_TX_LATENCY_100;
		break;
	case SPEED_1000:
		adjust = IGC_I225_TX_LATENCY_1000;
		break;
	case SPEED_2500:
		adjust = IGC_I225_TX_LATENCY_2500;
		break;
	}

	tx_timestamp += adjust;
	*timestamp = rte_ns_to_timespec(tx_timestamp);

	return 0;
}

static int
eth_igc_timesync_disable(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	uint32_t val;

	/* Disable timestamping of transmitted PTP packets. */
	IGC_WRITE_REG(hw, IGC_TSYNCTXCTL, 0);

	/* Disable timestamping of received PTP packets. */
	IGC_WRITE_REG(hw, IGC_TSYNCRXCTL, 0);

	val = IGC_READ_REG(hw, IGC_RXPBS);
	val &= IGC_RXPBS_CFG_TS_EN;
	IGC_WRITE_REG(hw, IGC_RXPBS, val);

	val = IGC_READ_REG(hw, IGC_SRRCTL(0));
	val &= ~IGC_SRRCTL_TIMESTAMP;
	IGC_WRITE_REG(hw, IGC_SRRCTL(0), val);

	return 0;
}

static int
eth_igc_read_clock(__rte_unused struct rte_eth_dev *dev, uint64_t *clock)
{
	struct timespec system_time;

	clock_gettime(CLOCK_REALTIME, &system_time);
	*clock = system_time.tv_sec * NSEC_PER_SEC + system_time.tv_nsec;

	return 0;
}

static int
eth_igc_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	PMD_INIT_FUNC_TRACE();
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct igc_adapter), eth_igc_dev_init);
}

static int
eth_igc_pci_remove(struct rte_pci_device *pci_dev)
{
	PMD_INIT_FUNC_TRACE();
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_igc_dev_uninit);
}

static struct rte_pci_driver rte_igc_pmd = {
	.id_table = pci_id_igc_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_igc_pci_probe,
	.remove = eth_igc_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_igc, rte_igc_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_igc, pci_id_igc_map);
RTE_PMD_REGISTER_KMOD_DEP(net_igc, "* igb_uio | uio_pci_generic | vfio-pci");
