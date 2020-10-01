/*  SPDX-License-Identifier: BSD-3-Clause
 *  Copyright(c) 2018 Marvell International Ltd.
 */

#ifndef __RTE_MVEP_COMMON_H__
#define __RTE_MVEP_COMMON_H__

#include <rte_compat.h>
#include <rte_kvargs.h>

enum mvep_module_type {
	MVEP_MOD_T_NONE = 0,
	MVEP_MOD_T_PP2,
	MVEP_MOD_T_SAM,
	MVEP_MOD_T_NETA,
	MVEP_MOD_T_LAST
};

__rte_internal
int rte_mvep_init(enum mvep_module_type module, struct rte_kvargs *kvlist);
__rte_internal
int rte_mvep_deinit(enum mvep_module_type module);

#endif /* __RTE_MVEP_COMMON_H__ */
