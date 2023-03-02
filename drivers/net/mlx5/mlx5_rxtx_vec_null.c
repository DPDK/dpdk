/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#include <rte_common.h>

#include "mlx5_rx.h"

struct rte_mbuf;
struct rte_eth_dev;

uint16_t
mlx5_rx_burst_vec(void *dpdk_rxq __rte_unused,
		struct rte_mbuf **pkts __rte_unused,
		uint16_t pkts_n __rte_unused)
{
	return 0;
}

uint16_t
mlx5_rx_burst_mprq_vec(void *dpdk_rxq __rte_unused,
		struct rte_mbuf **pkts __rte_unused,
		uint16_t pkts_n __rte_unused)
{
	return 0;
}

int
mlx5_rxq_check_vec_support(struct mlx5_rxq_data *rxq __rte_unused)
{
	return -ENOTSUP;
}

int
mlx5_check_vec_rx_support(struct rte_eth_dev *dev __rte_unused)
{
	return -ENOTSUP;
}
