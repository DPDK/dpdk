/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_FILTER_H__
#define __SXE2_FILTER_H__
#include <ethdev_driver.h>

#define SXE2_PROMISC  (1UL << 0UL)
#define SXE2_PROMISC_MULTICAST  (1UL << 1UL)

struct sxe2_vlan_info {
	uint8_t port_vlan_exist;
	uint8_t is_switchdev;
	uint16_t max_cnt;
	uint16_t cnt;

	bool filter_on;
	bool hw_filter_on;

	uint16_t tpid;
	uint16_t vid;

	uint8_t outer_insert;
	uint8_t outer_strip;
	uint8_t inner_insert;
	uint8_t inner_strip;
};

struct sxe2_vlan {
	uint16_t tpid;
	uint16_t vid;
	uint8_t prio;
};

struct sxe2_vlan_filter {
	TAILQ_ENTRY(sxe2_vlan_filter) next;
	bool hw_config;
	bool default_config;
	struct sxe2_vlan vlan_info;
};

TAILQ_HEAD(sxe2_vlan_filter_list_head, sxe2_vlan_filter);

struct sxe2_mac_filter {
	TAILQ_ENTRY(sxe2_mac_filter) next;
	bool hw_config;
	bool default_config;
	struct rte_ether_addr mac_addr;
};

TAILQ_HEAD(sxe2_uc_filter_list_head, sxe2_mac_filter);
TAILQ_HEAD(sxe2_mc_filter_list_head, sxe2_mac_filter);

int32_t sxe2_uc_filter_add(struct sxe2_adapter *adapter,
			struct rte_ether_addr *mac_addr, bool default_config);

int32_t sxe2_uc_filter_del(struct sxe2_adapter *adapter,
			struct rte_ether_addr *mac_addr);

void sxe2_uc_filter_clear(struct sxe2_adapter *adapter, bool default_config);

int32_t sxe2_mc_filter_add(struct sxe2_adapter *adapter,
			struct rte_ether_addr *mac_addr, bool default_config);

int32_t sxe2_mc_filter_del(struct sxe2_adapter *adapter,
			struct rte_ether_addr *mac_addr);

void sxe2_mc_filter_clear(struct sxe2_adapter *adapter, bool default_config);

int32_t sxe2_vlan_filter_add(struct sxe2_adapter *adapter,
	struct sxe2_vlan *vlan, bool default_config);

int32_t sxe2_vlan_filter_del(struct sxe2_adapter *adapter, struct sxe2_vlan *vlan);

void sxe2_vlan_filters_clear(struct sxe2_adapter *adapter, bool default_config);

int32_t sxe2_vlan_filter_ctrl(struct sxe2_adapter *adapter, bool flag);

int32_t sxe2_promisc_add(struct sxe2_adapter *adapter);

int32_t sxe2_promisc_del(struct sxe2_adapter *adapter);

int32_t sxe2_allmulti_add(struct sxe2_adapter *adapter);

int32_t sxe2_allmulti_del(struct sxe2_adapter *adapter);

int32_t sxe2_l2_rule_update(struct sxe2_adapter *adapter);

int32_t sxe2_filter_rule_stop(struct rte_eth_dev *dev);

int32_t sxe2_switchdev_rule_update(struct sxe2_adapter *adapter);

int32_t sxe2_filter_rule_start(struct rte_eth_dev *dev);

int32_t sxe2_filter_init(struct rte_eth_dev *dev);

int32_t sxe2_filter_uinit(struct rte_eth_dev *dev);

#endif /* __SXE2_FILTER_H__ */
