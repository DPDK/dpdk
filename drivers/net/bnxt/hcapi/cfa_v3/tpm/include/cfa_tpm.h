/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_TPM_H_
#define _CFA_TPM_H_

#include "cfa_types.h"

/**
 * @addtogroup CFA_TPM CFA Table Scope Pool Manager
 * \ingroup CFA_V3
 * The purpose of the CFA Table Scope pool manager is to provide a centralized
 * management of Table Scope region pools. Each CFA TPM instance manages the
 * pools belonging to one region. The Table Scope Pool Manager(TPM) keeps
 * track of fids that are using the pools.
 *  @{
 */

/** CFA Table Scope Pool Manager query DB size API
 *
 * This API returns the size of memory required for internal data structures to
 * manage the table scope pool ids, and user fids.
 *
 * @param[in] max_pools
 *   Maximum number of pool ids available to manage.
 *
 * @param[out] tpm_db_size
 *   Pointer to 32 bit integer to return the amount of memory required.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_query(uint16_t max_pools, uint32_t *tpm_db_size);

/** CFA Table Scope Pool Manager open API
 *
 * This API initializes the CFA Table Scope Pool Manager database
 *
 * @param[in] tpm
 *   Pointer to the memory used for the CFA Table Scope Pool Manager Database.
 *
 * @param[in] tpm_db_size
 *   The size of memory block pointed to by tpm parameter.
 *
 * @param[in] max_pools
 *   Maximum number of pool ids to manage.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_open(void *tpm, uint32_t tpm_db_size, uint16_t max_pools);

/** CFA Table Scope Pool Manager close API
 *
 * This API resets the CFA Table Scope Pool Manager database
 *
 * @param[in] tpm
 *   Pointer to the database memory for the Table Scope Pool Manager.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_close(void *tpm);

/** CFA Table Scope pool Manager alloc API
 *
 * This API allocates a pool Id.
 *
 * @param[in] tpm
 *   Pointer to the database memory for the Table Scope Pool Manager.
 *
 * @param[out] pool_id
 *   Pointer to memory location to return the allocated Pool Id.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_alloc(void *tpm, uint16_t *pool_id);

/** CFA Table Scope Pool Manager free API
 *
 * This API frees a previously allocated Pool Id.
 *
 * @param[in] tpm
 *   Pointer to the database memory for the Table Scope Pool Manager.
 *
 * @param[in] pool_id
 *   Pool Id to be freed.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_free(void *tpm, uint16_t pool_id);

/** CFA Table Scope Pool Manager add fid API
 *
 * This API adds an fid to a Pool Id.
 *
 * @param[in] tpm
 *   Pointer to the database memory for the Table Scope Pool Manager.
 *
 * @param[in] pool_id
 *   Pool Id to which the fid has to be added.
 *
 * @param[in] fid
 *   Function id to be added.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_fid_add(void *tpm, uint16_t pool_id, uint16_t fid);

/** CFA Table Scope Pool Manager remove fid API
 *
 * This API removes a previously added fid from a Pool Id.
 *
 * @param[in] tpm
 *   Pointer to the database memory for the Table Scope Pool Manager.
 *
 * @param[in] pool_id
 *   Pool Id from which the fid has to be removed.
 *
 * @param[in] fid
 *   Function id to be removed.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_fid_rem(void *tpm, uint16_t pool_id, uint16_t fid);

/** CFA Table Scope Pool Manager search by pool id API
 *
 * This API searches for the fid that is added to the pool id.
 *
 * @param[in] tpm
 *   Pointer to the database memory for the Table Scope Pool Manager.
 *
 * @param[in] pool_id
 *   Pool id to be searched for.
 *
 * @param[out] fid
 *   Pointer to memory location to return the fid that is added
 *   to the Pool id..
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_srch_by_pool(void *tpm, uint16_t pool_id, uint16_t *fid);

/** CFA Table Scope Pool Manager search by fid API
 *
 * This API searches for the Pool ids to which fid is added.
 *
 * @param[in] tpm
 *   Pointer to the database memory for the Table Scope Pool Manager.
 *
 * @param[in] srch_mode
 *   srch_mode indicates if the iteration is for the first match, which
 *   indicates the start of new iteration or for the next match.
 *
 * @param[in] fid
 *   Function id to be searched for.
 *
 * @param[out] pool_id
 *   Pointer to memory location to return the Pool Id to which fid is
 *   added.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_srchm_by_fid(void *tpm, enum cfa_srch_mode srch_mode, uint16_t fid,
			 uint16_t *pool_id);

/** CFA Table Scope Pool Manager set pool size API
 *
 * This API sets the pool size into TPM.
 *
 * @param[in] tpm
 *   Pointer to the database memory for the Table Scope Pool Manager.
 *
 * @param[in] pool_sz_exp
 *   The size of each pool in power of 2.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_pool_size_set(void *tpm, uint8_t pool_sz_exp);

/** CFA Table Scope Pool Manager get pool size API
 *
 * This API returns the pool size from TPM.
 *
 * @param[in] tpm
 *   Pointer to the database memory for the Table Scope Pool Manager.
 *
 * @param[out] pool_sz_exp
 *   Pointer to memory location to return the pool size in power of 2.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tpm_pool_size_get(void *tpm, uint8_t *pool_sz_exp);

/**@}*/

#endif /* _CFA_TPM_H_ */
