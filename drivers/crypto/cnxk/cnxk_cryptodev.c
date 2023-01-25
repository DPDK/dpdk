/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <cryptodev_pmd.h>
#include <rte_cryptodev.h>

#include "roc_cpt.h"
#include "roc_model.h"

#include "cnxk_cryptodev.h"

uint64_t
cnxk_cpt_default_ff_get(void)
{
	uint64_t ff = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
		      RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO |
		      RTE_CRYPTODEV_FF_HW_ACCELERATED |
		      RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_QT |
		      RTE_CRYPTODEV_FF_RSA_PRIV_OP_KEY_EXP |
		      RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
		      RTE_CRYPTODEV_FF_IN_PLACE_SGL |
		      RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
		      RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
		      RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
		      RTE_CRYPTODEV_FF_SYM_SESSIONLESS |
		      RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED |
		      RTE_CRYPTODEV_FF_SECURITY;

	if (roc_model_is_cn10k())
		ff |= RTE_CRYPTODEV_FF_SECURITY_INNER_CSUM;

	return ff;
}

int
cnxk_cpt_eng_grp_add(struct roc_cpt *roc_cpt)
{
	int ret;

	ret = roc_cpt_eng_grp_add(roc_cpt, CPT_ENG_TYPE_SE);
	if (ret < 0) {
		plt_err("Could not add CPT SE engines");
		return -ENOTSUP;
	}

	ret = roc_cpt_eng_grp_add(roc_cpt, CPT_ENG_TYPE_IE);
	if (ret < 0) {
		plt_err("Could not add CPT IE engines");
		return -ENOTSUP;
	}

	ret = roc_cpt_eng_grp_add(roc_cpt, CPT_ENG_TYPE_AE);
	if (ret < 0) {
		plt_err("Could not add CPT AE engines");
		return -ENOTSUP;
	}

	return 0;
}

void
cnxk_cpt_int_misc_cb(struct roc_cpt_lf *lf, __rte_unused void *args)
{
	struct roc_cpt *roc_cpt = lf->roc_cpt;

	if (roc_cpt == NULL)
		return;

	rte_cryptodev_pmd_callback_process(roc_cpt->opaque, RTE_CRYPTODEV_EVENT_ERROR);
}
