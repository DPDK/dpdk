/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_pci.h>
#include <rte_regexdev.h>
#include <rte_regexdev_core.h>
#include <rte_regexdev_driver.h>
#include <rte_bus_pci.h>

#include <mlx5_common.h>
#include <mlx5_common_mr.h>
#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>

#include "mlx5_regex.h"
#include "mlx5_regex_utils.h"
#include "mlx5_rxp_csrs.h"

#define MLX5_REGEX_DRIVER_NAME regex_mlx5

int mlx5_regex_logtype;

TAILQ_HEAD(regex_mem_event, mlx5_regex_priv) mlx5_mem_event_list =
				TAILQ_HEAD_INITIALIZER(mlx5_mem_event_list);
static pthread_mutex_t mem_event_list_lock = PTHREAD_MUTEX_INITIALIZER;

const struct rte_regexdev_ops mlx5_regexdev_ops = {
	.dev_info_get = mlx5_regex_info_get,
	.dev_configure = mlx5_regex_configure,
	.dev_db_import = mlx5_regex_rules_db_import,
	.dev_qp_setup = mlx5_regex_qp_setup,
	.dev_start = mlx5_regex_start,
	.dev_stop = mlx5_regex_stop,
	.dev_close = mlx5_regex_close,
};

int
mlx5_regex_start(struct rte_regexdev *dev __rte_unused)
{
	return 0;
}

int
mlx5_regex_stop(struct rte_regexdev *dev __rte_unused)
{
	return 0;
}

int
mlx5_regex_close(struct rte_regexdev *dev __rte_unused)
{
	return 0;
}

static int
mlx5_regex_engines_status(struct ibv_context *ctx, int num_engines)
{
	uint32_t fpga_ident = 0;
	int err;
	int i;

	for (i = 0; i < num_engines; i++) {
		err = mlx5_devx_regex_register_read(ctx, i,
						    MLX5_RXP_CSR_IDENTIFIER,
						    &fpga_ident);
		fpga_ident = (fpga_ident & (0x0000FFFF));
		if (err || fpga_ident != MLX5_RXP_IDENTIFIER) {
			DRV_LOG(ERR, "Failed setup RXP %d err %d database "
				"memory 0x%x", i, err, fpga_ident);
			if (!err)
				err = EINVAL;
			return err;
		}
	}
	return 0;
}

static void
mlx5_regex_get_name(char *name, struct rte_device *dev)
{
	sprintf(name, "mlx5_regex_%s", dev->name);
}

/**
 * Callback for memory event.
 *
 * @param event_type
 *   Memory event type.
 * @param addr
 *   Address of memory.
 * @param len
 *   Size of memory.
 */
static void
mlx5_regex_mr_mem_event_cb(enum rte_mem_event event_type, const void *addr,
			   size_t len, void *arg __rte_unused)
{
	struct mlx5_regex_priv *priv;

	/* Must be called from the primary process. */
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	switch (event_type) {
	case RTE_MEM_EVENT_FREE:
		pthread_mutex_lock(&mem_event_list_lock);
		/* Iterate all the existing mlx5 devices. */
		TAILQ_FOREACH(priv, &mlx5_mem_event_list, mem_event_cb)
			mlx5_free_mr_by_addr(&priv->mr_scache,
					     priv->ctx->device->name,
					     addr, len);
		pthread_mutex_unlock(&mem_event_list_lock);
		break;
	case RTE_MEM_EVENT_ALLOC:
	default:
		break;
	}
}

static int
mlx5_regex_dev_probe(struct rte_device *rte_dev)
{
	struct ibv_device *ibv;
	struct mlx5_regex_priv *priv = NULL;
	struct ibv_context *ctx = NULL;
	struct mlx5_hca_attr attr;
	char name[RTE_REGEXDEV_NAME_MAX_LEN];
	int ret;
	uint32_t val;

	ibv = mlx5_os_get_ibv_dev(rte_dev);
	if (ibv == NULL)
		return -rte_errno;
	DRV_LOG(INFO, "Probe device \"%s\".", ibv->name);
	ctx = mlx5_glue->dv_open_device(ibv);
	if (!ctx) {
		DRV_LOG(ERR, "Failed to open IB device \"%s\".", ibv->name);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	ret = mlx5_devx_cmd_query_hca_attr(ctx, &attr);
	if (ret) {
		DRV_LOG(ERR, "Unable to read HCA capabilities.");
		rte_errno = ENOTSUP;
		goto dev_error;
	} else if (!attr.regex || attr.regexp_num_of_engines == 0) {
		DRV_LOG(ERR, "Not enough capabilities to support RegEx, maybe "
			"old FW/OFED version?");
		rte_errno = ENOTSUP;
		goto dev_error;
	}
	if (mlx5_regex_engines_status(ctx, 2)) {
		DRV_LOG(ERR, "RegEx engine error.");
		rte_errno = ENOMEM;
		goto dev_error;
	}
	priv = rte_zmalloc("mlx5 regex device private", sizeof(*priv),
			   RTE_CACHE_LINE_SIZE);
	if (!priv) {
		DRV_LOG(ERR, "Failed to allocate private memory.");
		rte_errno = ENOMEM;
		goto dev_error;
	}
	priv->sq_ts_format = attr.sq_ts_format;
	priv->ctx = ctx;
	priv->nb_engines = 2; /* attr.regexp_num_of_engines */
	ret = mlx5_devx_regex_register_read(priv->ctx, 0,
					    MLX5_RXP_CSR_IDENTIFIER, &val);
	if (ret) {
		DRV_LOG(ERR, "CSR read failed!");
		return -1;
	}
	if (val == MLX5_RXP_BF2_IDENTIFIER)
		priv->is_bf2 = 1;
	/* Default RXP programming mode to Shared. */
	priv->prog_mode = MLX5_RXP_SHARED_PROG_MODE;
	mlx5_regex_get_name(name, rte_dev);
	priv->regexdev = rte_regexdev_register(name);
	if (priv->regexdev == NULL) {
		DRV_LOG(ERR, "Failed to register RegEx device.");
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}
	/*
	 * This PMD always claims the write memory barrier on UAR
	 * registers writings, it is safe to allocate UAR with any
	 * memory mapping type.
	 */
	priv->uar = mlx5_devx_alloc_uar(ctx, -1);
	if (!priv->uar) {
		DRV_LOG(ERR, "can't allocate uar.");
		rte_errno = ENOMEM;
		goto error;
	}
	priv->pd = mlx5_glue->alloc_pd(ctx);
	if (!priv->pd) {
		DRV_LOG(ERR, "can't allocate pd.");
		rte_errno = ENOMEM;
		goto error;
	}
	priv->regexdev->dev_ops = &mlx5_regexdev_ops;
	priv->regexdev->enqueue = mlx5_regexdev_enqueue;
#ifdef HAVE_MLX5_UMR_IMKEY
	if (!attr.umr_indirect_mkey_disabled &&
	    !attr.umr_modify_entity_size_disabled)
		priv->has_umr = 1;
	if (priv->has_umr)
		priv->regexdev->enqueue = mlx5_regexdev_enqueue_gga;
#endif
	priv->regexdev->dequeue = mlx5_regexdev_dequeue;
	priv->regexdev->device = rte_dev;
	priv->regexdev->data->dev_private = priv;
	priv->regexdev->state = RTE_REGEXDEV_READY;
	priv->mr_scache.reg_mr_cb = mlx5_common_verbs_reg_mr;
	priv->mr_scache.dereg_mr_cb = mlx5_common_verbs_dereg_mr;
	ret = mlx5_mr_btree_init(&priv->mr_scache.cache,
				 MLX5_MR_BTREE_CACHE_N * 2,
				 rte_socket_id());
	if (ret) {
		DRV_LOG(ERR, "MR init tree failed.");
	    rte_errno = ENOMEM;
		goto error;
	}
	/* Register callback function for global shared MR cache management. */
	if (TAILQ_EMPTY(&mlx5_mem_event_list))
		rte_mem_event_callback_register("MLX5_MEM_EVENT_CB",
						mlx5_regex_mr_mem_event_cb,
						NULL);
	/* Add device to memory callback list. */
	pthread_mutex_lock(&mem_event_list_lock);
	TAILQ_INSERT_TAIL(&mlx5_mem_event_list, priv, mem_event_cb);
	pthread_mutex_unlock(&mem_event_list_lock);
	DRV_LOG(INFO, "RegEx GGA is %s.",
		priv->has_umr ? "supported" : "unsupported");
	return 0;

error:
	if (priv->pd)
		mlx5_glue->dealloc_pd(priv->pd);
	if (priv->uar)
		mlx5_glue->devx_free_uar(priv->uar);
	if (priv->regexdev)
		rte_regexdev_unregister(priv->regexdev);
dev_error:
	if (ctx)
		mlx5_glue->close_device(ctx);
	if (priv)
		rte_free(priv);
	return -rte_errno;
}

static int
mlx5_regex_dev_remove(struct rte_device *rte_dev)
{
	char name[RTE_REGEXDEV_NAME_MAX_LEN];
	struct rte_regexdev *dev;
	struct mlx5_regex_priv *priv = NULL;

	mlx5_regex_get_name(name, rte_dev);
	dev = rte_regexdev_get_device_by_name(name);
	if (!dev)
		return 0;
	priv = dev->data->dev_private;
	if (priv) {
		/* Remove from memory callback device list. */
		pthread_mutex_lock(&mem_event_list_lock);
		TAILQ_REMOVE(&mlx5_mem_event_list, priv, mem_event_cb);
		pthread_mutex_unlock(&mem_event_list_lock);
		if (TAILQ_EMPTY(&mlx5_mem_event_list))
			rte_mem_event_callback_unregister("MLX5_MEM_EVENT_CB",
							  NULL);
		if (priv->mr_scache.cache.table)
			mlx5_mr_release_cache(&priv->mr_scache);
		if (priv->pd)
			mlx5_glue->dealloc_pd(priv->pd);
		if (priv->uar)
			mlx5_glue->devx_free_uar(priv->uar);
		if (priv->regexdev)
			rte_regexdev_unregister(priv->regexdev);
		if (priv->ctx)
			mlx5_glue->close_device(priv->ctx);
		rte_free(priv);
	}
	return 0;
}

static const struct rte_pci_id mlx5_regex_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6DXBF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX7BF)
	},
	{
		.vendor_id = 0
	}
};

static struct mlx5_class_driver mlx5_regex_driver = {
	.drv_class = MLX5_CLASS_REGEX,
	.name = RTE_STR(MLX5_REGEX_DRIVER_NAME),
	.id_table = mlx5_regex_pci_id_map,
	.probe = mlx5_regex_dev_probe,
	.remove = mlx5_regex_dev_remove,
};

RTE_INIT(rte_mlx5_regex_init)
{
	mlx5_common_init();
	if (mlx5_glue)
		mlx5_class_driver_register(&mlx5_regex_driver);
}

RTE_LOG_REGISTER_DEFAULT(mlx5_regex_logtype, NOTICE)
RTE_PMD_EXPORT_NAME(MLX5_REGEX_DRIVER_NAME, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(MLX5_REGEX_DRIVER_NAME, mlx5_regex_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(MLX5_REGEX_DRIVER_NAME, "* ib_uverbs & mlx5_core & mlx5_ib");
