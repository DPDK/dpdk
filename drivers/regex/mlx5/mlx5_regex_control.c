/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <errno.h>

#include <rte_log.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_regexdev.h>
#include <rte_regexdev_core.h>
#include <rte_regexdev_driver.h>
#include <rte_dev.h>

#include <mlx5_common.h>
#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>
#include <mlx5_common_os.h>

#include "mlx5_regex.h"
#include "mlx5_regex_utils.h"
#include "mlx5_rxp_csrs.h"
#include "mlx5_rxp.h"

#define MLX5_REGEX_NUM_WQE_PER_PAGE (4096/64)

/**
 * Returns the number of qp obj to be created.
 *
 * @param nb_desc
 *   The number of descriptors for the queue.
 *
 * @return
 *   The number of obj to be created.
 */
static uint16_t
regex_ctrl_get_nb_obj(uint16_t nb_desc)
{
	return ((nb_desc / MLX5_REGEX_NUM_WQE_PER_PAGE) +
		!!(nb_desc % MLX5_REGEX_NUM_WQE_PER_PAGE));
}

/**
 * destroy CQ.
 *
 * @param priv
 *   Pointer to the priv object.
 * @param cp
 *   Pointer to the CQ to be destroyed.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
regex_ctrl_destroy_cq(struct mlx5_regex_priv *priv, struct mlx5_regex_cq *cq)
{
	if (cq->cqe_umem) {
		mlx5_glue->devx_umem_dereg(cq->cqe_umem);
		cq->cqe_umem = NULL;
	}
	if (cq->cqe) {
		rte_free((void *)(uintptr_t)cq->cqe);
		cq->cqe = NULL;
	}
	if (cq->dbr_offset) {
		mlx5_release_dbr(&priv->dbrpgs, cq->dbr_umem, cq->dbr_offset);
		cq->dbr_offset = -1;
	}
	if (cq->obj) {
		mlx5_devx_cmd_destroy(cq->obj);
		cq->obj = NULL;
	}
	return 0;
}

/**
 * create the CQ object.
 *
 * @param priv
 *   Pointer to the priv object.
 * @param cp
 *   Pointer to the CQ to be created.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
regex_ctrl_create_cq(struct mlx5_regex_priv *priv, struct mlx5_regex_cq *cq)
{
	struct mlx5_devx_cq_attr attr = {
		.q_umem_valid = 1,
		.db_umem_valid = 1,
		.eqn = priv->eqn,
	};
	struct mlx5_devx_dbr_page *dbr_page = NULL;
	void *buf = NULL;
	size_t pgsize = sysconf(_SC_PAGESIZE);
	uint32_t cq_size = 1 << cq->log_nb_desc;
	uint32_t i;

	cq->dbr_offset = mlx5_get_dbr(priv->ctx, &priv->dbrpgs, &dbr_page);
	if (cq->dbr_offset < 0) {
		DRV_LOG(ERR, "Can't allocate cq door bell record.");
		rte_errno  = ENOMEM;
		goto error;
	}
	cq->dbr_umem = mlx5_os_get_umem_id(dbr_page->umem);
	cq->dbr = (uint32_t *)((uintptr_t)dbr_page->dbrs +
			       (uintptr_t)cq->dbr_offset);

	buf = rte_calloc(NULL, 1, sizeof(struct mlx5_cqe) * cq_size, 4096);
	if (!buf) {
		DRV_LOG(ERR, "Can't allocate cqe buffer.");
		rte_errno  = ENOMEM;
		goto error;
	}
	cq->cqe = buf;
	for (i = 0; i < cq_size; i++)
		cq->cqe[i].op_own = 0xff;
	cq->cqe_umem = mlx5_glue->devx_umem_reg(priv->ctx, buf,
						sizeof(struct mlx5_cqe) *
						cq_size, 7);
	cq->ci = 0;
	if (!cq->cqe_umem) {
		DRV_LOG(ERR, "Can't register cqe mem.");
		rte_errno  = ENOMEM;
		goto error;
	}
	attr.db_umem_offset = cq->dbr_offset;
	attr.db_umem_id = cq->dbr_umem;
	attr.q_umem_id = mlx5_os_get_umem_id(cq->cqe_umem);
	attr.log_cq_size = cq->log_nb_desc;
	attr.uar_page_id = priv->uar->page_id;
	attr.log_page_size = rte_log2_u32(pgsize);
	cq->obj = mlx5_devx_cmd_create_cq(priv->ctx, &attr);
	if (!cq->obj) {
		DRV_LOG(ERR, "Can't create cq object.");
		rte_errno  = ENOMEM;
		goto error;
	}
	return 0;
error:
	if (cq->cqe_umem)
		mlx5_glue->devx_umem_dereg(cq->cqe_umem);
	if (buf)
		rte_free(buf);
	if (cq->dbr_offset)
		mlx5_release_dbr(&priv->dbrpgs, cq->dbr_umem, cq->dbr_offset);
	return -rte_errno;
}

#ifdef HAVE_IBV_FLOW_DV_SUPPORT
static int
regex_get_pdn(void *pd, uint32_t *pdn)
{
	struct mlx5dv_obj obj;
	struct mlx5dv_pd pd_info;
	int ret = 0;

	obj.pd.in = pd;
	obj.pd.out = &pd_info;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_PD);
	if (ret) {
		DRV_LOG(DEBUG, "Fail to get PD object info");
		return ret;
	}
	*pdn = pd_info.pdn;
	return 0;
}
#endif

/**
 * create the SQ object.
 *
 * @param priv
 *   Pointer to the priv object.
 * @param qp
 *   Pointer to the QP element
 * @param q_ind
 *   The index of the queue.
 * @param log_nb_desc
 *   Log 2 of the number of descriptors to be used.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
regex_ctrl_create_sq(struct mlx5_regex_priv *priv, struct mlx5_regex_qp *qp,
		     uint16_t q_ind, uint16_t log_nb_desc)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5_devx_create_sq_attr attr = { 0 };
	struct mlx5_devx_modify_sq_attr modify_attr = { 0 };
	struct mlx5_devx_wq_attr *wq_attr = &attr.wq_attr;
	struct mlx5_devx_dbr_page *dbr_page = NULL;
	struct mlx5_regex_sq *sq = &qp->sqs[q_ind];
	void *buf = NULL;
	uint32_t sq_size;
	uint32_t pd_num = 0;
	int ret;

	sq->log_nb_desc = log_nb_desc;
	sq_size = 1 << sq->log_nb_desc;
	sq->dbr_offset = mlx5_get_dbr(priv->ctx, &priv->dbrpgs, &dbr_page);
	if (sq->dbr_offset < 0) {
		DRV_LOG(ERR, "Can't allocate sq door bell record.");
		rte_errno  = ENOMEM;
		goto error;
	}
	sq->dbr_umem = mlx5_os_get_umem_id(dbr_page->umem);
	sq->dbr = (uint32_t *)((uintptr_t)dbr_page->dbrs +
			       (uintptr_t)sq->dbr_offset);

	buf = rte_calloc(NULL, 1, 64 * sq_size, 4096);
	if (!buf) {
		DRV_LOG(ERR, "Can't allocate wqe buffer.");
		rte_errno  = ENOMEM;
		goto error;
	}
	sq->wqe = buf;
	sq->wqe_umem = mlx5_glue->devx_umem_reg(priv->ctx, buf, 64 * sq_size,
						7);
	sq->ci = 0;
	sq->pi = 0;
	if (!sq->wqe_umem) {
		DRV_LOG(ERR, "Can't register wqe mem.");
		rte_errno  = ENOMEM;
		goto error;
	}
	attr.state = MLX5_SQC_STATE_RST;
	attr.tis_lst_sz = 0;
	attr.tis_num = 0;
	attr.user_index = q_ind;
	attr.cqn = qp->cq.obj->id;
	attr.ts_format = mlx5_ts_format_conv(priv->sq_ts_format);
	wq_attr->uar_page = priv->uar->page_id;
	regex_get_pdn(priv->pd, &pd_num);
	wq_attr->pd = pd_num;
	wq_attr->wq_type = MLX5_WQ_TYPE_CYCLIC;
	wq_attr->dbr_umem_id = sq->dbr_umem;
	wq_attr->dbr_addr = sq->dbr_offset;
	wq_attr->dbr_umem_valid = 1;
	wq_attr->wq_umem_id = mlx5_os_get_umem_id(sq->wqe_umem);
	wq_attr->wq_umem_offset = 0;
	wq_attr->wq_umem_valid = 1;
	wq_attr->log_wq_stride = 6;
	wq_attr->log_wq_sz = sq->log_nb_desc;
	sq->obj = mlx5_devx_cmd_create_sq(priv->ctx, &attr);
	if (!sq->obj) {
		DRV_LOG(ERR, "Can't create sq object.");
		rte_errno  = ENOMEM;
		goto error;
	}
	modify_attr.state = MLX5_SQC_STATE_RDY;
	ret = mlx5_devx_cmd_modify_sq(sq->obj, &modify_attr);
	if (ret) {
		DRV_LOG(ERR, "Can't change sq state to ready.");
		rte_errno  = ENOMEM;
		goto error;
	}

	return 0;
error:
	if (sq->wqe_umem)
		mlx5_glue->devx_umem_dereg(sq->wqe_umem);
	if (buf)
		rte_free(buf);
	if (sq->dbr_offset)
		mlx5_release_dbr(&priv->dbrpgs, sq->dbr_umem, sq->dbr_offset);
	return -rte_errno;
#else
	(void)priv;
	(void)qp;
	(void)q_ind;
	(void)log_nb_desc;
	DRV_LOG(ERR, "Cannot get pdn - no DV support.");
	return -ENOTSUP;
#endif
}

/**
 * Destroy the SQ object.
 *
 * @param priv
 *   Pointer to the priv object.
 * @param qp
 *   Pointer to the QP element
 * @param q_ind
 *   The index of the queue.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
regex_ctrl_destroy_sq(struct mlx5_regex_priv *priv, struct mlx5_regex_qp *qp,
		      uint16_t q_ind)
{
	struct mlx5_regex_sq *sq = &qp->sqs[q_ind];

	if (sq->wqe_umem) {
		mlx5_glue->devx_umem_dereg(sq->wqe_umem);
		sq->wqe_umem = NULL;
	}
	if (sq->wqe) {
		rte_free((void *)(uintptr_t)sq->wqe);
		sq->wqe = NULL;
	}
	if (sq->dbr_offset) {
		mlx5_release_dbr(&priv->dbrpgs, sq->dbr_umem, sq->dbr_offset);
		sq->dbr_offset = -1;
	}
	if (sq->obj) {
		mlx5_devx_cmd_destroy(sq->obj);
		sq->obj = NULL;
	}
	return 0;
}

/**
 * Setup the qp.
 *
 * @param dev
 *   Pointer to RegEx dev structure.
 * @param qp_ind
 *   The queue index to setup.
 * @param cfg
 *   The queue requested configuration.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_regex_qp_setup(struct rte_regexdev *dev, uint16_t qp_ind,
		    const struct rte_regexdev_qp_conf *cfg)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	struct mlx5_regex_qp *qp;
	int i;
	int nb_sq_config = 0;
	int ret;
	uint16_t log_desc;

	qp = &priv->qps[qp_ind];
	qp->flags = cfg->qp_conf_flags;
	qp->cq.log_nb_desc = rte_log2_u32(cfg->nb_desc);
	qp->nb_desc = 1 << qp->cq.log_nb_desc;
	if (qp->flags & RTE_REGEX_QUEUE_PAIR_CFG_OOS_F)
		qp->nb_obj = regex_ctrl_get_nb_obj(qp->nb_desc);
	else
		qp->nb_obj = 1;
	qp->sqs = rte_malloc(NULL,
			     qp->nb_obj * sizeof(struct mlx5_regex_sq), 64);
	if (!qp->sqs) {
		DRV_LOG(ERR, "Can't allocate sq array memory.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	log_desc = rte_log2_u32(qp->nb_desc / qp->nb_obj);
	ret = regex_ctrl_create_cq(priv, &qp->cq);
	if (ret) {
		DRV_LOG(ERR, "Can't create cq.");
		goto err_cq;
	}
	for (i = 0; i < qp->nb_obj; i++) {
		ret = regex_ctrl_create_sq(priv, qp, i, log_desc);
		if (ret) {
			DRV_LOG(ERR, "Can't create sq.");
			goto err_btree;
		}
		nb_sq_config++;
	}

	ret = mlx5_mr_btree_init(&qp->mr_ctrl.cache_bh, MLX5_MR_BTREE_CACHE_N,
				 rte_socket_id());
	if (ret) {
		DRV_LOG(ERR, "Error setting up mr btree");
		goto err_btree;
	}

	ret = mlx5_regexdev_setup_fastpath(priv, qp_ind);
	if (ret) {
		DRV_LOG(ERR, "Error setting up fastpath");
		goto err_fp;
	}
	return 0;

err_fp:
	mlx5_mr_btree_free(&qp->mr_ctrl.cache_bh);
err_btree:
	for (i = 0; i < nb_sq_config; i++)
		regex_ctrl_destroy_sq(priv, qp, i);
	regex_ctrl_destroy_cq(priv, &qp->cq);
err_cq:
	rte_free(qp->sqs);
	return ret;
}
