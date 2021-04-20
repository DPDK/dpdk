/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TABLE_ACL_H__
#define __INCLUDE_RTE_TABLE_ACL_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Table ACL
 *
 * This table uses the Access Control List (ACL) algorithm to uniquely
 * associate data to lookup keys.
 *
 * Use-cases: Firewall rule database, etc.
 *
 ***/

#include <stdint.h>

#include "rte_acl.h"

#include "rte_table.h"

/** ACL table parameters */
struct rte_table_acl_params {
	/** Name */
	const char *name;

	/** Maximum number of ACL rules in the table */
	uint32_t n_rules;

	/** Number of fields in the ACL rule specification */
	uint32_t n_rule_fields;

	/** Format specification of the fields of the ACL rule */
	struct rte_acl_field_def field_format[RTE_ACL_MAX_FIELDS];
};

/** ACL rule specification for entry add operation */
struct rte_table_acl_rule_add_params {
	/** ACL rule priority, with 0 as the highest priority */
	int32_t  priority;

	/** Values for the fields of the ACL rule to be added to the table */
	struct rte_acl_field field_value[RTE_ACL_MAX_FIELDS];
};

/** ACL rule specification for entry delete operation */
struct rte_table_acl_rule_delete_params {
	/** Values for the fields of the ACL rule to be deleted from table */
	struct rte_acl_field field_value[RTE_ACL_MAX_FIELDS];
};

/** ACL table operations */
extern struct rte_table_ops rte_table_acl_ops;

#ifdef __cplusplus
}
#endif

#endif
