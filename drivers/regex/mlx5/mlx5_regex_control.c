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
	int ret;

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
		rte_errno  = ENOMEM;
		return -rte_errno;
	}
	ret = regex_ctrl_create_cq(priv, &qp->cq);
	if (ret) {
		DRV_LOG(ERR, "Can't create cq.");
		goto error;
	}
	return 0;

error:
	regex_ctrl_destroy_cq(priv, &qp->cq);
	return -rte_errno;

}
