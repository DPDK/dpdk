/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <stdio.h>

#include <rte_errno.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_interrupts.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common.h>
#include <mlx5_win_ext.h>
#include <mlx5_malloc.h>
#include <mlx5.h>

/**
 * Get MAC address by querying netdevice.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] mac
 *   MAC address output buffer.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_get_mac(struct rte_eth_dev *dev, uint8_t (*mac)[RTE_ETHER_ADDR_LEN])
{
	struct mlx5_priv *priv;
	mlx5_context_st *context_obj;

	if (!dev) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	priv = dev->data->dev_private;
	context_obj = (mlx5_context_st *)priv->sh->ctx;
	memcpy(mac, context_obj->mlx5_dev.eth_mac, RTE_ETHER_ADDR_LEN);
	return 0;
}
