/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/queue.h>

#include "mlx5_autoconf.h"

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_common.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_common_mr.h>
#include <mlx5_rxtx.h>
#include <mlx5_verbs.h>
#include <mlx5_utils.h>
#include <mlx5_malloc.h>

/**
 * Register mr. Given protection domain pointer, pointer to addr and length
 * register the memory region.
 *
 * @param[in] pd
 *   Pointer to protection domain context.
 * @param[in] addr
 *   Pointer to memory start address.
 * @param[in] length
 *   Length of the memory to register.
 * @param[out] pmd_mr
 *   pmd_mr struct set with lkey, address, length and pointer to mr object
 *
 * @return
 *   0 on successful registration, -1 otherwise
 */
static int
mlx5_reg_mr(void *pd, void *addr, size_t length,
		 struct mlx5_pmd_mr *pmd_mr)
{
	return mlx5_common_verbs_reg_mr(pd, addr, length, pmd_mr);
}

/**
 * Deregister mr. Given the mlx5 pmd MR - deregister the MR
 *
 * @param[in] pmd_mr
 *   pmd_mr struct set with lkey, address, length and pointer to mr object
 *
 */
static void
mlx5_dereg_mr(struct mlx5_pmd_mr *pmd_mr)
{
	mlx5_common_verbs_dereg_mr(pmd_mr);
}

/* verbs operations. */
const struct mlx5_verbs_ops mlx5_verbs_ops = {
	.reg_mr = mlx5_reg_mr,
	.dereg_mr = mlx5_dereg_mr,
};

/**
 * Modify Rx WQ vlan stripping offload
 *
 * @param rxq_obj
 *   Rx queue object.
 *
 * @return 0 on success, non-0 otherwise
 */
static int
mlx5_rxq_obj_modify_wq_vlan_strip(struct mlx5_rxq_obj *rxq_obj, int on)
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

	return mlx5_glue->modify_wq(rxq_obj->wq, &mod);
}

/**
 * Modifies the attributes for the specified WQ.
 *
 * @param rxq_obj
 *   Verbs Rx queue object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_ibv_modify_wq(struct mlx5_rxq_obj *rxq_obj, bool is_start)
{
	struct ibv_wq_attr mod = {
		.attr_mask = IBV_WQ_ATTR_STATE,
		.wq_state = is_start ? IBV_WQS_RDY : IBV_WQS_RESET,
	};

	return mlx5_glue->modify_wq(rxq_obj->wq, &mod);
}

/**
 * Create a CQ Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array.
 *
 * @return
 *   The Verbs CQ object initialized, NULL otherwise and rte_errno is set.
 */
static struct ibv_cq *
mlx5_rxq_ibv_cq_create(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
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
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
		cq_attr.mlx5.cqe_comp_res_format =
				mlx5_rxq_mprq_enabled(rxq_data) ?
				MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX :
				MLX5DV_CQE_RES_FORMAT_HASH;
#else
		cq_attr.mlx5.cqe_comp_res_format = MLX5DV_CQE_RES_FORMAT_HASH;
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
			dev->data->port_id);
	}
#ifdef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
	if (priv->config.cqe_pad) {
		cq_attr.mlx5.comp_mask |= MLX5DV_CQ_INIT_ATTR_MASK_FLAGS;
		cq_attr.mlx5.flags |= MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD;
	}
#endif
	return mlx5_glue->cq_ex_to_cq(mlx5_glue->dv_create_cq(priv->sh->ctx,
							      &cq_attr.ibv,
							      &cq_attr.mlx5));
}

/**
 * Create a WQ Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array.
 *
 * @return
 *   The Verbs WQ object initialized, NULL otherwise and rte_errno is set.
 */
static struct ibv_wq *
mlx5_rxq_ibv_wq_create(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
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
		.pd = priv->sh->pd,
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
			.single_stride_log_num_of_bytes = rxq_data->strd_sz_n,
			.single_wqe_log_num_of_strides = rxq_data->strd_num_n,
			.two_byte_shift_en = MLX5_MPRQ_TWO_BYTE_SHIFT,
		};
	}
	rxq_obj->wq = mlx5_glue->dv_create_wq(priv->sh->ctx, &wq_attr.ibv,
					      &wq_attr.mlx5);
#else
	rxq_obj->wq = mlx5_glue->create_wq(priv->sh->ctx, &wq_attr.ibv);
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
				dev->data->port_id, idx,
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
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rxq_ibv_obj_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_rxq_obj *tmpl = rxq_ctrl->obj;
	struct mlx5dv_cq cq_info;
	struct mlx5dv_rwq rwq;
	int ret = 0;
	struct mlx5dv_obj obj;

	MLX5_ASSERT(rxq_data);
	MLX5_ASSERT(tmpl);
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_RX_QUEUE;
	priv->verbs_alloc_ctx.obj = rxq_ctrl;
	tmpl->type = MLX5_RXQ_OBJ_TYPE_IBV;
	tmpl->rxq_ctrl = rxq_ctrl;
	if (rxq_ctrl->irq) {
		tmpl->ibv_channel =
				mlx5_glue->create_comp_channel(priv->sh->ctx);
		if (!tmpl->ibv_channel) {
			DRV_LOG(ERR, "Port %u: comp channel creation failure.",
				dev->data->port_id);
			rte_errno = ENOMEM;
			goto error;
		}
		tmpl->fd = ((struct ibv_comp_channel *)(tmpl->ibv_channel))->fd;
	}
	/* Create CQ using Verbs API. */
	tmpl->ibv_cq = mlx5_rxq_ibv_cq_create(dev, idx);
	if (!tmpl->ibv_cq) {
		DRV_LOG(ERR, "Port %u Rx queue %u CQ creation failure.",
			dev->data->port_id, idx);
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
			dev->data->port_id, RTE_CACHE_LINE_SIZE);
		rte_errno = EINVAL;
		goto error;
	}
	/* Fill the rings. */
	rxq_data->cqe_n = log2above(cq_info.cqe_cnt);
	rxq_data->cq_db = cq_info.dbrec;
	rxq_data->cqes = (volatile struct mlx5_cqe (*)[])(uintptr_t)cq_info.buf;
	rxq_data->cq_uar = cq_info.cq_uar;
	rxq_data->cqn = cq_info.cqn;
	/* Create WQ (RQ) using Verbs API. */
	tmpl->wq = mlx5_rxq_ibv_wq_create(dev, idx);
	if (!tmpl->wq) {
		DRV_LOG(ERR, "Port %u Rx queue %u WQ creation failure.",
			dev->data->port_id, idx);
		rte_errno = ENOMEM;
		goto error;
	}
	/* Change queue state to ready. */
	ret = mlx5_ibv_modify_wq(tmpl, true);
	if (ret) {
		DRV_LOG(ERR,
			"Port %u Rx queue %u WQ state to IBV_WQS_RDY failed.",
			dev->data->port_id, idx);
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
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_NONE;
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STARTED;
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
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_NONE;
	return -rte_errno;
}

/**
 * Release an Rx verbs queue object.
 *
 * @param rxq_obj
 *   Verbs Rx queue object.
 */
static void
mlx5_rxq_ibv_obj_release(struct mlx5_rxq_obj *rxq_obj)
{
	MLX5_ASSERT(rxq_obj);
	MLX5_ASSERT(rxq_obj->wq);
	MLX5_ASSERT(rxq_obj->ibv_cq);
	claim_zero(mlx5_glue->destroy_wq(rxq_obj->wq));
	claim_zero(mlx5_glue->destroy_cq(rxq_obj->ibv_cq));
	if (rxq_obj->ibv_channel)
		claim_zero(mlx5_glue->destroy_comp_channel
							(rxq_obj->ibv_channel));
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
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[ind_tbl->queues[i]];
		struct mlx5_rxq_ctrl *rxq_ctrl =
				container_of(rxq, struct mlx5_rxq_ctrl, rxq);

		wq[i] = rxq_ctrl->obj->wq;
	}
	MLX5_ASSERT(i > 0);
	/* Finalise indirection table. */
	for (j = 0; i != (unsigned int)(1 << log_n); ++j, ++i)
		wq[i] = wq[j];
	ind_tbl->ind_table = mlx5_glue->create_rwq_ind_table(priv->sh->ctx,
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
			(priv->sh->ctx,
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
				.pd = priv->sh->pd,
			  },
			  &qp_init_attr);
#else
	qp = mlx5_glue->create_qp_ex
			(priv->sh->ctx,
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
				.pd = priv->sh->pd,
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
	struct mlx5_rxq_obj *rxq = priv->drop_queue.rxq;

	if (rxq->wq)
		claim_zero(mlx5_glue->destroy_wq(rxq->wq));
	if (rxq->ibv_cq)
		claim_zero(mlx5_glue->destroy_cq(rxq->ibv_cq));
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
	struct ibv_context *ctx = priv->sh->ctx;
	struct mlx5_rxq_obj *rxq = priv->drop_queue.rxq;

	if (rxq)
		return 0;
	rxq = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rxq), 0, SOCKET_ID_ANY);
	if (!rxq) {
		DEBUG("Port %u cannot allocate drop Rx queue memory.",
		      dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->drop_queue.rxq = rxq;
	rxq->ibv_cq = mlx5_glue->create_cq(ctx, 1, NULL, NULL, 0);
	if (!rxq->ibv_cq) {
		DEBUG("Port %u cannot allocate CQ for drop queue.",
		      dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	rxq->wq = mlx5_glue->create_wq(ctx, &(struct ibv_wq_init_attr){
						    .wq_type = IBV_WQT_RQ,
						    .max_wr = 1,
						    .max_sge = 1,
						    .pd = priv->sh->pd,
						    .cq = rxq->ibv_cq,
					      });
	if (!rxq->wq) {
		DEBUG("Port %u cannot allocate WQ for drop queue.",
		      dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	priv->drop_queue.rxq = rxq;
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
	rxq = priv->drop_queue.rxq;
	ind_tbl = mlx5_glue->create_rwq_ind_table
				(priv->sh->ctx,
				 &(struct ibv_rwq_ind_table_init_attr){
					.log_ind_tbl_size = 0,
					.ind_tbl = (struct ibv_wq **)&rxq->wq,
					.comp_mask = 0,
				 });
	if (!ind_tbl) {
		DEBUG("Port %u cannot allocate indirection table for drop"
		      " queue.", dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
	hrxq->qp = mlx5_glue->create_qp_ex(priv->sh->ctx,
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
			.pd = priv->sh->pd
		 });
	if (!hrxq->qp) {
		DEBUG("Port %u cannot allocate QP for drop queue.",
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
};
