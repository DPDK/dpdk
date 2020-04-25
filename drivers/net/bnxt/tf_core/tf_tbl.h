/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_TBL_H_
#define _TF_TBL_H_

#include <stdint.h>
#include "stack.h"

enum tf_pg_tbl_lvl {
	PT_LVL_0,
	PT_LVL_1,
	PT_LVL_2,
	PT_LVL_MAX
};

enum tf_em_table_type {
	KEY0_TABLE,
	KEY1_TABLE,
	RECORD_TABLE,
	EFC_TABLE,
	MAX_TABLE
};

struct tf_em_page_tbl {
	uint32_t	pg_count;
	uint32_t	pg_size;
	void		**pg_va_tbl;
	uint64_t	*pg_pa_tbl;
};

struct tf_em_table {
	int				type;
	uint32_t			num_entries;
	uint16_t			ctx_id;
	uint32_t			entry_size;
	int				num_lvl;
	uint32_t			page_cnt[PT_LVL_MAX];
	uint64_t			num_data_pages;
	void				*l0_addr;
	uint64_t			l0_dma_addr;
	struct tf_em_page_tbl pg_tbl[PT_LVL_MAX];
};

struct tf_em_ctx_mem_info {
	struct tf_em_table		em_tables[MAX_TABLE];
};

/** table scope control block content */
struct tf_em_caps {
	uint32_t flags;
	uint32_t supported;
	uint32_t max_entries_supported;
	uint16_t key_entry_size;
	uint16_t record_entry_size;
	uint16_t efc_entry_size;
};

/** Invalid table scope id */
#define TF_TBL_SCOPE_INVALID 0xffffffff

/**
 * Table Scope Control Block
 *
 * Holds private data for a table scope. Only one instance of a table
 * scope with Internal EM is supported.
 */
struct tf_tbl_scope_cb {
	uint32_t tbl_scope_id;
	int index;
	struct tf_em_ctx_mem_info  em_ctx_info[TF_DIR_MAX];
	struct tf_em_caps          em_caps[TF_DIR_MAX];
	struct stack               ext_act_pool[TF_DIR_MAX];
	uint32_t                  *ext_act_pool_mem[TF_DIR_MAX];
};

/** Hardware Page sizes supported for EEM: 4K, 8K, 64K, 256K, 1M, 2M, 4M, 1G.
 * Round-down other page sizes to the lower hardware page size supported.
 */
#define BNXT_PAGE_SHIFT 22 /** 2M */

#if (BNXT_PAGE_SHIFT < 12)				/** < 4K >> 4K */
#define TF_EM_PAGE_SHIFT 12
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_4K
#elif (BNXT_PAGE_SHIFT <= 13)			/** 4K, 8K */
#define TF_EM_PAGE_SHIFT 13
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_8K
#elif (BNXT_PAGE_SHIFT < 16)				/** 16K, 32K >> 8K */
#define TF_EM_PAGE_SHIFT 15
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_32K
#elif (BNXT_PAGE_SHIFT <= 17)			/** 64K, 128K >> 64K */
#define TF_EM_PAGE_SHIFT 16
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_64K
#elif (BNXT_PAGE_SHIFT <= 19)			/** 256K, 512K >> 256K */
#define TF_EM_PAGE_SHIFT 18
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_256K
#elif (BNXT_PAGE_SHIFT <= 21)			/** 1M */
#define TF_EM_PAGE_SHIFT 20
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_1M
#elif (BNXT_PAGE_SHIFT <= 22)			/** 2M, 4M */
#define TF_EM_PAGE_SHIFT 21
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_2M
#elif (BNXT_PAGE_SHIFT <= 29)			/** 8M ... 512M >> 4M */
#define TF_EM_PAGE_SHIFT 22
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_4M
#else						/** >= 1G >> 1G */
#define TF_EM_PAGE_SHIFT	30
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_1G
#endif

#define TF_EM_PAGE_SIZE	(1 << TF_EM_PAGE_SHIFT)
#define TF_EM_PAGE_ALIGNMENT (1 << TF_EM_PAGE_SHIFT)

/**
 * Initialize table pool structure to indicate
 * no table scope has been associated with the
 * external pool of indexes.
 *
 * [in] session
 */
void
tf_init_tbl_pool(struct tf_session *session);

#endif /* _TF_TBL_H_ */
