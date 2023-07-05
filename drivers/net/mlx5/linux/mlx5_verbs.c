/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/queue.h>

#include "mlx5_autoconf.h"

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <ethdev_driver.h>
#include <rte_common.h>
#include <rte_eal_paging.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_common_mr.h>
#include <mlx5_verbs.h>
#include <mlx5_rx.h>
#include <mlx5_tx.h>
#include <mlx5_utils.h>
#include <mlx5_malloc.h>

/**
 * Modify Rx WQ vlan stripping offload
 *
 * @param rxq
 *   Rx queue.
 *
 * @return 0 on success, non-0 otherwise
 */
static int
mlx5_rxq_obj_modify_wq_vlan_strip(struct mlx5_rxq_priv *rxq, int on)
{
	uint16_t vlan_offloads =
		(on ? IBV_WQ_FLAGS_CVLAN_STRIPPING : 0) |
		0;
	struct ibv_wq_attr mod;
	mod = (struct ibv_wq_attr){
		.attr_mask = IBV_WQ_ATTR_FLAGS,
		.flags_mask = IBV_WQ_FLAGS_CVLAN_STRIPPING,
		.flags = vlan_offloads,
	};

	return mlx5_glue->modify_wq(rxq->ctrl->obj->wq, &mod);
}

/**
 * Modifies the attributes for the specified WQ.
 *
 * @param rxq
 *   Verbs Rx queue.
 * @param type
 *   Type of change queue state.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_ibv_modify_wq(struct mlx5_rxq_priv *rxq, uint8_t type)
{
	struct ibv_wq_attr mod = {
		.attr_mask = IBV_WQ_ATTR_STATE,
		.wq_state = (enum ibv_wq_state)type,
	};

	return mlx5_glue->modify_wq(rxq->ctrl->obj->wq, &mod);
}

/**
 * Modify QP using Verbs API.
 *
 * @param txq_obj
 *   Verbs Tx queue object.
 * @param type
 *   Type of change queue state.
 * @param dev_port
 *   IB device port number.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_ibv_modify_qp(struct mlx5_txq_obj *obj, enum mlx5_txq_modify_type type,
		   uint8_t dev_port)
{
	struct ibv_qp_attr mod = {
		.qp_state = IBV_QPS_RESET,
		.port_num = dev_port,
	};
	int ret;

	if (type != MLX5_TXQ_MOD_RST2RDY) {
		ret = mlx5_glue->modify_qp(obj->qp, &mod, IBV_QP_STATE);
		if (ret) {
			DRV_LOG(ERR, "Cannot change Tx QP state to RESET %s",
				strerror(errno));
			rte_errno = errno;
			return ret;
		}
		if (type == MLX5_TXQ_MOD_RDY2RST)
			return 0;
	}
	mod.qp_state = IBV_QPS_INIT;
	ret = mlx5_glue->modify_qp(obj->qp, &mod, IBV_QP_STATE | IBV_QP_PORT);
	if (ret) {
		DRV_LOG(ERR, "Cannot change Tx QP state to INIT %s",
			strerror(errno));
		rte_errno = errno;
		return ret;
	}
	mod.qp_state = IBV_QPS_RTR;
	ret = mlx5_glue->modify_qp(obj->qp, &mod, IBV_QP_STATE);
	if (ret) {
		DRV_LOG(ERR, "Cannot change Tx QP state to RTR %s",
			strerror(errno));
		rte_errno = errno;
		return ret;
	}
	mod.qp_state = IBV_QPS_RTS;
	ret = mlx5_glue->modify_qp(obj->qp, &mod, IBV_QP_STATE);
	if (ret) {
		DRV_LOG(ERR, "Cannot change Tx QP state to RTS %s",
			strerror(errno));
		rte_errno = errno;
		return ret;
	}
	return 0;
}

/**
 * Create a CQ Verbs object.
 *
 * @param rxq
 *   Pointer to Rx queue.
 *
 * @return
 *   The Verbs CQ object initialized, NULL otherwise and rte_errno is set.
 */
static struct ibv_cq *
mlx5_rxq_ibv_cq_create(struct mlx5_rxq_priv *rxq)
{
	struct mlx5_priv *priv = rxq->priv;
	struct mlx5_rxq_ctrl *rxq_ctrl = rxq->ctrl;
	struct mlx5_rxq_data *rxq_data = &rxq_ctrl->rxq;
	struct mlx5_rxq_obj *rxq_obj = rxq_ctrl->obj;
	unsigned int cqe_n = mlx5_rxq_cqe_num(rxq_data);
	struct {
		struct ibv_cq_init_attr_ex ibv;
		struct mlx5dv_cq_init_attr mlx5;
	} cq_attr;

	cq_attr.ibv = (struct ibv_cq_init_attr_ex){
		.cqe = cqe_n,
		.channel = rxq_obj->ibv_channel,
		.comp_mask = 0,
	};
	cq_attr.mlx5 = (struct mlx5dv_cq_init_attr){
		.comp_mask = 0,
	};
	if (priv->config.cqe_comp && !rxq_data->hw_timestamp) {
		cq_attr.mlx5.comp_mask |=
				MLX5DV_CQ_INIT_ATTR_MASK_COMPRESSED_CQE;
		rxq_data->byte_mask = UINT32_MAX;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
		if (mlx5_rxq_mprq_enabled(rxq_data)) {
			cq_attr.mlx5.cqe_comp_res_format =
					MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX;
			rxq_data->mcqe_format =
					MLX5_CQE_RESP_FORMAT_CSUM_STRIDX;
		} else {
			cq_attr.mlx5.cqe_comp_res_format =
					MLX5DV_CQE_RES_FORMAT_HASH;
			rxq_data->mcqe_format =
					MLX5_CQE_RESP_FORMAT_HASH;
		}
#else
		cq_attr.mlx5.cqe_comp_res_format = MLX5DV_CQE_RES_FORMAT_HASH;
		rxq_data->mcqe_format = MLX5_CQE_RESP_FORMAT_HASH;
#endif
		/*
		 * For vectorized Rx, it must not be doubled in order to
		 * make cq_ci and rq_ci aligned.
		 */
		if (mlx5_rxq_check_vec_support(rxq_data) < 0)
			cq_attr.ibv.cqe *= 2;
	} else if (priv->config.cqe_comp && rxq_data->hw_timestamp) {
		DRV_LOG(DEBUG,
			"Port %u Rx CQE compression is disabled for HW"
			" timestamp.",
			priv->dev_data->port_id);
	}
#ifdef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
	if (RTE_CACHE_LINE_SIZE == 128) {
		cq_attr.mlx5.comp_mask |= MLX5DV_CQ_INIT_ATTR_MASK_FLAGS;
		cq_attr.mlx5.flags |= MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD;
	}
#endif
	return mlx5_glue->cq_ex_to_cq(mlx5_glue->dv_create_cq
							   (priv->sh->cdev->ctx,
							    &cq_attr.ibv,
							    &cq_attr.mlx5));
}

/**
 * Create a WQ Verbs object.
 *
 * @param rxq
 *   Pointer to Rx queue.
 *
 * @return
 *   The Verbs WQ object initialized, NULL otherwise and rte_errno is set.
 */
static struct ibv_wq *
mlx5_rxq_ibv_wq_create(struct mlx5_rxq_priv *rxq)
{
	struct mlx5_priv *priv = rxq->priv;
	struct mlx5_rxq_ctrl *rxq_ctrl = rxq->ctrl;
	struct mlx5_rxq_data *rxq_data = &rxq_ctrl->rxq;
	struct mlx5_rxq_obj *rxq_obj = rxq_ctrl->obj;
	unsigned int wqe_n = 1 << rxq_data->elts_n;
	struct {
		struct ibv_wq_init_attr ibv;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
		struct mlx5dv_wq_init_attr mlx5;
#endif
	} wq_attr;

	wq_attr.ibv = (struct ibv_wq_init_attr){
		.wq_context = NULL, /* Could be useful in the future. */
		.wq_type = IBV_WQT_RQ,
		/* Max number of outstanding WRs. */
		.max_wr = wqe_n >> rxq_data->sges_n,
		/* Max number of scatter/gather elements in a WR. */
		.max_sge = 1 << rxq_data->sges_n,
		.pd = priv->sh->cdev->pd,
		.cq = rxq_obj->ibv_cq,
		.comp_mask = IBV_WQ_FLAGS_CVLAN_STRIPPING | 0,
		.create_flags = (rxq_data->vlan_strip ?
				 IBV_WQ_FLAGS_CVLAN_STRIPPING : 0),
	};
	/* By default, FCS (CRC) is stripped by hardware. */
	if (rxq_data->crc_present) {
		wq_attr.ibv.create_flags |= IBV_WQ_FLAGS_SCATTER_FCS;
		wq_attr.ibv.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
	}
	if (priv->config.hw_padding) {
#if defined(HAVE_IBV_WQ_FLAG_RX_END_PADDING)
		wq_attr.ibv.create_flags |= IBV_WQ_FLAG_RX_END_PADDING;
		wq_attr.ibv.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
#elif defined(HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING)
		wq_attr.ibv.create_flags |= IBV_WQ_FLAGS_PCI_WRITE_END_PADDING;
		wq_attr.ibv.comp_mask |= IBV_WQ_INIT_ATTR_FLAGS;
#endif
	}
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	wq_attr.mlx5 = (struct mlx5dv_wq_init_attr){
		.comp_mask = 0,
	};
	if (mlx5_rxq_mprq_enabled(rxq_data)) {
		struct mlx5dv_striding_rq_init_attr *mprq_attr =
						&wq_attr.mlx5.striding_rq_attrs;

		wq_attr.mlx5.comp_mask |= MLX5DV_WQ_INIT_ATTR_MASK_STRIDING_RQ;
		*mprq_attr = (struct mlx5dv_striding_rq_init_attr){
			.single_stride_log_num_of_bytes = rxq_data->log_strd_sz,
			.single_wqe_log_num_of_strides = rxq_data->log_strd_num,
			.two_byte_shift_en = MLX5_MPRQ_TWO_BYTE_SHIFT,
		};
	}
	rxq_obj->wq = mlx5_glue->dv_create_wq(priv->sh->cdev->ctx, &wq_attr.ibv,
					      &wq_attr.mlx5);
#else
	rxq_obj->wq = mlx5_glue->create_wq(priv->sh->cdev->ctx, &wq_attr.ibv);
#endif
	if (rxq_obj->wq) {
		/*
		 * Make sure number of WRs*SGEs match expectations since a queue
		 * cannot allocate more than "desc" buffers.
		 */
		if (wq_attr.ibv.max_wr != (wqe_n >> rxq_data->sges_n) ||
		    wq_attr.ibv.max_sge != (1u << rxq_data->sges_n)) {
			DRV_LOG(ERR,
				"Port %u Rx queue %u requested %u*%u but got"
				" %u*%u WRs*SGEs.",
				priv->dev_data->port_id, rxq->idx,
				wqe_n >> rxq_data->sges_n,
				(1 << rxq_data->sges_n),
				wq_attr.ibv.max_wr, wq_attr.ibv.max_sge);
			claim_zero(mlx5_glue->destroy_wq(rxq_obj->wq));
			rxq_obj->wq = NULL;
			rte_errno = EINVAL;
		}
	}
	return rxq_obj->wq;
}

/**
 * Create the Rx queue Verbs object.
 *
 * @param rxq
 *   Pointer to Rx queue.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rxq_ibv_obj_new(struct mlx5_rxq_priv *rxq)
{
	uint16_t idx = rxq->idx;
	struct mlx5_priv *priv = rxq->priv;
	uint16_t port_id = priv->dev_data->port_id;
	struct mlx5_rxq_ctrl *rxq_ctrl = rxq->ctrl;
	struct mlx5_rxq_data *rxq_data = &rxq_ctrl->rxq;
	struct mlx5_rxq_obj *tmpl = rxq_ctrl->obj;
	struct mlx5dv_cq cq_info;
	struct mlx5dv_rwq rwq;
	int ret = 0;
	struct mlx5dv_obj obj;

	MLX5_ASSERT(rxq_data);
	MLX5_ASSERT(tmpl);
	tmpl->rxq_ctrl = rxq_ctrl;
	if (rxq_ctrl->irq) {
		tmpl->ibv_channel =
			mlx5_glue->create_comp_channel(priv->sh->cdev->ctx);
		if (!tmpl->ibv_channel) {
			DRV_LOG(ERR, "Port %u: comp channel creation failure.",
				port_id);
			rte_errno = ENOMEM;
			goto error;
		}
		tmpl->fd = ((struct ibv_comp_channel *)(tmpl->ibv_channel))->fd;
	}
	/* Create CQ using Verbs API. */
	tmpl->ibv_cq = mlx5_rxq_ibv_cq_create(rxq);
	if (!tmpl->ibv_cq) {
		DRV_LOG(ERR, "Port %u Rx queue %u CQ creation failure.",
			port_id, idx);
		rte_errno = ENOMEM;
		goto error;
	}
	obj.cq.in = tmpl->ibv_cq;
	obj.cq.out = &cq_info;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_CQ);
	if (ret) {
		rte_errno = ret;
		goto error;
	}
	if (cq_info.cqe_size != RTE_CACHE_LINE_SIZE) {
		DRV_LOG(ERR,
			"Port %u wrong MLX5_CQE_SIZE environment "
			"variable value: it should be set to %u.",
			port_id, RTE_CACHE_LINE_SIZE);
		rte_errno = EINVAL;
		goto error;
	}
	/* Fill the rings. */
	rxq_data->cqe_n = log2above(cq_info.cqe_cnt);
	rxq_data->cq_db = cq_info.dbrec;
	rxq_data->cqes = (volatile struct mlx5_cqe (*)[])(uintptr_t)cq_info.buf;
	rxq_data->uar_data.db = RTE_PTR_ADD(cq_info.cq_uar, MLX5_CQ_DOORBELL);
#ifndef RTE_ARCH_64
	rxq_data->uar_data.sl_p = &priv->sh->uar_lock_cq;
#endif
	rxq_data->cqn = cq_info.cqn;
	/* Create WQ (RQ) using Verbs API. */
	tmpl->wq = mlx5_rxq_ibv_wq_create(rxq);
	if (!tmpl->wq) {
		DRV_LOG(ERR, "Port %u Rx queue %u WQ creation failure.",
			port_id, idx);
		rte_errno = ENOMEM;
		goto error;
	}
	/* Change queue state to ready. */
	ret = mlx5_ibv_modify_wq(rxq, IBV_WQS_RDY);
	if (ret) {
		DRV_LOG(ERR,
			"Port %u Rx queue %u WQ state to IBV_WQS_RDY failed.",
			port_id, idx);
		rte_errno = ret;
		goto error;
	}
	obj.rwq.in = tmpl->wq;
	obj.rwq.out = &rwq;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_RWQ);
	if (ret) {
		rte_errno = ret;
		goto error;
	}
	rxq_data->wqes = rwq.buf;
	rxq_data->rq_db = rwq.dbrec;
	rxq_data->cq_arm_sn = 0;
	mlx5_rxq_initialize(rxq_data);
	rxq_data->cq_ci = 0;
	priv->dev_data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STARTED;
	rxq_ctrl->wqn = ((struct ibv_wq *)(tmpl->wq))->wq_num;
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (tmpl->wq)
		claim_zero(mlx5_glue->destroy_wq(tmpl->wq));
	if (tmpl->ibv_cq)
		claim_zero(mlx5_glue->destroy_cq(tmpl->ibv_cq));
	if (tmpl->ibv_channel)
		claim_zero(mlx5_glue->destroy_comp_channel(tmpl->ibv_channel));
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Release an Rx verbs queue object.
 *
 * @param rxq
 *   Pointer to Rx queue.
 */
static void
mlx5_rxq_ibv_obj_release(struct mlx5_rxq_priv *rxq)
{
	struct mlx5_rxq_obj *rxq_obj = rxq->ctrl->obj;

	if (rxq_obj == NULL || rxq_obj->wq == NULL)
		return;
	claim_zero(mlx5_glue->destroy_wq(rxq_obj->wq));
	rxq_obj->wq = NULL;
	MLX5_ASSERT(rxq_obj->ibv_cq);
	claim_zero(mlx5_glue->destroy_cq(rxq_obj->ibv_cq));
	if (rxq_obj->ibv_channel)
		claim_zero(mlx5_glue->destroy_comp_channel
							(rxq_obj->ibv_channel));
	rxq->ctrl->started = false;
}

/**
 * Get event for an Rx verbs queue object.
 *
 * @param rxq_obj
 *   Verbs Rx queue object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rx_ibv_get_event(struct mlx5_rxq_obj *rxq_obj)
{
	struct ibv_cq *ev_cq;
	void *ev_ctx;
	int ret = mlx5_glue->get_cq_event(rxq_obj->ibv_channel,
					  &ev_cq, &ev_ctx);

	if (ret < 0 || ev_cq != rxq_obj->ibv_cq)
		goto exit;
	mlx5_glue->ack_cq_events(rxq_obj->ibv_cq, 1);
	return 0;
exit:
	if (ret < 0)
		rte_errno = errno;
	else
		rte_errno = EINVAL;
	return -rte_errno;
}

/**
 * Creates a receive work queue as a filed of indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param log_n
 *   Log of number of queues in the array.
 * @param ind_tbl
 *   Verbs indirection table object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_ibv_ind_table_new(struct rte_eth_dev *dev, const unsigned int log_n,
		       struct mlx5_ind_table_obj *ind_tbl)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_wq *wq[1 << log_n];
	unsigned int i, j;

	MLX5_ASSERT(ind_tbl);
	for (i = 0; i != ind_tbl->queues_n; ++i) {
		struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev,
							 ind_tbl->queues[i]);

		wq[i] = rxq->ctrl->obj->wq;
	}
	MLX5_ASSERT(i > 0);
	/* Finalise indirection table. */
	for (j = 0; i != (unsigned int)(1 << log_n); ++j, ++i)
		wq[i] = wq[j];
	ind_tbl->ind_table = mlx5_glue->create_rwq_ind_table
					(priv->sh->cdev->ctx,
					 &(struct ibv_rwq_ind_table_init_attr){
						 .log_ind_tbl_size = log_n,
						 .ind_tbl = wq,
						 .comp_mask = 0,
					 });
	if (!ind_tbl->ind_table) {
		rte_errno = errno;
		return -rte_errno;
	}
	return 0;
}

/**
 * Destroys the specified Indirection Table.
 *
 * @param ind_table
 *   Indirection table to release.
 */
static void
mlx5_ibv_ind_table_destroy(struct mlx5_ind_table_obj *ind_tbl)
{
	claim_zero(mlx5_glue->destroy_rwq_ind_table(ind_tbl->ind_table));
}

/**
 * Create an Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param hrxq
 *   Pointer to Rx Hash queue.
 * @param tunnel
 *   Tunnel type.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_ibv_hrxq_new(struct rte_eth_dev *dev, struct mlx5_hrxq *hrxq,
		  int tunnel __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_qp *qp = NULL;
	struct mlx5_ind_table_obj *ind_tbl = hrxq->ind_table;
	const uint8_t *rss_key = hrxq->rss_key;
	uint64_t hash_fields = hrxq->hash_fields;
	int err;
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	struct mlx5dv_qp_init_attr qp_init_attr;

	memset(&qp_init_attr, 0, sizeof(qp_init_attr));
	if (tunnel) {
		qp_init_attr.comp_mask =
				       MLX5DV_QP_INIT_ATTR_MASK_QP_CREATE_FLAGS;
		qp_init_attr.create_flags = MLX5DV_QP_CREATE_TUNNEL_OFFLOADS;
	}
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	if (dev->data->dev_conf.lpbk_mode) {
		/* Allow packet sent from NIC loop back w/o source MAC check. */
		qp_init_attr.comp_mask |=
				MLX5DV_QP_INIT_ATTR_MASK_QP_CREATE_FLAGS;
		qp_init_attr.create_flags |=
				MLX5DV_QP_CREATE_TIR_ALLOW_SELF_LOOPBACK_UC;
	}
#endif
	qp = mlx5_glue->dv_create_qp
			(priv->sh->cdev->ctx,
			 &(struct ibv_qp_init_attr_ex){
				.qp_type = IBV_QPT_RAW_PACKET,
				.comp_mask =
					IBV_QP_INIT_ATTR_PD |
					IBV_QP_INIT_ATTR_IND_TABLE |
					IBV_QP_INIT_ATTR_RX_HASH,
				.rx_hash_conf = (struct ibv_rx_hash_conf){
					.rx_hash_function =
						IBV_RX_HASH_FUNC_TOEPLITZ,
					.rx_hash_key_len = hrxq->rss_key_len,
					.rx_hash_key =
						(void *)(uintptr_t)rss_key,
					.rx_hash_fields_mask = hash_fields,
				},
				.rwq_ind_tbl = ind_tbl->ind_table,
				.pd = priv->sh->cdev->pd,
			  },
			  &qp_init_attr);
#else
	qp = mlx5_glue->create_qp_ex
			(priv->sh->cdev->ctx,
			 &(struct ibv_qp_init_attr_ex){
				.qp_type = IBV_QPT_RAW_PACKET,
				.comp_mask =
					IBV_QP_INIT_ATTR_PD |
					IBV_QP_INIT_ATTR_IND_TABLE |
					IBV_QP_INIT_ATTR_RX_HASH,
				.rx_hash_conf = (struct ibv_rx_hash_conf){
					.rx_hash_function =
						IBV_RX_HASH_FUNC_TOEPLITZ,
					.rx_hash_key_len = hrxq->rss_key_len,
					.rx_hash_key =
						(void *)(uintptr_t)rss_key,
					.rx_hash_fields_mask = hash_fields,
				},
				.rwq_ind_tbl = ind_tbl->ind_table,
				.pd = priv->sh->cdev->pd,
			 });
#endif
	if (!qp) {
		rte_errno = errno;
		goto error;
	}
	hrxq->qp = qp;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	hrxq->action = mlx5_glue->dv_create_flow_action_dest_ibv_qp(hrxq->qp);
	if (!hrxq->action) {
		rte_errno = errno;
		goto error;
	}
#endif
	return 0;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	if (qp)
		claim_zero(mlx5_glue->destroy_qp(qp));
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Destroy a Verbs queue pair.
 *
 * @param hrxq
 *   Hash Rx queue to release its qp.
 */
static void
mlx5_ibv_qp_destroy(struct mlx5_hrxq *hrxq)
{
	claim_zero(mlx5_glue->destroy_qp(hrxq->qp));
}

/**
 * Release a drop Rx queue Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_rxq_ibv_obj_drop_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_priv *rxq = priv->drop_queue.rxq;
	struct mlx5_rxq_obj *rxq_obj;

	if (rxq == NULL)
		return;
	if (rxq->ctrl == NULL)
		goto free_priv;
	rxq_obj = rxq->ctrl->obj;
	if (rxq_obj == NULL)
		goto free_ctrl;
	if (rxq_obj->wq)
		claim_zero(mlx5_glue->destroy_wq(rxq_obj->wq));
	if (rxq_obj->ibv_cq)
		claim_zero(mlx5_glue->destroy_cq(rxq_obj->ibv_cq));
	mlx5_free(rxq_obj);
free_ctrl:
	mlx5_free(rxq->ctrl);
free_priv:
	mlx5_free(rxq);
	priv->drop_queue.rxq = NULL;
}

/**
 * Create a drop Rx queue Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rxq_ibv_obj_drop_create(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct ibv_context *ctx = priv->sh->cdev->ctx;
	struct mlx5_rxq_priv *rxq = priv->drop_queue.rxq;
	struct mlx5_rxq_ctrl *rxq_ctrl = NULL;
	struct mlx5_rxq_obj *rxq_obj = NULL;

	if (rxq != NULL)
		return 0;
	rxq = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rxq), 0, SOCKET_ID_ANY);
	if (rxq == NULL) {
		DRV_LOG(DEBUG, "Port %u cannot allocate drop Rx queue memory.",
		      dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->drop_queue.rxq = rxq;
	rxq_ctrl = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rxq_ctrl), 0,
			       SOCKET_ID_ANY);
	if (rxq_ctrl == NULL) {
		DRV_LOG(DEBUG, "Port %u cannot allocate drop Rx queue control memory.",
		      dev->data->port_id);
		rte_errno = ENOMEM;
		goto error;
	}
	rxq->ctrl = rxq_ctrl;
	rxq_obj = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rxq_obj), 0,
			      SOCKET_ID_ANY);
	if (rxq_obj == NULL) {
		DRV_LOG(DEBUG, "Port %u cannot allocate drop Rx queue memory.",
		      dev->data->port_id);
		rte_errno = ENOMEM;
		goto error;
	}
	rxq_ctrl->obj = rxq_obj;
	rxq_obj->ibv_cq = mlx5_glue->create_cq(ctx, 1, NULL, NULL, 0);
	if (!rxq_obj->ibv_cq) {
		DRV_LOG(DEBUG, "Port %u cannot allocate CQ for drop queue.",
		      dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	rxq_obj->wq = mlx5_glue->create_wq(ctx, &(struct ibv_wq_init_attr){
						    .wq_type = IBV_WQT_RQ,
						    .max_wr = 1,
						    .max_sge = 1,
						    .pd = priv->sh->cdev->pd,
						    .cq = rxq_obj->ibv_cq,
					      });
	if (!rxq_obj->wq) {
		DRV_LOG(DEBUG, "Port %u cannot allocate WQ for drop queue.",
		      dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	return 0;
error:
	mlx5_rxq_ibv_obj_drop_release(dev);
	return -rte_errno;
}

/**
 * Create a Verbs drop action for Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_ibv_drop_action_create(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq = priv->drop_queue.hrxq;
	struct ibv_rwq_ind_table *ind_tbl = NULL;
	struct mlx5_rxq_obj *rxq;
	int ret;

	MLX5_ASSERT(hrxq && hrxq->ind_table);
	ret = mlx5_rxq_ibv_obj_drop_create(dev);
	if (ret < 0)
		goto error;
	rxq = priv->drop_queue.rxq->ctrl->obj;
	ind_tbl = mlx5_glue->create_rwq_ind_table
				(priv->sh->cdev->ctx,
				 &(struct ibv_rwq_ind_table_init_attr){
					.log_ind_tbl_size = 0,
					.ind_tbl = (struct ibv_wq **)&rxq->wq,
					.comp_mask = 0,
				 });
	if (!ind_tbl) {
		DRV_LOG(DEBUG, "Port %u"
			" cannot allocate indirection table for drop queue.",
			dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	hrxq->qp = mlx5_glue->create_qp_ex(priv->sh->cdev->ctx,
		 &(struct ibv_qp_init_attr_ex){
			.qp_type = IBV_QPT_RAW_PACKET,
			.comp_mask = IBV_QP_INIT_ATTR_PD |
				     IBV_QP_INIT_ATTR_IND_TABLE |
				     IBV_QP_INIT_ATTR_RX_HASH,
			.rx_hash_conf = (struct ibv_rx_hash_conf){
				.rx_hash_function = IBV_RX_HASH_FUNC_TOEPLITZ,
				.rx_hash_key_len = MLX5_RSS_HASH_KEY_LEN,
				.rx_hash_key = rss_hash_default_key,
				.rx_hash_fields_mask = 0,
				},
			.rwq_ind_tbl = ind_tbl,
			.pd = priv->sh->cdev->pd
		 });
	if (!hrxq->qp) {
		DRV_LOG(DEBUG, "Port %u cannot allocate QP for drop queue.",
		      dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	hrxq->action = mlx5_glue->dv_create_flow_action_dest_ibv_qp(hrxq->qp);
	if (!hrxq->action) {
		rte_errno = errno;
		goto error;
	}
#endif
	hrxq->ind_table->ind_table = ind_tbl;
	return 0;
error:
	if (hrxq->qp)
		claim_zero(mlx5_glue->destroy_qp(hrxq->qp));
	if (ind_tbl)
		claim_zero(mlx5_glue->destroy_rwq_ind_table(ind_tbl));
	if (priv->drop_queue.rxq)
		mlx5_rxq_ibv_obj_drop_release(dev);
	return -rte_errno;
}

/**
 * Release a drop hash Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_ibv_drop_action_destroy(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq = priv->drop_queue.hrxq;
	struct ibv_rwq_ind_table *ind_tbl = hrxq->ind_table->ind_table;

#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	claim_zero(mlx5_glue->destroy_flow_action(hrxq->action));
#endif
	claim_zero(mlx5_glue->destroy_qp(hrxq->qp));
	claim_zero(mlx5_glue->destroy_rwq_ind_table(ind_tbl));
	mlx5_rxq_ibv_obj_drop_release(dev);
}

/**
 * Create a QP Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Tx queue array.
 *
 * @return
 *   The QP Verbs object, NULL otherwise and rte_errno is set.
 */
static struct ibv_qp *
mlx5_txq_ibv_qp_create(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
			container_of(txq_data, struct mlx5_txq_ctrl, txq);
	struct ibv_qp *qp_obj = NULL;
	struct ibv_qp_init_attr_ex qp_attr = { 0 };
	const int desc = 1 << txq_data->elts_n;

	MLX5_ASSERT(txq_ctrl->obj->cq);
	/* CQ to be associated with the send queue. */
	qp_attr.send_cq = txq_ctrl->obj->cq;
	/* CQ to be associated with the receive queue. */
	qp_attr.recv_cq = txq_ctrl->obj->cq;
	/* Max number of outstanding WRs. */
	qp_attr.cap.max_send_wr = RTE_MIN(priv->sh->dev_cap.max_qp_wr, desc);
	/*
	 * Max number of scatter/gather elements in a WR, must be 1 to prevent
	 * libmlx5 from trying to affect must be 1 to prevent libmlx5 from
	 * trying to affect too much memory. TX gather is not impacted by the
	 * dev_cap.max_sge limit and will still work properly.
	 */
	qp_attr.cap.max_send_sge = 1;
	qp_attr.qp_type = IBV_QPT_RAW_PACKET,
	/* Do *NOT* enable this, completions events are managed per Tx burst. */
	qp_attr.sq_sig_all = 0;
	qp_attr.pd = priv->sh->cdev->pd;
	qp_attr.comp_mask = IBV_QP_INIT_ATTR_PD;
	if (txq_data->inlen_send)
		qp_attr.cap.max_inline_data = txq_ctrl->max_inline_data;
	if (txq_data->tso_en) {
		qp_attr.max_tso_header = txq_ctrl->max_tso_header;
		qp_attr.comp_mask |= IBV_QP_INIT_ATTR_MAX_TSO_HEADER;
	}
	qp_obj = mlx5_glue->create_qp_ex(priv->sh->cdev->ctx, &qp_attr);
	if (qp_obj == NULL) {
		DRV_LOG(ERR, "Port %u Tx queue %u QP creation failure.",
			dev->data->port_id, idx);
		rte_errno = errno;
	}
	return qp_obj;
}

/**
 * Initialize Tx UAR registers for primary process.
 *
 * @param txq_ctrl
 *   Pointer to Tx queue control structure.
 * @param bf_reg
 *   BlueFlame register from Verbs UAR.
 */
static void
mlx5_txq_ibv_uar_init(struct mlx5_txq_ctrl *txq_ctrl, void *bf_reg)
{
	struct mlx5_priv *priv = txq_ctrl->priv;
	struct mlx5_proc_priv *ppriv = MLX5_PROC_PRIV(PORT_ID(priv));
	const size_t page_size = rte_mem_page_size();
	struct mlx5_txq_data *txq = &txq_ctrl->txq;
	off_t uar_mmap_offset = txq_ctrl->uar_mmap_offset;
#ifndef RTE_ARCH_64
	unsigned int lock_idx;
#endif

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	MLX5_ASSERT(ppriv);
	if (page_size == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
	}
	txq->db_heu = priv->sh->cdev->config.dbnc == MLX5_SQ_DB_HEURISTIC;
	txq->db_nc = mlx5_db_map_type_get(uar_mmap_offset, page_size);
	ppriv->uar_table[txq->idx].db = bf_reg;
#ifndef RTE_ARCH_64
	/* Assign an UAR lock according to UAR page number. */
	lock_idx = (uar_mmap_offset / page_size) & MLX5_UAR_PAGE_NUM_MASK;
	ppriv->uar_table[txq->idx].sl_p = &priv->sh->uar_lock[lock_idx];
#endif
}

/**
 * Create the Tx queue Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Tx queue array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_txq_ibv_obj_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq_data, struct mlx5_txq_ctrl, txq);
	struct mlx5_txq_obj *txq_obj = txq_ctrl->obj;
	unsigned int cqe_n;
	struct mlx5dv_qp qp;
	struct mlx5dv_cq cq_info;
	struct mlx5dv_obj obj;
	const int desc = 1 << txq_data->elts_n;
	int ret = 0;

	MLX5_ASSERT(txq_data);
	MLX5_ASSERT(txq_obj);
	txq_obj->txq_ctrl = txq_ctrl;
	if (mlx5_getenv_int("MLX5_ENABLE_CQE_COMPRESSION")) {
		DRV_LOG(ERR, "Port %u MLX5_ENABLE_CQE_COMPRESSION "
			"must never be set.", dev->data->port_id);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (__rte_trace_point_fp_is_enabled() &&
	    txq_data->offloads & RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP)
		cqe_n = UINT16_MAX / 2 - 1;
	else
		cqe_n = desc / MLX5_TX_COMP_THRESH +
			1 + MLX5_TX_COMP_THRESH_INLINE_DIV;
	txq_obj->cq = mlx5_glue->create_cq(priv->sh->cdev->ctx, cqe_n,
					   NULL, NULL, 0);
	if (txq_obj->cq == NULL) {
		DRV_LOG(ERR, "Port %u Tx queue %u CQ creation failure.",
			dev->data->port_id, idx);
		rte_errno = errno;
		goto error;
	}
	txq_obj->qp = mlx5_txq_ibv_qp_create(dev, idx);
	if (txq_obj->qp == NULL) {
		rte_errno = errno;
		goto error;
	}
	ret = mlx5_ibv_modify_qp(txq_obj, MLX5_TXQ_MOD_RST2RDY,
				 (uint8_t)priv->dev_port);
	if (ret) {
		DRV_LOG(ERR, "Port %u Tx queue %u QP state modifying failed.",
			dev->data->port_id, idx);
		rte_errno = errno;
		goto error;
	}
	qp.comp_mask = MLX5DV_QP_MASK_UAR_MMAP_OFFSET;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	/* If using DevX, need additional mask to read tisn value. */
	if (priv->sh->cdev->config.devx && !priv->sh->tdn)
		qp.comp_mask |= MLX5DV_QP_MASK_RAW_QP_HANDLES;
#endif
	obj.cq.in = txq_obj->cq;
	obj.cq.out = &cq_info;
	obj.qp.in = txq_obj->qp;
	obj.qp.out = &qp;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_CQ | MLX5DV_OBJ_QP);
	if (ret != 0) {
		rte_errno = errno;
		goto error;
	}
	if (cq_info.cqe_size != RTE_CACHE_LINE_SIZE) {
		DRV_LOG(ERR,
			"Port %u wrong MLX5_CQE_SIZE environment variable"
			" value: it should be set to %u.",
			dev->data->port_id, RTE_CACHE_LINE_SIZE);
		rte_errno = EINVAL;
		goto error;
	}
	txq_data->cqe_n = log2above(cq_info.cqe_cnt);
	txq_data->cqe_s = 1 << txq_data->cqe_n;
	txq_data->cqe_m = txq_data->cqe_s - 1;
	txq_data->qp_num_8s = ((struct ibv_qp *)txq_obj->qp)->qp_num << 8;
	txq_data->wqes = qp.sq.buf;
	txq_data->wqe_n = log2above(qp.sq.wqe_cnt);
	txq_data->wqe_s = 1 << txq_data->wqe_n;
	txq_data->wqe_m = txq_data->wqe_s - 1;
	txq_data->wqes_end = txq_data->wqes + txq_data->wqe_s;
	txq_data->qp_db = &qp.dbrec[MLX5_SND_DBR];
	txq_data->cq_db = cq_info.dbrec;
	txq_data->cqes = (volatile struct mlx5_cqe *)cq_info.buf;
	txq_data->cq_ci = 0;
	txq_data->cq_pi = 0;
	txq_data->wqe_ci = 0;
	txq_data->wqe_pi = 0;
	txq_data->wqe_comp = 0;
	txq_data->wqe_thres = txq_data->wqe_s / MLX5_TX_COMP_THRESH_INLINE_DIV;
	txq_data->wait_on_time = !!(!priv->sh->config.tx_pp &&
				 priv->sh->cdev->config.hca_attr.wait_on_time &&
				 txq_data->offloads &
				 RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP);
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	/*
	 * If using DevX need to query and store TIS transport domain value.
	 * This is done once per port.
	 * Will use this value on Rx, when creating matching TIR.
	 */
	if (priv->sh->cdev->config.devx && !priv->sh->tdn) {
		ret = mlx5_devx_cmd_qp_query_tis_td(txq_obj->qp, qp.tisn,
						    &priv->sh->tdn);
		if (ret) {
			DRV_LOG(ERR, "Fail to query port %u Tx queue %u QP TIS "
				"transport domain.", dev->data->port_id, idx);
			rte_errno = EINVAL;
			goto error;
		} else {
			DRV_LOG(DEBUG, "Port %u Tx queue %u TIS number %d "
				"transport domain %d.", dev->data->port_id,
				idx, qp.tisn, priv->sh->tdn);
		}
	}
#endif
	if (qp.comp_mask & MLX5DV_QP_MASK_UAR_MMAP_OFFSET) {
		txq_ctrl->uar_mmap_offset = qp.uar_mmap_offset;
		DRV_LOG(DEBUG, "Port %u: uar_mmap_offset 0x%" PRIx64 ".",
			dev->data->port_id, txq_ctrl->uar_mmap_offset);
	} else {
		DRV_LOG(ERR,
			"Port %u failed to retrieve UAR info, invalid libmlx5.so",
			dev->data->port_id);
		rte_errno = EINVAL;
		goto error;
	}
	mlx5_txq_ibv_uar_init(txq_ctrl, qp.bf.reg);
	dev->data->tx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (txq_obj->cq)
		claim_zero(mlx5_glue->destroy_cq(txq_obj->cq));
	if (txq_obj->qp)
		claim_zero(mlx5_glue->destroy_qp(txq_obj->qp));
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/*
 * Create the dummy QP with minimal resources for loopback.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_rxq_ibv_obj_dummy_lb_create(struct rte_eth_dev *dev)
{
#if defined(HAVE_IBV_DEVICE_TUNNEL_SUPPORT) && defined(HAVE_IBV_FLOW_DV_SUPPORT)
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct ibv_context *ctx = sh->cdev->ctx;
	struct mlx5dv_qp_init_attr qp_init_attr = {0};
	struct {
		struct ibv_cq_init_attr_ex ibv;
		struct mlx5dv_cq_init_attr mlx5;
	} cq_attr = {{0}};

	if (dev->data->dev_conf.lpbk_mode) {
		/* Allow packet sent from NIC loop back w/o source MAC check. */
		qp_init_attr.comp_mask |=
				MLX5DV_QP_INIT_ATTR_MASK_QP_CREATE_FLAGS;
		qp_init_attr.create_flags |=
				MLX5DV_QP_CREATE_TIR_ALLOW_SELF_LOOPBACK_UC;
	} else {
		return 0;
	}
	/* Only need to check refcnt, 0 after "sh" is allocated. */
	if (!!(__atomic_fetch_add(&sh->self_lb.refcnt, 1, __ATOMIC_RELAXED))) {
		MLX5_ASSERT(sh->self_lb.ibv_cq && sh->self_lb.qp);
		priv->lb_used = 1;
		return 0;
	}
	cq_attr.ibv = (struct ibv_cq_init_attr_ex){
		.cqe = 1,
		.channel = NULL,
		.comp_mask = 0,
	};
	cq_attr.mlx5 = (struct mlx5dv_cq_init_attr){
		.comp_mask = 0,
	};
	/* Only CQ is needed, no WQ(RQ) is required in this case. */
	sh->self_lb.ibv_cq = mlx5_glue->cq_ex_to_cq(mlx5_glue->dv_create_cq(ctx,
							&cq_attr.ibv,
							&cq_attr.mlx5));
	if (!sh->self_lb.ibv_cq) {
		DRV_LOG(ERR, "Port %u cannot allocate CQ for loopback.",
			dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	sh->self_lb.qp = mlx5_glue->dv_create_qp(ctx,
				&(struct ibv_qp_init_attr_ex){
					.qp_type = IBV_QPT_RAW_PACKET,
					.comp_mask = IBV_QP_INIT_ATTR_PD,
					.pd = sh->cdev->pd,
					.send_cq = sh->self_lb.ibv_cq,
					.recv_cq = sh->self_lb.ibv_cq,
					.cap.max_recv_wr = 1,
				},
				&qp_init_attr);
	if (!sh->self_lb.qp) {
		DRV_LOG(DEBUG, "Port %u cannot allocate QP for loopback.",
			dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	priv->lb_used = 1;
	return 0;
error:
	if (sh->self_lb.ibv_cq) {
		claim_zero(mlx5_glue->destroy_cq(sh->self_lb.ibv_cq));
		sh->self_lb.ibv_cq = NULL;
	}
	__atomic_fetch_sub(&sh->self_lb.refcnt, 1, __ATOMIC_RELAXED);
	return -rte_errno;
#else
	RTE_SET_USED(dev);
	return 0;
#endif
}

/*
 * Release the dummy queue resources for loopback.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_rxq_ibv_obj_dummy_lb_release(struct rte_eth_dev *dev)
{
#if defined(HAVE_IBV_DEVICE_TUNNEL_SUPPORT) && defined(HAVE_IBV_FLOW_DV_SUPPORT)
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;

	if (!priv->lb_used)
		return;
	MLX5_ASSERT(__atomic_load_n(&sh->self_lb.refcnt, __ATOMIC_RELAXED));
	if (!(__atomic_fetch_sub(&sh->self_lb.refcnt, 1, __ATOMIC_RELAXED) - 1)) {
		if (sh->self_lb.qp) {
			claim_zero(mlx5_glue->destroy_qp(sh->self_lb.qp));
			sh->self_lb.qp = NULL;
		}
		if (sh->self_lb.ibv_cq) {
			claim_zero(mlx5_glue->destroy_cq(sh->self_lb.ibv_cq));
			sh->self_lb.ibv_cq = NULL;
		}
	}
	priv->lb_used = 0;
#else
	RTE_SET_USED(dev);
	return;
#endif
}

/**
 * Release an Tx verbs queue object.
 *
 * @param txq_obj
 *   Verbs Tx queue object..
 */
void
mlx5_txq_ibv_obj_release(struct mlx5_txq_obj *txq_obj)
{
	MLX5_ASSERT(txq_obj);
	claim_zero(mlx5_glue->destroy_qp(txq_obj->qp));
	claim_zero(mlx5_glue->destroy_cq(txq_obj->cq));
}

struct mlx5_obj_ops ibv_obj_ops = {
	.rxq_obj_modify_vlan_strip = mlx5_rxq_obj_modify_wq_vlan_strip,
	.rxq_obj_new = mlx5_rxq_ibv_obj_new,
	.rxq_event_get = mlx5_rx_ibv_get_event,
	.rxq_obj_modify = mlx5_ibv_modify_wq,
	.rxq_obj_release = mlx5_rxq_ibv_obj_release,
	.ind_table_new = mlx5_ibv_ind_table_new,
	.ind_table_destroy = mlx5_ibv_ind_table_destroy,
	.hrxq_new = mlx5_ibv_hrxq_new,
	.hrxq_destroy = mlx5_ibv_qp_destroy,
	.drop_action_create = mlx5_ibv_drop_action_create,
	.drop_action_destroy = mlx5_ibv_drop_action_destroy,
	.txq_obj_new = mlx5_txq_ibv_obj_new,
	.txq_obj_modify = mlx5_ibv_modify_qp,
	.txq_obj_release = mlx5_txq_ibv_obj_release,
	.lb_dummy_queue_create = NULL,
	.lb_dummy_queue_release = NULL,
};
