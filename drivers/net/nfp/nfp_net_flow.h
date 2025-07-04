/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NET_FLOW_H__
#define __NFP_NET_FLOW_H__

#include "nfp_net_common.h"

struct nfp_net_flow_payload {
	uint16_t cmsg_type;
	uint8_t match_len;
	uint8_t action_len;
	char *match_data;
	char *action_data;
};

struct rte_flow {
	struct nfp_net_flow_payload payload;
	uint32_t hash_key;
	uint32_t port_id;
	uint32_t position;    /**< Use as priority */
};

int nfp_net_flow_priv_init(struct nfp_pf_dev *pf_dev, uint16_t port);
void nfp_net_flow_priv_uninit(struct nfp_pf_dev *pf_dev, uint16_t port);
int nfp_net_flow_ops_get(struct rte_eth_dev *dev, const struct rte_flow_ops **ops);

#endif /* __NFP_NET_FLOW_H__ */
