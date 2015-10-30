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

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_ethdev.h>
#include <rte_common.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5_utils.h"
#include "mlx5.h"

/**
 * Configure a VLAN filter.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param vlan_id
 *   VLAN ID to filter.
 * @param on
 *   Toggle filter.
 *
 * @return
 *   0 on success, errno value on failure.
 */
static int
vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;

	DEBUG("%p: %s VLAN filter ID %" PRIu16,
	      (void *)dev, (on ? "enable" : "disable"), vlan_id);
	assert(priv->vlan_filter_n <= RTE_DIM(priv->vlan_filter));
	for (i = 0; (i != priv->vlan_filter_n); ++i)
		if (priv->vlan_filter[i] == vlan_id)
			break;
	/* Check if there's room for another VLAN filter. */
	if (i == RTE_DIM(priv->vlan_filter))
		return ENOMEM;
	if (i < priv->vlan_filter_n) {
		assert(priv->vlan_filter_n != 0);
		/* Enabling an existing VLAN filter has no effect. */
		if (on)
			return 0;
		/* Remove VLAN filter from list. */
		--priv->vlan_filter_n;
		memmove(&priv->vlan_filter[i],
			&priv->vlan_filter[i + 1],
			priv->vlan_filter_n - i);
		priv->vlan_filter[priv->vlan_filter_n] = 0;
	} else {
		assert(i == priv->vlan_filter_n);
		/* Disabling an unknown VLAN filter has no effect. */
		if (!on)
			return 0;
		/* Add new VLAN filter. */
		priv->vlan_filter[priv->vlan_filter_n] = vlan_id;
		++priv->vlan_filter_n;
	}
	/* Rehash MAC flows in all hash RX queues. */
	priv_mac_addrs_disable(priv);
	return priv_mac_addrs_enable(priv);
}

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
	int ret;

	priv_lock(priv);
	ret = vlan_filter_set(dev, vlan_id, on);
	priv_unlock(priv);
	assert(ret >= 0);
	return -ret;
}
