/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_CONNTRACK_H__
#define __NFP_CONNTRACK_H__

#include <stdbool.h>

#include <ethdev_driver.h>
#include <rte_flow.h>

#include "../nfp_flow.h"

struct nfp_ct_map_entry;

struct nfp_ct_zone_entry;

struct nfp_ct_merge_entry;

struct nfp_ct_map_entry *nfp_ct_map_table_search(struct nfp_flow_priv *priv,
		char *hash_data,
		uint32_t hash_len);

int nfp_ct_offload_del(struct rte_eth_dev *dev,
		struct nfp_ct_map_entry *me,
		struct rte_flow_error *error);

struct rte_flow *nfp_ct_flow_setup(struct nfp_flower_representor *representor,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		const struct rte_flow_item *ct_item,
		bool validate_flag,
		uint64_t cookie);

struct nfp_fl_stats *nfp_ct_flow_stats_get(struct nfp_flow_priv *priv,
		struct nfp_ct_map_entry *me);

#endif /* __NFP_CONNTRACK_H__ */
