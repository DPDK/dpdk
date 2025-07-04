/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_FLOWER_REPRESENTOR_H__
#define __NFP_FLOWER_REPRESENTOR_H__

#include "nfp_flower.h"

struct nfp_flower_representor {
	uint16_t vf_id;
	uint16_t switch_domain_id;
	uint32_t repr_type;
	uint32_t port_id;
	uint32_t nfp_idx;    /**< Only valid for the repr of physical port */
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_ether_addr mac_addr;
	struct nfp_app_fw_flower *app_fw_flower;
	struct rte_ring **ring;
	struct rte_eth_link link;
	struct rte_eth_stats repr_stats;

	struct rte_eth_xstat *repr_xstats_base;
	uint8_t *mac_stats;
	/** Sequential physical port number, only valid for repr of physical port */
	uint8_t idx;
};

int nfp_flower_repr_create(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_net_hw_priv *hw_priv);
bool nfp_flower_repr_is_vf(struct nfp_flower_representor *repr);
bool nfp_flower_repr_is_phy(struct nfp_flower_representor *repr);
int nfp_flower_repr_stats_reset(struct rte_eth_dev *ethdev);
int nfp_flower_repr_link_update(struct rte_eth_dev *dev,
		__rte_unused int wait_to_complete);
bool nfp_flower_repr_is_pf(struct rte_eth_dev *dev);

#endif /* __NFP_FLOWER_REPRESENTOR_H__ */
