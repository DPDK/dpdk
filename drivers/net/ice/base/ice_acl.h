/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _ICE_ACL_H_
#define _ICE_ACL_H_

#include "ice_common.h"
#include "ice_adminq_cmd.h"

struct ice_acl_tbl_params {
	u16 width;	/* Select/match bytes */
	u16 depth;	/* Number of entries */

#define ICE_ACL_TBL_MAX_DEP_TBLS	15
	u16 dep_tbls[ICE_ACL_TBL_MAX_DEP_TBLS];

	u8 entry_act_pairs;	/* Action pairs per entry */
	u8 concurr;		/* Concurrent table lookup enable */
};

struct ice_acl_act_mem {
	u8 act_mem;
#define ICE_ACL_ACT_PAIR_MEM_INVAL	0xff
	u8 member_of_tcam;
};

struct ice_acl_tbl {
	/* TCAM configuration */
	u8 first_tcam;	/* Index of the first TCAM block */
	u8 last_tcam;	/* Index of the last TCAM block */
	/* Index of the first entry in the first TCAM */
	u16 first_entry;
	/* Index of the last entry in the last TCAM */
	u16 last_entry;

	/* List of active scenarios */
	struct LIST_HEAD_TYPE scens;

	struct ice_acl_tbl_params info;
	struct ice_acl_act_mem act_mems[ICE_AQC_MAX_ACTION_MEMORIES];

	/* Keep track of available 64-entry chunks in TCAMs */
	ice_declare_bitmap(avail, ICE_AQC_ACL_ALLOC_UNITS);

	u16 id;
};

#define ICE_MAX_ACL_TCAM_ENTRY (ICE_AQC_ACL_TCAM_DEPTH * ICE_AQC_ACL_SLICES)
enum ice_acl_entry_prio {
	ICE_ACL_PRIO_LOW = 0,
	ICE_ACL_PRIO_NORMAL,
	ICE_ACL_PRIO_HIGH,
	ICE_ACL_MAX_PRIO
};

/* Scenario structure
 * A scenario is a logical partition within an ACL table. It can span more
 * than one TCAM in cascade mode to support select/mask key widths larger.
 * than the width of a TCAM. It can also span more than one TCAM in stacked
 * mode to support larger number of entries than what a TCAM can hold. It is
 * used to select values from selection bases (field vectors holding extract
 * protocol header fields) to form lookup keys, and to associate action memory
 * banks to the TCAMs used.
 */
struct ice_acl_scen {
	struct LIST_ENTRY_TYPE list_entry;
	/* If nth bit of act_mem_bitmap is set, then nth action memory will
	 * participate in this scenario
	 */
	ice_declare_bitmap(act_mem_bitmap, ICE_AQC_MAX_ACTION_MEMORIES);

	/* If nth bit of entry_bitmap is set, then nth entry will
	 * be available in this scenario
	 */
	ice_declare_bitmap(entry_bitmap, ICE_MAX_ACL_TCAM_ENTRY);
	u16 first_idx[ICE_ACL_MAX_PRIO];
	u16 last_idx[ICE_ACL_MAX_PRIO];

	u16 id;
	u16 start;	/* Number of entry from the start of the parent table */
#define ICE_ACL_SCEN_MIN_WIDTH	0x3
	u16 width;	/* Number of select/mask bytes */
	u16 num_entry;	/* Number of scenario entry */
	u16 end;	/* Last addressable entry from start of table */
	u8 eff_width;	/* Available width in bytes to match */
#define ICE_ACL_SCEN_PKT_DIR_IDX_IN_TCAM	0x2
#define ICE_ACL_SCEN_PID_IDX_IN_TCAM		0x3
#define ICE_ACL_SCEN_RNG_CHK_IDX_IN_TCAM	0x4
	u8 pid_idx;	/* Byte index used to match profile ID */
	u8 rng_chk_idx;	/* Byte index used to match range checkers result */
	u8 pkt_dir_idx;	/* Byte index used to match packet direction */
};

/* This structure represents input fields needed to allocate ACL table */
struct ice_acl_alloc_tbl {
	/* Table's width in number of bytes matched */
	u16 width;
	/* Table's depth in number of entries. */
	u16 depth;
	u8 num_dependent_alloc_ids;	/* number of depdendent alloc IDs */
	u8 concurr;			/* true for concurrent table type */

	/* Amount of action pairs per table entry. Minimal valid
	 * value for this field is 1 (e.g. single pair of actions)
	 */
	u8 act_pairs_per_entry;
	union {
		struct ice_aqc_acl_alloc_table_data data_buf;
		struct ice_aqc_acl_generic resp_buf;
	} buf;
};

/* This structure is used to communicate input and output params for
 * [de]allocate_acl_counters
 */
struct ice_acl_cntrs {
	u8 amount;
	u8 type;
	u8 bank;

	/* Next 2 variables are used for output in case of alloc_acl_counters
	 * and input in case of deallocate_acl_counters
	 */
	u16 first_cntr;
	u16 last_cntr;
};

enum ice_status
ice_acl_create_tbl(struct ice_hw *hw, struct ice_acl_tbl_params *params);
enum ice_status ice_acl_destroy_tbl(struct ice_hw *hw);
enum ice_status
ice_acl_create_scen(struct ice_hw *hw, u16 match_width, u16 num_entries,
		    u16 *scen_id);
enum ice_status
ice_aq_alloc_acl_tbl(struct ice_hw *hw, struct ice_acl_alloc_tbl *tbl,
		     struct ice_sq_cd *cd);
enum ice_status
ice_aq_dealloc_acl_tbl(struct ice_hw *hw, u16 alloc_id,
		       struct ice_aqc_acl_generic *buf, struct ice_sq_cd *cd);
enum ice_status
ice_aq_program_acl_entry(struct ice_hw *hw, u8 tcam_idx, u16 entry_idx,
			 struct ice_aqc_acl_data *buf, struct ice_sq_cd *cd);
enum ice_status
ice_aq_query_acl_entry(struct ice_hw *hw, u8 tcam_idx, u16 entry_idx,
		       struct ice_aqc_acl_data *buf, struct ice_sq_cd *cd);
enum ice_status
ice_aq_alloc_actpair(struct ice_hw *hw, u16 alloc_id,
		     struct ice_aqc_acl_generic *buf, struct ice_sq_cd *cd);
enum ice_status
ice_aq_dealloc_actpair(struct ice_hw *hw, u16 alloc_id,
		       struct ice_aqc_acl_generic *buf, struct ice_sq_cd *cd);
enum ice_status
ice_aq_program_actpair(struct ice_hw *hw, u8 act_mem_idx, u16 act_entry_idx,
		       struct ice_aqc_actpair *buf, struct ice_sq_cd *cd);
enum ice_status
ice_aq_query_actpair(struct ice_hw *hw, u8 act_mem_idx, u16 act_entry_idx,
		     struct ice_aqc_actpair *buf, struct ice_sq_cd *cd);
enum ice_status ice_aq_dealloc_acl_res(struct ice_hw *hw, struct ice_sq_cd *cd);
enum ice_status
ice_prgm_acl_prof_xtrct(struct ice_hw *hw, u8 prof_id,
			struct ice_aqc_acl_prof_generic_frmt *buf,
			struct ice_sq_cd *cd);
enum ice_status
ice_query_acl_prof(struct ice_hw *hw, u8 prof_id,
		   struct ice_aqc_acl_prof_generic_frmt *buf,
		   struct ice_sq_cd *cd);
enum ice_status
ice_aq_alloc_acl_cntrs(struct ice_hw *hw, struct ice_acl_cntrs *cntrs,
		       struct ice_sq_cd *cd);
enum ice_status
ice_aq_dealloc_acl_cntrs(struct ice_hw *hw, struct ice_acl_cntrs *cntrs,
			 struct ice_sq_cd *cd);
enum ice_status
ice_prog_acl_prof_ranges(struct ice_hw *hw, u8 prof_id,
			 struct ice_aqc_acl_profile_ranges *buf,
			 struct ice_sq_cd *cd);
enum ice_status
ice_query_acl_prof_ranges(struct ice_hw *hw, u8 prof_id,
			  struct ice_aqc_acl_profile_ranges *buf,
			  struct ice_sq_cd *cd);
enum ice_status
ice_aq_alloc_acl_scen(struct ice_hw *hw, u16 *scen_id,
		      struct ice_aqc_acl_scen *buf, struct ice_sq_cd *cd);
enum ice_status
ice_aq_dealloc_acl_scen(struct ice_hw *hw, u16 scen_id, struct ice_sq_cd *cd);
enum ice_status
ice_aq_update_acl_scen(struct ice_hw *hw, u16 scen_id,
		       struct ice_aqc_acl_scen *buf, struct ice_sq_cd *cd);
enum ice_status
ice_aq_query_acl_scen(struct ice_hw *hw, u16 scen_id,
		      struct ice_aqc_acl_scen *buf, struct ice_sq_cd *cd);
enum ice_status
ice_acl_add_entry(struct ice_hw *hw, struct ice_acl_scen *scen,
		  enum ice_acl_entry_prio prio, u8 *keys, u8 *inverts,
		  struct ice_acl_act_entry *acts, u8 acts_cnt, u16 *entry_idx);
enum ice_status
ice_acl_prog_act(struct ice_hw *hw, struct ice_acl_scen *scen,
		 struct ice_acl_act_entry *acts, u8 acts_cnt, u16 entry_idx);
enum ice_status
ice_acl_rem_entry(struct ice_hw *hw, struct ice_acl_scen *scen, u16 entry_idx);
#endif /* _ICE_ACL_H_ */
