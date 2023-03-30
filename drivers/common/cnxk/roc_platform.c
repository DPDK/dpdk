/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_log.h>

#include "roc_api.h"

#define PLT_INIT_CB_MAX 8

static int plt_init_cb_num;
static roc_plt_init_cb_t plt_init_cbs[PLT_INIT_CB_MAX];

int
roc_plt_init_cb_register(roc_plt_init_cb_t cb)
{
	if (plt_init_cb_num >= PLT_INIT_CB_MAX)
		return -ERANGE;

	plt_init_cbs[plt_init_cb_num++] = cb;
	return 0;
}

int
roc_plt_init(void)
{
	const struct rte_memzone *mz;
	int i, rc;

	mz = rte_memzone_lookup(PLT_MODEL_MZ_NAME);
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		if (mz == NULL) {
			mz = rte_memzone_reserve(PLT_MODEL_MZ_NAME,
						 sizeof(struct roc_model),
						 SOCKET_ID_ANY, 0);
			if (mz == NULL) {
				plt_err("Failed to reserve mem for roc_model");
				return -ENOMEM;
			}
			if (roc_model_init(mz->addr)) {
				plt_err("Failed to init roc_model");
				rte_memzone_free(mz);
				return -EINVAL;
			}
		}
	} else {
		if (mz == NULL) {
			plt_err("Failed to lookup mem for roc_model");
			return -ENOMEM;
		}
		roc_model = mz->addr;
	}

	for (i = 0; i < plt_init_cb_num; i++) {
		rc = (*plt_init_cbs[i])();
		if (rc)
			return rc;
	}

	return 0;
}

RTE_LOG_REGISTER(cnxk_logtype_base, pmd.cnxk.base, NOTICE);
RTE_LOG_REGISTER(cnxk_logtype_mbox, pmd.cnxk.mbox, NOTICE);
RTE_LOG_REGISTER(cnxk_logtype_cpt, pmd.crypto.cnxk, NOTICE);
RTE_LOG_REGISTER(cnxk_logtype_ml, pmd.ml.cnxk, NOTICE);
RTE_LOG_REGISTER(cnxk_logtype_npa, pmd.mempool.cnxk, NOTICE);
RTE_LOG_REGISTER(cnxk_logtype_nix, pmd.net.cnxk, NOTICE);
RTE_LOG_REGISTER(cnxk_logtype_npc, pmd.net.cnxk.flow, NOTICE);
RTE_LOG_REGISTER(cnxk_logtype_sso, pmd.event.cnxk, NOTICE);
RTE_LOG_REGISTER(cnxk_logtype_tim, pmd.event.cnxk.timer, NOTICE);
RTE_LOG_REGISTER(cnxk_logtype_tm, pmd.net.cnxk.tm, NOTICE);
RTE_LOG_REGISTER_DEFAULT(cnxk_logtype_ree, NOTICE);
