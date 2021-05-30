/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _HCAPI_CFA_H_
#define _HCAPI_CFA_H_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include "hcapi_cfa_defs.h"

#define INVALID_U64 (0xFFFFFFFFFFFFFFFFULL)
#define INVALID_U32 (0xFFFFFFFFUL)
#define INVALID_U16 (0xFFFFUL)
#define INVALID_U8 (0xFFUL)

struct hcapi_cfa_devops;

/**
 * CFA device information
 */
struct hcapi_cfa_devinfo {
	/** [out] CFA device ops function pointer table */
	const struct hcapi_cfa_devops *devops;
};

/**
 *  \defgroup CFA_HCAPI_DEVICE_API
 *  HCAPI used for writing to the hardware
 *  @{
 */

/** CFA device specific function hooks structure
 *
 * The following device hooks can be defined; unless noted otherwise, they are
 * optional and can be filled with a null pointer. The purpose of these hooks
 * to support CFA device operations for different device variants.
 */
struct hcapi_cfa_devops {
	/** calculate a key hash for the provided key_data
	 *
	 * This API computes hash for a key.
	 *
	 * @param[in] key_data
	 *   A pointer of the key data buffer
	 *
	 * @param[in] bitlen
	 *   Number of bits of the key data
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	uint64_t (*hcapi_cfa_key_hash)(uint64_t *key_data, uint16_t bitlen);

	/** hardware operation on the CFA EM key
	 *
	 * This API provides the functionality to program the exact match and
	 * key data to exact match record memory.
	 *
	 * @param[in] op
	 *   A pointer to the Hardware operation parameter
	 *
	 * @param[in] key_tbl
	 *   A pointer to the off-chip EM key table (applicable to EEM and
	 *   SR2 EM only), set to NULL for on-chip EM key table or WC
	 *   TCAM table.
	 *
	 * @param[in/out] key_obj
	 *   A pointer to the key data object for the hardware operation which
	 *   has the following contents:
	 *     1. key record memory offset (index to WC TCAM or EM key hash
	 *        value)
	 *     2. key data
	 *   When using the HWOP PUT, the key_obj holds the LREC and key to
	 *   be written.
	 *   When using the HWOP GET, the key_obj be populated with the LREC
	 *   and key which was specified by the key location object.
	 *
	 * @param[in/out] key_loc
	 *   When using the HWOP PUT, this is a pointer to the key location
	 *   data structure which holds the information of where the EM key
	 *   is stored.  It holds the bucket index and the data pointer of
	 *   a dynamic bucket that is chained to static bucket
	 *   When using the HWOP GET, this is a pointer to the key location
	 *   which should be retrieved.
	 *
	 *   (valid for SR2 only).
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	int (*hcapi_cfa_key_hw_op)(struct hcapi_cfa_hwop *op,
				   struct hcapi_cfa_key_tbl *key_tbl,
				   struct hcapi_cfa_key_data *key_data,
				   struct hcapi_cfa_key_loc *key_loc);
};

/*@}*/

extern const size_t CFA_RM_HANDLE_DATA_SIZE;

#if SUPPORT_CFA_HW_ALL
extern const struct hcapi_cfa_devops cfa_p4_devops;
extern const struct hcapi_cfa_devops cfa_p58_devops;

#elif defined(SUPPORT_CFA_HW_P4) && SUPPORT_CFA_HW_P4
extern const struct hcapi_cfa_devops cfa_p4_devops;
uint64_t hcapi_cfa_p4_key_hash(uint64_t *key_data, uint16_t bitlen);
/* SUPPORT_CFA_HW_P4 */
#elif defined(SUPPORT_CFA_HW_P58) && SUPPORT_CFA_HW_P58
extern const struct hcapi_cfa_devops cfa_p58_devops;
uint64_t hcapi_cfa_p58_key_hash(uint64_t *key_data, uint16_t bitlen);
/* SUPPORT_CFA_HW_P58 */
#endif

#endif /* HCAPI_CFA_H_ */
