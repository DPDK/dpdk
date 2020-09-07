/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#include "ice_acl.h"
#include "ice_flow.h"

/* Determine the TCAM index of entry 'e' within the ACL table */
#define ICE_ACL_TBL_TCAM_IDX(e) ((e) / ICE_AQC_ACL_TCAM_DEPTH)

/* Determine the entry index within the TCAM */
#define ICE_ACL_TBL_TCAM_ENTRY_IDX(e) ((e) % ICE_AQC_ACL_TCAM_DEPTH)

#define ICE_ACL_SCEN_ENTRY_INVAL 0xFFFF

/**
 * ice_acl_init_entry
 * @scen: pointer to the scenario struct
 *
 * Initialize the scenario control structure.
 */
static void ice_acl_init_entry(struct ice_acl_scen *scen)
{
	/* low priority: start from the highest index, 25% of total entries
	 * normal priority: start from the highest index, 50% of total entries
	 * high priority: start from the lowest index, 25% of total entries
	 */
	scen->first_idx[ICE_ACL_PRIO_LOW] = scen->num_entry - 1;
	scen->first_idx[ICE_ACL_PRIO_NORMAL] = scen->num_entry -
		scen->num_entry / 4 - 1;
	scen->first_idx[ICE_ACL_PRIO_HIGH] = 0;

	scen->last_idx[ICE_ACL_PRIO_LOW] = scen->num_entry -
		scen->num_entry / 4;
	scen->last_idx[ICE_ACL_PRIO_NORMAL] = scen->num_entry / 4;
	scen->last_idx[ICE_ACL_PRIO_HIGH] = scen->num_entry / 4 - 1;
}

/**
 * ice_acl_scen_assign_entry_idx
 * @scen: pointer to the scenario struct
 * @prio: the priority of the flow entry being allocated
 *
 * To find the index of an available entry in scenario
 *
 * Returns ICE_ACL_SCEN_ENTRY_INVAL if fails
 * Returns index on success
 */
static u16
ice_acl_scen_assign_entry_idx(struct ice_acl_scen *scen,
			      enum ice_acl_entry_prio prio)
{
	u16 first_idx, last_idx, i;
	s8 step;

	if (prio >= ICE_ACL_MAX_PRIO)
		return ICE_ACL_SCEN_ENTRY_INVAL;

	first_idx = scen->first_idx[prio];
	last_idx = scen->last_idx[prio];
	step = first_idx <= last_idx ? 1 : -1;

	for (i = first_idx; i != last_idx + step; i += step)
		if (!ice_test_and_set_bit(i, scen->entry_bitmap))
			return i;

	return ICE_ACL_SCEN_ENTRY_INVAL;
}

/**
 * ice_acl_scen_free_entry_idx
 * @scen: pointer to the scenario struct
 * @idx: the index of the flow entry being de-allocated
 *
 * To mark an entry available in scenario
 */
static enum ice_status
ice_acl_scen_free_entry_idx(struct ice_acl_scen *scen, u16 idx)
{
	if (idx >= scen->num_entry)
		return ICE_ERR_MAX_LIMIT;

	if (!ice_test_and_clear_bit(idx, scen->entry_bitmap))
		return ICE_ERR_DOES_NOT_EXIST;

	return ICE_SUCCESS;
}

/**
 * ice_acl_tbl_calc_end_idx
 * @start: start index of the TCAM entry of this partition
 * @num_entries: number of entries in this partition
 * @width: width of a partition in number of TCAMs
 *
 * Calculate the end entry index for a partition with starting entry index
 * 'start', entries 'num_entries', and width 'width'.
 */
static u16 ice_acl_tbl_calc_end_idx(u16 start, u16 num_entries, u16 width)
{
	u16 end_idx, add_entries = 0;

	end_idx = start + (num_entries - 1);

	/* In case that our ACL partition requires cascading TCAMs */
	if (width > 1) {
		u16 num_stack_level;

		/* Figure out the TCAM stacked level in this ACL scenario */
		num_stack_level = (start % ICE_AQC_ACL_TCAM_DEPTH) +
			num_entries;
		num_stack_level = DIVIDE_AND_ROUND_UP(num_stack_level,
						      ICE_AQC_ACL_TCAM_DEPTH);

		/* In this case, each entries in our ACL partition span
		 * multiple TCAMs. Thus, we will need to add
		 * ((width - 1) * num_stack_level) TCAM's entries to
		 * end_idx.
		 *
		 * For example : In our case, our scenario is 2x2:
		 *	[TCAM 0]	[TCAM 1]
		 *	[TCAM 2]	[TCAM 3]
		 * Assuming that a TCAM will have 512 entries. If "start"
		 * is 500, "num_entries" is 3 and "width" = 2, then end_idx
		 * should be 1024 (belongs to TCAM 2).
		 * Before going to this if statement, end_idx will have the
		 * value of 512. If "width" is 1, then the final value of
		 * end_idx is 512. However, in our case, width is 2, then we
		 * will need add (2 - 1) * 1 * 512. As result, end_idx will
		 * have the value of 1024.
		 */
		add_entries = (width - 1) * num_stack_level *
			ICE_AQC_ACL_TCAM_DEPTH;
	}

	return end_idx + add_entries;
}

/**
 * ice_acl_init_tbl
 * @hw: pointer to the hardware structure
 *
 * Initialize the ACL table by invalidating TCAM entries and action pairs.
 */
static enum ice_status ice_acl_init_tbl(struct ice_hw *hw)
{
	struct ice_aqc_actpair act_buf;
	struct ice_aqc_acl_data buf;
	enum ice_status status = ICE_SUCCESS;
	struct ice_acl_tbl *tbl;
	u8 tcam_idx, i;
	u16 idx;

	tbl = hw->acl_tbl;
	if (!tbl)
		return ICE_ERR_CFG;

	ice_memset(&buf, 0, sizeof(buf), ICE_NONDMA_MEM);
	ice_memset(&act_buf, 0, sizeof(act_buf), ICE_NONDMA_MEM);

	tcam_idx = tbl->first_tcam;
	idx = tbl->first_entry;
	while (tcam_idx < tbl->last_tcam ||
	       (tcam_idx == tbl->last_tcam && idx <= tbl->last_entry)) {
		/* Use the same value for entry_key and entry_key_inv since
		 * we are initializing the fields to 0
		 */
		status = ice_aq_program_acl_entry(hw, tcam_idx, idx, &buf,
						  NULL);
		if (status)
			return status;

		if (++idx > tbl->last_entry) {
			tcam_idx++;
			idx = tbl->first_entry;
		}
	}

	for (i = 0; i < ICE_AQC_MAX_ACTION_MEMORIES; i++) {
		u16 act_entry_idx, start, end;

		if (tbl->act_mems[i].act_mem == ICE_ACL_ACT_PAIR_MEM_INVAL)
			continue;

		start = tbl->first_entry;
		end = tbl->last_entry;

		for (act_entry_idx = start; act_entry_idx <= end;
		     act_entry_idx++) {
			/* Invalidate all allocated action pairs */
			status = ice_aq_program_actpair(hw, i, act_entry_idx,
							&act_buf, NULL);
			if (status)
				return status;
		}
	}

	return status;
}

/**
 * ice_acl_assign_act_mems_to_tcam
 * @tbl: pointer to ACL table structure
 * @cur_tcam: Index of current TCAM. Value = 0 to (ICE_AQC_ACL_SLICES - 1)
 * @cur_mem_idx: Index of current action memory bank. Value = 0 to
 *		 (ICE_AQC_MAX_ACTION_MEMORIES - 1)
 * @num_mem: Number of action memory banks for this TCAM
 *
 * Assign "num_mem" valid action memory banks from "curr_mem_idx" to
 * "curr_tcam" TCAM.
 */
static void
ice_acl_assign_act_mems_to_tcam(struct ice_acl_tbl *tbl, u8 cur_tcam,
				u8 *cur_mem_idx, u8 num_mem)
{
	u8 mem_cnt;

	for (mem_cnt = 0;
	     *cur_mem_idx < ICE_AQC_MAX_ACTION_MEMORIES && mem_cnt < num_mem;
	     (*cur_mem_idx)++) {
		struct ice_acl_act_mem *p_mem = &tbl->act_mems[*cur_mem_idx];

		if (p_mem->act_mem == ICE_ACL_ACT_PAIR_MEM_INVAL)
			continue;

		p_mem->member_of_tcam = cur_tcam;

		mem_cnt++;
	}
}

/**
 * ice_acl_divide_act_mems_to_tcams
 * @tbl: pointer to ACL table structure
 *
 * Figure out how to divide given action memory banks to given TCAMs. This
 * division is for SW book keeping. In the time when scenario is created,
 * an action memory bank can be used for different TCAM.
 *
 * For example, given that we have 2x2 ACL table with each table entry has
 * 2 action memory pairs. As the result, we will have 4 TCAMs (T1,T2,T3,T4)
 * and 4 action memory banks (A1,A2,A3,A4)
 *	[T1 - T2] { A1 - A2 }
 *	[T3 - T4] { A3 - A4 }
 * In the time when we need to create a scenario, for example, 2x1 scenario,
 * we will use [T3,T4] in a cascaded layout. As it is a requirement that all
 * action memory banks in a cascaded TCAM's row will need to associate with
 * the last TCAM. Thus, we will associate action memory banks [A3] and [A4]
 * for TCAM [T4].
 * For SW book-keeping purpose, we will keep theoretical maps between TCAM
 * [Tn] to action memory bank [An].
 */
static void ice_acl_divide_act_mems_to_tcams(struct ice_acl_tbl *tbl)
{
	u16 num_cscd, stack_level, stack_idx, min_act_mem;
	u8 tcam_idx = tbl->first_tcam;
	u16 max_idx_to_get_extra;
	u8 mem_idx = 0;

	/* Determine number of stacked TCAMs */
	stack_level = DIVIDE_AND_ROUND_UP(tbl->info.depth,
					  ICE_AQC_ACL_TCAM_DEPTH);

	/* Determine number of cascaded TCAMs */
	num_cscd = DIVIDE_AND_ROUND_UP(tbl->info.width,
				       ICE_AQC_ACL_KEY_WIDTH_BYTES);

	/* In a line of cascaded TCAM, given the number of action memory
	 * banks per ACL table entry, we want to fairly divide these action
	 * memory banks between these TCAMs.
	 *
	 * For example, there are 3 TCAMs (TCAM 3,4,5) in a line of
	 * cascaded TCAM, and there are 7 act_mems for each ACL table entry.
	 * The result is:
	 *	[TCAM_3 will have 3 act_mems]
	 *	[TCAM_4 will have 2 act_mems]
	 *	[TCAM_5 will have 2 act_mems]
	 */
	min_act_mem = tbl->info.entry_act_pairs / num_cscd;
	max_idx_to_get_extra = tbl->info.entry_act_pairs % num_cscd;

	for (stack_idx = 0; stack_idx < stack_level; stack_idx++) {
		u16 i;

		for (i = 0; i < num_cscd; i++) {
			u8 total_act_mem = min_act_mem;

			if (i < max_idx_to_get_extra)
				total_act_mem++;

			ice_acl_assign_act_mems_to_tcam(tbl, tcam_idx,
							&mem_idx,
							total_act_mem);

			tcam_idx++;
		}
	}
}

/**
 * ice_acl_create_tbl
 * @hw: pointer to the HW struct
 * @params: parameters for the table to be created
 *
 * Create a LEM table for ACL usage. We are currently starting with some fixed
 * values for the size of the table, but this will need to grow as more flow
 * entries are added by the user level.
 */
enum ice_status
ice_acl_create_tbl(struct ice_hw *hw, struct ice_acl_tbl_params *params)
{
	u16 width, depth, first_e, last_e, i;
	struct ice_aqc_acl_generic *resp_buf;
	struct ice_acl_alloc_tbl tbl_alloc;
	struct ice_acl_tbl *tbl;
	enum ice_status status;

	if (hw->acl_tbl)
		return ICE_ERR_ALREADY_EXISTS;

	if (!params)
		return ICE_ERR_PARAM;

	/* round up the width to the next TCAM width boundary. */
	width = ROUND_UP(params->width, (u16)ICE_AQC_ACL_KEY_WIDTH_BYTES);
	/* depth should be provided in chunk (64 entry) increments */
	depth = ICE_ALIGN(params->depth, ICE_ACL_ENTRY_ALLOC_UNIT);

	if (params->entry_act_pairs < width / ICE_AQC_ACL_KEY_WIDTH_BYTES) {
		params->entry_act_pairs = width / ICE_AQC_ACL_KEY_WIDTH_BYTES;

		if (params->entry_act_pairs > ICE_AQC_TBL_MAX_ACTION_PAIRS)
			params->entry_act_pairs = ICE_AQC_TBL_MAX_ACTION_PAIRS;
	}

	/* Validate that width*depth will not exceed the TCAM limit */
	if ((DIVIDE_AND_ROUND_UP(depth, ICE_AQC_ACL_TCAM_DEPTH) *
	     (width / ICE_AQC_ACL_KEY_WIDTH_BYTES)) > ICE_AQC_ACL_SLICES)
		return ICE_ERR_MAX_LIMIT;

	ice_memset(&tbl_alloc, 0, sizeof(tbl_alloc), ICE_NONDMA_MEM);
	tbl_alloc.width = width;
	tbl_alloc.depth = depth;
	tbl_alloc.act_pairs_per_entry = params->entry_act_pairs;
	tbl_alloc.concurr = params->concurr;
	/* Set dependent_alloc_id only for concurrent table type */
	if (params->concurr) {
		tbl_alloc.num_dependent_alloc_ids =
			ICE_AQC_MAX_CONCURRENT_ACL_TBL;

		for (i = 0; i < ICE_AQC_MAX_CONCURRENT_ACL_TBL; i++)
			tbl_alloc.buf.data_buf.alloc_ids[i] =
				CPU_TO_LE16(params->dep_tbls[i]);
	}

	/* call the AQ command to create the ACL table with these values */
	status = ice_aq_alloc_acl_tbl(hw, &tbl_alloc, NULL);
	if (status) {
		if (LE16_TO_CPU(tbl_alloc.buf.resp_buf.alloc_id) <
		    ICE_AQC_ALLOC_ID_LESS_THAN_4K)
			ice_debug(hw, ICE_DBG_ACL, "Alloc ACL table failed. Unavailable resource.\n");
		else
			ice_debug(hw, ICE_DBG_ACL, "AQ allocation of ACL failed with error. status: %d\n",
				  status);
		return status;
	}

	tbl = (struct ice_acl_tbl *)ice_malloc(hw, sizeof(*tbl));
	if (!tbl) {
		status = ICE_ERR_NO_MEMORY;

		goto out;
	}

	resp_buf = &tbl_alloc.buf.resp_buf;

	/* Retrieve information of the allocated table */
	tbl->id = LE16_TO_CPU(resp_buf->alloc_id);
	tbl->first_tcam = resp_buf->ops.table.first_tcam;
	tbl->last_tcam = resp_buf->ops.table.last_tcam;
	tbl->first_entry = LE16_TO_CPU(resp_buf->first_entry);
	tbl->last_entry = LE16_TO_CPU(resp_buf->last_entry);

	tbl->info = *params;
	tbl->info.width = width;
	tbl->info.depth = depth;
	hw->acl_tbl = tbl;

	for (i = 0; i < ICE_AQC_MAX_ACTION_MEMORIES; i++)
		tbl->act_mems[i].act_mem = resp_buf->act_mem[i];

	/* Figure out which TCAMs that these newly allocated action memories
	 * belong to.
	 */
	ice_acl_divide_act_mems_to_tcams(tbl);

	/* Initialize the resources allocated by invalidating all TCAM entries
	 * and all the action pairs
	 */
	status = ice_acl_init_tbl(hw);
	if (status) {
		ice_free(hw, tbl);
		hw->acl_tbl = NULL;
		ice_debug(hw, ICE_DBG_ACL, "Initialization of TCAM entries failed. status: %d\n",
			  status);
		goto out;
	}

	first_e = (tbl->first_tcam * ICE_AQC_MAX_TCAM_ALLOC_UNITS) +
		(tbl->first_entry / ICE_ACL_ENTRY_ALLOC_UNIT);
	last_e = (tbl->last_tcam * ICE_AQC_MAX_TCAM_ALLOC_UNITS) +
		(tbl->last_entry / ICE_ACL_ENTRY_ALLOC_UNIT);

	/* Indicate available entries in the table */
	ice_bitmap_set(tbl->avail, first_e, last_e - first_e + 1);

	INIT_LIST_HEAD(&tbl->scens);
out:

	return status;
}

/**
 * ice_acl_alloc_partition - Allocate a partition from the ACL table
 * @hw: pointer to the hardware structure
 * @req: info of partition being allocated
 */
static enum ice_status
ice_acl_alloc_partition(struct ice_hw *hw, struct ice_acl_scen *req)
{
	u16 start = 0, cnt = 0, off = 0;
	u16 width, r_entries, row;
	bool done = false;
	int dir;

	/* Determine the number of TCAMs each entry overlaps */
	width = DIVIDE_AND_ROUND_UP(req->width, ICE_AQC_ACL_KEY_WIDTH_BYTES);

	/* Check if we have enough TCAMs to accommodate the width */
	if (width > hw->acl_tbl->last_tcam - hw->acl_tbl->first_tcam + 1)
		return ICE_ERR_MAX_LIMIT;

	/* Number of entries must be multiple of ICE_ACL_ENTRY_ALLOC_UNIT's */
	r_entries = ICE_ALIGN(req->num_entry, ICE_ACL_ENTRY_ALLOC_UNIT);

	/* To look for an available partition that can accommodate the request,
	 * the process first logically arranges available TCAMs in rows such
	 * that each row produces entries with the requested width. It then
	 * scans the TCAMs' available bitmap, one bit at a time, and
	 * accumulates contiguous available 64-entry chunks until there are
	 * enough of them or when all TCAM configurations have been checked.
	 *
	 * For width of 1 TCAM, the scanning process starts from the top most
	 * TCAM, and goes downward. Available bitmaps are examined from LSB
	 * to MSB.
	 *
	 * For width of multiple TCAMs, the process starts from the bottom-most
	 * row of TCAMs, and goes upward. Available bitmaps are examined from
	 * the MSB to the LSB.
	 *
	 * To make sure that adjacent TCAMs can be logically arranged in the
	 * same row, the scanning process may have multiple passes. In each
	 * pass, the first TCAM of the bottom-most row is displaced by one
	 * additional TCAM. The width of the row and the number of the TCAMs
	 * available determine the number of passes. When the displacement is
	 * more than the size of width, the TCAM row configurations will
	 * repeat. The process will terminate when the configurations repeat.
	 *
	 * Available partitions can span more than one row of TCAMs.
	 */
	if (width == 1) {
		row = hw->acl_tbl->first_tcam;
		dir = 1;
	} else {
		/* Start with the bottom-most row, and scan for available
		 * entries upward
		 */
		row = hw->acl_tbl->last_tcam + 1 - width;
		dir = -1;
	}

	do {
		u16 i;

		/* Scan all 64-entry chunks, one chunk at a time, in the
		 * current TCAM row
		 */
		for (i = 0;
		     i < ICE_AQC_MAX_TCAM_ALLOC_UNITS && cnt < r_entries;
		     i++) {
			bool avail = true;
			u16 w, p;

			/* Compute the cumulative available mask across the
			 * TCAM row to determine if the current 64-entry chunk
			 * is available.
			 */
			p = dir > 0 ? i : ICE_AQC_MAX_TCAM_ALLOC_UNITS - i - 1;
			for (w = row; w < row + width && avail; w++) {
				u16 b;

				b = (w * ICE_AQC_MAX_TCAM_ALLOC_UNITS) + p;
				avail &= ice_is_bit_set(hw->acl_tbl->avail, b);
			}

			if (!avail) {
				cnt = 0;
			} else {
				/* Compute the starting index of the newly
				 * found partition. When 'dir' is negative, the
				 * scan processes is going upward. If so, the
				 * starting index needs to be updated for every
				 * available 64-entry chunk found.
				 */
				if (!cnt || dir < 0)
					start = (row * ICE_AQC_ACL_TCAM_DEPTH) +
						(p * ICE_ACL_ENTRY_ALLOC_UNIT);
				cnt += ICE_ACL_ENTRY_ALLOC_UNIT;
			}
		}

		if (cnt >= r_entries) {
			req->start = start;
			req->num_entry = r_entries;
			req->end = ice_acl_tbl_calc_end_idx(start, r_entries,
							    width);
			break;
		}

		row = dir > 0 ? row + width : row - width;
		if (row > hw->acl_tbl->last_tcam ||
		    row < hw->acl_tbl->first_tcam) {
			/* All rows have been checked. Increment 'off' that
			 * will help yield a different TCAM configuration in
			 * which adjacent TCAMs can be alternatively in the
			 * same row.
			 */
			off++;

			/* However, if the new 'off' value yields previously
			 * checked configurations, then exit.
			 */
			if (off >= width)
				done = true;
			else
				row = dir > 0 ? off :
					hw->acl_tbl->last_tcam + 1 - off -
					width;
		}
	} while (!done);

	return cnt >= r_entries ? ICE_SUCCESS : ICE_ERR_MAX_LIMIT;
}

/**
 * ice_acl_fill_tcam_select
 * @scen_buf: Pointer to the scenario buffer that needs to be populated
 * @scen: Pointer to the available space for the scenario
 * @tcam_idx: Index of the TCAM used for this scenario
 * @tcam_idx_in_cascade : Local index of the TCAM in the cascade scenario
 *
 * For all TCAM that participate in this scenario, fill out the tcam_select
 * value.
 */
static void
ice_acl_fill_tcam_select(struct ice_aqc_acl_scen *scen_buf,
			 struct ice_acl_scen *scen, u16 tcam_idx,
			 u16 tcam_idx_in_cascade)
{
	u16 cascade_cnt, idx;
	u8 j;

	idx = tcam_idx_in_cascade * ICE_AQC_ACL_KEY_WIDTH_BYTES;
	cascade_cnt = DIVIDE_AND_ROUND_UP(scen->width,
					  ICE_AQC_ACL_KEY_WIDTH_BYTES);

	/* For each scenario, we reserved last three bytes of scenario width for
	 * profile ID, range checker, and packet direction. Thus, the last three
	 * bytes of the last cascaded TCAMs will have value of 1st, 31st and
	 * 32nd byte location of BYTE selection base.
	 *
	 * For other bytes in the TCAMs:
	 * For non-cascade mode (1 TCAM wide) scenario, TCAM[x]'s Select {0-1}
	 * select indices 0-1 of the Byte Selection Base
	 * For cascade mode, the leftmost TCAM of the first cascade row selects
	 * indices 0-4 of the Byte Selection Base; the second TCAM in the
	 * cascade row selects indices starting with 5-n
	 */
	for (j = 0; j < ICE_AQC_ACL_KEY_WIDTH_BYTES; j++) {
		/* PKT DIR uses the 1st location of Byte Selection Base: + 1 */
		u8 val = ICE_AQC_ACL_BYTE_SEL_BASE + 1 + idx;

		if (tcam_idx_in_cascade == cascade_cnt - 1) {
			if (j == ICE_ACL_SCEN_RNG_CHK_IDX_IN_TCAM)
				val = ICE_AQC_ACL_BYTE_SEL_BASE_RNG_CHK;
			else if (j == ICE_ACL_SCEN_PID_IDX_IN_TCAM)
				val = ICE_AQC_ACL_BYTE_SEL_BASE_PID;
			else if (j == ICE_ACL_SCEN_PKT_DIR_IDX_IN_TCAM)
				val = ICE_AQC_ACL_BYTE_SEL_BASE_PKT_DIR;
		}

		/* In case that scenario's width is greater than the width of
		 * the Byte selection base, we will not assign a value to the
		 * tcam_select[j]. As a result, the tcam_select[j] will have
		 * default value which is zero.
		 */
		if (val > ICE_AQC_ACL_BYTE_SEL_BASE_RNG_CHK)
			continue;

		scen_buf->tcam_cfg[tcam_idx].tcam_select[j] = val;

		idx++;
	}
}

/**
 * ice_acl_set_scen_chnk_msk
 * @scen_buf: Pointer to the scenario buffer that needs to be populated
 * @scen: pointer to the available space for the scenario
 *
 * Set the chunk mask for the entries that will be used by this scenario
 */
static void
ice_acl_set_scen_chnk_msk(struct ice_aqc_acl_scen *scen_buf,
			  struct ice_acl_scen *scen)
{
	u16 tcam_idx, num_cscd, units, cnt;
	u8 chnk_offst;

	/* Determine the starting TCAM index and offset of the start entry */
	tcam_idx = ICE_ACL_TBL_TCAM_IDX(scen->start);
	chnk_offst = (u8)((scen->start % ICE_AQC_ACL_TCAM_DEPTH) /
			  ICE_ACL_ENTRY_ALLOC_UNIT);

	/* Entries are allocated and tracked in multiple of 64's */
	units = scen->num_entry / ICE_ACL_ENTRY_ALLOC_UNIT;

	/* Determine number of cascaded TCAMs */
	num_cscd = scen->width / ICE_AQC_ACL_KEY_WIDTH_BYTES;

	for (cnt = 0; cnt < units; cnt++) {
		u16 i;

		/* Set the corresponding bitmap of individual 64-entry
		 * chunk spans across a cascade of 1 or more TCAMs
		 * For each TCAM, there will be (ICE_AQC_ACL_TCAM_DEPTH
		 * / ICE_ACL_ENTRY_ALLOC_UNIT) or 8 chunks.
		 */
		for (i = tcam_idx; i < tcam_idx + num_cscd; i++)
			scen_buf->tcam_cfg[i].chnk_msk |= BIT(chnk_offst);

		chnk_offst = (chnk_offst + 1) % ICE_AQC_MAX_TCAM_ALLOC_UNITS;
		if (!chnk_offst)
			tcam_idx += num_cscd;
	}
}

/**
 * ice_acl_assign_act_mem_for_scen
 * @tbl: pointer to ACL table structure
 * @scen: pointer to the scenario struct
 * @scen_buf: pointer to the available space for the scenario
 * @current_tcam_idx: theoretical index of the TCAM that we associated those
 *		      action memory banks with, at the table creation time.
 * @target_tcam_idx: index of the TCAM that we want to associate those action
 *		     memory banks with.
 */
static void
ice_acl_assign_act_mem_for_scen(struct ice_acl_tbl *tbl,
				struct ice_acl_scen *scen,
				struct ice_aqc_acl_scen *scen_buf,
				u8 current_tcam_idx, u8 target_tcam_idx)
{
	u8 i;

	for (i = 0; i < ICE_AQC_MAX_ACTION_MEMORIES; i++) {
		struct ice_acl_act_mem *p_mem = &tbl->act_mems[i];

		if (p_mem->act_mem == ICE_ACL_ACT_PAIR_MEM_INVAL ||
		    p_mem->member_of_tcam != current_tcam_idx)
			continue;

		scen_buf->act_mem_cfg[i] = target_tcam_idx;
		scen_buf->act_mem_cfg[i] |= ICE_AQC_ACL_SCE_ACT_MEM_EN;
		ice_set_bit(i, scen->act_mem_bitmap);
	}
}

/**
 * ice_acl_commit_partition - Indicate if the specified partition is active
 * @hw: pointer to the hardware structure
 * @scen: pointer to the scenario struct
 * @commit: true if the partition is being commit
 */
static void
ice_acl_commit_partition(struct ice_hw *hw, struct ice_acl_scen *scen,
			 bool commit)
{
	u16 tcam_idx, off, num_cscd, units, cnt;

	/* Determine the starting TCAM index and offset of the start entry */
	tcam_idx = ICE_ACL_TBL_TCAM_IDX(scen->start);
	off = (scen->start % ICE_AQC_ACL_TCAM_DEPTH) /
		ICE_ACL_ENTRY_ALLOC_UNIT;

	/* Entries are allocated and tracked in multiple of 64's */
	units = scen->num_entry / ICE_ACL_ENTRY_ALLOC_UNIT;

	/* Determine number of cascaded TCAM */
	num_cscd = scen->width / ICE_AQC_ACL_KEY_WIDTH_BYTES;

	for (cnt = 0; cnt < units; cnt++) {
		u16 w;

		/* Set/clear the corresponding bitmap of individual 64-entry
		 * chunk spans across a row of 1 or more TCAMs
		 */
		for (w = 0; w < num_cscd; w++) {
			u16 b;

			b = ((tcam_idx + w) * ICE_AQC_MAX_TCAM_ALLOC_UNITS) +
				off;
			if (commit)
				ice_set_bit(b, hw->acl_tbl->avail);
			else
				ice_clear_bit(b, hw->acl_tbl->avail);
		}

		off = (off + 1) % ICE_AQC_MAX_TCAM_ALLOC_UNITS;
		if (!off)
			tcam_idx += num_cscd;
	}
}

/**
 * ice_acl_create_scen
 * @hw: pointer to the hardware structure
 * @match_width: number of bytes to be matched in this scenario
 * @num_entries: number of entries to be allocated for the scenario
 * @scen_id: holds returned scenario ID if successful
 */
enum ice_status
ice_acl_create_scen(struct ice_hw *hw, u16 match_width, u16 num_entries,
		    u16 *scen_id)
{
	u8 cascade_cnt, first_tcam, last_tcam, i, k;
	struct ice_aqc_acl_scen scen_buf;
	struct ice_acl_scen *scen;
	enum ice_status status;

	if (!hw->acl_tbl)
		return ICE_ERR_DOES_NOT_EXIST;

	scen = (struct ice_acl_scen *)ice_malloc(hw, sizeof(*scen));
	if (!scen)
		return ICE_ERR_NO_MEMORY;

	scen->start = hw->acl_tbl->first_entry;
	scen->width = ICE_AQC_ACL_KEY_WIDTH_BYTES *
		DIVIDE_AND_ROUND_UP(match_width, ICE_AQC_ACL_KEY_WIDTH_BYTES);
	scen->num_entry = num_entries;

	status = ice_acl_alloc_partition(hw, scen);
	if (status)
		goto out;

	ice_memset(&scen_buf, 0, sizeof(scen_buf), ICE_NONDMA_MEM);

	/* Determine the number of cascade TCAMs, given the scenario's width */
	cascade_cnt = DIVIDE_AND_ROUND_UP(scen->width,
					  ICE_AQC_ACL_KEY_WIDTH_BYTES);
	first_tcam = ICE_ACL_TBL_TCAM_IDX(scen->start);
	last_tcam = ICE_ACL_TBL_TCAM_IDX(scen->end);

	/* For each scenario, we reserved last three bytes of scenario width for
	 * packet direction flag, profile ID and range checker. Thus, we want to
	 * return back to the caller the eff_width, pkt_dir_idx, rng_chk_idx and
	 * pid_idx.
	 */
	scen->eff_width = cascade_cnt * ICE_AQC_ACL_KEY_WIDTH_BYTES -
		ICE_ACL_SCEN_MIN_WIDTH;
	scen->rng_chk_idx = (cascade_cnt - 1) * ICE_AQC_ACL_KEY_WIDTH_BYTES +
		ICE_ACL_SCEN_RNG_CHK_IDX_IN_TCAM;
	scen->pid_idx = (cascade_cnt - 1) * ICE_AQC_ACL_KEY_WIDTH_BYTES +
		ICE_ACL_SCEN_PID_IDX_IN_TCAM;
	scen->pkt_dir_idx = (cascade_cnt - 1) * ICE_AQC_ACL_KEY_WIDTH_BYTES +
		ICE_ACL_SCEN_PKT_DIR_IDX_IN_TCAM;

	/* set the chunk mask for the tcams */
	ice_acl_set_scen_chnk_msk(&scen_buf, scen);

	/* set the TCAM select and start_cmp and start_set bits */
	k = first_tcam;
	/* set the START_SET bit at the beginning of the stack */
	scen_buf.tcam_cfg[k].start_cmp_set |= ICE_AQC_ACL_ALLOC_SCE_START_SET;
	while (k <= last_tcam) {
		u8 last_tcam_idx_cascade = cascade_cnt + k - 1;

		/* set start_cmp for the first cascaded TCAM */
		scen_buf.tcam_cfg[k].start_cmp_set |=
			ICE_AQC_ACL_ALLOC_SCE_START_CMP;

		/* cascade TCAMs up to the width of the scenario */
		for (i = k; i < cascade_cnt + k; i++) {
			ice_acl_fill_tcam_select(&scen_buf, scen, i, i - k);
			ice_acl_assign_act_mem_for_scen(hw->acl_tbl, scen,
							&scen_buf,
							i,
							last_tcam_idx_cascade);
		}

		k = i;
	}

	/* We need to set the start_cmp bit for the unused TCAMs. */
	i = 0;
	while (i < first_tcam)
		scen_buf.tcam_cfg[i++].start_cmp_set =
					ICE_AQC_ACL_ALLOC_SCE_START_CMP;

	i = last_tcam + 1;
	while (i < ICE_AQC_ACL_SLICES)
		scen_buf.tcam_cfg[i++].start_cmp_set =
					ICE_AQC_ACL_ALLOC_SCE_START_CMP;

	status = ice_aq_alloc_acl_scen(hw, scen_id, &scen_buf, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_ACL, "AQ allocation of ACL scenario failed. status: %d\n",
			  status);
		goto out;
	}

	scen->id = *scen_id;
	ice_acl_commit_partition(hw, scen, false);
	ice_acl_init_entry(scen);
	LIST_ADD(&scen->list_entry, &hw->acl_tbl->scens);

out:
	if (status)
		ice_free(hw, scen);

	return status;
}

/**
 * ice_acl_destroy_scen - Destroy an ACL scenario
 * @hw: pointer to the HW struct
 * @scen_id: ID of the remove scenario
 */
static enum ice_status ice_acl_destroy_scen(struct ice_hw *hw, u16 scen_id)
{
	struct ice_acl_scen *scen, *tmp_scen;
	struct ice_flow_prof *p, *tmp;
	enum ice_status status;

	if (!hw->acl_tbl)
		return ICE_ERR_DOES_NOT_EXIST;

	/* Remove profiles that use "scen_id" scenario */
	LIST_FOR_EACH_ENTRY_SAFE(p, tmp, &hw->fl_profs[ICE_BLK_ACL],
				 ice_flow_prof, l_entry)
		if (p->cfg.scen && p->cfg.scen->id == scen_id) {
			status = ice_flow_rem_prof(hw, ICE_BLK_ACL, p->id);
			if (status) {
				ice_debug(hw, ICE_DBG_ACL, "ice_flow_rem_prof failed. status: %d\n",
					  status);
				return status;
			}
		}

	/* Call the AQ command to destroy the targeted scenario */
	status = ice_aq_dealloc_acl_scen(hw, scen_id, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_ACL, "AQ de-allocation of scenario failed. status: %d\n",
			  status);
		return status;
	}

	/* Remove scenario from hw->acl_tbl->scens */
	LIST_FOR_EACH_ENTRY_SAFE(scen, tmp_scen, &hw->acl_tbl->scens,
				 ice_acl_scen, list_entry)
		if (scen->id == scen_id) {
			LIST_DEL(&scen->list_entry);
			ice_free(hw, scen);
		}

	return ICE_SUCCESS;
}

/**
 * ice_acl_destroy_tbl - Destroy a previously created LEM table for ACL
 * @hw: pointer to the HW struct
 */
enum ice_status ice_acl_destroy_tbl(struct ice_hw *hw)
{
	struct ice_acl_scen *pos_scen, *tmp_scen;
	struct ice_aqc_acl_generic resp_buf;
	struct ice_aqc_acl_scen buf;
	enum ice_status status;
	u8 i;

	if (!hw->acl_tbl)
		return ICE_ERR_DOES_NOT_EXIST;

	/* Mark all the created scenario's TCAM to stop the packet lookup and
	 * delete them afterward
	 */
	LIST_FOR_EACH_ENTRY_SAFE(pos_scen, tmp_scen, &hw->acl_tbl->scens,
				 ice_acl_scen, list_entry) {
		status = ice_aq_query_acl_scen(hw, pos_scen->id, &buf, NULL);
		if (status) {
			ice_debug(hw, ICE_DBG_ACL, "ice_aq_query_acl_scen() failed. status: %d\n",
				  status);
			return status;
		}

		for (i = 0; i < ICE_AQC_ACL_SLICES; i++) {
			buf.tcam_cfg[i].chnk_msk = 0;
			buf.tcam_cfg[i].start_cmp_set =
					ICE_AQC_ACL_ALLOC_SCE_START_CMP;
		}

		for (i = 0; i < ICE_AQC_MAX_ACTION_MEMORIES; i++)
			buf.act_mem_cfg[i] = 0;

		status = ice_aq_update_acl_scen(hw, pos_scen->id, &buf, NULL);
		if (status) {
			ice_debug(hw, ICE_DBG_ACL, "ice_aq_update_acl_scen() failed. status: %d\n",
				  status);
			return status;
		}

		status = ice_acl_destroy_scen(hw, pos_scen->id);
		if (status) {
			ice_debug(hw, ICE_DBG_ACL, "deletion of scenario failed. status: %d\n",
				  status);
			return status;
		}
	}

	/* call the AQ command to destroy the ACL table */
	status = ice_aq_dealloc_acl_tbl(hw, hw->acl_tbl->id, &resp_buf, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_ACL, "AQ de-allocation of ACL failed. status: %d\n",
			  status);
		return status;
	}

	ice_free(hw, hw->acl_tbl);
	hw->acl_tbl = NULL;

	return ICE_SUCCESS;
}

/**
 * ice_acl_add_entry - Add a flow entry to an ACL scenario
 * @hw: pointer to the HW struct
 * @scen: scenario to add the entry to
 * @prio: priority level of the entry being added
 * @keys: buffer of the value of the key to be programmed to the ACL entry
 * @inverts: buffer of the value of the key inverts to be programmed
 * @acts: pointer to a buffer containing formatted actions
 * @acts_cnt: indicates the number of actions stored in "acts"
 * @entry_idx: returned scenario relative index of the added flow entry
 *
 * Given an ACL table and a scenario, to add the specified key and key invert
 * to an available entry in the specified scenario.
 * The "keys" and "inverts" buffers must be of the size which is the same as
 * the scenario's width
 */
enum ice_status
ice_acl_add_entry(struct ice_hw *hw, struct ice_acl_scen *scen,
		  enum ice_acl_entry_prio prio, u8 *keys, u8 *inverts,
		  struct ice_acl_act_entry *acts, u8 acts_cnt, u16 *entry_idx)
{
	u8 i, entry_tcam, num_cscd, offset;
	struct ice_aqc_acl_data buf;
	enum ice_status status = ICE_SUCCESS;
	u16 idx;

	if (!scen)
		return ICE_ERR_DOES_NOT_EXIST;

	*entry_idx = ice_acl_scen_assign_entry_idx(scen, prio);
	if (*entry_idx >= scen->num_entry) {
		*entry_idx = 0;
		return ICE_ERR_MAX_LIMIT;
	}

	/* Determine number of cascaded TCAMs */
	num_cscd = DIVIDE_AND_ROUND_UP(scen->width,
				       ICE_AQC_ACL_KEY_WIDTH_BYTES);

	entry_tcam = ICE_ACL_TBL_TCAM_IDX(scen->start);
	idx = ICE_ACL_TBL_TCAM_ENTRY_IDX(scen->start + *entry_idx);

	ice_memset(&buf, 0, sizeof(buf), ICE_NONDMA_MEM);
	for (i = 0; i < num_cscd; i++) {
		/* If the key spans more than one TCAM in the case of cascaded
		 * TCAMs, the key and key inverts need to be properly split
		 * among TCAMs.E.g.bytes 0 - 4 go to an index in the first TCAM
		 * and bytes 5 - 9 go to the same index in the next TCAM, etc.
		 * If the entry spans more than one TCAM in a cascaded TCAM
		 * mode, the programming of the entries in the TCAMs must be in
		 * reversed order - the TCAM entry of the rightmost TCAM should
		 * be programmed first; the TCAM entry of the leftmost TCAM
		 * should be programmed last.
		 */
		offset = num_cscd - i - 1;
		ice_memcpy(&buf.entry_key.val,
			   &keys[offset * sizeof(buf.entry_key.val)],
			   sizeof(buf.entry_key.val), ICE_NONDMA_TO_NONDMA);
		ice_memcpy(&buf.entry_key_invert.val,
			   &inverts[offset * sizeof(buf.entry_key_invert.val)],
			   sizeof(buf.entry_key_invert.val),
			   ICE_NONDMA_TO_NONDMA);
		status = ice_aq_program_acl_entry(hw, entry_tcam + offset, idx,
						  &buf, NULL);
		if (status) {
			ice_debug(hw, ICE_DBG_ACL, "aq program acl entry failed status: %d\n",
				  status);
			goto out;
		}
	}

	/* Program the action memory */
	status = ice_acl_prog_act(hw, scen, acts, acts_cnt, *entry_idx);

out:
	if (status) {
		ice_acl_rem_entry(hw, scen, *entry_idx);
		*entry_idx = 0;
	}

	return status;
}

/**
 * ice_acl_prog_act - Program a scenario's action memory
 * @hw: pointer to the HW struct
 * @scen: scenario to add the entry to
 * @acts: pointer to a buffer containing formatted actions
 * @acts_cnt: indicates the number of actions stored in "acts"
 * @entry_idx: scenario relative index of the added flow entry
 *
 * Program a scenario's action memory
 */
enum ice_status
ice_acl_prog_act(struct ice_hw *hw, struct ice_acl_scen *scen,
		 struct ice_acl_act_entry *acts, u8 acts_cnt,
		 u16 entry_idx)
{
	u8 entry_tcam, num_cscd, i, actx_idx = 0;
	struct ice_aqc_actpair act_buf;
	enum ice_status status = ICE_SUCCESS;
	u16 idx;

	if (entry_idx >= scen->num_entry)
		return ICE_ERR_MAX_LIMIT;

	ice_memset(&act_buf, 0, sizeof(act_buf), ICE_NONDMA_MEM);

	/* Determine number of cascaded TCAMs */
	num_cscd = DIVIDE_AND_ROUND_UP(scen->width,
				       ICE_AQC_ACL_KEY_WIDTH_BYTES);

	entry_tcam = ICE_ACL_TBL_TCAM_IDX(scen->start);
	idx = ICE_ACL_TBL_TCAM_ENTRY_IDX(scen->start + entry_idx);

	ice_for_each_set_bit(i, scen->act_mem_bitmap,
			     ICE_AQC_MAX_ACTION_MEMORIES) {
		struct ice_acl_act_mem *mem = &hw->acl_tbl->act_mems[i];

		if (actx_idx >= acts_cnt)
			break;
		if (mem->member_of_tcam >= entry_tcam &&
		    mem->member_of_tcam < entry_tcam + num_cscd) {
			ice_memcpy(&act_buf.act[0], &acts[actx_idx],
				   sizeof(struct ice_acl_act_entry),
				   ICE_NONDMA_TO_NONDMA);

			if (++actx_idx < acts_cnt) {
				ice_memcpy(&act_buf.act[1], &acts[actx_idx],
					   sizeof(struct ice_acl_act_entry),
					   ICE_NONDMA_TO_NONDMA);
			}

			status = ice_aq_program_actpair(hw, i, idx, &act_buf,
							NULL);
			if (status) {
				ice_debug(hw, ICE_DBG_ACL, "program actpair failed status: %d\n",
					  status);
				break;
			}
			actx_idx++;
		}
	}

	if (!status && actx_idx < acts_cnt)
		status = ICE_ERR_MAX_LIMIT;

	return status;
}

/**
 * ice_acl_rem_entry - Remove a flow entry from an ACL scenario
 * @hw: pointer to the HW struct
 * @scen: scenario to remove the entry from
 * @entry_idx: the scenario-relative index of the flow entry being removed
 */
enum ice_status
ice_acl_rem_entry(struct ice_hw *hw, struct ice_acl_scen *scen, u16 entry_idx)
{
	struct ice_aqc_actpair act_buf;
	struct ice_aqc_acl_data buf;
	u8 entry_tcam, num_cscd, i;
	enum ice_status status = ICE_SUCCESS;
	u16 idx;

	if (!scen)
		return ICE_ERR_DOES_NOT_EXIST;

	if (entry_idx >= scen->num_entry)
		return ICE_ERR_MAX_LIMIT;

	if (!ice_is_bit_set(scen->entry_bitmap, entry_idx))
		return ICE_ERR_DOES_NOT_EXIST;

	/* Determine number of cascaded TCAMs */
	num_cscd = DIVIDE_AND_ROUND_UP(scen->width,
				       ICE_AQC_ACL_KEY_WIDTH_BYTES);

	entry_tcam = ICE_ACL_TBL_TCAM_IDX(scen->start);
	idx = ICE_ACL_TBL_TCAM_ENTRY_IDX(scen->start + entry_idx);

	/* invalidate the flow entry */
	ice_memset(&buf, 0, sizeof(buf), ICE_NONDMA_MEM);
	for (i = 0; i < num_cscd; i++) {
		status = ice_aq_program_acl_entry(hw, entry_tcam + i, idx, &buf,
						  NULL);
		if (status)
			ice_debug(hw, ICE_DBG_ACL, "AQ program ACL entry failed status: %d\n",
				  status);
	}

	ice_memset(&act_buf, 0, sizeof(act_buf), ICE_NONDMA_MEM);

	ice_for_each_set_bit(i, scen->act_mem_bitmap,
			     ICE_AQC_MAX_ACTION_MEMORIES) {
		struct ice_acl_act_mem *mem = &hw->acl_tbl->act_mems[i];

		if (mem->member_of_tcam >= entry_tcam &&
		    mem->member_of_tcam < entry_tcam + num_cscd) {
			/* Invalidate allocated action pairs */
			status = ice_aq_program_actpair(hw, i, idx, &act_buf,
							NULL);
			if (status)
				ice_debug(hw, ICE_DBG_ACL, "program actpair failed status: %d\n",
					  status);
		}
	}

	ice_acl_scen_free_entry_idx(scen, entry_idx);

	return status;
}
