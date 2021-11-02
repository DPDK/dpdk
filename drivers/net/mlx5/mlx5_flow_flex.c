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
			struct mlx5_flex_item *flex = &priv->flex_item[i];

			claim_zero(mlx5_list_unregister
					(priv->sh->flex_parsers_dv,
					 &flex->devx_fp->entry));
			flex->devx_fp = NULL;
			flex->refcnt = 0;
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
	struct mlx5_flex_parser_devx devx_config = { .devx_obj = NULL };
	struct mlx5_flex_item *flex;
	struct mlx5_list_entry *ent;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	flex = mlx5_flex_alloc(priv);
	if (!flex) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "too many flex items created on the port");
		return NULL;
	}
	ent = mlx5_list_register(priv->sh->flex_parsers_dv, &devx_config);
	if (!ent) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "flex item creation failure");
		goto error;
	}
	flex->devx_fp = container_of(ent, struct mlx5_flex_parser_devx, entry);
	RTE_SET_USED(conf);
	/* Mark initialized flex item valid. */
	__atomic_add_fetch(&flex->refcnt, 1, __ATOMIC_RELEASE);
	return (struct rte_flow_item_flex_handle *)flex;

error:
	mlx5_flex_free(priv, flex);
	return NULL;
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
	int rc;

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
	MLX5_ASSERT(flex->devx_fp);
	rc = mlx5_list_unregister(priv->sh->flex_parsers_dv,
				  &flex->devx_fp->entry);
	flex->devx_fp = NULL;
	mlx5_flex_free(priv, flex);
	if (rc < 0)
		return rte_flow_error_set(error, EBUSY,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "flex item release failure");
	return 0;
}

/* DevX flex parser list callbacks. */
struct mlx5_list_entry *
mlx5_flex_parser_create_cb(void *list_ctx, void *ctx)
{
	struct mlx5_dev_ctx_shared *sh = list_ctx;
	struct mlx5_flex_parser_devx *fp, *conf = ctx;
	int ret;

	fp = mlx5_malloc(MLX5_MEM_ZERO,	sizeof(struct mlx5_flex_parser_devx),
			 0, SOCKET_ID_ANY);
	if (!fp)
		return NULL;
	/* Copy the requested configurations. */
	fp->num_samples = conf->num_samples;
	memcpy(&fp->devx_conf, &conf->devx_conf, sizeof(fp->devx_conf));
	/* Create DevX flex parser. */
	fp->devx_obj = mlx5_devx_cmd_create_flex_parser(sh->cdev->ctx,
							&fp->devx_conf);
	if (!fp->devx_obj)
		goto error;
	/* Query the firmware assigned sample ids. */
	ret = mlx5_devx_cmd_query_parse_samples(fp->devx_obj,
						fp->sample_ids,
						fp->num_samples);
	if (ret)
		goto error;
	DRV_LOG(DEBUG, "DEVx flex parser %p created, samples num: %u",
		(const void *)fp, fp->num_samples);
	return &fp->entry;
error:
	if (fp->devx_obj)
		mlx5_devx_cmd_destroy((void *)(uintptr_t)fp->devx_obj);
	if (fp)
		mlx5_free(fp);
	return NULL;
}

int
mlx5_flex_parser_match_cb(void *list_ctx,
			  struct mlx5_list_entry *iter, void *ctx)
{
	struct mlx5_flex_parser_devx *fp =
		container_of(iter, struct mlx5_flex_parser_devx, entry);
	struct mlx5_flex_parser_devx *org =
		container_of(ctx, struct mlx5_flex_parser_devx, entry);

	RTE_SET_USED(list_ctx);
	return !iter || !ctx || memcmp(&fp->devx_conf,
				       &org->devx_conf,
				       sizeof(fp->devx_conf));
}

void
mlx5_flex_parser_remove_cb(void *list_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_flex_parser_devx *fp =
		container_of(entry, struct mlx5_flex_parser_devx, entry);

	RTE_SET_USED(list_ctx);
	MLX5_ASSERT(fp->devx_obj);
	claim_zero(mlx5_devx_cmd_destroy(fp->devx_obj));
	DRV_LOG(DEBUG, "DEVx flex parser %p destroyed", (const void *)fp);
	mlx5_free(entry);
}

struct mlx5_list_entry *
mlx5_flex_parser_clone_cb(void *list_ctx,
			  struct mlx5_list_entry *entry, void *ctx)
{
	struct mlx5_flex_parser_devx *fp;

	RTE_SET_USED(list_ctx);
	RTE_SET_USED(entry);
	fp = mlx5_malloc(0, sizeof(struct mlx5_flex_parser_devx),
			 0, SOCKET_ID_ANY);
	if (!fp)
		return NULL;
	memcpy(fp, ctx, sizeof(struct mlx5_flex_parser_devx));
	return &fp->entry;
}

void
mlx5_flex_parser_clone_free_cb(void *list_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_flex_parser_devx *fp =
		container_of(entry, struct mlx5_flex_parser_devx, entry);
	RTE_SET_USED(list_ctx);
	mlx5_free(fp);
}
