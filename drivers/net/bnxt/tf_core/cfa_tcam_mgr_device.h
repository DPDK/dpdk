/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#ifndef CFA_TCAM_MGR_DEVICE_H
#define CFA_TCAM_MGR_DEVICE_H

#include <inttypes.h>
#include "cfa_tcam_mgr.h"

/*
 * This identifier is to be used for one-off variable sizes.  Do not use it for
 * sizing keys in an array.
 */
#define CFA_TCAM_MGR_MAX_KEY_SIZE 96

/* Note that this macro's arguments are not macro expanded due to
 * concatenation.
 */
#define TF_TCAM_TABLE_ROWS_DEF(_slices)					\
	struct cfa_tcam_mgr_table_rows_ ## _slices {			\
		uint16_t priority;					\
		uint8_t entry_size;		/* Slices per entry */	\
		uint8_t entry_inuse;	        /* bit[entry] set if in use */ \
		uint16_t entries[_slices];				\
	}

/*
 * Have to explicitly declare this struct since some compilers don't accept the
 * GNU C extension of zero length arrays.
 */
struct cfa_tcam_mgr_table_rows_0 {
	uint16_t priority;
	uint8_t entry_size;		/* Slices per entry */
	uint8_t entry_inuse;	        /* bit[entry] set if in use */
	uint16_t entries[];
};

TF_TCAM_TABLE_ROWS_DEF(1);
TF_TCAM_TABLE_ROWS_DEF(2);
TF_TCAM_TABLE_ROWS_DEF(4);
TF_TCAM_TABLE_ROWS_DEF(8);

#define TF_TCAM_MAX_ENTRIES (L2_CTXT_TCAM_RX_MAX_ENTRIES +	\
			     L2_CTXT_TCAM_TX_MAX_ENTRIES +	\
			     PROF_TCAM_RX_MAX_ENTRIES +		\
			     PROF_TCAM_TX_MAX_ENTRIES +		\
			     WC_TCAM_RX_MAX_ENTRIES +		\
			     WC_TCAM_TX_MAX_ENTRIES +		\
			     SP_TCAM_RX_MAX_ENTRIES +		\
			     SP_TCAM_TX_MAX_ENTRIES +		\
			     CT_RULE_TCAM_RX_MAX_ENTRIES +	\
			     CT_RULE_TCAM_TX_MAX_ENTRIES +	\
			     VEB_TCAM_RX_MAX_ENTRIES +		\
			     VEB_TCAM_TX_MAX_ENTRIES)

struct cfa_tcam_mgr_entry_data {
	uint16_t row;
	uint8_t slice;
	uint8_t ref_cnt;
};

struct cfa_tcam_mgr_table_data {
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	uint16_t hcapi_type;
	uint16_t num_rows;		/* Rows in physical TCAM */
	uint16_t start_row;		/* Where the logical TCAM starts */
	uint16_t end_row;		/* Where the logical TCAM ends */
	uint16_t max_entries;
	uint16_t used_entries;
	uint8_t  row_width;		/* bytes */
	uint8_t  result_size;		/* bytes */
	uint8_t  max_slices;
};

extern int cfa_tcam_mgr_max_entries[TF_TCAM_MAX_SESSIONS];

extern struct cfa_tcam_mgr_table_data
cfa_tcam_mgr_tables[TF_TCAM_MAX_SESSIONS][TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];

/* HW OP definitions begin here */
typedef int (*cfa_tcam_mgr_hwop_set_func_t)(int sess_idx,
					    struct cfa_tcam_mgr_set_parms
					    *parms, int row, int slice,
					    int max_slices);
typedef int (*cfa_tcam_mgr_hwop_get_func_t)(int sess_idx,
					    struct cfa_tcam_mgr_get_parms
					    *parms, int row, int slice,
					    int max_slices);
typedef int (*cfa_tcam_mgr_hwop_free_func_t)(int sess_idx,
					     struct cfa_tcam_mgr_free_parms
					     *parms, int row, int slice,
					     int max_slices);

struct cfa_tcam_mgr_hwops_funcs {
	cfa_tcam_mgr_hwop_set_func_t set;
	cfa_tcam_mgr_hwop_get_func_t get;
	cfa_tcam_mgr_hwop_free_func_t free;
};
#endif /* CFA_TCAM_MGR_DEVICE_H */
