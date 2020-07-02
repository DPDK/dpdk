/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_shadow_tbl.h"

/**
 * Shadow table DB element
 */
struct tf_shadow_tbl_element {
	/**
	 * Hash table
	 */
	void *hash;

	/**
	 * Reference count, array of number of table type entries
	 */
	uint16_t *ref_count;
};

/**
 * Shadow table DB definition
 */
struct tf_shadow_tbl_db {
	/**
	 * The DB consists of an array of elements
	 */
	struct tf_shadow_tbl_element *db;
};

int
tf_shadow_tbl_create_db(struct tf_shadow_tbl_create_db_parms *parms __rte_unused)
{
	return 0;
}

int
tf_shadow_tbl_free_db(struct tf_shadow_tbl_free_db_parms *parms __rte_unused)
{
	return 0;
}

int
tf_shadow_tbl_search(struct tf_shadow_tbl_search_parms *parms __rte_unused)
{
	return 0;
}

int
tf_shadow_tbl_insert(struct tf_shadow_tbl_insert_parms *parms __rte_unused)
{
	return 0;
}

int
tf_shadow_tbl_remove(struct tf_shadow_tbl_remove_parms *parms __rte_unused)
{
	return 0;
}
