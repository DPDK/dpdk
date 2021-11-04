/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_device.h"
#include "adf_transport_access_macros.h"
#include "qat_dev_gens.h"

#include <stdint.h>

#define ADF_ARB_REG_SLOT			0x1000

int
qat_reset_ring_pairs_gen1(struct qat_pci_device *qat_pci_dev __rte_unused)
{
	/*
	 * Ring pairs reset not supported on base, continue
	 */
	return 0;
}

const struct rte_mem_resource *
qat_dev_get_transport_bar_gen1(struct rte_pci_device *pci_dev)
{
	return &pci_dev->mem_resource[0];
}

int
qat_dev_get_misc_bar_gen1(struct rte_mem_resource **mem_resource __rte_unused,
		struct rte_pci_device *pci_dev __rte_unused)
{
	return -1;
}

int
qat_dev_read_config_gen1(struct qat_pci_device *qat_dev __rte_unused)
{
	/*
	 * Base generations do not have configuration,
	 * but set this pointer anyway that we can
	 * distinguish higher generations faulty set to NULL
	 */
	return 0;
}

int
qat_dev_get_extra_size_gen1(void)
{
	return 0;
}

static struct qat_dev_hw_spec_funcs qat_dev_hw_spec_gen1 = {
	.qat_dev_reset_ring_pairs = qat_reset_ring_pairs_gen1,
	.qat_dev_get_transport_bar = qat_dev_get_transport_bar_gen1,
	.qat_dev_get_misc_bar = qat_dev_get_misc_bar_gen1,
	.qat_dev_read_config = qat_dev_read_config_gen1,
	.qat_dev_get_extra_size = qat_dev_get_extra_size_gen1,
};

RTE_INIT(qat_dev_gen_gen1_init)
{
	qat_dev_hw_spec[QAT_GEN1] = &qat_dev_hw_spec_gen1;
	qat_gen_config[QAT_GEN1].dev_gen = QAT_GEN1;
}
