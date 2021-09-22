/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CNXK_TM_H__
#define __CNXK_TM_H__

#include <stdbool.h>

#include <rte_tm_driver.h>

#include "roc_api.h"

struct cnxk_nix_tm_node {
	struct roc_nix_tm_node nix_node;
	struct rte_tm_node_params params;
};

#endif /* __CNXK_TM_H__ */
