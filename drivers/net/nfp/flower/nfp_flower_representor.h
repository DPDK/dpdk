/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef _NFP_FLOWER_REPRESENTOR_H_
#define _NFP_FLOWER_REPRESENTOR_H_

#include "nfp_flower.h"

/*
 * enum nfp_repr_type - type of representor
 * @NFP_REPR_TYPE_PHYS_PORT:   external NIC port
 * @NFP_REPR_TYPE_PF:          physical function
 * @NFP_REPR_TYPE_VF:          virtual function
 * @NFP_REPR_TYPE_MAX:         number of representor types
 */
enum nfp_repr_type {
	NFP_REPR_TYPE_PHYS_PORT = 0,
	NFP_REPR_TYPE_PF,
	NFP_REPR_TYPE_VF,
	NFP_REPR_TYPE_MAX,
};

struct nfp_flower_representor {
	uint16_t vf_id;
	uint16_t switch_domain_id;
	uint32_t repr_type;
	uint32_t port_id;
	uint32_t nfp_idx;    /* only valid for the repr of physical port */
	char name[RTE_ETH_NAME_MAX_LEN];
	struct rte_ether_addr mac_addr;
	struct nfp_app_fw_flower *app_fw_flower;
	struct rte_ring *ring;
	struct rte_eth_link link;
	struct rte_eth_stats repr_stats;
};

int nfp_flower_repr_create(struct nfp_app_fw_flower *app_fw_flower);

#endif /* _NFP_FLOWER_REPRESENTOR_H_ */
