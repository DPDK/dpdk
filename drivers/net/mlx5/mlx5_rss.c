/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>

#include "mlx5.h"
#include "mlx5_defs.h"
#include "mlx5_rxtx.h"

/**
 * DPDK callback to update the RSS hash configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[in] rss_conf
 *   RSS configuration data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rss_hash_update(struct rte_eth_dev *dev,
		     struct rte_eth_rss_conf *rss_conf)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int idx;

	if (rss_conf->rss_hf & MLX5_RSS_HF_MASK) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (rss_conf->rss_key && rss_conf->rss_key_len) {
		if (rss_conf->rss_key_len != MLX5_RSS_HASH_KEY_LEN) {
			DRV_LOG(ERR,
				"port %u RSS key len must be %s Bytes long",
				dev->data->port_id,
				RTE_STR(MLX5_RSS_HASH_KEY_LEN));
			rte_errno = EINVAL;
			return -rte_errno;
		}
		priv->rss_conf.rss_key = rte_realloc(priv->rss_conf.rss_key,
						     rss_conf->rss_key_len, 0);
		if (!priv->rss_conf.rss_key) {
			rte_errno = ENOMEM;
			return -rte_errno;
		}
		memcpy(priv->rss_conf.rss_key, rss_conf->rss_key,
		       rss_conf->rss_key_len);
		priv->rss_conf.rss_key_len = rss_conf->rss_key_len;
	}
	priv->rss_conf.rss_hf = rss_conf->rss_hf;
	/* Enable the RSS hash in all Rx queues. */
	for (i = 0, idx = 0; idx != priv->rxqs_n; ++i) {
		if (!(*priv->rxqs)[i])
			continue;
		(*priv->rxqs)[i]->rss_hash = !!rss_conf->rss_hf &&
			!!(dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS);
		++idx;
	}
	return 0;
}

/**
 * DPDK callback to get the RSS hash configuration.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[in, out] rss_conf
 *   RSS configuration data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rss_hash_conf_get(struct rte_eth_dev *dev,
		       struct rte_eth_rss_conf *rss_conf)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!rss_conf) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (rss_conf->rss_key &&
	    (rss_conf->rss_key_len >= priv->rss_conf.rss_key_len)) {
		memcpy(rss_conf->rss_key, priv->rss_conf.rss_key,
		       priv->rss_conf.rss_key_len);
	}
	rss_conf->rss_key_len = priv->rss_conf.rss_key_len;
	rss_conf->rss_hf = priv->rss_conf.rss_hf;
	return 0;
}

/**
 * Allocate/reallocate RETA index table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @praram reta_size
 *   The size of the array to allocate.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rss_reta_index_resize(struct rte_eth_dev *dev, unsigned int reta_size)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	void *mem;
	unsigned int old_size = priv->reta_idx_n;

	if (priv->reta_idx_n == reta_size)
		return 0;

	mem = rte_realloc(priv->reta_idx,
			  reta_size * sizeof((*priv->reta_idx)[0]), 0);
	if (!mem) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->reta_idx = mem;
	priv->reta_idx_n = reta_size;
	if (old_size < reta_size)
		memset(&(*priv->reta_idx)[old_size], 0,
		       (reta_size - old_size) *
		       sizeof((*priv->reta_idx)[0]));
	return 0;
}

/**
 * DPDK callback to get the RETA indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param reta_conf
 *   Pointer to RETA configuration structure array.
 * @param reta_size
 *   Size of the RETA table.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int idx;
	unsigned int i;

	if (!reta_size || reta_size > priv->reta_idx_n) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	/* Fill each entry of the table even if its bit is not set. */
	for (idx = 0, i = 0; (i != reta_size); ++i) {
		idx = i / RTE_RETA_GROUP_SIZE;
		reta_conf[idx].reta[i % RTE_RETA_GROUP_SIZE] =
			(*priv->reta_idx)[i];
	}
	return 0;
}

/**
 * DPDK callback to update the RETA indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param reta_conf
 *   Pointer to RETA configuration structure array.
 * @param reta_size
 *   Size of the RETA table.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	int ret;
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int idx;
	unsigned int i;
	unsigned int pos;

	if (!reta_size) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ret = mlx5_rss_reta_index_resize(dev, reta_size);
	if (ret)
		return ret;
	for (idx = 0, i = 0; (i != reta_size); ++i) {
		idx = i / RTE_RETA_GROUP_SIZE;
		pos = i % RTE_RETA_GROUP_SIZE;
		if (((reta_conf[idx].mask >> i) & 0x1) == 0)
			continue;
		assert(reta_conf[idx].reta[pos] < priv->rxqs_n);
		(*priv->reta_idx)[i] = reta_conf[idx].reta[pos];
	}
	if (dev->data->dev_started) {
		mlx5_dev_stop(dev);
		return mlx5_dev_start(dev);
	}
	return 0;
}
