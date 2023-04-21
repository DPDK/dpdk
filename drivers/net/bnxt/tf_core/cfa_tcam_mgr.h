/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_TCAM_MGR_H_
#define _CFA_TCAM_MGR_H_

#include <errno.h>
#include "rte_common.h"
#include "hsi_struct_def_dpdk.h"
#include "tf_core.h"

/**
 * The TCAM module provides processing of Internal TCAM types.
 */

#ifndef TF_TCAM_MAX_SESSIONS
#define TF_TCAM_MAX_SESSIONS 16
#endif

#define ENTRY_ID_INVALID UINT16_MAX

#define TF_TCAM_PRIORITY_MIN 0
#define TF_TCAM_PRIORITY_MAX UINT16_MAX

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_array) (sizeof(_array) / sizeof(_array[0]))
#endif

/* Use TFP_DRV_LOG definition in tfp.h */
#define CFA_TCAM_MGR_LOG(level, fmt, args...)	\
	TFP_DRV_LOG(level, fmt, ## args)
#define CFA_TCAM_MGR_LOG_DIR(level, dir, fmt, args...)			\
	TFP_DRV_LOG(level, "%s: " fmt, tf_dir_2_str(dir), ## args)
#define CFA_TCAM_MGR_LOG_DIR_TYPE(level, dir, type, fmt, args...)	\
	TFP_DRV_LOG(level, "%s: %s " fmt, tf_dir_2_str(dir),		\
		    cfa_tcam_mgr_tbl_2_str(type), ## args)

#define CFA_TCAM_MGR_LOG_0(level, fmt)		\
	TFP_DRV_LOG(level, fmt)
#define CFA_TCAM_MGR_LOG_DIR_0(level, dir, fmt)			\
	TFP_DRV_LOG(level, "%s: " fmt, tf_dir_2_str(dir))
#define CFA_TCAM_MGR_LOG_DIR_TYPE_0(level, dir, type, fmt)	\
	TFP_DRV_LOG(level, "%s: %s " fmt, tf_dir_2_str(dir),	\
		    cfa_tcam_mgr_tbl_2_str(type))

#define CFA_TCAM_MGR_ERR_CODE(type) E ## type

/**
 * Checks 1 parameter against NULL.
 */
#define CFA_TCAM_MGR_CHECK_PARMS1(parms) do {				\
		if ((parms) == NULL) {					\
			CFA_TCAM_MGR_LOG_0(ERR, "Invalid Argument(s)\n"); \
			return -CFA_TCAM_MGR_ERR_CODE(INVAL);		\
		}							\
	} while (0)

/**
 * Checks 2 parameters against NULL.
 */
#define CFA_TCAM_MGR_CHECK_PARMS2(parms1, parms2) do {			\
		if ((parms1) == NULL || (parms2) == NULL) {		\
			CFA_TCAM_MGR_LOG_0(ERR, "Invalid Argument(s)\n"); \
			return -CFA_TCAM_MGR_ERR_CODE(INVAL);		\
		}							\
	} while (0)

/**
 * Checks 3 parameters against NULL.
 */
#define CFA_TCAM_MGR_CHECK_PARMS3(parms1, parms2, parms3) do {		\
		if ((parms1) == NULL ||					\
		    (parms2) == NULL ||					\
		    (parms3) == NULL) {					\
			CFA_TCAM_MGR_LOG_0(ERR, "Invalid Argument(s)\n"); \
			return -CFA_TCAM_MGR_ERR_CODE(INVAL);		\
		}							\
	} while (0)

enum cfa_tcam_mgr_tbl_type {
	/* Logical TCAM tables */
	CFA_TCAM_MGR_TBL_TYPE_START,
	CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_AFM =
		CFA_TCAM_MGR_TBL_TYPE_START,
	CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_APPS,
	CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_AFM,
	CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_APPS,
	CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_AFM,
	CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_HIGH_APPS,
	CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_AFM,
	CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_LOW_APPS,
	CFA_TCAM_MGR_TBL_TYPE_MAX
};

enum cfa_tcam_mgr_device_type {
	CFA_TCAM_MGR_DEVICE_TYPE_P4 = 0,
	CFA_TCAM_MGR_DEVICE_TYPE_SR,
	CFA_TCAM_MGR_DEVICE_TYPE_P5,
	CFA_TCAM_MGR_DEVICE_TYPE_MAX
};

struct cfa_tcam_mgr_context {
	struct tf *tfp;
};

/**
 * TCAM Manager initialization parameters
 */
struct cfa_tcam_mgr_init_parms {
	/**
	 * [in] TCAM resources reserved
	 *      type element is not used.
	 */
	struct tf_rm_resc_entry resc[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];
	/**
	 * [out] maximum number of entries available.
	 */
	uint32_t max_entries;
};

/**
 * TCAM Manager initialization parameters
 */
struct cfa_tcam_mgr_qcaps_parms {
	/**
	 * [out] Bitmasks.  Set if TCAM Manager is managing a logical TCAM.
	 * Each bitmask is indexed by logical TCAM table ID.
	 */
	uint32_t rx_tcam_supported;
	uint32_t tx_tcam_supported;
};

/**
 * TCAM Manager configuration parameters
 */
struct cfa_tcam_mgr_cfg_parms {
	/**
	 * [in] Number of tcam types in each of the configuration arrays
	 */
	uint16_t num_elements;
	/**
	 * [in] Session resource allocations
	 */
	uint16_t tcam_cnt[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];

	/**
	 * [in] TCAM Locations reserved
	 */
	struct tf_rm_resc_entry (*resv_res)[CFA_TCAM_MGR_TBL_TYPE_MAX];
};

/**
 * TCAM Manager allocation parameters
 */
struct cfa_tcam_mgr_alloc_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of the allocation
	 */
	enum cfa_tcam_mgr_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [in] key size (bytes)
	 */
	uint16_t key_size;
	/**
	 * [in] Priority of entry requested (definition TBD)
	 */
	uint16_t priority;
	/**
	 * [out] Id of allocated entry or found entry (if search_enable)
	 */
	uint16_t id;
};

/**
 * TCAM Manager free parameters
 */
struct cfa_tcam_mgr_free_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of the allocation
	 * If the type is not known, set the type to CFA_TCAM_MGR_TBL_TYPE_MAX.
	 */
	enum cfa_tcam_mgr_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [in] Entry ID to free
	 */
	uint16_t id;
};

/**
 * TCAM Manager set parameters
 */
struct cfa_tcam_mgr_set_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to set
	 */
	enum cfa_tcam_mgr_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [in] Entry ID to write to
	 */
	uint16_t id;
	/**
	 * [in] array containing key
	 */
	uint8_t *key;
	/**
	 * [in] array containing mask fields
	 */
	uint8_t *mask;
	/**
	 * [in] key size (bytes)
	 */
	uint16_t key_size;
	/**
	 * [in] array containing result
	 */
	uint8_t *result;
	/**
	 * [in] result size (bytes)
	 */
	uint16_t result_size;
};

/**
 * TCAM Manager get parameters
 */
struct cfa_tcam_mgr_get_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to get
	 */
	enum cfa_tcam_mgr_tbl_type type;
	/**
	 * [in] Type of HCAPI
	 */
	uint16_t hcapi_type;
	/**
	 * [in] Entry ID to read
	 */
	uint16_t id;
	/**
	 * [out] array containing key
	 */
	uint8_t *key;
	/**
	 * [out] array containing mask fields
	 */
	uint8_t *mask;
	/**
	 * [out] key size (bytes)
	 */
	uint16_t key_size;
	/**
	 * [out] array containing result
	 */
	uint8_t *result;
	/**
	 * [out] result size (bytes)
	 */
	uint16_t result_size;
};

/**
 * cfa_tcam_mgr_shared_clear_parms parameter definition
 */
struct cfa_tcam_mgr_shared_clear_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum cfa_tcam_mgr_tbl_type type;
};

/**
 * cfa_tcam_mgr_shared_move_parms parameter definition
 */
struct cfa_tcam_mgr_shared_move_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum cfa_tcam_mgr_tbl_type type;
};

/**
 * @page tcam TCAM Manager
 *
 * @ref cfa_tcam_mgr_init
 *
 * @ref cfa_tcam_mgr_get_phys_table_type
 *
 * @ref cfa_tcam_mgr_bind
 *
 * @ref cfa_tcam_mgr_unbind
 *
 * @ref cfa_tcam_mgr_alloc
 *
 * @ref cfa_tcam_mgr_free
 *
 * @ref cfa_tcam_mgr_set
 *
 * @ref cfa_tcam_mgr_get
 *
 */

const char *
cfa_tcam_mgr_tbl_2_str(enum cfa_tcam_mgr_tbl_type tcam_type);

/**
 * Initializes the TCAM Manager
 *
 * [in] type
 *   Device type
 *
 * Returns
 *   - (0) if successful.
 *   - (<0) on failure.
 */
int
cfa_tcam_mgr_init(int sess_idx, enum cfa_tcam_mgr_device_type type,
		  struct cfa_tcam_mgr_init_parms *parms);

/**
 * Returns the physical TCAM table that a logical TCAM table uses.
 *
 * [in] type
 *   Logical table type
 *
 * Returns
 *   - (tf_tcam_tbl_type) if successful.
 *   - (<0) on failure.
 */
int
cfa_tcam_mgr_get_phys_table_type(enum cfa_tcam_mgr_tbl_type type);

/**
 * Queries the capabilities of TCAM Manager.
 *
 * [in] context
 *   Pointer to context information
 *
 * [out] parms
 *   Pointer to parameters to be returned
 *
 * Returns
 *   - (0) if successful.
 *   - (<0) on failure.
 */
int
cfa_tcam_mgr_qcaps(struct cfa_tcam_mgr_context *context __rte_unused,
		   struct cfa_tcam_mgr_qcaps_parms *parms);

/**
 * Initializes the TCAM module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * [in] context
 *   Pointer to context information
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_bind(struct cfa_tcam_mgr_context *context,
		      struct cfa_tcam_mgr_cfg_parms *parms);

/**
 * Cleans up the private DBs and releases all the data.
 *
 * [in] context
 *   Pointer to context information
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_unbind(struct cfa_tcam_mgr_context *context);

/**
 * Allocates the requested tcam type from the internal RM DB.
 *
 * [in] context
 *   Pointer to context information
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_alloc(struct cfa_tcam_mgr_context *context,
		       struct cfa_tcam_mgr_alloc_parms *parms);

/**
 * Free's the requested table type and returns it to the DB.
 * If refcount goes to 0 then it is returned to the table type DB.
 *
 * [in] context
 *   Pointer to context information
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_free(struct cfa_tcam_mgr_context *context,
		      struct cfa_tcam_mgr_free_parms *parms);

/**
 * Configures the requested element by sending a firmware request which
 * then installs it into the device internal structures.
 *
 * [in] context
 *   Pointer to context information
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_set(struct cfa_tcam_mgr_context *context,
		     struct cfa_tcam_mgr_set_parms *parms);

/**
 * Retrieves the requested element by sending a firmware request to get
 * the element.
 *
 * [in] context
 *   Pointer to context information
 *
 * [in] parms
 *   Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_get(struct cfa_tcam_mgr_context *context,
		     struct cfa_tcam_mgr_get_parms *parms);

int
cfa_tcam_mgr_tables_get(int sess_idx, enum tf_dir dir,
			enum cfa_tcam_mgr_tbl_type type,
			uint16_t *start_row,
			uint16_t *end_row,
			uint16_t *max_entries,
			uint16_t *slices);
int
cfa_tcam_mgr_tables_set(int sess_idx, enum tf_dir dir,
			enum cfa_tcam_mgr_tbl_type type,
			uint16_t start_row,
			uint16_t end_row,
			uint16_t max_entries);

int cfa_tcam_mgr_shared_clear(struct cfa_tcam_mgr_context *context,
		     struct cfa_tcam_mgr_shared_clear_parms *parms);

int cfa_tcam_mgr_shared_move(struct cfa_tcam_mgr_context *context,
		     struct cfa_tcam_mgr_shared_move_parms *parms);

void cfa_tcam_mgr_rows_dump(int sess_idx, enum tf_dir dir, enum cfa_tcam_mgr_tbl_type type);
void cfa_tcam_mgr_tables_dump(int sess_idx, enum tf_dir dir, enum cfa_tcam_mgr_tbl_type type);
void cfa_tcam_mgr_entries_dump(int sess_idx);
#endif /* _CFA_TCAM_MGR_H */
