
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_MAC_H__
#define __SXE2_MAC_H__
#include <ethdev_driver.h>
#include "sxe2_host_regs.h"

#define SXE2_NUM_MACADDR_MAX   64

#define SXE2_DPDK_OFFLOAD_OUTER_INSERT_8021Q SXE2_VSI_L2TAGSTXVALID_ID_OUT_VLAN1
#define SXE2_DPDK_OFFLOAD_OUTER_INSERT_8021AD SXE2_VSI_L2TAGSTXVALID_ID_STAG
#define SXE2_DPDK_OFFLOAD_OUTER_INSERT_QINQ1 SXE2_VSI_L2TAGSTXVALID_ID_OUT_VLAN2
#define SXE2_DPDK_OFFLOAD_OUTER_INSERT_VLAN  SXE2_VSI_L2TAGSTXVALID_ID_VLAN

#define SXE2_DPDK_OFFLOAD_OUTER_INSERT_ENABLE SXE2_VSI_L2TAGSTXVALID_L2TAG1_VALID

#define SXE2_DPDK_OFFLOAD_OUTER_STRIP_8021Q SXE2_VSI_TSR_ID_OUT_VLAN1
#define SXE2_DPDK_OFFLOAD_OUTER_STRIP_8021AD SXE2_VSI_TSR_ID_STAG
#define SXE2_DPDK_OFFLOAD_OUTER_STRIP_QINQ1 SXE2_VSI_TSR_ID_OUT_VLAN2

#define SXE2_DPDK_OFFLOAD_INNER_INSERT_QINQ1  SXE2_VSI_L2TAGSTXVALID_ID_VLAN
#define SXE2_DPDK_OFFLOAD_INNER_INSERT_ENABLE SXE2_VSI_L2TAGSTXVALID_L2TAG2_VALID

#define SXE2_DPDK_OFFLOAD_INNER_STRIP_QINQ1 SXE2_VSI_TSR_ID_VLAN

#define SXE2_DPDK_OFFLOAD_FIELD                (0X0F)
#define SXE2_DPDK_OFFLOAD_TAGID_FIELD          (0X07)

#define SXE2_DPDK_OFFLOAD_OUTER_STRIP_MASK (SXE2_DPDK_OFFLOAD_OUTER_STRIP_8021Q | \
					SXE2_DPDK_OFFLOAD_OUTER_STRIP_8021AD | \
					SXE2_DPDK_OFFLOAD_OUTER_STRIP_QINQ1)
#define SXE2_DPDK_OFFLOAD_STRIP_OFFSET SXE2_VSI_TSR_SHOW_TAG_S

#define SXE2_DPDK_OFFLOAD_INSERT_ENABLE (RTE_BIT32(3))

struct sxe2_mac_mc_list {
	uint32_t count;
	struct rte_ether_addr addr[SXE2_NUM_MACADDR_MAX];
};

int32_t sxe2_link_update_init(struct rte_eth_dev *dev);

int32_t sxe2_mac_default_cfg(struct rte_eth_dev *dev);

int32_t sxe2_vlan_cfg_init(struct rte_eth_dev *dev);

int32_t sxe2_mac_addr_init(struct rte_eth_dev *dev);

void sxe2_mac_addr_uinit(struct rte_eth_dev *dev);

int32_t sxe2_mac_addr_add(struct rte_eth_dev *dev,
			struct rte_ether_addr *mac_addr,
			__rte_unused uint32_t index, __rte_unused uint32_t pool);

void sxe2_mac_addr_del(struct rte_eth_dev *dev,  uint32_t index);

int32_t sxe2_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr);

int32_t sxe2_set_mc_addr_list(struct rte_eth_dev *dev,
	struct rte_ether_addr *mc_addrs,
	uint32_t mc_addrs_num);

int32_t sxe2_promisc_enable(struct rte_eth_dev *dev);

int32_t sxe2_promisc_disable(struct rte_eth_dev *dev);

int32_t sxe2_allmulti_enable(struct rte_eth_dev *dev);

int32_t sxe2_allmulti_disable(struct rte_eth_dev *dev);

int32_t sxe2_dev_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int32_t on);

int32_t sxe2_dev_vlan_offload_set(struct rte_eth_dev *dev, int32_t mask);

int32_t sxe2_vlan_default_cfg(struct rte_eth_dev *dev);

int32_t sxe2_link_update(struct rte_eth_dev *dev, __rte_unused int32_t wait_to_complete);

int32_t sxe2_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);

#endif /* __SXE2_MAC_H__ */
