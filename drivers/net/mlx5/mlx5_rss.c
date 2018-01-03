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
#include <rte_ethdev.h>

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
 *   0 on success, negative errno value on failure.
 */
int
mlx5_rss_hash_update(struct rte_eth_dev *dev,
		     struct rte_eth_rss_conf *rss_conf)
{
	struct priv *priv = dev->data->dev_private;
	int ret = 0;

	priv_lock(priv);
	if (rss_conf->rss_hf & MLX5_RSS_HF_MASK) {
		ret = -EINVAL;
		goto out;
	}
	if (rss_conf->rss_key && rss_conf->rss_key_len) {
		priv->rss_conf.rss_key = rte_realloc(priv->rss_conf.rss_key,
						     rss_conf->rss_key_len, 0);
		if (!priv->rss_conf.rss_key) {
			ret = -ENOMEM;
			goto out;
		}
		memcpy(priv->rss_conf.rss_key, rss_conf->rss_key,
		       rss_conf->rss_key_len);
		priv->rss_conf.rss_key_len = rss_conf->rss_key_len;
	}
	priv->rss_conf.rss_hf = rss_conf->rss_hf;
out:
	priv_unlock(priv);
	return ret;
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
 *   0 on success, negative errno value on failure.
 */
int
mlx5_rss_hash_conf_get(struct rte_eth_dev *dev,
		       struct rte_eth_rss_conf *rss_conf)
{
	struct priv *priv = dev->data->dev_private;

	if (!rss_conf)
		return -EINVAL;
	priv_lock(priv);
	if (rss_conf->rss_key &&
	    (rss_conf->rss_key_len >= priv->rss_conf.rss_key_len)) {
		memcpy(rss_conf->rss_key, priv->rss_conf.rss_key,
		       priv->rss_conf.rss_key_len);
	}
	rss_conf->rss_key_len = priv->rss_conf.rss_key_len;
	rss_conf->rss_hf = priv->rss_conf.rss_hf;
	priv_unlock(priv);
	return 0;
}

/**
 * Allocate/reallocate RETA index table.
 *
 * @param priv
 *   Pointer to private structure.
 * @praram reta_size
 *   The size of the array to allocate.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_rss_reta_index_resize(struct priv *priv, unsigned int reta_size)
{
	void *mem;
	unsigned int old_size = priv->reta_idx_n;

	if (priv->reta_idx_n == reta_size)
		return 0;

	mem = rte_realloc(priv->reta_idx,
			  reta_size * sizeof((*priv->reta_idx)[0]), 0);
	if (!mem)
		return ENOMEM;
	priv->reta_idx = mem;
	priv->reta_idx_n = reta_size;

	if (old_size < reta_size)
		memset(&(*priv->reta_idx)[old_size], 0,
		       (reta_size - old_size) *
		       sizeof((*priv->reta_idx)[0]));
	return 0;
}

/**
 * Query RETA table.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in, out] reta_conf
 *   Pointer to the first RETA configuration structure.
 * @param reta_size
 *   Number of entries.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_dev_rss_reta_query(struct priv *priv,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			unsigned int reta_size)
{
	unsigned int idx;
	unsigned int i;

	if (!reta_size || reta_size > priv->reta_idx_n)
		return EINVAL;
	/* Fill each entry of the table even if its bit is not set. */
	for (idx = 0, i = 0; (i != reta_size); ++i) {
		idx = i / RTE_RETA_GROUP_SIZE;
		reta_conf[idx].reta[i % RTE_RETA_GROUP_SIZE] =
			(*priv->reta_idx)[i];
	}
	return 0;
}

/**
 * Update RETA table.
 *
 * @param priv
 *   Pointer to private structure.
 * @param[in] reta_conf
 *   Pointer to the first RETA configuration structure.
 * @param reta_size
 *   Number of entries.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
priv_dev_rss_reta_update(struct priv *priv,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 unsigned int reta_size)
{
	unsigned int idx;
	unsigned int i;
	unsigned int pos;
	int ret;

	if (!reta_size)
		return EINVAL;
	ret = priv_rss_reta_index_resize(priv, reta_size);
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
 *   0 on success, negative errno value on failure.
 */
int
mlx5_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	int ret;
	struct priv *priv = dev->data->dev_private;

	priv_lock(priv);
	ret = priv_dev_rss_reta_query(priv, reta_conf, reta_size);
	priv_unlock(priv);
	return -ret;
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
 *   0 on success, negative errno value on failure.
 */
int
mlx5_dev_rss_reta_update(struct rte_eth_dev *dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	int ret;
	struct priv *priv = dev->data->dev_private;

	priv_lock(priv);
	ret = priv_dev_rss_reta_update(priv, reta_conf, reta_size);
	priv_unlock(priv);
	if (dev->data->dev_started) {
		mlx5_dev_stop(dev);
		mlx5_dev_start(dev);
	}
	return -ret;
}
