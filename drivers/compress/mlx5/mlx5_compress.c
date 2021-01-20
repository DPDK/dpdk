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
#include <mlx5_prm.h>

#include "mlx5_compress_utils.h"

#define MLX5_COMPRESS_DRIVER_NAME mlx5_compress
#define MLX5_COMPRESS_LOG_NAME    pmd.compress.mlx5

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
};

TAILQ_HEAD(mlx5_compress_privs, mlx5_compress_priv) mlx5_compress_priv_list =
				TAILQ_HEAD_INITIALIZER(mlx5_compress_priv_list);
static pthread_mutex_t priv_list_lock = PTHREAD_MUTEX_INITIALIZER;

int mlx5_compress_logtype;

static struct rte_compressdev_ops mlx5_compress_ops = {
	.dev_configure		= NULL,
	.dev_start		= NULL,
	.dev_stop		= NULL,
	.dev_close		= NULL,
	.dev_infos_get		= NULL,
	.stats_get		= NULL,
	.stats_reset		= NULL,
	.queue_pair_setup	= NULL,
	.queue_pair_release	= NULL,
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
