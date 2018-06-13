/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "qat_device.h"
#include "adf_transport_access_macros.h"
#include "qat_qp.h"

/* Hardware device information per generation */
__extension__
struct qat_gen_hw_data qp_gen_config[] =  {
	[QAT_GEN1] = {
		.dev_gen = QAT_GEN1,
		.qp_hw_data = qat_gen1_qps,
	},
	[QAT_GEN2] = {
		.dev_gen = QAT_GEN2,
		.qp_hw_data = qat_gen1_qps,
		/* gen2 has same ring layout as gen1 */
	},
};

int qat_dev_config(__rte_unused struct rte_cryptodev *dev,
		__rte_unused struct rte_cryptodev_config *config)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

int qat_dev_start(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

void qat_dev_stop(__rte_unused struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();
}

int qat_dev_close(struct rte_cryptodev *dev)
{
	int i, ret;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = qat_sym_qp_release(dev, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

void qat_dev_info_get(struct rte_cryptodev *dev,
			struct rte_cryptodev_info *info)
{
	struct qat_pmd_private *internals = dev->data->dev_private;
	const struct qat_qp_hw_data *sym_hw_qps =
		qp_gen_config[internals->qat_dev_gen]
			      .qp_hw_data[QAT_SERVICE_SYMMETRIC];

	PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs =
			qat_qps_per_service(sym_hw_qps, QAT_SERVICE_SYMMETRIC);
		info->feature_flags = dev->feature_flags;
		info->capabilities = internals->qat_dev_capabilities;
		info->sym.max_nb_sessions = internals->max_nb_sessions;
		info->driver_id = cryptodev_qat_driver_id;
		info->pci_dev = RTE_DEV_TO_PCI(dev->device);
	}
}
