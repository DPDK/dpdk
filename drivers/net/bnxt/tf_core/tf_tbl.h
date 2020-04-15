/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_TBL_H_
#define _TF_TBL_H_

#include <stdint.h>

enum tf_pg_tbl_lvl {
	PT_LVL_0,
	PT_LVL_1,
	PT_LVL_2,
	PT_LVL_MAX
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
	uint32_t              *ext_pool_mem[TF_DIR_MAX][TF_EXT_POOL_CNT_MAX];
};

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
