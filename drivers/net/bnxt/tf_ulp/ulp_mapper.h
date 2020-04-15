/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_MAPPER_H_
#define _ULP_MAPPER_H_

#include <tf_core.h>
#include <rte_log.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include "ulp_template_db.h"
#include "ulp_template_struct.h"
#include "bnxt_ulp.h"
#include "ulp_utils.h"

#define ULP_SZ_BITS2BYTES(x) (((x) + 7) / 8)

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
};

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
