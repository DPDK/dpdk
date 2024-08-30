/****************************************************************************
 * Copyright(c) 2021 Broadcom Corporation, all rights reserved
 * Proprietary and Confidential Information.
 *
 * This source file is the property of Broadcom Corporation, and
 * may not be copied or distributed in any isomorphic form without
 * the prior written consent of Broadcom Corporation.
 *
 * @file cfa_tpm.h
 *
 * @brief CFA Table Scope Pool Manager private API definitions
 */

#ifndef _CFA_TPM_PRIV_H_
#define _CFA_TPM_PRIV_H_

#include "cfa_types.h"
#include "bitalloc.h"

#define CFA_TPM_SIGNATURE 0xCFACF0CD

#define CFA_TPM_MAX_POOLS 1040
#define CFA_TPM_MIN_POOLS 1

#define CFA_INVALID_FID UINT16_MAX

/**
 * CFA Table Scope Manager Pool Database
 *
 *  Structure used to store CFA Table Scope Pool Manager database info
 */
struct cfa_tpm {
	/* Signature of the CFA Table Scope Pool Manager Database */
	uint32_t signature;
	/* Maximum number of pools */
	uint16_t max_pools;
	/* Size of each pool, in powers of 2 */
	uint8_t pool_sz_exp;
	/* Next index for search multiple by fid */
	uint16_t next_index;
	/* Bitmap to keep track of pool usage */
	struct bitalloc *pool_ba;
	/* Fid table */
	uint16_t *fid_tbl;
};

#endif /* _CFA_TPM_PRIV_H_ */