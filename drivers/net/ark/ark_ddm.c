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
	uint32_t hw_ver;
	if (sizeof(struct ark_ddm_t) != ARK_DDM_EXPECTED_SIZE) {
		ARK_PMD_LOG(ERR, "DDM structure looks incorrect %d vs %zd\n",
			    ARK_DDM_EXPECTED_SIZE, sizeof(struct ark_ddm_t));
		return -1;
	}

	hw_const = ddm->cfg.idnum;
	hw_ver = ddm->cfg.vernum;
	if (hw_const == ARK_DDM_MODID && hw_ver == ARK_DDM_MODVER)
		return 0;

	ARK_PMD_LOG(ERR,
		    "ARK: DDM module not found as expected"
		    " id: %08x ver: %08x\n",
		    hw_const, hw_ver);
	return -1;
}

void
ark_ddm_queue_enable(struct ark_ddm_t *ddm, int enable)
{
	ddm->setup.qcommand = enable ? 1U : 0U;
}

void
ark_ddm_queue_setup(struct ark_ddm_t *ddm, rte_iova_t cons_addr)
{
	ddm->setup.cons_write_index_addr = cons_addr;
	ddm->setup.cons_index = 0;
}

/* Global stats clear */
void
ark_ddm_stats_reset(struct ark_ddm_t *ddm)
{
	ddm->cfg.tlp_stats_clear = 1;
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
