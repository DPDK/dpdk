/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_TABLE_EM_H__
#define __INCLUDE_RTE_SWX_TABLE_EM_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Exact Match Table
 */


#include <rte_swx_table.h>

/** Exact match table operations - unoptimized. */
extern struct rte_swx_table_ops rte_swx_table_exact_match_unoptimized_ops;

/** Exact match table operations. */
extern struct rte_swx_table_ops rte_swx_table_exact_match_ops;

#ifdef __cplusplus
}
#endif

#endif
