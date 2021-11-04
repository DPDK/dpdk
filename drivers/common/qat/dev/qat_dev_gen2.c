/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_device.h"
#include "adf_transport_access_macros.h"
#include "qat_dev_gens.h"

#include <stdint.h>

static struct qat_dev_hw_spec_funcs qat_dev_hw_spec_gen2 = {
	.qat_dev_reset_ring_pairs = qat_reset_ring_pairs_gen1,
	.qat_dev_get_transport_bar = qat_dev_get_transport_bar_gen1,
	.qat_dev_get_misc_bar = qat_dev_get_misc_bar_gen1,
	.qat_dev_read_config = qat_dev_read_config_gen1,
	.qat_dev_get_extra_size = qat_dev_get_extra_size_gen1,
};

RTE_INIT(qat_dev_gen_gen2_init)
{
	qat_dev_hw_spec[QAT_GEN2] = &qat_dev_hw_spec_gen2;
	qat_gen_config[QAT_GEN2].dev_gen = QAT_GEN2;
}
