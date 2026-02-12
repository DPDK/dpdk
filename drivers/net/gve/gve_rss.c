/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Google LLC
 */

#include "gve_rss.h"

int
gve_generate_rss_reta(struct rte_eth_dev *dev, struct gve_rss_config *config)
{
	int i;
	if (!config || !config->indir)
		return -EINVAL;

	for (i = 0; i < config->indir_size; i++)
		config->indir[i] = i % dev->data->nb_rx_queues;

	return 0;
}


int
gve_init_rss_config(struct gve_rss_config *gve_rss_conf,
		uint16_t key_size, uint16_t indir_size)
{
	int err;

	gve_rss_conf->alg = GVE_RSS_HASH_TOEPLITZ;

	gve_rss_conf->key_size = key_size;
	gve_rss_conf->key = rte_zmalloc("rss key",
		key_size * sizeof(*gve_rss_conf->key),
		RTE_CACHE_LINE_SIZE);
	if (!gve_rss_conf->key)
		return -ENOMEM;

	gve_rss_conf->indir_size = indir_size;
	gve_rss_conf->indir = rte_zmalloc("rss reta",
		indir_size * sizeof(*gve_rss_conf->indir),
		RTE_CACHE_LINE_SIZE);
	if (!gve_rss_conf->indir) {
		err = -ENOMEM;
		goto err_with_key;
	}

	return 0;
err_with_key:
	rte_free(gve_rss_conf->key);
	return err;
}

int
gve_init_rss_config_from_priv(struct gve_priv *priv,
	struct gve_rss_config *gve_rss_conf)
{
	int err = gve_init_rss_config(gve_rss_conf, priv->rss_config.key_size,
		priv->rss_config.indir_size);
	if (err)
		return err;

	gve_rss_conf->hash_types = priv->rss_config.hash_types;
	gve_rss_conf->alg = priv->rss_config.alg;
	memcpy(gve_rss_conf->key, priv->rss_config.key,
		gve_rss_conf->key_size * sizeof(*gve_rss_conf->key));
	memcpy(gve_rss_conf->indir, priv->rss_config.indir,
		gve_rss_conf->indir_size * sizeof(*gve_rss_conf->indir));

	return 0;
}

void
gve_free_rss_config(struct gve_rss_config *gve_rss_conf)
{
	rte_free(gve_rss_conf->indir);
	gve_rss_conf->indir = NULL;
	rte_free(gve_rss_conf->key);
	gve_rss_conf->key = NULL;
}

int
gve_update_priv_rss_config(struct gve_priv *priv, struct gve_rss_config *config)
{
	struct gve_rss_config *priv_config = &priv->rss_config;
	int key_bytes, indir_bytes;

	if (!config)
		return -EINVAL;
	if (config->key_size == 0 || !config->key)
		return -EINVAL;
	if (config->indir_size == 0 || !config->indir)
		return -EINVAL;

	priv_config->hash_types = config->hash_types;
	priv_config->alg = config->alg;

	priv_config->key_size = config->key_size;
	key_bytes = priv_config->key_size * sizeof(*priv_config->key);
	if (!priv_config->key)
		priv_config->key = rte_zmalloc("priv rss key", key_bytes,
			RTE_CACHE_LINE_SIZE);
	else
		priv_config->key = rte_realloc(priv_config->key, key_bytes,
			RTE_CACHE_LINE_SIZE);
	if (!priv_config->key)
		return -ENOMEM;

	priv_config->indir_size = config->indir_size;
	indir_bytes = priv_config->indir_size * sizeof(*priv_config->indir);
	if (!priv_config->indir)
		priv_config->indir = rte_zmalloc("priv rss reta", indir_bytes,
			RTE_CACHE_LINE_SIZE);
	else
		priv_config->indir = rte_realloc(priv_config->indir,
			indir_bytes, RTE_CACHE_LINE_SIZE);

	if (!priv_config->indir)
		return -ENOMEM;

	memcpy(priv_config->indir, config->indir,
		config->indir_size * sizeof(*priv_config->indir));
	memcpy(priv_config->key, config->key,
		config->key_size * sizeof(*priv_config->key));

	return 0;
}

int
gve_update_rss_key(struct gve_priv *priv, struct gve_rss_config *gve_rss_conf,
	struct rte_eth_rss_conf *rss_conf)
{
	if (rss_conf->rss_key_len && rss_conf->rss_key) {
		gve_rss_conf->key_size = rss_conf->rss_key_len;
		memcpy(gve_rss_conf->key, rss_conf->rss_key,
			gve_rss_conf->key_size * sizeof(*gve_rss_conf->key));
	} else if (priv->rss_config.key_size && priv->rss_config.key) {
		gve_rss_conf->key_size = priv->rss_config.key_size;
		memcpy(gve_rss_conf->key, priv->rss_config.key,
			gve_rss_conf->key_size * sizeof(*gve_rss_conf->key));
	} else {
		PMD_DRV_LOG(ERR, "RSS key must be set as part of initial RSS "
			"configuration.");
		return -EINVAL;
	}
	return 0;
}

int
gve_update_rss_hash_types(struct gve_priv *priv,
	struct gve_rss_config *gve_rss_conf, struct rte_eth_rss_conf *rss_conf)
{
	/* Initialize to 0 before modifying. */
	gve_rss_conf->hash_types = 0;
	if (rss_conf->rss_hf)
		rte_to_gve_rss_hf(rss_conf->rss_hf, gve_rss_conf);
	else if (priv->rss_config.key_size && priv->rss_config.key)
		gve_rss_conf->hash_types = priv->rss_config.hash_types;
	else
		gve_rss_conf->hash_types = GVE_RSS_OFFLOAD_DEFAULT;
	return 0;
}

void
rte_to_gve_rss_hf(uint64_t rte_rss_hf, struct gve_rss_config *gve_rss_conf)
{
	if (rte_rss_hf & RTE_ETH_RSS_IPV4)
		gve_rss_conf->hash_types |= GVE_RSS_HASH_IPV4;
	if (rte_rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP)
		gve_rss_conf->hash_types |= GVE_RSS_HASH_TCPV4;
	if (rte_rss_hf & RTE_ETH_RSS_IPV6)
		gve_rss_conf->hash_types |= GVE_RSS_HASH_IPV6;
	if (rte_rss_hf & RTE_ETH_RSS_IPV6_EX)
		gve_rss_conf->hash_types |= GVE_RSS_HASH_IPV6_EX;
	if (rte_rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP)
		gve_rss_conf->hash_types |= GVE_RSS_HASH_TCPV6;
	if (rte_rss_hf & RTE_ETH_RSS_IPV6_TCP_EX)
		gve_rss_conf->hash_types |= GVE_RSS_HASH_TCPV6_EX;
	if (rte_rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
		gve_rss_conf->hash_types |= GVE_RSS_HASH_UDPV4;
	if (rte_rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP)
		gve_rss_conf->hash_types |= GVE_RSS_HASH_UDPV6;
	if (rte_rss_hf & RTE_ETH_RSS_IPV6_UDP_EX)
		gve_rss_conf->hash_types |= GVE_RSS_HASH_UDPV6_EX;
}

void
gve_to_rte_rss_hf(uint16_t gve_rss_types, struct rte_eth_rss_conf *rss_conf)
{
	if (gve_rss_types & GVE_RSS_HASH_IPV4)
		rss_conf->rss_hf |= RTE_ETH_RSS_IPV4;
	if (gve_rss_types & GVE_RSS_HASH_TCPV4)
		rss_conf->rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;
	if (gve_rss_types & GVE_RSS_HASH_IPV6)
		rss_conf->rss_hf |= RTE_ETH_RSS_IPV6;
	if (gve_rss_types & GVE_RSS_HASH_IPV6_EX)
		rss_conf->rss_hf |= RTE_ETH_RSS_IPV6_EX;
	if (gve_rss_types & GVE_RSS_HASH_TCPV6)
		rss_conf->rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_TCP;
	if (gve_rss_types & GVE_RSS_HASH_TCPV6_EX)
		rss_conf->rss_hf |= RTE_ETH_RSS_IPV6_TCP_EX;
	if (gve_rss_types & GVE_RSS_HASH_UDPV4)
		rss_conf->rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;
	if (gve_rss_types & GVE_RSS_HASH_UDPV6)
		rss_conf->rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_UDP;
	if (gve_rss_types & GVE_RSS_HASH_UDPV6_EX)
		rss_conf->rss_hf |= RTE_ETH_RSS_IPV6_UDP_EX;
}
