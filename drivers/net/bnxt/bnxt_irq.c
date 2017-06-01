/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2015 Broadcom Corporation.
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
 *     * Neither the name of Broadcom Corporation nor the names of its
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

#include <rte_malloc.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_irq.h"
#include "bnxt_ring.h"
#include "hsi_struct_def_dpdk.h"

/*
 * Interrupts
 */

static void bnxt_int_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_cp_ring_info *cpr = bp->def_cp_ring;
	uint32_t raw_cons = cpr->cp_raw_cons;
	uint32_t cons;
	struct cmpl_base *cmp;

	while (1) {
		cons = RING_CMP(cpr->cp_ring_struct, raw_cons);
		cmp = &cpr->cp_desc_ring[cons];

		if (!CMP_VALID(cmp, raw_cons, cpr->cp_ring_struct))
			break;

		switch (CMP_TYPE(cmp)) {
		case CMPL_BASE_TYPE_HWRM_ASYNC_EVENT:
			/* Handle any async event */
			bnxt_handle_async_event(bp, cmp);
			break;
		case CMPL_BASE_TYPE_HWRM_FWD_REQ:
			/* Handle HWRM forwarded responses */
			bnxt_handle_fwd_req(bp, cmp);
			break;
		default:
			/* Ignore any other events */
			if (cmp->type & rte_cpu_to_le_16(0x01)) {
				if (!CMP_VALID(cmp, raw_cons,
					       cpr->cp_ring_struct))
					goto no_more;
			}
			RTE_LOG(INFO, PMD,
				"Ignoring %02x completion\n", CMP_TYPE(cmp));
			break;
		}
		raw_cons = NEXT_RAW_CMP(raw_cons);

	};
no_more:
	cpr->cp_raw_cons = raw_cons;
	B_CP_DB_REARM(cpr, cpr->cp_raw_cons);
}

void bnxt_free_int(struct bnxt *bp)
{
	struct bnxt_irq *irq;

	irq = bp->irq_tbl;
	if (irq) {
		if (irq->requested) {
			rte_intr_disable(&bp->pdev->intr_handle);
			rte_intr_callback_unregister(&bp->pdev->intr_handle,
						     irq->handler,
						     (void *)bp->eth_dev);
			irq->requested = 0;
		}
		rte_free((void *)bp->irq_tbl);
		bp->irq_tbl = NULL;
	}
}

void bnxt_disable_int(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = bp->def_cp_ring;

	/* Only the default completion ring */
	if (cpr != NULL && cpr->cp_doorbell != NULL)
		B_CP_DB_DISARM(cpr);
}

void bnxt_enable_int(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = bp->def_cp_ring;

	B_CP_DB_ARM(cpr);
}

int bnxt_setup_int(struct bnxt *bp)
{
	uint16_t total_vecs;
	const int len = sizeof(bp->irq_tbl[0].name);
	int i, rc = 0;

	/* DPDK host only supports 1 MSI-X vector */
	total_vecs = 1;
	bp->irq_tbl = rte_calloc("bnxt_irq_tbl", total_vecs,
				 sizeof(struct bnxt_irq), 0);
	if (bp->irq_tbl) {
		for (i = 0; i < total_vecs; i++) {
			bp->irq_tbl[i].vector = i;
			snprintf(bp->irq_tbl[i].name, len,
				 "%s-%d", bp->eth_dev->data->name, i);
			bp->irq_tbl[i].handler = bnxt_int_handler;
		}
	} else {
		rc = -ENOMEM;
		goto setup_exit;
	}
	return 0;

setup_exit:
	RTE_LOG(ERR, PMD, "bnxt_irq_tbl setup failed\n");
	return rc;
}

int bnxt_request_int(struct bnxt *bp)
{
	int rc = 0;

	struct bnxt_irq *irq = bp->irq_tbl;

	rte_intr_callback_register(&bp->pdev->intr_handle, irq->handler,
				   (void *)bp->eth_dev);
	rte_intr_enable(&bp->pdev->intr_handle);

	irq->requested = 1;
	return rc;
}
