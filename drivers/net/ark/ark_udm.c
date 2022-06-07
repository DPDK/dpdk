/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <unistd.h>

#include "ark_logs.h"
#include "ark_udm.h"

static_assert(sizeof(struct ark_rx_meta) == 32, "Unexpected struct size ark_rx_meta");

int
ark_udm_verify(struct ark_udm_t *udm)
{
	uint32_t idnum = udm->setup.idnum;
	uint32_t vernum = udm->setup.vernum;
	if (sizeof(struct ark_udm_t) != ARK_UDM_EXPECT_SIZE) {
		ARK_PMD_LOG(ERR,
			    "ARK: UDM structure looks incorrect %d vs %zd\n",
			    ARK_UDM_EXPECT_SIZE, sizeof(struct ark_udm_t));
		return -1;
	}

	if (idnum != ARK_UDM_MODID || vernum != ARK_UDM_MODVER) {
		ARK_PMD_LOG(ERR,
			    "ARK: UDM module not found as expected 0x%08x 0x%08x\n",
			    idnum, vernum);
		return -1;
	}
	return 0;
}

void
ark_udm_configure(struct ark_udm_t *udm,
		  uint32_t headroom,
		  uint32_t dataroom)
{
	/* headroom and data room are in DWords in the UDM */
	udm->cfg.dataroom = dataroom / 4;
	udm->cfg.headroom = headroom / 4;
}

void
ark_udm_write_addr(struct ark_udm_t *udm, rte_iova_t addr)
{
	udm->rt_cfg.hw_prod_addr = addr;
	udm->rt_cfg.prod_idx = 0;
}

uint64_t
ark_udm_dropped(struct ark_udm_t *udm)
{
	return udm->qstats.q_pkt_drop;
}

uint64_t
ark_udm_bytes(struct ark_udm_t *udm)
{
	return udm->qstats.q_byte_count;
}

uint64_t
ark_udm_packets(struct ark_udm_t *udm)
{
	return udm->qstats.q_ff_packet_count;
}

void
ark_udm_dump_stats(struct ark_udm_t *udm, const char *msg)
{
	ARK_PMD_LOG(INFO, "UDM Stats: %s"
		      ARK_SU64 ARK_SU64 ARK_SU64 ARK_SU64 "\n",
		      msg,
		      "Pkts Received", udm->stats.rx_packet_count,
		      "Pkts Finalized", udm->stats.rx_sent_packets,
		      "Bytes Count", udm->stats.rx_byte_count,
		      "MBuf Count", udm->stats.rx_mbuf_count);
}

void
ark_udm_dump_queue_stats(struct ark_udm_t *udm, const char *msg, uint16_t qid)
{
	ARK_PMD_LOG(INFO, "UDM Queue %3u Stats: %s"
		      ARK_SU64 ARK_SU64
		      ARK_SU64 ARK_SU64
		      ARK_SU64 "\n",
		      qid, msg,
		      "Pkts Received", udm->qstats.q_packet_count,
		      "Pkts Finalized", udm->qstats.q_ff_packet_count,
		      "Pkts Dropped", udm->qstats.q_pkt_drop,
		      "Bytes Count", udm->qstats.q_byte_count,
		      "MBuf Count", udm->qstats.q_mbuf_count);
}

void
ark_udm_dump_setup(struct ark_udm_t *udm, uint16_t q_id)
{
	ARK_PMD_LOG(DEBUG, "UDM Setup Q: %u"
		      ARK_SU64X ARK_SU32 "\n",
		      q_id,
		      "hw_prod_addr", udm->rt_cfg.hw_prod_addr,
		      "prod_idx", udm->rt_cfg.prod_idx);
}

void
ark_udm_queue_stats_reset(struct ark_udm_t *udm)
{
	udm->qstats.q_byte_count = 1;
}

void
ark_udm_queue_enable(struct ark_udm_t *udm, int enable)
{
	udm->qstats.q_enable = enable ? 1 : 0;
}
