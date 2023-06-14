/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TABLE_ARRAY_H__
#define __INCLUDE_RTE_TABLE_ARRAY_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Table Array
 *
 * Simple array indexing. Lookup key is the array entry index.
 */

#include <stdint.h>

#include "rte_table.h"

/** Array table parameters */
struct rte_table_array_params {
	/** Number of array entries. Has to be a power of two. */
	uint32_t n_entries;

	/** Byte offset within input packet meta-data where lookup key (i.e. the
	    array entry index) is located. */
	uint32_t offset;
};

/** Array table key format */
struct rte_table_array_key {
	/** Array entry index */
	uint32_t pos;
};

/** Array table operations */
extern struct rte_table_ops rte_table_array_ops;

#ifdef __cplusplus
}
#endif

#endif
