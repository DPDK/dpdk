/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_shadow_tcam.h"

/**
 * Shadow tcam DB element
 */
struct tf_shadow_tcam_element {
	/**
	 * Hash table
	 */
	void *hash;

	/**
	 * Reference count, array of number of tcam entries
	 */
	uint16_t *ref_count;
};

/**
 * Shadow tcam DB definition
 */
struct tf_shadow_tcam_db {
	/**
	 * The DB consists of an array of elements
	 */
	struct tf_shadow_tcam_element *db;
};

int
tf_shadow_tcam_create_db(struct tf_shadow_tcam_create_db_parms *parms __rte_unused)
{
	return 0;
}

int
tf_shadow_tcam_free_db(struct tf_shadow_tcam_free_db_parms *parms __rte_unused)
{
	return 0;
}

int
tf_shadow_tcam_search(struct tf_shadow_tcam_search_parms *parms __rte_unused)
{
	return 0;
}

int
tf_shadow_tcam_insert(struct tf_shadow_tcam_insert_parms *parms __rte_unused)
{
	return 0;
}

int
tf_shadow_tcam_remove(struct tf_shadow_tcam_remove_parms *parms __rte_unused)
{
	return 0;
}
