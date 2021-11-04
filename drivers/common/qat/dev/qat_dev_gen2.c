/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_device.h"
#include "qat_qp.h"
#include "adf_transport_access_macros.h"
#include "qat_dev_gens.h"

#include <stdint.h>

static struct qat_qp_hw_spec_funcs qat_qp_hw_spec_gen2 = {
	.qat_qp_rings_per_service = qat_qp_rings_per_service_gen1,
	.qat_qp_build_ring_base = qat_qp_csr_build_ring_base_gen1,
	.qat_qp_adf_arb_enable = qat_qp_adf_arb_enable_gen1,
	.qat_qp_adf_arb_disable = qat_qp_adf_arb_disable_gen1,
	.qat_qp_adf_configure_queues = qat_qp_adf_configure_queues_gen1,
	.qat_qp_csr_write_tail = qat_qp_csr_write_tail_gen1,
	.qat_qp_csr_write_head = qat_qp_csr_write_head_gen1,
	.qat_qp_csr_setup = qat_qp_csr_setup_gen1,
	.qat_qp_get_hw_data = qat_qp_get_hw_data_gen1,
};

static struct qat_dev_hw_spec_funcs qat_dev_hw_spec_gen2 = {
	.qat_dev_reset_ring_pairs = qat_reset_ring_pairs_gen1,
	.qat_dev_get_transport_bar = qat_dev_get_transport_bar_gen1,
	.qat_dev_get_misc_bar = qat_dev_get_misc_bar_gen1,
	.qat_dev_read_config = qat_dev_read_config_gen1,
	.qat_dev_get_extra_size = qat_dev_get_extra_size_gen1,
};

RTE_INIT(qat_dev_gen_gen2_init)
{
	qat_qp_hw_spec[QAT_GEN2] = &qat_qp_hw_spec_gen2;
	qat_dev_hw_spec[QAT_GEN2] = &qat_dev_hw_spec_gen2;
	qat_gen_config[QAT_GEN2].dev_gen = QAT_GEN2;
}
