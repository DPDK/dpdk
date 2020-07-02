/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_tcam.h"

struct tf;

/**
 * TCAM DBs.
 */
/* static void *tcam_db[TF_DIR_MAX]; */

/**
 * TCAM Shadow DBs
 */
/* static void *shadow_tcam_db[TF_DIR_MAX]; */

/**
 * Init flag, set on bind and cleared on unbind
 */
/* static uint8_t init; */

/**
 * Shadow init flag, set on bind and cleared on unbind
 */
/* static uint8_t shadow_init; */

int
tf_tcam_bind(struct tf *tfp __rte_unused,
	     struct tf_tcam_cfg_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tcam_unbind(struct tf *tfp __rte_unused)
{
	return 0;
}

int
tf_tcam_alloc(struct tf *tfp __rte_unused,
	      struct tf_tcam_alloc_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tcam_free(struct tf *tfp __rte_unused,
	     struct tf_tcam_free_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tcam_alloc_search(struct tf *tfp __rte_unused,
		     struct tf_tcam_alloc_search_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tcam_set(struct tf *tfp __rte_unused,
	    struct tf_tcam_set_parms *parms __rte_unused)
{
	return 0;
}

int
tf_tcam_get(struct tf *tfp __rte_unused,
	    struct tf_tcam_get_parms *parms __rte_unused)
{
	return 0;
}
