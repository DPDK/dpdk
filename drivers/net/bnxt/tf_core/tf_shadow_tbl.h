/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_SHADOW_TBL_H_
#define _TF_SHADOW_TBL_H_

#include "tf_core.h"

/**
 * The Shadow Table module provides shadow DB handling for table based
 * TF types. A shadow DB provides the capability that allows for reuse
 * of TF resources.
 *
 * A Shadow table DB is intended to be used by the Table Type module
 * only.
 */

/**
 * Shadow DB configuration information for a single table type.
 *
 * During Device initialization the HCAPI device specifics are learned
 * and as well as the RM DB creation. From that those initial steps
 * this structure can be populated.
 *
 * NOTE:
 * If used in an array of table types then such array must be ordered
 * by the TF type is represents.
 */
struct tf_shadow_tbl_cfg_parms {
	/**
	 * [in] The number of elements in the alloc_cnt and base_addr
	 * For now, it should always be equal to TF_TBL_TYPE_MAX
	 */
	int num_entries;

	/**
	 * [in] Resource allocation count array
	 * This array content originates from the tf_session_resources
	 * that is passed in on session open
	 * Array size is TF_TBL_TYPE_MAX
	 */
	uint16_t *alloc_cnt;
	/**
	 * [in] The base index for each table
	 */
	uint16_t base_addr[TF_TBL_TYPE_MAX];
};

/**
 * Shadow table DB creation parameters
 */
struct tf_shadow_tbl_create_db_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Configuration information for the shadow db
	 */
	struct tf_shadow_tbl_cfg_parms *cfg;
	/**
	 * [out] Shadow table DB handle
	 */
	void **shadow_db;
};

/**
 * Shadow table DB free parameters
 */
struct tf_shadow_tbl_free_db_parms {
	/**
	 * [in] Shadow table DB handle
	 */
	void *shadow_db;
};

/**
 * Shadow table search parameters
 */
struct tf_shadow_tbl_search_parms {
	/**
	 * [in] Shadow table DB handle
	 */
	void *shadow_db;
	/**
	 * [in,out] The search parms from tf core
	 */
	struct tf_tbl_alloc_search_parms *sparms;
	/**
	 * [out] Reference count incremented if hit
	 */
	uint32_t hb_handle;
};

/**
 * Shadow Table bind index parameters
 */
struct tf_shadow_tbl_bind_index_parms {
	/**
	 * [in] Shadow tcam DB handle
	 */
	void *shadow_db;
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum tf_tbl_type type;
	/**
	 * [in] index of the entry to program
	 */
	uint16_t idx;
	/**
	 * [in] struct containing key
	 */
	uint8_t *data;
	/**
	 * [in] data size in bytes
	 */
	uint16_t data_sz_in_bytes;
	/**
	 * [in] The hash bucket handled returned from the search
	 */
	uint32_t hb_handle;
};

/**
 * Shadow table insert parameters
 */
struct tf_shadow_tbl_insert_parms {
	/**
	 * [in] Shadow table DB handle
	 */
	void *shadow_db;
	/**
	 * [in] The insert parms from tf core
	 */
	struct tf_tbl_set_parms *sparms;
};

/**
 * Shadow table remove parameters
 */
struct tf_shadow_tbl_remove_parms {
	/**
	 * [in] Shadow table DB handle
	 */
	void *shadow_db;
	/**
	 * [in] The free parms from tf core
	 */
	struct tf_tbl_free_parms *fparms;
};

/**
 * @page shadow_tbl Shadow table DB
 *
 * @ref tf_shadow_tbl_create_db
 *
 * @ref tf_shadow_tbl_free_db
 *
 * @reg tf_shadow_tbl_search
 *
 * @reg tf_shadow_tbl_insert
 *
 * @reg tf_shadow_tbl_remove
 */

/**
 * Creates and fills a Shadow table DB. The DB is indexed per the
 * parms structure.
 *
 * [in] parms
 *   Pointer to create db parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_tbl_create_db(struct tf_shadow_tbl_create_db_parms *parms);

/**
 * Closes the Shadow table DB and frees all allocated
 * resources per the associated database.
 *
 * [in] parms
 *   Pointer to the free DB parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_tbl_free_db(struct tf_shadow_tbl_free_db_parms *parms);

/**
 * Search Shadow table db for matching result
 *
 * [in] parms
 *   Pointer to the search parameters
 *
 * Returns
 *   - (0) if successful, element was found.
 *   - (-EINVAL) on failure.
 *
 * If there is a miss, but there is room for insertion, the hb_handle returned
 * is used for insertion during the bind index API
 */
int tf_shadow_tbl_search(struct tf_shadow_tbl_search_parms *parms);

/**
 * Bind Shadow table db hash and result tables with result from search/alloc
 *
 * [in] parms
 *   Pointer to the search parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure.
 *
 * This is only called after a MISS in the search returns a hb_handle
 */
int tf_shadow_tbl_bind_index(struct tf_shadow_tbl_bind_index_parms *parms);

/**
 * Inserts an element into the Shadow table DB. Will fail if the
 * elements ref_count is different from 0. Ref_count after insert will
 * be incremented.
 *
 * [in] parms
 *   Pointer to insert parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_tbl_insert(struct tf_shadow_tbl_insert_parms *parms);

/**
 * Removes an element from the Shadow table DB. Will fail if the
 * elements ref_count is 0. Ref_count after removal will be
 * decremented.
 *
 * [in] parms
 *   Pointer to remove parameter
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_tbl_remove(struct tf_shadow_tbl_remove_parms *parms);

#endif /* _TF_SHADOW_TBL_H_ */
