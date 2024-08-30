/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */

#include <assert.h>

#include <stdint.h>
#include <stdbool.h>

#include "tfc.h"
#include "tfo.h"

/* HWRM Direct messages */

int
tfc_msg_tbl_scope_qcaps(struct tfc *tfcp,
			bool *tbl_scope_capable,
			uint32_t *max_lkup_rec_cnt,
			uint32_t *max_act_rec_cnt,
			uint8_t	*max_lkup_static_buckets_exp);

int tfc_msg_tbl_scope_id_alloc(struct tfc *tfcp, uint16_t fid, bool shared,
			       enum cfa_app_type app_type, uint8_t *tsid,
			       bool *first);

int
tfc_msg_backing_store_cfg_v2(struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir,
			     enum cfa_region_type region, uint64_t base_addr,
			     uint8_t pbl_level, uint32_t pbl_page_sz,
			     uint32_t rec_cnt, uint8_t static_bkt_cnt_exp,
			     bool cfg_done);

int
tfc_msg_tbl_scope_deconfig(struct tfc *tfcp, uint8_t tsid);

int
tfc_msg_tbl_scope_fid_add(struct tfc *tfcp, uint16_t fid,
			  uint8_t tsid, uint16_t *fid_cnt);

int
tfc_msg_tbl_scope_fid_rem(struct tfc *tfcp, uint16_t fid,
			  uint8_t tsid, uint16_t *fid_cnt);

int
tfc_msg_idx_tbl_alloc(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		      enum cfa_track_type tt, enum cfa_dir dir,
		      enum cfa_resource_subtype_idx_tbl rsubtype,
		      uint16_t *id);

int
tfc_msg_idx_tbl_alloc_set(struct tfc *tfcp, uint16_t fid, uint16_t sid,
			  enum cfa_track_type tt, enum cfa_dir dir,
			  enum cfa_resource_subtype_idx_tbl subtype,
			  const uint32_t *dev_data, uint8_t data_size,
			  uint16_t *id);

int
tfc_msg_idx_tbl_set(struct tfc *tfcp, uint16_t fid,
		    uint16_t sid, enum cfa_dir dir,
		    enum cfa_resource_subtype_idx_tbl subtype,
		    uint16_t id, const uint32_t *dev_data, uint8_t data_size);

int
tfc_msg_idx_tbl_get(struct tfc *tfcp, uint16_t fid,
		    uint16_t sid, enum cfa_dir dir,
		    enum cfa_resource_subtype_idx_tbl subtype,
		    uint16_t id, uint32_t *dev_data, uint8_t *data_size);

int
tfc_msg_idx_tbl_free(struct tfc *tfcp, uint16_t fid,
		     uint16_t sid, enum cfa_dir dir,
		     enum cfa_resource_subtype_idx_tbl subtype, uint16_t id);

int tfc_msg_global_id_alloc(struct tfc *tfcp, uint16_t fid, uint16_t sid,
			    enum tfc_domain_id domain_id, uint16_t req_cnt,
			    const struct tfc_global_id_req *glb_id_req,
			    struct tfc_global_id *rsp, uint16_t *rsp_cnt,
			    bool *first);
int
tfc_msg_session_id_alloc(struct tfc *tfcp, uint16_t fid, uint16_t *tsid);

int
tfc_msg_session_fid_add(struct tfc *tfcp, uint16_t fid,
			uint16_t sid, uint16_t *fid_cnt);
int
tfc_msg_session_fid_rem(struct tfc *tfcp, uint16_t fid,
			uint16_t sid, uint16_t *fid_cnt);

int tfc_msg_identifier_alloc(struct tfc *tfcp, enum cfa_dir dir,
			     enum cfa_resource_subtype_ident subtype,
			     enum cfa_track_type tt, uint16_t fid,
			     uint16_t sid, uint16_t *ident_id);

int tfc_msg_identifier_free(struct tfc *tfcp, enum cfa_dir dir,
			    enum cfa_resource_subtype_ident subtype,
			    uint16_t fid, uint16_t sid,
			    uint16_t ident_id);

#ifndef TFC_FORCE_POOL_0
int
tfc_msg_tbl_scope_pool_alloc(struct tfc *tfcp,
			     uint8_t tsid,
			     enum cfa_dir dir,
			     enum tfc_ts_table_type type,
			     uint16_t *pool_id,
			     uint8_t *lkup_pool_sz_exp);

int
tfc_msg_tbl_scope_pool_free(struct tfc *tfcp,
			    uint8_t tsid,
			    enum cfa_dir dir,
			    enum tfc_ts_table_type type,
			    uint16_t pool_id);
#endif /* !TFC_FORCE_POOL_0 */

int
tfc_msg_tbl_scope_config_get(struct tfc *tfcp, uint8_t tsid, bool *configured);

int
tfc_msg_tcam_alloc(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		   enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		   enum cfa_track_type tt, uint16_t pri, uint16_t key_sz_words,
		   uint16_t *tcam_id);

int
tfc_msg_tcam_alloc_set(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		       enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		       enum cfa_track_type tt, uint16_t *tcam_id, uint16_t pri,
		       const uint8_t *key, uint8_t key_size, const uint8_t *mask,
		       const uint8_t *remap, uint8_t remap_size);

int
tfc_msg_tcam_set(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		 enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		 uint16_t tcam_id, const uint8_t *key, uint8_t key_size,
		 const uint8_t *mask, const uint8_t *remap,
		 uint8_t remap_size);

int
tfc_msg_tcam_get(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		 enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		 uint16_t tcam_id, uint8_t *key, uint8_t *key_size,
		 uint8_t *mask, uint8_t *remap, uint8_t *remap_size);

int
tfc_msg_tcam_free(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		 enum cfa_dir dir, enum cfa_resource_subtype_tcam subtype,
		 uint16_t tcam_id);

int
tfc_msg_if_tbl_set(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		   enum cfa_dir dir, enum cfa_resource_subtype_if_tbl subtype,
		   uint16_t index, uint8_t data_size, const uint8_t *data);

int
tfc_msg_if_tbl_get(struct tfc *tfcp, uint16_t fid, uint16_t sid,
		   enum cfa_dir dir, enum cfa_resource_subtype_if_tbl subtype,
		   uint16_t index, uint8_t *data_size, uint8_t *data);

#ifdef TF_FLOW_SCALE_QUERY
int tfc_msg_resc_usage_query(struct tfc *tfcp, uint16_t sid, enum cfa_dir dir,
			     uint16_t *data_size, void *data);
#endif /* TF_FLOW_SCALE_QUERY  */
