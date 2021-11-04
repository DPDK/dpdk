/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_device.h"
#include "qat_qp.h"
#include "adf_transport_access_macros.h"
#include "qat_dev_gens.h"

#include <stdint.h>

__extension__
const struct qat_qp_hw_data qat_gen3_qps[QAT_MAX_SERVICES]
					 [ADF_MAX_QPS_ON_ANY_SERVICE] = {
	/* queue pairs which provide an asymmetric crypto service */
	[QAT_SERVICE_ASYMMETRIC] = {
		{
			.service_type = QAT_SERVICE_ASYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 0,
			.rx_ring_num = 4,
			.tx_msg_size = 64,
			.rx_msg_size = 32,
		}
	},
	/* queue pairs which provide a symmetric crypto service */
	[QAT_SERVICE_SYMMETRIC] = {
		{
			.service_type = QAT_SERVICE_SYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 1,
			.rx_ring_num = 5,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}
	},
	/* queue pairs which provide a compression service */
	[QAT_SERVICE_COMPRESSION] = {
		{
			.service_type = QAT_SERVICE_COMPRESSION,
			.hw_bundle_num = 0,
			.tx_ring_num = 3,
			.rx_ring_num = 7,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}
	}
};


static const struct qat_qp_hw_data *
qat_qp_get_hw_data_gen3(struct qat_pci_device *dev __rte_unused,
		enum qat_service_type service_type, uint16_t qp_id)
{
	return qat_gen3_qps[service_type] + qp_id;
}

static struct qat_qp_hw_spec_funcs qat_qp_hw_spec_gen3 = {
	.qat_qp_rings_per_service  = qat_qp_rings_per_service_gen1,
	.qat_qp_build_ring_base = qat_qp_csr_build_ring_base_gen1,
	.qat_qp_adf_arb_enable = qat_qp_adf_arb_enable_gen1,
	.qat_qp_adf_arb_disable = qat_qp_adf_arb_disable_gen1,
	.qat_qp_adf_configure_queues = qat_qp_adf_configure_queues_gen1,
	.qat_qp_csr_write_tail = qat_qp_csr_write_tail_gen1,
	.qat_qp_csr_write_head = qat_qp_csr_write_head_gen1,
	.qat_qp_csr_setup = qat_qp_csr_setup_gen1,
	.qat_qp_get_hw_data = qat_qp_get_hw_data_gen3
};

static struct qat_dev_hw_spec_funcs qat_dev_hw_spec_gen3 = {
	.qat_dev_reset_ring_pairs = qat_reset_ring_pairs_gen1,
	.qat_dev_get_transport_bar = qat_dev_get_transport_bar_gen1,
	.qat_dev_get_misc_bar = qat_dev_get_misc_bar_gen1,
	.qat_dev_read_config = qat_dev_read_config_gen1,
	.qat_dev_get_extra_size = qat_dev_get_extra_size_gen1,
};

RTE_INIT(qat_dev_gen_gen3_init)
{
	qat_qp_hw_spec[QAT_GEN3] = &qat_qp_hw_spec_gen3;
	qat_dev_hw_spec[QAT_GEN3] = &qat_dev_hw_spec_gen3;
	qat_gen_config[QAT_GEN3].dev_gen = QAT_GEN3;
}
