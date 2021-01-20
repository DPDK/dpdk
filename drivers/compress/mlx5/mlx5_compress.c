/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_pci.h>
#include <rte_comp.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_common_pci.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common_os.h>
#include <mlx5_common_devx.h>
#include <mlx5_common_mr.h>
#include <mlx5_prm.h>

#include "mlx5_compress_utils.h"

#define MLX5_COMPRESS_DRIVER_NAME mlx5_compress
#define MLX5_COMPRESS_LOG_NAME    pmd.compress.mlx5
#define MLX5_COMPRESS_MAX_QPS 1024

struct mlx5_compress_priv {
	TAILQ_ENTRY(mlx5_compress_priv) next;
	struct ibv_context *ctx; /* Device context. */
	struct rte_pci_device *pci_dev;
	struct rte_compressdev *cdev;
	void *uar;
	uint32_t pdn; /* Protection Domain number. */
	uint8_t min_block_size;
	/* Minimum huffman block size supported by the device. */
	struct ibv_pd *pd;
	struct rte_compressdev_config dev_config;
};

struct mlx5_compress_qp {
	uint16_t qp_id;
	uint16_t entries_n;
	uint16_t pi;
	uint16_t ci;
	volatile uint64_t *uar_addr;
	int socket_id;
	struct mlx5_devx_cq cq;
	struct mlx5_devx_sq sq;
	struct mlx5_pmd_mr opaque_mr;
	struct rte_comp_op **ops;
	struct mlx5_compress_priv *priv;
};

TAILQ_HEAD(mlx5_compress_privs, mlx5_compress_priv) mlx5_compress_priv_list =
				TAILQ_HEAD_INITIALIZER(mlx5_compress_priv_list);
static pthread_mutex_t priv_list_lock = PTHREAD_MUTEX_INITIALIZER;

int mlx5_compress_logtype;

const struct rte_compressdev_capabilities mlx5_caps[RTE_COMP_ALGO_LIST_END];


static void
mlx5_compress_dev_info_get(struct rte_compressdev *dev,
			   struct rte_compressdev_info *info)
{
	RTE_SET_USED(dev);
	if (info != NULL) {
		info->max_nb_queue_pairs = MLX5_COMPRESS_MAX_QPS;
		info->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;
		info->capabilities = mlx5_caps;
	}
}

static int
mlx5_compress_dev_configure(struct rte_compressdev *dev,
			    struct rte_compressdev_config *config)
{
	struct mlx5_compress_priv *priv;

	if (dev == NULL || config == NULL)
		return -EINVAL;
	priv = dev->data->dev_private;
	priv->dev_config = *config;
	return 0;
}

static int
mlx5_compress_dev_close(struct rte_compressdev *dev)
{
	RTE_SET_USED(dev);
	return 0;
}

static int
mlx5_compress_qp_release(struct rte_compressdev *dev, uint16_t qp_id)
{
	struct mlx5_compress_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp->sq.sq != NULL)
		mlx5_devx_sq_destroy(&qp->sq);
	if (qp->cq.cq != NULL)
		mlx5_devx_cq_destroy(&qp->cq);
	if (qp->opaque_mr.obj != NULL) {
		void *opaq = qp->opaque_mr.addr;

		mlx5_common_verbs_dereg_mr(&qp->opaque_mr);
		if (opaq != NULL)
			rte_free(opaq);
	}
	rte_free(qp);
	dev->data->queue_pairs[qp_id] = NULL;
	return 0;
}

static void
mlx5_compress_init_sq(struct mlx5_compress_qp *qp)
{
	volatile struct mlx5_gga_wqe *restrict wqe =
				    (volatile struct mlx5_gga_wqe *)qp->sq.wqes;
	volatile struct mlx5_gga_compress_opaque *opaq = qp->opaque_mr.addr;
	const uint32_t sq_ds = rte_cpu_to_be_32((qp->sq.sq->id << 8) | 4u);
	const uint32_t flags = RTE_BE32(MLX5_COMP_ALWAYS <<
					MLX5_COMP_MODE_OFFSET);
	const uint32_t opaq_lkey = rte_cpu_to_be_32(qp->opaque_mr.lkey);
	int i;

	/* All the next fields state should stay constant. */
	for (i = 0; i < qp->entries_n; ++i, ++wqe) {
		wqe->sq_ds = sq_ds;
		wqe->flags = flags;
		wqe->opaque_lkey = opaq_lkey;
		wqe->opaque_vaddr = rte_cpu_to_be_64
						((uint64_t)(uintptr_t)&opaq[i]);
	}
}

static int
mlx5_compress_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		       uint32_t max_inflight_ops, int socket_id)
{
	struct mlx5_compress_priv *priv = dev->data->dev_private;
	struct mlx5_compress_qp *qp;
	struct mlx5_devx_cq_attr cq_attr = {
		.uar_page_id = mlx5_os_get_devx_uar_page_id(priv->uar),
	};
	struct mlx5_devx_create_sq_attr sq_attr = {
		.user_index = qp_id,
		.wq_attr = (struct mlx5_devx_wq_attr){
			.pd = priv->pdn,
			.uar_page = mlx5_os_get_devx_uar_page_id(priv->uar),
		},
	};
	struct mlx5_devx_modify_sq_attr modify_attr = {
		.state = MLX5_SQC_STATE_RDY,
	};
	uint32_t log_ops_n = rte_log2_u32(max_inflight_ops);
	uint32_t alloc_size = sizeof(*qp);
	void *opaq_buf;
	int ret;

	alloc_size = RTE_ALIGN(alloc_size, RTE_CACHE_LINE_SIZE);
	alloc_size += sizeof(struct rte_comp_op *) * (1u << log_ops_n);
	qp = rte_zmalloc_socket(__func__, alloc_size, RTE_CACHE_LINE_SIZE,
				socket_id);
	if (qp == NULL) {
		DRV_LOG(ERR, "Failed to allocate qp memory.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	dev->data->queue_pairs[qp_id] = qp;
	opaq_buf = rte_calloc(__func__, 1u << log_ops_n,
			      sizeof(struct mlx5_gga_compress_opaque),
			      sizeof(struct mlx5_gga_compress_opaque));
	if (opaq_buf == NULL) {
		DRV_LOG(ERR, "Failed to allocate opaque memory.");
		rte_errno = ENOMEM;
		goto err;
	}
	qp->entries_n = 1 << log_ops_n;
	qp->socket_id = socket_id;
	qp->qp_id = qp_id;
	qp->priv = priv;
	qp->ops = (struct rte_comp_op **)RTE_ALIGN((uintptr_t)(qp + 1),
						   RTE_CACHE_LINE_SIZE);
	qp->uar_addr = mlx5_os_get_devx_uar_reg_addr(priv->uar);
	MLX5_ASSERT(qp->uar_addr);
	if (mlx5_common_verbs_reg_mr(priv->pd, opaq_buf, qp->entries_n *
					sizeof(struct mlx5_gga_compress_opaque),
							 &qp->opaque_mr) != 0) {
		rte_free(opaq_buf);
		DRV_LOG(ERR, "Failed to register opaque MR.");
		rte_errno = ENOMEM;
		goto err;
	}
	ret = mlx5_devx_cq_create(priv->ctx, &qp->cq, log_ops_n, &cq_attr,
				  socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create CQ.");
		goto err;
	}
	sq_attr.cqn = qp->cq.cq->id;
	ret = mlx5_devx_sq_create(priv->ctx, &qp->sq, log_ops_n, &sq_attr,
				  socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create SQ.");
		goto err;
	}
	mlx5_compress_init_sq(qp);
	ret = mlx5_devx_cmd_modify_sq(qp->sq.sq, &modify_attr);
	if (ret != 0) {
		DRV_LOG(ERR, "Can't change SQ state to ready.");
		goto err;
	}
	DRV_LOG(INFO, "QP %u: SQN=0x%X CQN=0x%X entries num = %u\n",
		(uint32_t)qp_id, qp->sq.sq->id, qp->cq.cq->id, qp->entries_n);
	return 0;
err:
	mlx5_compress_qp_release(dev, qp_id);
	return -1;
}

static struct rte_compressdev_ops mlx5_compress_ops = {
	.dev_configure		= mlx5_compress_dev_configure,
	.dev_start		= NULL,
	.dev_stop		= NULL,
	.dev_close		= mlx5_compress_dev_close,
	.dev_infos_get		= mlx5_compress_dev_info_get,
	.stats_get		= NULL,
	.stats_reset		= NULL,
	.queue_pair_setup	= mlx5_compress_qp_setup,
	.queue_pair_release	= mlx5_compress_qp_release,
	.private_xform_create	= NULL,
	.private_xform_free	= NULL,
	.stream_create		= NULL,
	.stream_free		= NULL,
};

static struct ibv_device *
mlx5_compress_get_ib_device_match(struct rte_pci_addr *addr)
{
	int n;
	struct ibv_device **ibv_list = mlx5_glue->get_device_list(&n);
	struct ibv_device *ibv_match = NULL;

	if (ibv_list == NULL) {
		rte_errno = ENOSYS;
		return NULL;
	}
	while (n-- > 0) {
		struct rte_pci_addr paddr;

		DRV_LOG(DEBUG, "Checking device \"%s\"..", ibv_list[n]->name);
		if (mlx5_dev_to_pci_addr(ibv_list[n]->ibdev_path, &paddr) != 0)
			continue;
		if (rte_pci_addr_cmp(addr, &paddr) != 0)
			continue;
		ibv_match = ibv_list[n];
		break;
	}
	if (ibv_match == NULL)
		rte_errno = ENOENT;
	mlx5_glue->free_device_list(ibv_list);
	return ibv_match;
}

static void
mlx5_compress_hw_global_release(struct mlx5_compress_priv *priv)
{
	if (priv->pd != NULL) {
		claim_zero(mlx5_glue->dealloc_pd(priv->pd));
		priv->pd = NULL;
	}
	if (priv->uar != NULL) {
		mlx5_glue->devx_free_uar(priv->uar);
		priv->uar = NULL;
	}
}

static int
mlx5_compress_pd_create(struct mlx5_compress_priv *priv)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5dv_obj obj;
	struct mlx5dv_pd pd_info;
	int ret;

	priv->pd = mlx5_glue->alloc_pd(priv->ctx);
	if (priv->pd == NULL) {
		DRV_LOG(ERR, "Failed to allocate PD.");
		return errno ? -errno : -ENOMEM;
	}
	obj.pd.in = priv->pd;
	obj.pd.out = &pd_info;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_PD);
	if (ret != 0) {
		DRV_LOG(ERR, "Fail to get PD object info.");
		mlx5_glue->dealloc_pd(priv->pd);
		priv->pd = NULL;
		return -errno;
	}
	priv->pdn = pd_info.pdn;
	return 0;
#else
	(void)priv;
	DRV_LOG(ERR, "Cannot get pdn - no DV support.");
	return -ENOTSUP;
#endif /* HAVE_IBV_FLOW_DV_SUPPORT */
}

static int
mlx5_compress_hw_global_prepare(struct mlx5_compress_priv *priv)
{
	if (mlx5_compress_pd_create(priv) != 0)
		return -1;
	priv->uar = mlx5_devx_alloc_uar(priv->ctx, -1);
	if (priv->uar == NULL || mlx5_os_get_devx_uar_reg_addr(priv->uar) ==
	    NULL) {
		rte_errno = errno;
		claim_zero(mlx5_glue->dealloc_pd(priv->pd));
		DRV_LOG(ERR, "Failed to allocate UAR.");
		return -1;
	}
	return 0;
}

/**
 * DPDK callback to register a PCI device.
 *
 * This function spawns compress device out of a given PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx5_compress_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, 1 to skip this driver, a negative errno value otherwise
 *   and rte_errno is set.
 */
static int
mlx5_compress_pci_probe(struct rte_pci_driver *pci_drv,
			struct rte_pci_device *pci_dev)
{
	struct ibv_device *ibv;
	struct rte_compressdev *cdev;
	struct ibv_context *ctx;
	struct mlx5_compress_priv *priv;
	struct mlx5_hca_attr att = { 0 };
	struct rte_compressdev_pmd_init_params init_params = {
		.name = "",
		.socket_id = pci_dev->device.numa_node,
	};

	RTE_SET_USED(pci_drv);
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		DRV_LOG(ERR, "Non-primary process type is not supported.");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	ibv = mlx5_compress_get_ib_device_match(&pci_dev->addr);
	if (ibv == NULL) {
		DRV_LOG(ERR, "No matching IB device for PCI slot "
			PCI_PRI_FMT ".", pci_dev->addr.domain,
			pci_dev->addr.bus, pci_dev->addr.devid,
			pci_dev->addr.function);
		return -rte_errno;
	}
	DRV_LOG(INFO, "PCI information matches for device \"%s\".", ibv->name);
	ctx = mlx5_glue->dv_open_device(ibv);
	if (ctx == NULL) {
		DRV_LOG(ERR, "Failed to open IB device \"%s\".", ibv->name);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	if (mlx5_devx_cmd_query_hca_attr(ctx, &att) != 0 ||
	    att.mmo_compress_en == 0 || att.mmo_decompress_en == 0 ||
	    att.mmo_dma_en == 0) {
		DRV_LOG(ERR, "Not enough capabilities to support compress "
			"operations, maybe old FW/OFED version?");
		claim_zero(mlx5_glue->close_device(ctx));
		rte_errno = ENOTSUP;
		return -ENOTSUP;
	}
	cdev = rte_compressdev_pmd_create(ibv->name, &pci_dev->device,
					  sizeof(*priv), &init_params);
	if (cdev == NULL) {
		DRV_LOG(ERR, "Failed to create device \"%s\".", ibv->name);
		claim_zero(mlx5_glue->close_device(ctx));
		return -ENODEV;
	}
	DRV_LOG(INFO,
		"Compress device %s was created successfully.", ibv->name);
	cdev->dev_ops = &mlx5_compress_ops;
	cdev->dequeue_burst = NULL;
	cdev->enqueue_burst = NULL;
	cdev->feature_flags = RTE_COMPDEV_FF_HW_ACCELERATED;
	priv = cdev->data->dev_private;
	priv->ctx = ctx;
	priv->pci_dev = pci_dev;
	priv->cdev = cdev;
	priv->min_block_size = att.compress_min_block_size;
	if (mlx5_compress_hw_global_prepare(priv) != 0) {
		rte_compressdev_pmd_destroy(priv->cdev);
		claim_zero(mlx5_glue->close_device(priv->ctx));
		return -1;
	}
	pthread_mutex_lock(&priv_list_lock);
	TAILQ_INSERT_TAIL(&mlx5_compress_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	return 0;
}

/**
 * DPDK callback to remove a PCI device.
 *
 * This function removes all compress devices belong to a given PCI device.
 *
 * @param[in] pci_dev
 *   Pointer to the PCI device.
 *
 * @return
 *   0 on success, the function cannot fail.
 */
static int
mlx5_compress_pci_remove(struct rte_pci_device *pdev)
{
	struct mlx5_compress_priv *priv = NULL;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &mlx5_compress_priv_list, next)
		if (rte_pci_addr_cmp(&priv->pci_dev->addr, &pdev->addr) != 0)
			break;
	if (priv)
		TAILQ_REMOVE(&mlx5_compress_priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	if (priv) {
		mlx5_compress_hw_global_release(priv);
		rte_compressdev_pmd_destroy(priv->cdev);
		claim_zero(mlx5_glue->close_device(priv->ctx));
	}
	return 0;
}

static const struct rte_pci_id mlx5_compress_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6DXBF)
	},
	{
		.vendor_id = 0
	}
};

static struct mlx5_pci_driver mlx5_compress_driver = {
	.driver_class = MLX5_CLASS_COMPRESS,
	.pci_driver = {
		.driver = {
			.name = RTE_STR(MLX5_COMPRESS_DRIVER_NAME),
		},
		.id_table = mlx5_compress_pci_id_map,
		.probe = mlx5_compress_pci_probe,
		.remove = mlx5_compress_pci_remove,
		.drv_flags = 0,
	},
};

RTE_INIT(rte_mlx5_compress_init)
{
	mlx5_common_init();
	if (mlx5_glue != NULL)
		mlx5_pci_driver_register(&mlx5_compress_driver);
}

RTE_LOG_REGISTER(mlx5_compress_logtype, MLX5_COMPRESS_LOG_NAME, NOTICE)
RTE_PMD_EXPORT_NAME(MLX5_COMPRESS_DRIVER_NAME, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(MLX5_COMPRESS_DRIVER_NAME, mlx5_compress_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(MLX5_COMPRESS_DRIVER_NAME, "* ib_uverbs & mlx5_core & mlx5_ib");
