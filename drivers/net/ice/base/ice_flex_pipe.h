/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2019
 */

#ifndef _ICE_FLEX_PIPE_H_
#define _ICE_FLEX_PIPE_H_

#include "ice_type.h"

/* Package format version */
#define ICE_PKG_FMT_VER_MAJ	1
#define ICE_PKG_FMT_VER_MNR	0
#define ICE_PKG_FMT_VER_UPD	0
#define ICE_PKG_FMT_VER_DFT	0

enum ice_status
ice_update_pkg(struct ice_hw *hw, struct ice_buf *bufs, u32 count);

struct ice_generic_seg_hdr *
ice_find_seg_in_pkg(struct ice_hw *hw, u32 seg_type,
		    struct ice_pkg_hdr *pkg_hdr);
enum ice_status ice_download_pkg(struct ice_hw *hw, struct ice_seg *ice_seg);

enum ice_status
ice_init_pkg_info(struct ice_hw *hw, struct ice_pkg_hdr *pkg_header);

void ice_init_pkg_hints(struct ice_hw *hw, struct ice_seg *ice_seg);

enum ice_status
ice_find_label_value(struct ice_seg *ice_seg, char const *name, u32 type,
		     u16 *value);
enum ice_status
ice_get_sw_fv_list(struct ice_hw *hw, u16 *prot_ids, u8 ids_cnt,
		   struct LIST_HEAD_TYPE *fv_list);
enum ice_status
ice_aq_upload_section(struct ice_hw *hw, struct ice_buf_hdr *pkg_buf,
		      u16 buf_size, struct ice_sq_cd *cd);

enum ice_status
ice_pkg_buf_unreserve_section(struct ice_buf_build *bld, u16 count);
u16 ice_pkg_buf_get_free_space(struct ice_buf_build *bld);
u16 ice_pkg_buf_get_active_sections(struct ice_buf_build *bld);

/* package buffer building routines */

struct ice_buf_build *ice_pkg_buf_alloc(struct ice_hw *hw);
enum ice_status
ice_pkg_buf_reserve_section(struct ice_buf_build *bld, u16 count);
void *ice_pkg_buf_alloc_section(struct ice_buf_build *bld, u32 type, u16 size);
struct ice_buf *ice_pkg_buf(struct ice_buf_build *bld);
void ice_pkg_buf_free(struct ice_hw *hw, struct ice_buf_build *bld);

/* XLT1/PType group functions */
enum ice_status ice_ptg_update_xlt1(struct ice_hw *hw, enum ice_block blk);
enum ice_status
ice_ptg_find_ptype(struct ice_hw *hw, enum ice_block blk, u16 ptype, u8 *ptg);
u8 ice_ptg_alloc(struct ice_hw *hw, enum ice_block blk);
void ice_ptg_free(struct ice_hw *hw, enum ice_block blk, u8 ptg);
enum ice_status
ice_ptg_add_mv_ptype(struct ice_hw *hw, enum ice_block blk, u16 ptype, u8 ptg);

/* XLT2/VSI group functions */
enum ice_status ice_vsig_update_xlt2(struct ice_hw *hw, enum ice_block blk);
enum ice_status
ice_vsig_find_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 *vsig);
enum ice_status
ice_find_dup_props_vsig(struct ice_hw *hw, enum ice_block blk,
			struct LIST_HEAD_TYPE *chs, u16 *vsig);

enum ice_status
ice_vsig_add_mv_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig);
enum ice_status ice_vsig_free(struct ice_hw *hw, enum ice_block blk, u16 vsig);
enum ice_status
ice_vsig_add_mv_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig);
enum ice_status
ice_vsig_remove_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig);
enum ice_status
ice_add_prof(struct ice_hw *hw, enum ice_block blk, u64 id, u8 ptypes[],
	     struct ice_fv_word *es);
struct ice_prof_map *
ice_search_prof_id(struct ice_hw *hw, enum ice_block blk, u64 id);
enum ice_status
ice_add_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl);
enum ice_status
ice_rem_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl);
struct ice_prof_map *
ice_set_prof_context(struct ice_hw *hw, enum ice_block blk, u64 id, u64 cntxt);
struct ice_prof_map *
ice_get_prof_context(struct ice_hw *hw, enum ice_block blk, u64 id, u64 *cntxt);
enum ice_status
ice_init_pkg(struct ice_hw *hw, u8 *buff, u32 len);
enum ice_status
ice_copy_and_init_pkg(struct ice_hw *hw, const u8 *buf, u32 len);
enum ice_status ice_init_hw_tbls(struct ice_hw *hw);
void ice_free_seg(struct ice_hw *hw);
void ice_free_hw_tbls(struct ice_hw *hw);
enum ice_status
ice_add_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi[], u8 count,
	     u64 id);
enum ice_status
ice_rem_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi[], u8 count,
	     u64 id);
enum ice_status
ice_rem_prof(struct ice_hw *hw, enum ice_block blk, u64 id);

enum ice_status
ice_set_key(u8 *key, u16 size, u8 *val, u8 *upd, u8 *dc, u8 *nm, u16 off,
	    u16 len);
#endif /* _ICE_FLEX_PIPE_H_ */
