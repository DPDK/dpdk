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
	struct rte_ring *ring;
	struct rte_eth_link link;
	struct rte_eth_stats repr_stats;
};

int nfp_flower_repr_create(struct nfp_app_fw_flower *app_fw_flower);

#endif /* __NFP_FLOWER_REPRESENTOR_H__ */
