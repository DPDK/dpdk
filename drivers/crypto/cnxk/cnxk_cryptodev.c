/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_cpt.h"

#include "cnxk_cryptodev.h"

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
