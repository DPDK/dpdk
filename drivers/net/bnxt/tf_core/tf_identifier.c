/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_identifier.h"

struct tf;

/**
 * Identifier DBs.
 */
/* static void *ident_db[TF_DIR_MAX]; */

/**
 * Init flag, set on bind and cleared on unbind
 */
/* static uint8_t init; */

int
tf_ident_bind(struct tf *tfp __rte_unused,
	      struct tf_ident_cfg *parms __rte_unused)
{
	return 0;
}

int
tf_ident_unbind(struct tf *tfp __rte_unused)
{
	return 0;
}

int
tf_ident_alloc(struct tf *tfp __rte_unused,
	       struct tf_ident_alloc_parms *parms __rte_unused)
{
	return 0;
}

int
tf_ident_free(struct tf *tfp __rte_unused,
	      struct tf_ident_free_parms *parms __rte_unused)
{
	return 0;
}
