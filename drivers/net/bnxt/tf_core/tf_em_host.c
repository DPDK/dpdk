/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <math.h>
#include <sys/param.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "tf_core.h"
#include "tf_util.h"
#include "tf_common.h"
#include "tf_em.h"
#include "tf_em_common.h"
#include "tf_msg.h"
#include "tfp.h"
#include "lookup3.h"
#include "tf_ext_flow_handle.h"

#include "bnxt.h"

#define PTU_PTE_VALID          0x1UL
#define PTU_PTE_LAST           0x2UL
#define PTU_PTE_NEXT_TO_LAST   0x4UL

/* Number of pointers per page_size */
#define MAX_PAGE_PTRS(page_size)  ((page_size) / sizeof(void *))

/**
 * Function to free a page table
 *
 * [in] tp
 *   Pointer to the page table to free
 */
static void
tf_em_free_pg_tbl(struct hcapi_cfa_em_page_tbl *tp)
{
	uint32_t i;

	for (i = 0; i < tp->pg_count; i++) {
		if (!tp->pg_va_tbl[i]) {
			TFP_DRV_LOG(WARNING,
				    "No mapping for page: %d table: %016" PRIu64 "\n",
				    i,
				    (uint64_t)(uintptr_t)tp);
			continue;
		}

		tfp_free(tp->pg_va_tbl[i]);
		tp->pg_va_tbl[i] = NULL;
	}

	tp->pg_count = 0;
	tfp_free(tp->pg_va_tbl);
	tp->pg_va_tbl = NULL;
	tfp_free(tp->pg_pa_tbl);
	tp->pg_pa_tbl = NULL;
}

/**
 * Function to free an EM table
 *
 * [in] tbl
 *   Pointer to the EM table to free
 */
static void
tf_em_free_page_table(struct hcapi_cfa_em_table *tbl)
{
	struct hcapi_cfa_em_page_tbl *tp;
	int i;

	for (i = 0; i < tbl->num_lvl; i++) {
		tp = &tbl->pg_tbl[i];
		TFP_DRV_LOG(INFO,
			   "EEM: Freeing page table: size %u lvl %d cnt %u\n",
			   TF_EM_PAGE_SIZE,
			    i,
			    tp->pg_count);

		tf_em_free_pg_tbl(tp);
	}

	tbl->l0_addr = NULL;
	tbl->l0_dma_addr = 0;
	tbl->num_lvl = 0;
	tbl->num_data_pages = 0;
}

/**
 * Allocation of page tables
 *
 * [in] tfp
 *   Pointer to a TruFlow handle
 *
 * [in] pg_count
 *   Page count to allocate
 *
 * [in] pg_size
 *   Size of each page
 *
 * Returns:
 *   0       - Success
 *   -ENOMEM - Out of memory
 */
static int
tf_em_alloc_pg_tbl(struct hcapi_cfa_em_page_tbl *tp,
		   uint32_t pg_count,
		   uint32_t pg_size)
{
	uint32_t i;
	struct tfp_calloc_parms parms;

	parms.nitems = pg_count;
	parms.size = sizeof(void *);
	parms.alignment = 0;

	if (tfp_calloc(&parms) != 0)
		return -ENOMEM;

	tp->pg_va_tbl = parms.mem_va;

	if (tfp_calloc(&parms) != 0) {
		tfp_free(tp->pg_va_tbl);
		return -ENOMEM;
	}

	tp->pg_pa_tbl = parms.mem_va;

	tp->pg_count = 0;
	tp->pg_size = pg_size;

	for (i = 0; i < pg_count; i++) {
		parms.nitems = 1;
		parms.size = pg_size;
		parms.alignment = TF_EM_PAGE_ALIGNMENT;

		if (tfp_calloc(&parms) != 0)
			goto cleanup;

		tp->pg_pa_tbl[i] = (uintptr_t)parms.mem_pa;
		tp->pg_va_tbl[i] = parms.mem_va;

		memset(tp->pg_va_tbl[i], 0, pg_size);
		tp->pg_count++;
	}

	return 0;

cleanup:
	tf_em_free_pg_tbl(tp);
	return -ENOMEM;
}

/**
 * Allocates EM page tables
 *
 * [in] tbl
 *   Table to allocate pages for
 *
 * Returns:
 *   0       - Success
 *   -ENOMEM - Out of memory
 */
static int
tf_em_alloc_page_table(struct hcapi_cfa_em_table *tbl)
{
	struct hcapi_cfa_em_page_tbl *tp;
	int rc = 0;
	int i;
	uint32_t j;

	for (i = 0; i < tbl->num_lvl; i++) {
		tp = &tbl->pg_tbl[i];

		rc = tf_em_alloc_pg_tbl(tp,
					tbl->page_cnt[i],
					TF_EM_PAGE_SIZE);
		if (rc) {
			TFP_DRV_LOG(WARNING,
				"Failed to allocate page table: lvl: %d, rc:%s\n",
				i,
				strerror(-rc));
			goto cleanup;
		}

		for (j = 0; j < tp->pg_count; j++) {
			TFP_DRV_LOG(INFO,
				"EEM: Allocated page table: size %u lvl %d cnt"
				" %u VA:%p PA:%p\n",
				TF_EM_PAGE_SIZE,
				i,
				tp->pg_count,
				(void *)(uintptr_t)tp->pg_va_tbl[j],
				(void *)(uintptr_t)tp->pg_pa_tbl[j]);
		}
	}
	return rc;

cleanup:
	tf_em_free_page_table(tbl);
	return rc;
}

/**
 * Links EM page tables
 *
 * [in] tp
 *   Pointer to page table
 *
 * [in] tp_next
 *   Pointer to the next page table
 *
 * [in] set_pte_last
 *   Flag controlling if the page table is last
 */
static void
tf_em_link_page_table(struct hcapi_cfa_em_page_tbl *tp,
		      struct hcapi_cfa_em_page_tbl *tp_next,
		      bool set_pte_last)
{
	uint64_t *pg_pa = tp_next->pg_pa_tbl;
	uint64_t *pg_va;
	uint64_t valid;
	uint32_t k = 0;
	uint32_t i;
	uint32_t j;

	for (i = 0; i < tp->pg_count; i++) {
		pg_va = tp->pg_va_tbl[i];

		for (j = 0; j < MAX_PAGE_PTRS(tp->pg_size); j++) {
			if (k == tp_next->pg_count - 2 && set_pte_last)
				valid = PTU_PTE_NEXT_TO_LAST | PTU_PTE_VALID;
			else if (k == tp_next->pg_count - 1 && set_pte_last)
				valid = PTU_PTE_LAST | PTU_PTE_VALID;
			else
				valid = PTU_PTE_VALID;

			pg_va[j] = tfp_cpu_to_le_64(pg_pa[k] | valid);
			if (++k >= tp_next->pg_count)
				return;
		}
	}
}

/**
 * Setup a EM page table
 *
 * [in] tbl
 *   Pointer to EM page table
 */
static void
tf_em_setup_page_table(struct hcapi_cfa_em_table *tbl)
{
	struct hcapi_cfa_em_page_tbl *tp_next;
	struct hcapi_cfa_em_page_tbl *tp;
	bool set_pte_last = 0;
	int i;

	for (i = 0; i < tbl->num_lvl - 1; i++) {
		tp = &tbl->pg_tbl[i];
		tp_next = &tbl->pg_tbl[i + 1];
		if (i == tbl->num_lvl - 2)
			set_pte_last = 1;
		tf_em_link_page_table(tp, tp_next, set_pte_last);
	}

	tbl->l0_addr = tbl->pg_tbl[TF_PT_LVL_0].pg_va_tbl[0];
	tbl->l0_dma_addr = tbl->pg_tbl[TF_PT_LVL_0].pg_pa_tbl[0];
}

/**
 * Unregisters EM Ctx in Firmware
 *
 * [in] tfp
 *   Pointer to a TruFlow handle
 *
 * [in] tbl_scope_cb
 *   Pointer to a table scope control block
 *
 * [in] dir
 *   Receive or transmit direction
 */
static void
tf_em_ctx_unreg(struct tf *tfp,
		struct tf_tbl_scope_cb *tbl_scope_cb,
		int dir)
{
	struct hcapi_cfa_em_ctx_mem_info *ctxp = &tbl_scope_cb->em_ctx_info[dir];
	struct hcapi_cfa_em_table *tbl;
	int i;

	for (i = TF_KEY0_TABLE; i < TF_MAX_TABLE; i++) {
		tbl = &ctxp->em_tables[i];

		if (tbl->num_entries != 0 && tbl->entry_size != 0) {
			tf_msg_em_mem_unrgtr(tfp, &tbl->ctx_id);
			tf_em_free_page_table(tbl);
		}
	}
}

/**
 * Registers EM Ctx in Firmware
 *
 * [in] tfp
 *   Pointer to a TruFlow handle
 *
 * [in] tbl_scope_cb
 *   Pointer to a table scope control block
 *
 * [in] dir
 *   Receive or transmit direction
 *
 * Returns:
 *   0       - Success
 *   -ENOMEM - Out of Memory
 */
static int
tf_em_ctx_reg(struct tf *tfp,
	      struct tf_tbl_scope_cb *tbl_scope_cb,
	      int dir)
{
	struct hcapi_cfa_em_ctx_mem_info *ctxp = &tbl_scope_cb->em_ctx_info[dir];
	struct hcapi_cfa_em_table *tbl;
	int rc = 0;
	int i;

	for (i = TF_KEY0_TABLE; i < TF_MAX_TABLE; i++) {
		tbl = &ctxp->em_tables[i];

		if (tbl->num_entries && tbl->entry_size) {
			rc = tf_em_size_table(tbl, TF_EM_PAGE_SIZE);

			if (rc)
				goto cleanup;

			rc = tf_em_alloc_page_table(tbl);
			if (rc)
				goto cleanup;

			tf_em_setup_page_table(tbl);
			rc = tf_msg_em_mem_rgtr(tfp,
						tbl->num_lvl - 1,
						TF_EM_PAGE_SIZE_ENUM,
						tbl->l0_dma_addr,
						&tbl->ctx_id);
			if (rc)
				goto cleanup;
		}
	}
	return rc;

cleanup:
	tf_em_ctx_unreg(tfp, tbl_scope_cb, dir);
	return rc;
}

int
tf_em_ext_alloc(struct tf *tfp,
		struct tf_alloc_tbl_scope_parms *parms)
{
	int rc;
	enum tf_dir dir;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct hcapi_cfa_em_table *em_tables;
	struct tf_free_tbl_scope_parms free_parms;
	struct tf_rm_allocate_parms aparms = { 0 };
	struct tf_rm_free_parms fparms = { 0 };
	struct tfp_calloc_parms cparms;
	struct tf_session *tfs = NULL;
	struct em_ext_db *ext_db = NULL;
	void *ext_ptr = NULL;
	uint16_t pf;


	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR, "Failed to get tf_session, rc:%s\n",
		strerror(-rc));
		return rc;
	}

	rc = tf_session_get_em_ext_db(tfp, &ext_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			"Failed to get em_ext_db from session, rc:%s\n",
			strerror(-rc));
		return rc;
	}
	ext_db = (struct em_ext_db *)ext_ptr;

	rc = tfp_get_pf(tfp, &pf);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "EEM: PF query error rc:%s\n",
			    strerror(-rc));
		goto cleanup;
	}

	/* Get Table Scope control block from the session pool */
	aparms.rm_db = ext_db->eem_db[TF_DIR_RX];
	aparms.subtype = TF_EM_TBL_TYPE_TBL_SCOPE;
	aparms.index = (uint32_t *)&parms->tbl_scope_id;
	rc = tf_rm_allocate(&aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to allocate table scope\n");
		goto cleanup;
	}

	/* Create tbl_scope, initialize and attach to the session */
	cparms.nitems = 1;
	cparms.size = sizeof(struct tf_tbl_scope_cb);
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			"Failed to allocate session table scope, rc:%s\n",
			strerror(-rc));
		goto cleanup;
	}

	tbl_scope_cb = cparms.mem_va;
	tbl_scope_cb->tbl_scope_id = parms->tbl_scope_id;
	tbl_scope_cb->pf = pf;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		rc = tf_msg_em_qcaps(tfp,
				     dir,
				     &tbl_scope_cb->em_caps[dir]);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "EEM: Unable to query for EEM capability,"
				    " rc:%s\n",
				    strerror(-rc));
			goto cleanup_ts;
		}
	}

	/*
	 * Validate and setup table sizes
	 */
	if (tf_em_validate_num_entries(tbl_scope_cb, parms))
		goto cleanup_ts;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		/*
		 * Allocate tables and signal configuration to FW
		 */
		rc = tf_em_ctx_reg(tfp, tbl_scope_cb, dir);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "EEM: Unable to register for EEM ctx,"
				    " rc:%s\n",
				    strerror(-rc));
			goto cleanup_ts;
		}

		em_tables = tbl_scope_cb->em_ctx_info[dir].em_tables;
		rc = tf_msg_em_cfg(tfp,
				   em_tables[TF_KEY0_TABLE].num_entries,
				   em_tables[TF_KEY0_TABLE].ctx_id,
				   em_tables[TF_KEY1_TABLE].ctx_id,
				   em_tables[TF_RECORD_TABLE].ctx_id,
				   em_tables[TF_EFC_TABLE].ctx_id,
				   parms->hw_flow_cache_flush_timer,
				   dir);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "TBL: Unable to configure EEM in firmware"
				    " rc:%s\n",
				    strerror(-rc));
			goto cleanup_full;
		}

		rc = tf_msg_em_op(tfp,
				  dir,
				  HWRM_TF_EXT_EM_OP_INPUT_OP_EXT_EM_ENABLE);

		if (rc) {
			TFP_DRV_LOG(ERR,
				    "EEM: Unable to enable EEM in firmware"
				    " rc:%s\n",
				    strerror(-rc));
			goto cleanup_full;
		}

		/* Allocate the pool of offsets of the external memory.
		 * Initially, this is a single fixed size pool for all external
		 * actions related to a single table scope.
		 */
		rc = tf_create_tbl_pool_external(dir,
					    tbl_scope_cb,
					    em_tables[TF_RECORD_TABLE].num_entries,
					    em_tables[TF_RECORD_TABLE].entry_size);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s TBL: Unable to allocate idx pools %s\n",
				    tf_dir_2_str(dir),
				    strerror(-rc));
			goto cleanup_full;
		}
	}

	/* Insert into session tbl_scope list */
	ll_insert(&ext_db->tbl_scope_ll, &tbl_scope_cb->ll_entry);
	return 0;

cleanup_full:
	free_parms.tbl_scope_id = parms->tbl_scope_id;
	/* Insert into session list prior to ext_free */
	ll_insert(&ext_db->tbl_scope_ll, &tbl_scope_cb->ll_entry);
	tf_em_ext_free(tfp, &free_parms);
	return -EINVAL;

cleanup_ts:
	tfp_free(tbl_scope_cb);

cleanup:
	/* Free Table control block */
	fparms.rm_db = ext_db->eem_db[TF_DIR_RX];
	fparms.subtype = TF_EM_TBL_TYPE_TBL_SCOPE;
	fparms.index = parms->tbl_scope_id;
	rc = tf_rm_free(&fparms);
	if (rc)
		TFP_DRV_LOG(ERR, "Failed to free table scope\n");

	return -EINVAL;
}

int
tf_em_ext_free(struct tf *tfp,
	       struct tf_free_tbl_scope_parms *parms)
{
	int rc = 0;
	enum tf_dir  dir;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct tf_session *tfs;
	struct em_ext_db *ext_db = NULL;
	void *ext_ptr = NULL;
	struct tf_rm_free_parms aparms = { 0 };

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR, "Failed to get tf_session, rc:%s\n",
			    strerror(-rc));
		return -EINVAL;
	}

	rc = tf_session_get_em_ext_db(tfp, &ext_ptr);
	if (rc) {
		TFP_DRV_LOG(ERR,
			"Failed to get em_ext_db from session, rc:%s\n",
			strerror(-rc));
		return rc;
	}
	ext_db = (struct em_ext_db *)ext_ptr;

	tbl_scope_cb = tf_em_ext_common_tbl_scope_find(tfp, parms->tbl_scope_id);
	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR, "Table scope error\n");
		return -EINVAL;
	}

	/* Free Table control block */
	aparms.rm_db = ext_db->eem_db[TF_DIR_RX];
	aparms.subtype = TF_EM_TBL_TYPE_TBL_SCOPE;
	aparms.index = parms->tbl_scope_id;
	rc = tf_rm_free(&aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to free table scope\n");
	}

	/* free table scope locks */
	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		/* Free associated external pools
		 */
		tf_destroy_tbl_pool_external(dir,
					     tbl_scope_cb);
		tf_msg_em_op(tfp,
			     dir,
			     HWRM_TF_EXT_EM_OP_INPUT_OP_EXT_EM_DISABLE);

		/* free table scope and all associated resources */
		tf_em_ctx_unreg(tfp, tbl_scope_cb, dir);
	}

	/* remove from session list and free tbl_scope */
	ll_delete(&ext_db->tbl_scope_ll, &tbl_scope_cb->ll_entry);
	tfp_free(tbl_scope_cb);
	return rc;
}
