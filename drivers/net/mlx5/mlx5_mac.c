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
#include <linux/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

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
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_common.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
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
 * @param rxq
 *   Pointer to RX queue structure.
 * @param mac_index
 *   MAC address index.
 */
static void
rxq_del_mac_flow(struct rxq *rxq, unsigned int mac_index)
{
#ifndef NDEBUG
	const uint8_t (*mac)[ETHER_ADDR_LEN] =
		(const uint8_t (*)[ETHER_ADDR_LEN])
		rxq->priv->mac[mac_index].addr_bytes;
#endif

	assert(mac_index < RTE_DIM(rxq->mac_flow));
	if (rxq->mac_flow[mac_index] == NULL)
		return;
	DEBUG("%p: removing MAC address %02x:%02x:%02x:%02x:%02x:%02x index %u",
	      (void *)rxq,
	      (*mac)[0], (*mac)[1], (*mac)[2], (*mac)[3], (*mac)[4], (*mac)[5],
	      mac_index);
	claim_zero(ibv_destroy_flow(rxq->mac_flow[mac_index]));
	rxq->mac_flow[mac_index] = NULL;
}

/**
 * Unregister a MAC address from a RX queue.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param mac_index
 *   MAC address index.
 */
static void
rxq_mac_addr_del(struct rxq *rxq, unsigned int mac_index)
{
	assert(mac_index < RTE_DIM(rxq->mac_flow));
	rxq_del_mac_flow(rxq, mac_index);
}

/**
 * Unregister all MAC addresses from a RX queue.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 */
void
rxq_mac_addrs_del(struct rxq *rxq)
{
	unsigned int i;

	for (i = 0; (i != RTE_DIM(rxq->mac_flow)); ++i)
		rxq_mac_addr_del(rxq, i);
}

/**
 * Unregister a MAC address.
 *
 * In RSS mode, the MAC address is unregistered from the parent queue,
 * otherwise it is unregistered from each queue directly.
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
	if (priv->rss) {
		rxq_mac_addr_del(&priv->rxq_parent, mac_index);
		goto end;
	}
	for (i = 0; (i != priv->dev->data->nb_rx_queues); ++i)
		rxq_mac_addr_del((*priv->rxqs)[i], mac_index);
end:
	BITFIELD_RESET(priv->mac_configured, mac_index);
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

	priv_lock(priv);
	DEBUG("%p: removing MAC address from index %" PRIu32,
	      (void *)dev, index);
	/* Last array entry is reserved for broadcast. */
	if (index >= (RTE_DIM(priv->mac) - 1))
		goto end;
	priv_mac_addr_del(priv, index);
end:
	priv_unlock(priv);
}

/**
 * Add MAC flow steering rule.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param mac_index
 *   MAC address index to register.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
rxq_add_mac_flow(struct rxq *rxq, unsigned int mac_index)
{
	struct ibv_flow *flow;
	struct priv *priv = rxq->priv;
	const uint8_t (*mac)[ETHER_ADDR_LEN] =
			(const uint8_t (*)[ETHER_ADDR_LEN])
			priv->mac[mac_index].addr_bytes;
	struct __attribute__((packed)) {
		struct ibv_flow_attr attr;
		struct ibv_flow_spec_eth spec;
	} data;
	struct ibv_flow_attr *attr = &data.attr;
	struct ibv_flow_spec_eth *spec = &data.spec;

	assert(mac_index < RTE_DIM(rxq->mac_flow));
	if (rxq->mac_flow[mac_index] != NULL)
		return 0;
	/*
	 * No padding must be inserted by the compiler between attr and spec.
	 * This layout is expected by libibverbs.
	 */
	assert(((uint8_t *)attr + sizeof(*attr)) == (uint8_t *)spec);
	*attr = (struct ibv_flow_attr){
		.type = IBV_FLOW_ATTR_NORMAL,
		.num_of_specs = 1,
		.port = priv->port,
		.flags = 0
	};
	*spec = (struct ibv_flow_spec_eth){
		.type = IBV_FLOW_SPEC_ETH,
		.size = sizeof(*spec),
		.val = {
			.dst_mac = {
				(*mac)[0], (*mac)[1], (*mac)[2],
				(*mac)[3], (*mac)[4], (*mac)[5]
			},
		},
		.mask = {
			.dst_mac = "\xff\xff\xff\xff\xff\xff",
		},
	};
	DEBUG("%p: adding MAC address %02x:%02x:%02x:%02x:%02x:%02x index %u",
	      (void *)rxq,
	      (*mac)[0], (*mac)[1], (*mac)[2], (*mac)[3], (*mac)[4], (*mac)[5],
	      mac_index);
	/* Create related flow. */
	errno = 0;
	flow = ibv_create_flow(rxq->qp, attr);
	if (flow == NULL) {
		/* It's not clear whether errno is always set in this case. */
		ERROR("%p: flow configuration failed, errno=%d: %s",
		      (void *)rxq, errno,
		      (errno ? strerror(errno) : "Unknown error"));
		if (errno)
			return errno;
		return EINVAL;
	}
	rxq->mac_flow[mac_index] = flow;
	return 0;
}

/**
 * Register a MAC address in a RX queue.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param mac_index
 *   MAC address index to register.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
rxq_mac_addr_add(struct rxq *rxq, unsigned int mac_index)
{
	int ret;

	assert(mac_index < RTE_DIM(rxq->mac_flow));
	ret = rxq_add_mac_flow(rxq, mac_index);
	if (ret)
		return ret;
	return 0;
}

/**
 * Register all MAC addresses in a RX queue.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 *
 * @return
 *   0 on success, errno value on failure.
 */
int
rxq_mac_addrs_add(struct rxq *rxq)
{
	struct priv *priv = rxq->priv;
	unsigned int i;
	int ret;

	assert(RTE_DIM(priv->mac) == RTE_DIM(rxq->mac_flow));
	for (i = 0; (i != RTE_DIM(priv->mac)); ++i) {
		if (!BITFIELD_ISSET(priv->mac_configured, i))
			continue;
		ret = rxq_mac_addr_add(rxq, i);
		if (!ret)
			continue;
		/* Failure, rollback. */
		while (i != 0)
			rxq_mac_addr_del(rxq, --i);
		assert(ret > 0);
		return ret;
	}
	return 0;
}

/**
 * Register a MAC address.
 *
 * In RSS mode, the MAC address is registered in the parent queue,
 * otherwise it is registered in each queue directly.
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
	/* If device isn't started, this is all we need to do. */
	if (!priv->started)
		goto end;
	if (priv->rss) {
		ret = rxq_mac_addr_add(&priv->rxq_parent, mac_index);
		if (ret)
			return ret;
		goto end;
	}
	for (i = 0; (i != priv->rxqs_n); ++i) {
		if ((*priv->rxqs)[i] == NULL)
			continue;
		ret = rxq_mac_addr_add((*priv->rxqs)[i], mac_index);
		if (!ret)
			continue;
		/* Failure, rollback. */
		while (i != 0)
			if ((*priv->rxqs)[--i] != NULL)
				rxq_mac_addr_del((*priv->rxqs)[i], mac_index);
		return ret;
	}
end:
	BITFIELD_SET(priv->mac_configured, mac_index);
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
void
mlx5_mac_addr_add(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		  uint32_t index, uint32_t vmdq)
{
	struct priv *priv = dev->data->dev_private;

	(void)vmdq;
	priv_lock(priv);
	DEBUG("%p: adding MAC address at index %" PRIu32,
	      (void *)dev, index);
	/* Last array entry is reserved for broadcast. */
	if (index >= (RTE_DIM(priv->mac) - 1))
		goto end;
	priv_mac_addr_add(priv, index,
			  (const uint8_t (*)[ETHER_ADDR_LEN])
			  mac_addr->addr_bytes);
end:
	priv_unlock(priv);
}
