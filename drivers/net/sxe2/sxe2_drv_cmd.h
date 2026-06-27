/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_DRV_CMD_H
#define SXE2_DRV_CMD_H

#include "sxe2_osal.h"

#define SXE2_DRV_CMD_MODULE_S        (16)
#define SXE2_MK_DRV_CMD(module, cmd) (((module) << SXE2_DRV_CMD_MODULE_S) | ((cmd) & 0xFFFF))

#define SXE2_DEV_CAPS_OFFLOAD_L2    RTE_BIT32(0)
#define SXE2_DEV_CAPS_OFFLOAD_VLAN  RTE_BIT32(1)
#define SXE2_DEV_CAPS_OFFLOAD_RSS   RTE_BIT32(2)
#define SXE2_DEV_CAPS_OFFLOAD_IPSEC RTE_BIT32(3)
#define SXE2_DEV_CAPS_OFFLOAD_FNAV  RTE_BIT32(4)
#define SXE2_DEV_CAPS_OFFLOAD_TM    RTE_BIT32(5)
#define SXE2_DEV_CAPS_OFFLOAD_PTP   RTE_BIT32(6)
#define SXE2_DEV_CAPS_OFFLOAD_Q_MAP RTE_BIT32(7)
#define SXE2_DEV_CAPS_OFFLOAD_FC_STATE RTE_BIT32(8)

#define SXE2_TXQ_STATS_MAP_MAX_NUM 16
#define SXE2_RXQ_STATS_MAP_MAX_NUM 4
#define SXE2_RXQ_MAP_Q_MAX_NUM 256

#define SXE2_STAT_MAP_INVALID_QID 0xFFFF

#define SXE2_SCHED_MODE_DEFAULT			0
#define SXE2_SCHED_MODE_TM			1
#define SXE2_SCHED_MODE_HIGH_PERFORMANCE	2
#define SXE2_SCHED_MODE_INVALID			3

#define SXE2_SRCVSI_PRUNE_MAX_NUM		2

#define SXE2_PTYPE_UNKNOWN                   RTE_BIT32(0)
#define SXE2_PTYPE_L2_ETHER                  RTE_BIT32(1)
#define SXE2_PTYPE_L3_IPV4                   RTE_BIT32(2)
#define SXE2_PTYPE_L3_IPV6                   RTE_BIT32(4)
#define SXE2_PTYPE_L4_TCP                    RTE_BIT32(6)
#define SXE2_PTYPE_L4_UDP                    RTE_BIT32(7)
#define SXE2_PTYPE_L4_SCTP                   RTE_BIT32(8)
#define SXE2_PTYPE_INNER_L2_ETHER            RTE_BIT32(9)
#define SXE2_PTYPE_INNER_L3_IPV4             RTE_BIT32(10)
#define SXE2_PTYPE_INNER_L3_IPV6             RTE_BIT32(12)
#define SXE2_PTYPE_INNER_L4_TCP              RTE_BIT32(14)
#define SXE2_PTYPE_INNER_L4_UDP              RTE_BIT32(15)
#define SXE2_PTYPE_INNER_L4_SCTP             RTE_BIT32(16)
#define SXE2_PTYPE_TUNNEL_GRENAT             RTE_BIT32(17)

#define SXE2_PTYPE_L2_MASK       (SXE2_PTYPE_L2_ETHER)
#define SXE2_PTYPE_L3_MASK       (SXE2_PTYPE_L3_IPV4 | SXE2_PTYPE_L3_IPV6)
#define SXE2_PTYPE_L4_MASK       (SXE2_PTYPE_L4_TCP | SXE2_PTYPE_L4_UDP | \
		SXE2_PTYPE_L4_SCTP)
#define SXE2_PTYPE_INNER_L2_MASK (SXE2_PTYPE_INNER_L2_ETHER)
#define SXE2_PTYPE_INNER_L3_MASK (SXE2_PTYPE_INNER_L3_IPV4 | \
		SXE2_PTYPE_INNER_L3_IPV6)
#define SXE2_PTYPE_INNER_L4_MASK (SXE2_PTYPE_INNER_L4_TCP | \
		SXE2_PTYPE_INNER_L4_UDP | \
		SXE2_PTYPE_INNER_L4_SCTP)
#define SXE2_PTYPE_TUNNEL_MASK   (SXE2_PTYPE_TUNNEL_GRENAT)

enum sxe2_dev_type {
	SXE2_DEV_T_PF = 0,
	SXE2_DEV_T_VF,
	SXE2_DEV_T_PF_BOND,
	SXE2_DEV_T_MAX,
};

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_queue_caps {
	uint16_t queues_cnt;
	uint16_t base_idx_in_pf;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_msix_caps {
	uint16_t msix_vectors_cnt;
	uint16_t base_idx_in_func;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_rss_hash_caps {
	uint16_t hash_key_size;
	uint16_t lut_key_size;
} __rte_packed_end;

enum sxe2_vf_vsi_valid {
	SXE2_VF_VSI_BOTH = 0,
	SXE2_VF_VSI_ONLY_DPDK,
	SXE2_VF_VSI_ONLY_KERNEL,
	SXE2_VF_VSI_MAX,
};

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_vsi_caps {
	uint16_t func_id;
	uint16_t dpdk_vsi_id;
	uint16_t kernel_vsi_id;
	uint16_t vsi_type;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_representor_caps {
	uint16_t cnt_repr_vf;
	uint8_t rsv[2];
	struct sxe2_drv_vsi_caps repr_vf_id[256];
} __rte_packed_end;

enum sxe2_phys_port_name_type {
	SXE2_PHYS_PORT_NAME_TYPE_NOTSET = 0,
	SXE2_PHYS_PORT_NAME_TYPE_LEGACY,
	SXE2_PHYS_PORT_NAME_TYPE_UPLINK,
	SXE2_PHYS_PORT_NAME_TYPE_PFVF,

	SXE2_PHYS_PORT_NAME_TYPE_UNKNOWN,
};

struct __rte_aligned(4) __rte_packed_begin sxe2_switchdev_mode_info {
	uint8_t pf_id;
	uint8_t is_switchdev;
	uint8_t rsv[2];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_switchdev_cpvsi_info {
	uint16_t cp_vsi_id;
	uint8_t rsv[2];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_txsch_caps {
	uint8_t layer_cap;
	uint8_t tm_mid_node_num;
	uint8_t prio_num;
	uint8_t rev;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_dev_caps_resp {
	struct sxe2_drv_queue_caps queue_caps;
	struct sxe2_drv_msix_caps msix_caps;
	struct sxe2_drv_rss_hash_caps rss_hash_caps;
	struct sxe2_drv_vsi_caps vsi_caps;
	struct sxe2_txsch_caps   txsch_caps;
	struct sxe2_drv_representor_caps repr_caps;
	uint8_t port_idx;
	uint8_t pf_idx;
	uint8_t dev_type;
	uint8_t rev;
	uint32_t cap_flags;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_dev_info_resp {
	uint64_t dsn;
	uint16_t vsi_id;
	uint8_t rsv[2];
	uint8_t mac_addr[SXE2_ETH_ALEN];
	uint8_t rsv2[2];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_dev_fw_info_resp {
	uint8_t main_version_id;
	uint8_t sub_version_id;
	uint8_t fix_version_id;
	uint8_t build_id;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_rxq_ctxt {
	uint64_t dma_addr;
	uint32_t max_lro_size;
	uint32_t split_type_mask;
	uint16_t hdr_len;
	uint16_t buf_len;
	uint16_t depth;
	uint16_t queue_id;
	uint8_t lro_en;
	uint8_t keep_crc_en;
	uint8_t split_en;
	uint8_t desc_size;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_rxq_cfg_req {
	uint16_t q_cnt;
	uint16_t vsi_id;
	uint16_t max_frame_size;
	uint8_t rsv[2];
	struct sxe2_drv_rxq_ctxt cfg[];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_txq_ctxt {
	uint64_t dma_addr;
	uint32_t sched_mode;
	uint16_t queue_id;
	uint16_t depth;
	uint16_t vsi_id;
	uint8_t rsv[2];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_txq_cfg_req {
	uint16_t q_cnt;
	uint16_t vsi_id;
	struct sxe2_drv_txq_ctxt cfg[];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_q_switch_req {
	uint16_t q_idx;
	uint16_t vsi_id;
	uint8_t is_enable;
	uint8_t sched_mode;
	uint8_t rsv[2];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_vsi_create_req_resp {
	uint16_t vsi_id;
	uint16_t vsi_type;
	struct sxe2_drv_queue_caps used_queues;
	struct sxe2_drv_msix_caps used_msix;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_vsi_free_req {
	uint16_t vsi_id;
	uint8_t rsv[2];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_vsi_info_get_req {
	uint16_t vsi_id;
	uint8_t rsv[2];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_vsi_info_get_resp {
	uint16_t vsi_id;
	uint16_t vsi_type;
	struct sxe2_drv_queue_caps used_queues;
	struct sxe2_drv_msix_caps used_msix;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_link_info_resp {
	uint32_t speed;
	uint8_t status;
	uint8_t rsv[3];
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_switchdev_info {
	uint8_t is_switchdev;
	uint8_t primary;
	uint8_t representor;
	uint8_t port_name_type;
	uint32_t ctrl_num;
	uint32_t pf_num;
	uint32_t vf_num;
	uint32_t mpesw_owner;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_vlan_cfg_query_resp {
	uint16_t vsi_id;
	uint8_t port_vlan_exist;
	uint8_t is_switchdev;
	uint16_t tpid;
	uint16_t vid;
	uint8_t outer_insert;
	uint8_t outer_strip;
	uint8_t inner_insert;
	uint8_t inner_strip;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_vlan_offload_cfg_req {
	uint16_t vsi_id;
	uint16_t tpid;
	uint8_t outer_insert;
	uint8_t outer_strip;
	uint8_t inner_insert;
	uint8_t inner_strip;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_drv_port_vlan_cfg_req {
	uint16_t vsi_id;
	uint16_t tpid;
	uint16_t vid;
	uint8_t prio;
	uint8_t rsv;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_mac_filter_cfg_req {
	uint16_t vsi_id;
	uint8_t addr[SXE2_ETH_ALEN];
	uint8_t type;
	uint8_t is_add;
	uint8_t rsv[2];
} __rte_packed_end;

enum sxe2_promisc_filter_type {
	SXE2_PROMISC_FILTER_TYPE_PROMISC = 0,
	SXE2_PROMISC_FILTER_TYPE_ALLMULTI,
	SXE2_PROMISC_FILTER_TYPE_MAX,
};

enum sxe2_mac_filter_type {
	SXE2_MAC_FILTER_TYPE_UC = 0,
	SXE2_MAC_FILTER_TYPE_MC,
	SXE2_MAC_FILTER_TYPE_MAX,
};

struct __rte_aligned(4) __rte_packed_begin sxe2_promisc_filter_cfg_req {
	uint16_t vsi_id;
	uint8_t type;
	uint8_t is_add;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_srcvsi_ext_cfg_req {
	uint16_t vsi_id;
	uint16_t srcvsi_list[SXE2_SRCVSI_PRUNE_MAX_NUM];
	uint8_t srcvsi_cnt;
	uint8_t is_add;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_vlan_filter_cfg_req {
	uint16_t vsi_id;
	uint16_t vlan_id;
	uint16_t tpid_id;
	uint8_t prio;
	uint8_t is_add;
} __rte_packed_end;

struct __rte_aligned(4) __rte_packed_begin sxe2_vlan_filter_switch_req {
	uint16_t vsi_id;
	uint8_t is_oper_enable;
	uint8_t rsv;
} __rte_packed_end;

enum sxe2_drv_cmd_module {
	SXE2_DRV_CMD_MODULE_HANDSHAKE = 0,
	SXE2_DRV_CMD_MODULE_DEV = 1,
	SXE2_DRV_CMD_MODULE_VSI = 2,
	SXE2_DRV_CMD_MODULE_QUEUE = 3,
	SXE2_DRV_CMD_MODULE_STATS = 4,
	SXE2_DRV_CMD_MODULE_SUBSCRIBE = 5,
	SXE2_DRV_CMD_MODULE_RSS = 6,
	SXE2_DRV_CMD_MODULE_FLOW = 7,
	SXE2_DRV_CMD_MODULE_TM = 8,
	SXE2_DRV_CMD_MODULE_IPSEC = 9,
	SXE2_DRV_CMD_MODULE_PTP = 10,

	SXE2_DRV_CMD_MODULE_VLAN = 11,
	SXE2_DRV_CMD_MODULE_RDMA = 12,
	SXE2_DRV_CMD_MODULE_LINK = 13,
	SXE2_DRV_CMD_MODULE_MACADDR = 14,
	SXE2_DRV_CMD_MODULE_PROMISC = 15,

	SXE2_DRV_CMD_MODULE_LED = 16,
	SXE2_DEV_CMD_MODULE_OPT = 17,
	SXE2_DEV_CMD_MODULE_SWITCH = 18,
	SXE2_DRV_CMD_MODULE_ACL = 19,
	SXE2_DRV_CMD_MODULE_UDPTUNEEL = 20,
	SXE2_DRV_CMD_MODULE_QUEUE_MAP = 21,

	SXE2_DRV_CMD_MODULE_SCHED = 22,

	SXE2_DRV_CMD_MODULE_IRQ = 23,

	SXE2_DRV_CMD_MODULE_OPT = 24,
};

enum sxe2_drv_cmd_code {
	SXE2_DRV_CMD_HANDSHAKE_ENABLE =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_HANDSHAKE, 1),
	SXE2_DRV_CMD_HANDSHAKE_DISABLE,

	SXE2_DRV_CMD_DEV_GET_CAPS =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_DEV, 1),
	SXE2_DRV_CMD_DEV_GET_INFO,
	SXE2_DRV_CMD_DEV_GET_FW_INFO,
	SXE2_DRV_CMD_DEV_RESET,
	SXE2_DRV_CMD_DEV_GET_SWITCHDEV_INFO,

	SXE2_DRV_CMD_VSI_CREATE =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_VSI, 1),
	SXE2_DRV_CMD_VSI_FREE,
	SXE2_DRV_CMD_VSI_INFO_GET,
	SXE2_DRV_CMD_VSI_SRCVSI_PRUNE,
	SXE2_DRV_CMD_VSI_FC_GET,

	SXE2_DRV_CMD_RX_MAP_SET =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_QUEUE_MAP, 1),
	SXE2_DRV_CMD_TX_MAP_SET,
	SXE2_DRV_CMD_TX_RX_MAP_GET,
	SXE2_DRV_CMD_TX_RX_MAP_RESET,
	SXE2_DRV_CMD_TX_RX_MAP_INFO_CLEAR,

	SXE2_DRV_CMD_SCHED_ROOT_TREE_ALLOC =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_SCHED, 1),
	SXE2_DRV_CMD_SCHED_ROOT_TREE_RELEASE,
	SXE2_DRV_CMD_SCHED_ROOT_CHILDREN_DELETE,
	SXE2_DRV_CMD_SCHED_TM_ADD_MID_NODE,
	SXE2_DRV_CMD_SCHED_TM_ADD_QUEUE_NODE,

	SXE2_DRV_CMD_RXQ_CFG_ENABLE =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_QUEUE, 1),
	SXE2_DRV_CMD_TXQ_CFG_ENABLE,
	SXE2_DRV_CMD_RXQ_DISABLE,
	SXE2_DRV_CMD_TXQ_DISABLE,

	SXE2_DRV_CMD_VSI_STATS_GET =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_STATS, 1),
	SXE2_DRV_CMD_VSI_STATS_CLEAR,
	SXE2_DRV_CMD_MAC_STATS_GET,
	SXE2_DRV_CMD_MAC_STATS_CLEAR,

	SXE2_DRV_CMD_RSS_KEY_SET =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_RSS, 1),
	SXE2_DRV_CMD_RSS_LUT_SET,
	SXE2_DRV_CMD_RSS_FUNC_SET,
	SXE2_DRV_CMD_RSS_HF_ADD,
	SXE2_DRV_CMD_RSS_HF_DEL,
	SXE2_DRV_CMD_RSS_HF_CLEAR,

	SXE2_DRV_CMD_FLOW_FILTER_ADD =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_FLOW, 1),
	SXE2_DRV_CMD_FLOW_FILTER_DEL,
	SXE2_DRV_CMD_FLOW_FILTER_CLEAR,
	SXE2_DRV_CMD_FLOW_FNAV_STAT_ALLOC,
	SXE2_DRV_CMD_FLOW_FNAV_STAT_FREE,
	SXE2_DRV_CMD_FLOW_FNAV_STAT_QUERY,

	SXE2_DRV_CMD_DEL_TM_ROOT =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_TM, 1),
	SXE2_DRV_CMD_ADD_TM_ROOT,
	SXE2_DRV_CMD_ADD_TM_NODE,
	SXE2_DRV_CMD_ADD_TM_QUEUE,

	SXE2_DRV_CMD_GET_PTP_CLOCK =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_PTP, 1),

	SXE2_DRV_CMD_VLAN_FILTER_ADD_DEL =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_VLAN, 1),
	SXE2_DRV_CMD_VLAN_FILTER_SWITCH,
	SXE2_DRV_CMD_VLAN_OFFLOAD_CFG,
	SXE2_DRV_CMD_VLAN_PORTVLAN_CFG,
	SXE2_DRV_CMD_VLAN_CFG_QUERY,

	SXE2_DRV_CMD_RDMA_DUMP_PCAP =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_RDMA, 1),

	SXE2_DRV_CMD_LINK_STATUS_GET =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_LINK, 1),

	SXE2_DRV_CMD_MAC_ADDR_UC =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_MACADDR, 1),
	SXE2_DRV_CMD_MAC_ADDR_MC,

	SXE2_DRV_CMD_PROMISC_CFG =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_PROMISC, 1),
	SXE2_DRV_CMD_ALLMULTI_CFG,

	SXE2_DRV_CMD_LED_CTRL =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_LED, 1),

	SXE2_DRV_CMD_OPT_EEP =
		SXE2_MK_DRV_CMD(SXE2_DEV_CMD_MODULE_OPT, 1),

	SXE2_DRV_CMD_SWITCH =
		SXE2_MK_DRV_CMD(SXE2_DEV_CMD_MODULE_SWITCH, 1),
	SXE2_DRV_CMD_SWITCH_UPLINK,
	SXE2_DRV_CMD_SWITCH_REPR,
	SXE2_DRV_CMD_SWITCH_MODE,
	SXE2_DRV_CMD_SWITCH_CPVSI,

	SXE2_DRV_CMD_UDPTUNNEL_ADD =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_UDPTUNEEL, 1),
	SXE2_DRV_CMD_UDPTUNNEL_DEL,
	SXE2_DRV_CMD_UDPTUNNEL_GET,

	SXE2_DRV_CMD_IPSEC_CAP_GET =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_IPSEC, 1),
	SXE2_DRV_CMD_IPSEC_TXSA_ADD,
	SXE2_DRV_CMD_IPSEC_RXSA_ADD,
	SXE2_DRV_CMD_IPSEC_TXSA_DEL,
	SXE2_DRV_CMD_IPSEC_RXSA_DEL,
	SXE2_DRV_CMD_IPSEC_RESOURCE_CLEAR,

	SXE2_DRV_CMD_EVT_IRQ_BAND_RXQ =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_IRQ, 1),

	SXE2_DRV_CMD_OPT_EEP_GET =
		SXE2_MK_DRV_CMD(SXE2_DRV_CMD_MODULE_OPT, 1),

};

#endif /* SXE2_DRV_CMD_H */
