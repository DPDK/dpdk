/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_SHADOW_TCAM_H_
#define _TF_SHADOW_TCAM_H_

#include "tf_core.h"

/**
 * Shadow DB configuration information
 *
 * The shadow configuration is for all tcam table types for a direction
 */
struct tf_shadow_tcam_cfg_parms {
	/**
	 * [in] The number of elements in the alloc_cnt and base_addr
	 * For now, it should always be equal to TF_TCAM_TBL_TYPE_MAX
	 */
	int num_entries;
	/**
	 * [in] Resource allocation count array
	 * This array content originates from the tf_session_resources
	 * that is passed in on session open
	 * Array size is TF_TCAM_TBL_TYPE_MAX
	 */
	uint16_t *alloc_cnt;
	/**
	 * [in] The base index for each tcam table
	 */
	uint16_t base_addr[TF_TCAM_TBL_TYPE_MAX];
};

/**
 * Shadow TCAM  DB creation parameters.  The shadow db for this direction
 * is returned
 */
struct tf_shadow_tcam_create_db_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Configuration information for the shadow db
	 */
	struct tf_shadow_tcam_cfg_parms *cfg;
	/**
	 * [out] Shadow tcam DB handle
	 */
	void **shadow_db;
};

/**
 * Create the shadow db for a single direction
 *
 * The returned shadow db must be free using the free db API when no longer
 * needed
 */
int
tf_shadow_tcam_create_db(struct tf_shadow_tcam_create_db_parms *parms);

/**
 * Shadow TCAM free parameters
 */
struct tf_shadow_tcam_free_db_parms {
	/**
	 * [in] Shadow tcam DB handle
	 */
	void *shadow_db;
};

/**
 * Free all resources associated with the shadow db
 */
int
tf_shadow_tcam_free_db(struct tf_shadow_tcam_free_db_parms *parms);

/**
 * Shadow TCAM bind index parameters
 */
struct tf_shadow_tcam_bind_index_parms {
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
	enum tf_tcam_tbl_type type;
	/**
	 * [in] index of the entry to program
	 */
	uint16_t idx;
	/**
	 * [in] struct containing key
	 */
	uint8_t *key;
	/**
	 * [in] struct containing mask fields
	 */
	uint8_t *mask;
	/**
	 * [in] key size in bits (if search)
	 */
	uint16_t key_size;
	/**
	 * [in] The hash bucket handled returned from the search
	 */
	uint32_t hb_handle;
};

/**
 * Binds the allocated tcam index with the hash and shadow tables
 */
int
tf_shadow_tcam_bind_index(struct tf_shadow_tcam_bind_index_parms *parms);

/**
 * Shadow TCAM insert parameters
 */
struct	tf_shadow_tcam_insert_parms {
	/**
	 * [in] Shadow tcam DB handle
	 */
	void *shadow_db;
	/**
	 * [in] The set parms from tf core
	 */
	struct tf_tcam_set_parms *sparms;
};

/**
 * Set the entry into the tcam manager hash and shadow tables
 *
 * The search must have been used prior to setting the entry so that the
 * hash has been calculated and duplicate entries will not be added
 */
int
tf_shadow_tcam_insert(struct tf_shadow_tcam_insert_parms *parms);

/**
 * Shadow TCAM remove parameters
 */
struct tf_shadow_tcam_remove_parms {
	/**
	 * [in] Shadow tcam DB handle
	 */
	void *shadow_db;
	/**
	 * [in,out] The set parms from tf core
	 */
	struct tf_tcam_free_parms *fparms;
};

/**
 * Remove the entry from the tcam hash and shadow tables
 *
 * The search must have been used prior to setting the entry so that the
 * hash has been calculated and duplicate entries will not be added
 */
int
tf_shadow_tcam_remove(struct tf_shadow_tcam_remove_parms *parms);

/**
 * Shadow TCAM search parameters
 */
struct tf_shadow_tcam_search_parms {
	/**
	 * [in] Shadow tcam DB handle
	 */
	void *shadow_db;
	/**
	 * [in,out] The search parameters from tf core
	 */
	struct tf_tcam_alloc_search_parms *sparms;
	/**
	 * [out] The hash handle to use for the set
	 */
	uint32_t hb_handle;
};

/**
 * Search for an entry in the tcam hash/shadow tables
 *
 * If there is a miss, but there is room for insertion, the hb_handle returned
 * is used for insertion during the bind index API
 */
int
tf_shadow_tcam_search(struct tf_shadow_tcam_search_parms *parms);
#endif
