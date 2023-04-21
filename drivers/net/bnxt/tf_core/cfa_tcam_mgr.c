/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include <signal.h>

#include "hcapi_cfa_defs.h"

#include "tfp.h"
#include "tf_session.h"
#include "tf_util.h"
#include "cfa_tcam_mgr.h"
#include "cfa_tcam_mgr_hwop_msg.h"
#include "cfa_tcam_mgr_device.h"
#include "cfa_tcam_mgr_session.h"
#include "cfa_tcam_mgr_p58.h"
#include "cfa_tcam_mgr_p4.h"

#define TF_TCAM_SLICE_INVALID (-1)

/*
 * The following macros are for setting the entry status in a row entry.
 * row is (struct cfa_tcam_mgr_table_rows_0 *)
 */
#define ROW_ENTRY_INUSE(row, entry)  ((row)->entry_inuse &   (1U << (entry)))
#define ROW_ENTRY_SET(row, entry)    ((row)->entry_inuse |=  (1U << (entry)))
#define ROW_ENTRY_CLEAR(row, entry)  ((row)->entry_inuse &= ~(1U << (entry)))
#define ROW_INUSE(row)               ((row)->entry_inuse != 0)

static struct cfa_tcam_mgr_entry_data *entry_data[TF_TCAM_MAX_SESSIONS];

static int global_data_initialized[TF_TCAM_MAX_SESSIONS];
int cfa_tcam_mgr_max_entries[TF_TCAM_MAX_SESSIONS];

struct cfa_tcam_mgr_table_data
cfa_tcam_mgr_tables[TF_TCAM_MAX_SESSIONS][TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];

static int physical_table_types[CFA_TCAM_MGR_TBL_TYPE_MAX] = {
	[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_APPS] =
		TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_APPS]  =
		TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	[CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_APPS]	      =
		TF_TCAM_TBL_TYPE_PROF_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS]	      =
		TF_TCAM_TBL_TYPE_WC_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_APPS]	      =
		TF_TCAM_TBL_TYPE_SP_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_APPS]      =
		TF_TCAM_TBL_TYPE_CT_RULE_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_APPS]	      =
		TF_TCAM_TBL_TYPE_VEB_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS]     =
		TF_TCAM_TBL_TYPE_WC_TCAM_HIGH,
	[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS]      =
		TF_TCAM_TBL_TYPE_WC_TCAM_LOW,
};

int
cfa_tcam_mgr_get_phys_table_type(enum cfa_tcam_mgr_tbl_type type)
{
	if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX)
		assert(0);
	else
		return physical_table_types[type];
}

const char *
cfa_tcam_mgr_tbl_2_str(enum cfa_tcam_mgr_tbl_type tcam_type)
{
	switch (tcam_type) {
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_AFM:
		return "l2_ctxt_tcam_high AFM";
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_APPS:
		return "l2_ctxt_tcam_high Apps";
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_AFM:
		return "l2_ctxt_tcam_low AFM";
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_APPS:
		return "l2_ctxt_tcam_low Apps";
	case CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_AFM:
		return "prof_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_APPS:
		return "prof_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_AFM:
		return "wc_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS:
		return "wc_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_AFM:
		return "veb_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_APPS:
		return "veb_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_AFM:
		return "sp_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_APPS:
		return "sp_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_AFM:
		return "ct_rule_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_APPS:
		return "ct_rule_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_AFM:
		return "wc_tcam_high AFM";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS:
		return "wc_tcam_high Apps";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_AFM:
		return "wc_tcam_low AFM";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS:
		return "wc_tcam_low Apps";
	default:
		return "Invalid tcam table type";
	}
}

/* key_size and slice_width are in bytes */
static int
cfa_tcam_mgr_get_num_slices(unsigned int key_size, unsigned int slice_width)
{
	int num_slices = 0;

	if (key_size == 0)
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);

	num_slices = ((key_size - 1U) / slice_width) + 1U;
	/* Round up to next highest power of 2 */
	/* This is necessary since, for example, 3 slices is not a valid entry
	 * width.
	 */
	num_slices--;
	/* Repeat to maximum number of bits actually used */
	/* This fills in all the bits. */
	num_slices |= num_slices >> 1;
	num_slices |= num_slices >> 2;
	num_slices |= num_slices >> 4;
	/*
	 * If the maximum number of slices that are supported by the HW
	 * increases, then additional shifts are needed.
	 */
	num_slices++;
	return num_slices;
}

static struct cfa_tcam_mgr_entry_data *
cfa_tcam_mgr_entry_get(int sess_idx, uint16_t id)
{
	if (id > cfa_tcam_mgr_max_entries[sess_idx])
		return NULL;

	return &entry_data[sess_idx][id];
}

/* Insert an entry into the entry table */
static int
cfa_tcam_mgr_entry_insert(int sess_idx, uint16_t id,
			  struct cfa_tcam_mgr_entry_data *entry)
{
	if (id > cfa_tcam_mgr_max_entries[sess_idx])
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);

	memcpy(&entry_data[sess_idx][id], entry,
	       sizeof(entry_data[sess_idx][id]));

	return 0;
}

/* Delete an entry from the entry table */
static int
cfa_tcam_mgr_entry_delete(int sess_idx, uint16_t id)
{
	if (id > cfa_tcam_mgr_max_entries[sess_idx])
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);

	memset(&entry_data[sess_idx][id], 0, sizeof(entry_data[sess_idx][id]));

	return 0;
}

/* Returns the size of the row structure taking into account how many slices a
 * TCAM supports.
 */
static int
cfa_tcam_mgr_row_size_get(int sess_idx, enum tf_dir dir,
			  enum cfa_tcam_mgr_tbl_type type)
{
	return sizeof(struct cfa_tcam_mgr_table_rows_0) +
		(cfa_tcam_mgr_tables[sess_idx][dir][type].max_slices *
		 sizeof(((struct cfa_tcam_mgr_table_rows_0 *)0)->entries[0]));
}

static void *
cfa_tcam_mgr_row_ptr_get(void *base, int index, int row_size)
{
	return (uint8_t *)base + (index * row_size);
}

/*
 * Searches a table to find the direction and type of an entry.
 */
static int
cfa_tcam_mgr_entry_find_in_table(int sess_idx, int id, enum tf_dir dir,
				 enum cfa_tcam_mgr_tbl_type type)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_table_rows_0 *row;
	int max_slices, row_idx, row_size, slice;

	table_data = &cfa_tcam_mgr_tables[sess_idx][dir][type];
	if (table_data->max_entries > 0 &&
	    table_data->hcapi_type > 0) {
		max_slices = table_data->max_slices;
		row_size = cfa_tcam_mgr_row_size_get(sess_idx, dir, type);
		for (row_idx = table_data->start_row;
		     row_idx <= table_data->end_row;
		     row_idx++) {
			row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows,
						       row_idx, row_size);
			if (!ROW_INUSE(row))
				continue;
			for (slice = 0;
			     slice < (max_slices / row->entry_size);
			     slice++) {
				if (!ROW_ENTRY_INUSE(row, slice))
					continue;
				if (row->entries[slice] == id)
					return 0;
			}
		}
	}

	return -CFA_TCAM_MGR_ERR_CODE(NOENT);
}

/*
 * Searches all the tables to find the direction and type of an entry.
 */
static int
cfa_tcam_mgr_entry_find(int sess_idx, int id, enum tf_dir *tbl_dir,
			enum cfa_tcam_mgr_tbl_type *tbl_type)
{
	enum tf_dir dir;
	enum cfa_tcam_mgr_tbl_type type;
	int rc = -CFA_TCAM_MGR_ERR_CODE(NOENT);

	for (dir = TF_DIR_RX; dir < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx]); dir++) {
		for (type = CFA_TCAM_MGR_TBL_TYPE_START;
		     type < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx][dir]);
		     type++) {
			rc = cfa_tcam_mgr_entry_find_in_table(sess_idx, id, dir, type);
			if (rc == 0) {
				*tbl_dir  = dir;
				*tbl_type = type;
				return rc;
			}
		}
	}

	return rc;
}

static int
cfa_tcam_mgr_row_is_entry_free(struct cfa_tcam_mgr_table_rows_0 *row,
			      int max_slices,
			      int key_slices)
{
	int j;

	if (ROW_INUSE(row) &&
	    row->entry_size == key_slices) {
		for (j = 0; j < (max_slices / row->entry_size); j++) {
			if (!ROW_ENTRY_INUSE(row, j))
				return j;
		}
	}
	return -1;
}

static int
cfa_tcam_mgr_entry_move(int sess_idx, struct cfa_tcam_mgr_context *context,
		       enum tf_dir dir, enum cfa_tcam_mgr_tbl_type type,
		       int entry_id,
		       struct cfa_tcam_mgr_table_data *table_data,
		       int dest_row_index, int dest_row_slice,
		       struct cfa_tcam_mgr_table_rows_0 *dest_row,
		       int source_row_index,
		       struct cfa_tcam_mgr_table_rows_0 *source_row,
		       bool free_source_entry)
{
	struct cfa_tcam_mgr_get_parms gparms = { 0 };
	struct cfa_tcam_mgr_set_parms sparms = { 0 };
	struct cfa_tcam_mgr_free_parms fparms = { 0 };
	struct cfa_tcam_mgr_entry_data *entry;
	uint8_t  key[CFA_TCAM_MGR_MAX_KEY_SIZE];
	uint8_t  mask[CFA_TCAM_MGR_MAX_KEY_SIZE];
	uint8_t  result[CFA_TCAM_MGR_MAX_KEY_SIZE];

	int j, rc;

	entry = cfa_tcam_mgr_entry_get(sess_idx, entry_id);
	if (entry == NULL)
		return -1;

	gparms.dir	   = dir;
	gparms.type	   = type;
	gparms.hcapi_type  = table_data->hcapi_type;
	gparms.key	   = key;
	gparms.mask	   = mask;
	gparms.result	   = result;
	gparms.id	   = source_row->entries[entry->slice];
	gparms.key_size	   = sizeof(key);
	gparms.result_size = sizeof(result);

	rc = cfa_tcam_mgr_entry_get_msg(sess_idx, context, &gparms,
					source_row_index,
					entry->slice * source_row->entry_size,
					table_data->max_slices);
	if (rc != 0)
		return rc;

	sparms.dir	   = dir;
	sparms.type	   = type;
	sparms.hcapi_type  = table_data->hcapi_type;
	sparms.key	   = key;
	sparms.mask	   = mask;
	sparms.result	   = result;
	sparms.id	   = gparms.id;
	sparms.key_size	   = gparms.key_size;
	sparms.result_size = gparms.result_size;

	/* Slice in destination row not specified. Find first free slice. */
	if (dest_row_slice < 0)
		for (j = 0;
		     j < (table_data->max_slices / dest_row->entry_size);
		     j++) {
			if (!ROW_ENTRY_INUSE(dest_row, j)) {
				dest_row_slice = j;
				break;
			}
		}

	/* If no free slice found, return error. */
	if (dest_row_slice < 0)
		return -CFA_TCAM_MGR_ERR_CODE(PERM);

	rc = cfa_tcam_mgr_entry_set_msg(sess_idx, context, &sparms,
					dest_row_index,
					dest_row_slice * dest_row->entry_size,
					table_data->max_slices);
	if (rc != 0)
		return rc;

	if (free_source_entry) {
		fparms.dir	  = dir;
		fparms.type	  = type;
		fparms.hcapi_type = table_data->hcapi_type;
		rc = cfa_tcam_mgr_entry_free_msg(sess_idx, context, &fparms,
						 source_row_index,
						 entry->slice *
						 dest_row->entry_size,
						 table_data->row_width /
						 table_data->max_slices *
						 source_row->entry_size,
						 table_data->result_size,
						 table_data->max_slices);
		if (rc != 0) {
			CFA_TCAM_MGR_LOG_DIR_TYPE(ERR,
						  dir, type,
						 "Failed to free entry ID %d at"
						 " row %d, slice %d for sess_idx %d. rc: %d.\n",
						  gparms.id,
						  source_row_index,
						  entry->slice,
						  sess_idx,
						  -rc);
		}
	}

	ROW_ENTRY_SET(dest_row, dest_row_slice);
	dest_row->entries[dest_row_slice] = entry_id;
	ROW_ENTRY_CLEAR(source_row, entry->slice);
	entry->row   = dest_row_index;
	entry->slice = dest_row_slice;

	return 0;
}

static int
cfa_tcam_mgr_row_move(int sess_idx, struct cfa_tcam_mgr_context *context,
		      enum tf_dir dir, enum cfa_tcam_mgr_tbl_type type,
		      struct cfa_tcam_mgr_table_data *table_data,
		      int dest_row_index,
		      struct cfa_tcam_mgr_table_rows_0 *dest_row,
		      int source_row_index,
		      struct cfa_tcam_mgr_table_rows_0 *source_row)
{
	struct cfa_tcam_mgr_free_parms fparms = { 0 };
	int j, rc;

	dest_row->priority   = source_row->priority;
	dest_row->entry_size = source_row->entry_size;
	dest_row->entry_inuse = 0;

	fparms.dir	  = dir;
	fparms.type	  = type;
	fparms.hcapi_type = table_data->hcapi_type;

	for (j = 0;
	     j < (table_data->max_slices / source_row->entry_size);
	     j++) {
		if (ROW_ENTRY_INUSE(source_row, j)) {
			cfa_tcam_mgr_entry_move(sess_idx, context, dir, type,
						source_row->entries[j],
						table_data,
						dest_row_index, j, dest_row,
						source_row_index, source_row,
						true);
		} else {
			/* Slice not in use, write an empty slice. */
			rc = cfa_tcam_mgr_entry_free_msg(sess_idx, context, &fparms,
							dest_row_index,
							j *
							dest_row->entry_size,
							table_data->row_width /
							table_data->max_slices *
							dest_row->entry_size,
							table_data->result_size,
							table_data->max_slices);
			if (rc != 0)
				return rc;
		}
	}

	return 0;
}

/* Install entry into in-memory tables, not into TCAM (yet). */
static void
cfa_tcam_mgr_row_entry_install(int sess_idx,
			       struct cfa_tcam_mgr_table_rows_0 *row,
			       struct cfa_tcam_mgr_alloc_parms *parms,
			       struct cfa_tcam_mgr_entry_data *entry,
			       uint16_t id,
			       int key_slices,
			       int row_index, int slice)
{
	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(INFO, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return;
	}

	if (slice == TF_TCAM_SLICE_INVALID) {
		slice = 0;
		row->entry_size = key_slices;
		row->priority = parms->priority;
	}

	ROW_ENTRY_SET(row, slice);
	row->entries[slice] = id;
	entry->row = row_index;
	entry->slice = slice;
}

/* Finds an empty row that can be used and reserve for entry.  If necessary,
 * entries will be shuffled in order to make room.
 */
static struct cfa_tcam_mgr_table_rows_0 *
cfa_tcam_mgr_empty_row_alloc(int sess_idx, struct cfa_tcam_mgr_context *context,
			     struct cfa_tcam_mgr_alloc_parms *parms,
			     struct cfa_tcam_mgr_entry_data *entry,
			     uint16_t id,
			     int key_slices)
{
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	struct cfa_tcam_mgr_table_rows_0 *from_row;
	struct cfa_tcam_mgr_table_rows_0 *to_row;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct cfa_tcam_mgr_table_data *table_data;
	int i, max_slices, row_size;
	int to_row_idx, from_row_idx, slice, start_row, end_row;
	int empty_row = -1;
	int target_row = -1;

	table_data = &cfa_tcam_mgr_tables[sess_idx][parms->dir][parms->type];

	start_row  = table_data->start_row;
	end_row	   = table_data->end_row;
	max_slices = table_data->max_slices;
	tcam_rows  = table_data->tcam_rows;

	row_size   = cfa_tcam_mgr_row_size_get(sess_idx, parms->dir, parms->type);

	/*
	 * First check for partially used entries, but only if the key needs
	 * fewer slices than there are in a row.
	 */
	if (key_slices < max_slices) {
		for (i = start_row; i <= end_row; i++) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (!ROW_INUSE(row))
				continue;
			if (row->priority < parms->priority)
				break;
			if (row->priority > parms->priority)
				continue;
			slice = cfa_tcam_mgr_row_is_entry_free(row,
							       max_slices,
							       key_slices);
			if (slice >= 0) {
				cfa_tcam_mgr_row_entry_install(sess_idx, row, parms,
							       entry, id,
							       key_slices,
							       i, slice);
				return row;
			}
		}
	}

	/* No partially used rows available.  Find an empty row, if any. */

	/*
	 * All max priority entries are placed in the beginning of the TCAM.  It
	 * should not be necessary to shuffle any of these entries.  All other
	 * priorities are placed from the end of the TCAM and may require
	 * shuffling.
	 */
	if (parms->priority == TF_TCAM_PRIORITY_MAX) {
		/* Handle max priority first. */
		for (i = start_row; i <= end_row; i++) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (!ROW_INUSE(row)) {
				cfa_tcam_mgr_row_entry_install(sess_idx,
							       row, parms,
							       entry,
							       id, key_slices,
							       i,
							 TF_TCAM_SLICE_INVALID);
				return row;
			}
			if (row->priority < parms->priority) {
				/*
				 * No free entries before priority change, table
				 * is full.
				 */
				return NULL;
			}
		}
		/* No free entries found, table is full. */
		return NULL;
	}

	/* Use the highest available entry */
	for (i = end_row; i >= start_row; i--) {
		row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
		if (!ROW_INUSE(row)) {
			empty_row = i;
			break;
		}

		if (row->priority > parms->priority &&
		    target_row < 0)
			target_row = i;
	}

	if (empty_row < 0) {
		/* No free entries found, table is full. */
		return NULL;
	}

	if (target_row < 0) {
		/*
		 * Did not find a row with higher priority before unused row so
		 * just install new entry in empty_row.
		 */
		row = cfa_tcam_mgr_row_ptr_get(tcam_rows, empty_row, row_size);
		cfa_tcam_mgr_row_entry_install(sess_idx, row, parms, entry, id,
					       key_slices, empty_row,
					       TF_TCAM_SLICE_INVALID);
		return row;
	}

	to_row_idx = empty_row;
	to_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, to_row_idx, row_size);
	while (to_row_idx < target_row) {
		from_row_idx = to_row_idx + 1;
		from_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, from_row_idx,
						    row_size);
		/*
		 * Find the highest row with the same priority as the initial
		 * source row (from_row).  It's only necessary to copy one row
		 * of each priority.
		 */
		for (i = from_row_idx + 1; i <= target_row; i++) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (row->priority != from_row->priority)
				break;
			from_row_idx = i;
			from_row = row;
		}
		cfa_tcam_mgr_row_move(sess_idx, context, parms->dir, parms->type,
				      table_data, to_row_idx, to_row,
				      from_row_idx, from_row);
		to_row = from_row;
		to_row_idx = from_row_idx;
	}
	to_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, target_row, row_size);
	memset(to_row, 0, row_size);
	cfa_tcam_mgr_row_entry_install(sess_idx, to_row, parms, entry, id,
				       key_slices, target_row,
				       TF_TCAM_SLICE_INVALID);

	return row;
}

/*
 * This function will combine rows when possible to result in the fewest rows
 * used necessary for the entries that are installed.
 */
static void
cfa_tcam_mgr_rows_combine(int sess_idx, struct cfa_tcam_mgr_context *context,
			  struct cfa_tcam_mgr_free_parms *parms,
			  struct cfa_tcam_mgr_table_data *table_data,
			  int changed_row_index)
{
	struct cfa_tcam_mgr_table_rows_0 *from_row = NULL;
	struct cfa_tcam_mgr_table_rows_0 *to_row;
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	int  i, j, row_size;
	int  to_row_idx, from_row_idx, start_row, end_row, max_slices;
	bool entry_moved = false;

	start_row  = table_data->start_row;
	end_row	   = table_data->end_row;
	max_slices = table_data->max_slices;
	tcam_rows  = table_data->tcam_rows;

	row_size   = cfa_tcam_mgr_row_size_get(sess_idx, parms->dir, parms->type);

	from_row_idx = changed_row_index;
	from_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, from_row_idx, row_size);

	if (ROW_INUSE(from_row)) {
		/*
		 * Row is still in partial use.  See if remaining entry(s) can
		 * be moved to free up a row.
		 */
		for (i = 0; i < (max_slices / from_row->entry_size); i++) {
			if (!ROW_ENTRY_INUSE(from_row, i))
				continue;
			for (to_row_idx = end_row;
			     to_row_idx >= start_row;
			     to_row_idx--) {
				to_row = cfa_tcam_mgr_row_ptr_get(tcam_rows,
								  to_row_idx,
								  row_size);
				if (!ROW_INUSE(to_row))
					continue;
				if (to_row->priority > from_row->priority)
					break;
				if (to_row->priority != from_row->priority)
					continue;
				if (to_row->entry_size != from_row->entry_size)
					continue;
				if (to_row_idx == changed_row_index)
					continue;
				for (j = 0;
				     j < (max_slices / to_row->entry_size);
				     j++) {
					if (!ROW_ENTRY_INUSE(to_row, j)) {
						cfa_tcam_mgr_entry_move
							(sess_idx,
							 context,
							 parms->dir,
							 parms->type,
							 from_row->entries[i],
							 table_data,
							 to_row_idx,
							 -1, to_row,
							 from_row_idx,
							 from_row,
							 true);
						entry_moved = true;
						break;
					}
				}
				if (entry_moved)
					break;
			}
			if (ROW_INUSE(from_row))
				entry_moved = false;
			else
				break;
		}
	}
}

/*
 * This function will ensure that all rows, except those of the highest
 * priority, at the end of the table.  When this function is finished, all the
 * empty rows should be between the highest priority rows at the beginning of
 * the table and the rest of the rows with lower priorities.
 */
/*
 * Will need to free the row left newly empty as a result of moving.
 *
 * Return row to free to caller.  If new_row_to_free < 0, then no new row to
 * free.
 */
static void
cfa_tcam_mgr_rows_compact(int sess_idx, struct cfa_tcam_mgr_context *context,
			  struct cfa_tcam_mgr_free_parms *parms,
			  struct cfa_tcam_mgr_table_data *table_data,
			  int *new_row_to_free,
			  int changed_row_index)
{
	struct cfa_tcam_mgr_table_rows_0 *from_row = NULL;
	struct cfa_tcam_mgr_table_rows_0 *to_row;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	int  i, row_size, priority;
	int  to_row_idx = 0, from_row_idx = 0, start_row = 0, end_row = 0;

	*new_row_to_free = -1;

	start_row  = table_data->start_row;
	end_row	   = table_data->end_row;
	tcam_rows  = table_data->tcam_rows;

	row_size   = cfa_tcam_mgr_row_size_get(sess_idx, parms->dir, parms->type);

	/*
	 * The row is no longer in use, so see if rows need to be moved in order
	 * to not leave any gaps.
	 */
	to_row_idx = changed_row_index;
	to_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, to_row_idx, row_size);

	priority = to_row->priority;
	if (priority == TF_TCAM_PRIORITY_MAX) {
		if (changed_row_index == end_row)
			/*
			 * Nothing to move - the last row in the TCAM is being
			 * deleted.
			 */
			return;
		for (i = changed_row_index + 1; i <= end_row; i++) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (!ROW_INUSE(row))
				break;

			if (row->priority < priority)
				break;

			from_row = row;
			from_row_idx = i;
		}
	} else {
		if (changed_row_index == start_row)
			/*
			 * Nothing to move - the first row in the TCAM is being
			 * deleted.
			 */
			return;
		for (i = changed_row_index - 1; i >= start_row; i--) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (!ROW_INUSE(row))
				break;

			if (row->priority > priority) {
				/* Don't move the highest priority rows. */
				if (row->priority == TF_TCAM_PRIORITY_MAX)
					break;
				/*
				 * If from_row is NULL, that means that there
				 * were no rows of the deleted priority.
				 * Nothing to move yet.
				 *
				 * If from_row is not NULL, then it is the last
				 * row with the same priority and must be moved
				 * to fill the newly empty (by free or by move)
				 * row.
				 */
				if (from_row != NULL) {
					cfa_tcam_mgr_row_move(sess_idx, context,
							      parms->dir,
							      parms->type,
							      table_data,
							     to_row_idx, to_row,
							      from_row_idx,
							      from_row);
					*new_row_to_free = from_row_idx;
					to_row	   = from_row;
					to_row_idx = from_row_idx;
				}

				priority = row->priority;
			}
			from_row = row;
			from_row_idx = i;
		}
	}

	if (from_row != NULL) {
		cfa_tcam_mgr_row_move(sess_idx, context, parms->dir, parms->type,
				      table_data,
				      to_row_idx, to_row,
				      from_row_idx, from_row);
		*new_row_to_free = from_row_idx;
	}
}

/*
 * This function is to set table limits for the logical TCAM tables.
 */
static int
cfa_tcam_mgr_table_limits_set(int sess_idx, struct cfa_tcam_mgr_init_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	unsigned int dir, type;
	int start, stride;

	if (parms == NULL)
		return 0;

	for (dir = 0; dir < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx]); dir++)
		for (type = 0;
		     type < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx][dir]);
		     type++) {
			table_data = &cfa_tcam_mgr_tables[sess_idx][dir][type];
			/*
			 * If num_rows is zero, then TCAM Manager did not
			 * allocate any row storage for that table so cannot
			 * manage it.
			 */
			if (table_data->num_rows == 0)
				continue;
			start  = parms->resc[dir][type].start;
			stride = parms->resc[dir][type].stride;
			if (start % table_data->max_slices > 0) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, type,
							  "Start of resources (%d) for table (%d) "
							  "does not begin on row boundary.\n",
							  start, sess_idx);
				CFA_TCAM_MGR_LOG_DIR(ERR, dir,
						     "Start is %d, number of slices "
						     "is %d.\n",
						     start,
						     table_data->max_slices);
				return -CFA_TCAM_MGR_ERR_CODE(INVAL);
			}
			if (stride % table_data->max_slices > 0) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, type,
							  "Stride of resources (%d) for table (%d)"
							  " does not end on row boundary.\n",
							  stride, sess_idx);
				CFA_TCAM_MGR_LOG_DIR(ERR, dir,
						     "Stride is %d, number of "
						     "slices is %d.\n",
						     stride,
						     table_data->max_slices);
				return -CFA_TCAM_MGR_ERR_CODE(INVAL);
			}
			if (stride == 0) {
				table_data->start_row	= 0;
				table_data->end_row	= 0;
				table_data->max_entries = 0;
			} else {
				table_data->start_row = start /
					table_data->max_slices;
				table_data->end_row = table_data->start_row +
					(stride / table_data->max_slices) - 1;
				table_data->max_entries =
					table_data->max_slices *
					(table_data->end_row -
					 table_data->start_row + 1);
			}
		}

	return 0;
}

int
cfa_tcam_mgr_init(int sess_idx, enum cfa_tcam_mgr_device_type type,
		  struct cfa_tcam_mgr_init_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	unsigned int dir, tbl_type;
	int rc;

	switch (type) {
	case CFA_TCAM_MGR_DEVICE_TYPE_P4:
	case CFA_TCAM_MGR_DEVICE_TYPE_SR:
		rc = cfa_tcam_mgr_init_p4(sess_idx, &entry_data[sess_idx]);
		break;
	case CFA_TCAM_MGR_DEVICE_TYPE_P5:
		rc = cfa_tcam_mgr_init_p58(sess_idx, &entry_data[sess_idx]);
		break;
	default:
		CFA_TCAM_MGR_LOG(ERR, "No such device %d for sess_idx %d\n",
				 type, sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}
	if (rc < 0)
		return rc;

	rc = cfa_tcam_mgr_table_limits_set(sess_idx, parms);
	if (rc < 0)
		return rc;

	/* Now calculate the max entries per table and global max entries based
	 * on the updated table limits.
	 */
	for (dir = 0; dir < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx]); dir++)
		for (tbl_type = 0;
		     tbl_type < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx][dir]);
		     tbl_type++) {
			table_data = &cfa_tcam_mgr_tables[sess_idx][dir][tbl_type];
			/*
			 * If num_rows is zero, then TCAM Manager did not
			 * allocate any row storage for that table so cannot
			 * manage it.
			 */
			if (table_data->num_rows == 0) {
				table_data->start_row = 0;
				table_data->end_row = 0;
				table_data->max_entries = 0;
			} else if (table_data->end_row >=
				   table_data->num_rows) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(EMERG, dir, tbl_type,
							  "End row is out of "
							  "range (%d >= %d) for sess_idx %d\n",
							  table_data->end_row,
							  table_data->num_rows,
							  sess_idx);
				return -CFA_TCAM_MGR_ERR_CODE(FAULT);
			} else if (table_data->max_entries == 0 &&
				   table_data->start_row == 0 &&
				   table_data->end_row == 0) {
				/* Nothing to do */
			} else {
				table_data->max_entries =
					table_data->max_slices *
					(table_data->end_row -
					 table_data->start_row + 1);
			}
			cfa_tcam_mgr_max_entries[sess_idx] += table_data->max_entries;
		}

	rc = cfa_tcam_mgr_hwops_init(type);
	if (rc < 0)
		return rc;

	rc = cfa_tcam_mgr_session_init(sess_idx, type);
	if (rc < 0)
		return rc;

	global_data_initialized[sess_idx] = 1;

	if (parms != NULL)
		parms->max_entries = cfa_tcam_mgr_max_entries[sess_idx];

	CFA_TCAM_MGR_LOG(INFO, "Global TCAM table initialized for sess_idx %d.\n",
			 sess_idx);

	return 0;
}

int
cfa_tcam_mgr_qcaps(struct cfa_tcam_mgr_context *context __rte_unused,
		   struct cfa_tcam_mgr_qcaps_parms *parms)
{
	unsigned int type;
	int rc, sess_idx;
	uint32_t session_id;

	CFA_TCAM_MGR_CHECK_PARMS2(context, parms);

	rc = cfa_tcam_mgr_get_session_from_context(context, &session_id);
	if (rc < 0)
		return rc;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG_0(ERR, "Session not found.\n");
		return sess_idx;
	}

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(ERR, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	/*
	 * This code will indicate if TCAM Manager is managing a logical TCAM
	 * table or not.  If not, then the physical TCAM will have to be
	 * accessed using the traditional methods.
	 */
	parms->rx_tcam_supported = 0;
	parms->tx_tcam_supported = 0;
	for (type = 0; type < CFA_TCAM_MGR_TBL_TYPE_MAX; type++) {
		if (cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX][type].max_entries > 0 &&
		    cfa_tcam_mgr_tables[sess_idx][TF_DIR_RX][type].hcapi_type > 0)
			parms->rx_tcam_supported |= 1 << cfa_tcam_mgr_get_phys_table_type(type);
		if (cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX][type].max_entries > 0 &&
		    cfa_tcam_mgr_tables[sess_idx][TF_DIR_TX][type].hcapi_type > 0)
			parms->tx_tcam_supported |= 1 << cfa_tcam_mgr_get_phys_table_type(type);
	}

	return 0;
}

/*
 * Manipulate the tables to split the WC TCAM into HIGH and LOW ranges
 * and also update the sizes in the tcam count array
 */
static int
cfa_tcam_mgr_shared_wc_bind(uint32_t sess_idx, bool dual_ha_app,
			    uint16_t tcam_cnt[][CFA_TCAM_MGR_TBL_TYPE_MAX])
{
	uint16_t start_row, end_row, max_entries, slices;
	uint16_t num_pools = dual_ha_app ? 4 : 2;
	enum tf_dir dir;
	int rc;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		rc = cfa_tcam_mgr_tables_get(sess_idx, dir,
					     CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS,
					     &start_row, &end_row, &max_entries, &slices);
		if (rc)
			return rc;
		if (max_entries) {
			rc = cfa_tcam_mgr_tables_set(sess_idx, dir,
						     CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS,
						     start_row,
						     start_row +
						     ((max_entries / slices) / num_pools) - 1,
						     max_entries / num_pools);
			if (rc)
				return rc;
			rc = cfa_tcam_mgr_tables_set(sess_idx, dir,
						     CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS,
						     start_row +
						     ((max_entries / slices) / num_pools),
						     start_row +
						     (max_entries / slices) - 1,
						     max_entries / num_pools);
			if (rc)
				return rc;
			rc = cfa_tcam_mgr_tables_set(sess_idx, dir,
						     CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS,
						     0, 0, 0);
			if (rc)
				return rc;
			tcam_cnt[dir][CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS] =
				max_entries / num_pools;
			tcam_cnt[dir][CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS] =
				max_entries / num_pools;
			tcam_cnt[dir][CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS] = 0;
		}
	}

	return 0;
}

int
cfa_tcam_mgr_bind(struct cfa_tcam_mgr_context *context,
		  struct cfa_tcam_mgr_cfg_parms *parms)
{
	struct cfa_tcam_mgr_table_data   *table_data;
	struct tf_dev_info *dev;
	unsigned int dir;
	int rc, sess_idx;
	uint32_t session_id;
	struct tf_session *tfs;
	unsigned int type;
	int prev_max_entries;
	int start, stride;
	enum cfa_tcam_mgr_device_type device_type;

	CFA_TCAM_MGR_CHECK_PARMS2(context, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(context->tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	switch (dev->type) {
	case TF_DEVICE_TYPE_P4:
		device_type = CFA_TCAM_MGR_DEVICE_TYPE_P4;
		break;
	case TF_DEVICE_TYPE_SR:
		device_type = CFA_TCAM_MGR_DEVICE_TYPE_SR;
		break;
	case TF_DEVICE_TYPE_P5:
		device_type = CFA_TCAM_MGR_DEVICE_TYPE_P5;
		break;
	default:
		CFA_TCAM_MGR_LOG(ERR, "No such device %d\n", dev->type);
		return -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}

	rc = cfa_tcam_mgr_get_session_from_context(context, &session_id);
	if (rc < 0)
		return rc;

	sess_idx = cfa_tcam_mgr_session_add(session_id);
	if (sess_idx < 0)
		return sess_idx;

	if (global_data_initialized[sess_idx] == 0) {
		rc = cfa_tcam_mgr_init(sess_idx, device_type, NULL);
		if (rc < 0)
			return rc;
	}

	if (parms->num_elements != ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx][dir])) {
		CFA_TCAM_MGR_LOG(ERR,
				 "Session element count (%d) differs "
				 "from table count (%zu) for sess_idx %d.\n",
				 parms->num_elements,
				 ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx][dir]),
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	/*
	 * Only managing one session. resv_res contains the resources allocated
	 * to this session by the resource manager.  Update the limits on TCAMs.
	 */
	for (dir = 0; dir < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx]); dir++) {
		for (type = 0;
		     type < ARRAY_SIZE(cfa_tcam_mgr_tables[sess_idx][dir]);
		     type++) {
			table_data = &cfa_tcam_mgr_tables[sess_idx][dir][type];
			prev_max_entries = table_data->max_entries;
			/*
			 * In AFM logical tables, max_entries is initialized to
			 * zero.  These logical tables are not used when TCAM
			 * Manager is in the core so skip.
			 */
			if (prev_max_entries == 0)
				continue;
			start  = parms->resv_res[dir][type].start;
			stride = parms->resv_res[dir][type].stride;
			if (start % table_data->max_slices > 0) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, type,
					 "Start of resources (%d) for table(%d) "
					 "does not begin on row boundary.\n",
					 start, sess_idx);
				CFA_TCAM_MGR_LOG_DIR(ERR, dir,
					    "Start is %d, number of slices "
					    "is %d.\n",
					    start,
					    table_data->max_slices);
				(void)cfa_tcam_mgr_session_free(session_id, context);
				return -CFA_TCAM_MGR_ERR_CODE(INVAL);
			}
			if (stride % table_data->max_slices > 0) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, type,
					   "Stride of resources (%d) for table(%d) "
					   "does not end on row boundary.\n",
					   stride, sess_idx);
				CFA_TCAM_MGR_LOG_DIR(ERR, dir,
					    "Stride is %d, number of "
					    "slices is %d.\n",
					    stride,
					    table_data->max_slices);
				(void)cfa_tcam_mgr_session_free(session_id, context);
				return -CFA_TCAM_MGR_ERR_CODE(INVAL);
			}
			if (stride == 0) {
				table_data->start_row	= 0;
				table_data->end_row	= 0;
				table_data->max_entries = 0;
			} else {
				table_data->start_row = start /
					table_data->max_slices;
				table_data->end_row = table_data->start_row +
					(stride / table_data->max_slices) - 1;
				table_data->max_entries =
					table_data->max_slices *
					(table_data->end_row -
					 table_data->start_row + 1);
			}
			cfa_tcam_mgr_max_entries[sess_idx] += (table_data->max_entries -
						     prev_max_entries);
		}
	}

	if (tf_session_is_shared_hotup_session(tfs)) {
		rc = cfa_tcam_mgr_shared_wc_bind(sess_idx, false, parms->tcam_cnt);
		if (rc) {
			(void)cfa_tcam_mgr_session_free(session_id, context);
			return rc;
		}
	}

	rc = cfa_tcam_mgr_session_cfg(session_id, parms->tcam_cnt);
	if (rc < 0) {
		(void)cfa_tcam_mgr_session_free(session_id, context);
		return rc;
	}

	return 0;
}

int
cfa_tcam_mgr_unbind(struct cfa_tcam_mgr_context *context)
{
	int rc, sess_idx;
	uint32_t session_id;

	CFA_TCAM_MGR_CHECK_PARMS1(context);

	rc = cfa_tcam_mgr_get_session_from_context(context, &session_id);
	if (rc < 0)
		return rc;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG_0(ERR, "Session not found.\n");
		return sess_idx;
	}

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(INFO, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	(void)cfa_tcam_mgr_session_free(session_id, context);

	global_data_initialized[sess_idx] = 0;
	return 0;
}

int
cfa_tcam_mgr_alloc(struct cfa_tcam_mgr_context *context,
		   struct cfa_tcam_mgr_alloc_parms *parms)
{
	struct cfa_tcam_mgr_entry_data    entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct cfa_tcam_mgr_table_data   *table_data;
	int dir, tbl_type;
	int key_slices, rc, sess_idx;
	int new_entry_id;
	uint32_t session_id;

	CFA_TCAM_MGR_CHECK_PARMS2(context, parms);

	dir = parms->dir;
	tbl_type = parms->type;

	if (dir >= TF_DIR_MAX) {
		CFA_TCAM_MGR_LOG(ERR, "Invalid direction: %d.\n", dir);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	if (tbl_type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		CFA_TCAM_MGR_LOG_DIR(ERR, dir,
				     "Invalid table type: %d.\n",
				     tbl_type);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

#if TF_TCAM_PRIORITY_MAX < UINT16_MAX
	if (parms->priority > TF_TCAM_PRIORITY_MAX) {
		CFA_TCAM_MGR_LOG_DIR(ERR, dir,
				     "Priority (%u) out of range (%u -%u).\n",
				     parms->priority,
				     TF_TCAM_PRIORITY_MIN,
				     TF_TCAM_PRIORITY_MAX);
	}
#endif

	/* Check for session limits */
	rc = cfa_tcam_mgr_get_session_from_context(context, &session_id);
	if (rc < 0)
		return rc;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG(ERR, "Session 0x%08x not found.\n",
				 session_id);
		return -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(ERR, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	table_data = &cfa_tcam_mgr_tables[sess_idx][dir][tbl_type];

	if (parms->key_size == 0 ||
	    parms->key_size > table_data->row_width) {
		CFA_TCAM_MGR_LOG_DIR(ERR, dir,
				     "Invalid key size:%d (range 1-%d) sess_idx %d.\n",
				     parms->key_size,
				     table_data->row_width,
				     sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	/* Check global limits */
	if (table_data->used_entries >=
	    table_data->max_entries) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, tbl_type,
					    "Table full sess_idx %d.\n",
					    sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(NOSPC);
	}

	/* There is room, now increment counts and allocate an entry. */
	new_entry_id = cfa_tcam_mgr_session_entry_alloc(session_id,
							parms->dir,
							parms->type);
	if (new_entry_id < 0)
		return new_entry_id;

	memset(&entry, 0, sizeof(entry));
	entry.ref_cnt++;

	key_slices = cfa_tcam_mgr_get_num_slices(parms->key_size,
						 (table_data->row_width /
						  table_data->max_slices));

	row = cfa_tcam_mgr_empty_row_alloc(sess_idx, context, parms, &entry,
					   new_entry_id, key_slices);
	if (row == NULL) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					    "Table full (HW) sess_idx %d.\n",
					    sess_idx);
		(void)cfa_tcam_mgr_session_entry_free(session_id, new_entry_id,
						      parms->dir, parms->type);
		return -CFA_TCAM_MGR_ERR_CODE(NOSPC);
	}

	memcpy(&entry_data[sess_idx][new_entry_id],
	       &entry,
	       sizeof(entry_data[sess_idx][new_entry_id]));
	table_data->used_entries += 1;

	cfa_tcam_mgr_entry_insert(sess_idx, new_entry_id, &entry);

	parms->id = new_entry_id;

	return 0;
}

int
cfa_tcam_mgr_free(struct cfa_tcam_mgr_context *context,
		  struct cfa_tcam_mgr_free_parms *parms)
{
	struct cfa_tcam_mgr_entry_data *entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct cfa_tcam_mgr_table_data *table_data;
	int row_size, rc, sess_idx, new_row_to_free;
	uint32_t session_id;
	uint16_t id;

	CFA_TCAM_MGR_CHECK_PARMS2(context, parms);

	rc = cfa_tcam_mgr_get_session_from_context(context, &session_id);
	if (rc < 0)
		return rc;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG(ERR, "Session 0x%08x not found.\n",
				 session_id);
		return sess_idx;
	}

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(INFO, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	id = parms->id;
	entry = cfa_tcam_mgr_entry_get(sess_idx, id);
	if (entry == NULL) {
		CFA_TCAM_MGR_LOG(INFO, "Entry %d not found for sess_idx %d.\n",
				 id, sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	if (entry->ref_cnt == 0) {
		CFA_TCAM_MGR_LOG(ERR, "Entry %d not in use for sess_idx %d.\n",
				 id, sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	/*
	 * If the TCAM type is CFA_TCAM_MGR_TBL_TYPE_MAX, that implies that the
	 * caller does not know the table or direction of the entry and TCAM
	 * Manager must search the tables to find out which table has the entry
	 * installed.
	 *
	 * This would be the case if RM has informed TCAM Mgr that an entry must
	 * be freed.  Clients (sessions, AFM) should always know the type and
	 * direction of the table where an entry is installed.
	 */
	if (parms->type == CFA_TCAM_MGR_TBL_TYPE_MAX) {
		/* Need to search for the entry in the tables */
		rc = cfa_tcam_mgr_entry_find(sess_idx, id, &parms->dir, &parms->type);
		if (rc < 0) {
			CFA_TCAM_MGR_LOG(ERR, "Entry %d not in tables for sess_idx %d.\n",
					 id, sess_idx);
			return rc;
		}
	}

	table_data = &cfa_tcam_mgr_tables[sess_idx][parms->dir][parms->type];
	parms->hcapi_type = table_data->hcapi_type;

	row_size = cfa_tcam_mgr_row_size_get(sess_idx, parms->dir, parms->type);

	row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, entry->row,
				       row_size);

	entry->ref_cnt--;

	(void)cfa_tcam_mgr_session_entry_free(session_id, id,
					      parms->dir, parms->type);

	if (entry->ref_cnt == 0) {
		cfa_tcam_mgr_entry_free_msg(sess_idx, context, parms,
					    entry->row,
					    entry->slice * row->entry_size,
					    table_data->row_width /
					    table_data->max_slices *
					    row->entry_size,
					    table_data->result_size,
					    table_data->max_slices);
		ROW_ENTRY_CLEAR(row, entry->slice);

		new_row_to_free = entry->row;
		cfa_tcam_mgr_rows_combine(sess_idx, context, parms, table_data,
					  new_row_to_free);

		if (!ROW_INUSE(row)) {
			cfa_tcam_mgr_rows_compact(sess_idx, context,
						  parms, table_data,
						  &new_row_to_free,
						  new_row_to_free);
			if (new_row_to_free >= 0)
				cfa_tcam_mgr_entry_free_msg(sess_idx, context, parms,
						   new_row_to_free, 0,
						   table_data->row_width,
						   table_data->result_size,
						   table_data->max_slices);
		}

		cfa_tcam_mgr_entry_delete(sess_idx, id);
		table_data->used_entries -= 1;
	}

	return 0;
}

int
cfa_tcam_mgr_set(struct cfa_tcam_mgr_context *context,
		 struct cfa_tcam_mgr_set_parms *parms)
{
	struct cfa_tcam_mgr_entry_data *entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct cfa_tcam_mgr_table_data *table_data;
	int rc;
	int row_size, sess_idx;
	int entry_size_in_bytes;
	uint32_t session_id;

	CFA_TCAM_MGR_CHECK_PARMS2(context, parms);

	rc = cfa_tcam_mgr_get_session_from_context(context, &session_id);
	if (rc < 0)
		return rc;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG(ERR, "Session 0x%08x not found.\n",
				 session_id);
		return sess_idx;
	}

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(ERR, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	entry = cfa_tcam_mgr_entry_get(sess_idx, parms->id);
	if (entry == NULL) {
		CFA_TCAM_MGR_LOG(ERR, "Entry %d not found for sess_idx %d.\n",
				 parms->id, sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	table_data = &cfa_tcam_mgr_tables[sess_idx][parms->dir][parms->type];
	parms->hcapi_type = table_data->hcapi_type;

	row_size = cfa_tcam_mgr_row_size_get(sess_idx, parms->dir, parms->type);
	row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, entry->row,
				       row_size);

	entry_size_in_bytes = table_data->row_width /
			      table_data->max_slices *
			      row->entry_size;
	if (parms->key_size != entry_size_in_bytes) {
		CFA_TCAM_MGR_LOG(ERR,
				"Key size(%d) is different from entry "
				"size(%d).\n",
				parms->key_size,
				entry_size_in_bytes);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	rc = cfa_tcam_mgr_entry_set_msg(sess_idx, context, parms,
					entry->row,
					entry->slice * row->entry_size,
					table_data->max_slices);
	if (rc < 0) {
		CFA_TCAM_MGR_LOG_0(ERR, "Failed to set TCAM data.\n");
		return rc;
	}

	return 0;
}

int
cfa_tcam_mgr_get(struct cfa_tcam_mgr_context *context __rte_unused,
		 struct cfa_tcam_mgr_get_parms *parms)
{
	struct cfa_tcam_mgr_entry_data *entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct cfa_tcam_mgr_table_data *table_data;
	int rc;
	int row_size, sess_idx;
	uint32_t session_id;

	CFA_TCAM_MGR_CHECK_PARMS2(context, parms);

	rc = cfa_tcam_mgr_get_session_from_context(context, &session_id);
	if (rc < 0)
		return rc;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG(ERR, "Session 0x%08x not found.\n",
				 session_id);
		return sess_idx;
	}

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(ERR, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	entry = cfa_tcam_mgr_entry_get(sess_idx, parms->id);
	if (entry == NULL) {
		CFA_TCAM_MGR_LOG(ERR, "Entry %d not found.\n", parms->id);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	table_data = &cfa_tcam_mgr_tables[sess_idx][parms->dir][parms->type];
	parms->hcapi_type = table_data->hcapi_type;

	row_size = cfa_tcam_mgr_row_size_get(sess_idx, parms->dir, parms->type);
	row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, entry->row,
				       row_size);

	rc = cfa_tcam_mgr_entry_get_msg(sess_idx, context, parms,
					entry->row,
					entry->slice * row->entry_size,
					table_data->max_slices);
	if (rc < 0) {
		CFA_TCAM_MGR_LOG_0(ERR, "Failed to read from TCAM.\n");
		return rc;
	}

	return 0;
}

int cfa_tcam_mgr_shared_clear(struct cfa_tcam_mgr_context *context,
		     struct cfa_tcam_mgr_shared_clear_parms *parms)
{
	int rc;
	uint16_t row, slice = 0;
	int sess_idx;
	uint32_t session_id;
	struct cfa_tcam_mgr_free_parms fparms;
	struct cfa_tcam_mgr_table_data *table_data;
	uint16_t start_row, end_row, max_entries, max_slices;

	CFA_TCAM_MGR_CHECK_PARMS2(context, parms);

	rc = cfa_tcam_mgr_get_session_from_context(context, &session_id);
	if (rc < 0)
		return rc;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG(ERR, "Session 0x%08x not found.\n",
				 session_id);
		return sess_idx;
	}

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(ERR, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	table_data = &cfa_tcam_mgr_tables[sess_idx][parms->dir][parms->type];
	fparms.dir = parms->dir;
	fparms.type = parms->type;
	fparms.hcapi_type = table_data->hcapi_type;
	fparms.id = 0;

	rc = cfa_tcam_mgr_tables_get(sess_idx, parms->dir, parms->type,
				&start_row, &end_row, &max_entries, &max_slices);
	if (rc)
		return rc;

	for (row = start_row; row <= end_row; row++) {
		cfa_tcam_mgr_entry_free_msg(sess_idx, context, &fparms,
					    row,
					    slice,
					    table_data->row_width,
					    table_data->result_size,
					    table_data->max_slices);
	}
	return rc;
}

static void
cfa_tcam_mgr_mv_used_entries_cnt(int sess_idx, enum tf_dir dir,
				 struct cfa_tcam_mgr_table_data *dst_table_data,
				 struct cfa_tcam_mgr_table_data *src_table_data)
{
	dst_table_data->used_entries++;
	src_table_data->used_entries--;

	cfa_tcam_mgr_mv_session_used_entries_cnt(sess_idx, dir,
						 CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS,
						 CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS);
}

/*
 * Move HI WC TCAM entries to LOW TCAM region for HA
 * This happens when secondary is becoming primary
 */
static int
cfa_tcam_mgr_shared_entry_move(int sess_idx, struct cfa_tcam_mgr_context *context,
		       enum tf_dir dir, enum cfa_tcam_mgr_tbl_type type,
		       int entry_id,
		       struct cfa_tcam_mgr_table_data *dst_table_data,
		       struct cfa_tcam_mgr_table_data *table_data,
		       int dst_row_index, int dst_row_slice,
		       struct cfa_tcam_mgr_table_rows_0 *dst_row,
		       int src_row_index,
		       struct cfa_tcam_mgr_table_rows_0 *src_row)
{
	struct cfa_tcam_mgr_get_parms gparms = { 0 };
	struct cfa_tcam_mgr_set_parms sparms = { 0 };
	struct cfa_tcam_mgr_free_parms fparms = { 0 };
	struct cfa_tcam_mgr_entry_data *entry;
	uint8_t  key[CFA_TCAM_MGR_MAX_KEY_SIZE];
	uint8_t  mask[CFA_TCAM_MGR_MAX_KEY_SIZE];
	uint8_t  result[CFA_TCAM_MGR_MAX_KEY_SIZE];

	int rc;

	entry = cfa_tcam_mgr_entry_get(sess_idx, entry_id);
	if (entry == NULL)
		return -1;

	gparms.dir	   = dir;
	gparms.type	   = type;
	gparms.hcapi_type  = table_data->hcapi_type;
	gparms.key	   = key;
	gparms.mask	   = mask;
	gparms.result	   = result;
	gparms.id	   = src_row->entries[entry->slice];
	gparms.key_size	   = sizeof(key);
	gparms.result_size = sizeof(result);

	rc = cfa_tcam_mgr_entry_get_msg(sess_idx, context, &gparms,
					src_row_index,
					entry->slice * src_row->entry_size,
					table_data->max_slices);
	if (rc != 0)
		return rc;

	sparms.dir	   = dir;
	sparms.type	   = CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS;
	sparms.hcapi_type  = table_data->hcapi_type;
	sparms.key	   = key;
	sparms.mask	   = mask;
	sparms.result	   = result;
	sparms.id	   = gparms.id;
	sparms.key_size	   = gparms.key_size;
	sparms.result_size = gparms.result_size;

	rc = cfa_tcam_mgr_entry_set_msg(sess_idx, context, &sparms,
					dst_row_index,
					dst_row_slice * dst_row->entry_size,
					table_data->max_slices);
	if (rc != 0)
		return rc;

	fparms.dir	  = dir;
	fparms.type	  = type;
	fparms.hcapi_type = table_data->hcapi_type;
	rc = cfa_tcam_mgr_entry_free_msg(sess_idx, context, &fparms,
					 src_row_index,
					 entry->slice *
					 dst_row->entry_size,
					 table_data->row_width /
					 table_data->max_slices *
					 src_row->entry_size,
					 table_data->result_size,
					 table_data->max_slices);
	if (rc != 0) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR,
					  dir, type,
					  "Failed to free entry ID %d at"
					  " row %d, slice %d for sess_idx %d. rc: %d.\n",
					  gparms.id,
					  src_row_index,
					  entry->slice,
					  sess_idx,
					  -rc);
	}

#ifdef CFA_TCAM_MGR_TRACING
	CFA_TCAM_MGR_TRACE(INFO, "Moved entry %d from row %d, slice %d to "
			   "row %d, slice %d.\n",
			   entry_id, src_row_index, entry->slice,
			   dst_row_index, dst_row_slice);
#endif

	ROW_ENTRY_SET(dst_row, dst_row_slice);
	dst_row->entries[dst_row_slice] = entry_id;
	dst_row->entry_size = src_row->entry_size;
	dst_row->priority = src_row->priority;
	ROW_ENTRY_CLEAR(src_row, entry->slice);
	entry->row = dst_row_index;
	entry->slice = dst_row_slice;

	cfa_tcam_mgr_mv_used_entries_cnt(sess_idx, dir, dst_table_data, table_data);

#ifdef CFA_TCAM_MGR_TRACING
	cfa_tcam_mgr_rows_dump(sess_idx, dir, type);
	cfa_tcam_mgr_rows_dump(sess_idx, dir, CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS);
#endif

	return 0;
}

int cfa_tcam_mgr_shared_move(struct cfa_tcam_mgr_context *context,
		     struct cfa_tcam_mgr_shared_move_parms *parms)
{
	int rc;
	int sess_idx;
	uint32_t session_id;
	uint16_t src_row, dst_row, row_size, slice;
	struct cfa_tcam_mgr_table_rows_0 *src_table_row;
	struct cfa_tcam_mgr_table_rows_0 *dst_table_row;
	struct cfa_tcam_mgr_table_data *src_table_data;
	struct cfa_tcam_mgr_table_data *dst_table_data;

	CFA_TCAM_MGR_CHECK_PARMS2(context, parms);

	rc = cfa_tcam_mgr_get_session_from_context(context, &session_id);
	if (rc < 0)
		return rc;

	sess_idx = cfa_tcam_mgr_session_find(session_id);
	if (sess_idx < 0) {
		CFA_TCAM_MGR_LOG(ERR, "Session 0x%08x not found.\n",
				 session_id);
		return sess_idx;
	}

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(ERR, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	src_table_data =
		&cfa_tcam_mgr_tables[sess_idx][parms->dir][CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS];
	dst_table_data =
		&cfa_tcam_mgr_tables[sess_idx][parms->dir][CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS];

	row_size =
		cfa_tcam_mgr_row_size_get(sess_idx,
					  parms->dir,
					  CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS);

	for (src_row = src_table_data->start_row,
	     dst_row = dst_table_data->start_row;
	     src_row <= src_table_data->end_row;
	     src_row++, dst_row++) {
		src_table_row = cfa_tcam_mgr_row_ptr_get(src_table_data->tcam_rows,
							 src_row, row_size);
		dst_table_row = cfa_tcam_mgr_row_ptr_get(dst_table_data->tcam_rows,
							 dst_row, row_size);
		if (ROW_INUSE(src_table_row)) {
			for (slice = 0;
			     slice < src_table_data->max_slices / src_table_row->entry_size;
			     slice++) {
				if (ROW_ENTRY_INUSE(src_table_row, slice)) {
#ifdef CFA_TCAM_MGR_TRACING
					CFA_TCAM_MGR_TRACE(INFO, "Move entry id %d "
							   "from src_row %d, slice %d "
							   "to dst_row %d, slice %d.\n",
							   src_table_row->entries[slice],
							   src_row, slice,
							   dst_row, slice);
#endif
					rc = cfa_tcam_mgr_shared_entry_move(sess_idx,
							context,
							parms->dir,
							CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS,
							src_table_row->entries[slice],
							dst_table_data,
							src_table_data,
							dst_row, slice,
							dst_table_row,
							src_row,
							src_table_row);
				}
			}
		}
	}

	return rc;
}

static void
cfa_tcam_mgr_tbl_get(int sess_idx, enum tf_dir dir,
			enum cfa_tcam_mgr_tbl_type type,
				uint16_t *start_row,
				uint16_t *end_row,
				uint16_t *max_entries,
				uint16_t *slices)
{
	struct cfa_tcam_mgr_table_data *table_data =
		&cfa_tcam_mgr_tables[sess_idx][dir][type];

	/* Get start, end and max for tcam type*/
	*start_row = table_data->start_row;
	*end_row = table_data->end_row;
	*max_entries = table_data->max_entries;
	*slices = table_data->max_slices;
}

int
cfa_tcam_mgr_tables_get(int sess_idx, enum tf_dir dir,
			enum cfa_tcam_mgr_tbl_type type,
			uint16_t *start_row,
			uint16_t *end_row,
			uint16_t *max_entries,
			uint16_t *slices)
{
	CFA_TCAM_MGR_CHECK_PARMS3(start_row, end_row, max_entries);

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(ERR, "PANIC: TCAM not initialized for sess_idx %d.\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	if (dir >= TF_DIR_MAX) {
		CFA_TCAM_MGR_LOG(ERR, "Must specify valid dir (0-%d) forsess_idx %d.\n",
				 TF_DIR_MAX - 1, sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		CFA_TCAM_MGR_LOG(ERR, "Must specify valid tbl type (0-%d) forsess_idx %d.\n",
				 CFA_TCAM_MGR_TBL_TYPE_MAX - 1, sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	cfa_tcam_mgr_tbl_get(sess_idx, dir,
				  type,
				  start_row,
				  end_row,
				  max_entries,
				  slices);
	return 0;
}

static void
cfa_tcam_mgr_tbl_set(int sess_idx, enum tf_dir dir,
			enum cfa_tcam_mgr_tbl_type type,
				uint16_t start_row,
				uint16_t end_row,
				uint16_t max_entries)
{
	struct cfa_tcam_mgr_table_data *table_data =
		&cfa_tcam_mgr_tables[sess_idx][dir][type];

	/* Update start, end and max for tcam type*/
	table_data->start_row = start_row;
	table_data->end_row = end_row;
	table_data->max_entries = max_entries;
}

int
cfa_tcam_mgr_tables_set(int sess_idx, enum tf_dir dir,
			enum cfa_tcam_mgr_tbl_type type,
			uint16_t start_row,
			uint16_t end_row,
			uint16_t max_entries)
{
	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(ERR, "PANIC: TCAM not initialized for sess_idx %d.\n",
				 sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	if (dir >= TF_DIR_MAX) {
		CFA_TCAM_MGR_LOG(ERR, "Must specify valid dir (0-%d) forsess_idx %d.\n",
				 TF_DIR_MAX - 1, sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		CFA_TCAM_MGR_LOG(ERR, "Must specify valid tbl type (0-%d) forsess_idx %d.\n",
				 CFA_TCAM_MGR_TBL_TYPE_MAX - 1, sess_idx);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	cfa_tcam_mgr_tbl_set(sess_idx, dir,
				  type,
				  start_row,
				  end_row,
				  max_entries);
	return 0;
}

void
cfa_tcam_mgr_rows_dump(int sess_idx, enum tf_dir dir,
		       enum cfa_tcam_mgr_tbl_type type)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_table_rows_0 *table_row;
	int i, row, row_size;
	bool row_found = false;
	bool empty_row = false;

	if (global_data_initialized[sess_idx] == 0) {
		printf("PANIC: TCAM not initialized for sess_idx %d.\n", sess_idx);
		return;
	}

	if (dir >= TF_DIR_MAX) {
		printf("Must specify a valid direction (0-%d).\n",
		       TF_DIR_MAX - 1);
		return;
	}
	if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		printf("Must specify a valid type (0-%d).\n",
		       CFA_TCAM_MGR_TBL_TYPE_MAX - 1);
		return;
	}

	table_data = &cfa_tcam_mgr_tables[sess_idx][dir][type];
	row_size = cfa_tcam_mgr_row_size_get(sess_idx, dir, type);

	printf("\nTCAM Rows:\n");
	printf("Rows for direction %s, Logical table type %s\n",
	       tf_dir_2_str(dir), cfa_tcam_mgr_tbl_2_str(type));
	printf("Managed rows %d-%d for sess_idx %d:\n",
	       table_data->start_row, table_data->end_row, sess_idx);

	printf("Index Pri   Size  Entry IDs\n");
	printf("                  Sl 0");
	for (i = 1; i < table_data->max_slices; i++)
		printf("  Sl %d", i);
	printf("\n");
	for (row = table_data->start_row; row <= table_data->end_row; row++) {
		table_row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, row,
						    row_size);
		if (ROW_INUSE(table_row)) {
			empty_row = false;
			printf("%5u %5u %4u",
			       row,
			       TF_TCAM_PRIORITY_MAX - table_row->priority - 1,
			       table_row->entry_size);
			for (i = 0;
			     i < table_data->max_slices / table_row->entry_size;
			     i++) {
				if (ROW_ENTRY_INUSE(table_row, i))
					printf(" %5u", table_row->entries[i]);
				else
					printf("     x");
			}
			printf("\n");
			row_found = true;
		} else if (!empty_row) {
			empty_row = true;
			printf("\n");
		}
	}

	if (!row_found)
		printf("No rows in use.\n");
}

static void
cfa_tcam_mgr_table_dump(int sess_idx, enum tf_dir dir,
			enum cfa_tcam_mgr_tbl_type type)
{
	struct cfa_tcam_mgr_table_data *table_data =
		&cfa_tcam_mgr_tables[sess_idx][dir][type];

	printf("%3s %-22s %5u %5u %5u %5u %6u %7u %2u\n",
	       tf_dir_2_str(dir),
	       cfa_tcam_mgr_tbl_2_str(type),
	       table_data->row_width,
	       table_data->num_rows,
	       table_data->start_row,
	       table_data->end_row,
	       table_data->max_entries,
	       table_data->used_entries,
	       table_data->max_slices);
}

#define TABLE_DUMP_HEADER \
	"Dir Table                  Width  Rows Start   End " \
	"MaxEnt UsedEnt Slices\n"

void
cfa_tcam_mgr_tables_dump(int sess_idx, enum tf_dir dir,
			 enum cfa_tcam_mgr_tbl_type type)
{
	if (global_data_initialized[sess_idx] == 0) {
		printf("PANIC: TCAM not initialized for sess_idx %d.\n", sess_idx);
		return;
	}

	printf("\nTCAM Table(s) for sess_idx %d:\n", sess_idx);
	printf(TABLE_DUMP_HEADER);
	if (dir >= TF_DIR_MAX) {
		/* Iterate over all directions */
		for (dir = 0; dir < TF_DIR_MAX; dir++) {
			if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
				/* Iterate over all types */
				for (type = 0;
				     type < CFA_TCAM_MGR_TBL_TYPE_MAX;
				     type++) {
					cfa_tcam_mgr_table_dump(sess_idx, dir, type);
				}
			} else {
				/* Display a specific type */
				cfa_tcam_mgr_table_dump(sess_idx, dir, type);
			}
		}
	} else if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		/* Iterate over all types for a direction */
		for (type = 0; type < CFA_TCAM_MGR_TBL_TYPE_MAX; type++)
			cfa_tcam_mgr_table_dump(sess_idx, dir, type);
	} else {
		/* Display a specific direction and type */
		cfa_tcam_mgr_table_dump(sess_idx, dir, type);
	}
}

#define ENTRY_DUMP_HEADER "Entry RefCnt  Row Slice\n"

void
cfa_tcam_mgr_entries_dump(int sess_idx)
{
	struct cfa_tcam_mgr_entry_data *entry;
	bool entry_found = false;
	uint16_t id;

	if (global_data_initialized[sess_idx] == 0) {
		CFA_TCAM_MGR_LOG(INFO, "PANIC: No TCAM data created for sess_idx %d\n",
				 sess_idx);
		return;
	}

	printf("\nGlobal Maximum Entries: %d\n\n",
	       cfa_tcam_mgr_max_entries[sess_idx]);
	printf("TCAM Entry Table:\n");
	for (id = 0; id < cfa_tcam_mgr_max_entries[sess_idx]; id++) {
		if (entry_data[sess_idx][id].ref_cnt > 0) {
			entry = &entry_data[sess_idx][id];
			if (!entry_found)
				printf(ENTRY_DUMP_HEADER);
			printf("%5u %5u %5u %5u",
			       id, entry->ref_cnt,
			       entry->row, entry->slice);
			printf("\n");
			entry_found = true;
		}
	}

	if (!entry_found)
		printf("No entries found.\n");
}
