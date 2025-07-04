/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Google LLC
 */

#include "gve_adminq.h"
#include "gve_ethdev.h"

#define GVE_RSS_HASH_IPV4		BIT(0)
#define GVE_RSS_HASH_TCPV4		BIT(1)
#define GVE_RSS_HASH_IPV6		BIT(2)
#define GVE_RSS_HASH_IPV6_EX		BIT(3)
#define GVE_RSS_HASH_TCPV6		BIT(4)
#define GVE_RSS_HASH_TCPV6_EX		BIT(5)
#define GVE_RSS_HASH_UDPV4		BIT(6)
#define GVE_RSS_HASH_UDPV6		BIT(7)
#define GVE_RSS_HASH_UDPV6_EX		BIT(8)

#define GVE_RSS_OFFLOAD_DEFAULT (	\
	GVE_RSS_HASH_IPV4 |		\
	GVE_RSS_HASH_TCPV4 |		\
	GVE_RSS_HASH_IPV6 |		\
	GVE_RSS_HASH_IPV6_EX |		\
	GVE_RSS_HASH_TCPV6 |		\
	GVE_RSS_HASH_TCPV6_EX |		\
	GVE_RSS_HASH_UDPV4 |		\
	GVE_RSS_HASH_UDPV6 |		\
	GVE_RSS_HASH_UDPV6_EX)

/**
 * Generates default RSS redirection table based on the number of queues the
 * device is configured with. This assigns hash values to queues in a
 * round-robin manner.
 */
int
gve_generate_rss_reta(struct rte_eth_dev *dev, struct gve_rss_config *config);

/**
 * Initializes `gve_rss_conf`, setting the fields to default values and
 * allocating memory for the RSS key and redirection table.
 */
int
gve_init_rss_config(struct gve_rss_config *gve_rss_conf,
		uint16_t key_size, uint16_t indir_size);

/**
 * Initializes `gve_rss_conf` based on the RSS configuration stored in `priv`.
 */
int
gve_init_rss_config_from_priv(struct gve_priv *priv,
	struct gve_rss_config *gve_rss_conf);

/**
 * Frees RSS key and redriection table pointers stored in `gve_rss_conf`.
 */
void
gve_free_rss_config(struct gve_rss_config *gve_rss_conf);

/**
 * Updates the rss_config stored in `priv` with the contents of `config`.
 */
int
gve_update_priv_rss_config(struct gve_priv *priv,
			   struct gve_rss_config *config);

/**
 * Updates the RSS key stored in `gve_rss_conf`. It is prioritized as follows:
 *	1) retrieve from `rss_conf`, if non-null
 *	2) retrieve from `priv`, if non-null
 * If keys from both sources are unset, return -EINVAL.
 */
int
gve_update_rss_key(struct gve_priv *priv, struct gve_rss_config *gve_rss_conf,
		   struct rte_eth_rss_conf *rss_conf);

/**
 * Updates the RSS hash types stored in `gve_rss_conf`. It is prioritized as
 * follows:
 *	1) retrieve from `rss_conf`, if set
 *	2) retrieve from priv, if RSS has been configured
 *	3) set default RSS offload
 */
int
gve_update_rss_hash_types(struct gve_priv *priv,
	struct gve_rss_config *gve_rss_conf, struct rte_eth_rss_conf *rss_conf);

/**
 * Ensures that only supported RSS hash fields are set in `rte_rss_hf`.
 */
static inline int
gve_validate_rss_hf(uint64_t rte_rss_hf) {
	return rte_rss_hf & ~GVE_RTE_RSS_OFFLOAD_ALL;
}

/**
 * Converts RSS hash types from RTE values to GVE values, storing them in
 * `gve_rss_conf`.
 */
void
rte_to_gve_rss_hf(uint64_t rte_rss_hf, struct gve_rss_config *gve_rss_conf);

/**
 * Converts RSS hash types from GVE values to RTE values, storing them in
 * `rss_conf`.
 */
void
gve_to_rte_rss_hf(uint16_t gve_rss_types, struct rte_eth_rss_conf *rss_conf);

