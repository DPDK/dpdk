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

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_ether.h>
#include <rte_ethdev.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

/**
 * DPDK callback to start the device.
 *
 * Simulate device start by attaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
mlx5_dev_start(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i = 0;
	unsigned int r;
	struct rxq *rxq;

	priv_lock(priv);
	if (priv->started) {
		priv_unlock(priv);
		return 0;
	}
	DEBUG("%p: attaching configured flows to all RX queues", (void *)dev);
	priv->started = 1;
	if (priv->rss) {
		rxq = &priv->rxq_parent;
		r = 1;
	} else {
		rxq = (*priv->rxqs)[0];
		r = priv->rxqs_n;
	}
	/* Iterate only once when RSS is enabled. */
	do {
		int ret;

		/* Ignore nonexistent RX queues. */
		if (rxq == NULL)
			continue;
		ret = rxq_mac_addrs_add(rxq);
		if (!ret && priv->promisc_req)
			ret = rxq_promiscuous_enable(rxq);
		if (!ret && priv->allmulti_req)
			ret = rxq_allmulticast_enable(rxq);
		if (!ret)
			continue;
		WARN("%p: QP flow attachment failed: %s",
		     (void *)dev, strerror(ret));
		/* Rollback. */
		while (i != 0) {
			rxq = (*priv->rxqs)[--i];
			if (rxq != NULL) {
				rxq_allmulticast_disable(rxq);
				rxq_promiscuous_disable(rxq);
				rxq_mac_addrs_del(rxq);
			}
		}
		priv->started = 0;
		priv_unlock(priv);
		return -ret;
	} while ((--r) && ((rxq = (*priv->rxqs)[++i]), i));
	priv_unlock(priv);
	return 0;
}

/**
 * DPDK callback to stop the device.
 *
 * Simulate device stop by detaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
void
mlx5_dev_stop(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i = 0;
	unsigned int r;
	struct rxq *rxq;

	priv_lock(priv);
	if (!priv->started) {
		priv_unlock(priv);
		return;
	}
	DEBUG("%p: detaching flows from all RX queues", (void *)dev);
	priv->started = 0;
	if (priv->rss) {
		rxq = &priv->rxq_parent;
		r = 1;
	} else {
		rxq = (*priv->rxqs)[0];
		r = priv->rxqs_n;
	}
	/* Iterate only once when RSS is enabled. */
	do {
		/* Ignore nonexistent RX queues. */
		if (rxq == NULL)
			continue;
		rxq_allmulticast_disable(rxq);
		rxq_promiscuous_disable(rxq);
		rxq_mac_addrs_del(rxq);
	} while ((--r) && ((rxq = (*priv->rxqs)[++i]), i));
	priv_unlock(priv);
}
