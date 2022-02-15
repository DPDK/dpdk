/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <unistd.h>

#include "ark_logs.h"
#include "ark_ddm.h"

static_assert(sizeof(union ark_tx_meta) == 8, "Unexpected struct size ark_tx_meta");

/* ************************************************************************* */
int
ark_ddm_verify(struct ark_ddm_t *ddm)
{
	uint32_t hw_const;
	if (sizeof(struct ark_ddm_t) != ARK_DDM_EXPECTED_SIZE) {
		ARK_PMD_LOG(ERR, "DDM structure looks incorrect %d vs %zd\n",
			    ARK_DDM_EXPECTED_SIZE, sizeof(struct ark_ddm_t));
		return -1;
	}

	hw_const = ddm->cfg.const0;
	if (hw_const == ARK_DDM_CONST3)
		return 0;

	if (hw_const == ARK_DDM_CONST1) {
		ARK_PMD_LOG(ERR,
			    "ARK: DDM module is version 1, "
			    "PMD expects version 2\n");
		return -1;
	}

	if (hw_const == ARK_DDM_CONST2) {
		ARK_PMD_LOG(ERR,
			    "ARK: DDM module is version 2, "
			    "PMD expects version 3\n");
		return -1;
	}
	ARK_PMD_LOG(ERR,
		    "ARK: DDM module not found as expected 0x%08x\n",
		    ddm->cfg.const0);
	return -1;
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
	rte_wmb();
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
		ARK_PMD_LOG(NOTICE, "%s  stop failed  doing forced reset\n",
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
	ARK_PMD_LOG(DEBUG, "%s Stopped: %d\n", msg,
		     ark_ddm_is_stopped(ddm)
		     );
}

void
ark_ddm_dump_stats(struct ark_ddm_t *ddm, const char *msg)
{
	struct ark_ddm_stats_t *stats = &ddm->stats;

	ARK_PMD_LOG(INFO, "DDM Stats: %s"
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
