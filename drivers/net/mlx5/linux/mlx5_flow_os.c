/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include "mlx5_flow_os.h"

#include <rte_thread.h>

/* Key of thread specific flow workspace data. */
static rte_thread_key key_workspace;
/* Flow workspace global list head for garbage collector. */
static struct mlx5_flow_workspace *gc_head;
/* Spinlock for operating flow workspace list. */
static rte_spinlock_t mlx5_flow_workspace_lock = RTE_SPINLOCK_INITIALIZER;

int
mlx5_flow_os_validate_item_esp(const struct rte_eth_dev *dev,
			       const struct rte_flow_item *item,
			       uint64_t item_flags,
			       uint8_t target_protocol,
			       bool allow_seq,
			       struct rte_flow_error *error)
{
	const struct rte_flow_item_esp *mask = item->mask;
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t l3m = tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
				      MLX5_FLOW_LAYER_OUTER_L3;
	const uint64_t l4m = tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				      MLX5_FLOW_LAYER_OUTER_L4;
	static const struct rte_flow_item_esp mlx5_flow_item_esp_mask = {
		.hdr = {
			.spi = RTE_BE32(0xffffffff),
			.seq = RTE_BE32(0xffffffff),
		},
	};
	int ret;

	if (!mlx5_hws_active(dev)) {
		if (!(item_flags & l3m))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "L3 is mandatory to filter on L4");
	}
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (target_protocol != 0xff && target_protocol != IPPROTO_ESP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "protocol filtering not compatible"
					  " with ESP layer");
	if (!mask)
		mask = &rte_flow_item_esp_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 allow_seq ? (const uint8_t *)&mlx5_flow_item_esp_mask :
			     (const uint8_t *)&rte_flow_item_esp_mask,
		 sizeof(struct rte_flow_item_esp), MLX5_ITEM_RANGE_NOT_ACCEPTED,
		 error);
	if (ret < 0)
		return ret;
	return 0;
}

void
mlx5_flow_os_workspace_gc_add(struct mlx5_flow_workspace *ws)
{
	rte_spinlock_lock(&mlx5_flow_workspace_lock);
	ws->gc = gc_head;
	gc_head = ws;
	rte_spinlock_unlock(&mlx5_flow_workspace_lock);
}

static void
mlx5_flow_os_workspace_gc_release(void)
{
	while (gc_head) {
		struct mlx5_flow_workspace *wks = gc_head;

		gc_head = wks->gc;
		flow_release_workspace(wks);
	}
}

int
mlx5_flow_os_init_workspace_once(void)
{
	if (rte_thread_key_create(&key_workspace, NULL)) {
		DRV_LOG(ERR, "Can't create flow workspace data thread key.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	return 0;
}

void *
mlx5_flow_os_get_specific_workspace(void)
{
	return rte_thread_value_get(key_workspace);
}

int
mlx5_flow_os_set_specific_workspace(struct mlx5_flow_workspace *data)
{
	return rte_thread_value_set(key_workspace, data);
}

void
mlx5_flow_os_release_workspace(void)
{
	rte_thread_key_delete(key_workspace);
	mlx5_flow_os_workspace_gc_release();
}
