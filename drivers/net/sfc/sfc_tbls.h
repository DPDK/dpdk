/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#ifndef _SFC_TBLS_H
#define _SFC_TBLS_H

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Table types:
 *   CAM - Content addressable memory
 *  BCAM - Binary CAM
 *  TCAM - Ternary CAM
 * STCAM - Semi-ternary CAM
 *
 * Short description:
 * TCAM:  Each entry has a key, mask, response and priority. An entry matches
 *        when (key & mask) == (request & mask). In the case of multiple
 *        matches, the entry with the highest priority wins; Each entry may
 *        have its own mask, but TCAM table definitions may place constraints
 *        on the possible masks allowed for each of the individual fields.
 * STCAM: A limited form of TCAM in which only a limited number of masks and
 *        associated priorities), up to some maximum fixed by the definition
 *        of the table, may be in use at any one time.
 * BCAM:  Each entry has only a key and response, with the whole request
 *        matched against the key (like a typical hash table or "map").
 * Direct (sometimes "DCAM", although it's not really content-addressable):
 *        Essentially just an array, where the key bits are used simply as an
 *        index.
 */

/* Priority is used only for TCAM or STCAM, use 0 in case of BCAM */
#define SFC_TBLS_BCAM_PRIORITY		0

/* Mask ID is used only for STCAM with ALLOC_MASKS flag, use 0 for BCAM */
#define SFC_TBLS_BCAM_MASK_ID		0

/* Mask is used only for STCAM */
#define SFC_TBLS_BCAM_MASK_WIDTH	0

static inline int
sfc_tbls_bcam_entry_insert(efx_nic_t *enp, efx_table_id_t table_id, uint16_t key_width,
			   uint16_t resp_width, uint8_t *data, unsigned int data_size)
{
	return efx_table_entry_insert(enp, table_id, SFC_TBLS_BCAM_PRIORITY,
				      SFC_TBLS_BCAM_MASK_ID, key_width,
				      SFC_TBLS_BCAM_MASK_WIDTH, resp_width,
				      data, data_size);
}

static inline int
sfc_tbls_bcam_entry_delete(efx_nic_t *enp, efx_table_id_t table_id, uint16_t key_width,
			   uint8_t *data, unsigned int data_size)
{
	return efx_table_entry_delete(enp, table_id, SFC_TBLS_BCAM_MASK_ID,
				      key_width, SFC_TBLS_BCAM_MASK_WIDTH,
				      data, data_size);
}

#ifdef __cplusplus
}
#endif
#endif /* _SFC_TBLS_H */
