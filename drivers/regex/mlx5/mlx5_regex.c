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

#include <mlx5_common_pci.h>
#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>

#include "mlx5_regex.h"
#include "mlx5_regex_utils.h"
#include "mlx5_rxp_csrs.h"

#define MLX5_REGEX_DRIVER_NAME regex_mlx5
#define MLX5_REGEX_LOG_NAME    pmd.regex.mlx5

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

static struct ibv_device *
mlx5_regex_get_ib_device_match(struct rte_pci_addr *addr)
{
	int n;
	struct ibv_device **ibv_list = mlx5_glue->get_device_list(&n);
	struct ibv_device *ibv_match = NULL;

	if (!ibv_list) {
		rte_errno = ENOSYS;
		return NULL;
	}
	while (n-- > 0) {
		struct rte_pci_addr pci_addr;

		DRV_LOG(DEBUG, "Checking device \"%s\"..", ibv_list[n]->name);
		if (mlx5_dev_to_pci_addr(ibv_list[n]->ibdev_path, &pci_addr))
			continue;
		if (rte_pci_addr_cmp(addr, &pci_addr))
			continue;
		ibv_match = ibv_list[n];
		break;
	}
	if (!ibv_match)
		rte_errno = ENOENT;
	mlx5_glue->free_device_list(ibv_list);
	return ibv_match;
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
mlx5_regex_get_name(char *name, struct rte_pci_device *pci_dev __rte_unused)
{
	sprintf(name, "mlx5_regex_%02x:%02x.%02x", pci_dev->addr.bus,
		pci_dev->addr.devid, pci_dev->addr.function);
}

static int
mlx5_regex_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		     struct rte_pci_device *pci_dev)
{
	struct ibv_device *ibv;
	struct mlx5_regex_priv *priv = NULL;
	struct ibv_context *ctx = NULL;
	struct mlx5_hca_attr attr;
	char name[RTE_REGEXDEV_NAME_MAX_LEN];
	int ret;

	ibv = mlx5_regex_get_ib_device_match(&pci_dev->addr);
	if (!ibv) {
		DRV_LOG(ERR, "No matching IB device for PCI slot "
			PCI_PRI_FMT ".", pci_dev->addr.domain,
			pci_dev->addr.bus, pci_dev->addr.devid,
			pci_dev->addr.function);
		return -rte_errno;
	}
	DRV_LOG(INFO, "PCI information matches for device \"%s\".",
		ibv->name);
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
		goto error;
	}
	priv->ctx = ctx;
	priv->nb_engines = 2; /* attr.regexp_num_of_engines */
	/* Default RXP programming mode to Shared. */
	priv->prog_mode = MLX5_RXP_SHARED_PROG_MODE;
	mlx5_regex_get_name(name, pci_dev);
	priv->regexdev = rte_regexdev_register(name);
	if (priv->regexdev == NULL) {
		DRV_LOG(ERR, "Failed to register RegEx device.");
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}
	ret = mlx5_glue->devx_query_eqn(ctx, 0, &priv->eqn);
	if (ret) {
		DRV_LOG(ERR, "can't query event queue number.");
		rte_errno = ENOMEM;
		goto error;
	}
	priv->uar = mlx5_glue->devx_alloc_uar(ctx, 0);
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
	priv->regexdev->dequeue = mlx5_regexdev_dequeue;
	priv->regexdev->device = (struct rte_device *)pci_dev;
	priv->regexdev->data->dev_private = priv;
	priv->regexdev->state = RTE_REGEXDEV_READY;
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
mlx5_regex_pci_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_REGEXDEV_NAME_MAX_LEN];
	struct rte_regexdev *dev;
	struct mlx5_regex_priv *priv = NULL;

	mlx5_regex_get_name(name, pci_dev);
	dev = rte_regexdev_get_device_by_name(name);
	if (!dev)
		return 0;
	priv = dev->data->dev_private;
	if (priv) {
		if (priv->pd)
			mlx5_glue->dealloc_pd(priv->pd);
		if (priv->uar)
			mlx5_glue->devx_free_uar(priv->uar);
		if (priv->regexdev)
			rte_regexdev_unregister(priv->regexdev);
		if (priv->ctx)
			mlx5_glue->close_device(priv->ctx);
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
		.vendor_id = 0
	}
};

static struct mlx5_pci_driver mlx5_regex_driver = {
	.driver_class = MLX5_CLASS_REGEX,
	.pci_driver = {
		.driver = {
			.name = RTE_STR(MLX5_REGEX_DRIVER_NAME),
		},
		.id_table = mlx5_regex_pci_id_map,
		.probe = mlx5_regex_pci_probe,
		.remove = mlx5_regex_pci_remove,
		.drv_flags = 0,
	},
};

RTE_INIT(rte_mlx5_regex_init)
{
	mlx5_common_init();
	if (mlx5_glue)
		mlx5_pci_driver_register(&mlx5_regex_driver);
}

RTE_LOG_REGISTER(mlx5_regex_logtype, MLX5_REGEX_LOG_NAME, NOTICE)
RTE_PMD_EXPORT_NAME(MLX5_REGEX_DRIVER_NAME, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(MLX5_REGEX_DRIVER_NAME, mlx5_regex_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(MLX5_REGEX_DRIVER_NAME, "* ib_uverbs & mlx5_core & mlx5_ib");
