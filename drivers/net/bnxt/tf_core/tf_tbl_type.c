/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_tbl_type.h"

struct tf;

/**
 * Table Type DBs.
 */
/* static void *tbl_db[TF_DIR_MAX]; */

/**
 * Table Type Shadow DBs
 */
/* static void *shadow_tbl_db[TF_DIR_MAX]; */

/**
 * Init flag, set on bind and cleared on unbind
 */
/* static uint8_t init; */

/**
 * Shadow init flag, set on bind and cleared on unbind
 */
/* static uint8_t shadow_init; */

int
tf_tbl_type_bind(struct tf *tfp __rte_unused,
		 struct tf_tbl_type_cfg_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tbl_type_unbind(struct tf *tfp __rte_unused)
{
	return 0;
}

int
tf_tbl_type_alloc(struct tf *tfp __rte_unused,
		  struct tf_tbl_type_alloc_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tbl_type_free(struct tf *tfp __rte_unused,
		 struct tf_tbl_type_free_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tbl_type_alloc_search(struct tf *tfp __rte_unused,
			 struct tf_tbl_type_alloc_search_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tbl_type_set(struct tf *tfp __rte_unused,
		struct tf_tbl_type_set_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tbl_type_get(struct tf *tfp __rte_unused,
		struct tf_tbl_type_get_parms *parms __rte_unused)
{
	return 0;
}
