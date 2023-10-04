/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_CONNTRACK_H__
#define __NFP_CONNTRACK_H__

#include <stdbool.h>

#include <rte_flow.h>

#include "../nfp_flow.h"

struct nfp_ct_map_entry;

struct nfp_ct_zone_entry;

struct nfp_ct_merge_entry;

struct nfp_ct_map_entry *nfp_ct_map_table_search(struct nfp_flow_priv *priv,
		char *hash_data,
		uint32_t hash_len);

struct rte_flow *nfp_ct_flow_setup(struct nfp_flower_representor *representor,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		const struct rte_flow_item *ct_item,
		bool validate_flag,
		uint64_t cookie);

#endif /* __NFP_CONNTRACK_H__ */
