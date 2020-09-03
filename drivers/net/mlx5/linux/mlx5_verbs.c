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
 * Create an indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param queues
 *   Queues entering in the indirection table.
 * @param queues_n
 *   Number of queues in the array.
 *
 * @return
 *   The Verbs object initialized, NULL otherwise and rte_errno is set.
 */
static struct mlx5_ind_table_obj *
mlx5_ibv_ind_table_obj_new(struct rte_eth_dev *dev, const uint16_t *queues,
			   uint32_t queues_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_obj *ind_tbl;
	const unsigned int wq_n = rte_is_power_of_2(queues_n) ?
				  log2above(queues_n) :
				  log2above(priv->config.ind_table_max_size);
	struct ibv_wq *wq[1 << wq_n];
	unsigned int i = 0, j = 0, k = 0;

	ind_tbl = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*ind_tbl) +
			      queues_n * sizeof(uint16_t), 0, SOCKET_ID_ANY);
	if (!ind_tbl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	for (i = 0; i != queues_n; ++i) {
		struct mlx5_rxq_ctrl *rxq = mlx5_rxq_get(dev, queues[i]);
		if (!rxq)
			goto error;
		wq[i] = rxq->obj->wq;
		ind_tbl->queues[i] = queues[i];
	}
	ind_tbl->queues_n = queues_n;
	/* Finalise indirection table. */
	k = i; /* Retain value of i for use in error case. */
	for (j = 0; k != (unsigned int)(1 << wq_n); ++k, ++j)
		wq[k] = wq[j];
	ind_tbl->ind_table = mlx5_glue->create_rwq_ind_table(priv->sh->ctx,
					&(struct ibv_rwq_ind_table_init_attr){
						.log_ind_tbl_size = wq_n,
						.ind_tbl = wq,
						.comp_mask = 0,
					});
	if (!ind_tbl->ind_table) {
		rte_errno = errno;
		goto error;
	}
	rte_atomic32_inc(&ind_tbl->refcnt);
	LIST_INSERT_HEAD(&priv->ind_tbls, ind_tbl, next);
	return ind_tbl;
error:
	for (j = 0; j < i; j++)
		mlx5_rxq_release(dev, ind_tbl->queues[j]);
	mlx5_free(ind_tbl);
	DEBUG("Port %u cannot create indirection table.", dev->data->port_id);
	return NULL;
}

/**
 * Destroys the specified Indirection Table.
 *
 * @param ind_table
 *   Indirection table to release.
 */
static void
mlx5_ibv_ind_table_obj_destroy(struct mlx5_ind_table_obj *ind_tbl)
{
	claim_zero(mlx5_glue->destroy_rwq_ind_table(ind_tbl->ind_table));
}

/**
 * Create an Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param rss_key
 *   RSS key for the Rx hash queue.
 * @param rss_key_len
 *   RSS key length.
 * @param hash_fields
 *   Verbs protocol hash field to make the RSS on.
 * @param queues
 *   Queues entering in hash queue. In case of empty hash_fields only the
 *   first queue index will be taken for the indirection table.
 * @param queues_n
 *   Number of queues.
 * @param tunnel
 *   Tunnel type.
 *
 * @return
 *   The Verbs object initialized index, 0 otherwise and rte_errno is set.
 */
static uint32_t
mlx5_ibv_hrxq_new(struct rte_eth_dev *dev,
		  const uint8_t *rss_key, uint32_t rss_key_len,
		  uint64_t hash_fields,
		  const uint16_t *queues, uint32_t queues_n,
		  int tunnel __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hrxq *hrxq = NULL;
	uint32_t hrxq_idx = 0;
	struct ibv_qp *qp = NULL;
	struct mlx5_ind_table_obj *ind_tbl;
	int err;

	queues_n = hash_fields ? queues_n : 1;
	ind_tbl = mlx5_ind_table_obj_get(dev, queues, queues_n);
	if (!ind_tbl)
		ind_tbl = priv->obj_ops->ind_table_obj_new(dev, queues,
							   queues_n);
	if (!ind_tbl) {
		rte_errno = ENOMEM;
		return 0;
	}
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
					.rx_hash_key_len = rss_key_len,
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
					.rx_hash_key_len = rss_key_len,
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
	hrxq = mlx5_ipool_zmalloc(priv->sh->ipool[MLX5_IPOOL_HRXQ], &hrxq_idx);
	if (!hrxq)
		goto error;
	hrxq->ind_table = ind_tbl;
	hrxq->qp = qp;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	hrxq->action = mlx5_glue->dv_create_flow_action_dest_ibv_qp(hrxq->qp);
	if (!hrxq->action) {
		rte_errno = errno;
		goto error;
	}
#endif
	hrxq->rss_key_len = rss_key_len;
	hrxq->hash_fields = hash_fields;
	memcpy(hrxq->rss_key, rss_key, rss_key_len);
	rte_atomic32_inc(&hrxq->refcnt);
	ILIST_INSERT(priv->sh->ipool[MLX5_IPOOL_HRXQ], &priv->hrxqs, hrxq_idx,
		     hrxq, next);
	return hrxq_idx;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	mlx5_ind_table_obj_release(dev, ind_tbl);
	if (qp)
		claim_zero(mlx5_glue->destroy_qp(qp));
	if (hrxq)
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_HRXQ], hrxq_idx);
	rte_errno = err; /* Restore rte_errno. */
	return 0;
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

struct mlx5_obj_ops ibv_obj_ops = {
	.rxq_obj_modify_vlan_strip = mlx5_rxq_obj_modify_wq_vlan_strip,
	.rxq_obj_new = mlx5_rxq_ibv_obj_new,
	.rxq_event_get = mlx5_rx_ibv_get_event,
	.rxq_obj_modify = mlx5_ibv_modify_wq,
	.rxq_obj_release = mlx5_rxq_ibv_obj_release,
	.ind_table_obj_new = mlx5_ibv_ind_table_obj_new,
	.ind_table_obj_destroy = mlx5_ibv_ind_table_obj_destroy,
	.hrxq_new = mlx5_ibv_hrxq_new,
	.hrxq_destroy = mlx5_ibv_qp_destroy,
};
