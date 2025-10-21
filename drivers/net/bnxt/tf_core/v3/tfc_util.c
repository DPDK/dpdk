/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <string.h>

#include "tfc.h"
#include "tfo.h"
#include "tfc_util.h"

const char *
tfc_dir_2_str(enum cfa_dir dir)
{
	switch (dir) {
	case CFA_DIR_RX:
		return "RX";
	case CFA_DIR_TX:
		return "TX";
	default:
		return "Invalid direction";
	}
}

const char *
tfc_ident_2_str(enum cfa_resource_subtype_ident id_stype)
{
	switch (id_stype) {
	case CFA_RSUBTYPE_IDENT_L2CTX:
		return "ident_l2_ctx";
	case CFA_RSUBTYPE_IDENT_PROF_FUNC:
		return "ident_prof_func";
	case CFA_RSUBTYPE_IDENT_WC_PROF:
		return "ident_wc_prof";
	case CFA_RSUBTYPE_IDENT_EM_PROF:
		return "ident_em_prof";
	case CFA_RSUBTYPE_IDENT_L2_FUNC:
		return "ident_l2_func";
	default:
		return "Invalid identifier subtype";
	}
}

const char *
tfc_tcam_2_str(enum cfa_resource_subtype_tcam tcam_stype)
{
	switch (tcam_stype) {
	case CFA_RSUBTYPE_TCAM_L2CTX:
		return "tcam_l2_ctx";
	case CFA_RSUBTYPE_TCAM_PROF_TCAM:
		return "tcam_prof_tcam";
	case CFA_RSUBTYPE_TCAM_WC:
		return "tcam_wc";
	case CFA_RSUBTYPE_TCAM_CT_RULE:
		return "tcam_ct_rule";
	case CFA_RSUBTYPE_TCAM_VEB:
		return "tcam_veb";
	case CFA_RSUBTYPE_TCAM_FEATURE_CHAIN:
		return "tcam_fc";
	default:
		return "Invalid tcam subtype";
	}
}

const char *
tfc_idx_tbl_2_str(enum cfa_resource_subtype_idx_tbl tbl_stype)
{
	switch (tbl_stype) {
	case CFA_RSUBTYPE_IDX_TBL_STAT64:
		return "idx_tbl_64b_statistics";
	case CFA_RSUBTYPE_IDX_TBL_METER_PROF:
		return "idx_tbl_meter_prof";
	case CFA_RSUBTYPE_IDX_TBL_METER_INST:
		return "idx_tbl_meter_inst";
	case CFA_RSUBTYPE_IDX_TBL_MIRROR:
		return "idx_tbl_mirror";
	case CFA_RSUBTYPE_IDX_TBL_METADATA_PROF:
		return "idx_tbl_metadata_prof";
	case CFA_RSUBTYPE_IDX_TBL_METADATA_LKUP:
		return "idx_tbl_metadata_lkup";
	case CFA_RSUBTYPE_IDX_TBL_METADATA_ACT:
		return "idx_tbl_metadata_act";
	case CFA_RSUBTYPE_IDX_TBL_EM_FKB:
		return "idx_tbl_em_fkb";
	case CFA_RSUBTYPE_IDX_TBL_WC_FKB:
		return "idx_tbl_wc_fkb";
	case CFA_RSUBTYPE_IDX_TBL_EM_FKB_MASK:
		return "idx_tbl_em_fkb_mask";
	case CFA_RSUBTYPE_IDX_TBL_CT_STATE:
		return "idx_tbl_ct_state";
	case CFA_RSUBTYPE_IDX_TBL_RANGE_PROF:
		return "idx_tbl_range_prof";
	case CFA_RSUBTYPE_IDX_TBL_RANGE_ENTRY:
		return "idx_tbl_range_entry";
	case CFA_RSUBTYPE_IDX_TBL_DYN_UPAR:
		return "idx_tbl_dyn_upar";
	default:
		return "Invalid idx tbl subtype";
	}
}

const char *
tfc_if_tbl_2_str(enum cfa_resource_subtype_if_tbl tbl_stype)
{
	switch (tbl_stype) {
	case CFA_RSUBTYPE_IF_TBL_ILT:
		return "if_tbl_ilt";
	case CFA_RSUBTYPE_IF_TBL_VSPT:
		return "if_tbl_vspt";
	case CFA_RSUBTYPE_IF_TBL_PROF_PARIF_DFLT_ACT_PTR:
		return "if_tbl_parif_dflt_act_ptr";
	case CFA_RSUBTYPE_IF_TBL_PROF_PARIF_ERR_ACT_PTR:
		return "if_tbl_parif_err_act_ptr";
	case CFA_RSUBTYPE_IF_TBL_EPOCH0:
		return "if_tbl_epoch0";
	case CFA_RSUBTYPE_IF_TBL_EPOCH1:
		return "if_tbl_epoch1";
	case CFA_RSUBTYPE_IF_TBL_LAG:
		return "if_tbl_lag";
	default:
		return "Invalid if tbl subtype";
	}
}

const char *
tfc_ts_region_2_str(enum cfa_region_type region, enum cfa_dir dir)
{
	switch (region) {
	case CFA_REGION_TYPE_LKUP:
		if (dir == CFA_DIR_RX)
			return "ts_lookup_rx";
		else if (dir == CFA_DIR_TX)
			return "lookup_tx";
		else
			return "ts_lookup_invalid_dir";
	case CFA_REGION_TYPE_ACT:
		if (dir == CFA_DIR_RX)
			return "ts_action_rx";
		else if (dir == CFA_DIR_TX)
			return "ts_action_tx";
		else
			return "ts_action_invalid_dir";
	default:
		return "Invalid ts region";
	}
}

uint32_t
tfc_getbits(uint32_t *data, int offset, int blen)
{
	int start = offset >> 5;
	int end = (offset + blen - 1) >> 5;
	uint32_t val = data[start] >> (offset & 0x1f);

	if (start != end)
		val |= (data[start + 1] << (32 - (offset & 0x1f)));
	return (blen == 32) ? val : (val & ((1 << blen) - 1));
}

#define BITS_IN_VAR(x) (sizeof(x) * 8)

/*
 * Calculate the smallest power of 2 that is >= x.  The return value is the
 * exponent of 2.
 */
uint32_t next_pow2(uint32_t x)
{
	/*
	 * This algorithm calculates the nearest power of 2 greater than or
	 * equal to x:
	 * The function __builtin_clz returns the number of leading 0-bits in
	 * an unsigned int.
	 * Subtract this from the number of bits in x to get the power of 2.  In
	 * the examples below, an int is assumed to have 32 bits.
	 *
	 * Example 1:
	 *    x == 2
	 *    __builtin_clz(1) = 31
	 *    32 - 31 = 1
	 *    2^1 = 2
	 * Example 2:
	 *    x = 63
	 *    __builtin_clz(62) = 26
	 *    32 - 26 = 6
	 *    2^6 = 64
	 */
	return x == 1 ? 1 : (BITS_IN_VAR(x) - __builtin_clz(x - 1));
}

/*
 * Calculate the largest power of 2 that is less than x.  The return value is
 * the exponent of 2.
 */
uint32_t prev_pow2(uint32_t x)
{
	/*
	 * This algorithm calculates the nearest power of 2 less than x:
	 * The function __builtin_clz returns the number of leading 0-bits in
	 * an unsigned int.
	 * Subtract this from one less than the number of bits in x to get
	 * the power of 2.  In the examples below, an int is assumed to have
	 * 32 bits.
	 *
	 * Example 1:
	 *    x = 2
	 *    __builtin_clz(1) = 31
	 *    31 - 31 = 0
	 *    2^0 = 1
	 * Example 2:
	 *    x = 63
	 *    __builtin_clz(62) = 26
	 *    31 - 26 = 5
	 *    2^5 = 32
	 * Example 3:
	 *   x = 64
	 *    __builtin_clz(63) = 26
	 *    31 - 26 = 5
	 *    2^5 = 32
	 */
	return x == 1 ? 0 : (BITS_IN_VAR(x) - 1 - __builtin_clz(x - 1));
}

uint32_t roundup32(uint32_t x, uint32_t y)
{
	return ((x + y - 1) / y) * y;
}

uint64_t roundup64(uint64_t x, uint64_t y)
{
	return ((x + y - 1) / y) * y;
}
