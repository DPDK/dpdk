/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <eal_export.h>
#include <rte_memzone.h>
#include <rte_malloc.h>

#include "nitrox_qp.h"
#include "nitrox_hal.h"
#include "nitrox_logs.h"

#define MAX_CMD_QLEN 16384
#define CMDQ_PKT_IN_ALIGN 16

static int
nitrox_setup_cmdq(struct nitrox_qp *qp, uint8_t *bar_addr,
		  const char *dev_name, uint8_t instr_size, int socket_id)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	size_t cmdq_size = qp->count * instr_size;
	uint64_t offset;
	int err = 0;

	snprintf(mz_name, sizeof(mz_name), "%s_cmdq_%d", dev_name, qp->qno);
	mz = rte_memzone_reserve_aligned(mz_name, cmdq_size, socket_id,
					 RTE_MEMZONE_SIZE_HINT_ONLY |
					 RTE_MEMZONE_256MB,
					 CMDQ_PKT_IN_ALIGN);
	if (!mz) {
		NITROX_LOG_LINE(ERR, "cmdq memzone reserve failed for %s queue",
			   mz_name);
		return -ENOMEM;
	}

	switch (qp->type) {
	case NITROX_QUEUE_SE:
		offset = NPS_PKT_IN_INSTR_BAOFF_DBELLX(qp->qno);
		qp->cmdq.dbell_csr_addr = NITROX_CSR_ADDR(bar_addr, offset);
		setup_nps_pkt_input_ring(bar_addr, qp->qno, qp->count,
					 mz->iova);
		setup_nps_pkt_solicit_output_port(bar_addr, qp->qno);
		break;
	case NITROX_QUEUE_ZIP:
		offset = ZQMQ_DRBLX(qp->qno);
		qp->cmdq.dbell_csr_addr = NITROX_CSR_ADDR(bar_addr, offset);
		err = setup_zqmq_input_ring(bar_addr, qp->qno, qp->count,
					    mz->iova);
		break;
	default:
		NITROX_LOG_LINE(ERR, "Invalid queue type %d", qp->type);
		err = -EINVAL;
		break;
	}

	if (err) {
		rte_memzone_free(mz);
		return err;
	}

	qp->cmdq.mz = mz;
	qp->cmdq.ring = mz->addr;
	qp->cmdq.instr_size = instr_size;
	return 0;
}

static int
nitrox_setup_ridq(struct nitrox_qp *qp, int socket_id)
{
	size_t ridq_size = qp->count * sizeof(*qp->ridq);

	qp->ridq = rte_zmalloc_socket("nitrox ridq", ridq_size,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (!qp->ridq) {
		NITROX_LOG_LINE(ERR, "Failed to create rid queue");
		return -ENOMEM;
	}

	return 0;
}

static int
nitrox_release_cmdq(struct nitrox_qp *qp, uint8_t *bar_addr)
{
	int err = 0;

	switch (qp->type) {
	case NITROX_QUEUE_SE:
		nps_pkt_solicited_port_disable(bar_addr, qp->qno);
		nps_pkt_input_ring_disable(bar_addr, qp->qno);
		break;
	case NITROX_QUEUE_ZIP:
		err = zqmq_input_ring_disable(bar_addr, qp->qno);
		break;
	default:
		err = -EINVAL;
	}

	if (err)
		return err;

	return rte_memzone_free(qp->cmdq.mz);
}

RTE_EXPORT_INTERNAL_SYMBOL(nitrox_qp_setup)
int
nitrox_qp_setup(struct nitrox_qp *qp, uint8_t *bar_addr, const char *dev_name,
		uint32_t nb_descriptors, uint8_t instr_size, int socket_id)
{
	int err;
	uint32_t count;

	count = rte_align32pow2(nb_descriptors);
	if (count > MAX_CMD_QLEN) {
		NITROX_LOG_LINE(ERR, "%s: Number of descriptors too big %d,"
			   " greater than max queue length %d",
			   dev_name, count,
			   MAX_CMD_QLEN);
		return -EINVAL;
	}

	qp->bar_addr = bar_addr;
	qp->count = count;
	qp->head = qp->tail = 0;
	rte_atomic_store_explicit(&qp->pending_count, 0,
				  rte_memory_order_relaxed);
	err = nitrox_setup_cmdq(qp, bar_addr, dev_name, instr_size, socket_id);
	if (err)
		return err;

	err = nitrox_setup_ridq(qp, socket_id);
	if (err)
		goto ridq_err;

	return 0;

ridq_err:
	nitrox_release_cmdq(qp, bar_addr);
	return err;
}

static void
nitrox_release_ridq(struct nitrox_qp *qp)
{
	rte_free(qp->ridq);
}

RTE_EXPORT_INTERNAL_SYMBOL(nitrox_qp_release)
int
nitrox_qp_release(struct nitrox_qp *qp, uint8_t *bar_addr)
{
	nitrox_release_ridq(qp);
	return nitrox_release_cmdq(qp, bar_addr);
}
