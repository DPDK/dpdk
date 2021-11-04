/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#include "qat_comp_pmd.h"
#include "qat_comp.h"
#include "qat_comp_pmd_gens.h"

#define QAT_NUM_INTERM_BUFS_GEN1 12

const struct rte_compressdev_capabilities qat_gen1_comp_capabilities[] = {
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
				RTE_COMP_FF_OOP_LB_IN_SGL_OUT |
				RTE_COMP_FF_STATEFUL_DECOMPRESSION,
	 .window_size = {.min = 15, .max = 15, .increment = 0} },
	{RTE_COMP_ALGO_LIST_END, 0, {0, 0, 0} } };

static int
qat_comp_dev_config_gen1(struct rte_compressdev *dev,
		struct rte_compressdev_config *config)
{
	struct qat_comp_dev_private *comp_dev = dev->data->dev_private;

	if (RTE_PMD_QAT_COMP_IM_BUFFER_SIZE == 0) {
		QAT_LOG(WARNING,
			"RTE_PMD_QAT_COMP_IM_BUFFER_SIZE = 0 in config file, so"
			" QAT device can't be used for Dynamic Deflate.");
	} else {
		comp_dev->interm_buff_mz =
				qat_comp_setup_inter_buffers(comp_dev,
					RTE_PMD_QAT_COMP_IM_BUFFER_SIZE);
		if (comp_dev->interm_buff_mz == NULL)
			return -ENOMEM;
	}

	return qat_comp_dev_config(dev, config);
}

struct rte_compressdev_ops qat_comp_ops_gen1 = {

	/* Device related operations */
	.dev_configure		= qat_comp_dev_config_gen1,
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

struct qat_comp_capabilities_info
qat_comp_cap_get_gen1(struct qat_pci_device *qat_dev __rte_unused)
{
	struct qat_comp_capabilities_info capa_info = {
		.data = qat_gen1_comp_capabilities,
		.size = sizeof(qat_gen1_comp_capabilities)
	};
	return capa_info;
}

uint16_t
qat_comp_get_ram_bank_flags_gen1(void)
{
	/* Enable A, B, C, D, and E (CAMs). */
	return ICP_QAT_FW_COMP_RAM_FLAGS_BUILD(
			ICP_QAT_FW_COMP_BANK_DISABLED, /* Bank I */
			ICP_QAT_FW_COMP_BANK_DISABLED, /* Bank H */
			ICP_QAT_FW_COMP_BANK_DISABLED, /* Bank G */
			ICP_QAT_FW_COMP_BANK_DISABLED, /* Bank F */
			ICP_QAT_FW_COMP_BANK_ENABLED,  /* Bank E */
			ICP_QAT_FW_COMP_BANK_ENABLED,  /* Bank D */
			ICP_QAT_FW_COMP_BANK_ENABLED,  /* Bank C */
			ICP_QAT_FW_COMP_BANK_ENABLED,  /* Bank B */
			ICP_QAT_FW_COMP_BANK_ENABLED); /* Bank A */
}

int
qat_comp_set_slice_cfg_word_gen1(struct qat_comp_xform *qat_xform,
		const struct rte_comp_xform *xform,
		__rte_unused enum rte_comp_op_type op_type,
		uint32_t *comp_slice_cfg_word)
{
	unsigned int algo, comp_level, direction;

	if (xform->compress.algo == RTE_COMP_ALGO_DEFLATE)
		algo = ICP_QAT_HW_COMPRESSION_ALGO_DEFLATE;
	else {
		QAT_LOG(ERR, "compression algorithm not supported");
		return -EINVAL;
	}

	if (qat_xform->qat_comp_request_type == QAT_COMP_REQUEST_DECOMPRESS) {
		direction = ICP_QAT_HW_COMPRESSION_DIR_DECOMPRESS;
		comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_8;
	} else {
		direction = ICP_QAT_HW_COMPRESSION_DIR_COMPRESS;

		if (xform->compress.level == RTE_COMP_LEVEL_PMD_DEFAULT)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_8;
		else if (xform->compress.level == 1)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_1;
		else if (xform->compress.level == 2)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_4;
		else if (xform->compress.level == 3)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_8;
		else if (xform->compress.level >= 4 &&
			 xform->compress.level <= 9)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_16;
		else {
			QAT_LOG(ERR, "compression level not supported");
			return -EINVAL;
		}
	}

	comp_slice_cfg_word[0] =
			ICP_QAT_HW_COMPRESSION_CONFIG_BUILD(
				direction,
				/* In CPM 1.6 only valid mode ! */
				ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_ENABLED,
				algo,
				/* Translate level to depth */
				comp_level,
				ICP_QAT_HW_COMPRESSION_FILE_TYPE_0);

	return 0;
}

static unsigned int
qat_comp_get_num_im_bufs_required_gen1(void)
{
	return QAT_NUM_INTERM_BUFS_GEN1;
}

uint64_t
qat_comp_get_features_gen1(void)
{
	return RTE_COMPDEV_FF_HW_ACCELERATED;
}

RTE_INIT(qat_comp_pmd_gen1_init)
{
	qat_comp_gen_dev_ops[QAT_GEN1].compressdev_ops =
			&qat_comp_ops_gen1;
	qat_comp_gen_dev_ops[QAT_GEN1].qat_comp_get_capabilities =
			qat_comp_cap_get_gen1;
	qat_comp_gen_dev_ops[QAT_GEN1].qat_comp_get_num_im_bufs_required =
			qat_comp_get_num_im_bufs_required_gen1;
	qat_comp_gen_dev_ops[QAT_GEN1].qat_comp_get_ram_bank_flags =
			qat_comp_get_ram_bank_flags_gen1;
	qat_comp_gen_dev_ops[QAT_GEN1].qat_comp_set_slice_cfg_word =
			qat_comp_set_slice_cfg_word_gen1;
	qat_comp_gen_dev_ops[QAT_GEN1].qat_comp_get_feature_flags =
			qat_comp_get_features_gen1;
}
