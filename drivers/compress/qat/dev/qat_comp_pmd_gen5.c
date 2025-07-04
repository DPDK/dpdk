/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#include "qat_comp.h"
#include "qat_comp_pmd.h"
#include "qat_comp_pmd_gens.h"
#include "icp_qat_hw_gen4_comp.h"
#include "icp_qat_hw_gen4_comp_defs.h"

static const struct rte_compressdev_capabilities
qat_gen5_comp_capabilities[] = {
	{/* COMPRESSION - deflate */
	 .algo = RTE_COMP_ALGO_DEFLATE,
	 .comp_feature_flags = RTE_COMP_FF_MULTI_PKT_CHECKSUM |
				RTE_COMP_FF_CRC32_CHECKSUM |
				RTE_COMP_FF_ADLER32_CHECKSUM |
				RTE_COMP_FF_CRC32_ADLER32_CHECKSUM |
				RTE_COMP_FF_SHAREABLE_PRIV_XFORM |
				RTE_COMP_FF_HUFFMAN_FIXED |
				RTE_COMP_FF_HUFFMAN_DYNAMIC |
				RTE_COMP_FF_OOP_SGL_IN_SGL_OUT |
				RTE_COMP_FF_OOP_SGL_IN_LB_OUT |
				RTE_COMP_FF_OOP_LB_IN_SGL_OUT,
	 .window_size = {.min = 15, .max = 15, .increment = 0} },
	 RTE_COMP_END_OF_CAPABILITIES_LIST() };

static struct rte_compressdev_ops qat_comp_ops_gen5 = {

	/* Device related operations */
	.dev_configure		= qat_comp_dev_config_gen4,
	.dev_start		= qat_comp_dev_start,
	.dev_stop		= qat_comp_dev_stop,
	.dev_close		= qat_comp_dev_close,
	.dev_infos_get		= qat_comp_dev_info_get,

	.stats_get		= qat_comp_stats_get,
	.stats_reset		= qat_comp_stats_reset,
	.queue_pair_setup	= qat_comp_qp_setup,
	.queue_pair_release	= qat_comp_qp_release,

	/* Compression related operations */
	.private_xform_create	= qat_comp_private_xform_create,
	.private_xform_free	= qat_comp_private_xform_free,
	.stream_create		= qat_comp_stream_create,
	.stream_free		= qat_comp_stream_free
};

static struct qat_comp_capabilities_info
qat_comp_cap_get_gen5(struct qat_pci_device *qat_dev __rte_unused)
{
	struct qat_comp_capabilities_info capa_info = {
		.data = qat_gen5_comp_capabilities,
		.size = sizeof(qat_gen5_comp_capabilities)
	};
	return capa_info;
}

RTE_INIT(qat_comp_pmd_gen5_init)
{
	qat_comp_gen_dev_ops[QAT_GEN5].compressdev_ops =
			&qat_comp_ops_gen5;
	qat_comp_gen_dev_ops[QAT_GEN5].qat_comp_get_capabilities =
			qat_comp_cap_get_gen5;
	qat_comp_gen_dev_ops[QAT_GEN5].qat_comp_get_num_im_bufs_required =
			qat_comp_get_num_im_bufs_required_gen4;
	qat_comp_gen_dev_ops[QAT_GEN5].qat_comp_get_ram_bank_flags =
			qat_comp_get_ram_bank_flags_gen4;
	qat_comp_gen_dev_ops[QAT_GEN5].qat_comp_set_slice_cfg_word =
			qat_comp_set_slice_cfg_word_gen4;
	qat_comp_gen_dev_ops[QAT_GEN5].qat_comp_get_feature_flags =
			qat_comp_get_features_gen1;
}
