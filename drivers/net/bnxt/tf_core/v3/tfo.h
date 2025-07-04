/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */
#ifndef _TFO_H_
#define _TFO_H_

#include <stdint.h>
#include <stdbool.h>
#include "rte_memzone.h"
#include "cfa_types.h"
#include "cfa_bld_mpcops.h"
#include "tfc.h"
#include "tfc_cpm.h"

/**
 * @file
 *
 * @brief TFC (Truflow Core v3) Object Header File
 *
 * @page TFCOV3 Truflow Core v3 Object
 *
 * The TF Object stores internal TFC data.
 * This data must be set/get using accessor functions.
 *
 * @ref tfo_open
 *
 * @ref tfo_close
 *
 * @ref tfo_mpcinfo_get
 *
 * @ref tfo_ts_set
 *
 * @ref tfo_ts_get
 *
 * @ref tfo_ts_validate
 *
 * @ref tfo_ts_set_mem_cfg
 *
 * @ref tfo_ts_get_mem_cfg
 *
 * @ref tfo_ts_get_cpm_inst
 *
 * @ref tfo_ts_set_cpm_inst
 *
 * @ref tfo_ts_get_pool_info
 *
 * @ref tfo_ts_set_pool_info
 *
 * @ref tfo_sid_set
 *
 * @ref tfo_sid_get
 */

/** Invalid Table Scope ID */
#define INVALID_TSID UINT8_MAX

/** Maximum number of table scopes */
#define TFC_TBL_SCOPE_MAX 32

/** Backing store/memory page levels */
enum tfc_ts_pg_tbl_lvl {
	TFC_TS_PT_LVL_0 = 0,
	TFC_TS_PT_LVL_1,
	TFC_TS_PT_LVL_2,
	TFC_TS_PT_LVL_MAX
};

/**
 * Backing store and page table control struct that
 * is used for storing the memory zone pointer and
 * page allocation.
 */
struct tfc_ts_mz {
	const struct rte_memzone *mz;
	uint32_t page_count;
	uint32_t page_size;
	uint32_t alloc_count;
};

/**
 * Backing store/memory page table level config structure
 */
struct tfc_ts_page_tbl {
	uint64_t *pg_pa_tbl; /**< Array of pointers to physical addresses */
	void    **pg_va_tbl; /**< Array of pointers to virtual addresses */
	uint32_t pg_count; /**< Number of pages in this level */
	uint32_t pg_size; /**< Size of each page in bytes */
};

/**
 * Backing store/memory config structure
 */
struct tfc_ts_mem_cfg {
	/** page table configuration */
	struct tfc_ts_page_tbl pg_tbl[TFC_TS_PT_LVL_MAX];
	uint64_t num_data_pages; /**< Total number of pages */
	uint64_t l0_dma_addr; /**< Physical base memory address */
	void *l0_addr; /**< Virtual base memory address */
	int num_lvl; /**< Number of page levels */
	uint32_t page_cnt[TFC_TS_PT_LVL_MAX]; /**< Page count per level */
	uint32_t rec_cnt; /**< Total number of records in memory */
	uint32_t lkup_rec_start_offset; /**< Offset of lkup rec start (in records) */
	uint32_t entry_size; /**< Size of record in bytes */
	struct tfc_ts_mz ts_mz; /**< Memory zone control struct */
};

/**
 * Backing store pool info
 */
struct tfc_ts_pool_info {
	uint16_t lkup_max_contig_rec; /**< max contig records */
	uint16_t act_max_contig_rec; /**< max contig records */
	uint8_t lkup_pool_sz_exp; /**< lookup pool size exp */
	uint8_t act_pool_sz_exp; /**< action pool size exp */
	struct tfc_cpm *lkup_cpm; /**< CPM lookup pool manager pointer */
	struct tfc_cpm *act_cpm; /**< CPM action pool manager pointer */
};


/* TFO APIs */

/**
 * Allocate a TFC object for this DPDK port/function.
 *
 * @param[out] tfo
 *   Pointer to TFC object
 *
 * @param[in] is_pf
 *   Indicates whether the DPDK port is a PF.
 */
void tfo_open(void **tfo, bool is_pf);

/**
 * Free the TFC object for this DPDK port/function.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 */
void tfo_close(void **tfo);

/**
 * Validate table scope id
 *
 * @param[in] tfo
 *
 * @param[in] ts_tsid
 *
 * @param[out] ts_valid
 *
 * @return 0 for tsid within range
 */
int tfo_ts_validate(void *tfo, uint8_t ts_tsid, bool *ts_valid);
/**
 * Set the table scope configuration.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] ts_tsid
 *   The table scope ID
 *
 * @param[in] ts_is_shared
 *   True if the table scope is shared
 *
 * @param[in] ts_app
 *   Application type TF/AFM
 *
 * @param[in] ts_valid
 *   True if the table scope is valid
 *
 * @param[in] ts_max_pools
 *   Maximum number of pools if shared.
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_set(void *tfo, uint8_t ts_tsid, bool ts_is_shared,
	       enum cfa_app_type ts_app, bool ts_valid,
	       uint16_t ts_max_pools);

/**
 * Get the table scope configuration.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] ts_tsid
 *   The table scope ID
 *
 * @param[out] ts_is_shared
 *   True if the table scope is shared
 *
 * @param[out] ts_app
 *   Application type TF/AFM
 *
 * @param[out] ts_valid
 *   True if the table scope is valid
 *
 * @param[out] ts_max_pools
 *   Maximum number of pools returned if shared.
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_get(void *tfo, uint8_t ts_tsid, bool *ts_is_shared,
	       enum cfa_app_type *ts_app, bool *ts_valid,
	       uint16_t *ts_max_pools);

/**
 * Set the table scope memory configuration for this direction.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] ts_tsid
 *   The table scope ID
 *
 * @param[in] dir
 *   The direction (RX/TX)
 *
 * @param[in] region
 *   The memory region (lookup/action)
 *
 * @param[in] is_bs_owner
 *   True if the caller is the owner of the backing store
 *
 * @param[in] mem_cfg
 *   Backing store/memory config structure
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_set_mem_cfg(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
		       enum cfa_region_type region, bool is_bs_owner,
		       struct tfc_ts_mem_cfg *mem_cfg);

/**
 * Get the table scope memory configuration for this direction.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] ts_tsid
 *   The table scope ID
 *
 * @param[in] dir
 *   The direction (RX/TX)
 *
 * @param[in] region
 *   The memory table region (lookup/action)
 *
 * @param[out] is_bs_owner
 *   True if the caller is the owner of the backing store
 *
 * @param[out] mem_cfg
 *   Backing store/memory config structure
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_get_mem_cfg(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
		       enum cfa_region_type region, bool *is_bs_owner,
		       struct tfc_ts_mem_cfg *mem_cfg);

/**
 * Set the pool memory configuration for this direction.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] ts_tsid
 *   The table scope ID
 *
 * @param[in] dir
 *   The direction (RX/TX)
 *
 * @param[out] ts_pool
 *   Table scope pool info
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_get_pool_info(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
			 struct tfc_ts_pool_info *ts_pool);

/**
 * Get the pool memory configuration for this direction.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] ts_tsid
 *   The table scope ID
 *
 * @param[in] dir
 *   The direction (RX/TX)
 *
 * @param[in] ts_pool
 *   Table scope pool info
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_set_pool_info(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
			 struct tfc_ts_pool_info *ts_pool);


/** Get the Pool Manager instance
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] ts_tsid
 *   The table scope ID
 *
 * @param[in] dir
 *   The direction (RX/TX)
 *
 * @param[in] cpm_lkup
 *   Lookup CPM instance
 *
 * @param[in] cpm_act
 *   Action CPM instance
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_get_cpm_inst(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
			struct tfc_cpm **cpm_lkup, struct tfc_cpm **cpm_act);

/** Set the Pool Manager instance
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] ts_tsid
 *   The table scope ID
 *
 * @param[in] dir
 *   The direction (RX/TX)
 *
 * @param[in] cpm_lkup
 *   Lookup CPM instance
 *
 * @param[in] cpm_act
 *   Action CPM instance
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_set_cpm_inst(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
			struct tfc_cpm *cpm_lkup, struct tfc_cpm *cpm_act);

/** Get the MPC info reference
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] mpc_info
 *   MPC reference
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_mpcinfo_get(void *tfo, struct cfa_bld_mpcinfo **mpc_info);


/** Invalid session ID */
#define INVALID_SID UINT16_MAX

/**
 * Set the session ID.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] sid
 *   The session ID
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_sid_set(void *tfo, uint16_t sid);

/**
 * Get the session ID.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[out] sid
 *   The session ID
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_sid_get(void *tfo, uint16_t *sid);

/**
 * Set the table scope instance manager.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[in] tim
 *   Pointer to the table scope instance manager
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_tim_set(void *tfo, void *tim);

/**
 * Get the table scope instance manager.
 *
 * @param[in] tfo
 *   Pointer to TFC object
 *
 * @param[out] tim
 *   Pointer to a pointer to the table scope instance manager
 *
 * @return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_tim_get(void *tfo, void **tim);

#endif /* _TFO_H_ */
