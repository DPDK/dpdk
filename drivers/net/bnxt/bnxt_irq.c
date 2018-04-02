/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
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
	struct cmpl_base *cmp;
	uint32_t raw_cons;
	uint32_t cons;

	if (cpr == NULL)
		return;

	raw_cons = cpr->cp_raw_cons;
	while (1) {
		if (!cpr || !cpr->cp_ring_struct)
			return;

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
			PMD_DRV_LOG(INFO,
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
				 "%s-%d", bp->eth_dev->device->name, i);
			bp->irq_tbl[i].handler = bnxt_int_handler;
		}
	} else {
		rc = -ENOMEM;
		goto setup_exit;
	}
	return 0;

setup_exit:
	PMD_DRV_LOG(ERR, "bnxt_irq_tbl setup failed\n");
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
