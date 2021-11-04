/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_comp.h"
#include "qat_comp_pmd.h"
#include "qat_comp_pmd_gens.h"
#include "icp_qat_hw_gen4_comp.h"
#include "icp_qat_hw_gen4_comp_defs.h"

#define QAT_NUM_INTERM_BUFS_GEN4 0

static const struct rte_compressdev_capabilities
qat_gen4_comp_capabilities[] = {
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
	{RTE_COMP_ALGO_LIST_END, 0, {0, 0, 0} } };

static int
qat_comp_dev_config_gen4(struct rte_compressdev *dev,
		struct rte_compressdev_config *config)
{
	/* QAT GEN4 doesn't need preallocated intermediate buffers */

	return qat_comp_dev_config(dev, config);
}

static struct rte_compressdev_ops qat_comp_ops_gen4 = {

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
qat_comp_cap_get_gen4(struct qat_pci_device *qat_dev __rte_unused)
{
	struct qat_comp_capabilities_info capa_info = {
		.data = qat_gen4_comp_capabilities,
		.size = sizeof(qat_gen4_comp_capabilities)
	};
	return capa_info;
}

static uint16_t
qat_comp_get_ram_bank_flags_gen4(void)
{
	return 0;
}

static int
qat_comp_set_slice_cfg_word_gen4(struct qat_comp_xform *qat_xform,
		const struct rte_comp_xform *xform,
		enum rte_comp_op_type op_type, uint32_t *comp_slice_cfg_word)
{
	if (qat_xform->qat_comp_request_type ==
			QAT_COMP_REQUEST_FIXED_COMP_STATELESS ||
	    qat_xform->qat_comp_request_type ==
			QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS) {
		/* Compression */
		struct icp_qat_hw_comp_20_config_csr_upper hw_comp_upper_csr;
		struct icp_qat_hw_comp_20_config_csr_lower hw_comp_lower_csr;

		memset(&hw_comp_upper_csr, 0, sizeof(hw_comp_upper_csr));
		memset(&hw_comp_lower_csr, 0, sizeof(hw_comp_lower_csr));

		hw_comp_lower_csr.lllbd =
			ICP_QAT_HW_COMP_20_LLLBD_CTRL_LLLBD_DISABLED;

		if (xform->compress.algo == RTE_COMP_ALGO_DEFLATE) {
			hw_comp_lower_csr.skip_ctrl =
				ICP_QAT_HW_COMP_20_BYTE_SKIP_3BYTE_LITERAL;

			if (qat_xform->qat_comp_request_type ==
				QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS) {
				hw_comp_lower_csr.algo =
					ICP_QAT_HW_COMP_20_HW_COMP_FORMAT_ILZ77;
				hw_comp_lower_csr.lllbd =
				    ICP_QAT_HW_COMP_20_LLLBD_CTRL_LLLBD_ENABLED;
			} else {
				hw_comp_lower_csr.algo =
				      ICP_QAT_HW_COMP_20_HW_COMP_FORMAT_DEFLATE;
				hw_comp_upper_csr.scb_ctrl =
					ICP_QAT_HW_COMP_20_SCB_CONTROL_DISABLE;
			}

			if (op_type == RTE_COMP_OP_STATEFUL) {
				hw_comp_upper_csr.som_ctrl =
				     ICP_QAT_HW_COMP_20_SOM_CONTROL_REPLAY_MODE;
			}
		} else {
			QAT_LOG(ERR, "Compression algorithm not supported");
			return -EINVAL;
		}

		switch (xform->compress.level) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			hw_comp_lower_csr.sd =
					ICP_QAT_HW_COMP_20_SEARCH_DEPTH_LEVEL_1;
			hw_comp_lower_csr.hash_col =
			      ICP_QAT_HW_COMP_20_SKIP_HASH_COLLISION_DONT_ALLOW;
			break;
		case 6:
		case 7:
		case 8:
		case RTE_COMP_LEVEL_PMD_DEFAULT:
			hw_comp_lower_csr.sd =
					ICP_QAT_HW_COMP_20_SEARCH_DEPTH_LEVEL_6;
			break;
		case 9:
		case 10:
		case 11:
		case 12:
			hw_comp_lower_csr.sd =
					ICP_QAT_HW_COMP_20_SEARCH_DEPTH_LEVEL_9;
			break;
		default:
			QAT_LOG(ERR, "Compression level not supported");
			return -EINVAL;
		}

		hw_comp_lower_csr.abd = ICP_QAT_HW_COMP_20_ABD_ABD_DISABLED;
		hw_comp_lower_csr.hash_update =
			ICP_QAT_HW_COMP_20_SKIP_HASH_UPDATE_DONT_ALLOW;
		hw_comp_lower_csr.edmm =
		      ICP_QAT_HW_COMP_20_EXTENDED_DELAY_MATCH_MODE_EDMM_ENABLED;

		hw_comp_upper_csr.nice =
			ICP_QAT_HW_COMP_20_CONFIG_CSR_NICE_PARAM_DEFAULT_VAL;
		hw_comp_upper_csr.lazy =
			ICP_QAT_HW_COMP_20_CONFIG_CSR_LAZY_PARAM_DEFAULT_VAL;

		comp_slice_cfg_word[0] =
				ICP_QAT_FW_COMP_20_BUILD_CONFIG_LOWER(
					hw_comp_lower_csr);
		comp_slice_cfg_word[1] =
				ICP_QAT_FW_COMP_20_BUILD_CONFIG_UPPER(
					hw_comp_upper_csr);
	} else {
		/* Decompression */
		struct icp_qat_hw_decomp_20_config_csr_lower
				hw_decomp_lower_csr;

		memset(&hw_decomp_lower_csr, 0, sizeof(hw_decomp_lower_csr));

		if (xform->compress.algo == RTE_COMP_ALGO_DEFLATE)
			hw_decomp_lower_csr.algo =
				ICP_QAT_HW_DECOMP_20_HW_DECOMP_FORMAT_DEFLATE;
		else {
			QAT_LOG(ERR, "Compression algorithm not supported");
			return -EINVAL;
		}

		comp_slice_cfg_word[0] =
				ICP_QAT_FW_DECOMP_20_BUILD_CONFIG_LOWER(
					hw_decomp_lower_csr);
		comp_slice_cfg_word[1] = 0;
	}

	return 0;
}

static unsigned int
qat_comp_get_num_im_bufs_required_gen4(void)
{
	return QAT_NUM_INTERM_BUFS_GEN4;
}


RTE_INIT(qat_comp_pmd_gen4_init)
{
	qat_comp_gen_dev_ops[QAT_GEN4].compressdev_ops =
			&qat_comp_ops_gen4;
	qat_comp_gen_dev_ops[QAT_GEN4].qat_comp_get_capabilities =
			qat_comp_cap_get_gen4;
	qat_comp_gen_dev_ops[QAT_GEN4].qat_comp_get_num_im_bufs_required =
			qat_comp_get_num_im_bufs_required_gen4;
	qat_comp_gen_dev_ops[QAT_GEN4].qat_comp_get_ram_bank_flags =
			qat_comp_get_ram_bank_flags_gen4;
	qat_comp_gen_dev_ops[QAT_GEN4].qat_comp_set_slice_cfg_word =
			qat_comp_set_slice_cfg_word_gen4;
	qat_comp_gen_dev_ops[QAT_GEN4].qat_comp_get_feature_flags =
			qat_comp_get_features_gen1;
}
