/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_ULP_TFC_H_
#define _BNXT_ULP_TFC_H_

#include "bnxt.h"
#include <inttypes.h>

bool
bnxt_ulp_cntxt_shared_tbl_scope_enabled(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_tfcp_set(struct bnxt_ulp_context *ulp, struct tfc *tfcp);

struct tfc *
bnxt_ulp_cntxt_tfcp_get(struct bnxt_ulp_context *ulp);

uint32_t
bnxt_ulp_cntxt_tbl_scope_max_pools_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_tbl_scope_max_pools_set(struct bnxt_ulp_context *ulp_ctx,
				       uint32_t max);
enum tfc_tbl_scope_bucket_factor
bnxt_ulp_cntxt_em_mulitplier_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_em_mulitplier_set(struct bnxt_ulp_context *ulp_ctx,
				 enum tfc_tbl_scope_bucket_factor factor);

uint32_t
bnxt_ulp_cntxt_num_rx_flows_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_num_rx_flows_set(struct bnxt_ulp_context *ulp_ctx, uint32_t num);

uint32_t
bnxt_ulp_cntxt_num_tx_flows_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_num_tx_flows_set(struct bnxt_ulp_context *ulp_ctx, uint32_t num);

uint16_t
bnxt_ulp_cntxt_em_rx_key_max_sz_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_em_rx_key_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				    uint16_t max);

uint16_t
bnxt_ulp_cntxt_em_tx_key_max_sz_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_em_tx_key_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				    uint16_t max);

uint16_t
bnxt_ulp_cntxt_act_rec_rx_max_sz_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_act_rec_rx_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				     int16_t max);

uint16_t
bnxt_ulp_cntxt_act_rec_tx_max_sz_get(struct bnxt_ulp_context *ulp_ctx);

int32_t
bnxt_ulp_cntxt_act_rec_tx_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				     int16_t max);
uint32_t
bnxt_ulp_cntxt_page_sz_get(struct bnxt_ulp_context *ulp_ctxt);

int32_t
bnxt_ulp_cntxt_page_sz_set(struct bnxt_ulp_context *ulp_ctxt,
			   uint32_t page_sz);
#endif
