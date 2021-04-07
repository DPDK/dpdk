/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <errno.h>

#include <rte_log.h>
#include <rte_errno.h>
#include <rte_memory.h>
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
#include <mlx5_common_devx.h>

#include "mlx5_regex.h"
#include "mlx5_regex_utils.h"
#include "mlx5_rxp_csrs.h"
#include "mlx5_rxp.h"

#define MLX5_REGEX_NUM_WQE_PER_PAGE (4096/64)

#define MLX5_REGEX_WQE_LOG_NUM(has_umr, log_desc) \
		((has_umr) ? ((log_desc) + 2) : (log_desc))

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
 * @param cp
 *   Pointer to the CQ to be destroyed.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
regex_ctrl_destroy_cq(struct mlx5_regex_cq *cq)
{
	mlx5_devx_cq_destroy(&cq->cq_obj);
	memset(cq, 0, sizeof(*cq));
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
		.uar_page_id = priv->uar->page_id,
	};
	int ret;

	cq->ci = 0;
	ret = mlx5_devx_cq_create(priv->ctx, &cq->cq_obj, cq->log_nb_desc,
				  &attr, SOCKET_ID_ANY);
	if (ret) {
		DRV_LOG(ERR, "Can't create CQ object.");
		memset(cq, 0, sizeof(*cq));
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	return 0;
}

/**
 * Destroy the SQ object.
 *
 * @param qp
 *   Pointer to the QP element
 * @param q_ind
 *   The index of the queue.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
regex_ctrl_destroy_sq(struct mlx5_regex_qp *qp, uint16_t q_ind)
{
	struct mlx5_regex_sq *sq = &qp->sqs[q_ind];

	mlx5_devx_sq_destroy(&sq->sq_obj);
	memset(sq, 0, sizeof(*sq));
	return 0;
}

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
	struct mlx5_devx_create_sq_attr attr = {
		.user_index = q_ind,
		.cqn = qp->cq.cq_obj.cq->id,
		.wq_attr = (struct mlx5_devx_wq_attr){
			.uar_page = priv->uar->page_id,
		},
		.ts_format = mlx5_ts_format_conv(priv->sq_ts_format),
	};
	struct mlx5_devx_modify_sq_attr modify_attr = {
		.state = MLX5_SQC_STATE_RDY,
	};
	struct mlx5_regex_sq *sq = &qp->sqs[q_ind];
	uint32_t pd_num = 0;
	int ret;

	sq->log_nb_desc = log_nb_desc;
	sq->sqn = q_ind;
	sq->ci = 0;
	sq->pi = 0;
	ret = regex_get_pdn(priv->pd, &pd_num);
	if (ret)
		return ret;
	attr.wq_attr.pd = pd_num;
	ret = mlx5_devx_sq_create(priv->ctx, &sq->sq_obj,
			MLX5_REGEX_WQE_LOG_NUM(priv->has_umr, log_nb_desc),
			&attr, SOCKET_ID_ANY);
	if (ret) {
		DRV_LOG(ERR, "Can't create SQ object.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	ret = mlx5_devx_cmd_modify_sq(sq->sq_obj.sq, &modify_attr);
	if (ret) {
		DRV_LOG(ERR, "Can't change SQ state to ready.");
		regex_ctrl_destroy_sq(qp, q_ind);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	return 0;
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
	log_desc = rte_log2_u32(cfg->nb_desc);
	/*
	 * UMR mode requires two WQEs(UMR and RegEx WQE) for one descriptor.
	 * For CQ, expand the CQE number multiple with 2.
	 * For SQ, the UMR and RegEx WQE for one descriptor consumes 4 WQEBBS,
	 * expand the WQE number multiple with 4.
	 */
	qp->cq.log_nb_desc = log_desc + (!!priv->has_umr);
	qp->nb_desc = 1 << log_desc;
	if (qp->flags & RTE_REGEX_QUEUE_PAIR_CFG_OOS_F)
		qp->nb_obj = regex_ctrl_get_nb_obj
			(1 << MLX5_REGEX_WQE_LOG_NUM(priv->has_umr, log_desc));
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
		regex_ctrl_destroy_sq(qp, i);
	regex_ctrl_destroy_cq(&qp->cq);
err_cq:
	rte_free(qp->sqs);
	return ret;
}
