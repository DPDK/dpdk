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
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_common.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_defs.h"

/**
 * Get MAC address by querying netdevice.
 *
 * @param[in] priv
 *   struct priv for the requested device.
 * @param[out] mac
 *   MAC address output buffer.
 *
 * @return
 *   0 on success, -1 on failure and errno is set.
 */
int
priv_get_mac(struct priv *priv, uint8_t (*mac)[ETHER_ADDR_LEN])
{
	struct ifreq request;

	if (priv_ifreq(priv, SIOCGIFHWADDR, &request))
		return -1;
	memcpy(mac, request.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
	return 0;
}

/**
 * Delete MAC flow steering rule.
 *
 * @param hash_rxq
 *   Pointer to hash RX queue structure.
 * @param mac_index
 *   MAC address index.
 * @param vlan_index
 *   VLAN index to use.
 */
static void
hash_rxq_del_mac_flow(struct hash_rxq *hash_rxq, unsigned int mac_index,
		      unsigned int vlan_index)
{
#ifndef NDEBUG
	const uint8_t (*mac)[ETHER_ADDR_LEN] =
		(const uint8_t (*)[ETHER_ADDR_LEN])
		hash_rxq->priv->mac[mac_index].addr_bytes;
#endif

	assert(mac_index < RTE_DIM(hash_rxq->mac_flow));
	assert(vlan_index < RTE_DIM(hash_rxq->mac_flow[mac_index]));
	if (hash_rxq->mac_flow[mac_index][vlan_index] == NULL)
		return;
	DEBUG("%p: removing MAC address %02x:%02x:%02x:%02x:%02x:%02x index %u"
	      " VLAN index %u",
	      (void *)hash_rxq,
	      (*mac)[0], (*mac)[1], (*mac)[2], (*mac)[3], (*mac)[4], (*mac)[5],
	      mac_index,
	      vlan_index);
	claim_zero(ibv_exp_destroy_flow(hash_rxq->mac_flow
					[mac_index][vlan_index]));
	hash_rxq->mac_flow[mac_index][vlan_index] = NULL;
}

/**
 * Unregister a MAC address from a hash RX queue.
 *
 * @param hash_rxq
 *   Pointer to hash RX queue structure.
 * @param mac_index
 *   MAC address index.
 */
static void
hash_rxq_mac_addr_del(struct hash_rxq *hash_rxq, unsigned int mac_index)
{
	unsigned int i;

	assert(mac_index < RTE_DIM(hash_rxq->mac_flow));
	for (i = 0; (i != RTE_DIM(hash_rxq->mac_flow[mac_index])); ++i)
		hash_rxq_del_mac_flow(hash_rxq, mac_index, i);
}

/**
 * Unregister all MAC addresses from a hash RX queue.
 *
 * @param hash_rxq
 *   Pointer to hash RX queue structure.
 */
void
hash_rxq_mac_addrs_del(struct hash_rxq *hash_rxq)
{
	unsigned int i;

	for (i = 0; (i != RTE_DIM(hash_rxq->mac_flow)); ++i)
		hash_rxq_mac_addr_del(hash_rxq, i);
}

/**
 * Unregister a MAC address.
 *
 * This is done for each hash RX queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param mac_index
 *   MAC address index.
 */
static void
priv_mac_addr_del(struct priv *priv, unsigned int mac_index)
{
	unsigned int i;

	assert(mac_index < RTE_DIM(priv->mac));
	if (!BITFIELD_ISSET(priv->mac_configured, mac_index))
		return;
	for (i = 0; (i != priv->hash_rxqs_n); ++i)
		hash_rxq_mac_addr_del(&(*priv->hash_rxqs)[i], mac_index);
	BITFIELD_RESET(priv->mac_configured, mac_index);
}

/**
 * Unregister all MAC addresses from all hash RX queues.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
priv_mac_addrs_disable(struct priv *priv)
{
	unsigned int i;

	for (i = 0; (i != priv->hash_rxqs_n); ++i)
		hash_rxq_mac_addrs_del(&(*priv->hash_rxqs)[i]);
}

/**
 * DPDK callback to remove a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param index
 *   MAC address index.
 */
void
mlx5_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct priv *priv = dev->data->dev_private;

	if (mlx5_is_secondary())
		return;

	priv_lock(priv);
	DEBUG("%p: removing MAC address from index %" PRIu32,
	      (void *)dev, index);
	if (index >= RTE_DIM(priv->mac))
		goto end;
	priv_mac_addr_del(priv, index);
end:
	priv_unlock(priv);
}

/**
 * Add MAC flow steering rule.
 *
 * @param hash_rxq
 *   Pointer to hash RX queue structure.
 * @param mac_index
 *   MAC address index to register.
 * @param vlan_index
 *   VLAN index to use.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
hash_rxq_add_mac_flow(struct hash_rxq *hash_rxq, unsigned int mac_index,
		      unsigned int vlan_index)
{
	struct ibv_exp_flow *flow;
	struct priv *priv = hash_rxq->priv;
	const uint8_t (*mac)[ETHER_ADDR_LEN] =
			(const uint8_t (*)[ETHER_ADDR_LEN])
			priv->mac[mac_index].addr_bytes;
	FLOW_ATTR_SPEC_ETH(data, priv_flow_attr(priv, NULL, 0, hash_rxq->type));
	struct ibv_exp_flow_attr *attr = &data->attr;
	struct ibv_exp_flow_spec_eth *spec = &data->spec;
	unsigned int vlan_enabled = !!priv->vlan_filter_n;
	unsigned int vlan_id = priv->vlan_filter[vlan_index];

	assert(mac_index < RTE_DIM(hash_rxq->mac_flow));
	assert(vlan_index < RTE_DIM(hash_rxq->mac_flow[mac_index]));
	if (hash_rxq->mac_flow[mac_index][vlan_index] != NULL)
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
	*spec = (struct ibv_exp_flow_spec_eth){
		.type = IBV_EXP_FLOW_SPEC_ETH,
		.size = sizeof(*spec),
		.val = {
			.dst_mac = {
				(*mac)[0], (*mac)[1], (*mac)[2],
				(*mac)[3], (*mac)[4], (*mac)[5]
			},
			.vlan_tag = (vlan_enabled ? htons(vlan_id) : 0),
		},
		.mask = {
			.dst_mac = "\xff\xff\xff\xff\xff\xff",
			.vlan_tag = (vlan_enabled ? htons(0xfff) : 0),
		},
	};
	DEBUG("%p: adding MAC address %02x:%02x:%02x:%02x:%02x:%02x index %u"
	      " VLAN index %u filtering %s, ID %u",
	      (void *)hash_rxq,
	      (*mac)[0], (*mac)[1], (*mac)[2], (*mac)[3], (*mac)[4], (*mac)[5],
	      mac_index,
	      vlan_index,
	      (vlan_enabled ? "enabled" : "disabled"),
	      vlan_id);
	/* Create related flow. */
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
	hash_rxq->mac_flow[mac_index][vlan_index] = flow;
	return 0;
}

/**
 * Register a MAC address in a hash RX queue.
 *
 * @param hash_rxq
 *   Pointer to hash RX queue structure.
 * @param mac_index
 *   MAC address index to register.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
hash_rxq_mac_addr_add(struct hash_rxq *hash_rxq, unsigned int mac_index)
{
	struct priv *priv = hash_rxq->priv;
	unsigned int i = 0;
	int ret;

	assert(mac_index < RTE_DIM(hash_rxq->mac_flow));
	assert(RTE_DIM(hash_rxq->mac_flow[mac_index]) ==
	       RTE_DIM(priv->vlan_filter));
	/* Add a MAC address for each VLAN filter, or at least once. */
	do {
		ret = hash_rxq_add_mac_flow(hash_rxq, mac_index, i);
		if (ret) {
			/* Failure, rollback. */
			while (i != 0)
				hash_rxq_del_mac_flow(hash_rxq, mac_index,
						      --i);
			return ret;
		}
	} while (++i < priv->vlan_filter_n);
	return 0;
}

/**
 * Register all MAC addresses in a hash RX queue.
 *
 * @param hash_rxq
 *   Pointer to hash RX queue structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
hash_rxq_mac_addrs_add(struct hash_rxq *hash_rxq)
{
	struct priv *priv = hash_rxq->priv;
	unsigned int i;
	int ret;

	assert(RTE_DIM(priv->mac) == RTE_DIM(hash_rxq->mac_flow));
	for (i = 0; (i != RTE_DIM(priv->mac)); ++i) {
		if (!BITFIELD_ISSET(priv->mac_configured, i))
			continue;
		ret = hash_rxq_mac_addr_add(hash_rxq, i);
		if (!ret)
			continue;
		/* Failure, rollback. */
		while (i != 0)
			hash_rxq_mac_addr_del(hash_rxq, --i);
		assert(ret > 0);
		return ret;
	}
	return 0;
}

/**
 * Register a MAC address.
 *
 * This is done for each hash RX queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param mac_index
 *   MAC address index to use.
 * @param mac
 *   MAC address to register.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_mac_addr_add(struct priv *priv, unsigned int mac_index,
		  const uint8_t (*mac)[ETHER_ADDR_LEN])
{
	unsigned int i;
	int ret;

	assert(mac_index < RTE_DIM(priv->mac));
	/* First, make sure this address isn't already configured. */
	for (i = 0; (i != RTE_DIM(priv->mac)); ++i) {
		/* Skip this index, it's going to be reconfigured. */
		if (i == mac_index)
			continue;
		if (!BITFIELD_ISSET(priv->mac_configured, i))
			continue;
		if (memcmp(priv->mac[i].addr_bytes, *mac, sizeof(*mac)))
			continue;
		/* Address already configured elsewhere, return with error. */
		return EADDRINUSE;
	}
	if (BITFIELD_ISSET(priv->mac_configured, mac_index))
		priv_mac_addr_del(priv, mac_index);
	priv->mac[mac_index] = (struct ether_addr){
		{
			(*mac)[0], (*mac)[1], (*mac)[2],
			(*mac)[3], (*mac)[4], (*mac)[5]
		}
	};
	if (!priv_allow_flow_type(priv, HASH_RXQ_FLOW_TYPE_MAC))
		goto end;
	for (i = 0; (i != priv->hash_rxqs_n); ++i) {
		ret = hash_rxq_mac_addr_add(&(*priv->hash_rxqs)[i], mac_index);
		if (!ret)
			continue;
		/* Failure, rollback. */
		while (i != 0)
			hash_rxq_mac_addr_del(&(*priv->hash_rxqs)[--i],
					      mac_index);
		return ret;
	}
end:
	BITFIELD_SET(priv->mac_configured, mac_index);
	return 0;
}

/**
 * Register all MAC addresses in all hash RX queues.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
priv_mac_addrs_enable(struct priv *priv)
{
	unsigned int i;
	int ret;

	if (!priv_allow_flow_type(priv, HASH_RXQ_FLOW_TYPE_MAC))
		return 0;
	for (i = 0; (i != priv->hash_rxqs_n); ++i) {
		ret = hash_rxq_mac_addrs_add(&(*priv->hash_rxqs)[i]);
		if (!ret)
			continue;
		/* Failure, rollback. */
		while (i != 0)
			hash_rxq_mac_addrs_del(&(*priv->hash_rxqs)[--i]);
		assert(ret > 0);
		return ret;
	}
	return 0;
}

/**
 * DPDK callback to add a MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 * @param index
 *   MAC address index.
 * @param vmdq
 *   VMDq pool index to associate address with (ignored).
 */
int
mlx5_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		  uint32_t index, uint32_t vmdq)
{
	struct priv *priv = dev->data->dev_private;
	int re;

	if (mlx5_is_secondary())
		return -ENOTSUP;

	(void)vmdq;
	priv_lock(priv);
	DEBUG("%p: adding MAC address at index %" PRIu32,
	      (void *)dev, index);
	if (index >= RTE_DIM(priv->mac)) {
		re = EINVAL;
		goto end;
	}
	re = priv_mac_addr_add(priv, index,
			       (const uint8_t (*)[ETHER_ADDR_LEN])
			       mac_addr->addr_bytes);
end:
	priv_unlock(priv);
	return -re;
}

/**
 * DPDK callback to set primary MAC address.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 */
void
mlx5_mac_addr_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr)
{
	DEBUG("%p: setting primary MAC address", (void *)dev);
	mlx5_mac_addr_remove(dev, 0);
	mlx5_mac_addr_add(dev, mac_addr, 0, 0);
}
