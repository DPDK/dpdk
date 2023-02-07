/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_common.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_kvargs.h>
#include <rte_mldev.h>
#include <rte_mldev_pmd.h>
#include <rte_pci.h>

#include <roc_api.h>

#include "cn10k_ml_dev.h"
#include "cn10k_ml_ops.h"

#define CN10K_ML_FW_PATH "fw_path"

#define CN10K_ML_FW_PATH_DEFAULT "/lib/firmware/mlip-fw.bin"

static const char *const valid_args[] = {CN10K_ML_FW_PATH, NULL};

/* Dummy operations for ML device */
struct rte_ml_dev_ops ml_dev_dummy_ops = {0};

static int
parse_string_arg(const char *key __rte_unused, const char *value, void *extra_args)
{
	if (value == NULL || extra_args == NULL)
		return -EINVAL;

	*(char **)extra_args = strdup(value);

	if (!*(char **)extra_args)
		return -ENOMEM;

	return 0;
}

static int
cn10k_mldev_parse_devargs(struct rte_devargs *devargs, struct cn10k_ml_dev *mldev)
{
	struct rte_kvargs *kvlist = NULL;
	bool fw_path_set = false;
	char *fw_path = NULL;
	int ret = 0;

	if (devargs == NULL)
		goto check_args;

	kvlist = rte_kvargs_parse(devargs->args, valid_args);
	if (kvlist == NULL) {
		plt_err("Error parsing devargs\n");
		return -EINVAL;
	}

	if (rte_kvargs_count(kvlist, CN10K_ML_FW_PATH) == 1) {
		ret = rte_kvargs_process(kvlist, CN10K_ML_FW_PATH, &parse_string_arg, &fw_path);
		if (ret < 0) {
			plt_err("Error processing arguments, key = %s\n", CN10K_ML_FW_PATH);
			ret = -EINVAL;
			goto exit;
		}
		fw_path_set = true;
	}

check_args:
	if (!fw_path_set)
		mldev->fw.path = CN10K_ML_FW_PATH_DEFAULT;
	else
		mldev->fw.path = fw_path;
	plt_info("ML: %s = %s", CN10K_ML_FW_PATH, mldev->fw.path);

exit:
	if (kvlist)
		rte_kvargs_free(kvlist);

	return ret;
}

static int
cn10k_ml_pci_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct rte_ml_dev_pmd_init_params init_params;
	struct cn10k_ml_dev *mldev;
	char name[RTE_ML_STR_MAX];
	struct rte_ml_dev *dev;
	int ret;

	PLT_SET_USED(pci_drv);

	init_params = (struct rte_ml_dev_pmd_init_params){
		.socket_id = rte_socket_id(), .private_data_size = sizeof(struct cn10k_ml_dev)};

	ret = roc_plt_init();
	if (ret < 0) {
		plt_err("Failed to initialize platform model");
		return ret;
	}

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));
	dev = rte_ml_dev_pmd_create(name, &pci_dev->device, &init_params);
	if (dev == NULL) {
		ret = -ENODEV;
		goto error_exit;
	}

	/* Get private data space allocated */
	mldev = dev->data->dev_private;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mldev->roc.pci_dev = pci_dev;

		ret = cn10k_mldev_parse_devargs(dev->device->devargs, mldev);
		if (ret) {
			plt_err("Failed to parse devargs ret = %d", ret);
			goto pmd_destroy;
		}

		ret = roc_ml_dev_init(&mldev->roc);
		if (ret) {
			plt_err("Failed to initialize ML ROC, ret = %d", ret);
			goto pmd_destroy;
		}

		dev->dev_ops = &cn10k_ml_ops;
	} else {
		plt_err("CN10K ML Ops are not supported on secondary process");
		dev->dev_ops = &ml_dev_dummy_ops;
	}

	dev->enqueue_burst = NULL;
	dev->dequeue_burst = NULL;
	dev->op_error_get = NULL;

	mldev->state = ML_CN10K_DEV_STATE_PROBED;

	return 0;

pmd_destroy:
	rte_ml_dev_pmd_destroy(dev);

error_exit:
	plt_err("Could not create device (vendor_id: 0x%x device_id: 0x%x)", pci_dev->id.vendor_id,
		pci_dev->id.device_id);

	return ret;
}

static int
cn10k_ml_pci_remove(struct rte_pci_device *pci_dev)
{
	struct cn10k_ml_dev *mldev;
	char name[RTE_ML_STR_MAX];
	struct rte_ml_dev *dev;
	int ret;

	if (pci_dev == NULL)
		return -EINVAL;

	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dev = rte_ml_dev_pmd_get_named_dev(name);
	if (dev == NULL)
		return -ENODEV;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mldev = dev->data->dev_private;
		ret = roc_ml_dev_fini(&mldev->roc);
		if (ret)
			return ret;
	}

	return rte_ml_dev_pmd_destroy(dev);
}

static struct rte_pci_id pci_id_ml_table[] = {
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN10K_ML_PF)},
	/* sentinel */
	{},
};

static struct rte_pci_driver cn10k_mldev_pmd = {
	.id_table = pci_id_ml_table,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cn10k_ml_pci_probe,
	.remove = cn10k_ml_pci_remove,
};

RTE_PMD_REGISTER_PCI(MLDEV_NAME_CN10K_PMD, cn10k_mldev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(MLDEV_NAME_CN10K_PMD, pci_id_ml_table);
RTE_PMD_REGISTER_KMOD_DEP(MLDEV_NAME_CN10K_PMD, "vfio-pci");

RTE_PMD_REGISTER_PARAM_STRING(MLDEV_NAME_CN10K_PMD, CN10K_ML_FW_PATH "=<path>");
