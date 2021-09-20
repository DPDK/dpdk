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
#include "tf_device.h"
#include "tf_ext_flow_handle.h"
#include "hcapi_cfa.h"

#include "bnxt.h"


/** Invalid table scope id */
#define TF_TBL_SCOPE_INVALID 0xffffffff

/* Number of pointers per page_size */
#define MAX_PAGE_PTRS(page_size)  ((page_size) / sizeof(void *))

/**
 * Host or system
 */
static enum tf_mem_type mem_type;

/* API defined in tf_em.h */
int
tf_create_tbl_pool_external(enum tf_dir dir,
			    struct tf_tbl_scope_cb *tbl_scope_cb,
			    uint32_t num_entries,
			    uint32_t entry_sz_bytes)
{
	struct tfp_calloc_parms parms;
	uint32_t i;
	int32_t j;
	int rc = 0;
	struct stack *pool = &tbl_scope_cb->ext_act_pool[dir];

	parms.nitems = num_entries;
	parms.size = sizeof(uint32_t);
	parms.alignment = 0;

	if (tfp_calloc(&parms) != 0) {
		TFP_DRV_LOG(ERR, "%s: TBL: external pool failure %s\n",
			    tf_dir_2_str(dir), strerror(ENOMEM));
		return -ENOMEM;
	}

	/* Create empty stack
	 */
	rc = stack_init(num_entries, parms.mem_va, pool);

	if (rc != 0) {
		TFP_DRV_LOG(ERR, "%s: TBL: stack init failure %s\n",
			    tf_dir_2_str(dir), strerror(-rc));
		goto cleanup;
	}

	/* Save the  malloced memory address so that it can
	 * be freed when the table scope is freed.
	 */
	tbl_scope_cb->ext_act_pool_mem[dir] = (uint32_t *)parms.mem_va;

	/* Fill pool with indexes in reverse
	 */
	j = (num_entries - 1) * entry_sz_bytes;

	for (i = 0; i < num_entries; i++) {
		rc = stack_push(pool, j);
		if (rc != 0) {
			TFP_DRV_LOG(ERR, "%s TBL: stack failure %s\n",
				    tf_dir_2_str(dir), strerror(-rc));
			goto cleanup;
		}

		if (j < 0) {
			TFP_DRV_LOG(ERR, "%d TBL: invalid offset (%d)\n",
				    dir, j);
			goto cleanup;
		}
		j -= entry_sz_bytes;
	}

	if (!stack_is_full(pool)) {
		rc = -EINVAL;
		TFP_DRV_LOG(ERR, "%s TBL: stack failure %s\n",
			    tf_dir_2_str(dir), strerror(-rc));
		goto cleanup;
	}
	return 0;
cleanup:
	tfp_free((void *)parms.mem_va);
	return rc;
}

/**
 * Destroy External Tbl pool of memory indexes.
 *
 * [in] dir
 *   direction
 * [in] tbl_scope_cb
 *   pointer to the table scope
 */
void
tf_destroy_tbl_pool_external(enum tf_dir dir,
			     struct tf_tbl_scope_cb *tbl_scope_cb)
{
	uint32_t *ext_act_pool_mem =
		tbl_scope_cb->ext_act_pool_mem[dir];

	tfp_free(ext_act_pool_mem);
}

/**
 * Looks up table scope control block using tbl_scope_id from tf_session.
 *
 * [in] tfp
 *   Pointer to Truflow Handle
 * [in] tbl_scope_id
 *   table scope id
 *
 * Return:
 *  - Pointer to the tf_tbl_scope_cb, if found.
 *  - (NULL) on failure, not found.
 */
struct tf_tbl_scope_cb *
tf_em_ext_common_tbl_scope_find(struct tf *tfp,
			uint32_t tbl_scope_id)
{
	int rc;
	struct em_ext_db *ext_db;
	void *ext_ptr = NULL;
	struct tf_tbl_scope_cb *tbl_scope_cb = NULL;
	struct ll_entry *entry;

	rc = tf_session_get_em_ext_db(tfp, &ext_ptr);
	if (rc)
		return NULL;

	ext_db = (struct em_ext_db *)ext_ptr;

	for (entry = ext_db->tbl_scope_ll.head; entry != NULL;
			entry = entry->next) {
		tbl_scope_cb = (struct tf_tbl_scope_cb *)entry;
		if (tbl_scope_cb->tbl_scope_id == tbl_scope_id)
			return tbl_scope_cb;
	}

	return NULL;
}

/**
 * Allocate External Tbl entry from the scope pool.
 *
 * [in] tfp
 *   Pointer to Truflow Handle
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry allocated - no search support
 *  -ENOMEM -EINVAL -EOPNOTSUPP
 *          - Failure, entry not allocated, out of resources
 */
int
tf_tbl_ext_alloc(struct tf *tfp,
		 struct tf_tbl_alloc_parms *parms)
{
	int rc;
	uint32_t index;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct stack *pool;

	TF_CHECK_PARMS2(tfp, parms);

	tbl_scope_cb = tf_em_ext_common_tbl_scope_find(tfp, parms->tbl_scope_id);
	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR,
			    "%s, table scope not allocated\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	pool = &tbl_scope_cb->ext_act_pool[parms->dir];

	/* Allocate an element
	 */
	rc = stack_pop(pool, &index);

	if (rc != 0) {
		TFP_DRV_LOG(ERR,
		   "%s, Allocation failed, type:%d\n",
		   tf_dir_2_str(parms->dir),
		   parms->type);
		return rc;
	}

	*parms->idx = index;
	return rc;
}

/**
 * Free External Tbl entry to the scope pool.
 *
 * [in] tfp
 *   Pointer to Truflow Handle
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry freed
 *
 * - Failure, entry not successfully freed for these reasons
 *  -ENOMEM
 *  -EOPNOTSUPP
 *  -EINVAL
 */
int
tf_tbl_ext_free(struct tf *tfp,
		struct tf_tbl_free_parms *parms)
{
	int rc = 0;
	uint32_t index;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct stack *pool;

	TF_CHECK_PARMS2(tfp, parms);

	tbl_scope_cb = tf_em_ext_common_tbl_scope_find(tfp, parms->tbl_scope_id);
	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR,
			    "%s, table scope error\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}
	pool = &tbl_scope_cb->ext_act_pool[parms->dir];

	index = parms->idx;

	rc = stack_push(pool, index);

	if (rc != 0) {
		TFP_DRV_LOG(ERR,
		   "%s, consistency error, stack full, type:%d, idx:%d\n",
		   tf_dir_2_str(parms->dir),
		   parms->type,
		   index);
	}
	return rc;
}

uint32_t
tf_em_get_key_mask(int num_entries)
{
	uint32_t mask = num_entries - 1;

	if (num_entries & TF_EM_MAX_MASK)
		return 0;

	if (num_entries > TF_EM_MAX_ENTRY)
		return 0;

	return mask;
}

void
tf_em_create_key_entry(struct cfa_p4_eem_entry_hdr *result,
		       uint8_t *in_key,
		       struct cfa_p4_eem_64b_entry *key_entry)
{
	key_entry->hdr.word1 = result->word1;
	key_entry->hdr.pointer = result->pointer;
	memcpy(key_entry->key, in_key, TF_P4_HW_EM_KEY_MAX_SIZE + 4);
}


/**
 * Return the number of page table pages needed to
 * reference the given number of next level pages.
 *
 * [in] num_pages
 *   Number of EM pages
 *
 * [in] page_size
 *   Size of each EM page
 *
 * Returns:
 *   Number of EM page table pages
 */
static uint32_t
tf_em_page_tbl_pgcnt(uint32_t num_pages,
		     uint32_t page_size)
{
	return roundup(num_pages, MAX_PAGE_PTRS(page_size)) /
		       MAX_PAGE_PTRS(page_size);
	return 0;
}

/**
 * Given the number of data pages, page_size and the maximum
 * number of page table levels (already determined), size
 * the number of page table pages required at each level.
 *
 * [in] max_lvl
 *   Max number of levels
 *
 * [in] num_data_pages
 *   Number of EM data pages
 *
 * [in] page_size
 *   Size of an EM page
 *
 * [out] *page_cnt
 *   EM page count
 */
static void
tf_em_size_page_tbls(int max_lvl,
		     uint64_t num_data_pages,
		     uint32_t page_size,
		     uint32_t *page_cnt)
{
	if (max_lvl == TF_PT_LVL_0) {
		page_cnt[TF_PT_LVL_0] = num_data_pages;
	} else if (max_lvl == TF_PT_LVL_1) {
		page_cnt[TF_PT_LVL_1] = num_data_pages;
		page_cnt[TF_PT_LVL_0] =
		tf_em_page_tbl_pgcnt(page_cnt[TF_PT_LVL_1], page_size);
	} else if (max_lvl == TF_PT_LVL_2) {
		page_cnt[TF_PT_LVL_2] = num_data_pages;
		page_cnt[TF_PT_LVL_1] =
		tf_em_page_tbl_pgcnt(page_cnt[TF_PT_LVL_2], page_size);
		page_cnt[TF_PT_LVL_0] =
		tf_em_page_tbl_pgcnt(page_cnt[TF_PT_LVL_1], page_size);
	} else {
		return;
	}
}

/**
 * Given the page size, size of each data item (entry size),
 * and the total number of entries needed, determine the number
 * of page table levels and the number of data pages required.
 *
 * [in] page_size
 *   Page size
 *
 * [in] entry_size
 *   Entry size
 *
 * [in] num_entries
 *   Number of entries needed
 *
 * [out] num_data_pages
 *   Number of pages required
 *
 * Returns:
 *   Success  - Number of EM page levels required
 *   -ENOMEM  - Out of memory
 */
static int
tf_em_size_page_tbl_lvl(uint32_t page_size,
			uint32_t entry_size,
			uint32_t num_entries,
			uint64_t *num_data_pages)
{
	uint64_t lvl_data_size = page_size;
	int lvl = TF_PT_LVL_0;
	uint64_t data_size;

	*num_data_pages = 0;
	data_size = (uint64_t)num_entries * entry_size;

	while (lvl_data_size < data_size) {
		lvl++;

		if (lvl == TF_PT_LVL_1)
			lvl_data_size = (uint64_t)MAX_PAGE_PTRS(page_size) *
				page_size;
		else if (lvl == TF_PT_LVL_2)
			lvl_data_size = (uint64_t)MAX_PAGE_PTRS(page_size) *
				MAX_PAGE_PTRS(page_size) * page_size;
		else
			return -ENOMEM;
	}

	*num_data_pages = roundup(data_size, page_size) / page_size;

	return lvl;
}

/**
 * Size the EM table based on capabilities
 *
 * [in] tbl
 *   EM table to size
 *
 * Returns:
 *   0        - Success
 *   - EINVAL - Parameter error
 *   - ENOMEM - Out of memory
 */
int
tf_em_size_table(struct hcapi_cfa_em_table *tbl,
		 uint32_t page_size)
{
	uint64_t num_data_pages;
	uint32_t *page_cnt;
	int max_lvl;
	uint32_t num_entries;
	uint32_t cnt = TF_EM_MIN_ENTRIES;

	/* Ignore entry if both size and number are zero */
	if (!tbl->entry_size && !tbl->num_entries)
		return 0;

	/* If only one is set then error */
	if (!tbl->entry_size || !tbl->num_entries)
		return -EINVAL;

	/* Determine number of page table levels and the number
	 * of data pages needed to process the given eem table.
	 */
	if (tbl->type == TF_RECORD_TABLE) {
		/*
		 * For action records just a memory size is provided. Work
		 * backwards to resolve to number of entries
		 */
		num_entries = tbl->num_entries / tbl->entry_size;
		if (num_entries < TF_EM_MIN_ENTRIES) {
			num_entries = TF_EM_MIN_ENTRIES;
		} else {
			while (num_entries > cnt && cnt <= TF_EM_MAX_ENTRIES)
				cnt *= 2;
			num_entries = cnt;
		}
	} else {
		num_entries = tbl->num_entries;
	}

	max_lvl = tf_em_size_page_tbl_lvl(page_size,
					  tbl->entry_size,
					  tbl->num_entries,
					  &num_data_pages);
	if (max_lvl < 0) {
		TFP_DRV_LOG(WARNING, "EEM: Failed to size page table levels\n");
		TFP_DRV_LOG(WARNING,
			    "table: %d data-sz: %016" PRIu64 " page-sz: %u\n",
			    tbl->type, (uint64_t)num_entries * tbl->entry_size,
			    page_size);
		return -ENOMEM;
	}

	tbl->num_lvl = max_lvl + 1;
	tbl->num_data_pages = num_data_pages;

	/* Determine the number of pages needed at each level */
	page_cnt = tbl->page_cnt;
	memset(page_cnt, 0, sizeof(tbl->page_cnt));
	tf_em_size_page_tbls(max_lvl, num_data_pages, page_size,
				page_cnt);

	TFP_DRV_LOG(INFO, "EEM: Sized page table: %d\n", tbl->type);
	TFP_DRV_LOG(INFO,
		    "EEM: lvls: %d sz: %016" PRIu64 " pgs: %016" PRIu64 \
		    " l0: %u l1: %u l2: %u\n",
		    max_lvl + 1,
		    (uint64_t)num_data_pages * page_size,
		    num_data_pages,
		    page_cnt[TF_PT_LVL_0],
		    page_cnt[TF_PT_LVL_1],
		    page_cnt[TF_PT_LVL_2]);

	return 0;
}

/**
 * Validates EM number of entries requested
 *
 * [in] tbl_scope_cb
 *   Pointer to table scope control block to be populated
 *
 * [in] parms
 *   Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int
tf_em_validate_num_entries(struct tf_tbl_scope_cb *tbl_scope_cb,
			   struct tf_alloc_tbl_scope_parms *parms)
{
	uint32_t cnt;

	if (parms->rx_mem_size_in_mb != 0) {
		uint32_t key_b = 2 * ((parms->rx_max_key_sz_in_bits / 8) + 1);
		uint32_t action_b = ((parms->rx_max_action_entry_sz_in_bits / 8)
				     + 1);
		uint32_t num_entries = (parms->rx_mem_size_in_mb *
					TF_MEGABYTE) / (key_b + action_b);

		if (num_entries < TF_EM_MIN_ENTRIES) {
			TFP_DRV_LOG(ERR, "EEM: Insufficient memory requested:"
				    "%uMB\n",
				    parms->rx_mem_size_in_mb);
			return -EINVAL;
		}

		cnt = TF_EM_MIN_ENTRIES;
		while (num_entries > cnt &&
		       cnt <= TF_EM_MAX_ENTRIES)
			cnt *= 2;

		if (cnt > TF_EM_MAX_ENTRIES) {
			TFP_DRV_LOG(ERR, "EEM: Invalid number of Tx requested: "
				    "%u\n",
		       (parms->tx_num_flows_in_k * TF_KILOBYTE));
			return -EINVAL;
		}

		parms->rx_num_flows_in_k = cnt / TF_KILOBYTE;
	} else {
		if ((parms->rx_num_flows_in_k * TF_KILOBYTE) <
		    TF_EM_MIN_ENTRIES ||
		    (parms->rx_num_flows_in_k * TF_KILOBYTE) >
		    tbl_scope_cb->em_caps[TF_DIR_RX].max_entries_supported) {
			TFP_DRV_LOG(ERR,
				    "EEM: Invalid number of Rx flows "
				    "requested:%u max:%u\n",
				    parms->rx_num_flows_in_k * TF_KILOBYTE,
			tbl_scope_cb->em_caps[TF_DIR_RX].max_entries_supported);
			return -EINVAL;
		}

		/* must be a power-of-2 supported value
		 * in the range 32K - 128M
		 */
		cnt = TF_EM_MIN_ENTRIES;
		while ((parms->rx_num_flows_in_k * TF_KILOBYTE) != cnt &&
		       cnt <= TF_EM_MAX_ENTRIES)
			cnt *= 2;

		if (cnt > TF_EM_MAX_ENTRIES) {
			TFP_DRV_LOG(ERR,
				    "EEM: Invalid number of Rx requested: %u\n",
				    (parms->rx_num_flows_in_k * TF_KILOBYTE));
			return -EINVAL;
		}
	}

	if (parms->tx_mem_size_in_mb != 0) {
		uint32_t key_b = 2 * (parms->tx_max_key_sz_in_bits / 8 + 1);
		uint32_t action_b = ((parms->tx_max_action_entry_sz_in_bits / 8)
				     + 1);
		uint32_t num_entries = (parms->tx_mem_size_in_mb *
					(TF_KILOBYTE * TF_KILOBYTE)) /
			(key_b + action_b);

		if (num_entries < TF_EM_MIN_ENTRIES) {
			TFP_DRV_LOG(ERR,
				    "EEM: Insufficient memory requested:%uMB\n",
				    parms->rx_mem_size_in_mb);
			return -EINVAL;
		}

		cnt = TF_EM_MIN_ENTRIES;
		while (num_entries > cnt &&
		       cnt <= TF_EM_MAX_ENTRIES)
			cnt *= 2;

		if (cnt > TF_EM_MAX_ENTRIES) {
			TFP_DRV_LOG(ERR,
				    "EEM: Invalid number of Tx requested: %u\n",
		       (parms->tx_num_flows_in_k * TF_KILOBYTE));
			return -EINVAL;
		}

		parms->tx_num_flows_in_k = cnt / TF_KILOBYTE;
	} else {
		if ((parms->tx_num_flows_in_k * TF_KILOBYTE) <
		    TF_EM_MIN_ENTRIES ||
		    (parms->tx_num_flows_in_k * TF_KILOBYTE) >
		    tbl_scope_cb->em_caps[TF_DIR_TX].max_entries_supported) {
			TFP_DRV_LOG(ERR,
				    "EEM: Invalid number of Tx flows "
				    "requested:%u max:%u\n",
				    (parms->tx_num_flows_in_k * TF_KILOBYTE),
			tbl_scope_cb->em_caps[TF_DIR_TX].max_entries_supported);
			return -EINVAL;
		}

		cnt = TF_EM_MIN_ENTRIES;
		while ((parms->tx_num_flows_in_k * TF_KILOBYTE) != cnt &&
		       cnt <= TF_EM_MAX_ENTRIES)
			cnt *= 2;

		if (cnt > TF_EM_MAX_ENTRIES) {
			TFP_DRV_LOG(ERR,
				    "EEM: Invalid number of Tx requested: %u\n",
		       (parms->tx_num_flows_in_k * TF_KILOBYTE));
			return -EINVAL;
		}
	}

	if (parms->rx_num_flows_in_k != 0 &&
	    parms->rx_max_key_sz_in_bits / 8 == 0) {
		TFP_DRV_LOG(ERR,
			    "EEM: Rx key size required: %u\n",
			    (parms->rx_max_key_sz_in_bits));
		return -EINVAL;
	}

	if (parms->tx_num_flows_in_k != 0 &&
	    parms->tx_max_key_sz_in_bits / 8 == 0) {
		TFP_DRV_LOG(ERR,
			    "EEM: Tx key size required: %u\n",
			    (parms->tx_max_key_sz_in_bits));
		return -EINVAL;
	}
	/* Rx */
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_KEY0_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_KEY0_TABLE].entry_size =
		parms->rx_max_key_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_KEY1_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_KEY1_TABLE].entry_size =
		parms->rx_max_key_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_RECORD_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_RECORD_TABLE].entry_size =
		parms->rx_max_action_entry_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_EFC_TABLE].num_entries =
		0;

	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_ACTION_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_ACTION_TABLE].entry_size =
		parms->rx_max_action_entry_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_EM_LKUP_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_EM_LKUP_TABLE].entry_size =
		parms->rx_max_key_sz_in_bits / 8;

	/* Tx */
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_KEY0_TABLE].num_entries =
		parms->tx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_KEY0_TABLE].entry_size =
		parms->tx_max_key_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_KEY1_TABLE].num_entries =
		parms->tx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_KEY1_TABLE].entry_size =
		parms->tx_max_key_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_RECORD_TABLE].num_entries =
		parms->tx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_RECORD_TABLE].entry_size =
		parms->tx_max_action_entry_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_EFC_TABLE].num_entries =
		0;

	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_ACTION_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_ACTION_TABLE].entry_size =
		parms->tx_max_action_entry_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_EM_LKUP_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[TF_EM_LKUP_TABLE].entry_size =
		parms->tx_max_key_sz_in_bits / 8;

	return 0;
}

/** insert EEM entry API
 *
 * returns:
 *  0
 *  TF_ERR	    - unable to get lock
 *
 * insert callback returns:
 *   0
 *   TF_ERR_EM_DUP  - key is already in table
 */
static int
tf_insert_eem_entry(struct tf_dev_info *dev,
		    struct tf_tbl_scope_cb *tbl_scope_cb,
		    struct tf_insert_em_entry_parms *parms)
{
	uint32_t mask;
	uint32_t key0_hash;
	uint32_t key1_hash;
	uint32_t key0_index;
	uint32_t key1_index;
	struct cfa_p4_eem_64b_entry key_entry;
	uint32_t index;
	enum hcapi_cfa_em_table_type table_type;
	uint32_t gfid;
	struct hcapi_cfa_hwop op;
	struct hcapi_cfa_key_tbl key_tbl;
	struct hcapi_cfa_key_data key_obj;
	struct hcapi_cfa_key_loc key_loc;
	uint64_t big_hash;
	int rc;

	/* Get mask to use on hash */
	mask = tf_em_get_key_mask(tbl_scope_cb->em_ctx_info[parms->dir].em_tables[TF_KEY0_TABLE].num_entries);

	if (!mask)
		return -EINVAL;

	if (dev->ops->tf_dev_cfa_key_hash == NULL)
		return -EINVAL;

	big_hash = dev->ops->tf_dev_cfa_key_hash((uint64_t *)parms->key,
					 (TF_P4_HW_EM_KEY_MAX_SIZE + 4) * 8);
	key0_hash = (uint32_t)(big_hash >> 32);
	key1_hash = (uint32_t)(big_hash & 0xFFFFFFFF);

	key0_index = key0_hash & mask;
	key1_index = key1_hash & mask;

	/*
	 * Use the "result" arg to populate all of the key entry then
	 * store the byte swapped "raw" entry in a local copy ready
	 * for insertion in to the table.
	 */
	tf_em_create_key_entry((struct cfa_p4_eem_entry_hdr *)parms->em_record,
				((uint8_t *)parms->key),
				&key_entry);

	/*
	 * Try to add to Key0 table, if that does not work then
	 * try the key1 table.
	 */
	index = key0_index;
	op.opcode = HCAPI_CFA_HWOPS_ADD;
	key_tbl.base0 =
		(uint8_t *)&tbl_scope_cb->em_ctx_info[parms->dir].em_tables[TF_KEY0_TABLE];
	key_tbl.page_size = TF_EM_PAGE_SIZE;
	key_obj.offset = index * TF_P4_EM_KEY_RECORD_SIZE;
	key_obj.data = (uint8_t *)&key_entry;
	key_obj.size = TF_P4_EM_KEY_RECORD_SIZE;

	rc = cfa_p4_devops.hcapi_cfa_key_hw_op(&op,
					       &key_tbl,
					       &key_obj,
					       &key_loc);

	if (rc == 0) {
		table_type = TF_KEY0_TABLE;
	} else {
		index = key1_index;

		key_tbl.base0 =
			(uint8_t *)&tbl_scope_cb->em_ctx_info[parms->dir].em_tables[TF_KEY1_TABLE];
		key_obj.offset = index * TF_P4_EM_KEY_RECORD_SIZE;

		rc = cfa_p4_devops.hcapi_cfa_key_hw_op(&op,
						       &key_tbl,
						       &key_obj,
						       &key_loc);
		if (rc != 0)
			return rc;

		table_type = TF_KEY1_TABLE;
	}

	TF_SET_GFID(gfid,
		    index,
		    table_type);
	TF_SET_FLOW_ID(parms->flow_id,
		       gfid,
		       TF_GFID_TABLE_EXTERNAL,
		       parms->dir);
	TF_SET_FIELDS_IN_FLOW_HANDLE(parms->flow_handle,
				     0,
				     0,
				     0,
				     index,
				     0,
				     table_type);

	return 0;
}

/** delete EEM hash entry API
 *
 * returns:
 *   0
 *   -EINVAL	  - parameter error
 *   TF_NO_SESSION    - bad session ID
 *   TF_ERR_TBL_SCOPE - invalid table scope
 *   TF_ERR_TBL_IF    - invalid table interface
 *
 * insert callback returns
 *   0
 *   TF_NO_EM_MATCH - entry not found
 */
static int
tf_delete_eem_entry(struct tf_tbl_scope_cb *tbl_scope_cb,
		    struct tf_delete_em_entry_parms *parms)
{
	enum hcapi_cfa_em_table_type hash_type;
	uint32_t index;
	struct hcapi_cfa_hwop op;
	struct hcapi_cfa_key_tbl key_tbl;
	struct hcapi_cfa_key_data key_obj;
	struct hcapi_cfa_key_loc key_loc;
	int rc;

	TF_GET_HASH_TYPE_FROM_FLOW_HANDLE(parms->flow_handle, hash_type);
	TF_GET_INDEX_FROM_FLOW_HANDLE(parms->flow_handle, index);

	op.opcode = HCAPI_CFA_HWOPS_DEL;
	key_tbl.base0 =
		(uint8_t *)&tbl_scope_cb->em_ctx_info[parms->dir].em_tables
			[(hash_type == 0 ? TF_KEY0_TABLE : TF_KEY1_TABLE)];
	key_tbl.page_size = TF_EM_PAGE_SIZE;
	key_obj.offset = index * TF_P4_EM_KEY_RECORD_SIZE;
	key_obj.data = NULL;
	key_obj.size = TF_P4_EM_KEY_RECORD_SIZE;

	rc = cfa_p4_devops.hcapi_cfa_key_hw_op(&op,
					       &key_tbl,
					       &key_obj,
					       &key_loc);

	if (!rc)
		return rc;

	return 0;
}

/** insert EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int
tf_em_insert_ext_entry(struct tf *tfp,
		       struct tf_insert_em_entry_parms *parms)
{
	int rc;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct tf_session *tfs;
	struct tf_dev_info *dev;

	tbl_scope_cb = tf_em_ext_common_tbl_scope_find(tfp, parms->tbl_scope_id);
	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR, "Invalid tbl_scope_cb\n");
		return -EINVAL;
	}

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	return tf_insert_eem_entry
		(dev,
		 tbl_scope_cb,
		 parms);
}

/** Delete EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int
tf_em_delete_ext_entry(struct tf *tfp,
		       struct tf_delete_em_entry_parms *parms)
{
	struct tf_tbl_scope_cb *tbl_scope_cb;

	tbl_scope_cb = tf_em_ext_common_tbl_scope_find(tfp, parms->tbl_scope_id);
	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR, "Invalid tbl_scope_cb\n");
		return -EINVAL;
	}

	return tf_delete_eem_entry(tbl_scope_cb, parms);
}


int
tf_em_ext_common_bind(struct tf *tfp,
		      struct tf_em_cfg_parms *parms)
{
	int rc;
	int i;
	struct tf_rm_create_db_parms db_cfg = { 0 };
	struct em_ext_db *ext_db;
	struct tfp_calloc_parms cparms;

	TF_CHECK_PARMS2(tfp, parms);

	cparms.nitems = 1;
	cparms.size = sizeof(struct em_ext_db);
	cparms.alignment = 0;
	if (tfp_calloc(&cparms) != 0) {
		TFP_DRV_LOG(ERR, "em_ext_db alloc error %s\n",
			    strerror(ENOMEM));
		return -ENOMEM;
	}

	ext_db = cparms.mem_va;
	ll_init(&ext_db->tbl_scope_ll);
	for (i = 0; i < TF_DIR_MAX; i++)
		ext_db->eem_db[i] = NULL;
	tf_session_set_em_ext_db(tfp, ext_db);

	db_cfg.module = TF_MODULE_TYPE_EM;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.dir = i;
		db_cfg.alloc_cnt = parms->resources->em_cnt[i].cnt;

		/* Check if we got any request to support EEM, if so
		 * we build an EM Ext DB holding Table Scopes.
		 */
		if (db_cfg.alloc_cnt[TF_EM_TBL_TYPE_TBL_SCOPE] == 0)
			continue;

		db_cfg.rm_db = (void *)&ext_db->eem_db[i];
		rc = tf_rm_create_db(tfp, &db_cfg);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s: EM Ext DB creation failed\n",
				    tf_dir_2_str(i));

			return rc;
		}
	}

	mem_type = parms->mem_type;

	return 0;
}

int
tf_em_ext_common_unbind(struct tf *tfp)
{
	int rc;
	int i;
	struct tf_rm_free_db_parms fparms = { 0 };
	struct em_ext_db *ext_db = NULL;
	struct tf_session *tfs = NULL;
	struct tf_dev_info *dev;
	struct ll_entry *entry;
	struct tf_tbl_scope_cb *tbl_scope_cb = NULL;
	void *ext_ptr = NULL;
	struct tf_free_tbl_scope_parms tparms = { 0 };

	TF_CHECK_PARMS1(tfp);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR, "Failed to get tf_session, rc:%s\n",
		strerror(-rc));
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup device, rc:%s\n",
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

	if (ext_db != NULL) {
		entry = ext_db->tbl_scope_ll.head;
		while (entry != NULL) {
			tbl_scope_cb = (struct tf_tbl_scope_cb *)entry;
			entry = entry->next;
			tparms.tbl_scope_id =
				tbl_scope_cb->tbl_scope_id;

			if (dev->ops->tf_dev_free_tbl_scope) {
				dev->ops->tf_dev_free_tbl_scope(tfp,
								&tparms);
			} else {
				/* should not reach here */
				ll_delete(&ext_db->tbl_scope_ll,
					  &tbl_scope_cb->ll_entry);
				tfp_free(tbl_scope_cb);
			}
		}

		for (i = 0; i < TF_DIR_MAX; i++) {
			if (ext_db->eem_db[i] == NULL)
				continue;

			fparms.dir = i;
			fparms.rm_db = ext_db->eem_db[i];
			rc = tf_rm_free_db(tfp, &fparms);
			if (rc)
				return rc;

			ext_db->eem_db[i] = NULL;
		}

		tfp_free(ext_db);
	}

	tf_session_set_em_ext_db(tfp, NULL);

	return 0;
}

/**
 * Sets the specified external table type element.
 *
 * This API sets the specified element data
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to table set parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_ext_common_set(struct tf *tfp,
			  struct tf_tbl_set_parms *parms)
{
	int rc = 0;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	uint32_t tbl_scope_id;
	struct hcapi_cfa_hwop op;
	struct hcapi_cfa_key_tbl key_tbl;
	struct hcapi_cfa_key_data key_obj;
	struct hcapi_cfa_key_loc key_loc;

	TF_CHECK_PARMS2(tfp, parms);

	if (parms->data == NULL) {
		TFP_DRV_LOG(ERR,
			    "%s, invalid parms->data\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	tbl_scope_id = parms->tbl_scope_id;

	if (tbl_scope_id == TF_TBL_SCOPE_INVALID)  {
		TFP_DRV_LOG(ERR,
			    "%s, Table scope not allocated\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	tbl_scope_cb = tf_em_ext_common_tbl_scope_find(tfp, tbl_scope_id);
	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR,
			    "%s, table scope error\n",
			    tf_dir_2_str(parms->dir));
		return -EINVAL;
	}

	op.opcode = HCAPI_CFA_HWOPS_PUT;
	key_tbl.base0 =
		(uint8_t *)&tbl_scope_cb->em_ctx_info[parms->dir].em_tables[TF_RECORD_TABLE];
	key_tbl.page_size = TF_EM_PAGE_SIZE;
	key_obj.offset = parms->idx;
	key_obj.data = parms->data;
	key_obj.size = parms->data_sz_in_bytes;

	rc = cfa_p4_devops.hcapi_cfa_key_hw_op(&op,
					       &key_tbl,
					       &key_obj,
					       &key_loc);

	return rc;
}

int
tf_em_ext_common_alloc(struct tf *tfp,
		       struct tf_alloc_tbl_scope_parms *parms)
{
	return tf_em_ext_alloc(tfp, parms);
}

int
tf_em_ext_common_free(struct tf *tfp,
		      struct tf_free_tbl_scope_parms *parms)
{
	return tf_em_ext_free(tfp, parms);
}

int tf_em_ext_map_tbl_scope(struct tf *tfp,
			    struct tf_map_tbl_scope_parms *parms)
{
	int rc = 0;
	struct tf_session *tfs;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct tf_global_cfg_parms gcfg_parms = { 0 };
	struct tfp_calloc_parms aparms;
	uint32_t *data, *mask;
	uint32_t sz_in_bytes = 8;
	struct tf_dev_info *dev;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	tbl_scope_cb = tf_em_ext_common_tbl_scope_find(tfp, parms->tbl_scope_id);
	if (tbl_scope_cb == NULL) {
		TFP_DRV_LOG(ERR, "Invalid tbl_scope_cb tbl_scope_id(%d)\n",
			    parms->tbl_scope_id);
		return -EINVAL;
	}

	if (dev->ops->tf_dev_map_tbl_scope == NULL) {
		rc = -EOPNOTSUPP;
		TFP_DRV_LOG(ERR,
			    "Map table scope operation not supported, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	aparms.nitems = 2;
	aparms.size = sizeof(uint32_t);
	aparms.alignment = 0;

	if (tfp_calloc(&aparms) != 0) {
		TFP_DRV_LOG(ERR, "Map tbl scope alloc data error %s\n",
			    strerror(ENOMEM));
		return -ENOMEM;
	}
	data = aparms.mem_va;

	if (tfp_calloc(&aparms) != 0) {
		TFP_DRV_LOG(ERR, "Map tbl scope alloc mask error %s\n",
			    strerror(ENOMEM));
		rc = -ENOMEM;
		goto clean;
	}
	mask = aparms.mem_va;

	rc = dev->ops->tf_dev_map_parif(tfp, parms->parif_bitmask,
					tbl_scope_cb->pf,
					(uint8_t *)data, (uint8_t *)mask,
					sz_in_bytes);

	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Map table scope config failure, rc:%s\n",
			    strerror(-rc));
		goto cleaner;
	}

	/* Note that TF_GLOBAL_CFG_INTERNAL_PARIF_2_PF is same as below enum */
	gcfg_parms.type = TF_GLOBAL_CFG_TYPE_MAX;
	gcfg_parms.offset = 0;
	gcfg_parms.config = (uint8_t *)data;
	gcfg_parms.config_mask = (uint8_t *)mask;
	gcfg_parms.config_sz_in_bytes = sizeof(uint64_t);


	rc = tf_msg_set_global_cfg(tfp, &gcfg_parms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Map tbl scope, set failed, rc:%s\n",
			    strerror(-rc));
	}
cleaner:
	tfp_free(mask);
clean:
	tfp_free(data);

	return rc;
}
