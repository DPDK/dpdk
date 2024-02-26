/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#include <dev_driver.h>
#include <rte_pci.h>

#include "qat_device.h"
#include "qat_qp.h"
#include "adf_pf2vf_msg.h"
#include "qat_dev_gens.h"

#include <stdint.h>

static struct qat_pf2vf_dev qat_pf2vf_gen5 = {
	.pf2vf_offset = ADF_4XXXIOV_PF2VM_OFFSET,
	.vf2pf_offset = ADF_4XXXIOV_VM2PF_OFFSET,
	.pf2vf_type_shift = ADF_PFVF_2X_MSGTYPE_SHIFT,
	.pf2vf_type_mask = ADF_PFVF_2X_MSGTYPE_MASK,
	.pf2vf_data_shift = ADF_PFVF_2X_MSGDATA_SHIFT,
	.pf2vf_data_mask = ADF_PFVF_2X_MSGDATA_MASK,
};

static struct qat_qp_hw_spec_funcs qat_qp_hw_spec_gen5 = {
	.qat_qp_rings_per_service = qat_qp_rings_per_service_gen4,
	.qat_qp_build_ring_base = qat_qp_build_ring_base_gen4,
	.qat_qp_adf_arb_enable = qat_qp_adf_arb_enable_gen4,
	.qat_qp_adf_arb_disable = qat_qp_adf_arb_disable_gen4,
	.qat_qp_adf_configure_queues = qat_qp_adf_configure_queues_gen4,
	.qat_qp_csr_write_tail = qat_qp_csr_write_tail_gen4,
	.qat_qp_csr_write_head = qat_qp_csr_write_head_gen4,
	.qat_qp_csr_setup = qat_qp_csr_setup_gen4,
	.qat_qp_get_hw_data = qat_qp_get_hw_data_gen4,
};

static struct qat_dev_hw_spec_funcs qat_dev_hw_spec_gen5 = {
	.qat_dev_reset_ring_pairs = qat_reset_ring_pairs_gen4,
	.qat_dev_get_transport_bar = qat_dev_get_transport_bar_gen4,
	.qat_dev_get_misc_bar = qat_dev_get_misc_bar_gen4,
	.qat_dev_read_config = qat_dev_read_config_gen4,
	.qat_dev_get_extra_size = qat_dev_get_extra_size_gen4,
	.qat_dev_get_slice_map = qat_dev_get_slice_map_gen4,
};

RTE_INIT(qat_dev_gen_5_init)
{
	qat_qp_hw_spec[QAT_GEN5] = &qat_qp_hw_spec_gen5;
	qat_dev_hw_spec[QAT_GEN5] = &qat_dev_hw_spec_gen5;
	qat_gen_config[QAT_GEN5].dev_gen = QAT_GEN5;
	qat_gen_config[QAT_GEN5].pf2vf_dev = &qat_pf2vf_gen5;
}
