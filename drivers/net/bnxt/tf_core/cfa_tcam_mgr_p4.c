/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#include "hcapi_cfa_defs.h"

#include "cfa_tcam_mgr.h"
#include "cfa_tcam_mgr_p4.h"
#include "cfa_tcam_mgr_device.h"
#include "cfa_resource_types.h"
#include "tfp.h"
#include "assert.h"
#include "tf_util.h"

/*
 * Sizings of the TCAMs on P4
 */

#define MAX_ROW_WIDTH    48
#define MAX_RESULT_SIZE  8

#if MAX_ROW_WIDTH > CFA_TCAM_MGR_MAX_KEY_SIZE
#error MAX_ROW_WIDTH > CFA_TCAM_MGR_MAX_KEY_SIZE
#endif

/*
 * TCAM definitions
 *
 * These define the TCAMs in HW.
 *
 * Note: Set xxx_TCAM_[R|T]X_NUM_ROWS to zero if a TCAM is either not supported
 * by HW or not supported by TCAM Manager.
 */

/** L2 Context TCAM */
#define L2_CTXT_TCAM_RX_MAX_SLICES  1
#define L2_CTXT_TCAM_RX_ROW_WIDTH   TF_BITS2BYTES_WORD_ALIGN(167)
#define L2_CTXT_TCAM_RX_NUM_ROWS    1024
#define L2_CTXT_TCAM_RX_MAX_ENTRIES (L2_CTXT_TCAM_RX_MAX_SLICES * \
				     L2_CTXT_TCAM_RX_NUM_ROWS)
#define L2_CTXT_TCAM_RX_RESULT_SIZE 8

#define L2_CTXT_TCAM_TX_MAX_SLICES  L2_CTXT_TCAM_RX_MAX_SLICES
#define L2_CTXT_TCAM_TX_ROW_WIDTH   L2_CTXT_TCAM_RX_ROW_WIDTH
#define L2_CTXT_TCAM_TX_NUM_ROWS    L2_CTXT_TCAM_RX_NUM_ROWS
#define L2_CTXT_TCAM_TX_MAX_ENTRIES L2_CTXT_TCAM_RX_MAX_ENTRIES
#define L2_CTXT_TCAM_TX_RESULT_SIZE L2_CTXT_TCAM_RX_RESULT_SIZE

/** Profile TCAM */
#define PROF_TCAM_RX_MAX_SLICES  1
#define PROF_TCAM_RX_ROW_WIDTH   TF_BITS2BYTES_WORD_ALIGN(81)
#define PROF_TCAM_RX_NUM_ROWS    1024
#define PROF_TCAM_RX_MAX_ENTRIES (PROF_TCAM_RX_MAX_SLICES * \
				  PROF_TCAM_RX_NUM_ROWS)
#define PROF_TCAM_RX_RESULT_SIZE 8

#define PROF_TCAM_TX_MAX_SLICES  PROF_TCAM_RX_MAX_SLICES
#define PROF_TCAM_TX_ROW_WIDTH   PROF_TCAM_RX_ROW_WIDTH
#define PROF_TCAM_TX_NUM_ROWS    PROF_TCAM_RX_NUM_ROWS
#define PROF_TCAM_TX_MAX_ENTRIES PROF_TCAM_RX_MAX_ENTRIES
#define PROF_TCAM_TX_RESULT_SIZE PROF_TCAM_RX_RESULT_SIZE

/** Wildcard TCAM */
#define WC_TCAM_RX_MAX_SLICES  4
/* 82 bits per slice */
#define WC_TCAM_RX_ROW_WIDTH   (TF_BITS2BYTES_WORD_ALIGN(82) *	\
				WC_TCAM_RX_MAX_SLICES)
#define WC_TCAM_RX_NUM_ROWS    256
#define WC_TCAM_RX_MAX_ENTRIES (WC_TCAM_RX_MAX_SLICES * WC_TCAM_RX_NUM_ROWS)
#define WC_TCAM_RX_RESULT_SIZE 4

#define WC_TCAM_TX_MAX_SLICES  WC_TCAM_RX_MAX_SLICES
#define WC_TCAM_TX_ROW_WIDTH   WC_TCAM_RX_ROW_WIDTH
#define WC_TCAM_TX_NUM_ROWS    WC_TCAM_RX_NUM_ROWS
#define WC_TCAM_TX_MAX_ENTRIES WC_TCAM_RX_MAX_ENTRIES
#define WC_TCAM_TX_RESULT_SIZE WC_TCAM_RX_RESULT_SIZE

/** Source Properties TCAM */
#define SP_TCAM_RX_MAX_SLICES  1
#define SP_TCAM_RX_ROW_WIDTH   TF_BITS2BYTES_WORD_ALIGN(89)
#define SP_TCAM_RX_NUM_ROWS    512
#define SP_TCAM_RX_MAX_ENTRIES (SP_TCAM_RX_MAX_SLICES * SP_TCAM_RX_NUM_ROWS)
#define SP_TCAM_RX_RESULT_SIZE 8

#define SP_TCAM_TX_MAX_SLICES  SP_TCAM_RX_MAX_SLICES
#define SP_TCAM_TX_ROW_WIDTH   SP_TCAM_RX_ROW_WIDTH
#define SP_TCAM_TX_NUM_ROWS    SP_TCAM_RX_NUM_ROWS
#define SP_TCAM_TX_MAX_ENTRIES SP_TCAM_RX_MAX_ENTRIES
#define SP_TCAM_TX_RESULT_SIZE SP_TCAM_RX_RESULT_SIZE

/** Connection Tracking Rule TCAM */
#define CT_RULE_TCAM_RX_MAX_SLICES  1
#define CT_RULE_TCAM_RX_ROW_WIDTH   TF_BITS2BYTES_WORD_ALIGN(16)
#define CT_RULE_TCAM_RX_NUM_ROWS    0
#define CT_RULE_TCAM_RX_MAX_ENTRIES (CT_RULE_TCAM_RX_MAX_SLICES * \
				     CT_RULE_TCAM_RX_NUM_ROWS)
#define CT_RULE_TCAM_RX_RESULT_SIZE 8

#define CT_RULE_TCAM_TX_MAX_SLICES  CT_RULE_TCAM_RX_MAX_SLICES
#define CT_RULE_TCAM_TX_ROW_WIDTH   CT_RULE_TCAM_RX_ROW_WIDTH
#define CT_RULE_TCAM_TX_NUM_ROWS    CT_RULE_TCAM_RX_NUM_ROWS
#define CT_RULE_TCAM_TX_MAX_ENTRIES CT_RULE_TCAM_RX_MAX_ENTRIES
#define CT_RULE_TCAM_TX_RESULT_SIZE CT_RULE_TCAM_RX_RESULT_SIZE

/** Virtual Edge Bridge TCAM */
#define VEB_TCAM_RX_MAX_SLICES  1
#define VEB_TCAM_RX_ROW_WIDTH   TF_BITS2BYTES_WORD_ALIGN(78)
/* Tx only */
#define VEB_TCAM_RX_NUM_ROWS    0
#define VEB_TCAM_RX_MAX_ENTRIES (VEB_TCAM_RX_MAX_SLICES * VEB_TCAM_RX_NUM_ROWS)
#define VEB_TCAM_RX_RESULT_SIZE 8

#define VEB_TCAM_TX_MAX_SLICES  VEB_TCAM_RX_MAX_SLICES
#define VEB_TCAM_TX_ROW_WIDTH   VEB_TCAM_RX_ROW_WIDTH
#define VEB_TCAM_TX_NUM_ROWS    1024
#define VEB_TCAM_TX_MAX_ENTRIES (VEB_TCAM_TX_MAX_SLICES * VEB_TCAM_TX_NUM_ROWS)
#define VEB_TCAM_TX_RESULT_SIZE VEB_TCAM_RX_RESULT_SIZE

/* Declare the table rows for each table here.  If new tables are added to the
 * enum tf_tcam_tbl_type, then new declarations will be needed here.
 *
 * The numeric suffix of the structure type indicates how many slices a
 * particular TCAM supports.
 *
 * Array sizes have 1 added to avoid zero length arrays.
 */

static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_RX[TF_TCAM_MAX_SESSIONS][L2_CTXT_TCAM_RX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_TX[TF_TCAM_MAX_SESSIONS][L2_CTXT_TCAM_TX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_PROF_TCAM_RX[TF_TCAM_MAX_SESSIONS][PROF_TCAM_RX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_PROF_TCAM_TX[TF_TCAM_MAX_SESSIONS][PROF_TCAM_TX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_4
	cfa_tcam_mgr_table_rows_WC_TCAM_RX[TF_TCAM_MAX_SESSIONS][WC_TCAM_RX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_4
	cfa_tcam_mgr_table_rows_WC_TCAM_TX[TF_TCAM_MAX_SESSIONS][WC_TCAM_TX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_SP_TCAM_RX[TF_TCAM_MAX_SESSIONS][SP_TCAM_RX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_SP_TCAM_TX[TF_TCAM_MAX_SESSIONS][SP_TCAM_TX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_CT_RULE_TCAM_RX[TF_TCAM_MAX_SESSIONS][CT_RULE_TCAM_RX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_CT_RULE_TCAM_TX[TF_TCAM_MAX_SESSIONS][CT_RULE_TCAM_TX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_VEB_TCAM_RX[TF_TCAM_MAX_SESSIONS][VEB_TCAM_RX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_1
	cfa_tcam_mgr_table_rows_VEB_TCAM_TX[TF_TCAM_MAX_SESSIONS][VEB_TCAM_TX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_4
	cfa_tcam_mgr_table_rows_WC_TCAM_RX_HIGH[TF_TCAM_MAX_SESSIONS][WC_TCAM_RX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_4
	cfa_tcam_mgr_table_rows_WC_TCAM_RX_LOW[TF_TCAM_MAX_SESSIONS][WC_TCAM_RX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_4
	cfa_tcam_mgr_table_rows_WC_TCAM_TX_HIGH[TF_TCAM_MAX_SESSIONS][WC_TCAM_TX_NUM_ROWS + 1];
static struct cfa_tcam_mgr_table_rows_4
	cfa_tcam_mgr_table_rows_WC_TCAM_TX_LOW[TF_TCAM_MAX_SESSIONS][WC_TCAM_TX_NUM_ROWS + 1];

struct cfa_tcam_mgr_table_data
cfa_tcam_mgr_tables_p4[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX] = {
	{				/* RX */
		{			/* High AFM */
			.max_slices  = L2_CTXT_TCAM_RX_MAX_SLICES,
			.row_width   = L2_CTXT_TCAM_RX_ROW_WIDTH,
			.num_rows    = L2_CTXT_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = L2_CTXT_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_HIGH,
		},
		{			/* High APPS */
			.max_slices  = L2_CTXT_TCAM_RX_MAX_SLICES,
			.row_width   = L2_CTXT_TCAM_RX_ROW_WIDTH,
			.num_rows    = L2_CTXT_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = (L2_CTXT_TCAM_RX_NUM_ROWS / 2) - 1,
			.max_entries = (L2_CTXT_TCAM_RX_MAX_ENTRIES / 2),
			.result_size = L2_CTXT_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_HIGH,
		},
		{			/* Low AFM */
			.max_slices  = L2_CTXT_TCAM_RX_MAX_SLICES,
			.row_width   = L2_CTXT_TCAM_RX_ROW_WIDTH,
			.num_rows    = L2_CTXT_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = L2_CTXT_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_LOW,
		},
		{			/* Low APPS */
			.max_slices  = L2_CTXT_TCAM_RX_MAX_SLICES,
			.row_width   = L2_CTXT_TCAM_RX_ROW_WIDTH,
			.num_rows    = L2_CTXT_TCAM_RX_NUM_ROWS,
			.start_row   = (L2_CTXT_TCAM_RX_NUM_ROWS / 2),
			.end_row     = L2_CTXT_TCAM_RX_NUM_ROWS - 1,
			.max_entries = (L2_CTXT_TCAM_RX_MAX_ENTRIES / 2),
			.result_size = L2_CTXT_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_LOW,
		},
		{			/* AFM */
			.max_slices  = PROF_TCAM_RX_MAX_SLICES,
			.row_width   = PROF_TCAM_RX_ROW_WIDTH,
			.num_rows    = PROF_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = PROF_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_PROF_TCAM,
		},
		{			/* APPS */
			.max_slices  = PROF_TCAM_RX_MAX_SLICES,
			.row_width   = PROF_TCAM_RX_ROW_WIDTH,
			.num_rows    = PROF_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = PROF_TCAM_RX_NUM_ROWS - 1,
			.max_entries = PROF_TCAM_RX_MAX_ENTRIES,
			.result_size = PROF_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_PROF_TCAM,
		},
		{			/* AFM */
			.max_slices  = WC_TCAM_RX_MAX_SLICES,
			.row_width   = WC_TCAM_RX_ROW_WIDTH,
			.num_rows    = WC_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = WC_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* APPS */
			.max_slices  = WC_TCAM_RX_MAX_SLICES,
			.row_width   = WC_TCAM_RX_ROW_WIDTH,
			.num_rows    = WC_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = WC_TCAM_RX_NUM_ROWS - 1,
			.max_entries = WC_TCAM_RX_MAX_ENTRIES,
			.result_size = WC_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* AFM */
			.max_slices  = SP_TCAM_RX_MAX_SLICES,
			.row_width   = SP_TCAM_RX_ROW_WIDTH,
			.num_rows    = SP_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = SP_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_SP_TCAM,
		},
		{			/* APPS */
			.max_slices  = SP_TCAM_RX_MAX_SLICES,
			.row_width   = SP_TCAM_RX_ROW_WIDTH,
			.num_rows    = SP_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = SP_TCAM_RX_NUM_ROWS - 1,
			.max_entries = SP_TCAM_RX_MAX_ENTRIES,
			.result_size = SP_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_SP_TCAM,
		},
		{			/* AFM */
			.max_slices  = CT_RULE_TCAM_RX_MAX_SLICES,
			.row_width   = CT_RULE_TCAM_RX_ROW_WIDTH,
			.num_rows    = CT_RULE_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = CT_RULE_TCAM_RX_RESULT_SIZE,
		},
		{			/* APPS */
			.max_slices  = CT_RULE_TCAM_RX_MAX_SLICES,
			.row_width   = CT_RULE_TCAM_RX_ROW_WIDTH,
			.num_rows    = CT_RULE_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
#if CT_RULE_TCAM_RX_NUM_ROWS > 0
			.end_row     = CT_RULE_TCAM_RX_NUM_ROWS - 1,
#else
			.end_row     = CT_RULE_TCAM_RX_NUM_ROWS,
#endif
			.max_entries = CT_RULE_TCAM_RX_MAX_ENTRIES,
			.result_size = CT_RULE_TCAM_RX_RESULT_SIZE,
		},
		{			/* AFM */
			.max_slices  = VEB_TCAM_RX_MAX_SLICES,
			.row_width   = VEB_TCAM_RX_ROW_WIDTH,
			.num_rows    = VEB_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = VEB_TCAM_RX_RESULT_SIZE,
		},
		{			/* APPS */
			.max_slices  = VEB_TCAM_RX_MAX_SLICES,
			.row_width   = VEB_TCAM_RX_ROW_WIDTH,
			.num_rows    = VEB_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
#if VEB_TCAM_RX_NUM_ROWS > 0
			.end_row     = VEB_TCAM_RX_NUM_ROWS - 1,
#else
			.end_row     = VEB_TCAM_RX_NUM_ROWS,
#endif
			.max_entries = VEB_TCAM_RX_MAX_ENTRIES,
			.result_size = VEB_TCAM_RX_RESULT_SIZE,
		},
		{			/* AFM */
			.max_slices  = WC_TCAM_RX_MAX_SLICES,
			.row_width   = WC_TCAM_RX_ROW_WIDTH,
			.num_rows    = WC_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = WC_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* APPS */
			.max_slices  = WC_TCAM_RX_MAX_SLICES,
			.row_width   = WC_TCAM_RX_ROW_WIDTH,
			.num_rows    = WC_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = WC_TCAM_RX_NUM_ROWS - 1,
			.max_entries = WC_TCAM_RX_MAX_ENTRIES,
			.result_size = WC_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* AFM */
			.max_slices  = WC_TCAM_RX_MAX_SLICES,
			.row_width   = WC_TCAM_RX_ROW_WIDTH,
			.num_rows    = WC_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = WC_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* APPS */
			.max_slices  = WC_TCAM_RX_MAX_SLICES,
			.row_width   = WC_TCAM_RX_ROW_WIDTH,
			.num_rows    = WC_TCAM_RX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = WC_TCAM_RX_NUM_ROWS - 1,
			.max_entries = WC_TCAM_RX_MAX_ENTRIES,
			.result_size = WC_TCAM_RX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
	},
	{				/* TX */
		{			/* AFM */
			.max_slices  = L2_CTXT_TCAM_TX_MAX_SLICES,
			.row_width   = L2_CTXT_TCAM_TX_ROW_WIDTH,
			.num_rows    = L2_CTXT_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = L2_CTXT_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_HIGH,
		},
		{			/* APPS */
			.max_slices  = L2_CTXT_TCAM_TX_MAX_SLICES,
			.row_width   = L2_CTXT_TCAM_TX_ROW_WIDTH,
			.num_rows    = L2_CTXT_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = (L2_CTXT_TCAM_TX_NUM_ROWS / 2) - 1,
			.max_entries = (L2_CTXT_TCAM_TX_MAX_ENTRIES / 2),
			.result_size = L2_CTXT_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_HIGH,
		},
		{			/* AFM */
			.max_slices  = L2_CTXT_TCAM_TX_MAX_SLICES,
			.row_width   = L2_CTXT_TCAM_TX_ROW_WIDTH,
			.num_rows    = L2_CTXT_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = L2_CTXT_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_LOW,
		},
		{			/* APPS */
			.max_slices  = L2_CTXT_TCAM_TX_MAX_SLICES,
			.row_width   = L2_CTXT_TCAM_TX_ROW_WIDTH,
			.num_rows    = L2_CTXT_TCAM_TX_NUM_ROWS,
			.start_row   = (L2_CTXT_TCAM_TX_NUM_ROWS / 2),
			.end_row     = L2_CTXT_TCAM_TX_NUM_ROWS - 1,
			.max_entries = (L2_CTXT_TCAM_TX_MAX_ENTRIES / 2),
			.result_size = L2_CTXT_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_L2_CTXT_TCAM_LOW,
		},
		{			/* AFM */
			.max_slices  = PROF_TCAM_TX_MAX_SLICES,
			.row_width   = PROF_TCAM_TX_ROW_WIDTH,
			.num_rows    = PROF_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = PROF_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_PROF_TCAM,
		},
		{			/* APPS */
			.max_slices  = PROF_TCAM_TX_MAX_SLICES,
			.row_width   = PROF_TCAM_TX_ROW_WIDTH,
			.num_rows    = PROF_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = PROF_TCAM_TX_NUM_ROWS - 1,
			.max_entries = PROF_TCAM_TX_MAX_ENTRIES,
			.result_size = PROF_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_PROF_TCAM,
		},
		{			/* AFM */
			.max_slices  = WC_TCAM_TX_MAX_SLICES,
			.row_width   = WC_TCAM_TX_ROW_WIDTH,
			.num_rows    = WC_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = WC_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* APPS */
			.max_slices  = WC_TCAM_TX_MAX_SLICES,
			.row_width   = WC_TCAM_TX_ROW_WIDTH,
			.num_rows    = WC_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = WC_TCAM_TX_NUM_ROWS - 1,
			.max_entries = WC_TCAM_TX_MAX_ENTRIES,
			.result_size = WC_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* AFM */
			.max_slices  = SP_TCAM_TX_MAX_SLICES,
			.row_width   = SP_TCAM_TX_ROW_WIDTH,
			.num_rows    = SP_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = SP_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_SP_TCAM,
		},
		{			/* APPS */
			.max_slices  = SP_TCAM_TX_MAX_SLICES,
			.row_width   = SP_TCAM_TX_ROW_WIDTH,
			.num_rows    = SP_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = SP_TCAM_TX_NUM_ROWS - 1,
			.max_entries = SP_TCAM_TX_MAX_ENTRIES,
			.result_size = SP_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_SP_TCAM,
		},
		{			/* AFM */
			.max_slices  = CT_RULE_TCAM_TX_MAX_SLICES,
			.row_width   = CT_RULE_TCAM_TX_ROW_WIDTH,
			.num_rows    = CT_RULE_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = CT_RULE_TCAM_RX_RESULT_SIZE,
		},
		{			/* APPS */
			.max_slices  = CT_RULE_TCAM_TX_MAX_SLICES,
			.row_width   = CT_RULE_TCAM_TX_ROW_WIDTH,
			.num_rows    = CT_RULE_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
#if CT_RULE_TCAM_TX_NUM_ROWS > 0
			.end_row     = CT_RULE_TCAM_TX_NUM_ROWS - 1,
#else
			.end_row     = CT_RULE_TCAM_TX_NUM_ROWS,
#endif
			.max_entries = CT_RULE_TCAM_TX_MAX_ENTRIES,
			.result_size = CT_RULE_TCAM_RX_RESULT_SIZE,
		},
		{			/* AFM */
			.max_slices  = VEB_TCAM_TX_MAX_SLICES,
			.row_width   = VEB_TCAM_TX_ROW_WIDTH,
			.num_rows    = VEB_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = VEB_TCAM_RX_RESULT_SIZE,
		},
		{			/* APPS */
			.max_slices  = VEB_TCAM_TX_MAX_SLICES,
			.row_width   = VEB_TCAM_TX_ROW_WIDTH,
			.num_rows    = VEB_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = VEB_TCAM_TX_NUM_ROWS - 1,
			.max_entries = VEB_TCAM_TX_MAX_ENTRIES,
			.result_size = VEB_TCAM_RX_RESULT_SIZE,
		},
		{			/* AFM */
			.max_slices  = WC_TCAM_TX_MAX_SLICES,
			.row_width   = WC_TCAM_TX_ROW_WIDTH,
			.num_rows    = WC_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = WC_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* APPS */
			.max_slices  = WC_TCAM_TX_MAX_SLICES,
			.row_width   = WC_TCAM_TX_ROW_WIDTH,
			.num_rows    = WC_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = WC_TCAM_TX_NUM_ROWS - 1,
			.max_entries = WC_TCAM_TX_MAX_ENTRIES,
			.result_size = WC_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* AFM */
			.max_slices  = WC_TCAM_TX_MAX_SLICES,
			.row_width   = WC_TCAM_TX_ROW_WIDTH,
			.num_rows    = WC_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = 0,
			.max_entries = 0,
			.result_size = WC_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
		{			/* APPS */
			.max_slices  = WC_TCAM_TX_MAX_SLICES,
			.row_width   = WC_TCAM_TX_ROW_WIDTH,
			.num_rows    = WC_TCAM_TX_NUM_ROWS,
			.start_row   = 0,
			.end_row     = WC_TCAM_TX_NUM_ROWS - 1,
			.max_entries = WC_TCAM_TX_MAX_ENTRIES,
			.result_size = WC_TCAM_TX_RESULT_SIZE,
			.hcapi_type  = CFA_RESOURCE_TYPE_P4_WC_TCAM,
		},
	},
};

static struct cfa_tcam_mgr_entry_data entry_data_p4[TF_TCAM_MAX_SESSIONS][TF_TCAM_MAX_ENTRIES];

static struct sbmp session_bmp_p4[TF_TCAM_MAX_SESSIONS][TF_TCAM_MAX_ENTRIES];

int
cfa_tcam_mgr_sess_table_get_p4(int sess_idx, struct sbmp **session_bmp)
{
	*session_bmp = session_bmp_p4[sess_idx];
	return 0;
}

int
cfa_tcam_mgr_init_p4(int sess_idx, struct cfa_tcam_mgr_entry_data **global_entry_data)
{
	int max_row_width = 0;
	int max_result_size = 0;
	int dir, type;

	*global_entry_data = entry_data_p4[sess_idx];

	memcpy(&cfa_tcam_mgr_tables[sess_idx],
	       &cfa_tcam_mgr_tables_p4,
	       sizeof(cfa_tcam_mgr_tables[sess_idx]));

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_RX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_RX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_TX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_TX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_RX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_RX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_TX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_L2_CTXT_TCAM_TX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_PROF_TCAM_RX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_PROF_TCAM_RX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_PROF_TCAM_TX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_PROF_TCAM_TX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_RX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_RX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_TX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_TX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_SP_TCAM_RX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_SP_TCAM_RX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_SP_TCAM_TX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_SP_TCAM_TX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_CT_RULE_TCAM_RX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_CT_RULE_TCAM_RX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_CT_RULE_TCAM_TX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_CT_RULE_TCAM_TX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_VEB_TCAM_RX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_VEB_TCAM_RX[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_VEB_TCAM_TX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_VEB_TCAM_TX[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_RX_HIGH[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_RX_HIGH[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_TX_HIGH[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_TX_HIGH[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_RX_LOW[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_RX_LOW[sess_idx];

	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_AFM].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_TX_LOW[sess_idx];
	cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX]
		[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS].tcam_rows =
		(struct cfa_tcam_mgr_table_rows_0 *)
		&cfa_tcam_mgr_table_rows_WC_TCAM_TX_LOW[sess_idx];

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		for (type = 0; type < CFA_TCAM_MGR_TBL_TYPE_MAX; type++) {
			if (cfa_tcam_mgr_tables[sess_idx][dir][type].row_width >
			    max_row_width)
				max_row_width =
				       cfa_tcam_mgr_tables[sess_idx][dir][type].row_width;
			if (cfa_tcam_mgr_tables[sess_idx][dir][type].result_size >
			    max_result_size)
				max_result_size =
				     cfa_tcam_mgr_tables[sess_idx][dir][type].result_size;
		}
	}

	if (max_row_width != MAX_ROW_WIDTH) {
		CFA_TCAM_MGR_LOG(ERR,
				 "MAX_ROW_WIDTH (%d) does not match actual "
				 "value (%d).\n",
				 MAX_ROW_WIDTH,
				 max_row_width);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}
	if (max_result_size != MAX_RESULT_SIZE) {
		CFA_TCAM_MGR_LOG(ERR,
				 "MAX_RESULT_SIZE (%d) does not match actual "
				 "value (%d).\n",
				 MAX_RESULT_SIZE,
				 max_result_size);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}
	return 0;
}

/* HW OP declarations begin here */
struct cfa_tcam_mgr_TCAM_row_data {
	int key_size;
	int result_size;
	uint8_t key[MAX_ROW_WIDTH];
	uint8_t mask[MAX_ROW_WIDTH];
	uint8_t result[MAX_RESULT_SIZE];
};

/* These macros are only needed to avoid exceeding 80 columns */
#define L2_CTXT_RX_MAX_ROWS \
	(L2_CTXT_TCAM_RX_MAX_SLICES * L2_CTXT_TCAM_RX_NUM_ROWS)
#define PROF_RX_MAX_ROWS    (PROF_TCAM_RX_MAX_SLICES * PROF_TCAM_RX_NUM_ROWS)
#define WC_RX_MAX_ROWS	    (WC_TCAM_RX_MAX_SLICES * WC_TCAM_RX_NUM_ROWS)
#define SP_RX_MAX_ROWS	    (SP_TCAM_RX_MAX_SLICES * SP_TCAM_RX_NUM_ROWS)
#define CT_RULE_RX_MAX_ROWS \
	(CT_RULE_TCAM_RX_MAX_SLICES * CT_RULE_TCAM_RX_NUM_ROWS)
#define VEB_RX_MAX_ROWS	    (VEB_TCAM_RX_MAX_SLICES * VEB_TCAM_RX_NUM_ROWS)

#define L2_CTXT_TX_MAX_ROWS \
	(L2_CTXT_TCAM_TX_MAX_SLICES * L2_CTXT_TCAM_TX_NUM_ROWS)
#define PROF_TX_MAX_ROWS    (PROF_TCAM_TX_MAX_SLICES * PROF_TCAM_TX_NUM_ROWS)
#define WC_TX_MAX_ROWS	    (WC_TCAM_TX_MAX_SLICES * WC_TCAM_TX_NUM_ROWS)
#define SP_TX_MAX_ROWS	    (SP_TCAM_TX_MAX_SLICES * SP_TCAM_TX_NUM_ROWS)
#define CT_RULE_TX_MAX_ROWS \
	(CT_RULE_TCAM_TX_MAX_SLICES * CT_RULE_TCAM_TX_NUM_ROWS)
#define VEB_TX_MAX_ROWS	    (VEB_TCAM_TX_MAX_SLICES * VEB_TCAM_TX_NUM_ROWS)

static int cfa_tcam_mgr_max_rows[TF_TCAM_TBL_TYPE_MAX] = {
	L2_CTXT_RX_MAX_ROWS,
	L2_CTXT_RX_MAX_ROWS,
	PROF_RX_MAX_ROWS,
	WC_RX_MAX_ROWS,
	SP_RX_MAX_ROWS,
	CT_RULE_RX_MAX_ROWS,
	VEB_RX_MAX_ROWS,
	WC_RX_MAX_ROWS,
	WC_RX_MAX_ROWS
};
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_L2_CTXT_TCAM_RX_row_data[TF_TCAM_MAX_SESSIONS][L2_CTXT_RX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_PROF_TCAM_RX_row_data[TF_TCAM_MAX_SESSIONS][PROF_RX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_WC_TCAM_RX_row_data[TF_TCAM_MAX_SESSIONS][WC_RX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_SP_TCAM_RX_row_data[TF_TCAM_MAX_SESSIONS][SP_RX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_CT_RULE_TCAM_RX_row_data[TF_TCAM_MAX_SESSIONS][CT_RULE_RX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_VEB_TCAM_RX_row_data[TF_TCAM_MAX_SESSIONS][VEB_RX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_WC_TCAM_RX_row_data[TF_TCAM_MAX_SESSIONS][WC_RX_MAX_ROWS];

static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_L2_CTXT_TCAM_TX_row_data[TF_TCAM_MAX_SESSIONS][L2_CTXT_TX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_PROF_TCAM_TX_row_data[TF_TCAM_MAX_SESSIONS][PROF_TX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_WC_TCAM_TX_row_data[TF_TCAM_MAX_SESSIONS][WC_TX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_SP_TCAM_TX_row_data[TF_TCAM_MAX_SESSIONS][SP_TX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_CT_RULE_TCAM_TX_row_data[TF_TCAM_MAX_SESSIONS][CT_RULE_TX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_VEB_TCAM_TX_row_data[TF_TCAM_MAX_SESSIONS][VEB_TX_MAX_ROWS];
static struct cfa_tcam_mgr_TCAM_row_data
	cfa_tcam_mgr_WC_TCAM_TX_row_data[TF_TCAM_MAX_SESSIONS][WC_TX_MAX_ROWS];

static struct cfa_tcam_mgr_TCAM_row_data *
row_tables[TF_DIR_MAX][TF_TCAM_TBL_TYPE_MAX] = {
	{
		cfa_tcam_mgr_L2_CTXT_TCAM_RX_row_data[0],
		cfa_tcam_mgr_L2_CTXT_TCAM_RX_row_data[0],
		cfa_tcam_mgr_PROF_TCAM_RX_row_data[0],
		cfa_tcam_mgr_WC_TCAM_RX_row_data[0],
		cfa_tcam_mgr_SP_TCAM_RX_row_data[0],
		cfa_tcam_mgr_CT_RULE_TCAM_RX_row_data[0],
		cfa_tcam_mgr_VEB_TCAM_RX_row_data[0],
		cfa_tcam_mgr_WC_TCAM_RX_row_data[0],
		cfa_tcam_mgr_WC_TCAM_RX_row_data[0],
	},
	{
		cfa_tcam_mgr_L2_CTXT_TCAM_TX_row_data[0],
		cfa_tcam_mgr_L2_CTXT_TCAM_TX_row_data[0],
		cfa_tcam_mgr_PROF_TCAM_TX_row_data[0],
		cfa_tcam_mgr_WC_TCAM_TX_row_data[0],
		cfa_tcam_mgr_SP_TCAM_TX_row_data[0],
		cfa_tcam_mgr_CT_RULE_TCAM_TX_row_data[0],
		cfa_tcam_mgr_VEB_TCAM_TX_row_data[0],
		cfa_tcam_mgr_WC_TCAM_TX_row_data[0],
		cfa_tcam_mgr_WC_TCAM_TX_row_data[0],
	}
};

static int cfa_tcam_mgr_get_max_rows(enum tf_tcam_tbl_type type)
{
	if (type >= TF_TCAM_TBL_TYPE_MAX)
		assert(0);
	else
		return cfa_tcam_mgr_max_rows[type];
}

static int cfa_tcam_mgr_hwop_set(int sess_idx,
				 struct cfa_tcam_mgr_set_parms *parms, int row,
				 int slice, int max_slices)
{
	struct cfa_tcam_mgr_TCAM_row_data *this_table;
	struct cfa_tcam_mgr_TCAM_row_data *this_row;
	this_table = row_tables[parms->dir]
		[cfa_tcam_mgr_get_phys_table_type(parms->type)];
	this_table += (sess_idx *
		       cfa_tcam_mgr_get_max_rows(cfa_tcam_mgr_get_phys_table_type(parms->type)));
	this_row   = &this_table[row * max_slices + slice];
	this_row->key_size = parms->key_size;
	memcpy(&this_row->key, parms->key, parms->key_size);
	memcpy(&this_row->mask, parms->mask, parms->key_size);
	this_row->result_size = parms->result_size;
	if (parms->result != ((void *)0))
		memcpy(&this_row->result, parms->result, parms->result_size);
	return 0;
};

static int cfa_tcam_mgr_hwop_get(int sess_idx,
				 struct cfa_tcam_mgr_get_parms *parms, int row,
				 int slice, int max_slices)
{
	struct cfa_tcam_mgr_TCAM_row_data *this_table;
	struct cfa_tcam_mgr_TCAM_row_data *this_row;
	this_table = row_tables[parms->dir]
		[cfa_tcam_mgr_get_phys_table_type(parms->type)];
	this_table += (sess_idx *
		       cfa_tcam_mgr_get_max_rows(cfa_tcam_mgr_get_phys_table_type(parms->type)));
	this_row   = &this_table[row * max_slices + slice];
	parms->key_size = this_row->key_size;
	parms->result_size = this_row->result_size;
	if (parms->key != ((void *)0))
		memcpy(parms->key, &this_row->key, parms->key_size);
	if (parms->mask != ((void *)0))
		memcpy(parms->mask, &this_row->mask, parms->key_size);
	if (parms->result != ((void *)0))
		memcpy(parms->result, &this_row->result, parms->result_size);
	return 0;
};

static int cfa_tcam_mgr_hwop_free(int sess_idx,
				  struct cfa_tcam_mgr_free_parms *parms,
				  int row, int slice, int max_slices)
{
	struct cfa_tcam_mgr_TCAM_row_data *this_table;
	struct cfa_tcam_mgr_TCAM_row_data *this_row;
	this_table = row_tables[parms->dir]
		[cfa_tcam_mgr_get_phys_table_type(parms->type)];
	this_table += (sess_idx *
		       cfa_tcam_mgr_get_max_rows(cfa_tcam_mgr_get_phys_table_type(parms->type)));
	this_row   = &this_table[row * max_slices + slice];
	memset(&this_row->key, 0, sizeof(this_row->key));
	memset(&this_row->mask, 0, sizeof(this_row->mask));
	memset(&this_row->result, 0, sizeof(this_row->result));
	this_row->key_size = 0;
	this_row->result_size = 0;
	return 0;
};

int cfa_tcam_mgr_hwops_get_funcs_p4(struct cfa_tcam_mgr_hwops_funcs *hwop_funcs)
{
	hwop_funcs->set	 = cfa_tcam_mgr_hwop_set;
	hwop_funcs->get	 = cfa_tcam_mgr_hwop_get;
	hwop_funcs->free = cfa_tcam_mgr_hwop_free;
	return 0;
}
