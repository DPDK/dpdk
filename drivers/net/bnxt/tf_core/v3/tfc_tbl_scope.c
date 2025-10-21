/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */

#include <stdio.h>
#include <math.h> /* May need to link with -lm */
#include <bnxt.h>
#include <bnxt_mpc.h>

#include "cfa_bld_mpc_field_ids.h"
#include "cfa_bld_mpcops.h"
#include "rte_malloc.h"
#include "rte_memzone.h"
#include "tfc.h"
#include "tfc_em.h"
#include "tfc_priv.h"
#include "tfc_msg.h"
#include "cfa_types.h"
#include "cfa_tim.h"
#include "cfa_tpm.h"
#include "tfo.h"
#include "tfc_cpm.h"
#include "cfa_mm.h"
#include "tfc_vf2pf_msg.h"
#include "tfc_util.h"

/*
 * These values are for Thor2.	Take care to adjust them appropriately when
 * support for additional HW is added.
 */
/* How many entries a single bucket can point to */
#define ENTRIES_PER_BUCKET 6
/* sizes in bytes */
#define LREC_SIZE 16
#define RECORD_SIZE 32

/*
 * Page alignments must be some power of 2.  These bits define the powers of 2
 * that are valid for page alignments.  It is taken from
 * cfa_hw_ts_pbl_page_size.
 */
#define VALID_PAGE_ALIGNMENTS 0x40753000

#define MAX_PAGE_PTRS(page_size) ((page_size) / sizeof(void *))

/* Private functions */

/*
 * This function calculates how many buckets and records are required for a
 * given flow_cnt and factor.
 *
 * @param[in] flow_cnt
 *   The total number of flows for which to compute memory
 *
 * @param[in] key_sz_in_bytes
 *   The lookup key size in bytes
 *
 * @param[in] shared
 *   True if the table scope will be shared.  Shared table scopes cannot have
 *   dynamic buckets.
 *
 * @param[in] factor
 *   This indicates a multiplier factor for determining the static and dynamic
 *   bucket counts.  The larger the factor, the more buckets will be allocated.
 *
 * @param[out] lkup_rec_cnt
 *   The total number of lookup records to allocate (includes buckets)
 *
 * @param[out] static_bucket_cnt_exp
 *   The log2 of the number of static buckets to allocate. For example if 1024
 *   static buckets, 1024=2^10, so the value 10 would be returned.
 *
 * @param[out] dynamic_bucket_cnt
 *   The number of dynamic buckets to allocate
 *
 */
static int calc_lkup_rec_cnt(uint32_t flow_cnt, uint16_t key_sz_in_bytes,
			     __rte_unused bool shared,
			     enum tfc_tbl_scope_bucket_factor factor,
			     uint32_t *lkup_rec_cnt,
			     uint8_t *static_bucket_cnt_exp,
			     uint32_t *dynamic_bucket_cnt)
{
	unsigned int entry_size;
	unsigned int flow_adj;	  /* flow_cnt adjusted for factor */
	unsigned int key_rec_cnt;

	flow_cnt = 1 << next_pow2(flow_cnt);

	switch (factor) {
	case TFC_TBL_SCOPE_BUCKET_FACTOR_1:
		flow_adj = flow_cnt;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_2:
		flow_adj = flow_cnt * 2;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_4:
		flow_adj = flow_cnt * 4;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_8:
		flow_adj = flow_cnt * 8;
		break;
	case TFC_TBL_SCOPE_BUCKET_FACTOR_16:
		flow_adj = flow_cnt * 16;
		break;
	default:
		PMD_DRV_LOG_LINE(ERR, "Invalid factor (%u)", factor);
		return -EINVAL;
	}

	if (key_sz_in_bytes <= RECORD_SIZE - LREC_SIZE) {
		entry_size = 1;
	} else if (key_sz_in_bytes <= RECORD_SIZE * 2 - LREC_SIZE) {
		entry_size = 2;
	} else if (key_sz_in_bytes <= RECORD_SIZE * 3 - LREC_SIZE) {
		entry_size = 3;
	} else if (key_sz_in_bytes <= RECORD_SIZE * 4 - LREC_SIZE) {
		entry_size = 4;
	} else {
		PMD_DRV_LOG_LINE(ERR,
				 "Key size (%u) cannot be larger than (%u)",
				 key_sz_in_bytes,
				 RECORD_SIZE * 4 - LREC_SIZE);
		return -EINVAL;
	}
	key_rec_cnt = flow_cnt * entry_size;

#ifdef DYNAMIC_BUCKETS_SUPPORTED
	if (shared) {
#endif
		*static_bucket_cnt_exp =
			next_pow2(flow_adj / ENTRIES_PER_BUCKET);
		*dynamic_bucket_cnt = 0;
#ifdef DYNAMIC_BUCKETS_SUPPORTED
	} else {
		*static_bucket_cnt_exp =
			prev_pow2(flow_cnt / ENTRIES_PER_BUCKET);
		*dynamic_bucket_cnt =
			(flow_adj - flow_cnt) / ENTRIES_PER_BUCKET;
	}
#endif

	*lkup_rec_cnt = key_rec_cnt + (1 << *static_bucket_cnt_exp) +
		*dynamic_bucket_cnt;

	return 0;
}

static int calc_act_rec_cnt(uint32_t *act_rec_cnt, uint32_t flow_cnt,
			    uint16_t act_rec_sz_in_bytes)
{
	if (act_rec_sz_in_bytes % RECORD_SIZE) {
		PMD_DRV_LOG_LINE(ERR,
				 "Action record size (%u) must be a multiple "
				 "of %u",
				 act_rec_sz_in_bytes, RECORD_SIZE);
		return -EINVAL;
	}

	*act_rec_cnt = flow_cnt * (act_rec_sz_in_bytes / RECORD_SIZE);

	return 0;
}

/*
 * Using a #define for the number of bits since the size of an int can depend
 * upon the processor.
 */
#define BITS_IN_UINT (sizeof(unsigned int) * 8)

static int calc_pool_sz_exp(uint8_t *pool_sz_exp, uint32_t rec_cnt,
			    uint32_t max_pools)
{
	unsigned int recs_per_region = rec_cnt / max_pools;

	if (recs_per_region == 0) {
		PMD_DRV_LOG_LINE(ERR,
				 "rec_cnt (%u) must be larger than max_pools "
				 "(%u)", rec_cnt, max_pools);
		return -EINVAL;
	}

	*pool_sz_exp = prev_pow2(recs_per_region + 1);

	return 0;
}

static int calc_rec_start_offset(uint32_t *start_offset, uint32_t bucket_cnt_exp)
{
	*start_offset = 1 << bucket_cnt_exp;

	return 0;
}

static void free_pg_tbl(struct tfc_ts_page_tbl *tp)
{
	tp->pg_count = 0;
	rte_free(tp->pg_va_tbl);
	tp->pg_va_tbl = NULL;
	rte_free(tp->pg_pa_tbl);
	tp->pg_pa_tbl = NULL;
}

/* simple sequential allocator, there is no associated free */
static void tfc_mz_alloc(struct tfc_ts_mz *ts_mz, uint64_t *pa, void **va)
{
	if (ts_mz->alloc_count <= ts_mz->page_count) {
		ts_mz->alloc_count++;
	} else {
		*pa = 0;
		*va = NULL;
		return;
	}

	*pa = (uint64_t)(((uint8_t *)((uintptr_t)ts_mz->mz->iova)) +
			 ((ts_mz->alloc_count - 1) * ts_mz->page_size));
	*va = (void *)(((uint8_t *)ts_mz->mz->addr) +
		       ((ts_mz->alloc_count - 1) * ts_mz->page_size));
}

static int alloc_pg_tbl(struct tfc_ts_mem_cfg *mem_cfg, struct tfc_ts_page_tbl *tp,
			uint32_t pg_count, uint32_t pg_size)
{
	uint32_t i;

	/*
	 * The VA and PA tables are used locally to store the page information
	 * and are not shared via memory
	 */
	tp->pg_va_tbl =
		rte_zmalloc("tfc tbl scope", pg_count * sizeof(void *), 0);
	if (tp->pg_va_tbl == NULL)
		return -ENOMEM;

	tp->pg_pa_tbl =
		rte_zmalloc("tfc tbl scope", pg_count * sizeof(void *), 0);
	if (tp->pg_pa_tbl == NULL) {
		rte_free(tp->pg_va_tbl);
		return -ENOMEM;
	}

	tp->pg_count = 0;
	tp->pg_size = pg_size;

	for (i = 0; i < pg_count; i++) {
		tfc_mz_alloc(&mem_cfg->ts_mz, &tp->pg_pa_tbl[i], &tp->pg_va_tbl[i]);
		if (tp->pg_va_tbl[i] == NULL)
			goto cleanup;

		tp->pg_count++;
	}

	return 0;

cleanup:
	free_pg_tbl(tp);
	return -ENOMEM;
}

static void free_page_table(struct tfc_ts_mem_cfg *mem_cfg)
{
	struct tfc_ts_page_tbl *tp;
	int i;

	for (i = 0; i < mem_cfg->num_lvl; i++) {
		tp = &mem_cfg->pg_tbl[i];
		PMD_DRV_LOG_LINE(DEBUG, "EEM: Freeing page table: lvl %d cnt %u",
				 i, tp->pg_count);

		free_pg_tbl(tp);
	}

	rte_memzone_free(mem_cfg->ts_mz.mz);
	memset(&mem_cfg->ts_mz, 0, sizeof(mem_cfg->ts_mz));

	mem_cfg->l0_addr = NULL;
	mem_cfg->l0_dma_addr = 0;
	mem_cfg->num_lvl = 0;
	mem_cfg->num_data_pages = 0;
}

static int alloc_page_table(struct tfc_ts_mem_cfg *mem_cfg, uint32_t page_size)
{
	struct tfc_ts_page_tbl *tp;
	int rc = 0;
	int i;

	for (i = 0; i < mem_cfg->num_lvl; i++) {
		tp = &mem_cfg->pg_tbl[i];

		rc = alloc_pg_tbl(mem_cfg, tp, mem_cfg->page_cnt[i], page_size);
		if (rc) {
			PMD_DRV_LOG_LINE(WARNING,
					 "Failed to allocate page table: lvl: %d, "
					 "rc:%s", i, strerror(-rc));
			goto cleanup;
		}
	}
	return rc;

cleanup:
	free_page_table(mem_cfg);
	return rc;
}

static uint32_t page_tbl_pgcnt(uint32_t num_pages, uint32_t page_size)
{
	return roundup32(num_pages, MAX_PAGE_PTRS(page_size)) /
	       MAX_PAGE_PTRS(page_size);
	return 0;
}

static void size_page_tbls(int max_lvl, uint64_t num_data_pages,
			   uint32_t page_size, uint32_t *page_cnt,
			   uint32_t *total_pages)
{
	if (max_lvl == TFC_TS_PT_LVL_0) {
		page_cnt[TFC_TS_PT_LVL_0] = num_data_pages;
		*total_pages = page_cnt[TFC_TS_PT_LVL_0];
	} else if (max_lvl == TFC_TS_PT_LVL_1) {
		page_cnt[TFC_TS_PT_LVL_1] = num_data_pages;
		page_cnt[TFC_TS_PT_LVL_0] =
			page_tbl_pgcnt(page_cnt[TFC_TS_PT_LVL_1], page_size);
		*total_pages = page_cnt[TFC_TS_PT_LVL_0] + page_cnt[TFC_TS_PT_LVL_1];
	} else if (max_lvl == TFC_TS_PT_LVL_2) {
		page_cnt[TFC_TS_PT_LVL_2] = num_data_pages;
		page_cnt[TFC_TS_PT_LVL_1] =
			page_tbl_pgcnt(page_cnt[TFC_TS_PT_LVL_2], page_size);
		page_cnt[TFC_TS_PT_LVL_0] =
			page_tbl_pgcnt(page_cnt[TFC_TS_PT_LVL_1], page_size);
		*total_pages = page_cnt[TFC_TS_PT_LVL_0] +
			page_cnt[TFC_TS_PT_LVL_1] +
			page_cnt[TFC_TS_PT_LVL_2];
	} else {
		return;
	}
}

static int num_pages_get(struct tfc_ts_mem_cfg *mem_cfg, uint32_t page_size)
{
	uint64_t lvl_data_size = page_size;
	uint64_t data_size;
	uint64_t max_page_ptrs = MAX_PAGE_PTRS(page_size);
	int lvl = TFC_TS_PT_LVL_0;

	mem_cfg->num_data_pages = 0;
	data_size = (uint64_t)mem_cfg->rec_cnt * mem_cfg->entry_size;

	while (lvl_data_size < data_size) {
		lvl++;

		if (lvl == TFC_TS_PT_LVL_1)
			lvl_data_size = max_page_ptrs * page_size;
		else if (lvl == TFC_TS_PT_LVL_2)
			lvl_data_size =
				max_page_ptrs * max_page_ptrs * page_size;
		else
			return -ENOMEM;
	}

	mem_cfg->num_data_pages = roundup64(data_size, page_size) / page_size;
	mem_cfg->num_lvl = lvl + 1;

	return 0;
}

static void link_page_table(struct tfc_ts_page_tbl *tp,
			    struct tfc_ts_page_tbl *tp_next, bool set_pte_last)
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

			pg_va[j] = rte_cpu_to_le_64(pg_pa[k] | valid);
			if (++k >= tp_next->pg_count)
				return;
		}
	}
}

static void setup_page_table(struct tfc_ts_mem_cfg *mem_cfg)
{
	struct tfc_ts_page_tbl *tp_next;
	struct tfc_ts_page_tbl *tp;
	bool set_pte_last = 0;
	int i;

	for (i = 0; i < mem_cfg->num_lvl - 1; i++) {
		tp = &mem_cfg->pg_tbl[i];
		tp_next = &mem_cfg->pg_tbl[i + 1];
		if (i == mem_cfg->num_lvl - 2)
			set_pte_last = 1;
		link_page_table(tp, tp_next, set_pte_last);
	}

	mem_cfg->l0_addr = mem_cfg->pg_tbl[TFC_TS_PT_LVL_0].pg_va_tbl[0];
	mem_cfg->l0_dma_addr = mem_cfg->pg_tbl[TFC_TS_PT_LVL_0].pg_pa_tbl[0];
}

static void unlink_and_free(struct tfc_ts_mem_cfg *mem_cfg, uint32_t page_size)
{
	/* tf_em_free_page_table */
	struct tfc_ts_page_tbl *tp;
	int i;

	for (i = 0; i < mem_cfg->num_lvl; i++) {
		tp = &mem_cfg->pg_tbl[i];
		PMD_DRV_LOG_LINE(DEBUG,
				 "EEM: Freeing page table: size %u lvl %d cnt %u",
				 page_size, i, tp->pg_count);

		/* tf_em_free_pg_tbl */
		free_pg_tbl(tp);
	}

	mem_cfg->l0_addr = NULL;
	mem_cfg->l0_dma_addr = 0;
	mem_cfg->num_lvl = 0;
	mem_cfg->num_data_pages = 0;
}

static int alloc_link_pbl(struct tfc_ts_mem_cfg *mem_cfg, uint32_t page_size,
			  uint8_t tsid, int dir, const char *type)
{
	int rc;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	int mz_size;
	uint64_t total_size;
	uint32_t total_pages;

	/* tf_em_size_page_tbl_lvl */
	rc = num_pages_get(mem_cfg, page_size);
	if (rc) {
		PMD_DRV_LOG_LINE(WARNING, "EEM: Failed to size page table levels");
		PMD_DRV_LOG_LINE(WARNING, "data-sz: %016" PRIu64 " page-sz: %u",
				 (uint64_t)mem_cfg->rec_cnt * mem_cfg->entry_size,
				 page_size);
		return rc;
	}

	/* tf_em_size_page_tbls */
	size_page_tbls(mem_cfg->num_lvl - 1, mem_cfg->num_data_pages, page_size,
		       mem_cfg->page_cnt, &total_pages);

	PMD_DRV_LOG_LINE(DEBUG,
			 "EEM: lvls: %d sz: %016" PRIu64 " pgs: %016" PRIu64
			 " l0: %u l1: %u l2: %u",
			 mem_cfg->num_lvl, mem_cfg->num_data_pages * page_size,
			 mem_cfg->num_data_pages, mem_cfg->page_cnt[TFC_TS_PT_LVL_0],
			 mem_cfg->page_cnt[TFC_TS_PT_LVL_1],
			 mem_cfg->page_cnt[TFC_TS_PT_LVL_2]);

	/*
	 * Allocate single blob large enough for the backing store pages
	 * and page tables. The allocation will occur once only per backing
	 * store and will located by name and reused on subsequent runs.
	 */
	total_size = (uint64_t)page_size * (uint64_t)total_pages;

	if (total_size <= (1024 * 256))
		mz_size = RTE_MEMZONE_256KB;
	else if (total_size <= (1024 * 1024 * 2))
		mz_size = RTE_MEMZONE_2MB;
	else if (total_size <= (1024 * 1024 * 16))
		mz_size = RTE_MEMZONE_16MB;
	else if (total_size <= (1024 * 1024 * 256))
		mz_size = RTE_MEMZONE_256MB;
	else if (total_size <= (1024 * 1024 * 512))
		mz_size = RTE_MEMZONE_512MB;
	else if (total_size <= (1024 * 1024 * 1024))
		mz_size = RTE_MEMZONE_1GB;
	else if (total_size <= (1024ULL * 1024 * 1024 * 4))
		mz_size = RTE_MEMZONE_4GB;
	else if (total_size <= (1024ULL * 1024 * 1024 * 16))
		mz_size = RTE_MEMZONE_16GB;
	else
		return -ENOMEM;

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
		 "bnxt_bs_%d_%d_%s",
		 tsid, dir, type);
	PMD_DRV_LOG_LINE(DEBUG, "EEM: mz name:%s", mz_name);

	mem_cfg->ts_mz.mz = rte_memzone_lookup(mz_name);
	if (!mem_cfg->ts_mz.mz) {
		/* Backing store does not already exist so allocate */
		mem_cfg->ts_mz.mz = rte_memzone_reserve_aligned(mz_name,
								total_size,
								SOCKET_ID_ANY,
								mz_size |
								RTE_MEMZONE_SIZE_HINT_ONLY |
								RTE_MEMZONE_IOVA_CONTIG,
								page_size);
	}
	memset(mem_cfg->ts_mz.mz->addr, 0, mem_cfg->ts_mz.mz->len);
	mem_cfg->ts_mz.page_count = total_pages;
	mem_cfg->ts_mz.page_size = page_size;

	/* tf_em_alloc_page_table -> tf_em_alloc_pg_tbl */
	rc = alloc_page_table(mem_cfg, page_size);
	if (rc)
		goto cleanup;

	/* tf_em_setup_page_table */
	setup_page_table(mem_cfg);

	return 0;

cleanup:
	unlink_and_free(mem_cfg, page_size);
	return rc;
}

/**
 * tbl_scope_pools_create_parms contains the parameters for creating pools.
 */
struct tbl_scope_pools_create_parms {
	/**
	 * [in] Indicates if the table scope will be shared.
	 */
	bool shared;
	/**
	 * [in] The number of pools the table scope will be divided into. (set
	 * to 1 if not shared).
	 */
	uint16_t max_pools;
	/**
	 * [in] The size of each individual lookup record pool expressed as:
	 * log2(max_records/max_pools).	 For example if 1024 records and 2 pools
	 * 1024/2=512=2^9, so the value 9 would be entered.
	 */
	uint8_t lkup_pool_sz_exp[CFA_DIR_MAX];
	/**
	 * [in] The size of each individual action record pool expressed as:
	 * log2(max_records/max_pools).	 For example if 1024 records and 2 pools
	 * 1024/2=512=2^9, so the value 9 would be entered.
	 */
	uint8_t act_pool_sz_exp[CFA_DIR_MAX];
};
/**
 * Allocate and store TPM and TIM for shared scope
 *
 * Dynamically allocate and store TPM instances for shared scope
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[in] params
 *   Parameters for allocate and store TPM instances for shared scope
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
static int tbl_scope_pools_create(struct tfc *tfcp, uint8_t tsid,
				  struct tbl_scope_pools_create_parms *parms)
{
	int rc;
	int dir;
	enum cfa_region_type region;
	uint32_t tpm_db_size;
	void *tim;
	void *tpms[CFA_DIR_MAX][CFA_REGION_TYPE_MAX];
	void *tpm;

	/*
	 * Dynamically allocate and store base addresses for TIM,
	 * TPM instances for the given tsid
	 */
	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tfcp->tfo == NULL || tfcp->bp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tfcp pointer not initialized");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}

	rc = tfo_tim_get(tfcp->tfo, &tim);
	if (rc)
		return -EINVAL;

	rc = cfa_tpm_query(parms->max_pools, &tpm_db_size);
	if (rc)
		return -EINVAL;

	memset(tpms, 0, sizeof(void *) * CFA_DIR_MAX * CFA_REGION_TYPE_MAX);

	/* Allocate pool managers */
	for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			tpms[dir][region] = rte_zmalloc("TPM", tpm_db_size, 0);
			if (tpms[dir][region] == NULL)
				goto cleanup;

			rc = cfa_tpm_open(tpms[dir][region], tpm_db_size, parms->max_pools);
			if (rc)
				goto cleanup;

			rc = cfa_tpm_pool_size_set(tpms[dir][region],
				   (region == CFA_REGION_TYPE_LKUP ? parms->lkup_pool_sz_exp[dir] :
				    parms->act_pool_sz_exp[dir]));

			if (rc)
				goto cleanup;

			rc = cfa_tim_tpm_inst_set(tim, tsid, region, dir, tpms[dir][region]);
			if (rc)
				goto cleanup;
		}
	}

	return rc;

 cleanup:
	if (tim) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			for (dir = 0; dir < CFA_DIR_MAX; dir++) {
				/*
				 * It is possiible that a tpm has been
				 * allocated but not added to tim.
				 * Ensure that those instances are cleaned
				 * up.
				 */
				rc = cfa_tim_tpm_inst_get(tim,
							  tsid,
							  region,
							  dir,
							  &tpm);

				if (tpm) {
					rc = cfa_tim_tpm_inst_set(tim,
								  tsid,
								  region,
								  dir,
								  NULL);
					rte_free(tpm);
				} else if (tpms[dir][region] != NULL) {
					rte_free(tpms[dir][region]);
				}
			}
		}
	}

	return rc;
}

/**
 * Free TPM instances for shared scope
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
static int tbl_scope_pools_destroy(struct tfc *tfcp, uint8_t tsid)
{
	int rc;
	void *tim;
	int dir;
	enum cfa_region_type region;
	void *tpm;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tfcp->tfo == NULL || tfcp->bp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tfcp pointer not initialized");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}

	rc = tfo_tim_get(tfcp->tfo, &tim);
	if (rc)
		return -EINVAL;

	/* Free TIM, TPM instances for the given tsid. */
	if (tim) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			for (dir = 0; dir < CFA_DIR_MAX; dir++) {
				rc = cfa_tim_tpm_inst_get(tim,
							  tsid,
							  region,
							  dir,
							  &tpm);
				if (rc)
					return -EINVAL;

				if (tpm) {
					rc = cfa_tim_tpm_inst_set(tim,
								  tsid,
								  region,
								  dir,
								  NULL);
					rte_free(tpm);
				}
			}
		}
	}

	return rc;
}

/**
 * Remove all associated pools owned by a function from TPM
 *
 * @param[in] tfcp
 *   Pointer to TFC handle
 *
 * @param[in] fid
 *   function
 *
 * @param[in] tsid
 *   Table scope identifier
 *
 * @param[out] pool_cnt
 *   Pointer to the number of pools still associated with other fids.
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
static int tbl_scope_tpm_fid_rem(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
				 uint16_t *pool_cnt)
{
	int rc = 0;
	bool shared;
	bool valid;
	enum cfa_dir dir;
	uint16_t pool_id;
	uint16_t found_cnt = 0;
	void *tim;
	void *tpm;
	enum cfa_region_type region;
	uint16_t lfid;
	bool is_pf;
	uint16_t max_fid;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	if (pool_cnt == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid pool_cnt pointer");
		return -EINVAL;
	}
	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc)
		return rc;

	if (!is_pf) {
		PMD_DRV_LOG_LINE(ERR, "only valid for PF");
		return -EINVAL;
	}
	rc = tfo_ts_get(tfcp->tfo, tsid, &shared, NULL, &valid, NULL);
	if (!valid || !shared) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) valid(%s) shared(%s)",
				 tsid, valid ? "TRUE" : "FALSE",
				 shared ? "TRUE" : "FALSE");
		return -EINVAL;
	}

	rc = tfo_tim_get(tfcp->tfo, &tim);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get TIM");
		return -EINVAL;
	}

	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			/*
			 * Get the TPM and then check to see if the fid is associated
			 * with any of the pools
			 */
			rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
			if (rc) {
				PMD_DRV_LOG_LINE(ERR, "Failed to get TPM for tsid:%d dir:%d",
						 tsid, dir);
				return -EINVAL;
			}
			rc = cfa_tpm_srchm_by_fid(tpm, CFA_SRCH_MODE_FIRST, fid, &pool_id);
			if (rc) /* FID not used */
				continue;
			PMD_DRV_LOG_LINE(ERR, "tsid(%d) fid(%d) region(%s) pool_id(%d)",
					 tsid, fid, tfc_ts_region_2_str(region, dir),
					 pool_id);
			do {
				/* Remove fid from pool */
				rc = cfa_tpm_fid_rem(tpm, pool_id, fid);
				if (rc)
					PMD_DRV_LOG_LINE(ERR,
							 "cfa_tpm_fid_rem() failed for fid:%d pool:%d",
							 fid, pool_id);

				rc = cfa_tpm_srchm_by_fid(tpm,
							  CFA_SRCH_MODE_NEXT,
							  fid, &pool_id);
				if (!rc)
					PMD_DRV_LOG_LINE(ERR,
							 "tsid(%d) fid(%d) region(%s) pool_id(%d)",
							 tsid, fid,
							 tfc_ts_region_2_str(region, dir), pool_id);
			} while (!rc);
		}
	}
	rc = tfc_bp_vf_max(tfcp, &max_fid);
	if (rc)
		return rc;

	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
			/*
			 * Get the TPM and then check to see if the fid is associated
			 * with any of the pools
			 */
			rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
			if (rc) {
				PMD_DRV_LOG_LINE(ERR, "Failed to get TPM for tsid:%d dir:%d",
						 tsid, dir);
				return -EINVAL;
			}
			for (lfid = BNXT_FIRST_PF_FID; lfid <= max_fid; lfid++) {
				rc = cfa_tpm_srchm_by_fid(tpm, CFA_SRCH_MODE_FIRST,
							  lfid, &pool_id);
				if (rc) /* FID not used */
					continue;
				PMD_DRV_LOG_LINE(ERR, "tsid(%d) fid(%d) region(%s) pool_id(%d)",
						 tsid, lfid, tfc_ts_region_2_str(region, dir),
						 pool_id);
				do {
					found_cnt++;
					rc = cfa_tpm_srchm_by_fid(tpm,
								  CFA_SRCH_MODE_NEXT,
								  lfid, &pool_id);
					if (rc == 0) {
						PMD_DRV_LOG_LINE(ERR,
							 "tsid(%d) fid(%d) region(%s) pool_id(%d)",
							 tsid, lfid,
							 tfc_ts_region_2_str(region, dir), pool_id);
					}
				} while (!rc);
			}
		}
	}
	*pool_cnt = found_cnt;
	return 0;
}

/* Public APIs */

int tfc_tbl_scope_qcaps(struct tfc *tfcp, bool *tbl_scope_capable,
			uint32_t *max_lkup_rec_cnt,
			uint32_t *max_act_rec_cnt,
			uint8_t	*max_lkup_static_buckets_exp)
{
	int rc = 0;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	if (tbl_scope_capable == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tbl_scope_capable pointer");
		return -EINVAL;
	}

	rc = tfc_msg_tbl_scope_qcaps(tfcp, tbl_scope_capable, max_lkup_rec_cnt,
				     max_act_rec_cnt,
				     max_lkup_static_buckets_exp);
	if (rc)
		PMD_DRV_LOG_LINE(ERR,
				 "table scope qcaps message failed, rc:%s",
				 strerror(-rc));

	return rc;
}

int tfc_tbl_scope_size_query(struct tfc *tfcp,
			     struct tfc_tbl_scope_size_query_parms *parms)
{
	int rc = 0;
	enum cfa_dir dir;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	if (parms == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid parms pointer");
		return -EINVAL;
	}

	if (parms->factor > TFC_TBL_SCOPE_BUCKET_FACTOR_MAX) {
		PMD_DRV_LOG_LINE(ERR, "Invalid factor %u", parms->factor);
		return -EINVAL;
	}

	if (is_pow2(parms->max_pools)) {
		PMD_DRV_LOG(ERR, "%s: Invalid max_pools %u not pow2\n",
			    __func__, parms->max_pools);
		return -EINVAL;
	}

	for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
		rc = calc_lkup_rec_cnt(parms->flow_cnt[dir],
				       parms->key_sz_in_bytes[dir],
				       parms->shared, parms->factor,
				       &parms->lkup_rec_cnt[dir],
				       &parms->static_bucket_cnt_exp[dir],
				       &parms->dynamic_bucket_cnt[dir]);
		if (rc)
			break;

		rc = calc_act_rec_cnt(&parms->act_rec_cnt[dir],
				      parms->flow_cnt[dir],
				      parms->act_rec_sz_in_bytes[dir]);
		if (rc)
			break;

		rc = calc_pool_sz_exp(&parms->lkup_pool_sz_exp[dir],
				      parms->lkup_rec_cnt[dir] -
				      (1 << parms->static_bucket_cnt_exp[dir]),
				      parms->max_pools);
		if (rc)
			break;

		rc = calc_pool_sz_exp(&parms->act_pool_sz_exp[dir],
				      parms->act_rec_cnt[dir],
				      parms->max_pools);
		if (rc)
			break;

		rc = calc_rec_start_offset(&parms->lkup_rec_start_offset[dir],
					   parms->static_bucket_cnt_exp[dir]);
		if (rc)
			break;
	}

	return rc;
}

int tfc_tbl_scope_id_alloc(struct tfc *tfcp, bool shared,
			   enum cfa_app_type app_type, uint8_t *tsid,
			   bool *first)
{
	int rc;
	bool valid = true;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	if (tsid == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid pointer");
		return -EINVAL;
	}
	if (first == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid first pointer");
		return -EINVAL;
	}
	if (app_type >= CFA_APP_TYPE_INVALID) {
		PMD_DRV_LOG_LINE(ERR, "Invalid app type");
		return -EINVAL;
	}
	rc = tfc_msg_tbl_scope_id_alloc(tfcp, ((struct bnxt *)tfcp->bp)->fw_fid,
					shared, app_type, tsid, first);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR,
				 "table scope ID alloc message failed, rc:%s",
				 strerror(-rc));
	} else {
		rc = tfo_ts_set(tfcp->tfo, *tsid, shared, app_type, valid, 0);
	}
	return rc;
}

int tfc_tbl_scope_mem_alloc(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			    struct tfc_tbl_scope_mem_alloc_parms *parms)
{
	struct tfc_ts_mem_cfg lkup_mem_cfg[CFA_DIR_MAX];
	struct tfc_ts_mem_cfg act_mem_cfg[CFA_DIR_MAX];
	uint64_t lkup_base_addr[2];
	uint64_t act_base_addr[2];
	int dir;
	int rc = 0;
	bool shared = false;
	uint32_t page_sz;
	uint16_t pfid;
	uint8_t lkup_pbl_level[2];
	uint8_t act_pbl_level[2];
	bool valid, cfg_done;
	uint8_t cfg_cnt;
	bool is_pf;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (parms == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid parms pointer");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, &valid) != 0) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tsid(%d) object", tsid);
		return -EINVAL;
	}

	if (parms->local && !valid) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) not allocated", tsid);
		return -EINVAL;
	}

	if (is_pow2(parms->max_pools)) {
		PMD_DRV_LOG(ERR, "%s: Invalid max_pools %u not pow2\n",
			    __func__, parms->max_pools);
		return -EINVAL;
	}

	/* Normalize page size to a power of 2 */
	page_sz = 1 << next_pow2(parms->pbl_page_sz_in_bytes);
	if (parms->pbl_page_sz_in_bytes != page_sz ||
	    (page_sz & VALID_PAGE_ALIGNMENTS) == 0) {
		PMD_DRV_LOG_LINE(ERR, "Invalid page size %d",
				 parms->pbl_page_sz_in_bytes);
		return -EINVAL;
	}

	memset(lkup_mem_cfg, 0, sizeof(lkup_mem_cfg));
	memset(act_mem_cfg, 0, sizeof(act_mem_cfg));

	rc = tfc_get_pfid(tfcp, &pfid);
	if (rc)
		return rc;

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc)
		return rc;

	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		struct tfc_ts_pool_info pi;

		rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, dir, &pi);
		if (rc)
			return rc;

		pi.lkup_pool_sz_exp = parms->lkup_pool_sz_exp[dir];
		pi.act_pool_sz_exp = parms->act_pool_sz_exp[dir];
		rc = tfo_ts_set_pool_info(tfcp->tfo, tsid, dir, &pi);
		if (rc)
			return rc;
	}

	/*
	 * A shared table scope will have more than 1 pool
	 */
	if (parms->max_pools > 1)
		shared = true;

	/* If we are running on a PF, we will allocate memory locally
	 */
	if (is_pf) {
		struct tbl_scope_pools_create_parms cparms;

		cfg_done = false;
		cfg_cnt = 0;
		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			lkup_mem_cfg[dir].rec_cnt = parms->lkup_rec_cnt[dir];
			lkup_mem_cfg[dir].lkup_rec_start_offset =
				1 << parms->static_bucket_cnt_exp[dir];
			lkup_mem_cfg[dir].entry_size = RECORD_SIZE;

			PMD_DRV_LOG_LINE(DEBUG, "Alloc lkup table: dir %d", dir);

			rc = alloc_link_pbl(&lkup_mem_cfg[dir],
					    parms->pbl_page_sz_in_bytes,
					    tsid,
					    dir,
					    "lkup");
			if (rc)
				goto cleanup;

			lkup_base_addr[dir] = lkup_mem_cfg[dir].l0_dma_addr;
			lkup_pbl_level[dir] = lkup_mem_cfg[dir].num_lvl - 1;

			rc = tfc_msg_backing_store_cfg_v2(tfcp, tsid, dir,
							  CFA_REGION_TYPE_LKUP,
							  lkup_base_addr[dir],
							  lkup_pbl_level[dir],
							  parms->pbl_page_sz_in_bytes,
							  parms->lkup_rec_cnt[dir],
							  parms->static_bucket_cnt_exp[dir],
							  cfg_done);

			if (rc) {
				PMD_DRV_LOG_LINE(ERR,
						 "backing store config message "
						 "failed dir(%s) lkup, rc:%s",
						 dir == CFA_DIR_RX ? "rx" : "tx",
						 strerror(-rc));
				goto cleanup;
			}

			rc = tfo_ts_set_mem_cfg(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_LKUP,
						parms->local, &lkup_mem_cfg[dir]);
			if (rc)
				goto cleanup;

			PMD_DRV_LOG_LINE(DEBUG, "Alloc action table: dir %d", dir);

			act_mem_cfg[dir].rec_cnt = parms->act_rec_cnt[dir];
			act_mem_cfg[dir].entry_size = RECORD_SIZE;

			rc = alloc_link_pbl(&act_mem_cfg[dir],
					    parms->pbl_page_sz_in_bytes,
					    tsid,
					    dir,
					    "act");
			if (rc)
				goto cleanup;


			act_base_addr[dir] = act_mem_cfg[dir].l0_dma_addr;
			act_pbl_level[dir] = act_mem_cfg[dir].num_lvl - 1;

			cfg_done = false;

			if (cfg_cnt)
				cfg_done = true;

			rc = tfc_msg_backing_store_cfg_v2(tfcp, tsid, dir,
							  CFA_REGION_TYPE_ACT,
							  act_base_addr[dir],
							  act_pbl_level[dir],
							  parms->pbl_page_sz_in_bytes,
							  parms->act_rec_cnt[dir], 0,
							  cfg_done);
			if (rc) {
				PMD_DRV_LOG_LINE(ERR,
						 "backing store config message "
						 "failed dir(%s) action, rc:%s",
						 dir == CFA_DIR_RX ? "rx" : "tx",
						 strerror(-rc));
				goto cleanup;
			}

			/* Set shared and valid in local state */
			valid = true;
			rc = tfo_ts_set(tfcp->tfo, tsid, shared, CFA_APP_TYPE_TF,
					valid, parms->max_pools);
			if (rc)
				goto cleanup;

			rc = tfo_ts_set_mem_cfg(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_ACT,
						parms->local, &act_mem_cfg[dir]);
			if (rc)
				goto cleanup;

			cfg_cnt++;
		}
		cparms.shared = shared;
		cparms.max_pools = parms->max_pools;

		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			cparms.lkup_pool_sz_exp[dir] = parms->lkup_pool_sz_exp[dir];
			cparms.act_pool_sz_exp[dir] = parms->act_pool_sz_exp[dir];
		}

		rc = tbl_scope_pools_create(tfcp, tsid, &cparms);
		if (rc)
			goto cleanup;

		/* If not shared, allocate the single pool_id in each region
		 * so that we can save the associated fid for the table scope
		 */
		if (!shared) {
			uint16_t pool_id;
			enum cfa_region_type region;
			uint16_t max_vf;

			rc = tfc_bp_vf_max(tfcp, &max_vf);
			if (rc)
				return rc;

			if (fid > max_vf) {
				PMD_DRV_LOG_LINE(ERR, "fid out of range %d", fid);
				return -EINVAL;
			}

			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				for (dir = 0; dir < CFA_DIR_MAX; dir++) {
					rc = tfc_tbl_scope_pool_alloc(tfcp,
								      fid,
								      tsid,
								      region,
								      dir,
								      NULL,
								      &pool_id);
					if (rc)
						goto cleanup;
					/* only 1 pool available */
					if (pool_id != 0)
						goto cleanup;
				}
			}
		}

	} else /* this is a VF */ {
		/* If first or !shared, send message to PF to allocate the memory */
		if (parms->first || !shared) {
			struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd req = { { 0 } };
			struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp resp = { { 0 } };
			uint16_t fid;

			rc = tfc_get_fid(tfcp, &fid);
			if (rc)
				return rc;

			req.hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_ALLOC_CFG_CMD;
			req.hdr.fid = fid;
			req.tsid = tsid;
			req.max_pools = parms->max_pools;
			for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
				req.static_bucket_cnt_exp[dir] = parms->static_bucket_cnt_exp[dir];
				req.dynamic_bucket_cnt[dir] = parms->dynamic_bucket_cnt[dir];
				req.lkup_rec_cnt[dir] = parms->lkup_rec_cnt[dir];
				req.lkup_pool_sz_exp[dir] = parms->lkup_pool_sz_exp[dir];
				req.act_pool_sz_exp[dir] = parms->act_pool_sz_exp[dir];
				req.act_rec_cnt[dir] = parms->act_rec_cnt[dir];
				req.lkup_rec_start_offset[dir] = parms->lkup_rec_start_offset[dir];
			}

			rc = tfc_vf2pf_mem_alloc(tfcp, &req, &resp);
			if (rc != 0) {
				PMD_DRV_LOG_LINE(ERR, "tfc_vf2pf_mem_alloc failed");
				goto cleanup;
			}

			PMD_DRV_LOG_LINE(DEBUG, "tsid: %d, status %d", resp.tsid, resp.status);
		}
		/* Save off info for later use */
		for (dir = CFA_DIR_RX; dir < CFA_DIR_MAX; dir++) {
			lkup_mem_cfg[dir].rec_cnt = parms->lkup_rec_cnt[dir];
			lkup_mem_cfg[dir].lkup_rec_start_offset =
							1 << parms->static_bucket_cnt_exp[dir];
			lkup_mem_cfg[dir].entry_size = RECORD_SIZE;

			act_mem_cfg[dir].rec_cnt = parms->act_rec_cnt[dir];
			act_mem_cfg[dir].entry_size = RECORD_SIZE;

			rc = tfo_ts_set_mem_cfg(tfcp->tfo,
						tsid,
						dir,
						CFA_REGION_TYPE_LKUP,
						parms->local,
						&lkup_mem_cfg[dir]);
			if (rc)
				goto cleanup;

			rc = tfo_ts_set_mem_cfg(tfcp->tfo,
						tsid,
						dir,
						CFA_REGION_TYPE_ACT,
						parms->local,
						&act_mem_cfg[dir]);
			if (rc)
				goto cleanup;

			/* Set shared and valid in local state */
			valid = true;
			rc = tfo_ts_set(tfcp->tfo, tsid, shared, CFA_APP_TYPE_TF,
					valid, parms->max_pools);
		}
	}

	return rc;
cleanup:
	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		unlink_and_free(&lkup_mem_cfg[dir],
				parms->pbl_page_sz_in_bytes);
		unlink_and_free(&act_mem_cfg[dir],
				parms->pbl_page_sz_in_bytes);
	}

	memset(lkup_mem_cfg, 0, sizeof(lkup_mem_cfg));
	memset(act_mem_cfg, 0, sizeof(act_mem_cfg));

	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		(void)tfo_ts_set_mem_cfg(tfcp->tfo, tsid, dir,
					 CFA_REGION_TYPE_LKUP,
					 parms->local,
					 &lkup_mem_cfg[dir]);
		(void)tfo_ts_set_mem_cfg(tfcp->tfo, tsid, dir,
					 CFA_REGION_TYPE_ACT,
					 parms->local,
					 &act_mem_cfg[dir]);
	}
	return rc;
}

int tfc_tbl_scope_mem_free(struct tfc *tfcp, uint16_t fid, uint8_t tsid)
{
	struct tfc_ts_mem_cfg mem_cfg;
	bool local;
	int dir, region;
	int lrc = 0;
	int rc = 0;
	bool is_pf = false;
	bool shared;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tfcp->tfo == NULL || tfcp->bp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tfcp pointer not initialized");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}

	rc = tfo_ts_get(tfcp->tfo, tsid, &shared, NULL, NULL, NULL);
	if (rc)
		return rc;

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc)
		return rc;

	/* Lookup any memory config to get local */
	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, CFA_DIR_RX, CFA_REGION_TYPE_LKUP, &local,
				&mem_cfg);
	if (rc)
		return rc;

	if (!is_pf) {
		PMD_DRV_LOG_LINE(DEBUG, "Send VF2PF message and await response");
		struct tfc_vf2pf_tbl_scope_mem_free_cmd req = { { 0 } };
		struct tfc_vf2pf_tbl_scope_mem_free_resp resp = { { 0 } };
		uint16_t fid;

		rc = tfc_get_fid(tfcp, &fid);
		if (rc)
			return rc;

		req.hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD;
		req.hdr.fid = fid;
		req.tsid = tsid;

		rc = tfc_vf2pf_mem_free(tfcp, &req, &resp);
		if (rc != 0) {
			PMD_DRV_LOG_LINE(ERR, "tfc_vf2pf_mem_free failed");
			/* continue cleanup regardless */
		}
		PMD_DRV_LOG_LINE(DEBUG, "%s: tsid: %d, status %d",
				 __func__, resp.tsid, resp.status);
		if (shared) {
			/* reset scope */
			tfo_ts_set(tfcp->tfo, tsid, false, CFA_APP_TYPE_INVALID, false, 0);
			return rc;
		}
	}

	if (shared && is_pf) {
		uint16_t pool_cnt;
		uint16_t max_vf;

		rc = tfc_bp_vf_max(tfcp, &max_vf);
		if (rc)
			return rc;

		if (fid > max_vf) {
			PMD_DRV_LOG_LINE(ERR, "invalid fid 0x%x", fid);
			return -EINVAL;
		}
		rc = tbl_scope_tpm_fid_rem(tfcp, fid, tsid, &pool_cnt);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR, "error getting tsid(%d) pools status %s",
					 tsid, strerror(-rc));
			return rc;
		}
		/* Then if there are still fids present, return */
		if (pool_cnt) {
			PMD_DRV_LOG_LINE(DEBUG, "tsid(%d) fids still present #pools(%d)",
					 tsid, pool_cnt);
			return 0;
		}
	}

	/* Send Deconfig HWRM before freeing memory */
	rc = tfc_msg_tbl_scope_deconfig(tfcp, tsid);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "deconfig failure: %s", strerror(-rc));
		return rc;
	}

	for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			lrc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, dir, region, &local,
						 &mem_cfg);
			if (lrc) {
				rc = lrc;
				continue;
			}
			/* memory only allocated on PF */
			if (is_pf)
				unlink_and_free(&mem_cfg, mem_cfg.pg_tbl[0].pg_size);

			memset(&mem_cfg, 0, sizeof(mem_cfg));

			/* memory freed, set local to false */
			local = false;
			(void)tfo_ts_set_mem_cfg(tfcp->tfo, tsid, dir, region, local,
						 &mem_cfg);
		}
	}
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) db err(%s), continuing",
				 tsid, strerror(rc));
	}
	if (is_pf) {
		rc = tbl_scope_pools_destroy(tfcp, tsid);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR, "tsid(%d)  pool err(%s) continuing",
					 tsid, strerror(rc));
		}
	}
	/* cleanup state */
	rc = tfo_ts_set(tfcp->tfo, tsid, false, CFA_APP_TYPE_INVALID, false, 0);

	return rc;
}

int tfc_tbl_scope_fid_add(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			  uint16_t *fid_cnt)
{
	int rc = 0;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (((struct bnxt *)tfcp->bp)->fw_fid != fid) {
		PMD_DRV_LOG_LINE(ERR, "Invalid fid");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}

	rc = tfc_msg_tbl_scope_fid_add(tfcp, fid, tsid, fid_cnt);
	if (rc)
		PMD_DRV_LOG_LINE(ERR,
				 "table scope fid add message failed, rc:%s",
				 strerror(-rc));

	return rc;
}

int tfc_tbl_scope_fid_rem(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			  uint16_t *fid_cnt)
{
	struct tfc_cpm *cpm_lkup;
	struct tfc_cpm *cpm_act;
	int rc = 0;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tfcp->tfo == NULL || tfcp->bp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tfcp pointer not initialized");
		return -EINVAL;
	}

	if (((struct bnxt *)tfcp->bp)->fw_fid != fid) {
		PMD_DRV_LOG_LINE(ERR, "Invalid fid");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}

	rc = tfc_msg_tbl_scope_fid_rem(tfcp, fid, tsid, fid_cnt);
	if (rc)
		PMD_DRV_LOG_LINE(ERR,
				 "table scope fid rem message failed, rc:%s",
				 strerror(-rc));

	/*
	 * Check if any direction has a CPM instance and, if so, free
	 * it.
	 */
	rc = tfo_ts_get_cpm_inst(tfcp->tfo, tsid, CFA_DIR_RX, &cpm_lkup,
				 &cpm_act);
	if (rc == 0 && (cpm_lkup != NULL || cpm_act != NULL))
		(void)tfc_tbl_scope_cpm_free(tfcp, tsid);

	/* tbl_scope_mem_free() will reset the remaining tsid state */
	return rc;
}

int tfc_tbl_scope_cpm_alloc(struct tfc *tfcp, uint8_t tsid,
			    struct tfc_tbl_scope_cpm_alloc_parms *parms)
{
	int dir;
	struct tfc_ts_pool_info pi;
	bool is_shared;
	int rc;
	struct tfc_cmm *cmm_lkup = NULL;
	struct tfc_cmm *cmm_act = NULL;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}
	if (tfo_ts_get(tfcp->tfo, tsid, &is_shared, NULL, NULL, NULL)) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) info get failed", tsid);
		return -EINVAL;
	}

	/* Create 4 CPM instances and set the pool_sz_exp and max_pools for each
	 */
	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		tfo_ts_get_pool_info(tfcp->tfo, tsid, dir, &pi);
		pi.lkup_max_contig_rec = parms->lkup_max_contig_rec[dir];
		pi.act_max_contig_rec = parms->act_max_contig_rec[dir];
		tfc_cpm_open(&pi.lkup_cpm, parms->max_pools);
		tfc_cpm_set_pool_size(pi.lkup_cpm, (1 << pi.lkup_pool_sz_exp));
		tfc_cpm_open(&pi.act_cpm, parms->max_pools);
		tfc_cpm_set_pool_size(pi.act_cpm, (1 << pi.act_pool_sz_exp));
		tfo_ts_set_cpm_inst(tfcp->tfo, tsid, dir, pi.lkup_cpm, pi.act_cpm);
		tfo_ts_set_pool_info(tfcp->tfo, tsid, dir, &pi);

		/* If not shared create CMM instance for and populate CPM with pool_id 0.
		 * If shared, a pool_id will be allocated during tfc_act_alloc() or
		 * tfc_em_insert() and the CMM instance will be created on the first
		 * call.
		 */
		if (!is_shared) {
			struct cfa_mm_query_parms qparms;
			struct cfa_mm_open_parms oparms;
			uint32_t pool_id = 0;
			struct tfc_ts_mem_cfg mem_cfg;

			/* ACTION */
			rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_ACT,
						NULL, &mem_cfg);
			if (rc != 0) {
				PMD_DRV_LOG_LINE(ERR, "tfo_ts_get_mem_cfg() failed: %s",
						 strerror(-rc));
				return -EINVAL;
			}
			/* override the record size since a single pool because
			 * pool_sz_exp is 0 in this case
			 */
			tfc_cpm_set_pool_size(pi.act_cpm, mem_cfg.rec_cnt);

			/*create CMM instance */
			qparms.max_records = mem_cfg.rec_cnt;
			qparms.max_contig_records = pi.act_max_contig_rec;
			rc = cfa_mm_query(&qparms);
			if (rc != 0) {
				PMD_DRV_LOG_LINE(ERR, "cfa_mm_query() failed: %d",
						 rc);
				return -EINVAL;
			}

			cmm_act = rte_zmalloc("tf", qparms.db_size, 0);
			oparms.db_mem_size = qparms.db_size;
			oparms.max_contig_records = qparms.max_contig_records;
			oparms.max_records = qparms.max_records;
			rc = cfa_mm_open(cmm_act, &oparms);
			if (rc != 0) {
				PMD_DRV_LOG_LINE(ERR, "cfa_mm_open() failed: %d", rc);
				rc = -EINVAL;
				goto cleanup;
			}
			/* Store CMM instance in the CPM for pool_id 0 */
			rc = tfc_cpm_set_cmm_inst(pi.act_cpm, pool_id, cmm_act);
			if (rc != 0) {
				PMD_DRV_LOG_LINE(ERR, "tfc_cpm_set_cmm_inst() failed: %d",
						 rc);
				rc = -EINVAL;
				goto cleanup;
			}
			/* LOOKUP */
			rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid, dir, CFA_REGION_TYPE_LKUP,
						NULL, &mem_cfg);
			if (rc != 0) {
				PMD_DRV_LOG_LINE(ERR, "tfo_ts_get_mem_cfg() failed: %s",
						 strerror(-rc));
				rc = -EINVAL;
				goto cleanup;
			}
			/* Create lkup pool CMM instance */
			qparms.max_records = mem_cfg.rec_cnt - mem_cfg.lkup_rec_start_offset;
			qparms.max_contig_records = pi.lkup_max_contig_rec;
			rc = cfa_mm_query(&qparms);
			if (rc != 0) {
				PMD_DRV_LOG_LINE(ERR, "cfa_mm_query() failed: %d", rc);
				rc = -EINVAL;
				goto cleanup;
			}
			cmm_lkup = rte_zmalloc("tf", qparms.db_size, 0);
			oparms.db_mem_size = qparms.db_size;
			oparms.max_contig_records = qparms.max_contig_records;
			oparms.max_records = qparms.max_records;
			rc = cfa_mm_open(cmm_lkup, &oparms);
			if (rc != 0) {
				PMD_DRV_LOG_LINE(ERR, "cfa_mm_open() failed: %d", rc);
				rc = -EINVAL;
				goto cleanup;
			}
			/* override the record size since a single pool because
			 * pool_sz_exp is 0 in this case
			 */
			tfc_cpm_set_pool_size(pi.lkup_cpm,
					      mem_cfg.rec_cnt - mem_cfg.lkup_rec_start_offset);

			/* Store CMM instance in the CPM for pool_id 0 */
			rc = tfc_cpm_set_cmm_inst(pi.lkup_cpm, pool_id, cmm_lkup);
			if (rc != 0) {
				PMD_DRV_LOG_LINE(ERR, "tfc_cpm_set_cmm_inst() failed: %d", rc);
				rc = -EINVAL;
				goto cleanup;
			}
		}
	}

	return 0;

 cleanup:
	rte_free(cmm_lkup);
	rte_free(cmm_act);

	return rc;
}

int tfc_tbl_scope_cpm_free(struct tfc *tfcp, uint8_t tsid)
{
	int dir;
	struct tfc_ts_pool_info pi;
	int rc;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tfcp->tfo == NULL || tfcp->bp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "tfcp pointer not initialized");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}


	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		uint16_t pool_id;
		struct tfc_cmm *cmm;
		enum cfa_srch_mode srch_mode;

		rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, dir, &pi);
		if (rc)
			PMD_DRV_LOG_LINE(ERR, "pool info error(%s)", strerror(-rc));

		/* Clean up lkup cpm/cmm instances */
		srch_mode = CFA_SRCH_MODE_FIRST;
		do {
			rc = tfc_cpm_srchm_by_configured_pool(pi.lkup_cpm, srch_mode,
							      &pool_id, &cmm);
			srch_mode = CFA_SRCH_MODE_NEXT;

			if (rc == 0 && cmm) {
				PMD_DRV_LOG_LINE(DEBUG, "free lkup_%s CMM for pool(%d)",
						 dir == CFA_DIR_RX ? "rx" : "tx",
						 pool_id);
				cfa_mm_close(cmm);
				rte_free(cmm);
			}

		} while (!rc);

		tfc_cpm_close(pi.lkup_cpm);

		/* Clean up action cpm/cmm instances */
		srch_mode = CFA_SRCH_MODE_FIRST;
		do {
			uint16_t pool_id;
			struct tfc_cmm *cmm;

			rc = tfc_cpm_srchm_by_configured_pool(pi.act_cpm, srch_mode,
							      &pool_id, &cmm);
			srch_mode = CFA_SRCH_MODE_NEXT;

			if (rc == 0 && cmm) {
				PMD_DRV_LOG_LINE(DEBUG, "free act_%s CMM for pool(%d)",
						 dir == CFA_DIR_RX ? "rx" : "tx",
						 pool_id);
				cfa_mm_close(cmm);
				rte_free(cmm);
			}

		} while (!rc);

		tfc_cpm_close(pi.act_cpm);

		rc = tfo_ts_set_cpm_inst(tfcp->tfo, tsid, dir, NULL, NULL);
		if (rc)
			PMD_DRV_LOG_LINE(ERR, "cpm inst error(%s)", strerror(-rc));

		pi.lkup_cpm = NULL;
		pi.act_cpm = NULL;
		rc = tfo_ts_set_pool_info(tfcp->tfo, tsid, dir, &pi);
		if (rc)
			PMD_DRV_LOG_LINE(ERR, "pool info error(%s)", strerror(-rc));
	}

	return rc;
}


int tfc_tbl_scope_pool_alloc(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			     enum cfa_region_type region, enum cfa_dir dir,
			     uint8_t *pool_sz_exp, uint16_t *pool_id)
{
	int rc = 0;
	void *tim;
	void *tpm;
	bool is_pf;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (pool_id == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid pool_id pointer");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get PF status");
		return -EINVAL;
	}

	if (is_pf) {
		rc = tfo_tim_get(tfcp->tfo, &tim);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR, "Failed to get TIM");
			return -EINVAL;
		}

		rc = cfa_tim_tpm_inst_get(tim,
					  tsid,
					  region,
					  dir,
					  &tpm);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR,
					 "Failed to get TPM for tsid:%d region:%d dir:%d",
					 tsid, region, dir);
			return -EINVAL;
		}

		rc = cfa_tpm_alloc(tpm, pool_id);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR, "Failed allocate pool_id %s", strerror(-rc));
			return rc;
		}

		if (pool_sz_exp) {
			rc = cfa_tpm_pool_size_get(tpm, pool_sz_exp);
			if (rc) {
				PMD_DRV_LOG_LINE(ERR, "Failed get pool size exp %s",
						 strerror(-rc));
				return rc;
			}
		}

		rc = cfa_tpm_fid_add(tpm, *pool_id, fid);
		if (rc) {
			PMD_DRV_LOG_LINE(ERR, "Failed to set pool_id %d fid 0x%x %s",
					 *pool_id, fid, strerror(-rc));
			return rc;
		}

	} else { /* !PF */
		struct tfc_vf2pf_tbl_scope_pool_alloc_cmd req = { { 0 } };
		struct tfc_vf2pf_tbl_scope_pool_alloc_resp resp = { { 0 } };
		uint16_t fid;

		rc = tfc_get_fid(tfcp, &fid);
		if (rc)
			return rc;

		req.hdr.type = TFC_VF2PF_TYPE_TBL_SCOPE_POOL_ALLOC_CMD;
		req.hdr.fid = fid;
		req.tsid = tsid;
		req.dir = dir;
		req.region = region;

		/* Send message to PF to allocate pool */
		rc = tfc_vf2pf_pool_alloc(tfcp, &req, &resp);
		if (rc != 0) {
			PMD_DRV_LOG_LINE(ERR, "tfc_vf2pf_pool_alloc failed");
			return rc;
		}
		*pool_id = resp.pool_id;
		if (pool_sz_exp)
			*pool_sz_exp = resp.pool_sz_exp;
	}
	return rc;
}

int tfc_tbl_scope_pool_free(struct tfc *tfcp, uint16_t fid, uint8_t tsid,
			    enum cfa_region_type region, enum cfa_dir dir,
			    uint16_t pool_id)
{
	int rc = 0;
	void *tim;
	void *tpm;
	bool is_pf;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}

	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get PF status");
		return -EINVAL;
	}

	if (is_pf) {
		rc = tfo_tim_get(tfcp->tfo, &tim);
		if (rc)
			return -EINVAL;

		rc = cfa_tim_tpm_inst_get(tim,
					  tsid,
					  region,
					  dir,
					  &tpm);
		if (rc)
			return -EINVAL;

		rc = cfa_tpm_fid_rem(tpm, pool_id, fid);
		if (rc)
			return -EINVAL;

		rc = cfa_tpm_free(tpm, pool_id);

		return rc;
	}
	/* Pools are currently only deleted on the VF when the
	 * VF calls tfc_tbl_scope_mem_free() if shared.
	 */
	return rc;
}


int tfc_tbl_scope_config_state_get(struct tfc *tfcp,
				   uint8_t tsid,
				   bool *configured)
{
	int rc = 0;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}

	if (tfo_ts_validate(tfcp->tfo, tsid, NULL) != 0) {
		PMD_DRV_LOG_LINE(ERR, "tsid(%d) invalid", tsid);
		return -EINVAL;
	}

	rc =  tfc_msg_tbl_scope_config_get(tfcp, tsid, configured);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "message failed %s", strerror(-rc));
		return rc;
	}

	return rc;
}

static void tfc_tbl_scope_delete_by_pool(uint16_t *found_cnt,
					 struct tfc *tfcp,
					 uint16_t fid,
					 enum cfa_region_type region,
					 uint8_t tsid,
					 enum cfa_dir dir,
					 uint16_t pool_id,
					 uint8_t *data,
					 void *tpm)
{
	uint16_t fc = 0;
	int rc;

	do {
		fc++;

		if (region == CFA_REGION_TYPE_LKUP) {
			/*
			 * Flush EM entries associated with this TS.
			 */
			rc = tfc_em_delete_entries_by_pool_id(tfcp,
							      tsid,
							      dir,
							      pool_id,
							      0,
							      data);

			if (rc)
				PMD_DRV_LOG_LINE(ERR,
				 "delete_em_entries_by_pool_id()  failed for TS:%d  Dir:%d pool:%d",
				 tsid,  dir, pool_id);
		}

		/* Remove fid from pool */
		rc = cfa_tpm_fid_rem(tpm, pool_id, fid);
		if (rc)
			PMD_DRV_LOG_LINE(ERR,
					 "cfa_tpm_fid_rem() failed for fid:%d pool:%d",
					 fid, pool_id);

		/* Next! */
		rc = cfa_tpm_srchm_by_fid(tpm,
					  CFA_SRCH_MODE_NEXT,
					  fid,
					  &pool_id);
	} while (!rc);

	*found_cnt = fc;
}

int tfc_tbl_scope_func_reset(struct tfc *tfcp, uint16_t fid)
{
	int rc = 0;
	bool shared;
	enum cfa_app_type app;
	bool valid;
	uint8_t tsid;
	enum cfa_dir dir;
	uint16_t pool_id;
	uint16_t found_cnt = 0;
	void *tim;
	void *tpm;
	enum cfa_region_type region;
	bool is_pf;
	uint8_t *data;

	if (tfcp == NULL) {
		PMD_DRV_LOG_LINE(ERR, "Invalid tfcp pointer");
		return -EINVAL;
	}
	rc = tfc_bp_is_pf(tfcp, &is_pf);
	if (rc)
		return rc;

	if (!is_pf) {
		PMD_DRV_LOG_LINE(ERR, "only valid for PF");
		return -EINVAL;
	}

	rc = tfo_tim_get(tfcp->tfo, &tim);
	if (rc) {
		PMD_DRV_LOG_LINE(ERR, "Failed to get TIM");
		return -EINVAL;
	}

	data = rte_zmalloc("data", 32 * TFC_MPC_BYTES_PER_WORD, 32);

	for (tsid = 1; tsid < TFC_TBL_SCOPE_MAX; tsid++) {
		rc = tfo_ts_get(tfcp->tfo, tsid, &shared, &app, &valid, NULL);
		if (rc)
			continue; /* TS is not used, move on to the next */

		if (!shared || !valid)
			continue; /* TS invalid or not shared, move on */

		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				/*
				 * Get the TPM and then check to see if the fid is associated
				 * with any of the pools
				 */
				rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
				if (rc) {
					PMD_DRV_LOG_LINE(ERR,
							 "Failed to get TPM for tsid:%d dir:%d",
							 tsid, dir);
					rte_free(data);
					return -EINVAL;
				}

				rc = cfa_tpm_srchm_by_fid(tpm, CFA_SRCH_MODE_FIRST, fid, &pool_id);
				if (rc) /* FID not used */
					continue;

				tfc_tbl_scope_delete_by_pool(&found_cnt,
							     tfcp,
							     fid,
							     region,
							     tsid,
							     dir,
							     pool_id,
							     data,
							     tpm);
			}
		}
	}

	rte_free(data);

	if (found_cnt == 0)
		PMD_DRV_LOG_LINE(ERR, "FID:%d is not associated with any pool", fid);

	return 0;
}
