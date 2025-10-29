/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <sched.h>
#include <unistd.h>
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

#define ULP_SC_MAX_COUNT 0xFFFFFFFFFFFFFFFFULL

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
	uint8_t *data;
	int rc;
	int i;

	if (!ctxt) {
		BNXT_DRV_DBG(ERR, "Invalid ULP CTXT\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		BNXT_DRV_DBG(ERR, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		BNXT_DRV_DBG(ERR, "Failed to device parms\n");
		return -EINVAL;
	}

	sc_ops = bnxt_ulp_sc_ops_get(ctxt);
	if (sc_ops == NULL) {
		BNXT_DRV_DBG(ERR, "Failed to get the counter ops\n");
		return -EINVAL;
	}

	ulp_sc_info = rte_zmalloc("ulp_sc_info", sizeof(*ulp_sc_info), 0);
	if (!ulp_sc_info) {
		BNXT_DRV_DBG(ERR, "Failed to allocate stats cache container\n");
		rc = -ENOMEM;
		goto error;
	}

	ulp_sc_info->sc_ops = sc_ops;
	ulp_sc_info->flags = 0;

	/* Add the SC info tbl to the ulp context. */
	bnxt_ulp_cntxt_ptr2_sc_info_set(ctxt, ulp_sc_info);

	ulp_sc_info->num_counters = dparms->ext_flow_db_num_entries;
	if (!ulp_sc_info->num_counters) {
		/* No need for software counters, call fw directly */
		BNXT_DRV_DBG(ERR, "Num of flow entries is not configured\n");
		rc = -EINVAL;
		goto error;
	}

	/*
	 * Size is determined by the number of flows + 10% to cover IDs
	 * used for resources.
	 */
	ulp_sc_info->cache_tbl_size = ulp_sc_info->num_counters +
		(ulp_sc_info->num_counters / 10);
	stats_cache_tbl_sz = sizeof(struct ulp_sc_tfc_stats_cache_entry) *
		ulp_sc_info->cache_tbl_size;

	ulp_sc_info->stats_cache_tbl = rte_zmalloc("ulp_stats_cache_tbl",
						   stats_cache_tbl_sz, 0);
	if (!ulp_sc_info->stats_cache_tbl) {
		BNXT_DRV_DBG(ERR, "Failed to allocate stats cache table\n");
		rc = -ENOMEM;
		goto error;
	}

	ulp_sc_info->read_data = rte_zmalloc("ulp_stats_cache_read_data",
					     ULP_SC_BATCH_SIZE * ULP_SC_PAGE_SIZE,
					     ULP_SC_PAGE_SIZE);
	if (!ulp_sc_info->read_data) {
		BNXT_DRV_DBG(ERR, "Failed to allocate stats cache data\n");
		rc = -ENOMEM;
		goto error;
	}

	data = ulp_sc_info->read_data;
	for (i = 0; i < ULP_SC_BATCH_SIZE; i++) {
		ulp_sc_info->read_data_iova[i] = (uint64_t)rte_mem_virt2iova(data);
		data += ULP_SC_PAGE_SIZE;
	}

	rc = ulp_sc_mgr_thread_start(ctxt);
	if (rc) {
		BNXT_DRV_DBG(ERR, "Stats cache thread start failed\n");
		rc = -EIO;
		goto error;
	}

	return 0;
error:
	ulp_sc_mgr_deinit(ctxt);
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

	ulp_sc_mgr_thread_cancel(ctxt);

	if (ulp_sc_info->stats_cache_tbl)
		rte_free(ulp_sc_info->stats_cache_tbl);

	if (ulp_sc_info->read_data)
		rte_free(ulp_sc_info->read_data);

	rte_free(ulp_sc_info);

	/* Safe to ignore on deinit */
	(void)bnxt_ulp_cntxt_ptr2_sc_info_set(ctxt, NULL);

	return 0;
}

#define ULP_SC_PERIOD_US 100000
#define ULP_SC_CTX_DELAY 10000

static uint32_t ulp_stats_cache_main_loop(void *arg)
{
	struct ulp_sc_tfc_stats_cache_entry *count;
	const struct bnxt_ulp_sc_core_ops *sc_ops = NULL;
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct ulp_sc_tfc_stats_cache_entry *sce_end;
	struct tfc_mpc_batch_info_t batch_info;
	struct bnxt_ulp_sc_info *ulp_sc_info;
	struct bnxt_ulp_context *ctxt = NULL;
	uint16_t words = (ULP_TFC_CNTR_READ_BYTES + ULP_TFC_ACT_WORD_SZ - 1) / ULP_TFC_ACT_WORD_SZ;
	uint32_t batch_size;
	struct tfc *tfcp = NULL;
	uint32_t batch, stat_cnt;
	int oldstate;
	int oldtype;
	uint8_t *data;
	int rc;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);

	while (true) {
		ctxt = NULL;
		while (!ctxt) {
			ctxt = bnxt_ulp_cntxt_entry_acquire(arg);

			if (ctxt)
				break;
			/* If there are no more contexts just exit */
			if (bnxt_ulp_cntxt_list_count() == 0)
				goto terminate;
			rte_delay_us_block(ULP_SC_CTX_DELAY);
		}
		bnxt_ulp_cntxt_entry_release();

		/* get the stats counter info block from ulp context */
		ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);
		if (unlikely(!ulp_sc_info)) {
			goto terminate;
		}

		sce = ulp_sc_info->stats_cache_tbl;
		sce_end = sce + ulp_sc_info->cache_tbl_size;

		if (unlikely(!sc_ops))
			sc_ops = ulp_sc_info->sc_ops;

		stat_cnt = 0;
		while (stat_cnt < ulp_sc_info->num_entries && (sce < sce_end)) {
			data = ulp_sc_info->read_data;

			if (bnxt_ulp_cntxt_acquire_fdb_lock(ctxt))
				break;

			rc = tfc_mpc_batch_start(&batch_info);
			if (unlikely(rc)) {
				PMD_DRV_LOG_LINE(ERR,
						 "MPC batch start failed rc:%d", rc);
				bnxt_ulp_cntxt_release_fdb_lock(ctxt);
				break;
			}

			for (batch = 0; (batch < ULP_SC_BATCH_SIZE) &&
			     (sce < sce_end);) {
				if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID)) {
					sce++;
					continue;
				}
				stat_cnt++;
				tfcp = bnxt_ulp_cntxt_tfcp_get(sce->ctxt);
				if (unlikely(!tfcp)) {
					bnxt_ulp_cntxt_release_fdb_lock(ctxt);
					goto terminate;
				}

				/* Store the entry pointer to use for counter update */
				batch_info.em_hdl[batch_info.count] =
					(uint64_t)sce;

				rc = sc_ops->ulp_stats_cache_update(tfcp,
					    sce->dir,
					    &ulp_sc_info->read_data_iova[batch],
					    sce->handle,
					    &words,
					    &batch_info,
					    false);
				if (unlikely(rc)) {
					/* Abort this batch */
					PMD_DRV_LOG_LINE(ERR,
							 "read_counter() failed:%d", rc);
					break;
				}

				/* Next */
				batch++;
				sce++;
				data += ULP_SC_PAGE_SIZE;
			}

			batch_size = batch_info.count;
			rc = tfc_mpc_batch_end(tfcp, &batch_info);

			bnxt_ulp_cntxt_release_fdb_lock(ctxt);

			if (unlikely(rc)) {
				PMD_DRV_LOG_LINE(ERR,
						 "MPC batch end failed rc:%d", rc);
				batch_info.enabled = false;
				break;
			}

			/* Process counts */
			data = ulp_sc_info->read_data;

			for (batch = 0; batch < batch_size; batch++) {
				/* Check for error in completion */
				if (batch_info.result[batch]) {
					PMD_DRV_LOG_LINE(ERR, "batch:%d result:%d",
							 batch, batch_info.result[batch]);
				} else {
					uint64_t *cptr = (uint64_t *)data;
					uintptr_t em_hdl = batch_info.em_hdl[batch];

					count = (struct ulp_sc_tfc_stats_cache_entry *)em_hdl;
					if (*cptr != count->packet_count) {
						count->packet_count = *cptr;
						cptr++;
						count->byte_count = *cptr;
					}
				}

				data += ULP_SC_PAGE_SIZE;
			}
		}
		/* Sleep to give any other threads opportunity to access ULP */
		rte_delay_us_sleep(ULP_SC_PERIOD_US);
	}

 terminate:
	PMD_DRV_LOG_LINE(ERR, "Terminating the stats cachce thread");
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
	rte_thread_attr_t attr;
	rte_cpuset_t mask;
	size_t i;
	int rc;

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);

	if (ulp_sc_info && !(ulp_sc_info->flags & ULP_FLAG_SC_THREAD)) {
		rte_thread_attr_init(&attr);

		rte_thread_get_affinity(&mask);

		for (i = 1; i < CPU_SETSIZE; i++) {
			if (CPU_ISSET(i, &mask)) {
				CPU_ZERO(&mask);
				CPU_SET(i + 2, &mask);
				break;
			}
		}

		rc = rte_thread_attr_set_affinity(&attr, &mask);
		if (rc)
			return rc;

		rc = rte_thread_create(&ulp_sc_info->tid,
				       &attr,
				       &ulp_stats_cache_main_loop,
				       (void *)ctxt->cfg_data);
		if (rc)
			return rc;

		rte_thread_set_prefixed_name(ulp_sc_info->tid, "ulp_sc_mgr");

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

	/* if thread started then stop it */
	if (ulp_sc_info->flags & ULP_FLAG_SC_THREAD) {
		pthread_cancel((pthread_t)ulp_sc_info->tid.opaque_id);
		ulp_sc_info->flags &= ~ULP_FLAG_SC_THREAD;
	}
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
	struct ulp_fdb_parent_info *pc_entry;
	struct bnxt_ulp_flow_db *flow_db;
	uint32_t max_array;
	uint32_t child_fid;
	uint32_t a_idx;
	uint32_t f2_cnt;
	uint64_t *t;
	uint64_t bs;
	uint64_t packet_count;
	uint64_t byte_count;
	int rc = 0;

	/* Get stats cache info */
	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);
	if (!ulp_sc_info)
		return -ENODEV;

	sce = ulp_sc_info->stats_cache_tbl;
	sce += flow_id;

	/* Save the counts to local variables since they could be modified
	 * in the stats cache loop.
	 */
	packet_count = sce->packet_count;
	byte_count = sce->byte_count;

	/* To handle the parent flow */
	if (sce->flags & ULP_SC_ENTRY_FLAG_PARENT) {
		struct ulp_sc_tfc_stats_cache_entry *f1_sce = sce;

		if (!(f1_sce->flags & ULP_SC_ENTRY_FLAG_VALID))
			return -EBUSY;

		packet_count = 0;
		byte_count = 0;

		flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ctxt);
		if (!flow_db) {
			BNXT_DRV_DBG(ERR, "parent child db validation failed\n");
			return -EINVAL;
		}

		/* Validate the arguments and parent child entry */
		pc_entry = ulp_flow_db_pc_db_entry_get(ctxt, sce->pc_idx);
		if (!pc_entry) {
			BNXT_DRV_DBG(ERR, "failed to get the parent child entry\n");
			return -EINVAL;
		}

		t = pc_entry->child_fid_bitset;
		f2_cnt = pc_entry->f2_cnt;
		max_array = flow_db->parent_child_db.child_bitset_size * 8 / ULP_INDEX_BITMAP_SIZE;

		/* Iterate all possible child flows */
		for (a_idx = 0; (a_idx < max_array) && f2_cnt; a_idx++) {
			/* If it is zero, then check the next bitset */
			bs = t[a_idx];
			if (!bs)
				continue;

			/* check one bitset */
			do {
				/* get the next child fid */
				child_fid = (a_idx * ULP_INDEX_BITMAP_SIZE) + rte_clz64(bs);
				sce = ulp_sc_info->stats_cache_tbl;
				sce += child_fid;

				/* clear the bit for this child flow */
				ULP_INDEX_BITMAP_RESET(bs, child_fid);
				f2_cnt--;

				/* no counter action, then ignore flows */
				if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID))
					continue;

				packet_count += sce->packet_count;
				byte_count += sce->byte_count;
			} while (bs && f2_cnt);
		}

		sce = f1_sce;
	} else {
		/* To handle regular or child flows */
		/* If entry is not valid return an error */
		if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID))
			return -EBUSY;
	}

	if (count->reset) {
		/* Calculate packet count delta */
		count->hits = (packet_count - sce->last_packet_count) & ULP_SC_MAX_COUNT;
		count->bytes = (byte_count - sce->last_byte_count) & ULP_SC_MAX_COUNT;
	} else {
		count->hits = packet_count;
		count->bytes = byte_count;
	}

	/* Save the raw packet count */
	sce->last_packet_count = packet_count;
	sce->last_byte_count = byte_count;

	count->bytes_set = 1;
	count->hits_set = 1;

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

	sce = ulp_sc_info->stats_cache_tbl;
	sce += parms->flow_id;

	/* If entry is not free return an error */
	if (sce->flags & ULP_SC_ENTRY_FLAG_VALID) {
		BNXT_DRV_DBG(ERR, "Entry is not free, invalid flow id %u\n",
			     parms->flow_id);
		return -EBUSY;
	}

	memset(sce, 0, sizeof(*sce));
	sce->ctxt = parms->ulp_ctx;
	sce->flags |= ULP_SC_ENTRY_FLAG_VALID;
	if (parms->parent_flow)
		sce->flags |= ULP_SC_ENTRY_FLAG_PARENT;
	sce->handle = counter_handle;
	sce->dir = tbl->direction;
	ulp_sc_info->num_entries++;
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

	sce = ulp_sc_info->stats_cache_tbl;
	sce += fid;

	if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID)) {
		BNXT_DRV_DBG(ERR, "Entry already free, invalid flow id %u\n",
			     fid);
		return;
	}

	sce->flags = 0;
	ulp_sc_info->num_entries--;
}

/*
 * Set pc_idx for the flow if stat cache info is valid
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * flow_id [in] The HW flow ID
 *
 * pc_idx [in] The parent flow entry idx
 *
 */
void ulp_sc_mgr_set_pc_idx(struct bnxt_ulp_context *ctxt,
			   uint32_t flow_id,
			   uint32_t pc_idx)
{
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct bnxt_ulp_sc_info *ulp_sc_info;

	/* Get stats cache info */
	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);
	if (!ulp_sc_info)
		return;

	sce = ulp_sc_info->stats_cache_tbl;
	sce += flow_id;
	sce->pc_idx = pc_idx & ULP_SC_PC_IDX_MASK;
}
