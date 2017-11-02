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
#include <assert.h>
#include <stdint.h>

#include <rte_ethdev.h>
#include <rte_common.h>

#include "mlx5_utils.h"
#include "mlx5.h"
#include "mlx5_autoconf.h"

/**
 * DPDK callback to configure a VLAN filter.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param vlan_id
 *   VLAN ID to filter.
 * @param on
 *   Toggle filter.
 *
 * @return
 *   0 on success, negative errno value on failure.
 */
int
mlx5_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret = 0;

	priv_lock(priv);
	DEBUG("%p: %s VLAN filter ID %" PRIu16,
	      (void *)dev, (on ? "enable" : "disable"), vlan_id);
	assert(priv->vlan_filter_n <= RTE_DIM(priv->vlan_filter));
	for (i = 0; (i != priv->vlan_filter_n); ++i)
		if (priv->vlan_filter[i] == vlan_id)
			break;
	/* Check if there's room for another VLAN filter. */
	if (i == RTE_DIM(priv->vlan_filter)) {
		ret = -ENOMEM;
		goto out;
	}
	if (i < priv->vlan_filter_n) {
		assert(priv->vlan_filter_n != 0);
		/* Enabling an existing VLAN filter has no effect. */
		if (on)
			goto out;
		/* Remove VLAN filter from list. */
		--priv->vlan_filter_n;
		memmove(&priv->vlan_filter[i],
			&priv->vlan_filter[i + 1],
			sizeof(priv->vlan_filter[i]) *
			(priv->vlan_filter_n - i));
		priv->vlan_filter[priv->vlan_filter_n] = 0;
	} else {
		assert(i == priv->vlan_filter_n);
		/* Disabling an unknown VLAN filter has no effect. */
		if (!on)
			goto out;
		/* Add new VLAN filter. */
		priv->vlan_filter[priv->vlan_filter_n] = vlan_id;
		++priv->vlan_filter_n;
	}
	if (dev->data->dev_started)
		priv_dev_traffic_restart(priv, dev);
out:
	priv_unlock(priv);
	return ret;
}

/**
 * Set/reset VLAN stripping for a specific queue.
 *
 * @param priv
 *   Pointer to private structure.
 * @param idx
 *   RX queue index.
 * @param on
 *   Enable/disable VLAN stripping.
 */
static void
priv_vlan_strip_queue_set(struct priv *priv, uint16_t idx, int on)
{
	struct mlx5_rxq_data *rxq = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq, struct mlx5_rxq_ctrl, rxq);
	struct ibv_wq_attr mod;
	uint16_t vlan_offloads =
		(on ? IBV_WQ_FLAGS_CVLAN_STRIPPING : 0) |
		0;
	int err;

	DEBUG("set VLAN offloads 0x%x for port %d queue %d",
	      vlan_offloads, rxq->port_id, idx);
	mod = (struct ibv_wq_attr){
		.attr_mask = IBV_WQ_ATTR_FLAGS,
		.flags_mask = IBV_WQ_FLAGS_CVLAN_STRIPPING,
		.flags = vlan_offloads,
	};

	err = ibv_modify_wq(rxq_ctrl->ibv->wq, &mod);
	if (err) {
		ERROR("%p: failed to modified stripping mode: %s",
		      (void *)priv, strerror(err));
		return;
	}

	/* Update related bits in RX queue. */
	rxq->vlan_strip = !!on;
}

/**
 * Callback to set/reset VLAN stripping for a specific queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param queue
 *   RX queue index.
 * @param on
 *   Enable/disable VLAN stripping.
 */
void
mlx5_vlan_strip_queue_set(struct rte_eth_dev *dev, uint16_t queue, int on)
{
	struct priv *priv = dev->data->dev_private;

	/* Validate hw support */
	if (!priv->hw_vlan_strip) {
		ERROR("VLAN stripping is not supported");
		return;
	}

	/* Validate queue number */
	if (queue >= priv->rxqs_n) {
		ERROR("VLAN stripping, invalid queue number %d", queue);
		return;
	}

	priv_lock(priv);
	priv_vlan_strip_queue_set(priv, queue, on);
	priv_unlock(priv);
}

/**
 * Callback to set/reset VLAN offloads for a port.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mask
 *   VLAN offload bit mask.
 */
int
mlx5_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;

	if (mask & ETH_VLAN_STRIP_MASK) {
		int hw_vlan_strip = !!dev->data->dev_conf.rxmode.hw_vlan_strip;

		if (!priv->hw_vlan_strip) {
			ERROR("VLAN stripping is not supported");
			return 0;
		}

		/* Run on every RX queue and set/reset VLAN stripping. */
		priv_lock(priv);
		for (i = 0; (i != priv->rxqs_n); i++)
			priv_vlan_strip_queue_set(priv, i, hw_vlan_strip);
		priv_unlock(priv);
	}

	return 0;
}
