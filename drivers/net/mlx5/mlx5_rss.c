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
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_malloc.h>
#include <rte_ethdev.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5.h"
#include "mlx5_rxtx.h"

/**
 * Register a RSS key.
 *
 * @param priv
 *   Pointer to private structure.
 * @param key
 *   Hash key to register.
 * @param key_len
 *   Hash key length in bytes.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
rss_hash_rss_conf_new_key(struct priv *priv, const uint8_t *key,
			  unsigned int key_len)
{
	struct rte_eth_rss_conf *rss_conf;

	rss_conf = rte_realloc(priv->rss_conf,
			       (sizeof(*rss_conf) + key_len),
			       0);
	if (!rss_conf)
		return ENOMEM;
	rss_conf->rss_key = (void *)(rss_conf + 1);
	rss_conf->rss_key_len = key_len;
	memcpy(rss_conf->rss_key, key, key_len);
	priv->rss_conf = rss_conf;
	return 0;
}

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
	int err = 0;

	priv_lock(priv);

	assert(priv->rss_conf != NULL);

	/* Apply configuration. */
	if (rss_conf->rss_key)
		err = rss_hash_rss_conf_new_key(priv,
						rss_conf->rss_key,
						rss_conf->rss_key_len);
	else
		err = rss_hash_rss_conf_new_key(priv,
						rss_hash_default_key,
						rss_hash_default_key_len);

	/* Store the configuration set into port configure.
	 * This will enable/disable hash RX queues associated to the protocols
	 * enabled/disabled by this update. */
	priv->dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf =
		rss_conf->rss_hf;
	priv_unlock(priv);
	assert(err >= 0);
	return -err;
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

	priv_lock(priv);

	assert(priv->rss_conf != NULL);

	if (rss_conf->rss_key &&
	    rss_conf->rss_key_len >= priv->rss_conf->rss_key_len)
		memcpy(rss_conf->rss_key,
		       priv->rss_conf->rss_key,
		       priv->rss_conf->rss_key_len);
	rss_conf->rss_key_len = priv->rss_conf->rss_key_len;
	/* FIXME: rss_hf should be more specific. */
	rss_conf->rss_hf = ETH_RSS_PROTO_MASK;

	priv_unlock(priv);
	return 0;
}
