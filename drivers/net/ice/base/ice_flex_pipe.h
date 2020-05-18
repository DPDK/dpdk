/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _ICE_FLEX_PIPE_H_
#define _ICE_FLEX_PIPE_H_

#include "ice_type.h"

/* Package minimal version supported */
#define ICE_PKG_SUPP_VER_MAJ	1
#define ICE_PKG_SUPP_VER_MNR	3

/* Package format version */
#define ICE_PKG_FMT_VER_MAJ	1
#define ICE_PKG_FMT_VER_MNR	0
#define ICE_PKG_FMT_VER_UPD	0
#define ICE_PKG_FMT_VER_DFT	0

#define ICE_PKG_CNT 4

enum ice_status
ice_update_pkg(struct ice_hw *hw, struct ice_buf *bufs, u32 count);
enum ice_status
ice_acquire_change_lock(struct ice_hw *hw, enum ice_aq_res_access_type access);
void ice_release_change_lock(struct ice_hw *hw);
enum ice_status
ice_find_prot_off(struct ice_hw *hw, enum ice_block blk, u8 prof, u16 fv_idx,
		  u8 *prot, u16 *off);
enum ice_status
ice_find_label_value(struct ice_seg *ice_seg, char const *name, u32 type,
		     u16 *value);
void
ice_get_sw_fv_bitmap(struct ice_hw *hw, enum ice_prof_type type,
		     ice_bitmap_t *bm);
void
ice_init_prof_result_bm(struct ice_hw *hw);
enum ice_status
ice_get_sw_fv_list(struct ice_hw *hw, u8 *prot_ids, u16 ids_cnt,
		   ice_bitmap_t *bm, struct LIST_HEAD_TYPE *fv_list);
bool
ice_get_open_tunnel_port(struct ice_hw *hw, enum ice_tunnel_type type,
			 u16 *port);
enum ice_status
ice_create_tunnel(struct ice_hw *hw, enum ice_tunnel_type type, u16 port);
enum ice_status ice_destroy_tunnel(struct ice_hw *hw, u16 port, bool all);
bool ice_tunnel_port_in_use(struct ice_hw *hw, u16 port, u16 *index);
bool
ice_tunnel_get_type(struct ice_hw *hw, u16 port, enum ice_tunnel_type *type);

/* XLT2/VSI group functions */
enum ice_status
ice_vsig_find_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 *vsig);
enum ice_status
ice_add_prof(struct ice_hw *hw, enum ice_block blk, u64 id, u8 ptypes[],
	     const struct ice_ptype_attributes *attr, u16 attr_cnt,
	     struct ice_fv_word *es, u16 *masks);
void ice_init_all_prof_masks(struct ice_hw *hw);
void ice_shutdown_all_prof_masks(struct ice_hw *hw);
struct ice_prof_map *
ice_search_prof_id(struct ice_hw *hw, enum ice_block blk, u64 id);
enum ice_status
ice_add_vsi_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig);
enum ice_status
ice_add_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl);
enum ice_status
ice_rem_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl);
enum ice_status ice_init_pkg(struct ice_hw *hw, u8 *buff, u32 len);
enum ice_status
ice_copy_and_init_pkg(struct ice_hw *hw, const u8 *buf, u32 len);
enum ice_status ice_init_hw_tbls(struct ice_hw *hw);
void ice_free_seg(struct ice_hw *hw);
void ice_fill_blk_tbls(struct ice_hw *hw);
void ice_clear_hw_tbls(struct ice_hw *hw);
void ice_free_hw_tbls(struct ice_hw *hw);
enum ice_status
ice_rem_prof(struct ice_hw *hw, enum ice_block blk, u64 id);

enum ice_status
ice_set_key(u8 *key, u16 size, u8 *val, u8 *upd, u8 *dc, u8 *nm, u16 off,
	    u16 len);
#endif /* _ICE_FLEX_PIPE_H_ */
