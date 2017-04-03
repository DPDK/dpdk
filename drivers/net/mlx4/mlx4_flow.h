/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
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

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_byteorder.h>

#include "mlx4.h"

struct rte_flow {
	LIST_ENTRY(rte_flow) next; /**< Pointer to the next flow structure. */
	struct ibv_flow *ibv_flow; /**< Verbs flow. */
	struct ibv_flow_attr *ibv_attr; /**< Pointer to Verbs attributes. */
	struct ibv_qp *qp; /**< Verbs queue pair. */
};

int
mlx4_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error);

struct rte_flow *
mlx4_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error);

int
mlx4_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error);

int
mlx4_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error);

/** Structure to pass to the conversion function. */
struct mlx4_flow {
	struct ibv_flow_attr *ibv_attr; /**< Verbs attribute. */
	unsigned int offset; /**< Offset in bytes in the ibv_attr buffer. */
};

struct mlx4_flow_action {
	uint32_t drop:1; /**< Target is a drop queue. */
	uint32_t queue:1; /**< Target is a receive queue. */
	uint32_t queue_id; /**< Identifier of the queue. */
};

int mlx4_priv_flow_start(struct priv *priv);
void mlx4_priv_flow_stop(struct priv *priv);

#endif /* RTE_PMD_MLX4_FLOW_H_ */
