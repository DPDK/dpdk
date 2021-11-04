/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_comp_pmd.h"
#include "qat_comp_pmd_gens.h"

#define QAT_NUM_INTERM_BUFS_GEN2 20

static unsigned int
qat_comp_get_num_im_bufs_required_gen2(void)
{
	return QAT_NUM_INTERM_BUFS_GEN2;
}

RTE_INIT(qat_comp_pmd_gen2_init)
{
	qat_comp_gen_dev_ops[QAT_GEN2].compressdev_ops =
			&qat_comp_ops_gen1;
	qat_comp_gen_dev_ops[QAT_GEN2].qat_comp_get_capabilities =
			qat_comp_cap_get_gen1;
	qat_comp_gen_dev_ops[QAT_GEN2].qat_comp_get_num_im_bufs_required =
			qat_comp_get_num_im_bufs_required_gen2;
	qat_comp_gen_dev_ops[QAT_GEN2].qat_comp_get_ram_bank_flags =
			qat_comp_get_ram_bank_flags_gen1;
	qat_comp_gen_dev_ops[QAT_GEN2].qat_comp_set_slice_cfg_word =
			qat_comp_set_slice_cfg_word_gen1;
	qat_comp_gen_dev_ops[QAT_GEN2].qat_comp_get_feature_flags =
			qat_comp_get_features_gen1;
}
