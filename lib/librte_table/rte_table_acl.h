/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
