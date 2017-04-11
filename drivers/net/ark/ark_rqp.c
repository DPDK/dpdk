/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2017 Atomic Rules LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
