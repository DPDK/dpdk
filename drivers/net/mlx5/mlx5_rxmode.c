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
#include <rte_ethdev.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

/* Initialization data for special flows. */
static const struct special_flow_init special_flow_init[] = {
	[HASH_RXQ_FLOW_TYPE_PROMISC] = {
		.dst_mac_val = "\x00\x00\x00\x00\x00\x00",
		.dst_mac_mask = "\x00\x00\x00\x00\x00\x00",
		.hash_types =
			1 << HASH_RXQ_TCPV4 |
			1 << HASH_RXQ_UDPV4 |
			1 << HASH_RXQ_IPV4 |
#ifdef HAVE_FLOW_SPEC_IPV6
			1 << HASH_RXQ_TCPV6 |
			1 << HASH_RXQ_UDPV6 |
			1 << HASH_RXQ_IPV6 |
#endif /* HAVE_FLOW_SPEC_IPV6 */
			1 << HASH_RXQ_ETH |
			0,
	},
	[HASH_RXQ_FLOW_TYPE_ALLMULTI] = {
		.dst_mac_val = "\x01\x00\x00\x00\x00\x00",
		.dst_mac_mask = "\x01\x00\x00\x00\x00\x00",
		.hash_types =
			1 << HASH_RXQ_UDPV4 |
			1 << HASH_RXQ_IPV4 |
#ifdef HAVE_FLOW_SPEC_IPV6
			1 << HASH_RXQ_UDPV6 |
			1 << HASH_RXQ_IPV6 |
#endif /* HAVE_FLOW_SPEC_IPV6 */
			1 << HASH_RXQ_ETH |
			0,
	},
	[HASH_RXQ_FLOW_TYPE_BROADCAST] = {
		.dst_mac_val = "\xff\xff\xff\xff\xff\xff",
		.dst_mac_mask = "\xff\xff\xff\xff\xff\xff",
		.hash_types =
			1 << HASH_RXQ_UDPV4 |
			1 << HASH_RXQ_IPV4 |
#ifdef HAVE_FLOW_SPEC_IPV6
			1 << HASH_RXQ_UDPV6 |
			1 << HASH_RXQ_IPV6 |
#endif /* HAVE_FLOW_SPEC_IPV6 */
			1 << HASH_RXQ_ETH |
			0,
	},
#ifdef HAVE_FLOW_SPEC_IPV6
	[HASH_RXQ_FLOW_TYPE_IPV6MULTI] = {
		.dst_mac_val = "\x33\x33\x00\x00\x00\x00",
		.dst_mac_mask = "\xff\xff\x00\x00\x00\x00",
		.hash_types =
			1 << HASH_RXQ_UDPV6 |
			1 << HASH_RXQ_IPV6 |
			1 << HASH_RXQ_ETH |
			0,
	},
#endif /* HAVE_FLOW_SPEC_IPV6 */
};

/**
 * Enable a special flow in a hash RX queue.
 *
 * @param hash_rxq
 *   Pointer to hash RX queue structure.
 * @param flow_type
 *   Special flow type.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
hash_rxq_special_flow_enable(struct hash_rxq *hash_rxq,
			     enum hash_rxq_flow_type flow_type)
{
	struct priv *priv = hash_rxq->priv;
	struct ibv_exp_flow *flow;
	FLOW_ATTR_SPEC_ETH(data, priv_flow_attr(priv, NULL, 0, hash_rxq->type));
	struct ibv_exp_flow_attr *attr = &data->attr;
	struct ibv_exp_flow_spec_eth *spec = &data->spec;
	const uint8_t *mac;
	const uint8_t *mask;

	/* Check if flow is relevant for this hash_rxq. */
	if (!(special_flow_init[flow_type].hash_types & (1 << hash_rxq->type)))
		return 0;
	/* Check if flow already exists. */
	if (hash_rxq->special_flow[flow_type] != NULL)
		return 0;

	/*
	 * No padding must be inserted by the compiler between attr and spec.
	 * This layout is expected by libibverbs.
	 */
	assert(((uint8_t *)attr + sizeof(*attr)) == (uint8_t *)spec);
	priv_flow_attr(priv, attr, sizeof(data), hash_rxq->type);
	/* The first specification must be Ethernet. */
	assert(spec->type == IBV_EXP_FLOW_SPEC_ETH);
	assert(spec->size == sizeof(*spec));

	mac = special_flow_init[flow_type].dst_mac_val;
	mask = special_flow_init[flow_type].dst_mac_mask;
	*spec = (struct ibv_exp_flow_spec_eth){
		.type = IBV_EXP_FLOW_SPEC_ETH,
		.size = sizeof(*spec),
		.val = {
			.dst_mac = {
				mac[0], mac[1], mac[2],
				mac[3], mac[4], mac[5],
			},
		},
		.mask = {
			.dst_mac = {
				mask[0], mask[1], mask[2],
				mask[3], mask[4], mask[5],
			},
		},
	};

	errno = 0;
	flow = ibv_exp_create_flow(hash_rxq->qp, attr);
	if (flow == NULL) {
		/* It's not clear whether errno is always set in this case. */
		ERROR("%p: flow configuration failed, errno=%d: %s",
		      (void *)hash_rxq, errno,
		      (errno ? strerror(errno) : "Unknown error"));
		if (errno)
			return errno;
		return EINVAL;
	}
	hash_rxq->special_flow[flow_type] = flow;
	DEBUG("%p: enabling special flow %s (%d)",
	      (void *)hash_rxq, hash_rxq_flow_type_str(flow_type), flow_type);
	return 0;
}

/**
 * Disable a special flow in a hash RX queue.
 *
 * @param hash_rxq
 *   Pointer to hash RX queue structure.
 * @param flow_type
 *   Special flow type.
 */
static void
hash_rxq_special_flow_disable(struct hash_rxq *hash_rxq,
			      enum hash_rxq_flow_type flow_type)
{
	if (hash_rxq->special_flow[flow_type] == NULL)
		return;
	claim_zero(ibv_exp_destroy_flow(hash_rxq->special_flow[flow_type]));
	hash_rxq->special_flow[flow_type] = NULL;
	DEBUG("%p: special flow %s (%d) disabled",
	      (void *)hash_rxq, hash_rxq_flow_type_str(flow_type), flow_type);
}

/**
 * Enable a special flow in all hash RX queues.
 *
 * @param priv
 *   Private structure.
 * @param flow_type
 *   Special flow type.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_special_flow_enable(struct priv *priv, enum hash_rxq_flow_type flow_type)
{
	unsigned int i;

	if (!priv_allow_flow_type(priv, flow_type))
		return 0;
	for (i = 0; (i != priv->hash_rxqs_n); ++i) {
		struct hash_rxq *hash_rxq = &(*priv->hash_rxqs)[i];
		int ret;

		ret = hash_rxq_special_flow_enable(hash_rxq, flow_type);
		if (!ret)
			continue;
		/* Failure, rollback. */
		while (i != 0) {
			hash_rxq = &(*priv->hash_rxqs)[--i];
			hash_rxq_special_flow_disable(hash_rxq, flow_type);
		}
		return ret;
	}
	return 0;
}

/**
 * Disable a special flow in all hash RX queues.
 *
 * @param priv
 *   Private structure.
 * @param flow_type
 *   Special flow type.
 */
void
priv_special_flow_disable(struct priv *priv, enum hash_rxq_flow_type flow_type)
{
	unsigned int i;

	for (i = 0; (i != priv->hash_rxqs_n); ++i) {
		struct hash_rxq *hash_rxq = &(*priv->hash_rxqs)[i];

		hash_rxq_special_flow_disable(hash_rxq, flow_type);
	}
}

/**
 * Enable all special flows in all hash RX queues.
 *
 * @param priv
 *   Private structure.
 */
int
priv_special_flow_enable_all(struct priv *priv)
{
	enum hash_rxq_flow_type flow_type;

	for (flow_type = 0; flow_type != HASH_RXQ_FLOW_TYPE_MAC; ++flow_type) {
		int ret;

		ret = priv_special_flow_enable(priv, flow_type);
		if (!ret)
			continue;
		/* Failure, rollback. */
		while (flow_type)
			priv_special_flow_disable(priv, --flow_type);
		return ret;
	}
	return 0;
}

/**
 * Disable all special flows in all hash RX queues.
 *
 * @param priv
 *   Private structure.
 */
void
priv_special_flow_disable_all(struct priv *priv)
{
	enum hash_rxq_flow_type flow_type;

	for (flow_type = 0; flow_type != HASH_RXQ_FLOW_TYPE_MAC; ++flow_type)
		priv_special_flow_disable(priv, flow_type);
}

/**
 * DPDK callback to enable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	int ret;

	priv_lock(priv);
	priv->promisc_req = 1;
	ret = priv_rehash_flows(priv);
	if (ret)
		ERROR("error while enabling promiscuous mode: %s",
		      strerror(ret));
	priv_unlock(priv);
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
	struct priv *priv = dev->data->dev_private;
	int ret;

	priv_lock(priv);
	priv->promisc_req = 0;
	ret = priv_rehash_flows(priv);
	if (ret)
		ERROR("error while disabling promiscuous mode: %s",
		      strerror(ret));
	priv_unlock(priv);
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
	struct priv *priv = dev->data->dev_private;
	int ret;

	priv_lock(priv);
	priv->allmulti_req = 1;
	ret = priv_rehash_flows(priv);
	if (ret)
		ERROR("error while enabling allmulticast mode: %s",
		      strerror(ret));
	priv_unlock(priv);
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
	struct priv *priv = dev->data->dev_private;
	int ret;

	priv_lock(priv);
	priv->allmulti_req = 0;
	ret = priv_rehash_flows(priv);
	if (ret)
		ERROR("error while disabling allmulticast mode: %s",
		      strerror(ret));
	priv_unlock(priv);
}
