/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_TIM_H_
#define _CFA_TIM_H_

/**
 * @addtogroup CFA_TIM CFA Table Scope Instance Manager
 * \ingroup CFA_V3
 * The purpose of the CFA Table Scope Instance manager is to provide a
 * centralized management of Table Scope Pool Manager instances. Each instance
 * is identified by the Table Scope id and Region id. A caller can set and
 * retrieve the instance handle using the Table Scope Id and Region Id.
 *  @{
 */

/** CFA Table Scope Instance Manager query DB size API
 *
 * This API returns the size of memory required for internal data structures to
 * manage the table scope instances.
 *
 * @param[in] max_tbl_scopes
 *   Maximum number of table scope ids available to manage.
 *
 * @param[in] max_regions
 *   Maximum number of regions per table scope.
 *
 * @param[out] tim_db_size
 *   Pointer to 32 bit integer to return the amount of memory required.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tim_query(uint8_t max_tbl_scopes, uint8_t max_regions,
		  uint32_t *tim_db_size);

/** CFA Table Scope Instance Manager open API
 *
 * This API initializes the CFA Table Scope Instance Manager database
 *
 * @param[in] tim
 *   Pointer to the memory used for the CFA Table Scope Instance Manager
 *   Database.
 *
 * @param[in] tim_db_size
 *   The size of memory block pointed to by tim parameter.
 *
 * @param[in] max_tbl_scopes
 *   Maximum number of table scope ids available to manage.
 *
 * @param[in] max_regions
 *   Maximum number of regions per table scope.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tim_open(void *tim, uint32_t tim_db_size, uint8_t max_tbl_scopes,
		 uint8_t max_regions);

/** CFA Table Scope Instance Manager close API
 *
 * This API resets the CFA Table Scope Instance Manager database
 *
 * @param[in] tim
 *   Pointer to the database memory for the Table Scope Instance Manager.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tim_close(void *tim);

/** CFA Table Scope Instance Manager set instance API
 *
 * This API sets the TPM instance handle into TIM.
 *
 * @param[in] tim
 *   Pointer to the database memory for the Table Scope Instance Manager.
 *
 * @param[in] tsid
 *   The Table scope id of the instance.
 *
 * @param[in] region_id
 *   The region id of the instance.
 *
 * @param[in] dir
 *   The direction of the instance.
 *
 * @param[in] tpm_inst
 *   The handle of TPM instance.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tim_tpm_inst_set(void *tim, uint8_t tsid, uint8_t region_id,
			 int dir, void *tpm_inst);

/** CFA Table Scope Instance Manager get instance API
 *
 * This API gets the TPM instance handle from TIM.
 *
 * @param[in] tim
 *   Pointer to the database memory for the Table Scope Instance Manager.
 *
 * @param[in] tsid
 *   The Table scope id of the instance.
 *
 * @param[in] region_id
 *   The region id of the instance.
 *
 * @param[in] dir
 *   The direction of the instance.
 *
 * @param[out] tpm_inst
 *   Pointer to memory location to return the handle of TPM instance.
 *
 * @return
 *   Returns 0 if successful, Error Code otherwise
 */
int cfa_tim_tpm_inst_get(void *tim, uint8_t tsid, uint8_t region_id,
			 int dir, void **tpm_inst);

/**@}*/

#endif /* _CFA_TIM_H_ */
