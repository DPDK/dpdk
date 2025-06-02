/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_MODULE_API_H
#define APP_GRAPH_MODULE_API_H

#include <stdint.h>
#include <stdbool.h>

#include "cli.h"
#include "conn.h"
#include "commands.h"
#include "ethdev.h"
#include "ethdev_rx.h"
#include "graph.h"
#include "l2fwd.h"
#include "l3fwd.h"
#include "mempool.h"
#include "neigh.h"
#include "route.h"
#include "utils.h"
#include "feature.h"

/*
 * Externs
 */
extern volatile bool force_quit;
extern struct conn *conn;

enum ip4_lookup_mode {
	IP4_LOOKUP_LPM,
	IP4_LOOKUP_FIB
};

extern enum ip4_lookup_mode ip4_lookup_m;

bool app_graph_stats_enabled(void);
bool app_graph_feature_arc_enabled(void);
bool app_graph_exit(void);

#endif
