/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */
#include <string.h>

#include <rte_common.h>

#include "otx_cryptodev_hw_access.h"

#include "cpt_pmd_logs.h"

static int
otx_cpt_vf_init(struct cpt_vf *cptvf)
{
	int ret = 0;

	CPT_LOG_DP_DEBUG("%s: %s done", cptvf->dev_name, __func__);

	return ret;
}

void
otx_cpt_poll_misc(struct cpt_vf *cptvf)
{
	RTE_SET_USED(cptvf);
}

int
otx_cpt_hw_init(struct cpt_vf *cptvf, void *pdev, void *reg_base, char *name)
{
	memset(cptvf, 0, sizeof(struct cpt_vf));

	/* Bar0 base address */
	cptvf->reg_base = reg_base;
	strncpy(cptvf->dev_name, name, 32);

	cptvf->pdev = pdev;

	/* To clear if there are any pending mbox msgs */
	otx_cpt_poll_misc(cptvf);

	if (otx_cpt_vf_init(cptvf)) {
		CPT_LOG_ERR("Failed to initialize CPT VF device");
		return -1;
	}

	return 0;
}
