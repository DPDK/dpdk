/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <unistd.h>

#include "ark_rqp.h"
#include "ark_logs.h"

/* ************************************************************************* */
void
ark_rqp_stats_reset(struct ark_rqpace_t *rqp)
{
	rqp->stats_clear = 1;
	/* POR 992 */
	/* rqp->cpld_max = 992; */
	/* POR 64 */
	/* rqp->cplh_max = 64; */
}

/* ************************************************************************* */
void
ark_rqp_dump(struct ark_rqpace_t *rqp)
{
	if (rqp->err_count_other != 0)
		PMD_DRV_LOG(ERR,
			    "RQP Errors noted: ctrl: %d cplh_hmax %d cpld_max %d"
			    ARK_SU32
			    ARK_SU32 "\n",
			    rqp->ctrl, rqp->cplh_max, rqp->cpld_max,
			    "Error Count", rqp->err_cnt,
			    "Error General", rqp->err_count_other);

	PMD_STATS_LOG(INFO, "RQP Dump: ctrl: %d cplh_hmax %d cpld_max %d"
		      ARK_SU32
		      ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32
		      ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32
		      ARK_SU32 ARK_SU32 ARK_SU32
		      ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32 "\n",
		      rqp->ctrl, rqp->cplh_max, rqp->cpld_max,
		      "Error Count", rqp->err_cnt,
		      "Error General", rqp->err_count_other,
		      "stall_pS", rqp->stall_ps,
		      "stall_pS Min", rqp->stall_ps_min,
		      "stall_pS Max", rqp->stall_ps_max,
		      "req_pS", rqp->req_ps,
		      "req_pS Min", rqp->req_ps_min,
		      "req_pS Max", rqp->req_ps_max,
		      "req_dWPS", rqp->req_dw_ps,
		      "req_dWPS Min", rqp->req_dw_ps_min,
		      "req_dWPS Max", rqp->req_dw_ps_max,
		      "cpl_pS", rqp->cpl_ps,
		      "cpl_pS Min", rqp->cpl_ps_min,
		      "cpl_pS Max", rqp->cpl_ps_max,
		      "cpl_dWPS", rqp->cpl_dw_ps,
		      "cpl_dWPS Min", rqp->cpl_dw_ps_min,
		      "cpl_dWPS Max", rqp->cpl_dw_ps_max,
		      "cplh pending", rqp->cplh_pending,
		      "cpld pending", rqp->cpld_pending,
		      "cplh pending max", rqp->cplh_pending_max,
		      "cpld pending max", rqp->cpld_pending_max);
}

int
ark_rqp_lasped(struct ark_rqpace_t *rqp)
{
	return rqp->lasped;
}
