/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_TABLE_WM_H__
#define __INCLUDE_RTE_SWX_TABLE_WM_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Wildcard Match Table
 */

#include <stdint.h>

#include <rte_swx_table.h>

/** Wildcard match table operations. */
extern struct rte_swx_table_ops rte_swx_table_wildcard_match_ops;

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_SWX_TABLE_WM_H__ */
