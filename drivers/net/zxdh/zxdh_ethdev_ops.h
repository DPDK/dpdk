/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_ETHDEV_OPS_H
#define ZXDH_ETHDEV_OPS_H

#include "zxdh_ethdev.h"

int zxdh_dev_set_link_up(struct rte_eth_dev *dev);
int zxdh_dev_set_link_down(struct rte_eth_dev *dev);
int32_t zxdh_dev_link_update(struct rte_eth_dev *dev, int32_t wait_to_complete __rte_unused);
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

#endif /* ZXDH_ETHDEV_OPS_H */
