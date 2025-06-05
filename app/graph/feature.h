/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell International Ltd.
 */

#ifndef APP_GRAPH_FEATURE_H
#define APP_GRAPH_FEATURE_H

#include <cmdline_parse.h>
#include <rte_graph_feature_arc_worker.h>

int feature_enable(const char *arc_name, const char *feature_name, uint16_t portid);
int feature_disable(const char *arc_name, const char *feature_name, uint16_t portid);

#endif
