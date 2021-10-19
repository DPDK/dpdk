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
mlx5_regex_start(struct rte_regexdev *dev)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;

	return mlx5_dev_mempool_subscribe(priv->cdev);
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

static int
mlx5_regex_dev_probe(struct mlx5_common_device *cdev)
{
	struct mlx5_regex_priv *priv = NULL;
	struct mlx5_hca_attr *attr = &cdev->config.hca_attr;
	char name[RTE_REGEXDEV_NAME_MAX_LEN];
	int ret;
	uint32_t val;

	if ((!attr->regex && !attr->mmo_regex_sq_en && !attr->mmo_regex_qp_en)
	    || attr->regexp_num_of_engines == 0) {
		DRV_LOG(ERR, "Not enough capabilities to support RegEx, maybe "
			"old FW/OFED version?");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	if (mlx5_regex_engines_status(cdev->ctx, 2)) {
		DRV_LOG(ERR, "RegEx engine error.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv = rte_zmalloc("mlx5 regex device private", sizeof(*priv),
			   RTE_CACHE_LINE_SIZE);
	if (!priv) {
		DRV_LOG(ERR, "Failed to allocate private memory.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->mmo_regex_qp_cap = attr->mmo_regex_qp_en;
	priv->mmo_regex_sq_cap = attr->mmo_regex_sq_en;
	priv->cdev = cdev;
	priv->nb_engines = 2; /* attr.regexp_num_of_engines */
	ret = mlx5_devx_regex_register_read(priv->cdev->ctx, 0,
					    MLX5_RXP_CSR_IDENTIFIER, &val);
	if (ret) {
		DRV_LOG(ERR, "CSR read failed!");
		goto dev_error;
	}
	if (val == MLX5_RXP_BF2_IDENTIFIER)
		priv->is_bf2 = 1;
	/* Default RXP programming mode to Shared. */
	priv->prog_mode = MLX5_RXP_SHARED_PROG_MODE;
	mlx5_regex_get_name(name, cdev->dev);
	priv->regexdev = rte_regexdev_register(name);
	if (priv->regexdev == NULL) {
		DRV_LOG(ERR, "Failed to register RegEx device.");
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto dev_error;
	}
	/*
	 * This PMD always claims the write memory barrier on UAR
	 * registers writings, it is safe to allocate UAR with any
	 * memory mapping type.
	 */
	priv->uar = mlx5_devx_alloc_uar(priv->cdev->ctx, -1);
	if (!priv->uar) {
		DRV_LOG(ERR, "can't allocate uar.");
		rte_errno = ENOMEM;
		goto error;
	}
	priv->regexdev->dev_ops = &mlx5_regexdev_ops;
	priv->regexdev->enqueue = mlx5_regexdev_enqueue;
#ifdef HAVE_MLX5_UMR_IMKEY
	if (!attr->umr_indirect_mkey_disabled &&
	    !attr->umr_modify_entity_size_disabled)
		priv->has_umr = 1;
	if (priv->has_umr)
		priv->regexdev->enqueue = mlx5_regexdev_enqueue_gga;
#endif
	priv->regexdev->dequeue = mlx5_regexdev_dequeue;
	priv->regexdev->device = cdev->dev;
	priv->regexdev->data->dev_private = priv;
	priv->regexdev->state = RTE_REGEXDEV_READY;
	DRV_LOG(INFO, "RegEx GGA is %s.",
		priv->has_umr ? "supported" : "unsupported");
	return 0;

error:
	if (priv->uar)
		mlx5_glue->devx_free_uar(priv->uar);
	if (priv->regexdev)
		rte_regexdev_unregister(priv->regexdev);
dev_error:
	if (priv)
		rte_free(priv);
	return -rte_errno;
}

static int
mlx5_regex_dev_remove(struct mlx5_common_device *cdev)
{
	char name[RTE_REGEXDEV_NAME_MAX_LEN];
	struct rte_regexdev *dev;
	struct mlx5_regex_priv *priv = NULL;

	mlx5_regex_get_name(name, cdev->dev);
	dev = rte_regexdev_get_device_by_name(name);
	if (!dev)
		return 0;
	priv = dev->data->dev_private;
	if (priv) {
		if (priv->uar)
			mlx5_glue->devx_free_uar(priv->uar);
		if (priv->regexdev)
			rte_regexdev_unregister(priv->regexdev);
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
