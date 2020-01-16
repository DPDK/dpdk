/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>

#include <rte_cycles.h>
#include <rte_malloc.h>

#include "bnxt.h"
#include "bnxt_irq.h"
#include "bnxt_ring.h"
#include "hsi_struct_def_dpdk.h"

/*
 * Interrupts
 */

void bnxt_int_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct bnxt *bp = eth_dev->data->dev_private;
	struct bnxt_cp_ring_info *cpr = bp->async_cp_ring;
	struct cmpl_base *cmp;
	uint32_t raw_cons;
	uint32_t cons;

	if (cpr == NULL)
		return;

	raw_cons = cpr->cp_raw_cons;
	pthread_mutex_lock(&bp->def_cp_lock);
	while (1) {
		if (!cpr || !cpr->cp_ring_struct || !cpr->cp_db.doorbell) {
			pthread_mutex_unlock(&bp->def_cp_lock);
			return;
		}

		if (is_bnxt_in_error(bp)) {
			pthread_mutex_unlock(&bp->def_cp_lock);
			return;
		}

		cons = RING_CMP(cpr->cp_ring_struct, raw_cons);
		cmp = &cpr->cp_desc_ring[cons];

		if (!CMP_VALID(cmp, raw_cons, cpr->cp_ring_struct))
			break;

		bnxt_event_hwrm_resp_handler(bp, cmp);
		raw_cons = NEXT_RAW_CMP(raw_cons);
	}

	cpr->cp_raw_cons = raw_cons;
	if (BNXT_HAS_NQ(bp))
		bnxt_db_nq_arm(cpr);
	else
		B_CP_DB_REARM(cpr, cpr->cp_raw_cons);

	pthread_mutex_unlock(&bp->def_cp_lock);
}

int bnxt_free_int(struct bnxt *bp)
{
	struct rte_intr_handle *intr_handle = &bp->pdev->intr_handle;
	struct bnxt_irq *irq = bp->irq_tbl;
	int rc = 0;

	if (!irq)
		return 0;

	if (irq->requested) {
		int count = 0;

		/*
		 * Callback deregistration will fail with rc -EAGAIN if the
		 * callback is currently active. Retry every 50 ms until
		 * successful or 500 ms has elapsed.
		 */
		do {
			rc = rte_intr_callback_unregister(intr_handle,
							  irq->handler,
							  bp->eth_dev);
			if (rc >= 0) {
				irq->requested = 0;
				break;
			}
			rte_delay_ms(50);
		} while (count++ < 10);

		if (rc < 0) {
			PMD_DRV_LOG(ERR, "irq cb unregister failed rc: %d\n",
				    rc);
			return rc;
		}
	}

	rte_free(bp->irq_tbl);
	bp->irq_tbl = NULL;

	return 0;
}

void bnxt_disable_int(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = bp->async_cp_ring;

	if (BNXT_NUM_ASYNC_CPR(bp) == 0)
		return;

	if (is_bnxt_in_error(bp))
		return;

	if (!cpr || !cpr->cp_db.doorbell)
		return;

	/* Only the default completion ring */
	if (BNXT_HAS_NQ(bp))
		bnxt_db_nq(cpr);
	else
		B_CP_DB_DISARM(cpr);
}

void bnxt_enable_int(struct bnxt *bp)
{
	struct bnxt_cp_ring_info *cpr = bp->async_cp_ring;

	if (BNXT_NUM_ASYNC_CPR(bp) == 0)
		return;

	if (!cpr || !cpr->cp_db.doorbell)
		return;

	/* Only the default completion ring */
	if (BNXT_HAS_NQ(bp))
		bnxt_db_nq_arm(cpr);
	else
		B_CP_DB_ARM(cpr);
}

int bnxt_setup_int(struct bnxt *bp)
{
	uint16_t total_vecs;
	const int len = sizeof(bp->irq_tbl[0].name);
	int i;

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
		PMD_DRV_LOG(ERR, "bnxt_irq_tbl setup failed\n");
		return -ENOMEM;
	}

	return 0;
}

int bnxt_request_int(struct bnxt *bp)
{
	struct rte_intr_handle *intr_handle = &bp->pdev->intr_handle;
	struct bnxt_irq *irq = bp->irq_tbl;
	int rc = 0;

	if (!irq)
		return 0;

	if (!irq->requested) {
		rc = rte_intr_callback_register(intr_handle,
						irq->handler,
						bp->eth_dev);
		if (!rc)
			irq->requested = 1;
	}

#ifdef RTE_EXEC_ENV_FREEBSD
	/**
	 * In FreeBSD OS, nic_uio does not support interrupts and
	 * interrupt register callback will fail.
	 */
	rc = 0;
#endif

	return rc;
}
