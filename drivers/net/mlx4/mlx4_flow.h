/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include <rte_eth_ctrl.h>
#include <rte_ethdev.h>
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
	struct mlx4_rss *rss; /**< Rx target. */
};

/* mlx4_flow.c */

int mlx4_flow_sync(struct priv *priv, struct rte_flow_error *error);
void mlx4_flow_clean(struct priv *priv);
int mlx4_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg);

#endif /* RTE_PMD_MLX4_FLOW_H_ */
