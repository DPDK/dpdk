/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_MAPPER_H_
#define _ULP_MAPPER_H_

#include <rte_log.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include "tf_core.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_ulp.h"
#include "ulp_utils.h"
#include "ulp_gen_tbl.h"

#define ULP_IDENTS_INVALID ((uint16_t)0xffff)

struct bnxt_ulp_mapper_glb_resource_entry {
	enum bnxt_ulp_resource_func	resource_func;
	uint32_t			resource_type; /* TF_ enum type */
	uint64_t			resource_hndl;
	bool				shared;
};

struct bnxt_ulp_mapper_data {
	struct bnxt_ulp_mapper_glb_resource_entry
		glb_res_tbl[TF_DIR_MAX][BNXT_ULP_GLB_RF_IDX_LAST];
	struct ulp_mapper_gen_tbl_list gen_tbl_list[BNXT_ULP_GEN_TBL_MAX_SZ];
};

/* Internal Structure for passing the arguments around */
struct bnxt_ulp_mapper_parms {
	enum bnxt_ulp_template_type		tmpl_type;
	uint32_t				dev_id;
	uint32_t				act_tid;
	struct bnxt_ulp_mapper_tbl_info		*atbls; /* action table */
	uint32_t				num_atbls;
	uint32_t				class_tid;
	struct bnxt_ulp_mapper_tbl_info		*ctbls; /* class table */
	uint32_t				num_ctbls;
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
	uint32_t				fid;
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
};

struct bnxt_ulp_mapper_create_parms {
	uint32_t			app_priority;
	struct ulp_rte_hdr_bitmap	*hdr_bitmap;
	struct ulp_rte_hdr_bitmap	*enc_hdr_bitmap;
	struct ulp_rte_hdr_field	*hdr_field;
	struct ulp_rte_hdr_field	*enc_field;
	uint64_t			*comp_fld;
	struct ulp_rte_act_bitmap	*act;
	struct ulp_rte_act_prop		*act_prop;
	struct ulp_rte_field_bitmap	*fld_bitmap;
	uint32_t			class_tid;
	uint32_t			act_tid;
	uint16_t			func_id;
	uint32_t			dir_attr;
	enum bnxt_ulp_fdb_type		flow_type;

	uint32_t			flow_id;
	/* if set then create it as a child flow */
	uint32_t			child_flow;
	/* if set then create a parent flow */
	uint32_t			parent_flow;
	uint8_t				tun_idx;
	uint64_t			shared_hndl;

	/* support pattern based rejection */
	uint32_t			flow_pattern_id;
	uint32_t			act_pattern_id;
	uint8_t				app_id;
	uint16_t			port_id;
};

/* Function to initialize any dynamic mapper data. */
int32_t
ulp_mapper_init(struct bnxt_ulp_context	*ulp_ctx);

/* Function to release all dynamic mapper data. */
void
ulp_mapper_deinit(struct bnxt_ulp_context *ulp_ctx);

/*
 * Function to handle the mapping of the Flow to be compatible
 * with the underlying hardware.
 */
int32_t
ulp_mapper_flow_create(struct bnxt_ulp_context	*ulp_ctx,
		       struct bnxt_ulp_mapper_create_parms *parms);

/* Function that frees all resources associated with the flow. */
int32_t
ulp_mapper_flow_destroy(struct bnxt_ulp_context *ulp_ctx,
			enum bnxt_ulp_fdb_type flow_type,
			uint32_t fid);

/*
 * Function that frees all resources and can be called on default or regular
 * flows
 */
int32_t
ulp_mapper_resources_free(struct bnxt_ulp_context *ulp_ctx,
			  enum bnxt_ulp_fdb_type flow_type,
			  uint32_t fid);

int32_t
ulp_mapper_get_shared_fid(struct bnxt_ulp_context *ulp,
			  uint32_t id,
			  uint16_t key,
			  uint32_t *fid);

#endif /* _ULP_MAPPER_H_ */
