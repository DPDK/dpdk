/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#include <rte_graph_feature_arc.h>
#include <eal_export.h>
#include "graph_private.h"

/* global feature arc list */
static STAILQ_HEAD(, rte_graph_feature_arc_register) feature_arc_list =
					STAILQ_HEAD_INITIALIZER(feature_arc_list);

/* global feature arc list */
static STAILQ_HEAD(, rte_graph_feature_register) feature_list =
					STAILQ_HEAD_INITIALIZER(feature_list);

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_graph_feature_arc_register, 25.07)
void __rte_graph_feature_arc_register(struct rte_graph_feature_arc_register *reg,
				      const char *caller_name, int lineno)
{
	RTE_SET_USED(caller_name);
	RTE_SET_USED(lineno);
	/* Do not validate arc registration here but as part of rte_graph_feature_arc_init() */
	STAILQ_INSERT_TAIL(&feature_arc_list, reg, next_arc);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(__rte_graph_feature_register, 25.07)
void __rte_graph_feature_register(struct rte_graph_feature_register *reg,
				  const char *caller_name, int lineno)
{
	RTE_SET_USED(caller_name);
	RTE_SET_USED(lineno);

	/* Add to the feature_list*/
	STAILQ_INSERT_TAIL(&feature_list, reg, next_feature);
}
