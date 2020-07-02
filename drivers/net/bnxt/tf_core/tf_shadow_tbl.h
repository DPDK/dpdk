/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_SHADOW_TBL_H_
#define _TF_SHADOW_TBL_H_

#include "tf_core.h"

struct tf;

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
	 * TF Table type
	 */
	enum tf_tbl_type type;

	/**
	 * Number of entries the Shadow DB needs to hold
	 */
	int num_entries;

	/**
	 * Element width for this table type
	 */
	int element_width;
};

/**
 * Shadow table DB creation parameters
 */
struct tf_shadow_tbl_create_db_parms {
	/**
	 * [in] Configuration information for the shadow db
	 */
	struct tf_shadow_tbl_cfg_parms *cfg;
	/**
	 * [in] Number of elements in the parms structure
	 */
	uint16_t num_elements;
	/**
	 * [out] Shadow table DB handle
	 */
	void *tf_shadow_tbl_db;
};

/**
 * Shadow table DB free parameters
 */
struct tf_shadow_tbl_free_db_parms {
	/**
	 * Shadow table DB handle
	 */
	void *tf_shadow_tbl_db;
};

/**
 * Shadow table search parameters
 */
struct tf_shadow_tbl_search_parms {
	/**
	 * [in] Shadow table DB handle
	 */
	void *tf_shadow_tbl_db;
	/**
	 * [in] Table type
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Pointer to entry blob value in remap table to match
	 */
	uint8_t *entry;
	/**
	 * [in] Size of the entry blob passed in bytes
	 */
	uint16_t entry_sz;
	/**
	 * [out] Index of the found element returned if hit
	 */
	uint16_t *index;
	/**
	 * [out] Reference count incremented if hit
	 */
	uint16_t *ref_cnt;
};

/**
 * Shadow table insert parameters
 */
struct tf_shadow_tbl_insert_parms {
	/**
	 * [in] Shadow table DB handle
	 */
	void *tf_shadow_tbl_db;
	/**
	 * [in] Tbl type
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Pointer to entry blob value in remap table to match
	 */
	uint8_t *entry;
	/**
	 * [in] Size of the entry blob passed in bytes
	 */
	uint16_t entry_sz;
	/**
	 * [in] Entry to update
	 */
	uint16_t index;
	/**
	 * [out] Reference count after insert
	 */
	uint16_t *ref_cnt;
};

/**
 * Shadow table remove parameters
 */
struct tf_shadow_tbl_remove_parms {
	/**
	 * [in] Shadow table DB handle
	 */
	void *tf_shadow_tbl_db;
	/**
	 * [in] Tbl type
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Entry to update
	 */
	uint16_t index;
	/**
	 * [out] Reference count after removal
	 */
	uint16_t *ref_cnt;
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
 */
int tf_shadow_tbl_search(struct tf_shadow_tbl_search_parms *parms);

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
