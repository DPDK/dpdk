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
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
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
	int err;

	if (mlx5_is_secondary())
		return -E_RTE_SECONDARY;

	priv_lock(priv);
	if (priv->started) {
		priv_unlock(priv);
		return 0;
	}
	DEBUG("%p: allocating and configuring hash RX queues", (void *)dev);
	err = priv_create_hash_rxqs(priv);
	if (!err)
		err = priv_rehash_flows(priv);
	if (!err)
		priv->started = 1;
	else {
		ERROR("%p: an error occurred while configuring hash RX queues:"
		      " %s",
		      (void *)priv, strerror(err));
		/* Rollback. */
		priv_special_flow_disable_all(priv);
		priv_mac_addrs_disable(priv);
		priv_destroy_hash_rxqs(priv);
	}
	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_NONE)
		priv_fdir_enable(priv);
	priv_dev_interrupt_handler_install(priv, dev);
	priv_unlock(priv);
	return -err;
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

	if (mlx5_is_secondary())
		return;

	priv_lock(priv);
	if (!priv->started) {
		priv_unlock(priv);
		return;
	}
	DEBUG("%p: cleaning up and destroying hash RX queues", (void *)dev);
	priv_special_flow_disable_all(priv);
	priv_mac_addrs_disable(priv);
	priv_destroy_hash_rxqs(priv);
	priv_fdir_disable(priv);
	priv_dev_interrupt_handler_uninstall(priv, dev);
	priv->started = 0;
	priv_unlock(priv);
}
