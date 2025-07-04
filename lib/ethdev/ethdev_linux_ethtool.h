/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 NVIDIA Corporation & Affiliates
 */

#ifndef ETHDEV_ETHTOOL_H
#define ETHDEV_ETHTOOL_H

#include <stdint.h>
#include <linux/ethtool.h>

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Convert bit from ETHTOOL_LINK_MODE_* to RTE_ETH_LINK_SPEED_*
 */
__rte_internal
uint32_t rte_eth_link_speed_ethtool(enum ethtool_link_mode_bit_indices bit);

/*
 * Convert bitmap from ETHTOOL_GLINKSETTINGS ethtool_link_settings::link_mode_masks
 * to bitmap RTE_ETH_LINK_SPEED_*
 */
__rte_internal
uint32_t rte_eth_link_speed_glink(const uint32_t *bitmap, int8_t nwords);

/*
 * Convert bitmap from deprecated ETHTOOL_GSET ethtool_cmd::supported
 * to bitmap RTE_ETH_LINK_SPEED_*
 */
__rte_internal
uint32_t rte_eth_link_speed_gset(uint32_t legacy_bitmap);

#ifdef __cplusplus
}
#endif

#endif /* ETHDEV_ETHTOOL_H */
