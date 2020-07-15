/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_SHADOW_IDENTIFIER_H_
#define _TF_SHADOW_IDENTIFIER_H_

#include "tf_core.h"

struct tf;

/**
 * The Shadow Identifier module provides shadow DB handling for identifier based
 * TF types. A shadow DB provides the capability that allows for reuse
 * of TF resources.
 *
 * A Shadow identifier DB is intended to be used by the Identifier Type module
 * only.
 */

/**
 * Shadow DB configuration information for a single identifier type.
 *
 * It is used in an array of identifier types. The array must be ordered
 * by the TF type is represents.
 */
struct tf_shadow_ident_cfg_parms {
	/**
	 * TF Identifier type
	 */
	enum tf_identifier_type type;

	/**
	 * Number of entries the Shadow DB needs to hold
	 */
	int num_entries;

	/**
	 * Resource allocation count array. This array content
	 * originates from the tf_session_resources that is passed in
	 * on session open.
	 * Array size is num_elements.
	 */
	uint16_t *alloc_cnt;
};

/**
 * Shadow identifier DB creation parameters
 */
struct tf_shadow_ident_create_db_parms {
	/**
	 * [in] Receive or transmit direction.
	 */
	enum tf_dir dir;
	/**
	 * [in] Configuration information for the shadow db
	 */
	struct tf_shadow_ident_cfg_parms *cfg;
	/**
	 * [in] Number of elements in the parms structure
	 */
	uint16_t num_elements;
	/**
	 * [out] Shadow identifier DB handle
	 */
	void **tf_shadow_ident_db;
};

/**
 * Shadow identifier DB free parameters
 */
struct tf_shadow_ident_free_db_parms {
	/**
	 * Shadow identifier DB handle
	 */
	void *tf_shadow_ident_db;
};

/**
 * Shadow identifier search parameters
 */
struct tf_shadow_ident_search_parms {
	/**
	 * [in] Shadow identifier DB handle
	 */
	void *tf_shadow_ident_db;
	/**
	 * [in] Identifier type
	 */
	enum tf_identifier_type type;
	/**
	 * [in] id to search
	 */
	uint16_t search_id;
	/**
	 * [out] Index of the found element returned if hit
	 */
	bool *hit;
	/**
	 * [out] Reference count incremented if hit
	 */
	uint32_t *ref_cnt;
};

/**
 * Shadow identifier insert parameters
 */
struct tf_shadow_ident_insert_parms {
	/**
	 * [in] Shadow identifier DB handle
	 */
	void *tf_shadow_ident_db;
	/**
	 * [in] Tbl type
	 */
	enum tf_identifier_type type;
	/**
	 * [in] Entry to update
	 */
	uint16_t id;
	/**
	 * [out] Reference count after insert
	 */
	uint32_t ref_cnt;
};

/**
 * Shadow identifier remove parameters
 */
struct tf_shadow_ident_remove_parms {
	/**
	 * [in] Shadow identifier DB handle
	 */
	void *tf_shadow_ident_db;
	/**
	 * [in] Tbl type
	 */
	enum tf_identifier_type type;
	/**
	 * [in] Entry to update
	 */
	uint16_t id;
	/**
	 * [out] Reference count after removal
	 */
	uint32_t *ref_cnt;
};

/**
 * @page shadow_ident Shadow identifier DB
 *
 * @ref tf_shadow_ident_create_db
 *
 * @ref tf_shadow_ident_free_db
 *
 * @reg tf_shadow_ident_search
 *
 * @reg tf_shadow_ident_insert
 *
 * @reg tf_shadow_ident_remove
 */

/**
 * Creates and fills a Shadow identifier DB. The DB is indexed per the
 * parms structure.
 *
 * [in] parms
 *   Pointer to create db parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_ident_create_db(struct tf_shadow_ident_create_db_parms *parms);

/**
 * Closes the Shadow identifier DB and frees all allocated
 * resources per the associated database.
 *
 * [in] parms
 *   Pointer to the free DB parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_ident_free_db(struct tf_shadow_ident_free_db_parms *parms);

/**
 * Search Shadow identifier db for matching result
 *
 * [in] parms
 *   Pointer to the search parameters
 *
 * Returns
 *   - (0) if successful, element was found.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_ident_search(struct tf_shadow_ident_search_parms *parms);

/**
 * Inserts an element into the Shadow identifier DB. Ref_count after insert
 * will be incremented.
 *
 * [in] parms
 *   Pointer to insert parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_shadow_ident_insert(struct tf_shadow_ident_insert_parms *parms);

/**
 * Removes an element from the Shadow identifier DB. Will fail if the
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
int tf_shadow_ident_remove(struct tf_shadow_ident_remove_parms *parms);

#endif /* _TF_SHADOW_IDENTIFIER_H_ */
