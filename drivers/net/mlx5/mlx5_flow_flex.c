/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */
#include <rte_malloc.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_malloc.h>
#include "mlx5.h"
#include "mlx5_flow.h"

static_assert(sizeof(uint32_t) * CHAR_BIT >= MLX5_PORT_FLEX_ITEM_NUM,
	      "Flex item maximal number exceeds uint32_t bit width");

/**
 *  Routine called once on port initialization to init flex item
 *  related infrastructure initialization
 *
 * @param dev
 *   Ethernet device to perform flex item initialization
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flex_item_port_init(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	rte_spinlock_init(&priv->flex_item_sl);
	MLX5_ASSERT(!priv->flex_item_map);
	return 0;
}

/**
 *  Routine called once on port close to perform flex item
 *  related infrastructure cleanup.
 *
 * @param dev
 *   Ethernet device to perform cleanup
 */
void
mlx5_flex_item_port_cleanup(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < MLX5_PORT_FLEX_ITEM_NUM && priv->flex_item_map ; i++) {
		if (priv->flex_item_map & (1 << i)) {
			/* DevX object dereferencing should be provided here. */
			priv->flex_item_map &= ~(1 << i);
		}
	}
}

static int
mlx5_flex_index(struct mlx5_priv *priv, struct mlx5_flex_item *item)
{
	uintptr_t start = (uintptr_t)&priv->flex_item[0];
	uintptr_t entry = (uintptr_t)item;
	uintptr_t idx = (entry - start) / sizeof(struct mlx5_flex_item);

	if (entry < start ||
	    idx >= MLX5_PORT_FLEX_ITEM_NUM ||
	    (entry - start) % sizeof(struct mlx5_flex_item) ||
	    !(priv->flex_item_map & (1u << idx)))
		return -1;
	return (int)idx;
}

static struct mlx5_flex_item *
mlx5_flex_alloc(struct mlx5_priv *priv)
{
	struct mlx5_flex_item *item = NULL;

	rte_spinlock_lock(&priv->flex_item_sl);
	if (~priv->flex_item_map) {
		uint32_t idx = rte_bsf32(~priv->flex_item_map);

		if (idx < MLX5_PORT_FLEX_ITEM_NUM) {
			item = &priv->flex_item[idx];
			MLX5_ASSERT(!item->refcnt);
			MLX5_ASSERT(!item->devx_fp);
			item->devx_fp = NULL;
			__atomic_store_n(&item->refcnt, 0, __ATOMIC_RELEASE);
			priv->flex_item_map |= 1u << idx;
		}
	}
	rte_spinlock_unlock(&priv->flex_item_sl);
	return item;
}

static void
mlx5_flex_free(struct mlx5_priv *priv, struct mlx5_flex_item *item)
{
	int idx = mlx5_flex_index(priv, item);

	MLX5_ASSERT(idx >= 0 &&
		    idx < MLX5_PORT_FLEX_ITEM_NUM &&
		    (priv->flex_item_map & (1u << idx)));
	if (idx >= 0) {
		rte_spinlock_lock(&priv->flex_item_sl);
		MLX5_ASSERT(!item->refcnt);
		MLX5_ASSERT(!item->devx_fp);
		item->devx_fp = NULL;
		__atomic_store_n(&item->refcnt, 0, __ATOMIC_RELEASE);
		priv->flex_item_map &= ~(1u << idx);
		rte_spinlock_unlock(&priv->flex_item_sl);
	}
}

/**
 * Create the flex item with specified configuration over the Ethernet device.
 *
 * @param dev
 *   Ethernet device to create flex item on.
 * @param[in] conf
 *   Flex item configuration.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   Non-NULL opaque pointer on success, NULL otherwise and rte_errno is set.
 */
struct rte_flow_item_flex_handle *
flow_dv_item_create(struct rte_eth_dev *dev,
		    const struct rte_flow_item_flex_conf *conf,
		    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flex_item *flex;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	flex = mlx5_flex_alloc(priv);
	if (!flex) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "too many flex items created on the port");
		return NULL;
	}
	RTE_SET_USED(conf);
	/* Mark initialized flex item valid. */
	__atomic_add_fetch(&flex->refcnt, 1, __ATOMIC_RELEASE);
	return (struct rte_flow_item_flex_handle *)flex;
}

/**
 * Release the flex item on the specified Ethernet device.
 *
 * @param dev
 *   Ethernet device to destroy flex item on.
 * @param[in] handle
 *   Handle of the item existing on the specified device.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
flow_dv_item_release(struct rte_eth_dev *dev,
		     const struct rte_flow_item_flex_handle *handle,
		     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flex_item *flex =
		(struct mlx5_flex_item *)(uintptr_t)handle;
	uint32_t old_refcnt = 1;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	rte_spinlock_lock(&priv->flex_item_sl);
	if (mlx5_flex_index(priv, flex) < 0) {
		rte_spinlock_unlock(&priv->flex_item_sl);
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "invalid flex item handle value");
	}
	if (!__atomic_compare_exchange_n(&flex->refcnt, &old_refcnt, 0, 0,
					 __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
		rte_spinlock_unlock(&priv->flex_item_sl);
		return rte_flow_error_set(error, EBUSY,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "flex item has flow references");
	}
	/* Flex item is marked as invalid, we can leave locked section. */
	rte_spinlock_unlock(&priv->flex_item_sl);
	mlx5_flex_free(priv, flex);
	return 0;
}
