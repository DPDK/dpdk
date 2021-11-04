/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef _QAT_COMP_PMD_GENS_H_
#define _QAT_COMP_PMD_GENS_H_

#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>
#include <stdint.h>

#include "qat_comp_pmd.h"

extern const struct rte_compressdev_capabilities qat_gen1_comp_capabilities[];

struct qat_comp_capabilities_info
qat_comp_cap_get_gen1(struct qat_pci_device *qat_dev);

uint16_t qat_comp_get_ram_bank_flags_gen1(void);

int qat_comp_set_slice_cfg_word_gen1(struct qat_comp_xform *qat_xform,
		const struct rte_comp_xform *xform,
		enum rte_comp_op_type op_type,
		uint32_t *comp_slice_cfg_word);

uint64_t qat_comp_get_features_gen1(void);

extern struct rte_compressdev_ops qat_comp_ops_gen1;

#endif /* _QAT_COMP_PMD_GENS_H_ */
