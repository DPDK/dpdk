/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

/* Truflow Table APIs and supporting code */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/param.h>
#include <rte_common.h>
#include <rte_errno.h>
#include "hsi_struct_def_dpdk.h"

#include "tf_core.h"
#include "tf_em.h"
#include "tf_msg.h"
#include "tfp.h"
#include "hwrm_tf.h"
#include "bnxt.h"
#include "tf_resources.h"
#include "tf_rm.h"

#define PTU_PTE_VALID          0x1UL
#define PTU_PTE_LAST           0x2UL
#define PTU_PTE_NEXT_TO_LAST   0x4UL

/* Number of pointers per page_size */
#define	MAX_PAGE_PTRS(page_size)  ((page_size) / sizeof(void *))

#define TF_EM_PG_SZ_4K        (1 << 12)
#define TF_EM_PG_SZ_8K        (1 << 13)
#define TF_EM_PG_SZ_64K       (1 << 16)
#define TF_EM_PG_SZ_256K      (1 << 18)
#define TF_EM_PG_SZ_1M        (1 << 20)
#define TF_EM_PG_SZ_2M        (1 << 21)
#define TF_EM_PG_SZ_4M        (1 << 22)
#define TF_EM_PG_SZ_1G        (1 << 30)

#define	TF_EM_CTX_ID_INVALID   0xFFFF

#define	TF_EM_MIN_ENTRIES     (1 << 15) /* 32K */
#define	TF_EM_MAX_ENTRIES     (1 << 27) /* 128M */

/**
 * Function to free a page table
 *
 * [in] tp
 *   Pointer to the page table to free
 */
static void
tf_em_free_pg_tbl(struct tf_em_page_tbl *tp)
{
	uint32_t i;

	for (i = 0; i < tp->pg_count; i++) {
		if (!tp->pg_va_tbl[i]) {
			PMD_DRV_LOG(WARNING,
				    "No map for page %d table %016" PRIu64 "\n",
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
tf_em_free_page_table(struct tf_em_table *tbl)
{
	struct tf_em_page_tbl *tp;
	int i;

	for (i = 0; i < tbl->num_lvl; i++) {
		tp = &tbl->pg_tbl[i];

		PMD_DRV_LOG(INFO,
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
tf_em_alloc_pg_tbl(struct tf_em_page_tbl *tp,
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

		tp->pg_pa_tbl[i] = (uint64_t)(uintptr_t)parms.mem_pa;
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
tf_em_alloc_page_table(struct tf_em_table *tbl)
{
	struct tf_em_page_tbl *tp;
	int rc = 0;
	int i;
	uint32_t j;

	for (i = 0; i < tbl->num_lvl; i++) {
		tp = &tbl->pg_tbl[i];

		rc = tf_em_alloc_pg_tbl(tp,
					tbl->page_cnt[i],
					TF_EM_PAGE_SIZE);
		if (rc) {
			PMD_DRV_LOG(WARNING,
				"Failed to allocate page table: lvl: %d\n",
				i);
			goto cleanup;
		}

		for (j = 0; j < tp->pg_count; j++) {
			PMD_DRV_LOG(INFO,
				"EEM: Allocated page table: size %u lvl %d cnt"
				" %u VA:%p PA:%p\n",
				TF_EM_PAGE_SIZE,
				i,
				tp->pg_count,
				(uint32_t *)tp->pg_va_tbl[j],
				(uint32_t *)(uintptr_t)tp->pg_pa_tbl[j]);
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
tf_em_link_page_table(struct tf_em_page_tbl *tp,
		      struct tf_em_page_tbl *tp_next,
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
tf_em_setup_page_table(struct tf_em_table *tbl)
{
	struct tf_em_page_tbl *tp_next;
	struct tf_em_page_tbl *tp;
	bool set_pte_last = 0;
	int i;

	for (i = 0; i < tbl->num_lvl - 1; i++) {
		tp = &tbl->pg_tbl[i];
		tp_next = &tbl->pg_tbl[i + 1];
		if (i == tbl->num_lvl - 2)
			set_pte_last = 1;
		tf_em_link_page_table(tp, tp_next, set_pte_last);
	}

	tbl->l0_addr = tbl->pg_tbl[PT_LVL_0].pg_va_tbl[0];
	tbl->l0_dma_addr = tbl->pg_tbl[PT_LVL_0].pg_pa_tbl[0];
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
	int lvl = PT_LVL_0;
	uint64_t data_size;

	*num_data_pages = 0;
	data_size = (uint64_t)num_entries * entry_size;

	while (lvl_data_size < data_size) {
		lvl++;

		if (lvl == PT_LVL_1)
			lvl_data_size = (uint64_t)MAX_PAGE_PTRS(page_size) *
				page_size;
		else if (lvl == PT_LVL_2)
			lvl_data_size = (uint64_t)MAX_PAGE_PTRS(page_size) *
				MAX_PAGE_PTRS(page_size) * page_size;
		else
			return -ENOMEM;
	}

	*num_data_pages = roundup(data_size, page_size) / page_size;

	return lvl;
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
	if (max_lvl == PT_LVL_0) {
		page_cnt[PT_LVL_0] = num_data_pages;
	} else if (max_lvl == PT_LVL_1) {
		page_cnt[PT_LVL_1] = num_data_pages;
		page_cnt[PT_LVL_0] =
		tf_em_page_tbl_pgcnt(page_cnt[PT_LVL_1], page_size);
	} else if (max_lvl == PT_LVL_2) {
		page_cnt[PT_LVL_2] = num_data_pages;
		page_cnt[PT_LVL_1] =
		tf_em_page_tbl_pgcnt(page_cnt[PT_LVL_2], page_size);
		page_cnt[PT_LVL_0] =
		tf_em_page_tbl_pgcnt(page_cnt[PT_LVL_1], page_size);
	} else {
		return;
	}
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
static int
tf_em_size_table(struct tf_em_table *tbl)
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
	if (tbl->type == RECORD_TABLE) {
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

	max_lvl = tf_em_size_page_tbl_lvl(TF_EM_PAGE_SIZE,
					  tbl->entry_size,
					  tbl->num_entries,
					  &num_data_pages);
	if (max_lvl < 0) {
		PMD_DRV_LOG(WARNING, "EEM: Failed to size page table levels\n");
		PMD_DRV_LOG(WARNING,
			    "table: %d data-sz: %016" PRIu64 " page-sz: %u\n",
			    tbl->type,
			    (uint64_t)num_entries * tbl->entry_size,
			    TF_EM_PAGE_SIZE);
		return -ENOMEM;
	}

	tbl->num_lvl = max_lvl + 1;
	tbl->num_data_pages = num_data_pages;

	/* Determine the number of pages needed at each level */
	page_cnt = tbl->page_cnt;
	memset(page_cnt, 0, sizeof(tbl->page_cnt));
	tf_em_size_page_tbls(max_lvl, num_data_pages, TF_EM_PAGE_SIZE,
				page_cnt);

	PMD_DRV_LOG(INFO, "EEM: Sized page table: %d\n", tbl->type);
	PMD_DRV_LOG(INFO,
		    "EEM: lvls: %d sz: %016" PRIu64 " pgs: %016" PRIu64 " l0: %u l1: %u l2: %u\n",
		    max_lvl + 1,
		    (uint64_t)num_data_pages * TF_EM_PAGE_SIZE,
		    num_data_pages,
		    page_cnt[PT_LVL_0],
		    page_cnt[PT_LVL_1],
		    page_cnt[PT_LVL_2]);

	return 0;
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
	struct tf_em_ctx_mem_info *ctxp = &tbl_scope_cb->em_ctx_info[dir];
	struct tf_em_table *tbl;
	int i;

	for (i = KEY0_TABLE; i < MAX_TABLE; i++) {
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
	struct tf_em_ctx_mem_info *ctxp = &tbl_scope_cb->em_ctx_info[dir];
	struct tf_em_table *tbl;
	int rc;
	int i;

	for (i = KEY0_TABLE; i < MAX_TABLE; i++) {
		tbl = &ctxp->em_tables[i];

		if (tbl->num_entries && tbl->entry_size) {
			rc = tf_em_size_table(tbl);

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
static int
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
			PMD_DRV_LOG(ERR, "EEM: Insufficient memory requested:"
				    "%uMB\n",
				    parms->rx_mem_size_in_mb);
			return -EINVAL;
		}

		cnt = TF_EM_MIN_ENTRIES;
		while (num_entries > cnt &&
		       cnt <= TF_EM_MAX_ENTRIES)
			cnt *= 2;

		if (cnt > TF_EM_MAX_ENTRIES) {
			PMD_DRV_LOG(ERR, "EEM: Invalid number of Tx requested: "
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
			PMD_DRV_LOG(ERR,
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
			PMD_DRV_LOG(ERR,
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
			PMD_DRV_LOG(ERR,
				    "EEM: Insufficient memory requested:%uMB\n",
				    parms->rx_mem_size_in_mb);
			return -EINVAL;
		}

		cnt = TF_EM_MIN_ENTRIES;
		while (num_entries > cnt &&
		       cnt <= TF_EM_MAX_ENTRIES)
			cnt *= 2;

		if (cnt > TF_EM_MAX_ENTRIES) {
			PMD_DRV_LOG(ERR,
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
			PMD_DRV_LOG(ERR,
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
			PMD_DRV_LOG(ERR,
				    "EEM: Invalid number of Tx requested: %u\n",
		       (parms->tx_num_flows_in_k * TF_KILOBYTE));
			return -EINVAL;
		}
	}

	/* Rx */
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[KEY0_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[KEY0_TABLE].entry_size =
		parms->rx_max_key_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[KEY1_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[KEY1_TABLE].entry_size =
		parms->rx_max_key_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[RECORD_TABLE].num_entries =
		parms->rx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[RECORD_TABLE].entry_size =
		parms->tx_max_action_entry_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[EFC_TABLE].num_entries =
		0;

	/* Tx */
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[KEY0_TABLE].num_entries =
		parms->tx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[KEY0_TABLE].entry_size =
		parms->tx_max_key_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[KEY1_TABLE].num_entries =
		parms->tx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[KEY1_TABLE].entry_size =
		parms->tx_max_key_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[RECORD_TABLE].num_entries =
		parms->tx_num_flows_in_k * TF_KILOBYTE;
	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[RECORD_TABLE].entry_size =
		parms->tx_max_action_entry_sz_in_bits / 8;

	tbl_scope_cb->em_ctx_info[TF_DIR_TX].em_tables[EFC_TABLE].num_entries =
		0;

	return 0;
}

/**
 * Internal function to set a Table Entry. Supports all internal Table Types
 *
 * [in] tfp
 *   Pointer to TruFlow handle
 *
 * [in] parms
 *   Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
static int
tf_set_tbl_entry_internal(struct tf *tfp,
			  struct tf_set_tbl_entry_parms *parms)
{
	int rc;
	int id;
	uint32_t index;
	struct bitalloc *session_pool;
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	/* Lookup the pool using the table type of the element */
	rc = tf_rm_lookup_tbl_type_pool(tfs,
					parms->dir,
					parms->type,
					&session_pool);
	/* Error logging handled by tf_rm_lookup_tbl_type_pool */
	if (rc)
		return rc;

	index = parms->idx;

	if (parms->type != TF_TBL_TYPE_FULL_ACT_RECORD &&
	    parms->type != TF_TBL_TYPE_ACT_SP_SMAC_IPV4) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Type not supported, type:%d\n",
			    parms->dir,
			    parms->type);
		return -EOPNOTSUPP;
	}

	/* Adjust the returned index/offset as there is no guarantee
	 * that the start is 0 at time of RM allocation
	 */
	tf_rm_convert_index(tfs,
			    parms->dir,
			    parms->type,
			    TF_RM_CONVERT_RM_BASE,
			    parms->idx,
			    &index);

	/* Verify that the entry has been previously allocated */
	id = ba_inuse(session_pool, index);
	if (id != 1) {
		PMD_DRV_LOG(ERR,
		   "dir:%d, Invalid or not allocated index, type:%d, idx:%d\n",
		   parms->dir,
		   parms->type,
		   index);
		return -EINVAL;
	}

	/* Set the entry */
	rc = tf_msg_set_tbl_entry(tfp,
				  parms->dir,
				  parms->type,
				  parms->data_sz_in_bytes,
				  parms->data,
				  parms->idx);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Set failed, type:%d, rc:%d\n",
			    parms->dir,
			    parms->type,
			    rc);
	}

	return rc;
}

/**
 * Internal function to get a Table Entry. Supports all Table Types
 * except the TF_TBL_TYPE_EXT as that is handled as a table scope.
 *
 * [in] tfp
 *   Pointer to TruFlow handle
 *
 * [in] parms
 *   Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
static int
tf_get_tbl_entry_internal(struct tf *tfp,
			  struct tf_get_tbl_entry_parms *parms)
{
	int rc;
	int id;
	uint32_t index;
	struct bitalloc *session_pool;
	struct tf_session *tfs = (struct tf_session *)(tfp->session->core_data);

	/* Lookup the pool using the table type of the element */
	rc = tf_rm_lookup_tbl_type_pool(tfs,
					parms->dir,
					parms->type,
					&session_pool);
	/* Error logging handled by tf_rm_lookup_tbl_type_pool */
	if (rc)
		return rc;

	index = parms->idx;

	/* Adjust the returned index/offset as there is no guarantee
	 * that the start is 0 at time of RM allocation
	 */
	tf_rm_convert_index(tfs,
			    parms->dir,
			    parms->type,
			    TF_RM_CONVERT_RM_BASE,
			    parms->idx,
			    &index);

	/* Verify that the entry has been previously allocated */
	id = ba_inuse(session_pool, index);
	if (id != 1) {
		PMD_DRV_LOG(ERR,
		   "dir:%d, Invalid or not allocated index, type:%d, idx:%d\n",
		   parms->dir,
		   parms->type,
		   index);
		return -EINVAL;
	}

	/* Get the entry */
	rc = tf_msg_get_tbl_entry(tfp,
				  parms->dir,
				  parms->type,
				  parms->data_sz_in_bytes,
				  parms->data,
				  parms->idx);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Get failed, type:%d, rc:%d\n",
			    parms->dir,
			    parms->type,
			    rc);
	}

	return rc;
}

#if (TF_SHADOW == 1)
/**
 * Allocate Tbl entry from the Shadow DB. Shadow DB is searched for
 * the requested entry. If found the ref count is incremente and
 * returned.
 *
 * [in] tfs
 *   Pointer to session
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry found and ref count incremented
 *  -ENOENT - Failure, entry not found
 */
static int
tf_alloc_tbl_entry_shadow(struct tf_session *tfs __rte_unused,
			  struct tf_alloc_tbl_entry_parms *parms __rte_unused)
{
	PMD_DRV_LOG(ERR,
		    "dir:%d, Entry Alloc with search not supported\n",
		    parms->dir);


	return -EOPNOTSUPP;
}

/**
 * Free Tbl entry from the Shadow DB. Shadow DB is searched for
 * the requested entry. If found the ref count is decremente and
 * new ref_count returned.
 *
 * [in] tfs
 *   Pointer to session
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry found and ref count decremented
 *  -ENOENT - Failure, entry not found
 */
static int
tf_free_tbl_entry_shadow(struct tf_session *tfs,
			 struct tf_free_tbl_entry_parms *parms)
{
	PMD_DRV_LOG(ERR,
		    "dir:%d, Entry Free with search not supported\n",
		    parms->dir);

	return -EOPNOTSUPP;
}
#endif /* TF_SHADOW */

/**
 * Create External Tbl pool of memory indexes.
 *
 * [in] session
 *   Pointer to session
 * [in] dir
 *   direction
 * [in] tbl_scope_cb
 *   pointer to the table scope
 * [in] tbl_scope_id
 *   id of the table scope
 * [in] num_entries
 *   number of entries to write
 * [in] entry_sz_bytes
 *   size of each entry
 *
 * Return:
 *  0       - Success, entry allocated - no search support
 *  -ENOMEM -EINVAL -EOPNOTSUPP
 *          - Failure, entry not allocated, out of resources
 */
static int
tf_create_tbl_pool_external(struct tf_session *session,
			    enum tf_dir dir,
			    struct tf_tbl_scope_cb *tbl_scope_cb,
			    uint32_t table_scope_id,
			    uint32_t num_entries,
			    uint32_t entry_sz_bytes)

{
	struct tfp_calloc_parms parms;
	uint32_t i, j;
	int rc = 0;
	struct stack *pool = &tbl_scope_cb->ext_pool[dir][TF_EXT_POOL_0];

	parms.nitems = num_entries;
	parms.size = sizeof(uint32_t);
	parms.alignment = 0;

	if (tfp_calloc(&parms) != 0) {
		PMD_DRV_LOG(ERR, "%d: TBL: external pool failure %s\n",
			    dir, strerror(-ENOMEM));
		return -ENOMEM;
	}

	/* Create empty stack
	 */
	rc = stack_init(num_entries, parms.mem_va, pool);

	if (rc != 0) {
		PMD_DRV_LOG(ERR, "%d: TBL: stack init failure %s\n",
			    dir, strerror(-rc));
		goto cleanup;
	}

	/* Save the  malloced memory address so that it can
	 * be freed when the table scope is freed.
	 */
	tbl_scope_cb->ext_pool_mem[dir][TF_EXT_POOL_0] =
		(uint32_t *)parms.mem_va;

	/* Fill pool with indexes
	 */
	j = num_entries * entry_sz_bytes - 1;

	for (i = 0; i < num_entries; i++) {
		rc = stack_push(pool, j);
		if (rc != 0) {
			PMD_DRV_LOG(ERR, "%d TBL: stack failure %s\n",
				    dir, strerror(-rc));
			goto cleanup;
		}
		j -= entry_sz_bytes;
	}

	if (!stack_is_full(pool)) {
		rc = -EINVAL;
		PMD_DRV_LOG(ERR, "%d TBL: stack failure %s\n",
			    dir, strerror(-rc));
		goto cleanup;
	}
	/* Set the table scope associated with the pool
	 */
	session->ext_pool_2_scope[dir][TF_EXT_POOL_0] = table_scope_id;

	return 0;
cleanup:
	tfp_free((void *)parms.mem_va);
	return rc;
}

/**
 * Destroy External Tbl pool of memory indexes.
 *
 * [in] session
 *   Pointer to session
 * [in] dir
 *   direction
 * [in] tbl_scope_cb
 *   pointer to the table scope
 *
 */
static void
tf_destroy_tbl_pool_external(struct tf_session *session,
			    enum tf_dir dir,
			    struct tf_tbl_scope_cb *tbl_scope_cb)
{
	uint32_t *ext_pool_mem =
		tbl_scope_cb->ext_pool_mem[dir][TF_EXT_POOL_0];

	tfp_free(ext_pool_mem);

	/* Set the table scope associated with the pool
	 */
	session->ext_pool_2_scope[dir][TF_EXT_POOL_0] = TF_TBL_SCOPE_INVALID;
}

/**
 * Allocate External Tbl entry from the Session Pool.
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
static int
tf_alloc_tbl_entry_pool_external(struct tf *tfp,
				 struct tf_alloc_tbl_entry_parms *parms)
{
	int rc;
	uint32_t index;
	struct tf_session *tfs;
	uint32_t tbl_scope_id;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct stack *pool;

	/* Check parameters */
	if (tfp == NULL || parms == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameters\n");
		return -EINVAL;
	}

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Session info invalid\n",
			    parms->dir);
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	if (parms->type != TF_TBL_TYPE_EXT) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Type not supported, type:%d\n",
			    parms->dir,
			    parms->type);
		return -EOPNOTSUPP;
	}

	/* Get the pool info from the table scope
	 */
	tbl_scope_id = tfs->ext_pool_2_scope[parms->dir][TF_EXT_POOL_0];
	tbl_scope_cb = tbl_scope_cb_find(tfs, tbl_scope_id);

	if (tbl_scope_cb == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, table scope not allocated\n",
			    parms->dir);
		return -EINVAL;
	}
	pool = &tbl_scope_cb->ext_pool[parms->dir][TF_EXT_POOL_0];

	/* Allocate an element
	 */
	rc = stack_pop(pool, &index);

	if (rc != 0) {
		PMD_DRV_LOG(ERR,
		   "dir:%d, Allocation failed, type:%d\n",
		   parms->dir,
		   parms->type);
		return rc;
	}
	parms->idx = index;
	return rc;
}

/**
 * Allocate Internal Tbl entry from the Session Pool.
 *
 * [in] tfp
 *   Pointer to Truflow Handle
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry found and ref count decremented
 *  -ENOMEM - Failure, entry not allocated, out of resources
 */
static int
tf_alloc_tbl_entry_pool_internal(struct tf *tfp,
				 struct tf_alloc_tbl_entry_parms *parms)
{
	int rc;
	int id;
	int free_cnt;
	uint32_t index;
	struct bitalloc *session_pool;
	struct tf_session *tfs;

	/* Check parameters */
	if (tfp == NULL || parms == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameters\n");
		return -EINVAL;
	}

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Session info invalid\n",
			    parms->dir);
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	if (parms->type != TF_TBL_TYPE_FULL_ACT_RECORD &&
	    parms->type != TF_TBL_TYPE_ACT_SP_SMAC &&
	    parms->type != TF_TBL_TYPE_ACT_SP_SMAC_IPV4 &&
	    parms->type != TF_TBL_TYPE_ACT_ENCAP_8B &&
	    parms->type != TF_TBL_TYPE_ACT_ENCAP_16B &&
	    parms->type != TF_TBL_TYPE_ACT_ENCAP_64B) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Type not supported, type:%d\n",
			    parms->dir,
			    parms->type);
		return -EOPNOTSUPP;
	}

	/* Lookup the pool using the table type of the element */
	rc = tf_rm_lookup_tbl_type_pool(tfs,
					parms->dir,
					parms->type,
					&session_pool);
	/* Error logging handled by tf_rm_lookup_tbl_type_pool */
	if (rc)
		return rc;

	id = ba_alloc(session_pool);
	if (id == -1) {
		free_cnt = ba_free_count(session_pool);

		PMD_DRV_LOG(ERR,
		   "dir:%d, Allocation failed, type:%d, free:%d\n",
		   parms->dir,
		   parms->type,
		   free_cnt);
		return -ENOMEM;
	}

	/* Adjust the returned index/offset as there is no guarantee
	 * that the start is 0 at time of RM allocation
	 */
	tf_rm_convert_index(tfs,
			    parms->dir,
			    parms->type,
			    TF_RM_CONVERT_ADD_BASE,
			    id,
			    &index);
	parms->idx = index;
	return rc;
}

/**
 * Free External Tbl entry to the session pool.
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
static int
tf_free_tbl_entry_pool_external(struct tf *tfp,
		       struct tf_free_tbl_entry_parms *parms)
{
	int rc = 0;
	struct tf_session *tfs;
	uint32_t index;
	uint32_t tbl_scope_id;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct stack *pool;

	/* Check parameters */
	if (tfp == NULL || parms == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameters\n");
		return -EINVAL;
	}

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Session info invalid\n",
			    parms->dir);
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	if (parms->type != TF_TBL_TYPE_EXT) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Type not supported, type:%d\n",
			    parms->dir,
			    parms->type);
		return -EOPNOTSUPP;
	}

	/* Get the pool info from the table scope
	 */
	tbl_scope_id = tfs->ext_pool_2_scope[parms->dir][TF_EXT_POOL_0];
	tbl_scope_cb = tbl_scope_cb_find(tfs, tbl_scope_id);

	if (tbl_scope_cb == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, table scope error\n",
			    parms->dir);
		return -EINVAL;
	}
	pool = &tbl_scope_cb->ext_pool[parms->dir][TF_EXT_POOL_0];

	index = parms->idx;

	rc = stack_push(pool, index);

	if (rc != 0) {
		PMD_DRV_LOG(ERR,
		   "dir:%d, consistency error, stack full, type:%d, idx:%d\n",
		   parms->dir,
		   parms->type,
		   index);
	}
	return rc;
}

/**
 * Free Internal Tbl entry from the Session Pool.
 *
 * [in] tfp
 *   Pointer to Truflow Handle
 * [in] parms
 *   Allocation parameters
 *
 * Return:
 *  0       - Success, entry found and ref count decremented
 *  -ENOMEM - Failure, entry not allocated, out of resources
 */
static int
tf_free_tbl_entry_pool_internal(struct tf *tfp,
		       struct tf_free_tbl_entry_parms *parms)
{
	int rc = 0;
	int id;
	struct bitalloc *session_pool;
	struct tf_session *tfs;
	uint32_t index;

	/* Check parameters */
	if (tfp == NULL || parms == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameters\n");
		return -EINVAL;
	}

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Session info invalid\n",
			    parms->dir);
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	if (parms->type != TF_TBL_TYPE_FULL_ACT_RECORD &&
	    parms->type != TF_TBL_TYPE_ACT_SP_SMAC &&
	    parms->type != TF_TBL_TYPE_ACT_SP_SMAC_IPV4 &&
	    parms->type != TF_TBL_TYPE_ACT_ENCAP_8B &&
	    parms->type != TF_TBL_TYPE_ACT_ENCAP_16B &&
	    parms->type != TF_TBL_TYPE_ACT_ENCAP_64B) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Type not supported, type:%d\n",
			    parms->dir,
			    parms->type);
		return -EOPNOTSUPP;
	}

	/* Lookup the pool using the table type of the element */
	rc = tf_rm_lookup_tbl_type_pool(tfs,
					parms->dir,
					parms->type,
					&session_pool);
	/* Error logging handled by tf_rm_lookup_tbl_type_pool */
	if (rc)
		return rc;

	index = parms->idx;

	/* Adjust the returned index/offset as there is no guarantee
	 * that the start is 0 at time of RM allocation
	 */
	tf_rm_convert_index(tfs,
			    parms->dir,
			    parms->type,
			    TF_RM_CONVERT_RM_BASE,
			    parms->idx,
			    &index);

	/* Check if element was indeed allocated */
	id = ba_inuse_free(session_pool, index);
	if (id == -1) {
		PMD_DRV_LOG(ERR,
		   "dir:%d, Element not previously alloc'ed, type:%d, idx:%d\n",
		   parms->dir,
		   parms->type,
		   index);
		return -ENOMEM;
	}

	return rc;
}

/* API defined in tf_tbl.h */
void
tf_init_tbl_pool(struct tf_session *session)
{
	enum tf_dir dir;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		session->ext_pool_2_scope[dir][TF_EXT_POOL_0] =
			TF_TBL_SCOPE_INVALID;
	}
}

/* API defined in tf_em.h */
struct tf_tbl_scope_cb *
tbl_scope_cb_find(struct tf_session *session,
		  uint32_t tbl_scope_id)
{
	int i;

	/* Check that id is valid */
	i = ba_inuse(session->tbl_scope_pool_rx, tbl_scope_id);
	if (i < 0)
		return NULL;

	for (i = 0; i < TF_NUM_TBL_SCOPE; i++) {
		if (session->tbl_scopes[i].tbl_scope_id == tbl_scope_id)
			return &session->tbl_scopes[i];
	}

	return NULL;
}

/* API defined in tf_core.h */
int
tf_free_eem_tbl_scope_cb(struct tf *tfp,
			 struct tf_free_tbl_scope_parms *parms)
{
	int rc = 0;
	enum tf_dir  dir;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct tf_session *session;

	session = (struct tf_session *)(tfp->session->core_data);

	tbl_scope_cb = tbl_scope_cb_find(session,
					 parms->tbl_scope_id);

	if (tbl_scope_cb == NULL)
		return -EINVAL;

	/* Free Table control block */
	ba_free(session->tbl_scope_pool_rx, tbl_scope_cb->index);

	/* free table scope locks */
	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		/* Free associated external pools
		 */
		tf_destroy_tbl_pool_external(session,
					     dir,
					     tbl_scope_cb);
		tf_msg_em_op(tfp,
			     dir,
			     HWRM_TF_EXT_EM_OP_INPUT_OP_EXT_EM_DISABLE);

		/* free table scope and all associated resources */
		tf_em_ctx_unreg(tfp, tbl_scope_cb, dir);
	}

	return rc;
}

/* API defined in tf_em.h */
int
tf_alloc_eem_tbl_scope(struct tf *tfp,
		       struct tf_alloc_tbl_scope_parms *parms)
{
	int rc;
	enum tf_dir dir;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct tf_em_table *em_tables;
	int index;
	struct tf_session *session;
	struct tf_free_tbl_scope_parms free_parms;

	/* check parameters */
	if (parms == NULL || tfp->session == NULL) {
		PMD_DRV_LOG(ERR, "TBL: Invalid parameters\n");
		return -EINVAL;
	}

	session = (struct tf_session *)tfp->session->core_data;

	/* Get Table Scope control block from the session pool */
	index = ba_alloc(session->tbl_scope_pool_rx);
	if (index == -1) {
		PMD_DRV_LOG(ERR, "EEM: Unable to allocate table scope "
			    "Control Block\n");
		return -ENOMEM;
	}

	tbl_scope_cb = &session->tbl_scopes[index];
	tbl_scope_cb->index = index;
	tbl_scope_cb->tbl_scope_id = index;
	parms->tbl_scope_id = index;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		rc = tf_msg_em_qcaps(tfp,
				     dir,
				     &tbl_scope_cb->em_caps[dir]);
		if (rc) {
			PMD_DRV_LOG(ERR,
				"EEM: Unable to query for EEM capability\n");
			goto cleanup;
		}
	}

	/*
	 * Validate and setup table sizes
	 */
	if (tf_em_validate_num_entries(tbl_scope_cb, parms))
		goto cleanup;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		/*
		 * Allocate tables and signal configuration to FW
		 */
		rc = tf_em_ctx_reg(tfp, tbl_scope_cb, dir);
		if (rc) {
			PMD_DRV_LOG(ERR,
				    "EEM: Unable to register for EEM ctx\n");
			goto cleanup;
		}

		em_tables = tbl_scope_cb->em_ctx_info[dir].em_tables;
		rc = tf_msg_em_cfg(tfp,
				   em_tables[KEY0_TABLE].num_entries,
				   em_tables[KEY0_TABLE].ctx_id,
				   em_tables[KEY1_TABLE].ctx_id,
				   em_tables[RECORD_TABLE].ctx_id,
				   em_tables[EFC_TABLE].ctx_id,
				   dir);
		if (rc) {
			PMD_DRV_LOG(ERR,
				"TBL: Unable to configure EEM in firmware\n");
			goto cleanup_full;
		}

		rc = tf_msg_em_op(tfp,
				  dir,
				  HWRM_TF_EXT_EM_OP_INPUT_OP_EXT_EM_ENABLE);

		if (rc) {
			PMD_DRV_LOG(ERR,
				    "EEM: Unable to enable EEM in firmware\n");
			goto cleanup_full;
		}

		/* Allocate the pool of offsets of the external memory.
		 * Initially, this is a single fixed size pool for all external
		 * actions related to a single table scope.
		 */
		rc = tf_create_tbl_pool_external(session,
						 dir,
						 tbl_scope_cb,
						 index,
						 TF_EXT_POOL_ENTRY_CNT,
						 TF_EXT_POOL_ENTRY_SZ_BYTES);
		if (rc) {
			PMD_DRV_LOG(ERR,
				    "%d TBL: Unable to allocate idx pools %s\n",
				    dir,
				    strerror(-rc));
			goto cleanup_full;
		}
	}

	return 0;

cleanup_full:
	free_parms.tbl_scope_id = index;
	tf_free_eem_tbl_scope_cb(tfp, &free_parms);
	return -EINVAL;

cleanup:
	/* Free Table control block */
	ba_free(session->tbl_scope_pool_rx, tbl_scope_cb->index);
	return -EINVAL;
}

/* API defined in tf_core.h */
int
tf_set_tbl_entry(struct tf *tfp,
		 struct tf_set_tbl_entry_parms *parms)
{
	int rc = 0;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct tf_session *session;

	if (tfp == NULL || parms == NULL || parms->data == NULL)
		return -EINVAL;

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Session info invalid\n",
			    parms->dir);
		return -EINVAL;
	}

	if (parms->type == TF_TBL_TYPE_EXT) {
		void *base_addr;
		uint32_t offset = TF_ACT_REC_INDEX_2_OFFSET(parms->idx);
		uint32_t tbl_scope_id;

		session = (struct tf_session *)(tfp->session->core_data);

		tbl_scope_id =
			session->ext_pool_2_scope[parms->dir][TF_EXT_POOL_0];

		if (tbl_scope_id == TF_TBL_SCOPE_INVALID)  {
			PMD_DRV_LOG(ERR,
				    "dir:%d, Table scope not allocated\n",
				    parms->dir);
			return -EINVAL;
		}

		/* Get the table scope control block associated with the
		 * external pool
		 */

		tbl_scope_cb = tbl_scope_cb_find(session, tbl_scope_id);

		if (tbl_scope_cb == NULL)
			return -EINVAL;

		/* External table, implicitly the Action table */
		base_addr = tf_em_get_table_page(tbl_scope_cb,
						 parms->dir,
						 offset,
						 RECORD_TABLE);
		if (base_addr == NULL) {
			PMD_DRV_LOG(ERR,
				    "dir:%d, Base address lookup failed\n",
				    parms->dir);
			return -EINVAL;
		}

		offset %= TF_EM_PAGE_SIZE;
		rte_memcpy((char *)base_addr + offset,
			   parms->data,
			   parms->data_sz_in_bytes);
	} else {
		/* Internal table type processing */
		rc = tf_set_tbl_entry_internal(tfp, parms);
		if (rc) {
			PMD_DRV_LOG(ERR,
				    "dir:%d, Set failed, type:%d, rc:%d\n",
				    parms->dir,
				    parms->type,
				    rc);
		}
	}

	return rc;
}

/* API defined in tf_core.h */
int
tf_get_tbl_entry(struct tf *tfp,
		 struct tf_get_tbl_entry_parms *parms)
{
	int rc = 0;

	if (tfp == NULL || parms == NULL)
		return -EINVAL;

	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Session info invalid\n",
			    parms->dir);
		return -EINVAL;
	}

	if (parms->type == TF_TBL_TYPE_EXT) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, External table type not supported\n",
			    parms->dir);

		rc = -EOPNOTSUPP;
	} else {
		/* Internal table type processing */
		rc = tf_get_tbl_entry_internal(tfp, parms);
		if (rc)
			PMD_DRV_LOG(ERR,
				    "dir:%d, Get failed, type:%d, rc:%d\n",
				    parms->dir,
				    parms->type,
				    rc);
	}

	return rc;
}

/* API defined in tf_core.h */
int
tf_alloc_tbl_scope(struct tf *tfp,
		   struct tf_alloc_tbl_scope_parms *parms)
{
	int rc;

	/* check parameters */
	if (parms == NULL || tfp == NULL) {
		PMD_DRV_LOG(ERR, "TBL: Invalid parameters\n");
		return -EINVAL;
	}

	rc = tf_alloc_eem_tbl_scope(tfp, parms);

	return rc;
}

/* API defined in tf_core.h */
int
tf_free_tbl_scope(struct tf *tfp,
		  struct tf_free_tbl_scope_parms *parms)
{
	int rc;

	/* check parameters */
	if (parms == NULL || tfp == NULL) {
		PMD_DRV_LOG(ERR, "TBL: Invalid parameters\n");
		return -EINVAL;
	}

	/* free table scope and all associated resources */
	rc = tf_free_eem_tbl_scope_cb(tfp, parms);

	return rc;
}

/* API defined in tf_core.h */
int
tf_alloc_tbl_entry(struct tf *tfp,
		   struct tf_alloc_tbl_entry_parms *parms)
{
	int rc;
#if (TF_SHADOW == 1)
	struct tf_session *tfs;
#endif /* TF_SHADOW */

	/* Check parameters */
	if (parms == NULL || tfp == NULL) {
		PMD_DRV_LOG(ERR, "TBL: Invalid parameters\n");
		return -EINVAL;
	}
	/*
	 * No shadow copy support for external tables, allocate and return
	 */
	if (parms->type == TF_TBL_TYPE_EXT) {
		rc = tf_alloc_tbl_entry_pool_external(tfp, parms);
		return rc;
	}

#if (TF_SHADOW == 1)
	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Session info invalid\n",
			    parms->dir);
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	/* Search the Shadow DB for requested element. If not found go
	 * allocate one from the Session Pool
	 */
	if (parms->search_enable && tfs->shadow_copy) {
		rc = tf_alloc_tbl_entry_shadow(tfs, parms);
		/* Entry found and parms populated with return data */
		if (rc == 0)
			return rc;
	}
#endif /* TF_SHADOW */

	rc = tf_alloc_tbl_entry_pool_internal(tfp, parms);
	if (rc)
		PMD_DRV_LOG(ERR, "dir%d, Alloc failed, rc:%d\n",
			    parms->dir,
			    rc);

	return rc;
}

/* API defined in tf_core.h */
int
tf_free_tbl_entry(struct tf *tfp,
		  struct tf_free_tbl_entry_parms *parms)
{
	int rc;
#if (TF_SHADOW == 1)
	struct tf_session *tfs;
#endif /* TF_SHADOW */

	/* Check parameters */
	if (parms == NULL || tfp == NULL) {
		PMD_DRV_LOG(ERR, "TBL: Invalid parameters\n");
		return -EINVAL;
	}
	/*
	 * No shadow of external tables so just free the entry
	 */
	if (parms->type == TF_TBL_TYPE_EXT) {
		rc = tf_free_tbl_entry_pool_external(tfp, parms);
		return rc;
	}

#if (TF_SHADOW == 1)
	if (tfp->session == NULL || tfp->session->core_data == NULL) {
		PMD_DRV_LOG(ERR,
			    "dir:%d, Session info invalid\n",
			    parms->dir);
		return -EINVAL;
	}

	tfs = (struct tf_session *)(tfp->session->core_data);

	/* Search the Shadow DB for requested element. If not found go
	 * allocate one from the Session Pool
	 */
	if (parms->search_enable && tfs->shadow_copy) {
		rc = tf_free_tbl_entry_shadow(tfs, parms);
		/* Entry free'ed and parms populated with return data */
		if (rc == 0)
			return rc;
	}
#endif /* TF_SHADOW */

	rc = tf_free_tbl_entry_pool_internal(tfp, parms);

	if (rc)
		PMD_DRV_LOG(ERR, "dir:%d, Alloc failed, rc:%d\n",
			    parms->dir,
			    rc);
	return rc;
}
