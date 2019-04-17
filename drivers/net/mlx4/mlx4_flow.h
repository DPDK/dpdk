/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX4_FLOW_H_
#define RTE_PMD_MLX4_FLOW_H_

#include <stdint.h>
#include <sys/queue.h>

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ethdev_driver.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_byteorder.h>

/** Last and lowest priority level for a flow rule. */
#define MLX4_FLOW_PRIORITY_LAST UINT32_C(0xfff)

/** Meta pattern item used to distinguish internal rules. */
#define MLX4_FLOW_ITEM_TYPE_INTERNAL ((enum rte_flow_item_type)-1)

/** PMD-specific (mlx4) definition of a flow rule handle. */
struct rte_flow {
	LIST_ENTRY(rte_flow) next; /**< Pointer to the next flow structure. */
	struct ibv_flow *ibv_flow; /**< Verbs flow. */
	struct ibv_flow_attr *ibv_attr; /**< Pointer to Verbs attributes. */
	uint32_t ibv_attr_size; /**< Size of Verbs attributes. */
	uint32_t select:1; /**< Used by operations on the linked list. */
	uint32_t internal:1; /**< Internal flow rule outside isolated mode. */
	uint32_t mac:1; /**< Rule associated with a configured MAC address. */
	uint32_t promisc:1; /**< This rule matches everything. */
	uint32_t allmulti:1; /**< This rule matches all multicast traffic. */
	uint32_t drop:1; /**< This rule drops packets. */
	uint32_t priority; /**< Flow rule priority. */
	struct mlx4_rss *rss; /**< Rx target. */
};

/* mlx4_flow.c */

uint64_t mlx4_conv_rss_types(struct mlx4_priv *priv, uint64_t types,
			     int verbs_to_dpdk);
int mlx4_flow_sync(struct mlx4_priv *priv, struct rte_flow_error *error);
void mlx4_flow_clean(struct mlx4_priv *priv);
int mlx4_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg);

#endif /* RTE_PMD_MLX4_FLOW_H_ */
