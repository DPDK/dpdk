/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stdio.h>
#include <time.h>

#include <rte_eal.h>
#include <ethdev_driver.h>
#include <rte_string_fns.h>

#include <mlx5_common_mp.h>
#include <mlx5_common_mr.h>
#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

/**
 * Broadcast request of starting data-path to secondary processes. The request
 * is synchronous.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 */
void
mlx5_mp_os_req_start_rxtx(struct rte_eth_dev *dev)
{
	RTE_SET_USED(dev);
}

/**
 * Broadcast request of stopping data-path to secondary processes. The request
 * is synchronous.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 */
void
mlx5_mp_os_req_stop_rxtx(struct rte_eth_dev *dev)
{
	RTE_SET_USED(dev);
}

/**
 * Request Verbs Rx/Tx queue stop or start to the primary process.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 * @param queue_id
 *   Queue ID to control.
 * @param req_type
 *   request type
 *     MLX5_MP_REQ_QUEUE_RX_START - start Rx queue
 *     MLX5_MP_REQ_QUEUE_TX_START - stop Tx queue
 *     MLX5_MP_REQ_QUEUE_RX_STOP - stop Rx queue
 *     MLX5_MP_REQ_QUEUE_TX_STOP - stop Tx queue
 * @return
 *   0 on success, a negative errno value otherwise and
 *     rte_errno is set.
 */
int
mlx5_mp_os_req_queue_control(struct rte_eth_dev *dev, uint16_t queue_id,
			  enum mlx5_mp_req_type req_type)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);
	RTE_SET_USED(req_type);
	return -ENOTSUP;
}
