/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NET_COMMON_H__
#define __NFP_NET_COMMON_H__

#include <bus_pci_driver.h>
#include <ethdev_driver.h>
#include <nfp_common.h>
#include <nfp_dev.h>
#include <rte_spinlock.h>

#include "nfp_net_ctrl.h"

/* Interrupt definitions */
#define NFP_NET_IRQ_LSC_IDX             0

/* Default values for RX/TX configuration */
#define DEFAULT_RX_FREE_THRESH  32
#define DEFAULT_RX_PTHRESH      8
#define DEFAULT_RX_HTHRESH      8
#define DEFAULT_RX_WTHRESH      0

#define DEFAULT_TX_RS_THRESH    32
#define DEFAULT_TX_FREE_THRESH  32
#define DEFAULT_TX_PTHRESH      32
#define DEFAULT_TX_HTHRESH      0
#define DEFAULT_TX_WTHRESH      0
#define DEFAULT_TX_RSBIT_THRESH 32

/* Alignment for dma zones */
#define NFP_MEMZONE_ALIGN       128

/* Number of supported physical ports */
#define NFP_MAX_PHYPORTS        12

#define NFP_BEAT_LENGTH         8

/*
 * Each PF has corresponding word to beat:
 * Offset | Usage
 *   0    | magic number
 *   8    | beat of Pf0
 *   16   | beat of Pf1
 *   24   | beat of Pf2
 *   32   | beat of Pf3
 */
#define NFP_BEAT_OFFSET(_x)     (((_x) + 1) * NFP_BEAT_LENGTH)

/* Firmware application ID's */
enum nfp_app_fw_id {
	NFP_APP_FW_CORE_NIC               = 0x1,
	NFP_APP_FW_FLOWER_NIC             = 0x3,
};

enum nfp_net_meta_format {
	NFP_NET_METAFORMAT_SINGLE,
	NFP_NET_METAFORMAT_CHAINED,
};

/* Parsed control BAR TLV capabilities */
struct nfp_net_tlv_caps {
	uint32_t mbox_off;               /**< VNIC mailbox area offset */
	uint32_t mbox_len;               /**< VNIC mailbox area length */
	uint32_t mbox_cmsg_types;        /**< Cmsgs which can be passed through the mailbox */
};

struct nfp_multi_pf {
	/** Support multiple PF */
	bool enabled;
	/** Function index */
	uint8_t function_id;
	/** Pointer to CPP area for beat to keepalive */
	struct nfp_cpp_area *beat_area;
	/** Pointer to mapped beat address used for keepalive */
	uint8_t *beat_addr;
};

struct nfp_pf_dev {
	/** Backpointer to associated pci device */
	struct rte_pci_device *pci_dev;

	enum nfp_app_fw_id app_fw_id;

	/** Pointer to the app running on the PF */
	void *app_fw_priv;

	/** The eth table reported by firmware */
	struct nfp_eth_table *nfp_eth_table;

	uint8_t *ctrl_bar;

	struct nfp_cpp *cpp;
	struct nfp_cpp_area *ctrl_area;
	struct nfp_cpp_area *qc_area;

	uint8_t *qc_bar;

	struct nfp_hwinfo *hwinfo;
	struct nfp_rtsym_table *sym_tbl;

	/** Service id of cpp bridge service */
	uint32_t cpp_bridge_id;

	/** Multiple PF configuration */
	struct nfp_multi_pf multi_pf;
};

struct nfp_app_fw_nic {
	/** Backpointer to the PF device */
	struct nfp_pf_dev *pf_dev;

	/**
	 * Array of physical ports belonging to this CoreNIC app.
	 * This is really a list of vNIC's, one for each physical port.
	 */
	struct nfp_net_hw *ports[NFP_MAX_PHYPORTS];

	bool multiport;
	uint8_t total_phyports;
};

struct nfp_net_hw {
	/** The parent class */
	struct nfp_hw super;

	/** Backpointer to the PF this port belongs to */
	struct nfp_pf_dev *pf_dev;

	/** Backpointer to the eth_dev of this port */
	struct rte_eth_dev *eth_dev;

	/** Info from the firmware */
	struct nfp_net_fw_ver ver;
	uint32_t max_mtu;
	uint32_t mtu;
	uint32_t rx_offset;
	enum nfp_net_meta_format meta_format;

	/** NFP ASIC params */
	const struct nfp_dev_info *dev_info;

	uint8_t *tx_bar;
	uint8_t *rx_bar;

	int stride_rx;
	int stride_tx;

	uint16_t vxlan_ports[NFP_NET_N_VXLAN_PORTS];
	uint8_t vxlan_usecnt[NFP_NET_N_VXLAN_PORTS];

	uint32_t max_tx_queues;
	uint32_t max_rx_queues;
	uint16_t flbufsz;
	uint16_t device_id;
	uint16_t vendor_id;
	uint16_t subsystem_device_id;
	uint16_t subsystem_vendor_id;

	/** Records starting point for counters */
	struct rte_eth_stats eth_stats_base;
	struct rte_eth_xstat *eth_xstats_base;

	struct nfp_cpp *cpp;
	struct nfp_cpp_area *ctrl_area;
	struct nfp_cpp_area *mac_stats_area;
	uint8_t *mac_stats_bar;
	uint8_t *mac_stats;

	/** Sequential physical port number, only valid for CoreNIC firmware */
	uint8_t idx;
	/** Internal port number as seen from NFP */
	uint8_t nfp_idx;

	struct nfp_net_tlv_caps tlv_caps;

	struct nfp_net_ipsec_data *ipsec_data;
};

static inline uint32_t
nfp_qcp_queue_offset(const struct nfp_dev_info *dev_info,
		uint16_t queue)
{
	return dev_info->qc_addr_offset + NFP_QCP_QUEUE_ADDR_SZ *
			(queue & dev_info->qc_idx_mask);
}

/* Prototypes for common NFP functions */
int nfp_net_mbox_reconfig(struct nfp_net_hw *hw, uint32_t mbox_cmd);
int nfp_net_configure(struct rte_eth_dev *dev);
int nfp_net_common_init(struct rte_pci_device *pci_dev, struct nfp_net_hw *hw);
void nfp_net_log_device_information(const struct nfp_net_hw *hw);
void nfp_net_enable_queues(struct rte_eth_dev *dev);
void nfp_net_disable_queues(struct rte_eth_dev *dev);
void nfp_net_params_setup(struct nfp_net_hw *hw);
int nfp_net_set_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr);
int nfp_configure_rx_interrupt(struct rte_eth_dev *dev,
		struct rte_intr_handle *intr_handle);
uint32_t nfp_check_offloads(struct rte_eth_dev *dev);
int nfp_net_promisc_enable(struct rte_eth_dev *dev);
int nfp_net_promisc_disable(struct rte_eth_dev *dev);
int nfp_net_allmulticast_enable(struct rte_eth_dev *dev);
int nfp_net_allmulticast_disable(struct rte_eth_dev *dev);
int nfp_net_link_update_common(struct rte_eth_dev *dev,
		struct nfp_net_hw *hw,
		struct rte_eth_link *link,
		uint32_t link_status);
int nfp_net_link_update(struct rte_eth_dev *dev,
		__rte_unused int wait_to_complete);
int nfp_net_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
int nfp_net_stats_reset(struct rte_eth_dev *dev);
uint32_t nfp_net_xstats_size(const struct rte_eth_dev *dev);
int nfp_net_xstats_get_names(struct rte_eth_dev *dev,
		struct rte_eth_xstat_name *xstats_names,
		unsigned int size __rte_unused);
int nfp_net_xstats_get(struct rte_eth_dev *dev,
		struct rte_eth_xstat *xstats,
		unsigned int n __rte_unused);
int nfp_net_xstats_get_names_by_id(struct rte_eth_dev *dev,
		const uint64_t *ids,
		struct rte_eth_xstat_name *xstats_names,
		unsigned int size);
int nfp_net_xstats_get_by_id(struct rte_eth_dev *dev,
		const uint64_t *ids,
		uint64_t *values,
		unsigned int n);
int nfp_net_xstats_reset(struct rte_eth_dev *dev);
int nfp_net_infos_get(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info);
const uint32_t *nfp_net_supported_ptypes_get(struct rte_eth_dev *dev);
int nfp_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id);
int nfp_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id);
void nfp_net_params_setup(struct nfp_net_hw *hw);
void nfp_net_cfg_queue_setup(struct nfp_net_hw *hw);
void nfp_net_irq_unmask(struct rte_eth_dev *dev);
void nfp_net_dev_interrupt_handler(void *param);
void nfp_net_dev_interrupt_delayed_handler(void *param);
int nfp_net_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
int nfp_net_vlan_offload_set(struct rte_eth_dev *dev, int mask);
int nfp_net_reta_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size);
int nfp_net_reta_query(struct rte_eth_dev *dev,
		struct rte_eth_rss_reta_entry64 *reta_conf,
		uint16_t reta_size);
int nfp_net_rss_hash_update(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf);
int nfp_net_rss_hash_conf_get(struct rte_eth_dev *dev,
		struct rte_eth_rss_conf *rss_conf);
int nfp_net_rss_config_default(struct rte_eth_dev *dev);
void nfp_net_stop_rx_queue(struct rte_eth_dev *dev);
void nfp_net_close_rx_queue(struct rte_eth_dev *dev);
void nfp_net_stop_tx_queue(struct rte_eth_dev *dev);
void nfp_net_close_tx_queue(struct rte_eth_dev *dev);
int nfp_net_set_vxlan_port(struct nfp_net_hw *hw, size_t idx, uint16_t port);
void nfp_net_rx_desc_limits(struct nfp_net_hw *hw,
		uint16_t *min_rx_desc,
		uint16_t *max_rx_desc);
void nfp_net_tx_desc_limits(struct nfp_net_hw *hw,
		uint16_t *min_tx_desc,
		uint16_t *max_tx_desc);
int nfp_net_check_dma_mask(struct nfp_net_hw *hw, char *name);
void nfp_net_init_metadata_format(struct nfp_net_hw *hw);
void nfp_net_cfg_read_version(struct nfp_net_hw *hw);
int nfp_net_firmware_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size);
bool nfp_net_is_valid_nfd_version(struct nfp_net_fw_ver version);
struct nfp_net_hw *nfp_net_get_hw(const struct rte_eth_dev *dev);
int nfp_net_stop(struct rte_eth_dev *dev);
int nfp_net_flow_ctrl_get(struct rte_eth_dev *dev,
		struct rte_eth_fc_conf *fc_conf);
int nfp_net_flow_ctrl_set(struct rte_eth_dev *dev,
		struct rte_eth_fc_conf *fc_conf);

#define NFP_PRIV_TO_APP_FW_NIC(app_fw_priv)\
	((struct nfp_app_fw_nic *)app_fw_priv)

#define NFP_PRIV_TO_APP_FW_FLOWER(app_fw_priv)\
	((struct nfp_app_fw_flower *)app_fw_priv)

#endif /* __NFP_NET_COMMON_H__ */
