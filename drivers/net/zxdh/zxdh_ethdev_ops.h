/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_ETHDEV_OPS_H
#define ZXDH_ETHDEV_OPS_H

#include <stdint.h>

#include <rte_ether.h>

#include "zxdh_ethdev.h"

#define ZXDH_ETH_RSS_L2  RTE_ETH_RSS_L2_PAYLOAD
#define ZXDH_ETH_RSS_IP \
			(RTE_ETH_RSS_IPV4 | \
			 RTE_ETH_RSS_FRAG_IPV4 | \
			 RTE_ETH_RSS_IPV6 | \
			 RTE_ETH_RSS_FRAG_IPV6)
#define ZXDH_ETH_RSS_TCP    (RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_NONFRAG_IPV6_TCP)
#define ZXDH_ETH_RSS_UDP    (RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_NONFRAG_IPV6_UDP)
#define ZXDH_ETH_RSS_SCTP   (RTE_ETH_RSS_NONFRAG_IPV4_SCTP | RTE_ETH_RSS_NONFRAG_IPV6_SCTP)

#define ZXDH_HF_F5_ETH       (ZXDH_ETH_RSS_TCP | ZXDH_ETH_RSS_UDP | ZXDH_ETH_RSS_SCTP)
#define ZXDH_HF_F3_ETH        ZXDH_ETH_RSS_IP
#define ZXDH_HF_MAC_VLAN_ETH  ZXDH_ETH_RSS_L2
#define ZXDH_RSS_HF  ((ZXDH_HF_MAC_VLAN_ETH | ZXDH_HF_F3_ETH | ZXDH_HF_F5_ETH))

#define ZXDH_ETHER_MIN_MTU      68

struct zxdh_np_stats_data {
	uint64_t n_pkts_dropped;
	uint64_t n_bytes_dropped;
};

struct zxdh_hw_stats_data {
	uint64_t n_pkts_dropped;
	uint64_t n_bytes_dropped;
};

struct zxdh_hw_np_stats {
	uint64_t rx_unicast_pkts;
	uint64_t tx_unicast_pkts;
	uint64_t rx_unicast_bytes;
	uint64_t tx_unicast_bytes;
	uint64_t rx_multicast_pkts;
	uint64_t tx_multicast_pkts;
	uint64_t rx_multicast_bytes;
	uint64_t tx_multicast_bytes;
	uint64_t rx_broadcast_pkts;
	uint64_t tx_broadcast_pkts;
	uint64_t rx_broadcast_bytes;
	uint64_t tx_broadcast_bytes;
	uint64_t rx_mtu_drop_pkts;
	uint64_t tx_mtu_drop_pkts;
	uint64_t rx_mtu_drop_bytes;
	uint64_t tx_mtu_drop_bytes;
	uint64_t rx_mtr_drop_pkts;
	uint64_t tx_mtr_drop_pkts;
	uint64_t rx_mtr_drop_bytes;
	uint64_t tx_mtr_drop_bytes;
	uint64_t tx_ssvpc_pkts;
};

struct zxdh_ifc_hw_np_stats_bits {
	uint8_t rx_unicast_pkts[0x40];
	uint8_t tx_unicast_pkts[0x40];
	uint8_t rx_unicast_bytes[0x40];
	uint8_t tx_unicast_bytes[0x40];
	uint8_t rx_multicast_pkts[0x40];
	uint8_t tx_multicast_pkts[0x40];
	uint8_t rx_multicast_bytes[0x40];
	uint8_t tx_multicast_bytes[0x40];
	uint8_t rx_broadcast_pkts[0x40];
	uint8_t tx_broadcast_pkts[0x40];
	uint8_t rx_broadcast_bytes[0x40];
	uint8_t tx_broadcast_bytes[0x40];
	uint8_t rx_mtu_drop_pkts[0x40];
	uint8_t tx_mtu_drop_pkts[0x40];
	uint8_t rx_mtu_drop_bytes[0x40];
	uint8_t tx_mtu_drop_bytes[0x40];
	uint8_t rx_mtr_drop_pkts[0x40];
	uint8_t tx_mtr_drop_pkts[0x40];
	uint8_t rx_mtr_drop_bytes[0x40];
	uint8_t tx_mtr_drop_bytes[0x40];
	uint8_t tx_ssvpc_pkts[0x40];
};

struct zxdh_hw_vqm_stats {
	uint64_t rx_total;
	uint64_t tx_total;
	uint64_t rx_bytes;
	uint64_t tx_bytes;
	uint64_t rx_error;
	uint64_t tx_error;
	uint64_t rx_drop;
};

struct zxdh_ifc_hw_vqm_stats_bits {
	uint8_t rx_total[0x40];
	uint8_t tx_total[0x40];
	uint8_t rx_bytes[0x40];
	uint8_t tx_bytes[0x40];
	uint8_t rx_error[0x40];
	uint8_t tx_error[0x40];
	uint8_t rx_drop[0x40];
};


int zxdh_dev_set_link_up(struct rte_eth_dev *dev);
int zxdh_dev_set_link_down(struct rte_eth_dev *dev);
int32_t zxdh_dev_link_update(struct rte_eth_dev *dev, int32_t wait_to_complete);
int zxdh_dev_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		uint32_t index, uint32_t vmdq);
int zxdh_dev_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr);
void zxdh_dev_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index);
int zxdh_dev_promiscuous_enable(struct rte_eth_dev *dev);
int zxdh_dev_promiscuous_disable(struct rte_eth_dev *dev);
int zxdh_dev_allmulticast_enable(struct rte_eth_dev *dev);
int zxdh_dev_allmulticast_disable(struct rte_eth_dev *dev);
int zxdh_dev_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on);
int zxdh_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask);
int zxdh_vlan_tpid_set(struct rte_eth_dev *dev, enum rte_vlan_type vlan_type, uint16_t tpid);
int zxdh_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size);
int zxdh_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
int zxdh_rss_hash_update(struct rte_eth_dev *dev, struct rte_eth_rss_conf *rss_conf);
int zxdh_rss_hash_conf_get(struct rte_eth_dev *dev, struct rte_eth_rss_conf *rss_conf);
int zxdh_rss_configure(struct rte_eth_dev *dev);
int zxdh_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
int zxdh_dev_stats_reset(struct rte_eth_dev *dev);
int zxdh_dev_mtu_set(struct rte_eth_dev *dev, uint16_t new_mtu);
int zxdh_hw_np_stats_pf_reset(struct rte_eth_dev *dev, uint32_t stats_id);
void zxdh_data_hi_to_lo(uint64_t *data);
int32_t zxdh_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats, uint32_t n);
int32_t zxdh_dev_xstats_get_names(struct rte_eth_dev *dev,
			struct rte_eth_xstat_name *xstats_names, unsigned int limit);
int zxdh_dev_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size);
int zxdh_dev_get_module_info(struct rte_eth_dev *dev, struct rte_eth_dev_module_info *modinfo);
int zxdh_dev_get_module_eeprom(struct rte_eth_dev *dev, struct rte_dev_eeprom_info *info);
int zxdh_meter_ops_get(struct rte_eth_dev *dev, void *arg);
uint16_t zxdh_hw_qid_to_logic_qid(struct rte_eth_dev *dev, uint16_t qid);

#endif /* ZXDH_ETHDEV_OPS_H */
