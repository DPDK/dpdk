/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#include <rte_eal.h>
#include <rte_string_fns.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_alarm.h>
#include <rte_dev.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_hash_crc.h>

#include "i40e_logs.h"
#include "base/i40e_prototype.h"
#include "base/i40e_adminq_cmd.h"
#include "base/i40e_type.h"
#include "base/i40e_register.h"
#include "base/i40e_dcb.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"
#include "i40e_pf.h"
#include "i40e_regs.h"

#define ETH_I40E_FLOATING_VEB_ARG	"enable_floating_veb"
#define ETH_I40E_FLOATING_VEB_LIST_ARG	"floating_veb_list"

#define I40E_CLEAR_PXE_WAIT_MS     200

/* Maximun number of capability elements */
#define I40E_MAX_CAP_ELE_NUM       128

/* Wait count and interval */
#define I40E_CHK_Q_ENA_COUNT       1000
#define I40E_CHK_Q_ENA_INTERVAL_US 1000

/* Maximun number of VSI */
#define I40E_MAX_NUM_VSIS          (384UL)

#define I40E_PRE_TX_Q_CFG_WAIT_US       10 /* 10 us */

/* Flow control default timer */
#define I40E_DEFAULT_PAUSE_TIME 0xFFFFU

/* Flow control default high water */
#define I40E_DEFAULT_HIGH_WATER (0x1C40/1024)

/* Flow control default low water */
#define I40E_DEFAULT_LOW_WATER  (0x1A40/1024)

/* Flow control enable fwd bit */
#define I40E_PRTMAC_FWD_CTRL   0x00000001

/* Receive Packet Buffer size */
#define I40E_RXPBSIZE (968 * 1024)

/* Kilobytes shift */
#define I40E_KILOSHIFT 10

/* Receive Average Packet Size in Byte*/
#define I40E_PACKET_AVERAGE_SIZE 128

/* Mask of PF interrupt causes */
#define I40E_PFINT_ICR0_ENA_MASK ( \
		I40E_PFINT_ICR0_ENA_ECC_ERR_MASK | \
		I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK | \
		I40E_PFINT_ICR0_ENA_GRST_MASK | \
		I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK | \
		I40E_PFINT_ICR0_ENA_STORM_DETECT_MASK | \
		I40E_PFINT_ICR0_ENA_HMC_ERR_MASK | \
		I40E_PFINT_ICR0_ENA_PE_CRITERR_MASK | \
		I40E_PFINT_ICR0_ENA_VFLR_MASK | \
		I40E_PFINT_ICR0_ENA_ADMINQ_MASK)

#define I40E_FLOW_TYPES ( \
	(1UL << RTE_ETH_FLOW_FRAG_IPV4) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV4_TCP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV4_UDP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV4_SCTP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV4_OTHER) | \
	(1UL << RTE_ETH_FLOW_FRAG_IPV6) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV6_TCP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV6_UDP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV6_SCTP) | \
	(1UL << RTE_ETH_FLOW_NONFRAG_IPV6_OTHER) | \
	(1UL << RTE_ETH_FLOW_L2_PAYLOAD))

/* Additional timesync values. */
#define I40E_PTP_40GB_INCVAL     0x0199999999ULL
#define I40E_PTP_10GB_INCVAL     0x0333333333ULL
#define I40E_PTP_1GB_INCVAL      0x2000000000ULL
#define I40E_PRTTSYN_TSYNENA     0x80000000
#define I40E_PRTTSYN_TSYNTYPE    0x0e000000
#define I40E_CYCLECOUNTER_MASK   0xffffffffffffffffULL

#define I40E_MAX_PERCENT            100
#define I40E_DEFAULT_DCB_APP_NUM    1
#define I40E_DEFAULT_DCB_APP_PRIO   3

/**
 * Below are values for writing un-exposed registers suggested
 * by silicon experts
 */
/* Destination MAC address */
#define I40E_REG_INSET_L2_DMAC                   0xE000000000000000ULL
/* Source MAC address */
#define I40E_REG_INSET_L2_SMAC                   0x1C00000000000000ULL
/* Outer (S-Tag) VLAN tag in the outer L2 header */
#define I40E_REG_INSET_L2_OUTER_VLAN             0x0000000004000000ULL
/* Inner (C-Tag) or single VLAN tag in the outer L2 header */
#define I40E_REG_INSET_L2_INNER_VLAN             0x0080000000000000ULL
/* Single VLAN tag in the inner L2 header */
#define I40E_REG_INSET_TUNNEL_VLAN               0x0100000000000000ULL
/* Source IPv4 address */
#define I40E_REG_INSET_L3_SRC_IP4                0x0001800000000000ULL
/* Destination IPv4 address */
#define I40E_REG_INSET_L3_DST_IP4                0x0000001800000000ULL
/* Source IPv4 address for X722 */
#define I40E_X722_REG_INSET_L3_SRC_IP4           0x0006000000000000ULL
/* Destination IPv4 address for X722 */
#define I40E_X722_REG_INSET_L3_DST_IP4           0x0000060000000000ULL
/* IPv4 Protocol for X722 */
#define I40E_X722_REG_INSET_L3_IP4_PROTO         0x0010000000000000ULL
/* IPv4 Time to Live for X722 */
#define I40E_X722_REG_INSET_L3_IP4_TTL           0x0010000000000000ULL
/* IPv4 Type of Service (TOS) */
#define I40E_REG_INSET_L3_IP4_TOS                0x0040000000000000ULL
/* IPv4 Protocol */
#define I40E_REG_INSET_L3_IP4_PROTO              0x0004000000000000ULL
/* IPv4 Time to Live */
#define I40E_REG_INSET_L3_IP4_TTL                0x0004000000000000ULL
/* Source IPv6 address */
#define I40E_REG_INSET_L3_SRC_IP6                0x0007F80000000000ULL
/* Destination IPv6 address */
#define I40E_REG_INSET_L3_DST_IP6                0x000007F800000000ULL
/* IPv6 Traffic Class (TC) */
#define I40E_REG_INSET_L3_IP6_TC                 0x0040000000000000ULL
/* IPv6 Next Header */
#define I40E_REG_INSET_L3_IP6_NEXT_HDR           0x0008000000000000ULL
/* IPv6 Hop Limit */
#define I40E_REG_INSET_L3_IP6_HOP_LIMIT          0x0008000000000000ULL
/* Source L4 port */
#define I40E_REG_INSET_L4_SRC_PORT               0x0000000400000000ULL
/* Destination L4 port */
#define I40E_REG_INSET_L4_DST_PORT               0x0000000200000000ULL
/* SCTP verification tag */
#define I40E_REG_INSET_L4_SCTP_VERIFICATION_TAG  0x0000000180000000ULL
/* Inner destination MAC address (MAC-in-UDP/MAC-in-GRE)*/
#define I40E_REG_INSET_TUNNEL_L2_INNER_DST_MAC   0x0000000001C00000ULL
/* Source port of tunneling UDP */
#define I40E_REG_INSET_TUNNEL_L4_UDP_SRC_PORT    0x0000000000200000ULL
/* Destination port of tunneling UDP */
#define I40E_REG_INSET_TUNNEL_L4_UDP_DST_PORT    0x0000000000100000ULL
/* UDP Tunneling ID, NVGRE/GRE key */
#define I40E_REG_INSET_TUNNEL_ID                 0x00000000000C0000ULL
/* Last ether type */
#define I40E_REG_INSET_LAST_ETHER_TYPE           0x0000000000004000ULL
/* Tunneling outer destination IPv4 address */
#define I40E_REG_INSET_TUNNEL_L3_DST_IP4         0x00000000000000C0ULL
/* Tunneling outer destination IPv6 address */
#define I40E_REG_INSET_TUNNEL_L3_DST_IP6         0x0000000000003FC0ULL
/* 1st word of flex payload */
#define I40E_REG_INSET_FLEX_PAYLOAD_WORD1        0x0000000000002000ULL
/* 2nd word of flex payload */
#define I40E_REG_INSET_FLEX_PAYLOAD_WORD2        0x0000000000001000ULL
/* 3rd word of flex payload */
#define I40E_REG_INSET_FLEX_PAYLOAD_WORD3        0x0000000000000800ULL
/* 4th word of flex payload */
#define I40E_REG_INSET_FLEX_PAYLOAD_WORD4        0x0000000000000400ULL
/* 5th word of flex payload */
#define I40E_REG_INSET_FLEX_PAYLOAD_WORD5        0x0000000000000200ULL
/* 6th word of flex payload */
#define I40E_REG_INSET_FLEX_PAYLOAD_WORD6        0x0000000000000100ULL
/* 7th word of flex payload */
#define I40E_REG_INSET_FLEX_PAYLOAD_WORD7        0x0000000000000080ULL
/* 8th word of flex payload */
#define I40E_REG_INSET_FLEX_PAYLOAD_WORD8        0x0000000000000040ULL
/* all 8 words flex payload */
#define I40E_REG_INSET_FLEX_PAYLOAD_WORDS        0x0000000000003FC0ULL
#define I40E_REG_INSET_MASK_DEFAULT              0x0000000000000000ULL

#define I40E_TRANSLATE_INSET 0
#define I40E_TRANSLATE_REG   1

#define I40E_INSET_IPV4_TOS_MASK        0x0009FF00UL
#define I40E_INSET_IPv4_TTL_MASK        0x000D00FFUL
#define I40E_INSET_IPV4_PROTO_MASK      0x000DFF00UL
#define I40E_INSET_IPV6_TC_MASK         0x0009F00FUL
#define I40E_INSET_IPV6_HOP_LIMIT_MASK  0x000CFF00UL
#define I40E_INSET_IPV6_NEXT_HDR_MASK   0x000C00FFUL

/* PCI offset for querying capability */
#define PCI_DEV_CAP_REG            0xA4
/* PCI offset for enabling/disabling Extended Tag */
#define PCI_DEV_CTRL_REG           0xA8
/* Bit mask of Extended Tag capability */
#define PCI_DEV_CAP_EXT_TAG_MASK   0x20
/* Bit shift of Extended Tag enable/disable */
#define PCI_DEV_CTRL_EXT_TAG_SHIFT 8
/* Bit mask of Extended Tag enable/disable */
#define PCI_DEV_CTRL_EXT_TAG_MASK  (1 << PCI_DEV_CTRL_EXT_TAG_SHIFT)

static int eth_i40e_dev_init(struct rte_eth_dev *eth_dev);
static int eth_i40e_dev_uninit(struct rte_eth_dev *eth_dev);
static int i40e_dev_configure(struct rte_eth_dev *dev);
static int i40e_dev_start(struct rte_eth_dev *dev);
static void i40e_dev_stop(struct rte_eth_dev *dev);
static void i40e_dev_close(struct rte_eth_dev *dev);
static void i40e_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void i40e_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void i40e_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void i40e_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int i40e_dev_set_link_up(struct rte_eth_dev *dev);
static int i40e_dev_set_link_down(struct rte_eth_dev *dev);
static void i40e_dev_stats_get(struct rte_eth_dev *dev,
			       struct rte_eth_stats *stats);
static int i40e_dev_xstats_get(struct rte_eth_dev *dev,
			       struct rte_eth_xstat *xstats, unsigned n);
static int i40e_dev_xstats_get_names(struct rte_eth_dev *dev,
				     struct rte_eth_xstat_name *xstats_names,
				     unsigned limit);
static void i40e_dev_stats_reset(struct rte_eth_dev *dev);
static int i40e_dev_queue_stats_mapping_set(struct rte_eth_dev *dev,
					    uint16_t queue_id,
					    uint8_t stat_idx,
					    uint8_t is_rx);
static int i40e_fw_version_get(struct rte_eth_dev *dev,
				char *fw_version, size_t fw_size);
static void i40e_dev_info_get(struct rte_eth_dev *dev,
			      struct rte_eth_dev_info *dev_info);
static int i40e_vlan_filter_set(struct rte_eth_dev *dev,
				uint16_t vlan_id,
				int on);
static int i40e_vlan_tpid_set(struct rte_eth_dev *dev,
			      enum rte_vlan_type vlan_type,
			      uint16_t tpid);
static void i40e_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static void i40e_vlan_strip_queue_set(struct rte_eth_dev *dev,
				      uint16_t queue,
				      int on);
static int i40e_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t pvid, int on);
static int i40e_dev_led_on(struct rte_eth_dev *dev);
static int i40e_dev_led_off(struct rte_eth_dev *dev);
static int i40e_flow_ctrl_get(struct rte_eth_dev *dev,
			      struct rte_eth_fc_conf *fc_conf);
static int i40e_flow_ctrl_set(struct rte_eth_dev *dev,
			      struct rte_eth_fc_conf *fc_conf);
static int i40e_priority_flow_ctrl_set(struct rte_eth_dev *dev,
				       struct rte_eth_pfc_conf *pfc_conf);
static int i40e_macaddr_add(struct rte_eth_dev *dev,
			    struct ether_addr *mac_addr,
			    uint32_t index,
			    uint32_t pool);
static void i40e_macaddr_remove(struct rte_eth_dev *dev, uint32_t index);
static int i40e_dev_rss_reta_update(struct rte_eth_dev *dev,
				    struct rte_eth_rss_reta_entry64 *reta_conf,
				    uint16_t reta_size);
static int i40e_dev_rss_reta_query(struct rte_eth_dev *dev,
				   struct rte_eth_rss_reta_entry64 *reta_conf,
				   uint16_t reta_size);

static int i40e_get_cap(struct i40e_hw *hw);
static int i40e_pf_parameter_init(struct rte_eth_dev *dev);
static int i40e_pf_setup(struct i40e_pf *pf);
static int i40e_dev_rxtx_init(struct i40e_pf *pf);
static int i40e_vmdq_setup(struct rte_eth_dev *dev);
static int i40e_dcb_init_configure(struct rte_eth_dev *dev, bool sw_dcb);
static int i40e_dcb_setup(struct rte_eth_dev *dev);
static void i40e_stat_update_32(struct i40e_hw *hw, uint32_t reg,
		bool offset_loaded, uint64_t *offset, uint64_t *stat);
static void i40e_stat_update_48(struct i40e_hw *hw,
			       uint32_t hireg,
			       uint32_t loreg,
			       bool offset_loaded,
			       uint64_t *offset,
			       uint64_t *stat);
static void i40e_pf_config_irq0(struct i40e_hw *hw, bool no_queue);
static void i40e_dev_interrupt_handler(void *param);
static int i40e_res_pool_init(struct i40e_res_pool_info *pool,
				uint32_t base, uint32_t num);
static void i40e_res_pool_destroy(struct i40e_res_pool_info *pool);
static int i40e_res_pool_free(struct i40e_res_pool_info *pool,
			uint32_t base);
static int i40e_res_pool_alloc(struct i40e_res_pool_info *pool,
			uint16_t num);
static int i40e_dev_init_vlan(struct rte_eth_dev *dev);
static int i40e_veb_release(struct i40e_veb *veb);
static struct i40e_veb *i40e_veb_setup(struct i40e_pf *pf,
						struct i40e_vsi *vsi);
static int i40e_pf_config_mq_rx(struct i40e_pf *pf);
static int i40e_vsi_config_double_vlan(struct i40e_vsi *vsi, int on);
static inline int i40e_find_all_mac_for_vlan(struct i40e_vsi *vsi,
					     struct i40e_macvlan_filter *mv_f,
					     int num,
					     uint16_t vlan);
static int i40e_vsi_remove_all_macvlan_filter(struct i40e_vsi *vsi);
static int i40e_dev_rss_hash_update(struct rte_eth_dev *dev,
				    struct rte_eth_rss_conf *rss_conf);
static int i40e_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
				      struct rte_eth_rss_conf *rss_conf);
static int i40e_dev_udp_tunnel_port_add(struct rte_eth_dev *dev,
					struct rte_eth_udp_tunnel *udp_tunnel);
static int i40e_dev_udp_tunnel_port_del(struct rte_eth_dev *dev,
					struct rte_eth_udp_tunnel *udp_tunnel);
static void i40e_filter_input_set_init(struct i40e_pf *pf);
static int i40e_ethertype_filter_handle(struct rte_eth_dev *dev,
				enum rte_filter_op filter_op,
				void *arg);
static int i40e_dev_filter_ctrl(struct rte_eth_dev *dev,
				enum rte_filter_type filter_type,
				enum rte_filter_op filter_op,
				void *arg);
static int i40e_dev_get_dcb_info(struct rte_eth_dev *dev,
				  struct rte_eth_dcb_info *dcb_info);
static int i40e_dev_sync_phy_type(struct i40e_hw *hw);
static void i40e_configure_registers(struct i40e_hw *hw);
static void i40e_hw_init(struct rte_eth_dev *dev);
static int i40e_config_qinq(struct i40e_hw *hw, struct i40e_vsi *vsi);
static int i40e_mirror_rule_set(struct rte_eth_dev *dev,
			struct rte_eth_mirror_conf *mirror_conf,
			uint8_t sw_id, uint8_t on);
static int i40e_mirror_rule_reset(struct rte_eth_dev *dev, uint8_t sw_id);

static int i40e_timesync_enable(struct rte_eth_dev *dev);
static int i40e_timesync_disable(struct rte_eth_dev *dev);
static int i40e_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
					   struct timespec *timestamp,
					   uint32_t flags);
static int i40e_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
					   struct timespec *timestamp);
static void i40e_read_stats_registers(struct i40e_pf *pf, struct i40e_hw *hw);

static int i40e_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta);

static int i40e_timesync_read_time(struct rte_eth_dev *dev,
				   struct timespec *timestamp);
static int i40e_timesync_write_time(struct rte_eth_dev *dev,
				    const struct timespec *timestamp);

static int i40e_dev_rx_queue_intr_enable(struct rte_eth_dev *dev,
					 uint16_t queue_id);
static int i40e_dev_rx_queue_intr_disable(struct rte_eth_dev *dev,
					  uint16_t queue_id);

static int i40e_get_regs(struct rte_eth_dev *dev,
			 struct rte_dev_reg_info *regs);

static int i40e_get_eeprom_length(struct rte_eth_dev *dev);

static int i40e_get_eeprom(struct rte_eth_dev *dev,
			   struct rte_dev_eeprom_info *eeprom);

static void i40e_set_default_mac_addr(struct rte_eth_dev *dev,
				      struct ether_addr *mac_addr);

static int i40e_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);

static int i40e_ethertype_filter_convert(
	const struct rte_eth_ethertype_filter *input,
	struct i40e_ethertype_filter *filter);
static int i40e_sw_ethertype_filter_insert(struct i40e_pf *pf,
				   struct i40e_ethertype_filter *filter);

static int i40e_tunnel_filter_convert(
	struct i40e_aqc_add_rm_cloud_filt_elem_ext *cld_filter,
	struct i40e_tunnel_filter *tunnel_filter);
static int i40e_sw_tunnel_filter_insert(struct i40e_pf *pf,
				struct i40e_tunnel_filter *tunnel_filter);
static int i40e_cloud_filter_qinq_create(struct i40e_pf *pf);

static void i40e_ethertype_filter_restore(struct i40e_pf *pf);
static void i40e_tunnel_filter_restore(struct i40e_pf *pf);
static void i40e_filter_restore(struct i40e_pf *pf);
static void i40e_notify_all_vfs_link_status(struct rte_eth_dev *dev);

int i40e_logtype_init;
int i40e_logtype_driver;

static const struct rte_pci_id pci_id_i40e_map[] = {
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_XL710) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QEMU) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_B) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_C) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_A) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_B) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_C) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_20G_KR2) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_20G_KR2_A) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T4) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_25G_B) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_25G_SFP28) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_X722_A0) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_X722) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_X722) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_X722) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_1G_BASE_T_X722) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T_X722) },
	{ RTE_PCI_DEVICE(I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_I_X722) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops i40e_eth_dev_ops = {
	.dev_configure                = i40e_dev_configure,
	.dev_start                    = i40e_dev_start,
	.dev_stop                     = i40e_dev_stop,
	.dev_close                    = i40e_dev_close,
	.promiscuous_enable           = i40e_dev_promiscuous_enable,
	.promiscuous_disable          = i40e_dev_promiscuous_disable,
	.allmulticast_enable          = i40e_dev_allmulticast_enable,
	.allmulticast_disable         = i40e_dev_allmulticast_disable,
	.dev_set_link_up              = i40e_dev_set_link_up,
	.dev_set_link_down            = i40e_dev_set_link_down,
	.link_update                  = i40e_dev_link_update,
	.stats_get                    = i40e_dev_stats_get,
	.xstats_get                   = i40e_dev_xstats_get,
	.xstats_get_names             = i40e_dev_xstats_get_names,
	.stats_reset                  = i40e_dev_stats_reset,
	.xstats_reset                 = i40e_dev_stats_reset,
	.queue_stats_mapping_set      = i40e_dev_queue_stats_mapping_set,
	.fw_version_get               = i40e_fw_version_get,
	.dev_infos_get                = i40e_dev_info_get,
	.dev_supported_ptypes_get     = i40e_dev_supported_ptypes_get,
	.vlan_filter_set              = i40e_vlan_filter_set,
	.vlan_tpid_set                = i40e_vlan_tpid_set,
	.vlan_offload_set             = i40e_vlan_offload_set,
	.vlan_strip_queue_set         = i40e_vlan_strip_queue_set,
	.vlan_pvid_set                = i40e_vlan_pvid_set,
	.rx_queue_start               = i40e_dev_rx_queue_start,
	.rx_queue_stop                = i40e_dev_rx_queue_stop,
	.tx_queue_start               = i40e_dev_tx_queue_start,
	.tx_queue_stop                = i40e_dev_tx_queue_stop,
	.rx_queue_setup               = i40e_dev_rx_queue_setup,
	.rx_queue_intr_enable         = i40e_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable        = i40e_dev_rx_queue_intr_disable,
	.rx_queue_release             = i40e_dev_rx_queue_release,
	.rx_queue_count               = i40e_dev_rx_queue_count,
	.rx_descriptor_done           = i40e_dev_rx_descriptor_done,
	.rx_descriptor_status         = i40e_dev_rx_descriptor_status,
	.tx_descriptor_status         = i40e_dev_tx_descriptor_status,
	.tx_queue_setup               = i40e_dev_tx_queue_setup,
	.tx_queue_release             = i40e_dev_tx_queue_release,
	.dev_led_on                   = i40e_dev_led_on,
	.dev_led_off                  = i40e_dev_led_off,
	.flow_ctrl_get                = i40e_flow_ctrl_get,
	.flow_ctrl_set                = i40e_flow_ctrl_set,
	.priority_flow_ctrl_set       = i40e_priority_flow_ctrl_set,
	.mac_addr_add                 = i40e_macaddr_add,
	.mac_addr_remove              = i40e_macaddr_remove,
	.reta_update                  = i40e_dev_rss_reta_update,
	.reta_query                   = i40e_dev_rss_reta_query,
	.rss_hash_update              = i40e_dev_rss_hash_update,
	.rss_hash_conf_get            = i40e_dev_rss_hash_conf_get,
	.udp_tunnel_port_add          = i40e_dev_udp_tunnel_port_add,
	.udp_tunnel_port_del          = i40e_dev_udp_tunnel_port_del,
	.filter_ctrl                  = i40e_dev_filter_ctrl,
	.rxq_info_get                 = i40e_rxq_info_get,
	.txq_info_get                 = i40e_txq_info_get,
	.mirror_rule_set              = i40e_mirror_rule_set,
	.mirror_rule_reset            = i40e_mirror_rule_reset,
	.timesync_enable              = i40e_timesync_enable,
	.timesync_disable             = i40e_timesync_disable,
	.timesync_read_rx_timestamp   = i40e_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp   = i40e_timesync_read_tx_timestamp,
	.get_dcb_info                 = i40e_dev_get_dcb_info,
	.timesync_adjust_time         = i40e_timesync_adjust_time,
	.timesync_read_time           = i40e_timesync_read_time,
	.timesync_write_time          = i40e_timesync_write_time,
	.get_reg                      = i40e_get_regs,
	.get_eeprom_length            = i40e_get_eeprom_length,
	.get_eeprom                   = i40e_get_eeprom,
	.mac_addr_set                 = i40e_set_default_mac_addr,
	.mtu_set                      = i40e_dev_mtu_set,
};

/* store statistics names and its offset in stats structure */
struct rte_i40e_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned offset;
};

static const struct rte_i40e_xstats_name_off rte_i40e_stats_strings[] = {
	{"rx_unicast_packets", offsetof(struct i40e_eth_stats, rx_unicast)},
	{"rx_multicast_packets", offsetof(struct i40e_eth_stats, rx_multicast)},
	{"rx_broadcast_packets", offsetof(struct i40e_eth_stats, rx_broadcast)},
	{"rx_dropped", offsetof(struct i40e_eth_stats, rx_discards)},
	{"rx_unknown_protocol_packets", offsetof(struct i40e_eth_stats,
		rx_unknown_protocol)},
	{"tx_unicast_packets", offsetof(struct i40e_eth_stats, tx_unicast)},
	{"tx_multicast_packets", offsetof(struct i40e_eth_stats, tx_multicast)},
	{"tx_broadcast_packets", offsetof(struct i40e_eth_stats, tx_broadcast)},
	{"tx_dropped", offsetof(struct i40e_eth_stats, tx_discards)},
};

#define I40E_NB_ETH_XSTATS (sizeof(rte_i40e_stats_strings) / \
		sizeof(rte_i40e_stats_strings[0]))

static const struct rte_i40e_xstats_name_off rte_i40e_hw_port_strings[] = {
	{"tx_link_down_dropped", offsetof(struct i40e_hw_port_stats,
		tx_dropped_link_down)},
	{"rx_crc_errors", offsetof(struct i40e_hw_port_stats, crc_errors)},
	{"rx_illegal_byte_errors", offsetof(struct i40e_hw_port_stats,
		illegal_bytes)},
	{"rx_error_bytes", offsetof(struct i40e_hw_port_stats, error_bytes)},
	{"mac_local_errors", offsetof(struct i40e_hw_port_stats,
		mac_local_faults)},
	{"mac_remote_errors", offsetof(struct i40e_hw_port_stats,
		mac_remote_faults)},
	{"rx_length_errors", offsetof(struct i40e_hw_port_stats,
		rx_length_errors)},
	{"tx_xon_packets", offsetof(struct i40e_hw_port_stats, link_xon_tx)},
	{"rx_xon_packets", offsetof(struct i40e_hw_port_stats, link_xon_rx)},
	{"tx_xoff_packets", offsetof(struct i40e_hw_port_stats, link_xoff_tx)},
	{"rx_xoff_packets", offsetof(struct i40e_hw_port_stats, link_xoff_rx)},
	{"rx_size_64_packets", offsetof(struct i40e_hw_port_stats, rx_size_64)},
	{"rx_size_65_to_127_packets", offsetof(struct i40e_hw_port_stats,
		rx_size_127)},
	{"rx_size_128_to_255_packets", offsetof(struct i40e_hw_port_stats,
		rx_size_255)},
	{"rx_size_256_to_511_packets", offsetof(struct i40e_hw_port_stats,
		rx_size_511)},
	{"rx_size_512_to_1023_packets", offsetof(struct i40e_hw_port_stats,
		rx_size_1023)},
	{"rx_size_1024_to_1522_packets", offsetof(struct i40e_hw_port_stats,
		rx_size_1522)},
	{"rx_size_1523_to_max_packets", offsetof(struct i40e_hw_port_stats,
		rx_size_big)},
	{"rx_undersized_errors", offsetof(struct i40e_hw_port_stats,
		rx_undersize)},
	{"rx_oversize_errors", offsetof(struct i40e_hw_port_stats,
		rx_oversize)},
	{"rx_mac_short_dropped", offsetof(struct i40e_hw_port_stats,
		mac_short_packet_dropped)},
	{"rx_fragmented_errors", offsetof(struct i40e_hw_port_stats,
		rx_fragments)},
	{"rx_jabber_errors", offsetof(struct i40e_hw_port_stats, rx_jabber)},
	{"tx_size_64_packets", offsetof(struct i40e_hw_port_stats, tx_size_64)},
	{"tx_size_65_to_127_packets", offsetof(struct i40e_hw_port_stats,
		tx_size_127)},
	{"tx_size_128_to_255_packets", offsetof(struct i40e_hw_port_stats,
		tx_size_255)},
	{"tx_size_256_to_511_packets", offsetof(struct i40e_hw_port_stats,
		tx_size_511)},
	{"tx_size_512_to_1023_packets", offsetof(struct i40e_hw_port_stats,
		tx_size_1023)},
	{"tx_size_1024_to_1522_packets", offsetof(struct i40e_hw_port_stats,
		tx_size_1522)},
	{"tx_size_1523_to_max_packets", offsetof(struct i40e_hw_port_stats,
		tx_size_big)},
	{"rx_flow_director_atr_match_packets",
		offsetof(struct i40e_hw_port_stats, fd_atr_match)},
	{"rx_flow_director_sb_match_packets",
		offsetof(struct i40e_hw_port_stats, fd_sb_match)},
	{"tx_low_power_idle_status", offsetof(struct i40e_hw_port_stats,
		tx_lpi_status)},
	{"rx_low_power_idle_status", offsetof(struct i40e_hw_port_stats,
		rx_lpi_status)},
	{"tx_low_power_idle_count", offsetof(struct i40e_hw_port_stats,
		tx_lpi_count)},
	{"rx_low_power_idle_count", offsetof(struct i40e_hw_port_stats,
		rx_lpi_count)},
};

#define I40E_NB_HW_PORT_XSTATS (sizeof(rte_i40e_hw_port_strings) / \
		sizeof(rte_i40e_hw_port_strings[0]))

static const struct rte_i40e_xstats_name_off rte_i40e_rxq_prio_strings[] = {
	{"xon_packets", offsetof(struct i40e_hw_port_stats,
		priority_xon_rx)},
	{"xoff_packets", offsetof(struct i40e_hw_port_stats,
		priority_xoff_rx)},
};

#define I40E_NB_RXQ_PRIO_XSTATS (sizeof(rte_i40e_rxq_prio_strings) / \
		sizeof(rte_i40e_rxq_prio_strings[0]))

static const struct rte_i40e_xstats_name_off rte_i40e_txq_prio_strings[] = {
	{"xon_packets", offsetof(struct i40e_hw_port_stats,
		priority_xon_tx)},
	{"xoff_packets", offsetof(struct i40e_hw_port_stats,
		priority_xoff_tx)},
	{"xon_to_xoff_packets", offsetof(struct i40e_hw_port_stats,
		priority_xon_2_xoff)},
};

#define I40E_NB_TXQ_PRIO_XSTATS (sizeof(rte_i40e_txq_prio_strings) / \
		sizeof(rte_i40e_txq_prio_strings[0]))

static int eth_i40e_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct i40e_adapter), eth_i40e_dev_init);
}

static int eth_i40e_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_i40e_dev_uninit);
}

static struct rte_pci_driver rte_i40e_pmd = {
	.id_table = pci_id_i40e_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_i40e_pci_probe,
	.remove = eth_i40e_pci_remove,
};

static inline int
rte_i40e_dev_atomic_read_link_status(struct rte_eth_dev *dev,
				     struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &(dev->data->dev_link);

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static inline int
rte_i40e_dev_atomic_write_link_status(struct rte_eth_dev *dev,
				      struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &(dev->data->dev_link);
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

RTE_PMD_REGISTER_PCI(net_i40e, rte_i40e_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_i40e, pci_id_i40e_map);
RTE_PMD_REGISTER_KMOD_DEP(net_i40e, "* igb_uio | uio_pci_generic | vfio-pci");

#ifndef I40E_GLQF_ORT
#define I40E_GLQF_ORT(_i)    (0x00268900 + ((_i) * 4))
#endif
#ifndef I40E_GLQF_PIT
#define I40E_GLQF_PIT(_i)    (0x00268C80 + ((_i) * 4))
#endif
#ifndef I40E_GLQF_L3_MAP
#define I40E_GLQF_L3_MAP(_i) (0x0026C700 + ((_i) * 4))
#endif

static inline void i40e_GLQF_reg_init(struct i40e_hw *hw)
{
	/*
	 * Initialize registers for flexible payload, which should be set by NVM.
	 * This should be removed from code once it is fixed in NVM.
	 */
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(18), 0x00000030);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(19), 0x00000030);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(26), 0x0000002B);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(30), 0x0000002B);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(33), 0x000000E0);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(34), 0x000000E3);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(35), 0x000000E6);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(20), 0x00000031);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(23), 0x00000031);
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(63), 0x0000002D);
	I40E_WRITE_REG(hw, I40E_GLQF_PIT(16), 0x00007480);
	I40E_WRITE_REG(hw, I40E_GLQF_PIT(17), 0x00007440);

	/* Initialize registers for parsing packet type of QinQ */
	I40E_WRITE_REG(hw, I40E_GLQF_ORT(40), 0x00000029);
	I40E_WRITE_REG(hw, I40E_GLQF_PIT(9), 0x00009420);
}

#define I40E_FLOW_CONTROL_ETHERTYPE  0x8808

/*
 * Add a ethertype filter to drop all flow control frames transmitted
 * from VSIs.
*/
static void
i40e_add_tx_flow_control_drop_filter(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint16_t flags = I40E_AQC_ADD_CONTROL_PACKET_FLAGS_IGNORE_MAC |
			I40E_AQC_ADD_CONTROL_PACKET_FLAGS_DROP |
			I40E_AQC_ADD_CONTROL_PACKET_FLAGS_TX;
	int ret;

	ret = i40e_aq_add_rem_control_packet_filter(hw, NULL,
				I40E_FLOW_CONTROL_ETHERTYPE, flags,
				pf->main_vsi_seid, 0,
				TRUE, NULL, NULL);
	if (ret)
		PMD_INIT_LOG(ERR,
			"Failed to add filter to drop flow control frames from VSIs.");
}

static int
floating_veb_list_handler(__rte_unused const char *key,
			  const char *floating_veb_value,
			  void *opaque)
{
	int idx = 0;
	unsigned int count = 0;
	char *end = NULL;
	int min, max;
	bool *vf_floating_veb = opaque;

	while (isblank(*floating_veb_value))
		floating_veb_value++;

	/* Reset floating VEB configuration for VFs */
	for (idx = 0; idx < I40E_MAX_VF; idx++)
		vf_floating_veb[idx] = false;

	min = I40E_MAX_VF;
	do {
		while (isblank(*floating_veb_value))
			floating_veb_value++;
		if (*floating_veb_value == '\0')
			return -1;
		errno = 0;
		idx = strtoul(floating_veb_value, &end, 10);
		if (errno || end == NULL)
			return -1;
		while (isblank(*end))
			end++;
		if (*end == '-') {
			min = idx;
		} else if ((*end == ';') || (*end == '\0')) {
			max = idx;
			if (min == I40E_MAX_VF)
				min = idx;
			if (max >= I40E_MAX_VF)
				max = I40E_MAX_VF - 1;
			for (idx = min; idx <= max; idx++) {
				vf_floating_veb[idx] = true;
				count++;
			}
			min = I40E_MAX_VF;
		} else {
			return -1;
		}
		floating_veb_value = end + 1;
	} while (*end != '\0');

	if (count == 0)
		return -1;

	return 0;
}

static void
config_vf_floating_veb(struct rte_devargs *devargs,
		       uint16_t floating_veb,
		       bool *vf_floating_veb)
{
	struct rte_kvargs *kvlist;
	int i;
	const char *floating_veb_list = ETH_I40E_FLOATING_VEB_LIST_ARG;

	if (!floating_veb)
		return;
	/* All the VFs attach to the floating VEB by default
	 * when the floating VEB is enabled.
	 */
	for (i = 0; i < I40E_MAX_VF; i++)
		vf_floating_veb[i] = true;

	if (devargs == NULL)
		return;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return;

	if (!rte_kvargs_count(kvlist, floating_veb_list)) {
		rte_kvargs_free(kvlist);
		return;
	}
	/* When the floating_veb_list parameter exists, all the VFs
	 * will attach to the legacy VEB firstly, then configure VFs
	 * to the floating VEB according to the floating_veb_list.
	 */
	if (rte_kvargs_process(kvlist, floating_veb_list,
			       floating_veb_list_handler,
			       vf_floating_veb) < 0) {
		rte_kvargs_free(kvlist);
		return;
	}
	rte_kvargs_free(kvlist);
}

static int
i40e_check_floating_handler(__rte_unused const char *key,
			    const char *value,
			    __rte_unused void *opaque)
{
	if (strcmp(value, "1"))
		return -1;

	return 0;
}

static int
is_floating_veb_supported(struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	const char *floating_veb_key = ETH_I40E_FLOATING_VEB_ARG;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return 0;

	if (!rte_kvargs_count(kvlist, floating_veb_key)) {
		rte_kvargs_free(kvlist);
		return 0;
	}
	/* Floating VEB is enabled when there's key-value:
	 * enable_floating_veb=1
	 */
	if (rte_kvargs_process(kvlist, floating_veb_key,
			       i40e_check_floating_handler, NULL) < 0) {
		rte_kvargs_free(kvlist);
		return 0;
	}
	rte_kvargs_free(kvlist);

	return 1;
}

static void
config_floating_veb(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	memset(pf->floating_veb_list, 0, sizeof(pf->floating_veb_list));

	if (hw->aq.fw_maj_ver >= FLOATING_VEB_SUPPORTED_FW_MAJ) {
		pf->floating_veb =
			is_floating_veb_supported(pci_dev->device.devargs);
		config_vf_floating_veb(pci_dev->device.devargs,
				       pf->floating_veb,
				       pf->floating_veb_list);
	} else {
		pf->floating_veb = false;
	}
}

#define I40E_L2_TAGS_S_TAG_SHIFT 1
#define I40E_L2_TAGS_S_TAG_MASK I40E_MASK(0x1, I40E_L2_TAGS_S_TAG_SHIFT)

static int
i40e_init_ethtype_filter_list(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_ethertype_rule *ethertype_rule = &pf->ethertype;
	char ethertype_hash_name[RTE_HASH_NAMESIZE];
	int ret;

	struct rte_hash_parameters ethertype_hash_params = {
		.name = ethertype_hash_name,
		.entries = I40E_MAX_ETHERTYPE_FILTER_NUM,
		.key_len = sizeof(struct i40e_ethertype_filter_input),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	/* Initialize ethertype filter rule list and hash */
	TAILQ_INIT(&ethertype_rule->ethertype_list);
	snprintf(ethertype_hash_name, RTE_HASH_NAMESIZE,
		 "ethertype_%s", dev->data->name);
	ethertype_rule->hash_table = rte_hash_create(&ethertype_hash_params);
	if (!ethertype_rule->hash_table) {
		PMD_INIT_LOG(ERR, "Failed to create ethertype hash table!");
		return -EINVAL;
	}
	ethertype_rule->hash_map = rte_zmalloc("i40e_ethertype_hash_map",
				       sizeof(struct i40e_ethertype_filter *) *
				       I40E_MAX_ETHERTYPE_FILTER_NUM,
				       0);
	if (!ethertype_rule->hash_map) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for ethertype hash map!");
		ret = -ENOMEM;
		goto err_ethertype_hash_map_alloc;
	}

	return 0;

err_ethertype_hash_map_alloc:
	rte_hash_free(ethertype_rule->hash_table);

	return ret;
}

static int
i40e_init_tunnel_filter_list(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_tunnel_rule *tunnel_rule = &pf->tunnel;
	char tunnel_hash_name[RTE_HASH_NAMESIZE];
	int ret;

	struct rte_hash_parameters tunnel_hash_params = {
		.name = tunnel_hash_name,
		.entries = I40E_MAX_TUNNEL_FILTER_NUM,
		.key_len = sizeof(struct i40e_tunnel_filter_input),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	/* Initialize tunnel filter rule list and hash */
	TAILQ_INIT(&tunnel_rule->tunnel_list);
	snprintf(tunnel_hash_name, RTE_HASH_NAMESIZE,
		 "tunnel_%s", dev->data->name);
	tunnel_rule->hash_table = rte_hash_create(&tunnel_hash_params);
	if (!tunnel_rule->hash_table) {
		PMD_INIT_LOG(ERR, "Failed to create tunnel hash table!");
		return -EINVAL;
	}
	tunnel_rule->hash_map = rte_zmalloc("i40e_tunnel_hash_map",
				    sizeof(struct i40e_tunnel_filter *) *
				    I40E_MAX_TUNNEL_FILTER_NUM,
				    0);
	if (!tunnel_rule->hash_map) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for tunnel hash map!");
		ret = -ENOMEM;
		goto err_tunnel_hash_map_alloc;
	}

	return 0;

err_tunnel_hash_map_alloc:
	rte_hash_free(tunnel_rule->hash_table);

	return ret;
}

static int
i40e_init_fdir_filter_list(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	char fdir_hash_name[RTE_HASH_NAMESIZE];
	int ret;

	struct rte_hash_parameters fdir_hash_params = {
		.name = fdir_hash_name,
		.entries = I40E_MAX_FDIR_FILTER_NUM,
		.key_len = sizeof(struct rte_eth_fdir_input),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	/* Initialize flow director filter rule list and hash */
	TAILQ_INIT(&fdir_info->fdir_list);
	snprintf(fdir_hash_name, RTE_HASH_NAMESIZE,
		 "fdir_%s", dev->data->name);
	fdir_info->hash_table = rte_hash_create(&fdir_hash_params);
	if (!fdir_info->hash_table) {
		PMD_INIT_LOG(ERR, "Failed to create fdir hash table!");
		return -EINVAL;
	}
	fdir_info->hash_map = rte_zmalloc("i40e_fdir_hash_map",
					  sizeof(struct i40e_fdir_filter *) *
					  I40E_MAX_FDIR_FILTER_NUM,
					  0);
	if (!fdir_info->hash_map) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for fdir hash map!");
		ret = -ENOMEM;
		goto err_fdir_hash_map_alloc;
	}
	return 0;

err_fdir_hash_map_alloc:
	rte_hash_free(fdir_info->hash_table);

	return ret;
}

static int
eth_i40e_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev;
	struct rte_intr_handle *intr_handle;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi;
	int ret;
	uint32_t len;
	uint8_t aq_fail = 0;

	PMD_INIT_FUNC_TRACE();

	dev->dev_ops = &i40e_eth_dev_ops;
	dev->rx_pkt_burst = i40e_recv_pkts;
	dev->tx_pkt_burst = i40e_xmit_pkts;
	dev->tx_pkt_prepare = i40e_prep_pkts;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY){
		i40e_set_rx_function(dev);
		i40e_set_tx_function(dev);
		return 0;
	}
	i40e_set_default_ptype_table(dev);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	intr_handle = &pci_dev->intr_handle;

	rte_eth_copy_pci_info(dev, pci_dev);
	dev->data->dev_flags |= RTE_ETH_DEV_DETACHABLE;

	pf->adapter = I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	pf->adapter->eth_dev = dev;
	pf->dev_data = dev->data;

	hw->back = I40E_PF_TO_ADAPTER(pf);
	hw->hw_addr = (uint8_t *)(pci_dev->mem_resource[0].addr);
	if (!hw->hw_addr) {
		PMD_INIT_LOG(ERR,
			"Hardware is not available, as address is NULL");
		return -ENODEV;
	}

	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->bus.device = pci_dev->addr.devid;
	hw->bus.func = pci_dev->addr.function;
	hw->adapter_stopped = 0;

	/* Make sure all is clean before doing PF reset */
	i40e_clear_hw(hw);

	/* Initialize the hardware */
	i40e_hw_init(dev);

	/* Reset here to make sure all is clean for each PF */
	ret = i40e_pf_reset(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to reset pf: %d", ret);
		return ret;
	}

	/* Initialize the shared code (base driver) */
	ret = i40e_init_shared_code(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init shared code (base driver): %d", ret);
		return ret;
	}

	/*
	 * To work around the NVM issue, initialize registers
	 * for flexible payload and packet type of QinQ by
	 * software. It should be removed once issues are fixed
	 * in NVM.
	 */
	i40e_GLQF_reg_init(hw);

	/* Initialize the input set for filters (hash and fd) to default value */
	i40e_filter_input_set_init(pf);

	/* Initialize the parameters for adminq */
	i40e_init_adminq_parameter(hw);
	ret = i40e_init_adminq(hw);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to init adminq: %d", ret);
		return -EIO;
	}
	PMD_INIT_LOG(INFO, "FW %d.%d API %d.%d NVM %02d.%02d.%02d eetrack %04x",
		     hw->aq.fw_maj_ver, hw->aq.fw_min_ver,
		     hw->aq.api_maj_ver, hw->aq.api_min_ver,
		     ((hw->nvm.version >> 12) & 0xf),
		     ((hw->nvm.version >> 4) & 0xff),
		     (hw->nvm.version & 0xf), hw->nvm.eetrack);

	/* initialise the L3_MAP register */
	ret = i40e_aq_debug_write_register(hw, I40E_GLQF_L3_MAP(40),
				   0x00000028,	NULL);
	if (ret)
		PMD_INIT_LOG(ERR, "Failed to write L3 MAP register %d", ret);

	/* Need the special FW version to support floating VEB */
	config_floating_veb(dev);
	/* Clear PXE mode */
	i40e_clear_pxe_mode(hw);
	ret = i40e_dev_sync_phy_type(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to sync phy type: %d", ret);
		goto err_sync_phy_type;
	}
	/*
	 * On X710, performance number is far from the expectation on recent
	 * firmware versions. The fix for this issue may not be integrated in
	 * the following firmware version. So the workaround in software driver
	 * is needed. It needs to modify the initial values of 3 internal only
	 * registers. Note that the workaround can be removed when it is fixed
	 * in firmware in the future.
	 */
	i40e_configure_registers(hw);

	/* Get hw capabilities */
	ret = i40e_get_cap(hw);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to get capabilities: %d", ret);
		goto err_get_capabilities;
	}

	/* Initialize parameters for PF */
	ret = i40e_pf_parameter_init(dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to do parameter init: %d", ret);
		goto err_parameter_init;
	}

	/* Initialize the queue management */
	ret = i40e_res_pool_init(&pf->qp_pool, 0, hw->func_caps.num_tx_qp);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to init queue pool");
		goto err_qp_pool_init;
	}
	ret = i40e_res_pool_init(&pf->msix_pool, 1,
				hw->func_caps.num_msix_vectors - 1);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to init MSIX pool");
		goto err_msix_pool_init;
	}

	/* Initialize lan hmc */
	ret = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
				hw->func_caps.num_rx_qp, 0, 0);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to init lan hmc: %d", ret);
		goto err_init_lan_hmc;
	}

	/* Configure lan hmc */
	ret = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to configure lan hmc: %d", ret);
		goto err_configure_lan_hmc;
	}

	/* Get and check the mac address */
	i40e_get_mac_addr(hw, hw->mac.addr);
	if (i40e_validate_mac_addr(hw->mac.addr) != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "mac address is not valid");
		ret = -EIO;
		goto err_get_mac_addr;
	}
	/* Copy the permanent MAC address */
	ether_addr_copy((struct ether_addr *) hw->mac.addr,
			(struct ether_addr *) hw->mac.perm_addr);

	/* Disable flow control */
	hw->fc.requested_mode = I40E_FC_NONE;
	i40e_set_fc(hw, &aq_fail, TRUE);

	/* Set the global registers with default ether type value */
	ret = i40e_vlan_tpid_set(dev, ETH_VLAN_TYPE_OUTER, ETHER_TYPE_VLAN);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR,
			"Failed to set the default outer VLAN ether type");
		goto err_setup_pf_switch;
	}

	/* PF setup, which includes VSI setup */
	ret = i40e_pf_setup(pf);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to setup pf switch: %d", ret);
		goto err_setup_pf_switch;
	}

	/* reset all stats of the device, including pf and main vsi */
	i40e_dev_stats_reset(dev);

	vsi = pf->main_vsi;

	/* Disable double vlan by default */
	i40e_vsi_config_double_vlan(vsi, FALSE);

	/* Disable S-TAG identification when floating_veb is disabled */
	if (!pf->floating_veb) {
		ret = I40E_READ_REG(hw, I40E_PRT_L2TAGSEN);
		if (ret & I40E_L2_TAGS_S_TAG_MASK) {
			ret &= ~I40E_L2_TAGS_S_TAG_MASK;
			I40E_WRITE_REG(hw, I40E_PRT_L2TAGSEN, ret);
		}
	}

	if (!vsi->max_macaddrs)
		len = ETHER_ADDR_LEN;
	else
		len = ETHER_ADDR_LEN * vsi->max_macaddrs;

	/* Should be after VSI initialized */
	dev->data->mac_addrs = rte_zmalloc("i40e", len, 0);
	if (!dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR,
			"Failed to allocated memory for storing mac address");
		goto err_mac_alloc;
	}
	ether_addr_copy((struct ether_addr *)hw->mac.perm_addr,
					&dev->data->mac_addrs[0]);

	/* Init dcb to sw mode by default */
	ret = i40e_dcb_init_configure(dev, TRUE);
	if (ret != I40E_SUCCESS) {
		PMD_INIT_LOG(INFO, "Failed to init dcb.");
		pf->flags &= ~I40E_FLAG_DCB;
	}
	/* Update HW struct after DCB configuration */
	i40e_get_cap(hw);

	/* initialize pf host driver to setup SRIOV resource if applicable */
	i40e_pf_host_init(dev);

	/* register callback func to eal lib */
	rte_intr_callback_register(intr_handle,
				   i40e_dev_interrupt_handler, dev);

	/* configure and enable device interrupt */
	i40e_pf_config_irq0(hw, TRUE);
	i40e_pf_enable_irq0(hw);

	/* enable uio intr after callback register */
	rte_intr_enable(intr_handle);
	/*
	 * Add an ethertype filter to drop all flow control frames transmitted
	 * from VSIs. By doing so, we stop VF from sending out PAUSE or PFC
	 * frames to wire.
	 */
	i40e_add_tx_flow_control_drop_filter(pf);

	/* Set the max frame size to 0x2600 by default,
	 * in case other drivers changed the default value.
	 */
	i40e_aq_set_mac_config(hw, I40E_FRAME_SIZE_MAX, TRUE, 0, NULL);

	/* initialize mirror rule list */
	TAILQ_INIT(&pf->mirror_list);

	ret = i40e_init_ethtype_filter_list(dev);
	if (ret < 0)
		goto err_init_ethtype_filter_list;
	ret = i40e_init_tunnel_filter_list(dev);
	if (ret < 0)
		goto err_init_tunnel_filter_list;
	ret = i40e_init_fdir_filter_list(dev);
	if (ret < 0)
		goto err_init_fdir_filter_list;

	return 0;

err_init_fdir_filter_list:
	rte_free(pf->tunnel.hash_table);
	rte_free(pf->tunnel.hash_map);
err_init_tunnel_filter_list:
	rte_free(pf->ethertype.hash_table);
	rte_free(pf->ethertype.hash_map);
err_init_ethtype_filter_list:
	rte_free(dev->data->mac_addrs);
err_mac_alloc:
	i40e_vsi_release(pf->main_vsi);
err_setup_pf_switch:
err_get_mac_addr:
err_configure_lan_hmc:
	(void)i40e_shutdown_lan_hmc(hw);
err_init_lan_hmc:
	i40e_res_pool_destroy(&pf->msix_pool);
err_msix_pool_init:
	i40e_res_pool_destroy(&pf->qp_pool);
err_qp_pool_init:
err_parameter_init:
err_get_capabilities:
err_sync_phy_type:
	(void)i40e_shutdown_adminq(hw);

	return ret;
}

static void
i40e_rm_ethtype_filter_list(struct i40e_pf *pf)
{
	struct i40e_ethertype_filter *p_ethertype;
	struct i40e_ethertype_rule *ethertype_rule;

	ethertype_rule = &pf->ethertype;
	/* Remove all ethertype filter rules and hash */
	if (ethertype_rule->hash_map)
		rte_free(ethertype_rule->hash_map);
	if (ethertype_rule->hash_table)
		rte_hash_free(ethertype_rule->hash_table);

	while ((p_ethertype = TAILQ_FIRST(&ethertype_rule->ethertype_list))) {
		TAILQ_REMOVE(&ethertype_rule->ethertype_list,
			     p_ethertype, rules);
		rte_free(p_ethertype);
	}
}

static void
i40e_rm_tunnel_filter_list(struct i40e_pf *pf)
{
	struct i40e_tunnel_filter *p_tunnel;
	struct i40e_tunnel_rule *tunnel_rule;

	tunnel_rule = &pf->tunnel;
	/* Remove all tunnel director rules and hash */
	if (tunnel_rule->hash_map)
		rte_free(tunnel_rule->hash_map);
	if (tunnel_rule->hash_table)
		rte_hash_free(tunnel_rule->hash_table);

	while ((p_tunnel = TAILQ_FIRST(&tunnel_rule->tunnel_list))) {
		TAILQ_REMOVE(&tunnel_rule->tunnel_list, p_tunnel, rules);
		rte_free(p_tunnel);
	}
}

static void
i40e_rm_fdir_filter_list(struct i40e_pf *pf)
{
	struct i40e_fdir_filter *p_fdir;
	struct i40e_fdir_info *fdir_info;

	fdir_info = &pf->fdir;
	/* Remove all flow director rules and hash */
	if (fdir_info->hash_map)
		rte_free(fdir_info->hash_map);
	if (fdir_info->hash_table)
		rte_hash_free(fdir_info->hash_table);

	while ((p_fdir = TAILQ_FIRST(&fdir_info->fdir_list))) {
		TAILQ_REMOVE(&fdir_info->fdir_list, p_fdir, rules);
		rte_free(p_fdir);
	}
}

static int
eth_i40e_dev_uninit(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf;
	struct rte_pci_device *pci_dev;
	struct rte_intr_handle *intr_handle;
	struct i40e_hw *hw;
	struct i40e_filter_control_settings settings;
	struct rte_flow *p_flow;
	int ret;
	uint8_t aq_fail = 0;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	intr_handle = &pci_dev->intr_handle;

	if (hw->adapter_stopped == 0)
		i40e_dev_close(dev);

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;

	/* Clear PXE mode */
	i40e_clear_pxe_mode(hw);

	/* Unconfigure filter control */
	memset(&settings, 0, sizeof(settings));
	ret = i40e_set_filter_control(hw, &settings);
	if (ret)
		PMD_INIT_LOG(WARNING, "setup_pf_filter_control failed: %d",
					ret);

	/* Disable flow control */
	hw->fc.requested_mode = I40E_FC_NONE;
	i40e_set_fc(hw, &aq_fail, TRUE);

	/* uninitialize pf host driver */
	i40e_pf_host_uninit(dev);

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	/* register callback func to eal lib */
	rte_intr_callback_unregister(intr_handle,
				     i40e_dev_interrupt_handler, dev);

	i40e_rm_ethtype_filter_list(pf);
	i40e_rm_tunnel_filter_list(pf);
	i40e_rm_fdir_filter_list(pf);

	/* Remove all flows */
	while ((p_flow = TAILQ_FIRST(&pf->flow_list))) {
		TAILQ_REMOVE(&pf->flow_list, p_flow, node);
		rte_free(p_flow);
	}

	return 0;
}

static int
i40e_dev_configure(struct rte_eth_dev *dev)
{
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum rte_eth_rx_mq_mode mq_mode = dev->data->dev_conf.rxmode.mq_mode;
	int i, ret;

	/* Initialize to TRUE. If any of Rx queues doesn't meet the
	 * bulk allocation or vector Rx preconditions we will reset it.
	 */
	ad->rx_bulk_alloc_allowed = true;
	ad->rx_vec_allowed = true;
	ad->tx_simple_allowed = true;
	ad->tx_vec_allowed = true;

	if (dev->data->dev_conf.fdir_conf.mode == RTE_FDIR_MODE_PERFECT) {
		ret = i40e_fdir_setup(pf);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to setup flow director.");
			return -ENOTSUP;
		}
		ret = i40e_fdir_configure(dev);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "failed to configure fdir.");
			goto err;
		}
	} else
		i40e_fdir_teardown(pf);

	ret = i40e_dev_init_vlan(dev);
	if (ret < 0)
		goto err;

	/* VMDQ setup.
	 *  Needs to move VMDQ setting out of i40e_pf_config_mq_rx() as VMDQ and
	 *  RSS setting have different requirements.
	 *  General PMD driver call sequence are NIC init, configure,
	 *  rx/tx_queue_setup and dev_start. In rx/tx_queue_setup() function, it
	 *  will try to lookup the VSI that specific queue belongs to if VMDQ
	 *  applicable. So, VMDQ setting has to be done before
	 *  rx/tx_queue_setup(). This function is good  to place vmdq_setup.
	 *  For RSS setting, it will try to calculate actual configured RX queue
	 *  number, which will be available after rx_queue_setup(). dev_start()
	 *  function is good to place RSS setup.
	 */
	if (mq_mode & ETH_MQ_RX_VMDQ_FLAG) {
		ret = i40e_vmdq_setup(dev);
		if (ret)
			goto err;
	}

	if (mq_mode & ETH_MQ_RX_DCB_FLAG) {
		ret = i40e_dcb_setup(dev);
		if (ret) {
			PMD_DRV_LOG(ERR, "failed to configure DCB.");
			goto err_dcb;
		}
	}

	TAILQ_INIT(&pf->flow_list);

	return 0;

err_dcb:
	/* need to release vmdq resource if exists */
	for (i = 0; i < pf->nb_cfg_vmdq_vsi; i++) {
		i40e_vsi_release(pf->vmdq[i].vsi);
		pf->vmdq[i].vsi = NULL;
	}
	rte_free(pf->vmdq);
	pf->vmdq = NULL;
err:
	/* need to release fdir resource if exists */
	i40e_fdir_teardown(pf);
	return ret;
}

void
i40e_vsi_queues_unbind_intr(struct i40e_vsi *vsi)
{
	struct rte_eth_dev *dev = vsi->adapter->eth_dev;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	uint16_t msix_vect = vsi->msix_intr;
	uint16_t i;

	for (i = 0; i < vsi->nb_qps; i++) {
		I40E_WRITE_REG(hw, I40E_QINT_TQCTL(vsi->base_queue + i), 0);
		I40E_WRITE_REG(hw, I40E_QINT_RQCTL(vsi->base_queue + i), 0);
		rte_wmb();
	}

	if (vsi->type != I40E_VSI_SRIOV) {
		if (!rte_intr_allow_others(intr_handle)) {
			I40E_WRITE_REG(hw, I40E_PFINT_LNKLST0,
				       I40E_PFINT_LNKLST0_FIRSTQ_INDX_MASK);
			I40E_WRITE_REG(hw,
				       I40E_PFINT_ITR0(I40E_ITR_INDEX_DEFAULT),
				       0);
		} else {
			I40E_WRITE_REG(hw, I40E_PFINT_LNKLSTN(msix_vect - 1),
				       I40E_PFINT_LNKLSTN_FIRSTQ_INDX_MASK);
			I40E_WRITE_REG(hw,
				       I40E_PFINT_ITRN(I40E_ITR_INDEX_DEFAULT,
						       msix_vect - 1), 0);
		}
	} else {
		uint32_t reg;
		reg = (hw->func_caps.num_msix_vectors_vf - 1) *
			vsi->user_param + (msix_vect - 1);

		I40E_WRITE_REG(hw, I40E_VPINT_LNKLSTN(reg),
			       I40E_VPINT_LNKLSTN_FIRSTQ_INDX_MASK);
	}
	I40E_WRITE_FLUSH(hw);
}

static void
__vsi_queues_bind_intr(struct i40e_vsi *vsi, uint16_t msix_vect,
		       int base_queue, int nb_queue)
{
	int i;
	uint32_t val;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);

	/* Bind all RX queues to allocated MSIX interrupt */
	for (i = 0; i < nb_queue; i++) {
		val = (msix_vect << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
			I40E_QINT_RQCTL_ITR_INDX_MASK |
			((base_queue + i + 1) <<
			 I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
			(0 << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT) |
			I40E_QINT_RQCTL_CAUSE_ENA_MASK;

		if (i == nb_queue - 1)
			val |= I40E_QINT_RQCTL_NEXTQ_INDX_MASK;
		I40E_WRITE_REG(hw, I40E_QINT_RQCTL(base_queue + i), val);
	}

	/* Write first RX queue to Link list register as the head element */
	if (vsi->type != I40E_VSI_SRIOV) {
		uint16_t interval =
			i40e_calc_itr_interval(RTE_LIBRTE_I40E_ITR_INTERVAL);

		if (msix_vect == I40E_MISC_VEC_ID) {
			I40E_WRITE_REG(hw, I40E_PFINT_LNKLST0,
				       (base_queue <<
					I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT) |
				       (0x0 <<
					I40E_PFINT_LNKLST0_FIRSTQ_TYPE_SHIFT));
			I40E_WRITE_REG(hw,
				       I40E_PFINT_ITR0(I40E_ITR_INDEX_DEFAULT),
				       interval);
		} else {
			I40E_WRITE_REG(hw, I40E_PFINT_LNKLSTN(msix_vect - 1),
				       (base_queue <<
					I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT) |
				       (0x0 <<
					I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_SHIFT));
			I40E_WRITE_REG(hw,
				       I40E_PFINT_ITRN(I40E_ITR_INDEX_DEFAULT,
						       msix_vect - 1),
				       interval);
		}
	} else {
		uint32_t reg;

		if (msix_vect == I40E_MISC_VEC_ID) {
			I40E_WRITE_REG(hw,
				       I40E_VPINT_LNKLST0(vsi->user_param),
				       (base_queue <<
					I40E_VPINT_LNKLST0_FIRSTQ_INDX_SHIFT) |
				       (0x0 <<
					I40E_VPINT_LNKLST0_FIRSTQ_TYPE_SHIFT));
		} else {
			/* num_msix_vectors_vf needs to minus irq0 */
			reg = (hw->func_caps.num_msix_vectors_vf - 1) *
				vsi->user_param + (msix_vect - 1);

			I40E_WRITE_REG(hw, I40E_VPINT_LNKLSTN(reg),
				       (base_queue <<
					I40E_VPINT_LNKLSTN_FIRSTQ_INDX_SHIFT) |
				       (0x0 <<
					I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_SHIFT));
		}
	}

	I40E_WRITE_FLUSH(hw);
}

void
i40e_vsi_queues_bind_intr(struct i40e_vsi *vsi)
{
	struct rte_eth_dev *dev = vsi->adapter->eth_dev;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	uint16_t msix_vect = vsi->msix_intr;
	uint16_t nb_msix = RTE_MIN(vsi->nb_msix, intr_handle->nb_efd);
	uint16_t queue_idx = 0;
	int record = 0;
	uint32_t val;
	int i;

	for (i = 0; i < vsi->nb_qps; i++) {
		I40E_WRITE_REG(hw, I40E_QINT_TQCTL(vsi->base_queue + i), 0);
		I40E_WRITE_REG(hw, I40E_QINT_RQCTL(vsi->base_queue + i), 0);
	}

	/* INTENA flag is not auto-cleared for interrupt */
	val = I40E_READ_REG(hw, I40E_GLINT_CTL);
	val |= I40E_GLINT_CTL_DIS_AUTOMASK_PF0_MASK |
		I40E_GLINT_CTL_DIS_AUTOMASK_N_MASK |
		I40E_GLINT_CTL_DIS_AUTOMASK_VF0_MASK;
	I40E_WRITE_REG(hw, I40E_GLINT_CTL, val);

	/* VF bind interrupt */
	if (vsi->type == I40E_VSI_SRIOV) {
		__vsi_queues_bind_intr(vsi, msix_vect,
				       vsi->base_queue, vsi->nb_qps);
		return;
	}

	/* PF & VMDq bind interrupt */
	if (rte_intr_dp_is_en(intr_handle)) {
		if (vsi->type == I40E_VSI_MAIN) {
			queue_idx = 0;
			record = 1;
		} else if (vsi->type == I40E_VSI_VMDQ2) {
			struct i40e_vsi *main_vsi =
				I40E_DEV_PRIVATE_TO_MAIN_VSI(vsi->adapter);
			queue_idx = vsi->base_queue - main_vsi->nb_qps;
			record = 1;
		}
	}

	for (i = 0; i < vsi->nb_used_qps; i++) {
		if (nb_msix <= 1) {
			if (!rte_intr_allow_others(intr_handle))
				/* allow to share MISC_VEC_ID */
				msix_vect = I40E_MISC_VEC_ID;

			/* no enough msix_vect, map all to one */
			__vsi_queues_bind_intr(vsi, msix_vect,
					       vsi->base_queue + i,
					       vsi->nb_used_qps - i);
			for (; !!record && i < vsi->nb_used_qps; i++)
				intr_handle->intr_vec[queue_idx + i] =
					msix_vect;
			break;
		}
		/* 1:1 queue/msix_vect mapping */
		__vsi_queues_bind_intr(vsi, msix_vect,
				       vsi->base_queue + i, 1);
		if (!!record)
			intr_handle->intr_vec[queue_idx + i] = msix_vect;

		msix_vect++;
		nb_msix--;
	}
}

static void
i40e_vsi_enable_queues_intr(struct i40e_vsi *vsi)
{
	struct rte_eth_dev *dev = vsi->adapter->eth_dev;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	uint16_t interval = i40e_calc_itr_interval(\
		RTE_LIBRTE_I40E_ITR_INTERVAL);
	uint16_t msix_intr, i;

	if (rte_intr_allow_others(intr_handle))
		for (i = 0; i < vsi->nb_msix; i++) {
			msix_intr = vsi->msix_intr + i;
			I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTLN(msix_intr - 1),
				I40E_PFINT_DYN_CTLN_INTENA_MASK |
				I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
				(0 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT) |
				(interval <<
				 I40E_PFINT_DYN_CTLN_INTERVAL_SHIFT));
		}
	else
		I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTL0,
			       I40E_PFINT_DYN_CTL0_INTENA_MASK |
			       I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
			       (0 << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT) |
			       (interval <<
				I40E_PFINT_DYN_CTL0_INTERVAL_SHIFT));

	I40E_WRITE_FLUSH(hw);
}

static void
i40e_vsi_disable_queues_intr(struct i40e_vsi *vsi)
{
	struct rte_eth_dev *dev = vsi->adapter->eth_dev;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	uint16_t msix_intr, i;

	if (rte_intr_allow_others(intr_handle))
		for (i = 0; i < vsi->nb_msix; i++) {
			msix_intr = vsi->msix_intr + i;
			I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTLN(msix_intr - 1),
				       0);
		}
	else
		I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTL0, 0);

	I40E_WRITE_FLUSH(hw);
}

static inline uint8_t
i40e_parse_link_speeds(uint16_t link_speeds)
{
	uint8_t link_speed = I40E_LINK_SPEED_UNKNOWN;

	if (link_speeds & ETH_LINK_SPEED_40G)
		link_speed |= I40E_LINK_SPEED_40GB;
	if (link_speeds & ETH_LINK_SPEED_25G)
		link_speed |= I40E_LINK_SPEED_25GB;
	if (link_speeds & ETH_LINK_SPEED_20G)
		link_speed |= I40E_LINK_SPEED_20GB;
	if (link_speeds & ETH_LINK_SPEED_10G)
		link_speed |= I40E_LINK_SPEED_10GB;
	if (link_speeds & ETH_LINK_SPEED_1G)
		link_speed |= I40E_LINK_SPEED_1GB;
	if (link_speeds & ETH_LINK_SPEED_100M)
		link_speed |= I40E_LINK_SPEED_100MB;

	return link_speed;
}

static int
i40e_phy_conf_link(struct i40e_hw *hw,
		   uint8_t abilities,
		   uint8_t force_speed)
{
	enum i40e_status_code status;
	struct i40e_aq_get_phy_abilities_resp phy_ab;
	struct i40e_aq_set_phy_config phy_conf;
	const uint8_t mask = I40E_AQ_PHY_FLAG_PAUSE_TX |
			I40E_AQ_PHY_FLAG_PAUSE_RX |
			I40E_AQ_PHY_FLAG_PAUSE_RX |
			I40E_AQ_PHY_FLAG_LOW_POWER;
	const uint8_t advt = I40E_LINK_SPEED_40GB |
			I40E_LINK_SPEED_25GB |
			I40E_LINK_SPEED_10GB |
			I40E_LINK_SPEED_1GB |
			I40E_LINK_SPEED_100MB;
	int ret = -ENOTSUP;


	status = i40e_aq_get_phy_capabilities(hw, false, false, &phy_ab,
					      NULL);
	if (status)
		return ret;

	memset(&phy_conf, 0, sizeof(phy_conf));

	/* bits 0-2 use the values from get_phy_abilities_resp */
	abilities &= ~mask;
	abilities |= phy_ab.abilities & mask;

	/* update ablities and speed */
	if (abilities & I40E_AQ_PHY_AN_ENABLED)
		phy_conf.link_speed = advt;
	else
		phy_conf.link_speed = force_speed;

	phy_conf.abilities = abilities;

	/* use get_phy_abilities_resp value for the rest */
	phy_conf.phy_type = phy_ab.phy_type;
	phy_conf.phy_type_ext = phy_ab.phy_type_ext;
	phy_conf.fec_config = phy_ab.fec_cfg_curr_mod_ext_info;
	phy_conf.eee_capability = phy_ab.eee_capability;
	phy_conf.eeer = phy_ab.eeer_val;
	phy_conf.low_power_ctrl = phy_ab.d3_lpan;

	PMD_DRV_LOG(DEBUG, "\tCurrent: abilities %x, link_speed %x",
		    phy_ab.abilities, phy_ab.link_speed);
	PMD_DRV_LOG(DEBUG, "\tConfig:  abilities %x, link_speed %x",
		    phy_conf.abilities, phy_conf.link_speed);

	status = i40e_aq_set_phy_config(hw, &phy_conf, NULL);
	if (status)
		return ret;

	return I40E_SUCCESS;
}

static int
i40e_apply_link_speed(struct rte_eth_dev *dev)
{
	uint8_t speed;
	uint8_t abilities = 0;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_conf *conf = &dev->data->dev_conf;

	speed = i40e_parse_link_speeds(conf->link_speeds);
	abilities |= I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
	if (!(conf->link_speeds & ETH_LINK_SPEED_FIXED))
		abilities |= I40E_AQ_PHY_AN_ENABLED;
	abilities |= I40E_AQ_PHY_LINK_ENABLED;

	/* Skip changing speed on 40G interfaces, FW does not support */
	if (I40E_PHY_TYPE_SUPPORT_40G(hw->phy.phy_types)) {
		speed =  I40E_LINK_SPEED_UNKNOWN;
		abilities |= I40E_AQ_PHY_AN_ENABLED;
	}

	return i40e_phy_conf_link(hw, abilities, speed);
}

static int
i40e_dev_start(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *main_vsi = pf->main_vsi;
	int ret, i;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint32_t intr_vector = 0;
	struct i40e_vsi *vsi;

	hw->adapter_stopped = 0;

	if (dev->data->dev_conf.link_speeds & ETH_LINK_SPEED_FIXED) {
		PMD_INIT_LOG(ERR, "Invalid link_speeds for port %hhu; autonegotiation disabled",
			     dev->data->port_id);
		return -EINVAL;
	}

	rte_intr_disable(intr_handle);

	if ((rte_intr_cap_multiple(intr_handle) ||
	     !RTE_ETH_DEV_SRIOV(dev).active) &&
	    dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = dev->data->nb_rx_queues;
		ret = rte_intr_efd_enable(intr_handle, intr_vector);
		if (ret)
			return ret;
	}

	if (rte_intr_dp_is_en(intr_handle) && !intr_handle->intr_vec) {
		intr_handle->intr_vec =
			rte_zmalloc("intr_vec",
				    dev->data->nb_rx_queues * sizeof(int),
				    0);
		if (!intr_handle->intr_vec) {
			PMD_INIT_LOG(ERR,
				"Failed to allocate %d rx_queues intr_vec",
				dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}

	/* Initialize VSI */
	ret = i40e_dev_rxtx_init(pf);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to init rx/tx queues");
		goto err_up;
	}

	/* Map queues with MSIX interrupt */
	main_vsi->nb_used_qps = dev->data->nb_rx_queues -
		pf->nb_cfg_vmdq_vsi * RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM;
	i40e_vsi_queues_bind_intr(main_vsi);
	i40e_vsi_enable_queues_intr(main_vsi);

	/* Map VMDQ VSI queues with MSIX interrupt */
	for (i = 0; i < pf->nb_cfg_vmdq_vsi; i++) {
		pf->vmdq[i].vsi->nb_used_qps = RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM;
		i40e_vsi_queues_bind_intr(pf->vmdq[i].vsi);
		i40e_vsi_enable_queues_intr(pf->vmdq[i].vsi);
	}

	/* enable FDIR MSIX interrupt */
	if (pf->fdir.fdir_vsi) {
		i40e_vsi_queues_bind_intr(pf->fdir.fdir_vsi);
		i40e_vsi_enable_queues_intr(pf->fdir.fdir_vsi);
	}

	/* Enable all queues which have been configured */
	ret = i40e_dev_switch_queues(pf, TRUE);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to enable VSI");
		goto err_up;
	}

	/* Enable receiving broadcast packets */
	ret = i40e_aq_set_vsi_broadcast(hw, main_vsi->seid, true, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(INFO, "fail to set vsi broadcast");

	for (i = 0; i < pf->nb_cfg_vmdq_vsi; i++) {
		ret = i40e_aq_set_vsi_broadcast(hw, pf->vmdq[i].vsi->seid,
						true, NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(INFO, "fail to set vsi broadcast");
	}

	/* Enable the VLAN promiscuous mode. */
	if (pf->vfs) {
		for (i = 0; i < pf->vf_num; i++) {
			vsi = pf->vfs[i].vsi;
			i40e_aq_set_vsi_vlan_promisc(hw, vsi->seid,
						     true, NULL);
		}
	}

	/* Apply link configure */
	if (dev->data->dev_conf.link_speeds & ~(ETH_LINK_SPEED_100M |
				ETH_LINK_SPEED_1G | ETH_LINK_SPEED_10G |
				ETH_LINK_SPEED_20G | ETH_LINK_SPEED_25G |
				ETH_LINK_SPEED_40G)) {
		PMD_DRV_LOG(ERR, "Invalid link setting");
		goto err_up;
	}
	ret = i40e_apply_link_speed(dev);
	if (I40E_SUCCESS != ret) {
		PMD_DRV_LOG(ERR, "Fail to apply link setting");
		goto err_up;
	}

	if (!rte_intr_allow_others(intr_handle)) {
		rte_intr_callback_unregister(intr_handle,
					     i40e_dev_interrupt_handler,
					     (void *)dev);
		/* configure and enable device interrupt */
		i40e_pf_config_irq0(hw, FALSE);
		i40e_pf_enable_irq0(hw);

		if (dev->data->dev_conf.intr_conf.lsc != 0)
			PMD_INIT_LOG(INFO,
				"lsc won't enable because of no intr multiplex");
	} else if (dev->data->dev_conf.intr_conf.lsc != 0) {
		ret = i40e_aq_set_phy_int_mask(hw,
					       ~(I40E_AQ_EVENT_LINK_UPDOWN |
					       I40E_AQ_EVENT_MODULE_QUAL_FAIL |
					       I40E_AQ_EVENT_MEDIA_NA), NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(WARNING, "Fail to set phy mask");

		/* Call get_link_info aq commond to enable LSE */
		i40e_dev_link_update(dev, 0);
	}

	/* enable uio intr after callback register */
	rte_intr_enable(intr_handle);

	i40e_filter_restore(pf);

	return I40E_SUCCESS;

err_up:
	i40e_dev_switch_queues(pf, FALSE);
	i40e_dev_clear_queues(dev);

	return ret;
}

static void
i40e_dev_stop(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *main_vsi = pf->main_vsi;
	struct i40e_mirror_rule *p_mirror;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	int i;

	/* Disable all queues */
	i40e_dev_switch_queues(pf, FALSE);

	/* un-map queues with interrupt registers */
	i40e_vsi_disable_queues_intr(main_vsi);
	i40e_vsi_queues_unbind_intr(main_vsi);

	for (i = 0; i < pf->nb_cfg_vmdq_vsi; i++) {
		i40e_vsi_disable_queues_intr(pf->vmdq[i].vsi);
		i40e_vsi_queues_unbind_intr(pf->vmdq[i].vsi);
	}

	if (pf->fdir.fdir_vsi) {
		i40e_vsi_queues_unbind_intr(pf->fdir.fdir_vsi);
		i40e_vsi_disable_queues_intr(pf->fdir.fdir_vsi);
	}
	/* Clear all queues and release memory */
	i40e_dev_clear_queues(dev);

	/* Set link down */
	i40e_dev_set_link_down(dev);

	/* Remove all mirror rules */
	while ((p_mirror = TAILQ_FIRST(&pf->mirror_list))) {
		TAILQ_REMOVE(&pf->mirror_list, p_mirror, rules);
		rte_free(p_mirror);
	}
	pf->nb_mirror_rule = 0;

	if (!rte_intr_allow_others(intr_handle))
		/* resume to the default handler */
		rte_intr_callback_register(intr_handle,
					   i40e_dev_interrupt_handler,
					   (void *)dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
}

static void
i40e_dev_close(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint32_t reg;
	int i;

	PMD_INIT_FUNC_TRACE();

	i40e_dev_stop(dev);
	hw->adapter_stopped = 1;
	i40e_dev_free_queues(dev);

	/* Disable interrupt */
	i40e_pf_disable_irq0(hw);
	rte_intr_disable(intr_handle);

	/* shutdown and destroy the HMC */
	i40e_shutdown_lan_hmc(hw);

	for (i = 0; i < pf->nb_cfg_vmdq_vsi; i++) {
		i40e_vsi_release(pf->vmdq[i].vsi);
		pf->vmdq[i].vsi = NULL;
	}
	rte_free(pf->vmdq);
	pf->vmdq = NULL;

	/* release all the existing VSIs and VEBs */
	i40e_fdir_teardown(pf);
	i40e_vsi_release(pf->main_vsi);

	/* shutdown the adminq */
	i40e_aq_queue_shutdown(hw, true);
	i40e_shutdown_adminq(hw);

	i40e_res_pool_destroy(&pf->qp_pool);
	i40e_res_pool_destroy(&pf->msix_pool);

	/* force a PF reset to clean anything leftover */
	reg = I40E_READ_REG(hw, I40E_PFGEN_CTRL);
	I40E_WRITE_REG(hw, I40E_PFGEN_CTRL,
			(reg | I40E_PFGEN_CTRL_PFSWR_MASK));
	I40E_WRITE_FLUSH(hw);
}

static void
i40e_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int status;

	status = i40e_aq_set_vsi_unicast_promiscuous(hw, vsi->seid,
						     true, NULL, true);
	if (status != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to enable unicast promiscuous");

	status = i40e_aq_set_vsi_multicast_promiscuous(hw, vsi->seid,
							TRUE, NULL);
	if (status != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to enable multicast promiscuous");

}

static void
i40e_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int status;

	status = i40e_aq_set_vsi_unicast_promiscuous(hw, vsi->seid,
						     false, NULL, true);
	if (status != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to disable unicast promiscuous");

	status = i40e_aq_set_vsi_multicast_promiscuous(hw, vsi->seid,
							false, NULL);
	if (status != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to disable multicast promiscuous");
}

static void
i40e_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int ret;

	ret = i40e_aq_set_vsi_multicast_promiscuous(hw, vsi->seid, TRUE, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to enable multicast promiscuous");
}

static void
i40e_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	int ret;

	if (dev->data->promiscuous == 1)
		return; /* must remain in all_multicast mode */

	ret = i40e_aq_set_vsi_multicast_promiscuous(hw,
				vsi->seid, FALSE, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to disable multicast promiscuous");
}

/*
 * Set device link up.
 */
static int
i40e_dev_set_link_up(struct rte_eth_dev *dev)
{
	/* re-apply link speed setting */
	return i40e_apply_link_speed(dev);
}

/*
 * Set device link down.
 */
static int
i40e_dev_set_link_down(struct rte_eth_dev *dev)
{
	uint8_t speed = I40E_LINK_SPEED_UNKNOWN;
	uint8_t abilities = 0;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	abilities = I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
	return i40e_phy_conf_link(hw, abilities, speed);
}

int
i40e_dev_link_update(struct rte_eth_dev *dev,
		     int wait_to_complete)
{
#define CHECK_INTERVAL 100  /* 100ms */
#define MAX_REPEAT_TIME 10  /* 1s (10 * 100ms) in total */
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_link_status link_status;
	struct rte_eth_link link, old;
	int status;
	unsigned rep_cnt = MAX_REPEAT_TIME;
	bool enable_lse = dev->data->dev_conf.intr_conf.lsc ? true : false;

	memset(&link, 0, sizeof(link));
	memset(&old, 0, sizeof(old));
	memset(&link_status, 0, sizeof(link_status));
	rte_i40e_dev_atomic_read_link_status(dev, &old);

	do {
		/* Get link status information from hardware */
		status = i40e_aq_get_link_info(hw, enable_lse,
						&link_status, NULL);
		if (status != I40E_SUCCESS) {
			link.link_speed = ETH_SPEED_NUM_100M;
			link.link_duplex = ETH_LINK_FULL_DUPLEX;
			PMD_DRV_LOG(ERR, "Failed to get link info");
			goto out;
		}

		link.link_status = link_status.link_info & I40E_AQ_LINK_UP;
		if (!wait_to_complete || link.link_status)
			break;

		rte_delay_ms(CHECK_INTERVAL);
	} while (--rep_cnt);

	if (!link.link_status)
		goto out;

	/* i40e uses full duplex only */
	link.link_duplex = ETH_LINK_FULL_DUPLEX;

	/* Parse the link status */
	switch (link_status.link_speed) {
	case I40E_LINK_SPEED_100MB:
		link.link_speed = ETH_SPEED_NUM_100M;
		break;
	case I40E_LINK_SPEED_1GB:
		link.link_speed = ETH_SPEED_NUM_1G;
		break;
	case I40E_LINK_SPEED_10GB:
		link.link_speed = ETH_SPEED_NUM_10G;
		break;
	case I40E_LINK_SPEED_20GB:
		link.link_speed = ETH_SPEED_NUM_20G;
		break;
	case I40E_LINK_SPEED_25GB:
		link.link_speed = ETH_SPEED_NUM_25G;
		break;
	case I40E_LINK_SPEED_40GB:
		link.link_speed = ETH_SPEED_NUM_40G;
		break;
	default:
		link.link_speed = ETH_SPEED_NUM_100M;
		break;
	}

	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			ETH_LINK_SPEED_FIXED);

out:
	rte_i40e_dev_atomic_write_link_status(dev, &link);
	if (link.link_status == old.link_status)
		return -1;

	i40e_notify_all_vfs_link_status(dev);

	return 0;
}

/* Get all the statistics of a VSI */
void
i40e_update_vsi_stats(struct i40e_vsi *vsi)
{
	struct i40e_eth_stats *oes = &vsi->eth_stats_offset;
	struct i40e_eth_stats *nes = &vsi->eth_stats;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	int idx = rte_le_to_cpu_16(vsi->info.stat_counter_idx);

	i40e_stat_update_48(hw, I40E_GLV_GORCH(idx), I40E_GLV_GORCL(idx),
			    vsi->offset_loaded, &oes->rx_bytes,
			    &nes->rx_bytes);
	i40e_stat_update_48(hw, I40E_GLV_UPRCH(idx), I40E_GLV_UPRCL(idx),
			    vsi->offset_loaded, &oes->rx_unicast,
			    &nes->rx_unicast);
	i40e_stat_update_48(hw, I40E_GLV_MPRCH(idx), I40E_GLV_MPRCL(idx),
			    vsi->offset_loaded, &oes->rx_multicast,
			    &nes->rx_multicast);
	i40e_stat_update_48(hw, I40E_GLV_BPRCH(idx), I40E_GLV_BPRCL(idx),
			    vsi->offset_loaded, &oes->rx_broadcast,
			    &nes->rx_broadcast);
	/* exclude CRC bytes */
	nes->rx_bytes -= (nes->rx_unicast + nes->rx_multicast +
		nes->rx_broadcast) * ETHER_CRC_LEN;

	i40e_stat_update_32(hw, I40E_GLV_RDPC(idx), vsi->offset_loaded,
			    &oes->rx_discards, &nes->rx_discards);
	/* GLV_REPC not supported */
	/* GLV_RMPC not supported */
	i40e_stat_update_32(hw, I40E_GLV_RUPP(idx), vsi->offset_loaded,
			    &oes->rx_unknown_protocol,
			    &nes->rx_unknown_protocol);
	i40e_stat_update_48(hw, I40E_GLV_GOTCH(idx), I40E_GLV_GOTCL(idx),
			    vsi->offset_loaded, &oes->tx_bytes,
			    &nes->tx_bytes);
	i40e_stat_update_48(hw, I40E_GLV_UPTCH(idx), I40E_GLV_UPTCL(idx),
			    vsi->offset_loaded, &oes->tx_unicast,
			    &nes->tx_unicast);
	i40e_stat_update_48(hw, I40E_GLV_MPTCH(idx), I40E_GLV_MPTCL(idx),
			    vsi->offset_loaded, &oes->tx_multicast,
			    &nes->tx_multicast);
	i40e_stat_update_48(hw, I40E_GLV_BPTCH(idx), I40E_GLV_BPTCL(idx),
			    vsi->offset_loaded,  &oes->tx_broadcast,
			    &nes->tx_broadcast);
	/* exclude CRC bytes */
	nes->tx_bytes -= (nes->tx_unicast + nes->tx_multicast +
		nes->tx_broadcast) * ETHER_CRC_LEN;
	/* GLV_TDPC not supported */
	i40e_stat_update_32(hw, I40E_GLV_TEPC(idx), vsi->offset_loaded,
			    &oes->tx_errors, &nes->tx_errors);
	vsi->offset_loaded = true;

	PMD_DRV_LOG(DEBUG, "***************** VSI[%u] stats start *******************",
		    vsi->vsi_id);
	PMD_DRV_LOG(DEBUG, "rx_bytes:            %"PRIu64"", nes->rx_bytes);
	PMD_DRV_LOG(DEBUG, "rx_unicast:          %"PRIu64"", nes->rx_unicast);
	PMD_DRV_LOG(DEBUG, "rx_multicast:        %"PRIu64"", nes->rx_multicast);
	PMD_DRV_LOG(DEBUG, "rx_broadcast:        %"PRIu64"", nes->rx_broadcast);
	PMD_DRV_LOG(DEBUG, "rx_discards:         %"PRIu64"", nes->rx_discards);
	PMD_DRV_LOG(DEBUG, "rx_unknown_protocol: %"PRIu64"",
		    nes->rx_unknown_protocol);
	PMD_DRV_LOG(DEBUG, "tx_bytes:            %"PRIu64"", nes->tx_bytes);
	PMD_DRV_LOG(DEBUG, "tx_unicast:          %"PRIu64"", nes->tx_unicast);
	PMD_DRV_LOG(DEBUG, "tx_multicast:        %"PRIu64"", nes->tx_multicast);
	PMD_DRV_LOG(DEBUG, "tx_broadcast:        %"PRIu64"", nes->tx_broadcast);
	PMD_DRV_LOG(DEBUG, "tx_discards:         %"PRIu64"", nes->tx_discards);
	PMD_DRV_LOG(DEBUG, "tx_errors:           %"PRIu64"", nes->tx_errors);
	PMD_DRV_LOG(DEBUG, "***************** VSI[%u] stats end *******************",
		    vsi->vsi_id);
}

static void
i40e_read_stats_registers(struct i40e_pf *pf, struct i40e_hw *hw)
{
	unsigned int i;
	struct i40e_hw_port_stats *ns = &pf->stats; /* new stats */
	struct i40e_hw_port_stats *os = &pf->stats_offset; /* old stats */

	/* Get rx/tx bytes of internal transfer packets */
	i40e_stat_update_48(hw, I40E_GLV_GORCH(hw->port),
			I40E_GLV_GORCL(hw->port),
			pf->offset_loaded,
			&pf->internal_rx_bytes_offset,
			&pf->internal_rx_bytes);

	i40e_stat_update_48(hw, I40E_GLV_GOTCH(hw->port),
			I40E_GLV_GOTCL(hw->port),
			pf->offset_loaded,
			&pf->internal_tx_bytes_offset,
			&pf->internal_tx_bytes);

	/* Get statistics of struct i40e_eth_stats */
	i40e_stat_update_48(hw, I40E_GLPRT_GORCH(hw->port),
			    I40E_GLPRT_GORCL(hw->port),
			    pf->offset_loaded, &os->eth.rx_bytes,
			    &ns->eth.rx_bytes);
	i40e_stat_update_48(hw, I40E_GLPRT_UPRCH(hw->port),
			    I40E_GLPRT_UPRCL(hw->port),
			    pf->offset_loaded, &os->eth.rx_unicast,
			    &ns->eth.rx_unicast);
	i40e_stat_update_48(hw, I40E_GLPRT_MPRCH(hw->port),
			    I40E_GLPRT_MPRCL(hw->port),
			    pf->offset_loaded, &os->eth.rx_multicast,
			    &ns->eth.rx_multicast);
	i40e_stat_update_48(hw, I40E_GLPRT_BPRCH(hw->port),
			    I40E_GLPRT_BPRCL(hw->port),
			    pf->offset_loaded, &os->eth.rx_broadcast,
			    &ns->eth.rx_broadcast);
	/* Workaround: CRC size should not be included in byte statistics,
	 * so subtract ETHER_CRC_LEN from the byte counter for each rx packet.
	 */
	ns->eth.rx_bytes -= (ns->eth.rx_unicast + ns->eth.rx_multicast +
		ns->eth.rx_broadcast) * ETHER_CRC_LEN + pf->internal_rx_bytes;

	i40e_stat_update_32(hw, I40E_GLPRT_RDPC(hw->port),
			    pf->offset_loaded, &os->eth.rx_discards,
			    &ns->eth.rx_discards);
	/* GLPRT_REPC not supported */
	/* GLPRT_RMPC not supported */
	i40e_stat_update_32(hw, I40E_GLPRT_RUPP(hw->port),
			    pf->offset_loaded,
			    &os->eth.rx_unknown_protocol,
			    &ns->eth.rx_unknown_protocol);
	i40e_stat_update_48(hw, I40E_GLPRT_GOTCH(hw->port),
			    I40E_GLPRT_GOTCL(hw->port),
			    pf->offset_loaded, &os->eth.tx_bytes,
			    &ns->eth.tx_bytes);
	i40e_stat_update_48(hw, I40E_GLPRT_UPTCH(hw->port),
			    I40E_GLPRT_UPTCL(hw->port),
			    pf->offset_loaded, &os->eth.tx_unicast,
			    &ns->eth.tx_unicast);
	i40e_stat_update_48(hw, I40E_GLPRT_MPTCH(hw->port),
			    I40E_GLPRT_MPTCL(hw->port),
			    pf->offset_loaded, &os->eth.tx_multicast,
			    &ns->eth.tx_multicast);
	i40e_stat_update_48(hw, I40E_GLPRT_BPTCH(hw->port),
			    I40E_GLPRT_BPTCL(hw->port),
			    pf->offset_loaded, &os->eth.tx_broadcast,
			    &ns->eth.tx_broadcast);
	ns->eth.tx_bytes -= (ns->eth.tx_unicast + ns->eth.tx_multicast +
		ns->eth.tx_broadcast) * ETHER_CRC_LEN + pf->internal_tx_bytes;
	/* GLPRT_TEPC not supported */

	/* additional port specific stats */
	i40e_stat_update_32(hw, I40E_GLPRT_TDOLD(hw->port),
			    pf->offset_loaded, &os->tx_dropped_link_down,
			    &ns->tx_dropped_link_down);
	i40e_stat_update_32(hw, I40E_GLPRT_CRCERRS(hw->port),
			    pf->offset_loaded, &os->crc_errors,
			    &ns->crc_errors);
	i40e_stat_update_32(hw, I40E_GLPRT_ILLERRC(hw->port),
			    pf->offset_loaded, &os->illegal_bytes,
			    &ns->illegal_bytes);
	/* GLPRT_ERRBC not supported */
	i40e_stat_update_32(hw, I40E_GLPRT_MLFC(hw->port),
			    pf->offset_loaded, &os->mac_local_faults,
			    &ns->mac_local_faults);
	i40e_stat_update_32(hw, I40E_GLPRT_MRFC(hw->port),
			    pf->offset_loaded, &os->mac_remote_faults,
			    &ns->mac_remote_faults);
	i40e_stat_update_32(hw, I40E_GLPRT_RLEC(hw->port),
			    pf->offset_loaded, &os->rx_length_errors,
			    &ns->rx_length_errors);
	i40e_stat_update_32(hw, I40E_GLPRT_LXONRXC(hw->port),
			    pf->offset_loaded, &os->link_xon_rx,
			    &ns->link_xon_rx);
	i40e_stat_update_32(hw, I40E_GLPRT_LXOFFRXC(hw->port),
			    pf->offset_loaded, &os->link_xoff_rx,
			    &ns->link_xoff_rx);
	for (i = 0; i < 8; i++) {
		i40e_stat_update_32(hw, I40E_GLPRT_PXONRXC(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xon_rx[i],
				    &ns->priority_xon_rx[i]);
		i40e_stat_update_32(hw, I40E_GLPRT_PXOFFRXC(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xoff_rx[i],
				    &ns->priority_xoff_rx[i]);
	}
	i40e_stat_update_32(hw, I40E_GLPRT_LXONTXC(hw->port),
			    pf->offset_loaded, &os->link_xon_tx,
			    &ns->link_xon_tx);
	i40e_stat_update_32(hw, I40E_GLPRT_LXOFFTXC(hw->port),
			    pf->offset_loaded, &os->link_xoff_tx,
			    &ns->link_xoff_tx);
	for (i = 0; i < 8; i++) {
		i40e_stat_update_32(hw, I40E_GLPRT_PXONTXC(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xon_tx[i],
				    &ns->priority_xon_tx[i]);
		i40e_stat_update_32(hw, I40E_GLPRT_PXOFFTXC(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xoff_tx[i],
				    &ns->priority_xoff_tx[i]);
		i40e_stat_update_32(hw, I40E_GLPRT_RXON2OFFCNT(hw->port, i),
				    pf->offset_loaded,
				    &os->priority_xon_2_xoff[i],
				    &ns->priority_xon_2_xoff[i]);
	}
	i40e_stat_update_48(hw, I40E_GLPRT_PRC64H(hw->port),
			    I40E_GLPRT_PRC64L(hw->port),
			    pf->offset_loaded, &os->rx_size_64,
			    &ns->rx_size_64);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC127H(hw->port),
			    I40E_GLPRT_PRC127L(hw->port),
			    pf->offset_loaded, &os->rx_size_127,
			    &ns->rx_size_127);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC255H(hw->port),
			    I40E_GLPRT_PRC255L(hw->port),
			    pf->offset_loaded, &os->rx_size_255,
			    &ns->rx_size_255);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC511H(hw->port),
			    I40E_GLPRT_PRC511L(hw->port),
			    pf->offset_loaded, &os->rx_size_511,
			    &ns->rx_size_511);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC1023H(hw->port),
			    I40E_GLPRT_PRC1023L(hw->port),
			    pf->offset_loaded, &os->rx_size_1023,
			    &ns->rx_size_1023);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC1522H(hw->port),
			    I40E_GLPRT_PRC1522L(hw->port),
			    pf->offset_loaded, &os->rx_size_1522,
			    &ns->rx_size_1522);
	i40e_stat_update_48(hw, I40E_GLPRT_PRC9522H(hw->port),
			    I40E_GLPRT_PRC9522L(hw->port),
			    pf->offset_loaded, &os->rx_size_big,
			    &ns->rx_size_big);
	i40e_stat_update_32(hw, I40E_GLPRT_RUC(hw->port),
			    pf->offset_loaded, &os->rx_undersize,
			    &ns->rx_undersize);
	i40e_stat_update_32(hw, I40E_GLPRT_RFC(hw->port),
			    pf->offset_loaded, &os->rx_fragments,
			    &ns->rx_fragments);
	i40e_stat_update_32(hw, I40E_GLPRT_ROC(hw->port),
			    pf->offset_loaded, &os->rx_oversize,
			    &ns->rx_oversize);
	i40e_stat_update_32(hw, I40E_GLPRT_RJC(hw->port),
			    pf->offset_loaded, &os->rx_jabber,
			    &ns->rx_jabber);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC64H(hw->port),
			    I40E_GLPRT_PTC64L(hw->port),
			    pf->offset_loaded, &os->tx_size_64,
			    &ns->tx_size_64);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC127H(hw->port),
			    I40E_GLPRT_PTC127L(hw->port),
			    pf->offset_loaded, &os->tx_size_127,
			    &ns->tx_size_127);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC255H(hw->port),
			    I40E_GLPRT_PTC255L(hw->port),
			    pf->offset_loaded, &os->tx_size_255,
			    &ns->tx_size_255);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC511H(hw->port),
			    I40E_GLPRT_PTC511L(hw->port),
			    pf->offset_loaded, &os->tx_size_511,
			    &ns->tx_size_511);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC1023H(hw->port),
			    I40E_GLPRT_PTC1023L(hw->port),
			    pf->offset_loaded, &os->tx_size_1023,
			    &ns->tx_size_1023);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC1522H(hw->port),
			    I40E_GLPRT_PTC1522L(hw->port),
			    pf->offset_loaded, &os->tx_size_1522,
			    &ns->tx_size_1522);
	i40e_stat_update_48(hw, I40E_GLPRT_PTC9522H(hw->port),
			    I40E_GLPRT_PTC9522L(hw->port),
			    pf->offset_loaded, &os->tx_size_big,
			    &ns->tx_size_big);
	i40e_stat_update_32(hw, I40E_GLQF_PCNT(pf->fdir.match_counter_index),
			   pf->offset_loaded,
			   &os->fd_sb_match, &ns->fd_sb_match);
	/* GLPRT_MSPDC not supported */
	/* GLPRT_XEC not supported */

	pf->offset_loaded = true;

	if (pf->main_vsi)
		i40e_update_vsi_stats(pf->main_vsi);
}

/* Get all statistics of a port */
static void
i40e_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_hw_port_stats *ns = &pf->stats; /* new stats */
	unsigned i;

	/* call read registers - updates values, now write them to struct */
	i40e_read_stats_registers(pf, hw);

	stats->ipackets = pf->main_vsi->eth_stats.rx_unicast +
			pf->main_vsi->eth_stats.rx_multicast +
			pf->main_vsi->eth_stats.rx_broadcast -
			pf->main_vsi->eth_stats.rx_discards;
	stats->opackets = pf->main_vsi->eth_stats.tx_unicast +
			pf->main_vsi->eth_stats.tx_multicast +
			pf->main_vsi->eth_stats.tx_broadcast;
	stats->ibytes   = ns->eth.rx_bytes;
	stats->obytes   = ns->eth.tx_bytes;
	stats->oerrors  = ns->eth.tx_errors +
			pf->main_vsi->eth_stats.tx_errors;

	/* Rx Errors */
	stats->imissed  = ns->eth.rx_discards +
			pf->main_vsi->eth_stats.rx_discards;
	stats->ierrors  = ns->crc_errors +
			ns->rx_length_errors + ns->rx_undersize +
			ns->rx_oversize + ns->rx_fragments + ns->rx_jabber;

	PMD_DRV_LOG(DEBUG, "***************** PF stats start *******************");
	PMD_DRV_LOG(DEBUG, "rx_bytes:            %"PRIu64"", ns->eth.rx_bytes);
	PMD_DRV_LOG(DEBUG, "rx_unicast:          %"PRIu64"", ns->eth.rx_unicast);
	PMD_DRV_LOG(DEBUG, "rx_multicast:        %"PRIu64"", ns->eth.rx_multicast);
	PMD_DRV_LOG(DEBUG, "rx_broadcast:        %"PRIu64"", ns->eth.rx_broadcast);
	PMD_DRV_LOG(DEBUG, "rx_discards:         %"PRIu64"", ns->eth.rx_discards);
	PMD_DRV_LOG(DEBUG, "rx_unknown_protocol: %"PRIu64"",
		    ns->eth.rx_unknown_protocol);
	PMD_DRV_LOG(DEBUG, "tx_bytes:            %"PRIu64"", ns->eth.tx_bytes);
	PMD_DRV_LOG(DEBUG, "tx_unicast:          %"PRIu64"", ns->eth.tx_unicast);
	PMD_DRV_LOG(DEBUG, "tx_multicast:        %"PRIu64"", ns->eth.tx_multicast);
	PMD_DRV_LOG(DEBUG, "tx_broadcast:        %"PRIu64"", ns->eth.tx_broadcast);
	PMD_DRV_LOG(DEBUG, "tx_discards:         %"PRIu64"", ns->eth.tx_discards);
	PMD_DRV_LOG(DEBUG, "tx_errors:           %"PRIu64"", ns->eth.tx_errors);

	PMD_DRV_LOG(DEBUG, "tx_dropped_link_down:     %"PRIu64"",
		    ns->tx_dropped_link_down);
	PMD_DRV_LOG(DEBUG, "crc_errors:               %"PRIu64"", ns->crc_errors);
	PMD_DRV_LOG(DEBUG, "illegal_bytes:            %"PRIu64"",
		    ns->illegal_bytes);
	PMD_DRV_LOG(DEBUG, "error_bytes:              %"PRIu64"", ns->error_bytes);
	PMD_DRV_LOG(DEBUG, "mac_local_faults:         %"PRIu64"",
		    ns->mac_local_faults);
	PMD_DRV_LOG(DEBUG, "mac_remote_faults:        %"PRIu64"",
		    ns->mac_remote_faults);
	PMD_DRV_LOG(DEBUG, "rx_length_errors:         %"PRIu64"",
		    ns->rx_length_errors);
	PMD_DRV_LOG(DEBUG, "link_xon_rx:              %"PRIu64"", ns->link_xon_rx);
	PMD_DRV_LOG(DEBUG, "link_xoff_rx:             %"PRIu64"", ns->link_xoff_rx);
	for (i = 0; i < 8; i++) {
		PMD_DRV_LOG(DEBUG, "priority_xon_rx[%d]:      %"PRIu64"",
				i, ns->priority_xon_rx[i]);
		PMD_DRV_LOG(DEBUG, "priority_xoff_rx[%d]:     %"PRIu64"",
				i, ns->priority_xoff_rx[i]);
	}
	PMD_DRV_LOG(DEBUG, "link_xon_tx:              %"PRIu64"", ns->link_xon_tx);
	PMD_DRV_LOG(DEBUG, "link_xoff_tx:             %"PRIu64"", ns->link_xoff_tx);
	for (i = 0; i < 8; i++) {
		PMD_DRV_LOG(DEBUG, "priority_xon_tx[%d]:      %"PRIu64"",
				i, ns->priority_xon_tx[i]);
		PMD_DRV_LOG(DEBUG, "priority_xoff_tx[%d]:     %"PRIu64"",
				i, ns->priority_xoff_tx[i]);
		PMD_DRV_LOG(DEBUG, "priority_xon_2_xoff[%d]:  %"PRIu64"",
				i, ns->priority_xon_2_xoff[i]);
	}
	PMD_DRV_LOG(DEBUG, "rx_size_64:               %"PRIu64"", ns->rx_size_64);
	PMD_DRV_LOG(DEBUG, "rx_size_127:              %"PRIu64"", ns->rx_size_127);
	PMD_DRV_LOG(DEBUG, "rx_size_255:              %"PRIu64"", ns->rx_size_255);
	PMD_DRV_LOG(DEBUG, "rx_size_511:              %"PRIu64"", ns->rx_size_511);
	PMD_DRV_LOG(DEBUG, "rx_size_1023:             %"PRIu64"", ns->rx_size_1023);
	PMD_DRV_LOG(DEBUG, "rx_size_1522:             %"PRIu64"", ns->rx_size_1522);
	PMD_DRV_LOG(DEBUG, "rx_size_big:              %"PRIu64"", ns->rx_size_big);
	PMD_DRV_LOG(DEBUG, "rx_undersize:             %"PRIu64"", ns->rx_undersize);
	PMD_DRV_LOG(DEBUG, "rx_fragments:             %"PRIu64"", ns->rx_fragments);
	PMD_DRV_LOG(DEBUG, "rx_oversize:              %"PRIu64"", ns->rx_oversize);
	PMD_DRV_LOG(DEBUG, "rx_jabber:                %"PRIu64"", ns->rx_jabber);
	PMD_DRV_LOG(DEBUG, "tx_size_64:               %"PRIu64"", ns->tx_size_64);
	PMD_DRV_LOG(DEBUG, "tx_size_127:              %"PRIu64"", ns->tx_size_127);
	PMD_DRV_LOG(DEBUG, "tx_size_255:              %"PRIu64"", ns->tx_size_255);
	PMD_DRV_LOG(DEBUG, "tx_size_511:              %"PRIu64"", ns->tx_size_511);
	PMD_DRV_LOG(DEBUG, "tx_size_1023:             %"PRIu64"", ns->tx_size_1023);
	PMD_DRV_LOG(DEBUG, "tx_size_1522:             %"PRIu64"", ns->tx_size_1522);
	PMD_DRV_LOG(DEBUG, "tx_size_big:              %"PRIu64"", ns->tx_size_big);
	PMD_DRV_LOG(DEBUG, "mac_short_packet_dropped: %"PRIu64"",
			ns->mac_short_packet_dropped);
	PMD_DRV_LOG(DEBUG, "checksum_error:           %"PRIu64"",
		    ns->checksum_error);
	PMD_DRV_LOG(DEBUG, "fdir_match:               %"PRIu64"", ns->fd_sb_match);
	PMD_DRV_LOG(DEBUG, "***************** PF stats end ********************");
}

/* Reset the statistics */
static void
i40e_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Mark PF and VSI stats to update the offset, aka "reset" */
	pf->offset_loaded = false;
	if (pf->main_vsi)
		pf->main_vsi->offset_loaded = false;

	/* read the stats, reading current register values into offset */
	i40e_read_stats_registers(pf, hw);
}

static uint32_t
i40e_xstats_calc_num(void)
{
	return I40E_NB_ETH_XSTATS + I40E_NB_HW_PORT_XSTATS +
		(I40E_NB_RXQ_PRIO_XSTATS * 8) +
		(I40E_NB_TXQ_PRIO_XSTATS * 8);
}

static int i40e_dev_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
				     struct rte_eth_xstat_name *xstats_names,
				     __rte_unused unsigned limit)
{
	unsigned count = 0;
	unsigned i, prio;

	if (xstats_names == NULL)
		return i40e_xstats_calc_num();

	/* Note: limit checked in rte_eth_xstats_names() */

	/* Get stats from i40e_eth_stats struct */
	for (i = 0; i < I40E_NB_ETH_XSTATS; i++) {
		snprintf(xstats_names[count].name,
			 sizeof(xstats_names[count].name),
			 "%s", rte_i40e_stats_strings[i].name);
		count++;
	}

	/* Get individiual stats from i40e_hw_port struct */
	for (i = 0; i < I40E_NB_HW_PORT_XSTATS; i++) {
		snprintf(xstats_names[count].name,
			sizeof(xstats_names[count].name),
			 "%s", rte_i40e_hw_port_strings[i].name);
		count++;
	}

	for (i = 0; i < I40E_NB_RXQ_PRIO_XSTATS; i++) {
		for (prio = 0; prio < 8; prio++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "rx_priority%u_%s", prio,
				 rte_i40e_rxq_prio_strings[i].name);
			count++;
		}
	}

	for (i = 0; i < I40E_NB_TXQ_PRIO_XSTATS; i++) {
		for (prio = 0; prio < 8; prio++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "tx_priority%u_%s", prio,
				 rte_i40e_txq_prio_strings[i].name);
			count++;
		}
	}
	return count;
}

static int
i40e_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		    unsigned n)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	unsigned i, count, prio;
	struct i40e_hw_port_stats *hw_stats = &pf->stats;

	count = i40e_xstats_calc_num();
	if (n < count)
		return count;

	i40e_read_stats_registers(pf, hw);

	if (xstats == NULL)
		return 0;

	count = 0;

	/* Get stats from i40e_eth_stats struct */
	for (i = 0; i < I40E_NB_ETH_XSTATS; i++) {
		xstats[count].value = *(uint64_t *)(((char *)&hw_stats->eth) +
			rte_i40e_stats_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	/* Get individiual stats from i40e_hw_port struct */
	for (i = 0; i < I40E_NB_HW_PORT_XSTATS; i++) {
		xstats[count].value = *(uint64_t *)(((char *)hw_stats) +
			rte_i40e_hw_port_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	for (i = 0; i < I40E_NB_RXQ_PRIO_XSTATS; i++) {
		for (prio = 0; prio < 8; prio++) {
			xstats[count].value =
				*(uint64_t *)(((char *)hw_stats) +
				rte_i40e_rxq_prio_strings[i].offset +
				(sizeof(uint64_t) * prio));
			xstats[count].id = count;
			count++;
		}
	}

	for (i = 0; i < I40E_NB_TXQ_PRIO_XSTATS; i++) {
		for (prio = 0; prio < 8; prio++) {
			xstats[count].value =
				*(uint64_t *)(((char *)hw_stats) +
				rte_i40e_txq_prio_strings[i].offset +
				(sizeof(uint64_t) * prio));
			xstats[count].id = count;
			count++;
		}
	}

	return count;
}

static int
i40e_dev_queue_stats_mapping_set(__rte_unused struct rte_eth_dev *dev,
				 __rte_unused uint16_t queue_id,
				 __rte_unused uint8_t stat_idx,
				 __rte_unused uint8_t is_rx)
{
	PMD_INIT_FUNC_TRACE();

	return -ENOSYS;
}

static int
i40e_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	u32 full_ver;
	u8 ver, patch;
	u16 build;
	int ret;

	full_ver = hw->nvm.oem_ver;
	ver = (u8)(full_ver >> 24);
	build = (u16)((full_ver >> 8) & 0xffff);
	patch = (u8)(full_ver & 0xff);

	ret = snprintf(fw_version, fw_size,
		 "%d.%d%d 0x%08x %d.%d.%d",
		 ((hw->nvm.version >> 12) & 0xf),
		 ((hw->nvm.version >> 4) & 0xff),
		 (hw->nvm.version & 0xf), hw->nvm.eetrack,
		 ver, build, patch);

	ret += 1; /* add the size of '\0' */
	if (fw_size < (u32)ret)
		return ret;
	else
		return 0;
}

static void
i40e_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	dev_info->pci_dev = pci_dev;
	dev_info->max_rx_queues = vsi->nb_qps;
	dev_info->max_tx_queues = vsi->nb_qps;
	dev_info->min_rx_bufsize = I40E_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = I40E_FRAME_SIZE_MAX;
	dev_info->max_mac_addrs = vsi->max_macaddrs;
	dev_info->max_vfs = pci_dev->max_vfs;
	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_QINQ_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_QINQ_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_SCTP_CKSUM |
		DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_TX_OFFLOAD_TCP_TSO |
		DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
		DEV_TX_OFFLOAD_GRE_TNL_TSO |
		DEV_TX_OFFLOAD_IPIP_TNL_TSO |
		DEV_TX_OFFLOAD_GENEVE_TNL_TSO;
	dev_info->hash_key_size = (I40E_PFQF_HKEY_MAX_INDEX + 1) *
						sizeof(uint32_t);
	dev_info->reta_size = pf->hash_lut_size;
	dev_info->flow_type_rss_offloads = I40E_RSS_OFFLOAD_ALL;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = I40E_DEFAULT_RX_PTHRESH,
			.hthresh = I40E_DEFAULT_RX_HTHRESH,
			.wthresh = I40E_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = I40E_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = I40E_DEFAULT_TX_PTHRESH,
			.hthresh = I40E_DEFAULT_TX_HTHRESH,
			.wthresh = I40E_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = I40E_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = I40E_DEFAULT_TX_RSBIT_THRESH,
		.txq_flags = ETH_TXQ_FLAGS_NOMULTSEGS |
				ETH_TXQ_FLAGS_NOOFFLOADS,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = I40E_MAX_RING_DESC,
		.nb_min = I40E_MIN_RING_DESC,
		.nb_align = I40E_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = I40E_MAX_RING_DESC,
		.nb_min = I40E_MIN_RING_DESC,
		.nb_align = I40E_ALIGN_RING_DESC,
		.nb_seg_max = I40E_TX_MAX_SEG,
		.nb_mtu_seg_max = I40E_TX_MAX_MTU_SEG,
	};

	if (pf->flags & I40E_FLAG_VMDQ) {
		dev_info->max_vmdq_pools = pf->max_nb_vmdq_vsi;
		dev_info->vmdq_queue_base = dev_info->max_rx_queues;
		dev_info->vmdq_queue_num = pf->vmdq_nb_qps *
						pf->max_nb_vmdq_vsi;
		dev_info->vmdq_pool_base = I40E_VMDQ_POOL_BASE;
		dev_info->max_rx_queues += dev_info->vmdq_queue_num;
		dev_info->max_tx_queues += dev_info->vmdq_queue_num;
	}

	if (I40E_PHY_TYPE_SUPPORT_40G(hw->phy.phy_types))
		/* For XL710 */
		dev_info->speed_capa = ETH_LINK_SPEED_40G;
	else if (I40E_PHY_TYPE_SUPPORT_25G(hw->phy.phy_types))
		/* For XXV710 */
		dev_info->speed_capa = ETH_LINK_SPEED_25G;
	else
		/* For X710 */
		dev_info->speed_capa = ETH_LINK_SPEED_1G | ETH_LINK_SPEED_10G;
}

static int
i40e_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	PMD_INIT_FUNC_TRACE();

	if (on)
		return i40e_vsi_add_vlan(vsi, vlan_id);
	else
		return i40e_vsi_delete_vlan(vsi, vlan_id);
}

static int
i40e_vlan_tpid_set(struct rte_eth_dev *dev,
		   enum rte_vlan_type vlan_type,
		   uint16_t tpid)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t reg_r = 0, reg_w = 0;
	uint16_t reg_id = 0;
	int ret = 0;
	int qinq = dev->data->dev_conf.rxmode.hw_vlan_extend;

	switch (vlan_type) {
	case ETH_VLAN_TYPE_OUTER:
		if (qinq)
			reg_id = 2;
		else
			reg_id = 3;
		break;
	case ETH_VLAN_TYPE_INNER:
		if (qinq)
			reg_id = 3;
		else {
			ret = -EINVAL;
			PMD_DRV_LOG(ERR,
				"Unsupported vlan type in single vlan.");
			return ret;
		}
		break;
	default:
		ret = -EINVAL;
		PMD_DRV_LOG(ERR, "Unsupported vlan type %d", vlan_type);
		return ret;
	}
	ret = i40e_aq_debug_read_register(hw, I40E_GL_SWT_L2TAGCTRL(reg_id),
					  &reg_r, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR,
			   "Fail to debug read from I40E_GL_SWT_L2TAGCTRL[%d]",
			   reg_id);
		ret = -EIO;
		return ret;
	}
	PMD_DRV_LOG(DEBUG,
		"Debug read from I40E_GL_SWT_L2TAGCTRL[%d]: 0x%08"PRIx64,
		reg_id, reg_r);

	reg_w = reg_r & (~(I40E_GL_SWT_L2TAGCTRL_ETHERTYPE_MASK));
	reg_w |= ((uint64_t)tpid << I40E_GL_SWT_L2TAGCTRL_ETHERTYPE_SHIFT);
	if (reg_r == reg_w) {
		ret = 0;
		PMD_DRV_LOG(DEBUG, "No need to write");
		return ret;
	}

	ret = i40e_aq_debug_write_register(hw, I40E_GL_SWT_L2TAGCTRL(reg_id),
					   reg_w, NULL);
	if (ret != I40E_SUCCESS) {
		ret = -EIO;
		PMD_DRV_LOG(ERR,
			"Fail to debug write to I40E_GL_SWT_L2TAGCTRL[%d]",
			reg_id);
		return ret;
	}
	PMD_DRV_LOG(DEBUG,
		"Debug write 0x%08"PRIx64" to I40E_GL_SWT_L2TAGCTRL[%d]",
		reg_w, reg_id);

	return ret;
}

static void
i40e_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;

	if (mask & ETH_VLAN_FILTER_MASK) {
		if (dev->data->dev_conf.rxmode.hw_vlan_filter)
			i40e_vsi_config_vlan_filter(vsi, TRUE);
		else
			i40e_vsi_config_vlan_filter(vsi, FALSE);
	}

	if (mask & ETH_VLAN_STRIP_MASK) {
		/* Enable or disable VLAN stripping */
		if (dev->data->dev_conf.rxmode.hw_vlan_strip)
			i40e_vsi_config_vlan_stripping(vsi, TRUE);
		else
			i40e_vsi_config_vlan_stripping(vsi, FALSE);
	}

	if (mask & ETH_VLAN_EXTEND_MASK) {
		if (dev->data->dev_conf.rxmode.hw_vlan_extend) {
			i40e_vsi_config_double_vlan(vsi, TRUE);
			/* Set global registers with default ether type value */
			i40e_vlan_tpid_set(dev, ETH_VLAN_TYPE_OUTER,
					   ETHER_TYPE_VLAN);
			i40e_vlan_tpid_set(dev, ETH_VLAN_TYPE_INNER,
					   ETHER_TYPE_VLAN);
		}
		else
			i40e_vsi_config_double_vlan(vsi, FALSE);
	}
}

static void
i40e_vlan_strip_queue_set(__rte_unused struct rte_eth_dev *dev,
			  __rte_unused uint16_t queue,
			  __rte_unused int on)
{
	PMD_INIT_FUNC_TRACE();
}

static int
i40e_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t pvid, int on)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev_data *data = I40E_VSI_TO_DEV_DATA(vsi);
	struct i40e_vsi_vlan_pvid_info info;

	memset(&info, 0, sizeof(info));
	info.on = on;
	if (info.on)
		info.config.pvid = pvid;
	else {
		info.config.reject.tagged =
				data->dev_conf.txmode.hw_vlan_reject_tagged;
		info.config.reject.untagged =
				data->dev_conf.txmode.hw_vlan_reject_untagged;
	}

	return i40e_vsi_vlan_pvid_set(vsi, &info);
}

static int
i40e_dev_led_on(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t mode = i40e_led_get(hw);

	if (mode == 0)
		i40e_led_set(hw, 0xf, true); /* 0xf means led always true */

	return 0;
}

static int
i40e_dev_led_off(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t mode = i40e_led_get(hw);

	if (mode != 0)
		i40e_led_set(hw, 0, false);

	return 0;
}

static int
i40e_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	fc_conf->pause_time = pf->fc_conf.pause_time;
	fc_conf->high_water =  pf->fc_conf.high_water[I40E_MAX_TRAFFIC_CLASS];
	fc_conf->low_water = pf->fc_conf.low_water[I40E_MAX_TRAFFIC_CLASS];

	 /* Return current mode according to actual setting*/
	switch (hw->fc.current_mode) {
	case I40E_FC_FULL:
		fc_conf->mode = RTE_FC_FULL;
		break;
	case I40E_FC_TX_PAUSE:
		fc_conf->mode = RTE_FC_TX_PAUSE;
		break;
	case I40E_FC_RX_PAUSE:
		fc_conf->mode = RTE_FC_RX_PAUSE;
		break;
	case I40E_FC_NONE:
	default:
		fc_conf->mode = RTE_FC_NONE;
	};

	return 0;
}

static int
i40e_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	uint32_t mflcn_reg, fctrl_reg, reg;
	uint32_t max_high_water;
	uint8_t i, aq_failure;
	int err;
	struct i40e_hw *hw;
	struct i40e_pf *pf;
	enum i40e_fc_mode rte_fcmode_2_i40e_fcmode[] = {
		[RTE_FC_NONE] = I40E_FC_NONE,
		[RTE_FC_RX_PAUSE] = I40E_FC_RX_PAUSE,
		[RTE_FC_TX_PAUSE] = I40E_FC_TX_PAUSE,
		[RTE_FC_FULL] = I40E_FC_FULL
	};

	/* high_water field in the rte_eth_fc_conf using the kilobytes unit */

	max_high_water = I40E_RXPBSIZE >> I40E_KILOSHIFT;
	if ((fc_conf->high_water > max_high_water) ||
			(fc_conf->high_water < fc_conf->low_water)) {
		PMD_INIT_LOG(ERR,
			"Invalid high/low water setup value in KB, High_water must be <= %d.",
			max_high_water);
		return -EINVAL;
	}

	hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	hw->fc.requested_mode = rte_fcmode_2_i40e_fcmode[fc_conf->mode];

	pf->fc_conf.pause_time = fc_conf->pause_time;
	pf->fc_conf.high_water[I40E_MAX_TRAFFIC_CLASS] = fc_conf->high_water;
	pf->fc_conf.low_water[I40E_MAX_TRAFFIC_CLASS] = fc_conf->low_water;

	PMD_INIT_FUNC_TRACE();

	/* All the link flow control related enable/disable register
	 * configuration is handle by the F/W
	 */
	err = i40e_set_fc(hw, &aq_failure, true);
	if (err < 0)
		return -ENOSYS;

	if (I40E_PHY_TYPE_SUPPORT_40G(hw->phy.phy_types)) {
		/* Configure flow control refresh threshold,
		 * the value for stat_tx_pause_refresh_timer[8]
		 * is used for global pause operation.
		 */

		I40E_WRITE_REG(hw,
			       I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER(8),
			       pf->fc_conf.pause_time);

		/* configure the timer value included in transmitted pause
		 * frame,
		 * the value for stat_tx_pause_quanta[8] is used for global
		 * pause operation
		 */
		I40E_WRITE_REG(hw, I40E_PRTMAC_HSEC_CTL_TX_PAUSE_QUANTA(8),
			       pf->fc_conf.pause_time);

		fctrl_reg = I40E_READ_REG(hw,
					  I40E_PRTMAC_HSEC_CTL_RX_FORWARD_CONTROL);

		if (fc_conf->mac_ctrl_frame_fwd != 0)
			fctrl_reg |= I40E_PRTMAC_FWD_CTRL;
		else
			fctrl_reg &= ~I40E_PRTMAC_FWD_CTRL;

		I40E_WRITE_REG(hw, I40E_PRTMAC_HSEC_CTL_RX_FORWARD_CONTROL,
			       fctrl_reg);
	} else {
		/* Configure pause time (2 TCs per register) */
		reg = (uint32_t)pf->fc_conf.pause_time * (uint32_t)0x00010001;
		for (i = 0; i < I40E_MAX_TRAFFIC_CLASS / 2; i++)
			I40E_WRITE_REG(hw, I40E_PRTDCB_FCTTVN(i), reg);

		/* Configure flow control refresh threshold value */
		I40E_WRITE_REG(hw, I40E_PRTDCB_FCRTV,
			       pf->fc_conf.pause_time / 2);

		mflcn_reg = I40E_READ_REG(hw, I40E_PRTDCB_MFLCN);

		/* set or clear MFLCN.PMCF & MFLCN.DPF bits
		 *depending on configuration
		 */
		if (fc_conf->mac_ctrl_frame_fwd != 0) {
			mflcn_reg |= I40E_PRTDCB_MFLCN_PMCF_MASK;
			mflcn_reg &= ~I40E_PRTDCB_MFLCN_DPF_MASK;
		} else {
			mflcn_reg &= ~I40E_PRTDCB_MFLCN_PMCF_MASK;
			mflcn_reg |= I40E_PRTDCB_MFLCN_DPF_MASK;
		}

		I40E_WRITE_REG(hw, I40E_PRTDCB_MFLCN, mflcn_reg);
	}

	/* config the water marker both based on the packets and bytes */
	I40E_WRITE_REG(hw, I40E_GLRPB_PHW,
		       (pf->fc_conf.high_water[I40E_MAX_TRAFFIC_CLASS]
		       << I40E_KILOSHIFT) / I40E_PACKET_AVERAGE_SIZE);
	I40E_WRITE_REG(hw, I40E_GLRPB_PLW,
		       (pf->fc_conf.low_water[I40E_MAX_TRAFFIC_CLASS]
		       << I40E_KILOSHIFT) / I40E_PACKET_AVERAGE_SIZE);
	I40E_WRITE_REG(hw, I40E_GLRPB_GHW,
		       pf->fc_conf.high_water[I40E_MAX_TRAFFIC_CLASS]
		       << I40E_KILOSHIFT);
	I40E_WRITE_REG(hw, I40E_GLRPB_GLW,
		       pf->fc_conf.low_water[I40E_MAX_TRAFFIC_CLASS]
		       << I40E_KILOSHIFT);

	I40E_WRITE_FLUSH(hw);

	return 0;
}

static int
i40e_priority_flow_ctrl_set(__rte_unused struct rte_eth_dev *dev,
			    __rte_unused struct rte_eth_pfc_conf *pfc_conf)
{
	PMD_INIT_FUNC_TRACE();

	return -ENOSYS;
}

/* Add a MAC address, and update filters */
static int
i40e_macaddr_add(struct rte_eth_dev *dev,
		 struct ether_addr *mac_addr,
		 __rte_unused uint32_t index,
		 uint32_t pool)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_mac_filter_info mac_filter;
	struct i40e_vsi *vsi;
	int ret;

	/* If VMDQ not enabled or configured, return */
	if (pool != 0 && (!(pf->flags & I40E_FLAG_VMDQ) ||
			  !pf->nb_cfg_vmdq_vsi)) {
		PMD_DRV_LOG(ERR, "VMDQ not %s, can't set mac to pool %u",
			pf->flags & I40E_FLAG_VMDQ ? "configured" : "enabled",
			pool);
		return -ENOTSUP;
	}

	if (pool > pf->nb_cfg_vmdq_vsi) {
		PMD_DRV_LOG(ERR, "Pool number %u invalid. Max pool is %u",
				pool, pf->nb_cfg_vmdq_vsi);
		return -EINVAL;
	}

	(void)rte_memcpy(&mac_filter.mac_addr, mac_addr, ETHER_ADDR_LEN);
	if (dev->data->dev_conf.rxmode.hw_vlan_filter)
		mac_filter.filter_type = RTE_MACVLAN_PERFECT_MATCH;
	else
		mac_filter.filter_type = RTE_MAC_PERFECT_MATCH;

	if (pool == 0)
		vsi = pf->main_vsi;
	else
		vsi = pf->vmdq[pool - 1].vsi;

	ret = i40e_vsi_add_mac(vsi, &mac_filter);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MACVLAN filter");
		return -ENODEV;
	}
	return 0;
}

/* Remove a MAC address, and update filters */
static void
i40e_macaddr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_vsi *vsi;
	struct rte_eth_dev_data *data = dev->data;
	struct ether_addr *macaddr;
	int ret;
	uint32_t i;
	uint64_t pool_sel;

	macaddr = &(data->mac_addrs[index]);

	pool_sel = dev->data->mac_pool_sel[index];

	for (i = 0; i < sizeof(pool_sel) * CHAR_BIT; i++) {
		if (pool_sel & (1ULL << i)) {
			if (i == 0)
				vsi = pf->main_vsi;
			else {
				/* No VMDQ pool enabled or configured */
				if (!(pf->flags & I40E_FLAG_VMDQ) ||
					(i > pf->nb_cfg_vmdq_vsi)) {
					PMD_DRV_LOG(ERR,
						"No VMDQ pool enabled/configured");
					return;
				}
				vsi = pf->vmdq[i - 1].vsi;
			}
			ret = i40e_vsi_delete_mac(vsi, macaddr);

			if (ret) {
				PMD_DRV_LOG(ERR, "Failed to remove MACVLAN filter");
				return;
			}
		}
	}
}

/* Set perfect match or hash match of MAC and VLAN for a VF */
static int
i40e_vf_mac_filter_set(struct i40e_pf *pf,
		 struct rte_eth_mac_filter *filter,
		 bool add)
{
	struct i40e_hw *hw;
	struct i40e_mac_filter_info mac_filter;
	struct ether_addr old_mac;
	struct ether_addr *new_mac;
	struct i40e_pf_vf *vf = NULL;
	uint16_t vf_id;
	int ret;

	if (pf == NULL) {
		PMD_DRV_LOG(ERR, "Invalid PF argument.");
		return -EINVAL;
	}
	hw = I40E_PF_TO_HW(pf);

	if (filter == NULL) {
		PMD_DRV_LOG(ERR, "Invalid mac filter argument.");
		return -EINVAL;
	}

	new_mac = &filter->mac_addr;

	if (is_zero_ether_addr(new_mac)) {
		PMD_DRV_LOG(ERR, "Invalid ethernet address.");
		return -EINVAL;
	}

	vf_id = filter->dst_id;

	if (vf_id > pf->vf_num - 1 || !pf->vfs) {
		PMD_DRV_LOG(ERR, "Invalid argument.");
		return -EINVAL;
	}
	vf = &pf->vfs[vf_id];

	if (add && is_same_ether_addr(new_mac, &(pf->dev_addr))) {
		PMD_DRV_LOG(INFO, "Ignore adding permanent MAC address.");
		return -EINVAL;
	}

	if (add) {
		(void)rte_memcpy(&old_mac, hw->mac.addr, ETHER_ADDR_LEN);
		(void)rte_memcpy(hw->mac.addr, new_mac->addr_bytes,
				ETHER_ADDR_LEN);
		(void)rte_memcpy(&mac_filter.mac_addr, &filter->mac_addr,
				 ETHER_ADDR_LEN);

		mac_filter.filter_type = filter->filter_type;
		ret = i40e_vsi_add_mac(vf->vsi, &mac_filter);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to add MAC filter.");
			return -1;
		}
		ether_addr_copy(new_mac, &pf->dev_addr);
	} else {
		(void)rte_memcpy(hw->mac.addr, hw->mac.perm_addr,
				ETHER_ADDR_LEN);
		ret = i40e_vsi_delete_mac(vf->vsi, &filter->mac_addr);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to delete MAC filter.");
			return -1;
		}

		/* Clear device address as it has been removed */
		if (is_same_ether_addr(&(pf->dev_addr), new_mac))
			memset(&pf->dev_addr, 0, sizeof(struct ether_addr));
	}

	return 0;
}

/* MAC filter handle */
static int
i40e_mac_filter_handle(struct rte_eth_dev *dev, enum rte_filter_op filter_op,
		void *arg)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_eth_mac_filter *filter;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	int ret = I40E_NOT_SUPPORTED;

	filter = (struct rte_eth_mac_filter *)(arg);

	switch (filter_op) {
	case RTE_ETH_FILTER_NOP:
		ret = I40E_SUCCESS;
		break;
	case RTE_ETH_FILTER_ADD:
		i40e_pf_disable_irq0(hw);
		if (filter->is_vf)
			ret = i40e_vf_mac_filter_set(pf, filter, 1);
		i40e_pf_enable_irq0(hw);
		break;
	case RTE_ETH_FILTER_DELETE:
		i40e_pf_disable_irq0(hw);
		if (filter->is_vf)
			ret = i40e_vf_mac_filter_set(pf, filter, 0);
		i40e_pf_enable_irq0(hw);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown operation %u", filter_op);
		ret = I40E_ERR_PARAM;
		break;
	}

	return ret;
}

static int
i40e_get_rss_lut(struct i40e_vsi *vsi, uint8_t *lut, uint16_t lut_size)
{
	struct i40e_pf *pf = I40E_VSI_TO_PF(vsi);
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	int ret;

	if (!lut)
		return -EINVAL;

	if (pf->flags & I40E_FLAG_RSS_AQ_CAPABLE) {
		ret = i40e_aq_get_rss_lut(hw, vsi->vsi_id, TRUE,
					  lut, lut_size);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to get RSS lookup table");
			return ret;
		}
	} else {
		uint32_t *lut_dw = (uint32_t *)lut;
		uint16_t i, lut_size_dw = lut_size / 4;

		for (i = 0; i < lut_size_dw; i++)
			lut_dw[i] = I40E_READ_REG(hw, I40E_PFQF_HLUT(i));
	}

	return 0;
}

static int
i40e_set_rss_lut(struct i40e_vsi *vsi, uint8_t *lut, uint16_t lut_size)
{
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	int ret;

	if (!vsi || !lut)
		return -EINVAL;

	pf = I40E_VSI_TO_PF(vsi);
	hw = I40E_VSI_TO_HW(vsi);

	if (pf->flags & I40E_FLAG_RSS_AQ_CAPABLE) {
		ret = i40e_aq_set_rss_lut(hw, vsi->vsi_id, TRUE,
					  lut, lut_size);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to set RSS lookup table");
			return ret;
		}
	} else {
		uint32_t *lut_dw = (uint32_t *)lut;
		uint16_t i, lut_size_dw = lut_size / 4;

		for (i = 0; i < lut_size_dw; i++)
			I40E_WRITE_REG(hw, I40E_PFQF_HLUT(i), lut_dw[i]);
		I40E_WRITE_FLUSH(hw);
	}

	return 0;
}

static int
i40e_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	uint16_t i, lut_size = pf->hash_lut_size;
	uint16_t idx, shift;
	uint8_t *lut;
	int ret;

	if (reta_size != lut_size ||
		reta_size > ETH_RSS_RETA_SIZE_512) {
		PMD_DRV_LOG(ERR,
			"The size of hash lookup table configured (%d) doesn't match the number hardware can supported (%d)",
			reta_size, lut_size);
		return -EINVAL;
	}

	lut = rte_zmalloc("i40e_rss_lut", reta_size, 0);
	if (!lut) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}
	ret = i40e_get_rss_lut(pf->main_vsi, lut, reta_size);
	if (ret)
		goto out;
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			lut[i] = reta_conf[idx].reta[shift];
	}
	ret = i40e_set_rss_lut(pf->main_vsi, lut, reta_size);

out:
	rte_free(lut);

	return ret;
}

static int
i40e_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	uint16_t i, lut_size = pf->hash_lut_size;
	uint16_t idx, shift;
	uint8_t *lut;
	int ret;

	if (reta_size != lut_size ||
		reta_size > ETH_RSS_RETA_SIZE_512) {
		PMD_DRV_LOG(ERR,
			"The size of hash lookup table configured (%d) doesn't match the number hardware can supported (%d)",
			reta_size, lut_size);
		return -EINVAL;
	}

	lut = rte_zmalloc("i40e_rss_lut", reta_size, 0);
	if (!lut) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}

	ret = i40e_get_rss_lut(pf->main_vsi, lut, reta_size);
	if (ret)
		goto out;
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = lut[i];
	}

out:
	rte_free(lut);

	return ret;
}

/**
 * i40e_allocate_dma_mem_d - specific memory alloc for shared code (base driver)
 * @hw:   pointer to the HW structure
 * @mem:  pointer to mem struct to fill out
 * @size: size of memory requested
 * @alignment: what to align the allocation to
 **/
enum i40e_status_code
i40e_allocate_dma_mem_d(__attribute__((unused)) struct i40e_hw *hw,
			struct i40e_dma_mem *mem,
			u64 size,
			u32 alignment)
{
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	if (!mem)
		return I40E_ERR_PARAM;

	snprintf(z_name, sizeof(z_name), "i40e_dma_%"PRIu64, rte_rand());
	mz = rte_memzone_reserve_bounded(z_name, size, SOCKET_ID_ANY, 0,
					 alignment, RTE_PGSIZE_2M);
	if (!mz)
		return I40E_ERR_NO_MEMORY;

	mem->size = size;
	mem->va = mz->addr;
	mem->pa = rte_mem_phy2mch(mz->memseg_id, mz->phys_addr);
	mem->zone = (const void *)mz;
	PMD_DRV_LOG(DEBUG,
		"memzone %s allocated with physical address: %"PRIu64,
		mz->name, mem->pa);

	return I40E_SUCCESS;
}

/**
 * i40e_free_dma_mem_d - specific memory free for shared code (base driver)
 * @hw:   pointer to the HW structure
 * @mem:  ptr to mem struct to free
 **/
enum i40e_status_code
i40e_free_dma_mem_d(__attribute__((unused)) struct i40e_hw *hw,
		    struct i40e_dma_mem *mem)
{
	if (!mem)
		return I40E_ERR_PARAM;

	PMD_DRV_LOG(DEBUG,
		"memzone %s to be freed with physical address: %"PRIu64,
		((const struct rte_memzone *)mem->zone)->name, mem->pa);
	rte_memzone_free((const struct rte_memzone *)mem->zone);
	mem->zone = NULL;
	mem->va = NULL;
	mem->pa = (u64)0;

	return I40E_SUCCESS;
}

/**
 * i40e_allocate_virt_mem_d - specific memory alloc for shared code (base driver)
 * @hw:   pointer to the HW structure
 * @mem:  pointer to mem struct to fill out
 * @size: size of memory requested
 **/
enum i40e_status_code
i40e_allocate_virt_mem_d(__attribute__((unused)) struct i40e_hw *hw,
			 struct i40e_virt_mem *mem,
			 u32 size)
{
	if (!mem)
		return I40E_ERR_PARAM;

	mem->size = size;
	mem->va = rte_zmalloc("i40e", size, 0);

	if (mem->va)
		return I40E_SUCCESS;
	else
		return I40E_ERR_NO_MEMORY;
}

/**
 * i40e_free_virt_mem_d - specific memory free for shared code (base driver)
 * @hw:   pointer to the HW structure
 * @mem:  pointer to mem struct to free
 **/
enum i40e_status_code
i40e_free_virt_mem_d(__attribute__((unused)) struct i40e_hw *hw,
		     struct i40e_virt_mem *mem)
{
	if (!mem)
		return I40E_ERR_PARAM;

	rte_free(mem->va);
	mem->va = NULL;

	return I40E_SUCCESS;
}

void
i40e_init_spinlock_d(struct i40e_spinlock *sp)
{
	rte_spinlock_init(&sp->spinlock);
}

void
i40e_acquire_spinlock_d(struct i40e_spinlock *sp)
{
	rte_spinlock_lock(&sp->spinlock);
}

void
i40e_release_spinlock_d(struct i40e_spinlock *sp)
{
	rte_spinlock_unlock(&sp->spinlock);
}

void
i40e_destroy_spinlock_d(__attribute__((unused)) struct i40e_spinlock *sp)
{
	return;
}

/**
 * Get the hardware capabilities, which will be parsed
 * and saved into struct i40e_hw.
 */
static int
i40e_get_cap(struct i40e_hw *hw)
{
	struct i40e_aqc_list_capabilities_element_resp *buf;
	uint16_t len, size = 0;
	int ret;

	/* Calculate a huge enough buff for saving response data temporarily */
	len = sizeof(struct i40e_aqc_list_capabilities_element_resp) *
						I40E_MAX_CAP_ELE_NUM;
	buf = rte_zmalloc("i40e", len, 0);
	if (!buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	/* Get, parse the capabilities and save it to hw */
	ret = i40e_aq_discover_capabilities(hw, buf, len, &size,
			i40e_aqc_opc_list_func_capabilities, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to discover capabilities");

	/* Free the temporary buffer after being used */
	rte_free(buf);

	return ret;
}

static int
i40e_pf_parameter_init(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	uint16_t qp_count = 0, vsi_count = 0;

	if (pci_dev->max_vfs && !hw->func_caps.sr_iov_1_1) {
		PMD_INIT_LOG(ERR, "HW configuration doesn't support SRIOV");
		return -EINVAL;
	}
	/* Add the parameter init for LFC */
	pf->fc_conf.pause_time = I40E_DEFAULT_PAUSE_TIME;
	pf->fc_conf.high_water[I40E_MAX_TRAFFIC_CLASS] = I40E_DEFAULT_HIGH_WATER;
	pf->fc_conf.low_water[I40E_MAX_TRAFFIC_CLASS] = I40E_DEFAULT_LOW_WATER;

	pf->flags = I40E_FLAG_HEADER_SPLIT_DISABLED;
	pf->max_num_vsi = hw->func_caps.num_vsis;
	pf->lan_nb_qp_max = RTE_LIBRTE_I40E_QUEUE_NUM_PER_PF;
	pf->vmdq_nb_qp_max = RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM;
	pf->vf_nb_qp_max = RTE_LIBRTE_I40E_QUEUE_NUM_PER_VF;

	/* FDir queue/VSI allocation */
	pf->fdir_qp_offset = 0;
	if (hw->func_caps.fd) {
		pf->flags |= I40E_FLAG_FDIR;
		pf->fdir_nb_qps = I40E_DEFAULT_QP_NUM_FDIR;
	} else {
		pf->fdir_nb_qps = 0;
	}
	qp_count += pf->fdir_nb_qps;
	vsi_count += 1;

	/* LAN queue/VSI allocation */
	pf->lan_qp_offset = pf->fdir_qp_offset + pf->fdir_nb_qps;
	if (!hw->func_caps.rss) {
		pf->lan_nb_qps = 1;
	} else {
		pf->flags |= I40E_FLAG_RSS;
		if (hw->mac.type == I40E_MAC_X722)
			pf->flags |= I40E_FLAG_RSS_AQ_CAPABLE;
		pf->lan_nb_qps = pf->lan_nb_qp_max;
	}
	qp_count += pf->lan_nb_qps;
	vsi_count += 1;

	/* VF queue/VSI allocation */
	pf->vf_qp_offset = pf->lan_qp_offset + pf->lan_nb_qps;
	if (hw->func_caps.sr_iov_1_1 && pci_dev->max_vfs) {
		pf->flags |= I40E_FLAG_SRIOV;
		pf->vf_nb_qps = RTE_LIBRTE_I40E_QUEUE_NUM_PER_VF;
		pf->vf_num = pci_dev->max_vfs;
		PMD_DRV_LOG(DEBUG,
			"%u VF VSIs, %u queues per VF VSI, in total %u queues",
			pf->vf_num, pf->vf_nb_qps, pf->vf_nb_qps * pf->vf_num);
	} else {
		pf->vf_nb_qps = 0;
		pf->vf_num = 0;
	}
	qp_count += pf->vf_nb_qps * pf->vf_num;
	vsi_count += pf->vf_num;

	/* VMDq queue/VSI allocation */
	pf->vmdq_qp_offset = pf->vf_qp_offset + pf->vf_nb_qps * pf->vf_num;
	pf->vmdq_nb_qps = 0;
	pf->max_nb_vmdq_vsi = 0;
	if (hw->func_caps.vmdq) {
		if (qp_count < hw->func_caps.num_tx_qp &&
			vsi_count < hw->func_caps.num_vsis) {
			pf->max_nb_vmdq_vsi = (hw->func_caps.num_tx_qp -
				qp_count) / pf->vmdq_nb_qp_max;

			/* Limit the maximum number of VMDq vsi to the maximum
			 * ethdev can support
			 */
			pf->max_nb_vmdq_vsi = RTE_MIN(pf->max_nb_vmdq_vsi,
				hw->func_caps.num_vsis - vsi_count);
			pf->max_nb_vmdq_vsi = RTE_MIN(pf->max_nb_vmdq_vsi,
				ETH_64_POOLS);
			if (pf->max_nb_vmdq_vsi) {
				pf->flags |= I40E_FLAG_VMDQ;
				pf->vmdq_nb_qps = pf->vmdq_nb_qp_max;
				PMD_DRV_LOG(DEBUG,
					"%u VMDQ VSIs, %u queues per VMDQ VSI, in total %u queues",
					pf->max_nb_vmdq_vsi, pf->vmdq_nb_qps,
					pf->vmdq_nb_qps * pf->max_nb_vmdq_vsi);
			} else {
				PMD_DRV_LOG(INFO,
					"No enough queues left for VMDq");
			}
		} else {
			PMD_DRV_LOG(INFO, "No queue or VSI left for VMDq");
		}
	}
	qp_count += pf->vmdq_nb_qps * pf->max_nb_vmdq_vsi;
	vsi_count += pf->max_nb_vmdq_vsi;

	if (hw->func_caps.dcb)
		pf->flags |= I40E_FLAG_DCB;

	if (qp_count > hw->func_caps.num_tx_qp) {
		PMD_DRV_LOG(ERR,
			"Failed to allocate %u queues, which exceeds the hardware maximum %u",
			qp_count, hw->func_caps.num_tx_qp);
		return -EINVAL;
	}
	if (vsi_count > hw->func_caps.num_vsis) {
		PMD_DRV_LOG(ERR,
			"Failed to allocate %u VSIs, which exceeds the hardware maximum %u",
			vsi_count, hw->func_caps.num_vsis);
		return -EINVAL;
	}

	return 0;
}

static int
i40e_pf_get_switch_config(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_aqc_get_switch_config_resp *switch_config;
	struct i40e_aqc_switch_config_element_resp *element;
	uint16_t start_seid = 0, num_reported;
	int ret;

	switch_config = (struct i40e_aqc_get_switch_config_resp *)\
			rte_zmalloc("i40e", I40E_AQ_LARGE_BUF, 0);
	if (!switch_config) {
		PMD_DRV_LOG(ERR, "Failed to allocated memory");
		return -ENOMEM;
	}

	/* Get the switch configurations */
	ret = i40e_aq_get_switch_config(hw, switch_config,
		I40E_AQ_LARGE_BUF, &start_seid, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to get switch configurations");
		goto fail;
	}
	num_reported = rte_le_to_cpu_16(switch_config->header.num_reported);
	if (num_reported != 1) { /* The number should be 1 */
		PMD_DRV_LOG(ERR, "Wrong number of switch config reported");
		goto fail;
	}

	/* Parse the switch configuration elements */
	element = &(switch_config->element[0]);
	if (element->element_type == I40E_SWITCH_ELEMENT_TYPE_VSI) {
		pf->mac_seid = rte_le_to_cpu_16(element->uplink_seid);
		pf->main_vsi_seid = rte_le_to_cpu_16(element->seid);
	} else
		PMD_DRV_LOG(INFO, "Unknown element type");

fail:
	rte_free(switch_config);

	return ret;
}

static int
i40e_res_pool_init (struct i40e_res_pool_info *pool, uint32_t base,
			uint32_t num)
{
	struct pool_entry *entry;

	if (pool == NULL || num == 0)
		return -EINVAL;

	entry = rte_zmalloc("i40e", sizeof(*entry), 0);
	if (entry == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for resource pool");
		return -ENOMEM;
	}

	/* queue heap initialize */
	pool->num_free = num;
	pool->num_alloc = 0;
	pool->base = base;
	LIST_INIT(&pool->alloc_list);
	LIST_INIT(&pool->free_list);

	/* Initialize element  */
	entry->base = 0;
	entry->len = num;

	LIST_INSERT_HEAD(&pool->free_list, entry, next);
	return 0;
}

static void
i40e_res_pool_destroy(struct i40e_res_pool_info *pool)
{
	struct pool_entry *entry, *next_entry;

	if (pool == NULL)
		return;

	for (entry = LIST_FIRST(&pool->alloc_list);
			entry && (next_entry = LIST_NEXT(entry, next), 1);
			entry = next_entry) {
		LIST_REMOVE(entry, next);
		rte_free(entry);
	}

	for (entry = LIST_FIRST(&pool->free_list);
			entry && (next_entry = LIST_NEXT(entry, next), 1);
			entry = next_entry) {
		LIST_REMOVE(entry, next);
		rte_free(entry);
	}

	pool->num_free = 0;
	pool->num_alloc = 0;
	pool->base = 0;
	LIST_INIT(&pool->alloc_list);
	LIST_INIT(&pool->free_list);
}

static int
i40e_res_pool_free(struct i40e_res_pool_info *pool,
		       uint32_t base)
{
	struct pool_entry *entry, *next, *prev, *valid_entry = NULL;
	uint32_t pool_offset;
	int insert;

	if (pool == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameter");
		return -EINVAL;
	}

	pool_offset = base - pool->base;
	/* Lookup in alloc list */
	LIST_FOREACH(entry, &pool->alloc_list, next) {
		if (entry->base == pool_offset) {
			valid_entry = entry;
			LIST_REMOVE(entry, next);
			break;
		}
	}

	/* Not find, return */
	if (valid_entry == NULL) {
		PMD_DRV_LOG(ERR, "Failed to find entry");
		return -EINVAL;
	}

	/**
	 * Found it, move it to free list  and try to merge.
	 * In order to make merge easier, always sort it by qbase.
	 * Find adjacent prev and last entries.
	 */
	prev = next = NULL;
	LIST_FOREACH(entry, &pool->free_list, next) {
		if (entry->base > valid_entry->base) {
			next = entry;
			break;
		}
		prev = entry;
	}

	insert = 0;
	/* Try to merge with next one*/
	if (next != NULL) {
		/* Merge with next one */
		if (valid_entry->base + valid_entry->len == next->base) {
			next->base = valid_entry->base;
			next->len += valid_entry->len;
			rte_free(valid_entry);
			valid_entry = next;
			insert = 1;
		}
	}

	if (prev != NULL) {
		/* Merge with previous one */
		if (prev->base + prev->len == valid_entry->base) {
			prev->len += valid_entry->len;
			/* If it merge with next one, remove next node */
			if (insert == 1) {
				LIST_REMOVE(valid_entry, next);
				rte_free(valid_entry);
			} else {
				rte_free(valid_entry);
				insert = 1;
			}
		}
	}

	/* Not find any entry to merge, insert */
	if (insert == 0) {
		if (prev != NULL)
			LIST_INSERT_AFTER(prev, valid_entry, next);
		else if (next != NULL)
			LIST_INSERT_BEFORE(next, valid_entry, next);
		else /* It's empty list, insert to head */
			LIST_INSERT_HEAD(&pool->free_list, valid_entry, next);
	}

	pool->num_free += valid_entry->len;
	pool->num_alloc -= valid_entry->len;

	return 0;
}

static int
i40e_res_pool_alloc(struct i40e_res_pool_info *pool,
		       uint16_t num)
{
	struct pool_entry *entry, *valid_entry;

	if (pool == NULL || num == 0) {
		PMD_DRV_LOG(ERR, "Invalid parameter");
		return -EINVAL;
	}

	if (pool->num_free < num) {
		PMD_DRV_LOG(ERR, "No resource. ask:%u, available:%u",
			    num, pool->num_free);
		return -ENOMEM;
	}

	valid_entry = NULL;
	/* Lookup  in free list and find most fit one */
	LIST_FOREACH(entry, &pool->free_list, next) {
		if (entry->len >= num) {
			/* Find best one */
			if (entry->len == num) {
				valid_entry = entry;
				break;
			}
			if (valid_entry == NULL || valid_entry->len > entry->len)
				valid_entry = entry;
		}
	}

	/* Not find one to satisfy the request, return */
	if (valid_entry == NULL) {
		PMD_DRV_LOG(ERR, "No valid entry found");
		return -ENOMEM;
	}
	/**
	 * The entry have equal queue number as requested,
	 * remove it from alloc_list.
	 */
	if (valid_entry->len == num) {
		LIST_REMOVE(valid_entry, next);
	} else {
		/**
		 * The entry have more numbers than requested,
		 * create a new entry for alloc_list and minus its
		 * queue base and number in free_list.
		 */
		entry = rte_zmalloc("res_pool", sizeof(*entry), 0);
		if (entry == NULL) {
			PMD_DRV_LOG(ERR,
				"Failed to allocate memory for resource pool");
			return -ENOMEM;
		}
		entry->base = valid_entry->base;
		entry->len = num;
		valid_entry->base += num;
		valid_entry->len -= num;
		valid_entry = entry;
	}

	/* Insert it into alloc list, not sorted */
	LIST_INSERT_HEAD(&pool->alloc_list, valid_entry, next);

	pool->num_free -= valid_entry->len;
	pool->num_alloc += valid_entry->len;

	return valid_entry->base + pool->base;
}

/**
 * bitmap_is_subset - Check whether src2 is subset of src1
 **/
static inline int
bitmap_is_subset(uint8_t src1, uint8_t src2)
{
	return !((src1 ^ src2) & src2);
}

static enum i40e_status_code
validate_tcmap_parameter(struct i40e_vsi *vsi, uint8_t enabled_tcmap)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);

	/* If DCB is not supported, only default TC is supported */
	if (!hw->func_caps.dcb && enabled_tcmap != I40E_DEFAULT_TCMAP) {
		PMD_DRV_LOG(ERR, "DCB is not enabled, only TC0 is supported");
		return I40E_NOT_SUPPORTED;
	}

	if (!bitmap_is_subset(hw->func_caps.enabled_tcmap, enabled_tcmap)) {
		PMD_DRV_LOG(ERR,
			"Enabled TC map 0x%x not applicable to HW support 0x%x",
			hw->func_caps.enabled_tcmap, enabled_tcmap);
		return I40E_NOT_SUPPORTED;
	}
	return I40E_SUCCESS;
}

int
i40e_vsi_vlan_pvid_set(struct i40e_vsi *vsi,
				struct i40e_vsi_vlan_pvid_info *info)
{
	struct i40e_hw *hw;
	struct i40e_vsi_context ctxt;
	uint8_t vlan_flags = 0;
	int ret;

	if (vsi == NULL || info == NULL) {
		PMD_DRV_LOG(ERR, "invalid parameters");
		return I40E_ERR_PARAM;
	}

	if (info->on) {
		vsi->info.pvid = info->config.pvid;
		/**
		 * If insert pvid is enabled, only tagged pkts are
		 * allowed to be sent out.
		 */
		vlan_flags |= I40E_AQ_VSI_PVLAN_INSERT_PVID |
				I40E_AQ_VSI_PVLAN_MODE_TAGGED;
	} else {
		vsi->info.pvid = 0;
		if (info->config.reject.tagged == 0)
			vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_TAGGED;

		if (info->config.reject.untagged == 0)
			vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_UNTAGGED;
	}
	vsi->info.port_vlan_flags &= ~(I40E_AQ_VSI_PVLAN_INSERT_PVID |
					I40E_AQ_VSI_PVLAN_MODE_MASK);
	vsi->info.port_vlan_flags |= vlan_flags;
	vsi->info.valid_sections =
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
	memset(&ctxt, 0, sizeof(ctxt));
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.seid = vsi->seid;

	hw = I40E_VSI_TO_HW(vsi);
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret != I40E_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to update VSI params");

	return ret;
}

static int
i40e_vsi_update_tc_bandwidth(struct i40e_vsi *vsi, uint8_t enabled_tcmap)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	int i, ret;
	struct i40e_aqc_configure_vsi_tc_bw_data tc_bw_data;

	ret = validate_tcmap_parameter(vsi, enabled_tcmap);
	if (ret != I40E_SUCCESS)
		return ret;

	if (!vsi->seid) {
		PMD_DRV_LOG(ERR, "seid not valid");
		return -EINVAL;
	}

	memset(&tc_bw_data, 0, sizeof(tc_bw_data));
	tc_bw_data.tc_valid_bits = enabled_tcmap;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		tc_bw_data.tc_bw_credits[i] =
			(enabled_tcmap & (1 << i)) ? 1 : 0;

	ret = i40e_aq_config_vsi_tc_bw(hw, vsi->seid, &tc_bw_data, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to configure TC BW");
		return ret;
	}

	(void)rte_memcpy(vsi->info.qs_handle, tc_bw_data.qs_handles,
					sizeof(vsi->info.qs_handle));
	return I40E_SUCCESS;
}

static enum i40e_status_code
i40e_vsi_config_tc_queue_mapping(struct i40e_vsi *vsi,
				 struct i40e_aqc_vsi_properties_data *info,
				 uint8_t enabled_tcmap)
{
	enum i40e_status_code ret;
	int i, total_tc = 0;
	uint16_t qpnum_per_tc, bsf, qp_idx;

	ret = validate_tcmap_parameter(vsi, enabled_tcmap);
	if (ret != I40E_SUCCESS)
		return ret;

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		if (enabled_tcmap & (1 << i))
			total_tc++;
	vsi->enabled_tc = enabled_tcmap;

	/* Number of queues per enabled TC */
	qpnum_per_tc = i40e_align_floor(vsi->nb_qps / total_tc);
	qpnum_per_tc = RTE_MIN(qpnum_per_tc, I40E_MAX_Q_PER_TC);
	bsf = rte_bsf32(qpnum_per_tc);

	/* Adjust the queue number to actual queues that can be applied */
	if (!(vsi->type == I40E_VSI_MAIN && total_tc == 1))
		vsi->nb_qps = qpnum_per_tc * total_tc;

	/**
	 * Configure TC and queue mapping parameters, for enabled TC,
	 * allocate qpnum_per_tc queues to this traffic. For disabled TC,
	 * default queue will serve it.
	 */
	qp_idx = 0;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (vsi->enabled_tc & (1 << i)) {
			info->tc_mapping[i] = rte_cpu_to_le_16((qp_idx <<
					I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
				(bsf << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT));
			qp_idx += qpnum_per_tc;
		} else
			info->tc_mapping[i] = 0;
	}

	/* Associate queue number with VSI */
	if (vsi->type == I40E_VSI_SRIOV) {
		info->mapping_flags |=
			rte_cpu_to_le_16(I40E_AQ_VSI_QUE_MAP_NONCONTIG);
		for (i = 0; i < vsi->nb_qps; i++)
			info->queue_mapping[i] =
				rte_cpu_to_le_16(vsi->base_queue + i);
	} else {
		info->mapping_flags |=
			rte_cpu_to_le_16(I40E_AQ_VSI_QUE_MAP_CONTIG);
		info->queue_mapping[0] = rte_cpu_to_le_16(vsi->base_queue);
	}
	info->valid_sections |=
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_QUEUE_MAP_VALID);

	return I40E_SUCCESS;
}

static int
i40e_veb_release(struct i40e_veb *veb)
{
	struct i40e_vsi *vsi;
	struct i40e_hw *hw;

	if (veb == NULL)
		return -EINVAL;

	if (!TAILQ_EMPTY(&veb->head)) {
		PMD_DRV_LOG(ERR, "VEB still has VSI attached, can't remove");
		return -EACCES;
	}
	/* associate_vsi field is NULL for floating VEB */
	if (veb->associate_vsi != NULL) {
		vsi = veb->associate_vsi;
		hw = I40E_VSI_TO_HW(vsi);

		vsi->uplink_seid = veb->uplink_seid;
		vsi->veb = NULL;
	} else {
		veb->associate_pf->main_vsi->floating_veb = NULL;
		hw = I40E_VSI_TO_HW(veb->associate_pf->main_vsi);
	}

	i40e_aq_delete_element(hw, veb->seid, NULL);
	rte_free(veb);
	return I40E_SUCCESS;
}

/* Setup a veb */
static struct i40e_veb *
i40e_veb_setup(struct i40e_pf *pf, struct i40e_vsi *vsi)
{
	struct i40e_veb *veb;
	int ret;
	struct i40e_hw *hw;

	if (pf == NULL) {
		PMD_DRV_LOG(ERR,
			    "veb setup failed, associated PF shouldn't null");
		return NULL;
	}
	hw = I40E_PF_TO_HW(pf);

	veb = rte_zmalloc("i40e_veb", sizeof(struct i40e_veb), 0);
	if (!veb) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for veb");
		goto fail;
	}

	veb->associate_vsi = vsi;
	veb->associate_pf = pf;
	TAILQ_INIT(&veb->head);
	veb->uplink_seid = vsi ? vsi->uplink_seid : 0;

	/* create floating veb if vsi is NULL */
	if (vsi != NULL) {
		ret = i40e_aq_add_veb(hw, veb->uplink_seid, vsi->seid,
				      I40E_DEFAULT_TCMAP, false,
				      &veb->seid, false, NULL);
	} else {
		ret = i40e_aq_add_veb(hw, 0, 0, I40E_DEFAULT_TCMAP,
				      true, &veb->seid, false, NULL);
	}

	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Add veb failed, aq_err: %d",
			    hw->aq.asq_last_status);
		goto fail;
	}
	veb->enabled_tc = I40E_DEFAULT_TCMAP;

	/* get statistics index */
	ret = i40e_aq_get_veb_parameters(hw, veb->seid, NULL, NULL,
				&veb->stats_idx, NULL, NULL, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Get veb statistics index failed, aq_err: %d",
			    hw->aq.asq_last_status);
		goto fail;
	}
	/* Get VEB bandwidth, to be implemented */
	/* Now associated vsi binding to the VEB, set uplink to this VEB */
	if (vsi)
		vsi->uplink_seid = veb->seid;

	return veb;
fail:
	rte_free(veb);
	return NULL;
}

int
i40e_vsi_release(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	struct i40e_vsi_list *vsi_list;
	void *temp;
	int ret;
	struct i40e_mac_filter *f;
	uint16_t user_param;

	if (!vsi)
		return I40E_SUCCESS;

	if (!vsi->adapter)
		return -EFAULT;

	user_param = vsi->user_param;

	pf = I40E_VSI_TO_PF(vsi);
	hw = I40E_VSI_TO_HW(vsi);

	/* VSI has child to attach, release child first */
	if (vsi->veb) {
		TAILQ_FOREACH_SAFE(vsi_list, &vsi->veb->head, list, temp) {
			if (i40e_vsi_release(vsi_list->vsi) != I40E_SUCCESS)
				return -1;
		}
		i40e_veb_release(vsi->veb);
	}

	if (vsi->floating_veb) {
		TAILQ_FOREACH_SAFE(vsi_list, &vsi->floating_veb->head, list, temp) {
			if (i40e_vsi_release(vsi_list->vsi) != I40E_SUCCESS)
				return -1;
		}
	}

	/* Remove all macvlan filters of the VSI */
	i40e_vsi_remove_all_macvlan_filter(vsi);
	TAILQ_FOREACH_SAFE(f, &vsi->mac_list, next, temp)
		rte_free(f);

	if (vsi->type != I40E_VSI_MAIN &&
	    ((vsi->type != I40E_VSI_SRIOV) ||
	    !pf->floating_veb_list[user_param])) {
		/* Remove vsi from parent's sibling list */
		if (vsi->parent_vsi == NULL || vsi->parent_vsi->veb == NULL) {
			PMD_DRV_LOG(ERR, "VSI's parent VSI is NULL");
			return I40E_ERR_PARAM;
		}
		TAILQ_REMOVE(&vsi->parent_vsi->veb->head,
				&vsi->sib_vsi_list, list);

		/* Remove all switch element of the VSI */
		ret = i40e_aq_delete_element(hw, vsi->seid, NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(ERR, "Failed to delete element");
	}

	if ((vsi->type == I40E_VSI_SRIOV) &&
	    pf->floating_veb_list[user_param]) {
		/* Remove vsi from parent's sibling list */
		if (vsi->parent_vsi == NULL ||
		    vsi->parent_vsi->floating_veb == NULL) {
			PMD_DRV_LOG(ERR, "VSI's parent VSI is NULL");
			return I40E_ERR_PARAM;
		}
		TAILQ_REMOVE(&vsi->parent_vsi->floating_veb->head,
			     &vsi->sib_vsi_list, list);

		/* Remove all switch element of the VSI */
		ret = i40e_aq_delete_element(hw, vsi->seid, NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(ERR, "Failed to delete element");
	}

	i40e_res_pool_free(&pf->qp_pool, vsi->base_queue);

	if (vsi->type != I40E_VSI_SRIOV)
		i40e_res_pool_free(&pf->msix_pool, vsi->msix_intr);
	rte_free(vsi);

	return I40E_SUCCESS;
}

static int
i40e_update_default_filter_setting(struct i40e_vsi *vsi)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_remove_macvlan_element_data def_filter;
	struct i40e_mac_filter_info filter;
	int ret;

	if (vsi->type != I40E_VSI_MAIN)
		return I40E_ERR_CONFIG;
	memset(&def_filter, 0, sizeof(def_filter));
	(void)rte_memcpy(def_filter.mac_addr, hw->mac.perm_addr,
					ETH_ADDR_LEN);
	def_filter.vlan_tag = 0;
	def_filter.flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH |
				I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
	ret = i40e_aq_remove_macvlan(hw, vsi->seid, &def_filter, 1, NULL);
	if (ret != I40E_SUCCESS) {
		struct i40e_mac_filter *f;
		struct ether_addr *mac;

		PMD_DRV_LOG(DEBUG,
			    "Cannot remove the default macvlan filter");
		/* It needs to add the permanent mac into mac list */
		f = rte_zmalloc("macv_filter", sizeof(*f), 0);
		if (f == NULL) {
			PMD_DRV_LOG(ERR, "failed to allocate memory");
			return I40E_ERR_NO_MEMORY;
		}
		mac = &f->mac_info.mac_addr;
		(void)rte_memcpy(&mac->addr_bytes, hw->mac.perm_addr,
				ETH_ADDR_LEN);
		f->mac_info.filter_type = RTE_MACVLAN_PERFECT_MATCH;
		TAILQ_INSERT_TAIL(&vsi->mac_list, f, next);
		vsi->mac_num++;

		return ret;
	}
	(void)rte_memcpy(&filter.mac_addr,
		(struct ether_addr *)(hw->mac.perm_addr), ETH_ADDR_LEN);
	filter.filter_type = RTE_MACVLAN_PERFECT_MATCH;
	return i40e_vsi_add_mac(vsi, &filter);
}

/*
 * i40e_vsi_get_bw_config - Query VSI BW Information
 * @vsi: the VSI to be queried
 *
 * Returns 0 on success, negative value on failure
 */
static enum i40e_status_code
i40e_vsi_get_bw_config(struct i40e_vsi *vsi)
{
	struct i40e_aqc_query_vsi_bw_config_resp bw_config;
	struct i40e_aqc_query_vsi_ets_sla_config_resp ets_sla_config;
	struct i40e_hw *hw = &vsi->adapter->hw;
	i40e_status ret;
	int i;
	uint32_t bw_max;

	memset(&bw_config, 0, sizeof(bw_config));
	ret = i40e_aq_query_vsi_bw_config(hw, vsi->seid, &bw_config, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "VSI failed to get bandwidth configuration %u",
			    hw->aq.asq_last_status);
		return ret;
	}

	memset(&ets_sla_config, 0, sizeof(ets_sla_config));
	ret = i40e_aq_query_vsi_ets_sla_config(hw, vsi->seid,
					&ets_sla_config, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR,
			"VSI failed to get TC bandwdith configuration %u",
			hw->aq.asq_last_status);
		return ret;
	}

	/* store and print out BW info */
	vsi->bw_info.bw_limit = rte_le_to_cpu_16(bw_config.port_bw_limit);
	vsi->bw_info.bw_max = bw_config.max_bw;
	PMD_DRV_LOG(DEBUG, "VSI bw limit:%u", vsi->bw_info.bw_limit);
	PMD_DRV_LOG(DEBUG, "VSI max_bw:%u", vsi->bw_info.bw_max);
	bw_max = rte_le_to_cpu_16(ets_sla_config.tc_bw_max[0]) |
		    (rte_le_to_cpu_16(ets_sla_config.tc_bw_max[1]) <<
		     I40E_16_BIT_WIDTH);
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		vsi->bw_info.bw_ets_share_credits[i] =
				ets_sla_config.share_credits[i];
		vsi->bw_info.bw_ets_credits[i] =
				rte_le_to_cpu_16(ets_sla_config.credits[i]);
		/* 4 bits per TC, 4th bit is reserved */
		vsi->bw_info.bw_ets_max[i] =
			(uint8_t)((bw_max >> (i * I40E_4_BIT_WIDTH)) &
				  RTE_LEN2MASK(3, uint8_t));
		PMD_DRV_LOG(DEBUG, "\tVSI TC%u:share credits %u", i,
			    vsi->bw_info.bw_ets_share_credits[i]);
		PMD_DRV_LOG(DEBUG, "\tVSI TC%u:credits %u", i,
			    vsi->bw_info.bw_ets_credits[i]);
		PMD_DRV_LOG(DEBUG, "\tVSI TC%u: max credits: %u", i,
			    vsi->bw_info.bw_ets_max[i]);
	}

	return I40E_SUCCESS;
}

/* i40e_enable_pf_lb
 * @pf: pointer to the pf structure
 *
 * allow loopback on pf
 */
static inline void
i40e_enable_pf_lb(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi_context ctxt;
	int ret;

	/* Use the FW API if FW >= v5.0 */
	if (hw->aq.fw_maj_ver < 5) {
		PMD_INIT_LOG(ERR, "FW < v5.0, cannot enable loopback");
		return;
	}

	memset(&ctxt, 0, sizeof(ctxt));
	ctxt.seid = pf->main_vsi_seid;
	ctxt.pf_num = hw->pf_id;
	ret = i40e_aq_get_vsi_params(hw, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(ERR, "cannot get pf vsi config, err %d, aq_err %d",
			    ret, hw->aq.asq_last_status);
		return;
	}
	ctxt.flags = I40E_AQ_VSI_TYPE_PF;
	ctxt.info.valid_sections =
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SWITCH_VALID);
	ctxt.info.switch_id |=
		rte_cpu_to_le_16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);

	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret)
		PMD_DRV_LOG(ERR, "update vsi switch failed, aq_err=%d",
			    hw->aq.asq_last_status);
}

/* Setup a VSI */
struct i40e_vsi *
i40e_vsi_setup(struct i40e_pf *pf,
	       enum i40e_vsi_type type,
	       struct i40e_vsi *uplink_vsi,
	       uint16_t user_param)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi;
	struct i40e_mac_filter_info filter;
	int ret;
	struct i40e_vsi_context ctxt;
	struct ether_addr broadcast =
		{.addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

	if (type != I40E_VSI_MAIN && type != I40E_VSI_SRIOV &&
	    uplink_vsi == NULL) {
		PMD_DRV_LOG(ERR,
			"VSI setup failed, VSI link shouldn't be NULL");
		return NULL;
	}

	if (type == I40E_VSI_MAIN && uplink_vsi != NULL) {
		PMD_DRV_LOG(ERR,
			"VSI setup failed, MAIN VSI uplink VSI should be NULL");
		return NULL;
	}

	/* two situations
	 * 1.type is not MAIN and uplink vsi is not NULL
	 * If uplink vsi didn't setup VEB, create one first under veb field
	 * 2.type is SRIOV and the uplink is NULL
	 * If floating VEB is NULL, create one veb under floating veb field
	 */

	if (type != I40E_VSI_MAIN && uplink_vsi != NULL &&
	    uplink_vsi->veb == NULL) {
		uplink_vsi->veb = i40e_veb_setup(pf, uplink_vsi);

		if (uplink_vsi->veb == NULL) {
			PMD_DRV_LOG(ERR, "VEB setup failed");
			return NULL;
		}
		/* set ALLOWLOOPBACk on pf, when veb is created */
		i40e_enable_pf_lb(pf);
	}

	if (type == I40E_VSI_SRIOV && uplink_vsi == NULL &&
	    pf->main_vsi->floating_veb == NULL) {
		pf->main_vsi->floating_veb = i40e_veb_setup(pf, uplink_vsi);

		if (pf->main_vsi->floating_veb == NULL) {
			PMD_DRV_LOG(ERR, "VEB setup failed");
			return NULL;
		}
	}

	vsi = rte_zmalloc("i40e_vsi", sizeof(struct i40e_vsi), 0);
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for vsi");
		return NULL;
	}
	TAILQ_INIT(&vsi->mac_list);
	vsi->type = type;
	vsi->adapter = I40E_PF_TO_ADAPTER(pf);
	vsi->max_macaddrs = I40E_NUM_MACADDR_MAX;
	vsi->parent_vsi = uplink_vsi ? uplink_vsi : pf->main_vsi;
	vsi->user_param = user_param;
	vsi->vlan_anti_spoof_on = 0;
	vsi->vlan_filter_on = 0;
	/* Allocate queues */
	switch (vsi->type) {
	case I40E_VSI_MAIN  :
		vsi->nb_qps = pf->lan_nb_qps;
		break;
	case I40E_VSI_SRIOV :
		vsi->nb_qps = pf->vf_nb_qps;
		break;
	case I40E_VSI_VMDQ2:
		vsi->nb_qps = pf->vmdq_nb_qps;
		break;
	case I40E_VSI_FDIR:
		vsi->nb_qps = pf->fdir_nb_qps;
		break;
	default:
		goto fail_mem;
	}
	/*
	 * The filter status descriptor is reported in rx queue 0,
	 * while the tx queue for fdir filter programming has no
	 * such constraints, can be non-zero queues.
	 * To simplify it, choose FDIR vsi use queue 0 pair.
	 * To make sure it will use queue 0 pair, queue allocation
	 * need be done before this function is called
	 */
	if (type != I40E_VSI_FDIR) {
		ret = i40e_res_pool_alloc(&pf->qp_pool, vsi->nb_qps);
			if (ret < 0) {
				PMD_DRV_LOG(ERR, "VSI %d allocate queue failed %d",
						vsi->seid, ret);
				goto fail_mem;
			}
			vsi->base_queue = ret;
	} else
		vsi->base_queue = I40E_FDIR_QUEUE_ID;

	/* VF has MSIX interrupt in VF range, don't allocate here */
	if (type == I40E_VSI_MAIN) {
		ret = i40e_res_pool_alloc(&pf->msix_pool,
					  RTE_MIN(vsi->nb_qps,
						  RTE_MAX_RXTX_INTR_VEC_ID));
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "VSI MAIN %d get heap failed %d",
				    vsi->seid, ret);
			goto fail_queue_alloc;
		}
		vsi->msix_intr = ret;
		vsi->nb_msix = RTE_MIN(vsi->nb_qps, RTE_MAX_RXTX_INTR_VEC_ID);
	} else if (type != I40E_VSI_SRIOV) {
		ret = i40e_res_pool_alloc(&pf->msix_pool, 1);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "VSI %d get heap failed %d", vsi->seid, ret);
			goto fail_queue_alloc;
		}
		vsi->msix_intr = ret;
		vsi->nb_msix = 1;
	} else {
		vsi->msix_intr = 0;
		vsi->nb_msix = 0;
	}

	/* Add VSI */
	if (type == I40E_VSI_MAIN) {
		/* For main VSI, no need to add since it's default one */
		vsi->uplink_seid = pf->mac_seid;
		vsi->seid = pf->main_vsi_seid;
		/* Bind queues with specific MSIX interrupt */
		/**
		 * Needs 2 interrupt at least, one for misc cause which will
		 * enabled from OS side, Another for queues binding the
		 * interrupt from device side only.
		 */

		/* Get default VSI parameters from hardware */
		memset(&ctxt, 0, sizeof(ctxt));
		ctxt.seid = vsi->seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.vf_num = 0;
		ret = i40e_aq_get_vsi_params(hw, &ctxt, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to get VSI params");
			goto fail_msix_alloc;
		}
		(void)rte_memcpy(&vsi->info, &ctxt.info,
			sizeof(struct i40e_aqc_vsi_properties_data));
		vsi->vsi_id = ctxt.vsi_number;
		vsi->info.valid_sections = 0;

		/* Configure tc, enabled TC0 only */
		if (i40e_vsi_update_tc_bandwidth(vsi, I40E_DEFAULT_TCMAP) !=
			I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to update TC bandwidth");
			goto fail_msix_alloc;
		}

		/* TC, queue mapping */
		memset(&ctxt, 0, sizeof(ctxt));
		vsi->info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
		vsi->info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL |
					I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH;
		(void)rte_memcpy(&ctxt.info, &vsi->info,
			sizeof(struct i40e_aqc_vsi_properties_data));
		ret = i40e_vsi_config_tc_queue_mapping(vsi, &ctxt.info,
						I40E_DEFAULT_TCMAP);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR,
				"Failed to configure TC queue mapping");
			goto fail_msix_alloc;
		}
		ctxt.seid = vsi->seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.vf_num = 0;

		/* Update VSI parameters */
		ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to update VSI params");
			goto fail_msix_alloc;
		}

		(void)rte_memcpy(&vsi->info.tc_mapping, &ctxt.info.tc_mapping,
						sizeof(vsi->info.tc_mapping));
		(void)rte_memcpy(&vsi->info.queue_mapping,
				&ctxt.info.queue_mapping,
			sizeof(vsi->info.queue_mapping));
		vsi->info.mapping_flags = ctxt.info.mapping_flags;
		vsi->info.valid_sections = 0;

		(void)rte_memcpy(pf->dev_addr.addr_bytes, hw->mac.perm_addr,
				ETH_ADDR_LEN);

		/**
		 * Updating default filter settings are necessary to prevent
		 * reception of tagged packets.
		 * Some old firmware configurations load a default macvlan
		 * filter which accepts both tagged and untagged packets.
		 * The updating is to use a normal filter instead if needed.
		 * For NVM 4.2.2 or after, the updating is not needed anymore.
		 * The firmware with correct configurations load the default
		 * macvlan filter which is expected and cannot be removed.
		 */
		i40e_update_default_filter_setting(vsi);
		i40e_config_qinq(hw, vsi);
	} else if (type == I40E_VSI_SRIOV) {
		memset(&ctxt, 0, sizeof(ctxt));
		/**
		 * For other VSI, the uplink_seid equals to uplink VSI's
		 * uplink_seid since they share same VEB
		 */
		if (uplink_vsi == NULL)
			vsi->uplink_seid = pf->main_vsi->floating_veb->seid;
		else
			vsi->uplink_seid = uplink_vsi->uplink_seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = hw->func_caps.vf_base_id + user_param;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = 0x1;
		ctxt.flags = I40E_AQ_VSI_TYPE_VF;

		/* Use the VEB configuration if FW >= v5.0 */
		if (hw->aq.fw_maj_ver >= 5) {
			/* Configure switch ID */
			ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SWITCH_VALID);
			ctxt.info.switch_id =
			rte_cpu_to_le_16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);
		}

		/* Configure port/vlan */
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_ALL;
		ret = i40e_vsi_config_tc_queue_mapping(vsi, &ctxt.info,
						hw->func_caps.enabled_tcmap);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR,
				"Failed to configure TC queue mapping");
			goto fail_msix_alloc;
		}

		ctxt.info.up_enable_bits = hw->func_caps.enabled_tcmap;
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SCHED_VALID);
		/**
		 * Since VSI is not created yet, only configure parameter,
		 * will add vsi below.
		 */

		i40e_config_qinq(hw, vsi);
	} else if (type == I40E_VSI_VMDQ2) {
		memset(&ctxt, 0, sizeof(ctxt));
		/*
		 * For other VSI, the uplink_seid equals to uplink VSI's
		 * uplink_seid since they share same VEB
		 */
		vsi->uplink_seid = uplink_vsi->uplink_seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = 0;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = 0x1;
		ctxt.flags = I40E_AQ_VSI_TYPE_VMDQ2;

		ctxt.info.valid_sections |=
				rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SWITCH_VALID);
		/* user_param carries flag to enable loop back */
		if (user_param) {
			ctxt.info.switch_id =
			rte_cpu_to_le_16(I40E_AQ_VSI_SW_ID_FLAG_LOCAL_LB);
			ctxt.info.switch_id |=
			rte_cpu_to_le_16(I40E_AQ_VSI_SW_ID_FLAG_ALLOW_LB);
		}

		/* Configure port/vlan */
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_MODE_ALL;
		ret = i40e_vsi_config_tc_queue_mapping(vsi, &ctxt.info,
						I40E_DEFAULT_TCMAP);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR,
				"Failed to configure TC queue mapping");
			goto fail_msix_alloc;
		}
		ctxt.info.up_enable_bits = I40E_DEFAULT_TCMAP;
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SCHED_VALID);
	} else if (type == I40E_VSI_FDIR) {
		memset(&ctxt, 0, sizeof(ctxt));
		vsi->uplink_seid = uplink_vsi->uplink_seid;
		ctxt.pf_num = hw->pf_id;
		ctxt.vf_num = 0;
		ctxt.uplink_seid = vsi->uplink_seid;
		ctxt.connection_type = 0x1;     /* regular data port */
		ctxt.flags = I40E_AQ_VSI_TYPE_PF;
		ret = i40e_vsi_config_tc_queue_mapping(vsi, &ctxt.info,
						I40E_DEFAULT_TCMAP);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR,
				"Failed to configure TC queue mapping.");
			goto fail_msix_alloc;
		}
		ctxt.info.up_enable_bits = I40E_DEFAULT_TCMAP;
		ctxt.info.valid_sections |=
			rte_cpu_to_le_16(I40E_AQ_VSI_PROP_SCHED_VALID);
	} else {
		PMD_DRV_LOG(ERR, "VSI: Not support other type VSI yet");
		goto fail_msix_alloc;
	}

	if (vsi->type != I40E_VSI_MAIN) {
		ret = i40e_aq_add_vsi(hw, &ctxt, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "add vsi failed, aq_err=%d",
				    hw->aq.asq_last_status);
			goto fail_msix_alloc;
		}
		memcpy(&vsi->info, &ctxt.info, sizeof(ctxt.info));
		vsi->info.valid_sections = 0;
		vsi->seid = ctxt.seid;
		vsi->vsi_id = ctxt.vsi_number;
		vsi->sib_vsi_list.vsi = vsi;
		if (vsi->type == I40E_VSI_SRIOV && uplink_vsi == NULL) {
			TAILQ_INSERT_TAIL(&pf->main_vsi->floating_veb->head,
					  &vsi->sib_vsi_list, list);
		} else {
			TAILQ_INSERT_TAIL(&uplink_vsi->veb->head,
					  &vsi->sib_vsi_list, list);
		}
	}

	/* MAC/VLAN configuration */
	(void)rte_memcpy(&filter.mac_addr, &broadcast, ETHER_ADDR_LEN);
	filter.filter_type = RTE_MACVLAN_PERFECT_MATCH;

	ret = i40e_vsi_add_mac(vsi, &filter);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MACVLAN filter");
		goto fail_msix_alloc;
	}

	/* Get VSI BW information */
	i40e_vsi_get_bw_config(vsi);
	return vsi;
fail_msix_alloc:
	i40e_res_pool_free(&pf->msix_pool,vsi->msix_intr);
fail_queue_alloc:
	i40e_res_pool_free(&pf->qp_pool,vsi->base_queue);
fail_mem:
	rte_free(vsi);
	return NULL;
}

/* Configure vlan filter on or off */
int
i40e_vsi_config_vlan_filter(struct i40e_vsi *vsi, bool on)
{
	int i, num;
	struct i40e_mac_filter *f;
	void *temp;
	struct i40e_mac_filter_info *mac_filter;
	enum rte_mac_filter_type desired_filter;
	int ret = I40E_SUCCESS;

	if (on) {
		/* Filter to match MAC and VLAN */
		desired_filter = RTE_MACVLAN_PERFECT_MATCH;
	} else {
		/* Filter to match only MAC */
		desired_filter = RTE_MAC_PERFECT_MATCH;
	}

	num = vsi->mac_num;

	mac_filter = rte_zmalloc("mac_filter_info_data",
				 num * sizeof(*mac_filter), 0);
	if (mac_filter == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	i = 0;

	/* Remove all existing mac */
	TAILQ_FOREACH_SAFE(f, &vsi->mac_list, next, temp) {
		mac_filter[i] = f->mac_info;
		ret = i40e_vsi_delete_mac(vsi, &f->mac_info.mac_addr);
		if (ret) {
			PMD_DRV_LOG(ERR, "Update VSI failed to %s vlan filter",
				    on ? "enable" : "disable");
			goto DONE;
		}
		i++;
	}

	/* Override with new filter */
	for (i = 0; i < num; i++) {
		mac_filter[i].filter_type = desired_filter;
		ret = i40e_vsi_add_mac(vsi, &mac_filter[i]);
		if (ret) {
			PMD_DRV_LOG(ERR, "Update VSI failed to %s vlan filter",
				    on ? "enable" : "disable");
			goto DONE;
		}
	}

DONE:
	rte_free(mac_filter);
	return ret;
}

/* Configure vlan stripping on or off */
int
i40e_vsi_config_vlan_stripping(struct i40e_vsi *vsi, bool on)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_vsi_context ctxt;
	uint8_t vlan_flags;
	int ret = I40E_SUCCESS;

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID)) {
		if (on) {
			if ((vsi->info.port_vlan_flags &
				I40E_AQ_VSI_PVLAN_EMOD_MASK) == 0)
				return 0; /* already on */
		} else {
			if ((vsi->info.port_vlan_flags &
				I40E_AQ_VSI_PVLAN_EMOD_MASK) ==
				I40E_AQ_VSI_PVLAN_EMOD_MASK)
				return 0; /* already off */
		}
	}

	if (on)
		vlan_flags = I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH;
	else
		vlan_flags = I40E_AQ_VSI_PVLAN_EMOD_NOTHING;
	vsi->info.valid_sections =
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_VLAN_VALID);
	vsi->info.port_vlan_flags &= ~(I40E_AQ_VSI_PVLAN_EMOD_MASK);
	vsi->info.port_vlan_flags |= vlan_flags;
	ctxt.seid = vsi->seid;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret)
		PMD_DRV_LOG(INFO, "Update VSI failed to %s vlan stripping",
			    on ? "enable" : "disable");

	return ret;
}

static int
i40e_dev_init_vlan(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	int ret;
	int mask = 0;

	/* Apply vlan offload setting */
	mask = ETH_VLAN_STRIP_MASK | ETH_VLAN_FILTER_MASK;
	i40e_vlan_offload_set(dev, mask);

	/* Apply double-vlan setting, not implemented yet */

	/* Apply pvid setting */
	ret = i40e_vlan_pvid_set(dev, data->dev_conf.txmode.pvid,
				data->dev_conf.txmode.hw_vlan_insert_pvid);
	if (ret)
		PMD_DRV_LOG(INFO, "Failed to update VSI params");

	return ret;
}

static int
i40e_vsi_config_double_vlan(struct i40e_vsi *vsi, int on)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);

	return i40e_aq_set_port_parameters(hw, vsi->seid, 0, 1, on, NULL);
}

static int
i40e_update_flow_control(struct i40e_hw *hw)
{
#define I40E_LINK_PAUSE_RXTX (I40E_AQ_LINK_PAUSE_RX | I40E_AQ_LINK_PAUSE_TX)
	struct i40e_link_status link_status;
	uint32_t rxfc = 0, txfc = 0, reg;
	uint8_t an_info;
	int ret;

	memset(&link_status, 0, sizeof(link_status));
	ret = i40e_aq_get_link_info(hw, FALSE, &link_status, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to get link status information");
		goto write_reg; /* Disable flow control */
	}

	an_info = hw->phy.link_info.an_info;
	if (!(an_info & I40E_AQ_AN_COMPLETED)) {
		PMD_DRV_LOG(INFO, "Link auto negotiation not completed");
		ret = I40E_ERR_NOT_READY;
		goto write_reg; /* Disable flow control */
	}
	/**
	 * If link auto negotiation is enabled, flow control needs to
	 * be configured according to it
	 */
	switch (an_info & I40E_LINK_PAUSE_RXTX) {
	case I40E_LINK_PAUSE_RXTX:
		rxfc = 1;
		txfc = 1;
		hw->fc.current_mode = I40E_FC_FULL;
		break;
	case I40E_AQ_LINK_PAUSE_RX:
		rxfc = 1;
		hw->fc.current_mode = I40E_FC_RX_PAUSE;
		break;
	case I40E_AQ_LINK_PAUSE_TX:
		txfc = 1;
		hw->fc.current_mode = I40E_FC_TX_PAUSE;
		break;
	default:
		hw->fc.current_mode = I40E_FC_NONE;
		break;
	}

write_reg:
	I40E_WRITE_REG(hw, I40E_PRTDCB_FCCFG,
		txfc << I40E_PRTDCB_FCCFG_TFCE_SHIFT);
	reg = I40E_READ_REG(hw, I40E_PRTDCB_MFLCN);
	reg &= ~I40E_PRTDCB_MFLCN_RFCE_MASK;
	reg |= rxfc << I40E_PRTDCB_MFLCN_RFCE_SHIFT;
	I40E_WRITE_REG(hw, I40E_PRTDCB_MFLCN, reg);

	return ret;
}

/* PF setup */
static int
i40e_pf_setup(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_filter_control_settings settings;
	struct i40e_vsi *vsi;
	int ret;

	/* Clear all stats counters */
	pf->offset_loaded = FALSE;
	memset(&pf->stats, 0, sizeof(struct i40e_hw_port_stats));
	memset(&pf->stats_offset, 0, sizeof(struct i40e_hw_port_stats));
	pf->internal_rx_bytes = 0;
	pf->internal_tx_bytes = 0;
	pf->internal_rx_bytes_offset = 0;
	pf->internal_tx_bytes_offset = 0;

	ret = i40e_pf_get_switch_config(pf);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Could not get switch config, err %d", ret);
		return ret;
	}
	if (pf->flags & I40E_FLAG_FDIR) {
		/* make queue allocated first, let FDIR use queue pair 0*/
		ret = i40e_res_pool_alloc(&pf->qp_pool, I40E_DEFAULT_QP_NUM_FDIR);
		if (ret != I40E_FDIR_QUEUE_ID) {
			PMD_DRV_LOG(ERR,
				"queue allocation fails for FDIR: ret =%d",
				ret);
			pf->flags &= ~I40E_FLAG_FDIR;
		}
	}
	/*  main VSI setup */
	vsi = i40e_vsi_setup(pf, I40E_VSI_MAIN, NULL, 0);
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Setup of main vsi failed");
		return I40E_ERR_NOT_READY;
	}
	pf->main_vsi = vsi;

	/* Configure filter control */
	memset(&settings, 0, sizeof(settings));
	if (hw->func_caps.rss_table_size == ETH_RSS_RETA_SIZE_128)
		settings.hash_lut_size = I40E_HASH_LUT_SIZE_128;
	else if (hw->func_caps.rss_table_size == ETH_RSS_RETA_SIZE_512)
		settings.hash_lut_size = I40E_HASH_LUT_SIZE_512;
	else {
		PMD_DRV_LOG(ERR, "Hash lookup table size (%u) not supported",
			hw->func_caps.rss_table_size);
		return I40E_ERR_PARAM;
	}
	PMD_DRV_LOG(INFO, "Hardware capability of hash lookup table size: %u",
		hw->func_caps.rss_table_size);
	pf->hash_lut_size = hw->func_caps.rss_table_size;

	/* Enable ethtype and macvlan filters */
	settings.enable_ethtype = TRUE;
	settings.enable_macvlan = TRUE;
	ret = i40e_set_filter_control(hw, &settings);
	if (ret)
		PMD_INIT_LOG(WARNING, "setup_pf_filter_control failed: %d",
								ret);

	/* Update flow control according to the auto negotiation */
	i40e_update_flow_control(hw);

	return I40E_SUCCESS;
}

int
i40e_switch_tx_queue(struct i40e_hw *hw, uint16_t q_idx, bool on)
{
	uint32_t reg;
	uint16_t j;

	/**
	 * Set or clear TX Queue Disable flags,
	 * which is required by hardware.
	 */
	i40e_pre_tx_queue_cfg(hw, q_idx, on);
	rte_delay_us(I40E_PRE_TX_Q_CFG_WAIT_US);

	/* Wait until the request is finished */
	for (j = 0; j < I40E_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(I40E_CHK_Q_ENA_INTERVAL_US);
		reg = I40E_READ_REG(hw, I40E_QTX_ENA(q_idx));
		if (!(((reg >> I40E_QTX_ENA_QENA_REQ_SHIFT) & 0x1) ^
			((reg >> I40E_QTX_ENA_QENA_STAT_SHIFT)
							& 0x1))) {
			break;
		}
	}
	if (on) {
		if (reg & I40E_QTX_ENA_QENA_STAT_MASK)
			return I40E_SUCCESS; /* already on, skip next steps */

		I40E_WRITE_REG(hw, I40E_QTX_HEAD(q_idx), 0);
		reg |= I40E_QTX_ENA_QENA_REQ_MASK;
	} else {
		if (!(reg & I40E_QTX_ENA_QENA_STAT_MASK))
			return I40E_SUCCESS; /* already off, skip next steps */
		reg &= ~I40E_QTX_ENA_QENA_REQ_MASK;
	}
	/* Write the register */
	I40E_WRITE_REG(hw, I40E_QTX_ENA(q_idx), reg);
	/* Check the result */
	for (j = 0; j < I40E_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(I40E_CHK_Q_ENA_INTERVAL_US);
		reg = I40E_READ_REG(hw, I40E_QTX_ENA(q_idx));
		if (on) {
			if ((reg & I40E_QTX_ENA_QENA_REQ_MASK) &&
				(reg & I40E_QTX_ENA_QENA_STAT_MASK))
				break;
		} else {
			if (!(reg & I40E_QTX_ENA_QENA_REQ_MASK) &&
				!(reg & I40E_QTX_ENA_QENA_STAT_MASK))
				break;
		}
	}
	/* Check if it is timeout */
	if (j >= I40E_CHK_Q_ENA_COUNT) {
		PMD_DRV_LOG(ERR, "Failed to %s tx queue[%u]",
			    (on ? "enable" : "disable"), q_idx);
		return I40E_ERR_TIMEOUT;
	}

	return I40E_SUCCESS;
}

/* Swith on or off the tx queues */
static int
i40e_dev_switch_tx_queues(struct i40e_pf *pf, bool on)
{
	struct rte_eth_dev_data *dev_data = pf->dev_data;
	struct i40e_tx_queue *txq;
	struct rte_eth_dev *dev = pf->adapter->eth_dev;
	uint16_t i;
	int ret;

	for (i = 0; i < dev_data->nb_tx_queues; i++) {
		txq = dev_data->tx_queues[i];
		/* Don't operate the queue if not configured or
		 * if starting only per queue */
		if (!txq || !txq->q_set || (on && txq->tx_deferred_start))
			continue;
		if (on)
			ret = i40e_dev_tx_queue_start(dev, i);
		else
			ret = i40e_dev_tx_queue_stop(dev, i);
		if ( ret != I40E_SUCCESS)
			return ret;
	}

	return I40E_SUCCESS;
}

int
i40e_switch_rx_queue(struct i40e_hw *hw, uint16_t q_idx, bool on)
{
	uint32_t reg;
	uint16_t j;

	/* Wait until the request is finished */
	for (j = 0; j < I40E_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(I40E_CHK_Q_ENA_INTERVAL_US);
		reg = I40E_READ_REG(hw, I40E_QRX_ENA(q_idx));
		if (!((reg >> I40E_QRX_ENA_QENA_REQ_SHIFT) & 0x1) ^
			((reg >> I40E_QRX_ENA_QENA_STAT_SHIFT) & 0x1))
			break;
	}

	if (on) {
		if (reg & I40E_QRX_ENA_QENA_STAT_MASK)
			return I40E_SUCCESS; /* Already on, skip next steps */
		reg |= I40E_QRX_ENA_QENA_REQ_MASK;
	} else {
		if (!(reg & I40E_QRX_ENA_QENA_STAT_MASK))
			return I40E_SUCCESS; /* Already off, skip next steps */
		reg &= ~I40E_QRX_ENA_QENA_REQ_MASK;
	}

	/* Write the register */
	I40E_WRITE_REG(hw, I40E_QRX_ENA(q_idx), reg);
	/* Check the result */
	for (j = 0; j < I40E_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(I40E_CHK_Q_ENA_INTERVAL_US);
		reg = I40E_READ_REG(hw, I40E_QRX_ENA(q_idx));
		if (on) {
			if ((reg & I40E_QRX_ENA_QENA_REQ_MASK) &&
				(reg & I40E_QRX_ENA_QENA_STAT_MASK))
				break;
		} else {
			if (!(reg & I40E_QRX_ENA_QENA_REQ_MASK) &&
				!(reg & I40E_QRX_ENA_QENA_STAT_MASK))
				break;
		}
	}

	/* Check if it is timeout */
	if (j >= I40E_CHK_Q_ENA_COUNT) {
		PMD_DRV_LOG(ERR, "Failed to %s rx queue[%u]",
			    (on ? "enable" : "disable"), q_idx);
		return I40E_ERR_TIMEOUT;
	}

	return I40E_SUCCESS;
}
/* Switch on or off the rx queues */
static int
i40e_dev_switch_rx_queues(struct i40e_pf *pf, bool on)
{
	struct rte_eth_dev_data *dev_data = pf->dev_data;
	struct i40e_rx_queue *rxq;
	struct rte_eth_dev *dev = pf->adapter->eth_dev;
	uint16_t i;
	int ret;

	for (i = 0; i < dev_data->nb_rx_queues; i++) {
		rxq = dev_data->rx_queues[i];
		/* Don't operate the queue if not configured or
		 * if starting only per queue */
		if (!rxq || !rxq->q_set || (on && rxq->rx_deferred_start))
			continue;
		if (on)
			ret = i40e_dev_rx_queue_start(dev, i);
		else
			ret = i40e_dev_rx_queue_stop(dev, i);
		if (ret != I40E_SUCCESS)
			return ret;
	}

	return I40E_SUCCESS;
}

/* Switch on or off all the rx/tx queues */
int
i40e_dev_switch_queues(struct i40e_pf *pf, bool on)
{
	int ret;

	if (on) {
		/* enable rx queues before enabling tx queues */
		ret = i40e_dev_switch_rx_queues(pf, on);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to switch rx queues");
			return ret;
		}
		ret = i40e_dev_switch_tx_queues(pf, on);
	} else {
		/* Stop tx queues before stopping rx queues */
		ret = i40e_dev_switch_tx_queues(pf, on);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to switch tx queues");
			return ret;
		}
		ret = i40e_dev_switch_rx_queues(pf, on);
	}

	return ret;
}

/* Initialize VSI for TX */
static int
i40e_dev_tx_init(struct i40e_pf *pf)
{
	struct rte_eth_dev_data *data = pf->dev_data;
	uint16_t i;
	uint32_t ret = I40E_SUCCESS;
	struct i40e_tx_queue *txq;

	for (i = 0; i < data->nb_tx_queues; i++) {
		txq = data->tx_queues[i];
		if (!txq || !txq->q_set)
			continue;
		ret = i40e_tx_queue_init(txq);
		if (ret != I40E_SUCCESS)
			break;
	}
	if (ret == I40E_SUCCESS)
		i40e_set_tx_function(container_of(pf, struct i40e_adapter, pf)
				     ->eth_dev);

	return ret;
}

/* Initialize VSI for RX */
static int
i40e_dev_rx_init(struct i40e_pf *pf)
{
	struct rte_eth_dev_data *data = pf->dev_data;
	int ret = I40E_SUCCESS;
	uint16_t i;
	struct i40e_rx_queue *rxq;

	i40e_pf_config_mq_rx(pf);
	for (i = 0; i < data->nb_rx_queues; i++) {
		rxq = data->rx_queues[i];
		if (!rxq || !rxq->q_set)
			continue;

		ret = i40e_rx_queue_init(rxq);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR,
				"Failed to do RX queue initialization");
			break;
		}
	}
	if (ret == I40E_SUCCESS)
		i40e_set_rx_function(container_of(pf, struct i40e_adapter, pf)
				     ->eth_dev);

	return ret;
}

static int
i40e_dev_rxtx_init(struct i40e_pf *pf)
{
	int err;

	err = i40e_dev_tx_init(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do TX initialization");
		return err;
	}
	err = i40e_dev_rx_init(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do RX initialization");
		return err;
	}

	return err;
}

static int
i40e_vmdq_setup(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int i, err, conf_vsis, j, loop;
	struct i40e_vsi *vsi;
	struct i40e_vmdq_info *vmdq_info;
	struct rte_eth_vmdq_rx_conf *vmdq_conf;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);

	/*
	 * Disable interrupt to avoid message from VF. Furthermore, it will
	 * avoid race condition in VSI creation/destroy.
	 */
	i40e_pf_disable_irq0(hw);

	if ((pf->flags & I40E_FLAG_VMDQ) == 0) {
		PMD_INIT_LOG(ERR, "FW doesn't support VMDQ");
		return -ENOTSUP;
	}

	conf_vsis = conf->rx_adv_conf.vmdq_rx_conf.nb_queue_pools;
	if (conf_vsis > pf->max_nb_vmdq_vsi) {
		PMD_INIT_LOG(ERR, "VMDQ config: %u, max support:%u",
			conf->rx_adv_conf.vmdq_rx_conf.nb_queue_pools,
			pf->max_nb_vmdq_vsi);
		return -ENOTSUP;
	}

	if (pf->vmdq != NULL) {
		PMD_INIT_LOG(INFO, "VMDQ already configured");
		return 0;
	}

	pf->vmdq = rte_zmalloc("vmdq_info_struct",
				sizeof(*vmdq_info) * conf_vsis, 0);

	if (pf->vmdq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory");
		return -ENOMEM;
	}

	vmdq_conf = &conf->rx_adv_conf.vmdq_rx_conf;

	/* Create VMDQ VSI */
	for (i = 0; i < conf_vsis; i++) {
		vsi = i40e_vsi_setup(pf, I40E_VSI_VMDQ2, pf->main_vsi,
				vmdq_conf->enable_loop_back);
		if (vsi == NULL) {
			PMD_INIT_LOG(ERR, "Failed to create VMDQ VSI");
			err = -1;
			goto err_vsi_setup;
		}
		vmdq_info = &pf->vmdq[i];
		vmdq_info->pf = pf;
		vmdq_info->vsi = vsi;
	}
	pf->nb_cfg_vmdq_vsi = conf_vsis;

	/* Configure Vlan */
	loop = sizeof(vmdq_conf->pool_map[0].pools) * CHAR_BIT;
	for (i = 0; i < vmdq_conf->nb_pool_maps; i++) {
		for (j = 0; j < loop && j < pf->nb_cfg_vmdq_vsi; j++) {
			if (vmdq_conf->pool_map[i].pools & (1UL << j)) {
				PMD_INIT_LOG(INFO, "Add vlan %u to vmdq pool %u",
					vmdq_conf->pool_map[i].vlan_id, j);

				err = i40e_vsi_add_vlan(pf->vmdq[j].vsi,
						vmdq_conf->pool_map[i].vlan_id);
				if (err) {
					PMD_INIT_LOG(ERR, "Failed to add vlan");
					err = -1;
					goto err_vsi_setup;
				}
			}
		}
	}

	i40e_pf_enable_irq0(hw);

	return 0;

err_vsi_setup:
	for (i = 0; i < conf_vsis; i++)
		if (pf->vmdq[i].vsi == NULL)
			break;
		else
			i40e_vsi_release(pf->vmdq[i].vsi);

	rte_free(pf->vmdq);
	pf->vmdq = NULL;
	i40e_pf_enable_irq0(hw);
	return err;
}

static void
i40e_stat_update_32(struct i40e_hw *hw,
		   uint32_t reg,
		   bool offset_loaded,
		   uint64_t *offset,
		   uint64_t *stat)
{
	uint64_t new_data;

	new_data = (uint64_t)I40E_READ_REG(hw, reg);
	if (!offset_loaded)
		*offset = new_data;

	if (new_data >= *offset)
		*stat = (uint64_t)(new_data - *offset);
	else
		*stat = (uint64_t)((new_data +
			((uint64_t)1 << I40E_32_BIT_WIDTH)) - *offset);
}

static void
i40e_stat_update_48(struct i40e_hw *hw,
		   uint32_t hireg,
		   uint32_t loreg,
		   bool offset_loaded,
		   uint64_t *offset,
		   uint64_t *stat)
{
	uint64_t new_data;

	new_data = (uint64_t)I40E_READ_REG(hw, loreg);
	new_data |= ((uint64_t)(I40E_READ_REG(hw, hireg) &
			I40E_16_BIT_MASK)) << I40E_32_BIT_WIDTH;

	if (!offset_loaded)
		*offset = new_data;

	if (new_data >= *offset)
		*stat = new_data - *offset;
	else
		*stat = (uint64_t)((new_data +
			((uint64_t)1 << I40E_48_BIT_WIDTH)) - *offset);

	*stat &= I40E_48_BIT_MASK;
}

/* Disable IRQ0 */
void
i40e_pf_disable_irq0(struct i40e_hw *hw)
{
	/* Disable all interrupt types */
	I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTL0, 0);
	I40E_WRITE_FLUSH(hw);
}

/* Enable IRQ0 */
void
i40e_pf_enable_irq0(struct i40e_hw *hw)
{
	I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTL0,
		I40E_PFINT_DYN_CTL0_INTENA_MASK |
		I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
		I40E_PFINT_DYN_CTL0_ITR_INDX_MASK);
	I40E_WRITE_FLUSH(hw);
}

static void
i40e_pf_config_irq0(struct i40e_hw *hw, bool no_queue)
{
	/* read pending request and disable first */
	i40e_pf_disable_irq0(hw);
	I40E_WRITE_REG(hw, I40E_PFINT_ICR0_ENA, I40E_PFINT_ICR0_ENA_MASK);
	I40E_WRITE_REG(hw, I40E_PFINT_STAT_CTL0,
		I40E_PFINT_STAT_CTL0_OTHER_ITR_INDX_MASK);

	if (no_queue)
		/* Link no queues with irq0 */
		I40E_WRITE_REG(hw, I40E_PFINT_LNKLST0,
			       I40E_PFINT_LNKLST0_FIRSTQ_INDX_MASK);
}

static void
i40e_dev_handle_vfr_event(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int i;
	uint16_t abs_vf_id;
	uint32_t index, offset, val;

	if (!pf->vfs)
		return;
	/**
	 * Try to find which VF trigger a reset, use absolute VF id to access
	 * since the reg is global register.
	 */
	for (i = 0; i < pf->vf_num; i++) {
		abs_vf_id = hw->func_caps.vf_base_id + i;
		index = abs_vf_id / I40E_UINT32_BIT_SIZE;
		offset = abs_vf_id % I40E_UINT32_BIT_SIZE;
		val = I40E_READ_REG(hw, I40E_GLGEN_VFLRSTAT(index));
		/* VFR event occurred */
		if (val & (0x1 << offset)) {
			int ret;

			/* Clear the event first */
			I40E_WRITE_REG(hw, I40E_GLGEN_VFLRSTAT(index),
							(0x1 << offset));
			PMD_DRV_LOG(INFO, "VF %u reset occurred", abs_vf_id);
			/**
			 * Only notify a VF reset event occurred,
			 * don't trigger another SW reset
			 */
			ret = i40e_pf_host_vf_reset(&pf->vfs[i], 0);
			if (ret != I40E_SUCCESS)
				PMD_DRV_LOG(ERR, "Failed to do VF reset");
		}
	}
}

static void
i40e_notify_all_vfs_link_status(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int i;

	for (i = 0; i < pf->vf_num; i++)
		i40e_notify_vf_link_status(dev, &pf->vfs[i]);
}

static void
i40e_dev_handle_aq_msg(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_arq_event_info info;
	uint16_t pending, opcode;
	int ret;

	info.buf_len = I40E_AQ_BUF_SZ;
	info.msg_buf = rte_zmalloc("msg_buffer", info.buf_len, 0);
	if (!info.msg_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate mem");
		return;
	}

	pending = 1;
	while (pending) {
		ret = i40e_clean_arq_element(hw, &info, &pending);

		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(INFO,
				"Failed to read msg from AdminQ, aq_err: %u",
				hw->aq.asq_last_status);
			break;
		}
		opcode = rte_le_to_cpu_16(info.desc.opcode);

		switch (opcode) {
		case i40e_aqc_opc_send_msg_to_pf:
			/* Refer to i40e_aq_send_msg_to_pf() for argument layout*/
			i40e_pf_host_handle_vf_msg(dev,
					rte_le_to_cpu_16(info.desc.retval),
					rte_le_to_cpu_32(info.desc.cookie_high),
					rte_le_to_cpu_32(info.desc.cookie_low),
					info.msg_buf,
					info.msg_len);
			break;
		case i40e_aqc_opc_get_link_status:
			ret = i40e_dev_link_update(dev, 0);
			if (!ret)
				_rte_eth_dev_callback_process(dev,
					RTE_ETH_EVENT_INTR_LSC, NULL);
			break;
		default:
			PMD_DRV_LOG(DEBUG, "Request %u is not supported yet",
				    opcode);
			break;
		}
	}
	rte_free(info.msg_buf);
}

/**
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) regsitered before.
 *
 * @return
 *  void
 */
static void
i40e_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t icr0;

	/* Disable interrupt */
	i40e_pf_disable_irq0(hw);

	/* read out interrupt causes */
	icr0 = I40E_READ_REG(hw, I40E_PFINT_ICR0);

	/* No interrupt event indicated */
	if (!(icr0 & I40E_PFINT_ICR0_INTEVENT_MASK)) {
		PMD_DRV_LOG(INFO, "No interrupt event");
		goto done;
	}
	if (icr0 & I40E_PFINT_ICR0_ECC_ERR_MASK)
		PMD_DRV_LOG(ERR, "ICR0: unrecoverable ECC error");
	if (icr0 & I40E_PFINT_ICR0_MAL_DETECT_MASK)
		PMD_DRV_LOG(ERR, "ICR0: malicious programming detected");
	if (icr0 & I40E_PFINT_ICR0_GRST_MASK)
		PMD_DRV_LOG(INFO, "ICR0: global reset requested");
	if (icr0 & I40E_PFINT_ICR0_PCI_EXCEPTION_MASK)
		PMD_DRV_LOG(INFO, "ICR0: PCI exception activated");
	if (icr0 & I40E_PFINT_ICR0_STORM_DETECT_MASK)
		PMD_DRV_LOG(INFO, "ICR0: a change in the storm control state");
	if (icr0 & I40E_PFINT_ICR0_HMC_ERR_MASK)
		PMD_DRV_LOG(ERR, "ICR0: HMC error");
	if (icr0 & I40E_PFINT_ICR0_PE_CRITERR_MASK)
		PMD_DRV_LOG(ERR, "ICR0: protocol engine critical error");

	if (icr0 & I40E_PFINT_ICR0_VFLR_MASK) {
		PMD_DRV_LOG(INFO, "ICR0: VF reset detected");
		i40e_dev_handle_vfr_event(dev);
	}
	if (icr0 & I40E_PFINT_ICR0_ADMINQ_MASK) {
		PMD_DRV_LOG(INFO, "ICR0: adminq event");
		i40e_dev_handle_aq_msg(dev);
	}

done:
	/* Enable interrupt */
	i40e_pf_enable_irq0(hw);
	rte_intr_enable(dev->intr_handle);
}

int
i40e_add_macvlan_filters(struct i40e_vsi *vsi,
			 struct i40e_macvlan_filter *filter,
			 int total)
{
	int ele_num, ele_buff_size;
	int num, actual_num, i;
	uint16_t flags;
	int ret = I40E_SUCCESS;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_add_macvlan_element_data *req_list;

	if (filter == NULL  || total == 0)
		return I40E_ERR_PARAM;
	ele_num = hw->aq.asq_buf_size / sizeof(*req_list);
	ele_buff_size = hw->aq.asq_buf_size;

	req_list = rte_zmalloc("macvlan_add", ele_buff_size, 0);
	if (req_list == NULL) {
		PMD_DRV_LOG(ERR, "Fail to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	num = 0;
	do {
		actual_num = (num + ele_num > total) ? (total - num) : ele_num;
		memset(req_list, 0, ele_buff_size);

		for (i = 0; i < actual_num; i++) {
			(void)rte_memcpy(req_list[i].mac_addr,
				&filter[num + i].macaddr, ETH_ADDR_LEN);
			req_list[i].vlan_tag =
				rte_cpu_to_le_16(filter[num + i].vlan_id);

			switch (filter[num + i].filter_type) {
			case RTE_MAC_PERFECT_MATCH:
				flags = I40E_AQC_MACVLAN_ADD_PERFECT_MATCH |
					I40E_AQC_MACVLAN_ADD_IGNORE_VLAN;
				break;
			case RTE_MACVLAN_PERFECT_MATCH:
				flags = I40E_AQC_MACVLAN_ADD_PERFECT_MATCH;
				break;
			case RTE_MAC_HASH_MATCH:
				flags = I40E_AQC_MACVLAN_ADD_HASH_MATCH |
					I40E_AQC_MACVLAN_ADD_IGNORE_VLAN;
				break;
			case RTE_MACVLAN_HASH_MATCH:
				flags = I40E_AQC_MACVLAN_ADD_HASH_MATCH;
				break;
			default:
				PMD_DRV_LOG(ERR, "Invalid MAC match type");
				ret = I40E_ERR_PARAM;
				goto DONE;
			}

			req_list[i].queue_number = 0;

			req_list[i].flags = rte_cpu_to_le_16(flags);
		}

		ret = i40e_aq_add_macvlan(hw, vsi->seid, req_list,
						actual_num, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to add macvlan filter");
			goto DONE;
		}
		num += actual_num;
	} while (num < total);

DONE:
	rte_free(req_list);
	return ret;
}

int
i40e_remove_macvlan_filters(struct i40e_vsi *vsi,
			    struct i40e_macvlan_filter *filter,
			    int total)
{
	int ele_num, ele_buff_size;
	int num, actual_num, i;
	uint16_t flags;
	int ret = I40E_SUCCESS;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_remove_macvlan_element_data *req_list;

	if (filter == NULL  || total == 0)
		return I40E_ERR_PARAM;

	ele_num = hw->aq.asq_buf_size / sizeof(*req_list);
	ele_buff_size = hw->aq.asq_buf_size;

	req_list = rte_zmalloc("macvlan_remove", ele_buff_size, 0);
	if (req_list == NULL) {
		PMD_DRV_LOG(ERR, "Fail to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	num = 0;
	do {
		actual_num = (num + ele_num > total) ? (total - num) : ele_num;
		memset(req_list, 0, ele_buff_size);

		for (i = 0; i < actual_num; i++) {
			(void)rte_memcpy(req_list[i].mac_addr,
				&filter[num + i].macaddr, ETH_ADDR_LEN);
			req_list[i].vlan_tag =
				rte_cpu_to_le_16(filter[num + i].vlan_id);

			switch (filter[num + i].filter_type) {
			case RTE_MAC_PERFECT_MATCH:
				flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH |
					I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
				break;
			case RTE_MACVLAN_PERFECT_MATCH:
				flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
				break;
			case RTE_MAC_HASH_MATCH:
				flags = I40E_AQC_MACVLAN_DEL_HASH_MATCH |
					I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
				break;
			case RTE_MACVLAN_HASH_MATCH:
				flags = I40E_AQC_MACVLAN_DEL_HASH_MATCH;
				break;
			default:
				PMD_DRV_LOG(ERR, "Invalid MAC filter type");
				ret = I40E_ERR_PARAM;
				goto DONE;
			}
			req_list[i].flags = rte_cpu_to_le_16(flags);
		}

		ret = i40e_aq_remove_macvlan(hw, vsi->seid, req_list,
						actual_num, NULL);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Failed to remove macvlan filter");
			goto DONE;
		}
		num += actual_num;
	} while (num < total);

DONE:
	rte_free(req_list);
	return ret;
}

/* Find out specific MAC filter */
static struct i40e_mac_filter *
i40e_find_mac_filter(struct i40e_vsi *vsi,
			 struct ether_addr *macaddr)
{
	struct i40e_mac_filter *f;

	TAILQ_FOREACH(f, &vsi->mac_list, next) {
		if (is_same_ether_addr(macaddr, &f->mac_info.mac_addr))
			return f;
	}

	return NULL;
}

static bool
i40e_find_vlan_filter(struct i40e_vsi *vsi,
			 uint16_t vlan_id)
{
	uint32_t vid_idx, vid_bit;

	if (vlan_id > ETH_VLAN_ID_MAX)
		return 0;

	vid_idx = I40E_VFTA_IDX(vlan_id);
	vid_bit = I40E_VFTA_BIT(vlan_id);

	if (vsi->vfta[vid_idx] & vid_bit)
		return 1;
	else
		return 0;
}

static void
i40e_store_vlan_filter(struct i40e_vsi *vsi,
		       uint16_t vlan_id, bool on)
{
	uint32_t vid_idx, vid_bit;

	vid_idx = I40E_VFTA_IDX(vlan_id);
	vid_bit = I40E_VFTA_BIT(vlan_id);

	if (on)
		vsi->vfta[vid_idx] |= vid_bit;
	else
		vsi->vfta[vid_idx] &= ~vid_bit;
}

void
i40e_set_vlan_filter(struct i40e_vsi *vsi,
		     uint16_t vlan_id, bool on)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	struct i40e_aqc_add_remove_vlan_element_data vlan_data = {0};
	int ret;

	if (vlan_id > ETH_VLAN_ID_MAX)
		return;

	i40e_store_vlan_filter(vsi, vlan_id, on);

	if ((!vsi->vlan_anti_spoof_on && !vsi->vlan_filter_on) || !vlan_id)
		return;

	vlan_data.vlan_tag = rte_cpu_to_le_16(vlan_id);

	if (on) {
		ret = i40e_aq_add_vlan(hw, vsi->seid,
				       &vlan_data, 1, NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(ERR, "Failed to add vlan filter");
	} else {
		ret = i40e_aq_remove_vlan(hw, vsi->seid,
					  &vlan_data, 1, NULL);
		if (ret != I40E_SUCCESS)
			PMD_DRV_LOG(ERR,
				    "Failed to remove vlan filter");
	}
}

/**
 * Find all vlan options for specific mac addr,
 * return with actual vlan found.
 */
int
i40e_find_all_vlan_for_mac(struct i40e_vsi *vsi,
			   struct i40e_macvlan_filter *mv_f,
			   int num, struct ether_addr *addr)
{
	int i;
	uint32_t j, k;

	/**
	 * Not to use i40e_find_vlan_filter to decrease the loop time,
	 * although the code looks complex.
	  */
	if (num < vsi->vlan_num)
		return I40E_ERR_PARAM;

	i = 0;
	for (j = 0; j < I40E_VFTA_SIZE; j++) {
		if (vsi->vfta[j]) {
			for (k = 0; k < I40E_UINT32_BIT_SIZE; k++) {
				if (vsi->vfta[j] & (1 << k)) {
					if (i > num - 1) {
						PMD_DRV_LOG(ERR,
							"vlan number doesn't match");
						return I40E_ERR_PARAM;
					}
					(void)rte_memcpy(&mv_f[i].macaddr,
							addr, ETH_ADDR_LEN);
					mv_f[i].vlan_id =
						j * I40E_UINT32_BIT_SIZE + k;
					i++;
				}
			}
		}
	}
	return I40E_SUCCESS;
}

static inline int
i40e_find_all_mac_for_vlan(struct i40e_vsi *vsi,
			   struct i40e_macvlan_filter *mv_f,
			   int num,
			   uint16_t vlan)
{
	int i = 0;
	struct i40e_mac_filter *f;

	if (num < vsi->mac_num)
		return I40E_ERR_PARAM;

	TAILQ_FOREACH(f, &vsi->mac_list, next) {
		if (i > num - 1) {
			PMD_DRV_LOG(ERR, "buffer number not match");
			return I40E_ERR_PARAM;
		}
		(void)rte_memcpy(&mv_f[i].macaddr, &f->mac_info.mac_addr,
				ETH_ADDR_LEN);
		mv_f[i].vlan_id = vlan;
		mv_f[i].filter_type = f->mac_info.filter_type;
		i++;
	}

	return I40E_SUCCESS;
}

static int
i40e_vsi_remove_all_macvlan_filter(struct i40e_vsi *vsi)
{
	int i, j, num;
	struct i40e_mac_filter *f;
	struct i40e_macvlan_filter *mv_f;
	int ret = I40E_SUCCESS;

	if (vsi == NULL || vsi->mac_num == 0)
		return I40E_ERR_PARAM;

	/* Case that no vlan is set */
	if (vsi->vlan_num == 0)
		num = vsi->mac_num;
	else
		num = vsi->mac_num * vsi->vlan_num;

	mv_f = rte_zmalloc("macvlan_data", num * sizeof(*mv_f), 0);
	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	i = 0;
	if (vsi->vlan_num == 0) {
		TAILQ_FOREACH(f, &vsi->mac_list, next) {
			(void)rte_memcpy(&mv_f[i].macaddr,
				&f->mac_info.mac_addr, ETH_ADDR_LEN);
			mv_f[i].filter_type = f->mac_info.filter_type;
			mv_f[i].vlan_id = 0;
			i++;
		}
	} else {
		TAILQ_FOREACH(f, &vsi->mac_list, next) {
			ret = i40e_find_all_vlan_for_mac(vsi,&mv_f[i],
					vsi->vlan_num, &f->mac_info.mac_addr);
			if (ret != I40E_SUCCESS)
				goto DONE;
			for (j = i; j < i + vsi->vlan_num; j++)
				mv_f[j].filter_type = f->mac_info.filter_type;
			i += vsi->vlan_num;
		}
	}

	ret = i40e_remove_macvlan_filters(vsi, mv_f, num);
DONE:
	rte_free(mv_f);

	return ret;
}

int
i40e_vsi_add_vlan(struct i40e_vsi *vsi, uint16_t vlan)
{
	struct i40e_macvlan_filter *mv_f;
	int mac_num;
	int ret = I40E_SUCCESS;

	if (!vsi || vlan > ETHER_MAX_VLAN_ID)
		return I40E_ERR_PARAM;

	/* If it's already set, just return */
	if (i40e_find_vlan_filter(vsi,vlan))
		return I40E_SUCCESS;

	mac_num = vsi->mac_num;

	if (mac_num == 0) {
		PMD_DRV_LOG(ERR, "Error! VSI doesn't have a mac addr");
		return I40E_ERR_PARAM;
	}

	mv_f = rte_zmalloc("macvlan_data", mac_num * sizeof(*mv_f), 0);

	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	ret = i40e_find_all_mac_for_vlan(vsi, mv_f, mac_num, vlan);

	if (ret != I40E_SUCCESS)
		goto DONE;

	ret = i40e_add_macvlan_filters(vsi, mv_f, mac_num);

	if (ret != I40E_SUCCESS)
		goto DONE;

	i40e_set_vlan_filter(vsi, vlan, 1);

	vsi->vlan_num++;
	ret = I40E_SUCCESS;
DONE:
	rte_free(mv_f);
	return ret;
}

int
i40e_vsi_delete_vlan(struct i40e_vsi *vsi, uint16_t vlan)
{
	struct i40e_macvlan_filter *mv_f;
	int mac_num;
	int ret = I40E_SUCCESS;

	/**
	 * Vlan 0 is the generic filter for untagged packets
	 * and can't be removed.
	 */
	if (!vsi || vlan == 0 || vlan > ETHER_MAX_VLAN_ID)
		return I40E_ERR_PARAM;

	/* If can't find it, just return */
	if (!i40e_find_vlan_filter(vsi, vlan))
		return I40E_ERR_PARAM;

	mac_num = vsi->mac_num;

	if (mac_num == 0) {
		PMD_DRV_LOG(ERR, "Error! VSI doesn't have a mac addr");
		return I40E_ERR_PARAM;
	}

	mv_f = rte_zmalloc("macvlan_data", mac_num * sizeof(*mv_f), 0);

	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	ret = i40e_find_all_mac_for_vlan(vsi, mv_f, mac_num, vlan);

	if (ret != I40E_SUCCESS)
		goto DONE;

	ret = i40e_remove_macvlan_filters(vsi, mv_f, mac_num);

	if (ret != I40E_SUCCESS)
		goto DONE;

	/* This is last vlan to remove, replace all mac filter with vlan 0 */
	if (vsi->vlan_num == 1) {
		ret = i40e_find_all_mac_for_vlan(vsi, mv_f, mac_num, 0);
		if (ret != I40E_SUCCESS)
			goto DONE;

		ret = i40e_add_macvlan_filters(vsi, mv_f, mac_num);
		if (ret != I40E_SUCCESS)
			goto DONE;
	}

	i40e_set_vlan_filter(vsi, vlan, 0);

	vsi->vlan_num--;
	ret = I40E_SUCCESS;
DONE:
	rte_free(mv_f);
	return ret;
}

int
i40e_vsi_add_mac(struct i40e_vsi *vsi, struct i40e_mac_filter_info *mac_filter)
{
	struct i40e_mac_filter *f;
	struct i40e_macvlan_filter *mv_f;
	int i, vlan_num = 0;
	int ret = I40E_SUCCESS;

	/* If it's add and we've config it, return */
	f = i40e_find_mac_filter(vsi, &mac_filter->mac_addr);
	if (f != NULL)
		return I40E_SUCCESS;
	if ((mac_filter->filter_type == RTE_MACVLAN_PERFECT_MATCH) ||
		(mac_filter->filter_type == RTE_MACVLAN_HASH_MATCH)) {

		/**
		 * If vlan_num is 0, that's the first time to add mac,
		 * set mask for vlan_id 0.
		 */
		if (vsi->vlan_num == 0) {
			i40e_set_vlan_filter(vsi, 0, 1);
			vsi->vlan_num = 1;
		}
		vlan_num = vsi->vlan_num;
	} else if ((mac_filter->filter_type == RTE_MAC_PERFECT_MATCH) ||
			(mac_filter->filter_type == RTE_MAC_HASH_MATCH))
		vlan_num = 1;

	mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	for (i = 0; i < vlan_num; i++) {
		mv_f[i].filter_type = mac_filter->filter_type;
		(void)rte_memcpy(&mv_f[i].macaddr, &mac_filter->mac_addr,
				ETH_ADDR_LEN);
	}

	if (mac_filter->filter_type == RTE_MACVLAN_PERFECT_MATCH ||
		mac_filter->filter_type == RTE_MACVLAN_HASH_MATCH) {
		ret = i40e_find_all_vlan_for_mac(vsi, mv_f, vlan_num,
					&mac_filter->mac_addr);
		if (ret != I40E_SUCCESS)
			goto DONE;
	}

	ret = i40e_add_macvlan_filters(vsi, mv_f, vlan_num);
	if (ret != I40E_SUCCESS)
		goto DONE;

	/* Add the mac addr into mac list */
	f = rte_zmalloc("macv_filter", sizeof(*f), 0);
	if (f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		ret = I40E_ERR_NO_MEMORY;
		goto DONE;
	}
	(void)rte_memcpy(&f->mac_info.mac_addr, &mac_filter->mac_addr,
			ETH_ADDR_LEN);
	f->mac_info.filter_type = mac_filter->filter_type;
	TAILQ_INSERT_TAIL(&vsi->mac_list, f, next);
	vsi->mac_num++;

	ret = I40E_SUCCESS;
DONE:
	rte_free(mv_f);

	return ret;
}

int
i40e_vsi_delete_mac(struct i40e_vsi *vsi, struct ether_addr *addr)
{
	struct i40e_mac_filter *f;
	struct i40e_macvlan_filter *mv_f;
	int i, vlan_num;
	enum rte_mac_filter_type filter_type;
	int ret = I40E_SUCCESS;

	/* Can't find it, return an error */
	f = i40e_find_mac_filter(vsi, addr);
	if (f == NULL)
		return I40E_ERR_PARAM;

	vlan_num = vsi->vlan_num;
	filter_type = f->mac_info.filter_type;
	if (filter_type == RTE_MACVLAN_PERFECT_MATCH ||
		filter_type == RTE_MACVLAN_HASH_MATCH) {
		if (vlan_num == 0) {
			PMD_DRV_LOG(ERR, "VLAN number shouldn't be 0");
			return I40E_ERR_PARAM;
		}
	} else if (filter_type == RTE_MAC_PERFECT_MATCH ||
			filter_type == RTE_MAC_HASH_MATCH)
		vlan_num = 1;

	mv_f = rte_zmalloc("macvlan_data", vlan_num * sizeof(*mv_f), 0);
	if (mv_f == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}

	for (i = 0; i < vlan_num; i++) {
		mv_f[i].filter_type = filter_type;
		(void)rte_memcpy(&mv_f[i].macaddr, &f->mac_info.mac_addr,
				ETH_ADDR_LEN);
	}
	if (filter_type == RTE_MACVLAN_PERFECT_MATCH ||
			filter_type == RTE_MACVLAN_HASH_MATCH) {
		ret = i40e_find_all_vlan_for_mac(vsi, mv_f, vlan_num, addr);
		if (ret != I40E_SUCCESS)
			goto DONE;
	}

	ret = i40e_remove_macvlan_filters(vsi, mv_f, vlan_num);
	if (ret != I40E_SUCCESS)
		goto DONE;

	/* Remove the mac addr into mac list */
	TAILQ_REMOVE(&vsi->mac_list, f, next);
	rte_free(f);
	vsi->mac_num--;

	ret = I40E_SUCCESS;
DONE:
	rte_free(mv_f);
	return ret;
}

/* Configure hash enable flags for RSS */
uint64_t
i40e_config_hena(uint64_t flags, enum i40e_mac_type type)
{
	uint64_t hena = 0;

	if (!flags)
		return hena;

	if (flags & ETH_RSS_FRAG_IPV4)
		hena |= 1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4;
	if (flags & ETH_RSS_NONFRAG_IPV4_TCP) {
		if (type == I40E_MAC_X722) {
			hena |= (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP) |
			 (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK);
		} else
			hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP;
	}
	if (flags & ETH_RSS_NONFRAG_IPV4_UDP) {
		if (type == I40E_MAC_X722) {
			hena |= (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP) |
			 (1ULL << I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP) |
			 (1ULL << I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP);
		} else
			hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
	}
	if (flags & ETH_RSS_NONFRAG_IPV4_SCTP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP;
	if (flags & ETH_RSS_NONFRAG_IPV4_OTHER)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER;
	if (flags & ETH_RSS_FRAG_IPV6)
		hena |= 1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6;
	if (flags & ETH_RSS_NONFRAG_IPV6_TCP) {
		if (type == I40E_MAC_X722) {
			hena |= (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP) |
			 (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK);
		} else
			hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP;
	}
	if (flags & ETH_RSS_NONFRAG_IPV6_UDP) {
		if (type == I40E_MAC_X722) {
			hena |= (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP) |
			 (1ULL << I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP) |
			 (1ULL << I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP);
		} else
			hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP;
	}
	if (flags & ETH_RSS_NONFRAG_IPV6_SCTP)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP;
	if (flags & ETH_RSS_NONFRAG_IPV6_OTHER)
		hena |= 1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER;
	if (flags & ETH_RSS_L2_PAYLOAD)
		hena |= 1ULL << I40E_FILTER_PCTYPE_L2_PAYLOAD;

	return hena;
}

/* Parse the hash enable flags */
uint64_t
i40e_parse_hena(uint64_t flags)
{
	uint64_t rss_hf = 0;

	if (!flags)
		return rss_hf;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_FRAG_IPV4))
		rss_hf |= ETH_RSS_FRAG_IPV4;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_TCP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_TCP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_UDP))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_SCTP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER))
		rss_hf |= ETH_RSS_NONFRAG_IPV4_OTHER;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_FRAG_IPV6))
		rss_hf |= ETH_RSS_FRAG_IPV6;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_TCP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_TCP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_UDP))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_UDP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_SCTP;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER))
		rss_hf |= ETH_RSS_NONFRAG_IPV6_OTHER;
	if (flags & (1ULL << I40E_FILTER_PCTYPE_L2_PAYLOAD))
		rss_hf |= ETH_RSS_L2_PAYLOAD;

	return rss_hf;
}

/* Disable RSS */
static void
i40e_pf_disable_rss(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint64_t hena;

	hena = (uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0));
	hena |= ((uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1))) << 32;
	if (hw->mac.type == I40E_MAC_X722)
		hena &= ~I40E_RSS_HENA_ALL_X722;
	else
		hena &= ~I40E_RSS_HENA_ALL;
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(0), (uint32_t)hena);
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(1), (uint32_t)(hena >> 32));
	I40E_WRITE_FLUSH(hw);
}

static int
i40e_set_rss_key(struct i40e_vsi *vsi, uint8_t *key, uint8_t key_len)
{
	struct i40e_pf *pf = I40E_VSI_TO_PF(vsi);
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	int ret = 0;

	if (!key || key_len == 0) {
		PMD_DRV_LOG(DEBUG, "No key to be configured");
		return 0;
	} else if (key_len != (I40E_PFQF_HKEY_MAX_INDEX + 1) *
		sizeof(uint32_t)) {
		PMD_DRV_LOG(ERR, "Invalid key length %u", key_len);
		return -EINVAL;
	}

	if (pf->flags & I40E_FLAG_RSS_AQ_CAPABLE) {
		struct i40e_aqc_get_set_rss_key_data *key_dw =
			(struct i40e_aqc_get_set_rss_key_data *)key;

		ret = i40e_aq_set_rss_key(hw, vsi->vsi_id, key_dw);
		if (ret)
			PMD_INIT_LOG(ERR, "Failed to configure RSS key via AQ");
	} else {
		uint32_t *hash_key = (uint32_t *)key;
		uint16_t i;

		for (i = 0; i <= I40E_PFQF_HKEY_MAX_INDEX; i++)
			i40e_write_rx_ctl(hw, I40E_PFQF_HKEY(i), hash_key[i]);
		I40E_WRITE_FLUSH(hw);
	}

	return ret;
}

static int
i40e_get_rss_key(struct i40e_vsi *vsi, uint8_t *key, uint8_t *key_len)
{
	struct i40e_pf *pf = I40E_VSI_TO_PF(vsi);
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	int ret;

	if (!key || !key_len)
		return -EINVAL;

	if (pf->flags & I40E_FLAG_RSS_AQ_CAPABLE) {
		ret = i40e_aq_get_rss_key(hw, vsi->vsi_id,
			(struct i40e_aqc_get_set_rss_key_data *)key);
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to get RSS key via AQ");
			return ret;
		}
	} else {
		uint32_t *key_dw = (uint32_t *)key;
		uint16_t i;

		for (i = 0; i <= I40E_PFQF_HKEY_MAX_INDEX; i++)
			key_dw[i] = i40e_read_rx_ctl(hw, I40E_PFQF_HKEY(i));
	}
	*key_len = (I40E_PFQF_HKEY_MAX_INDEX + 1) * sizeof(uint32_t);

	return 0;
}

static int
i40e_hw_rss_hash_set(struct i40e_pf *pf, struct rte_eth_rss_conf *rss_conf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint64_t rss_hf;
	uint64_t hena;
	int ret;

	ret = i40e_set_rss_key(pf->main_vsi, rss_conf->rss_key,
			       rss_conf->rss_key_len);
	if (ret)
		return ret;

	rss_hf = rss_conf->rss_hf;
	hena = (uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0));
	hena |= ((uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1))) << 32;
	if (hw->mac.type == I40E_MAC_X722)
		hena &= ~I40E_RSS_HENA_ALL_X722;
	else
		hena &= ~I40E_RSS_HENA_ALL;
	hena |= i40e_config_hena(rss_hf, hw->mac.type);
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(0), (uint32_t)hena);
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(1), (uint32_t)(hena >> 32));
	I40E_WRITE_FLUSH(hw);

	return 0;
}

static int
i40e_dev_rss_hash_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_conf *rss_conf)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t rss_hf = rss_conf->rss_hf & I40E_RSS_OFFLOAD_ALL;
	uint64_t hena;

	hena = (uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0));
	hena |= ((uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1))) << 32;
	if (!(hena & ((hw->mac.type == I40E_MAC_X722)
		 ? I40E_RSS_HENA_ALL_X722
		 : I40E_RSS_HENA_ALL))) { /* RSS disabled */
		if (rss_hf != 0) /* Enable RSS */
			return -EINVAL;
		return 0; /* Nothing to do */
	}
	/* RSS enabled */
	if (rss_hf == 0) /* Disable RSS */
		return -EINVAL;

	return i40e_hw_rss_hash_set(pf, rss_conf);
}

static int
i40e_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			   struct rte_eth_rss_conf *rss_conf)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t hena;

	i40e_get_rss_key(pf->main_vsi, rss_conf->rss_key,
			 &rss_conf->rss_key_len);

	hena = (uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0));
	hena |= ((uint64_t)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1))) << 32;
	rss_conf->rss_hf = i40e_parse_hena(hena);

	return 0;
}

static int
i40e_dev_get_filter_type(uint16_t filter_type, uint16_t *flag)
{
	switch (filter_type) {
	case RTE_TUNNEL_FILTER_IMAC_IVLAN:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_IMAC_IVLAN;
		break;
	case RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_IMAC_IVLAN_TEN_ID;
		break;
	case RTE_TUNNEL_FILTER_IMAC_TENID:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_IMAC_TEN_ID;
		break;
	case RTE_TUNNEL_FILTER_OMAC_TENID_IMAC:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_OMAC_TEN_ID_IMAC;
		break;
	case ETH_TUNNEL_FILTER_IMAC:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_IMAC;
		break;
	case ETH_TUNNEL_FILTER_OIP:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_OIP;
		break;
	case ETH_TUNNEL_FILTER_IIP:
		*flag = I40E_AQC_ADD_CLOUD_FILTER_IIP;
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid tunnel filter type");
		return -EINVAL;
	}

	return 0;
}

/* Convert tunnel filter structure */
static int
i40e_tunnel_filter_convert(
	struct i40e_aqc_add_rm_cloud_filt_elem_ext *cld_filter,
	struct i40e_tunnel_filter *tunnel_filter)
{
	ether_addr_copy((struct ether_addr *)&cld_filter->element.outer_mac,
			(struct ether_addr *)&tunnel_filter->input.outer_mac);
	ether_addr_copy((struct ether_addr *)&cld_filter->element.inner_mac,
			(struct ether_addr *)&tunnel_filter->input.inner_mac);
	tunnel_filter->input.inner_vlan = cld_filter->element.inner_vlan;
	if ((rte_le_to_cpu_16(cld_filter->element.flags) &
	     I40E_AQC_ADD_CLOUD_FLAGS_IPV6) ==
	    I40E_AQC_ADD_CLOUD_FLAGS_IPV6)
		tunnel_filter->input.ip_type = I40E_TUNNEL_IPTYPE_IPV6;
	else
		tunnel_filter->input.ip_type = I40E_TUNNEL_IPTYPE_IPV4;
	tunnel_filter->input.flags = cld_filter->element.flags;
	tunnel_filter->input.tenant_id = cld_filter->element.tenant_id;
	tunnel_filter->queue = cld_filter->element.queue_number;
	rte_memcpy(tunnel_filter->input.general_fields,
		   cld_filter->general_fields,
		   sizeof(cld_filter->general_fields));

	return 0;
}

/* Check if there exists the tunnel filter */
struct i40e_tunnel_filter *
i40e_sw_tunnel_filter_lookup(struct i40e_tunnel_rule *tunnel_rule,
			     const struct i40e_tunnel_filter_input *input)
{
	int ret;

	ret = rte_hash_lookup(tunnel_rule->hash_table, (const void *)input);
	if (ret < 0)
		return NULL;

	return tunnel_rule->hash_map[ret];
}

/* Add a tunnel filter into the SW list */
static int
i40e_sw_tunnel_filter_insert(struct i40e_pf *pf,
			     struct i40e_tunnel_filter *tunnel_filter)
{
	struct i40e_tunnel_rule *rule = &pf->tunnel;
	int ret;

	ret = rte_hash_add_key(rule->hash_table, &tunnel_filter->input);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to insert tunnel filter to hash table %d!",
			    ret);
		return ret;
	}
	rule->hash_map[ret] = tunnel_filter;

	TAILQ_INSERT_TAIL(&rule->tunnel_list, tunnel_filter, rules);

	return 0;
}

/* Delete a tunnel filter from the SW list */
int
i40e_sw_tunnel_filter_del(struct i40e_pf *pf,
			  struct i40e_tunnel_filter_input *input)
{
	struct i40e_tunnel_rule *rule = &pf->tunnel;
	struct i40e_tunnel_filter *tunnel_filter;
	int ret;

	ret = rte_hash_del_key(rule->hash_table, input);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to delete tunnel filter to hash table %d!",
			    ret);
		return ret;
	}
	tunnel_filter = rule->hash_map[ret];
	rule->hash_map[ret] = NULL;

	TAILQ_REMOVE(&rule->tunnel_list, tunnel_filter, rules);
	rte_free(tunnel_filter);

	return 0;
}

int
i40e_dev_tunnel_filter_set(struct i40e_pf *pf,
			struct rte_eth_tunnel_filter_conf *tunnel_filter,
			uint8_t add)
{
	uint16_t ip_type;
	uint32_t ipv4_addr;
	uint8_t i, tun_type = 0;
	/* internal varialbe to convert ipv6 byte order */
	uint32_t convert_ipv6[4];
	int val, ret = 0;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi = pf->main_vsi;
	struct i40e_aqc_add_rm_cloud_filt_elem_ext *cld_filter;
	struct i40e_aqc_add_rm_cloud_filt_elem_ext *pfilter;
	struct i40e_tunnel_rule *tunnel_rule = &pf->tunnel;
	struct i40e_tunnel_filter *tunnel, *node;
	struct i40e_tunnel_filter check_filter; /* Check if filter exists */

	cld_filter = rte_zmalloc("tunnel_filter",
			 sizeof(struct i40e_aqc_add_rm_cloud_filt_elem_ext),
	0);

	if (NULL == cld_filter) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}
	pfilter = cld_filter;

	ether_addr_copy(&tunnel_filter->outer_mac,
			(struct ether_addr *)&pfilter->element.outer_mac);
	ether_addr_copy(&tunnel_filter->inner_mac,
			(struct ether_addr *)&pfilter->element.inner_mac);

	pfilter->element.inner_vlan =
		rte_cpu_to_le_16(tunnel_filter->inner_vlan);
	if (tunnel_filter->ip_type == RTE_TUNNEL_IPTYPE_IPV4) {
		ip_type = I40E_AQC_ADD_CLOUD_FLAGS_IPV4;
		ipv4_addr = rte_be_to_cpu_32(tunnel_filter->ip_addr.ipv4_addr);
		rte_memcpy(&pfilter->element.ipaddr.v4.data,
				&rte_cpu_to_le_32(ipv4_addr),
				sizeof(pfilter->element.ipaddr.v4.data));
	} else {
		ip_type = I40E_AQC_ADD_CLOUD_FLAGS_IPV6;
		for (i = 0; i < 4; i++) {
			convert_ipv6[i] =
			rte_cpu_to_le_32(rte_be_to_cpu_32(tunnel_filter->ip_addr.ipv6_addr[i]));
		}
		rte_memcpy(&pfilter->element.ipaddr.v6.data,
			   &convert_ipv6,
			   sizeof(pfilter->element.ipaddr.v6.data));
	}

	/* check tunneled type */
	switch (tunnel_filter->tunnel_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_VXLAN;
		break;
	case RTE_TUNNEL_TYPE_NVGRE:
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_NVGRE_OMAC;
		break;
	case RTE_TUNNEL_TYPE_IP_IN_GRE:
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_IP;
		break;
	default:
		/* Other tunnel types is not supported. */
		PMD_DRV_LOG(ERR, "tunnel type is not supported.");
		rte_free(cld_filter);
		return -EINVAL;
	}

	val = i40e_dev_get_filter_type(tunnel_filter->filter_type,
				       &pfilter->element.flags);
	if (val < 0) {
		rte_free(cld_filter);
		return -EINVAL;
	}

	pfilter->element.flags |= rte_cpu_to_le_16(
		I40E_AQC_ADD_CLOUD_FLAGS_TO_QUEUE |
		ip_type | (tun_type << I40E_AQC_ADD_CLOUD_TNL_TYPE_SHIFT));
	pfilter->element.tenant_id = rte_cpu_to_le_32(tunnel_filter->tenant_id);
	pfilter->element.queue_number =
		rte_cpu_to_le_16(tunnel_filter->queue_id);

	/* Check if there is the filter in SW list */
	memset(&check_filter, 0, sizeof(check_filter));
	i40e_tunnel_filter_convert(cld_filter, &check_filter);
	node = i40e_sw_tunnel_filter_lookup(tunnel_rule, &check_filter.input);
	if (add && node) {
		PMD_DRV_LOG(ERR, "Conflict with existing tunnel rules!");
		return -EINVAL;
	}

	if (!add && !node) {
		PMD_DRV_LOG(ERR, "There's no corresponding tunnel filter!");
		return -EINVAL;
	}

	if (add) {
		ret = i40e_aq_add_cloud_filters(hw,
					vsi->seid, &cld_filter->element, 1);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to add a tunnel filter.");
			return -ENOTSUP;
		}
		tunnel = rte_zmalloc("tunnel_filter", sizeof(*tunnel), 0);
		rte_memcpy(tunnel, &check_filter, sizeof(check_filter));
		ret = i40e_sw_tunnel_filter_insert(pf, tunnel);
	} else {
		ret = i40e_aq_remove_cloud_filters(hw, vsi->seid,
						   &cld_filter->element, 1);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to delete a tunnel filter.");
			return -ENOTSUP;
		}
		ret = i40e_sw_tunnel_filter_del(pf, &node->input);
	}

	rte_free(cld_filter);
	return ret;
}

#define I40E_AQC_REPLACE_CLOUD_CMD_INPUT_TR_WORD0 0x48
#define I40E_TR_VXLAN_GRE_KEY_MASK		0x4
#define I40E_TR_GENEVE_KEY_MASK			0x8
#define I40E_TR_GENERIC_UDP_TUNNEL_MASK		0x40
#define I40E_TR_GRE_KEY_MASK			0x400
#define I40E_TR_GRE_KEY_WITH_XSUM_MASK		0x800
#define I40E_TR_GRE_NO_KEY_MASK			0x8000

static enum
i40e_status_code i40e_replace_mpls_l1_filter(struct i40e_pf *pf)
{
	struct i40e_aqc_replace_cloud_filters_cmd  filter_replace;
	struct i40e_aqc_replace_cloud_filters_cmd_buf  filter_replace_buf;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	enum i40e_status_code status = I40E_SUCCESS;

	memset(&filter_replace, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd));
	memset(&filter_replace_buf, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd_buf));

	/* create L1 filter */
	filter_replace.old_filter_type =
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_IMAC;
	filter_replace.new_filter_type = I40E_AQC_ADD_L1_FILTER_TEID_MPLS;
	filter_replace.tr_bit = 0;

	/* Prepare the buffer, 3 entries */
	filter_replace_buf.data[0] =
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TEID_WORD0;
	filter_replace_buf.data[0] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	filter_replace_buf.data[2] = 0xFF;
	filter_replace_buf.data[3] = 0xFF;
	filter_replace_buf.data[4] =
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TEID_WORD1;
	filter_replace_buf.data[4] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	filter_replace_buf.data[7] = 0xF0;
	filter_replace_buf.data[8]
		= I40E_AQC_REPLACE_CLOUD_CMD_INPUT_TR_WORD0;
	filter_replace_buf.data[8] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	filter_replace_buf.data[10] = I40E_TR_VXLAN_GRE_KEY_MASK |
		I40E_TR_GENEVE_KEY_MASK |
		I40E_TR_GENERIC_UDP_TUNNEL_MASK;
	filter_replace_buf.data[11] = (I40E_TR_GRE_KEY_MASK |
		I40E_TR_GRE_KEY_WITH_XSUM_MASK |
		I40E_TR_GRE_NO_KEY_MASK) >> 8;

	status = i40e_aq_replace_cloud_filters(hw, &filter_replace,
					       &filter_replace_buf);
	return status;
}

static enum
i40e_status_code i40e_replace_mpls_cloud_filter(struct i40e_pf *pf)
{
	struct i40e_aqc_replace_cloud_filters_cmd  filter_replace;
	struct i40e_aqc_replace_cloud_filters_cmd_buf  filter_replace_buf;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	enum i40e_status_code status = I40E_SUCCESS;

	/* For MPLSoUDP */
	memset(&filter_replace, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd));
	memset(&filter_replace_buf, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd_buf));
	filter_replace.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER |
		I40E_AQC_MIRROR_CLOUD_FILTER;
	filter_replace.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_IIP;
	filter_replace.new_filter_type =
		I40E_AQC_ADD_CLOUD_FILTER_TEID_MPLSoUDP;
	/* Prepare the buffer, 2 entries */
	filter_replace_buf.data[0] = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG;
	filter_replace_buf.data[0] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	filter_replace_buf.data[4] = I40E_AQC_ADD_L1_FILTER_TEID_MPLS;
	filter_replace_buf.data[4] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	status = i40e_aq_replace_cloud_filters(hw, &filter_replace,
					       &filter_replace_buf);
	if (status < 0)
		return status;

	/* For MPLSoGRE */
	memset(&filter_replace, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd));
	memset(&filter_replace_buf, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd_buf));

	filter_replace.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER |
		I40E_AQC_MIRROR_CLOUD_FILTER;
	filter_replace.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_IMAC;
	filter_replace.new_filter_type =
		I40E_AQC_ADD_CLOUD_FILTER_TEID_MPLSoGRE;
	/* Prepare the buffer, 2 entries */
	filter_replace_buf.data[0] = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG;
	filter_replace_buf.data[0] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	filter_replace_buf.data[4] = I40E_AQC_ADD_L1_FILTER_TEID_MPLS;
	filter_replace_buf.data[4] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;

	status = i40e_aq_replace_cloud_filters(hw, &filter_replace,
					       &filter_replace_buf);
	return status;
}

int
i40e_dev_consistent_tunnel_filter_set(struct i40e_pf *pf,
		      struct i40e_tunnel_filter_conf *tunnel_filter,
		      uint8_t add)
{
	uint16_t ip_type;
	uint32_t ipv4_addr;
	uint8_t i, tun_type = 0;
	/* internal variable to convert ipv6 byte order */
	uint32_t convert_ipv6[4];
	int val, ret = 0;
	struct i40e_pf_vf *vf = NULL;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi;
	struct i40e_aqc_add_rm_cloud_filt_elem_ext *cld_filter;
	struct i40e_aqc_add_rm_cloud_filt_elem_ext *pfilter;
	struct i40e_tunnel_rule *tunnel_rule = &pf->tunnel;
	struct i40e_tunnel_filter *tunnel, *node;
	struct i40e_tunnel_filter check_filter; /* Check if filter exists */
	uint32_t teid_le;
	bool big_buffer = 0;

	cld_filter = rte_zmalloc("tunnel_filter",
			 sizeof(struct i40e_aqc_add_rm_cloud_filt_elem_ext),
			 0);

	if (cld_filter == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory.");
		return -ENOMEM;
	}
	pfilter = cld_filter;

	ether_addr_copy(&tunnel_filter->outer_mac,
			(struct ether_addr *)&pfilter->element.outer_mac);
	ether_addr_copy(&tunnel_filter->inner_mac,
			(struct ether_addr *)&pfilter->element.inner_mac);

	pfilter->element.inner_vlan =
		rte_cpu_to_le_16(tunnel_filter->inner_vlan);
	if (tunnel_filter->ip_type == I40E_TUNNEL_IPTYPE_IPV4) {
		ip_type = I40E_AQC_ADD_CLOUD_FLAGS_IPV4;
		ipv4_addr = rte_be_to_cpu_32(tunnel_filter->ip_addr.ipv4_addr);
		rte_memcpy(&pfilter->element.ipaddr.v4.data,
				&rte_cpu_to_le_32(ipv4_addr),
				sizeof(pfilter->element.ipaddr.v4.data));
	} else {
		ip_type = I40E_AQC_ADD_CLOUD_FLAGS_IPV6;
		for (i = 0; i < 4; i++) {
			convert_ipv6[i] =
			rte_cpu_to_le_32(rte_be_to_cpu_32(
					 tunnel_filter->ip_addr.ipv6_addr[i]));
		}
		rte_memcpy(&pfilter->element.ipaddr.v6.data,
			   &convert_ipv6,
			   sizeof(pfilter->element.ipaddr.v6.data));
	}

	/* check tunneled type */
	switch (tunnel_filter->tunnel_type) {
	case I40E_TUNNEL_TYPE_VXLAN:
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_VXLAN;
		break;
	case I40E_TUNNEL_TYPE_NVGRE:
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_NVGRE_OMAC;
		break;
	case I40E_TUNNEL_TYPE_IP_IN_GRE:
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_IP;
		break;
	case I40E_TUNNEL_TYPE_MPLSoUDP:
		if (!pf->mpls_replace_flag) {
			i40e_replace_mpls_l1_filter(pf);
			i40e_replace_mpls_cloud_filter(pf);
			pf->mpls_replace_flag = 1;
		}
		teid_le = rte_cpu_to_le_32(tunnel_filter->tenant_id);
		pfilter->general_fields[I40E_AQC_ADD_CLOUD_FV_FLU_0X11_WORD0] =
			teid_le >> 4;
		pfilter->general_fields[I40E_AQC_ADD_CLOUD_FV_FLU_0X11_WORD1] =
			(teid_le & 0xF) << 12;
		pfilter->general_fields[I40E_AQC_ADD_CLOUD_FV_FLU_0X11_WORD2] =
			0x40;
		big_buffer = 1;
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_MPLSoUDP;
		break;
	case I40E_TUNNEL_TYPE_MPLSoGRE:
		if (!pf->mpls_replace_flag) {
			i40e_replace_mpls_l1_filter(pf);
			i40e_replace_mpls_cloud_filter(pf);
			pf->mpls_replace_flag = 1;
		}
		teid_le = rte_cpu_to_le_32(tunnel_filter->tenant_id);
		pfilter->general_fields[I40E_AQC_ADD_CLOUD_FV_FLU_0X11_WORD0] =
			teid_le >> 4;
		pfilter->general_fields[I40E_AQC_ADD_CLOUD_FV_FLU_0X11_WORD1] =
			(teid_le & 0xF) << 12;
		pfilter->general_fields[I40E_AQC_ADD_CLOUD_FV_FLU_0X11_WORD2] =
			0x0;
		big_buffer = 1;
		tun_type = I40E_AQC_ADD_CLOUD_TNL_TYPE_MPLSoGRE;
		break;
	case I40E_TUNNEL_TYPE_QINQ:
		if (!pf->qinq_replace_flag) {
			ret = i40e_cloud_filter_qinq_create(pf);
			if (ret < 0)
				PMD_DRV_LOG(DEBUG,
					    "QinQ tunnel filter already created.");
			pf->qinq_replace_flag = 1;
		}
		/*	Add in the General fields the values of
		 *	the Outer and Inner VLAN
		 *	Big Buffer should be set, see changes in
		 *	i40e_aq_add_cloud_filters
		 */
		pfilter->general_fields[0] = tunnel_filter->inner_vlan;
		pfilter->general_fields[1] = tunnel_filter->outer_vlan;
		big_buffer = 1;
		break;
	default:
		/* Other tunnel types is not supported. */
		PMD_DRV_LOG(ERR, "tunnel type is not supported.");
		rte_free(cld_filter);
		return -EINVAL;
	}

	if (tunnel_filter->tunnel_type == I40E_TUNNEL_TYPE_MPLSoUDP)
		pfilter->element.flags =
			I40E_AQC_ADD_CLOUD_FILTER_TEID_MPLSoUDP;
	else if (tunnel_filter->tunnel_type == I40E_TUNNEL_TYPE_MPLSoGRE)
		pfilter->element.flags =
			I40E_AQC_ADD_CLOUD_FILTER_TEID_MPLSoGRE;
	else if (tunnel_filter->tunnel_type == I40E_TUNNEL_TYPE_QINQ)
		pfilter->element.flags |=
			I40E_AQC_ADD_CLOUD_FILTER_CUSTOM_QINQ;
	else {
		val = i40e_dev_get_filter_type(tunnel_filter->filter_type,
						&pfilter->element.flags);
		if (val < 0) {
			rte_free(cld_filter);
			return -EINVAL;
		}
	}

	pfilter->element.flags |= rte_cpu_to_le_16(
		I40E_AQC_ADD_CLOUD_FLAGS_TO_QUEUE |
		ip_type | (tun_type << I40E_AQC_ADD_CLOUD_TNL_TYPE_SHIFT));
	pfilter->element.tenant_id = rte_cpu_to_le_32(tunnel_filter->tenant_id);
	pfilter->element.queue_number =
		rte_cpu_to_le_16(tunnel_filter->queue_id);

	if (!tunnel_filter->is_to_vf)
		vsi = pf->main_vsi;
	else {
		if (tunnel_filter->vf_id >= pf->vf_num) {
			PMD_DRV_LOG(ERR, "Invalid argument.");
			return -EINVAL;
		}
		vf = &pf->vfs[tunnel_filter->vf_id];
		vsi = vf->vsi;
	}

	/* Check if there is the filter in SW list */
	memset(&check_filter, 0, sizeof(check_filter));
	i40e_tunnel_filter_convert(cld_filter, &check_filter);
	check_filter.is_to_vf = tunnel_filter->is_to_vf;
	check_filter.vf_id = tunnel_filter->vf_id;
	node = i40e_sw_tunnel_filter_lookup(tunnel_rule, &check_filter.input);
	if (add && node) {
		PMD_DRV_LOG(ERR, "Conflict with existing tunnel rules!");
		return -EINVAL;
	}

	if (!add && !node) {
		PMD_DRV_LOG(ERR, "There's no corresponding tunnel filter!");
		return -EINVAL;
	}

	if (add) {
		if (big_buffer)
			ret = i40e_aq_add_cloud_filters_big_buffer(hw,
						   vsi->seid, cld_filter, 1);
		else
			ret = i40e_aq_add_cloud_filters(hw,
					vsi->seid, &cld_filter->element, 1);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to add a tunnel filter.");
			return -ENOTSUP;
		}
		tunnel = rte_zmalloc("tunnel_filter", sizeof(*tunnel), 0);
		rte_memcpy(tunnel, &check_filter, sizeof(check_filter));
		ret = i40e_sw_tunnel_filter_insert(pf, tunnel);
	} else {
		if (big_buffer)
			ret = i40e_aq_remove_cloud_filters_big_buffer(
				hw, vsi->seid, cld_filter, 1);
		else
			ret = i40e_aq_remove_cloud_filters(hw, vsi->seid,
						   &cld_filter->element, 1);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to delete a tunnel filter.");
			return -ENOTSUP;
		}
		ret = i40e_sw_tunnel_filter_del(pf, &node->input);
	}

	rte_free(cld_filter);
	return ret;
}

static int
i40e_get_vxlan_port_idx(struct i40e_pf *pf, uint16_t port)
{
	uint8_t i;

	for (i = 0; i < I40E_MAX_PF_UDP_OFFLOAD_PORTS; i++) {
		if (pf->vxlan_ports[i] == port)
			return i;
	}

	return -1;
}

static int
i40e_add_vxlan_port(struct i40e_pf *pf, uint16_t port)
{
	int  idx, ret;
	uint8_t filter_idx;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);

	idx = i40e_get_vxlan_port_idx(pf, port);

	/* Check if port already exists */
	if (idx >= 0) {
		PMD_DRV_LOG(ERR, "Port %d already offloaded", port);
		return -EINVAL;
	}

	/* Now check if there is space to add the new port */
	idx = i40e_get_vxlan_port_idx(pf, 0);
	if (idx < 0) {
		PMD_DRV_LOG(ERR,
			"Maximum number of UDP ports reached, not adding port %d",
			port);
		return -ENOSPC;
	}

	ret =  i40e_aq_add_udp_tunnel(hw, port, I40E_AQC_TUNNEL_TYPE_VXLAN,
					&filter_idx, NULL);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to add VXLAN UDP port %d", port);
		return -1;
	}

	PMD_DRV_LOG(INFO, "Added port %d with AQ command with index %d",
			 port,  filter_idx);

	/* New port: add it and mark its index in the bitmap */
	pf->vxlan_ports[idx] = port;
	pf->vxlan_bitmap |= (1 << idx);

	if (!(pf->flags & I40E_FLAG_VXLAN))
		pf->flags |= I40E_FLAG_VXLAN;

	return 0;
}

static int
i40e_del_vxlan_port(struct i40e_pf *pf, uint16_t port)
{
	int idx;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);

	if (!(pf->flags & I40E_FLAG_VXLAN)) {
		PMD_DRV_LOG(ERR, "VXLAN UDP port was not configured.");
		return -EINVAL;
	}

	idx = i40e_get_vxlan_port_idx(pf, port);

	if (idx < 0) {
		PMD_DRV_LOG(ERR, "Port %d doesn't exist", port);
		return -EINVAL;
	}

	if (i40e_aq_del_udp_tunnel(hw, idx, NULL) < 0) {
		PMD_DRV_LOG(ERR, "Failed to delete VXLAN UDP port %d", port);
		return -1;
	}

	PMD_DRV_LOG(INFO, "Deleted port %d with AQ command with index %d",
			port, idx);

	pf->vxlan_ports[idx] = 0;
	pf->vxlan_bitmap &= ~(1 << idx);

	if (!pf->vxlan_bitmap)
		pf->flags &= ~I40E_FLAG_VXLAN;

	return 0;
}

/* Add UDP tunneling port */
static int
i40e_dev_udp_tunnel_port_add(struct rte_eth_dev *dev,
			     struct rte_eth_udp_tunnel *udp_tunnel)
{
	int ret = 0;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (udp_tunnel == NULL)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		ret = i40e_add_vxlan_port(pf, udp_tunnel->udp_port);
		break;

	case RTE_TUNNEL_TYPE_GENEVE:
	case RTE_TUNNEL_TYPE_TEREDO:
		PMD_DRV_LOG(ERR, "Tunnel type is not supported now.");
		ret = -1;
		break;

	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -1;
		break;
	}

	return ret;
}

/* Remove UDP tunneling port */
static int
i40e_dev_udp_tunnel_port_del(struct rte_eth_dev *dev,
			     struct rte_eth_udp_tunnel *udp_tunnel)
{
	int ret = 0;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (udp_tunnel == NULL)
		return -EINVAL;

	switch (udp_tunnel->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		ret = i40e_del_vxlan_port(pf, udp_tunnel->udp_port);
		break;
	case RTE_TUNNEL_TYPE_GENEVE:
	case RTE_TUNNEL_TYPE_TEREDO:
		PMD_DRV_LOG(ERR, "Tunnel type is not supported now.");
		ret = -1;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid tunnel type");
		ret = -1;
		break;
	}

	return ret;
}

/* Calculate the maximum number of contiguous PF queues that are configured */
static int
i40e_pf_calc_configured_queues_num(struct i40e_pf *pf)
{
	struct rte_eth_dev_data *data = pf->dev_data;
	int i, num;
	struct i40e_rx_queue *rxq;

	num = 0;
	for (i = 0; i < pf->lan_nb_qps; i++) {
		rxq = data->rx_queues[i];
		if (rxq && rxq->q_set)
			num++;
		else
			break;
	}

	return num;
}

/* Configure RSS */
static int
i40e_pf_config_rss(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct rte_eth_rss_conf rss_conf;
	uint32_t i, lut = 0;
	uint16_t j, num;

	/*
	 * If both VMDQ and RSS enabled, not all of PF queues are configured.
	 * It's necessary to calculate the actual PF queues that are configured.
	 */
	if (pf->dev_data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_VMDQ_FLAG)
		num = i40e_pf_calc_configured_queues_num(pf);
	else
		num = pf->dev_data->nb_rx_queues;

	num = RTE_MIN(num, I40E_MAX_Q_PER_TC);
	PMD_INIT_LOG(INFO, "Max of contiguous %u PF queues are configured",
			num);

	if (num == 0) {
		PMD_INIT_LOG(ERR, "No PF queues are configured to enable RSS");
		return -ENOTSUP;
	}

	for (i = 0, j = 0; i < hw->func_caps.rss_table_size; i++, j++) {
		if (j == num)
			j = 0;
		lut = (lut << 8) | (j & ((0x1 <<
			hw->func_caps.rss_table_entry_width) - 1));
		if ((i & 3) == 3)
			I40E_WRITE_REG(hw, I40E_PFQF_HLUT(i >> 2), lut);
	}

	rss_conf = pf->dev_data->dev_conf.rx_adv_conf.rss_conf;
	if ((rss_conf.rss_hf & I40E_RSS_OFFLOAD_ALL) == 0) {
		i40e_pf_disable_rss(pf);
		return 0;
	}
	if (rss_conf.rss_key == NULL || rss_conf.rss_key_len <
		(I40E_PFQF_HKEY_MAX_INDEX + 1) * sizeof(uint32_t)) {
		/* Random default keys */
		static uint32_t rss_key_default[] = {0x6b793944,
			0x23504cb5, 0x5bea75b6, 0x309f4f12, 0x3dc0a2b8,
			0x024ddcdf, 0x339b8ca0, 0x4c4af64a, 0x34fac605,
			0x55d85839, 0x3a58997d, 0x2ec938e1, 0x66031581};

		rss_conf.rss_key = (uint8_t *)rss_key_default;
		rss_conf.rss_key_len = (I40E_PFQF_HKEY_MAX_INDEX + 1) *
							sizeof(uint32_t);
	}

	return i40e_hw_rss_hash_set(pf, &rss_conf);
}

static int
i40e_tunnel_filter_param_check(struct i40e_pf *pf,
			       struct rte_eth_tunnel_filter_conf *filter)
{
	if (pf == NULL || filter == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameter");
		return -EINVAL;
	}

	if (filter->queue_id >= pf->dev_data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "Invalid queue ID");
		return -EINVAL;
	}

	if (filter->inner_vlan > ETHER_MAX_VLAN_ID) {
		PMD_DRV_LOG(ERR, "Invalid inner VLAN ID");
		return -EINVAL;
	}

	if ((filter->filter_type & ETH_TUNNEL_FILTER_OMAC) &&
		(is_zero_ether_addr(&filter->outer_mac))) {
		PMD_DRV_LOG(ERR, "Cannot add NULL outer MAC address");
		return -EINVAL;
	}

	if ((filter->filter_type & ETH_TUNNEL_FILTER_IMAC) &&
		(is_zero_ether_addr(&filter->inner_mac))) {
		PMD_DRV_LOG(ERR, "Cannot add NULL inner MAC address");
		return -EINVAL;
	}

	return 0;
}

#define I40E_GL_PRS_FVBM_MSK_ENA 0x80000000
#define I40E_GL_PRS_FVBM(_i)     (0x00269760 + ((_i) * 4))
static int
i40e_dev_set_gre_key_len(struct i40e_hw *hw, uint8_t len)
{
	uint32_t val, reg;
	int ret = -EINVAL;

	val = I40E_READ_REG(hw, I40E_GL_PRS_FVBM(2));
	PMD_DRV_LOG(DEBUG, "Read original GL_PRS_FVBM with 0x%08x", val);

	if (len == 3) {
		reg = val | I40E_GL_PRS_FVBM_MSK_ENA;
	} else if (len == 4) {
		reg = val & ~I40E_GL_PRS_FVBM_MSK_ENA;
	} else {
		PMD_DRV_LOG(ERR, "Unsupported GRE key length of %u", len);
		return ret;
	}

	if (reg != val) {
		ret = i40e_aq_debug_write_register(hw, I40E_GL_PRS_FVBM(2),
						   reg, NULL);
		if (ret != 0)
			return ret;
	} else {
		ret = 0;
	}
	PMD_DRV_LOG(DEBUG, "Read modified GL_PRS_FVBM with 0x%08x",
		    I40E_READ_REG(hw, I40E_GL_PRS_FVBM(2)));

	return ret;
}

static int
i40e_dev_global_config_set(struct i40e_hw *hw, struct rte_eth_global_cfg *cfg)
{
	int ret = -EINVAL;

	if (!hw || !cfg)
		return -EINVAL;

	switch (cfg->cfg_type) {
	case RTE_ETH_GLOBAL_CFG_TYPE_GRE_KEY_LEN:
		ret = i40e_dev_set_gre_key_len(hw, cfg->cfg.gre_key_len);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown config type %u", cfg->cfg_type);
		break;
	}

	return ret;
}

static int
i40e_filter_ctrl_global_config(struct rte_eth_dev *dev,
			       enum rte_filter_op filter_op,
			       void *arg)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret = I40E_ERR_PARAM;

	switch (filter_op) {
	case RTE_ETH_FILTER_SET:
		ret = i40e_dev_global_config_set(hw,
			(struct rte_eth_global_cfg *)arg);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown operation %u", filter_op);
		break;
	}

	return ret;
}

static int
i40e_tunnel_filter_handle(struct rte_eth_dev *dev,
			  enum rte_filter_op filter_op,
			  void *arg)
{
	struct rte_eth_tunnel_filter_conf *filter;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret = I40E_SUCCESS;

	filter = (struct rte_eth_tunnel_filter_conf *)(arg);

	if (i40e_tunnel_filter_param_check(pf, filter) < 0)
		return I40E_ERR_PARAM;

	switch (filter_op) {
	case RTE_ETH_FILTER_NOP:
		if (!(pf->flags & I40E_FLAG_VXLAN))
			ret = I40E_NOT_SUPPORTED;
		break;
	case RTE_ETH_FILTER_ADD:
		ret = i40e_dev_tunnel_filter_set(pf, filter, 1);
		break;
	case RTE_ETH_FILTER_DELETE:
		ret = i40e_dev_tunnel_filter_set(pf, filter, 0);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown operation %u", filter_op);
		ret = I40E_ERR_PARAM;
		break;
	}

	return ret;
}

static int
i40e_pf_config_mq_rx(struct i40e_pf *pf)
{
	int ret = 0;
	enum rte_eth_rx_mq_mode mq_mode = pf->dev_data->dev_conf.rxmode.mq_mode;

	/* RSS setup */
	if (mq_mode & ETH_MQ_RX_RSS_FLAG)
		ret = i40e_pf_config_rss(pf);
	else
		i40e_pf_disable_rss(pf);

	return ret;
}

/* Get the symmetric hash enable configurations per port */
static void
i40e_get_symmetric_hash_enable_per_port(struct i40e_hw *hw, uint8_t *enable)
{
	uint32_t reg = i40e_read_rx_ctl(hw, I40E_PRTQF_CTL_0);

	*enable = reg & I40E_PRTQF_CTL_0_HSYM_ENA_MASK ? 1 : 0;
}

/* Set the symmetric hash enable configurations per port */
static void
i40e_set_symmetric_hash_enable_per_port(struct i40e_hw *hw, uint8_t enable)
{
	uint32_t reg = i40e_read_rx_ctl(hw, I40E_PRTQF_CTL_0);

	if (enable > 0) {
		if (reg & I40E_PRTQF_CTL_0_HSYM_ENA_MASK) {
			PMD_DRV_LOG(INFO,
				"Symmetric hash has already been enabled");
			return;
		}
		reg |= I40E_PRTQF_CTL_0_HSYM_ENA_MASK;
	} else {
		if (!(reg & I40E_PRTQF_CTL_0_HSYM_ENA_MASK)) {
			PMD_DRV_LOG(INFO,
				"Symmetric hash has already been disabled");
			return;
		}
		reg &= ~I40E_PRTQF_CTL_0_HSYM_ENA_MASK;
	}
	i40e_write_rx_ctl(hw, I40E_PRTQF_CTL_0, reg);
	I40E_WRITE_FLUSH(hw);
}

/*
 * Get global configurations of hash function type and symmetric hash enable
 * per flow type (pctype). Note that global configuration means it affects all
 * the ports on the same NIC.
 */
static int
i40e_get_hash_filter_global_config(struct i40e_hw *hw,
				   struct rte_eth_hash_global_conf *g_cfg)
{
	uint32_t reg, mask = I40E_FLOW_TYPES;
	uint16_t i;
	enum i40e_filter_pctype pctype;

	memset(g_cfg, 0, sizeof(*g_cfg));
	reg = i40e_read_rx_ctl(hw, I40E_GLQF_CTL);
	if (reg & I40E_GLQF_CTL_HTOEP_MASK)
		g_cfg->hash_func = RTE_ETH_HASH_FUNCTION_TOEPLITZ;
	else
		g_cfg->hash_func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
	PMD_DRV_LOG(DEBUG, "Hash function is %s",
		(reg & I40E_GLQF_CTL_HTOEP_MASK) ? "Toeplitz" : "Simple XOR");

	for (i = 0; mask && i < RTE_ETH_FLOW_MAX; i++) {
		if (!(mask & (1UL << i)))
			continue;
		mask &= ~(1UL << i);
		/* Bit set indicats the coresponding flow type is supported */
		g_cfg->valid_bit_mask[0] |= (1UL << i);
		/* if flowtype is invalid, continue */
		if (!I40E_VALID_FLOW(i))
			continue;
		pctype = i40e_flowtype_to_pctype(i);
		reg = i40e_read_rx_ctl(hw, I40E_GLQF_HSYM(pctype));
		if (reg & I40E_GLQF_HSYM_SYMH_ENA_MASK)
			g_cfg->sym_hash_enable_mask[0] |= (1UL << i);
	}

	return 0;
}

static int
i40e_hash_global_config_check(struct rte_eth_hash_global_conf *g_cfg)
{
	uint32_t i;
	uint32_t mask0, i40e_mask = I40E_FLOW_TYPES;

	if (g_cfg->hash_func != RTE_ETH_HASH_FUNCTION_TOEPLITZ &&
		g_cfg->hash_func != RTE_ETH_HASH_FUNCTION_SIMPLE_XOR &&
		g_cfg->hash_func != RTE_ETH_HASH_FUNCTION_DEFAULT) {
		PMD_DRV_LOG(ERR, "Unsupported hash function type %d",
						g_cfg->hash_func);
		return -EINVAL;
	}

	/*
	 * As i40e supports less than 32 flow types, only first 32 bits need to
	 * be checked.
	 */
	mask0 = g_cfg->valid_bit_mask[0];
	for (i = 0; i < RTE_SYM_HASH_MASK_ARRAY_SIZE; i++) {
		if (i == 0) {
			/* Check if any unsupported flow type configured */
			if ((mask0 | i40e_mask) ^ i40e_mask)
				goto mask_err;
		} else {
			if (g_cfg->valid_bit_mask[i])
				goto mask_err;
		}
	}

	return 0;

mask_err:
	PMD_DRV_LOG(ERR, "i40e unsupported flow type bit(s) configured");

	return -EINVAL;
}

/*
 * Set global configurations of hash function type and symmetric hash enable
 * per flow type (pctype). Note any modifying global configuration will affect
 * all the ports on the same NIC.
 */
static int
i40e_set_hash_filter_global_config(struct i40e_hw *hw,
				   struct rte_eth_hash_global_conf *g_cfg)
{
	int ret;
	uint16_t i;
	uint32_t reg;
	uint32_t mask0 = g_cfg->valid_bit_mask[0];
	enum i40e_filter_pctype pctype;

	/* Check the input parameters */
	ret = i40e_hash_global_config_check(g_cfg);
	if (ret < 0)
		return ret;

	for (i = 0; mask0 && i < UINT32_BIT; i++) {
		if (!(mask0 & (1UL << i)))
			continue;
		mask0 &= ~(1UL << i);
		/* if flowtype is invalid, continue */
		if (!I40E_VALID_FLOW(i))
			continue;
		pctype = i40e_flowtype_to_pctype(i);
		reg = (g_cfg->sym_hash_enable_mask[0] & (1UL << i)) ?
				I40E_GLQF_HSYM_SYMH_ENA_MASK : 0;
		if (hw->mac.type == I40E_MAC_X722) {
			if (pctype == I40E_FILTER_PCTYPE_NONF_IPV4_UDP) {
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_IPV4_UDP), reg);
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP),
				  reg);
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP),
				  reg);
			} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV4_TCP) {
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_IPV4_TCP), reg);
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK),
				  reg);
			} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV6_UDP) {
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_IPV6_UDP), reg);
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP),
				  reg);
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP),
				  reg);
			} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV6_TCP) {
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_IPV6_TCP), reg);
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(
				  I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK),
				  reg);
			} else {
				i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(pctype),
				  reg);
			}
		} else {
			i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(pctype), reg);
		}
	}

	reg = i40e_read_rx_ctl(hw, I40E_GLQF_CTL);
	if (g_cfg->hash_func == RTE_ETH_HASH_FUNCTION_TOEPLITZ) {
		/* Toeplitz */
		if (reg & I40E_GLQF_CTL_HTOEP_MASK) {
			PMD_DRV_LOG(DEBUG,
				"Hash function already set to Toeplitz");
			goto out;
		}
		reg |= I40E_GLQF_CTL_HTOEP_MASK;
	} else if (g_cfg->hash_func == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
		/* Simple XOR */
		if (!(reg & I40E_GLQF_CTL_HTOEP_MASK)) {
			PMD_DRV_LOG(DEBUG,
				"Hash function already set to Simple XOR");
			goto out;
		}
		reg &= ~I40E_GLQF_CTL_HTOEP_MASK;
	} else
		/* Use the default, and keep it as it is */
		goto out;

	i40e_write_rx_ctl(hw, I40E_GLQF_CTL, reg);

out:
	I40E_WRITE_FLUSH(hw);

	return 0;
}

/**
 * Valid input sets for hash and flow director filters per PCTYPE
 */
static uint64_t
i40e_get_valid_input_set(enum i40e_filter_pctype pctype,
		enum rte_filter_type filter)
{
	uint64_t valid;

	static const uint64_t valid_hash_inset_table[] = {
		[I40E_FILTER_PCTYPE_FRAG_IPV4] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV4_SRC |
			I40E_INSET_IPV4_DST | I40E_INSET_IPV4_TOS |
			I40E_INSET_IPV4_PROTO | I40E_INSET_IPV4_TTL |
			I40E_INSET_TUNNEL_DMAC | I40E_INSET_TUNNEL_ID |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV4_UDP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV4_TOS |
			I40E_INSET_IPV4_PROTO | I40E_INSET_IPV4_TTL |
			I40E_INSET_TUNNEL_DMAC | I40E_INSET_TUNNEL_ID |
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV4_TOS |
			I40E_INSET_IPV4_PROTO | I40E_INSET_IPV4_TTL |
			I40E_INSET_TUNNEL_DMAC | I40E_INSET_TUNNEL_ID |
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV4_TOS |
			I40E_INSET_IPV4_PROTO | I40E_INSET_IPV4_TTL |
			I40E_INSET_TUNNEL_DMAC | I40E_INSET_TUNNEL_ID |
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV4_TOS |
			I40E_INSET_IPV4_PROTO | I40E_INSET_IPV4_TTL |
			I40E_INSET_TUNNEL_DMAC | I40E_INSET_TUNNEL_ID |
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
			I40E_INSET_TCP_FLAGS | I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV4_TOS |
			I40E_INSET_IPV4_PROTO | I40E_INSET_IPV4_TTL |
			I40E_INSET_TUNNEL_DMAC | I40E_INSET_TUNNEL_ID |
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
			I40E_INSET_TCP_FLAGS | I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV4_SCTP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV4_TOS |
			I40E_INSET_IPV4_PROTO | I40E_INSET_IPV4_TTL |
			I40E_INSET_TUNNEL_DMAC | I40E_INSET_TUNNEL_ID |
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
			I40E_INSET_SCTP_VT | I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV4_OTHER] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV4_TOS |
			I40E_INSET_IPV4_PROTO | I40E_INSET_IPV4_TTL |
			I40E_INSET_TUNNEL_DMAC | I40E_INSET_TUNNEL_ID |
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_FRAG_IPV6] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV6_TC |
			I40E_INSET_IPV6_FLOW | I40E_INSET_IPV6_NEXT_HDR |
			I40E_INSET_IPV6_HOP_LIMIT | I40E_INSET_TUNNEL_DMAC |
			I40E_INSET_TUNNEL_ID | I40E_INSET_IPV6_SRC |
			I40E_INSET_IPV6_DST | I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV6_UDP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV6_TC |
			I40E_INSET_IPV6_FLOW | I40E_INSET_IPV6_NEXT_HDR |
			I40E_INSET_IPV6_HOP_LIMIT | I40E_INSET_IPV6_SRC |
			I40E_INSET_IPV6_DST | I40E_INSET_SRC_PORT |
			I40E_INSET_DST_PORT | I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV6_TC |
			I40E_INSET_IPV6_FLOW | I40E_INSET_IPV6_NEXT_HDR |
			I40E_INSET_IPV6_HOP_LIMIT | I40E_INSET_IPV6_SRC |
			I40E_INSET_IPV6_DST | I40E_INSET_SRC_PORT |
			I40E_INSET_DST_PORT | I40E_INSET_TCP_FLAGS |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV6_TC |
			I40E_INSET_IPV6_FLOW | I40E_INSET_IPV6_NEXT_HDR |
			I40E_INSET_IPV6_HOP_LIMIT | I40E_INSET_IPV6_SRC |
			I40E_INSET_IPV6_DST | I40E_INSET_SRC_PORT |
			I40E_INSET_DST_PORT | I40E_INSET_TCP_FLAGS |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV6_TC |
			I40E_INSET_IPV6_FLOW | I40E_INSET_IPV6_NEXT_HDR |
			I40E_INSET_IPV6_HOP_LIMIT | I40E_INSET_IPV6_SRC |
			I40E_INSET_IPV6_DST | I40E_INSET_SRC_PORT |
			I40E_INSET_DST_PORT | I40E_INSET_TCP_FLAGS |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV6_TC |
			I40E_INSET_IPV6_FLOW | I40E_INSET_IPV6_NEXT_HDR |
			I40E_INSET_IPV6_HOP_LIMIT | I40E_INSET_IPV6_SRC |
			I40E_INSET_IPV6_DST | I40E_INSET_SRC_PORT |
			I40E_INSET_DST_PORT | I40E_INSET_TCP_FLAGS |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV6_SCTP] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV6_TC |
			I40E_INSET_IPV6_FLOW | I40E_INSET_IPV6_NEXT_HDR |
			I40E_INSET_IPV6_HOP_LIMIT | I40E_INSET_IPV6_SRC |
			I40E_INSET_IPV6_DST | I40E_INSET_SRC_PORT |
			I40E_INSET_DST_PORT | I40E_INSET_SCTP_VT |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_NONF_IPV6_OTHER] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_IPV6_TC |
			I40E_INSET_IPV6_FLOW | I40E_INSET_IPV6_NEXT_HDR |
			I40E_INSET_IPV6_HOP_LIMIT | I40E_INSET_IPV6_SRC |
			I40E_INSET_IPV6_DST | I40E_INSET_TUNNEL_ID |
			I40E_INSET_FLEX_PAYLOAD,
		[I40E_FILTER_PCTYPE_L2_PAYLOAD] =
			I40E_INSET_DMAC | I40E_INSET_SMAC |
			I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
			I40E_INSET_VLAN_TUNNEL | I40E_INSET_LAST_ETHER_TYPE |
			I40E_INSET_FLEX_PAYLOAD,
	};

	/**
	 * Flow director supports only fields defined in
	 * union rte_eth_fdir_flow.
	 */
	static const uint64_t valid_fdir_inset_table[] = {
		[I40E_FILTER_PCTYPE_FRAG_IPV4] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
		I40E_INSET_IPV4_TOS | I40E_INSET_IPV4_PROTO |
		I40E_INSET_IPV4_TTL,
		[I40E_FILTER_PCTYPE_NONF_IPV4_UDP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
		I40E_INSET_IPV4_TOS | I40E_INSET_IPV4_TTL |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
		I40E_INSET_IPV4_TOS | I40E_INSET_IPV4_TTL |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
		I40E_INSET_IPV4_TOS | I40E_INSET_IPV4_TTL |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
		I40E_INSET_IPV4_TOS | I40E_INSET_IPV4_TTL |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
		I40E_INSET_IPV4_TOS | I40E_INSET_IPV4_TTL |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV4_SCTP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
		I40E_INSET_IPV4_TOS | I40E_INSET_IPV4_TTL |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
		I40E_INSET_SCTP_VT,
		[I40E_FILTER_PCTYPE_NONF_IPV4_OTHER] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
		I40E_INSET_IPV4_TOS | I40E_INSET_IPV4_PROTO |
		I40E_INSET_IPV4_TTL,
		[I40E_FILTER_PCTYPE_FRAG_IPV6] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
		I40E_INSET_IPV6_TC | I40E_INSET_IPV6_NEXT_HDR |
		I40E_INSET_IPV6_HOP_LIMIT,
		[I40E_FILTER_PCTYPE_NONF_IPV6_UDP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
		I40E_INSET_IPV6_TC | I40E_INSET_IPV6_HOP_LIMIT |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
		I40E_INSET_IPV6_TC | I40E_INSET_IPV6_HOP_LIMIT |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
		I40E_INSET_IPV6_TC | I40E_INSET_IPV6_HOP_LIMIT |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
		I40E_INSET_IPV6_TC | I40E_INSET_IPV6_HOP_LIMIT |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
		I40E_INSET_IPV6_TC | I40E_INSET_IPV6_HOP_LIMIT |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV6_SCTP] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
		I40E_INSET_IPV6_TC | I40E_INSET_IPV6_HOP_LIMIT |
		I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
		I40E_INSET_SCTP_VT,
		[I40E_FILTER_PCTYPE_NONF_IPV6_OTHER] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
		I40E_INSET_IPV6_TC | I40E_INSET_IPV6_NEXT_HDR |
		I40E_INSET_IPV6_HOP_LIMIT,
		[I40E_FILTER_PCTYPE_L2_PAYLOAD] =
		I40E_INSET_VLAN_OUTER | I40E_INSET_VLAN_INNER |
		I40E_INSET_LAST_ETHER_TYPE,
	};

	if (pctype > I40E_FILTER_PCTYPE_L2_PAYLOAD)
		return 0;
	if (filter == RTE_ETH_FILTER_HASH)
		valid = valid_hash_inset_table[pctype];
	else
		valid = valid_fdir_inset_table[pctype];

	return valid;
}

/**
 * Validate if the input set is allowed for a specific PCTYPE
 */
int
i40e_validate_input_set(enum i40e_filter_pctype pctype,
		enum rte_filter_type filter, uint64_t inset)
{
	uint64_t valid;

	valid = i40e_get_valid_input_set(pctype, filter);
	if (inset & (~valid))
		return -EINVAL;

	return 0;
}

/* default input set fields combination per pctype */
uint64_t
i40e_get_default_input_set(uint16_t pctype)
{
	static const uint64_t default_inset_table[] = {
		[I40E_FILTER_PCTYPE_FRAG_IPV4] =
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST,
		[I40E_FILTER_PCTYPE_NONF_IPV4_UDP] =
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP] =
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP] =
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP] =
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK] =
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV4_SCTP] =
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
			I40E_INSET_SCTP_VT,
		[I40E_FILTER_PCTYPE_NONF_IPV4_OTHER] =
			I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST,
		[I40E_FILTER_PCTYPE_FRAG_IPV6] =
			I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST,
		[I40E_FILTER_PCTYPE_NONF_IPV6_UDP] =
			I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP] =
			I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP] =
			I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP] =
			I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK] =
			I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT,
		[I40E_FILTER_PCTYPE_NONF_IPV6_SCTP] =
			I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
			I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT |
			I40E_INSET_SCTP_VT,
		[I40E_FILTER_PCTYPE_NONF_IPV6_OTHER] =
			I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST,
		[I40E_FILTER_PCTYPE_L2_PAYLOAD] =
			I40E_INSET_LAST_ETHER_TYPE,
	};

	if (pctype > I40E_FILTER_PCTYPE_L2_PAYLOAD)
		return 0;

	return default_inset_table[pctype];
}

/**
 * Parse the input set from index to logical bit masks
 */
static int
i40e_parse_input_set(uint64_t *inset,
		     enum i40e_filter_pctype pctype,
		     enum rte_eth_input_set_field *field,
		     uint16_t size)
{
	uint16_t i, j;
	int ret = -EINVAL;

	static const struct {
		enum rte_eth_input_set_field field;
		uint64_t inset;
	} inset_convert_table[] = {
		{RTE_ETH_INPUT_SET_NONE, I40E_INSET_NONE},
		{RTE_ETH_INPUT_SET_L2_SRC_MAC, I40E_INSET_SMAC},
		{RTE_ETH_INPUT_SET_L2_DST_MAC, I40E_INSET_DMAC},
		{RTE_ETH_INPUT_SET_L2_OUTER_VLAN, I40E_INSET_VLAN_OUTER},
		{RTE_ETH_INPUT_SET_L2_INNER_VLAN, I40E_INSET_VLAN_INNER},
		{RTE_ETH_INPUT_SET_L2_ETHERTYPE, I40E_INSET_LAST_ETHER_TYPE},
		{RTE_ETH_INPUT_SET_L3_SRC_IP4, I40E_INSET_IPV4_SRC},
		{RTE_ETH_INPUT_SET_L3_DST_IP4, I40E_INSET_IPV4_DST},
		{RTE_ETH_INPUT_SET_L3_IP4_TOS, I40E_INSET_IPV4_TOS},
		{RTE_ETH_INPUT_SET_L3_IP4_PROTO, I40E_INSET_IPV4_PROTO},
		{RTE_ETH_INPUT_SET_L3_IP4_TTL, I40E_INSET_IPV4_TTL},
		{RTE_ETH_INPUT_SET_L3_SRC_IP6, I40E_INSET_IPV6_SRC},
		{RTE_ETH_INPUT_SET_L3_DST_IP6, I40E_INSET_IPV6_DST},
		{RTE_ETH_INPUT_SET_L3_IP6_TC, I40E_INSET_IPV6_TC},
		{RTE_ETH_INPUT_SET_L3_IP6_NEXT_HEADER,
			I40E_INSET_IPV6_NEXT_HDR},
		{RTE_ETH_INPUT_SET_L3_IP6_HOP_LIMITS,
			I40E_INSET_IPV6_HOP_LIMIT},
		{RTE_ETH_INPUT_SET_L4_UDP_SRC_PORT, I40E_INSET_SRC_PORT},
		{RTE_ETH_INPUT_SET_L4_TCP_SRC_PORT, I40E_INSET_SRC_PORT},
		{RTE_ETH_INPUT_SET_L4_SCTP_SRC_PORT, I40E_INSET_SRC_PORT},
		{RTE_ETH_INPUT_SET_L4_UDP_DST_PORT, I40E_INSET_DST_PORT},
		{RTE_ETH_INPUT_SET_L4_TCP_DST_PORT, I40E_INSET_DST_PORT},
		{RTE_ETH_INPUT_SET_L4_SCTP_DST_PORT, I40E_INSET_DST_PORT},
		{RTE_ETH_INPUT_SET_L4_SCTP_VERIFICATION_TAG,
			I40E_INSET_SCTP_VT},
		{RTE_ETH_INPUT_SET_TUNNEL_L2_INNER_DST_MAC,
			I40E_INSET_TUNNEL_DMAC},
		{RTE_ETH_INPUT_SET_TUNNEL_L2_INNER_VLAN,
			I40E_INSET_VLAN_TUNNEL},
		{RTE_ETH_INPUT_SET_TUNNEL_L4_UDP_KEY,
			I40E_INSET_TUNNEL_ID},
		{RTE_ETH_INPUT_SET_TUNNEL_GRE_KEY, I40E_INSET_TUNNEL_ID},
		{RTE_ETH_INPUT_SET_FLEX_PAYLOAD_1ST_WORD,
			I40E_INSET_FLEX_PAYLOAD_W1},
		{RTE_ETH_INPUT_SET_FLEX_PAYLOAD_2ND_WORD,
			I40E_INSET_FLEX_PAYLOAD_W2},
		{RTE_ETH_INPUT_SET_FLEX_PAYLOAD_3RD_WORD,
			I40E_INSET_FLEX_PAYLOAD_W3},
		{RTE_ETH_INPUT_SET_FLEX_PAYLOAD_4TH_WORD,
			I40E_INSET_FLEX_PAYLOAD_W4},
		{RTE_ETH_INPUT_SET_FLEX_PAYLOAD_5TH_WORD,
			I40E_INSET_FLEX_PAYLOAD_W5},
		{RTE_ETH_INPUT_SET_FLEX_PAYLOAD_6TH_WORD,
			I40E_INSET_FLEX_PAYLOAD_W6},
		{RTE_ETH_INPUT_SET_FLEX_PAYLOAD_7TH_WORD,
			I40E_INSET_FLEX_PAYLOAD_W7},
		{RTE_ETH_INPUT_SET_FLEX_PAYLOAD_8TH_WORD,
			I40E_INSET_FLEX_PAYLOAD_W8},
	};

	if (!inset || !field || size > RTE_ETH_INSET_SIZE_MAX)
		return ret;

	/* Only one item allowed for default or all */
	if (size == 1) {
		if (field[0] == RTE_ETH_INPUT_SET_DEFAULT) {
			*inset = i40e_get_default_input_set(pctype);
			return 0;
		} else if (field[0] == RTE_ETH_INPUT_SET_NONE) {
			*inset = I40E_INSET_NONE;
			return 0;
		}
	}

	for (i = 0, *inset = 0; i < size; i++) {
		for (j = 0; j < RTE_DIM(inset_convert_table); j++) {
			if (field[i] == inset_convert_table[j].field) {
				*inset |= inset_convert_table[j].inset;
				break;
			}
		}

		/* It contains unsupported input set, return immediately */
		if (j == RTE_DIM(inset_convert_table))
			return ret;
	}

	return 0;
}

/**
 * Translate the input set from bit masks to register aware bit masks
 * and vice versa
 */
uint64_t
i40e_translate_input_set_reg(enum i40e_mac_type type, uint64_t input)
{
	uint64_t val = 0;
	uint16_t i;

	struct inset_map {
		uint64_t inset;
		uint64_t inset_reg;
	};

	static const struct inset_map inset_map_common[] = {
		{I40E_INSET_DMAC, I40E_REG_INSET_L2_DMAC},
		{I40E_INSET_SMAC, I40E_REG_INSET_L2_SMAC},
		{I40E_INSET_VLAN_OUTER, I40E_REG_INSET_L2_OUTER_VLAN},
		{I40E_INSET_VLAN_INNER, I40E_REG_INSET_L2_INNER_VLAN},
		{I40E_INSET_LAST_ETHER_TYPE, I40E_REG_INSET_LAST_ETHER_TYPE},
		{I40E_INSET_IPV4_TOS, I40E_REG_INSET_L3_IP4_TOS},
		{I40E_INSET_IPV6_SRC, I40E_REG_INSET_L3_SRC_IP6},
		{I40E_INSET_IPV6_DST, I40E_REG_INSET_L3_DST_IP6},
		{I40E_INSET_IPV6_TC, I40E_REG_INSET_L3_IP6_TC},
		{I40E_INSET_IPV6_NEXT_HDR, I40E_REG_INSET_L3_IP6_NEXT_HDR},
		{I40E_INSET_IPV6_HOP_LIMIT, I40E_REG_INSET_L3_IP6_HOP_LIMIT},
		{I40E_INSET_SRC_PORT, I40E_REG_INSET_L4_SRC_PORT},
		{I40E_INSET_DST_PORT, I40E_REG_INSET_L4_DST_PORT},
		{I40E_INSET_SCTP_VT, I40E_REG_INSET_L4_SCTP_VERIFICATION_TAG},
		{I40E_INSET_TUNNEL_ID, I40E_REG_INSET_TUNNEL_ID},
		{I40E_INSET_TUNNEL_DMAC,
			I40E_REG_INSET_TUNNEL_L2_INNER_DST_MAC},
		{I40E_INSET_TUNNEL_IPV4_DST, I40E_REG_INSET_TUNNEL_L3_DST_IP4},
		{I40E_INSET_TUNNEL_IPV6_DST, I40E_REG_INSET_TUNNEL_L3_DST_IP6},
		{I40E_INSET_TUNNEL_SRC_PORT,
			I40E_REG_INSET_TUNNEL_L4_UDP_SRC_PORT},
		{I40E_INSET_TUNNEL_DST_PORT,
			I40E_REG_INSET_TUNNEL_L4_UDP_DST_PORT},
		{I40E_INSET_VLAN_TUNNEL, I40E_REG_INSET_TUNNEL_VLAN},
		{I40E_INSET_FLEX_PAYLOAD_W1, I40E_REG_INSET_FLEX_PAYLOAD_WORD1},
		{I40E_INSET_FLEX_PAYLOAD_W2, I40E_REG_INSET_FLEX_PAYLOAD_WORD2},
		{I40E_INSET_FLEX_PAYLOAD_W3, I40E_REG_INSET_FLEX_PAYLOAD_WORD3},
		{I40E_INSET_FLEX_PAYLOAD_W4, I40E_REG_INSET_FLEX_PAYLOAD_WORD4},
		{I40E_INSET_FLEX_PAYLOAD_W5, I40E_REG_INSET_FLEX_PAYLOAD_WORD5},
		{I40E_INSET_FLEX_PAYLOAD_W6, I40E_REG_INSET_FLEX_PAYLOAD_WORD6},
		{I40E_INSET_FLEX_PAYLOAD_W7, I40E_REG_INSET_FLEX_PAYLOAD_WORD7},
		{I40E_INSET_FLEX_PAYLOAD_W8, I40E_REG_INSET_FLEX_PAYLOAD_WORD8},
	};

    /* some different registers map in x722*/
	static const struct inset_map inset_map_diff_x722[] = {
		{I40E_INSET_IPV4_SRC, I40E_X722_REG_INSET_L3_SRC_IP4},
		{I40E_INSET_IPV4_DST, I40E_X722_REG_INSET_L3_DST_IP4},
		{I40E_INSET_IPV4_PROTO, I40E_X722_REG_INSET_L3_IP4_PROTO},
		{I40E_INSET_IPV4_TTL, I40E_X722_REG_INSET_L3_IP4_TTL},
	};

	static const struct inset_map inset_map_diff_not_x722[] = {
		{I40E_INSET_IPV4_SRC, I40E_REG_INSET_L3_SRC_IP4},
		{I40E_INSET_IPV4_DST, I40E_REG_INSET_L3_DST_IP4},
		{I40E_INSET_IPV4_PROTO, I40E_REG_INSET_L3_IP4_PROTO},
		{I40E_INSET_IPV4_TTL, I40E_REG_INSET_L3_IP4_TTL},
	};

	if (input == 0)
		return val;

	/* Translate input set to register aware inset */
	if (type == I40E_MAC_X722) {
		for (i = 0; i < RTE_DIM(inset_map_diff_x722); i++) {
			if (input & inset_map_diff_x722[i].inset)
				val |= inset_map_diff_x722[i].inset_reg;
		}
	} else {
		for (i = 0; i < RTE_DIM(inset_map_diff_not_x722); i++) {
			if (input & inset_map_diff_not_x722[i].inset)
				val |= inset_map_diff_not_x722[i].inset_reg;
		}
	}

	for (i = 0; i < RTE_DIM(inset_map_common); i++) {
		if (input & inset_map_common[i].inset)
			val |= inset_map_common[i].inset_reg;
	}

	return val;
}

int
i40e_generate_inset_mask_reg(uint64_t inset, uint32_t *mask, uint8_t nb_elem)
{
	uint8_t i, idx = 0;
	uint64_t inset_need_mask = inset;

	static const struct {
		uint64_t inset;
		uint32_t mask;
	} inset_mask_map[] = {
		{I40E_INSET_IPV4_TOS, I40E_INSET_IPV4_TOS_MASK},
		{I40E_INSET_IPV4_PROTO | I40E_INSET_IPV4_TTL, 0},
		{I40E_INSET_IPV4_PROTO, I40E_INSET_IPV4_PROTO_MASK},
		{I40E_INSET_IPV4_TTL, I40E_INSET_IPv4_TTL_MASK},
		{I40E_INSET_IPV6_TC, I40E_INSET_IPV6_TC_MASK},
		{I40E_INSET_IPV6_NEXT_HDR | I40E_INSET_IPV6_HOP_LIMIT, 0},
		{I40E_INSET_IPV6_NEXT_HDR, I40E_INSET_IPV6_NEXT_HDR_MASK},
		{I40E_INSET_IPV6_HOP_LIMIT, I40E_INSET_IPV6_HOP_LIMIT_MASK},
	};

	if (!inset || !mask || !nb_elem)
		return 0;

	for (i = 0, idx = 0; i < RTE_DIM(inset_mask_map); i++) {
		/* Clear the inset bit, if no MASK is required,
		 * for example proto + ttl
		 */
		if ((inset & inset_mask_map[i].inset) ==
		     inset_mask_map[i].inset && inset_mask_map[i].mask == 0)
			inset_need_mask &= ~inset_mask_map[i].inset;
		if (!inset_need_mask)
			return 0;
	}
	for (i = 0, idx = 0; i < RTE_DIM(inset_mask_map); i++) {
		if ((inset_need_mask & inset_mask_map[i].inset) ==
		    inset_mask_map[i].inset) {
			if (idx >= nb_elem) {
				PMD_DRV_LOG(ERR, "exceed maximal number of bitmasks");
				return -EINVAL;
			}
			mask[idx] = inset_mask_map[i].mask;
			idx++;
		}
	}

	return idx;
}

void
i40e_check_write_reg(struct i40e_hw *hw, uint32_t addr, uint32_t val)
{
	uint32_t reg = i40e_read_rx_ctl(hw, addr);

	PMD_DRV_LOG(DEBUG, "[0x%08x] original: 0x%08x", addr, reg);
	if (reg != val)
		i40e_write_rx_ctl(hw, addr, val);
	PMD_DRV_LOG(DEBUG, "[0x%08x] after: 0x%08x", addr,
		    (uint32_t)i40e_read_rx_ctl(hw, addr));
}

static void
i40e_filter_input_set_init(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	enum i40e_filter_pctype pctype;
	uint64_t input_set, inset_reg;
	uint32_t mask_reg[I40E_INSET_MASK_NUM_REG] = {0};
	int num, i;

	for (pctype = I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
	     pctype <= I40E_FILTER_PCTYPE_L2_PAYLOAD; pctype++) {
		if (hw->mac.type == I40E_MAC_X722) {
			if (!I40E_VALID_PCTYPE_X722(pctype))
				continue;
		} else {
			if (!I40E_VALID_PCTYPE(pctype))
				continue;
		}

		input_set = i40e_get_default_input_set(pctype);

		num = i40e_generate_inset_mask_reg(input_set, mask_reg,
						   I40E_INSET_MASK_NUM_REG);
		if (num < 0)
			return;
		inset_reg = i40e_translate_input_set_reg(hw->mac.type,
					input_set);

		i40e_check_write_reg(hw, I40E_PRTQF_FD_INSET(pctype, 0),
				      (uint32_t)(inset_reg & UINT32_MAX));
		i40e_check_write_reg(hw, I40E_PRTQF_FD_INSET(pctype, 1),
				     (uint32_t)((inset_reg >>
				     I40E_32_BIT_WIDTH) & UINT32_MAX));
		i40e_check_write_reg(hw, I40E_GLQF_HASH_INSET(0, pctype),
				      (uint32_t)(inset_reg & UINT32_MAX));
		i40e_check_write_reg(hw, I40E_GLQF_HASH_INSET(1, pctype),
				     (uint32_t)((inset_reg >>
				     I40E_32_BIT_WIDTH) & UINT32_MAX));

		for (i = 0; i < num; i++) {
			i40e_check_write_reg(hw, I40E_GLQF_FD_MSK(i, pctype),
					     mask_reg[i]);
			i40e_check_write_reg(hw, I40E_GLQF_HASH_MSK(i, pctype),
					     mask_reg[i]);
		}
		/*clear unused mask registers of the pctype */
		for (i = num; i < I40E_INSET_MASK_NUM_REG; i++) {
			i40e_check_write_reg(hw, I40E_GLQF_FD_MSK(i, pctype),
					     0);
			i40e_check_write_reg(hw, I40E_GLQF_HASH_MSK(i, pctype),
					     0);
		}
		I40E_WRITE_FLUSH(hw);

		/* store the default input set */
		pf->hash_input_set[pctype] = input_set;
		pf->fdir.input_set[pctype] = input_set;
	}
}

int
i40e_hash_filter_inset_select(struct i40e_hw *hw,
			 struct rte_eth_input_set_conf *conf)
{
	struct i40e_pf *pf = &((struct i40e_adapter *)hw->back)->pf;
	enum i40e_filter_pctype pctype;
	uint64_t input_set, inset_reg = 0;
	uint32_t mask_reg[I40E_INSET_MASK_NUM_REG] = {0};
	int ret, i, num;

	if (!conf) {
		PMD_DRV_LOG(ERR, "Invalid pointer");
		return -EFAULT;
	}
	if (conf->op != RTE_ETH_INPUT_SET_SELECT &&
	    conf->op != RTE_ETH_INPUT_SET_ADD) {
		PMD_DRV_LOG(ERR, "Unsupported input set operation");
		return -EINVAL;
	}

	if (!I40E_VALID_FLOW(conf->flow_type)) {
		PMD_DRV_LOG(ERR, "invalid flow_type input.");
		return -EINVAL;
	}

	if (hw->mac.type == I40E_MAC_X722) {
		/* get translated pctype value in fd pctype register */
		pctype = (enum i40e_filter_pctype)i40e_read_rx_ctl(hw,
			I40E_GLQF_FD_PCTYPES((int)i40e_flowtype_to_pctype(
			conf->flow_type)));
	} else
		pctype = i40e_flowtype_to_pctype(conf->flow_type);

	ret = i40e_parse_input_set(&input_set, pctype, conf->field,
				   conf->inset_size);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to parse input set");
		return -EINVAL;
	}
	if (i40e_validate_input_set(pctype, RTE_ETH_FILTER_HASH,
				    input_set) != 0) {
		PMD_DRV_LOG(ERR, "Invalid input set");
		return -EINVAL;
	}
	if (conf->op == RTE_ETH_INPUT_SET_ADD) {
		/* get inset value in register */
		inset_reg = i40e_read_rx_ctl(hw, I40E_GLQF_HASH_INSET(1, pctype));
		inset_reg <<= I40E_32_BIT_WIDTH;
		inset_reg |= i40e_read_rx_ctl(hw, I40E_GLQF_HASH_INSET(0, pctype));
		input_set |= pf->hash_input_set[pctype];
	}
	num = i40e_generate_inset_mask_reg(input_set, mask_reg,
					   I40E_INSET_MASK_NUM_REG);
	if (num < 0)
		return -EINVAL;

	inset_reg |= i40e_translate_input_set_reg(hw->mac.type, input_set);

	i40e_check_write_reg(hw, I40E_GLQF_HASH_INSET(0, pctype),
			      (uint32_t)(inset_reg & UINT32_MAX));
	i40e_check_write_reg(hw, I40E_GLQF_HASH_INSET(1, pctype),
			     (uint32_t)((inset_reg >>
			     I40E_32_BIT_WIDTH) & UINT32_MAX));

	for (i = 0; i < num; i++)
		i40e_check_write_reg(hw, I40E_GLQF_HASH_MSK(i, pctype),
				     mask_reg[i]);
	/*clear unused mask registers of the pctype */
	for (i = num; i < I40E_INSET_MASK_NUM_REG; i++)
		i40e_check_write_reg(hw, I40E_GLQF_HASH_MSK(i, pctype),
				     0);
	I40E_WRITE_FLUSH(hw);

	pf->hash_input_set[pctype] = input_set;
	return 0;
}

int
i40e_fdir_filter_inset_select(struct i40e_pf *pf,
			 struct rte_eth_input_set_conf *conf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	enum i40e_filter_pctype pctype;
	uint64_t input_set, inset_reg = 0;
	uint32_t mask_reg[I40E_INSET_MASK_NUM_REG] = {0};
	int ret, i, num;

	if (!hw || !conf) {
		PMD_DRV_LOG(ERR, "Invalid pointer");
		return -EFAULT;
	}
	if (conf->op != RTE_ETH_INPUT_SET_SELECT &&
	    conf->op != RTE_ETH_INPUT_SET_ADD) {
		PMD_DRV_LOG(ERR, "Unsupported input set operation");
		return -EINVAL;
	}

	if (!I40E_VALID_FLOW(conf->flow_type)) {
		PMD_DRV_LOG(ERR, "invalid flow_type input.");
		return -EINVAL;
	}

	pctype = i40e_flowtype_to_pctype(conf->flow_type);

	ret = i40e_parse_input_set(&input_set, pctype, conf->field,
				   conf->inset_size);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to parse input set");
		return -EINVAL;
	}
	if (i40e_validate_input_set(pctype, RTE_ETH_FILTER_FDIR,
				    input_set) != 0) {
		PMD_DRV_LOG(ERR, "Invalid input set");
		return -EINVAL;
	}

	/* get inset value in register */
	inset_reg = i40e_read_rx_ctl(hw, I40E_PRTQF_FD_INSET(pctype, 1));
	inset_reg <<= I40E_32_BIT_WIDTH;
	inset_reg |= i40e_read_rx_ctl(hw, I40E_PRTQF_FD_INSET(pctype, 0));

	/* Can not change the inset reg for flex payload for fdir,
	 * it is done by writing I40E_PRTQF_FD_FLXINSET
	 * in i40e_set_flex_mask_on_pctype.
	 */
	if (conf->op == RTE_ETH_INPUT_SET_SELECT)
		inset_reg &= I40E_REG_INSET_FLEX_PAYLOAD_WORDS;
	else
		input_set |= pf->fdir.input_set[pctype];
	num = i40e_generate_inset_mask_reg(input_set, mask_reg,
					   I40E_INSET_MASK_NUM_REG);
	if (num < 0)
		return -EINVAL;

	inset_reg |= i40e_translate_input_set_reg(hw->mac.type, input_set);

	i40e_check_write_reg(hw, I40E_PRTQF_FD_INSET(pctype, 0),
			      (uint32_t)(inset_reg & UINT32_MAX));
	i40e_check_write_reg(hw, I40E_PRTQF_FD_INSET(pctype, 1),
			     (uint32_t)((inset_reg >>
			     I40E_32_BIT_WIDTH) & UINT32_MAX));

	for (i = 0; i < num; i++)
		i40e_check_write_reg(hw, I40E_GLQF_FD_MSK(i, pctype),
				     mask_reg[i]);
	/*clear unused mask registers of the pctype */
	for (i = num; i < I40E_INSET_MASK_NUM_REG; i++)
		i40e_check_write_reg(hw, I40E_GLQF_FD_MSK(i, pctype),
				     0);
	I40E_WRITE_FLUSH(hw);

	pf->fdir.input_set[pctype] = input_set;
	return 0;
}

static int
i40e_hash_filter_get(struct i40e_hw *hw, struct rte_eth_hash_filter_info *info)
{
	int ret = 0;

	if (!hw || !info) {
		PMD_DRV_LOG(ERR, "Invalid pointer");
		return -EFAULT;
	}

	switch (info->info_type) {
	case RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT:
		i40e_get_symmetric_hash_enable_per_port(hw,
					&(info->info.enable));
		break;
	case RTE_ETH_HASH_FILTER_GLOBAL_CONFIG:
		ret = i40e_get_hash_filter_global_config(hw,
				&(info->info.global_conf));
		break;
	default:
		PMD_DRV_LOG(ERR, "Hash filter info type (%d) not supported",
							info->info_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
i40e_hash_filter_set(struct i40e_hw *hw, struct rte_eth_hash_filter_info *info)
{
	int ret = 0;

	if (!hw || !info) {
		PMD_DRV_LOG(ERR, "Invalid pointer");
		return -EFAULT;
	}

	switch (info->info_type) {
	case RTE_ETH_HASH_FILTER_SYM_HASH_ENA_PER_PORT:
		i40e_set_symmetric_hash_enable_per_port(hw, info->info.enable);
		break;
	case RTE_ETH_HASH_FILTER_GLOBAL_CONFIG:
		ret = i40e_set_hash_filter_global_config(hw,
				&(info->info.global_conf));
		break;
	case RTE_ETH_HASH_FILTER_INPUT_SET_SELECT:
		ret = i40e_hash_filter_inset_select(hw,
					       &(info->info.input_set_conf));
		break;

	default:
		PMD_DRV_LOG(ERR, "Hash filter info type (%d) not supported",
							info->info_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* Operations for hash function */
static int
i40e_hash_filter_ctrl(struct rte_eth_dev *dev,
		      enum rte_filter_op filter_op,
		      void *arg)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret = 0;

	switch (filter_op) {
	case RTE_ETH_FILTER_NOP:
		break;
	case RTE_ETH_FILTER_GET:
		ret = i40e_hash_filter_get(hw,
			(struct rte_eth_hash_filter_info *)arg);
		break;
	case RTE_ETH_FILTER_SET:
		ret = i40e_hash_filter_set(hw,
			(struct rte_eth_hash_filter_info *)arg);
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter operation (%d) not supported",
								filter_op);
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

/* Convert ethertype filter structure */
static int
i40e_ethertype_filter_convert(const struct rte_eth_ethertype_filter *input,
			      struct i40e_ethertype_filter *filter)
{
	rte_memcpy(&filter->input.mac_addr, &input->mac_addr, ETHER_ADDR_LEN);
	filter->input.ether_type = input->ether_type;
	filter->flags = input->flags;
	filter->queue = input->queue;

	return 0;
}

/* Check if there exists the ehtertype filter */
struct i40e_ethertype_filter *
i40e_sw_ethertype_filter_lookup(struct i40e_ethertype_rule *ethertype_rule,
				const struct i40e_ethertype_filter_input *input)
{
	int ret;

	ret = rte_hash_lookup(ethertype_rule->hash_table, (const void *)input);
	if (ret < 0)
		return NULL;

	return ethertype_rule->hash_map[ret];
}

/* Add ethertype filter in SW list */
static int
i40e_sw_ethertype_filter_insert(struct i40e_pf *pf,
				struct i40e_ethertype_filter *filter)
{
	struct i40e_ethertype_rule *rule = &pf->ethertype;
	int ret;

	ret = rte_hash_add_key(rule->hash_table, &filter->input);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to insert ethertype filter"
			    " to hash table %d!",
			    ret);
		return ret;
	}
	rule->hash_map[ret] = filter;

	TAILQ_INSERT_TAIL(&rule->ethertype_list, filter, rules);

	return 0;
}

/* Delete ethertype filter in SW list */
int
i40e_sw_ethertype_filter_del(struct i40e_pf *pf,
			     struct i40e_ethertype_filter_input *input)
{
	struct i40e_ethertype_rule *rule = &pf->ethertype;
	struct i40e_ethertype_filter *filter;
	int ret;

	ret = rte_hash_del_key(rule->hash_table, input);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to delete ethertype filter"
			    " to hash table %d!",
			    ret);
		return ret;
	}
	filter = rule->hash_map[ret];
	rule->hash_map[ret] = NULL;

	TAILQ_REMOVE(&rule->ethertype_list, filter, rules);
	rte_free(filter);

	return 0;
}

/*
 * Configure ethertype filter, which can director packet by filtering
 * with mac address and ether_type or only ether_type
 */
int
i40e_ethertype_filter_set(struct i40e_pf *pf,
			struct rte_eth_ethertype_filter *filter,
			bool add)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_ethertype_rule *ethertype_rule = &pf->ethertype;
	struct i40e_ethertype_filter *ethertype_filter, *node;
	struct i40e_ethertype_filter check_filter;
	struct i40e_control_filter_stats stats;
	uint16_t flags = 0;
	int ret;

	if (filter->queue >= pf->dev_data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "Invalid queue ID");
		return -EINVAL;
	}
	if (filter->ether_type == ETHER_TYPE_IPv4 ||
		filter->ether_type == ETHER_TYPE_IPv6) {
		PMD_DRV_LOG(ERR,
			"unsupported ether_type(0x%04x) in control packet filter.",
			filter->ether_type);
		return -EINVAL;
	}
	if (filter->ether_type == ETHER_TYPE_VLAN)
		PMD_DRV_LOG(WARNING,
			"filter vlan ether_type in first tag is not supported.");

	/* Check if there is the filter in SW list */
	memset(&check_filter, 0, sizeof(check_filter));
	i40e_ethertype_filter_convert(filter, &check_filter);
	node = i40e_sw_ethertype_filter_lookup(ethertype_rule,
					       &check_filter.input);
	if (add && node) {
		PMD_DRV_LOG(ERR, "Conflict with existing ethertype rules!");
		return -EINVAL;
	}

	if (!add && !node) {
		PMD_DRV_LOG(ERR, "There's no corresponding ethertype filter!");
		return -EINVAL;
	}

	if (!(filter->flags & RTE_ETHTYPE_FLAGS_MAC))
		flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_IGNORE_MAC;
	if (filter->flags & RTE_ETHTYPE_FLAGS_DROP)
		flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_DROP;
	flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_TO_QUEUE;

	memset(&stats, 0, sizeof(stats));
	ret = i40e_aq_add_rem_control_packet_filter(hw,
			filter->mac_addr.addr_bytes,
			filter->ether_type, flags,
			pf->main_vsi->seid,
			filter->queue, add, &stats, NULL);

	PMD_DRV_LOG(INFO,
		"add/rem control packet filter, return %d, mac_etype_used = %u, etype_used = %u, mac_etype_free = %u, etype_free = %u",
		ret, stats.mac_etype_used, stats.etype_used,
		stats.mac_etype_free, stats.etype_free);
	if (ret < 0)
		return -ENOSYS;

	/* Add or delete a filter in SW list */
	if (add) {
		ethertype_filter = rte_zmalloc("ethertype_filter",
				       sizeof(*ethertype_filter), 0);
		rte_memcpy(ethertype_filter, &check_filter,
			   sizeof(check_filter));
		ret = i40e_sw_ethertype_filter_insert(pf, ethertype_filter);
	} else {
		ret = i40e_sw_ethertype_filter_del(pf, &node->input);
	}

	return ret;
}

/*
 * Handle operations for ethertype filter.
 */
static int
i40e_ethertype_filter_handle(struct rte_eth_dev *dev,
				enum rte_filter_op filter_op,
				void *arg)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret = 0;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return ret;

	if (arg == NULL) {
		PMD_DRV_LOG(ERR, "arg shouldn't be NULL for operation %u",
			    filter_op);
		return -EINVAL;
	}

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		ret = i40e_ethertype_filter_set(pf,
			(struct rte_eth_ethertype_filter *)arg,
			TRUE);
		break;
	case RTE_ETH_FILTER_DELETE:
		ret = i40e_ethertype_filter_set(pf,
			(struct rte_eth_ethertype_filter *)arg,
			FALSE);
		break;
	default:
		PMD_DRV_LOG(ERR, "unsupported operation %u", filter_op);
		ret = -ENOSYS;
		break;
	}
	return ret;
}

static int
i40e_dev_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg)
{
	int ret = 0;

	if (dev == NULL)
		return -EINVAL;

	switch (filter_type) {
	case RTE_ETH_FILTER_NONE:
		/* For global configuration */
		ret = i40e_filter_ctrl_global_config(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_HASH:
		ret = i40e_hash_filter_ctrl(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_MACVLAN:
		ret = i40e_mac_filter_handle(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_ETHERTYPE:
		ret = i40e_ethertype_filter_handle(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_TUNNEL:
		ret = i40e_tunnel_filter_handle(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_FDIR:
		ret = i40e_fdir_ctrl_func(dev, filter_op, arg);
		break;
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &i40e_flow_ops;
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
							filter_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * Check and enable Extended Tag.
 * Enabling Extended Tag is important for 40G performance.
 */
static void
i40e_enable_extended_tag(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	uint32_t buf = 0;
	int ret;

	ret = rte_pci_read_config(pci_dev, &buf, sizeof(buf),
				      PCI_DEV_CAP_REG);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to read PCI offset 0x%x",
			    PCI_DEV_CAP_REG);
		return;
	}
	if (!(buf & PCI_DEV_CAP_EXT_TAG_MASK)) {
		PMD_DRV_LOG(ERR, "Does not support Extended Tag");
		return;
	}

	buf = 0;
	ret = rte_pci_read_config(pci_dev, &buf, sizeof(buf),
				      PCI_DEV_CTRL_REG);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to read PCI offset 0x%x",
			    PCI_DEV_CTRL_REG);
		return;
	}
	if (buf & PCI_DEV_CTRL_EXT_TAG_MASK) {
		PMD_DRV_LOG(DEBUG, "Extended Tag has already been enabled");
		return;
	}
	buf |= PCI_DEV_CTRL_EXT_TAG_MASK;
	ret = rte_pci_write_config(pci_dev, &buf, sizeof(buf),
				       PCI_DEV_CTRL_REG);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to write PCI offset 0x%x",
			    PCI_DEV_CTRL_REG);
		return;
	}
}

/*
 * As some registers wouldn't be reset unless a global hardware reset,
 * hardware initialization is needed to put those registers into an
 * expected initial state.
 */
static void
i40e_hw_init(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	i40e_enable_extended_tag(dev);

	/* clear the PF Queue Filter control register */
	i40e_write_rx_ctl(hw, I40E_PFQF_CTL_0, 0);

	/* Disable symmetric hash per port */
	i40e_set_symmetric_hash_enable_per_port(hw, 0);
}

enum i40e_filter_pctype
i40e_flowtype_to_pctype(uint16_t flow_type)
{
	static const enum i40e_filter_pctype pctype_table[] = {
		[RTE_ETH_FLOW_FRAG_IPV4] = I40E_FILTER_PCTYPE_FRAG_IPV4,
		[RTE_ETH_FLOW_NONFRAG_IPV4_UDP] =
			I40E_FILTER_PCTYPE_NONF_IPV4_UDP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_TCP] =
			I40E_FILTER_PCTYPE_NONF_IPV4_TCP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_SCTP] =
			I40E_FILTER_PCTYPE_NONF_IPV4_SCTP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_OTHER] =
			I40E_FILTER_PCTYPE_NONF_IPV4_OTHER,
		[RTE_ETH_FLOW_FRAG_IPV6] = I40E_FILTER_PCTYPE_FRAG_IPV6,
		[RTE_ETH_FLOW_NONFRAG_IPV6_UDP] =
			I40E_FILTER_PCTYPE_NONF_IPV6_UDP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_TCP] =
			I40E_FILTER_PCTYPE_NONF_IPV6_TCP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_SCTP] =
			I40E_FILTER_PCTYPE_NONF_IPV6_SCTP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_OTHER] =
			I40E_FILTER_PCTYPE_NONF_IPV6_OTHER,
		[RTE_ETH_FLOW_L2_PAYLOAD] = I40E_FILTER_PCTYPE_L2_PAYLOAD,
	};

	return pctype_table[flow_type];
}

uint16_t
i40e_pctype_to_flowtype(enum i40e_filter_pctype pctype)
{
	static const uint16_t flowtype_table[] = {
		[I40E_FILTER_PCTYPE_FRAG_IPV4] = RTE_ETH_FLOW_FRAG_IPV4,
		[I40E_FILTER_PCTYPE_NONF_IPV4_UDP] =
			RTE_ETH_FLOW_NONFRAG_IPV4_UDP,
		[I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP] =
			RTE_ETH_FLOW_NONFRAG_IPV4_UDP,
		[I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP] =
			RTE_ETH_FLOW_NONFRAG_IPV4_UDP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP] =
			RTE_ETH_FLOW_NONFRAG_IPV4_TCP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK] =
			RTE_ETH_FLOW_NONFRAG_IPV4_TCP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_SCTP] =
			RTE_ETH_FLOW_NONFRAG_IPV4_SCTP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_OTHER] =
			RTE_ETH_FLOW_NONFRAG_IPV4_OTHER,
		[I40E_FILTER_PCTYPE_FRAG_IPV6] = RTE_ETH_FLOW_FRAG_IPV6,
		[I40E_FILTER_PCTYPE_NONF_IPV6_UDP] =
			RTE_ETH_FLOW_NONFRAG_IPV6_UDP,
		[I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP] =
			RTE_ETH_FLOW_NONFRAG_IPV6_UDP,
		[I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP] =
			RTE_ETH_FLOW_NONFRAG_IPV6_UDP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP] =
			RTE_ETH_FLOW_NONFRAG_IPV6_TCP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK] =
			RTE_ETH_FLOW_NONFRAG_IPV6_TCP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_SCTP] =
			RTE_ETH_FLOW_NONFRAG_IPV6_SCTP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_OTHER] =
			RTE_ETH_FLOW_NONFRAG_IPV6_OTHER,
		[I40E_FILTER_PCTYPE_L2_PAYLOAD] = RTE_ETH_FLOW_L2_PAYLOAD,
	};

	return flowtype_table[pctype];
}

/*
 * On X710, performance number is far from the expectation on recent firmware
 * versions; on XL710, performance number is also far from the expectation on
 * recent firmware versions, if promiscuous mode is disabled, or promiscuous
 * mode is enabled and port MAC address is equal to the packet destination MAC
 * address. The fix for this issue may not be integrated in the following
 * firmware version. So the workaround in software driver is needed. It needs
 * to modify the initial values of 3 internal only registers for both X710 and
 * XL710. Note that the values for X710 or XL710 could be different, and the
 * workaround can be removed when it is fixed in firmware in the future.
 */

/* For both X710 and XL710 */
#define I40E_GL_SWR_PRI_JOIN_MAP_0_VALUE 0x10000200
#define I40E_GL_SWR_PRI_JOIN_MAP_0       0x26CE00

#define I40E_GL_SWR_PRI_JOIN_MAP_2_VALUE 0x011f0200
#define I40E_GL_SWR_PRI_JOIN_MAP_2       0x26CE08

/* For X722 */
#define I40E_X722_GL_SWR_PRI_JOIN_MAP_0_VALUE 0x20000200
#define I40E_X722_GL_SWR_PRI_JOIN_MAP_2_VALUE 0x013F0200

/* For X710 */
#define I40E_GL_SWR_PM_UP_THR_EF_VALUE   0x03030303
/* For XL710 */
#define I40E_GL_SWR_PM_UP_THR_SF_VALUE   0x06060606
#define I40E_GL_SWR_PM_UP_THR            0x269FBC

static int
i40e_dev_sync_phy_type(struct i40e_hw *hw)
{
	enum i40e_status_code status;
	struct i40e_aq_get_phy_abilities_resp phy_ab;
	int ret = -ENOTSUP;

	status = i40e_aq_get_phy_capabilities(hw, false, true, &phy_ab,
					      NULL);

	if (status)
		return ret;

	return 0;
}

static void
i40e_configure_registers(struct i40e_hw *hw)
{
	static struct {
		uint32_t addr;
		uint64_t val;
	} reg_table[] = {
		{I40E_GL_SWR_PRI_JOIN_MAP_0, 0},
		{I40E_GL_SWR_PRI_JOIN_MAP_2, 0},
		{I40E_GL_SWR_PM_UP_THR, 0}, /* Compute value dynamically */
	};
	uint64_t reg;
	uint32_t i;
	int ret;

	for (i = 0; i < RTE_DIM(reg_table); i++) {
		if (reg_table[i].addr == I40E_GL_SWR_PRI_JOIN_MAP_0) {
			if (hw->mac.type == I40E_MAC_X722) /* For X722 */
				reg_table[i].val =
					I40E_X722_GL_SWR_PRI_JOIN_MAP_0_VALUE;
			else /* For X710/XL710/XXV710 */
				reg_table[i].val =
					I40E_GL_SWR_PRI_JOIN_MAP_0_VALUE;
		}

		if (reg_table[i].addr == I40E_GL_SWR_PRI_JOIN_MAP_2) {
			if (hw->mac.type == I40E_MAC_X722) /* For X722 */
				reg_table[i].val =
					I40E_X722_GL_SWR_PRI_JOIN_MAP_2_VALUE;
			else /* For X710/XL710/XXV710 */
				reg_table[i].val =
					I40E_GL_SWR_PRI_JOIN_MAP_2_VALUE;
		}

		if (reg_table[i].addr == I40E_GL_SWR_PM_UP_THR) {
			if (I40E_PHY_TYPE_SUPPORT_40G(hw->phy.phy_types) || /* For XL710 */
			    I40E_PHY_TYPE_SUPPORT_25G(hw->phy.phy_types)) /* For XXV710 */
				reg_table[i].val =
					I40E_GL_SWR_PM_UP_THR_SF_VALUE;
			else /* For X710 */
				reg_table[i].val =
					I40E_GL_SWR_PM_UP_THR_EF_VALUE;
		}

		ret = i40e_aq_debug_read_register(hw, reg_table[i].addr,
							&reg, NULL);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to read from 0x%"PRIx32,
							reg_table[i].addr);
			break;
		}
		PMD_DRV_LOG(DEBUG, "Read from 0x%"PRIx32": 0x%"PRIx64,
						reg_table[i].addr, reg);
		if (reg == reg_table[i].val)
			continue;

		ret = i40e_aq_debug_write_register(hw, reg_table[i].addr,
						reg_table[i].val, NULL);
		if (ret < 0) {
			PMD_DRV_LOG(ERR,
				"Failed to write 0x%"PRIx64" to the address of 0x%"PRIx32,
				reg_table[i].val, reg_table[i].addr);
			break;
		}
		PMD_DRV_LOG(DEBUG, "Write 0x%"PRIx64" to the address of "
			"0x%"PRIx32, reg_table[i].val, reg_table[i].addr);
	}
}

#define I40E_VSI_TSR(_i)            (0x00050800 + ((_i) * 4))
#define I40E_VSI_TSR_QINQ_CONFIG    0xc030
#define I40E_VSI_L2TAGSTXVALID(_i)  (0x00042800 + ((_i) * 4))
#define I40E_VSI_L2TAGSTXVALID_QINQ 0xab
static int
i40e_config_qinq(struct i40e_hw *hw, struct i40e_vsi *vsi)
{
	uint32_t reg;
	int ret;

	if (vsi->vsi_id >= I40E_MAX_NUM_VSIS) {
		PMD_DRV_LOG(ERR, "VSI ID exceeds the maximum");
		return -EINVAL;
	}

	/* Configure for double VLAN RX stripping */
	reg = I40E_READ_REG(hw, I40E_VSI_TSR(vsi->vsi_id));
	if ((reg & I40E_VSI_TSR_QINQ_CONFIG) != I40E_VSI_TSR_QINQ_CONFIG) {
		reg |= I40E_VSI_TSR_QINQ_CONFIG;
		ret = i40e_aq_debug_write_register(hw,
						   I40E_VSI_TSR(vsi->vsi_id),
						   reg, NULL);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to update VSI_TSR[%d]",
				    vsi->vsi_id);
			return I40E_ERR_CONFIG;
		}
	}

	/* Configure for double VLAN TX insertion */
	reg = I40E_READ_REG(hw, I40E_VSI_L2TAGSTXVALID(vsi->vsi_id));
	if ((reg & 0xff) != I40E_VSI_L2TAGSTXVALID_QINQ) {
		reg = I40E_VSI_L2TAGSTXVALID_QINQ;
		ret = i40e_aq_debug_write_register(hw,
						   I40E_VSI_L2TAGSTXVALID(
						   vsi->vsi_id), reg, NULL);
		if (ret < 0) {
			PMD_DRV_LOG(ERR,
				"Failed to update VSI_L2TAGSTXVALID[%d]",
				vsi->vsi_id);
			return I40E_ERR_CONFIG;
		}
	}

	return 0;
}

/**
 * i40e_aq_add_mirror_rule
 * @hw: pointer to the hardware structure
 * @seid: VEB seid to add mirror rule to
 * @dst_id: destination vsi seid
 * @entries: Buffer which contains the entities to be mirrored
 * @count: number of entities contained in the buffer
 * @rule_id:the rule_id of the rule to be added
 *
 * Add a mirror rule for a given veb.
 *
 **/
static enum i40e_status_code
i40e_aq_add_mirror_rule(struct i40e_hw *hw,
			uint16_t seid, uint16_t dst_id,
			uint16_t rule_type, uint16_t *entries,
			uint16_t count, uint16_t *rule_id)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_delete_mirror_rule cmd;
	struct i40e_aqc_add_delete_mirror_rule_completion *resp =
		(struct i40e_aqc_add_delete_mirror_rule_completion *)
		&desc.params.raw;
	uint16_t buff_len;
	enum i40e_status_code status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_add_mirror_rule);
	memset(&cmd, 0, sizeof(cmd));

	buff_len = sizeof(uint16_t) * count;
	desc.datalen = rte_cpu_to_le_16(buff_len);
	if (buff_len > 0)
		desc.flags |= rte_cpu_to_le_16(
			(uint16_t)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	cmd.rule_type = rte_cpu_to_le_16(rule_type <<
				I40E_AQC_MIRROR_RULE_TYPE_SHIFT);
	cmd.num_entries = rte_cpu_to_le_16(count);
	cmd.seid = rte_cpu_to_le_16(seid);
	cmd.destination = rte_cpu_to_le_16(dst_id);

	rte_memcpy(&desc.params.raw, &cmd, sizeof(cmd));
	status = i40e_asq_send_command(hw, &desc, entries, buff_len, NULL);
	PMD_DRV_LOG(INFO,
		"i40e_aq_add_mirror_rule, aq_status %d, rule_id = %u mirror_rules_used = %u, mirror_rules_free = %u,",
		hw->aq.asq_last_status, resp->rule_id,
		resp->mirror_rules_used, resp->mirror_rules_free);
	*rule_id = rte_le_to_cpu_16(resp->rule_id);

	return status;
}

/**
 * i40e_aq_del_mirror_rule
 * @hw: pointer to the hardware structure
 * @seid: VEB seid to add mirror rule to
 * @entries: Buffer which contains the entities to be mirrored
 * @count: number of entities contained in the buffer
 * @rule_id:the rule_id of the rule to be delete
 *
 * Delete a mirror rule for a given veb.
 *
 **/
static enum i40e_status_code
i40e_aq_del_mirror_rule(struct i40e_hw *hw,
		uint16_t seid, uint16_t rule_type, uint16_t *entries,
		uint16_t count, uint16_t rule_id)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_delete_mirror_rule cmd;
	uint16_t buff_len = 0;
	enum i40e_status_code status;
	void *buff = NULL;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_delete_mirror_rule);
	memset(&cmd, 0, sizeof(cmd));
	if (rule_type == I40E_AQC_MIRROR_RULE_TYPE_VLAN) {
		desc.flags |= rte_cpu_to_le_16((uint16_t)(I40E_AQ_FLAG_BUF |
							  I40E_AQ_FLAG_RD));
		cmd.num_entries = count;
		buff_len = sizeof(uint16_t) * count;
		desc.datalen = rte_cpu_to_le_16(buff_len);
		buff = (void *)entries;
	} else
		/* rule id is filled in destination field for deleting mirror rule */
		cmd.destination = rte_cpu_to_le_16(rule_id);

	cmd.rule_type = rte_cpu_to_le_16(rule_type <<
				I40E_AQC_MIRROR_RULE_TYPE_SHIFT);
	cmd.seid = rte_cpu_to_le_16(seid);

	rte_memcpy(&desc.params.raw, &cmd, sizeof(cmd));
	status = i40e_asq_send_command(hw, &desc, buff, buff_len, NULL);

	return status;
}

/**
 * i40e_mirror_rule_set
 * @dev: pointer to the hardware structure
 * @mirror_conf: mirror rule info
 * @sw_id: mirror rule's sw_id
 * @on: enable/disable
 *
 * set a mirror rule.
 *
 **/
static int
i40e_mirror_rule_set(struct rte_eth_dev *dev,
			struct rte_eth_mirror_conf *mirror_conf,
			uint8_t sw_id, uint8_t on)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_mirror_rule *it, *mirr_rule = NULL;
	struct i40e_mirror_rule *parent = NULL;
	uint16_t seid, dst_seid, rule_id;
	uint16_t i, j = 0;
	int ret;

	PMD_DRV_LOG(DEBUG, "i40e_mirror_rule_set: sw_id = %d.", sw_id);

	if (pf->main_vsi->veb == NULL || pf->vfs == NULL) {
		PMD_DRV_LOG(ERR,
			"mirror rule can not be configured without veb or vfs.");
		return -ENOSYS;
	}
	if (pf->nb_mirror_rule > I40E_MAX_MIRROR_RULES) {
		PMD_DRV_LOG(ERR, "mirror table is full.");
		return -ENOSPC;
	}
	if (mirror_conf->dst_pool > pf->vf_num) {
		PMD_DRV_LOG(ERR, "invalid destination pool %u.",
				 mirror_conf->dst_pool);
		return -EINVAL;
	}

	seid = pf->main_vsi->veb->seid;

	TAILQ_FOREACH(it, &pf->mirror_list, rules) {
		if (sw_id <= it->index) {
			mirr_rule = it;
			break;
		}
		parent = it;
	}
	if (mirr_rule && sw_id == mirr_rule->index) {
		if (on) {
			PMD_DRV_LOG(ERR, "mirror rule exists.");
			return -EEXIST;
		} else {
			ret = i40e_aq_del_mirror_rule(hw, seid,
					mirr_rule->rule_type,
					mirr_rule->entries,
					mirr_rule->num_entries, mirr_rule->id);
			if (ret < 0) {
				PMD_DRV_LOG(ERR,
					"failed to remove mirror rule: ret = %d, aq_err = %d.",
					ret, hw->aq.asq_last_status);
				return -ENOSYS;
			}
			TAILQ_REMOVE(&pf->mirror_list, mirr_rule, rules);
			rte_free(mirr_rule);
			pf->nb_mirror_rule--;
			return 0;
		}
	} else if (!on) {
		PMD_DRV_LOG(ERR, "mirror rule doesn't exist.");
		return -ENOENT;
	}

	mirr_rule = rte_zmalloc("i40e_mirror_rule",
				sizeof(struct i40e_mirror_rule) , 0);
	if (!mirr_rule) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return I40E_ERR_NO_MEMORY;
	}
	switch (mirror_conf->rule_type) {
	case ETH_MIRROR_VLAN:
		for (i = 0, j = 0; i < ETH_MIRROR_MAX_VLANS; i++) {
			if (mirror_conf->vlan.vlan_mask & (1ULL << i)) {
				mirr_rule->entries[j] =
					mirror_conf->vlan.vlan_id[i];
				j++;
			}
		}
		if (j == 0) {
			PMD_DRV_LOG(ERR, "vlan is not specified.");
			rte_free(mirr_rule);
			return -EINVAL;
		}
		mirr_rule->rule_type = I40E_AQC_MIRROR_RULE_TYPE_VLAN;
		break;
	case ETH_MIRROR_VIRTUAL_POOL_UP:
	case ETH_MIRROR_VIRTUAL_POOL_DOWN:
		/* check if the specified pool bit is out of range */
		if (mirror_conf->pool_mask > (uint64_t)(1ULL << (pf->vf_num + 1))) {
			PMD_DRV_LOG(ERR, "pool mask is out of range.");
			rte_free(mirr_rule);
			return -EINVAL;
		}
		for (i = 0, j = 0; i < pf->vf_num; i++) {
			if (mirror_conf->pool_mask & (1ULL << i)) {
				mirr_rule->entries[j] = pf->vfs[i].vsi->seid;
				j++;
			}
		}
		if (mirror_conf->pool_mask & (1ULL << pf->vf_num)) {
			/* add pf vsi to entries */
			mirr_rule->entries[j] = pf->main_vsi_seid;
			j++;
		}
		if (j == 0) {
			PMD_DRV_LOG(ERR, "pool is not specified.");
			rte_free(mirr_rule);
			return -EINVAL;
		}
		/* egress and ingress in aq commands means from switch but not port */
		mirr_rule->rule_type =
			(mirror_conf->rule_type == ETH_MIRROR_VIRTUAL_POOL_UP) ?
			I40E_AQC_MIRROR_RULE_TYPE_VPORT_EGRESS :
			I40E_AQC_MIRROR_RULE_TYPE_VPORT_INGRESS;
		break;
	case ETH_MIRROR_UPLINK_PORT:
		/* egress and ingress in aq commands means from switch but not port*/
		mirr_rule->rule_type = I40E_AQC_MIRROR_RULE_TYPE_ALL_EGRESS;
		break;
	case ETH_MIRROR_DOWNLINK_PORT:
		mirr_rule->rule_type = I40E_AQC_MIRROR_RULE_TYPE_ALL_INGRESS;
		break;
	default:
		PMD_DRV_LOG(ERR, "unsupported mirror type %d.",
			mirror_conf->rule_type);
		rte_free(mirr_rule);
		return -EINVAL;
	}

	/* If the dst_pool is equal to vf_num, consider it as PF */
	if (mirror_conf->dst_pool == pf->vf_num)
		dst_seid = pf->main_vsi_seid;
	else
		dst_seid = pf->vfs[mirror_conf->dst_pool].vsi->seid;

	ret = i40e_aq_add_mirror_rule(hw, seid, dst_seid,
				      mirr_rule->rule_type, mirr_rule->entries,
				      j, &rule_id);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			"failed to add mirror rule: ret = %d, aq_err = %d.",
			ret, hw->aq.asq_last_status);
		rte_free(mirr_rule);
		return -ENOSYS;
	}

	mirr_rule->index = sw_id;
	mirr_rule->num_entries = j;
	mirr_rule->id = rule_id;
	mirr_rule->dst_vsi_seid = dst_seid;

	if (parent)
		TAILQ_INSERT_AFTER(&pf->mirror_list, parent, mirr_rule, rules);
	else
		TAILQ_INSERT_HEAD(&pf->mirror_list, mirr_rule, rules);

	pf->nb_mirror_rule++;
	return 0;
}

/**
 * i40e_mirror_rule_reset
 * @dev: pointer to the device
 * @sw_id: mirror rule's sw_id
 *
 * reset a mirror rule.
 *
 **/
static int
i40e_mirror_rule_reset(struct rte_eth_dev *dev, uint8_t sw_id)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_mirror_rule *it, *mirr_rule = NULL;
	uint16_t seid;
	int ret;

	PMD_DRV_LOG(DEBUG, "i40e_mirror_rule_reset: sw_id = %d.", sw_id);

	seid = pf->main_vsi->veb->seid;

	TAILQ_FOREACH(it, &pf->mirror_list, rules) {
		if (sw_id == it->index) {
			mirr_rule = it;
			break;
		}
	}
	if (mirr_rule) {
		ret = i40e_aq_del_mirror_rule(hw, seid,
				mirr_rule->rule_type,
				mirr_rule->entries,
				mirr_rule->num_entries, mirr_rule->id);
		if (ret < 0) {
			PMD_DRV_LOG(ERR,
				"failed to remove mirror rule: status = %d, aq_err = %d.",
				ret, hw->aq.asq_last_status);
			return -ENOSYS;
		}
		TAILQ_REMOVE(&pf->mirror_list, mirr_rule, rules);
		rte_free(mirr_rule);
		pf->nb_mirror_rule--;
	} else {
		PMD_DRV_LOG(ERR, "mirror rule doesn't exist.");
		return -ENOENT;
	}
	return 0;
}

static uint64_t
i40e_read_systime_cyclecounter(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t systim_cycles;

	systim_cycles = (uint64_t)I40E_READ_REG(hw, I40E_PRTTSYN_TIME_L);
	systim_cycles |= (uint64_t)I40E_READ_REG(hw, I40E_PRTTSYN_TIME_H)
			<< 32;

	return systim_cycles;
}

static uint64_t
i40e_read_rx_tstamp_cyclecounter(struct rte_eth_dev *dev, uint8_t index)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t rx_tstamp;

	rx_tstamp = (uint64_t)I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_L(index));
	rx_tstamp |= (uint64_t)I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_H(index))
			<< 32;

	return rx_tstamp;
}

static uint64_t
i40e_read_tx_tstamp_cyclecounter(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t tx_tstamp;

	tx_tstamp = (uint64_t)I40E_READ_REG(hw, I40E_PRTTSYN_TXTIME_L);
	tx_tstamp |= (uint64_t)I40E_READ_REG(hw, I40E_PRTTSYN_TXTIME_H)
			<< 32;

	return tx_tstamp;
}

static void
i40e_start_timecounters(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_adapter *adapter =
			(struct i40e_adapter *)dev->data->dev_private;
	struct rte_eth_link link;
	uint32_t tsync_inc_l;
	uint32_t tsync_inc_h;

	/* Get current link speed. */
	memset(&link, 0, sizeof(link));
	i40e_dev_link_update(dev, 1);
	rte_i40e_dev_atomic_read_link_status(dev, &link);

	switch (link.link_speed) {
	case ETH_SPEED_NUM_40G:
		tsync_inc_l = I40E_PTP_40GB_INCVAL & 0xFFFFFFFF;
		tsync_inc_h = I40E_PTP_40GB_INCVAL >> 32;
		break;
	case ETH_SPEED_NUM_10G:
		tsync_inc_l = I40E_PTP_10GB_INCVAL & 0xFFFFFFFF;
		tsync_inc_h = I40E_PTP_10GB_INCVAL >> 32;
		break;
	case ETH_SPEED_NUM_1G:
		tsync_inc_l = I40E_PTP_1GB_INCVAL & 0xFFFFFFFF;
		tsync_inc_h = I40E_PTP_1GB_INCVAL >> 32;
		break;
	default:
		tsync_inc_l = 0x0;
		tsync_inc_h = 0x0;
	}

	/* Set the timesync increment value. */
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_L, tsync_inc_l);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_H, tsync_inc_h);

	memset(&adapter->systime_tc, 0, sizeof(struct rte_timecounter));
	memset(&adapter->rx_tstamp_tc, 0, sizeof(struct rte_timecounter));
	memset(&adapter->tx_tstamp_tc, 0, sizeof(struct rte_timecounter));

	adapter->systime_tc.cc_mask = I40E_CYCLECOUNTER_MASK;
	adapter->systime_tc.cc_shift = 0;
	adapter->systime_tc.nsec_mask = 0;

	adapter->rx_tstamp_tc.cc_mask = I40E_CYCLECOUNTER_MASK;
	adapter->rx_tstamp_tc.cc_shift = 0;
	adapter->rx_tstamp_tc.nsec_mask = 0;

	adapter->tx_tstamp_tc.cc_mask = I40E_CYCLECOUNTER_MASK;
	adapter->tx_tstamp_tc.cc_shift = 0;
	adapter->tx_tstamp_tc.nsec_mask = 0;
}

static int
i40e_timesync_adjust_time(struct rte_eth_dev *dev, int64_t delta)
{
	struct i40e_adapter *adapter =
			(struct i40e_adapter *)dev->data->dev_private;

	adapter->systime_tc.nsec += delta;
	adapter->rx_tstamp_tc.nsec += delta;
	adapter->tx_tstamp_tc.nsec += delta;

	return 0;
}

static int
i40e_timesync_write_time(struct rte_eth_dev *dev, const struct timespec *ts)
{
	uint64_t ns;
	struct i40e_adapter *adapter =
			(struct i40e_adapter *)dev->data->dev_private;

	ns = rte_timespec_to_ns(ts);

	/* Set the timecounters to a new value. */
	adapter->systime_tc.nsec = ns;
	adapter->rx_tstamp_tc.nsec = ns;
	adapter->tx_tstamp_tc.nsec = ns;

	return 0;
}

static int
i40e_timesync_read_time(struct rte_eth_dev *dev, struct timespec *ts)
{
	uint64_t ns, systime_cycles;
	struct i40e_adapter *adapter =
			(struct i40e_adapter *)dev->data->dev_private;

	systime_cycles = i40e_read_systime_cyclecounter(dev);
	ns = rte_timecounter_update(&adapter->systime_tc, systime_cycles);
	*ts = rte_ns_to_timespec(ns);

	return 0;
}

static int
i40e_timesync_enable(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t tsync_ctl_l;
	uint32_t tsync_ctl_h;

	/* Stop the timesync system time. */
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_L, 0x0);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_H, 0x0);
	/* Reset the timesync system time value. */
	I40E_WRITE_REG(hw, I40E_PRTTSYN_TIME_L, 0x0);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_TIME_H, 0x0);

	i40e_start_timecounters(dev);

	/* Clear timesync registers. */
	I40E_READ_REG(hw, I40E_PRTTSYN_STAT_0);
	I40E_READ_REG(hw, I40E_PRTTSYN_TXTIME_H);
	I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_H(0));
	I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_H(1));
	I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_H(2));
	I40E_READ_REG(hw, I40E_PRTTSYN_RXTIME_H(3));

	/* Enable timestamping of PTP packets. */
	tsync_ctl_l = I40E_READ_REG(hw, I40E_PRTTSYN_CTL0);
	tsync_ctl_l |= I40E_PRTTSYN_TSYNENA;

	tsync_ctl_h = I40E_READ_REG(hw, I40E_PRTTSYN_CTL1);
	tsync_ctl_h |= I40E_PRTTSYN_TSYNENA;
	tsync_ctl_h |= I40E_PRTTSYN_TSYNTYPE;

	I40E_WRITE_REG(hw, I40E_PRTTSYN_CTL0, tsync_ctl_l);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_CTL1, tsync_ctl_h);

	return 0;
}

static int
i40e_timesync_disable(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t tsync_ctl_l;
	uint32_t tsync_ctl_h;

	/* Disable timestamping of transmitted PTP packets. */
	tsync_ctl_l = I40E_READ_REG(hw, I40E_PRTTSYN_CTL0);
	tsync_ctl_l &= ~I40E_PRTTSYN_TSYNENA;

	tsync_ctl_h = I40E_READ_REG(hw, I40E_PRTTSYN_CTL1);
	tsync_ctl_h &= ~I40E_PRTTSYN_TSYNENA;

	I40E_WRITE_REG(hw, I40E_PRTTSYN_CTL0, tsync_ctl_l);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_CTL1, tsync_ctl_h);

	/* Reset the timesync increment value. */
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_L, 0x0);
	I40E_WRITE_REG(hw, I40E_PRTTSYN_INC_H, 0x0);

	return 0;
}

static int
i40e_timesync_read_rx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp, uint32_t flags)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_adapter *adapter =
		(struct i40e_adapter *)dev->data->dev_private;

	uint32_t sync_status;
	uint32_t index = flags & 0x03;
	uint64_t rx_tstamp_cycles;
	uint64_t ns;

	sync_status = I40E_READ_REG(hw, I40E_PRTTSYN_STAT_1);
	if ((sync_status & (1 << index)) == 0)
		return -EINVAL;

	rx_tstamp_cycles = i40e_read_rx_tstamp_cyclecounter(dev, index);
	ns = rte_timecounter_update(&adapter->rx_tstamp_tc, rx_tstamp_cycles);
	*timestamp = rte_ns_to_timespec(ns);

	return 0;
}

static int
i40e_timesync_read_tx_timestamp(struct rte_eth_dev *dev,
				struct timespec *timestamp)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_adapter *adapter =
		(struct i40e_adapter *)dev->data->dev_private;

	uint32_t sync_status;
	uint64_t tx_tstamp_cycles;
	uint64_t ns;

	sync_status = I40E_READ_REG(hw, I40E_PRTTSYN_STAT_0);
	if ((sync_status & I40E_PRTTSYN_STAT_0_TXTIME_MASK) == 0)
		return -EINVAL;

	tx_tstamp_cycles = i40e_read_tx_tstamp_cyclecounter(dev);
	ns = rte_timecounter_update(&adapter->tx_tstamp_tc, tx_tstamp_cycles);
	*timestamp = rte_ns_to_timespec(ns);

	return 0;
}

/*
 * i40e_parse_dcb_configure - parse dcb configure from user
 * @dev: the device being configured
 * @dcb_cfg: pointer of the result of parse
 * @*tc_map: bit map of enabled traffic classes
 *
 * Returns 0 on success, negative value on failure
 */
static int
i40e_parse_dcb_configure(struct rte_eth_dev *dev,
			 struct i40e_dcbx_config *dcb_cfg,
			 uint8_t *tc_map)
{
	struct rte_eth_dcb_rx_conf *dcb_rx_conf;
	uint8_t i, tc_bw, bw_lf;

	memset(dcb_cfg, 0, sizeof(struct i40e_dcbx_config));

	dcb_rx_conf = &dev->data->dev_conf.rx_adv_conf.dcb_rx_conf;
	if (dcb_rx_conf->nb_tcs > I40E_MAX_TRAFFIC_CLASS) {
		PMD_INIT_LOG(ERR, "number of tc exceeds max.");
		return -EINVAL;
	}

	/* assume each tc has the same bw */
	tc_bw = I40E_MAX_PERCENT / dcb_rx_conf->nb_tcs;
	for (i = 0; i < dcb_rx_conf->nb_tcs; i++)
		dcb_cfg->etscfg.tcbwtable[i] = tc_bw;
	/* to ensure the sum of tcbw is equal to 100 */
	bw_lf = I40E_MAX_PERCENT % dcb_rx_conf->nb_tcs;
	for (i = 0; i < bw_lf; i++)
		dcb_cfg->etscfg.tcbwtable[i]++;

	/* assume each tc has the same Transmission Selection Algorithm */
	for (i = 0; i < dcb_rx_conf->nb_tcs; i++)
		dcb_cfg->etscfg.tsatable[i] = I40E_IEEE_TSA_ETS;

	for (i = 0; i < I40E_MAX_USER_PRIORITY; i++)
		dcb_cfg->etscfg.prioritytable[i] =
				dcb_rx_conf->dcb_tc[i];

	/* FW needs one App to configure HW */
	dcb_cfg->numapps = I40E_DEFAULT_DCB_APP_NUM;
	dcb_cfg->app[0].selector = I40E_APP_SEL_ETHTYPE;
	dcb_cfg->app[0].priority = I40E_DEFAULT_DCB_APP_PRIO;
	dcb_cfg->app[0].protocolid = I40E_APP_PROTOID_FCOE;

	if (dcb_rx_conf->nb_tcs == 0)
		*tc_map = 1; /* tc0 only */
	else
		*tc_map = RTE_LEN2MASK(dcb_rx_conf->nb_tcs, uint8_t);

	if (dev->data->dev_conf.dcb_capability_en & ETH_DCB_PFC_SUPPORT) {
		dcb_cfg->pfc.willing = 0;
		dcb_cfg->pfc.pfccap = I40E_MAX_TRAFFIC_CLASS;
		dcb_cfg->pfc.pfcenable = *tc_map;
	}
	return 0;
}


static enum i40e_status_code
i40e_vsi_update_queue_mapping(struct i40e_vsi *vsi,
			      struct i40e_aqc_vsi_properties_data *info,
			      uint8_t enabled_tcmap)
{
	enum i40e_status_code ret;
	int i, total_tc = 0;
	uint16_t qpnum_per_tc, bsf, qp_idx;
	struct rte_eth_dev_data *dev_data = I40E_VSI_TO_DEV_DATA(vsi);
	struct i40e_pf *pf = I40E_VSI_TO_PF(vsi);
	uint16_t used_queues;

	ret = validate_tcmap_parameter(vsi, enabled_tcmap);
	if (ret != I40E_SUCCESS)
		return ret;

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (enabled_tcmap & (1 << i))
			total_tc++;
	}
	if (total_tc == 0)
		total_tc = 1;
	vsi->enabled_tc = enabled_tcmap;

	/* different VSI has different queues assigned */
	if (vsi->type == I40E_VSI_MAIN)
		used_queues = dev_data->nb_rx_queues -
			pf->nb_cfg_vmdq_vsi * RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM;
	else if (vsi->type == I40E_VSI_VMDQ2)
		used_queues = RTE_LIBRTE_I40E_QUEUE_NUM_PER_VM;
	else {
		PMD_INIT_LOG(ERR, "unsupported VSI type.");
		return I40E_ERR_NO_AVAILABLE_VSI;
	}

	qpnum_per_tc = used_queues / total_tc;
	/* Number of queues per enabled TC */
	if (qpnum_per_tc == 0) {
		PMD_INIT_LOG(ERR, " number of queues is less that tcs.");
		return I40E_ERR_INVALID_QP_ID;
	}
	qpnum_per_tc = RTE_MIN(i40e_align_floor(qpnum_per_tc),
				I40E_MAX_Q_PER_TC);
	bsf = rte_bsf32(qpnum_per_tc);

	/**
	 * Configure TC and queue mapping parameters, for enabled TC,
	 * allocate qpnum_per_tc queues to this traffic. For disabled TC,
	 * default queue will serve it.
	 */
	qp_idx = 0;
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (vsi->enabled_tc & (1 << i)) {
			info->tc_mapping[i] = rte_cpu_to_le_16((qp_idx <<
					I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
				(bsf << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT));
			qp_idx += qpnum_per_tc;
		} else
			info->tc_mapping[i] = 0;
	}

	/* Associate queue number with VSI, Keep vsi->nb_qps unchanged */
	if (vsi->type == I40E_VSI_SRIOV) {
		info->mapping_flags |=
			rte_cpu_to_le_16(I40E_AQ_VSI_QUE_MAP_NONCONTIG);
		for (i = 0; i < vsi->nb_qps; i++)
			info->queue_mapping[i] =
				rte_cpu_to_le_16(vsi->base_queue + i);
	} else {
		info->mapping_flags |=
			rte_cpu_to_le_16(I40E_AQ_VSI_QUE_MAP_CONTIG);
		info->queue_mapping[0] = rte_cpu_to_le_16(vsi->base_queue);
	}
	info->valid_sections |=
		rte_cpu_to_le_16(I40E_AQ_VSI_PROP_QUEUE_MAP_VALID);

	return I40E_SUCCESS;
}

/*
 * i40e_config_switch_comp_tc - Configure VEB tc setting for given TC map
 * @veb: VEB to be configured
 * @tc_map: enabled TC bitmap
 *
 * Returns 0 on success, negative value on failure
 */
static enum i40e_status_code
i40e_config_switch_comp_tc(struct i40e_veb *veb, uint8_t tc_map)
{
	struct i40e_aqc_configure_switching_comp_bw_config_data veb_bw;
	struct i40e_aqc_query_switching_comp_bw_config_resp bw_query;
	struct i40e_aqc_query_switching_comp_ets_config_resp ets_query;
	struct i40e_hw *hw = I40E_VSI_TO_HW(veb->associate_vsi);
	enum i40e_status_code ret = I40E_SUCCESS;
	int i;
	uint32_t bw_max;

	/* Check if enabled_tc is same as existing or new TCs */
	if (veb->enabled_tc == tc_map)
		return ret;

	/* configure tc bandwidth */
	memset(&veb_bw, 0, sizeof(veb_bw));
	veb_bw.tc_valid_bits = tc_map;
	/* Enable ETS TCs with equal BW Share for now across all VSIs */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (tc_map & BIT_ULL(i))
			veb_bw.tc_bw_share_credits[i] = 1;
	}
	ret = i40e_aq_config_switch_comp_bw_config(hw, veb->seid,
						   &veb_bw, NULL);
	if (ret) {
		PMD_INIT_LOG(ERR,
			"AQ command Config switch_comp BW allocation per TC failed = %d",
			hw->aq.asq_last_status);
		return ret;
	}

	memset(&ets_query, 0, sizeof(ets_query));
	ret = i40e_aq_query_switch_comp_ets_config(hw, veb->seid,
						   &ets_query, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR,
			"Failed to get switch_comp ETS configuration %u",
			hw->aq.asq_last_status);
		return ret;
	}
	memset(&bw_query, 0, sizeof(bw_query));
	ret = i40e_aq_query_switch_comp_bw_config(hw, veb->seid,
						  &bw_query, NULL);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR,
			"Failed to get switch_comp bandwidth configuration %u",
			hw->aq.asq_last_status);
		return ret;
	}

	/* store and print out BW info */
	veb->bw_info.bw_limit = rte_le_to_cpu_16(ets_query.port_bw_limit);
	veb->bw_info.bw_max = ets_query.tc_bw_max;
	PMD_DRV_LOG(DEBUG, "switch_comp bw limit:%u", veb->bw_info.bw_limit);
	PMD_DRV_LOG(DEBUG, "switch_comp max_bw:%u", veb->bw_info.bw_max);
	bw_max = rte_le_to_cpu_16(bw_query.tc_bw_max[0]) |
		    (rte_le_to_cpu_16(bw_query.tc_bw_max[1]) <<
		     I40E_16_BIT_WIDTH);
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		veb->bw_info.bw_ets_share_credits[i] =
				bw_query.tc_bw_share_credits[i];
		veb->bw_info.bw_ets_credits[i] =
				rte_le_to_cpu_16(bw_query.tc_bw_limits[i]);
		/* 4 bits per TC, 4th bit is reserved */
		veb->bw_info.bw_ets_max[i] =
			(uint8_t)((bw_max >> (i * I40E_4_BIT_WIDTH)) &
				  RTE_LEN2MASK(3, uint8_t));
		PMD_DRV_LOG(DEBUG, "\tVEB TC%u:share credits %u", i,
			    veb->bw_info.bw_ets_share_credits[i]);
		PMD_DRV_LOG(DEBUG, "\tVEB TC%u:credits %u", i,
			    veb->bw_info.bw_ets_credits[i]);
		PMD_DRV_LOG(DEBUG, "\tVEB TC%u: max credits: %u", i,
			    veb->bw_info.bw_ets_max[i]);
	}

	veb->enabled_tc = tc_map;

	return ret;
}


/*
 * i40e_vsi_config_tc - Configure VSI tc setting for given TC map
 * @vsi: VSI to be configured
 * @tc_map: enabled TC bitmap
 *
 * Returns 0 on success, negative value on failure
 */
static enum i40e_status_code
i40e_vsi_config_tc(struct i40e_vsi *vsi, uint8_t tc_map)
{
	struct i40e_aqc_configure_vsi_tc_bw_data bw_data;
	struct i40e_vsi_context ctxt;
	struct i40e_hw *hw = I40E_VSI_TO_HW(vsi);
	enum i40e_status_code ret = I40E_SUCCESS;
	int i;

	/* Check if enabled_tc is same as existing or new TCs */
	if (vsi->enabled_tc == tc_map)
		return ret;

	/* configure tc bandwidth */
	memset(&bw_data, 0, sizeof(bw_data));
	bw_data.tc_valid_bits = tc_map;
	/* Enable ETS TCs with equal BW Share for now across all VSIs */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (tc_map & BIT_ULL(i))
			bw_data.tc_bw_credits[i] = 1;
	}
	ret = i40e_aq_config_vsi_tc_bw(hw, vsi->seid, &bw_data, NULL);
	if (ret) {
		PMD_INIT_LOG(ERR,
			"AQ command Config VSI BW allocation per TC failed = %d",
			hw->aq.asq_last_status);
		goto out;
	}
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		vsi->info.qs_handle[i] = bw_data.qs_handles[i];

	/* Update Queue Pairs Mapping for currently enabled UPs */
	ctxt.seid = vsi->seid;
	ctxt.pf_num = hw->pf_id;
	ctxt.vf_num = 0;
	ctxt.uplink_seid = vsi->uplink_seid;
	ctxt.info = vsi->info;
	i40e_get_cap(hw);
	ret = i40e_vsi_update_queue_mapping(vsi, &ctxt.info, tc_map);
	if (ret)
		goto out;

	/* Update the VSI after updating the VSI queue-mapping information */
	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to configure TC queue mapping = %d",
			hw->aq.asq_last_status);
		goto out;
	}
	/* update the local VSI info with updated queue map */
	(void)rte_memcpy(&vsi->info.tc_mapping, &ctxt.info.tc_mapping,
					sizeof(vsi->info.tc_mapping));
	(void)rte_memcpy(&vsi->info.queue_mapping,
			&ctxt.info.queue_mapping,
		sizeof(vsi->info.queue_mapping));
	vsi->info.mapping_flags = ctxt.info.mapping_flags;
	vsi->info.valid_sections = 0;

	/* query and update current VSI BW information */
	ret = i40e_vsi_get_bw_config(vsi);
	if (ret) {
		PMD_INIT_LOG(ERR,
			 "Failed updating vsi bw info, err %s aq_err %s",
			 i40e_stat_str(hw, ret),
			 i40e_aq_str(hw, hw->aq.asq_last_status));
		goto out;
	}

	vsi->enabled_tc = tc_map;

out:
	return ret;
}

/*
 * i40e_dcb_hw_configure - program the dcb setting to hw
 * @pf: pf the configuration is taken on
 * @new_cfg: new configuration
 * @tc_map: enabled TC bitmap
 *
 * Returns 0 on success, negative value on failure
 */
static enum i40e_status_code
i40e_dcb_hw_configure(struct i40e_pf *pf,
		      struct i40e_dcbx_config *new_cfg,
		      uint8_t tc_map)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_dcbx_config *old_cfg = &hw->local_dcbx_config;
	struct i40e_vsi *main_vsi = pf->main_vsi;
	struct i40e_vsi_list *vsi_list;
	enum i40e_status_code ret;
	int i;
	uint32_t val;

	/* Use the FW API if FW > v4.4*/
	if (!(((hw->aq.fw_maj_ver == 4) && (hw->aq.fw_min_ver >= 4)) ||
	      (hw->aq.fw_maj_ver >= 5))) {
		PMD_INIT_LOG(ERR,
			"FW < v4.4, can not use FW LLDP API to configure DCB");
		return I40E_ERR_FIRMWARE_API_VERSION;
	}

	/* Check if need reconfiguration */
	if (!memcmp(new_cfg, old_cfg, sizeof(struct i40e_dcbx_config))) {
		PMD_INIT_LOG(ERR, "No Change in DCB Config required.");
		return I40E_SUCCESS;
	}

	/* Copy the new config to the current config */
	*old_cfg = *new_cfg;
	old_cfg->etsrec = old_cfg->etscfg;
	ret = i40e_set_dcb_config(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Set DCB Config failed, err %s aq_err %s",
			 i40e_stat_str(hw, ret),
			 i40e_aq_str(hw, hw->aq.asq_last_status));
		return ret;
	}
	/* set receive Arbiter to RR mode and ETS scheme by default */
	for (i = 0; i <= I40E_PRTDCB_RETSTCC_MAX_INDEX; i++) {
		val = I40E_READ_REG(hw, I40E_PRTDCB_RETSTCC(i));
		val &= ~(I40E_PRTDCB_RETSTCC_BWSHARE_MASK     |
			 I40E_PRTDCB_RETSTCC_UPINTC_MODE_MASK |
			 I40E_PRTDCB_RETSTCC_ETSTC_SHIFT);
		val |= ((uint32_t)old_cfg->etscfg.tcbwtable[i] <<
			I40E_PRTDCB_RETSTCC_BWSHARE_SHIFT) &
			 I40E_PRTDCB_RETSTCC_BWSHARE_MASK;
		val |= ((uint32_t)1 << I40E_PRTDCB_RETSTCC_UPINTC_MODE_SHIFT) &
			 I40E_PRTDCB_RETSTCC_UPINTC_MODE_MASK;
		val |= ((uint32_t)1 << I40E_PRTDCB_RETSTCC_ETSTC_SHIFT) &
			 I40E_PRTDCB_RETSTCC_ETSTC_MASK;
		I40E_WRITE_REG(hw, I40E_PRTDCB_RETSTCC(i), val);
	}
	/* get local mib to check whether it is configured correctly */
	/* IEEE mode */
	hw->local_dcbx_config.dcbx_mode = I40E_DCBX_MODE_IEEE;
	/* Get Local DCB Config */
	i40e_aq_get_dcb_config(hw, I40E_AQ_LLDP_MIB_LOCAL, 0,
				     &hw->local_dcbx_config);

	/* if Veb is created, need to update TC of it at first */
	if (main_vsi->veb) {
		ret = i40e_config_switch_comp_tc(main_vsi->veb, tc_map);
		if (ret)
			PMD_INIT_LOG(WARNING,
				 "Failed configuring TC for VEB seid=%d",
				 main_vsi->veb->seid);
	}
	/* Update each VSI */
	i40e_vsi_config_tc(main_vsi, tc_map);
	if (main_vsi->veb) {
		TAILQ_FOREACH(vsi_list, &main_vsi->veb->head, list) {
			/* Beside main VSI and VMDQ VSIs, only enable default
			 * TC for other VSIs
			 */
			if (vsi_list->vsi->type == I40E_VSI_VMDQ2)
				ret = i40e_vsi_config_tc(vsi_list->vsi,
							 tc_map);
			else
				ret = i40e_vsi_config_tc(vsi_list->vsi,
							 I40E_DEFAULT_TCMAP);
			if (ret)
				PMD_INIT_LOG(WARNING,
					"Failed configuring TC for VSI seid=%d",
					vsi_list->vsi->seid);
			/* continue */
		}
	}
	return I40E_SUCCESS;
}

/*
 * i40e_dcb_init_configure - initial dcb config
 * @dev: device being configured
 * @sw_dcb: indicate whether dcb is sw configured or hw offload
 *
 * Returns 0 on success, negative value on failure
 */
static int
i40e_dcb_init_configure(struct rte_eth_dev *dev, bool sw_dcb)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int i, ret = 0;

	if ((pf->flags & I40E_FLAG_DCB) == 0) {
		PMD_INIT_LOG(ERR, "HW doesn't support DCB");
		return -ENOTSUP;
	}

	/* DCB initialization:
	 * Update DCB configuration from the Firmware and configure
	 * LLDP MIB change event.
	 */
	if (sw_dcb == TRUE) {
		ret = i40e_init_dcb(hw);
		/* If lldp agent is stopped, the return value from
		 * i40e_init_dcb we expect is failure with I40E_AQ_RC_EPERM
		 * adminq status. Otherwise, it should return success.
		 */
		if ((ret == I40E_SUCCESS) || (ret != I40E_SUCCESS &&
		    hw->aq.asq_last_status == I40E_AQ_RC_EPERM)) {
			memset(&hw->local_dcbx_config, 0,
				sizeof(struct i40e_dcbx_config));
			/* set dcb default configuration */
			hw->local_dcbx_config.etscfg.willing = 0;
			hw->local_dcbx_config.etscfg.maxtcs = 0;
			hw->local_dcbx_config.etscfg.tcbwtable[0] = 100;
			hw->local_dcbx_config.etscfg.tsatable[0] =
						I40E_IEEE_TSA_ETS;
			/* all UPs mapping to TC0 */
			for (i = 0; i < I40E_MAX_USER_PRIORITY; i++)
				hw->local_dcbx_config.etscfg.prioritytable[i] = 0;
			hw->local_dcbx_config.etsrec =
				hw->local_dcbx_config.etscfg;
			hw->local_dcbx_config.pfc.willing = 0;
			hw->local_dcbx_config.pfc.pfccap =
						I40E_MAX_TRAFFIC_CLASS;
			/* FW needs one App to configure HW */
			hw->local_dcbx_config.numapps = 1;
			hw->local_dcbx_config.app[0].selector =
						I40E_APP_SEL_ETHTYPE;
			hw->local_dcbx_config.app[0].priority = 3;
			hw->local_dcbx_config.app[0].protocolid =
						I40E_APP_PROTOID_FCOE;
			ret = i40e_set_dcb_config(hw);
			if (ret) {
				PMD_INIT_LOG(ERR,
					"default dcb config fails. err = %d, aq_err = %d.",
					ret, hw->aq.asq_last_status);
				return -ENOSYS;
			}
		} else {
			PMD_INIT_LOG(ERR,
				"DCB initialization in FW fails, err = %d, aq_err = %d.",
				ret, hw->aq.asq_last_status);
			return -ENOTSUP;
		}
	} else {
		ret = i40e_aq_start_lldp(hw, NULL);
		if (ret != I40E_SUCCESS)
			PMD_INIT_LOG(DEBUG, "Failed to start lldp");

		ret = i40e_init_dcb(hw);
		if (!ret) {
			if (hw->dcbx_status == I40E_DCBX_STATUS_DISABLED) {
				PMD_INIT_LOG(ERR,
					"HW doesn't support DCBX offload.");
				return -ENOTSUP;
			}
		} else {
			PMD_INIT_LOG(ERR,
				"DCBX configuration failed, err = %d, aq_err = %d.",
				ret, hw->aq.asq_last_status);
			return -ENOTSUP;
		}
	}
	return 0;
}

/*
 * i40e_dcb_setup - setup dcb related config
 * @dev: device being configured
 *
 * Returns 0 on success, negative value on failure
 */
static int
i40e_dcb_setup(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_dcbx_config dcb_cfg;
	uint8_t tc_map = 0;
	int ret = 0;

	if ((pf->flags & I40E_FLAG_DCB) == 0) {
		PMD_INIT_LOG(ERR, "HW doesn't support DCB");
		return -ENOTSUP;
	}

	if (pf->vf_num != 0)
		PMD_INIT_LOG(DEBUG, " DCB only works on pf and vmdq vsis.");

	ret = i40e_parse_dcb_configure(dev, &dcb_cfg, &tc_map);
	if (ret) {
		PMD_INIT_LOG(ERR, "invalid dcb config");
		return -EINVAL;
	}
	ret = i40e_dcb_hw_configure(pf, &dcb_cfg, tc_map);
	if (ret) {
		PMD_INIT_LOG(ERR, "dcb sw configure fails");
		return -ENOSYS;
	}

	return 0;
}

static int
i40e_dev_get_dcb_info(struct rte_eth_dev *dev,
		      struct rte_eth_dcb_info *dcb_info)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vsi *vsi = pf->main_vsi;
	struct i40e_dcbx_config *dcb_cfg = &hw->local_dcbx_config;
	uint16_t bsf, tc_mapping;
	int i, j = 0;

	if (dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_DCB_FLAG)
		dcb_info->nb_tcs = rte_bsf32(vsi->enabled_tc + 1);
	else
		dcb_info->nb_tcs = 1;
	for (i = 0; i < I40E_MAX_USER_PRIORITY; i++)
		dcb_info->prio_tc[i] = dcb_cfg->etscfg.prioritytable[i];
	for (i = 0; i < dcb_info->nb_tcs; i++)
		dcb_info->tc_bws[i] = dcb_cfg->etscfg.tcbwtable[i];

	/* get queue mapping if vmdq is disabled */
	if (!pf->nb_cfg_vmdq_vsi) {
		for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
			if (!(vsi->enabled_tc & (1 << i)))
				continue;
			tc_mapping = rte_le_to_cpu_16(vsi->info.tc_mapping[i]);
			dcb_info->tc_queue.tc_rxq[j][i].base =
				(tc_mapping & I40E_AQ_VSI_TC_QUE_OFFSET_MASK) >>
				I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT;
			dcb_info->tc_queue.tc_txq[j][i].base =
				dcb_info->tc_queue.tc_rxq[j][i].base;
			bsf = (tc_mapping & I40E_AQ_VSI_TC_QUE_NUMBER_MASK) >>
				I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT;
			dcb_info->tc_queue.tc_rxq[j][i].nb_queue = 1 << bsf;
			dcb_info->tc_queue.tc_txq[j][i].nb_queue =
				dcb_info->tc_queue.tc_rxq[j][i].nb_queue;
		}
		return 0;
	}

	/* get queue mapping if vmdq is enabled */
	do {
		vsi = pf->vmdq[j].vsi;
		for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
			if (!(vsi->enabled_tc & (1 << i)))
				continue;
			tc_mapping = rte_le_to_cpu_16(vsi->info.tc_mapping[i]);
			dcb_info->tc_queue.tc_rxq[j][i].base =
				(tc_mapping & I40E_AQ_VSI_TC_QUE_OFFSET_MASK) >>
				I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT;
			dcb_info->tc_queue.tc_txq[j][i].base =
				dcb_info->tc_queue.tc_rxq[j][i].base;
			bsf = (tc_mapping & I40E_AQ_VSI_TC_QUE_NUMBER_MASK) >>
				I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT;
			dcb_info->tc_queue.tc_rxq[j][i].nb_queue = 1 << bsf;
			dcb_info->tc_queue.tc_txq[j][i].nb_queue =
				dcb_info->tc_queue.tc_rxq[j][i].nb_queue;
		}
		j++;
	} while (j < RTE_MIN(pf->nb_cfg_vmdq_vsi, ETH_MAX_VMDQ_POOL));
	return 0;
}

static int
i40e_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t interval =
		i40e_calc_itr_interval(RTE_LIBRTE_I40E_ITR_INTERVAL);
	uint16_t msix_intr;

	msix_intr = intr_handle->intr_vec[queue_id];
	if (msix_intr == I40E_MISC_VEC_ID)
		I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTL0,
			       I40E_PFINT_DYN_CTLN_INTENA_MASK |
			       I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
			       (0 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT) |
			       (interval <<
				I40E_PFINT_DYN_CTLN_INTERVAL_SHIFT));
	else
		I40E_WRITE_REG(hw,
			       I40E_PFINT_DYN_CTLN(msix_intr -
						   I40E_RX_VEC_START),
			       I40E_PFINT_DYN_CTLN_INTENA_MASK |
			       I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
			       (0 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT) |
			       (interval <<
				I40E_PFINT_DYN_CTLN_INTERVAL_SHIFT));

	I40E_WRITE_FLUSH(hw);
	rte_intr_enable(&pci_dev->intr_handle);

	return 0;
}

static int
i40e_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t msix_intr;

	msix_intr = intr_handle->intr_vec[queue_id];
	if (msix_intr == I40E_MISC_VEC_ID)
		I40E_WRITE_REG(hw, I40E_PFINT_DYN_CTL0, 0);
	else
		I40E_WRITE_REG(hw,
			       I40E_PFINT_DYN_CTLN(msix_intr -
						   I40E_RX_VEC_START),
			       0);
	I40E_WRITE_FLUSH(hw);

	return 0;
}

static int i40e_get_regs(struct rte_eth_dev *dev,
			 struct rte_dev_reg_info *regs)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t *ptr_data = regs->data;
	uint32_t reg_idx, arr_idx, arr_idx2, reg_offset;
	const struct i40e_reg_info *reg_info;

	if (ptr_data == NULL) {
		regs->length = I40E_GLGEN_STAT_CLEAR + 4;
		regs->width = sizeof(uint32_t);
		return 0;
	}

	/* The first few registers have to be read using AQ operations */
	reg_idx = 0;
	while (i40e_regs_adminq[reg_idx].name) {
		reg_info = &i40e_regs_adminq[reg_idx++];
		for (arr_idx = 0; arr_idx <= reg_info->count1; arr_idx++)
			for (arr_idx2 = 0;
					arr_idx2 <= reg_info->count2;
					arr_idx2++) {
				reg_offset = arr_idx * reg_info->stride1 +
					arr_idx2 * reg_info->stride2;
				reg_offset += reg_info->base_addr;
				ptr_data[reg_offset >> 2] =
					i40e_read_rx_ctl(hw, reg_offset);
			}
	}

	/* The remaining registers can be read using primitives */
	reg_idx = 0;
	while (i40e_regs_others[reg_idx].name) {
		reg_info = &i40e_regs_others[reg_idx++];
		for (arr_idx = 0; arr_idx <= reg_info->count1; arr_idx++)
			for (arr_idx2 = 0;
					arr_idx2 <= reg_info->count2;
					arr_idx2++) {
				reg_offset = arr_idx * reg_info->stride1 +
					arr_idx2 * reg_info->stride2;
				reg_offset += reg_info->base_addr;
				ptr_data[reg_offset >> 2] =
					I40E_READ_REG(hw, reg_offset);
			}
	}

	return 0;
}

static int i40e_get_eeprom_length(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Convert word count to byte count */
	return hw->nvm.sr_size << 1;
}

static int i40e_get_eeprom(struct rte_eth_dev *dev,
			   struct rte_dev_eeprom_info *eeprom)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t *data = eeprom->data;
	uint16_t offset, length, cnt_words;
	int ret_code;

	offset = eeprom->offset >> 1;
	length = eeprom->length >> 1;
	cnt_words = length;

	if (offset > hw->nvm.sr_size ||
		offset + length > hw->nvm.sr_size) {
		PMD_DRV_LOG(ERR, "Requested EEPROM bytes out of range.");
		return -EINVAL;
	}

	eeprom->magic = hw->vendor_id | (hw->device_id << 16);

	ret_code = i40e_read_nvm_buffer(hw, offset, &cnt_words, data);
	if (ret_code != I40E_SUCCESS || cnt_words != length) {
		PMD_DRV_LOG(ERR, "EEPROM read failed.");
		return -EIO;
	}

	return 0;
}

static void i40e_set_default_mac_addr(struct rte_eth_dev *dev,
				      struct ether_addr *mac_addr)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!is_valid_assigned_ether_addr(mac_addr)) {
		PMD_DRV_LOG(ERR, "Tried to set invalid MAC address.");
		return;
	}

	/* Flags: 0x3 updates port address */
	i40e_aq_mac_address_write(hw, 0x3, mac_addr->addr_bytes, NULL);
}

static int
i40e_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_eth_dev_data *dev_data = pf->dev_data;
	uint32_t frame_size = mtu + I40E_ETH_OVERHEAD;
	int ret = 0;

	/* check if mtu is within the allowed range */
	if ((mtu < ETHER_MIN_MTU) || (frame_size > I40E_FRAME_SIZE_MAX))
		return -EINVAL;

	/* mtu setting is forbidden if port is start */
	if (dev_data->dev_started) {
		PMD_DRV_LOG(ERR, "port %d must be stopped before configuration",
			    dev_data->port_id);
		return -EBUSY;
	}

	if (frame_size > ETHER_MAX_LEN)
		dev_data->dev_conf.rxmode.jumbo_frame = 1;
	else
		dev_data->dev_conf.rxmode.jumbo_frame = 0;

	dev_data->dev_conf.rxmode.max_rx_pkt_len = frame_size;

	return ret;
}

/* Restore ethertype filter */
static void
i40e_ethertype_filter_restore(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_ethertype_filter_list
		*ethertype_list = &pf->ethertype.ethertype_list;
	struct i40e_ethertype_filter *f;
	struct i40e_control_filter_stats stats;
	uint16_t flags;

	TAILQ_FOREACH(f, ethertype_list, rules) {
		flags = 0;
		if (!(f->flags & RTE_ETHTYPE_FLAGS_MAC))
			flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_IGNORE_MAC;
		if (f->flags & RTE_ETHTYPE_FLAGS_DROP)
			flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_DROP;
		flags |= I40E_AQC_ADD_CONTROL_PACKET_FLAGS_TO_QUEUE;

		memset(&stats, 0, sizeof(stats));
		i40e_aq_add_rem_control_packet_filter(hw,
					    f->input.mac_addr.addr_bytes,
					    f->input.ether_type,
					    flags, pf->main_vsi->seid,
					    f->queue, 1, &stats, NULL);
	}
	PMD_DRV_LOG(INFO, "Ethertype filter:"
		    " mac_etype_used = %u, etype_used = %u,"
		    " mac_etype_free = %u, etype_free = %u",
		    stats.mac_etype_used, stats.etype_used,
		    stats.mac_etype_free, stats.etype_free);
}

/* Restore tunnel filter */
static void
i40e_tunnel_filter_restore(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi;
	struct i40e_pf_vf *vf;
	struct i40e_tunnel_filter_list
		*tunnel_list = &pf->tunnel.tunnel_list;
	struct i40e_tunnel_filter *f;
	struct i40e_aqc_add_rm_cloud_filt_elem_ext cld_filter;
	bool big_buffer = 0;

	TAILQ_FOREACH(f, tunnel_list, rules) {
		if (!f->is_to_vf)
			vsi = pf->main_vsi;
		else {
			vf = &pf->vfs[f->vf_id];
			vsi = vf->vsi;
		}
		memset(&cld_filter, 0, sizeof(cld_filter));
		ether_addr_copy((struct ether_addr *)&f->input.outer_mac,
			(struct ether_addr *)&cld_filter.element.outer_mac);
		ether_addr_copy((struct ether_addr *)&f->input.inner_mac,
			(struct ether_addr *)&cld_filter.element.inner_mac);
		cld_filter.element.inner_vlan = f->input.inner_vlan;
		cld_filter.element.flags = f->input.flags;
		cld_filter.element.tenant_id = f->input.tenant_id;
		cld_filter.element.queue_number = f->queue;
		rte_memcpy(cld_filter.general_fields,
			   f->input.general_fields,
			   sizeof(f->input.general_fields));

		if (((f->input.flags &
		     I40E_AQC_ADD_CLOUD_FILTER_TEID_MPLSoUDP) ==
		     I40E_AQC_ADD_CLOUD_FILTER_TEID_MPLSoUDP) ||
		    ((f->input.flags &
		     I40E_AQC_ADD_CLOUD_FILTER_TEID_MPLSoGRE) ==
		     I40E_AQC_ADD_CLOUD_FILTER_TEID_MPLSoGRE) ||
		    ((f->input.flags &
		     I40E_AQC_ADD_CLOUD_FILTER_CUSTOM_QINQ) ==
		     I40E_AQC_ADD_CLOUD_FILTER_CUSTOM_QINQ))
			big_buffer = 1;

		if (big_buffer)
			i40e_aq_add_cloud_filters_big_buffer(hw,
					     vsi->seid, &cld_filter, 1);
		else
			i40e_aq_add_cloud_filters(hw, vsi->seid,
						  &cld_filter.element, 1);
	}
}

static void
i40e_filter_restore(struct i40e_pf *pf)
{
	i40e_ethertype_filter_restore(pf);
	i40e_tunnel_filter_restore(pf);
	i40e_fdir_filter_restore(pf);
}

static bool
is_device_supported(struct rte_eth_dev *dev, struct rte_pci_driver *drv)
{
	if (strcmp(dev->device->driver->name, drv->driver.name))
		return false;

	return true;
}

bool
is_i40e_supported(struct rte_eth_dev *dev)
{
	return is_device_supported(dev, &rte_i40e_pmd);
}

/* Create a QinQ cloud filter
 *
 * The Fortville NIC has limited resources for tunnel filters,
 * so we can only reuse existing filters.
 *
 * In step 1 we define which Field Vector fields can be used for
 * filter types.
 * As we do not have the inner tag defined as a field,
 * we have to define it first, by reusing one of L1 entries.
 *
 * In step 2 we are replacing one of existing filter types with
 * a new one for QinQ.
 * As we reusing L1 and replacing L2, some of the default filter
 * types will disappear,which depends on L1 and L2 entries we reuse.
 *
 * Step 1: Create L1 filter of outer vlan (12b) + inner vlan (12b)
 *
 * 1.	Create L1 filter of outer vlan (12b) which will be in use
 *		later when we define the cloud filter.
 *	a.	Valid_flags.replace_cloud = 0
 *	b.	Old_filter = 10 (Stag_Inner_Vlan)
 *	c.	New_filter = 0x10
 *	d.	TR bit = 0xff (optional, not used here)
 *	e.	Buffer – 2 entries:
 *		i.	Byte 0 = 8 (outer vlan FV index).
 *			Byte 1 = 0 (rsv)
 *			Byte 2-3 = 0x0fff
 *		ii.	Byte 0 = 37 (inner vlan FV index).
 *			Byte 1 =0 (rsv)
 *			Byte 2-3 = 0x0fff
 *
 * Step 2:
 * 2.	Create cloud filter using two L1 filters entries: stag and
 *		new filter(outer vlan+ inner vlan)
 *	a.	Valid_flags.replace_cloud = 1
 *	b.	Old_filter = 1 (instead of outer IP)
 *	c.	New_filter = 0x10
 *	d.	Buffer – 2 entries:
 *		i.	Byte 0 = 0x80 | 7 (valid | Stag).
 *			Byte 1-3 = 0 (rsv)
 *		ii.	Byte 8 = 0x80 | 0x10 (valid | new l1 filter step1)
 *			Byte 9-11 = 0 (rsv)
 */
static int
i40e_cloud_filter_qinq_create(struct i40e_pf *pf)
{
	int ret = -ENOTSUP;
	struct i40e_aqc_replace_cloud_filters_cmd  filter_replace;
	struct i40e_aqc_replace_cloud_filters_cmd_buf  filter_replace_buf;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);

	/* Init */
	memset(&filter_replace, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd));
	memset(&filter_replace_buf, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd_buf));

	/* create L1 filter */
	filter_replace.old_filter_type =
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG_IVLAN;
	filter_replace.new_filter_type = I40E_AQC_ADD_CLOUD_FILTER_CUSTOM_QINQ;
	filter_replace.tr_bit = 0;

	/* Prepare the buffer, 2 entries */
	filter_replace_buf.data[0] = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_VLAN;
	filter_replace_buf.data[0] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	/* Field Vector 12b mask */
	filter_replace_buf.data[2] = 0xff;
	filter_replace_buf.data[3] = 0x0f;
	filter_replace_buf.data[4] =
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_INNER_VLAN;
	filter_replace_buf.data[4] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	/* Field Vector 12b mask */
	filter_replace_buf.data[6] = 0xff;
	filter_replace_buf.data[7] = 0x0f;
	ret = i40e_aq_replace_cloud_filters(hw, &filter_replace,
			&filter_replace_buf);
	if (ret != I40E_SUCCESS)
		return ret;

	/* Apply the second L2 cloud filter */
	memset(&filter_replace, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd));
	memset(&filter_replace_buf, 0,
	       sizeof(struct i40e_aqc_replace_cloud_filters_cmd_buf));

	/* create L2 filter, input for L2 filter will be L1 filter  */
	filter_replace.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER;
	filter_replace.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_OIP;
	filter_replace.new_filter_type = I40E_AQC_ADD_CLOUD_FILTER_CUSTOM_QINQ;

	/* Prepare the buffer, 2 entries */
	filter_replace_buf.data[0] = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG;
	filter_replace_buf.data[0] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	filter_replace_buf.data[4] = I40E_AQC_ADD_CLOUD_FILTER_CUSTOM_QINQ;
	filter_replace_buf.data[4] |=
		I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED;
	ret = i40e_aq_replace_cloud_filters(hw, &filter_replace,
			&filter_replace_buf);
	return ret;
}

RTE_INIT(i40e_init_log);
static void
i40e_init_log(void)
{
	i40e_logtype_init = rte_log_register("pmd.i40e.init");
	if (i40e_logtype_init >= 0)
		rte_log_set_level(i40e_logtype_init, RTE_LOG_NOTICE);
	i40e_logtype_driver = rte_log_register("pmd.i40e.driver");
	if (i40e_logtype_driver >= 0)
		rte_log_set_level(i40e_logtype_driver, RTE_LOG_NOTICE);
}
