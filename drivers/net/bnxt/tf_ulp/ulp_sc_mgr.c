/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include "bnxt.h"
#include "bnxt_ulp.h"
#include "bnxt_ulp_utils.h"
#include "bnxt_ulp_tfc.h"
#include "bnxt_tf_common.h"
#include "ulp_sc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "tfc.h"
#include "tfc_debug.h"
#include "tfc_action_handle.h"

#define ULP_TFC_CNTR_READ_BYTES 32
#define ULP_TFC_CNTR_ALIGN 32
#define ULP_TFC_ACT_WORD_SZ 32

static const struct bnxt_ulp_sc_core_ops *
bnxt_ulp_sc_ops_get(struct bnxt_ulp_context *ctxt)
{
	int32_t rc;
	enum bnxt_ulp_device_id  dev_id;
	const struct bnxt_ulp_sc_core_ops *func_ops;

	rc = bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id);
	if (rc)
		return NULL;

	switch (dev_id) {
	case BNXT_ULP_DEVICE_ID_THOR2:
		func_ops = &ulp_sc_tfc_core_ops;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
	case BNXT_ULP_DEVICE_ID_STINGRAY:
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
	default:
		func_ops = NULL;
		break;
	}
	return func_ops;
}

int32_t ulp_sc_mgr_init(struct bnxt_ulp_context *ctxt)
{
	const struct bnxt_ulp_sc_core_ops *sc_ops;
	struct bnxt_ulp_device_params *dparms;
	struct bnxt_ulp_sc_info *ulp_sc_info;
	uint32_t stats_cache_tbl_sz;
	uint32_t dev_id;
	int rc;

	if (!ctxt) {
		BNXT_DRV_DBG(DEBUG, "Invalid ULP CTXT\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		BNXT_DRV_DBG(DEBUG, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(DEBUG, "Failed to device parms\n");
		return -EINVAL;
	}

	sc_ops = bnxt_ulp_sc_ops_get(ctxt);
	if (sc_ops == NULL) {
		BNXT_DRV_DBG(DEBUG, "Failed to get the counter ops\n");
		return -EINVAL;
	}

	ulp_sc_info = rte_zmalloc("ulp_sc_info", sizeof(*ulp_sc_info), 0);
	if (!ulp_sc_info) {
		rc = -ENOMEM;
		goto error;
	}

	ulp_sc_info->sc_ops = sc_ops;
	ulp_sc_info->flags = 0;

	rc = pthread_mutex_init(&ulp_sc_info->sc_lock, NULL);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Failed to initialize sc mutex\n");
		goto error;
	}

	/* Add the SC info tbl to the ulp context. */
	bnxt_ulp_cntxt_ptr2_sc_info_set(ctxt, ulp_sc_info);

	ulp_sc_info->num_counters = dparms->ext_flow_db_num_entries;
	if (!ulp_sc_info->num_counters) {
		/* No need for software counters, call fw directly */
		BNXT_DRV_DBG(DEBUG, "Sw flow counter support not enabled\n");
		return 0;
	}

	/*
	 * Size is determined by the number of flows + 10% to cover IDs
	 * used for resources.
	 */
	stats_cache_tbl_sz = sizeof(struct ulp_sc_tfc_stats_cache_entry) *
		(ulp_sc_info->num_counters +
		 (ulp_sc_info->num_counters / 10));

	ulp_sc_info->stats_cache_tbl = rte_zmalloc("ulp_stats_cache_tbl",
						   stats_cache_tbl_sz, 0);
	if (!ulp_sc_info->stats_cache_tbl) {
		rc = -ENOMEM;
		goto error;
	}

	ulp_sc_info->read_data = rte_zmalloc("ulp_stats_cache_read_data",
					     ULP_SC_BATCH_SIZE * ULP_SC_PAGE_SIZE,
					     ULP_SC_PAGE_SIZE);
	if (!ulp_sc_info->read_data) {
		rte_free(ulp_sc_info->stats_cache_tbl);
		rc = -ENOMEM;
		goto error;
	}

	rc = ulp_sc_mgr_thread_start(ctxt);
	if (rc)
		BNXT_DRV_DBG(DEBUG, "Stats counter thread start failed\n");

 error:
	return rc;
}

/*
 * Release all resources in the Flow Counter Manager for this ulp context
 *
 * ctxt [in] The ulp context for the Flow Counter manager
 *
 */
int32_t
ulp_sc_mgr_deinit(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_sc_info *ulp_sc_info;

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);

	if (!ulp_sc_info)
		return -EINVAL;

	pthread_mutex_lock(&ulp_sc_info->sc_lock);

	ulp_sc_mgr_thread_cancel(ctxt);

	pthread_mutex_destroy(&ulp_sc_info->sc_lock);

	if (ulp_sc_info->stats_cache_tbl)
		rte_free(ulp_sc_info->stats_cache_tbl);

	if (ulp_sc_info->read_data)
		rte_free(ulp_sc_info->read_data);

	rte_free(ulp_sc_info);

	/* Safe to ignore on deinit */
	(void)bnxt_ulp_cntxt_ptr2_sc_info_set(ctxt, NULL);

	return 0;
}

#define ULP_SC_PERIOD_S 1
#define ULP_SC_PERIOD_MS (ULP_SC_PERIOD_S * 1000)

static uint32_t ulp_stats_cache_main_loop(void *arg)
{
	struct ulp_sc_tfc_stats_cache_entry *count;
	const struct bnxt_ulp_sc_core_ops *sc_ops;
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct ulp_sc_tfc_stats_cache_entry *sce_end;
	struct tfc_mpc_batch_info_t batch_info;
	struct bnxt_ulp_sc_info *ulp_sc_info;
	struct bnxt_ulp_context *ctxt = NULL;
	uint16_t words = (ULP_TFC_CNTR_READ_BYTES + ULP_TFC_ACT_WORD_SZ - 1) / ULP_TFC_ACT_WORD_SZ;
	uint32_t batch_size;
	struct tfc *tfcp = NULL;
	uint32_t batch;
	uint32_t delay = ULP_SC_PERIOD_MS;
	uint64_t start;
	uint64_t stop;
	uint64_t hz;
	uint8_t *data;
	int rc;
	static uint32_t loop;
	uint64_t cycles = 0;
	uint64_t cpms = 0;

	while (!ctxt) {
		ctxt = bnxt_ulp_cntxt_entry_acquire(arg);

		if (ctxt)
			break;

		BNXT_DRV_DBG(INFO, "could not get the ulp context lock\n");
		rte_delay_us_block(1000);
	}


	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);
	if (!ulp_sc_info) {
		bnxt_ulp_cntxt_entry_release();
		goto terminate;
	}

	sc_ops = ulp_sc_info->sc_ops;

	hz = rte_get_timer_hz();
	cpms = hz / 1000;

	while (true) {
		bnxt_ulp_cntxt_entry_release();
		ctxt = NULL;
		rte_delay_ms(delay);

		while (!ctxt) {
			ctxt = bnxt_ulp_cntxt_entry_acquire(arg);

			if (ctxt)
				break;

			BNXT_DRV_DBG(INFO, "could not get the ulp context lock\n");
			rte_delay_us_block(1);
		}

		start = rte_get_timer_cycles();
		sce = ulp_sc_info->stats_cache_tbl;
		sce_end = sce + (ulp_sc_info->num_counters + (ulp_sc_info->num_counters / 10));

		while (ulp_sc_info->num_entries && (sce < sce_end)) {
			data = ulp_sc_info->read_data;

			rc = tfc_mpc_batch_start(&batch_info);
			if (rc) {
				PMD_DRV_LOG_LINE(ERR,
						 "MPC batch start failed rc:%d loop:%d",
						 rc, loop);
				break;
			}

			if (bnxt_ulp_cntxt_acquire_fdb_lock(ctxt))
				break;

			rc = pthread_mutex_lock(&ulp_sc_info->sc_lock);
			if (rc) {
				PMD_DRV_LOG_LINE(ERR,
						 "Failed to get SC lock, terminating main loop rc:%d loop:%d",
						 rc, loop);
				goto terminate;
			}

			for (batch = 0; (batch < ULP_SC_BATCH_SIZE) && (sce < sce_end);) {
				if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID)) {
					sce++;
					continue;
				}

				tfcp = bnxt_ulp_cntxt_tfcp_get(sce->ctxt);
				if (tfcp == NULL) {
					bnxt_ulp_cntxt_entry_release();
					goto terminate;
				}


				/* Store the entry pointer to use for counter update */
				batch_info.em_hdl[batch_info.count] = (uint64_t)sce;

				rc = sc_ops->ulp_stats_cache_update(tfcp,
								    sce->dir,
								    data,
								    sce->handle,
								    &words,
								    &batch_info,
								    sce->reset);
				if (rc) {
					/* Abort this batch */
					PMD_DRV_LOG_LINE(ERR,
							 "loop:%d read_counter() failed:%d",
							 loop, rc);
					break;
				}

				if (sce->reset)
					sce->reset = false;

				/* Next */
				batch++;
				sce++;
				data += ULP_SC_PAGE_SIZE;
			}

			batch_size = batch_info.count;
			rc = tfc_mpc_batch_end(tfcp, &batch_info);

			pthread_mutex_unlock(&ulp_sc_info->sc_lock);
			bnxt_ulp_cntxt_release_fdb_lock(ctxt);

			if (rc) {
				PMD_DRV_LOG_LINE(ERR,
						 "MPC batch end failed rc:%d loop:%d",
						 rc, loop);
				batch_info.enabled = false;
				break;
			}

			/* Process counts */
			data = ulp_sc_info->read_data;

			for (batch = 0; batch < batch_size; batch++) {
				/* Check for error in completion */
				if (batch_info.result[batch]) {
					PMD_DRV_LOG_LINE(ERR,
							 "batch:%d result:%d",
							 batch, batch_info.result[batch]);
				} else {
					count = (struct ulp_sc_tfc_stats_cache_entry *)
						((uintptr_t)batch_info.em_hdl[batch]);
					memcpy(&count->packet_count, data, ULP_TFC_ACT_WORD_SZ);
				}

				data += ULP_SC_PAGE_SIZE;
			}
		}

		loop++;
		stop = rte_get_timer_cycles();
		cycles = stop - start;
		if (cycles > (hz * ULP_SC_PERIOD_S)) {
			PMD_DRV_LOG_LINE(ERR,
					 "Stats collection time exceeded %dmS Cycles:%" PRIu64,
					 ULP_SC_PERIOD_MS, cycles);
			delay = ULP_SC_PERIOD_MS;
		} else {
			delay = ULP_SC_PERIOD_MS - (cycles / cpms);

			if (delay > ULP_SC_PERIOD_MS) {
				PMD_DRV_LOG_LINE(ERR,
						 "Stats collection delay:%dmS exceedes %dmS",
						 delay, ULP_SC_PERIOD_MS);
				delay = ULP_SC_PERIOD_MS;
			}
		}
	}

 terminate:
	return 0;
}

/*
 * Check if the alarm thread that walks through the flows is started
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */
bool ulp_sc_mgr_thread_isstarted(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_sc_info *ulp_sc_info;

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);

	if (ulp_sc_info)
		return !!(ulp_sc_info->flags & ULP_FLAG_SC_THREAD);

	return false;
}

/*
 * Setup the Flow counter timer thread that will fetch/accumulate raw counter
 * data from the chip's internal flow counters
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */
int32_t
ulp_sc_mgr_thread_start(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_sc_info *ulp_sc_info;
	int rc;

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);

	if (ulp_sc_info && !(ulp_sc_info->flags & ULP_FLAG_SC_THREAD)) {
		rc = rte_thread_create(&ulp_sc_info->tid,
				       NULL,
				       &ulp_stats_cache_main_loop,
				       (void *)ctxt->cfg_data);
		if (rc)
			return rc;

		ulp_sc_info->flags |= ULP_FLAG_SC_THREAD;
	}

	return 0;
}

/*
 * Cancel the alarm handler
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */
void ulp_sc_mgr_thread_cancel(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_sc_info *ulp_sc_info;

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);
	if (!ulp_sc_info)
		return;

	ulp_sc_info->flags &= ~ULP_FLAG_SC_THREAD;
}

/*
 * Fill the rte_flow_query_count 'data' argument passed
 * in the rte_flow_query() with the values obtained and
 * accumulated locally.
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * flow_id [in] The HW flow ID
 *
 * count [out] The rte_flow_query_count 'data' that is set
 *
 */
int ulp_sc_mgr_query_count_get(struct bnxt_ulp_context *ctxt,
			       uint32_t flow_id,
			       struct rte_flow_query_count *count)
{
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct bnxt_ulp_sc_info *ulp_sc_info;
	int rc = 0;

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);
	if (!ulp_sc_info)
		return -ENODEV;

	sce = ulp_sc_info->stats_cache_tbl;
	sce += flow_id;

	/* If entry is not valid return an error */
	if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID))
		return -EBUSY;

	count->hits = sce->packet_count;
	count->hits_set = 1;
	count->bytes = sce->byte_count;
	count->bytes_set = 1;

	if (count->reset)
		sce->reset = true;

	return rc;
}


int ulp_sc_mgr_entry_alloc(struct bnxt_ulp_mapper_parms *parms,
			   uint64_t counter_handle,
			   struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct bnxt_ulp_sc_info *ulp_sc_info;

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(parms->ulp_ctx);
	if (!ulp_sc_info)
		return -ENODEV;

	pthread_mutex_lock(&ulp_sc_info->sc_lock);

	sce = ulp_sc_info->stats_cache_tbl;
	sce += parms->flow_id;

	/* If entry is not free return an error */
	if (sce->flags & ULP_SC_ENTRY_FLAG_VALID) {
		pthread_mutex_unlock(&ulp_sc_info->sc_lock);
		return -EBUSY;
	}

	memset(sce, 0, sizeof(*sce));
	sce->ctxt = parms->ulp_ctx;
	sce->flags |= ULP_SC_ENTRY_FLAG_VALID;
	sce->handle = counter_handle;
	sce->dir = tbl->direction;
	ulp_sc_info->num_entries++;
	pthread_mutex_unlock(&ulp_sc_info->sc_lock);

	return 0;
}

void ulp_sc_mgr_entry_free(struct bnxt_ulp_context *ulp,
			   uint32_t fid)
{
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct bnxt_ulp_sc_info *ulp_sc_info;

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ulp);
	if (!ulp_sc_info)
		return;

	pthread_mutex_lock(&ulp_sc_info->sc_lock);

	sce = ulp_sc_info->stats_cache_tbl;
	sce += fid;

	if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID)) {
		pthread_mutex_unlock(&ulp_sc_info->sc_lock);
		return;
	}

	sce->flags = 0;
	ulp_sc_info->num_entries--;

	pthread_mutex_unlock(&ulp_sc_info->sc_lock);
}
