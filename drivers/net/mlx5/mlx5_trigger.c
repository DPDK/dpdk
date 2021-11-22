/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <unistd.h>

#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>

#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_mr.h"
#include "mlx5_rxtx.h"
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
		if (txq_ctrl->type == MLX5_TXQ_TYPE_STANDARD)
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
		if (txq_ctrl->type == MLX5_TXQ_TYPE_STANDARD) {
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
	DRV_LOG(DEBUG, "Port %u device_attr.max_qp_wr is %d.",
		dev->data->port_id, priv->sh->device_attr.max_qp_wr);
	DRV_LOG(DEBUG, "Port %u device_attr.max_sge is %d.",
		dev->data->port_id, priv->sh->device_attr.max_sge);
	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl = mlx5_rxq_get(dev, i);

		if (!rxq_ctrl)
			continue;
		if (rxq_ctrl->type == MLX5_RXQ_TYPE_STANDARD) {
			/* Pre-register Rx mempools. */
			if (mlx5_rxq_mprq_enabled(&rxq_ctrl->rxq)) {
				mlx5_mr_update_mp(dev, &rxq_ctrl->rxq.mr_ctrl,
						  rxq_ctrl->rxq.mprq_mp);
			} else {
				uint32_t s;

				for (s = 0; s < rxq_ctrl->rxq.rxseg_n; s++)
					mlx5_mr_update_mp
						(dev, &rxq_ctrl->rxq.mr_ctrl,
						rxq_ctrl->rxq.rxseg[s].mp);
			}
			ret = rxq_alloc_elts(rxq_ctrl);
			if (ret)
				goto error;
		}
		MLX5_ASSERT(!rxq_ctrl->obj);
		rxq_ctrl->obj = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO,
					    sizeof(*rxq_ctrl->obj), 0,
					    rxq_ctrl->socket);
		if (!rxq_ctrl->obj) {
			DRV_LOG(ERR,
				"Port %u Rx queue %u can't allocate resources.",
				dev->data->port_id, (*priv->rxqs)[i]->idx);
			rte_errno = ENOMEM;
			goto error;
		}
		ret = priv->obj_ops.rxq_obj_new(dev, i);
		if (ret) {
			mlx5_free(rxq_ctrl->obj);
			rxq_ctrl->obj = NULL;
			goto error;
		}
		DRV_LOG(DEBUG, "Port %u rxq %u updated with %p.",
			dev->data->port_id, i, (void *)&rxq_ctrl->obj);
		LIST_INSERT_HEAD(&priv->rxqsobj, rxq_ctrl->obj, next);
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
		if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN ||
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
		if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN ||
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
		rxq_ctrl = mlx5_rxq_get(dev,
					txq_ctrl->hairpin_conf.peers[0].queue);
		if (!rxq_ctrl) {
			mlx5_txq_release(dev, i);
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u no rxq object found: %d",
				dev->data->port_id,
				txq_ctrl->hairpin_conf.peers[0].queue);
			return -rte_errno;
		}
		if (rxq_ctrl->type != MLX5_RXQ_TYPE_HAIRPIN ||
		    rxq_ctrl->hairpin_conf.peers[0].queue != i) {
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
		sq_attr.hairpin_peer_vhca = priv->config.hca_attr.vhca_id;
		ret = mlx5_devx_cmd_modify_sq(sq, &sq_attr);
		if (ret)
			goto error;
		rq_attr.state = MLX5_SQC_STATE_RDY;
		rq_attr.rq_state = MLX5_SQC_STATE_RST;
		rq_attr.hairpin_peer_sq = sq->id;
		rq_attr.hairpin_peer_vhca = priv->config.hca_attr.vhca_id;
		ret = mlx5_devx_cmd_modify_rq(rq, &rq_attr);
		if (ret)
			goto error;
		/* Qs with auto-bind will be destroyed directly. */
		rxq_ctrl->hairpin_status = 1;
		txq_ctrl->hairpin_status = 1;
		mlx5_txq_release(dev, i);
		mlx5_rxq_release(dev, txq_ctrl->hairpin_conf.peers[0].queue);
	}
	return 0;
error:
	mlx5_txq_release(dev, i);
	mlx5_rxq_release(dev, txq_ctrl->hairpin_conf.peers[0].queue);
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
		if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN) {
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
		peer_info->qp_id = txq_ctrl->obj->sq->id;
		peer_info->vhca_id = priv->config.hca_attr.vhca_id;
		/* 1-to-1 mapping, only the first one is used. */
		peer_info->peer_q = txq_ctrl->hairpin_conf.peers[0].queue;
		peer_info->tx_explicit = txq_ctrl->hairpin_conf.tx_explicit;
		peer_info->manual_bind = txq_ctrl->hairpin_conf.manual_bind;
		mlx5_txq_release(dev, peer_queue);
	} else { /* Peer port used as ingress. */
		struct mlx5_rxq_ctrl *rxq_ctrl;

		rxq_ctrl = mlx5_rxq_get(dev, peer_queue);
		if (rxq_ctrl == NULL) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "Failed to get port %u Rx queue %d",
				dev->data->port_id, peer_queue);
			return -rte_errno;
		}
		if (rxq_ctrl->type != MLX5_RXQ_TYPE_HAIRPIN) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u queue %d is not a hairpin Rxq",
				dev->data->port_id, peer_queue);
			mlx5_rxq_release(dev, peer_queue);
			return -rte_errno;
		}
		if (rxq_ctrl->obj == NULL || rxq_ctrl->obj->rq == NULL) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no Rxq object found: %d",
				dev->data->port_id, peer_queue);
			mlx5_rxq_release(dev, peer_queue);
			return -rte_errno;
		}
		peer_info->qp_id = rxq_ctrl->obj->rq->id;
		peer_info->vhca_id = priv->config.hca_attr.vhca_id;
		peer_info->peer_q = rxq_ctrl->hairpin_conf.peers[0].queue;
		peer_info->tx_explicit = rxq_ctrl->hairpin_conf.tx_explicit;
		peer_info->manual_bind = rxq_ctrl->hairpin_conf.manual_bind;
		mlx5_rxq_release(dev, peer_queue);
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
		if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN) {
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
		struct mlx5_rxq_ctrl *rxq_ctrl;
		struct mlx5_devx_modify_rq_attr rq_attr = { 0 };

		rxq_ctrl = mlx5_rxq_get(dev, cur_queue);
		if (rxq_ctrl == NULL) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "Failed to get port %u Rx queue %d",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		if (rxq_ctrl->type != MLX5_RXQ_TYPE_HAIRPIN) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u queue %d not a hairpin Rxq",
				dev->data->port_id, cur_queue);
			mlx5_rxq_release(dev, cur_queue);
			return -rte_errno;
		}
		if (rxq_ctrl->obj == NULL || rxq_ctrl->obj->rq == NULL) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no Rxq object found: %d",
				dev->data->port_id, cur_queue);
			mlx5_rxq_release(dev, cur_queue);
			return -rte_errno;
		}
		if (rxq_ctrl->hairpin_status != 0) {
			DRV_LOG(DEBUG, "port %u Rx queue %d is already bound",
				dev->data->port_id, cur_queue);
			mlx5_rxq_release(dev, cur_queue);
			return 0;
		}
		if (peer_info->tx_explicit !=
		    rxq_ctrl->hairpin_conf.tx_explicit) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u Rx queue %d and peer Tx rule mode"
				" mismatch", dev->data->port_id, cur_queue);
			mlx5_rxq_release(dev, cur_queue);
			return -rte_errno;
		}
		if (peer_info->manual_bind !=
		    rxq_ctrl->hairpin_conf.manual_bind) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u Rx queue %d and peer binding mode"
				" mismatch", dev->data->port_id, cur_queue);
			mlx5_rxq_release(dev, cur_queue);
			return -rte_errno;
		}
		rq_attr.state = MLX5_SQC_STATE_RDY;
		rq_attr.rq_state = MLX5_SQC_STATE_RST;
		rq_attr.hairpin_peer_sq = peer_info->qp_id;
		rq_attr.hairpin_peer_vhca = peer_info->vhca_id;
		ret = mlx5_devx_cmd_modify_rq(rxq_ctrl->obj->rq, &rq_attr);
		if (ret == 0)
			rxq_ctrl->hairpin_status = 1;
		mlx5_rxq_release(dev, cur_queue);
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
		if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN) {
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
		struct mlx5_rxq_ctrl *rxq_ctrl;
		struct mlx5_devx_modify_rq_attr rq_attr = { 0 };

		rxq_ctrl = mlx5_rxq_get(dev, cur_queue);
		if (rxq_ctrl == NULL) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "Failed to get port %u Rx queue %d",
				dev->data->port_id, cur_queue);
			return -rte_errno;
		}
		if (rxq_ctrl->type != MLX5_RXQ_TYPE_HAIRPIN) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u queue %d not a hairpin Rxq",
				dev->data->port_id, cur_queue);
			mlx5_rxq_release(dev, cur_queue);
			return -rte_errno;
		}
		if (rxq_ctrl->hairpin_status == 0) {
			DRV_LOG(DEBUG, "port %u Rx queue %d is already unbound",
				dev->data->port_id, cur_queue);
			mlx5_rxq_release(dev, cur_queue);
			return 0;
		}
		if (rxq_ctrl->obj == NULL || rxq_ctrl->obj->rq == NULL) {
			rte_errno = ENOMEM;
			DRV_LOG(ERR, "port %u no Rxq object found: %d",
				dev->data->port_id, cur_queue);
			mlx5_rxq_release(dev, cur_queue);
			return -rte_errno;
		}
		rq_attr.state = MLX5_SQC_STATE_RST;
		rq_attr.rq_state = MLX5_SQC_STATE_RST;
		ret = mlx5_devx_cmd_modify_rq(rxq_ctrl->obj->rq, &rq_attr);
		if (ret == 0)
			rxq_ctrl->hairpin_status = 0;
		mlx5_rxq_release(dev, cur_queue);
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

	if (mlx5_eth_find_next(rx_port, priv->pci_dev) != rx_port) {
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
		if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN) {
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
		if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN) {
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
		cur.qp_id = txq_ctrl->obj->sq->id;
		cur.vhca_id = priv->config.hca_attr.vhca_id;
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

	if (mlx5_eth_find_next(rx_port, priv->pci_dev) != rx_port) {
		rte_errno = ENODEV;
		DRV_LOG(ERR, "Rx port %u does not belong to mlx5", rx_port);
		return -rte_errno;
	}
	for (i = 0; i != priv->txqs_n; i++) {
		uint16_t rx_queue;

		txq_ctrl = mlx5_txq_get(dev, i);
		if (txq_ctrl == NULL)
			continue;
		if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN) {
			mlx5_txq_release(dev, i);
			continue;
		}
		if (txq_ctrl->hairpin_conf.peers[0].port != rx_port) {
			mlx5_txq_release(dev, i);
			continue;
		}
		/* Indeed, only the first used queue needs to be checked. */
		if (txq_ctrl->hairpin_conf.manual_bind == 0) {
			if (cur_port != rx_port) {
				rte_errno = EINVAL;
				DRV_LOG(ERR, "port %u and port %u are in"
					" auto-bind mode", cur_port, rx_port);
				mlx5_txq_release(dev, i);
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
	struct mlx5_priv *priv = dev->data->dev_private;

	/*
	 * If the Rx port has no hairpin configuration with the current port,
	 * the binding will be skipped in the called function of single port.
	 * Device started status will be checked only before the queue
	 * information updating.
	 */
	if (rx_port == RTE_MAX_ETHPORTS) {
		MLX5_ETH_FOREACH_DEV(p, priv->pci_dev) {
			ret = mlx5_hairpin_bind_single_port(dev, p);
			if (ret != 0)
				goto unbind;
		}
		return ret;
	} else {
		return mlx5_hairpin_bind_single_port(dev, rx_port);
	}
unbind:
	MLX5_ETH_FOREACH_DEV(pp, priv->pci_dev)
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
	struct mlx5_priv *priv = dev->data->dev_private;

	if (rx_port == RTE_MAX_ETHPORTS)
		MLX5_ETH_FOREACH_DEV(p, priv->pci_dev) {
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
	struct mlx5_rxq_ctrl *rxq_ctrl;
	uint32_t i;
	uint16_t pp;
	uint32_t bits[(RTE_MAX_ETHPORTS + 31) / 32] = {0};
	int ret = 0;

	if (direction) {
		for (i = 0; i < priv->txqs_n; i++) {
			txq_ctrl = mlx5_txq_get(dev, i);
			if (!txq_ctrl)
				continue;
			if (txq_ctrl->type != MLX5_TXQ_TYPE_HAIRPIN) {
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
			rxq_ctrl = mlx5_rxq_get(dev, i);
			if (!rxq_ctrl)
				continue;
			if (rxq_ctrl->type != MLX5_RXQ_TYPE_HAIRPIN) {
				mlx5_rxq_release(dev, i);
				continue;
			}
			pp = rxq_ctrl->hairpin_conf.peers[0].port;
			if (pp >= RTE_MAX_ETHPORTS) {
				rte_errno = ERANGE;
				mlx5_rxq_release(dev, i);
				DRV_LOG(ERR, "port %hu queue %u peer port "
					"out of range %hu",
					priv->dev_data->port_id, i, pp);
				return -rte_errno;
			}
			bits[pp / 32] |= 1 << (pp % 32);
			mlx5_rxq_release(dev, i);
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
 */
int
mlx5_dev_start(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;
	int fine_inline;

	DRV_LOG(DEBUG, "port %u starting device", dev->data->port_id);
	fine_inline = rte_mbuf_dynflag_lookup
		(RTE_PMD_MLX5_FINE_GRANULARITY_INLINE, NULL);
	if (fine_inline >= 0)
		rte_net_mlx5_dynf_inline_mask = 1UL << fine_inline;
	else
		rte_net_mlx5_dynf_inline_mask = 0;
	if (dev->data->nb_rx_queues > 0) {
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
	if ((priv->config.devx && priv->config.dv_flow_en &&
	    priv->config.dest_tir) && priv->obj_ops.lb_dummy_queue_create) {
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
	rte_wmb();
	dev->tx_pkt_burst = mlx5_select_tx_function(dev);
	dev->rx_pkt_burst = mlx5_select_rx_function(dev);
	/* Enable datapath on secondary process. */
	mlx5_mp_os_req_start_rxtx(dev);
	if (priv->sh->intr_handle.fd >= 0) {
		priv->sh->port[priv->dev_port - 1].ih_port_id =
					(uint32_t)dev->data->port_id;
	} else {
		DRV_LOG(INFO, "port %u starts without LSC and RMV interrupts.",
			dev->data->port_id);
		dev->data->dev_conf.intr_conf.lsc = 0;
		dev->data->dev_conf.intr_conf.rmv = 0;
	}
	if (priv->sh->intr_handle_devx.fd >= 0)
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

/**
 * DPDK callback to stop the device.
 *
 * Simulate device stop by detaching all configured flows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
int
mlx5_dev_stop(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	dev->data->dev_started = 0;
	/* Prevent crashes when queues are still in use. */
	dev->rx_pkt_burst = removed_rx_burst;
	dev->tx_pkt_burst = removed_tx_burst;
	rte_wmb();
	/* Disable datapath on secondary process. */
	mlx5_mp_os_req_stop_rxtx(dev);
	usleep(1000 * priv->rxqs_n);
	DRV_LOG(DEBUG, "port %u stopping device", dev->data->port_id);
	mlx5_flow_stop_default(dev);
	/* Control flows for default traffic can be removed firstly. */
	mlx5_traffic_disable(dev);
	/* All RX queue flags will be cleared in the flush interface. */
	mlx5_flow_list_flush(dev, &priv->flows, true);
	mlx5_rx_intr_vec_disable(dev);
	priv->sh->port[priv->dev_port - 1].ih_port_id = RTE_MAX_ETHPORTS;
	priv->sh->port[priv->dev_port - 1].devx_ih_port_id = RTE_MAX_ETHPORTS;
	mlx5_txq_stop(dev);
	mlx5_rxq_stop(dev);
	if (priv->obj_ops.lb_dummy_queue_release)
		priv->obj_ops.lb_dummy_queue_release(dev);
	mlx5_txpp_stop(dev);

	return 0;
}

/**
 * Enable traffic flows configured by control plane
 *
 * @param dev
 *   Pointer to Ethernet device private data.
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
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	};
	struct rte_flow_item_eth ipv6_multi_spec = {
		.dst.addr_bytes = "\x33\x33\x00\x00\x00\x00",
	};
	struct rte_flow_item_eth ipv6_multi_mask = {
		.dst.addr_bytes = "\xff\xff\x00\x00\x00\x00",
	};
	struct rte_flow_item_eth unicast = {
		.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	};
	struct rte_flow_item_eth unicast_mask = {
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	};
	const unsigned int vlan_filter_n = priv->vlan_filter_n;
	const struct rte_ether_addr cmp = {
		.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	};
	unsigned int i;
	unsigned int j;
	int ret;

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
		if (txq_ctrl->type == MLX5_TXQ_TYPE_HAIRPIN &&
		    txq_ctrl->hairpin_conf.tx_explicit == 0 &&
		    txq_ctrl->hairpin_conf.peers[0].port ==
		    priv->dev_data->port_id) {
			ret = mlx5_ctrl_flow_source_queue(dev, i);
			if (ret) {
				mlx5_txq_release(dev, i);
				goto error;
			}
		}
		mlx5_txq_release(dev, i);
	}
	if (priv->config.dv_esw_en && !priv->config.vf) {
		if (mlx5_flow_create_esw_table_zero_flow(dev))
			priv->fdb_def_rule = 1;
		else
			DRV_LOG(INFO, "port %u FDB default rule cannot be"
				" configured - only Eswitch group 0 flows are"
				" supported.", dev->data->port_id);
	}
	if (!priv->config.lacp_by_user && priv->pf_bond >= 0) {
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
			.dst.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.type = 0,
		};

		ret = mlx5_ctrl_flow(dev, &promisc, &promisc);
		if (ret)
			goto error;
	}
	if (dev->data->all_multicast) {
		struct rte_flow_item_eth multicast = {
			.dst.addr_bytes = "\x01\x00\x00\x00\x00\x00",
			.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.type = 0,
		};

		ret = mlx5_ctrl_flow(dev, &multicast, &multicast);
		if (ret)
			goto error;
	} else {
		/* Add broadcast/multicast flows. */
		for (i = 0; i != vlan_filter_n; ++i) {
			uint16_t vlan = priv->vlan_filter[i];

			struct rte_flow_item_vlan vlan_spec = {
				.tci = rte_cpu_to_be_16(vlan),
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
			if (ret)
				goto error;
		}
	}
	/* Add MAC address flows. */
	for (i = 0; i != MLX5_MAX_MAC_ADDRESSES; ++i) {
		struct rte_ether_addr *mac = &dev->data->mac_addrs[i];

		if (!memcmp(mac, &cmp, sizeof(*mac)))
			continue;
		memcpy(&unicast.dst.addr_bytes,
		       mac->addr_bytes,
		       RTE_ETHER_ADDR_LEN);
		for (j = 0; j != vlan_filter_n; ++j) {
			uint16_t vlan = priv->vlan_filter[j];

			struct rte_flow_item_vlan vlan_spec = {
				.tci = rte_cpu_to_be_16(vlan),
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
	mlx5_flow_list_flush(dev, &priv->ctrl_flows, false);
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
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_flow_list_flush(dev, &priv->ctrl_flows, false);
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
		return mlx5_traffic_enable(dev);
	}
	return 0;
}
