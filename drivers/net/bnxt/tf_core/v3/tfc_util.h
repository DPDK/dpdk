/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_UTIL_H_
#define _TFC_UTIL_H_

#include "tfc.h"
#include "tfo.h"

#define TFC_MPC_ERROR_MIN -1
#define TFC_MPC_ERROR_MAX -9
static inline bool tfc_is_mpc_error(int rc)
{
	return ((((rc) <= TFC_MPC_ERROR_MIN) &&
		 ((rc) >= TFC_MPC_ERROR_MAX)) ? true : false);
}

/**
 * Helper function converting direction to text string
 *
 * [in] dir
 *   Receive or transmit direction identifier
 *
 * Returns:
 *   Pointer to a char string holding the string for the direction
 */
const char *tfc_dir_2_str(enum cfa_dir dir);

/**
 * Helper function converting identifier subtype to text string
 *
 * [in] id_stype
 *   Identifier subtype
 *
 * Returns:
 *   Pointer to a char string holding the string for the identifier
 */
const char *tfc_ident_2_str(enum cfa_resource_subtype_ident id_stype);

/**
 * Helper function converting tcam subtype to text string
 *
 * [in] tcam_stype
 *   TCAM subtype
 *
 * Returns:
 *   Pointer to a char string holding the string for the tcam
 */
const char *tfc_tcam_2_str(enum cfa_resource_subtype_tcam tcam_stype);

/**
 * Helper function converting index tbl subtype to text string
 *
 * [in] idx_tbl_stype
 *   Index table subtype
 *
 * Returns:
 *   Pointer to a char string holding the string for the table subtype
 */
const char *tfc_idx_tbl_2_str(enum cfa_resource_subtype_idx_tbl idx_tbl_stype);

/**
 * Helper function converting table scope lkup/act type and direction (region)
 * to string
 *
 * [in] region type
 *   Region type (LKUP/ACT)
 *
 * [in] dir
 *   Direction
 *
 * Returns:
 *   Pointer to a char string holding the string for the table subtype
 */
const char *tfc_ts_region_2_str(enum cfa_region_type region, enum cfa_dir dir);

/**
 * Helper function converting if tbl subtype to text string
 *
 * [in] if_tbl_stype
 *   If table subtype
 *
 * Returns:
 *   Pointer to a char string holding the string for the table subtype
 */
const char *tfc_if_tbl_2_str(enum cfa_resource_subtype_if_tbl if_tbl_stype);

/**
 * Helper function converting the scope type to text string
 *
 * [in] scope_type: table scope type
 *
 * Returns:
 *   Pointer to a char string holding the string for scope type
 */
const char *tfc_scope_type_2_str(enum cfa_scope_type scope_type);


/**
 * Helper function retrieving field value from the buffer
 *
 * [in] data
 *   buffer
 *
 * [in] offset
 *   field start bit position in the buffer
 *
 * [in] blen
 *   field length in bit
 *
 * Returns:
 *   field value
 */
uint32_t tfc_getbits(uint32_t *data, int offset, int blen);

/*
 * Calculate the smallest power of 2 that is >= x.  The return value is the
 * exponent of 2.
 */
uint32_t next_pow2(uint32_t x);

/* returns 0 if power of 2 */
int32_t is_pow2(uint32_t x);

/*
 * Calculate the largest power of 2 that is less than x.  The return value is
 * the exponent of 2.
 */
uint32_t prev_pow2(uint32_t x);

uint32_t roundup32(uint32_t x, uint32_t y);

uint64_t roundup64(uint64_t x, uint64_t y);

#endif /* _TFC_UTIL_H_ */
