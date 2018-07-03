/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <unistd.h>

#include "ark_logs.h"
#include "ark_udm.h"

int
ark_udm_verify(struct ark_udm_t *udm)
{
	if (sizeof(struct ark_udm_t) != ARK_UDM_EXPECT_SIZE) {
		PMD_DRV_LOG(ERR,
			    "ARK: UDM structure looks incorrect %d vs %zd\n",
			    ARK_UDM_EXPECT_SIZE, sizeof(struct ark_udm_t));
		return -1;
	}

	if (udm->setup.const0 != ARK_UDM_CONST) {
		PMD_DRV_LOG(ERR,
			    "ARK: UDM module not found as expected 0x%08x\n",
			    udm->setup.const0);
		return -1;
	}
	return 0;
}

int
ark_udm_stop(struct ark_udm_t *udm, const int wait)
{
	int cnt = 0;

	udm->cfg.command = 2;

	while (wait && (udm->cfg.stop_flushed & 0x01) == 0) {
		if (cnt++ > 1000)
			return 1;

		usleep(10);
	}
	return 0;
}

int
ark_udm_reset(struct ark_udm_t *udm)
{
	int status;

	status = ark_udm_stop(udm, 1);
	if (status != 0) {
		PMD_DEBUG_LOG(INFO, "%s  stop failed  doing forced reset\n",
			      __func__);
		udm->cfg.command = 4;
		usleep(10);
		udm->cfg.command = 3;
		status = ark_udm_stop(udm, 0);
		PMD_DEBUG_LOG(INFO, "%s  stop status %d post failure"
			      " and forced reset\n",
			      __func__, status);
	} else {
		udm->cfg.command = 3;
	}

	return status;
}

void
ark_udm_start(struct ark_udm_t *udm)
{
	udm->cfg.command = 1;
}

void
ark_udm_stats_reset(struct ark_udm_t *udm)
{
	udm->pcibp.pci_clear = 1;
	udm->tlp_ps.tlp_clear = 1;
}

void
ark_udm_configure(struct ark_udm_t *udm,
		  uint32_t headroom,
		  uint32_t dataroom,
		  uint32_t write_interval_ns)
{
	/* headroom and data room are in DWords in the UDM */
	udm->cfg.dataroom = dataroom / 4;
	udm->cfg.headroom = headroom / 4;

	/* 4 NS period ns */
	udm->rt_cfg.write_interval = write_interval_ns / 4;
}

void
ark_udm_write_addr(struct ark_udm_t *udm, rte_iova_t addr)
{
	udm->rt_cfg.hw_prod_addr = addr;
}

int
ark_udm_is_flushed(struct ark_udm_t *udm)
{
	return (udm->cfg.stop_flushed & 0x01) != 0;
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
	PMD_STATS_LOG(INFO, "UDM Stats: %s"
		      ARK_SU64 ARK_SU64 ARK_SU64 ARK_SU64 ARK_SU64 "\n",
		      msg,
		      "Pkts Received", udm->stats.rx_packet_count,
		      "Pkts Finalized", udm->stats.rx_sent_packets,
		      "Pkts Dropped", udm->tlp.pkt_drop,
		      "Bytes Count", udm->stats.rx_byte_count,
		      "MBuf Count", udm->stats.rx_mbuf_count);
}

void
ark_udm_dump_queue_stats(struct ark_udm_t *udm, const char *msg, uint16_t qid)
{
	PMD_STATS_LOG(INFO, "UDM Queue %3u Stats: %s"
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
ark_udm_dump(struct ark_udm_t *udm, const char *msg)
{
	PMD_DEBUG_LOG(DEBUG, "UDM Dump: %s Stopped: %d\n", msg,
		      udm->cfg.stop_flushed);
}

void
ark_udm_dump_setup(struct ark_udm_t *udm, uint16_t q_id)
{
	PMD_DEBUG_LOG(DEBUG, "UDM Setup Q: %u"
		      ARK_SU64X ARK_SU32 "\n",
		      q_id,
		      "hw_prod_addr", udm->rt_cfg.hw_prod_addr,
		      "prod_idx", udm->rt_cfg.prod_idx);
}

void
ark_udm_dump_perf(struct ark_udm_t *udm, const char *msg)
{
	struct ark_udm_pcibp_t *bp = &udm->pcibp;

	PMD_STATS_LOG(INFO, "UDM Performance %s"
		      ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32 ARK_SU32
		      "\n",
		      msg,
		      "PCI Empty", bp->pci_empty,
		      "PCI Q1", bp->pci_q1,
		      "PCI Q2", bp->pci_q2,
		      "PCI Q3", bp->pci_q3,
		      "PCI Q4", bp->pci_q4,
		      "PCI Full", bp->pci_full);
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
