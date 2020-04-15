/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_MAPPER_H_
#define _ULP_MAPPER_H_

#include <rte_log.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include "tf_core.h"
#include "ulp_template_db.h"
#include "ulp_template_struct.h"
#include "bnxt_ulp.h"
#include "ulp_utils.h"

#define ULP_SZ_BITS2BYTES(x) (((x) + 7) / 8)
#define ULP_IDENTS_INVALID ((uint16_t)0xffff)
#define ULP_MAPPER_CACHE_RES_TBL_ID_SHFT 16
#define ULP_MAPPER_CACHE_RES_TBL_TYPE_SHFT 0
#define ULP_MAPPER_CACHE_RES_TBL_MASK ((uint32_t)0x0000ffff)

/*
 * The cache table opcode is used to convey informat from the cache handler
 * to the tcam handler.  The opcodes do the following:
 * NORMAL - tcam should process all instructions as normal
 * SKIP - tcam is using the cached entry and doesn't need to process the
 *	instruction.
 * ALLOC - tcam needs to allocate the tcam index and store in the cache entry
 */
enum bnxt_ulp_cache_table_opc {
	BNXT_ULP_MAPPER_TCAM_TBL_OPC_NORMAL,
	BNXT_ULP_MAPPER_TCAM_TBL_OPC_CACHE_SKIP,
	BNXT_ULP_MAPPER_TCAM_TBL_OPC_CACHE_ALLOC
};

struct bnxt_ulp_mapper_cache_entry {
	uint32_t ref_count;
	uint16_t tcam_idx;
	uint16_t idents[BNXT_ULP_CACHE_TBL_IDENT_MAX_NUM];
	uint8_t ident_types[BNXT_ULP_CACHE_TBL_IDENT_MAX_NUM];
};

struct bnxt_ulp_mapper_def_id_entry {
	enum tf_identifier_type ident_type;
	uint64_t ident;
};

struct bnxt_ulp_mapper_data {
	struct bnxt_ulp_mapper_def_id_entry
		dflt_ids[TF_DIR_MAX][BNXT_ULP_DEF_IDENT_INFO_TBL_MAX_SZ];
	struct bnxt_ulp_mapper_cache_entry
		*cache_tbl[BNXT_ULP_CACHE_TBL_MAX_SZ];
};

/* Internal Structure for passing the arguments around */
struct bnxt_ulp_mapper_parms {
	uint32_t				dev_id;
	enum bnxt_ulp_byte_order		order;
	uint32_t				act_tid;
	struct bnxt_ulp_mapper_act_tbl_info	*atbls;
	uint32_t				num_atbls;
	uint32_t				class_tid;
	struct bnxt_ulp_mapper_class_tbl_info	*ctbls;
	uint32_t				num_ctbls;
	struct ulp_rte_act_prop			*act_prop;
	struct ulp_rte_act_bitmap		*act_bitmap;
	struct ulp_rte_hdr_field		*hdr_field;
	struct ulp_regfile			*regfile;
	struct tf				*tfp;
	struct bnxt_ulp_context			*ulp_ctx;
	uint8_t					encap_byte_swap;
	uint32_t				fid;
	enum bnxt_ulp_flow_db_tables		tbl_idx;
	struct bnxt_ulp_mapper_data		*mapper_data;
	enum bnxt_ulp_cache_table_opc		tcam_tbl_opc;
	struct bnxt_ulp_mapper_cache_entry	*cache_ptr;
};

struct bnxt_ulp_mapper_create_parms {
	uint32_t			app_priority;
	struct ulp_rte_hdr_bitmap	*hdr_bitmap;
	struct ulp_rte_hdr_field	*hdr_field;
	struct ulp_rte_act_bitmap	*act;
	struct ulp_rte_act_prop		*act_prop;
	uint32_t			class_tid;
	uint32_t			act_tid;
	uint16_t			func_id;
	enum ulp_direction_type		dir;
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
		       struct bnxt_ulp_mapper_create_parms *parms,
		       uint32_t *flowid);

/* Function that frees all resources associated with the flow. */
int32_t
ulp_mapper_flow_destroy(struct bnxt_ulp_context	*ulp_ctx, uint32_t fid);

/*
 * Function that frees all resources and can be called on default or regular
 * flows
 */
int32_t
ulp_mapper_resources_free(struct bnxt_ulp_context	*ulp_ctx,
			  uint32_t fid,
			  enum bnxt_ulp_flow_db_tables	tbl_type);

#endif /* _ULP_MAPPER_H_ */
