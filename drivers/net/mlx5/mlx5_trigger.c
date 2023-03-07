/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <unistd.h>

#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>
#include <rte_cycles.h>

#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_flow.h"
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "mlx5_utils.h"
#include "rte_pmd_mlx5.h"

/**
 * Stop traffic on Tx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx5_txq_stop(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	for (i = 0; i != priv->txqs_n; ++i)
		mlx5_txq_release(dev, i);
}

/**
 * Start traffic on Tx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_txq_start(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret;

	for (i = 0; i != priv->txqs_n; ++i) {
		struct mlx5_txq_ctrl *txq_ctrl = mlx5_txq_get(dev, i);
		struct mlx5_txq_data *txq_data = &txq_ctrl->txq;
		uint32_t flags = MLX5_MEM_RTE | MLX5_MEM_ZERO;

		if (!txq_ctrl)
			continue;
		if (!txq_ctrl->is_hairpin)
			txq_alloc_elts(txq_ctrl);
		MLX5_ASSERT(!txq_ctrl->obj);
		txq_ctrl->obj = mlx5_malloc(flags, sizeof(struct mlx5_txq_obj),
					    0, txq_ctrl->socket);
		if (!txq_ctrl->obj) {
			DRV_LOG(ERR, "Port %u Tx queue %u cannot allocate "
				"memory resources.", dev->data->port_id,
				txq_data->idx);
			rte_errno = ENOMEM;
			goto error;
		}
		ret = priv->obj_ops.txq_obj_new(dev, i);
		if (ret < 0) {
			mlx5_free(txq_ctrl->obj);
			txq_ctrl->obj = NULL;
			goto error;
		}
		if (!txq_ctrl->is_hairpin) {
			size_t size = txq_data->cqe_s * sizeof(*txq_data->fcqs);

			txq_data->fcqs = mlx5_malloc(flags, size,
						     RTE_CACHE_LINE_SIZE,
						     txq_ctrl->socket);
			if (!txq_data->fcqs) {
				DRV_LOG(ERR, "Port %u Tx queue %u cannot "
					"allocate memory (FCQ).",
					dev->data->port_id, i);
				rte_errno = ENOMEM;
				goto error;
			}
		}
		DRV_LOG(DEBUG, "Port %u txq %u updated with %p.",
			dev->data->port_id, i, (void *)&txq_ctrl->obj);
		LIST_INSERT_HEAD(&priv->txqsobj, txq_ctrl->obj, next);
	}
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	do {
		mlx5_txq_release(dev, i);
	} while (i-- != 0);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Register Rx queue mempools and fill the Rx queue cache.
 * This function tolerates repeated mempool registration.
 *
 * @param[in] rxq_ctrl
 *   Rx queue control data.
 *
 * @return
 *   0 on success, (-1) on failure and rte_errno is set.
 */
static int
mlx5_rxq_mempool_register(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	struct rte_mempool *mp;
	uint32_t s;
	int ret = 0;

	mlx5_mr_flush_local_cache(&rxq_ctrl->rxq.mr_ctrl);
	/* MPRQ mempool is registered on creation, just fill the cache. */
	if (mlx5_rxq_mprq_enabled(&rxq_ctrl->rxq))
		return mlx5_mr_mempool_populate_cache(&rxq_ctrl->rxq.mr_ctrl,
						      rxq_ctrl->rxq.mprq_mp);
	for (s = 0; s < rxq_ctrl->rxq.rxseg_n; s++) {
		bool is_extmem;

		mp = rxq_ctrl->rxq.rxseg[s].mp;
		is_extmem = (rte_pktmbuf_priv_flags(mp) &
			     RTE_PKTMBUF_POOL_F_PINNED_EXT_BUF) != 0;
		ret = mlx5_mr_mempool_register(rxq_ctrl->sh->cdev, mp,
					       is_extmem);
		if (ret < 0 && rte_errno != EEXIST)
			return ret;
		ret = mlx5_mr_mempool_populate_cache(&rxq_ctrl->rxq.mr_ctrl,
						     mp);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/**
 * Stop traffic on Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx5_rxq_stop(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	for (i = 0; i != priv->rxqs_n; ++i)
		mlx5_rxq_release(dev, i);
}

static int
mlx5_rxq_ctrl_prepare(struct rte_eth_dev *dev, struct mlx5_rxq_ctrl *rxq_ctrl,
		      unsigned int idx)
{
	int ret = 0;

	if (!rxq_ctrl->is_hairpin) {
		/*
		 * Pre-register the mempools. Regardless of whether
		 * the implicit registration is enabled or not,
		 * Rx mempool destruction is tracked to free MRs.
		 */
		if (mlx5_rxq_mempool_register(rxq_ctrl) < 0)
			return -rte_errno;
		ret = rxq_alloc_elts(rxq_ctrl);
		if (ret)
			return ret;
	}
	MLX5_ASSERT(!rxq_ctrl->obj);
	rxq_ctrl->obj = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO,
				    sizeof(*rxq_ctrl->obj), 0,
				    rxq_ctrl->socket);
	if (!rxq_ctrl->obj) {
		DRV_LOG(ERR, "Port %u Rx queue %u can't allocate resources.",
			dev->data->port_id, idx);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	DRV_LOG(DEBUG, "Port %u rxq %u updated with %p.", dev->data->port_id,
		idx, (void *)&rxq_ctrl->obj);
	return 0;
}

/**
 * Start traffic on Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rxq_start(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret = 0;

	/* Allocate/reuse/resize mempool for Multi-Packet RQ. */
	if (mlx5_mprq_alloc_mp(dev)) {
		/* Should not release Rx queues but return immediately. */
		return -rte_errno;
	}
	DRV_LOG(DEBUG, "Port %u dev_cap.max_qp_wr is %d.",
		dev->data->port_id, priv->sh->dev_cap.max_qp_wr);
	DRV_LOG(DEBUG, "Port %u dev_cap.max_sge is %d.",
		dev->data->port_id, priv->sh->dev_cap.max_sge);
	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_priv *rxq = mlx5_rxq_ref(dev, i);
		struct mlx5_rxq_ctrl *rxq_ctrl;

		if (rxq == NULL)
			continue;
		rxq_ctrl = rxq->ctrl;
		if (!rxq_ctrl->started) {
			if (mlx5_rxq_ctrl_prepare(dev, rxq_ctrl, i) < 0)
				goto error;
			LIST_INSERT_HEAD(&priv->rxqsobj, rxq_ctrl->obj, next);
		}
		ret = priv->obj_ops.rxq_obj_new(rxq);
		if (ret) {
			mlx5_free(rxq_ctrl->obj);
			rxq_ctrl->obj = NULL;
			goto error;
		}
		rxq_ctrl->started = true;
	}
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	do {
		mlx5_rxq_release(dev, i);
	} while (i-- != 0);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Binds Tx queues to Rx queues for hairpin.
 *
 * Binds Tx queues to the target Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_hairpin_auto_bind(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_modify_sq_attr sq_attr = { 0 };
	struct mlx5_devx_modify_rq_attr rq_attr = { 0 };
	struct mlx5_txq_ctrl *txq_ctrl;
	struct mlx5_rxq_priv *rxq;
	struct mlx5_rxq_ctrl *rxq_ctrl;
	struct mlx5_devx_obj *sq;
	struct mlx5_devx_obj *rq;
	unsigned int i;
	int ret = 0;
	bool need_auto = false;
	uint16_t self_port = dev->data->port_id;

	for (i = 0; i != priv->txqs_n; ++i) {
		txq_ctrl = mlx5_txq_get(dev, i);
		if (!txq_ctrl)
			continue;
		if (!txq_ctrl->is_hairpin ||
		    txq_ctrl->hairpin_conf.peers[0].port != self_port) {
			mlx5_txq_release(dev, i);
			continue;
		}
		if (txq_ctrl->hairpin_conf.manual_bind) {
			mlx5_txq_release(dev, i);
			return 0;
		}
		need_auto = true;
		mlx5_txq_release(dev, i);
	}
	if (!need_auto)
		return 0;
	for (i = 0; i != priv->txqs_n; ++i) {
		txq_ctrl = mlx5_txq_get(dev, i);
		if (!txq_ctrl)
			continue;
		/* Skip hairpin queues with other peer ports. */
		if (!txq_ctrl->is_hairpin ||
		    txq_ctrl->hairpin_conf.peers[0].port != self_port) {
			mlx5_txq_release(dev, i);
			continue;
		}
		if (!txq_ctrl->obj) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no txq object found: %d",
				dev->data->port_id, i);
			mlx5_txq_release(dev, i);
			return -rte_errno;
		}
		sq = txq_ctrl->obj->sq;
		rxq = mlx5_rxq_get(dev, txq_ctrl->hairpin_conf.peers[0].queue);
		if (rxq == NULL) {
			mlx5_txq_release(dev, i);
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u no rxq object found: %d",
				dev->data->port_id,
				txq_ctrl->hairpin_conf.peers[0].queue);
			return -rte_errno;
		}
		rxq_ctrl = rxq->ctrl;
		if (!rxq_ctrl->is_hairpin ||
		    rxq->hairpin_conf.peers[0].queue != i) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u Tx queue %d can't be binded to "
				"Rx queue %d", dev->data->port_id,
				i, txq_ctrl->hairpin_conf.peers[0].queue);
			goto error;
		}
		rq = rxq_ctrl->obj->rq;
		if (!rq) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u hairpin no matching rxq: %d",
				dev->data->port_id,
				txq_ctrl->hairpin_conf.peers[0].queue);
			goto error;
		}
		sq_attr.state = MLX5_SQC_STATE_RDY;
		sq_attr.sq_state = MLX5_SQC_STATE_RST;
		sq_attr.hairpin_peer_rq = rq->id;
		sq_attr.hairpin_peer_vhca =
				priv->sh->cdev->config.hca_attr.vhca_id;
		ret = mlx5_devx_cmd_modify_sq(sq, &sq_attr);
		if (ret)
			goto error;
		rq_attr.state = MLX5_SQC_STATE_RDY;
		rq_attr.rq_state = MLX5_SQC_STATE_RST;
		rq_attr.hairpin_peer_sq = sq->id;
		rq_attr.hairpin_peer_vhca =
				priv->sh->cdev->config.hca_attr.vhca_id;
		ret = mlx5_devx_cmd_modify_rq(rq, &rq_attr);
		if (ret)
			goto error;
		/* Qs with auto-bind will be destroyed directly. */
		rxq->hairpin_status = 1;
		txq_ctrl->hairpin_status = 1;
		mlx5_txq_release(dev, i);
	}
	return 0;
error:
	mlx5_txq_release(dev, i);
	return -rte_errno;
}

/*
 * Fetch the peer queue's SW & HW information.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param peer_queue
 *   Index of the queue to fetch the information.
 * @param current_info
 *   Pointer to the input peer information, not used currently.
 * @param peer_info
 *   Pointer to the structure to store the information, output.
 * @param direction
 *   Positive to get the RxQ information, zero to get the TxQ information.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_hairpin_queue_peer_update(struct rte_eth_dev *dev, uint16_t peer_queue,
			       struct rte_hairpin_peer_info *current_info,
			       struct rte_hairpin_peer_info *peer_info,
			       uint32_t direction)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	RTE_SET_USED(current_info);

	if (dev->data->dev_started == 0) {
		rte_errno = EBUSY;
		DRV_LOG(ERR, "peer port %u is not started",
			dev->data->port_id);
		return -rte_errno;
	}
	/*
	 * Peer port used as egress. In the current design, hairpin Tx queue
	 * will be bound to the peer Rx queue. Indeed, only the information of
	 * peer Rx queue needs to be fetched.
	 */
	if (direction == 0) {
		struct mlx5_txq_ctrl *txq_ctrl;

		txq_ctrl = mlx5_txq_get(dev, peer_queue);
		if (txq_ctrl == NULL) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "Failed to get port %u Tx queue %d",
				dev->data->port_id, peer_queue);
			return -rte_errno;
		}
		if (!txq_ctrl->is_hairpin) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u queue %d is not a hairpin Txq",
				dev->data->port_id, peer_queue);
			mlx5_txq_release(dev, peer_queue);
			return -rte_errno;
		}
		if (txq_ctrl->obj == NULL || txq_ctrl->obj->sq == NULL) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no Txq object found: %d",
				dev->data->port_id, peer_queue);
			mlx5_txq_release(dev, peer_queue);
			return -rte_errno;
		}
		peer_info->qp_id = mlx5_txq_get_sqn(txq_ctrl);
		peer_info->vhca_id = priv->sh->cdev->config.hca_attr.vhca_id;
		/* 1-to-1 mapping, only the first one is used. */
		peer_info->peer_q = txq_ctrl->hairpin_conf.peers[0].queue;
		peer_info->tx_explicit = txq_ctrl->hairpin_conf.tx_explicit;
		peer_info->manual_bind = txq_ctrl->hairpin_conf.manual_bind;
		mlx5_txq_release(dev, peer_queue);
	} else { /* Peer port used as ingress. */
		struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev, peer_queue);
		struct mlx5_rxq_ctrl *rxq_ctrl;

		if (rxq == NULL) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "Failed to get port %u Rx queue %d",
				dev->data->port_id, peer_queue);
			return -rte_errno;
		}
		rxq_ctrl = rxq->ctrl;
		if (!rxq_ctrl->is_hairpin) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u queue %d is not a hairpin Rxq",
				dev->data->port_id, peer_queue);
			return -rte_errno;
		}
		if (rxq_ctrl->obj == NULL || rxq_ctrl->obj->rq == NULL) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no Rxq object found: %d",
				dev->data->port_id, peer_queue);
			return -rte_errno;
		}
		peer_info->qp_id = rxq_ctrl->obj->rq->id;
		peer_info->vhca_id = priv->sh->cdev->config.hca_attr.vhca_id;
		peer_info->peer_q = rxq->hairpin_conf.peers[0].queue;
		peer_info->tx_explicit = rxq->hairpin_conf.tx_explicit;
		peer_info->manual_bind = rxq->hairpin_conf.manual_bind;
	}
	return 0;
}

/*
 * Bind the hairpin queue with the peer HW information.
 * This needs to be called twice both for Tx and Rx queues of a pair.
 * If the queue is already bound, it is considered successful.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param cur_queue
 *   Index of the queue to change the HW configuration to bind.
 * @param peer_info
 *   Pointer to information of the peer queue.
 * @param direction
 *   Positive to configure the TxQ, zero to configure the RxQ.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_hairpin_queue_peer_bind(struct rte_eth_dev *dev, uint16_t cur_queue,
			     struct rte_hairpin_peer_info *peer_info,
			     uint32_t direction)
{
	int ret = 0;

	/*
	 * Consistency checking of the peer queue: opposite direction is used
	 * to get the peer queue info with ethdev port ID, no need to check.
	 */
	if (peer_info->peer_q != cur_queue) {
		rte_errno = EINVAL;
		DRV_LOG(ERR, "port %u queue %d and peer queue %d mismatch",
			dev->data->port_id, cur_queue, peer_info->peer_q);
		return -rte_errno;
	}
	if (direction != 0) {
		struct mlx5_txq_ctrl *txq_ctrl;
		struct mlx5_devx_modify_sq_attr sq_attr = { 0 };

		txq_ctrl = mlx5_txq_get(dev, cur_queue);
		if (txq_ctrl == NULL) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "Failed to get port %u Tx queue %d",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		if (!txq_ctrl->is_hairpin) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u queue %d not a hairpin Txq",
				dev->data->port_id, cur_queue);
			mlx5_txq_release(dev, cur_queue);
			return -rte_errno;
		}
		if (txq_ctrl->obj == NULL || txq_ctrl->obj->sq == NULL) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no Txq object found: %d",
				dev->data->port_id, cur_queue);
			mlx5_txq_release(dev, cur_queue);
			return -rte_errno;
		}
		if (txq_ctrl->hairpin_status != 0) {
			DRV_LOG(DEBUG, "port %u Tx queue %d is already bound",
				dev->data->port_id, cur_queue);
			mlx5_txq_release(dev, cur_queue);
			return 0;
		}
		/*
		 * All queues' of one port consistency checking is done in the
		 * bind() function, and that is optional.
		 */
		if (peer_info->tx_explicit !=
		    txq_ctrl->hairpin_conf.tx_explicit) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u Tx queue %d and peer Tx rule mode"
				" mismatch", dev->data->port_id, cur_queue);
			mlx5_txq_release(dev, cur_queue);
			return -rte_errno;
		}
		if (peer_info->manual_bind !=
		    txq_ctrl->hairpin_conf.manual_bind) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u Tx queue %d and peer binding mode"
				" mismatch", dev->data->port_id, cur_queue);
			mlx5_txq_release(dev, cur_queue);
			return -rte_errno;
		}
		sq_attr.state = MLX5_SQC_STATE_RDY;
		sq_attr.sq_state = MLX5_SQC_STATE_RST;
		sq_attr.hairpin_peer_rq = peer_info->qp_id;
		sq_attr.hairpin_peer_vhca = peer_info->vhca_id;
		ret = mlx5_devx_cmd_modify_sq(txq_ctrl->obj->sq, &sq_attr);
		if (ret == 0)
			txq_ctrl->hairpin_status = 1;
		mlx5_txq_release(dev, cur_queue);
	} else {
		struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev, cur_queue);
		struct mlx5_rxq_ctrl *rxq_ctrl;
		struct mlx5_devx_modify_rq_attr rq_attr = { 0 };

		if (rxq == NULL) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "Failed to get port %u Rx queue %d",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		rxq_ctrl = rxq->ctrl;
		if (!rxq_ctrl->is_hairpin) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u queue %d not a hairpin Rxq",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		if (rxq_ctrl->obj == NULL || rxq_ctrl->obj->rq == NULL) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no Rxq object found: %d",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		if (rxq->hairpin_status != 0) {
			DRV_LOG(DEBUG, "port %u Rx queue %d is already bound",
				dev->data->port_id, cur_queue);
			return 0;
		}
		if (peer_info->tx_explicit !=
		    rxq->hairpin_conf.tx_explicit) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u Rx queue %d and peer Tx rule mode"
				" mismatch", dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		if (peer_info->manual_bind !=
		    rxq->hairpin_conf.manual_bind) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u Rx queue %d and peer binding mode"
				" mismatch", dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		rq_attr.state = MLX5_SQC_STATE_RDY;
		rq_attr.rq_state = MLX5_SQC_STATE_RST;
		rq_attr.hairpin_peer_sq = peer_info->qp_id;
		rq_attr.hairpin_peer_vhca = peer_info->vhca_id;
		ret = mlx5_devx_cmd_modify_rq(rxq_ctrl->obj->rq, &rq_attr);
		if (ret == 0)
			rxq->hairpin_status = 1;
	}
	return ret;
}

/*
 * Unbind the hairpin queue and reset its HW configuration.
 * This needs to be called twice both for Tx and Rx queues of a pair.
 * If the queue is already unbound, it is considered successful.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param cur_queue
 *   Index of the queue to change the HW configuration to unbind.
 * @param direction
 *   Positive to reset the TxQ, zero to reset the RxQ.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_hairpin_queue_peer_unbind(struct rte_eth_dev *dev, uint16_t cur_queue,
			       uint32_t direction)
{
	int ret = 0;

	if (direction != 0) {
		struct mlx5_txq_ctrl *txq_ctrl;
		struct mlx5_devx_modify_sq_attr sq_attr = { 0 };

		txq_ctrl = mlx5_txq_get(dev, cur_queue);
		if (txq_ctrl == NULL) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "Failed to get port %u Tx queue %d",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		if (!txq_ctrl->is_hairpin) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u queue %d not a hairpin Txq",
				dev->data->port_id, cur_queue);
			mlx5_txq_release(dev, cur_queue);
			return -rte_errno;
		}
		/* Already unbound, return success before obj checking. */
		if (txq_ctrl->hairpin_status == 0) {
			DRV_LOG(DEBUG, "port %u Tx queue %d is already unbound",
				dev->data->port_id, cur_queue);
			mlx5_txq_release(dev, cur_queue);
			return 0;
		}
		if (!txq_ctrl->obj || !txq_ctrl->obj->sq) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no Txq object found: %d",
				dev->data->port_id, cur_queue);
			mlx5_txq_release(dev, cur_queue);
			return -rte_errno;
		}
		sq_attr.state = MLX5_SQC_STATE_RST;
		sq_attr.sq_state = MLX5_SQC_STATE_RST;
		ret = mlx5_devx_cmd_modify_sq(txq_ctrl->obj->sq, &sq_attr);
		if (ret == 0)
			txq_ctrl->hairpin_status = 0;
		mlx5_txq_release(dev, cur_queue);
	} else {
		struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev, cur_queue);
		struct mlx5_rxq_ctrl *rxq_ctrl;
		struct mlx5_devx_modify_rq_attr rq_attr = { 0 };

		if (rxq == NULL) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "Failed to get port %u Rx queue %d",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		rxq_ctrl = rxq->ctrl;
		if (!rxq_ctrl->is_hairpin) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u queue %d not a hairpin Rxq",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		if (rxq->hairpin_status == 0) {
			DRV_LOG(DEBUG, "port %u Rx queue %d is already unbound",
				dev->data->port_id, cur_queue);
			return 0;
		}
		if (rxq_ctrl->obj == NULL || rxq_ctrl->obj->rq == NULL) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no Rxq object found: %d",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		rq_attr.state = MLX5_SQC_STATE_RST;
		rq_attr.rq_state = MLX5_SQC_STATE_RST;
		ret = mlx5_devx_cmd_modify_rq(rxq_ctrl->obj->rq, &rq_attr);
		if (ret == 0)
			rxq->hairpin_status = 0;
	}
	return ret;
}

/*
 * Bind the hairpin port pairs, from the Tx to the peer Rx.
 * This function only supports to bind the Tx to one Rx.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rx_port
 *   Port identifier of the Rx port.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_hairpin_bind_single_port(struct rte_eth_dev *dev, uint16_t rx_port)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;
	struct mlx5_txq_ctrl *txq_ctrl;
	uint32_t i;
	struct rte_hairpin_peer_info peer = {0xffffff};
	struct rte_hairpin_peer_info cur;
	const struct rte_eth_hairpin_conf *conf;
	uint16_t num_q = 0;
	uint16_t local_port = priv->dev_data->port_id;
	uint32_t manual;
	uint32_t explicit;
	uint16_t rx_queue;

	if (mlx5_eth_find_next(rx_port, dev->device) != rx_port) {
		rte_errno = ENODEV;
		DRV_LOG(ERR, "Rx port %u does not belong to mlx5", rx_port);
		return -rte_errno;
	}
	/*
	 * Before binding TxQ to peer RxQ, first round loop will be used for
	 * checking the queues' configuration consistency. This would be a
	 * little time consuming but better than doing the rollback.
	 */
	for (i = 0; i != priv->txqs_n; i++) {
		txq_ctrl = mlx5_txq_get(dev, i);
		if (txq_ctrl == NULL)
			continue;
		if (!txq_ctrl->is_hairpin) {
			mlx5_txq_release(dev, i);
			continue;
		}
		/*
		 * All hairpin Tx queues of a single port that connected to the
		 * same peer Rx port should have the same "auto binding" and
		 * "implicit Tx flow" modes.
		 * Peer consistency checking will be done in per queue binding.
		 */
		conf = &txq_ctrl->hairpin_conf;
		if (conf->peers[0].port == rx_port) {
			if (num_q == 0) {
				manual = conf->manual_bind;
				explicit = conf->tx_explicit;
			} else {
				if (manual != conf->manual_bind ||
				    explicit != conf->tx_explicit) {
					rte_errno = EINVAL;
					DRV_LOG(ERR, "port %u queue %d mode"
						" mismatch: %u %u, %u %u",
						local_port, i, manual,
						conf->manual_bind, explicit,
						conf->tx_explicit);
					mlx5_txq_release(dev, i);
					return -rte_errno;
				}
			}
			num_q++;
		}
		mlx5_txq_release(dev, i);
	}
	/* Once no queue is configured, success is returned directly. */
	if (num_q == 0)
		return ret;
	/* All the hairpin TX queues need to be traversed again. */
	for (i = 0; i != priv->txqs_n; i++) {
		txq_ctrl = mlx5_txq_get(dev, i);
		if (txq_ctrl == NULL)
			continue;
		if (!txq_ctrl->is_hairpin) {
			mlx5_txq_release(dev, i);
			continue;
		}
		if (txq_ctrl->hairpin_conf.peers[0].port != rx_port) {
			mlx5_txq_release(dev, i);
			continue;
		}
		rx_queue = txq_ctrl->hairpin_conf.peers[0].queue;
		/*
		 * Fetch peer RxQ's information.
		 * No need to pass the information of the current queue.
		 */
		ret = rte_eth_hairpin_queue_peer_update(rx_port, rx_queue,
							NULL, &peer, 1);
		if (ret != 0) {
			mlx5_txq_release(dev, i);
			goto error;
		}
		/* Accessing its own device, inside mlx5 PMD. */
		ret = mlx5_hairpin_queue_peer_bind(dev, i, &peer, 1);
		if (ret != 0) {
			mlx5_txq_release(dev, i);
			goto error;
		}
		/* Pass TxQ's information to peer RxQ and try binding. */
		cur.peer_q = rx_queue;
		cur.qp_id = mlx5_txq_get_sqn(txq_ctrl);
		cur.vhca_id = priv->sh->cdev->config.hca_attr.vhca_id;
		cur.tx_explicit = txq_ctrl->hairpin_conf.tx_explicit;
		cur.manual_bind = txq_ctrl->hairpin_conf.manual_bind;
		/*
		 * In order to access another device in a proper way, RTE level
		 * private function is needed.
		 */
		ret = rte_eth_hairpin_queue_peer_bind(rx_port, rx_queue,
						      &cur, 0);
		if (ret != 0) {
			mlx5_txq_release(dev, i);
			goto error;
		}
		mlx5_txq_release(dev, i);
	}
	return 0;
error:
	/*
	 * Do roll-back process for the queues already bound.
	 * No need to check the return value of the queue unbind function.
	 */
	do {
		/* No validation is needed here. */
		txq_ctrl = mlx5_txq_get(dev, i);
		if (txq_ctrl == NULL)
			continue;
		rx_queue = txq_ctrl->hairpin_conf.peers[0].queue;
		rte_eth_hairpin_queue_peer_unbind(rx_port, rx_queue, 0);
		mlx5_hairpin_queue_peer_unbind(dev, i, 1);
		mlx5_txq_release(dev, i);
	} while (i--);
	return ret;
}

/*
 * Unbind the hairpin port pair, HW configuration of both devices will be clear
 * and status will be reset for all the queues used between them.
 * This function only supports to unbind the Tx from one Rx.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param rx_port
 *   Port identifier of the Rx port.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_hairpin_unbind_single_port(struct rte_eth_dev *dev, uint16_t rx_port)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *txq_ctrl;
	uint32_t i;
	int ret;
	uint16_t cur_port = priv->dev_data->port_id;

	if (mlx5_eth_find_next(rx_port, dev->device) != rx_port) {
		rte_errno = ENODEV;
		DRV_LOG(ERR, "Rx port %u does not belong to mlx5", rx_port);
		return -rte_errno;
	}
	for (i = 0; i != priv->txqs_n; i++) {
		uint16_t rx_queue;

		txq_ctrl = mlx5_txq_get(dev, i);
		if (txq_ctrl == NULL)
			continue;
		if (!txq_ctrl->is_hairpin) {
			mlx5_txq_release(dev, i);
			continue;
		}
		if (txq_ctrl->hairpin_conf.peers[0].port != rx_port) {
			mlx5_txq_release(dev, i);
			continue;
		}
		/* Indeed, only the first used queue needs to be checked. */
		if (txq_ctrl->hairpin_conf.manual_bind == 0) {
			mlx5_txq_release(dev, i);
			if (cur_port != rx_port) {
				rte_errno = EINVAL;
				DRV_LOG(ERR, "port %u and port %u are in"
					" auto-bind mode", cur_port, rx_port);
				return -rte_errno;
			} else {
				return 0;
			}
		}
		rx_queue = txq_ctrl->hairpin_conf.peers[0].queue;
		mlx5_txq_release(dev, i);
		ret = rte_eth_hairpin_queue_peer_unbind(rx_port, rx_queue, 0);
		if (ret) {
			DRV_LOG(ERR, "port %u Rx queue %d unbind - failure",
				rx_port, rx_queue);
			return ret;
		}
		ret = mlx5_hairpin_queue_peer_unbind(dev, i, 1);
		if (ret) {
			DRV_LOG(ERR, "port %u Tx queue %d unbind - failure",
				cur_port, i);
			return ret;
		}
	}
	return 0;
}

/*
 * Bind hairpin ports, Rx could be all ports when using RTE_MAX_ETHPORTS.
 * @see mlx5_hairpin_bind_single_port()
 */
int
mlx5_hairpin_bind(struct rte_eth_dev *dev, uint16_t rx_port)
{
	int ret = 0;
	uint16_t p, pp;

	/*
	 * If the Rx port has no hairpin configuration with the current port,
	 * the binding will be skipped in the called function of single port.
	 * Device started status will be checked only before the queue
	 * information updating.
	 */
	if (rx_port == RTE_MAX_ETHPORTS) {
		MLX5_ETH_FOREACH_DEV(p, dev->device) {
			ret = mlx5_hairpin_bind_single_port(dev, p);
			if (ret != 0)
				goto unbind;
		}
		return ret;
	} else {
		return mlx5_hairpin_bind_single_port(dev, rx_port);
	}
unbind:
	MLX5_ETH_FOREACH_DEV(pp, dev->device)
		if (pp < p)
			mlx5_hairpin_unbind_single_port(dev, pp);
	return ret;
}

/*
 * Unbind hairpin ports, Rx could be all ports when using RTE_MAX_ETHPORTS.
 * @see mlx5_hairpin_unbind_single_port()
 */
int
mlx5_hairpin_unbind(struct rte_eth_dev *dev, uint16_t rx_port)
{
	int ret = 0;
	uint16_t p;

	if (rx_port == RTE_MAX_ETHPORTS)
		MLX5_ETH_FOREACH_DEV(p, dev->device) {
			ret = mlx5_hairpin_unbind_single_port(dev, p);
			if (ret != 0)
				return ret;
		}
	else
		ret = mlx5_hairpin_unbind_single_port(dev, rx_port);
	return ret;
}

/*
 * DPDK callback to get the hairpin peer ports list.
 * This will return the actual number of peer ports and save the identifiers
 * into the array (sorted, may be different from that when setting up the
 * hairpin peer queues).
 * The peer port ID could be the same as the port ID of the current device.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param peer_ports
 *   Pointer to array to save the port identifiers.
 * @param len
 *   The length of the array.
 * @param direction
 *   Current port to peer port direction.
 *   positive - current used as Tx to get all peer Rx ports.
 *   zero - current used as Rx to get all peer Tx ports.
 *
 * @return
 *   0 or positive value on success, actual number of peer ports.
 *   a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_hairpin_get_peer_ports(struct rte_eth_dev *dev, uint16_t *peer_ports,
			    size_t len, uint32_t direction)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *txq_ctrl;
	uint32_t i;
	uint16_t pp;
	uint32_t bits[(RTE_MAX_ETHPORTS + 31) / 32] = {0};
	int ret = 0;

	if (direction) {
		for (i = 0; i < priv->txqs_n; i++) {
			txq_ctrl = mlx5_txq_get(dev, i);
			if (!txq_ctrl)
				continue;
			if (!txq_ctrl->is_hairpin) {
				mlx5_txq_release(dev, i);
				continue;
			}
			pp = txq_ctrl->hairpin_conf.peers[0].port;
			if (pp >= RTE_MAX_ETHPORTS) {
				rte_errno = ERANGE;
				mlx5_txq_release(dev, i);
				DRV_LOG(ERR, "port %hu queue %u peer port "
					"out of range %hu",
					priv->dev_data->port_id, i, pp);
				return -rte_errno;
			}
			bits[pp / 32] |= 1 << (pp % 32);
			mlx5_txq_release(dev, i);
		}
	} else {
		for (i = 0; i < priv->rxqs_n; i++) {
			struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev, i);
			struct mlx5_rxq_ctrl *rxq_ctrl;

			if (rxq == NULL)
				continue;
			rxq_ctrl = rxq->ctrl;
			if (!rxq_ctrl->is_hairpin)
				continue;
			pp = rxq->hairpin_conf.peers[0].port;
			if (pp >= RTE_MAX_ETHPORTS) {
				rte_errno = ERANGE;
				DRV_LOG(ERR, "port %hu queue %u peer port "
					"out of range %hu",
					priv->dev_data->port_id, i, pp);
				return -rte_errno;
			}
			bits[pp / 32] |= 1 << (pp % 32);
		}
	}
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (bits[i / 32] & (1 << (i % 32))) {
			if ((size_t)ret >= len) {
				rte_errno = E2BIG;
				return -rte_errno;
			}
			peer_ports[ret++] = i;
		}
	}
	return ret;
}

#ifdef HAVE_MLX5_HWS_SUPPORT

/**
 * Check if starting representor port is allowed.
 *
 * If transfer proxy port is configured for HWS, then starting representor port
 * is allowed if and only if transfer proxy port is started as well.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   If stopping representor port is allowed, then 0 is returned.
 *   Otherwise rte_errno is set, and negative errno value is returned.
 */
static int
mlx5_hw_representor_port_allowed_start(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_eth_dev *proxy_dev;
	struct mlx5_priv *proxy_priv;
	uint16_t proxy_port_id = UINT16_MAX;
	int ret;

	MLX5_ASSERT(priv->sh->config.dv_flow_en == 2);
	MLX5_ASSERT(priv->sh->config.dv_esw_en);
	MLX5_ASSERT(priv->representor);
	ret = rte_flow_pick_transfer_proxy(dev->data->port_id, &proxy_port_id, NULL);
	if (ret) {
		if (ret == -ENODEV)
			DRV_LOG(ERR, "Starting representor port %u is not allowed. Transfer "
				     "proxy port is not available.", dev->data->port_id);
		else
			DRV_LOG(ERR, "Failed to pick transfer proxy for port %u (ret = %d)",
				dev->data->port_id, ret);
		return ret;
	}
	proxy_dev = &rte_eth_devices[proxy_port_id];
	proxy_priv = proxy_dev->data->dev_private;
	if (proxy_priv->dr_ctx == NULL) {
		DRV_LOG(DEBUG, "Starting representor port %u is allowed, but default traffic flows"
			       " will not be created. Transfer proxy port must be configured"
			       " for HWS and started.",
			       dev->data->port_id);
		return 0;
	}
	if (!proxy_dev->data->dev_started) {
		DRV_LOG(ERR, "Failed to start port %u: transfer proxy (port %u) must be started",
			     dev->data->port_id, proxy_port_id);
		rte_errno = EAGAIN;
		return -rte_errno;
	}
	if (priv->sh->config.repr_matching && !priv->dr_ctx) {
		DRV_LOG(ERR, "Failed to start port %u: with representor matching enabled, port "
			     "must be configured for HWS", dev->data->port_id);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
}

#endif

/**
 * DPDK callback to start the device.
 *
 * Simulate device start by attaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 *   The following error values are defined:
 *
 *   - -EAGAIN: If port representor cannot be started,
 *     because transfer proxy port is not started.
 */
int
mlx5_dev_start(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;
	int fine_inline;

	DRV_LOG(DEBUG, "port %u starting device", dev->data->port_id);
#ifdef HAVE_MLX5_HWS_SUPPORT
	if (priv->sh->config.dv_flow_en == 2) {
		/* If there is no E-Switch, then there are no start/stop order limitations. */
		if (!priv->sh->config.dv_esw_en)
			goto continue_dev_start;
		/* If master is being started, then it is always allowed. */
		if (priv->master)
			goto continue_dev_start;
		if (mlx5_hw_representor_port_allowed_start(dev))
			return -rte_errno;
	}
continue_dev_start:
#endif
	fine_inline = rte_mbuf_dynflag_lookup
		(RTE_PMD_MLX5_FINE_GRANULARITY_INLINE, NULL);
	if (fine_inline >= 0)
		rte_net_mlx5_dynf_inline_mask = 1UL << fine_inline;
	else
		rte_net_mlx5_dynf_inline_mask = 0;
	if (dev->data->nb_rx_queues > 0) {
		uint32_t max_lro_msg_size = priv->max_lro_msg_size;

		if (max_lro_msg_size < MLX5_LRO_SEG_CHUNK_SIZE) {
			uint32_t i;
			struct mlx5_rxq_priv *rxq;

			for (i = 0; i != priv->rxqs_n; ++i) {
				rxq = mlx5_rxq_get(dev, i);
				if (rxq && rxq->ctrl && rxq->ctrl->rxq.lro) {
					DRV_LOG(ERR, "port %u invalid max LRO size",
						dev->data->port_id);
					rte_errno = EINVAL;
					return -rte_errno;
				}
			}
		}
		ret = mlx5_dev_configure_rss_reta(dev);
		if (ret) {
			DRV_LOG(ERR, "port %u reta config failed: %s",
				dev->data->port_id, strerror(rte_errno));
			return -rte_errno;
		}
	}
	ret = mlx5_txpp_start(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u Tx packet pacing init failed: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	if (mlx5_devx_obj_ops_en(priv->sh) &&
	    priv->obj_ops.lb_dummy_queue_create) {
		ret = priv->obj_ops.lb_dummy_queue_create(dev);
		if (ret)
			goto error;
	}
	ret = mlx5_txq_start(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u Tx queue allocation failed: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	if (priv->config.std_delay_drop || priv->config.hp_delay_drop) {
		if (!priv->sh->dev_cap.vf && !priv->sh->dev_cap.sf &&
		    !priv->representor) {
			ret = mlx5_get_flag_dropless_rq(dev);
			if (ret < 0)
				DRV_LOG(WARNING,
					"port %u cannot query dropless flag",
					dev->data->port_id);
			else if (!ret)
				DRV_LOG(WARNING,
					"port %u dropless_rq OFF, no rearming",
					dev->data->port_id);
		} else {
			DRV_LOG(DEBUG,
				"port %u doesn't support dropless_rq flag",
				dev->data->port_id);
		}
	}
	ret = mlx5_rxq_start(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u Rx queue allocation failed: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	/*
	 * Such step will be skipped if there is no hairpin TX queue configured
	 * with RX peer queue from the same device.
	 */
	ret = mlx5_hairpin_auto_bind(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u hairpin auto binding failed: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	/* Set started flag here for the following steps like control flow. */
	dev->data->dev_started = 1;
	ret = mlx5_rx_intr_vec_enable(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u Rx interrupt vector creation failed",
			dev->data->port_id);
		goto error;
	}
	mlx5_os_stats_init(dev);
	/*
	 * Attach indirection table objects detached on port stop.
	 * They may be needed to create RSS in non-isolated mode.
	 */
	ret = mlx5_action_handle_attach(dev);
	if (ret) {
		DRV_LOG(ERR,
			"port %u failed to attach indirect actions: %s",
			dev->data->port_id, rte_strerror(rte_errno));
		goto error;
	}
#ifdef HAVE_MLX5_HWS_SUPPORT
	if (priv->sh->config.dv_flow_en == 2) {
		ret = flow_hw_table_update(dev, NULL);
		if (ret) {
			DRV_LOG(ERR, "port %u failed to update HWS tables",
				dev->data->port_id);
			goto error;
		}
	}
#endif
	ret = mlx5_traffic_enable(dev);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to set defaults flows",
			dev->data->port_id);
		goto error;
	}
	/* Set a mask and offset of dynamic metadata flows into Rx queues. */
	mlx5_flow_rxq_dynf_metadata_set(dev);
	/* Set flags and context to convert Rx timestamps. */
	mlx5_rxq_timestamp_set(dev);
	/* Set a mask and offset of scheduling on timestamp into Tx queues. */
	mlx5_txq_dynf_timestamp_set(dev);
	/*
	 * In non-cached mode, it only needs to start the default mreg copy
	 * action and no flow created by application exists anymore.
	 * But it is worth wrapping the interface for further usage.
	 */
	ret = mlx5_flow_start_default(dev);
	if (ret) {
		DRV_LOG(DEBUG, "port %u failed to start default actions: %s",
			dev->data->port_id, strerror(rte_errno));
		goto error;
	}
	if (mlx5_dev_ctx_shared_mempool_subscribe(dev) != 0) {
		DRV_LOG(ERR, "port %u failed to subscribe for mempool life cycle: %s",
			dev->data->port_id, rte_strerror(rte_errno));
		goto error;
	}
	rte_wmb();
	dev->tx_pkt_burst = mlx5_select_tx_function(dev);
	dev->rx_pkt_burst = mlx5_select_rx_function(dev);
	/* Enable datapath on secondary process. */
	mlx5_mp_os_req_start_rxtx(dev);
	if (rte_intr_fd_get(priv->sh->intr_handle) >= 0) {
		priv->sh->port[priv->dev_port - 1].ih_port_id =
					(uint32_t)dev->data->port_id;
	} else {
		DRV_LOG(INFO, "port %u starts without RMV interrupts.",
			dev->data->port_id);
		dev->data->dev_conf.intr_conf.rmv = 0;
	}
	if (rte_intr_fd_get(priv->sh->intr_handle_nl) >= 0) {
		priv->sh->port[priv->dev_port - 1].nl_ih_port_id =
					(uint32_t)dev->data->port_id;
	} else {
		DRV_LOG(INFO, "port %u starts without LSC interrupts.",
			dev->data->port_id);
		dev->data->dev_conf.intr_conf.lsc = 0;
	}
	if (rte_intr_fd_get(priv->sh->intr_handle_devx) >= 0)
		priv->sh->port[priv->dev_port - 1].devx_ih_port_id =
					(uint32_t)dev->data->port_id;
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	/* Rollback. */
	dev->data->dev_started = 0;
	mlx5_flow_stop_default(dev);
	mlx5_traffic_disable(dev);
	mlx5_txq_stop(dev);
	mlx5_rxq_stop(dev);
	if (priv->obj_ops.lb_dummy_queue_release)
		priv->obj_ops.lb_dummy_queue_release(dev);
	mlx5_txpp_stop(dev); /* Stop last. */
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

#ifdef HAVE_MLX5_HWS_SUPPORT
/**
 * Check if stopping transfer proxy port is allowed.
 *
 * If transfer proxy port is configured for HWS, then it is allowed to stop it
 * if and only if all other representor ports are stopped.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   If stopping transfer proxy port is allowed, then 0 is returned.
 *   Otherwise rte_errno is set, and negative errno value is returned.
 */
static int
mlx5_hw_proxy_port_allowed_stop(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	bool representor_started = false;
	uint16_t port_id;

	MLX5_ASSERT(priv->sh->config.dv_flow_en == 2);
	MLX5_ASSERT(priv->sh->config.dv_esw_en);
	MLX5_ASSERT(priv->master);
	/* If transfer proxy port was not configured for HWS, then stopping it is allowed. */
	if (!priv->dr_ctx)
		return 0;
	MLX5_ETH_FOREACH_DEV(port_id, dev->device) {
		const struct rte_eth_dev *port_dev = &rte_eth_devices[port_id];
		const struct mlx5_priv *port_priv = port_dev->data->dev_private;

		if (port_id != dev->data->port_id &&
		    port_priv->domain_id == priv->domain_id &&
		    port_dev->data->dev_started)
			representor_started = true;
	}
	if (representor_started) {
		DRV_LOG(ERR, "Failed to stop port %u: attached representor ports"
			     " must be stopped before stopping transfer proxy port",
			     dev->data->port_id);
		rte_errno = EBUSY;
		return -rte_errno;
	}
	return 0;
}
#endif

/**
 * DPDK callback to stop the device.
 *
 * Simulate device stop by detaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 *   The following error values are defined:
 *
 *   - -EBUSY: If transfer proxy port cannot be stopped,
 *     because other port representors are still running.
 */
int
mlx5_dev_stop(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

#ifdef HAVE_MLX5_HWS_SUPPORT
	if (priv->sh->config.dv_flow_en == 2) {
		/* If there is no E-Switch, then there are no start/stop order limitations. */
		if (!priv->sh->config.dv_esw_en)
			goto continue_dev_stop;
		/* If representor is being stopped, then it is always allowed. */
		if (priv->representor)
			goto continue_dev_stop;
		if (mlx5_hw_proxy_port_allowed_stop(dev)) {
			dev->data->dev_started = 1;
			return -rte_errno;
		}
	}
continue_dev_stop:
#endif
	dev->data->dev_started = 0;
	/* Prevent crashes when queues are still in use. */
	dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	rte_wmb();
	/* Disable datapath on secondary process. */
	mlx5_mp_os_req_stop_rxtx(dev);
	rte_delay_us_sleep(1000 * priv->rxqs_n);
	DRV_LOG(DEBUG, "port %u stopping device", dev->data->port_id);
	if (priv->sh->config.dv_flow_en == 2) {
		if (!__atomic_load_n(&priv->hws_mark_refcnt, __ATOMIC_RELAXED))
			flow_hw_rxq_flag_set(dev, false);
	} else {
		mlx5_flow_stop_default(dev);
	}
	/* Control flows for default traffic can be removed firstly. */
	mlx5_traffic_disable(dev);
	/* All RX queue flags will be cleared in the flush interface. */
	mlx5_flow_list_flush(dev, MLX5_FLOW_TYPE_GEN, true);
	mlx5_flow_meter_rxq_flush(dev);
	mlx5_action_handle_detach(dev);
#ifdef HAVE_MLX5_HWS_SUPPORT
	mlx5_flow_hw_cleanup_ctrl_rx_templates(dev);
#endif
	mlx5_rx_intr_vec_disable(dev);
	priv->sh->port[priv->dev_port - 1].ih_port_id = RTE_MAX_ETHPORTS;
	priv->sh->port[priv->dev_port - 1].devx_ih_port_id = RTE_MAX_ETHPORTS;
	priv->sh->port[priv->dev_port - 1].nl_ih_port_id = RTE_MAX_ETHPORTS;
	mlx5_txq_stop(dev);
	mlx5_rxq_stop(dev);
	if (priv->obj_ops.lb_dummy_queue_release)
		priv->obj_ops.lb_dummy_queue_release(dev);
	mlx5_txpp_stop(dev);

	return 0;
}

#ifdef HAVE_MLX5_HWS_SUPPORT

static int
mlx5_traffic_enable_hws(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *config = &priv->sh->config;
	uint64_t flags = 0;
	unsigned int i;
	int ret;

	/*
	 * With extended metadata enabled, the Tx metadata copy is handled by default
	 * Tx tagging flow rules, so default Tx flow rule is not needed. It is only
	 * required when representor matching is disabled.
	 */
	if (config->dv_esw_en &&
	    !config->repr_matching &&
	    config->dv_xmeta_en == MLX5_XMETA_MODE_META32_HWS &&
	    priv->master) {
		if (mlx5_flow_hw_create_tx_default_mreg_copy_flow(dev))
			goto error;
	}
	for (i = 0; i < priv->txqs_n; ++i) {
		struct mlx5_txq_ctrl *txq = mlx5_txq_get(dev, i);
		uint32_t queue;

		if (!txq)
			continue;
		queue = mlx5_txq_get_sqn(txq);
		if ((priv->representor || priv->master) && config->dv_esw_en) {
			if (mlx5_flow_hw_esw_create_sq_miss_flow(dev, queue)) {
				mlx5_txq_release(dev, i);
				goto error;
			}
		}
		if (config->dv_esw_en && config->repr_matching) {
			if (mlx5_flow_hw_tx_repr_matching_flow(dev, queue)) {
				mlx5_txq_release(dev, i);
				goto error;
			}
		}
		mlx5_txq_release(dev, i);
	}
	if (config->fdb_def_rule) {
		if ((priv->master || priv->representor) && config->dv_esw_en) {
			if (!mlx5_flow_hw_esw_create_default_jump_flow(dev))
				priv->fdb_def_rule = 1;
			else
				goto error;
		}
	} else {
		DRV_LOG(INFO, "port %u FDB default rule is disabled", dev->data->port_id);
	}
	if (priv->isolated)
		return 0;
	if (dev->data->promiscuous)
		flags |= MLX5_CTRL_PROMISCUOUS;
	if (dev->data->all_multicast)
		flags |= MLX5_CTRL_ALL_MULTICAST;
	else
		flags |= MLX5_CTRL_BROADCAST | MLX5_CTRL_IPV4_MULTICAST | MLX5_CTRL_IPV6_MULTICAST;
	flags |= MLX5_CTRL_DMAC;
	if (priv->vlan_filter_n)
		flags |= MLX5_CTRL_VLAN_FILTER;
	return mlx5_flow_hw_ctrl_flows(dev, flags);
error:
	ret = rte_errno;
	mlx5_flow_hw_flush_ctrl_flows(dev);
	rte_errno = ret;
	return -rte_errno;
}

#endif

/**
 * Enable traffic flows configured by control plane
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_traffic_enable(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_item_eth bcast = {
		.hdr.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	};
	struct rte_flow_item_eth ipv6_multi_spec = {
		.hdr.dst_addr.addr_bytes = "\x33\x33\x00\x00\x00\x00",
	};
	struct rte_flow_item_eth ipv6_multi_mask = {
		.hdr.dst_addr.addr_bytes = "\xff\xff\x00\x00\x00\x00",
	};
	struct rte_flow_item_eth unicast = {
		.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	};
	struct rte_flow_item_eth unicast_mask = {
		.hdr.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	};
	const unsigned int vlan_filter_n = priv->vlan_filter_n;
	const struct rte_ether_addr cmp = {
		.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	};
	unsigned int i;
	unsigned int j;
	int ret;

#ifdef HAVE_MLX5_HWS_SUPPORT
	if (priv->sh->config.dv_flow_en == 2)
		return mlx5_traffic_enable_hws(dev);
#endif
	/*
	 * Hairpin txq default flow should be created no matter if it is
	 * isolation mode. Or else all the packets to be sent will be sent
	 * out directly without the TX flow actions, e.g. encapsulation.
	 */
	for (i = 0; i != priv->txqs_n; ++i) {
		struct mlx5_txq_ctrl *txq_ctrl = mlx5_txq_get(dev, i);
		if (!txq_ctrl)
			continue;
		/* Only Tx implicit mode requires the default Tx flow. */
		if (txq_ctrl->is_hairpin &&
		    txq_ctrl->hairpin_conf.tx_explicit == 0 &&
		    txq_ctrl->hairpin_conf.peers[0].port ==
		    priv->dev_data->port_id) {
			ret = mlx5_ctrl_flow_source_queue(dev,
					mlx5_txq_get_sqn(txq_ctrl));
			if (ret) {
				mlx5_txq_release(dev, i);
				goto error;
			}
		}
		if (priv->sh->config.dv_esw_en) {
			uint32_t q = mlx5_txq_get_sqn(txq_ctrl);

			if (mlx5_flow_create_devx_sq_miss_flow(dev, q) == 0) {
				mlx5_txq_release(dev, i);
				DRV_LOG(ERR,
					"Port %u Tx queue %u SQ create representor devx default miss rule failed.",
					dev->data->port_id, i);
				goto error;
			}
		}
		mlx5_txq_release(dev, i);
	}
	if (priv->sh->config.fdb_def_rule) {
		if (priv->sh->config.dv_esw_en) {
			if (mlx5_flow_create_esw_table_zero_flow(dev))
				priv->fdb_def_rule = 1;
			else
				DRV_LOG(INFO, "port %u FDB default rule cannot be configured - only Eswitch group 0 flows are supported.",
					dev->data->port_id);
		}
	} else {
		DRV_LOG(INFO, "port %u FDB default rule is disabled",
			dev->data->port_id);
	}
	if (!priv->sh->config.lacp_by_user && priv->pf_bond >= 0) {
		ret = mlx5_flow_lacp_miss(dev);
		if (ret)
			DRV_LOG(INFO, "port %u LACP rule cannot be created - "
				"forward LACP to kernel.", dev->data->port_id);
		else
			DRV_LOG(INFO, "LACP traffic will be missed in port %u."
				, dev->data->port_id);
	}
	if (priv->isolated)
		return 0;
	if (dev->data->promiscuous) {
		struct rte_flow_item_eth promisc = {
			.hdr.dst_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.hdr.ether_type = 0,
		};

		ret = mlx5_ctrl_flow(dev, &promisc, &promisc);
		if (ret)
			goto error;
	}
	if (dev->data->all_multicast) {
		struct rte_flow_item_eth multicast = {
			.hdr.dst_addr.addr_bytes = "\x01\x00\x00\x00\x00\x00",
			.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.hdr.ether_type = 0,
		};

		ret = mlx5_ctrl_flow(dev, &multicast, &multicast);
		if (ret)
			goto error;
	} else {
		/* Add broadcast/multicast flows. */
		for (i = 0; i != vlan_filter_n; ++i) {
			uint16_t vlan = priv->vlan_filter[i];

			struct rte_flow_item_vlan vlan_spec = {
				.hdr.vlan_tci = rte_cpu_to_be_16(vlan),
			};
			struct rte_flow_item_vlan vlan_mask =
				rte_flow_item_vlan_mask;

			ret = mlx5_ctrl_flow_vlan(dev, &bcast, &bcast,
						  &vlan_spec, &vlan_mask);
			if (ret)
				goto error;
			ret = mlx5_ctrl_flow_vlan(dev, &ipv6_multi_spec,
						  &ipv6_multi_mask,
						  &vlan_spec, &vlan_mask);
			if (ret)
				goto error;
		}
		if (!vlan_filter_n) {
			ret = mlx5_ctrl_flow(dev, &bcast, &bcast);
			if (ret)
				goto error;
			ret = mlx5_ctrl_flow(dev, &ipv6_multi_spec,
					     &ipv6_multi_mask);
			if (ret) {
				/* Do not fail on IPv6 broadcast creation failure. */
				DRV_LOG(WARNING,
					"IPv6 broadcast is not supported");
				ret = 0;
			}
		}
	}
	/* Add MAC address flows. */
	for (i = 0; i != MLX5_MAX_MAC_ADDRESSES; ++i) {
		struct rte_ether_addr *mac = &dev->data->mac_addrs[i];

		if (!memcmp(mac, &cmp, sizeof(*mac)))
			continue;
		memcpy(&unicast.hdr.dst_addr.addr_bytes,
		       mac->addr_bytes,
		       RTE_ETHER_ADDR_LEN);
		for (j = 0; j != vlan_filter_n; ++j) {
			uint16_t vlan = priv->vlan_filter[j];

			struct rte_flow_item_vlan vlan_spec = {
				.hdr.vlan_tci = rte_cpu_to_be_16(vlan),
			};
			struct rte_flow_item_vlan vlan_mask =
				rte_flow_item_vlan_mask;

			ret = mlx5_ctrl_flow_vlan(dev, &unicast,
						  &unicast_mask,
						  &vlan_spec,
						  &vlan_mask);
			if (ret)
				goto error;
		}
		if (!vlan_filter_n) {
			ret = mlx5_ctrl_flow(dev, &unicast, &unicast_mask);
			if (ret)
				goto error;
		}
	}
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	mlx5_flow_list_flush(dev, MLX5_FLOW_TYPE_CTL, false);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}


/**
 * Disable traffic flows configured by control plane
 *
 * @param dev
 *   Pointer to Ethernet device private data.
 */
void
mlx5_traffic_disable(struct rte_eth_dev *dev)
{
#ifdef HAVE_MLX5_HWS_SUPPORT
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->sh->config.dv_flow_en == 2)
		mlx5_flow_hw_flush_ctrl_flows(dev);
	else
#endif
		mlx5_flow_list_flush(dev, MLX5_FLOW_TYPE_CTL, false);
}

/**
 * Restart traffic flows configured by control plane
 *
 * @param dev
 *   Pointer to Ethernet device private data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_traffic_restart(struct rte_eth_dev *dev)
{
	if (dev->data->dev_started) {
		mlx5_traffic_disable(dev);
#ifdef HAVE_MLX5_HWS_SUPPORT
		mlx5_flow_hw_cleanup_ctrl_rx_templates(dev);
#endif
		return mlx5_traffic_enable(dev);
	}
	return 0;
}
