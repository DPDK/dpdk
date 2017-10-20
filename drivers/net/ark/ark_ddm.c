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

#include "ark_logs.h"
#include "ark_ddm.h"

/* ************************************************************************* */
int
ark_ddm_verify(struct ark_ddm_t *ddm)
{
	if (sizeof(struct ark_ddm_t) != ARK_DDM_EXPECTED_SIZE) {
		PMD_DRV_LOG(ERR, "ARK: DDM structure looks incorrect %d vs %zd\n",
			    ARK_DDM_EXPECTED_SIZE, sizeof(struct ark_ddm_t));
		return -1;
	}

	if (ddm->cfg.const0 != ARK_DDM_CONST) {
		PMD_DRV_LOG(ERR, "ARK: DDM module not found as expected 0x%08x\n",
			    ddm->cfg.const0);
		return -1;
	}
	return 0;
}

void
ark_ddm_start(struct ark_ddm_t *ddm)
{
	ddm->cfg.command = 1;
}

int
ark_ddm_stop(struct ark_ddm_t *ddm, const int wait)
{
	int cnt = 0;

	ddm->cfg.command = 2;
	while (wait && (ddm->cfg.stop_flushed & 0x01) == 0) {
		if (cnt++ > 1000)
			return 1;

		usleep(10);
	}
	return 0;
}

void
ark_ddm_reset(struct ark_ddm_t *ddm)
{
	int status;

	/* reset only works if ddm has stopped properly. */
	status = ark_ddm_stop(ddm, 1);

	if (status != 0) {
		PMD_DEBUG_LOG(INFO, "%s  stop failed  doing forced reset\n",
			      __func__);
		ddm->cfg.command = 4;
		usleep(10);
	}
	ddm->cfg.command = 3;
}

void
ark_ddm_setup(struct ark_ddm_t *ddm, rte_iova_t cons_addr, uint32_t interval)
{
	ddm->setup.cons_write_index_addr = cons_addr;
	ddm->setup.write_index_interval = interval / 4;	/* 4 ns period */
}

void
ark_ddm_stats_reset(struct ark_ddm_t *ddm)
{
	ddm->cfg.tlp_stats_clear = 1;
}

void
ark_ddm_dump(struct ark_ddm_t *ddm, const char *msg)
{
	PMD_FUNC_LOG(DEBUG, "%s Stopped: %d\n", msg,
		     ark_ddm_is_stopped(ddm)
		     );
}

void
ark_ddm_dump_stats(struct ark_ddm_t *ddm, const char *msg)
{
	struct ark_ddm_stats_t *stats = &ddm->stats;

	PMD_STATS_LOG(INFO, "DDM Stats: %s"
		      ARK_SU64 ARK_SU64 ARK_SU64
		      "\n", msg,
		      "Bytes:", stats->tx_byte_count,
		      "Packets:", stats->tx_pkt_count,
		      "MBufs", stats->tx_mbuf_count);
}

int
ark_ddm_is_stopped(struct ark_ddm_t *ddm)
{
	return (ddm->cfg.stop_flushed & 0x01) != 0;
}

uint64_t
ark_ddm_queue_byte_count(struct ark_ddm_t *ddm)
{
	return ddm->queue_stats.byte_count;
}

uint64_t
ark_ddm_queue_pkt_count(struct ark_ddm_t *ddm)
{
	return ddm->queue_stats.pkt_count;
}

void
ark_ddm_queue_reset_stats(struct ark_ddm_t *ddm)
{
	ddm->queue_stats.byte_count = 1;
}
