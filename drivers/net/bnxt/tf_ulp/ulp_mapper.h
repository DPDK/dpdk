/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_MAPPER_H_
#define _ULP_MAPPER_H_

/* TBD: it is added Thor2 testing */
/* #define ULP_MAPPER_TFC_TEST 1 */

#include <rte_log.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include "tf_core.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_ulp.h"
#include "ulp_utils.h"
#include "ulp_gen_tbl.h"
#include "tfc_em.h"
#include "bitalloc.h"
#include "ulp_alloc_tbl.h"

#define ULP_IDENTS_INVALID ((uint16_t)0xffff)

struct bnxt_ulp_mapper_glb_resource_entry {
	enum bnxt_ulp_resource_func	resource_func;
	uint32_t			resource_type; /* TF_ enum type */
	uint64_t			resource_hndl;
	bool				shared;
};

#define BNXT_ULP_KEY_RECIPE_MAX_FLDS 128
struct bnxt_ulp_key_recipe_entry {
	uint32_t cnt;
	struct bnxt_ulp_mapper_key_info	flds[BNXT_ULP_KEY_RECIPE_MAX_FLDS];
};

#define ULP_RECIPE_TYPE_MAX (BNXT_ULP_RESOURCE_SUB_TYPE_KEY_RECIPE_TABLE_WM + 1)
struct bnxt_ulp_key_recipe_info {
	uint32_t num_recipes;
	uint8_t max_fields;
	struct bnxt_ulp_key_recipe_entry **recipes[BNXT_ULP_DIRECTION_LAST][ULP_RECIPE_TYPE_MAX];
	struct bitalloc *recipe_ba[BNXT_ULP_DIRECTION_LAST][ULP_RECIPE_TYPE_MAX];
};

struct ulp_mapper_core_ops;

struct bnxt_ulp_mapper_data {
	const struct ulp_mapper_core_ops *mapper_oper;
	struct bnxt_ulp_mapper_glb_resource_entry
		glb_res_tbl[TF_DIR_MAX][BNXT_ULP_GLB_RF_IDX_LAST];
	struct ulp_mapper_gen_tbl_list gen_tbl_list[BNXT_ULP_GEN_TBL_MAX_SZ];
	struct bnxt_ulp_key_recipe_info key_recipe_info;
	struct ulp_allocator_tbl_entry alloc_tbl[BNXT_ULP_ALLOCATOR_TBL_MAX_SZ];
};

/* Internal Structure for passing the arguments around */
struct bnxt_ulp_mapper_parms {
	enum bnxt_ulp_template_type		tmpl_type;
	uint32_t				dev_id;
	uint32_t				act_tid;
	uint32_t				class_tid;
	struct ulp_rte_act_prop			*act_prop;
	struct ulp_rte_act_bitmap		*act_bitmap;
	struct ulp_rte_hdr_bitmap		*hdr_bitmap;
	struct ulp_rte_hdr_bitmap		*enc_hdr_bitmap;
	struct ulp_rte_hdr_field		*hdr_field;
	struct ulp_rte_hdr_field		*enc_field;
	struct ulp_rte_field_bitmap		*fld_bitmap;
	uint64_t				*comp_fld;
	struct ulp_regfile			*regfile;
	struct bnxt_ulp_context			*ulp_ctx;
	uint32_t				flow_id;
	uint16_t				func_id;
	uint32_t				rid;
	enum bnxt_ulp_fdb_type			flow_type;
	struct bnxt_ulp_mapper_data		*mapper_data;
	struct bnxt_ulp_device_params		*device_params;
	uint32_t				child_flow;
	uint32_t				parent_flow;
	uint8_t					tun_idx;
	uint32_t				app_priority;
	uint64_t				shared_hndl;
	uint32_t				flow_pattern_id;
	uint32_t				act_pattern_id;
	uint8_t					app_id;
	uint16_t				port_id;
	uint16_t				fw_fid;
	uint64_t				cf_bitmap;
	uint64_t				wc_field_bitmap;
	uint64_t				exclude_field_bitmap;
	struct tfc_mpc_batch_info_t		batch_info;
};

/* Function to initialize any dynamic mapper data. */
struct ulp_mapper_core_ops {
	int32_t
	(*ulp_mapper_core_tcam_tbl_process)(struct bnxt_ulp_mapper_parms *parms,
					    struct bnxt_ulp_mapper_tbl_info *t);
	int32_t
	(*ulp_mapper_core_tcam_entry_free)(struct bnxt_ulp_context *ulp_ctx,
					   struct ulp_flow_db_res_params *res);
	int32_t
	(*ulp_mapper_core_em_tbl_process)(struct bnxt_ulp_mapper_parms *parms,
					  struct bnxt_ulp_mapper_tbl_info *t,
					  void *error);
	int32_t
	(*ulp_mapper_core_em_entry_free)(struct bnxt_ulp_context *ulp,
					 struct ulp_flow_db_res_params *res,
					 void *error);

	int32_t
	(*ulp_mapper_core_index_tbl_process)(struct bnxt_ulp_mapper_parms *parm,
					     struct bnxt_ulp_mapper_tbl_info
					     *t);
	int32_t
	(*ulp_mapper_core_index_entry_free)(struct bnxt_ulp_context *ulp,
					    struct ulp_flow_db_res_params *res);
	int32_t
	(*ulp_mapper_core_cmm_tbl_process)(struct bnxt_ulp_mapper_parms *parm,
					   struct bnxt_ulp_mapper_tbl_info *t,
					   void  *error);
	int32_t
	(*ulp_mapper_core_cmm_entry_free)(struct bnxt_ulp_context *ulp,
					  struct ulp_flow_db_res_params *res,
					  void *error);
	int32_t
	(*ulp_mapper_core_if_tbl_process)(struct bnxt_ulp_mapper_parms *parms,
					  struct bnxt_ulp_mapper_tbl_info *t);

	int32_t
	(*ulp_mapper_core_ident_alloc_process)(struct bnxt_ulp_context *ulp_ctx,
					       uint32_t session_type,
					       uint16_t ident_type,
					       uint8_t direction,
					       enum cfa_track_type tt,
					       uint64_t *identifier_id);

	int32_t
	(*ulp_mapper_core_index_tbl_alloc_process)(struct bnxt_ulp_context *ulp,
						   uint32_t session_type,
						   uint16_t table_type,
						   uint8_t direction,
						   uint64_t *index);
	int32_t
	(*ulp_mapper_core_ident_free)(struct bnxt_ulp_context *ulp_ctx,
				      struct ulp_flow_db_res_params *res);
	uint32_t
	(*ulp_mapper_core_dyn_tbl_type_get)(struct bnxt_ulp_mapper_parms *parms,
					    struct bnxt_ulp_mapper_tbl_info *t,
					    uint16_t blob_len,
					    uint16_t *out_len);
	int32_t
	(*ulp_mapper_core_app_glb_res_info_init)(struct bnxt_ulp_context *ulp_ctx,
						 struct bnxt_ulp_mapper_data *mapper_data);

	int32_t
	(*ulp_mapper_core_handle_to_offset)(struct bnxt_ulp_mapper_parms *parms,
					    uint64_t handle,
					    uint32_t offset,
					    uint64_t *result);
	int
	(*ulp_mapper_mpc_batch_start)(struct tfc_mpc_batch_info_t *batch_info);

	bool
	(*ulp_mapper_mpc_batch_started)(struct tfc_mpc_batch_info_t *batch_info);

	int
	(*ulp_mapper_mpc_batch_end)(struct tfc *tfcp,
				    struct tfc_mpc_batch_info_t *batch_info);
};

extern const struct ulp_mapper_core_ops ulp_mapper_tf_core_ops;
extern const struct ulp_mapper_core_ops ulp_mapper_tfc_core_ops;

int32_t
ulp_mapper_glb_resource_read(struct bnxt_ulp_mapper_data *mapper_data,
			     enum tf_dir dir,
			     uint16_t idx,
			     uint64_t *regval,
			     bool *shared);

int32_t
ulp_mapper_glb_resource_write(struct bnxt_ulp_mapper_data *data,
			      struct bnxt_ulp_glb_resource_info *res,
			      uint64_t regval, bool shared);

int32_t
ulp_mapper_resource_ident_allocate(struct bnxt_ulp_context *ulp_ctx,
				   struct bnxt_ulp_mapper_data *mapper_data,
				   struct bnxt_ulp_glb_resource_info *glb_res,
				   bool shared);

int32_t
ulp_mapper_resource_index_tbl_alloc(struct bnxt_ulp_context *ulp_ctx,
				    struct bnxt_ulp_mapper_data *mapper_data,
				    struct bnxt_ulp_glb_resource_info *glb_res,
				    bool shared);

struct bnxt_ulp_mapper_key_info *
ulp_mapper_key_fields_get(struct bnxt_ulp_mapper_parms *mparms,
			  struct bnxt_ulp_mapper_tbl_info *tbl,
			  uint32_t *num_flds);

uint32_t
ulp_mapper_partial_key_fields_get(struct bnxt_ulp_mapper_parms *mparms,
				  struct bnxt_ulp_mapper_tbl_info *tbl);

int32_t
ulp_mapper_fdb_opc_process(struct bnxt_ulp_mapper_parms *parms,
			   struct bnxt_ulp_mapper_tbl_info *tbl,
			   struct ulp_flow_db_res_params *fid_parms);

int32_t
ulp_mapper_priority_opc_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				uint32_t *priority);

int32_t
ulp_mapper_tbl_ident_scan_ext(struct bnxt_ulp_mapper_parms *parms,
			      struct bnxt_ulp_mapper_tbl_info *tbl,
			      uint8_t *byte_data,
			      uint32_t byte_data_size,
			      enum bnxt_ulp_byte_order byte_order);

int32_t
ulp_mapper_field_opc_process(struct bnxt_ulp_mapper_parms *parms,
			     enum tf_dir dir,
			     struct bnxt_ulp_mapper_field_info *fld,
			     struct ulp_blob *blob,
			     uint8_t is_key,
			     const char *name);

int32_t
ulp_mapper_key_recipe_field_opc_process(struct bnxt_ulp_mapper_parms *parms,
					uint8_t dir,
					struct bnxt_ulp_mapper_field_info *fld,
					uint8_t is_key,
					const char *name,
					bool *written,
					struct bnxt_ulp_mapper_field_info *ofld);

int32_t
ulp_mapper_tbl_result_build(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl,
			    struct ulp_blob *data,
			    const char *name);

int32_t
ulp_mapper_mark_gfid_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl,
			     uint64_t flow_id);

int32_t
ulp_mapper_mark_act_ptr_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl);

int32_t
ulp_mapper_mark_vfr_idx_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl);

int32_t
ulp_mapper_tcam_tbl_ident_alloc(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl);

uint32_t
ulp_mapper_wc_tcam_tbl_dyn_post_process(struct bnxt_ulp_device_params *dparms,
					struct ulp_blob *key,
					struct ulp_blob *mask,
					struct ulp_blob *tkey,
					struct ulp_blob *tmask);

void ulp_mapper_wc_tcam_tbl_post_process(struct ulp_blob *blob);

int32_t
ulp_mapper_resources_free(struct bnxt_ulp_context *ulp_ctx,
			  enum bnxt_ulp_fdb_type flow_type,
			  uint32_t fid,
			  void *error);

int32_t
ulp_mapper_flow_destroy(struct bnxt_ulp_context *ulp_ctx,
			enum bnxt_ulp_fdb_type flow_type,
			uint32_t fid,
			void *error);

int32_t
ulp_mapper_flow_create(struct bnxt_ulp_context	*ulp_ctx,
		       struct bnxt_ulp_mapper_parms *parms,
		       void *error);

struct bnxt_ulp_mapper_key_info *
ulp_mapper_key_recipe_fields_get(struct bnxt_ulp_mapper_parms *parms,
				 struct bnxt_ulp_mapper_tbl_info *tbl,
				 uint32_t *num_flds);

int32_t
ulp_mapper_init(struct bnxt_ulp_context	*ulp_ctx);

void
ulp_mapper_deinit(struct bnxt_ulp_context *ulp_ctx);

#ifdef TF_FLOW_SCALE_QUERY
int32_t
ulp_resc_usage_sync(struct bnxt_ulp_context *ulp_ctx);
#endif /* TF_FLOW_SCALE_QUERY */

#endif /* _ULP_MAPPER_H_ */
