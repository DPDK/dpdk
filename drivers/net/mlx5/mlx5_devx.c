/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_malloc.h>
#include <rte_common.h>
#include <rte_eal_paging.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_common_os.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"
#include "mlx5_devx.h"
#include "mlx5_flow.h"


/**
 * Modify RQ vlan stripping offload
 *
 * @param rxq_obj
 *   Rx queue object.
 *
 * @return
 *   0 on success, non-0 otherwise
 */
static int
mlx5_rxq_obj_modify_rq_vlan_strip(struct mlx5_rxq_obj *rxq_obj, int on)
{
	struct mlx5_devx_modify_rq_attr rq_attr;

	memset(&rq_attr, 0, sizeof(rq_attr));
	rq_attr.rq_state = MLX5_RQC_STATE_RDY;
	rq_attr.state = MLX5_RQC_STATE_RDY;
	rq_attr.vsd = (on ? 0 : 1);
	rq_attr.modify_bitmask = MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_VSD;
	return mlx5_devx_cmd_modify_rq(rxq_obj->rq, &rq_attr);
}

/**
 * Modify RQ using DevX API.
 *
 * @param rxq_obj
 *   DevX Rx queue object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_modify_rq(struct mlx5_rxq_obj *rxq_obj, bool is_start)
{
	struct mlx5_devx_modify_rq_attr rq_attr;

	memset(&rq_attr, 0, sizeof(rq_attr));
	if (is_start) {
		rq_attr.rq_state = MLX5_RQC_STATE_RST;
		rq_attr.state = MLX5_RQC_STATE_RDY;
	} else {
		rq_attr.rq_state = MLX5_RQC_STATE_RDY;
		rq_attr.state = MLX5_RQC_STATE_RST;
	}
	return mlx5_devx_cmd_modify_rq(rxq_obj->rq, &rq_attr);
}

/**
 * Release the resources allocated for an RQ DevX object.
 *
 * @param rxq_ctrl
 *   DevX Rx queue object.
 */
static void
rxq_release_devx_rq_resources(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	struct mlx5_devx_dbr_page *dbr_page = rxq_ctrl->rq_dbrec_page;

	if (rxq_ctrl->rxq.wqes) {
		mlx5_free((void *)(uintptr_t)rxq_ctrl->rxq.wqes);
		rxq_ctrl->rxq.wqes = NULL;
	}
	if (rxq_ctrl->wq_umem) {
		mlx5_glue->devx_umem_dereg(rxq_ctrl->wq_umem);
		rxq_ctrl->wq_umem = NULL;
	}
	if (dbr_page) {
		claim_zero(mlx5_release_dbr(&rxq_ctrl->priv->dbrpgs,
					    mlx5_os_get_umem_id(dbr_page->umem),
					    rxq_ctrl->rq_dbr_offset));
		rxq_ctrl->rq_dbrec_page = NULL;
	}
}

/**
 * Release the resources allocated for the Rx CQ DevX object.
 *
 * @param rxq_ctrl
 *   DevX Rx queue object.
 */
static void
rxq_release_devx_cq_resources(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	struct mlx5_devx_dbr_page *dbr_page = rxq_ctrl->cq_dbrec_page;

	if (rxq_ctrl->rxq.cqes) {
		rte_free((void *)(uintptr_t)rxq_ctrl->rxq.cqes);
		rxq_ctrl->rxq.cqes = NULL;
	}
	if (rxq_ctrl->cq_umem) {
		mlx5_glue->devx_umem_dereg(rxq_ctrl->cq_umem);
		rxq_ctrl->cq_umem = NULL;
	}
	if (dbr_page) {
		claim_zero(mlx5_release_dbr(&rxq_ctrl->priv->dbrpgs,
					    mlx5_os_get_umem_id(dbr_page->umem),
					    rxq_ctrl->cq_dbr_offset));
		rxq_ctrl->cq_dbrec_page = NULL;
	}
}

/**
 * Release an Rx DevX queue object.
 *
 * @param rxq_obj
 *   DevX Rx queue object.
 */
static void
mlx5_rxq_devx_obj_release(struct mlx5_rxq_obj *rxq_obj)
{
	MLX5_ASSERT(rxq_obj);
	MLX5_ASSERT(rxq_obj->rq);
	if (rxq_obj->type == MLX5_RXQ_OBJ_TYPE_DEVX_HAIRPIN) {
		mlx5_devx_modify_rq(rxq_obj, false);
		claim_zero(mlx5_devx_cmd_destroy(rxq_obj->rq));
	} else {
		MLX5_ASSERT(rxq_obj->devx_cq);
		claim_zero(mlx5_devx_cmd_destroy(rxq_obj->rq));
		claim_zero(mlx5_devx_cmd_destroy(rxq_obj->devx_cq));
		if (rxq_obj->devx_channel)
			mlx5_glue->devx_destroy_event_channel
							(rxq_obj->devx_channel);
		rxq_release_devx_rq_resources(rxq_obj->rxq_ctrl);
		rxq_release_devx_cq_resources(rxq_obj->rxq_ctrl);
	}
}

/**
 * Get event for an Rx DevX queue object.
 *
 * @param rxq_obj
 *   DevX Rx queue object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rx_devx_get_event(struct mlx5_rxq_obj *rxq_obj)
{
#ifdef HAVE_IBV_DEVX_EVENT
	union {
		struct mlx5dv_devx_async_event_hdr event_resp;
		uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr) + 128];
	} out;
	int ret = mlx5_glue->devx_get_event(rxq_obj->devx_channel,
					    &out.event_resp,
					    sizeof(out.buf));

	if (ret < 0) {
		rte_errno = errno;
		return -rte_errno;
	}
	if (out.event_resp.cookie != (uint64_t)(uintptr_t)rxq_obj->devx_cq) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
#else
	(void)rxq_obj;
	rte_errno = ENOTSUP;
	return -rte_errno;
#endif /* HAVE_IBV_DEVX_EVENT */
}

/**
 * Fill common fields of create RQ attributes structure.
 *
 * @param rxq_data
 *   Pointer to Rx queue data.
 * @param cqn
 *   CQ number to use with this RQ.
 * @param rq_attr
 *   RQ attributes structure to fill..
 */
static void
mlx5_devx_create_rq_attr_fill(struct mlx5_rxq_data *rxq_data, uint32_t cqn,
			      struct mlx5_devx_create_rq_attr *rq_attr)
{
	rq_attr->state = MLX5_RQC_STATE_RST;
	rq_attr->vsd = (rxq_data->vlan_strip) ? 0 : 1;
	rq_attr->cqn = cqn;
	rq_attr->scatter_fcs = (rxq_data->crc_present) ? 1 : 0;
}

/**
 * Fill common fields of DevX WQ attributes structure.
 *
 * @param priv
 *   Pointer to device private data.
 * @param rxq_ctrl
 *   Pointer to Rx queue control structure.
 * @param wq_attr
 *   WQ attributes structure to fill..
 */
static void
mlx5_devx_wq_attr_fill(struct mlx5_priv *priv, struct mlx5_rxq_ctrl *rxq_ctrl,
		       struct mlx5_devx_wq_attr *wq_attr)
{
	wq_attr->end_padding_mode = priv->config.cqe_pad ?
					MLX5_WQ_END_PAD_MODE_ALIGN :
					MLX5_WQ_END_PAD_MODE_NONE;
	wq_attr->pd = priv->sh->pdn;
	wq_attr->dbr_addr = rxq_ctrl->rq_dbr_offset;
	wq_attr->dbr_umem_id =
			mlx5_os_get_umem_id(rxq_ctrl->rq_dbrec_page->umem);
	wq_attr->dbr_umem_valid = 1;
	wq_attr->wq_umem_id = mlx5_os_get_umem_id(rxq_ctrl->wq_umem);
	wq_attr->wq_umem_valid = 1;
}

/**
 * Create a RQ object using DevX.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array.
 *
 * @return
 *   The DevX RQ object initialized, NULL otherwise and rte_errno is set.
 */
static struct mlx5_devx_obj *
rxq_create_devx_rq_resources(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_devx_create_rq_attr rq_attr = { 0 };
	uint32_t wqe_n = 1 << (rxq_data->elts_n - rxq_data->sges_n);
	uint32_t cqn = rxq_ctrl->obj->devx_cq->id;
	struct mlx5_devx_dbr_page *dbr_page;
	int64_t dbr_offset;
	uint32_t wq_size = 0;
	uint32_t wqe_size = 0;
	uint32_t log_wqe_size = 0;
	void *buf = NULL;
	struct mlx5_devx_obj *rq;

	/* Fill RQ attributes. */
	rq_attr.mem_rq_type = MLX5_RQC_MEM_RQ_TYPE_MEMORY_RQ_INLINE;
	rq_attr.flush_in_error_en = 1;
	mlx5_devx_create_rq_attr_fill(rxq_data, cqn, &rq_attr);
	/* Fill WQ attributes for this RQ. */
	if (mlx5_rxq_mprq_enabled(rxq_data)) {
		rq_attr.wq_attr.wq_type = MLX5_WQ_TYPE_CYCLIC_STRIDING_RQ;
		/*
		 * Number of strides in each WQE:
		 * 512*2^single_wqe_log_num_of_strides.
		 */
		rq_attr.wq_attr.single_wqe_log_num_of_strides =
				rxq_data->strd_num_n -
				MLX5_MIN_SINGLE_WQE_LOG_NUM_STRIDES;
		/* Stride size = (2^single_stride_log_num_of_bytes)*64B. */
		rq_attr.wq_attr.single_stride_log_num_of_bytes =
				rxq_data->strd_sz_n -
				MLX5_MIN_SINGLE_STRIDE_LOG_NUM_BYTES;
		wqe_size = sizeof(struct mlx5_wqe_mprq);
	} else {
		rq_attr.wq_attr.wq_type = MLX5_WQ_TYPE_CYCLIC;
		wqe_size = sizeof(struct mlx5_wqe_data_seg);
	}
	log_wqe_size = log2above(wqe_size) + rxq_data->sges_n;
	rq_attr.wq_attr.log_wq_stride = log_wqe_size;
	rq_attr.wq_attr.log_wq_sz = rxq_data->elts_n - rxq_data->sges_n;
	/* Calculate and allocate WQ memory space. */
	wqe_size = 1 << log_wqe_size; /* round up power of two.*/
	wq_size = wqe_n * wqe_size;
	size_t alignment = MLX5_WQE_BUF_ALIGNMENT;
	if (alignment == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
		return NULL;
	}
	buf = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, wq_size,
			  alignment, rxq_ctrl->socket);
	if (!buf)
		return NULL;
	rxq_data->wqes = buf;
	rxq_ctrl->wq_umem = mlx5_glue->devx_umem_reg(priv->sh->ctx,
						     buf, wq_size, 0);
	if (!rxq_ctrl->wq_umem)
		goto error;
	/* Allocate RQ door-bell. */
	dbr_offset = mlx5_get_dbr(priv->sh->ctx, &priv->dbrpgs, &dbr_page);
	if (dbr_offset < 0) {
		DRV_LOG(ERR, "Failed to allocate RQ door-bell.");
		goto error;
	}
	rxq_ctrl->rq_dbr_offset = dbr_offset;
	rxq_ctrl->rq_dbrec_page = dbr_page;
	rxq_data->rq_db = (uint32_t *)((uintptr_t)dbr_page->dbrs +
			  (uintptr_t)rxq_ctrl->rq_dbr_offset);
	/* Create RQ using DevX API. */
	mlx5_devx_wq_attr_fill(priv, rxq_ctrl, &rq_attr.wq_attr);
	rq = mlx5_devx_cmd_create_rq(priv->sh->ctx, &rq_attr, rxq_ctrl->socket);
	if (!rq)
		goto error;
	return rq;
error:
	rxq_release_devx_rq_resources(rxq_ctrl);
	return NULL;
}

/**
 * Create a DevX CQ object for an Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array.
 *
 * @return
 *   The DevX CQ object initialized, NULL otherwise and rte_errno is set.
 */
static struct mlx5_devx_obj *
rxq_create_devx_cq_resources(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_devx_obj *cq_obj = 0;
	struct mlx5_devx_cq_attr cq_attr = { 0 };
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	size_t page_size = rte_mem_page_size();
	uint32_t lcore = (uint32_t)rte_lcore_to_cpu_id(-1);
	unsigned int cqe_n = mlx5_rxq_cqe_num(rxq_data);
	struct mlx5_devx_dbr_page *dbr_page;
	int64_t dbr_offset;
	uint32_t eqn = 0;
	void *buf = NULL;
	uint16_t event_nums[1] = {0};
	uint32_t log_cqe_n;
	uint32_t cq_size;
	int ret = 0;

	if (page_size == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get page_size.");
		goto error;
	}
	if (priv->config.cqe_comp && !rxq_data->hw_timestamp &&
	    !rxq_data->lro) {
		cq_attr.cqe_comp_en = MLX5DV_CQ_INIT_ATTR_MASK_COMPRESSED_CQE;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
		cq_attr.mini_cqe_res_format =
				mlx5_rxq_mprq_enabled(rxq_data) ?
				MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX :
				MLX5DV_CQE_RES_FORMAT_HASH;
#else
		cq_attr.mini_cqe_res_format = MLX5DV_CQE_RES_FORMAT_HASH;
#endif
		/*
		 * For vectorized Rx, it must not be doubled in order to
		 * make cq_ci and rq_ci aligned.
		 */
		if (mlx5_rxq_check_vec_support(rxq_data) < 0)
			cqe_n *= 2;
	} else if (priv->config.cqe_comp && rxq_data->hw_timestamp) {
		DRV_LOG(DEBUG,
			"Port %u Rx CQE compression is disabled for HW"
			" timestamp.",
			dev->data->port_id);
	} else if (priv->config.cqe_comp && rxq_data->lro) {
		DRV_LOG(DEBUG,
			"Port %u Rx CQE compression is disabled for LRO.",
			dev->data->port_id);
	}
#ifdef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
	if (priv->config.cqe_pad)
		cq_attr.cqe_size = MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD;
#endif
	log_cqe_n = log2above(cqe_n);
	cq_size = sizeof(struct mlx5_cqe) * (1 << log_cqe_n);
	/* Query the EQN for this core. */
	if (mlx5_glue->devx_query_eqn(priv->sh->ctx, lcore, &eqn)) {
		DRV_LOG(ERR, "Failed to query EQN for CQ.");
		goto error;
	}
	cq_attr.eqn = eqn;
	buf = rte_calloc_socket(__func__, 1, cq_size, page_size,
				rxq_ctrl->socket);
	if (!buf) {
		DRV_LOG(ERR, "Failed to allocate memory for CQ.");
		goto error;
	}
	rxq_data->cqes = (volatile struct mlx5_cqe (*)[])(uintptr_t)buf;
	rxq_ctrl->cq_umem = mlx5_glue->devx_umem_reg(priv->sh->ctx, buf,
						     cq_size,
						     IBV_ACCESS_LOCAL_WRITE);
	if (!rxq_ctrl->cq_umem) {
		DRV_LOG(ERR, "Failed to register umem for CQ.");
		goto error;
	}
	/* Allocate CQ door-bell. */
	dbr_offset = mlx5_get_dbr(priv->sh->ctx, &priv->dbrpgs, &dbr_page);
	if (dbr_offset < 0) {
		DRV_LOG(ERR, "Failed to allocate CQ door-bell.");
		goto error;
	}
	rxq_ctrl->cq_dbr_offset = dbr_offset;
	rxq_ctrl->cq_dbrec_page = dbr_page;
	rxq_data->cq_db = (uint32_t *)((uintptr_t)dbr_page->dbrs +
			  (uintptr_t)rxq_ctrl->cq_dbr_offset);
	rxq_data->cq_uar =
			mlx5_os_get_devx_uar_base_addr(priv->sh->devx_rx_uar);
	/* Create CQ using DevX API. */
	cq_attr.uar_page_id =
			mlx5_os_get_devx_uar_page_id(priv->sh->devx_rx_uar);
	cq_attr.q_umem_id = mlx5_os_get_umem_id(rxq_ctrl->cq_umem);
	cq_attr.q_umem_valid = 1;
	cq_attr.log_cq_size = log_cqe_n;
	cq_attr.log_page_size = rte_log2_u32(page_size);
	cq_attr.db_umem_offset = rxq_ctrl->cq_dbr_offset;
	cq_attr.db_umem_id = mlx5_os_get_umem_id(dbr_page->umem);
	cq_attr.db_umem_valid = 1;
	cq_obj = mlx5_devx_cmd_create_cq(priv->sh->ctx, &cq_attr);
	if (!cq_obj)
		goto error;
	rxq_data->cqe_n = log_cqe_n;
	rxq_data->cqn = cq_obj->id;
	if (rxq_ctrl->obj->devx_channel) {
		ret = mlx5_glue->devx_subscribe_devx_event
						(rxq_ctrl->obj->devx_channel,
						 cq_obj->obj,
						 sizeof(event_nums),
						 event_nums,
						 (uint64_t)(uintptr_t)cq_obj);
		if (ret) {
			DRV_LOG(ERR, "Fail to subscribe CQ to event channel.");
			rte_errno = errno;
			goto error;
		}
	}
	/* Initialise CQ to 1's to mark HW ownership for all CQEs. */
	memset((void *)(uintptr_t)rxq_data->cqes, 0xFF, cq_size);
	return cq_obj;
error:
	if (cq_obj)
		mlx5_devx_cmd_destroy(cq_obj);
	rxq_release_devx_cq_resources(rxq_ctrl);
	return NULL;
}

/**
 * Create the Rx hairpin queue object.
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
mlx5_rxq_obj_hairpin_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_devx_create_rq_attr attr = { 0 };
	struct mlx5_rxq_obj *tmpl = rxq_ctrl->obj;
	uint32_t max_wq_data;

	MLX5_ASSERT(rxq_data);
	MLX5_ASSERT(tmpl);
	tmpl->type = MLX5_RXQ_OBJ_TYPE_DEVX_HAIRPIN;
	tmpl->rxq_ctrl = rxq_ctrl;
	attr.hairpin = 1;
	max_wq_data = priv->config.hca_attr.log_max_hairpin_wq_data_sz;
	/* Jumbo frames > 9KB should be supported, and more packets. */
	if (priv->config.log_hp_size != (uint32_t)MLX5_ARG_UNSET) {
		if (priv->config.log_hp_size > max_wq_data) {
			DRV_LOG(ERR, "Total data size %u power of 2 is "
				"too large for hairpin.",
				priv->config.log_hp_size);
			rte_errno = ERANGE;
			return -rte_errno;
		}
		attr.wq_attr.log_hairpin_data_sz = priv->config.log_hp_size;
	} else {
		attr.wq_attr.log_hairpin_data_sz =
				(max_wq_data < MLX5_HAIRPIN_JUMBO_LOG_SIZE) ?
				 max_wq_data : MLX5_HAIRPIN_JUMBO_LOG_SIZE;
	}
	/* Set the packets number to the maximum value for performance. */
	attr.wq_attr.log_hairpin_num_packets =
			attr.wq_attr.log_hairpin_data_sz -
			MLX5_HAIRPIN_QUEUE_STRIDE;
	tmpl->rq = mlx5_devx_cmd_create_rq(priv->sh->ctx, &attr,
					   rxq_ctrl->socket);
	if (!tmpl->rq) {
		DRV_LOG(ERR,
			"Port %u Rx hairpin queue %u can't create rq object.",
			dev->data->port_id, idx);
		rte_errno = errno;
		return -rte_errno;
	}
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_HAIRPIN;
	return 0;
}

/**
 * Create the Rx queue DevX object.
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
mlx5_rxq_devx_obj_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_rxq_obj *tmpl = rxq_ctrl->obj;
	int ret = 0;

	MLX5_ASSERT(rxq_data);
	MLX5_ASSERT(tmpl);
	if (rxq_ctrl->type == MLX5_RXQ_TYPE_HAIRPIN)
		return mlx5_rxq_obj_hairpin_new(dev, idx);
	tmpl->type = MLX5_RXQ_OBJ_TYPE_DEVX_RQ;
	tmpl->rxq_ctrl = rxq_ctrl;
	if (rxq_ctrl->irq) {
		int devx_ev_flag =
			  MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA;

		tmpl->devx_channel = mlx5_glue->devx_create_event_channel
								(priv->sh->ctx,
								 devx_ev_flag);
		if (!tmpl->devx_channel) {
			rte_errno = errno;
			DRV_LOG(ERR, "Failed to create event channel %d.",
				rte_errno);
			goto error;
		}
		tmpl->fd = mlx5_os_get_devx_channel_fd(tmpl->devx_channel);
	}
	/* Create CQ using DevX API. */
	tmpl->devx_cq = rxq_create_devx_cq_resources(dev, idx);
	if (!tmpl->devx_cq) {
		DRV_LOG(ERR, "Failed to create CQ.");
		goto error;
	}
	/* Create RQ using DevX API. */
	tmpl->rq = rxq_create_devx_rq_resources(dev, idx);
	if (!tmpl->rq) {
		DRV_LOG(ERR, "Port %u Rx queue %u RQ creation failure.",
			dev->data->port_id, idx);
		rte_errno = ENOMEM;
		goto error;
	}
	/* Change queue state to ready. */
	ret = mlx5_devx_modify_rq(tmpl, true);
	if (ret)
		goto error;
	rxq_data->cq_arm_sn = 0;
	mlx5_rxq_initialize(rxq_data);
	rxq_data->cq_ci = 0;
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STARTED;
	rxq_ctrl->wqn = tmpl->rq->id;
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (tmpl->rq)
		claim_zero(mlx5_devx_cmd_destroy(tmpl->rq));
	if (tmpl->devx_cq)
		claim_zero(mlx5_devx_cmd_destroy(tmpl->devx_cq));
	if (tmpl->devx_channel)
		mlx5_glue->devx_destroy_event_channel(tmpl->devx_channel);
	rxq_release_devx_rq_resources(rxq_ctrl);
	rxq_release_devx_cq_resources(rxq_ctrl);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Create RQT using DevX API as a filed of indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param log_n
 *   Log of number of queues in the array.
 * @param ind_tbl
 *   DevX indirection table object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_ind_table_new(struct rte_eth_dev *dev, const unsigned int log_n,
			struct mlx5_ind_table_obj *ind_tbl)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_rqt_attr *rqt_attr = NULL;
	const unsigned int rqt_n = 1 << log_n;
	unsigned int i, j;

	MLX5_ASSERT(ind_tbl);
	rqt_attr = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rqt_attr) +
			      rqt_n * sizeof(uint32_t), 0, SOCKET_ID_ANY);
	if (!rqt_attr) {
		DRV_LOG(ERR, "Port %u cannot allocate RQT resources.",
			dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	rqt_attr->rqt_max_size = priv->config.ind_table_max_size;
	rqt_attr->rqt_actual_size = rqt_n;
	for (i = 0; i != ind_tbl->queues_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[ind_tbl->queues[i]];
		struct mlx5_rxq_ctrl *rxq_ctrl =
				container_of(rxq, struct mlx5_rxq_ctrl, rxq);

		rqt_attr->rq_list[i] = rxq_ctrl->obj->rq->id;
	}
	MLX5_ASSERT(i > 0);
	for (j = 0; i != rqt_n; ++j, ++i)
		rqt_attr->rq_list[i] = rqt_attr->rq_list[j];
	ind_tbl->rqt = mlx5_devx_cmd_create_rqt(priv->sh->ctx, rqt_attr);
	mlx5_free(rqt_attr);
	if (!ind_tbl->rqt) {
		DRV_LOG(ERR, "Port %u cannot create DevX RQT.",
			dev->data->port_id);
		rte_errno = errno;
		return -rte_errno;
	}
	return 0;
}

/**
 * Destroy the DevX RQT object.
 *
 * @param ind_table
 *   Indirection table to release.
 */
static void
mlx5_devx_ind_table_destroy(struct mlx5_ind_table_obj *ind_tbl)
{
	claim_zero(mlx5_devx_cmd_destroy(ind_tbl->rqt));
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
mlx5_devx_hrxq_new(struct rte_eth_dev *dev, struct mlx5_hrxq *hrxq,
		   int tunnel __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ind_table_obj *ind_tbl = hrxq->ind_table;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[ind_tbl->queues[0]];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_devx_tir_attr tir_attr;
	const uint8_t *rss_key = hrxq->rss_key;
	uint64_t hash_fields = hrxq->hash_fields;
	bool lro = true;
	uint32_t i;
	int err;

	/* Enable TIR LRO only if all the queues were configured for. */
	for (i = 0; i < ind_tbl->queues_n; ++i) {
		if (!(*priv->rxqs)[ind_tbl->queues[i]]->lro) {
			lro = false;
			break;
		}
	}
	memset(&tir_attr, 0, sizeof(tir_attr));
	tir_attr.disp_type = MLX5_TIRC_DISP_TYPE_INDIRECT;
	tir_attr.rx_hash_fn = MLX5_RX_HASH_FN_TOEPLITZ;
	tir_attr.tunneled_offload_en = !!tunnel;
	/* If needed, translate hash_fields bitmap to PRM format. */
	if (hash_fields) {
		struct mlx5_rx_hash_field_select *rx_hash_field_select = NULL;
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
		rx_hash_field_select = hash_fields & IBV_RX_HASH_INNER ?
				       &tir_attr.rx_hash_field_selector_inner :
				       &tir_attr.rx_hash_field_selector_outer;
#else
		rx_hash_field_select = &tir_attr.rx_hash_field_selector_outer;
#endif
		/* 1 bit: 0: IPv4, 1: IPv6. */
		rx_hash_field_select->l3_prot_type =
					!!(hash_fields & MLX5_IPV6_IBV_RX_HASH);
		/* 1 bit: 0: TCP, 1: UDP. */
		rx_hash_field_select->l4_prot_type =
					 !!(hash_fields & MLX5_UDP_IBV_RX_HASH);
		/* Bitmask which sets which fields to use in RX Hash. */
		rx_hash_field_select->selected_fields =
			((!!(hash_fields & MLX5_L3_SRC_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_SRC_IP) |
			(!!(hash_fields & MLX5_L3_DST_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_DST_IP |
			(!!(hash_fields & MLX5_L4_SRC_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_SPORT |
			(!!(hash_fields & MLX5_L4_DST_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_DPORT;
	}
	if (rxq_ctrl->type == MLX5_RXQ_TYPE_HAIRPIN)
		tir_attr.transport_domain = priv->sh->td->id;
	else
		tir_attr.transport_domain = priv->sh->tdn;
	memcpy(tir_attr.rx_hash_toeplitz_key, rss_key, MLX5_RSS_HASH_KEY_LEN);
	tir_attr.indirect_table = ind_tbl->rqt->id;
	if (dev->data->dev_conf.lpbk_mode)
		tir_attr.self_lb_block = MLX5_TIRC_SELF_LB_BLOCK_BLOCK_UNICAST;
	if (lro) {
		tir_attr.lro_timeout_period_usecs = priv->config.lro.timeout;
		tir_attr.lro_max_msg_sz = priv->max_lro_msg_size;
		tir_attr.lro_enable_mask = MLX5_TIRC_LRO_ENABLE_MASK_IPV4_LRO |
					   MLX5_TIRC_LRO_ENABLE_MASK_IPV6_LRO;
	}
	hrxq->tir = mlx5_devx_cmd_create_tir(priv->sh->ctx, &tir_attr);
	if (!hrxq->tir) {
		DRV_LOG(ERR, "Port %u cannot create DevX TIR.",
			dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	hrxq->action = mlx5_glue->dv_create_flow_action_dest_devx_tir
							       (hrxq->tir->obj);
	if (!hrxq->action) {
		rte_errno = errno;
		goto error;
	}
#endif
	return 0;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	if (hrxq->tir)
		claim_zero(mlx5_devx_cmd_destroy(hrxq->tir));
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Destroy a DevX TIR object.
 *
 * @param hrxq
 *   Hash Rx queue to release its tir.
 */
static void
mlx5_devx_tir_destroy(struct mlx5_hrxq *hrxq)
{
	claim_zero(mlx5_devx_cmd_destroy(hrxq->tir));
}

/**
 * Create a DevX drop action for Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_drop_action_create(struct rte_eth_dev *dev)
{
	(void)dev;
	DRV_LOG(ERR, "DevX drop action is not supported yet");
	rte_errno = ENOTSUP;
	return -rte_errno;
}

/**
 * Release a drop hash Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_devx_drop_action_destroy(struct rte_eth_dev *dev)
{
	(void)dev;
	DRV_LOG(ERR, "DevX drop action is not supported yet");
	rte_errno = ENOTSUP;
}

struct mlx5_obj_ops devx_obj_ops = {
	.rxq_obj_modify_vlan_strip = mlx5_rxq_obj_modify_rq_vlan_strip,
	.rxq_obj_new = mlx5_rxq_devx_obj_new,
	.rxq_event_get = mlx5_rx_devx_get_event,
	.rxq_obj_modify = mlx5_devx_modify_rq,
	.rxq_obj_release = mlx5_rxq_devx_obj_release,
	.ind_table_new = mlx5_devx_ind_table_new,
	.ind_table_destroy = mlx5_devx_ind_table_destroy,
	.hrxq_new = mlx5_devx_hrxq_new,
	.hrxq_destroy = mlx5_devx_tir_destroy,
	.drop_action_create = mlx5_devx_drop_action_create,
	.drop_action_destroy = mlx5_devx_drop_action_destroy,
};
