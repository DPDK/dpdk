/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_MM_H_
#define _CFA_MM_H_

/**
 *  @addtogroup CFA_MM CFA Memory Manager
 *  \ingroup CFA_V3
 *  A CFA memory manager (Document Control:DCSG00988445) is a object instance
 *  within the CFA service module that is responsible for managing CFA related
 *  memories such as Thor2 CFA backings stores, Thor CFA action SRAM, etc. It
 *  is designed to operate in firmware or as part of the host Truflow stack.
 *  Each manager instance consists of a number of bank databases with each
 *  database managing a pool of CFA memory.
 *
 *  @{
 */

/** CFA Memory Manager database query params structure
 *
 *  Structure of database params
 */
struct cfa_mm_query_parms {
	/** [in] Maximum number of CFA records */
	uint32_t max_records;
	/** [in] Max contiguous CFA records per Alloc (Must be a power of 2). */
	uint32_t max_contig_records;
	/** [out] Memory required for Database */
	uint32_t db_size;
};

/** CFA Memory Manager open parameters
 *
 * Structure to store CFA MM open parameters
 */
struct cfa_mm_open_parms {
	/** [in] Size of memory allocated for CFA MM database */
	uint32_t db_mem_size;
	/** [in] Max number of CFA records */
	uint32_t max_records;
	/** [in] Maximum number of contiguous CFA records */
	uint16_t max_contig_records;
};

/** CFA Memory Manager record alloc  parameters
 *
 * Structure to contain parameters for record alloc
 */
struct cfa_mm_alloc_parms {
	/** [in] Number of contiguous CFA records */
	uint32_t num_contig_records;
	/** [out] Offset of the first of the records allocated */
	uint32_t record_offset;
	/** [out] Total number of records already allocated */
	uint32_t used_count;
	/** [out] Flag to indicate if all the records are allocated */
	uint32_t all_used;
};

/** CFA Memory Manager record free  parameters
 *
 * Structure to contain parameters for record free
 */
struct cfa_mm_free_parms {
	/** [in] Offset of the first of the records allocated */
	uint32_t record_offset;
	/** [in] Number of contiguous CFA records */
	uint32_t num_contig_records;
	/** [out] Total number of records already allocated */
	uint32_t used_count;
};

/** CFA Memory Manager query API
 *
 * This API returns the size of memory required for internal data structures to
 * manage the pool of CFA Records with given parameters.
 *
 * @param[in,out] parms
 *   CFA Memory manager query data base parameters.
 *
 * @return
 *   Returns 0 if the query is successful, Error Code otherwise
 */
int cfa_mm_query(struct cfa_mm_query_parms *parms);

/** CFA Memory Manager open API
 *
 * This API initializes the CFA Memory Manager database
 *
 * @param[in] cmm
 *   Pointer to the memory used for the CFA Mmeory Manager Database
 *
 * @param[in] parms
 *   CFA Memory manager data base parameters.
 *
 * @return
 *   Returns 0 if the initialization is successful, Error Code otherwise
 */
int cfa_mm_open(void *cmm, struct cfa_mm_open_parms *parms);

/** CFA Memory Manager close API
 *
 * This API frees the CFA Memory NManager database
 *
 * @param[in] cmm
 *   Pointer to the database memory for the record pool
 *
 * @return
 *   Returns 0 if the initialization is successful, Error Code otherwise
 */
int cfa_mm_close(void *cmm);

/** CFA Memory Manager Allocate CFA Records API
 *
 * This API allocates the request number of contiguous CFA Records
 *
 * @param[in] cmm
 *   Pointer to the database from which to allocate CFA Records
 *
 * @param[in,out] parms
 *   CFA MM alloc records parameters
 *
 * @return
 *   Returns 0 if the initialization is successful, Error Code otherwise
 */
int cfa_mm_alloc(void *cmm, struct cfa_mm_alloc_parms *parms);

/** CFA MemoryManager Free CFA Records API
 *
 * This API frees the requested number of contiguous CFA Records
 *
 * @param[in] cmm
 *   Pointer to the database from which to free CFA Records
 *
 * @param[in,out] parms
 *   CFA MM free records parameters
 *
 * @return
 *   Returns 0 if the initialization is successful, Error Code otherwise
 */
int cfa_mm_free(void *cmm, struct cfa_mm_free_parms *parms);

/** CFA Memory Manager Get Entry Size API
 *
 * This API retrieves the size of an allocated CMM entry.
 *
 * @param[in] cmm
 *   Pointer to the database from which to allocate CFA Records
 *
 * @param[in] entry_id
 *   Index of the allocated entry.
 *
 * @param[out] size
 *   Number of contiguous records in the entry.
 *
 * @return
 *   Returns 0 if successful, negative errno otherwise
 */
int cfa_mm_entry_size_get(void *cmm, uint32_t entry_id, uint8_t *size);

/**@}*/

#endif /* _CFA_MM_H_ */
