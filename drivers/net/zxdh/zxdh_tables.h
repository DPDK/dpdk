/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_TABLES_H
#define ZXDH_TABLES_H

#include <stdint.h>

#include <zxdh_msg.h>

/* eram */
#define ZXDH_SDT_VPORT_ATT_TABLE          1
#define ZXDH_SDT_PANEL_ATT_TABLE          2
#define ZXDH_SDT_RSS_ATT_TABLE            3
#define ZXDH_SDT_VLAN_ATT_TABLE           4
#define ZXDH_SDT_BROCAST_ATT_TABLE        6
#define ZXDH_SDT_UNICAST_ATT_TABLE        10
#define ZXDH_SDT_MULTICAST_ATT_TABLE      11
#define ZXDH_SDT_PORT_VLAN_ATT_TABLE      16
#define ZXDH_SDT_TUNNEL_ENCAP0_TABLE      28
#define ZXDH_SDT_TUNNEL_ENCAP1_TABLE      29
/* hash */
#define ZXDH_SDT_L2_ENTRY_TABLE0          64
#define ZXDH_SDT_L2_ENTRY_TABLE1          65
#define ZXDH_SDT_L2_ENTRY_TABLE2          66
#define ZXDH_SDT_L2_ENTRY_TABLE3          67

#define ZXDH_SDT_MC_TABLE0                76
#define ZXDH_SDT_MC_TABLE1                77
#define ZXDH_SDT_MC_TABLE2                78
#define ZXDH_SDT_MC_TABLE3                79

#define ZXDH_SDT_FD_TABLE                 130

#define ZXDH_PORT_VHCA_FLAG                       1
#define ZXDH_PORT_RSS_HASH_FACTOR_FLAG            3
#define ZXDH_PORT_HASH_ALG_FLAG                   4
#define ZXDH_PORT_PHY_PORT_FLAG                   5
#define ZXDH_PORT_LAG_ID_FLAG                     6
#define ZXDH_PORT_VXLAN_OFFLOAD_EN_OFF            7
#define ZXDH_PORT_PF_VQM_VFID_FLAG                8

#define ZXDH_PORT_MTU_FLAG                        10
#define ZXDH_PORT_BASE_QID_FLAG                   11
#define ZXDH_PORT_HASH_SEARCH_INDEX_FLAG          12

#define ZXDH_PORT_EGRESS_METER_EN_OFF_FLAG        14
#define ZXDH_PORT_EGRESS_MODE_FLAG                16
#define ZXDH_PORT_INGRESS_METER_EN_OFF_FLAG       15
#define ZXDH_PORT_INGRESS_MODE_FLAG               17
#define ZXDH_PORT_EGRESS_TM_ENABLE_FLAG           18
#define ZXDH_PORT_INGRESS_TM_ENABLE_FLAG          19
#define ZXDH_PORT_SPOOFCHK_EN_OFF_FLAG            21
#define ZXDH_PORT_INLINE_SEC_OFFLOAD_FLAG         22
#define ZXDH_PORT_FD_EN_OFF_FLAG                  23
#define ZXDH_PORT_LAG_EN_OFF_FLAG                 24
#define ZXDH_PORT_VEPA_EN_OFF_FLAG                25
#define ZXDH_PORT_IS_VF_FLAG                      26
#define ZXDH_PORT_VIRTION_VERSION_FLAG            27
#define ZXDH_PORT_VIRTION_EN_OFF_FLAG             28

#define ZXDH_PORT_ACCELERATOR_OFFLOAD_FLAG_FLAG   29
#define ZXDH_PORT_LRO_OFFLOAD_FLAG                30
#define ZXDH_PORT_IP_FRAGMENT_OFFLOAD_FLAG        31
#define ZXDH_PORT_TCP_UDP_CHKSUM_FLAG             32
#define ZXDH_PORT_IP_CHKSUM_FLAG                  33
#define ZXDH_PORT_OUTER_IP_CHECKSUM_OFFLOAD_FLAG  34
#define ZXDH_PORT_VPORT_IS_UP_FLAG                35
#define ZXDH_PORT_BUSINESS_EN_OFF_FLAG            36

#define ZXDH_PORT_HW_BOND_EN_OFF_FLAG             37
#define ZXDH_PORT_RDMA_OFFLOAD_EN_OFF_FLAG        38
#define ZXDH_PORT_PROMISC_EN_FLAG                 39
#define ZXDH_PORT_VLAN_OFFLOAD_EN_FLAG            40
#define ZXDH_PORT_BUSINESS_VLAN_OFFLOAD_EN_FLAG   41
#define ZXDH_PORT_RSS_EN_OFF_FLAG                 42
#define ZXDH_PORT_MTU_OFFLOAD_EN_OFF_FLAG         43

#define ZXDH_MTU_STATS_EGRESS_BASE           0x8481
#define ZXDH_MTU_STATS_INGRESS_BASE          0x8981
#define ZXDH_BROAD_STATS_EGRESS_BASE         0xA8C1
#define ZXDH_BROAD_STATS_INGRESS_BASE        0xA3C1
#define ZXDH_MTR_STATS_EGRESS_BASE           0x7481
#define ZXDH_MTR_STATS_INGRESS_BASE          0x7C81
#define ZXDH_MULTICAST_STATS_EGRESS_BASE     0x9EC1
#define ZXDH_MULTICAST_STATS_INGRESS_BASE    0x99C1
#define ZXDH_UNICAST_STATS_EGRESS_BASE       0x94C1
#define ZXDH_UNICAST_STATS_INGRESS_BASE      0x8FC1

#define ZXDH_FLOW_STATS_INGRESS_BASE         0xADC1
#define ZXDH_MTR_STATS_EGRESS_BASE           0x7481
#define ZXDH_MTR_STATS_INGRESS_BASE          0x7C81

struct zxdh_port_vlan_table {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint16_t business_vlan_tpid:16;
	/* byte[3,4] */
	uint8_t rsv2:8;
	/* byte[2] */
	uint8_t rsv1: 4;
	uint8_t business_vlan_strip: 1;
	uint8_t business_qinq_strip: 1;
	uint8_t business_vlan_filter: 1;
	uint8_t hit_flag: 1;
	/* byte[1] */
	uint16_t sriov_vlan_tci:16;
	/* byte[7:8] */
	uint16_t sriov_vlan_tpid:16;
	/* byte[5:6] */
#else
	uint8_t rsv1: 4;
	uint8_t business_vlan_strip: 1;
	uint8_t business_qinq_strip: 1;
	uint8_t business_vlan_filter: 1;
	uint8_t hit_flag: 1;
	/* byte[1] */
	uint8_t rsv2:8;
	/* byte[2] */
	uint16_t business_vlan_tpid:16;
	/* byte[3:4] */
	uint16_t sriov_vlan_tpid:16;
	/* byte[5:6] */
	uint16_t sriov_vlan_tci:16;
	/* byte[7:8] */
#endif
};

struct zxdh_port_attr_table {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t egress_meter_enable: 1; /* np view */
	uint8_t ingress_meter_enable: 1; /* np view */
	uint8_t egress_meter_mode: 1; /* np view */
	uint8_t ingress_meter_mode: 1; /* np view */
	uint8_t egress_tm_enable: 1; /* np view */
	uint8_t ingress_tm_enable: 1; /* np view */
	uint8_t rsv1: 1;
	uint8_t spoof_check_enable: 1;

	uint8_t inline_sec_offload: 1;
	uint8_t fd_enable: 1;
	uint8_t lag_enable: 1;
	uint8_t vepa_enable: 1;
	uint8_t is_vf: 1;
	uint8_t virtio_ver: 2;
	uint8_t virtio_enable: 1;

	uint8_t accelerator_offload_flag: 1;
	uint8_t lro_offload: 1;
	uint8_t ip_fragment_offload: 1;
	uint8_t tcp_udp_checksum_offload: 1;
	uint8_t ip_checksum_offload: 1;
	uint8_t outer_ip_checksum_offload: 1;
	uint8_t is_up: 1;
	uint8_t business_enable: 1;

	uint8_t hw_bond_enable : 1;
	uint8_t rdma_offload_enable: 1;
	uint8_t promisc_enable: 1;
	uint8_t sriov_vlan_enable: 1;
	uint8_t business_vlan_enable: 1;
	uint8_t rss_enable: 1;
	uint8_t mtu_enable: 1;
	uint8_t hit_flag: 1;

	uint16_t mtu;

	uint16_t port_base_qid : 12;
	uint16_t hash_search_index : 3;
	uint16_t hpm: 1;

	uint8_t rss_hash_factor;

	uint8_t hash_alg: 4;
	uint8_t phy_port: 4;

	uint16_t lag_id : 3;
	uint16_t fd_vxlan_offload_en : 1;
	uint16_t pf_vfid : 11;
	uint16_t rsv82 : 1;

	uint16_t tpid;

	uint16_t vhca : 10;
	uint16_t rsv16_1 : 6;
#else
	uint8_t hw_bond_enable : 1;
	uint8_t rdma_offload_enable: 1;
	uint8_t promisc_enable: 1;
	uint8_t sriov_vlan_enable: 1;
	uint8_t business_vlan_enable: 1;
	uint8_t rss_enable: 1;
	uint8_t mtu_enable: 1;
	uint8_t hit_flag: 1;

	uint8_t accelerator_offload_flag: 1;
	uint8_t lro_offload: 1;
	uint8_t ip_fragment_offload: 1;
	uint8_t tcp_udp_checksum_offload: 1;
	uint8_t ip_checksum_offload: 1;
	uint8_t outer_ip_checksum_offload: 1;
	uint8_t is_up: 1;
	uint8_t business_enable: 1;

	uint8_t inline_sec_offload: 1;
	uint8_t fd_enable: 1;
	uint8_t lag_enable: 1;
	uint8_t vepa_enable: 1;
	uint8_t is_vf: 1;
	uint8_t virtio_ver: 2;
	uint8_t virtio_enable: 1;

	uint8_t egress_meter_enable: 1; /* np view */
	uint8_t ingress_meter_enable: 1; /* np view */
	uint8_t egress_meter_mode: 1; /* np view */
	uint8_t ingress_meter_mode: 1; /* np view */
	uint8_t egress_tm_enable: 1; /* np view */
	uint8_t ingress_tm_enable: 1; /* np view */
	uint8_t rsv1: 1;
	uint8_t spoof_check_enable: 1;

	uint16_t port_base_qid : 12; /* need rte_bwap16 */
	uint16_t hash_search_index : 3;
	uint16_t rsv: 1;

	uint16_t mtu;

	uint16_t lag_id : 3; /* need rte_bwap16 */
	uint16_t rsv81 : 1;
	uint16_t pf_vfid : 11;
	uint16_t rsv82 : 1;

	uint8_t hash_alg: 4;
	uint8_t phy_port: 4;

	uint8_t rss_hash_factor;

	uint16_t tpid;

	uint16_t vhca : 10;
	uint16_t rsv16_1 : 6;
#endif
};

struct zxdh_panel_table {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint16_t port_vfid_1588   : 11,
				rsv2             : 5;
	uint16_t rsv1             : 11,
			 tm_shape_enable  : 1,
			 enable_1588_tc   : 2,
			 trust_mode       : 1,
			 hit_flag         : 1;
	uint16_t mtu              : 16;
	uint16_t mtu_enable       : 1,
			 rsv              : 3,
			 tm_base_queue    : 12;
	uint16_t lacp_pf_qid      : 12,
				rsv5             : 4;
	uint16_t lacp_pf_vfid     : 11,
				rsv6             : 2,
				member_port_up   : 1,
				bond_link_up     : 1,
				hw_bond_enable   : 1;
	uint16_t rsv3             : 16;
	uint16_t pf_vfid          : 11,
				rsv4             : 5;
#else
	uint16_t rsv1             : 11,
				tm_shape_enable  : 1,
				enable_1588_tc   : 2,
				trust_mode       : 1,
				hit_flag         : 1;
	uint16_t port_vfid_1588   : 11,
				rsv2             : 5;
	uint16_t mtu_enable       : 1,
				rsv              : 3,
				tm_base_queue    : 12;
	uint16_t mtu              : 16;
	uint16_t lacp_pf_vfid     : 11,
				rsv6             : 2,
				member_port_up   : 1,
				bond_link_up     : 1,
				hw_bond_enable   : 1;
	uint16_t lacp_pf_qid      : 12,
				rsv5             : 4;
	uint16_t pf_vfid          : 11,
				rsv4             : 5;
	uint16_t rsv3             : 16;
#endif
}; /* 16B */

struct zxdh_mac_unicast_key {
	uint8_t  dmac_addr[6];
	uint16_t sriov_vlan_tpid;
	uint16_t sriov_vlan_id;
};

struct zxdh_mac_unicast_entry {
	uint8_t rsv1     : 7,
			hit_flag : 1;
	uint8_t rsv;
	uint16_t vfid;
};

struct zxdh_mac_unicast_table {
	struct zxdh_mac_unicast_key key;
	struct zxdh_mac_unicast_entry entry;
};

struct zxdh_mac_multicast_key {
	uint8_t rsv;
	uint8_t vf_group_id;
	uint8_t mac_addr[6];
};

struct zxdh_mac_multicast_entry {
	uint32_t mc_pf_enable;
	uint32_t rsv1;
	uint32_t mc_bitmap[2];
};

struct zxdh_mac_multicast_table {
	struct zxdh_mac_multicast_key key;
	struct zxdh_mac_multicast_entry entry;
};

struct zxdh_brocast_table {
	uint32_t flag;
	uint32_t rsv;
	uint32_t bitmap[2];
};

struct zxdh_unitcast_table {
	uint32_t uc_flood_pf_enable;
	uint32_t rsv;
	uint32_t bitmap[2];
};

struct zxdh_multicast_table {
	uint32_t mc_flood_pf_enable;
	uint32_t rsv;
	uint32_t bitmap[2];
};

struct zxdh_vlan_filter_table {
	uint32_t vlans[4];
};

struct zxdh_rss_to_vqid_table {
	uint16_t vqm_qid[8];
};

int zxdh_port_attr_init(struct rte_eth_dev *dev);
int zxdh_panel_table_init(struct rte_eth_dev *dev);
int zxdh_set_port_attr(struct zxdh_hw *hw, uint16_t vport,
		struct zxdh_port_attr_table *port_attr);
int zxdh_port_attr_uninit(struct rte_eth_dev *dev);
int zxdh_get_port_attr(struct zxdh_hw *hw, uint16_t vport,
		struct zxdh_port_attr_table *port_attr);
int zxdh_delete_port_attr(struct zxdh_hw *hw, uint16_t vport,
		struct zxdh_port_attr_table *port_attr);
int zxdh_add_mac_table(struct zxdh_hw *hw, uint16_t vport, struct rte_ether_addr *addr,
				uint8_t hash_search_idx, uint16_t srv_tpid, uint16_t srv_vlanid);
int zxdh_del_mac_table(struct zxdh_hw *hw, uint16_t vport, struct rte_ether_addr *addr,
			 uint8_t hash_search_idx, uint16_t srv_tpid, uint16_t srv_vlanid);
int zxdh_promisc_table_init(struct rte_eth_dev *dev);
int zxdh_promisc_table_uninit(struct rte_eth_dev *dev);
int zxdh_dev_unicast_table_set(struct zxdh_hw *hw, uint16_t vport, bool enable);
int zxdh_dev_multicast_table_set(struct zxdh_hw *hw, uint16_t vport, bool enable);
int zxdh_vlan_filter_table_init(struct zxdh_hw *hw, uint16_t vport);
int zxdh_vlan_filter_table_set(struct zxdh_hw *hw, uint16_t vport,
		uint16_t vlan_id, uint8_t enable);
int zxdh_rss_table_set(struct zxdh_hw *hw, uint16_t vport, struct zxdh_rss_reta *rss_reta);
int zxdh_rss_table_get(struct zxdh_hw *hw, uint16_t vport, struct zxdh_rss_reta *rss_reta);
int zxdh_get_panel_attr(struct rte_eth_dev *dev, struct zxdh_panel_table *panel_attr);
int zxdh_set_panel_attr(struct rte_eth_dev *dev, struct zxdh_panel_table *panel_attr);
int zxdh_dev_broadcast_set(struct zxdh_hw *hw, uint16_t vport, bool enable);
int zxdh_set_vlan_filter(struct zxdh_hw *hw, uint16_t vport, uint8_t enable);
int zxdh_set_vlan_offload(struct zxdh_hw *hw, uint16_t vport, uint8_t type, uint8_t enable);
int zxdh_set_port_vlan_attr(struct zxdh_hw *hw, uint16_t vport,
		struct zxdh_port_vlan_table *port_vlan);
int zxdh_get_port_vlan_attr(struct zxdh_hw *hw, uint16_t vport,
		struct zxdh_port_vlan_table *port_vlan);
int zxdh_port_vlan_table_init(struct zxdh_hw *hw, uint16_t vport);

#endif /* ZXDH_TABLES_H */
