/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_SHADOW_TCAM_H_
#define _TF_SHADOW_TCAM_H_

#include "tf_core.h"

struct tf;

/**
 * The Shadow tcam module provides shadow DB handling for tcam based
 * TF types. A shadow DB provides the capability that allows for reuse
 * of TF resources.
 *
 * A Shadow tcam DB is intended to be used by the Tcam module only.
 */

/**
 * Shadow DB configuration information for a single tcam type.
 *
 * During Device initialization the HCAPI device specifics are learned
 * and as well as the RM DB creation. From that those initial steps
 * this structure can be populated.
 *
 * NOTE:
 * If used in an array of tcam types then such array must be ordered
 * by the TF type is represents.
 */
struct tf_shadow_tcam_cfg_parms {
	/**
	 * TF tcam type
	 */
	enum tf_tcam_tbl_type type;

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
 * Shadow tcam DB creation parameters
 */
struct tf_shadow_tcam_create_db_parms {
	/**
	 * [in] Configuration information for the shadow db
	 */
	struct tf_shadow_tcam_cfg_parms *cfg;
	/**
	 * [in] Number of elements in the parms structure
	 */
	uint16_t num_elements;
	/**
	 * [out] Shadow tcam DB handle
	 */
	void *tf_shadow_tcam_db;
};

/**
 * Shadow tcam DB free parameters
 */
struct tf_shadow_tcam_free_db_parms {
	/**
	 * Shadow tcam DB handle
	 */
	void *tf_shadow_tcam_db;
};

/**
 * Shadow tcam search parameters
 */
struct tf_shadow_tcam_search_parms {
	/**
	 * [in] Shadow tcam DB handle
	 */
	void *tf_shadow_tcam_db;
	/**
	 * [in] TCAM tbl type
	 */
	enum tf_tcam_tbl_type type;
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
 * Shadow tcam insert parameters
 */
struct tf_shadow_tcam_insert_parms {
	/**
	 * [in] Shadow tcam DB handle
	 */
	void *tf_shadow_tcam_db;
	/**
	 * [in] TCAM tbl type
	 */
	enum tf_tcam_tbl_type type;
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
 * Shadow tcam remove parameters
 */
struct tf_shadow_tcam_remove_parms {
	/**
	 * [in] Shadow tcam DB handle
	 */
	void *tf_shadow_tcam_db;
	/**
	 * [in] TCAM tbl type
	 */
	enum tf_tcam_tbl_type type;
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
 * @page shadow_tcam Shadow tcam DB
 *
 * @ref tf_shadow_tcam_create_db
 *
 * @ref tf_shadow_tcam_free_db
 *
 * @reg tf_shadow_tcam_search
 *
 * @reg tf_shadow_tcam_insert
 *
 * @reg tf_shadow_tcam_remove
 */

/**
 * Creates and fills a Shadow tcam DB. The DB is indexed per the
 * parms structure.
 *
 * [in] parms
 *   Pointer to create db parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_tcam_create_db(struct tf_shadow_tcam_create_db_parms *parms);

/**
 * Closes the Shadow tcam DB and frees all allocated
 * resources per the associated database.
 *
 * [in] parms
 *   Pointer to the free DB parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_tcam_free_db(struct tf_shadow_tcam_free_db_parms *parms);

/**
 * Search Shadow tcam db for matching result
 *
 * [in] parms
 *   Pointer to the search parameters
 *
 * Returns
 *   - (0) if successful, element was found.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_tcam_search(struct tf_shadow_tcam_search_parms *parms);

/**
 * Inserts an element into the Shadow tcam DB. Will fail if the
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
int tf_shadow_tcam_insert(struct tf_shadow_tcam_insert_parms *parms);

/**
 * Removes an element from the Shadow tcam DB. Will fail if the
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
int tf_shadow_tcam_remove(struct tf_shadow_tcam_remove_parms *parms);

#endif /* _TF_SHADOW_TCAM_H_ */
