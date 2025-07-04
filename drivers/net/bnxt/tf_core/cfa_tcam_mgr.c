/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2024 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include <signal.h>

#include "hcapi_cfa_defs.h"

#include "tfp.h"
#include "tf_session.h"
#include "tf_util.h"
#include "cfa_tcam_mgr.h"
#include "cfa_tcam_mgr_device.h"
#include "cfa_tcam_mgr_hwop_msg.h"
#include "cfa_tcam_mgr_p58.h"
#include "cfa_tcam_mgr_p4.h"

#define TF_TCAM_SLICE_INVALID (-1)

static int physical_table_types[CFA_TCAM_MGR_TBL_TYPE_MAX] = {
	[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH] =
		TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW]  =
		TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	[CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM]	      =
		TF_TCAM_TBL_TYPE_PROF_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM]	      =
		TF_TCAM_TBL_TYPE_WC_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_SP_TCAM]	      =
		TF_TCAM_TBL_TYPE_SP_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM]      =
		TF_TCAM_TBL_TYPE_CT_RULE_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM]	      =
		TF_TCAM_TBL_TYPE_VEB_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH]     =
		TF_TCAM_TBL_TYPE_WC_TCAM_HIGH,
	[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW]      =
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
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH:
		return "l2_ctxt_tcam_high Apps";
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW:
		return "l2_ctxt_tcam_low Apps";
	case CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM:
		return "prof_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM:
		return "wc_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM:
		return "veb_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_SP_TCAM:
		return "sp_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM:
		return "ct_rule_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH:
		return "wc_tcam_high Apps";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW:
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

	if (!key_size)
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
cfa_tcam_mgr_entry_get(struct cfa_tcam_mgr_data *tcam_mgr_data, uint16_t id)
{
	if (id > tcam_mgr_data->cfa_tcam_mgr_max_entries)
		return NULL;

	return &tcam_mgr_data->entry_data[id];
}

/* Insert an entry into the entry table */
static int
cfa_tcam_mgr_entry_insert(struct cfa_tcam_mgr_data *tcam_mgr_data,
			  struct tf *tfp __rte_unused, uint16_t id,
			  struct cfa_tcam_mgr_entry_data *entry)
{
	if (id > tcam_mgr_data->cfa_tcam_mgr_max_entries)
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);

	memcpy(&tcam_mgr_data->entry_data[id], entry,
	       sizeof(tcam_mgr_data->entry_data[id]));

	PMD_DRV_LOG_LINE(INFO, "Added entry %d to table", id);

	return 0;
}

/* Delete an entry from the entry table */
static int
cfa_tcam_mgr_entry_delete(struct cfa_tcam_mgr_data *tcam_mgr_data,
			  struct tf *tfp __rte_unused, uint16_t id)
{
	if (id > tcam_mgr_data->cfa_tcam_mgr_max_entries)
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);

	memset(&tcam_mgr_data->entry_data[id], 0,
	       sizeof(tcam_mgr_data->entry_data[id]));

	PMD_DRV_LOG_LINE(INFO, "Deleted entry %d from table.", id);

	return 0;
}

/* Returns the size of the row structure taking into account how many slices a
 * TCAM supports.
 */
static int
cfa_tcam_mgr_row_size_get(struct cfa_tcam_mgr_data *tcam_mgr_data,
			  enum tf_dir dir,
			  enum cfa_tcam_mgr_tbl_type type)
{
	return sizeof(struct cfa_tcam_mgr_table_rows_0) +
		(tcam_mgr_data->cfa_tcam_mgr_tables[dir][type].max_slices *
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
cfa_tcam_mgr_entry_find_in_table(struct cfa_tcam_mgr_data *tcam_mgr_data,
				 int id, enum tf_dir dir,
				 enum cfa_tcam_mgr_tbl_type type)
{
	struct cfa_tcam_mgr_table_data *table_data;
	int max_slices, row_idx, row_size, slice;
	struct cfa_tcam_mgr_table_rows_0 *row;

	table_data = &tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
	if (table_data->max_entries > 0 &&
	    table_data->hcapi_type > 0) {
		max_slices = table_data->max_slices;
		row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, dir, type);
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
cfa_tcam_mgr_entry_find(struct cfa_tcam_mgr_data *tcam_mgr_data, int id,
			enum tf_dir *tbl_dir,
			enum cfa_tcam_mgr_tbl_type *tbl_type)
{
	enum cfa_tcam_mgr_tbl_type type;
	int rc = -CFA_TCAM_MGR_ERR_CODE(NOENT);
	enum tf_dir dir;

	for (dir = TF_DIR_RX; dir <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
			dir++) {
		for (type = CFA_TCAM_MGR_TBL_TYPE_START;
		     type <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     type++) {
			rc = cfa_tcam_mgr_entry_find_in_table(tcam_mgr_data,
							      id, dir, type);
			if (!rc) {
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
	return -CFA_TCAM_MGR_ERR_CODE(INVAL);
}

static int
cfa_tcam_mgr_entry_move(struct cfa_tcam_mgr_data *tcam_mgr_data, struct tf *tfp,
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
	uint8_t  result[CFA_TCAM_MGR_MAX_KEY_SIZE];
	uint8_t  mask[CFA_TCAM_MGR_MAX_KEY_SIZE];
	uint8_t  key[CFA_TCAM_MGR_MAX_KEY_SIZE];

	int j, rc;

	entry = cfa_tcam_mgr_entry_get(tcam_mgr_data, entry_id);
	if (!entry)
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);

	gparms.dir	   = dir;
	gparms.type	   = type;
	gparms.hcapi_type  = table_data->hcapi_type;
	gparms.key	   = key;
	gparms.mask	   = mask;
	gparms.result	   = result;
	gparms.id	   = source_row->entries[entry->slice];
	gparms.key_size	   = sizeof(key);
	gparms.result_size = sizeof(result);

	rc = cfa_tcam_mgr_entry_get_msg(tcam_mgr_data, tfp, &gparms,
					source_row_index,
					entry->slice * source_row->entry_size,
					table_data->max_slices);
	if (rc)
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

	rc = cfa_tcam_mgr_entry_set_msg(tcam_mgr_data, tfp, &sparms,
					dest_row_index,
					dest_row_slice * dest_row->entry_size,
					table_data->max_slices);
	if (rc)
		return rc;

	if (free_source_entry) {
		fparms.dir	  = dir;
		fparms.type	  = type;
		fparms.hcapi_type = table_data->hcapi_type;
		rc = cfa_tcam_mgr_entry_free_msg(tcam_mgr_data,
						 tfp, &fparms,
						 source_row_index,
						 entry->slice *
						 dest_row->entry_size,
						 table_data->row_width /
						 table_data->max_slices *
						 source_row->entry_size,
						 table_data->result_size,
						 table_data->max_slices);
		if (rc) {
			CFA_TCAM_MGR_LOG_DIR_TYPE(ERR,
						  dir, type,
						  "Failed to free entry ID:%d"
						  " at row:%d slice:%d"
						  " rc:%d\n",
						  gparms.id,
						  source_row_index,
						  entry->slice,
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
cfa_tcam_mgr_row_move(struct cfa_tcam_mgr_data *tcam_mgr_data, struct tf *tfp,
		      enum tf_dir dir, enum cfa_tcam_mgr_tbl_type type,
		      struct cfa_tcam_mgr_table_data *tbl_data,
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
	fparms.hcapi_type = tbl_data->hcapi_type;

	for (j = 0;
	     j < (tbl_data->max_slices / source_row->entry_size);
	     j++) {
		if (ROW_ENTRY_INUSE(source_row, j)) {
			cfa_tcam_mgr_entry_move(tcam_mgr_data, tfp,
						dir, type,
						source_row->entries[j],
						tbl_data,
						dest_row_index, j, dest_row,
						source_row_index, source_row,
						true);
		} else {
			/* Slice not in use, write an empty slice. */
			rc = cfa_tcam_mgr_entry_free_msg(tcam_mgr_data,
							 tfp, &fparms,
							 dest_row_index,
							 j *
							 dest_row->entry_size,
							 tbl_data->row_width /
							 tbl_data->max_slices *
							 dest_row->entry_size,
							 tbl_data->result_size,
							 tbl_data->max_slices);
			if (rc)
				return rc;
		}
	}

	return 0;
}

/* Install entry into in-memory tables, not into TCAM (yet). */
static void
cfa_tcam_mgr_row_entry_install(struct tf *tfp __rte_unused,
			       struct cfa_tcam_mgr_table_rows_0 *row,
			       struct cfa_tcam_mgr_alloc_parms *parms,
			       struct cfa_tcam_mgr_entry_data *entry,
			       uint16_t id,
			       int key_slices,
			       int row_index, int slice)
{
	if (slice == TF_TCAM_SLICE_INVALID) {
		slice = 0;
		row->entry_size = key_slices;
		row->priority = parms->priority;
	}

	ROW_ENTRY_SET(row, slice);
	row->entries[slice] = id;
	entry->row = row_index;
	entry->slice = slice;

	PMD_DRV_LOG_LINE(INFO,
			 "Entry %d installed row:%d slice:%d prio:%d",
			 id, row_index, slice, row->priority);
}

/* Finds an empty row that can be used and reserve for entry.  If necessary,
 * entries will be shuffled in order to make room.
 */
static struct cfa_tcam_mgr_table_rows_0 *
cfa_tcam_mgr_empty_row_alloc(struct cfa_tcam_mgr_data *tcam_mgr_data,
			     struct tf *tfp,
			     struct cfa_tcam_mgr_alloc_parms *parms,
			     struct cfa_tcam_mgr_entry_data *entry,
			     uint16_t id, int key_slices)
{
	int to_row_idx, from_row_idx, slice, start_row, end_row;
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_table_rows_0 *from_row;
	struct cfa_tcam_mgr_table_rows_0 *to_row;
	struct cfa_tcam_mgr_table_rows_0 *row;
	int i, max_slices, row_size;
	int target_row = -1;
	int empty_row = -1;

	table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir][parms->type];

	start_row = table_data->start_row;
	end_row = table_data->end_row;
	max_slices = table_data->max_slices;
	tcam_rows = table_data->tcam_rows;

	row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					     parms->type);
	/*
	 * Note: The rows are ordered from highest priority to lowest priority.
	 * That is, the first row in the table will have the highest priority
	 * and the last row in the table will have the lowest priority.
	 */

	PMD_DRV_LOG_LINE(INFO, "Trying to alloc space for entry with "
			 "priority %d and width %d slices.",
			 parms->priority,
			 key_slices);

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
				cfa_tcam_mgr_row_entry_install(tfp,
							       row, parms,
							       entry, id,
							       key_slices, i,
							       slice);
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
				cfa_tcam_mgr_row_entry_install(tfp,
							       row, parms,
							       entry, id,
							       key_slices, i,
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
		cfa_tcam_mgr_row_entry_install(tfp, row, parms, entry, id,
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
		cfa_tcam_mgr_row_move(tcam_mgr_data, tfp, parms->dir,
				      parms->type,
				      table_data, to_row_idx, to_row,
				      from_row_idx, from_row);
		PMD_DRV_LOG_LINE(INFO, "Moved row %d to row %d.",
				 from_row_idx, to_row_idx);

		to_row = from_row;
		to_row_idx = from_row_idx;
	}
	to_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, target_row, row_size);
	memset(to_row, 0, row_size);
	cfa_tcam_mgr_row_entry_install(tfp, to_row, parms, entry, id,
				       key_slices, target_row,
				       TF_TCAM_SLICE_INVALID);

	return row;
}

/*
 * This function will combine rows when possible to result in the fewest rows
 * used necessary for the entries that are installed.
 */
static void
cfa_tcam_mgr_rows_combine(struct cfa_tcam_mgr_data *tcam_mgr_data,
			  struct tf *tfp,
			  struct cfa_tcam_mgr_free_parms *parms,
			  struct cfa_tcam_mgr_table_data *table_data,
			  int changed_row_index)
{
	int  to_row_idx, from_row_idx, start_row, end_row, max_slices;
	struct cfa_tcam_mgr_table_rows_0 *from_row = NULL;
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	struct cfa_tcam_mgr_table_rows_0 *to_row;
	bool entry_moved = false;
	int  i, j, row_size;

	start_row  = table_data->start_row;
	end_row	   = table_data->end_row;
	max_slices = table_data->max_slices;
	tcam_rows  = table_data->tcam_rows;

	row_size   = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					       parms->type);

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
							(tcam_mgr_data,
							 tfp,
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

#ifdef TF_FLOW_SCALE_QUERY
			/* CFA update usage state when moved entries */
			if (entry_moved) {
				if (tf_tcam_usage_update(tfp,
							 parms->dir,
							 parms->type,
							 to_row,
							 TF_RESC_ALLOC)) {
					PMD_DRV_LOG_LINE(DEBUG, "TF tcam usage update failed");
				}
				if (tf_tcam_usage_update(tfp,
							 parms->dir,
							 parms->type,
							 from_row,
							 TF_RESC_FREE)) {
					PMD_DRV_LOG_LINE(DEBUG, "TF tcam usage update failed");
				}
			}
#endif /* TF_FLOW_SCALE_QUERY */

			if (ROW_INUSE(from_row))
				entry_moved = false;
			else
				break;
		}
	}
}

/*
 * This function will ensure that all rows, except those of the highest
 * priority, are at the end of the table.  When this function is finished, all
 * empty rows should be between the highest priority rows at the beginning of
 * the table and the rest of the rows with lower priorities.
 *
 * Will need to free the row left newly empty as a result of moving.
 * Return row to free to caller.  If new_row_to_free < 0, then no new row to
 * free.
 */
static void
cfa_tcam_mgr_rows_compact(struct cfa_tcam_mgr_data *tcam_mgr_data,
			  struct tf *tfp,
			  struct cfa_tcam_mgr_free_parms *parms,
			  struct cfa_tcam_mgr_table_data *table_data,
			  int *new_row_to_free,
			  int changed_row_index)
{
	int  to_row_idx = 0, from_row_idx = 0, start_row = 0, end_row = 0;
	struct cfa_tcam_mgr_table_rows_0 *from_row = NULL;
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	struct cfa_tcam_mgr_table_rows_0 *to_row;
	struct cfa_tcam_mgr_table_rows_0 *row;
	int  i, row_size, priority;

	*new_row_to_free = -1;

	start_row  = table_data->start_row;
	end_row	   = table_data->end_row;
	tcam_rows  = table_data->tcam_rows;

	row_size   = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					       parms->type);

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
				if (from_row) {
					cfa_tcam_mgr_row_move(tcam_mgr_data,
							      tfp,
							      parms->dir,
							      parms->type,
							      table_data,
							      to_row_idx,
							      to_row,
							      from_row_idx,
							      from_row);
					PMD_DRV_LOG_LINE(INFO,
							 "Moved row %d "
							 "to row %d.",
							 from_row_idx,
							 to_row_idx);
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

	if (from_row) {
		cfa_tcam_mgr_row_move(tcam_mgr_data, tfp, parms->dir,
				      parms->type, table_data, to_row_idx,
				      to_row, from_row_idx, from_row);
		PMD_DRV_LOG_LINE(INFO, "Moved row %d to row %d.",
				 from_row_idx, to_row_idx);
		*new_row_to_free = from_row_idx;
	}
}

/*
 * This function is to set table limits for the logical TCAM tables.
 */
static int
cfa_tcam_mgr_table_limits_set(struct cfa_tcam_mgr_data *tcam_mgr_data,
			      struct cfa_tcam_mgr_init_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	unsigned int dir, type;
	int start, stride;

	if (!parms)
		return 0;

	for (dir = 0; dir < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
	     dir++)
		for (type = 0;
		     type <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     type++) {
			table_data =
				&tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
			/*
			 * If num_rows is zero, then TCAM Manager did not
			 * allocate any row storage for that table so cannot
			 * manage it.
			 */
			if (!table_data->num_rows)
				continue;
			start  = parms->resc[dir][type].start;
			stride = parms->resc[dir][type].stride;
			if (start % table_data->max_slices > 0) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, type,
							  "Start of resources"
							  " (%d) does not begin"
							  " on row boundary.\n",
							  start);
				CFA_TCAM_MGR_LOG_DIR(ERR, dir,
						     "Start is %d, number of"
						     " slices is %d.\n",
						     start,
						     table_data->max_slices);
				return -CFA_TCAM_MGR_ERR_CODE(INVAL);
			}
			if (stride % table_data->max_slices > 0) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, type,
							  "Stride of resources (%d) "
							  " does not end on row boundary.\n",
							  stride);
				CFA_TCAM_MGR_LOG_DIR(ERR, dir,
						     "Stride is %d, number of "
						     "slices is %d.\n",
						     stride,
						     table_data->max_slices);
				return -CFA_TCAM_MGR_ERR_CODE(INVAL);
			}
			if (!stride) {
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

static int
cfa_tcam_mgr_bitmap_alloc(struct tf *tfp __rte_unused,
			  struct cfa_tcam_mgr_data *tcam_mgr_data)
{
	struct tfp_calloc_parms cparms;
	uint64_t session_bmp_size;
	struct bitalloc *session_bmp;
	int32_t first_idx;
	int max_entries;
	int rc;

	if (!tcam_mgr_data->cfa_tcam_mgr_max_entries)
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);

	max_entries = tcam_mgr_data->cfa_tcam_mgr_max_entries;
	session_bmp_size = (sizeof(uint64_t) *
				(((max_entries - 1) / sizeof(uint64_t)) + 1));

	cparms.nitems = 1;
	cparms.size = session_bmp_size;
	cparms.alignment = 0;
	rc = tfp_calloc(&cparms);
	if (rc) {
		/* Log error */
		TFP_DRV_LOG(ERR,
			    "Failed to allocate session bmp, rc:%s\n",
			    strerror(-rc));
		return -CFA_TCAM_MGR_ERR_CODE(NOMEM);
	}

	session_bmp = (struct bitalloc *)cparms.mem_va;
	rc = ba_init(session_bmp, max_entries, true);

	tcam_mgr_data->session_bmp = session_bmp;
	tcam_mgr_data->session_bmp_size = max_entries;

	/* Allocate first index to avoid idx 0 */
	first_idx = ba_alloc(tcam_mgr_data->session_bmp);
	if (first_idx == BA_FAIL) {
		tfp_free(tcam_mgr_data->session_bmp);
		tcam_mgr_data->session_bmp = NULL;
		return -CFA_TCAM_MGR_ERR_CODE(NOSPC);
	}

	TFP_DRV_LOG(DEBUG,
		    "session bitmap size is %" PRIX64 "\n",
		    tcam_mgr_data->session_bmp_size);

	return 0;
}

static void
cfa_tcam_mgr_uninit(struct tf *tfp,
				enum cfa_tcam_mgr_device_type type)
{
	switch (type) {
	case CFA_TCAM_MGR_DEVICE_TYPE_P4:
		cfa_tcam_mgr_uninit_p4(tfp);
		break;
	case CFA_TCAM_MGR_DEVICE_TYPE_P5:
		cfa_tcam_mgr_uninit_p58(tfp);
		break;
	default:
		CFA_TCAM_MGR_LOG(ERR, "No such device %d\n", type);
		return;
	}
}

int
cfa_tcam_mgr_init(struct tf *tfp, enum cfa_tcam_mgr_device_type type,
		  struct cfa_tcam_mgr_init_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	unsigned int dir, tbl_type;
	struct tf_session *tfs;
	int rc;

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	switch (type) {
	case CFA_TCAM_MGR_DEVICE_TYPE_P4:
		rc = cfa_tcam_mgr_init_p4(tfp);
		break;
	case CFA_TCAM_MGR_DEVICE_TYPE_P5:
		rc = cfa_tcam_mgr_init_p58(tfp);
		break;
	default:
		CFA_TCAM_MGR_LOG(ERR, "No such device %d\n", type);
		return -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	rc = cfa_tcam_mgr_table_limits_set(tcam_mgr_data, parms);
	if (rc)
		return rc;

	/* Now calculate the max entries per table and global max entries based
	 * on the updated table limits.
	 */
	tcam_mgr_data->cfa_tcam_mgr_max_entries = 0;
	for (dir = 0; dir < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
	     dir++)
		for (tbl_type = 0;
		     tbl_type <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     tbl_type++) {
			table_data =
				&tcam_mgr_data->cfa_tcam_mgr_tables[dir]
								[tbl_type];
			/*
			 * If num_rows is zero, then TCAM Manager did not
			 * allocate any row storage for that table so cannot
			 * manage it.
			 */
			if (!table_data->num_rows) {
				table_data->start_row = 0;
				table_data->end_row = 0;
				table_data->max_entries = 0;
			} else if (table_data->end_row >=
				   table_data->num_rows) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(EMERG, dir, tbl_type,
							  "End row is out of "
							  "range (%d >= %d)\n",
							  table_data->end_row,
							  table_data->num_rows);
				return -CFA_TCAM_MGR_ERR_CODE(FAULT);
			} else if (!table_data->max_entries &&
				   !table_data->start_row &&
				   !table_data->end_row) {
				/* Nothing to do */
			} else {
				table_data->max_entries =
					table_data->max_slices *
					(table_data->end_row -
					 table_data->start_row + 1);
			}
			tcam_mgr_data->cfa_tcam_mgr_max_entries +=
				table_data->max_entries;
		}

	rc = cfa_tcam_mgr_hwops_init(tcam_mgr_data, type);
	if (rc)
		return rc;

	rc = cfa_tcam_mgr_bitmap_alloc(tfp, tcam_mgr_data);
	if (rc)
		return rc;

	if (parms)
		parms->max_entries = tcam_mgr_data->cfa_tcam_mgr_max_entries;

	PMD_DRV_LOG_LINE(DEBUG,
			 "Global TCAM tbl initialized max entries %d",
			 tcam_mgr_data->cfa_tcam_mgr_max_entries);

	return 0;
}

static
int cfa_tcam_mgr_validate_tcam_cnt(struct tf *tfp  __rte_unused,
				   struct cfa_tcam_mgr_data *tcam_mgr_data,
				   uint16_t tcam_cnt[]
						[CFA_TCAM_MGR_TBL_TYPE_MAX])
{
	struct cfa_tcam_mgr_table_data *table_data;
	unsigned int dir, type;
	uint16_t requested_cnt;

	/* Validate session request */
	for (dir = 0; dir < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
			dir++) {
		for (type = 0;
		     type < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     type++) {
			table_data =
				&tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
			requested_cnt = tcam_cnt[dir][type];
			/* Only check if table supported (max_entries > 0). */
			if (table_data->max_entries > 0 &&
			    requested_cnt > table_data->max_entries) {
				PMD_DRV_LOG_LINE(ERR,
						 "%s: %s Requested %d, available %d",
						 tf_dir_2_str(dir),
						 cfa_tcam_mgr_tbl_2_str(type),
						 requested_cnt,
						 table_data->max_entries);
				return -CFA_TCAM_MGR_ERR_CODE(NOSPC);
			}
		}
	}

	return 0;
}

static int cfa_tcam_mgr_free_entries(struct tf *tfp)
{
	struct cfa_tcam_mgr_free_parms free_parms;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_session *tfs;
	int entry_id = 0;
	int rc = 0;

	PMD_DRV_LOG_LINE(DEBUG, "Unbinding session");

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	memset(&free_parms, 0, sizeof(free_parms));

	/*
	 * Since we are freeing all pending TCAM entries (which is typically
	 * done during tcam_unbind), we don't know the type of each entry.
	 * So we set the type to MAX as a hint to cfa_tcam_mgr_free() to
	 * figure out the actual type. We need to set it through each
	 * iteration in the loop below; otherwise, the type determined for
	 * the first entry would be used for subsequent entries that may or
	 * may not be of the same type, resulting in errors.
	 */

	while ((entry_id = ba_find_next_inuse_free(tcam_mgr_data->session_bmp,
						   0)) >= 0) {
		free_parms.id = entry_id;
		free_parms.type = CFA_TCAM_MGR_TBL_TYPE_MAX;
		cfa_tcam_mgr_free(tfp, &free_parms);
	}

	return 0;
}

/*
 * Manipulate the tables to split the WC TCAM into HIGH and LOW ranges
 * and also update the sizes in the tcam count array
 */
static int
cfa_tcam_mgr_shared_wc_bind(struct tf *tfp, bool dual_ha_app,
			    uint16_t tcam_cnt[][CFA_TCAM_MGR_TBL_TYPE_MAX])
{
	uint16_t start_row, end_row, max_entries, slices;
	uint16_t num_pools = dual_ha_app ? 4 : 2;
	enum tf_dir dir;
	int rc;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		rc = cfa_tcam_mgr_tables_get(tfp, dir,
					     CFA_TCAM_MGR_TBL_TYPE_WC_TCAM,
					     &start_row, &end_row, &max_entries,
					     &slices);
		if (rc)
			return rc;
		if (max_entries) {
			rc = cfa_tcam_mgr_tables_set(tfp, dir,
					     CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH,
						     start_row,
						     start_row +
						     ((max_entries / slices) /
						     num_pools) - 1,
						     max_entries / num_pools);
			if (rc)
				return rc;
			rc = cfa_tcam_mgr_tables_set(tfp, dir,
					      CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW,
						     start_row +
						     ((max_entries / slices) /
						     num_pools),
						     start_row +
						     (max_entries / slices) - 1,
						     max_entries / num_pools);
			if (rc)
				return rc;
			rc = cfa_tcam_mgr_tables_set(tfp, dir,
						  CFA_TCAM_MGR_TBL_TYPE_WC_TCAM,
						     0, 0, 0);
			if (rc)
				return rc;
			tcam_cnt[dir][CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH] =
				max_entries / num_pools;
			tcam_cnt[dir][CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW] =
				max_entries / num_pools;
			tcam_cnt[dir][CFA_TCAM_MGR_TBL_TYPE_WC_TCAM] = 0;
		}
	}

	return 0;
}

int
cfa_tcam_mgr_bind(struct tf *tfp,
		  struct cfa_tcam_mgr_cfg_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	enum cfa_tcam_mgr_device_type device_type;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int prev_max_entries;
	unsigned int type;
	int start, stride;
	unsigned int dir;
	int rc;

	CFA_TCAM_MGR_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
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
	case TF_DEVICE_TYPE_P5:
		device_type = CFA_TCAM_MGR_DEVICE_TYPE_P5;
		break;
	default:
		CFA_TCAM_MGR_LOG(ERR, "No such device %d\n", dev->type);
		return -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		rc = cfa_tcam_mgr_init(tfp, device_type, NULL);
		if (rc)
			return rc;
		tcam_mgr_data = tfs->tcam_mgr_handle;
	}

	if (parms->num_elements !=
		ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir])) {
		CFA_TCAM_MGR_LOG(ERR,
				 "Session element count (%d) differs "
				 "from table count (%zu)\n",
				 parms->num_elements,
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]));
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	/*
	 * Only managing one session. resv_res contains the resources allocated
	 * to this session by the resource manager.  Update the limits on TCAMs.
	 */
	for (dir = 0; dir < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
		dir++) {
		for (type = 0;
		     type <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     type++) {
			table_data =
				&tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
			prev_max_entries = table_data->max_entries;
			/*
			 * In AFM logical tables, max_entries is initialized to
			 * zero.  These logical tables are not used when TCAM
			 * Manager is in the core so skip.
			 */
			if (!prev_max_entries)
				continue;
			start  = parms->resv_res[dir][type].start;
			stride = parms->resv_res[dir][type].stride;
			if (start % table_data->max_slices > 0) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, type,
					 "%s: %s Resource:%d not row bounded\n",
					 tf_dir_2_str(dir),
					 cfa_tcam_mgr_tbl_2_str(type),
					 start);
				CFA_TCAM_MGR_LOG_DIR(ERR, dir,
					    "%s: Start:%d, num slices:%d\n",
					    tf_dir_2_str(dir), start,
					    table_data->max_slices);
				cfa_tcam_mgr_free_entries(tfp);
				return -CFA_TCAM_MGR_ERR_CODE(INVAL);
			}
			if (stride % table_data->max_slices > 0) {
				CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, type,
					   "%s: %s Resource:%d not row bound\n",
					   tf_dir_2_str(dir),
					   cfa_tcam_mgr_tbl_2_str(type),
					   stride);
				CFA_TCAM_MGR_LOG_DIR(ERR, dir,
					    "%s: Stride:%d num slices:%d\n",
					    tf_dir_2_str(dir), stride,
					    table_data->max_slices);
				cfa_tcam_mgr_free_entries(tfp);
				return -CFA_TCAM_MGR_ERR_CODE(INVAL);
			}
			if (!stride) {
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
			tcam_mgr_data->cfa_tcam_mgr_max_entries +=
				(table_data->max_entries - prev_max_entries);
		}
	}

	PMD_DRV_LOG_LINE(DEBUG, "TCAM table bind for max entries %d.",
			 tcam_mgr_data->cfa_tcam_mgr_max_entries);

	if (tf_session_is_shared_hotup_session(tfs)) {
		rc = cfa_tcam_mgr_shared_wc_bind(tfp, false,
						 parms->tcam_cnt);
		if (rc) {
			cfa_tcam_mgr_free_entries(tfp);
			return rc;
		}
	}

	rc = cfa_tcam_mgr_validate_tcam_cnt(tfp, tcam_mgr_data,
					    parms->tcam_cnt);
	if (rc) {
		cfa_tcam_mgr_free_entries(tfp);
		return rc;
	}

#ifdef TF_FLOW_SCALE_QUERY
	/* Initialize the WC TCAM usage state */
	tf_tcam_usage_init(tfp);
#endif /* TF_FLOW_SCALE_QUERY */

	return 0;
}

int
cfa_tcam_mgr_unbind(struct tf *tfp)
{
	enum cfa_tcam_mgr_device_type device_type;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	CFA_TCAM_MGR_CHECK_PARMS1(tfp);

	rc = tf_session_get_session_internal(tfp, &tfs);
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
	case TF_DEVICE_TYPE_P5:
		device_type = CFA_TCAM_MGR_DEVICE_TYPE_P5;
		break;
	default:
		PMD_DRV_LOG_LINE(DEBUG,
				 "TF tcam get dev type failed");
		return -CFA_TCAM_MGR_ERR_CODE(NODEV);
	}

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		PMD_DRV_LOG_LINE(ERR,
				 "No TCAM data created for session");
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	cfa_tcam_mgr_free_entries(tfp);
	cfa_tcam_mgr_uninit(tfp, device_type);

	return 0;
}

static int cfa_tcam_mgr_alloc_entry(struct tf *tfp __rte_unused,
				    struct cfa_tcam_mgr_data *tcam_mgr_data,
				    enum tf_dir dir __rte_unused)
{
	int32_t free_idx;

	/* Scan bitmap to get the free pool */
	free_idx = ba_alloc(tcam_mgr_data->session_bmp);
	if (free_idx == BA_FAIL) {
		PMD_DRV_LOG_LINE(ERR,
				 "Table full (session)");
		return -CFA_TCAM_MGR_ERR_CODE(NOSPC);
	}

	return free_idx;
}

static int cfa_tcam_mgr_free_entry(struct tf *tfp __rte_unused,
				   struct cfa_tcam_mgr_data *tcam_mgr_data,
				   unsigned int entry_id,
				   enum tf_dir dir __rte_unused,
				   enum cfa_tcam_mgr_tbl_type type __rte_unused)
{
	int rc = 0;

	if (entry_id >= tcam_mgr_data->session_bmp_size)
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);

	rc = ba_free(tcam_mgr_data->session_bmp, entry_id);
	if (rc)
		return rc;

	PMD_DRV_LOG_LINE(INFO,
			 "Remove session from entry %d",
			 entry_id);

	return 0;
}

int
cfa_tcam_mgr_alloc(struct tf *tfp,
		   struct cfa_tcam_mgr_alloc_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct cfa_tcam_mgr_entry_data entry;
	struct tf_session *tfs;
	int key_slices, rc;
	int dir, tbl_type;
	int new_entry_id;

	CFA_TCAM_MGR_CHECK_PARMS2(tfp, parms);

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

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		CFA_TCAM_MGR_LOG(ERR, "No TCAM data created for session\n");
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	table_data = &tcam_mgr_data->cfa_tcam_mgr_tables[dir][tbl_type];

	if (!parms->key_size ||
	    parms->key_size > table_data->row_width) {
		CFA_TCAM_MGR_LOG_DIR(ERR, dir,
				     "Invalid key size:%d (range 1-%d).\n",
				     parms->key_size,
				     table_data->row_width);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	/* Check global limits */
	if (table_data->used_entries >=
	    table_data->max_entries) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, dir, tbl_type,
					    "Table full.\n");
		return -CFA_TCAM_MGR_ERR_CODE(NOSPC);
	}

	/* There is room, now increment counts and allocate an entry. */
	new_entry_id = cfa_tcam_mgr_alloc_entry(tfp, tcam_mgr_data, parms->dir);
	if (new_entry_id < 0)
		return new_entry_id;

	memset(&entry, 0, sizeof(entry));
	entry.ref_cnt++;

	PMD_DRV_LOG_LINE(INFO, "Allocated entry ID %d.", new_entry_id);

	key_slices = cfa_tcam_mgr_get_num_slices(parms->key_size,
						 (table_data->row_width /
						  table_data->max_slices));

	row = cfa_tcam_mgr_empty_row_alloc(tcam_mgr_data, tfp, parms, &entry,
					   new_entry_id, key_slices);
	if (!row) {
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR, parms->dir, parms->type,
					    "Table full (HW).\n");
		cfa_tcam_mgr_free_entry(tfp, tcam_mgr_data, new_entry_id,
					parms->dir, parms->type);
		return -CFA_TCAM_MGR_ERR_CODE(NOSPC);
	}

	memcpy(&tcam_mgr_data->entry_data[new_entry_id],
	       &entry,
	       sizeof(tcam_mgr_data->entry_data[new_entry_id]));
	table_data->used_entries += 1;

	cfa_tcam_mgr_entry_insert(tcam_mgr_data, tfp, new_entry_id, &entry);

	parms->id = new_entry_id;

#ifdef TF_FLOW_SCALE_QUERY
	/* CFA update usage state */
	if (tf_tcam_usage_update(tfp,
				 parms->dir,
				 parms->type,
				 row,
				 TF_RESC_ALLOC)) {
		PMD_DRV_LOG_LINE(DEBUG, "TF tcam usage update failed");
	}
#endif /* TF_FLOW_SCALE_QUERY */

	return 0;
}

int
cfa_tcam_mgr_free(struct tf *tfp,
		  struct cfa_tcam_mgr_free_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_entry_data *entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	int row_size, rc, new_row_to_free;
	struct tf_session *tfs;
	uint16_t id;

	CFA_TCAM_MGR_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		CFA_TCAM_MGR_LOG(ERR, "No TCAM data created for session\n");
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	id = parms->id;
	entry = cfa_tcam_mgr_entry_get(tcam_mgr_data, id);
	if (!entry) {
		CFA_TCAM_MGR_LOG(ERR, "Entry %d not found\n", id);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	if (!entry->ref_cnt) {
		CFA_TCAM_MGR_LOG(ERR, "Entry %d not in use.\n", id);
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
		rc = cfa_tcam_mgr_entry_find(tcam_mgr_data, id, &parms->dir,
					     &parms->type);
		if (rc) {
			CFA_TCAM_MGR_LOG(ERR,
					 "Entry %d not in tables\n", id);
			return rc;
		}
		PMD_DRV_LOG_LINE(INFO, "id: %d dir: 0x%x type: 0x%x",
				 id, parms->dir, parms->type);
	}

	table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir][parms->type];
	parms->hcapi_type = table_data->hcapi_type;

	row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					     parms->type);

	row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, entry->row,
				       row_size);

	entry->ref_cnt--;

	cfa_tcam_mgr_free_entry(tfp, tcam_mgr_data, id, parms->dir,
				parms->type);

	if (!entry->ref_cnt) {
		PMD_DRV_LOG_LINE(INFO,
				 "Freeing entry %d, row %d, slice %d.",
				 id, entry->row, entry->slice);
		cfa_tcam_mgr_entry_free_msg(tcam_mgr_data, tfp,
					    parms, entry->row,
					    entry->slice * row->entry_size,
					    table_data->row_width /
					    table_data->max_slices *
					    row->entry_size,
					    table_data->result_size,
					    table_data->max_slices);
		ROW_ENTRY_CLEAR(row, entry->slice);

#ifdef TF_FLOW_SCALE_QUERY
		/* CFA update usage state */
		if (tf_tcam_usage_update(tfp,
					 parms->dir,
					 parms->type,
					 row,
					 TF_RESC_FREE)) {
			PMD_DRV_LOG_LINE(DEBUG, "TF tcam usage update failed");
		}
#endif /* TF_FLOW_SCALE_QUERY */

		new_row_to_free = entry->row;
		cfa_tcam_mgr_rows_combine(tcam_mgr_data, tfp, parms,
					  table_data, new_row_to_free);

		if (!ROW_INUSE(row)) {
			cfa_tcam_mgr_rows_compact(tcam_mgr_data, tfp,
						  parms, table_data,
						  &new_row_to_free,
						  new_row_to_free);
			if (new_row_to_free >= 0)
				cfa_tcam_mgr_entry_free_msg(tcam_mgr_data,
						   tfp, parms,
						   new_row_to_free, 0,
						   table_data->row_width,
						   table_data->result_size,
						   table_data->max_slices);
		}

		cfa_tcam_mgr_entry_delete(tcam_mgr_data, tfp, id);
		table_data->used_entries -= 1;
		PMD_DRV_LOG_LINE(INFO, "Freed entry %d.", id);
	} else {
		PMD_DRV_LOG_LINE(INFO, "Entry %d ref cnt = %d.",
				   id,
				   entry->ref_cnt);
	}

	return 0;
}

int
cfa_tcam_mgr_set(struct tf *tfp,
		 struct cfa_tcam_mgr_set_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_entry_data *entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	int entry_size_in_bytes;
	struct tf_session *tfs;
	int row_size;
	int rc;

	CFA_TCAM_MGR_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		CFA_TCAM_MGR_LOG(ERR, "No TCAM data created for session\n");
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	entry = cfa_tcam_mgr_entry_get(tcam_mgr_data, parms->id);
	if (!entry) {
		CFA_TCAM_MGR_LOG(ERR, "Entry %d not found.\n", parms->id);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir][parms->type];
	parms->hcapi_type = table_data->hcapi_type;

	row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					     parms->type);
	row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, entry->row,
				       row_size);

	entry_size_in_bytes = table_data->row_width /
			      table_data->max_slices *
			      row->entry_size;
	if (parms->key_size != entry_size_in_bytes) {
		CFA_TCAM_MGR_LOG(ERR,
				"Key size(%d) is different from entry "
				"size(%d).\n",
				parms->key_size, entry_size_in_bytes);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	rc = cfa_tcam_mgr_entry_set_msg(tcam_mgr_data, tfp, parms,
					entry->row,
					entry->slice * row->entry_size,
					table_data->max_slices);
	if (rc) {
		CFA_TCAM_MGR_LOG_0(ERR, "Failed to set TCAM data.\n");
		return rc;
	}

	PMD_DRV_LOG_LINE(INFO, "Set data for entry %d", parms->id);

	return 0;
}

int
cfa_tcam_mgr_get(struct tf *tfp __rte_unused,
		 struct cfa_tcam_mgr_get_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_entry_data *entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct tf_session *tfs;
	int row_size;
	int rc;

	CFA_TCAM_MGR_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		CFA_TCAM_MGR_LOG(ERR, "No TCAM data created for session\n");
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	entry = cfa_tcam_mgr_entry_get(tcam_mgr_data, parms->id);
	if (!entry) {
		CFA_TCAM_MGR_LOG(ERR, "Entry %d not found.\n", parms->id);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir][parms->type];
	parms->hcapi_type = table_data->hcapi_type;

	row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					     parms->type);
	row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, entry->row,
				       row_size);

	rc = cfa_tcam_mgr_entry_get_msg(tcam_mgr_data, tfp, parms,
					entry->row,
					entry->slice * row->entry_size,
					table_data->max_slices);
	if (rc) {
		CFA_TCAM_MGR_LOG_0(ERR, "Failed to read from TCAM.\n");
		return rc;
	}

	return 0;
}

int cfa_tcam_mgr_shared_clear(struct tf *tfp,
			      struct cfa_tcam_mgr_shared_clear_parms *parms)
{
	uint16_t start_row, end_row, max_entries, max_slices;
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_free_parms fparms;
	uint16_t row, slice = 0;
	struct tf_session *tfs;
	int rc;

	CFA_TCAM_MGR_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		CFA_TCAM_MGR_LOG(ERR, "No TCAM data created for session\n");
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir][parms->type];
	fparms.dir = parms->dir;
	fparms.type = parms->type;
	fparms.hcapi_type = table_data->hcapi_type;
	fparms.id = 0;

	rc = cfa_tcam_mgr_tables_get(tfp, parms->dir, parms->type,
				     &start_row, &end_row, &max_entries,
				     &max_slices);
	if (rc)
		return rc;

	for (row = start_row; row <= end_row; row++) {
		cfa_tcam_mgr_entry_free_msg(tcam_mgr_data, tfp, &fparms,
					    row,
					    slice,
					    table_data->row_width,
					    table_data->result_size,
					    table_data->max_slices);
	}
	return rc;
}

static void
cfa_tcam_mgr_mv_used_entries_cnt(struct cfa_tcam_mgr_table_data *dst_table_data,
				 struct cfa_tcam_mgr_table_data *src_table_data)
{
	dst_table_data->used_entries++;
	src_table_data->used_entries--;
}

/*
 * Move HI WC TCAM entries to LOW TCAM region for HA
 * This happens when secondary is becoming primary
 */
static int
cfa_tcam_mgr_shared_entry_move(struct cfa_tcam_mgr_data *tcam_mgr_data,
			       struct tf *tfp,
			       enum tf_dir dir, enum cfa_tcam_mgr_tbl_type type,
			       int entry_id,
			       struct cfa_tcam_mgr_table_data *dst_table_data,
			       struct cfa_tcam_mgr_table_data *table_data,
			       int dst_row_index, int dst_row_slice,
			       struct cfa_tcam_mgr_table_rows_0 *dst_row,
			       int src_row_index,
			       struct cfa_tcam_mgr_table_rows_0 *src_row)
{
	struct cfa_tcam_mgr_free_parms fparms = { 0 };
	struct cfa_tcam_mgr_get_parms gparms = { 0 };
	struct cfa_tcam_mgr_set_parms sparms = { 0 };
	uint8_t  result[CFA_TCAM_MGR_MAX_KEY_SIZE];
	uint8_t  mask[CFA_TCAM_MGR_MAX_KEY_SIZE];
	uint8_t  key[CFA_TCAM_MGR_MAX_KEY_SIZE];
	struct cfa_tcam_mgr_entry_data *entry;
	int rc;

	/*
	 * Copy entry size before moving else if
	 * slice number is non zero and entry size is zero it will cause issues
	 */
	dst_row->entry_size = src_row->entry_size;

	entry = cfa_tcam_mgr_entry_get(tcam_mgr_data, entry_id);
	if (!entry)
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

	rc = cfa_tcam_mgr_entry_get_msg(tcam_mgr_data, tfp, &gparms,
					src_row_index,
					entry->slice * src_row->entry_size,
					table_data->max_slices);
	if (rc)
		return rc;

	sparms.dir	   = dir;
	sparms.type	   = CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW;
	sparms.hcapi_type  = table_data->hcapi_type;
	sparms.key	   = key;
	sparms.mask	   = mask;
	sparms.result	   = result;
	sparms.id	   = gparms.id;
	sparms.key_size	   = gparms.key_size;
	sparms.result_size = gparms.result_size;

	rc = cfa_tcam_mgr_entry_set_msg(tcam_mgr_data, tfp, &sparms,
					dst_row_index,
					dst_row_slice * dst_row->entry_size,
					table_data->max_slices);
	if (rc)
		return rc;

	fparms.dir	  = dir;
	fparms.type	  = type;
	fparms.hcapi_type = table_data->hcapi_type;
	rc = cfa_tcam_mgr_entry_free_msg(tcam_mgr_data, tfp, &fparms,
					 src_row_index,
					 entry->slice *
					 dst_row->entry_size,
					 table_data->row_width /
					 table_data->max_slices *
					 src_row->entry_size,
					 table_data->result_size,
					 table_data->max_slices);
	if (rc)
		CFA_TCAM_MGR_LOG_DIR_TYPE(ERR,
					  dir, type,
					  "Failed to free entry ID %d at"
					  " row %d, slice %d. rc: %d.\n",
					  gparms.id,
					  src_row_index,
					  entry->slice,
					  -rc);

#ifdef CFA_TCAM_MGR_TRACING
	PMD_DRV_LOG_LINE(INFO, "Moved entry %d from row %d, slice %d to "
			 "row %d, slice %d.\n",
			 entry_id, src_row_index, entry->slice,
			 dst_row_index, dst_row_slice);
#endif

	ROW_ENTRY_SET(dst_row, dst_row_slice);
	dst_row->entries[dst_row_slice] = entry_id;
	dst_row->priority = src_row->priority;
	ROW_ENTRY_CLEAR(src_row, entry->slice);
	entry->row = dst_row_index;
	entry->slice = dst_row_slice;

	cfa_tcam_mgr_mv_used_entries_cnt(dst_table_data, table_data);

	return 0;
}

int cfa_tcam_mgr_shared_move(struct tf *tfp,
			     struct cfa_tcam_mgr_shared_move_parms *parms)
{
	struct cfa_tcam_mgr_table_rows_0 *src_table_row;
	struct cfa_tcam_mgr_table_rows_0 *dst_table_row;
	struct cfa_tcam_mgr_table_data *src_table_data;
	struct cfa_tcam_mgr_table_data *dst_table_data;
	uint16_t src_row, dst_row, row_size, slice;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_session *tfs;
	int rc;

	CFA_TCAM_MGR_CHECK_PARMS2(tfp, parms);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc < 0)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		CFA_TCAM_MGR_LOG(ERR, "No TCAM data created for session\n");
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	src_table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir]
			[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH];
	dst_table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir]
			[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW];

	row_size =
		cfa_tcam_mgr_row_size_get(tcam_mgr_data,
					  parms->dir,
					  CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH);

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
					PMD_DRV_LOG_LINE(INFO, "Move entry id %d "
							 "from src_row %d, slice %d "
							 "to dst_row %d, slice %d",
							 src_table_row->entries[slice],
							 src_row, slice,
							 dst_row, slice);
#endif
			rc = cfa_tcam_mgr_shared_entry_move(tcam_mgr_data,
							tfp,
							parms->dir,
					     CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH,
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
cfa_tcam_mgr_tbl_get(struct cfa_tcam_mgr_data *tcam_mgr_data, enum tf_dir dir,
		     enum cfa_tcam_mgr_tbl_type type,
		     uint16_t *start_row,
		     uint16_t *end_row,
		     uint16_t *max_entries,
		     uint16_t *slices)
{
	struct cfa_tcam_mgr_table_data *table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];

	/* Get start, end and max for tcam type*/
	*start_row = table_data->start_row;
	*end_row = table_data->end_row;
	*max_entries = table_data->max_entries;
	*slices = table_data->max_slices;
}

int
cfa_tcam_mgr_tables_get(struct tf *tfp, enum tf_dir dir,
			enum cfa_tcam_mgr_tbl_type type,
			uint16_t *start_row,
			uint16_t *end_row,
			uint16_t *max_entries,
			uint16_t *slices)
{
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_session *tfs;
	int rc;

	CFA_TCAM_MGR_CHECK_PARMS3(start_row, end_row, max_entries);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		CFA_TCAM_MGR_LOG_0(ERR, "No TCAM data created for session.\n");
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	if (dir >= TF_DIR_MAX) {
		CFA_TCAM_MGR_LOG(ERR, "Must specify valid dir (0-%d).\n",
				 TF_DIR_MAX - 1);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		CFA_TCAM_MGR_LOG(ERR, "Must specify valid tbl type (0-%d).\n",
				 CFA_TCAM_MGR_TBL_TYPE_MAX - 1);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	cfa_tcam_mgr_tbl_get(tcam_mgr_data, dir,
			     type,
			     start_row,
			     end_row,
			     max_entries,
			     slices);
	return 0;
}

static void
cfa_tcam_mgr_tbl_set(struct cfa_tcam_mgr_data *tcam_mgr_data, enum tf_dir dir,
		     enum cfa_tcam_mgr_tbl_type type,
		     uint16_t start_row,
		     uint16_t end_row,
		     uint16_t max_entries)
{
	struct cfa_tcam_mgr_table_data *table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];

	/* Update start, end and max for tcam type*/
	table_data->start_row = start_row;
	table_data->end_row = end_row;
	table_data->max_entries = max_entries;
}

int
cfa_tcam_mgr_tables_set(struct tf *tfp, enum tf_dir dir,
			enum cfa_tcam_mgr_tbl_type type,
			uint16_t start_row,
			uint16_t end_row,
			uint16_t max_entries)
{
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_session *tfs;
	int rc;

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		CFA_TCAM_MGR_LOG_0(ERR, "No TCAM data created for session.\n");
		return -CFA_TCAM_MGR_ERR_CODE(PERM);
	}

	if (dir >= TF_DIR_MAX) {
		CFA_TCAM_MGR_LOG(ERR, "Must specify valid dir (0-%d).\n",
				 TF_DIR_MAX - 1);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		CFA_TCAM_MGR_LOG(ERR, "Must specify valid tbl type (0-%d).\n",
				 CFA_TCAM_MGR_TBL_TYPE_MAX - 1);
		return -CFA_TCAM_MGR_ERR_CODE(INVAL);
	}

	cfa_tcam_mgr_tbl_set(tcam_mgr_data, dir,
			     type,
			     start_row,
			     end_row,
			     max_entries);
	return 0;
}
