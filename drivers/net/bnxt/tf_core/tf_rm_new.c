/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "tf_rm_new.h"

/**
 * Resource query single entry. Used when accessing HCAPI RM on the
 * firmware.
 */
struct tf_rm_query_entry {
	/** Minimum guaranteed number of elements */
	uint16_t min;
	/** Maximum non-guaranteed number of elements */
	uint16_t max;
};

/**
 * Generic RM Element data type that an RM DB is build upon.
 */
struct tf_rm_element {
	/**
	 * RM Element configuration type. If Private then the
	 * hcapi_type can be ignored. If Null then the element is not
	 * valid for the device.
	 */
	enum tf_rm_elem_cfg_type type;

	/**
	 * HCAPI RM Type for the element.
	 */
	uint16_t hcapi_type;

	/**
	 * HCAPI RM allocated range information for the element.
	 */
	struct tf_rm_alloc_info alloc;

	/**
	 * Bit allocator pool for the element. Pool size is controlled
	 * by the struct tf_session_resources at time of session creation.
	 * Null indicates that the element is not used for the device.
	 */
	struct bitalloc *pool;
};

/**
 * TF RM DB definition
 */
struct tf_rm_db {
	/**
	 * The DB consists of an array of elements
	 */
	struct tf_rm_element *db;
};

int
tf_rm_create_db(struct tf *tfp __rte_unused,
		struct tf_rm_create_db_parms *parms __rte_unused)
{
	return 0;
}

int
tf_rm_free_db(struct tf *tfp __rte_unused,
	      struct tf_rm_free_db_parms *parms __rte_unused)
{
	return 0;
}

int
tf_rm_allocate(struct tf_rm_allocate_parms *parms __rte_unused)
{
	return 0;
}

int
tf_rm_free(struct tf_rm_free_parms *parms __rte_unused)
{
	return 0;
}

int
tf_rm_is_allocated(struct tf_rm_is_allocated_parms *parms __rte_unused)
{
	return 0;
}

int
tf_rm_get_info(struct tf_rm_get_alloc_info_parms *parms __rte_unused)
{
	return 0;
}

int
tf_rm_get_hcapi_type(struct tf_rm_get_hcapi_parms *parms __rte_unused)
{
	return 0;
}
