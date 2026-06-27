/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_ETHDEV_H
#define SXE2_ETHDEV_H

#include <rte_compat.h>
#include <rte_kvargs.h>
#include <rte_time.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_tm_driver.h>
#include <rte_io.h>

#include "sxe2_common.h"
#include "sxe2_vsi.h"
#include "sxe2_irq.h"
#include "sxe2_queue.h"
#include "sxe2_mac.h"
#include "sxe2_osal.h"
#include "sxe2_filter.h"

struct sxe2_link_msg {
	uint32_t speed;
	uint8_t status;
};

enum sxe2_fnav_tunnel_flag_type {
	SXE2_FNAV_TUN_FLAG_NO_TUNNEL,
	SXE2_FNAV_TUN_FLAG_TUNNEL,
	SXE2_FNAV_TUN_FLAG_ANY,
};

#define SXE2_VF_MAX_NUM        256
#define SXE2_VSI_MAX_NUM       768
#define SXE2_FRAME_SIZE_MAX    9832
#define SXE2_VLAN_TAG_SIZE     4
#define SXE2_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + 2 * SXE2_VLAN_TAG_SIZE)
#define SXE2_ETH_MAX_LEN (RTE_ETHER_MTU + SXE2_ETH_OVERHEAD)

#ifdef SXE2_TEST
#define SXE2_RESET_ACTIVE_WAIT_COUNT   (5)
#else
#define SXE2_RESET_ACTIVE_WAIT_COUNT   (10000)
#endif
#define SXE2_NO_ACTIVE_CNT           (10)

#define SXE2_WOKER_DELAY_5MS         (5)
#define SXE2_WOKER_DELAY_10MS        (10)
#define SXE2_WOKER_DELAY_20MS        (20)
#define SXE2_WOKER_DELAY_30MS        (30)

#define SXE2_RESET_DETEC_WAIT_COUNT    (100)
#define SXE2_RESET_DONE_WAIT_COUNT     (250)
#define SXE2_RESET_WAIT_MS             (10)

#define SXE2_RESET_WAIT_MIN   (10)
#define SXE2_RESET_WAIT_MAX   (20)
#define upper_32_bits(n) ((uint32_t)(((n) >> 16) >> 16))
#define lower_32_bits(n) ((uint32_t)((n) & 0xffffffff))

#define SXE2_I2C_EEPROM_DEV_ADDR	0xA0
#define SXE2_I2C_EEPROM_DEV_ADDR2	0xA2
#define SXE2_MODULE_TYPE_SFP		0x03
#define SXE2_MODULE_TYPE_QSFP_PLUS	0x0D
#define SXE2_MODULE_TYPE_QSFP28	0x11
#define SXE2_MODULE_SFF_ADDR_MODE	0x04
#define SXE2_MODULE_SFF_DIAG_CAPAB	0x40
#define SXE2_MODULE_REVISION_ADDR	0x01
#define SXE2_MODULE_SFF_8472_COMP	0x5E
#define SXE2_MODULE_SFF_8472_SWAP	0x5C
#define SXE2_MODULE_QSFP_MAX_LEN	640
#define SXE2_MODULE_SFF_8472_UNSUP	0x0
#define SXE2_MODULE_SFF_DDM_IMPLEMENTED	0x40
#define SXE2_MODULE_SFF_SFP_TYPE   0x03
#define SXE2_MODULE_TYPE_QSFP_PLUS	0x0D
#define SXE2_MODULE_TYPE_QSFP28	0x11

#define SXE2_MODULE_SFF_8079		0x1
#define SXE2_MODULE_SFF_8079_LEN	256
#define SXE2_MODULE_SFF_8472		0x2
#define SXE2_MODULE_SFF_8472_LEN	512
#define SXE2_MODULE_SFF_8636		0x3
#define SXE2_MODULE_SFF_8636_LEN	256
#define SXE2_MODULE_SFF_8636_MAX_LEN     640
#define SXE2_MODULE_SFF_8436		0x4
#define SXE2_MODULE_SFF_8436_LEN	256
#define SXE2_MODULE_SFF_8436_MAX_LEN     640

enum sxe2_wk_type {
	SXE2_WK_MONITOR,
	SXE2_WK_MONITOR_IM,
	SXE2_WK_POST,
	SXE2_WK_MBX,
};

enum {
	SXE2_FLAG_LEGACY_RX_ENABLE   = 0,
	SXE2_FLAG_LRO_ENABLE = 1,
	SXE2_FLAG_RXQ_DISABLED = 2,
	SXE2_FLAG_TXQ_DISABLED = 3,
	SXE2_FLAG_DRV_REMOVING = 4,
	SXE2_FLAG_RESET_DETECTED = 5,
	SXE2_FLAG_CORE_RESET_DONE = 6,
	SXE2_FLAG_RESET_ACTIVED = 7,
	SXE2_FLAG_RESET_PENDING = 8,
	SXE2_FLAG_RESET_REQUEST = 9,
	SXE2_FLAGS_RESET_PROCESS_DONE = 10,
	SXE2_FLAG_RESET_FAILED = 11,
	SXE2_FLAG_DRV_PROBE_DONE = 12,
	SXE2_FLAG_NETDEV_REGISTED = 13,
	SXE2_FLAG_DRV_UP = 15,
	SXE2_FLAG_DCB_ENABLE = 16,
	SXE2_FLAG_FLTR_SYNC = 17,

	SXE2_FLAG_EVENT_IRQ_DISABLED = 18,
	SXE2_FLAG_SUSPEND = 19,
	SXE2_FLAG_FNAV_ENABLE = 20,

	SXE2_FLAGS_NBITS
};

struct sxe2_devargs {
	uint8_t flow_dup_pattern_mode;
	uint8_t func_flow_direct_en;
	uint8_t fnav_stat_type;
	uint8_t no_sched_mode;
	uint8_t sched_layer_mode;
	uint8_t rx_low_latency;
};

#define SXE2_PCI_MAP_BAR_INVALID ((uint8_t)0xff)
#define SXE2_PCI_MAP_INVALID_VAL ((uint32_t)0xffffffff)

enum sxe2_pci_map_resource {
	SXE2_PCI_MAP_RES_INVALID = 0,
	SXE2_PCI_MAP_RES_DOORBELL_TX,
	SXE2_PCI_MAP_RES_DOORBELL_RX_TAIL,
	SXE2_PCI_MAP_RES_IRQ_DYN,
	SXE2_PCI_MAP_RES_IRQ_ITR,
	SXE2_PCI_MAP_RES_IRQ_MSIX,
	SXE2_PCI_MAP_RES_PTP,
	SXE2_PCI_MAP_RES_MAX_COUNT,
};

enum sxe2_udp_tunnel_protocol {
	SXE2_UDP_TUNNEL_PROTOCOL_VXLAN = 0,
	SXE2_UDP_TUNNEL_PROTOCOL_VXLAN_GPE,
	SXE2_UDP_TUNNEL_PROTOCOL_GENEVE,
	SXE2_UDP_TUNNEL_PROTOCOL_GTP_C = 4,
	SXE2_UDP_TUNNEL_PROTOCOL_GTP_U,
	SXE2_UDP_TUNNEL_PROTOCOL_PFCP,
	SXE2_UDP_TUNNEL_PROTOCOL_ECPRI,
	SXE2_UDP_TUNNEL_PROTOCOL_MPLS,
	SXE2_UDP_TUNNEL_PROTOCOL_NVGRE = 10,
	SXE2_UDP_TUNNEL_PROTOCOL_L2TP,
	SXE2_UDP_TUNNEL_PROTOCOL_TEREDO,
	SXE2_UDP_TUNNEL_MAX,
};

struct sxe2_pci_map_addr_info {
	uint64_t addr_base;
	uint8_t bar_idx;
	uint8_t reg_width;
};

struct sxe2_pci_map_segment_info {
	enum sxe2_pci_map_resource	type;
	void				*addr;
	uint64_t			page_inner_offset;
	uint64_t			len;
};

struct sxe2_pci_map_bar_info {
	uint8_t    bar_idx;
	uint8_t    map_cnt;
	struct sxe2_pci_map_segment_info    *seg_info;
};

struct sxe2_pci_map_context {
	uint8_t    bar_cnt;
	struct sxe2_pci_map_bar_info *bar_info;
	struct sxe2_pci_map_addr_info *addr_info;
};

struct sxe2_dev_mac_info {
	uint8_t perm_addr[SXE2_ETH_ALEN];
};

struct sxe2_pci_info {
	uint64_t                     serial_number;
	uint8_t                      bus_devid;
	uint8_t                      bus_function;
	uint16_t                     max_vfs;
};

struct sxe2_fw_info {
	uint8_t                      main_version_id;
	uint8_t                      sub_version_id;
	uint8_t                      fix_version_id;
	uint8_t                      build_id;
};

struct sxe2_dev_info {
	struct rte_eth_dev_data        *dev_data;
	struct sxe2_pci_info           pci;
	struct sxe2_fw_info            fw;
	struct sxe2_dev_mac_info       mac;
};

enum sxe2_udp_tunnel_status {
	SXE2_UDP_TUNNEL_DISABLE = 0x0,
	SXE2_UDP_TUNNEL_ENABLE,
};

struct sxe2_udp_tunnel_cfg {
	uint8_t			protocol;
	uint8_t			dev_status;
	uint16_t			dev_port;
	uint16_t			dev_ref_cnt;

	uint16_t			fw_port;
	uint8_t			fw_status;
	uint8_t			fw_dst_en;
	uint8_t			fw_src_en;
	uint8_t			fw_used;
};

struct sxe2_udp_tunnel_ctx {
	struct sxe2_udp_tunnel_cfg   tunnel_conf[SXE2_UDP_TUNNEL_MAX];
	rte_spinlock_t                lock;
};

struct sxe2_repr_context {
	uint16_t nb_vf;
	uint16_t nb_repr_vf;
	struct rte_eth_dev **vf_rep_eth_dev;
	struct sxe2_drv_vsi_caps repr_vf_id[SXE2_VF_MAX_NUM];
};

struct sxe2_repr_private_data {
	struct rte_eth_dev *rep_eth_dev;
	struct sxe2_adapter *parent_adapter;

	struct sxe2_vsi *cp_vsi;
	uint16_t repr_q_id;

	uint16_t repr_id;
	uint16_t repr_pf_id;
	uint16_t repr_vf_id;
	uint16_t repr_vf_vsi_id;
	uint16_t repr_vf_k_vsi_id;
	uint16_t repr_vf_u_vsi_id;
};

struct sxe2_sched_hw_cap {
	uint32_t tm_layers;
	uint8_t root_max_children;
	uint8_t prio_max;
	uint8_t adj_lvl;
};

struct sxe2_link_context {
	rte_spinlock_t link_lock;
	bool link_up;
	uint32_t  speed;
};

struct sxe2_filter_context {
	rte_spinlock_t filter_lock;
	struct sxe2_vlan_info               vlan_info;
	struct sxe2_uc_filter_list_head    uc_list;
	struct sxe2_mc_filter_list_head    mc_list;
	struct sxe2_vlan_filter_list_head  vlan_list;
	uint8_t                                 uc_num;
	uint8_t                                 mc_num;
	uint8_t                                 vlan_num;
	uint8_t                                 rsv;
	uint32_t hw_promisc_flags;
	uint32_t cur_promisc_flags;

	bool hw_uplink_config;
	bool cur_uplink_config;
	bool hw_repr_config;
	bool cur_repr_config;
	bool hw_l2_config;
	bool cur_l2_config;
};

struct sxe2_adapter {
	struct sxe2_common_device      *cdev;
	struct sxe2_dev_info            dev_info;
	struct rte_pci_device            *pci_dev;
	struct sxe2_repr_private_data  *repr_priv_data;
	struct sxe2_pci_map_context   map_ctxt;
	struct sxe2_irq_context       irq_ctxt;
	struct sxe2_queue_context     q_ctxt;
	struct sxe2_vsi_context       vsi_ctxt;
	struct sxe2_filter_context    filter_ctxt;
	struct sxe2_link_context      link_ctxt;
	struct sxe2_devargs           devargs;
	struct sxe2_switchdev_info    switchdev_info;
	bool                          rule_started;
	bool                          flow_isolated;
	uint16_t                           dev_port_id;
	uint64_t                           cap_flags;
	enum sxe2_dev_type            dev_type;
	struct rte_ether_addr           mac_addr;
	uint8_t                              port_idx;
	uint8_t                              pf_idx;
	uint32_t                             tx_mode_flags;
	uint32_t                             rx_mode_flags;
	uint8_t                              started;
};

#define SXE2_DEV_PRIVATE_TO_ADAPTER(dev) \
	((struct sxe2_adapter *)(dev)->data->dev_private)

void *sxe2_pci_map_addr_get(struct sxe2_adapter *adapter,
			    enum sxe2_pci_map_resource res_type,
			    uint16_t idx_in_func);

struct sxe2_pci_map_bar_info *sxe2_dev_get_bar_info(struct sxe2_adapter *adapter,
						    enum sxe2_pci_map_resource res_type);

int32_t sxe2_dev_pci_seg_map(struct sxe2_adapter *adapter,
			     enum sxe2_pci_map_resource res_type,
			     uint64_t org_len, uint64_t org_offset);

int32_t sxe2_dev_pci_res_seg_map(struct sxe2_adapter *adapter,
				 uint32_t res_type,
				 uint32_t item_cnt,
				 uint32_t item_base);

void sxe2_dev_pci_seg_unmap(struct sxe2_adapter *adapter, uint32_t res_type);

int32_t sxe2_dev_pci_map_init(struct rte_eth_dev *dev);

void sxe2_dev_pci_map_uinit(struct rte_eth_dev *dev);

static inline bool
sxe2_dev_port_vlan_check(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *ad = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);

	return ad->filter_ctxt.vlan_info.port_vlan_exist;
}

#endif /* SXE2_ETHDEV_H */
