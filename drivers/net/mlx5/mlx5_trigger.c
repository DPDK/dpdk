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
#include <unistd.h>

#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

static void
priv_txq_stop(struct priv *priv)
{
	unsigned int i;

	for (i = 0; i != priv->txqs_n; ++i)
		mlx5_priv_txq_release(priv, i);
}

static int
priv_txq_start(struct priv *priv)
{
	unsigned int i;
	int ret = 0;

	/* Add memory regions to Tx queues. */
	for (i = 0; i != priv->txqs_n; ++i) {
		unsigned int idx = 0;
		struct mlx5_mr *mr;
		struct mlx5_txq_ctrl *txq_ctrl = mlx5_priv_txq_get(priv, i);

		if (!txq_ctrl)
			continue;
		LIST_FOREACH(mr, &priv->mr, next)
			priv_txq_mp2mr_reg(priv, &txq_ctrl->txq, mr->mp, idx++);
		txq_alloc_elts(txq_ctrl);
		txq_ctrl->ibv = mlx5_priv_txq_ibv_new(priv, i);
		if (!txq_ctrl->ibv) {
			ret = ENOMEM;
			goto error;
		}
	}
	return -ret;
error:
	priv_txq_stop(priv);
	return -ret;
}

static void
priv_rxq_stop(struct priv *priv)
{
	unsigned int i;

	for (i = 0; i != priv->rxqs_n; ++i)
		mlx5_priv_rxq_release(priv, i);
}

static int
priv_rxq_start(struct priv *priv)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl = mlx5_priv_rxq_get(priv, i);

		if (!rxq_ctrl)
			continue;
		ret = rxq_alloc_elts(rxq_ctrl);
		if (ret)
			goto error;
		rxq_ctrl->ibv = mlx5_priv_rxq_ibv_new(priv, i);
		if (!rxq_ctrl->ibv) {
			ret = ENOMEM;
			goto error;
		}
	}
	return -ret;
error:
	priv_rxq_stop(priv);
	return -ret;
}

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
	struct mlx5_mr *mr = NULL;
	int err;

	if (mlx5_is_secondary())
		return -E_RTE_SECONDARY;

	priv_lock(priv);
	DEBUG("%p: allocating and configuring hash RX queues", (void *)dev);
	rte_mempool_walk(mlx5_mp2mr_iter, priv);
	err = priv_txq_start(priv);
	if (err) {
		ERROR("%p: TXQ allocation failed: %s",
		      (void *)dev, strerror(err));
		goto error;
	}
	/* Update send callback. */
	priv_dev_select_tx_function(priv, dev);
	err = priv_rxq_start(priv);
	if (err) {
		ERROR("%p: RXQ allocation failed: %s",
		      (void *)dev, strerror(err));
		goto error;
	}
	/* Update receive callback. */
	priv_dev_select_rx_function(priv, dev);
	err = priv_create_hash_rxqs(priv);
	if (!err)
		err = priv_rehash_flows(priv);
	else {
		ERROR("%p: an error occurred while configuring hash RX queues:"
		      " %s",
		      (void *)priv, strerror(err));
		goto error;
	}
	if (dev->data->promiscuous)
		mlx5_promiscuous_enable(dev);
	err = priv_flow_start(priv, &priv->ctrl_flows);
	if (err) {
		ERROR("%p: an error occurred while configuring control flows:"
		      " %s",
		      (void *)priv, strerror(err));
		goto error;
	}
	err = priv_flow_start(priv, &priv->flows);
	if (err) {
		ERROR("%p: an error occurred while configuring flows:"
		      " %s",
		      (void *)priv, strerror(err));
		goto error;
	}
	err = priv_rx_intr_vec_enable(priv);
	if (err) {
		ERROR("%p: RX interrupt vector creation failed",
		      (void *)priv);
		goto error;
	}
	priv_dev_interrupt_handler_install(priv, dev);
	priv_xstats_init(priv);
	priv_unlock(priv);
	return 0;
error:
	/* Rollback. */
	LIST_FOREACH(mr, &priv->mr, next)
		priv_mr_release(priv, mr);
	priv_special_flow_disable_all(priv);
	priv_mac_addrs_disable(priv);
	priv_destroy_hash_rxqs(priv);
	priv_flow_stop(priv, &priv->flows);
	priv_flow_flush(priv, &priv->ctrl_flows);
	priv_rxq_stop(priv);
	priv_txq_stop(priv);
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
	struct mlx5_mr *mr;

	if (mlx5_is_secondary())
		return;

	priv_lock(priv);
	dev->data->dev_started = 0;
	/* Prevent crashes when queues are still in use. */
	dev->rx_pkt_burst = removed_rx_burst;
	dev->tx_pkt_burst = removed_tx_burst;
	rte_wmb();
	usleep(1000 * priv->rxqs_n);
	DEBUG("%p: cleaning up and destroying hash RX queues", (void *)dev);
	priv_special_flow_disable_all(priv);
	priv_mac_addrs_disable(priv);
	priv_destroy_hash_rxqs(priv);
	priv_flow_stop(priv, &priv->flows);
	priv_flow_flush(priv, &priv->ctrl_flows);
	priv_rx_intr_vec_disable(priv);
	priv_dev_interrupt_handler_uninstall(priv, dev);
	priv_txq_stop(priv);
	priv_rxq_stop(priv);
	LIST_FOREACH(mr, &priv->mr, next) {
		priv_mr_release(priv, mr);
	}
	priv_unlock(priv);
}
