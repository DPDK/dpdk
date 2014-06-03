/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inttypes.h>
#include <stdio.h>
#include <math.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_cycles.h>

#include "rte_meter.h"

#ifndef RTE_METER_TB_PERIOD_MIN
#define RTE_METER_TB_PERIOD_MIN      100
#endif

static void
rte_meter_get_tb_params(uint64_t hz, uint64_t rate, uint64_t *tb_period, uint64_t *tb_bytes_per_period)
{
	double period = ((double) hz) / ((double) rate);

	if (period >= RTE_METER_TB_PERIOD_MIN) {
		*tb_bytes_per_period = 1;
		*tb_period = (uint64_t) period;
	} else {
		*tb_bytes_per_period = (uint64_t) ceil(RTE_METER_TB_PERIOD_MIN / period);
		*tb_period = (hz * (*tb_bytes_per_period)) / rate;
	}
}

int
rte_meter_srtcm_config(struct rte_meter_srtcm *m, struct rte_meter_srtcm_params *params)
{
	uint64_t hz;

	/* Check input parameters */
	if ((m == NULL) || (params == NULL)) {
		return -1;
	}

	if ((params->cir == 0) || ((params->cbs == 0) && (params->ebs == 0))) {
		return -2;
	}

	/* Initialize srTCM run-time structure */
	hz = rte_get_tsc_hz();
	m->time = rte_get_tsc_cycles();
	m->tc = m->cbs = params->cbs;
	m->te = m->ebs = params->ebs;
	rte_meter_get_tb_params(hz, params->cir, &m->cir_period, &m->cir_bytes_per_period);

	RTE_LOG(INFO, METER, "Low level srTCM config: \n"
		"\tCIR period = %" PRIu64 ", CIR bytes per period = %" PRIu64 "\n",
		m->cir_period, m->cir_bytes_per_period);

	return 0;
}

int
rte_meter_trtcm_config(struct rte_meter_trtcm *m, struct rte_meter_trtcm_params *params)
{
	uint64_t hz;

	/* Check input parameters */
	if ((m == NULL) || (params == NULL)) {
		return -1;
	}

	if ((params->cir == 0) || (params->pir == 0) || (params->pir < params->cir) ||
		(params->cbs == 0) || (params->pbs == 0)) {
		return -2;
	}

	/* Initialize trTCM run-time structure */
	hz = rte_get_tsc_hz();
	m->time_tc = m->time_tp = rte_get_tsc_cycles();
	m->tc = m->cbs = params->cbs;
	m->tp = m->pbs = params->pbs;
	rte_meter_get_tb_params(hz, params->cir, &m->cir_period, &m->cir_bytes_per_period);
	rte_meter_get_tb_params(hz, params->pir, &m->pir_period, &m->pir_bytes_per_period);

	RTE_LOG(INFO, METER, "Low level trTCM config: \n"
		"\tCIR period = %" PRIu64 ", CIR bytes per period = %" PRIu64 "\n"
		"\tPIR period = %" PRIu64 ", PIR bytes per period = %" PRIu64 "\n",
		m->cir_period, m->cir_bytes_per_period,
		m->pir_period, m->pir_bytes_per_period);

	return 0;
}
