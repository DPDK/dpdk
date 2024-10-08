/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TABLE_STUB_H__
#define __INCLUDE_RTE_TABLE_STUB_H__

/**
 * @file
 * RTE Table Stub
 *
 * The stub table lookup operation produces lookup miss for all input packets.
 */

#include "rte_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Stub table parameters: NONE */

/** Stub table operations */
extern struct rte_table_ops rte_table_stub_ops;

#ifdef __cplusplus
}
#endif

#endif
