/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_log.h>

#include "otx2_common.h"

/**
 * @internal
 */
int otx2_logtype_base;
/**
 * @internal
 */
int otx2_logtype_mbox;
/**
 * @internal
 */
int otx2_logtype_npa;
/**
 * @internal
 */
int otx2_logtype_nix;
/**
 * @internal
 */
int otx2_logtype_npc;
/**
 * @internal
 */
int otx2_logtype_tm;
/**
 * @internal
 */
int otx2_logtype_sso;
/**
 * @internal
 */
int otx2_logtype_tim;
/**
 * @internal
 */
int otx2_logtype_dpi;

RTE_INIT(otx2_log_init);
static void
otx2_log_init(void)
{
	otx2_logtype_base = rte_log_register("pmd.octeontx2.base");
	if (otx2_logtype_base >= 0)
		rte_log_set_level(otx2_logtype_base, RTE_LOG_NOTICE);

	otx2_logtype_mbox = rte_log_register("pmd.octeontx2.mbox");
	if (otx2_logtype_mbox >= 0)
		rte_log_set_level(otx2_logtype_mbox, RTE_LOG_NOTICE);

	otx2_logtype_npa = rte_log_register("pmd.mempool.octeontx2");
	if (otx2_logtype_npa >= 0)
		rte_log_set_level(otx2_logtype_npa, RTE_LOG_NOTICE);

	otx2_logtype_nix = rte_log_register("pmd.net.octeontx2");
	if (otx2_logtype_nix >= 0)
		rte_log_set_level(otx2_logtype_nix, RTE_LOG_NOTICE);

	otx2_logtype_npc = rte_log_register("pmd.net.octeontx2.flow");
	if (otx2_logtype_npc >= 0)
		rte_log_set_level(otx2_logtype_npc, RTE_LOG_NOTICE);

	otx2_logtype_tm = rte_log_register("pmd.net.octeontx2.tm");
	if (otx2_logtype_tm >= 0)
		rte_log_set_level(otx2_logtype_tm, RTE_LOG_NOTICE);

	otx2_logtype_sso = rte_log_register("pmd.event.octeontx2");
	if (otx2_logtype_sso >= 0)
		rte_log_set_level(otx2_logtype_sso, RTE_LOG_NOTICE);

	otx2_logtype_tim = rte_log_register("pmd.event.octeontx2.timer");
	if (otx2_logtype_tim >= 0)
		rte_log_set_level(otx2_logtype_tim, RTE_LOG_NOTICE);

	otx2_logtype_dpi = rte_log_register("pmd.raw.octeontx2.dpi");
	if (otx2_logtype_dpi >= 0)
		rte_log_set_level(otx2_logtype_dpi, RTE_LOG_NOTICE);
}
