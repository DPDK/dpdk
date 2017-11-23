/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
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

#include <stddef.h>
#include <errno.h>
#include <string.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ethdev.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

/**
 * DPDK callback to enable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_promiscuous_enable(struct rte_eth_dev *dev)
{
	dev->data->promiscuous = 1;
	mlx5_traffic_restart(dev);
}

/**
 * DPDK callback to disable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_promiscuous_disable(struct rte_eth_dev *dev)
{
	dev->data->promiscuous = 0;
	mlx5_traffic_restart(dev);
}

/**
 * DPDK callback to enable allmulti mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_allmulticast_enable(struct rte_eth_dev *dev)
{
	dev->data->all_multicast = 1;
	mlx5_traffic_restart(dev);
}

/**
 * DPDK callback to disable allmulti mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_allmulticast_disable(struct rte_eth_dev *dev)
{
	dev->data->all_multicast = 0;
	mlx5_traffic_restart(dev);
}
