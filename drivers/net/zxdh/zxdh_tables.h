/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_TABLES_H
#define ZXDH_TABLES_H

#include <stdint.h>

#define ZXDH_DEVICE_NO                    0
#define ZXDH_PORT_MTU_FLAG                9
#define ZXDH_PORT_BASE_QID_FLAG           10
#define ZXDH_PORT_ATTR_IS_UP_FLAG         35
#define ZXDH_PORT_MTU_EN_FLAG             42

#define ZXDH_MTU_STATS_EGRESS_BASE        0x8481
#define ZXDH_MTU_STATS_INGRESS_BASE       0x8981
#define ZXDH_BROAD_STATS_EGRESS_BASE      0xC902
#define ZXDH_BROAD_STATS_INGRESS_BASE     0xD102

extern struct zxdh_dtb_shared_data g_dtb_data;

struct zxdh_port_attr_table {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t byte4_rsv1: 1;
	uint8_t ingress_meter_enable: 1;
	uint8_t egress_meter_enable: 1;
	uint8_t byte4_rsv2: 2;
	uint8_t fd_enable: 1;
	uint8_t vepa_enable: 1;
	uint8_t spoof_check_enable: 1;

	uint8_t inline_sec_offload: 1;
	uint8_t ovs_enable: 1;
	uint8_t lag_enable: 1;
	uint8_t is_passthrough: 1;
	uint8_t is_vf: 1;
	uint8_t virtion_version: 2;
	uint8_t virtio_enable: 1;

	uint8_t accelerator_offload_flag: 1;
	uint8_t lro_offload: 1;
	uint8_t ip_fragment_offload: 1;
	uint8_t tcp_udp_checksum_offload: 1;
	uint8_t ip_checksum_offload: 1;
	uint8_t outer_ip_checksum_offload: 1;
	uint8_t is_up: 1;
	uint8_t rsv1: 1;

	uint8_t rsv3 : 1;
	uint8_t rdma_offload_enable: 1;
	uint8_t vlan_filter_enable: 1;
	uint8_t vlan_strip_offload: 1;
	uint8_t qinq_strip_offload: 1;
	uint8_t rss_enable: 1;
	uint8_t mtu_enable: 1;
	uint8_t hit_flag: 1;

	uint16_t mtu;

	uint16_t port_base_qid : 12;
	uint16_t hash_search_index : 3;
	uint16_t rsv: 1;

	uint8_t rss_hash_factor;

	uint8_t hash_alg: 4;
	uint8_t phy_port: 4;

	uint16_t lag_id : 3;
	uint16_t pf_vfid : 11;
	uint16_t ingress_tm_enable : 1;
	uint16_t egress_tm_enable : 1;

	uint16_t tpid;

	uint16_t vhca : 10;
	uint16_t uplink_port : 6;
#else
	uint8_t rsv3 : 1;
	uint8_t rdma_offload_enable: 1;
	uint8_t vlan_filter_enable: 1;
	uint8_t vlan_strip_offload: 1;
	uint8_t qinq_strip_offload: 1;
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
	uint8_t rsv1: 1;

	uint8_t inline_sec_offload: 1;
	uint8_t ovs_enable: 1;
	uint8_t lag_enable: 1;
	uint8_t is_passthrough: 1;
	uint8_t is_vf: 1;
	uint8_t virtion_version: 2;
	uint8_t virtio_enable: 1;

	uint8_t byte4_rsv1: 1;
	uint8_t ingress_meter_enable: 1;
	uint8_t egress_meter_enable: 1;
	uint8_t byte4_rsv2: 2;
	uint8_t fd_enable: 1;
	uint8_t vepa_enable: 1;
	uint8_t spoof_check_enable: 1;

	uint16_t port_base_qid : 12;
	uint16_t hash_search_index : 3;
	uint16_t rsv: 1;

	uint16_t mtu;

	uint16_t lag_id : 3;
	uint16_t pf_vfid : 11;
	uint16_t ingress_tm_enable : 1;
	uint16_t egress_tm_enable : 1;

	uint8_t hash_alg: 4;
	uint8_t phy_port: 4;

	uint8_t rss_hash_factor;

	uint16_t tpid;

	uint16_t vhca : 10;
	uint16_t uplink_port : 6;
#endif
};

struct zxdh_panel_table {
	uint16_t port_vfid_1588 : 11,
			 rsv2           : 5;
	uint16_t pf_vfid        : 11,
			 rsv1           : 1,
			 enable_1588_tc : 2,
			 trust_mode     : 1,
			 hit_flag       : 1;
	uint32_t mtu            : 16,
			 mtu_enable     : 1,
			 rsv            : 3,
			 tm_base_queue  : 12;
	uint32_t rsv_1;
	uint32_t rsv_2;
}; /* 16B */

struct zxdh_mac_unicast_key {
	uint16_t rsv;
	uint8_t  dmac_addr[6];
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
int zxdh_set_port_attr(uint16_t vfid, struct zxdh_port_attr_table *port_attr);
int zxdh_port_attr_uninit(struct rte_eth_dev *dev);
int zxdh_get_port_attr(uint16_t vfid, struct zxdh_port_attr_table *port_attr);
int zxdh_set_mac_table(uint16_t vport, struct rte_ether_addr *addr,  uint8_t hash_search_idx);
int zxdh_del_mac_table(uint16_t vport, struct rte_ether_addr *addr,  uint8_t hash_search_idx);
int zxdh_promisc_table_init(struct rte_eth_dev *dev);
int zxdh_promisc_table_uninit(struct rte_eth_dev *dev);
int zxdh_dev_unicast_table_set(struct zxdh_hw *hw, uint16_t vport, bool enable);
int zxdh_dev_multicast_table_set(struct zxdh_hw *hw, uint16_t vport, bool enable);
int zxdh_vlan_filter_table_init(struct rte_eth_dev *dev);
int zxdh_vlan_filter_table_set(uint16_t vport, uint16_t vlan_id, uint8_t enable);
int zxdh_rss_table_set(uint16_t vport, struct zxdh_rss_reta *rss_reta);
int zxdh_rss_table_get(uint16_t vport, struct zxdh_rss_reta *rss_reta);
int zxdh_get_panel_attr(struct rte_eth_dev *dev, struct zxdh_panel_table *panel_attr);
int zxdh_set_panel_attr(struct rte_eth_dev *dev, struct zxdh_panel_table *panel_attr);

#endif /* ZXDH_TABLES_H */
